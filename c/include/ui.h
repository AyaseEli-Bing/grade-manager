/**
 * 终端界面层（ncurses）。
 *
 * 【项目铁律的 C 版映射】业务代码禁止直接调用 init_pair / 硬编码颜色编号，
 * 一律通过 ui_role() 取「语义角色」，颜色定义集中在 ui_theme_init() 一处。
 * 这条规则由 scripts/verify-c.sh 静态检查。
 */
#ifndef GM_UI_H
#define GM_UI_H

#include <stddef.h>

#ifdef _WIN32
/* Windows（含 Win7）没有 ncurses，改用自带的 Win32 控制台后端。
   两者接口一致，因此视图层无需任何 #ifdef。 */
#include "win_curses.h"
#else
#include <curses.h>
#endif

#include "model.h"

/* 语义颜色角色——相当于设计 Token 层 */
enum {
  UI_TEXT = 0,
  UI_DIM,
  UI_ACCENT,
  UI_SUCCESS,
  UI_WARN,
  UI_DANGER,
  UI_HEADER,
  UI_SELECTED,
  UI_STATUS,
  UI_TITLE,
  UI_BAR,
  UI_BAR_FAIL,
  UI_BORDER,
  UI_ROLE_COUNT
};

void ui_theme_init(void);
int ui_role(int role); /* 返回可直接 or 进 attron 的属性位 */

/* Esc 键 ncurses 没有常量，全项目统一用这个 */
#define KEY_ESC 27

/* ---------- 生命周期与布局 ---------- */

int ui_init(void);
void ui_shutdown(void);
void ui_terminal_size(int *rows, int *cols);

/* ---------- 文本度量（中文在终端里占 2 列，必须按显示宽度排版） ---------- */

/** 字符串的显示列宽；CJK 与全角标点计 2 列 */
int ui_str_cols(const char *text);

/** 按显示列宽截断，超出部分以「…」结尾。out 至少要有 max_cols*4+8 字节 */
void ui_trunc(const char *text, int max_cols, char *out, size_t outsz);

/**
 * 在固定列宽内打印一格文本：align_right=1 时右对齐，超出自动截断。
 * 表格排版一律走这个函数，不要直接 mvwprintw。
 */
void ui_col(WINDOW *win, int y, int x, int width, int align_right, const char *text, int role);

/* ---------- 绘制 ---------- */

void ui_header(WINDOW *win, const char *title, const char *subtitle);
void ui_footer(WINDOW *win, const char *hint);
void ui_hline(WINDOW *win, int y, int x, int width);

/** 水平条形图：ratio 为 0~1 */
void ui_bar(WINDOW *win, int y, int x, int width, double ratio, int role);

/**
 * 极简折线图（字符画）：values 中 -1 表示缺考（断开）。
 * labels 可为 NULL；为 NULL 时不画横轴文字。
 */
void ui_line_chart(WINDOW *win, int y, int x, int width, int height,
                   const double *values, int n, const char **labels);

/* ---------- 交互控件 ---------- */

/** 表单：labels 为字段标签，out 为等长的输出缓冲区，widths 为每个缓冲区的字节数。返回 1 确定 / 0 取消 */
int ui_form(const char *title, const char **labels, char **out, const size_t *widths, int n);

/** 二次确认，返回 1 确定 / 0 取消 */
int ui_confirm(const char *title, const char *message);

/** 信息提示，任意键关闭 */
void ui_message(const char *title, const char *message);
void ui_error(const char *message);

/** 从一组选项中挑选，返回下标；-1 表示取消 */
int ui_pick(const char *title, const char **options, int n, int initial);

/** 单行输入，返回 1 确定 / 0 取消 */
int ui_input(const char *title, const char *label, char *buf, size_t bufsz);

/** 路径输入（带默认名预填与存在性校验），返回 1 确定 / 0 取消 */
int ui_filepath(const char *title, const char *default_name, char *path, size_t pathsz, int must_exist);

/** 列表对按键的处理结果 */
typedef enum {
  LIST_CONTINUE = 0, /* 继续循环 */
  LIST_QUIT,         /* 放弃选择，返回 -1 */
  LIST_DONE,         /* 确认当前选择，返回下标 */
  LIST_REFRESH       /* 条目集合变了（增删改/换筛选），需要重新进入列表 */
} ListAction;

/** ui_list 在收到 LIST_REFRESH 时的返回值：调用方应重新调用 ui_list 以刷新数量与标题 */
#define UI_LIST_REFRESH (-2)

/** 最近一次列表退出时的光标位置，用于重新进入时续用而不会跳回顶部 */
int ui_list_last_index(void);

/**
 * 可滚动的选择列表，是四个视图的主干。
 *
 * @param render  画第 index 行（y 为窗口内相对行）
 * @param on_key  按键回调：导航键（上下/翻页/首尾）由 ui_list 自己处理，
 *                其余键交给调用方；回调可以修改 *index（如删除后上移）
 * @param ctx     透传给两个回调
 * @return 选中的下标；-1 表示用户按 q/Esc 放弃
 */
int ui_list(const char *title, const char *subtitle, int count, int initial,
            void (*draw_columns)(WINDOW *win, int y, void *ctx),
            void (*render)(WINDOW *win, int y, int index, int selected, void *ctx),
            ListAction (*on_key)(int key, int *index, void *ctx), void *ctx);

#endif /* GM_UI_H */
