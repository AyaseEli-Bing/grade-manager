/**
 * 视图：成绩录入 —— 科目为列、学生为行的网格，支持左右移动与就地编辑。
 * 越界输入自动钳制到 [0, 满分]；清空表示缺考（不计入总分）。
 */
#include "views.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "calc.h"
#include "ui.h"

#define SUBJECT_COL_W 9

typedef struct {
  AppCtx *app;
  const Exam *exam;
  int *index;
  int count;
  const Subject **subs;
  int nsub;
  int col;         /* 当前列（科目下标） */
  int col_offset;  /* 横向滚动 */
  int editing;
  char editbuf[16];
} ScoresCtx;

static void rebuild(ScoresCtx *sc) {
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

/** 计算布局：返回可见科目列数，并给出各科目的起始 x */
static int layout(ScoresCtx *sc, int width, int *x_first) {
  const int x_student = 1;
  const int w_student = 20;
  int room = width - (x_student + w_student) - 18;
  if (room < SUBJECT_COL_W) room = SUBJECT_COL_W;
  int visible = room / SUBJECT_COL_W;
  if (visible > sc->nsub) visible = sc->nsub;
  if (visible < 1) visible = 1;

  if (sc->col < sc->col_offset) sc->col_offset = sc->col;
  if (sc->col >= sc->col_offset + visible) sc->col_offset = sc->col - visible + 1;
  if (sc->col_offset < 0) sc->col_offset = 0;
  if (sc->col_offset + visible > sc->nsub) sc->col_offset = sc->nsub - visible;
  if (sc->col_offset < 0) sc->col_offset = 0;

  *x_first = x_student + w_student + 1;
  return visible;
}

static void draw_columns(WINDOW *win, int y, void *ctx) {
  ScoresCtx *sc = ctx;
  int x_first = 0;
  int visible = layout(sc, getmaxx(win), &x_first);

  ui_col(win, y, 1, 20, 0, "学生", UI_HEADER);
  for (int i = 0; i < visible; i++) {
    const Subject *subject = sc->subs[sc->col_offset + i];
    char head[32];
    snprintf(head, sizeof(head), "%.*s/%d", 6, subject->name, subject->full_mark);
    ui_col(win, y, x_first + i * SUBJECT_COL_W, SUBJECT_COL_W - 1, 1, head,
           sc->col_offset + i == sc->col ? UI_ACCENT : UI_HEADER);
  }
  int x_right = getmaxx(win) - 17;
  ui_col(win, y, x_right, 8, 1, "总分", UI_HEADER);
  ui_col(win, y, x_right + 9, 8, 1, "平均分", UI_HEADER);
}

static void render_row(WINDOW *win, int y, int index, int selected, void *ctx) {
  ScoresCtx *sc = ctx;
  int x_first = 0;
  int visible = layout(sc, getmaxx(win), &x_first);
  const Student *student = &sc->app->gb->students[sc->index[index]];
  const char *exam_id = sc->exam->id;

  int row_role = selected ? UI_SELECTED : UI_TEXT;
  wattron(win, ui_role(row_role));
  mvwhline(win, y, 0, ' ', getmaxx(win));
  wattroff(win, ui_role(row_role));

  ui_col(win, y, 1, 20, 0, student->name, row_role);

  double total = 0;
  int counted = 0;
  for (int i = 0; i < visible; i++) {
    int subject_i = sc->col_offset + i;
    const Subject *subject = sc->subs[subject_i];
    double value = gb_get_score(sc->app->gb, exam_id, student->id, subject->id);

    const char *text;
    char buf[16];
    if (value < 0) {
      text = "-";
    } else {
      snprintf(buf, sizeof(buf), "%.0f", value);
      text = buf;
    }
    int is_cursor = selected && subject_i == sc->col;
    if (is_cursor && sc->editing) {
      snprintf(buf, sizeof(buf), "%s_", sc->editbuf);
      text = buf;
    }
    int role = is_cursor ? UI_SELECTED : row_role;
    if (!is_cursor && value >= 0) {
      if (value < subject->full_mark * 0.6) role = UI_DANGER;
      else if (value >= subject->full_mark * 0.9) role = UI_SUCCESS;
    }
    ui_col(win, y, x_first + i * SUBJECT_COL_W, SUBJECT_COL_W - 1, 1, text, role);
  }

  for (int i = 0; i < sc->nsub; i++) {
    double value = gb_get_score(sc->app->gb, exam_id, student->id, sc->subs[i]->id);
    if (value >= 0) {
      total += value;
      counted++;
    }
  }

  int x_right = getmaxx(win) - 17;
  char buf[16];
  snprintf(buf, sizeof(buf), "%.0f", total);
  ui_col(win, y, x_right, 8, 1, counted ? buf : "-", row_role);
  snprintf(buf, sizeof(buf), "%.1f", counted ? total / counted : 0.0);
  ui_col(win, y, x_right + 9, 8, 1, counted ? buf : "-", row_role);
}

/** 提交编辑缓冲：非法输入钳制到边界；空内容视为缺考 */
static void commit_cell(ScoresCtx *sc, int row) {
  const Student *student = &sc->app->gb->students[sc->index[row]];
  const Subject *subject = sc->subs[sc->col];
  char message[160];

  if (sc->editbuf[0] == '\0') {
    gb_clear_score(sc->app->gb, sc->exam->id, student->id, subject->id);
    sc->app->dirty = 1;
    view_status(sc->app, "已清空该格（记为缺考）");
    sc->editing = 0;
    return;
  }

  double value = atof(sc->editbuf);
  message[0] = '\0';
  if (value < 0) {
    snprintf(message, sizeof(message), "「%s」不能为负数，已修正为 0。", subject->name);
    value = 0;
  } else if (value > subject->full_mark) {
    snprintf(message, sizeof(message), "「%s」满分为 %d，已修正为 %d。", subject->name, subject->full_mark, subject->full_mark);
    value = (double)subject->full_mark;
  }

  gb_set_score(sc->app->gb, sc->exam->id, student->id, subject->id, value);
  sc->app->dirty = 1;
  if (message[0]) {
    ui_error(message);
    view_status(sc->app, "已按满分范围修正");
  } else {
    snprintf(message, sizeof(message), "已记录 %s：%.0f", subject->name, value);
    view_status(sc->app, message);
  }
  sc->editing = 0;
}

static ListAction on_key(int key, int *index, void *ctx) {
  ScoresCtx *sc = ctx;
  AppCtx *app = sc->app;

  if (sc->count == 0 || sc->nsub == 0) {
    if (key == '/') {
      char keyword[64];
      snprintf(keyword, sizeof(keyword), "%s", app->search);
      if (ui_input("筛选学生", "按学号或姓名筛选（留空显示全部）：", keyword, sizeof(keyword))) {
        snprintf(app->search, sizeof(app->search), "%s", keyword);
        rebuild(sc);
        *index = 0;
      }
    }
    return LIST_CONTINUE;
  }

  if (sc->editing) {
    if (key == '\n' || key == KEY_ENTER) {
      commit_cell(sc, *index);
      return LIST_CONTINUE;
    }
    if (key == KEY_ESC) {
      sc->editing = 0;
      sc->editbuf[0] = '\0';
      return LIST_CONTINUE;
    }
    if (key == KEY_BACKSPACE || key == 127 || key == '\b') {
      size_t len = strlen(sc->editbuf);
      if (len > 0) sc->editbuf[len - 1] = '\0';
      return LIST_CONTINUE;
    }
    if ((key >= '0' && key <= '9') || key == '.') {
      size_t len = strlen(sc->editbuf);
      if (len + 1 < sizeof(sc->editbuf)) {
        sc->editbuf[len] = (char)key;
        sc->editbuf[len + 1] = '\0';
      }
      return LIST_CONTINUE;
    }
    /* 其余按键在编辑态下先提交，再交还列表处理 */
    commit_cell(sc, *index);
    return LIST_CONTINUE;
  }

  if (key == KEY_LEFT) {
    if (sc->col > 0) sc->col--;
    return LIST_CONTINUE;
  }
  if (key == KEY_RIGHT) {
    if (sc->col < sc->nsub - 1) sc->col++;
    return LIST_CONTINUE;
  }
  if (key == 'd') {
    sc->editbuf[0] = '\0';
    sc->editing = 1;
    commit_cell(sc, *index);
    return LIST_CONTINUE;
  }
  if (key == '/' || key == 'f') {
    char keyword[64];
    snprintf(keyword, sizeof(keyword), "%s", app->search);
    if (ui_input("筛选学生", "按学号或姓名筛选（留空显示全部）：", keyword, sizeof(keyword))) {
      snprintf(app->search, sizeof(app->search), "%s", keyword);
      rebuild(sc);
      *index = 0;
    }
    return LIST_CONTINUE;
  }
  if (key >= '0' && key <= '9') {
    sc->editing = 1;
    sc->editbuf[0] = (char)key;
    sc->editbuf[1] = '\0';
    return LIST_CONTINUE;
  }
  if (key == '\n' || key == KEY_ENTER) {
    const Student *student = &app->gb->students[sc->index[*index]];
    double value = gb_get_score(app->gb, sc->exam->id, student->id, sc->subs[sc->col]->id);
    sc->editing = 1;
    if (value >= 0) snprintf(sc->editbuf, sizeof(sc->editbuf), "%.0f", value);
    else sc->editbuf[0] = '\0';
    return LIST_CONTINUE;
  }
  return LIST_CONTINUE;
}

void view_scores(AppCtx *ctx) {
  const Exam *exam = gb_active_exam(ctx->gb);
  if (!exam) {
    ui_error("还没有任何考试，请先到「考试管理」创建一场。");
    return;
  }
  if (ctx->gb->student_count == 0) {
    ui_error("还没有学生，请先到「学生管理」添加或从 CSV 导入。");
    return;
  }

  ScoresCtx sc;
  memset(&sc, 0, sizeof(sc));
  sc.app = ctx;
  sc.exam = exam;
  sc.subs = malloc(sizeof(Subject *) * (size_t)(ctx->gb->subject_count > 0 ? ctx->gb->subject_count : 1));
  if (!sc.subs) return;
  sc.nsub = calc_exam_subjects(ctx->gb, exam, sc.subs, ctx->gb->subject_count);
  if (sc.nsub == 0) {
    free(sc.subs);
    ui_error("本场考试没有参考科目，请先到「科目管理」添加科目。");
    return;
  }

  rebuild(&sc);
  for (;;) {
    int total = 0, done = 0;
    double percent = 0;
    calc_progress(ctx->gb, exam, &total, &done, &percent);

    char subtitle[160];
    snprintf(subtitle, sizeof(subtitle), "%s · 已录 %d/%d（%.0f%%）", exam->name, done, total, percent);

    /* 回车在 on_key 中已进入编辑态，因此这里的返回值只用于判退出 */
    if (ui_list("成绩录入  [0-9]直接输入 [←→]换列 [Enter]编辑 [d]清空 [/]筛选 [q]返回",
                subtitle, sc.count, 0, draw_columns, render_row, on_key, &sc) < 0) break;
  }
  free(sc.index);
  free(sc.subs);
}
