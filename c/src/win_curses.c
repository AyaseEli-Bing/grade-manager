/**
 * Win32 控制台后端：设备初始化、窗口与字符网格。
 * 渲染模型：窗口自带字符网格，refresh 时叠加到屏幕网格并整块刷到控制台，
 * 因此 Win7 下既不闪烁也不需要 VT 序列支持。刷新上屏见 win_refresh.c。
 */
#include "win_curses.h"
#include "win_internal.h"

#ifdef _WIN32

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

WINDOW *stdscr = NULL;
int LINES = 0;
int COLS = 0;

HANDLE gm_out_handle = INVALID_HANDLE_VALUE;
static HANDLE in_handle = INVALID_HANDLE_VALUE;
static DWORD saved_out_mode = 0;
static DWORD saved_in_mode = 0;
int gm_cursor_visible = 0;

/* 颜色对表：0 号为默认，1..255 由 init_pair 填充 */
static short pair_fg[256];
static short pair_bg[256];

/* 屏幕网格（所有窗口叠加后的最终画面） */
GmCell *gm_screen_cells = NULL;
WINDOW *gm_current_win = NULL;
static int screen_dirty = 1;

const WINDOW *gm_win_current(void) { return gm_current_win; }
void gm_win_set_current(WINDOW *win) { gm_current_win = win; }

/* ---------- 颜色映射 ---------- */

static WORD color_bits(short color) {
  switch (color) {
    case COLOR_BLUE: return FOREGROUND_BLUE;
    case COLOR_GREEN: return FOREGROUND_GREEN;
    case COLOR_CYAN: return FOREGROUND_GREEN | FOREGROUND_BLUE;
    case COLOR_RED: return FOREGROUND_RED;
    case COLOR_MAGENTA: return FOREGROUND_RED | FOREGROUND_BLUE;
    case COLOR_YELLOW: return FOREGROUND_RED | FOREGROUND_GREEN;
    case COLOR_WHITE: return FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
    default: return 0; /* BLACK 或 -1 默认 */
  }
}

/** 把内部属性（颜色对 + 属性位）翻译成 Win32 控制台属性 */
static unsigned short to_console_attr(unsigned internal) {
  unsigned pair = GM_PAIR_OF(internal);
  unsigned flags = GM_ATTR_OF(internal);
  if (pair > 255) pair = 0;

  WORD attr = color_bits(pair_fg[pair]) | (WORD)(color_bits(pair_bg[pair]) << 4);
  if (flags & A_BOLD) attr |= FOREGROUND_INTENSITY;
  if (flags & A_DIM) attr &= (WORD)~FOREGROUND_INTENSITY;
  if (flags & A_UNDERLINE) attr |= COMMON_LVB_UNDERSCORE;
  /* 前景/背景对调：COMMON_LVB_REVERSE_VIDEO 在 Win7 上也生效 */
  if (flags & A_REVERSE) attr |= COMMON_LVB_REVERSE_VIDEO;
  return attr;
}

int start_color(void) { return OK; }

int use_default_colors(void) {
  /* Win7 控制台没有"透明背景"概念，用默认前景 + 默认背景近似 */
  return OK;
}

int has_colors(void) { return TRUE; }

int init_pair(short pair, short fg, short bg) {
  if (pair < 0 || pair > 255) return ERR;
  pair_fg[pair] = fg;
  pair_bg[pair] = bg;
  screen_dirty = 1;
  return OK;
}

/* ---------- 字符网格 ---------- */

int gm_win_usable(const WINDOW *win, int y, int x) {
  return win && win->cells && y >= 0 && y < win->height && x >= 0 && x < win->width;
}

