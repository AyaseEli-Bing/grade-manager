/**
 * 视图：统计排名 —— 按学生排名与按科目统计两个模式（Tab 切换）。
 */
#include "views.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "calc.h"
#include "ui.h"

void stats_student_detail(AppCtx *ctx, const Exam *exam, const Student *student);
void stats_subject_detail(GradeBook *gb, const Exam *exam, const Subject *subject);

typedef struct {
  AppCtx *app;
  const Exam *exam;
  RankRow *rows;
  int nrows;
  int mode; /* 0 按学生，1 按科目 */
  int *index;
  int count;
} StatsCtx;

static void rebuild(StatsCtx *st) {
  free(st->index);
  int capacity = st->mode == 0 ? st->nrows : st->app->gb->subject_count;
  if (capacity < 1) capacity = 1;
  st->index = malloc(sizeof(int) * (size_t)capacity);
  st->count = 0;
  if (!st->index) return;

  const char *keyword = st->app->search;
  if (st->mode == 0) {
    for (int i = 0; i < st->nrows; i++) {
      const Student *student = st->rows[i].student;
      if (view_match(student->name, keyword) || view_match(student->sid, keyword)) st->index[st->count++] = i;
    }
  } else {
    for (int i = 0; i < st->app->gb->subject_count; i++) {
      if (view_match(st->app->gb->subjects[i].name, keyword)) st->index[st->count++] = i;
    }
  }
}

static void draw_columns(WINDOW *win, int y, void *ctx) {
  StatsCtx *st = ctx;
  int width = getmaxx(win);

  if (st->mode == 0) {
    ui_col(win, y, 1, 4, 1, "名次", UI_HEADER);
    ui_col(win, y, 6, 10, 0, "学号", UI_HEADER);
    ui_col(win, y, 17, 12, 0, "姓名", UI_HEADER);
    int x = 30;
    int nsub = st->app->gb->subject_count;
    int room = width - x - 26;
    int visible = room / 8;
    if (visible > nsub) visible = nsub;
    if (visible < 0) visible = 0;
    for (int i = 0; i < visible; i++) {
      ui_col(win, y, x, 7, 1, st->app->gb->subjects[i].name, UI_HEADER);
      x += 8;
    }
    int right = width - 25;
    ui_col(win, y, right, 7, 1, "总分", UI_HEADER);
    ui_col(win, y, right + 8, 7, 1, "平均分", UI_HEADER);
    ui_col(win, y, right + 16, 8, 1, "得分率", UI_HEADER);
    return;
  }

  ui_col(win, y, 1, 14, 0, "科目", UI_HEADER);
  ui_col(win, y, 16, 8, 1, "满分", UI_HEADER);
  ui_col(win, y, 25, 8, 1, "平均分", UI_HEADER);
  ui_col(win, y, 34, 8, 1, "最高", UI_HEADER);
  ui_col(win, y, 43, 8, 1, "最低", UI_HEADER);
  ui_col(win, y, 52, 9, 1, "及格率", UI_HEADER);
  ui_col(win, y, 62, 9, 1, "优秀率", UI_HEADER);
  ui_col(win, y, 72, 10, 1, "已录", UI_HEADER);
}

static void render_row(WINDOW *win, int y, int index, int selected, void *ctx) {
  StatsCtx *st = ctx;
  int width = getmaxx(win);
  int role = selected ? UI_SELECTED : UI_TEXT;
  int dim = selected ? UI_SELECTED : UI_DIM;

  wattron(win, ui_role(role));
  mvwhline(win, y, 0, ' ', width);
  wattroff(win, ui_role(role));

  char buf[32];

  if (st->mode == 0) {
    const RankRow *row = &st->rows[st->index[index]];
    snprintf(buf, sizeof(buf), "%d", row->rank);
    int rank_role = selected ? UI_SELECTED
                   : row->counted == 0 ? UI_DIM
                   : row->rank <= 3 ? UI_WARN
                                    : UI_TEXT;
    ui_col(win, y, 1, 4, 1, buf, rank_role);
    ui_col(win, y, 6, 10, 0, row->student->sid[0] ? row->student->sid : "-", dim);
    ui_col(win, y, 17, 12, 0, row->student->name, role);

    int x = 30;
    int nsub = st->app->gb->subject_count;
    int room = width - x - 26;
    int visible = room / 8;
    if (visible > nsub) visible = nsub;
    if (visible < 0) visible = 0;
    for (int i = 0; i < visible; i++) {
      double value = row->scores[i];
      if (value < 0) {
        ui_col(win, y, x, 7, 1, "-", dim);
      } else {
        snprintf(buf, sizeof(buf), "%.0f", value);
        ui_col(win, y, x, 7, 1, buf, role);
      }
      x += 8;
    }

    int right = width - 25;
    snprintf(buf, sizeof(buf), "%.0f", row->total);
    ui_col(win, y, right, 7, 1, row->counted ? buf : "-", role);
    snprintf(buf, sizeof(buf), "%.1f", row->average);
    ui_col(win, y, right + 8, 7, 1, row->counted ? buf : "-", role);
    snprintf(buf, sizeof(buf), "%.1f%%", row->rate);
    ui_col(win, y, right + 16, 8, 1, row->counted ? buf : "-", role);
    return;
  }

  const Subject *subject = &st->app->gb->subjects[st->index[index]];
  SubjectStats stats;
  calc_subject_stats(st->app->gb, st->exam, subject->id, &stats);

  ui_col(win, y, 1, 14, 0, subject->name, role);
  snprintf(buf, sizeof(buf), "%d", subject->full_mark);
  ui_col(win, y, 16, 8, 1, buf, dim);
  snprintf(buf, sizeof(buf), "%.1f", stats.average);
  ui_col(win, y, 25, 8, 1, stats.count ? buf : "-", role);
  snprintf(buf, sizeof(buf), "%.0f", stats.max);
  ui_col(win, y, 34, 8, 1, stats.count ? buf : "-", role);
  snprintf(buf, sizeof(buf), "%.0f", stats.min);
  ui_col(win, y, 43, 8, 1, stats.count ? buf : "-", role);
  snprintf(buf, sizeof(buf), "%.1f%%", stats.pass_rate);
  ui_col(win, y, 52, 9, 1, stats.count ? buf : "-", stats.pass_rate >= 60 ? UI_SUCCESS : UI_DANGER);
  snprintf(buf, sizeof(buf), "%.1f%%", stats.excellent_rate);
  ui_col(win, y, 62, 9, 1, stats.count ? buf : "-", role);
  snprintf(buf, sizeof(buf), "%d/%d", stats.count, st->nrows);
  ui_col(win, y, 72, 10, 1, buf, dim);
}

