/**
 * JSON 序列化。输出 UTF-8，中文按原样写出（不做 \u 转义，便于人眼阅读）。
 */
#include "json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  char *text;
  size_t len;
  size_t cap;
} Buffer;

static int buffer_put(Buffer *buffer, const char *text) {
  size_t need = strlen(text);
  if (buffer->len + need + 1 > buffer->cap) {
    size_t next = buffer->cap ? buffer->cap * 2 : 256;
    while (next < buffer->len + need + 1) next *= 2;
    char *grown = realloc(buffer->text, next);
    if (!grown) return -1;
    buffer->text = grown;
    buffer->cap = next;
  }
  memcpy(buffer->text + buffer->len, text, need);
  buffer->len += need;
  buffer->text[buffer->len] = '\0';
  return 0;
}

static void write_string(Buffer *buffer, const char *text) {
  buffer_put(buffer, "\"");
  for (const unsigned char *p = (const unsigned char *)text; *p; p++) {
    switch (*p) {
      case '"': buffer_put(buffer, "\\\""); break;
      case '\\': buffer_put(buffer, "\\\\"); break;
      case '\n': buffer_put(buffer, "\\n"); break;
      case '\r': buffer_put(buffer, "\\r"); break;
      case '\t': buffer_put(buffer, "\\t"); break;
      case '\b': buffer_put(buffer, "\\b"); break;
      case '\f': buffer_put(buffer, "\\f"); break;
      default:
        if (*p < 0x20) {
          char esc[8];
          snprintf(esc, sizeof(esc), "\\u%04x", (unsigned)*p);
          buffer_put(buffer, esc);
        } else {
          char one[2] = {(char)*p, '\0'};
          buffer_put(buffer, one);
        }
    }
  }
  buffer_put(buffer, "\"");
}

static void indent(Buffer *buffer, int pretty, int depth) {
  if (!pretty) return;
  buffer_put(buffer, "\n");
  for (int i = 0; i < depth; i++) buffer_put(buffer, "  ");
}

static void write_value(Buffer *buffer, const Json *value, int pretty, int depth);

static void write_items(Buffer *buffer, const Json *value, int pretty, int depth) {
  for (int i = 0; i < value->count; i++) {
    if (i) buffer_put(buffer, ",");
    indent(buffer, pretty, depth + 1);
    if (value->type == JSON_OBJECT) {
      write_string(buffer, value->keys[i] ? value->keys[i] : "");
      buffer_put(buffer, pretty ? ": " : ":");
    }
    write_value(buffer, value->items[i], pretty, depth + 1);
  }
}

static void write_value(Buffer *buffer, const Json *value, int pretty, int depth) {
  char number[40];

  if (!value) {
    buffer_put(buffer, "null");
    return;
  }
  switch (value->type) {
    case JSON_NULL:
      buffer_put(buffer, "null");
      break;
    case JSON_BOOL:
      buffer_put(buffer, value->bval ? "true" : "false");
      break;
    case JSON_NUMBER:
      /* 整数不带小数点，避免 150 写成 150.0 */
      if (value->num == (double)(long long)value->num) snprintf(number, sizeof(number), "%lld", (long long)value->num);
      else snprintf(number, sizeof(number), "%.10g", value->num);
      buffer_put(buffer, number);
      break;
    case JSON_STRING:
      write_string(buffer, value->str ? value->str : "");
      break;
    case JSON_ARRAY:
      buffer_put(buffer, "[");
      write_items(buffer, value, pretty, depth);
      indent(buffer, pretty, depth);
      buffer_put(buffer, "]");
      break;
    case JSON_OBJECT:
      buffer_put(buffer, "{");
      write_items(buffer, value, pretty, depth);
      indent(buffer, pretty, depth);
      buffer_put(buffer, "}");
      break;
  }
}

char *json_write(const Json *value, int pretty) {
  Buffer buffer = {NULL, 0, 0};
  write_value(&buffer, value, pretty ? 1 : 0, 0);
  if (!buffer.text) {
    char *empty = malloc(1);
    if (empty) empty[0] = '\0';
    return empty;
  }
  return buffer.text;
}
