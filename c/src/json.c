/**
 * JSON 节点的核心操作：构造、组装、读取、释放。序列化实现见 json_write.c。
 */
#include "json.h"

#include <stdlib.h>
#include <string.h>

#include "compat.h"

/* 解析实现见 json_parse.c / json_scan_string.c，序列化见 json_write.c */

static Json *node_new(JsonType type) {
  Json *value = calloc(1, sizeof(Json));
  if (!value) return NULL;
  value->type = type;
  return value;
}

/* ---------- 释放与构造 ---------- */

void json_free(Json *value) {
  if (!value) return;
  if (value->type == JSON_ARRAY || value->type == JSON_OBJECT) {
    for (int i = 0; i < value->count; i++) json_free(value->items[i]);
    if (value->type == JSON_OBJECT) {
      for (int i = 0; i < value->count; i++) free(value->keys[i]);
    }
    free(value->items);
    free(value->keys);
  }
  free(value->str);
  free(value);
}

Json *json_new_null(void) { return node_new(JSON_NULL); }

Json *json_new_bool(int value) {
  Json *node = node_new(JSON_BOOL);
  if (node) node->bval = value ? 1 : 0;
  return node;
}

Json *json_new_number(double value) {
  Json *node = node_new(JSON_NUMBER);
  if (node) node->num = value;
  return node;
}

Json *json_new_string(const char *text) {
  Json *node = node_new(JSON_STRING);
  if (!node) return NULL;
  node->str = gm_strdup(text ? text : "");
  if (!node->str) {
    free(node);
    return NULL;
  }
  return node;
}

Json *json_new_array(void) { return node_new(JSON_ARRAY); }
Json *json_new_object(void) { return node_new(JSON_OBJECT); }

/* ---------- 组装 ---------- */

static int reserve(Json *node) {
  int wanted = node->count + 1;
  Json **items = realloc(node->items, sizeof(Json *) * (size_t)wanted);
  if (!items) return -1;
  node->items = items;
  if (node->type == JSON_OBJECT) {
    char **keys = realloc(node->keys, sizeof(char *) * (size_t)wanted);
    if (!keys) return -1;
    node->keys = keys;
  }
  return 0;
}

void json_push(Json *array, Json *value) {
  if (!array || !value || reserve(array) != 0) {
    json_free(value);
    return;
  }
  array->items[array->count] = value;
  if (array->type == JSON_OBJECT) array->keys[array->count] = NULL;
  array->count++;
}

void json_set(Json *object, const char *key, Json *value) {
  if (!object || !key || !value) {
    json_free(value);
    return;
  }
  for (int i = 0; i < object->count; i++) {
    if (object->keys[i] && strcmp(object->keys[i], key) == 0) {
      json_free(object->items[i]);
      object->items[i] = value;
      return;
    }
  }
  if (reserve(object) != 0) {
    json_free(value);
    return;
  }
  char *copy = gm_strdup(key);
  if (!copy) {
    json_free(value);
    return;
  }
  object->keys[object->count] = copy;
  object->items[object->count] = value;
  object->count++;
}

/* ---------- 读取 ---------- */

const Json *json_get(const Json *object, const char *key) {
  if (!object || object->type != JSON_OBJECT || !key) return NULL;
  for (int i = 0; i < object->count; i++) {
    if (object->keys[i] && strcmp(object->keys[i], key) == 0) return object->items[i];
  }
  return NULL;
}

int json_is_null(const Json *value) { return !value || value->type == JSON_NULL; }

const char *json_str(const Json *value, const char *fallback) {
  return (value && value->type == JSON_STRING && value->str) ? value->str : fallback;
}

double json_num(const Json *value, double fallback) {
  return (value && value->type == JSON_NUMBER) ? value->num : fallback;
}

int json_bool(const Json *value, int fallback) {
  return (value && value->type == JSON_BOOL) ? value->bval : fallback;
}

const char *json_get_str(const Json *object, const char *key, const char *fallback) {
  return json_str(json_get(object, key), fallback);
}

double json_get_num(const Json *object, const char *key, double fallback) {
  return json_num(json_get(object, key), fallback);
}
