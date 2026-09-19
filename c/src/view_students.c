/**
 * 视图：学生管理 —— 名单维护、批量 CSV 导入、删除。
 */
#include "views.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ui.h"

typedef struct {
  AppCtx *app;
  int *index; /* 通过搜索筛选后的学生在 gb->students 中的下标 */
  int count;
} StudentsCtx;

static void rebuild(StudentsCtx *sc) {
  GradeBook *gb = sc->app->gb;
  free(sc->index);
  sc->index = malloc(sizeof(int) * (size_t)(gb->student_count > 0 ? gb->student_count : 1));
  sc->count = 0;
  if (!sc->index) return;
  for (int i = 0; i < gb->student_count; i++) {
    const Student *student = &gb->students[i];
    if (view_match(student->name, sc->app->search) || view_match(student->sid, sc->app->search)) {
      sc->index[sc->count++] = i;
    }
  }
}

static void draw_columns(WINDOW *win, int y, void *ctx) {
  (void)ctx;
  int width = getmaxx(win);
  ui_col(win, y, 1, 4, 1, "#", UI_HEADER);
  ui_col(win, y, 6, 10, 0, "学号", UI_HEADER);
  ui_col(win, y, 17, 14, 0, "姓名", UI_HEADER);
  ui_col(win, y, 32, 6, 0, "性别", UI_HEADER);
  ui_col(win, y, 39, width - 40, 0, "备注", UI_HEADER);
}

static void render_row(WINDOW *win, int y, int index, int selected, void *ctx) {
  StudentsCtx *sc = ctx;
  const Student *student = &sc->app->gb->students[sc->index[index]];
  int role = selected ? UI_SELECTED : UI_TEXT;
  int dim = selected ? UI_SELECTED : UI_DIM;
  int width = getmaxx(win);

  wattron(win, ui_role(selected ? UI_SELECTED : UI_TEXT));
  mvwhline(win, y, 0, ' ', width);
  wattroff(win, ui_role(selected ? UI_SELECTED : UI_TEXT));

  char buf[32];
  snprintf(buf, sizeof(buf), "%d", index + 1);
  ui_col(win, y, 1, 4, 1, buf, dim);
  ui_col(win, y, 6, 10, 0, student->sid[0] ? student->sid : "-", role);
  ui_col(win, y, 17, 14, 0, student->name, role);
  ui_col(win, y, 32, 6, 0, student->gender[0] ? student->gender : "-", role);
  ui_col(win, y, 39, width - 40, 0, student->note, dim);
}

static void edit_student(AppCtx *ctx, int student_index, int is_new) {
  static const char *labels[] = {"姓名（必填）", "学号", "性别（男/女，可留空）", "备注"};
  char name[GM_NAME_LEN] = "";
  char sid[GM_SID_LEN] = "";
  char gender[8] = "";
  char note[GM_NOTE_LEN] = "";

  if (!is_new && student_index >= 0) {
    const Student *student = &ctx->gb->students[student_index];
    snprintf(name, sizeof(name), "%s", student->name);
    snprintf(sid, sizeof(sid), "%s", student->sid);
    snprintf(gender, sizeof(gender), "%s", student->gender);
    snprintf(note, sizeof(note), "%s", student->note);
  }

  char *fields[] = {name, sid, gender, note};
  const size_t widths[] = {sizeof(name), sizeof(sid), sizeof(gender), sizeof(note)};

  if (!ui_form(is_new ? "新增学生" : "编辑学生", labels, fields, widths, 4)) return;

  if (name[0] == '\0') {
    ui_error("姓名不能为空。");
    return;
  }
  if (gender[0] && strcmp(gender, "男") != 0 && strcmp(gender, "女") != 0) {
    ui_error("性别只能填「男」或「女」，不需要就留空。");
    return;
  }

  if (is_new) {
    if (!gb_add_student(ctx->gb, sid, name, gender, note)) {
      ui_error("内存不足，新增学生失败。");
      return;
    }
    ctx->dirty = 1;
    view_status(ctx, "已新增学生");
    return;
  }

  Student *student = &ctx->gb->students[student_index];
  snprintf(student->name, sizeof(student->name), "%s", name);
  snprintf(student->sid, sizeof(student->sid), "%s", sid);
  snprintf(student->gender, sizeof(student->gender), "%s", gender);
  snprintf(student->note, sizeof(student->note), "%s", note);
  ctx->dirty = 1;
  view_status(ctx, "已保存修改");
}

