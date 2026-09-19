/**
 * 字符图表：水平条形图与折线趋势图。
 * 不使用任何绘图库，全部靠字符与 ncurses 属性实现。
 */
#include "ui.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

void ui_bar(WINDOW *win, int y, int x, int width, double ratio, int role) {
  if (width <= 0) return;
  if (!(ratio >= 0.0)) ratio = 0.0;
  if (ratio > 1.0) ratio = 1.0;

  int filled = (int)llround(ratio * width);
  if (filled == 0 && ratio > 0.0) filled = 1; /* 有数据就要看得见，避免"有值却没条" */
  if (filled > width) filled = width;

  wattron(win, ui_role(role));
  for (int i = 0; i < filled; i++) mvwaddstr(win, y, x + i, "\u2588"); /* █ */
  wattroff(win, ui_role(role));

  wattron(win, ui_role(UI_DIM));
  for (int i = filled; i < width; i++) mvwaddstr(win, y, x + i, "\u2591"); /* ░ */
  wattroff(win, ui_role(UI_DIM));
}

/* ---------- 折线图 ---------- */

static void plot_char(WINDOW *win, int y, int x, const char *text) {
  if (y < 0 || x < 0) return;
  if (y >= getmaxy(win) || x >= getmaxx(win)) return;
  mvwaddstr(win, y, x, text);
}

/** 两点之间画字符连线（Bresenham） */
static void plot_segment(WINDOW *win, int x0, int y0, int x1, int y1) {
  int dx = abs(x1 - x0);
  int dy = -abs(y1 - y0);
  int sx = x0 < x1 ? 1 : -1;
  int sy = y0 < y1 ? 1 : -1;
  int err = dx + dy;

  for (;;) {
    if (x0 != x1 || y0 != y1) {
      const char *glyph = "-";
      if (y0 != y1 && x0 != x1) glyph = (y1 > y0) ? "\\" : "/";
      plot_char(win, y0, x0, glyph);
    }
    if (x0 == x1 && y0 == y1) break;
    int e2 = 2 * err;
    if (e2 >= dy) {
      err += dy;
      x0 += sx;
    }
    if (e2 <= dx) {
      err += dx;
      y0 += sy;
    }
  }
}

void ui_line_chart(WINDOW *win, int y, int x, int width, int height,
                   const double *values, int n, const char **labels) {
  if (width <= 0 || height <= 0 || n <= 0) return;

  double min = 0, max = 0;
  int have = 0;
  for (int i = 0; i < n; i++) {
    if (!(values[i] >= 0)) continue; /* 缺考（-1）不参与坐标计算 */
    if (!have) {
      min = max = values[i];
      have = 1;
    } else {
      if (values[i] < min) min = values[i];
      if (values[i] > max) max = values[i];
    }
  }
  if (!have) {
    wattron(win, ui_role(UI_DIM));
    mvwprintw(win, y, x, "尚无成绩记录，无法绘制趋势。");
    wattroff(win, ui_role(UI_DIM));
    return;
  }
  if (max - min < 1e-9) {
    min -= 10;
    max += 10;
  }

  const int plot_h = height - 1; /* 末行留给横轴标签 */
  const int bottom = y + plot_h - 1;

  int *rows = calloc((size_t)n, sizeof(int));
  int *cols = calloc((size_t)n, sizeof(int));
  if (!rows || !cols) {
    free(rows);
    free(cols);
    return;
  }

  const double span = max - min;
  for (int i = 0; i < n; i++) {
    cols[i] = n == 1 ? x + width / 2 : x + (int)((double)i * (width - 1) / (n - 1));
    if (!(values[i] >= 0)) {
      rows[i] = -1;
      continue;
    }
    double norm = (values[i] - min) / span;
    rows[i] = (int)(bottom - llround(norm * (double)(plot_h - 1)));
  }

  /* 连线：只连接两端都有成绩的点，缺考处自然断开 */
  wattron(win, ui_role(UI_ACCENT));
  for (int i = 0; i < n - 1; i++) {
    if (rows[i] < 0 || rows[i + 1] < 0) continue;
    plot_segment(win, cols[i], rows[i], cols[i + 1], rows[i + 1]);
  }
  wattroff(win, ui_role(UI_ACCENT));

  /* 数据点：末次考试用实心强调 */
  for (int i = 0; i < n; i++) {
    if (rows[i] < 0) continue;
    int is_last = 1;
    for (int j = i + 1; j < n; j++)
      if (rows[j] >= 0) is_last = 0;
    wattron(win, ui_role(is_last ? UI_ACCENT : UI_TEXT));
    plot_char(win, rows[i], cols[i], is_last ? "\u25cf" : "\u25cb"); /* ● / ○ */
    wattroff(win, ui_role(is_last ? UI_ACCENT : UI_TEXT));
  }

  /* 横轴标签：放不下就跳过 */
  wattron(win, ui_role(UI_DIM));
  for (int i = 0; i < n; i++) {
    const char *text = labels && labels[i] ? labels[i] : "";
    if (text[0] == '\0') continue;
    int cols_text = ui_str_cols(text);
    int start = cols[i] - cols_text / 2;
    if (start < x) start = x;
    if (start + cols_text > x + width) continue;
    mvwprintw(win, y + plot_h, start, "%s", text);
  }
  wattroff(win, ui_role(UI_DIM));

  free(rows);
  free(cols);
}
