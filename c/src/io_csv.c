/**
 * CSV 导入导出。格式与图形版完全一致：
 *   表头含「姓名」，科目列按名称匹配；导出带 UTF-8 BOM，Excel 打开不乱码。
 */
#include "model.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "calc.h"

#define CSV_MAX_COLS 64

/* ---------- CSV 行解析（就地切分，支持引号与 "" 转义） ---------- */

static int csv_split(char *line, char **out, int max) {
  int count = 0;
  char *p = line;
  while (*p && count < max) {
    while (*p == ' ' || *p == '\t') p++;
    if (*p == '"') {
      p++;
      out[count] = p;
      while (*p) {
        if (*p == '"') {
          if (p[1] == '"') {
            /* 折叠 "" 为一个引号：整体前移一字节 */
            memmove(p, p + 1, strlen(p));
            p++;
            continue;
          }
          *p = '\0';
          p++;
          break;
        }
        p++;
      }
      count++;
      /* 跳过到下一个逗号 */
      while (*p && *p != ',') p++;
      if (*p == ',') p++;
      continue;
    }
    out[count] = p;
    while (*p && *p != ',') p++;
    if (*p == ',') {
      *p = '\0';
      p++;
    }
    count++;
  }
  return count;
}

static void trim_cr(char *line) {
  size_t len = strlen(line);
  while (len > 0 && (line[len - 1] == '\r' || line[len - 1] == '\n')) line[--len] = '\0';
}

/* ---------- 导入 ---------- */

int gb_import_csv(GradeBook *gb, const char *path, int write_scores, int *added, int *merged,
                  char *err, size_t errsz) {
  if (err && errsz) err[0] = '\0';
  if (added) *added = 0;
  if (merged) *merged = 0;
  if (!gb || !path) return -1;

  FILE *file = fopen(path, "rb");
  if (!file) {
    snprintf(err, errsz, "打不开文件：%s", path);
    return -1;
  }
  if (fseek(file, 0, SEEK_END) != 0) {
    fclose(file);
    snprintf(err, errsz, "读取文件失败。");
    return -1;
  }
  long size = ftell(file);
  if (size < 0) {
    fclose(file);
    snprintf(err, errsz, "读取文件失败。");
    return -1;
  }
  rewind(file);

  char *content = malloc((size_t)size + 1);
  if (!content) {
    fclose(file);
    snprintf(err, errsz, "内存不足，无法读取文件。");
    return -1;
  }
  size_t got = fread(content, 1, (size_t)size, file);
  fclose(file);
  content[got] = '\0';

  /* 剥掉 UTF-8 BOM */
  char *body = content;
  if (got >= 3 && (unsigned char)body[0] == 0xef && (unsigned char)body[1] == 0xbb &&
      (unsigned char)body[2] == 0xbf) {
    body += 3;
  }

  char *cursor = body;
  char *line = cursor;
  int line_no = 0;
  int col_sid = -1, col_name = -1, col_gender = -1, col_note = -1;
  int subject_col[CSV_MAX_COLS];
  for (int i = 0; i < CSV_MAX_COLS; i++) subject_col[i] = -1;

  int result = 0;
  for (;;) {
    char *newline = strchr(cursor, '\n');
    if (newline) *newline = '\0';
    trim_cr(line);
    line_no++;

    if (line[0] != '\0') {
      char *columns[CSV_MAX_COLS];
      int n = csv_split(line, columns, CSV_MAX_COLS);

      if (line_no == 1) {
        for (int i = 0; i < n; i++) {
          if (strcmp(columns[i], "姓名") == 0) col_name = i;
          else if (strcmp(columns[i], "学号") == 0) col_sid = i;
          else if (strcmp(columns[i], "性别") == 0) col_gender = i;
          else if (strcmp(columns[i], "备注") == 0) col_note = i;
          else {
            const Subject *subject = NULL;
            for (int s = 0; s < gb->subject_count; s++)
              if (strcmp(gb->subjects[s].name, columns[i]) == 0) subject = &gb->subjects[s];
            if (subject) subject_col[i] = (int)(subject - gb->subjects);
          }
        }
        if (col_name < 0) {
          snprintf(err, errsz, "CSV 缺少「姓名」列，无法导入。");
          result = -1;
          break;
        }
      } else {
        const char *name = col_name >= 0 && col_name < n ? columns[col_name] : "";
        if (name[0] == '\0') {
          if (!newline) break;
          cursor = newline + 1;
          line = cursor;
          continue;
        }
        const char *sid = col_sid >= 0 && col_sid < n ? columns[col_sid] : "";
        const char *gender = col_gender >= 0 && col_gender < n ? columns[col_gender] : "";
        const char *note = col_note >= 0 && col_note < n ? columns[col_note] : "";

        int existing = -1;
        for (int i = 0; i < gb->student_count; i++) {
          if (strcmp(gb->students[i].sid, sid) == 0 && strcmp(gb->students[i].name, name) == 0) {
            existing = i;
            break;
          }
        }
        const Student *student;
        if (existing >= 0) {
          student = &gb->students[existing];
          if (merged) (*merged)++;
        } else {
          student = gb_add_student(gb, sid, name, gender, note);
          if (!student) {
            snprintf(err, errsz, "内存不足，导入中断在第 %d 行。", line_no);
            result = -1;
            break;
          }
          if (added) (*added)++;
        }

        if (write_scores && gb->active_exam_id[0]) {
          for (int i = 0; i < n; i++) {
            if (subject_col[i] < 0) continue;
            if (columns[i][0] == '\0') continue;
            char *end = NULL;
            double value = strtod(columns[i], &end);
            if (end == columns[i]) continue; /* 非数字按缺考处理 */
            gb_set_score(gb, gb->active_exam_id, student->id, gb->subjects[subject_col[i]].id, value);
          }
        }
      }
    }

    if (!newline) break;
    cursor = newline + 1;
    line = cursor;
  }

  free(content);
  return result;
}

