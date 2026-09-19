/**
 * 逻辑层单元测试。不链接 ncurses，只测数据与计算。
 *   cd c && make test
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "calc.h"
#include "model.h"

static int passed = 0;
static int failed = 0;

#define CHECK(cond, ...)                              \
  do {                                                \
    if (cond) {                                       \
      passed++;                                       \
    } else {                                          \
      failed++;                                       \
      printf("  FAIL  ");                             \
      printf(__VA_ARGS__);                            \
      printf("\n");                                   \
    }                                                 \
  } while (0)

static int near(double a, double b) { return fabs(a - b) < 1e-6; }

/* ---------- JSON ---------- */

static void test_json_parse(void) {
  const char *err = NULL;
  Json *root = json_parse("{\"a\":1,\"b\":\"中文\",\"c\":[1,2,3],\"d\":{\"e\":null}}", 0, &err);
  CHECK(root != NULL, "基本对象应解析成功（%s）", err ? err : "");
  if (!root) return;

  CHECK(near(json_get_num(root, "a", -1), 1), "数字读取");
  CHECK(strcmp(json_get_str(root, "b", ""), "中文") == 0, "中文字符串读取");
  const Json *arr = json_get(root, "c");
  CHECK(arr && arr->type == JSON_ARRAY && arr->count == 3, "数组长度");
  CHECK(arr && json_num(arr->items[2], -1) == 3, "数组元素");
  CHECK(json_is_null(json_get(json_get(root, "d"), "e")), "null 判定");
  json_free(root);

  /* 转义与 \u */
  root = json_parse("{\"s\":\"a\\\"b\\\\c\\n\\u4e2d\"}", 0, &err);
  CHECK(root != NULL, "转义字符串应解析成功（%s）", err ? err : "");
  if (root) {
    const char *text = json_get_str(root, "s", "");
    CHECK(strcmp(text, "a\"b\\c\n中") == 0, "转义解码，实际为 [%s]", text);
    json_free(root);
  }

  /* 非法输入一律返回 NULL 并给出中文错误 */
  const char *bad[] = {"{\"a\":}", "{\"a\" 1}", "{,}", "[1,", "", "{\"a\":1}extra", "\"未闭合"};
  for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); i++) {
    const char *e = NULL;
    Json *v = json_parse(bad[i], 0, &e);
    CHECK(v == NULL, "非法输入 [%s] 应解析失败", bad[i]);
    CHECK(e != NULL && e[0] != '\0', "非法输入 [%s] 应给出错误说明", bad[i]);
    json_free(v);
  }
}

static void test_json_roundtrip(void) {
  Json *root = json_new_object();
  json_set(root, "className", json_new_string("高二(3)班"));
  json_set(root, "version", json_new_number(1));
  Json *arr = json_new_array();
  json_push(arr, json_new_number(150));
  json_push(arr, json_new_string("语文"));
  json_set(root, "items", arr);

  char *text = json_write(root, 1);
  CHECK(text != NULL, "序列化应成功");
  const char *err = NULL;
  Json *back = json_parse(text, 0, &err);
  CHECK(back != NULL, "往返解析应成功（%s）", err ? err : "");
  if (back) {
    CHECK(strcmp(json_get_str(back, "className", ""), "高二(3)班") == 0, "往返后中文不丢失");
    const Json *items = json_get(back, "items");
    CHECK(items && items->count == 2, "往返后数组长度");
    CHECK(items && near(json_num(items->items[0], -1), 150), "往返后数字");
    json_free(back);
  }
  /* 整数不应写成 150.0 */
  CHECK(text && strstr(text, "150") != NULL && strstr(text, "150.0") == NULL, "整数输出不带小数点");
  free(text);
  json_free(root);
}

/* ---------- 数据模型 ---------- */

static GradeBook *make_fixture(void) {
  GradeBook *gb = malloc(sizeof(GradeBook));
  gb_init(gb);
  snprintf(gb->class_name, sizeof(gb->class_name), "高二(3)班");

  Subject *zh = gb_add_subject(gb, "语文", 150);
  Subject *ma = gb_add_subject(gb, "数学", 100);
  Student *a = gb_add_student(gb, "001", "陈一鸣", "男", "");
  Student *b = gb_add_student(gb, "002", "李思远", "男", "");
  Student *c = gb_add_student(gb, "003", "王雨桐", "女", "");
  Exam *exam = gb_add_exam(gb, "第一次月考", "2026-03-10");

  gb_set_score(gb, exam->id, a->id, zh->id, 120);
  gb_set_score(gb, exam->id, a->id, ma->id, 90);
  gb_set_score(gb, exam->id, b->id, zh->id, 120);
  gb_set_score(gb, exam->id, b->id, ma->id, 90);
  gb_set_score(gb, exam->id, c->id, zh->id, 100); /* 数学缺考 */
  return gb;
}

