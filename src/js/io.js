/**
 * 导入 / 导出层：JSON 全量备份、CSV 成绩表导出、CSV 名单导入。
 * 桌面端走系统文件对话框 + 本地文件；浏览器端走下载 / 上传，行为一致。
 */

import {
  isDesktop,
  normalizeState,
  pickOpenPath,
  pickSavePath,
  readDiskFile,
  todayStr,
  uid,
  writeDiskFile,
} from './store.js';
import { buildRanking, examName, examSubjects } from './calc.js';

/* ------------------------------------------------------------------ */
/* JSON 全量备份                                                       */
/* ------------------------------------------------------------------ */

export async function exportJson(state) {
  const payload = JSON.stringify({ ...state, updatedAt: new Date().toISOString() }, null, 2);
  const name = `成绩数据_${state.className}_${todayStr()}.json`;
  if (isDesktop()) {
    const path = await pickSavePath(name, [{ name: 'JSON 数据', extensions: ['json'] }]);
    if (!path) return false;
    await writeDiskFile(path, payload);
    return true;
  }
  downloadBlob(name, payload, 'application/json');
  return true;
}

/** @returns {Promise<null | {ok:boolean, state?:object, error?:string}>} */
export async function importJson() {
  if (isDesktop()) {
    const path = await pickOpenPath([{ name: 'JSON 数据', extensions: ['json'] }]);
    if (!path) return null;
    return parseImportText(await readDiskFile(path));
  }
  const text = await pickTextViaInput('.json,application/json');
  return text ? parseImportText(text) : null;
}

function parseImportText(text) {
  try {
    return { ok: true, state: normalizeState(JSON.parse(text)) };
  } catch (err) {
    return { ok: false, error: `文件解析失败：${err.message}` };
  }
}

/* ------------------------------------------------------------------ */
/* CSV 成绩表导出                                                      */
/* ------------------------------------------------------------------ */

/** @param scope 'current' 仅当前考试 | 'all' 全部考试 */
export async function exportCsv(state, scope = 'current') {
  const exams = scope === 'all' ? state.exams : state.exams.filter((e) => e.id === state.activeExamId);
  const lines = [];

  for (const exam of exams) {
    const subjects = examSubjects(state, exam);
    lines.push(['学号', '姓名', ...subjects.map((s) => s.name), '总分', '平均分', '得分率', '排名'].map(csvCell).join(','));
    for (const row of buildRanking(state, exam.id)) {
      lines.push(
        [
          row.student.sid,
          row.student.name,
          ...subjects.map((s) => (row.scores[s.id] === null ? '' : row.scores[s.id])),
          row.counted === 0 ? '' : row.total,
          row.counted === 0 ? '' : row.average,
          `${row.rate}%`,
          row.counted === 0 ? '' : row.rank,
        ]
          .map(csvCell)
          .join(',')
      );
    }
    if (exams.length > 1) lines.push('');
  }

  const payload = `\uFEFF${lines.join('\n')}`;
  const label = scope === 'all' ? '全部考试' : examName(state, state.activeExamId);
  const name = `成绩表_${state.className}_${label}_${todayStr()}.csv`;

  if (isDesktop()) {
    const path = await pickSavePath(name, [{ name: 'CSV 表格', extensions: ['csv'] }]);
    if (!path) return false;
    await writeDiskFile(path, payload);
    return true;
  }
  downloadBlob(name, payload, 'text/csv;charset=utf-8');
  return true;
}

