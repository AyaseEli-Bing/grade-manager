/**
 * JSON 字符串扫描：处理转义序列与 \uXXXX（含 UTF-8 编码与代理对）。
 */
#include "json_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void utf8_encode(unsigned long cp, char *out, size_t *out_len) {
  if (cp < 0x80) {
    out[(*out_len)++] = (char)cp;
  } else if (cp < 0x800) {
    out[(*out_len)++] = (char)(0xc0 | (cp >> 6));
    out[(*out_len)++] = (char)(0x80 | (cp & 0x3f));
  } else if (cp < 0x10000) {
    out[(*out_len)++] = (char)(0xe0 | (cp >> 12));
    out[(*out_len)++] = (char)(0x80 | ((cp >> 6) & 0x3f));
    out[(*out_len)++] = (char)(0x80 | (cp & 0x3f));
  } else {
    out[(*out_len)++] = (char)(0xf0 | (cp >> 18));
    out[(*out_len)++] = (char)(0x80 | ((cp >> 12) & 0x3f));
    out[(*out_len)++] = (char)(0x80 | ((cp >> 6) & 0x3f));
    out[(*out_len)++] = (char)(0x80 | (cp & 0x3f));
  }
}

static int hex4(Parser *parser, unsigned long *out) {
  if (parser->pos + 4 > parser->len) return -1;
  unsigned long value = 0;
  for (int i = 0; i < 4; i++) {
    char c = parser->text[parser->pos + (size_t)i];
    value <<= 4;
    if (c >= '0' && c <= '9') value |= (unsigned long)(c - '0');
    else if (c >= 'a' && c <= 'f') value |= (unsigned long)(c - 'a' + 10);
    else if (c >= 'A' && c <= 'F') value |= (unsigned long)(c - 'A' + 10);
    else return -1;
  }
  parser->pos += 4;
  *out = value;
  return 0;
}

char *json_scan_string(Parser *parser) {
  /* 调用方已确认当前字符是引号，这里先吃掉开头的引号，再扫描内容 */
  if (parser->pos >= parser->len || parser->text[parser->pos] != '"') {
    json_fail_at(parser, "字符串应以双引号开始");
    return NULL;
  }
  parser->pos++;

  const size_t save = parser->pos;
  size_t need = 0;
  int closed = 0;

  /* 第一遍只量长度，避免反复 realloc */
  for (;;) {
    if (parser->pos >= parser->len) break;
    char c = parser->text[parser->pos];
    if (c == '"') {
      closed = 1;
      break;
    }
    if (c == '\\') {
      parser->pos++;
      if (parser->pos >= parser->len) break;
      if (parser->text[parser->pos] == 'u') {
        parser->pos++;
        unsigned long cp = 0;
        if (hex4(parser, &cp) != 0) break;
        need += 4;
        continue;
      }
      need += 1;
      parser->pos++;
      continue;
    }
    need += 1;
    parser->pos++;
  }
  if (!closed) {
    parser->pos = save;
    json_fail_at(parser, "字符串缺少结束的引号");
    return NULL;
  }

  char *out = malloc(need + 1);
  if (!out) return NULL;

  parser->pos = save;
  size_t written = 0;
  while (parser->pos < parser->len && parser->text[parser->pos] != '"') {
    char c = parser->text[parser->pos];
    if (c != '\\') {
      out[written++] = c;
      parser->pos++;
      continue;
    }
    parser->pos++;
    if (parser->pos >= parser->len) break;
    char esc = parser->text[parser->pos];
    switch (esc) {
      case '"': out[written++] = '"'; parser->pos++; break;
      case '\\': out[written++] = '\\'; parser->pos++; break;
      case '/': out[written++] = '/'; parser->pos++; break;
      case 'b': out[written++] = '\b'; parser->pos++; break;
      case 'f': out[written++] = '\f'; parser->pos++; break;
      case 'n': out[written++] = '\n'; parser->pos++; break;
      case 'r': out[written++] = '\r'; parser->pos++; break;
      case 't': out[written++] = '\t'; parser->pos++; break;
      case 'u': {
        parser->pos++;
        unsigned long cp = 0;
        if (hex4(parser, &cp) != 0) {
          out[written] = '\0';
          return out;
        }
        utf8_encode(cp, out, &written);
        break;
      }
      default: out[written++] = esc; parser->pos++; break;
    }
  }
  out[written] = '\0';
  parser->pos++; /* 吃掉结束引号 */
  return out;
}
