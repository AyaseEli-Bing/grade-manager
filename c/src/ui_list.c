/**
 * 滚动列表 —— 四个视图共用的主干控件。
 * 导航键由本文件处理，其余按键交给调用方回调（用于新增 / 删除 / 编辑）。
 */
#include "ui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


static int last_visible_index = 0;

int ui_list_last_index(void) { return last_visible_index; }

int ui_list(const char *title, const char *subtitle, int count, int initial,
            void (*draw_columns)(WINDOW *win, int y, void *ctx),
            void (*render)(WINDOW *win, int y, int index, int selected, void *ctx),
            ListAction (*on_key)(int key, int *index, void *ctx), void *ctx) {
  int maxy, maxx;
  ui_terminal_size(&maxy, &maxx);
  WINDOW *win = newwin(maxy, maxx, 0, 0);
  if (!win) return -1;
  keypad(win, TRUE);

  const int top = 3;                     /* 页眉占 2 行 + 1 行表头 */
  const int bottom_pad = 2;              /* 页脚 1 行 + 底部留白 */
  const int visible = maxy - top - bottom_pad;
  if (visible <= 0) {
    delwin(win);
    return -1;
  }

  int selected = initial >= 0 && initial < count ? initial : 0;
  int offset = 0;

  for (;;) {
    werase(win);
    ui_header(win, title, subtitle);
    if (draw_columns && count > 0) {
      wattron(win, ui_role(UI_HEADER));
      mvwhline(win, 2, 0, ' ', maxx);
      draw_columns(win, 2, ctx);
      wattroff(win, ui_role(UI_HEADER));
    }

    if (count <= 0) {
      wattron(win, ui_role(UI_DIM));
      mvwprintw(win, top + 1, 3, "暂无数据。");
      wattroff(win, ui_role(UI_DIM));
    } else {
      if (selected < offset) offset = selected;
      if (selected >= offset + visible) offset = selected - visible + 1;
      if (offset < 0) offset = 0;

      for (int i = 0; i < visible && offset + i < count; i++) {
        render(win, top + i, offset + i, offset + i == selected, ctx);
      }
    }

    char hint[160];
    snprintf(hint, sizeof(hint), "%s  第 %d/%d 条  上下键移动  Enter 打开  q 返回",
             subtitle ? "" : "", count <= 0 ? 0 : selected + 1, count);
    ui_footer(win, hint);
    wrefresh(win);

    int ch = wgetch(win);
    if (ch == KEY_RESIZE) {
      ui_terminal_size(&maxy, &maxx);
      wresize(win, maxy, maxx);
      continue;
    }

    /* 先给调用方处理的机会：这样"正在编辑单元格"这类模态状态可以抢在导航键之前
       提交内容（否则按方向键会把未提交的输入丢掉），也能拦截 Enter 做自定义动作。 */
    if (on_key) {
      ListAction action = on_key(ch, &selected, ctx);
      if (action == LIST_QUIT) {
        delwin(win);
        return -1;
      }
      if (action == LIST_DONE) {
        delwin(win);
        return selected;
      }
      last_visible_index = selected;
      if (action == LIST_REFRESH) {
        /* 条目集合已变化：必须退出让调用方重新进入，否则条目数还是旧快照 */
        delwin(win);
        return UI_LIST_REFRESH;
      }
      if (selected >= count && count > 0) selected = count - 1;
      if (selected < 0) selected = 0;
    }

    /* 调用方没有消费这个按键，走列表的默认行为 */
    if (ch == KEY_UP) {
      if (selected > 0) selected--;
      continue;
    }
    if (ch == KEY_DOWN) {
      if (selected < count - 1) selected++;
      continue;
    }
    if (ch == KEY_PPAGE || ch == KEY_HOME) {
      selected = 0;
      continue;
    }
    if (ch == KEY_NPAGE || ch == KEY_END) {
      selected = count > 0 ? count - 1 : 0;
      continue;
    }
    if (ch == 'q' || ch == KEY_ESC) {
      last_visible_index = selected;
      delwin(win);
      return -1;
    }
    if (ch == '\n' || ch == KEY_ENTER) {
      last_visible_index = selected;
      delwin(win);
      /* 列表为空时没有可选项，返回 -1 而不是 0——否则调用方会去取 index[0]，
         那是一段未初始化内存，会造成越界访问。 */
      return count > 0 ? selected : -1;
    }
  }
}