static void delete_student(AppCtx *ctx, int student_index) {
  const Student *student = &ctx->gb->students[student_index];
  char message[256];
  snprintf(message, sizeof(message), "确定删除「%s」吗？该生在所有考试中的成绩记录会一并清除，且不可恢复。", student->name);
  if (!ui_confirm("删除学生", message)) return;
  gb_remove_student(ctx->gb, student->id);
  ctx->dirty = 1;
  view_status(ctx, "已删除该学生及其成绩");
}

static void import_csv(AppCtx *ctx) {
  char path[512];
  if (!ui_filepath("导入 CSV 名单", "students.csv", path, sizeof(path), 1)) return;

  int write_scores = ui_confirm("导入名单中的成绩",
                                "若 CSV 中含有与现有科目同名的列，是否一并写入当前考试？已存在的成绩会被覆盖。");
  int added = 0, merged = 0;
  char err[256] = "";
  if (gb_import_csv(ctx->gb, path, write_scores, &added, &merged, err, sizeof(err)) != 0) {
    ui_error(err);
    return;
  }
  ctx->dirty = 1;
  char info[256];
  snprintf(info, sizeof(info), "导入完成：新增 %d 名，合并已存在 %d 名", added, merged);
  view_status(ctx, info);
}

static ListAction on_key(int key, int *index, void *ctx) {
  StudentsCtx *sc = ctx;
  AppCtx *app = sc->app;

  if (key == 'a') {
    edit_student(app, -1, 1);
    rebuild(sc);
    if (sc->count > 0) *index = sc->count - 1;
    return LIST_REFRESH;
  }
  if (key == 'i') {
    import_csv(app);
    rebuild(sc);
    return LIST_REFRESH;
  }
  if (key == '/' || key == 'f') {
    char keyword[64];
    snprintf(keyword, sizeof(keyword), "%s", app->search);
    if (ui_input("筛选学生", "按学号或姓名筛选（留空显示全部）：", keyword, sizeof(keyword))) {
      snprintf(app->search, sizeof(app->search), "%s", keyword);
      rebuild(sc);
      *index = 0;
    }
    return LIST_REFRESH;
  }

  if (sc->count == 0) return LIST_REFRESH;
  int student_index = sc->index[*index];

  if (key == 'e') {
    edit_student(app, student_index, 0);
    rebuild(sc);
    if (*index >= sc->count) *index = sc->count > 0 ? sc->count - 1 : 0;
    return LIST_REFRESH;
  }
  if (key == 'd') {
    delete_student(app, student_index);
    rebuild(sc);
    if (*index >= sc->count) *index = sc->count > 0 ? sc->count - 1 : 0;
    return LIST_REFRESH;
  }
  return LIST_CONTINUE;
}

void view_students(AppCtx *ctx) {
  StudentsCtx sc = {ctx, NULL, 0};
  rebuild(&sc);

  int cursor = 0;  /* 重新进入列表时续用光标位置 */

  for (;;) {
    char subtitle[128];
    if (ctx->search[0]) snprintf(subtitle, sizeof(subtitle), "筛选：%s", ctx->search);
    else snprintf(subtitle, sizeof(subtitle), "共 %d 名学生", ctx->gb->student_count);

    int picked = ui_list("学生管理  [a]新增 [e]编辑 [d]删除 [i]导入CSV [/]筛选 [q]返回",
                         subtitle, sc.count, cursor, draw_columns, render_row, on_key, &sc);
    if (picked == UI_LIST_REFRESH) { cursor = ui_list_last_index(); continue; }
    if (picked < 0 || picked >= sc.count) break;
    cursor = picked; /* 空列表时 picked 为 -1，必须挡住 */
    edit_student(ctx, sc.index[picked], 0);
    rebuild(&sc);
  }
  free(sc.index);
}
