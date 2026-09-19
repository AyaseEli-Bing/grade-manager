/**
 * JSON 树 ⇄ GradeBook 的转换与载入清洗。文件读写见 persist.c。
 *
 * 载入清洗规则：剔除空名科目与学生、剔除指向不存在对象的成绩单元格、
 * activeExamId 非法时回退到首场考试。
 */
/**
 * 持久化：JSON 树 ⇄ GradeBook，以及文件读写。
 *
 * 载入时做清洗：剔除空名科目与学生、剔除指向不存在对象的成绩单元格、
 * activeExamId 非法时回退到第一场考试。任何异常输入都降级为「空数据 + 错误提示」。
 */
#include "model.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "compat.h"

/* ---------- 载入 ---------- */

static void copy_field(char *dest, size_t destsz, const Json *object, const char *key, const char *fallback) {
  const char *value = json_get_str(object, key, fallback);
  if (!value) value = "";
  snprintf(dest, destsz, "%s", value);
  dest[destsz - 1] = '\0';
}

static void load_subjects(GradeBook *gb, const Json *array) {
  if (!array || array->type != JSON_ARRAY) return;
  for (int i = 0; i < array->count; i++) {
    const Json *item = array->items[i];
    const char *name = json_get_str(item, "name", "");
    if (name[0] == '\0') continue; /* 空名科目直接丢弃 */
    double mark = json_get_num(item, "fullMark", 0);
    int full = (int)mark;
    if (full <= 0) full = 100;

    Subject *subject = gb_add_subject(gb, name, full);
    if (!subject) return;
    const char *id = json_get_str(item, "id", NULL);
    if (id && id[0]) snprintf(subject->id, sizeof(subject->id), "%s", id);
  }
}

static void load_students(GradeBook *gb, const Json *array) {
  if (!array || array->type != JSON_ARRAY) return;
  for (int i = 0; i < array->count; i++) {
    const Json *item = array->items[i];
    const char *name = json_get_str(item, "name", "");
    if (name[0] == '\0') continue;

    const char *sid = json_get_str(item, "sid", "");
    const char *gender = json_get_str(item, "gender", "");
    const char *note = json_get_str(item, "note", "");
    Student *student = gb_add_student(gb, sid, name, gender, note);
    if (!student) return;
    const char *id = json_get_str(item, "id", NULL);
    if (id && id[0]) snprintf(student->id, sizeof(student->id), "%s", id);
  }
}

static void load_exams(GradeBook *gb, const Json *array) {
  if (!array || array->type != JSON_ARRAY) return;
  for (int i = 0; i < array->count; i++) {
    const Json *item = array->items[i];
    const char *name = json_get_str(item, "name", "");
    if (name[0] == '\0') continue;

    Exam *exam = gb_add_exam(gb, name, json_get_str(item, "date", ""));
    if (!exam) return;
    const char *id = json_get_str(item, "id", NULL);
    if (id && id[0]) snprintf(exam->id, sizeof(exam->id), "%s", id);

    const Json *ids = json_get(item, "subjectIds");
    if (!ids || ids->type != JSON_ARRAY) continue;
    for (int j = 0; j < ids->count && exam->subject_count < GM_EXAM_SUBJ_MAX; j++) {
      const char *subject_id = json_str(ids->items[j], NULL);
      if (!subject_id || !gb_find_subject(gb, subject_id)) continue; /* 未知科目引用丢弃 */
      snprintf(exam->subject_ids[exam->subject_count], GM_ID_LEN, "%s", subject_id);
      exam->subject_count++;
    }
  }
}

/** 成绩必须在学生、科目、考试三者都存在时才保留 */
static void load_scores(GradeBook *gb, const Json *object) {
  if (!object || object->type != JSON_OBJECT) return;
  for (int e = 0; e < object->count; e++) {
    const char *exam_id = object->keys[e];
    if (!gb_find_exam(gb, exam_id)) continue;
    const Json *by_student = object->items[e];
    if (!by_student || by_student->type != JSON_OBJECT) continue;

    for (int s = 0; s < by_student->count; s++) {
      const char *student_id = by_student->keys[s];
      if (!gb_find_student(gb, student_id)) continue;
      const Json *row = by_student->items[s];
      if (!row || row->type != JSON_OBJECT) continue;

      for (int c = 0; c < row->count; c++) {
        const char *subject_id = row->keys[c];
        if (!gb_find_subject(gb, subject_id)) continue;
        const Json *value = row->items[c];
        if (!value || value->type != JSON_NUMBER) continue; /* null 表示缺考，不落库 */
        gb_set_score(gb, exam_id, student_id, subject_id, value->num);
      }
    }
  }
}

