/**
 * 纯计算层 —— 总分 / 平均分 / 排名 / 单科统计 / 趋势 / 录入进度。
 * 无 IO、无界面依赖，可单独编译测试。
 */
#ifndef GM_CALC_H
#define GM_CALC_H

#include "model.h"

typedef struct {
  const Student *student;
  double *scores;    /* 长度为 subject_count；-1 表示缺考（本结构拥有） */
  double total;
  double average;
  double rate;       /* 得分率，0-100 */
  int counted;
  int missing;
  int rank;
} RankRow;

typedef struct {
  int count;
  double average;
  double max;
  double min;
  double pass_rate;      /* 0-100 */
  double excellent_rate; /* 0-100 */
  int dist[5];           /* 不及格 / 60-70 / 70-80 / 80-90 / 90 以上 */
  double dist_ratio[5];
  int full_mark;
} SubjectStats;

/**
 * 取该场考试实际参考的科目。subject_count==0 表示考全部科目。
 * @param out 容量至少为 gb->subject_count
 * @return 实际科目数
 */
int calc_exam_subjects(const GradeBook *gb, const Exam *exam, const Subject **out, int max);

/**
 * 计算全班排名，结果已按总分降序排好。同分同名次并跳号（1,1,3）。
 * 缺考科目不计入总分，也不拉低平均分。
 * @return 行数（== 学生数），0 表示无考试；调用方负责 calc_free_ranking
 */
int calc_ranking(const GradeBook *gb, const Exam *exam, RankRow **out);
void calc_free_ranking(RankRow *rows, int n);

/** 单科统计。无人录分时全部字段归零，不产生 NaN */
void calc_subject_stats(const GradeBook *gb, const Exam *exam, const char *subject_id, SubjectStats *out);

/** 全班及格率：所有已录分数中达到满分 60% 的比例（0-100） */
double calc_class_pass_rate(const GradeBook *gb, const Exam *exam, const RankRow *rows, int n);

/** 录入进度：应录格数、已录格数、完成百分比 */
void calc_progress(const GradeBook *gb, const Exam *exam, int *total, int *done, double *percent);

/** 某学生在各次考试中的总分与名次，按考试日期升序写入 rows（长度 = 考试数） */
void calc_student_trend(const GradeBook *gb, const char *student_id, double *totals, int *ranks, int *n);

/** 两次考试的名次变化：正数为进步，负数为退步；无前序考试时返回 0 */
int calc_rank_delta(int current_rank, int previous_rank);

#endif /* GM_CALC_H */
