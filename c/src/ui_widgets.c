/**
 * 交互控件：表单、确认框、提示框、选项、输入、路径、滚动列表。
 * 全部基于 ui.h 的绘制原语，颜色只通过 ui_role() 取语义角色。
 */
#include "ui.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

/* ---------- 通用窗口 ---------- */

static WINDOW *dialog_window(const char *title, int height, int width) {
  int maxy, maxx;
  ui_terminal_size(&maxy, &maxx);
  if (height > maxy - 2) height = maxy - 2;
  if (width > maxx - 4) width = maxx - 4;
  int y = (maxy - height) / 2;
  int x = (maxx - width) / 2;
  if (y < 1) y = 1;
  if (x < 2) x = 2;

  WINDOW *win = newwin(height, width, y, x);
  if (!win) return NULL;
  wattron(win, ui_role(UI_BORDER));
  box(win, 0, 0);
  wattroff(win, ui_role(UI_BORDER));
  if (title && title[0]) {
    wattron(win, ui_role(UI_TITLE));
    mvwprintw(win, 0, 2, " %s ", title);
    wattroff(win, ui_role(UI_TITLE));
  }
  keypad(win, TRUE);
  return win;
}

static void dialog_close(WINDOW *win) {
  delwin(win);
  touchwin(stdscr);
  refresh();
}

static void wrap_text(const char *text, int width, int *out_lines, char lines[][256], int max_lines) {
  int count = 0;
  const char *rest = text ? text : "";
  char buf[512];
  snprintf(buf, sizeof(buf), "%s", rest);

  char *token = strtok(buf, "\n");
  while (token && count < max_lines) {
    int len = ui_str_cols(token);
    if (len <= width) {
      snprintf(lines[count++], 256, "%s", token);
    } else {
      /* 按显示列宽切行 */
      int start = 0;
      while (start < (int)strlen(token) && count < max_lines) {
        char piece[256];
        ui_trunc(token + start, width, piece, sizeof(piece));
        snprintf(lines[count++], 256, "%s", piece);
        start += (int)strlen(piece);
      }
    }
    token = strtok(NULL, "\n");
  }
  *out_lines = count;
}

/* ---------- 提示与确认 ---------- */

void ui_message(const char *title, const char *message) {
  char lines[12][256];
  int n = 0;
  wrap_text(message, 60, &n, lines, 12);
  int height = n + 5;
  int width = 66;
  WINDOW *win = dialog_window(title, height, width);
  if (!win) return;

  for (int i = 0; i < n; i++) mvwprintw(win, 2 + i, 3, "%s", lines[i]);
  wattron(win, ui_role(UI_DIM));
  mvwprintw(win, height - 2, 3, "按任意键继续");
  wattroff(win, ui_role(UI_DIM));
  wrefresh(win);
  wgetch(win);
  dialog_close(win);
}

void ui_error(const char *message) { ui_message("出错了", message); }

int ui_confirm(const char *title, const char *message) {
  char lines[12][256];
  int n = 0;
  wrap_text(message, 58, &n, lines, 12);
  int height = n + 5;
  int width = 64;
  WINDOW *win = dialog_window(title, height, width);
  if (!win) return 0;

  for (int i = 0; i < n; i++) {
    wattron(win, ui_role(UI_WARN));
    mvwprintw(win, 2 + i, 3, "%s", lines[i]);
    wattroff(win, ui_role(UI_WARN));
  }
  mvwprintw(win, height - 2, 3, "[Enter] 确定    [Esc] 取消");
  wrefresh(win);

  int result = 0;
  for (;;) {
    int ch = wgetch(win);
    if (ch == '\n' || ch == KEY_ENTER || ch == 'y' || ch == 'Y') {
      result = 1;
      break;
    }
    if (ch == KEY_ESC || ch == 'n' || ch == 'N' || ch == 'q') break;
  }
  dialog_close(win);
  return result;
}

/* ---------- 选项与输入 ---------- */

int ui_pick(const char *title, const char **options, int n, int initial) {
  if (n <= 0) return -1;
  int height = n + 6;
  int width = 56;
  for (int i = 0; i < n; i++) {
    int w = ui_str_cols(options[i]) + 12;
    if (w > width) width = w;
  }
  WINDOW *win = dialog_window(title, height, width);
  if (!win) return -1;

  int active = initial >= 0 && initial < n ? initial : 0;
  for (;;) {
    for (int i = 0; i < n; i++) {
      int attr = i == active ? ui_role(UI_SELECTED) : ui_role(UI_TEXT);
      wattron(win, attr);
      mvwhline(win, 2 + i, 2, ' ', width - 4);
      mvwprintw(win, 2 + i, 3, "%s", options[i]);
      wattroff(win, attr);
    }
    wattron(win, ui_role(UI_DIM));
    mvwprintw(win, height - 2, 3, "上下键选择，Enter 确认，Esc 取消");
    wattroff(win, ui_role(UI_DIM));
    wrefresh(win);

    int ch = wgetch(win);
    if (ch == KEY_UP) active = (active - 1 + n) % n;
    else if (ch == KEY_DOWN) active = (active + 1) % n;
    else if (ch == '\n' || ch == KEY_ENTER) {
      dialog_close(win);
      return active;
    } else if (ch == KEY_ESC || ch == 'q') {
      dialog_close(win);
      return -1;
    }
  }
}