static void test_model(void) {
  GradeBook *gb = make_fixture();
  const Exam *exam = gb_active_exam(gb);

  CHECK(gb->student_count == 3 && gb->subject_count == 2, "基础数据建立");
  CHECK(near(gb_get_score(gb, exam->id, gb->students[0].id, gb->subjects[1].id), 90), "成绩读取");
  CHECK(gb_get_score(gb, exam->id, gb->students[2].id, gb->subjects[1].id) < 0, "缺考返回 -1");

  /* 删除学生应连带清除其成绩 */
  char id[GM_ID_LEN];
  snprintf(id, sizeof(id), "%s", gb->students[2].id);
  int before = gb->cell_count;
  gb_remove_student(gb, id);
  CHECK(gb->student_count == 2, "删除学生");
  CHECK(gb->cell_count == before - 1, "删除学生应连带清除其成绩");

  /* 删除科目应清除成绩并从考试中移除引用 */
  snprintf(id, sizeof(id), "%s", gb->subjects[1].id);
  gb_remove_subject(gb, id);
  CHECK(gb->subject_count == 1, "删除科目");
  CHECK(gb_count_subject_cells(gb, id) == 0, "删除科目应清除其成绩");

  gb_free(gb);
  free(gb);
}

static void test_normalize(void) {
  const char *dirty =
      "{\"className\":\"\",\"subjects\":[{\"id\":\"s1\",\"name\":\"语文\",\"fullMark\":150},"
      "{\"id\":\"s2\",\"name\":\"\",\"fullMark\":100}],"
      "\"students\":[{\"id\":\"st1\",\"sid\":\"001\",\"name\":\"张三\"},{\"id\":\"st2\",\"name\":\"\"}],"
      "\"exams\":[{\"id\":\"e1\",\"name\":\"月考\",\"subjectIds\":[\"s1\",\"ghost\"]}],"
      "\"scores\":{\"e1\":{\"st1\":{\"s1\":100,\"ghost\":50},\"ghost\":{\"s1\":10}},\"ghost\":{\"st1\":{\"s1\":1}}},"
      "\"activeExamId\":\"nope\"}";

  const char *err = NULL;
  Json *root = json_parse(dirty, 0, &err);
  CHECK(root != NULL, "脏数据应能解析（%s）", err ? err : "");
  if (!root) return;

  GradeBook gb;
  gb_init(&gb);
  CHECK(gb_load_json(&gb, root) == 0, "脏数据应能载入并完成清洗");

  CHECK(strlen(gb.class_name) > 0, "空班名应回退默认值");
  CHECK(gb.subject_count == 1, "空名科目应剔除，实际 %d", gb.subject_count);
  CHECK(gb.student_count == 1, "无姓名学生应剔除，实际 %d", gb.student_count);
  CHECK(gb.exam_count == 1, "考试应保留");
  CHECK(gb.exams[0].subject_count == 1, "未知科目引用应剔除，实际 %d", gb.exams[0].subject_count);
  CHECK(gb.cell_count == 1, "悬空成绩应剔除，实际 %d", gb.cell_count);
  CHECK(strcmp(gb.active_exam_id, gb.exams[0].id) == 0, "非法 activeExamId 应回退首场考试");

  json_free(root);
  gb_free(&gb);

  /* 根节点不是对象时不应崩溃 */
  Json *arr = json_parse("[1,2,3]", 0, &err);
  GradeBook gb2;
  gb_init(&gb2);
  CHECK(gb_load_json(&gb2, arr) != 0, "数组根节点应被拒绝");
  json_free(arr);
  gb_free(&gb2);
}

/* ---------- 排名与统计 ---------- */

