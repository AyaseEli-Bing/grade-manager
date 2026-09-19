/**
 * 四个业务视图 + 考试管理。
 * 每个视图自己接管键盘并循环，直到用户按 q / Esc 返回主菜单。
 */
#ifndef GM_VIEWS_H
#define GM_VIEWS_H

#include "model.h"

typedef struct AppCtx AppCtx;

struct AppCtx {
  GradeBook *gb;
  char search[64];   /* 当前视图的筛选关键词，切换视图时清空 */
  char data_path[512];
  int dirty;         /* 有改动尚未落盘；主循环据此触发自动保存 */
  void (*set_status)(AppCtx *ctx, const char *text);
  char status[256];
};

/** 写入状态栏文案（同时更新 ctx->status，供主循环在必要时显示） */
void view_status(AppCtx *ctx, const char *text);

/** 关键词匹配（忽略大小写）；空关键词恒匹配 */
int view_match(const char *text, const char *keyword);

void view_students(AppCtx *ctx);
void view_scores(AppCtx *ctx);
void view_stats(AppCtx *ctx);
void view_subjects(AppCtx *ctx);
void view_exams(AppCtx *ctx);

/** 各视图共用的落盘入口：写入 ctx->data_path 并更新 ctx->dirty 与状态栏 */
int view_save(AppCtx *ctx);

#endif /* GM_VIEWS_H */