int gb_load_json(GradeBook *gb, const Json *root) {
  if (!gb || !root || root->type != JSON_OBJECT) return -1;
  gb_init(gb);

  copy_field(gb->class_name, sizeof(gb->class_name), root, "className", "");
  if (gb->class_name[0] == '\0') snprintf(gb->class_name, sizeof(gb->class_name), "高一(1)班");
  load_subjects(gb, json_get(root, "subjects"));
  load_students(gb, json_get(root, "students"));
  load_exams(gb, json_get(root, "exams"));
  load_scores(gb, json_get(root, "scores"));

  const char *active = json_get_str(root, "activeExamId", "");
  if (active[0] && gb_find_exam(gb, active)) {
    snprintf(gb->active_exam_id, sizeof(gb->active_exam_id), "%s", active);
  } else if (gb->exam_count > 0) {
    snprintf(gb->active_exam_id, sizeof(gb->active_exam_id), "%s", gb->exams[0].id);
  }
  return 0;
}

/* ---------- 导出 ---------- */

Json *gb_to_json(const GradeBook *gb) {
  if (!gb) return NULL;
  Json *root = json_new_object();
  if (!root) return NULL;

  json_set(root, "version", json_new_number(1));
  json_set(root, "className", json_new_string(gb->class_name));

  Json *subjects = json_new_array();
  for (int i = 0; i < gb->subject_count; i++) {
    Json *item = json_new_object();
    json_set(item, "id", json_new_string(gb->subjects[i].id));
    json_set(item, "name", json_new_string(gb->subjects[i].name));
    json_set(item, "fullMark", json_new_number((double)gb->subjects[i].full_mark));
    json_push(subjects, item);
  }
  json_set(root, "subjects", subjects);

  Json *students = json_new_array();
  for (int i = 0; i < gb->student_count; i++) {
    Json *item = json_new_object();
    json_set(item, "id", json_new_string(gb->students[i].id));
    json_set(item, "sid", json_new_string(gb->students[i].sid));
    json_set(item, "name", json_new_string(gb->students[i].name));
    json_set(item, "gender", json_new_string(gb->students[i].gender));
    json_set(item, "note", json_new_string(gb->students[i].note));
    json_push(students, item);
  }
  json_set(root, "students", students);

  Json *exams = json_new_array();
  for (int i = 0; i < gb->exam_count; i++) {
    Json *item = json_new_object();
    json_set(item, "id", json_new_string(gb->exams[i].id));
    json_set(item, "name", json_new_string(gb->exams[i].name));
    json_set(item, "date", json_new_string(gb->exams[i].date));
    Json *ids = json_new_array();
    for (int j = 0; j < gb->exams[i].subject_count; j++) json_push(ids, json_new_string(gb->exams[i].subject_ids[j]));
    json_set(item, "subjectIds", ids);
    json_push(exams, item);
  }
  json_set(root, "exams", exams);

  Json *scores = json_new_object();
  for (int e = 0; e < gb->exam_count; e++) {
    Json *by_student = json_new_object();
    for (int s = 0; s < gb->student_count; s++) {
      Json *row = json_new_object();
      int has_any = 0;
      for (int c = 0; c < gb->cell_count; c++) {
        const ScoreCell *cell = &gb->cells[c];
        if (strcmp(cell->exam_id, gb->exams[e].id) != 0) continue;
        if (strcmp(cell->student_id, gb->students[s].id) != 0) continue;
        json_set(row, cell->subject_id, json_new_number(cell->value));
        has_any = 1;
      }
      if (has_any) json_set(by_student, gb->students[s].id, row);
      else json_free(row);
    }
    json_set(scores, gb->exams[e].id, by_student);
  }
  json_set(root, "scores", scores);
  json_set(root, "activeExamId", json_new_string(gb->active_exam_id));

  time_t now = time(NULL);
  struct tm utc;
  gm_gmtime(&now, &utc);
  char stamp[40];
  snprintf(stamp, sizeof(stamp), "%04d-%02d-%02dT%02d:%02d:%02d.000Z",
           utc.tm_year + 1900, utc.tm_mon + 1, utc.tm_mday, utc.tm_hour, utc.tm_min, utc.tm_sec);
  json_set(root, "updatedAt", json_new_string(stamp));

  return root;
}