static void test_ranking(void) {
  GradeBook *gb = make_fixture();
  const Exam *exam = gb_active_exam(gb);

  RankRow *rows = NULL;
  int n = calc_ranking(gb, exam, &rows);
  CHECK(n == 3, "排名行数");
  if (!rows) {
    gb_free(gb);
    free(gb);
    return;
  }

  CHECK(rows[0].rank == 1 && rows[1].rank == 1, "同分应并列第 1");
  CHECK(rows[2].rank == 3, "同分后应跳号到 3，实际 %d", rows[2].rank);

  /* 找到缺考的王雨桐 */
  const RankRow *absent = NULL;
  for (int i = 0; i < n; i++)
    if (strcmp(rows[i].student->name, "王雨桐") == 0) absent = &rows[i];
  CHECK(absent != NULL, "应能找到缺考学生");
  if (absent) {
    CHECK(near(absent->total, 100), "缺考不应按 0 分计入总分，实际 %.1f", absent->total);
    CHECK(near(absent->average, 100), "平均分只除以已录科目，实际 %.1f", absent->average);
    CHECK(absent->missing == 1 && absent->counted == 1, "缺考与已录计数");
  }

  const RankRow *top = NULL;
  for (int i = 0; i < n; i++)
    if (strcmp(rows[i].student->name, "陈一鸣") == 0) top = &rows[i];
  CHECK(top && near(top->rate, 84.0), "得分率 = 210/250 = 84%%，实际 %.1f", top ? top->rate : -1);

  calc_free_ranking(rows, n);

  /* 单科统计 */
  SubjectStats stats;
  calc_subject_stats(gb, exam, gb->subjects[1].id, &stats);
  CHECK(stats.count == 2, "数学有效分数人数");
  CHECK(near(stats.average, 90) && near(stats.max, 90) && near(stats.min, 90), "单科平均分/最高/最低");
  CHECK(near(stats.pass_rate, 100), "及格率");

  int bucket_sum = 0;
  for (int i = 0; i < 5; i++) bucket_sum += stats.dist[i];
  CHECK(bucket_sum == stats.count, "分数段人数之和应等于有效分数个数");

  /* 无人录分时不产生 NaN */
  Subject *new_subject = gb_add_subject(gb, "生物", 100);
  SubjectStats empty;
  calc_subject_stats(gb, exam, new_subject->id, &empty);
  CHECK(empty.count == 0 && empty.average == 0 && empty.max == 0, "无人录分时应全部归零");
  CHECK(!isnan(empty.average) && !isnan(empty.pass_rate), "无人录分时不产生 NaN");

  /* 及格率 */
  RankRow *rows2 = NULL;
  int n2 = calc_ranking(gb, exam, &rows2);
  CHECK(near(calc_class_pass_rate(gb, exam, rows2, n2), 100), "全班及格率");
  calc_free_ranking(rows2, n2);

  /* 录入进度 */
  int total = 0, done = 0;
  double percent = 0;
  calc_progress(gb, exam, &total, &done, &percent);
  CHECK(total == 3 * 3, "应录格数 = 学生 × 科目");
  CHECK(done == 5, "已录格数，实际 %d", done);

  gb_free(gb);
  free(gb);
}

static void test_trend(void) {
  GradeBook *gb = malloc(sizeof(GradeBook));
  gb_init(gb);
  Subject *zh = gb_add_subject(gb, "语文", 100);
  Student *a = gb_add_student(gb, "001", "陈一鸣", "男", "");
  Exam *e1 = gb_add_exam(gb, "第一次月考", "2026-03-10");
  Exam *e2 = gb_add_exam(gb, "期中考试", "2026-04-22");
  gb_set_score(gb, e1->id, a->id, zh->id, 80);
  gb_set_score(gb, e2->id, a->id, zh->id, 95);

  double totals[8];
  int ranks[8];
  int n = 0;
  calc_student_trend(gb, a->id, totals, ranks, &n);
  CHECK(n == 2, "趋势点数");
  CHECK(n == 2 && near(totals[0], 80) && near(totals[1], 95), "趋势按日期升序：%s 早于 %s", e1->name, e2->name);
  CHECK(calc_rank_delta(1, 3) == 2, "名次从 3 到 1 应记为进步 2");
  CHECK(calc_rank_delta(3, 1) == -2, "名次从 1 到 3 应记为退步 2");

  gb_free(gb);
  free(gb);
}

/* ---------- 持久化与 CSV ---------- */

static void test_persist_roundtrip(void) {
  GradeBook *gb = make_fixture();
  const char *path = "/tmp/gm_test_grades.json";

  char err[256] = "";
  CHECK(gb_save_file(gb, path, err, sizeof(err)) == 0, "保存成功（%s）", err);

  GradeBook loaded;
  gb_init(&loaded);
  CHECK(gb_load_file(&loaded, path, err, sizeof(err)) == 0, "读回成功（%s）", err);
  CHECK(strcmp(loaded.class_name, gb->class_name) == 0, "班名往返一致");
  CHECK(loaded.student_count == gb->student_count, "学生数往返一致");
  CHECK(loaded.subject_count == gb->subject_count, "科目数往返一致");
  CHECK(loaded.cell_count == gb->cell_count, "成绩条数往返一致（缺考不落库）");

  const Exam *exam = gb_active_exam(&loaded);
  CHECK(exam && near(gb_get_score(&loaded, exam->id, loaded.students[0].id, loaded.subjects[0].id), 120),
        "成绩值往返一致");

  /* 损坏文件应报错而不崩溃 */
  FILE *bad = fopen("/tmp/gm_test_bad.json", "wb");
  fputs("{ 这不是合法 JSON", bad);
  fclose(bad);
  GradeBook broken;
  gb_init(&broken);
  CHECK(gb_load_file(&broken, "/tmp/gm_test_bad.json", err, sizeof(err)) != 0, "损坏文件应返回失败");
  CHECK(err[0] != '\0', "损坏文件应给出错误说明");
  gb_free(&broken);

  gb_free(&loaded);
  gb_free(gb);
  free(gb);
  remove(path);
  remove("/tmp/gm_test_bad.json");
}

