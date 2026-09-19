/**
 * 数据模型的增删改查。只做内存结构操作，不涉及任何格式（JSON / CSV 在别处）。
 */
#include "model.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "compat.h"

#define INIT_CAP 16

static void *grow(void *base, int *cap, int need, size_t elem) {
  if (need <= *cap) return base;
  int next = *cap ? *cap * 2 : INIT_CAP;
  while (next < need) next *= 2;
  void *grown = realloc(base, (size_t)next * elem);
  if (!grown) return NULL;
  *cap = next;
  return grown;
}

static void copy_text(char *dest, size_t destsz, const char *src) {
  if (!src) src = "";
  if (destsz == 0) return;
  snprintf(dest, destsz, "%s", src);
  dest[destsz - 1] = '\0';
}

void gm_new_id(const char *prefix, char *out, size_t outsz) {
  static unsigned long counter = 0;
  counter++;
  snprintf(out, outsz, "%s_%lx%lu", prefix, (unsigned long)time(NULL) & 0xffffffL, counter);
}

void gm_today(char *out, size_t outsz) {
  time_t now = time(NULL);
  struct tm local;
  gm_localtime(&now, &local);
  snprintf(out, outsz, "%04d-%02d-%02d", local.tm_year + 1900, local.tm_mon + 1, local.tm_mday);
}

double gm_round2(double value) {
  return (double)(int)(value * 100.0 + (value >= 0 ? 0.5 : -0.5)) / 100.0;
}

/* ---------- 生命周期 ---------- */

void gb_init(GradeBook *gb) {
  memset(gb, 0, sizeof(*gb));
  copy_text(gb->class_name, sizeof(gb->class_name), "高一(1)班");
}

void gb_free(GradeBook *gb) {
  free(gb->subjects);
  free(gb->students);
  free(gb->exams);
  free(gb->cells);
  memset(gb, 0, sizeof(*gb));
}

/* ---------- 新增 ---------- */

Subject *gb_add_subject(GradeBook *gb, const char *name, int full_mark) {
  void *room = grow(gb->subjects, &gb->subject_cap, gb->subject_count + 1, sizeof(Subject));
  if (!room) return NULL;
  gb->subjects = room;
  Subject *subject = &gb->subjects[gb->subject_count++];
  memset(subject, 0, sizeof(*subject));
  gm_new_id("s", subject->id, sizeof(subject->id));
  copy_text(subject->name, sizeof(subject->name), name);
  subject->full_mark = full_mark > 0 ? full_mark : 100;
  return subject;
}

Student *gb_add_student(GradeBook *gb, const char *sid, const char *name, const char *gender, const char *note) {
  void *room = grow(gb->students, &gb->student_cap, gb->student_count + 1, sizeof(Student));
  if (!room) return NULL;
  gb->students = room;
  Student *student = &gb->students[gb->student_count++];
  memset(student, 0, sizeof(*student));
  gm_new_id("st", student->id, sizeof(student->id));
  copy_text(student->sid, sizeof(student->sid), sid);
  copy_text(student->name, sizeof(student->name), name);
  copy_text(student->gender, sizeof(student->gender), gender);
  copy_text(student->note, sizeof(student->note), note);
  return student;
}

Exam *gb_add_exam(GradeBook *gb, const char *name, const char *date) {
  void *room = grow(gb->exams, &gb->exam_cap, gb->exam_count + 1, sizeof(Exam));
  if (!room) return NULL;
  gb->exams = room;
  Exam *exam = &gb->exams[gb->exam_count++];
  memset(exam, 0, sizeof(*exam));
  gm_new_id("e", exam->id, sizeof(exam->id));
  copy_text(exam->name, sizeof(exam->name), name);
  if (date && date[0]) copy_text(exam->date, sizeof(exam->date), date);
  else gm_today(exam->date, sizeof(exam->date));
  /* 第一场考试自动成为当前考试，避免录入成绩时无处可写 */
  if (gb->exam_count == 1 || gb->active_exam_id[0] == '\0') {
    copy_text(gb->active_exam_id, sizeof(gb->active_exam_id), exam->id);
  }
  return exam;
}

/* ---------- 成绩 ---------- */

static int find_cell(const GradeBook *gb, const char *exam_id, const char *student_id, const char *subject_id) {
  for (int i = 0; i < gb->cell_count; i++) {
    const ScoreCell *cell = &gb->cells[i];
    if (strcmp(cell->exam_id, exam_id) == 0 &&
        strcmp(cell->student_id, student_id) == 0 &&
        strcmp(cell->subject_id, subject_id) == 0)
      return i;
  }
  return -1;
}