/** 写入一个宽字符，返回占用的列数（宽字符 2 列，右半格标记为 0） */
int gm_win_put_wchar(WINDOW *win, int y, int x, wchar_t ch) {
  if (!gm_win_usable(win, y, x)) return 1;

  /* 双宽字符判定。注意 Windows 的 wchar_t 是 16 位，
     所以只能判定 BMP 内的范围（CJK 扩展 B 区等非 BMP 字符在 Windows 上
     本就需要代理对表示，单个 wchar_t 装不下，见 utf8_decode 的处理）。 */
  unsigned long cp = (unsigned long)ch;
  int wide = iswprint(ch) &&
             ((cp >= 0x1100 && cp <= 0x115f) || (cp >= 0x2e80 && cp <= 0xa4cf) ||
              (cp >= 0xac00 && cp <= 0xd7a3) || (cp >= 0xf900 && cp <= 0xfaff) ||
              (cp >= 0xfe30 && cp <= 0xfe6f) || (cp >= 0xff00 && cp <= 0xff60) ||
              (cp >= 0xffe0 && cp <= 0xffe6));
  int slots = wide ? 2 : 1;

  GmCell *cell = &win->cells[(size_t)y * (size_t)win->width + (size_t)x];
  cell->ch = ch;
  cell->attr = to_console_attr((unsigned)win->attrs);

  if (wide && x + 1 < win->width) {
    GmCell *next = cell + 1;
    next->ch = 0; /* 右半格 */
    next->attr = cell->attr;
  }
  return slots;
}

void gm_win_fill(WINDOW *win, int y, int x, wchar_t ch, int count) {
  for (int i = 0; i < count; i++) gm_win_put_wchar(win, y, x + i, ch);
}

static void win_fill_all(WINDOW *win, wchar_t ch) {
  if (!win || !win->cells) return;
  unsigned short attr = to_console_attr((unsigned)win->attrs);
  for (int i = 0; i < win->height * win->width; i++) {
    win->cells[i].ch = ch;
    win->cells[i].attr = attr;
  }
}

/* ---------- 设备 ---------- */

static void read_console_size(void) {
  CONSOLE_SCREEN_BUFFER_INFO info;
  if (GetConsoleScreenBufferInfo(gm_out_handle, &info)) {
    COLS = info.srWindow.Right - info.srWindow.Left + 1;
    LINES = info.srWindow.Bottom - info.srWindow.Top + 1;
  }
  if (COLS <= 0) COLS = 80;
  if (LINES <= 0) LINES = 25;
}

void gm_win_refresh_size(void) { read_console_size(); }

WINDOW *initscr(void) {
  gm_out_handle = GetStdHandle(STD_OUTPUT_HANDLE);
  in_handle = GetStdHandle(STD_INPUT_HANDLE);
  if (gm_out_handle == INVALID_HANDLE_VALUE) return NULL;

  read_console_size();
  SetConsoleTitleA("班级成绩管理系统");

  GetConsoleMode(gm_out_handle, &saved_out_mode);
  DWORD out_mode = saved_out_mode | ENABLE_PROCESSED_OUTPUT | ENABLE_WRAP_AT_EOL_OUTPUT;
  out_mode &= (DWORD)~ENABLE_VIRTUAL_TERMINAL_PROCESSING; /* Win7 不支持，显式关掉 */
  SetConsoleMode(gm_out_handle, out_mode);

  if (in_handle != INVALID_HANDLE_VALUE) {
    GetConsoleMode(in_handle, &saved_in_mode);
    DWORD in_mode = saved_in_mode | ENABLE_WINDOW_INPUT | ENABLE_EXTENDED_FLAGS;
    in_mode &= (DWORD)~(ENABLE_MOUSE_INPUT | ENABLE_QUICK_EDIT_MODE |
                        ENABLE_PROCESSED_INPUT); /* 关掉快速编辑，避免点选后卡死 */
    SetConsoleMode(in_handle, in_mode);
  }

  gm_screen_cells = calloc((size_t)(COLS * LINES), sizeof(GmCell));
  if (!gm_screen_cells) return NULL;
  for (int i = 0; i < COLS * LINES; i++) gm_screen_cells[i].ch = L' ';

  stdscr = calloc(1, sizeof(WINDOW));
  if (!stdscr) return NULL;
  stdscr->height = LINES;
  stdscr->width = COLS;
  stdscr->y = 0;
  stdscr->x = 0;
  stdscr->attrs = 0;
  stdscr->is_screen = 1;
  stdscr->cells = calloc((size_t)(COLS * LINES), sizeof(GmCell));
  if (!stdscr->cells) return NULL;
  win_fill_all(stdscr, L' ');

  gm_current_win = stdscr;
  for (int i = 0; i < 256; i++) {
    pair_fg[i] = COLOR_WHITE;
    pair_bg[i] = COLOR_BLACK;
  }
  pair_fg[0] = COLOR_WHITE;
  pair_bg[0] = COLOR_BLACK;

  curs_set(0);
  CONSOLE_CURSOR_INFO cursor;
  if (GetConsoleCursorInfo(gm_out_handle, &cursor)) {
    cursor.dwSize = 25;
    SetConsoleCursorInfo(gm_out_handle, &cursor);
  }
  SetConsoleOutputCP(GetConsoleOutputCP()); /* 保持当前代码页，输出走宽字符不受影响 */
  screen_dirty = 1;
  return stdscr;
}

