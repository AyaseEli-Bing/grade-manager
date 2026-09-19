/**
 * 视图：科目管理 —— 自定义科目名称与满分，支持调整顺序。
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
} SubjectsCtx;

static void rebuild(SubjectsCtx *sc) {
  GradeBook *gb = sc->app->gb;
  free(sc->index);
  sc->index = malloc(sizeof(int) * (size_t)(gb->subject_count > 0 ? gb->subject_count : 1));
  sc->count = 0;
  if (!sc->index) return;
  for (int i = 0; i < gb->subject_count; i++) {
    if (view_match(gb->subjects[i].name, sc->app->search)) sc->index[sc->count++] = i;
  }
}

static void draw_columns(WINDOW *win, int y, void *ctx) {
  (void)ctx;
  int width = getmaxx(win);
  ui_col(win, y, 1, 4, 1, "#", UI_HEADER);
  ui_col(win, y, 6, 16, 0, "科目", UI_HEADER);
  ui_col(win, y, 23, 8, 1, "满分", UI_HEADER);
  ui_col(win, y, 32, 14, 1, "关联成绩数", UI_HEADER);
  ui_col(win, y, 47, width - 48, 0, "说明", UI_HEADER);
}

static void render_row(WINDOW *win, int y, int index, int selected, void *ctx) {
  SubjectsCtx *sc = ctx;
  const Subject *subject = &sc->app->gb->subjects[sc->index[index]];
  int role = selected ? UI_SELECTED : UI_TEXT;
  int dim = selected ? UI_SELECTED : UI_DIM;
  int width = getmaxx(win);

  wattron(win, ui_role(role));
  mvwhline(win, y, 0, ' ', width);
  wattroff(win, ui_role(role));

  char buf[32];
  snprintf(buf, sizeof(buf), "%d", index + 1);
  ui_col(win, y, 1, 4, 1, buf, dim);
  ui_col(win, y, 6, 16, 0, subject->name, role);
  snprintf(buf, sizeof(buf), "%d", subject->full_mark);
  ui_col(win, y, 23, 8, 1, buf, role);
  snprintf(buf, sizeof(buf), "%d", gb_count_subject_cells(sc->app->gb, subject->id));
  ui_col(win, y, 32, 14, 1, buf, dim);
  ui_col(win, y, 47, width - 48, 0, "删除将同时清除所有考试中的该科成绩", dim);
}

static void edit_subject(AppCtx *ctx, int subject_index, int is_new) {
  static const char *labels[] = {"科目名（必填）", "满分（默认 100）"};
  char name[GM_NAME_LEN] = "";
  char mark[16] = "";

  if (!is_new) {
    const Subject *subject = &ctx->gb->subjects[subject_index];
    snprintf(name, sizeof(name), "%s", subject->name);
    snprintf(mark, sizeof(mark), "%d", subject->full_mark);
  } else {
    snprintf(mark, sizeof(mark), "100");
  }

  char *fields[] = {name, mark};
  const size_t widths[] = {sizeof(name), sizeof(mark)};
  if (!ui_form(is_new ? "新增科目" : "编辑科目", labels, fields, widths, 2)) return;

  if (name[0] == '\0') {
    ui_error("科目名不能为空。");
    return;
  }
  int full_mark = atoi(mark);
  if (full_mark <= 0) full_mark = 100;

  if (is_new) {
    if (!gb_add_subject(ctx->gb, name, full_mark)) {
      ui_error("内存不足，新增科目失败。");
      return;
    }
    ctx->dirty = 1;
    view_status(ctx, "已新增科目");
    return;
  }

  Subject *subject = &ctx->gb->subjects[subject_index];
  snprintf(subject->name, sizeof(subject->name), "%s", name);
  subject->full_mark = full_mark;
  ctx->dirty = 1;
  view_status(ctx, "已保存修改");
}

static void delete_subject(AppCtx *ctx, int subject_index) {
  const Subject *subject = &ctx->gb->subjects[subject_index];
  int cells = gb_count_subject_cells(ctx->gb, subject->id);
  char message[256];
  snprintf(message, sizeof(message), "确定删除科目「%s」吗？将同时清除该科目在所有考试中的 %d 条成绩记录，且不可恢复。",
           subject->name, cells);
  if (!ui_confirm("删除科目", message)) return;
  gb_remove_subject(ctx->gb, subject->id);
  ctx->dirty = 1;
  view_status(ctx, "已删除该科目及其成绩");
}

static void move_subject(AppCtx *ctx, int from, int to) {
  if (from == to || to < 0 || to >= ctx->gb->subject_count) return;
  Subject tmp = ctx->gb->subjects[from];
  ctx->gb->subjects[from] = ctx->gb->subjects[to];
  ctx->gb->subjects[to] = tmp;
  ctx->dirty = 1;
  view_status(ctx, "已调整科目顺序");
}

static ListAction on_key(int key, int *index, void *ctx) {
  SubjectsCtx *sc = ctx;
  AppCtx *app = sc->app;

  if (key == 'a') {
    edit_subject(app, -1, 1);
    rebuild(sc);
    if (sc->count > 0) *index = sc->count - 1;
    return LIST_REFRESH;
  }
  if (sc->count == 0) return LIST_REFRESH;

  int subject_index = sc->index[*index];
  if (key == 'e') {
    edit_subject(app, subject_index, 0);
    rebuild(sc);
    return LIST_REFRESH;
  }
  if (key == 'd') {
    delete_subject(app, subject_index);
    rebuild(sc);
    if (*index >= sc->count) *index = sc->count > 0 ? sc->count - 1 : 0;
    return LIST_REFRESH;
  }
  if (key == 'u' && *index > 0) {
    move_subject(app, sc->index[*index], sc->index[*index - 1]);
    rebuild(sc);
    (*index)--;
    return LIST_REFRESH;
  }
  if (key == 'j' && *index < sc->count - 1) {
    move_subject(app, sc->index[*index], sc->index[*index + 1]);
    rebuild(sc);
    (*index)++;
    return LIST_REFRESH;
  }
  return LIST_CONTINUE;
}

void view_subjects(AppCtx *ctx) {
  SubjectsCtx sc = {ctx, NULL, 0};
  rebuild(&sc);

  int cursor = 0;  /* 重新进入列表时续用光标位置 */

  for (;;) {
    char subtitle[96];
    snprintf(subtitle, sizeof(subtitle), "共 %d 个科目", ctx->gb->subject_count);
    int picked = ui_list("科目管理  [a]新增 [e]编辑 [d]删除 [u/j]上移下移 [q]返回",
                         subtitle, sc.count, cursor, draw_columns, render_row, on_key, &sc);
    if (picked == UI_LIST_REFRESH) { cursor = ui_list_last_index(); continue; }
    if (picked < 0 || picked >= sc.count) break;
    cursor = picked;
    edit_subject(ctx, sc.index[picked], 0);
    rebuild(&sc);
  }
  free(sc.index);
}
