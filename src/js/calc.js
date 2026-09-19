/**
 * 纯计算层：总分 / 平均分 / 排名 / 单科统计 / 趋势 / 录入进度。
 * 无副作用、无 DOM、无持久化依赖，便于单独验证。
 */

export function round2(n) {
  return Math.round(n * 100) / 100;
}

/** 该场考试实际参考的科目；subjectIds 为空表示考全部科目 */
export function examSubjects(state, exam) {
  if (!exam) return state.subjects;
  if (!exam.subjectIds || exam.subjectIds.length === 0) return state.subjects;
  const map = new Map(state.subjects.map((s) => [s.id, s]));
  return exam.subjectIds.map((id) => map.get(id)).filter(Boolean);
}

export function subjectName(state, id) {
  const hit = state.subjects.find((s) => s.id === id);
  return hit ? hit.name : '—';
}

export function examName(state, id) {
  const hit = state.exams.find((e) => e.id === id);
  return hit ? hit.name : '—';
}

/**
 * 全班排名。
 * 缺考科目按 0 分计入会失真，因此总分只累计已录分科目，同时用 missing 标记缺考数供界面提示。
 * 排名按总分降序，同分同名次（1, 2, 2, 4）。
 */
export function buildRanking(state, examId) {
  const exam = state.exams.find((e) => e.id === examId);
  const subjects = examSubjects(state, exam);
  const table = state.scores[examId] || {};
  const fullSum = subjects.reduce((sum, s) => sum + s.fullMark, 0);

  const rows = state.students.map((student) => {
    const scores = {};
    let total = 0;
    let counted = 0;
    let missing = 0;
    for (const subject of subjects) {
      const raw = table[student.id] ? table[student.id][subject.id] : null;
      const value = typeof raw === 'number' && Number.isFinite(raw) ? raw : null;
      scores[subject.id] = value;
      if (value === null) missing++;
      else {
        total += value;
        counted++;
      }
    }
    return {
      student,
      scores,
      total: round2(total),
      counted,
      missing,
      average: counted ? round2(total / counted) : 0,
      rate: fullSum ? round2((total / fullSum) * 100) : 0,
      rank: 0,
    };
  });

  rows.sort((a, b) => b.total - a.total || a.student.name.localeCompare(b.student.name, 'zh'));
  let prevTotal = null;
  let prevRank = 0;
  rows.forEach((row, index) => {
    if (prevTotal !== null && row.total === prevTotal) row.rank = prevRank;
    else {
      row.rank = index + 1;
      prevRank = row.rank;
      prevTotal = row.total;
    }
  });
  return rows;
}

/** 单科统计：平均分 / 最高 / 最低 / 及格率 / 优秀率 / 分数段分布 */
export function subjectStats(state, examId, subjectId) {
  const subject = state.subjects.find((s) => s.id === subjectId);
  const fullMark = subject ? subject.fullMark : 100;
  const rows = buildRanking(state, examId);
  const values = rows.map((r) => r.scores[subjectId]).filter((v) => v !== null && v !== undefined);

  if (values.length === 0) {
    return { count: 0, average: 0, max: 0, min: 0, passRate: 0, excellentRate: 0, distribution: [], fullMark };
  }
  const sum = values.reduce((a, b) => a + b, 0);
  return {
    count: values.length,
    average: round2(sum / values.length),
    max: Math.max(...values),
    min: Math.min(...values),
    passRate: round2((values.filter((v) => v >= fullMark * 0.6).length / values.length) * 100),
    excellentRate: round2((values.filter((v) => v >= fullMark * 0.85).length / values.length) * 100),
    distribution: makeBuckets(values, fullMark),
    fullMark,
  };
}

/** 按满分切成 5 段：[0,60%) [60,70%) [70,80%) [80,90%) [90,100%] */
function makeBuckets(values, fullMark) {
  const edges = [0, 0.6, 0.7, 0.8, 0.9, 1.0001].map((ratio) => ratio * fullMark);
  const labels = ['不及格', '60-70', '70-80', '80-90', '90 以上'];
  const counts = [0, 0, 0, 0, 0];
  for (const value of values) {
    for (let i = 0; i < 5; i++) {
      if (value >= edges[i] && value < edges[i + 1]) {
        counts[i]++;
        break;
      }
    }
  }
  return labels.map((label, i) => ({ label, count: counts[i], ratio: counts[i] / values.length }));
}

/** 某学生在各次考试中的总分与名次走势，按考试日期升序 */
export function studentTrend(state, studentId) {
  return state.exams
    .slice()
    .sort((a, b) => String(a.date || '').localeCompare(String(b.date || '')))
    .map((exam) => {
      const rows = buildRanking(state, exam.id);
      const row = rows.find((r) => r.student.id === studentId);
      return { exam: exam.name, date: exam.date, total: row ? row.total : null, rank: row ? row.rank : null, size: rows.length };
    });
}

/** 成绩录入进度：应录格数 / 已录格数 */
export function entryProgress(state, examId, subjects) {
  const table = state.scores[examId] || {};
  const total = state.students.length * subjects.length;
  let done = 0;
  for (const student of state.students) {
    const row = table[student.id] || {};
    for (const subject of subjects) {
      if (row[subject.id] !== null && row[subject.id] !== undefined) done++;
    }
  }
  return { done, total, percent: total === 0 ? 0 : Math.round((done / total) * 1000) / 10, ratio: total === 0 ? 0 : (done / total) * 100 };
}

/** 全班及格率：所有已录分数中达到满分 60% 的比例 */
export function classPassRate(subjects, rows) {
  let entered = 0;
  let pass = 0;
  for (const row of rows) {
    for (const subject of subjects) {
      const value = row.scores[subject.id];
      if (value === null || value === undefined) continue;
      entered++;
      if (value >= subject.fullMark * 0.6) pass++;
    }
  }
  return entered === 0 ? 0 : (pass / entered) * 100;
}
