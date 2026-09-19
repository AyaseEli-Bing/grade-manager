/**
 * Win32 后端的内部契约 —— 仅供 win_*.c 使用，视图层不要包含。
 */
#ifndef GM_WIN_INTERNAL_H
#define GM_WIN_INTERNAL_H

#ifdef _WIN32

#include "win_curses.h"

/** 字符网格单元；ch == 0 表示这是宽字符的右半格，刷新时跳过 */
typedef struct {
  wchar_t ch;
  unsigned short attr;
} GmCell;

struct _win_st {
  int height;
  int width;
  int y;      /* 左上角在屏幕上的位置 */
  int x;
  int cur_y;  /* 窗口内光标 */
  int cur_x;
  int attrs;  /* 当前属性（颜色对索引 + 属性位） */
  GmCell *cells;
  int is_screen;
};

/** 重新读取控制台尺寸并同步 LINES / COLS（窗口尺寸变化时调用） */
void gm_win_refresh_size(void);

/* 设备与屏幕状态：定义在 win_curses.c，win_refresh.c 共享 */
extern void *gm_out_handle;
extern GmCell *gm_screen_cells;
extern WINDOW *gm_current_win;
extern int gm_cursor_visible;

int gm_win_usable(const WINDOW *win, int y, int x);
int gm_win_put_wchar(WINDOW *win, int y, int x, wchar_t ch);
void gm_win_fill(WINDOW *win, int y, int x, wchar_t ch, int count);
const WINDOW *gm_win_current(void);
void gm_win_set_current(WINDOW *win);

#endif /* _WIN32 */
#endif /* GM_WIN_INTERNAL_H */
