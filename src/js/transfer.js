/**
 * 导入 / 导出 与数据合并（学生按 学号+姓名 去重、科目按名称去重、考试按名称去重、成绩覆盖）。
 * 所有文件读写能力均由 store.js 提供，本模块不直接触碰 Tauri API。
 */

import { backupNow, normalizeState, todayStr, uid } from './store.js';
import { exportCsv, exportJson, importCsv, importJson } from './io.js';
import { examName } from './calc.js';
import { toast } from './ui.js';
import { choiceModal, confirmBox } from './form.js';

/* ------------------------------------------------------------------ */
/* 导出                                                                */
/* ------------------------------------------------------------------ */

export async function handleExportJson(state) {
  try {
    const done = await exportJson(state);
    if (done) toast(`已导出完整数据备份（${state.className}）`);
  } catch (err) {
    toast(`导出失败：${err && err.message ? err.message : err}`, 'error');
  }
}

export async function handleExportCsv(state) {
  const scope = await choiceModal({
    title: '导出成绩表 CSV',
    message: `请选择导出范围，文件将包含 ${state.className} 的全班成绩（总分、平均分、排名）。`,
    options: [
      { label: '仅当前考试', value: 'current', kind: 'primary', iconName: 'file-text', desc: '只导出当前选中考试的成绩表' },
      { label: '全部考试', value: 'all', kind: 'secondary', iconName: 'database', desc: '一次导出所有考试，按考试分段' },
    ],
    width: 420,
  });
  if (!scope) return;
  try {
    const done = await exportCsv(state, scope);
    if (done) toast(`已导出${scope === 'all' ? '全部考试' : '当前考试'}成绩表 CSV`);
  } catch (err) {
    toast(`CSV 导出失败：${err && err.message ? err.message : err}`, 'error');
  }
}

/* ------------------------------------------------------------------ */
/* 学生名单 CSV 导入合并                                                */
/* ------------------------------------------------------------------ */

/**
 * 导入 CSV 名单并与现有学生合并（学号+姓名 去重），可选同时写入成绩。
 * @returns {Promise<null | {state:object, added:number, merged:number, hasScores:boolean}>}
 */
export async function handleImportCsvStudents(state) {
  let result;
  try {
    result = await importCsv(state);
  } catch (err) {
    toast(`读取 CSV 失败：${err && err.message ? err.message : err}`, 'error');
    return null;
  }
  if (!result) return null;
  if (!result.ok) {
    toast(result.error, 'error');
    return null;
  }

  let writeScores = result.hasScores;
  if (writeScores) {
    writeScores = await confirmBox(
      `CSV 中检测到与现有科目同名的成绩列，是否一并写入当前考试「${examName(state, state.activeExamId)}」？已存在的学生成绩会被覆盖。`,
      { title: '导入名单中的成绩', confirmText: '写入当前考试', danger: false }
    );
  }

  const next = cloneState(state);
  const index = new Map();
  for (const student of next.students) index.set(`${student.sid}||${student.name}`, student.id);

  let added = 0;
  let merged = 0;

  for (const student of result.students) {
    const key = `${student.sid}||${student.name}`;
    const existingId = index.get(key);
    if (existingId) {
      merged++;
      if (writeScores) writeStudentScores(next, existingId, result.scores[student.id]);
      continue;
    }
    const id = uid('st');
    next.students.push({ ...student, id });
    index.set(key, id);
    added++;
    if (writeScores) writeStudentScores(next, id, result.scores[student.id]);
  }

  return { state: next, added, merged, hasScores: writeScores };
}

function writeStudentScores(state, studentId, bySubject) {
  if (!bySubject) return;
  const examId = state.activeExamId;
  if (!state.scores[examId]) state.scores[examId] = {};
  const row = state.scores[examId][studentId] || {};
  for (const [subjectId, value] of Object.entries(bySubject)) {
    if (value === null || value === undefined) continue;
    row[subjectId] = value;
  }
  state.scores[examId][studentId] = row;
}

/* ------------------------------------------------------------------ */
/* JSON 导入：替换 / 合并                                               */
/* ------------------------------------------------------------------ */

