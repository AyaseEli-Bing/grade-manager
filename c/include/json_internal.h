/**
 * JSON 模块内部契约 —— 仅供 json*.c 使用，不属于公开 API。
 * 拆分文件是为了遵守「单文件 ≤ 300 行」，Parser 结构因此需要共享。
 */
#ifndef GM_JSON_INTERNAL_H
#define GM_JSON_INTERNAL_H

#include <stddef.h>
#include "json.h"

#define JSON_MAX_DEPTH 64

typedef struct {
  const char *text;
  size_t len;
  size_t pos;
  int failed;
} Parser;

void json_skip_space(Parser *parser);
int json_fail_at(Parser *parser, const char *message);

/** 扫描并解码一个 JSON 字符串，返回 malloc 出来的字节串；失败返回 NULL */
char *json_scan_string(Parser *parser);

/** 扫描任意值（供数组/对象递归调用） */
Json *json_scan_value(Parser *parser, int depth);

#endif /* GM_JSON_INTERNAL_H */
