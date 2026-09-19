/**
 * 数据模型 —— 班级 / 科目 / 学生 / 考试 / 成绩。
 *
 * 成绩采用「扁平的稀疏单元格数组」而非嵌套映射：
 * 一条记录 = 一场考试 + 一名学生 + 一个科目 + 一个分数。
 * 缺考不存记录（而不是存 0 分），这样总分与平均分天然不会被引偏。
 */
#ifndef GM_MODEL_H
#define GM_MODEL_H

#include <stddef.h>
#include "json.h"

#define GM_ID_LEN 24
#define GM_NAME_LEN 64
#define GM_NOTE_LEN 128
#define GM_DATE_LEN 16
#define GM_SID_LEN 32
#define GM_EXAM_SUBJ_MAX 32

typedef struct {
  char id[GM_ID_LEN];
  char name[GM_NAME_LEN];
  int full_mark;
} Subject;

typedef struct {
  char id[GM_ID_LEN];
  char sid[GM_SID_LEN];
  char name[GM_NAME_LEN];
  char gender[8];
  char note[GM_NOTE_LEN];
} Student;

typedef struct {
  char id[GM_ID_LEN];
  char name[GM_NAME_LEN];
  char date[GM_DATE_LEN];
  char subject_ids[GM_EXAM_SUBJ_MAX][GM_ID_LEN];
  int subject_count; /* 0 表示考全部科目 */
} Exam;

typedef struct {
  char exam_id[GM_ID_LEN];
  char student_id[GM_ID_LEN];
  char subject_id[GM_ID_LEN];
  double value;
} ScoreCell;

typedef struct {
  char class_name[GM_NAME_LEN];
  Subject *subjects;
  int subject_count, subject_cap;
  Student *students;
  int student_count, student_cap;
  Exam *exams;
  int exam_count, exam_cap;
  ScoreCell *cells;
  int cell_count, cell_cap;
  char active_exam_id[GM_ID_LEN];
} GradeBook;

/* ---------- 生命周期 ---------- */

void gb_init(GradeBook *gb);
void gb_free(GradeBook *gb);

/* ---------- 增删改 ---------- */

Subject *gb_add_subject(GradeBook *gb, const char *name, int full_mark);
Student *gb_add_student(GradeBook *gb, const char *sid, const char *name, const char *gender, const char *note);
Exam *gb_add_exam(GradeBook *gb, const char *name, const char *date);

/** 写入成绩；value 非法（<0 或超出满分）时由调用方负责钳制 */
void gb_set_score(GradeBook *gb, const char *exam_id, const char *student_id, const char *subject_id, double value);
/** 删除一条成绩，等价于标记为缺考 */
void gb_clear_score(GradeBook *gb, const char *exam_id, const char *student_id, const char *subject_id);
/** 取成绩；返回 -1 表示缺考 */
double gb_get_score(const GradeBook *gb, const char *exam_id, const char *student_id, const char *subject_id);

/** 删除学生 / 科目 / 考试，并连带清除其所有成绩记录 */
void gb_remove_student(GradeBook *gb, const char *id);
void gb_remove_subject(GradeBook *gb, const char *id);
void gb_remove_exam(GradeBook *gb, const char *id);

/* ---------- 查询 ---------- */

const Subject *gb_find_subject(const GradeBook *gb, const char *id);
const Student *gb_find_student(const GradeBook *gb, const char *id);
const Exam *gb_find_exam(const GradeBook *gb, const char *id);
const Exam *gb_active_exam(const GradeBook *gb);
int gb_count_subject_cells(const GradeBook *gb, const char *subject_id);
int gb_count_exam_cells(const GradeBook *gb, const char *exam_id);

/* ---------- 持久化 ---------- */

/** 从 JSON 树载入；悬空引用（成绩指向不存在的学生/科目/考试）会被自动剔除 */
int gb_load_json(GradeBook *gb, const Json *root);
Json *gb_to_json(const GradeBook *gb);

/** 文件读写。成功返回 0；失败返回非 0 并写入 err */
int gb_load_file(GradeBook *gb, const char *path, char *err, size_t errsz);
int gb_save_file(const GradeBook *gb, const char *path, char *err, size_t errsz);

/**
 * 导入 CSV 名单。表头识别「学号/姓名/性别/备注」以及与现有科目同名的列。
 * write_scores=1 时把成绩列写入当前考试。added / merged 可为 NULL。
 */
int gb_import_csv(GradeBook *gb, const char *path, int write_scores, int *added, int *merged, char *err, size_t errsz);

/** 导出成绩表 CSV（带 UTF-8 BOM，Excel 直接打开不乱码）。all_exams=1 时导出每场考试 */
int gb_export_csv(const GradeBook *gb, const char *path, int all_exams, char *err, size_t errsz);

/* ---------- 工具 ---------- */

void gm_new_id(const char *prefix, char *out, size_t outsz);
void gm_today(char *out, size_t outsz);
double gm_round2(double value);

#endif /* GM_MODEL_H */