/** 在窗口内编辑一行文本：返回 1 表示内容已变更 */
static int field_key(char *buf, size_t bufsz, int ch) {
  size_t len = strlen(buf);
  if ((ch == KEY_BACKSPACE || ch == 127 || ch == '\b') && len > 0) {
    /* 退格按字节回退，遇到 UTF-8 续字节继续回退到首字节 */
    size_t at = len - 1;
    while (at > 0 && ((unsigned char)buf[at] & 0xc0) == 0x80) at--;
    buf[at] = '\0';
    return 1;
  }
  if (ch >= 32 && ch <= 255 && ch != 127) {
    if (len + 1 >= bufsz) return 0;
    buf[len] = (char)ch;
    buf[len + 1] = '\0';
    return 1;
  }
  return 0;
}

int ui_input(const char *title, const char *label, char *buf, size_t bufsz) {
  WINDOW *win = dialog_window(title, 6, 56);
  if (!win) return 0;
  char work[256];
  snprintf(work, sizeof(work), "%s", buf ? buf : "");

  curs_set(1);
  int result = 0;
  for (;;) {
    mvwhline(win, 2, 2, ' ', 52);
    mvwprintw(win, 2, 3, "%s", label ? label : "");
    mvwhline(win, 3, 2, ' ', 52);
    wattron(win, ui_role(UI_ACCENT));
    mvwprintw(win, 3, 3, "%s", work);
    wattroff(win, ui_role(UI_ACCENT));
    wattron(win, ui_role(UI_DIM));
    mvwprintw(win, 4, 3, "Enter 确定，Esc 取消");
    wattroff(win, ui_role(UI_DIM));
    int col = 3 + ui_str_cols(work);
    if (col > 54) col = 54;
    wmove(win, 3, col);
    wrefresh(win);

    int ch = wgetch(win);
    if (ch == '\n' || ch == KEY_ENTER) {
      snprintf(buf, bufsz, "%s", work);
      result = 1;
      break;
    }
    if (ch == KEY_ESC) break;
    field_key(work, sizeof(work), ch);
  }
  curs_set(0);
  dialog_close(win);
  return result;
}

int ui_filepath(const char *title, const char *default_name, char *path, size_t pathsz, int must_exist) {
  char work[512];
  snprintf(work, sizeof(work), "%s", default_name ? default_name : "");

  for (;;) {
    if (!ui_input(title, "文件路径（相对当前目录或绝对路径）：", work, sizeof(work))) return 0;
    if (work[0] == '\0') {
      ui_error("路径不能为空。");
      continue;
    }
    if (must_exist) {
      struct stat info;
      if (stat(work, &info) != 0) {
        ui_error("文件不存在，请检查路径。");
        continue;
      }
    }
    snprintf(path, pathsz, "%s", work);
    return 1;
  }
}

/* ---------- 表单 ---------- */

int ui_form(const char *title, const char **labels, char **out, const size_t *widths, int n) {
  if (n <= 0) return 0;
  int height = n * 2 + 5;
  int width = 60;
  for (int i = 0; i < n; i++) {
    int w = ui_str_cols(labels[i]) + 26;
    if (w > width) width = w;
  }
  WINDOW *win = dialog_window(title, height, width);
  if (!win) return 0;

  int active = 0;
  int result = 0;
  curs_set(1);
  for (;;) {
    for (int i = 0; i < n; i++) {
      int row = 2 + i * 2;
      int attr = i == active ? ui_role(UI_HEADER) : ui_role(UI_DIM);
      wattron(win, attr);
      mvwprintw(win, row, 3, "%s", labels[i]);
      wattroff(win, attr);

      mvwhline(win, row + 1, 3, ' ', width - 6);
      int field_attr = i == active ? ui_role(UI_ACCENT) : ui_role(UI_TEXT);
      wattron(win, field_attr);
      mvwprintw(win, row + 1, 3, "%s", out[i]);
      wattroff(win, field_attr);
      if (i == active) {
        wattron(win, ui_role(UI_ACCENT));
        mvwaddstr(win, row + 1, 3 + ui_str_cols(out[i]), "_");
        wattroff(win, ui_role(UI_ACCENT));
      }
    }
    wattron(win, ui_role(UI_DIM));
    mvwprintw(win, height - 2, 3, "上下键切换字段，Enter 保存，Esc 取消");
    wattroff(win, ui_role(UI_DIM));
    wrefresh(win);

    int ch = wgetch(win);
    if (ch == KEY_UP) active = (active - 1 + n) % n;
    else if (ch == KEY_DOWN || ch == '\t') active = (active + 1) % n;
    else if (ch == '\n' || ch == KEY_ENTER) {
      result = 1;
      break;
    } else if (ch == KEY_ESC) break;
    else if (field_key(out[active], widths[active], ch)) {
      /* 内容已变更，下一轮重绘 */
    }
  }
  curs_set(0);
  dialog_close(win);
  return result;
}

/* 滚动列表见 ui_list.c */
