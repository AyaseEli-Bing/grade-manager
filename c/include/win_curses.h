/**
 * 最小 curses 兼容层（Windows 专用）。
 *
 * 只在 Win7 就存在的 Win32 控制台 API 之上实现视图层用到的那部分 curses 接口，
 * 因此业务代码无需任何 #ifdef：ui.h 会按平台选择 <curses.h> 或本头文件。
 *
 * 关键取舍：
 *  - 输出走 WriteConsoleOutputW（宽字符），保证 Win7 控制台下中文可靠显示；
 *    Win7 控制台不支持 ANSI/VT 转义序列，所以绝不能靠转义字符上色。
 *  - 宏的数值与 ncurses 并不一致，但视图层只使用宏名，不依赖具体数值。
 */
#ifndef GM_WIN_CURSES_H
#define GM_WIN_CURSES_H

#ifdef _WIN32

#include <stdarg.h>
#include <stddef.h>
#include <wchar.h>

typedef unsigned int chtype;

/* ---------- 颜色 ---------- */
enum {
  COLOR_BLACK = 0,
  COLOR_BLUE,
  COLOR_GREEN,
  COLOR_CYAN,
  COLOR_RED,
  COLOR_MAGENTA,
  COLOR_YELLOW,
  COLOR_WHITE
};

/* ---------- 属性位 ---------- */
#define A_NORMAL 0u
#define A_BOLD 0x1u
#define A_DIM 0x2u
#define A_REVERSE 0x4u
#define A_UNDERLINE 0x8u
#define A_ATTR_MASK 0x0fu

/* 颜色对索引占高位，低位放属性位 */
#define COLOR_PAIR(n) ((chtype)((unsigned)(n) << 8))
#define GM_PAIR_OF(a) (((unsigned)(a) >> 8) & 0xffu)
#define GM_ATTR_OF(a) ((unsigned)(a) & A_ATTR_MASK)

/* ---------- 常量 ---------- */
#define OK 0
#define ERR (-1)
#define TRUE 1
#define FALSE 0

struct _win_st;
typedef struct _win_st WINDOW;

extern WINDOW *stdscr;
extern int LINES;
extern int COLS;

/* ---------- 按键（数值与 ncurses 不同，视图层只用宏名） ---------- */
#define KEY_CODE_YES 0x100
#define KEY_DOWN 0x102
#define KEY_UP 0x103
#define KEY_LEFT 0x104
#define KEY_RIGHT 0x105
#define KEY_HOME 0x106
#define KEY_BACKSPACE 0x107
#define KEY_NPAGE 0x108
#define KEY_PPAGE 0x109
#define KEY_END 0x10a
#define KEY_ENTER 0x10b
#define KEY_RESIZE 0x10c

/* ---------- 制表字符（用 Unicode 码点表示，addch 按码点解释） ---------- */
#define ACS_HLINE 0x2500
#define ACS_VLINE 0x2502
#define ACS_ULCORNER 0x250c
#define ACS_URCORNER 0x2510
#define ACS_LLCORNER 0x2514
#define ACS_LRCORNER 0x2518

/* ---------- 生命周期 ---------- */
WINDOW *initscr(void);
int endwin(void);
int start_color(void);
int use_default_colors(void);
int has_colors(void);
int init_pair(short pair, short fg, short bg);
int cbreak(void);
int noecho(void);
int keypad(WINDOW *win, int on);
int curs_set(int visibility);
int set_escdelay(int milliseconds);

/* ---------- 窗口 ---------- */
WINDOW *newwin(int height, int width, int y, int x);
int delwin(WINDOW *win);
int wresize(WINDOW *win, int height, int width);
int getmaxy(const WINDOW *win);
int getmaxx(const WINDOW *win);
int wmove(WINDOW *win, int y, int x);
int werase(WINDOW *win);
int clear(void);
int touchwin(WINDOW *win);
int refresh(void);
int wrefresh(WINDOW *win);

/* ---------- 绘制 ---------- */
int wattron(WINDOW *win, int attrs);
int wattroff(WINDOW *win, int attrs);
int mvwaddch(WINDOW *win, int y, int x, const chtype ch);
int mvwaddstr(WINDOW *win, int y, int x, const char *text);
int mvwprintw(WINDOW *win, int y, int x, const char *fmt, ...);
int mvwhline(WINDOW *win, int y, int x, chtype ch, int n);
int box(WINDOW *win, chtype verch, chtype horch);

/* ---------- 输入 ---------- */
int getch(void);
int wgetch(WINDOW *win);

/* 内部共享声明（字符网格结构体等）见 win_internal.h */

#endif /* _WIN32 */
#endif /* GM_WIN_CURSES_H */
