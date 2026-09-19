/**
 * 入口：数据文件定位、载入、主菜单循环、退出前保存。
 *
 * 数据文件与图形版（Tauri 版）共用同一路径，两个版本可以随时切换使用同一份数据。
 * 首次启动时若文件不存在，会写入一份含 6 个常规科目与一场考试的默认数据。
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "compat.h"

#include "ui.h"
#include "views.h"

/** 文件是否存在。用 fopen 实现，避免依赖 Windows 上没有的 stat 细节 */
static int file_exists(const char *path) {
  FILE *file = fopen(path, "rb");
  if (!file) return 0;
  fclose(file);
  return 1;
}

/** 把现有数据复制一份到 backups/ 下，失败也不影响主流程 */
static void backup_file(const char *path) {
  if (!file_exists(path)) return;

  char dir[512];
  snprintf(dir, sizeof(dir), "%s", path);
  char *slash = strrchr(dir, '/');
  if (!slash) return;
  *slash = '\0';
  char backup_dir[600];
  snprintf(backup_dir, sizeof(backup_dir), "%s/backups", dir);
  if (gm_mkdirs(backup_dir) != 0) return;

  char dest[700];
  snprintf(dest, sizeof(dest), "%s/grades-%ld.json", backup_dir, (long)time(NULL));

  FILE *in = fopen(path, "rb");
  FILE *out = fopen(dest, "wb");
  if (!in || !out) {
    if (in) fclose(in);
    if (out) fclose(out);
    return;
  }
  char chunk[4096];
  size_t got;
  while ((got = fread(chunk, 1, sizeof(chunk), in)) > 0) fwrite(chunk, 1, got, out);
  fclose(in);
  fclose(out);
}

static void seed_defaults(GradeBook *gb) {
  gb_add_subject(gb, "语文", 150);
  gb_add_subject(gb, "数学", 150);
  gb_add_subject(gb, "英语", 150);
  gb_add_subject(gb, "物理", 100);
  gb_add_subject(gb, "化学", 100);
  gb_add_subject(gb, "生物", 100);

  char today[GM_DATE_LEN];
  gm_today(today, sizeof(today));
  Exam *exam = gb_add_exam(gb, "第一次月考", today);
  if (exam) snprintf(gb->active_exam_id, sizeof(gb->active_exam_id), "%s", exam->id);
}

typedef struct {
  GradeBook *gb;
  AppCtx *ctx;
} MenuCtx;

static void on_status(AppCtx *ctx, const char *text) {
  (void)ctx;
  (void)text;
}

static void import_json(AppCtx *ctx) {
  char path[512];
  if (!ui_filepath("导入 JSON 备份", "grades.json", path, sizeof(path), 1)) return;

  const char *modes[] = {"合并到现有数据（按名称去重，成绩覆盖）", "替换全部数据（丢弃当前内容）", "取消"};
  int choice = ui_pick("选择导入方式", modes, 3, 0);
  if (choice < 0 || choice == 2) return;

  GradeBook incoming;
  gb_init(&incoming);
  char err[256] = "";
  if (gb_load_file(&incoming, path, err, sizeof(err)) != 0) {
    ui_error(err);
    gb_free(&incoming);
    return;
  }

  if (choice == 1) {
    /* 替换是不可逆操作，先给现有数据留一份备份 */
    backup_file(ctx->data_path);
    GradeBook tmp = *ctx->gb;
    *ctx->gb = incoming;
    incoming = tmp; /* 旧数据交给 gb_free 释放 */
  } else {
    /* 合并：学生按 学号+姓名 去重，科目与考试按名称去重，成绩覆盖 */
    for (int i = 0; i < incoming.subject_count; i++) {
      const Subject *subject = &incoming.subjects[i];
      int exists = 0;
      for (int j = 0; j < ctx->gb->subject_count; j++)
        if (strcmp(ctx->gb->subjects[j].name, subject->name) == 0) exists = 1;
      if (!exists) gb_add_subject(ctx->gb, subject->name, subject->full_mark);
    }
    for (int i = 0; i < incoming.exam_count; i++) {
      const Exam *exam = &incoming.exams[i];
      int exists = 0;
      for (int j = 0; j < ctx->gb->exam_count; j++)
        if (strcmp(ctx->gb->exams[j].name, exam->name) == 0) exists = 1;
      if (!exists) gb_add_exam(ctx->gb, exam->name, exam->date);
    }
    for (int i = 0; i < incoming.student_count; i++) {
      const Student *student = &incoming.students[i];
      int exists = 0;
      for (int j = 0; j < ctx->gb->student_count; j++)
        if (strcmp(ctx->gb->students[j].sid, student->sid) == 0 &&
            strcmp(ctx->gb->students[j].name, student->name) == 0) exists = 1;
      if (!exists) gb_add_student(ctx->gb, student->sid, student->name, student->gender, student->note);
    }
  }
  ctx->dirty = 1;
  gb_free(&incoming);
  view_status(ctx, "导入完成");
  ui_message("导入完成", "数据已载入，返回菜单后会自动保存。");
}