/* ---------- 导出 ---------- */

static void cell_text(char *buffer, size_t bufsz, const char *text) {
  /* 含逗号、引号或换行时用双引号包裹 */
  if (strchr(text, ',') || strchr(text, '"') || strchr(text, '\n')) {
    size_t used = 0;
    buffer[used++] = '"';
    for (const char *p = text; *p && used + 3 < bufsz; p++) {
      if (*p == '"') {
        buffer[used++] = '"';
        buffer[used++] = '"';
      } else {
        buffer[used++] = *p;
      }
    }
    buffer[used++] = '"';
    buffer[used] = '\0';
    return;
  }
  snprintf(buffer, bufsz, "%s", text);
}

static int write_exam_table(FILE *file, const GradeBook *gb, const Exam *exam) {
  int capacity = gb->subject_count > 0 ? gb->subject_count : 1;
  const Subject **subs = malloc(sizeof(Subject *) * (size_t)capacity);
  if (!subs) return -1;
  int nsub = calc_exam_subjects(gb, exam, subs, gb->subject_count);

  fputs("学号,姓名", file);
  for (int i = 0; i < nsub; i++) {
    char cell[256];
    cell_text(cell, sizeof(cell), subs[i]->name);
    fprintf(file, ",%s", cell);
  }
  fputs(",总分,平均分,得分率,排名\n", file);

  RankRow *rows = NULL;
  int n = calc_ranking(gb, exam, &rows);
  for (int i = 0; i < n; i++) {
    char cell[256];
    cell_text(cell, sizeof(cell), rows[i].student->sid);
    fprintf(file, "%s", cell);
    cell_text(cell, sizeof(cell), rows[i].student->name);
    fprintf(file, ",%s", cell);

    for (int j = 0; j < nsub; j++) {
      if (rows[i].scores && rows[i].scores[j] >= 0) {
        char number[32];
        snprintf(number, sizeof(number), "%g", rows[i].scores[j]);
        fprintf(file, ",%s", number);
      } else {
        fputs(",", file);
      }
    }
    if (rows[i].counted > 0) {
      fprintf(file, ",%g,%.2f,%.1f%%,%d\n", rows[i].total, rows[i].average, rows[i].rate, rows[i].rank);
    } else {
      fputs(",,,\n", file); /* 全缺考不写总分，避免看起来像 0 分 */
    }
  }
  calc_free_ranking(rows, n);
  free(subs);
  return 0;
}

int gb_export_csv(const GradeBook *gb, const char *path, int all_exams, char *err, size_t errsz) {
  if (err && errsz) err[0] = '\0';
  if (!gb || !path) return -1;

  FILE *file = fopen(path, "wb");
  if (!file) {
    snprintf(err, errsz, "无法写入文件：%s", path);
    return -1;
  }
  /* UTF-8 BOM：没有它 Excel 打开中文会是乱码 */
  const unsigned char bom[3] = {0xef, 0xbb, 0xbf};
  fwrite(bom, 1, sizeof(bom), file);

  int result = 0;
  if (all_exams) {
    for (int i = 0; i < gb->exam_count; i++) {
      if (write_exam_table(file, gb, &gb->exams[i]) != 0) {
        result = -1;
        break;
      }
      if (i + 1 < gb->exam_count) fputs("\n", file); /* 考试之间空一行分隔 */
    }
  } else {
    const Exam *exam = gb_active_exam(gb);
    if (!exam) {
      snprintf(err, errsz, "还没有任何考试，无法导出。");
      result = -1;
    } else {
      result = write_exam_table(file, gb, exam);
    }
  }

  fclose(file);
  if (result != 0 && (!err || !errsz || err[0] == '\0')) snprintf(err, errsz, "导出失败。");
  return result;
}