void gb_set_score(GradeBook *gb, const char *exam_id, const char *student_id, const char *subject_id, double value) {
  int at = find_cell(gb, exam_id, student_id, subject_id);
  if (at >= 0) {
    gb->cells[at].value = value;
    return;
  }
  void *room = grow(gb->cells, &gb->cell_cap, gb->cell_count + 1, sizeof(ScoreCell));
  if (!room) return;
  gb->cells = room;
  ScoreCell *cell = &gb->cells[gb->cell_count++];
  memset(cell, 0, sizeof(*cell));
  copy_text(cell->exam_id, sizeof(cell->exam_id), exam_id);
  copy_text(cell->student_id, sizeof(cell->student_id), student_id);
  copy_text(cell->subject_id, sizeof(cell->subject_id), subject_id);
  cell->value = value;
}

void gb_clear_score(GradeBook *gb, const char *exam_id, const char *student_id, const char *subject_id) {
  int at = find_cell(gb, exam_id, student_id, subject_id);
  if (at < 0) return;
  gb->cells[at] = gb->cells[gb->cell_count - 1];
  gb->cell_count--;
}

double gb_get_score(const GradeBook *gb, const char *exam_id, const char *student_id, const char *subject_id) {
  int at = find_cell(gb, exam_id, student_id, subject_id);
  return at < 0 ? -1.0 : gb->cells[at].value;
}

/* ---------- 删除（连带清理引用） ---------- */

static void drop_cells(GradeBook *gb, int field, const char *id) {
  int write = 0;
  for (int read = 0; read < gb->cell_count; read++) {
    const char *value = field == 0 ? gb->cells[read].exam_id
                      : field == 1 ? gb->cells[read].student_id
                                   : gb->cells[read].subject_id;
    if (strcmp(value, id) == 0) continue;
    gb->cells[write++] = gb->cells[read];
  }
  gb->cell_count = write;
}

void gb_remove_student(GradeBook *gb, const char *id) {
  int write = 0;
  for (int read = 0; read < gb->student_count; read++) {
    if (strcmp(gb->students[read].id, id) == 0) continue;
    gb->students[write++] = gb->students[read];
  }
  gb->student_count = write;
  drop_cells(gb, 1, id);
}

void gb_remove_subject(GradeBook *gb, const char *id) {
  int write = 0;
  for (int read = 0; read < gb->subject_count; read++) {
    if (strcmp(gb->subjects[read].id, id) == 0) continue;
    gb->subjects[write++] = gb->subjects[read];
  }
  gb->subject_count = write;
  drop_cells(gb, 2, id);

  for (int i = 0; i < gb->exam_count; i++) {
    Exam *exam = &gb->exams[i];
    int kept = 0;
    for (int j = 0; j < exam->subject_count; j++) {
      if (strcmp(exam->subject_ids[j], id) == 0) continue;
      if (kept != j) memcpy(exam->subject_ids[kept], exam->subject_ids[j], GM_ID_LEN);
      kept++;
    }
    exam->subject_count = kept;
  }
}

void gb_remove_exam(GradeBook *gb, const char *id) {
  int write = 0;
  for (int read = 0; read < gb->exam_count; read++) {
    if (strcmp(gb->exams[read].id, id) == 0) continue;
    gb->exams[write++] = gb->exams[read];
  }
  gb->exam_count = write;
  drop_cells(gb, 0, id);
  if (strcmp(gb->active_exam_id, id) == 0) {
    if (gb->exam_count > 0) copy_text(gb->active_exam_id, sizeof(gb->active_exam_id), gb->exams[0].id);
    else gb->active_exam_id[0] = '\0';
  }
}

/* ---------- 查询 ---------- */

const Subject *gb_find_subject(const GradeBook *gb, const char *id) {
  for (int i = 0; i < gb->subject_count; i++)
    if (strcmp(gb->subjects[i].id, id) == 0) return &gb->subjects[i];
  return NULL;
}

const Student *gb_find_student(const GradeBook *gb, const char *id) {
  for (int i = 0; i < gb->student_count; i++)
    if (strcmp(gb->students[i].id, id) == 0) return &gb->students[i];
  return NULL;
}

const Exam *gb_find_exam(const GradeBook *gb, const char *id) {
  for (int i = 0; i < gb->exam_count; i++)
    if (strcmp(gb->exams[i].id, id) == 0) return &gb->exams[i];
  return NULL;
}

const Exam *gb_active_exam(const GradeBook *gb) {
  const Exam *exam = gb_find_exam(gb, gb->active_exam_id);
  return exam ? exam : (gb->exam_count > 0 ? &gb->exams[0] : NULL);
}

int gb_count_subject_cells(const GradeBook *gb, const char *subject_id) {
  int total = 0;
  for (int i = 0; i < gb->cell_count; i++)
    if (strcmp(gb->cells[i].subject_id, subject_id) == 0) total++;
  return total;
}

int gb_count_exam_cells(const GradeBook *gb, const char *exam_id) {
  int total = 0;
  for (int i = 0; i < gb->cell_count; i++)
    if (strcmp(gb->cells[i].exam_id, exam_id) == 0) total++;
  return total;
}