static void test_csv(void) {
  GradeBook *gb = make_fixture();
  const char *csv = "/tmp/gm_test.csv";

  FILE *file = fopen(csv, "wb");
  const unsigned char bom[3] = {0xef, 0xbb, 0xbf};
  fwrite(bom, 1, 3, file);
  fputs("学号,姓名,性别,备注,语文,数学,地理\n", file);
  fputs("004,赵一鸣,男,\"走读, 需乘车\",130,88,70\n", file);
  fputs("005,孙悦宁,女,,,95,\n", file);
  fputs(",,空行应跳过,,,\n", file);
  fclose(file);

  int added = 0, merged = 0;
  char err[256] = "";
  CHECK(gb_import_csv(gb, csv, 1, &added, &merged, err, sizeof(err)) == 0, "CSV 导入成功（%s）", err);
  CHECK(added == 2, "新增 2 名学生，实际 %d", added);
  CHECK(gb->student_count == 5, "学生总数应为 5，实际 %d", gb->student_count);

  const Student *zhao = NULL;
  for (int i = 0; i < gb->student_count; i++)
    if (strcmp(gb->students[i].name, "赵一鸣") == 0) zhao = &gb->students[i];
  CHECK(zhao && strcmp(zhao->note, "走读, 需乘车") == 0, "引号内的逗号不应拆分单元格，实际 [%s]",
        zhao ? zhao->note : "");
  CHECK(zhao && strcmp(zhao->sid, "004") == 0, "学号读取");

  const Exam *exam = gb_active_exam(gb);
  CHECK(zhao && exam && near(gb_get_score(gb, exam->id, zhao->id, gb->subjects[0].id), 130), "成绩列写入");
  /* 数学列：原有 2 条（90/90）+ 赵一鸣 88 + 孙悦宁 95 = 4 条 */
  CHECK(gb_count_subject_cells(gb, gb->subjects[1].id) == 4, "数学列已录条数，实际 %d",
        gb_count_subject_cells(gb, gb->subjects[1].id));

  /* 重复导入应走合并而非重复新增 */
  int added2 = 0, merged2 = 0;
  gb_import_csv(gb, csv, 0, &added2, &merged2, err, sizeof(err));
  CHECK(added2 == 0 && merged2 == 2, "重复导入应合并，实际新增 %d 合并 %d", added2, merged2);
  CHECK(gb->student_count == 5, "重复导入不应增加学生数");

  /* 缺少姓名列应报错 */
  FILE *bad = fopen("/tmp/gm_test_bad.csv", "wb");
  fputs("学号,分数\n001,90\n", bad);
  fclose(bad);
  CHECK(gb_import_csv(gb, "/tmp/gm_test_bad.csv", 0, NULL, NULL, err, sizeof(err)) != 0, "缺姓名列应失败");
  CHECK(strstr(err, "姓名") != NULL, "错误信息应点明缺少姓名列");

  /* 导出 */
  const char *out = "/tmp/gm_test_out.csv";
  CHECK(gb_export_csv(gb, out, 0, err, sizeof(err)) == 0, "CSV 导出成功（%s）", err);

  FILE *check = fopen(out, "rb");
  CHECK(check != NULL, "导出文件应存在");
  if (check) {
    unsigned char head[3] = {0};
    size_t got = fread(head, 1, 3, check);
    CHECK(got == 3 && head[0] == 0xef && head[1] == 0xbb && head[2] == 0xbf, "导出应带 UTF-8 BOM");
    fclose(check);
  }

  gb_free(gb);
  free(gb);
  remove(csv);
  remove("/tmp/gm_test_bad.csv");
  remove(out);
}

int main(void) {
  printf("逻辑层单元测试\n\n");
  test_json_parse();
  test_json_roundtrip();
  test_model();
  test_normalize();
  test_ranking();
  test_trend();
  test_persist_roundtrip();
  test_csv();

  printf("\n%d 通过 / %d 失败\n", passed, failed);
  return failed == 0 ? 0 : 1;
}
