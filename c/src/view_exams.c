/**
 * 视图：考试管理 —— 新增 / 重命名 / 删除考试，指定参考科目，切换当前考试。
 */
#include "views.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ui.h"

typedef struct {
  AppCtx *app;
  int *index;
  int count;
} ExamsCtx;

static void rebuild(ExamsCtx *ec) {
  GradeBook *gb = ec->app->gb;
  free(ec->index);
  ec->index = malloc(sizeof(int) * (size_t)(gb->exam_count > 0 ? gb->exam_count : 1));
  ec->count = 0;
  if (!ec->index) return;
  for (int i = 0; i < gb->exam_count; i++) {
    if (gb->exams[i].id[0]) ec->index[ec->count++] = i;
  }
}

static void describe_subjects(const GradeBook *gb, const Exam *exam, char *out, size_t outsz) {
  if (exam->subject_count == 0) {
    snprintf(out, outsz, "全部 %d 科", gb->subject_count);
    return;
  }
  size_t used = 0;
  out[0] = '\0';
  for (int i = 0; i < exam->subject_count && used + 1 < outsz; i++) {
    const Subject *subject = gb_find_subject(gb, exam->subject_ids[i]);
    if (!subject) continue;
    int written = snprintf(out + used, outsz - used, "%s%s", i ? "、" : "", subject->name);
    if (written > 0) used += (size_t)written;
  }
  if (used == 0) snprintf(out, outsz, "未指定");
}

static void draw_columns(WINDOW *win, int y, void *ctx) {
  (void)ctx;
  int width = getmaxx(win);
  ui_col(win, y, 1, 4, 1, "#", UI_HEADER);
  ui_col(win, y, 6, 20, 0, "考试名称", UI_HEADER);
  ui_col(win, y, 27, 12, 0, "日期", UI_HEADER);
  ui_col(win, y, 40, 8, 1, "科目数", UI_HEADER);
  ui_col(win, y, 49, 10, 1, "成绩条数", UI_HEADER);
  ui_col(win, y, 60, width - 61, 0, "参考科目", UI_HEADER);
}

static void render_row(WINDOW *win, int y, int index, int selected, void *ctx) {
  ExamsCtx *ec = ctx;
  GradeBook *gb = ec->app->gb;
  const Exam *exam = &gb->exams[ec->index[index]];
  int role = selected ? UI_SELECTED : UI_TEXT;
  int dim = selected ? UI_SELECTED : UI_DIM;
  int width = getmaxx(win);
  int is_current = strcmp(exam->id, gb->active_exam_id) == 0;

  wattron(win, ui_role(role));
  mvwhline(win, y, 0, ' ', width);
  wattroff(win, ui_role(role));

  char buf[64];
  snprintf(buf, sizeof(buf), "%d", index + 1);
  ui_col(win, y, 1, 4, 1, buf, dim);

  snprintf(buf, sizeof(buf), "%s%s", exam->name, is_current ? " ←当前" : "");
  ui_col(win, y, 6, 20, 0, buf, is_current ? UI_ACCENT : role);
  ui_col(win, y, 27, 12, 0, exam->date, dim);

  snprintf(buf, sizeof(buf), "%d", exam->subject_count ? exam->subject_count : gb->subject_count);
  ui_col(win, y, 40, 8, 1, buf, role);
  snprintf(buf, sizeof(buf), "%d", gb_count_exam_cells(gb, exam->id));
  ui_col(win, y, 49, 10, 1, buf, dim);

  char desc[256];
  describe_subjects(gb, exam, desc, sizeof(desc));
  ui_col(win, y, 60, width - 61, 0, desc, dim);
}

static void edit_exam(AppCtx *ctx, int exam_index, int is_new) {
  static const char *labels[] = {"考试名称（必填）", "考试日期（YYYY-MM-DD）"};
  char name[GM_NAME_LEN] = "";
  char date[GM_DATE_LEN] = "";

  if (!is_new) {
    const Exam *exam = &ctx->gb->exams[exam_index];
    snprintf(name, sizeof(name), "%s", exam->name);
    snprintf(date, sizeof(date), "%s", exam->date);
  } else {
    gm_today(date, sizeof(date));
  }

  char *fields[] = {name, date};
  const size_t widths[] = {sizeof(name), sizeof(date)};
  if (!ui_form(is_new ? "新增考试" : "编辑考试", labels, fields, widths, 2)) return;

  if (name[0] == '\0') {
    ui_error("考试名称不能为空。");
    return;
  }
  if (date[0] && strlen(date) != 10) {
    ui_error("日期格式应为 YYYY-MM-DD，例如 2026-04-22。留空则用今天。");
    return;
  }

  if (is_new) {
    Exam *exam = gb_add_exam(ctx->gb, name, date);
    if (!exam) {
      ui_error("内存不足，新增考试失败。");
      return;
    }
    snprintf(ctx->gb->active_exam_id, sizeof(ctx->gb->active_exam_id), "%s", exam->id);
    ctx->dirty = 1;
    view_status(ctx, "已新增考试并设为当前");
    return;
  }

  Exam *exam = &ctx->gb->exams[exam_index];
  snprintf(exam->name, sizeof(exam->name), "%s", name);
  snprintf(exam->date, sizeof(exam->date), "%s", date);
  ctx->dirty = 1;
  view_status(ctx, "已保存修改");
}