function csvCell(value) {
  const text = String(value === undefined || value === null ? '' : value);
  return /[",\n]/.test(text) ? `"${text.replace(/"/g, '""')}"` : text;
}

/* ------------------------------------------------------------------ */
/* CSV 名单导入                                                        */
/* ------------------------------------------------------------------ */

/**
 * 解析 CSV 为「学生数组 + 各科成绩」，不直接写入状态，由调用方决定合并策略。
 * 表头识别：学号 / 姓名 / 性别 / 备注 + 与现有科目同名的列。
 */
export async function importCsv(state) {
  let text;
  if (isDesktop()) {
    const path = await pickOpenPath([{ name: 'CSV 表格', extensions: ['csv'] }]);
    if (!path) return null;
    text = await readDiskFile(path);
  } else {
    text = await pickTextViaInput('.csv,text/csv');
  }
  if (!text) return null;
  return parseCsvIntoStudents(state, text.replace(/^\uFEFF/, ''));
}

export function parseCsvIntoStudents(state, text) {
  const rows = splitCsv(text);
  if (rows.length === 0) return { ok: false, error: 'CSV 内容为空' };

  const header = rows[0].map((h) => h.trim());
  const at = {
    sid: header.findIndex((h) => h === '学号' || /^(sid|no|id)$/i.test(h)),
    name: header.findIndex((h) => h === '姓名' || /^name$/i.test(h)),
    gender: header.findIndex((h) => h === '性别' || /^(gender|sex)$/i.test(h)),
    note: header.findIndex((h) => h === '备注' || /^(note|remark)$/i.test(h)),
  };
  if (at.name < 0) return { ok: false, error: 'CSV 缺少「姓名」列，无法导入' };

  const subjectCols = header
    .map((h, i) => ({ i, subject: state.subjects.find((s) => s.name === h) }))
    .filter((col) => col.subject);

  const cell = (row, index) => (index >= 0 && row[index] !== undefined ? String(row[index]).trim() : '');
  const students = [];
  const scores = {};

  for (const row of rows.slice(1)) {
    const name = cell(row, at.name);
    if (!name) continue;
    const id = uid('st');
    students.push({ id, sid: cell(row, at.sid), name, gender: cell(row, at.gender), note: cell(row, at.note) });

    if (subjectCols.length === 0) continue;
    const rowScores = {};
    for (const col of subjectCols) {
      const raw = cell(row, col.i);
      const num = Number(raw);
      rowScores[col.subject.id] = raw !== '' && Number.isFinite(num) ? num : null;
    }
    if (Object.keys(rowScores).length) scores[id] = rowScores;
  }

  if (students.length === 0) return { ok: false, error: '未解析到任何有效学生记录' };
  return { ok: true, students, scores, hasScores: Object.keys(scores).length > 0 };
}

/** 极简 CSV 解析：支持引号包裹、转义双引号与换行内的逗号 */
function splitCsv(text) {
  const rows = [];
  let row = [];
  let cell = '';
  let quoted = false;
  for (let i = 0; i < text.length; i++) {
    const ch = text[i];
    if (quoted) {
      if (ch === '"') {
        if (text[i + 1] === '"') {
          cell += '"';
          i++;
        } else quoted = false;
      } else cell += ch;
    } else if (ch === '"') quoted = true;
    else if (ch === ',') {
      row.push(cell);
      cell = '';
    } else if (ch === '\n') {
      row.push(cell);
      rows.push(row);
      row = [];
      cell = '';
    } else if (ch !== '\r') cell += ch;
  }
  row.push(cell);
  rows.push(row);
  return rows.filter((r) => r.some((c) => c !== ''));
}

/* ------------------------------------------------------------------ */
/* 浏览器端降级通道                                                    */
/* ------------------------------------------------------------------ */

function pickTextViaInput(accept) {
  return new Promise((resolve) => {
    const input = document.createElement('input');
    input.type = 'file';
    input.accept = accept;
    input.onchange = () => {
      const file = input.files && input.files[0];
      if (!file) return resolve(null);
      const reader = new FileReader();
      reader.onload = () => resolve(String(reader.result));
      reader.onerror = () => resolve(null);
      reader.readAsText(file, 'utf-8');
    };
    input.click();
  });
}

function downloadBlob(name, content, mime) {
  const url = URL.createObjectURL(new Blob([content], { type: mime }));
  const a = document.createElement('a');
  a.href = url;
  a.download = name;
  a.click();
  setTimeout(() => URL.revokeObjectURL(url), 1000);
}