static void export_json(AppCtx *ctx) {
  char path[512];
  if (!ui_filepath("导出 JSON 备份", "grades-backup.json", path, sizeof(path), 0)) return;
  char err[256] = "";
  if (gb_save_file(ctx->gb, path, err, sizeof(err)) != 0) ui_error(err);
  else view_status(ctx, "已导出备份");
}

static void export_csv(AppCtx *ctx) {
  const char *modes[] = {"仅当前考试", "全部考试", "取消"};
  int choice = ui_pick("导出成绩表 CSV", modes, 3, 0);
  if (choice < 0 || choice == 2) return;

  char path[512];
  if (!ui_filepath("导出 CSV", "grades.csv", path, sizeof(path), 0)) return;
  char err[256] = "";
  if (gb_export_csv(ctx->gb, path, choice == 1, err, sizeof(err)) != 0) ui_error(err);
  else view_status(ctx, "已导出 CSV");
}

int main(int argc, char **argv) {
  GradeBook gb;
  gb_init(&gb);

  char path[512];
  gm_default_data_path(path, sizeof(path));
  for (int i = 1; i < argc; i++) {
    if ((strcmp(argv[i], "--file") == 0 || strcmp(argv[i], "-f") == 0) && i + 1 < argc) {
      snprintf(path, sizeof(path), "%s", argv[++i]);
    } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
      printf("班级成绩管理系统（终端版）\n\n用法: grades [--file 数据文件路径]\n");
      return 0;
    }
  }

  int had_data = file_exists(path);
  if (had_data) {
    char err[256] = "";
    if (gb_load_file(&gb, path, err, sizeof(err)) != 0) {
      fprintf(stderr, "载入数据失败：%s\n", err);
      gb_init(&gb);
      had_data = 0;
    }
  }
  if (!had_data) {
    seed_defaults(&gb);
    /* 确保目录存在，否则首次保存会失败 */
    char dir[512];
    snprintf(dir, sizeof(dir), "%s", path);
    char *slash = strrchr(dir, '/');
    if (slash) {
      *slash = '\0';
      gm_mkdirs(dir);
    }
  }

  if (ui_init() != 0) {
    fprintf(stderr, "无法初始化终端界面。\n");
    gb_free(&gb);
    return 1;
  }

  AppCtx ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.gb = &gb;
  ctx.set_status = on_status;
  snprintf(ctx.data_path, sizeof(ctx.data_path), "%s", path);

  static const char *menu[] = {
      "学生管理 —— 名单增删、批量导入",
      "成绩录入 —— 按科目填分",
      "统计排名 —— 总分、平均分、名次与趋势",
      "科目管理 —— 科目与满分",
      "考试管理 —— 多场考试切换",
      "导入 JSON 备份",
      "导出 JSON 备份",
      "导出 CSV 成绩表",
      "保存并退出",
  };

  for (;;) {
    char subtitle[200];
    snprintf(subtitle, sizeof(subtitle), "%s · %d 名学生 · %d 个科目 · %d 场考试%s",
             gb.class_name, gb.student_count, gb.subject_count, gb.exam_count,
             ctx.dirty ? " · 有未保存改动" : "");

    int choice = ui_pick(subtitle, menu, 9, 0);
    if (choice < 0) {
      if (ctx.dirty &&
          ui_confirm("退出", "还有未保存的改动，确定要放弃并退出吗？")) break;
      if (!ctx.dirty) break;
      continue;
    }

    ctx.search[0] = '\0';
    switch (choice) {
      case 0: view_students(&ctx); break;
      case 1: view_scores(&ctx); break;
      case 2: view_stats(&ctx); break;
      case 3: view_subjects(&ctx); break;
      case 4: view_exams(&ctx); break;
      case 5: import_json(&ctx); break;
      case 6: export_json(&ctx); break;
      case 7: export_csv(&ctx); break;
      default: break;
    }

    if (ctx.dirty) view_save(&ctx);
    if (choice == 8) break;
  }

  ui_shutdown();
  gb_free(&gb);
  printf("数据文件：%s\n", path);
  return 0;
}
