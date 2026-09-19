/**
 * 终端界面的绘制层：主题、页眉页脚、条形图、折线图。
 * 交互控件（表单 / 确认 / 列表）在 ui_widgets.c。
 *
 * 颜色定义集中在本文件顶部的 theme 表，业务代码一律用 ui_role() 取语义角色。
 */
#include "ui.h"

#include <locale.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 配色表 —— 相当于设计 Token 层，全局唯一。
   bg 为 -1 表示沿用终端自身背景色，从而自动适配用户的浅色/深色终端主题。 */
static struct {
  short fg;
  short bg;
  int attr;
} theme[UI_ROLE_COUNT] = {
    /* UI_TEXT    */ {COLOR_WHITE, -1, 0},
    /* UI_DIM     */ {COLOR_WHITE, -1, A_DIM},
    /* UI_ACCENT  */ {COLOR_CYAN, -1, A_BOLD},
    /* UI_SUCCESS */ {COLOR_GREEN, -1, 0},
    /* UI_WARN    */ {COLOR_YELLOW, -1, 0},
    /* UI_DANGER  */ {COLOR_RED, -1, 0},
    /* UI_HEADER  */ {COLOR_CYAN, -1, A_BOLD},
    /* UI_SELECTED*/ {COLOR_WHITE, COLOR_CYAN, A_BOLD},
    /* UI_STATUS  */ {COLOR_BLACK, COLOR_CYAN, 0},
    /* UI_TITLE   */ {COLOR_CYAN, -1, A_BOLD},
    /* UI_BAR     */ {COLOR_CYAN, -1, 0},
    /* UI_BAR_FAIL*/ {COLOR_RED, -1, 0},
    /* UI_BORDER  */ {COLOR_BLUE, -1, 0},
};

void ui_theme_init(void) {
  start_color();
  use_default_colors();
  for (int role = 0; role < UI_ROLE_COUNT; role++) {
    init_pair((short)(role + 1), theme[role].fg, theme[role].bg);
  }
}

int ui_role(int role) {
  if (role < 0 || role >= UI_ROLE_COUNT) return 0;
  /* ncurses 用 chtype（无符号）承载属性，这里显式转换以通过 -Wsign-conversion */
  return (int)((chtype)COLOR_PAIR(role + 1) | (chtype)theme[role].attr);
}

/* ---------- 生命周期 ---------- */

int ui_init(void) {
  setlocale(LC_ALL, ""); /* 启用 UTF-8 宽字符，否则方框字符会显示为乱码 */
  if (!initscr()) return -1;
  ui_theme_init();
  cbreak();
  noecho();
  keypad(stdscr, TRUE);
  curs_set(0);
  set_escdelay(50);
  return 0;
}

void ui_shutdown(void) { endwin(); }

void ui_terminal_size(int *rows, int *cols) {
  if (rows) *rows = LINES;
  if (cols) *cols = COLS;
}

/* ---------- 文本度量 ---------- */

/** 解码一个 UTF-8 序列，返回码点并前移下标；非法字节按 1 列处理 */
static unsigned long utf8_next(const char *text, int *index) {
  unsigned char lead = (unsigned char)text[*index];
  int extra = 0;
  unsigned long cp = lead;

  if (lead >= 0xf0) {
    extra = 3;
    cp = lead & 0x07UL;
  } else if (lead >= 0xe0) {
    extra = 2;
    cp = lead & 0x0fUL;
  } else if (lead >= 0xc0) {
    extra = 1;
    cp = lead & 0x1fUL;
  } else {
    (*index)++;
    return cp;
  }

  for (int i = 1; i <= extra; i++) {
    unsigned char next = (unsigned char)text[*index + i];
    if ((next & 0xc0) != 0x80) {
      (*index)++;
      return lead; /* 非法序列：退回单字节处理 */
    }
    cp = (cp << 6) | (next & 0x3fUL);
  }
  *index += extra + 1;
  return cp;
}

