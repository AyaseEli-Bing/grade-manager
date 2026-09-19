/**
 * 视图层共用：状态栏、关键词匹配、落盘。
 */
#include "views.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "ui.h"

void view_status(AppCtx *ctx, const char *text) {
  if (!ctx) return;
  snprintf(ctx->status, sizeof(ctx->status), "%s", text ? text : "");
  if (ctx->set_status) ctx->set_status(ctx, ctx->status);
}

int view_match(const char *text, const char *keyword) {
  if (!keyword || keyword[0] == '\0') return 1;
  if (!text) return 0;
  const unsigned char *hay = (const unsigned char *)text;
  const unsigned char *needle = (const unsigned char *)keyword;

  size_t needle_len = strlen(keyword);
  for (size_t i = 0; hay[i]; i++) {
    size_t j = 0;
    while (j < needle_len && hay[i + j] &&
           tolower(hay[i + j]) == tolower(needle[j])) {
      j++;
    }
    if (j == needle_len) return 1;
  }
  return 0;
}

int view_save(AppCtx *ctx) {
  if (!ctx || !ctx->gb) return -1;
  char err[256] = "";
  if (gb_save_file(ctx->gb, ctx->data_path, err, sizeof(err)) != 0) {
    ui_error(err);
    view_status(ctx, "保存失败");
    return -1;
  }
  ctx->dirty = 0;
  view_status(ctx, "已保存");
  return 0;
}