static void delete_exam(AppCtx *ctx, int exam_index) {
  if (ctx->gb->exam_count <= 1) {
    ui_error("至少需要保留一场考试，无法删除最后一场。");
    return;
  }
  const Exam *exam = &ctx->gb->exams[exam_index];
  char message[256];
  snprintf(message, sizeof(message), "确定删除考试「%s」吗？该场考试的全部成绩记录会一并清除，且不可恢复。", exam->name);
  if (!ui_confirm("删除考试", message)) return;
  gb_remove_exam(ctx->gb, exam->id);
  ctx->dirty = 1;
  view_status(ctx, "已删除该考试及其成绩");
}

/** 勾选本场考试的参考科目；全不勾表示考全部科目 */
static void pick_subjects(AppCtx *ctx, int exam_index) {
  Exam *exam = &ctx->gb->exams[exam_index];
  int count = ctx->gb->subject_count;
  if (count == 0) {
    ui_error("还没有科目，请先到「科目管理」添加。");
    return;
  }

  char **options = malloc(sizeof(char *) * (size_t)count);
  char **buffer = malloc(sizeof(char *) * (size_t)count);
  if (!options || !buffer) {
    free(options);
    free(buffer);
    return;
  }
  for (int i = 0; i < count; i++) {
    buffer[i] = malloc(96);
    if (!buffer[i]) {
      for (int j = 0; j < i; j++) free(buffer[j]);
      free(options);
      free(buffer);
      return;
    }
    int on = 0;
    for (int j = 0; j < exam->subject_count; j++)
      if (strcmp(exam->subject_ids[j], ctx->gb->subjects[i].id) == 0) on = 1;
    snprintf(buffer[i], 96, "[%s] %s（满分 %d）", on ? "x" : " ", ctx->gb->subjects[i].name, ctx->gb->subjects[i].full_mark);
    options[i] = buffer[i];
  }

  int choice = ui_pick("设置参考科目（空格/Enter 切换勾选，Esc 完成）", (const char **)options, count, 0);
  while (choice >= 0) {
    const char *id = ctx->gb->subjects[choice].id;
    int found = -1;
    for (int i = 0; i < exam->subject_count; i++)
      if (strcmp(exam->subject_ids[i], id) == 0) found = i;

    if (found >= 0) {
      for (int i = found; i < exam->subject_count - 1; i++)
        memcpy(exam->subject_ids[i], exam->subject_ids[i + 1], GM_ID_LEN);
      exam->subject_count--;
    } else if (exam->subject_count < GM_EXAM_SUBJ_MAX) {
      snprintf(exam->subject_ids[exam->subject_count], GM_ID_LEN, "%s", id);
      exam->subject_count++;
    }
    ctx->dirty = 1;

    for (int i = 0; i < count; i++) {
      int on = 0;
      for (int j = 0; j < exam->subject_count; j++)
        if (strcmp(exam->subject_ids[j], ctx->gb->subjects[i].id) == 0) on = 1;
      snprintf(buffer[i], 96, "[%s] %s（满分 %d）", on ? "x" : " ", ctx->gb->subjects[i].name, ctx->gb->subjects[i].full_mark);
    }
    choice = ui_pick("设置参考科目（空格/Enter 切换勾选，Esc 完成）", (const char **)options, count, choice);
  }

  for (int i = 0; i < count; i++) free(buffer[i]);
  free(options);
  free(buffer);
  view_status(ctx, exam->subject_count ? "已设置参考科目" : "未勾选，本次考全部科目");
}

static ListAction on_key(int key, int *index, void *ctx) {
  ExamsCtx *ec = ctx;
  AppCtx *app = ec->app;

  if (key == 'a') {
    edit_exam(app, -1, 1);
    rebuild(ec);
    if (ec->count > 0) *index = ec->count - 1;
    return LIST_REFRESH;
  }
  if (ec->count == 0) return LIST_REFRESH;

  int exam_index = ec->index[*index];
  if (key == 'e') {
    edit_exam(app, exam_index, 0);
    rebuild(ec);
    return LIST_REFRESH;
  }
  if (key == 's') {
    pick_subjects(app, exam_index);
    rebuild(ec);
    return LIST_REFRESH;
  }
  if (key == 'd') {
    delete_exam(app, exam_index);
    rebuild(ec);
    if (*index >= ec->count) *index = ec->count > 0 ? ec->count - 1 : 0;
    return LIST_REFRESH;
  }
  if (key == ' ' || key == 'c') {
    snprintf(app->gb->active_exam_id, sizeof(app->gb->active_exam_id), "%s", app->gb->exams[exam_index].id);
    app->dirty = 1;
    view_status(app, "已切换当前考试");
    return LIST_REFRESH;
  }
  return LIST_CONTINUE;
}

void view_exams(AppCtx *ctx) {
  ExamsCtx ec = {ctx, NULL, 0};
  rebuild(&ec);

  int cursor = 0;  /* 重新进入列表时续用光标位置 */

  for (;;) {
    char subtitle[96];
    snprintf(subtitle, sizeof(subtitle), "共 %d 场考试", ctx->gb->exam_count);
    int picked = ui_list("考试管理  [a]新增 [e]编辑 [s]设科目 [空格]设为当前 [d]删除 [q]返回",
                         subtitle, ec.count, cursor, draw_columns, render_row, on_key, &ec);
    if (picked == UI_LIST_REFRESH) { cursor = ui_list_last_index(); continue; }
    if (picked < 0 || picked >= ec.count) break;
    cursor = picked;
    edit_exam(ctx, ec.index[picked], 0);
    rebuild(&ec);
  }
  free(ec.index);
}