int endwin(void) {
  if (gm_out_handle != INVALID_HANDLE_VALUE) {
    SetConsoleMode(gm_out_handle, saved_out_mode);
    CONSOLE_CURSOR_INFO cursor;
    if (GetConsoleCursorInfo(gm_out_handle, &cursor)) {
      cursor.bVisible = TRUE;
      SetConsoleCursorInfo(gm_out_handle, &cursor);
    }
  }
  if (in_handle != INVALID_HANDLE_VALUE) SetConsoleMode(in_handle, saved_in_mode);

  if (stdscr) {
    free(stdscr->cells);
    free(stdscr);
    stdscr = NULL;
  }
  free(gm_screen_cells);
  gm_screen_cells = NULL;
  gm_current_win = NULL;
  return OK;
}

int cbreak(void) { return OK; }
int noecho(void) { return OK; }
int keypad(WINDOW *win, int on) { (void)win; (void)on; return OK; }
int set_escdelay(int milliseconds) { (void)milliseconds; return OK; }

int curs_set(int visibility) {
  gm_cursor_visible = visibility;
  return OK;
}

int wresize(WINDOW *win, int height, int width) {
  if (!win) return ERR;
  read_console_size();
  if (win->is_screen) {
    height = LINES;
    width = COLS;
    GmCell *grown = realloc(win->cells, (size_t)(height * width) * sizeof(GmCell));
    if (!grown) return ERR;
    win->cells = grown;
    GmCell *screen = realloc(gm_screen_cells, (size_t)(height * width) * sizeof(GmCell));
    if (screen) gm_screen_cells = screen;
  } else {
    GmCell *grown = realloc(win->cells, (size_t)(height * width) * sizeof(GmCell));
    if (!grown) return ERR;
    win->cells = grown;
  }
  win->height = height;
  win->width = width;
  win_fill_all(win, L' ');
  screen_dirty = 1;
  return OK;
}

WINDOW *newwin(int height, int width, int y, int x) {
  if (height <= 0 || width <= 0) return NULL;
  WINDOW *win = calloc(1, sizeof(WINDOW));
  if (!win) return NULL;
  win->height = height;
  win->width = width;
  win->y = y;
  win->x = x;
  win->attrs = 0;
  win->cells = calloc((size_t)(height * width), sizeof(GmCell));
  if (!win->cells) {
    free(win);
    return NULL;
  }
  win_fill_all(win, L' ');
  return win;
}

int delwin(WINDOW *win) {
  if (!win || win->is_screen) return ERR;
  if (gm_current_win == win) gm_current_win = stdscr;
  free(win->cells);
  free(win);
  screen_dirty = 1;
  return OK;
}

int getmaxy(const WINDOW *win) { return win ? win->height : 0; }
int getmaxx(const WINDOW *win) { return win ? win->width : 0; }
int werase(WINDOW *win) {
  win_fill_all(win, L' ');
  if (win) {
    win->cur_y = 0;
    win->cur_x = 0;
  }
  return OK;
}
int clear(void) { return werase(stdscr); }
int touchwin(WINDOW *win) { (void)win; screen_dirty = 1; return OK; }
/* 刷新上屏实现在 win_refresh.c */

#else
/* 非 Windows 平台：本文件不产出任何符号，避免空翻译单元告警 */
typedef int gm_win_curses_unused;
#endif /* _WIN32 */