static ListAction on_key(int key, int *index, void *ctx) {
  StatsCtx *st = ctx;

  if (key == '\t') {
    st->mode = st->mode == 0 ? 1 : 0;
    st->app->search[0] = '\0';
    rebuild(st);
    *index = 0;
    return LIST_REFRESH;
  }
  if (key == '/' || key == 'f') {
    char keyword[64];
    snprintf(keyword, sizeof(keyword), "%s", st->app->search);
    if (ui_input("筛选", st->mode == 0 ? "按学号或姓名筛选：" : "按科目名筛选：", keyword, sizeof(keyword))) {
      snprintf(st->app->search, sizeof(st->app->search), "%s", keyword);
      rebuild(st);
      *index = 0;
    }
    return LIST_REFRESH;
  }
  if (st->count == 0) return LIST_REFRESH;

  if (key == 'e' || key == ' ') {
    if (st->mode == 0) stats_student_detail(st->app, st->exam, st->rows[st->index[*index]].student);
    else stats_subject_detail(st->app->gb, st->exam, &st->app->gb->subjects[st->index[*index]]);
    return LIST_REFRESH;
  }
  if (key == '\n' || key == KEY_ENTER) return LIST_DONE;
  return LIST_CONTINUE;
}

void view_stats(AppCtx *ctx) {
  const Exam *exam = gb_active_exam(ctx->gb);
  if (!exam) {
    ui_error("还没有任何考试，请先到「考试管理」创建一场。");
    return;
  }
  if (ctx->gb->student_count == 0) {
    ui_error("还没有学生，请先到「学生管理」添加或从 CSV 导入，之后这里会自动生成排名与统计。");
    return;
  }
  if (ctx->gb->subject_count == 0) {
    ui_error("还没有科目，请先到「科目管理」添加。");
    return;
  }

  StatsCtx st;
  memset(&st, 0, sizeof(st));
  st.app = ctx;
  st.exam = exam;
  st.nrows = calc_ranking(ctx->gb, exam, &st.rows);
  if (!st.rows) return;

  rebuild(&st);

  double graded = 0, sum_avg = 0, top = 0;
  for (int i = 0; i < st.nrows; i++) {
    if (st.rows[i].counted == 0) continue;
    graded++;
    sum_avg += st.rows[i].average;
    if (st.rows[i].total > top) top = st.rows[i].total;
  }
  double class_avg = graded > 0 ? sum_avg / graded : 0.0;
  double pass_rate = calc_class_pass_rate(ctx->gb, exam, st.rows, st.nrows);

  int cursor = 0;  /* 重新进入列表时续用光标位置 */

  for (;;) {
    char subtitle[160];
    snprintf(subtitle, sizeof(subtitle),
             "%s · 参考 %d 人 · 全班平均 %.1f · 最高总分 %.0f · 及格率 %.1f%%",
             st.mode == 0 ? "按学生排名" : "按科目统计", st.nrows, class_avg, top, pass_rate);

    int picked = ui_list("统计排名  [Tab]切换视角 [Enter]详情 [/]筛选 [q]返回",
                         subtitle,
                         st.count, cursor, draw_columns, render_row, on_key, &st);
    if (picked == UI_LIST_REFRESH) { cursor = ui_list_last_index(); continue; }
    if (picked < 0 || picked >= st.count) break;
    cursor = picked;
    if (st.mode == 0) stats_student_detail(ctx, exam, st.rows[st.index[picked]].student);
    else stats_subject_detail(ctx->gb, exam, &ctx->gb->subjects[st.index[picked]]);
  }

  calc_free_ranking(st.rows, st.nrows);
  free(st.index);
}
