/**
 * Win32 后端的刷新上屏：把窗口网格叠加到屏幕网格，再一次性刷到控制台。
 *
 * 之所以用「整块 WriteConsoleOutputW」而不是逐字符输出：
 *  1. Win7 控制台不支持 ANSI/VT 序列，无法用转义字符定位与上色；
 *  2. 整块写入由控制台自己完成，天然不闪烁，中文宽字符也按 2 列正确占位。
 */
#include "win_curses.h"
#include "win_internal.h"

#ifdef _WIN32

#include <stdlib.h>
#include <windows.h>

/* 设备与屏幕状态定义在 win_curses.c，这里通过内部头文件声明共享 */
extern HANDLE gm_out_handle;
extern GmCell *gm_screen_cells;
extern WINDOW *gm_current_win;
extern int gm_cursor_visible;

/** 把窗口网格叠加到屏幕网格 */
static void blit_to_screen(WINDOW *win) {
  if (!win || !win->cells || !gm_screen_cells) return;
  for (int row = 0; row < win->height; row++) {
    int screen_row = win->y + row;
    if (screen_row < 0 || screen_row >= LINES) continue;
    for (int col = 0; col < win->width; col++) {
      int screen_col = win->x + col;
      if (screen_col < 0 || screen_col >= COLS) continue;
      const GmCell *src = &win->cells[(size_t)row * (size_t)win->width + (size_t)col];
      if (src->ch == 0) continue; /* 宽字符右半格已由左半格写入，跳过以免覆盖 */
      gm_screen_cells[(size_t)screen_row * (size_t)COLS + (size_t)screen_col] = *src;
    }
  }
}

/** 把屏幕网格的指定区域刷到控制台；height<=0 表示整屏 */
static void flush_screen(int top, int left, int height, int width) {
  if (!gm_screen_cells || gm_out_handle == INVALID_HANDLE_VALUE) return;
  if (height <= 0 || width <= 0) {
    top = 0;
    left = 0;
    height = LINES;
    width = COLS;
  }
  if (top < 0) top = 0;
  if (left < 0) left = 0;
  if (top + height > LINES) height = LINES - top;
  if (left + width > COLS) width = COLS - left;
  if (height <= 0 || width <= 0) return;

  CHAR_INFO *buffer = calloc((size_t)(height * width), sizeof(CHAR_INFO));
  if (!buffer) return;

  for (int row = 0; row < height; row++) {
    for (int col = 0; col < width; col++) {
      const GmCell *src =
          &gm_screen_cells[(size_t)(top + row) * (size_t)COLS + (size_t)(left + col)];
      CHAR_INFO *dst = &buffer[(size_t)row * (size_t)width + (size_t)col];
      dst->Char.UnicodeChar = src->ch ? src->ch : L' ';
      dst->Attributes = src->attr;
    }
  }

  COORD size = {(SHORT)width, (SHORT)height};
  COORD origin = {0, 0};
  SMALL_RECT region = {(SHORT)left, (SHORT)top, (SHORT)(left + width - 1),
                       (SHORT)(top + height - 1)};
  WriteConsoleOutputW(gm_out_handle, buffer, size, origin, &region);
  free(buffer);
}

/** 按需显示/隐藏并定位控制台光标（输入框需要看得见插入点） */
static void place_console_cursor(void) {
  CONSOLE_CURSOR_INFO cursor;
  if (!gm_cursor_visible || !gm_current_win) {
    if (GetConsoleCursorInfo(gm_out_handle, &cursor) && cursor.bVisible) {
      cursor.bVisible = FALSE;
      SetConsoleCursorInfo(gm_out_handle, &cursor);
    }
    return;
  }
  if (GetConsoleCursorInfo(gm_out_handle, &cursor) && !cursor.bVisible) {
    cursor.bVisible = TRUE;
    SetConsoleCursorInfo(gm_out_handle, &cursor);
  }
  COORD position = {(SHORT)(gm_current_win->x + gm_current_win->cur_x),
                    (SHORT)(gm_current_win->y + gm_current_win->cur_y)};
  if (position.X < 0) position.X = 0;
  if (position.Y < 0) position.Y = 0;
  if (position.X >= (SHORT)COLS) position.X = (SHORT)(COLS - 1);
  if (position.Y >= (SHORT)LINES) position.Y = (SHORT)(LINES - 1);
  SetConsoleCursorPosition(gm_out_handle, position);
}

int refresh(void) {
  blit_to_screen(stdscr);
  flush_screen(0, 0, 0, 0);
  place_console_cursor();
  return OK;
}

int wrefresh(WINDOW *win) {
  if (!win) return ERR;
  if (win->is_screen) return refresh();

  blit_to_screen(win);
  flush_screen(win->y, win->x, win->height, win->width);
  gm_win_set_current(win);
  place_console_cursor();
  return OK;
}

#else
typedef int gm_win_refresh_unused;
#endif /* _WIN32 */
