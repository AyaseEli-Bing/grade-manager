/**
 * 纯计算层：总分 / 平均分 / 排名 / 单科统计 / 录入进度 / 趋势。
 *
 * 两条核心业务规则（与图形版一致）：
 *   1. 缺考不按 0 分计入总分，也不拉低平均分
 *   2. 同分同名次并跳号（1, 1, 3）
 */
#include "calc.h"

#include <stdlib.h>
#include <string.h>

int calc_exam_subjects(const GradeBook *gb, const Exam *exam, const Subject **out, int max) {
  if (!gb || !exam || !out || max <= 0) return 0;

  int count = 0;
  if (exam->subject_count == 0) {
    for (int i = 0; i < gb->subject_count && count < max; i++) out[count++] = &gb->subjects[i];
    return count;
  }
  for (int i = 0; i < exam->subject_count && count < max; i++) {
    const Subject *subject = gb_find_subject(gb, exam->subject_ids[i]);
    if (subject) out[count++] = subject;
  }
  return count;
}

static int full_mark_sum(const Subject **subs, int n) {
  int sum = 0;
  for (int i = 0; i < n; i++) sum += subs[i]->full_mark;
  return sum;
}

static int rank_cmp(const void *lhs, const void *rhs) {
  const RankRow *a = lhs;
  const RankRow *b = rhs;
  if (a->total > b->total) return -1;
  if (a->total < b->total) return 1;
  return strcoll(a->student->name, b->student->name);
}

int calc_ranking(const GradeBook *gb, const Exam *exam, RankRow **out) {
  if (!gb || !exam || !out) return 0;
  *out = NULL;

  int capacity = gb->subject_count > 0 ? gb->subject_count : 1;
  const Subject **subs = malloc(sizeof(Subject *) * (size_t)capacity);
  if (!subs) return 0;
  int nsub = calc_exam_subjects(gb, exam, subs, gb->subject_count);
  int full = full_mark_sum(subs, nsub);

  int n = gb->student_count;
  RankRow *rows = calloc((size_t)(n > 0 ? n : 1), sizeof(RankRow));
  if (!rows) {
    free(subs);
    return 0;
  }

  for (int i = 0; i < n; i++) {
    const Student *student = &gb->students[i];
    RankRow *row = &rows[i];
    row->student = student;
    row->scores = calloc((size_t)(nsub > 0 ? nsub : 1), sizeof(double));
    if (!row->scores) continue;

    double total = 0;
    for (int j = 0; j < nsub; j++) {
      double value = gb_get_score(gb, exam->id, student->id, subs[j]->id);
      row->scores[j] = value;
      if (value >= 0) {
        total += value;
        row->counted++;
      } else {
        row->missing++;
      }
    }
    row->total = total;
    row->average = row->counted > 0 ? total / row->counted : 0.0;
    row->rate = full > 0 ? total / full * 100.0 : 0.0;
  }

  if (n > 0) qsort(rows, (size_t)n, sizeof(RankRow), rank_cmp);

  /* 同分同名次，后续跳号 */
  for (int i = 0; i < n; i++) {
    if (i > 0 && rows[i].total == rows[i - 1].total) rows[i].rank = rows[i - 1].rank;
    else rows[i].rank = i + 1;
  }

  free(subs);
  *out = rows;
  return n;
}

void calc_free_ranking(RankRow *rows, int n) {
  if (!rows) return;
  for (int i = 0; i < n; i++) free(rows[i].scores);
  free(rows);
}

