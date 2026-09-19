/**
 * 统计视图的两块详情面板：学生成绩趋势、单科分数段分布。
 */
#include "views.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "calc.h"
#include "ui.h"

/** 学生详情：本次各科成绩 + 历次考试总分趋势折线图 + 名次变化 */
void stats_student_detail(AppCtx *ctx, const Exam *exam, const Student *student) {
  GradeBook *gb = ctx->gb;
  int maxy, maxx;
  ui_terminal_size(&maxy, &maxx);

  WINDOW *win = newwin(maxy, maxx, 0, 0);
  if (!win) return;
  keypad(win, TRUE);

  char title[160];
  snprintf(title, sizeof(title), "%s 的成绩详情", student->name);
  ui_header(win, title, student->sid[0] ? student->sid : NULL);

  int y = 3;
  int subject_count = gb->subject_count > 0 ? gb->subject_count : 1;
  const Subject **subs = malloc(sizeof(Subject *) * (size_t)subject_count);
  int nsub = subs ? calc_exam_subjects(gb, exam, subs, gb->subject_count) : 0;

  wattron(win, ui_role(UI_HEADER));
  mvwprintw(win, y++, 2, "本次考试（%s）各科成绩", exam->name);
  wattroff(win, ui_role(UI_HEADER));

  int col_width = 14;
  int per_row = (maxx - 4) / col_width;
  if (per_row < 1) per_row = 1;
  for (int i = 0; i < nsub && y < maxy - 12; i++) {
    int col = 2 + (i % per_row) * col_width;
    if (i > 0 && i % per_row == 0) y++;
    double value = gb_get_score(gb, exam->id, student->id, subs[i]->id);
    char cell[32];
    if (value < 0) snprintf(cell, sizeof(cell), "%s -", subs[i]->name);
    else snprintf(cell, sizeof(cell), "%s %.0f", subs[i]->name, value);
    int role = value < 0 ? UI_DIM
             : value < subs[i]->full_mark * 0.6 ? UI_DANGER
             : value >= subs[i]->full_mark * 0.9 ? UI_SUCCESS
                                                 : UI_TEXT;
    ui_col(win, y, col, col_width - 1, 0, cell, role);
  }
  y += 2;
  free(subs);

  /* 历次考试总分与名次 */
  int exam_count = gb->exam_count;
  double *totals = malloc(sizeof(double) * (size_t)(exam_count > 0 ? exam_count : 1));
  int *ranks = malloc(sizeof(int) * (size_t)(exam_count > 0 ? exam_count : 1));
  if (!totals || !ranks) {
    free(totals);
    free(ranks);
    delwin(win);
    return;
  }
  int trend_n = 0;
  calc_student_trend(gb, student->id, totals, ranks, &trend_n);

  const char **labels = malloc(sizeof(char *) * (size_t)(exam_count > 0 ? exam_count : 1));
  if (labels) {
    for (int i = 0; i < trend_n; i++) labels[i] = gb->exams[i].name;
  }

  wattron(win, ui_role(UI_HEADER));
  mvwprintw(win, y++, 2, "总分趋势");
  wattroff(win, ui_role(UI_HEADER));
  int chart_h = 10;
  ui_line_chart(win, y, 3, maxx - 6, chart_h, totals, trend_n, labels);
  y += chart_h + 1;

  wattron(win, ui_role(UI_HEADER));
  mvwprintw(win, y++, 2, "历次考试记录");
  wattroff(win, ui_role(UI_HEADER));
  for (int i = 0; i < trend_n && y < maxy - 2; i++) {
    int previous = -1;
    for (int j = i - 1; j >= 0; j--)
      if (ranks[j] > 0) {
        previous = ranks[j];
        break;
      }
    int delta = previous > 0 ? calc_rank_delta(ranks[i], previous) : 0; /* 当前名次在前，上一场在后 */
    char line[192];
    snprintf(line, sizeof(line), "%s  总分 %.0f  第 %d 名", gb->exams[i].name, totals[i], ranks[i]);
    ui_col(win, y, 3, 40, 0, line, UI_TEXT);
    if (delta > 0) {
      snprintf(line, sizeof(line), "进步 %d 名", delta);
      ui_col(win, y, 44, 16, 0, line, UI_SUCCESS);
    } else if (delta < 0) {
      snprintf(line, sizeof(line), "退步 %d 名", -delta);
      ui_col(win, y, 44, 16, 0, line, UI_DANGER);
    } else if (previous > 0) {
      ui_col(win, y, 44, 16, 0, "名次持平", UI_DIM);
    }
    y++;
  }

  ui_footer(win, "按任意键返回");
  wrefresh(win);
  wgetch(win);
  delwin(win);
  touchwin(stdscr);
  refresh();

  free(totals);
  free(ranks);
  free(labels);
}

