/**
 * 数据文件读写：整文件读入、解析、原子保存。
 * JSON 与数据模型的转换见 persist_json.c。
 */
/**
 * 持久化：JSON 树 ⇄ GradeBook，以及文件读写。
 *
 * 载入时做清洗：剔除空名科目与学生、剔除指向不存在对象的成绩单元格、
 * activeExamId 非法时回退到第一场考试。任何异常输入都降级为「空数据 + 错误提示」。
 */
#include "model.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "compat.h"

/* ---------- 文件 ---------- */

static char *read_all(const char *path, size_t *out_len, char *err, size_t errsz) {
  FILE *file = fopen(path, "rb");
  if (!file) {
    snprintf(err, errsz, "打不开文件：%s", path);
    return NULL;
  }
  if (fseek(file, 0, SEEK_END) != 0) {
    fclose(file);
    snprintf(err, errsz, "读取文件失败：%s", path);
    return NULL;
  }
  long size = ftell(file);
  if (size < 0) {
    fclose(file);
    snprintf(err, errsz, "读取文件失败：%s", path);
    return NULL;
  }
  rewind(file);

  char *buffer = malloc((size_t)size + 1);
  if (!buffer) {
    fclose(file);
    snprintf(err, errsz, "内存不足，无法读取文件。");
    return NULL;
  }
  size_t got = fread(buffer, 1, (size_t)size, file);
  fclose(file);
  buffer[got] = '\0';
  if (out_len) *out_len = got;
  return buffer;
}

int gb_load_file(GradeBook *gb, const char *path, char *err, size_t errsz) {
  if (err && errsz) err[0] = '\0';
  if (!gb || !path) return -1;

  size_t len = 0;
  char *text = read_all(path, &len, err, errsz);
  if (!text) return -1;

  const char *json_err = NULL;
  Json *root = json_parse(text, len, &json_err);
  free(text);
  if (!root) {
    snprintf(err, errsz, "数据文件解析失败：%s", json_err ? json_err : "未知原因");
    return -1;
  }
  if (gb_load_json(gb, root) != 0) {
    json_free(root);
    snprintf(err, errsz, "数据文件结构不正确，应为 JSON 对象。");
    return -1;
  }
  json_free(root);
  return 0;
}

int gb_save_file(const GradeBook *gb, const char *path, char *err, size_t errsz) {
  if (err && errsz) err[0] = '\0';
  if (!gb || !path) return -1;

  Json *root = gb_to_json(gb);
  if (!root) {
    snprintf(err, errsz, "内存不足，无法生成数据。");
    return -1;
  }
  char *text = json_write(root, 1);
  json_free(root);
  if (!text) {
    snprintf(err, errsz, "内存不足，无法生成数据。");
    return -1;
  }

  /* 先写临时文件再改名，避免写入中断留下半截文件 */
  char tmp[600];
  snprintf(tmp, sizeof(tmp), "%s.tmp", path);
  FILE *file = fopen(tmp, "wb");
  if (!file) {
    free(text);
    snprintf(err, errsz, "无法写入文件：%s", path);
    return -1;
  }
  size_t want = strlen(text);
  size_t wrote = fwrite(text, 1, want, file);
  fclose(file);
  free(text);

  if (wrote != want) {
    snprintf(err, errsz, "写入不完整：%s", path);
    return -1;
  }
  if (gm_replace_file(tmp, path) != 0) {
    snprintf(err, errsz, "无法替换数据文件：%s", path);
    return -1;
  }
  return 0;
}