void calc_subject_stats(const GradeBook *gb, const Exam *exam, const char *subject_id, SubjectStats *out) {
  if (!out) return;
  memset(out, 0, sizeof(*out));
  if (!gb || !exam || !subject_id) return;

  const Subject *subject = gb_find_subject(gb, subject_id);
  int full_mark = subject ? subject->full_mark : 100;
  out->full_mark = full_mark;

  double sum = 0, max = 0, min = 0;
  int passed = 0, excellent = 0;
  const double pass_line = full_mark * 0.6;
  const double excellent_line = full_mark * 0.85;

  for (int i = 0; i < gb->student_count; i++) {
    double value = gb_get_score(gb, exam->id, gb->students[i].id, subject_id);
    if (value < 0) continue;
    if (out->count == 0) {
      max = min = value;
    } else {
      if (value > max) max = value;
      if (value < min) min = value;
    }
    sum += value;
    out->count++;
    if (value >= pass_line) passed++;
    if (value >= excellent_line) excellent++;

    /* 分数段：[0,60%) [60,70%) [70,80%) [80,90%) [90,100%] */
    double ratio = full_mark > 0 ? value / full_mark : 0.0;
    int bucket = 4;
    if (ratio < 0.6) bucket = 0;
    else if (ratio < 0.7) bucket = 1;
    else if (ratio < 0.8) bucket = 2;
    else if (ratio < 0.9) bucket = 3;
    out->dist[bucket]++;
  }

  if (out->count == 0) {
    max = min = 0; /* 无人录分时不留脏值，也避免除零 */
    return;
  }
  out->average = sum / out->count;
  out->max = max;
  out->min = min;
  out->pass_rate = (double)passed / out->count * 100.0;
  out->excellent_rate = (double)excellent / out->count * 100.0;
  for (int i = 0; i < 5; i++) out->dist_ratio[i] = (double)out->dist[i] / out->count;
}

double calc_class_pass_rate(const GradeBook *gb, const Exam *exam, const RankRow *rows, int n) {
  if (!gb || !exam || !rows || n <= 0) return 0.0;

  int capacity = gb->subject_count > 0 ? gb->subject_count : 1;
  const Subject **subs = malloc(sizeof(Subject *) * (size_t)capacity);
  if (!subs) return 0.0;
  int nsub = calc_exam_subjects(gb, exam, subs, gb->subject_count);

  int entered = 0, passed = 0;
  for (int i = 0; i < n; i++) {
    for (int j = 0; j < nsub; j++) {
      if (i >= n || !rows[i].scores) continue;
      double value = rows[i].scores[j];
      if (value < 0) continue;
      entered++;
      if (value >= subs[j]->full_mark * 0.6) passed++;
    }
  }
  free(subs);
  return entered > 0 ? (double)passed / entered * 100.0 : 0.0;
}

void calc_progress(const GradeBook *gb, const Exam *exam, int *total, int *done, double *percent) {
  int capacity = gb && gb->subject_count > 0 ? gb->subject_count : 1;
  const Subject **subs = malloc(sizeof(Subject *) * (size_t)capacity);
  if (!subs) return;
  int nsub = calc_exam_subjects(gb, exam, subs, gb->subject_count);

  int need = gb->student_count * nsub;
  int got = 0;
  for (int i = 0; i < gb->student_count; i++) {
    for (int j = 0; j < nsub; j++) {
      if (gb_get_score(gb, exam->id, gb->students[i].id, subs[j]->id) >= 0) got++;
    }
  }
  free(subs);

  if (total) *total = need;
  if (done) *done = got;
  if (percent) *percent = need > 0 ? (double)got / need * 100.0 : 0.0;
}

void calc_student_trend(const GradeBook *gb, const char *student_id, double *totals, int *ranks, int *n) {
  if (n) *n = 0;
  if (!gb || !totals || !ranks || !n) return;

  int count = gb->exam_count;
  if (count <= 0) return;

  /* 按日期升序排列考试 */
  int *order = malloc(sizeof(int) * (size_t)count);
  if (!order) return;
  for (int i = 0; i < count; i++) order[i] = i;
  for (int i = 1; i < count; i++) {
    int key = order[i];
    int j = i - 1;
    while (j >= 0 && strcmp(gb->exams[order[j]].date, gb->exams[key].date) > 0) {
      order[j + 1] = order[j];
      j--;
    }
    order[j + 1] = key;
  }

  for (int i = 0; i < count; i++) {
    const Exam *exam = &gb->exams[order[i]];
    RankRow *rows = NULL;
    int rows_n = calc_ranking(gb, exam, &rows);

    double total = 0;
    int rank = 0;
    for (int k = 0; k < rows_n; k++) {
      if (rows[k].student && strcmp(rows[k].student->id, student_id) == 0) {
        total = rows[k].total;
        rank = rows[k].rank;
        break;
      }
    }
    calc_free_ranking(rows, rows_n);

    totals[i] = total;
    ranks[i] = rank;
  }

  free(order);
  *n = count;
}

int calc_rank_delta(int current_rank, int previous_rank) {
  if (current_rank <= 0 || previous_rank <= 0) return 0;
  return previous_rank - current_rank; /* 名次数字变小即为进步 */
}