/** 单科详情：五项指标 + 五个分数段的分布条形图 */
void stats_subject_detail(GradeBook *gb, const Exam *exam, const Subject *subject) {
  int maxy, maxx;
  ui_terminal_size(&maxy, &maxx);
  int height = 16;
  int width = maxx - 8 > 60 ? maxx - 8 : 60;
  if (width > 72) width = 72;
  if (height > maxy - 2) height = maxy - 2;

  WINDOW *win = newwin(height, width, (maxy - height) / 2, (maxx - width) / 2);
  if (!win) return;
  keypad(win, TRUE);
  wattron(win, ui_role(UI_BORDER));
  box(win, 0, 0);
  wattroff(win, ui_role(UI_BORDER));

  char title[96];
  snprintf(title, sizeof(title), " %s（满分 %d）", subject->name, subject->full_mark);
  wattron(win, ui_role(UI_TITLE));
  mvwprintw(win, 0, 2, "%s", title);
  wattroff(win, ui_role(UI_TITLE));

  SubjectStats stats;
  calc_subject_stats(gb, exam, subject->id, &stats);

  const char *names[] = {"平均分", "最高分", "最低分", "及格率", "优秀率"};
  double values[] = {stats.average, stats.max, stats.min, stats.pass_rate, stats.excellent_rate};
  for (int i = 0; i < 5; i++) {
    char cell[32];
    /* 前三项是分数，后两项是百分比 */
    if (i >= 3) snprintf(cell, sizeof(cell), "%.1f%%", values[i]);
    else snprintf(cell, sizeof(cell), "%.1f", values[i]);
    ui_col(win, 2 + i, 3, 10, 0, names[i], UI_DIM);
    ui_col(win, 2 + i, 14, 12, 1, cell, UI_TEXT);
  }

  int y = 8;
  wattron(win, ui_role(UI_HEADER));
  mvwprintw(win, y++, 3, "分数段分布（共 %d 人已录）", stats.count);
  wattroff(win, ui_role(UI_HEADER));

  const char *bins[] = {"不及格", "60-70", "70-80", "80-90", "90 以上"};
  for (int i = 0; i < 5 && y < height - 2; i++) {
    ui_col(win, y, 3, 8, 0, bins[i], UI_DIM);
    ui_bar(win, y, 12, 30, stats.count ? stats.dist_ratio[i] : 0.0, i == 0 ? UI_BAR_FAIL : UI_BAR);
    char meta[32];
    snprintf(meta, sizeof(meta), "%d 人 %.0f%%", stats.dist[i], stats.dist_ratio[i] * 100);
    ui_col(win, y, 44, 24, 0, meta, UI_TEXT);
    y++;
  }

  wattron(win, ui_role(UI_DIM));
  mvwprintw(win, height - 2, 3, "按任意键返回");
  wattroff(win, ui_role(UI_DIM));
  wrefresh(win);
  wgetch(win);
  delwin(win);
  touchwin(stdscr);
  refresh();
}