export async function handleImportJson(state) {
  const mode = await choiceModal({
    title: '导入数据备份',
    message: `即将读取一份 JSON 备份文件覆盖或补充当前数据（当前共 ${state.students.length} 名学生、${state.subjects.length} 个科目、${state.exams.length} 场考试）。导入前会自动创建本地备份。`,
    options: [
      { label: '合并到现有数据', value: 'merge', kind: 'primary', iconName: 'copy', desc: '学生按学号+姓名去重、科目按名称去重、考试按名称去重、成绩按学生+科目覆盖' },
      { label: '替换全部数据', value: 'replace', kind: 'danger', iconName: 'alert', desc: '丢弃当前全部数据，仅保留备份文件中的内容' },
    ],
    width: 480,
  });
  if (!mode) return null;
  return runImport(state, mode);
}

async function runImport(state, mode) {
  await backupNow();
  let picked;
  try {
    picked = await importJson();
  } catch (err) {
    toast(`读取备份文件失败：${err && err.message ? err.message : err}`, 'error');
    return null;
  }
  if (!picked) return null;
  if (!picked.ok) {
    toast(picked.error || '备份文件解析失败', 'error');
    return null;
  }
  if (mode === 'replace') return { state: picked.state, mode, report: null };
  const merged = mergeState(state, picked.state);
  return { state: merged.state, mode, report: merged.report };
}

/** 深拷贝一份可安全变更的 state */
export function cloneState(state) {
  return normalizeState(JSON.parse(JSON.stringify(state)));
}

/**
 * 合并两份数据：学生按 学号+姓名 去重、科目按名称去重、考试按名称去重、成绩按 学生+科目 覆盖。
 */
export function mergeState(base, incoming) {
  const next = cloneState(base);
  const report = { students: 0, subjects: 0, exams: 0, scores: 0 };

  /* 科目：按名称去重，保留现有 id 与满分 */
  const subjectIdMap = new Map();
  for (const s of incoming.subjects) {
    const hit = next.subjects.find((x) => x.name === s.name);
    if (hit) {
      subjectIdMap.set(s.id, hit.id);
      continue;
    }
    const id = uid('s');
    const fullMark = Number(s.fullMark);
    next.subjects.push({ id, name: s.name, fullMark: Number.isFinite(fullMark) && fullMark > 0 ? fullMark : 100 });
    subjectIdMap.set(s.id, id);
    report.subjects++;
  }

  /* 学生：按 学号+姓名 去重 */
  const studentIdMap = new Map();
  for (const s of incoming.students) {
    const hit = next.students.find((x) => x.sid === s.sid && x.name === s.name);
    if (hit) {
      studentIdMap.set(s.id, hit.id);
      continue;
    }
    const id = uid('st');
    next.students.push({ id, sid: s.sid || '', name: s.name, gender: s.gender || '', note: s.note || '' });
    studentIdMap.set(s.id, id);
    report.students++;
  }

  /* 考试：按名称去重 */
  const examIdMap = new Map();
  for (const e of incoming.exams) {
    const hit = next.exams.find((x) => x.name === e.name);
    if (hit) {
      examIdMap.set(e.id, hit.id);
      continue;
    }
    const id = uid('e');
    next.exams.push({
      id,
      name: e.name,
      date: e.date || todayStr(),
      subjectIds: (e.subjectIds || []).map((sid) => subjectIdMap.get(sid)).filter(Boolean),
    });
    examIdMap.set(e.id, id);
    report.exams++;
  }

  /* 成绩：按 考试+学生+科目 覆盖 */
  for (const [examId, table] of Object.entries(incoming.scores || {})) {
    const targetExamId = examIdMap.get(examId);
    if (!targetExamId) continue;
    if (!next.scores[targetExamId]) next.scores[targetExamId] = {};
    for (const [studentId, row] of Object.entries(table || {})) {
      const targetStudentId = studentIdMap.get(studentId);
      if (!targetStudentId) continue;
      const target = next.scores[targetExamId][targetStudentId] || {};
      for (const [subjectId, value] of Object.entries(row || {})) {
        const targetSubjectId = subjectIdMap.get(subjectId);
        if (!targetSubjectId) continue;
        if (value === null || value === undefined) continue;
        if (target[targetSubjectId] !== value) report.scores++;
        target[targetSubjectId] = value;
      }
      next.scores[targetExamId][targetStudentId] = target;
    }
  }

  return { state: next, report };
}
