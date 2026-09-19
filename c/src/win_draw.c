/**
 * Win32 后端的绘制原语：属性、写字符、写字符串、水平线、边框。
 */
#include "win_curses.h"
#include "win_internal.h"

#ifdef _WIN32

#include <stdio.h>
#include <string.h>
#include <wchar.h>

int wattron(WINDOW *win, int attrs) {
  if (!win) return ERR;
  win->attrs |= attrs;
  return OK;
}

int wattroff(WINDOW *win, int attrs) {
  if (!win) return ERR;
  /* 整对属性一起关：ncurses 也是按位与非，这里保持一致即可 */
  win->attrs &= ~attrs;
  return OK;
}

int wmove(WINDOW *win, int y, int x) {
  if (!win) return ERR;
  win->cur_y = y;
  win->cur_x = x;
  return OK;
}

int mvwaddch(WINDOW *win, int y, int x, const chtype ch) {
  if (!win) return ERR;
  /* 视图层传进来的要么是 ASCII 字符，要么是 ACS_* 里定义的 Unicode 码点 */
  wchar_t wide = (wchar_t)(ch & 0x10ffffu);
  gm_win_put_wchar(win, y, x, wide);
  return OK;
}

/** 解码一个 UTF-8 序列；非法字节按 Latin-1 处理，保证不越界 */
static wchar_t utf8_decode(const char *text, int *index) {
  const unsigned char *bytes = (const unsigned char *)text;
  unsigned char lead = bytes[*index];

  if (lead < 0x80) {
    (*index)++;
    return (wchar_t)lead;
  }

  int extra = 0;
  unsigned long cp = lead;
  if (lead >= 0xf0) {
    extra = 3;
    cp = lead & 0x07ul;
  } else if (lead >= 0xe0) {
    extra = 2;
    cp = lead & 0x0ful;
  } else if (lead >= 0xc0) {
    extra = 1;
    cp = lead & 0x1ful;
  } else {
    (*index)++;
    return (wchar_t)lead;
  }

  for (int i = 1; i <= extra; i++) {
    unsigned char next = bytes[*index + i];
    if ((next & 0xc0) != 0x80) {
      (*index)++;
      return (wchar_t)lead;
    }
    cp = (cp << 6) | (next & 0x3ful);
  }
  *index += extra + 1;
  return (wchar_t)cp;
}

int mvwaddstr(WINDOW *win, int y, int x, const char *text) {
  if (!win || !text) return ERR;

  int col = x;
  int i = 0;
  while (text[i]) {
    wchar_t ch = utf8_decode(text, &i);
    int slots = gm_win_put_wchar(win, y, col, ch);
    col += slots;
  }
  return OK;
}

int mvwprintw(WINDOW *win, int y, int x, const char *fmt, ...) {
  char buffer[2048];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buffer, sizeof(buffer), fmt, args);
  va_end(args);
  return mvwaddstr(win, y, x, buffer);
}

int mvwhline(WINDOW *win, int y, int x, chtype ch, int n) {
  if (!win) return ERR;
  wchar_t wide = (wchar_t)(ch & 0x10ffffu);
  if (wide == 0) wide = L' ';
  gm_win_fill(win, y, x, wide, n);
  return OK;
}

int box(WINDOW *win, chtype verch, chtype horch) {
  if (!win || win->height < 2 || win->width < 2) return ERR;
  wchar_t vertical = (wchar_t)(verch & 0x10ffffu);
  wchar_t horizontal = (wchar_t)(horch & 0x10ffffu);
  if (vertical == 0) vertical = (wchar_t)ACS_VLINE;
  if (horizontal == 0) horizontal = (wchar_t)ACS_HLINE;

  int bottom = win->height - 1;
  int right = win->width - 1;

  gm_win_fill(win, 0, 1, horizontal, right - 1);
  gm_win_fill(win, bottom, 1, horizontal, right - 1);
  for (int row = 1; row < bottom; row++) {
    gm_win_put_wchar(win, row, 0, vertical);
    gm_win_put_wchar(win, row, right, vertical);
  }
  gm_win_put_wchar(win, 0, 0, (wchar_t)ACS_ULCORNER);
  gm_win_put_wchar(win, 0, right, (wchar_t)ACS_URCORNER);
  gm_win_put_wchar(win, bottom, 0, (wchar_t)ACS_LLCORNER);
  gm_win_put_wchar(win, bottom, right, (wchar_t)ACS_LRCORNER);
  return OK;
}

#else
typedef int gm_win_draw_unused;
#endif /* _WIN32 */
