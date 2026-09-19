/**
 * JSON 值扫描：对象 / 数组 / 数字 / 字面量。字符串扫描见 json_scan_string.c。
 */
#include "json_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 错误信息用静态缓冲，单线程场景足够；出错位置写进文本里 */
static char errbuf[256];

static Json *node_new(JsonType type) {
  Json *value = calloc(1, sizeof(Json));
  if (!value) return NULL;
  value->type = type;
  return value;
}

void json_skip_space(Parser *parser) {
  while (parser->pos < parser->len) {
    char c = parser->text[parser->pos];
    if (c == ' ' || c == '\t' || c == '\n' || c == '\r') parser->pos++;
    else break;
  }
}

int json_fail_at(Parser *parser, const char *message) {
  if (!parser->failed) {
    parser->failed = 1;
    snprintf(errbuf, sizeof(errbuf), "第 %lu 个字符附近：%s",
             (unsigned long)(parser->pos + 1), message);
  }
  return -1;
}

/* ---------- 数字 ---------- */

static Json *scan_number(Parser *parser) {
  const size_t start = parser->pos;
  if (parser->pos < parser->len && parser->text[parser->pos] == '-') parser->pos++;

  int digits = 0;
  while (parser->pos < parser->len) {
    char c = parser->text[parser->pos];
    if (c >= '0' && c <= '9') {
      digits++;
      parser->pos++;
    } else if (c == '.' || c == 'e' || c == 'E' || c == '+' || c == '-') {
      parser->pos++;
    } else break;
  }
  if (digits == 0) {
    json_fail_at(parser, "数字格式不正确");
    return NULL;
  }

  size_t count = parser->pos - start;
  char buf[64];
  if (count >= sizeof(buf)) count = sizeof(buf) - 1;
  memcpy(buf, parser->text + start, count);
  buf[count] = '\0';

  Json *value = node_new(JSON_NUMBER);
  if (!value) return NULL;
  value->num = strtod(buf, NULL);
  return value;
}

/* ---------- 数组与对象 ---------- */

static int literal(Parser *parser, const char *word) {
  size_t n = strlen(word);
  if (parser->pos + n > parser->len) return 0;
  return strncmp(parser->text + parser->pos, word, n) == 0;
}

static Json *scan_array(Parser *parser, int depth) {
  Json *array = node_new(JSON_ARRAY);
  if (!array) return NULL;
  parser->pos++; /* [ */

  json_skip_space(parser);
  if (parser->pos < parser->len && parser->text[parser->pos] == ']') {
    parser->pos++;
    return array;
  }
  for (;;) {
    Json *item = json_scan_value(parser, depth + 1);
    if (!item) {
      json_free(array);
      return NULL;
    }
    json_push(array, item);
    json_skip_space(parser);
    if (parser->pos < parser->len && parser->text[parser->pos] == ',') {
      parser->pos++;
      continue;
    }
    if (parser->pos < parser->len && parser->text[parser->pos] == ']') {
      parser->pos++;
      return array;
    }
    json_free(array);
    json_fail_at(parser, "数组缺少 ] 或分隔符");
    return NULL;
  }
}

static Json *scan_object(Parser *parser, int depth) {
  Json *object = node_new(JSON_OBJECT);
  if (!object) return NULL;
  parser->pos++; /* { */

  json_skip_space(parser);
  if (parser->pos < parser->len && parser->text[parser->pos] == '}') {
    parser->pos++;
    return object;
  }
  for (;;) {
    json_skip_space(parser);
    if (parser->pos >= parser->len || parser->text[parser->pos] != '"') {
      json_free(object);
      json_fail_at(parser, "对象的键必须是双引号字符串");
      return NULL;
    }
    char *key = json_scan_string(parser);
    if (!key) {
      json_free(object);
      return NULL;
    }
    json_skip_space(parser);
    if (parser->pos >= parser->len || parser->text[parser->pos] != ':') {
      free(key);
      json_free(object);
      json_fail_at(parser, "键后面缺少冒号");
      return NULL;
    }
    parser->pos++;

    Json *value = json_scan_value(parser, depth + 1);
    if (!value) {
      free(key);
      json_free(object);
      return NULL;
    }
    json_set(object, key, value);
    free(key);

    json_skip_space(parser);
    if (parser->pos < parser->len && parser->text[parser->pos] == ',') {
      parser->pos++;
      continue;
    }
    if (parser->pos < parser->len && parser->text[parser->pos] == '}') {
      parser->pos++;
      return object;
    }
    json_free(object);
    json_fail_at(parser, "对象缺少 } 或分隔符");
    return NULL;
  }
}

/* ---------- 入口 ---------- */

Json *json_scan_value(Parser *parser, int depth) {
  if (depth > JSON_MAX_DEPTH) {
    json_fail_at(parser, "JSON 嵌套层数过深");
    return NULL;
  }
  json_skip_space(parser);
  if (parser->pos >= parser->len) {
    json_fail_at(parser, "内容意外结束");
    return NULL;
  }

  char c = parser->text[parser->pos];
  if (c == '{') return scan_object(parser, depth);
  if (c == '[') return scan_array(parser, depth);
  if (c == '"') {
    char *text = json_scan_string(parser);
    if (!text) return NULL;
    Json *value = node_new(JSON_STRING);
    if (!value) {
      free(text);
      return NULL;
    }
    value->str = text;
    return value;
  }
  if ((c >= '0' && c <= '9') || c == '-') return scan_number(parser);

  if (literal(parser, "true")) {
    parser->pos += 4;
    Json *value = node_new(JSON_BOOL);
    if (value) value->bval = 1;
    return value;
  }
  if (literal(parser, "false")) {
    parser->pos += 5;
    return node_new(JSON_BOOL);
  }
  if (literal(parser, "null")) {
    parser->pos += 4;
    return node_new(JSON_NULL);
  }
  json_fail_at(parser, "无法识别的内容");
  return NULL;
}

Json *json_parse(const char *text, size_t len, const char **err) {
  errbuf[0] = '\0';
  if (err) *err = NULL;
  if (!text) {
    snprintf(errbuf, sizeof(errbuf), "输入为空");
    if (err) *err = errbuf;
    return NULL;
  }

  Parser parser = {text, len ? len : strlen(text), 0, 0};
  Json *root = json_scan_value(&parser, 0);
  if (!root) {
    if (err) *err = errbuf;
    return NULL;
  }
  json_skip_space(&parser);
  if (parser.pos != parser.len) {
    json_free(root);
    json_fail_at(&parser, "根节点之后还有多余内容");
    if (err) *err = errbuf;
    return NULL;
  }
  return root;
}
