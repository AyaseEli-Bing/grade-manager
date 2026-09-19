/**
 * 迷你 JSON 解析器 —— 本项目唯一的 JSON 实现，不引入任何第三方库。
 *
 * 设计取舍：只支持本项目需要的子集（对象 / 数组 / 字符串 / 数字 / 布尔 / null），
 * 不追求完整 JSON 规范。解析失败一律返回 NULL 并给出可读错误，绝不返回半截树。
 */
#ifndef GM_JSON_H
#define GM_JSON_H

#include <stddef.h>

typedef enum {
  JSON_NULL,
  JSON_BOOL,
  JSON_NUMBER,
  JSON_STRING,
  JSON_ARRAY,
  JSON_OBJECT
} JsonType;

typedef struct Json Json;

struct Json {
  JsonType type;
  int bval;       /* JSON_BOOL */
  double num;     /* JSON_NUMBER */
  char *str;      /* JSON_STRING —— 已解码转义，本结构拥有其内存 */
  Json **items;   /* JSON_ARRAY / JSON_OBJECT 的子节点，本结构拥有 */
  char **keys;    /* JSON_OBJECT 的键，与 items 一一对应，本结构拥有 */
  int count;      /* items / keys 的数量 */
};

/**
 * 解析一段文本。成功返回根节点（调用方负责 json_free），
 * 失败返回 NULL 并把 *err 指向一段静态错误说明（含出错位置）。
 */
Json *json_parse(const char *text, size_t len, const char **err);

/** 递归释放整棵树，对 NULL 安全 */
void json_free(Json *value);

/* ---------- 构造（返回值由调用方负责释放或交出所有权） ---------- */

Json *json_new_null(void);
Json *json_new_bool(int value);
Json *json_new_number(double value);
Json *json_new_string(const char *text);
Json *json_new_array(void);
Json *json_new_object(void);

/** 向对象写入键值（value 的所有权转移给 obj）；同键会被替换并释放旧值 */
void json_set(Json *object, const char *key, Json *value);

/** 向数组追加元素（value 的所有权转移给 array） */
void json_push(Json *array, Json *value);

/* ---------- 读取（全部对 NULL 安全） ---------- */

const Json *json_get(const Json *object, const char *key);
int json_is_null(const Json *value);
const char *json_str(const Json *value, const char *fallback);
double json_num(const Json *value, double fallback);
int json_bool(const Json *value, int fallback);

/** 便捷取值：直接用键从对象里读，缺失或类型不符时返回 fallback */
const char *json_get_str(const Json *object, const char *key, const char *fallback);
double json_get_num(const Json *object, const char *key, double fallback);

/** 序列化为 malloc 出来的 NUL 结尾字符串；pretty=1 时带缩进 */
char *json_write(const Json *value, int pretty);

#endif /* GM_JSON_H */