/** 双宽字符判定：CJK 统一表意文字、假名、全角符号等 */
static int is_wide(unsigned long cp) {
  return (cp >= 0x1100 && cp <= 0x115f) ||
         (cp >= 0x2e80 && cp <= 0xa4cf) ||
         (cp >= 0xac00 && cp <= 0xd7a3) ||
         (cp >= 0xf900 && cp <= 0xfaff) ||
         (cp >= 0xfe30 && cp <= 0xfe6f) ||
         (cp >= 0xff00 && cp <= 0xff60) ||
         (cp >= 0xffe0 && cp <= 0xffe6) ||
         (cp >= 0x20000 && cp <= 0x3fffd);
}

int ui_str_cols(const char *text) {
  if (!text) return 0;
  int cols = 0;
  int i = 0;
  while (text[i]) {
    unsigned long cp = utf8_next(text, &i);
    cols += is_wide(cp) ? 2 : 1;
  }
  return cols;
}

void ui_trunc(const char *text, int max_cols, char *out, size_t outsz) {
  if (outsz == 0) return;
  out[0] = '\0';
  if (!text || max_cols <= 0) return;

  if (ui_str_cols(text) <= max_cols) {
    snprintf(out, outsz, "%s", text);
    return;
  }

  /* 逐字符累加，留出 1 列给省略号 */
  int budget = max_cols - 1;
  if (budget < 1) budget = 1;
  int used = 0;
  int i = 0;
  size_t written = 0;
  while (text[i] && used < budget) {
    int start = i;
    unsigned long cp = utf8_next(text, &i);
    int w = is_wide(cp) ? 2 : 1;
    if (used + w > budget) break;
    for (int b = start; b < i && written + 1 < outsz; b++) out[written++] = text[b];
    used += w;
  }
  out[written] = '\0';
  snprintf(out + written, outsz - written, "\u2026");
}

void ui_col(WINDOW *win, int y, int x, int width, int align_right, const char *text, int role) {
  if (width <= 0) return;
  char buf[512];
  ui_trunc(text, width, buf, sizeof(buf));
  int pad = width - ui_str_cols(buf);
  if (pad < 0) pad = 0;

  int attr = role >= 0 ? ui_role(role) : 0;
  if (attr) wattron(win, attr);
  if (align_right) {
    mvwprintw(win, y, x + pad, "%s", buf);
  } else {
    mvwprintw(win, y, x, "%s", buf);
    for (int i = 0; i < pad; i++) mvwaddch(win, y, x + ui_str_cols(buf) + i, ' ');
  }
  if (attr) wattroff(win, attr);
}

/* ---------- 页眉页脚 ---------- */

void ui_hline(WINDOW *win, int y, int x, int width) {
  mvwhline(win, y, x, ACS_HLINE, width);
}

void ui_header(WINDOW *win, const char *title, const char *subtitle) {
  int width = getmaxx(win);
  wattron(win, ui_role(UI_TITLE));
  mvwprintw(win, 0, 0, " %s", title ? title : "");
  wattroff(win, ui_role(UI_TITLE));
  if (subtitle && subtitle[0]) {
    /* 必须按显示列宽定位：中文标题一个字符占 2 列，用 strlen 会把副标题推出屏幕 */
    int offset = ui_str_cols(title) + 2;
    wattron(win, ui_role(UI_DIM));
    mvwprintw(win, 0, offset, "%s", subtitle);
    wattroff(win, ui_role(UI_DIM));
  }
  wattron(win, ui_role(UI_BORDER));
  ui_hline(win, 1, 0, width);
  wattroff(win, ui_role(UI_BORDER));
}

void ui_footer(WINDOW *win, const char *hint) {
  int y = getmaxy(win) - 1;
  int width = getmaxx(win);
  wattron(win, ui_role(UI_STATUS));
  mvwhline(win, y, 0, ' ', width);
  mvwprintw(win, y, 0, " %s", hint ? hint : "");
  wattroff(win, ui_role(UI_STATUS));
}

/* 条形图与折线图见 ui_chart.c */
