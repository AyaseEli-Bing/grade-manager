/**
 * 数据层底层：运行环境探测、本地文件读写、状态规范化。
 *
 * 环境自适应：
 *  - 桌面端（Tauri）：数据落盘到应用数据目录，导入导出走系统文件对话框。
 *  - 浏览器端（开发/验收）：降级为 localStorage，下载/上传由 io.js 处理。
 *
 * 上层只依赖本文件导出的能力，不感知底层差异。统计计算见 calc.js，导入导出见 io.js。
 */

export const SCHEMA_VERSION = 1;

export function uid(prefix) {
  return `${prefix}_${Date.now().toString(36)}${Math.random().toString(36).slice(2, 7)}`;
}

export function todayStr() {
  const d = new Date();
  const p = (n) => String(n).padStart(2, '0');
  return `${d.getFullYear()}-${p(d.getMonth() + 1)}-${p(d.getDate())}`;
}

/** 首次运行时的默认数据：一班级、六个常规科目、一场空考试 */
export function createDefaultState() {
  return {
    version: SCHEMA_VERSION,
    className: '高一(1)班',
    subjects: [
      { id: 's_zh', name: '语文', fullMark: 150 },
      { id: 's_ma', name: '数学', fullMark: 150 },
      { id: 's_en', name: '英语', fullMark: 150 },
      { id: 's_ph', name: '物理', fullMark: 100 },
      { id: 's_ch', name: '化学', fullMark: 100 },
      { id: 's_bi', name: '生物', fullMark: 100 },
    ],
    students: [],
    exams: [{ id: 'e_default', name: '第一次月考', date: todayStr(), subjectIds: [] }],
    scores: {},
    activeExamId: 'e_default',
    updatedAt: new Date().toISOString(),
  };
}

/* ------------------------------------------------------------------ */
/* 运行环境与磁盘能力                                                   */
/* ------------------------------------------------------------------ */

/**
 * 桌面端桥接。
 *
 * 【关键约束】本项目前端没有打包器，WebView 里不存在 node_modules 解析，
 * 因此**绝不能出现裸模块说明符**（即不以 ./ 或 ../ 开头的导入路径）。
 * 这类写法在 Tauri 生产环境下会直接抛出模块解析失败，导致应用启动即报错。
 *
 * Tauri v2 的 JS 桥是恒定注入的，直接使用它即可，无需任何 npm 包参与运行时。
 */
function bridge() {
  if (typeof window === 'undefined') return null;
  const internals = window.__TAURI_INTERNALS__;
  return internals && typeof internals.invoke === 'function' ? internals : null;
}

/** 是否运行在桌面外壳内（同步判定，供分支逻辑使用） */
export function isDesktop() {
  return bridge() !== null;
}

async function invoke(cmd, args) {
  const api = bridge();
  if (!api) throw new Error(`当前不在桌面环境中，无法执行 ${cmd}`);
  return api.invoke(cmd, args || {});
}

/** 文件对话框只接受 { name, extensions } 形状的过滤器 */
function toFilters(filters) {
  return (filters || []).map((f) => ({ name: f.name, extensions: f.extensions }));
}

/** 弹出「另存为」对话框，返回用户选择的路径；取消返回 null */
export async function pickSavePath(defaultPath, filters) {
  if (!bridge()) return null;
  return invoke('plugin:dialog|save', { options: { defaultPath, filters: toFilters(filters) } });
}

/** 弹出「打开文件」对话框，返回用户选择的路径；取消返回 null */
export async function pickOpenPath(filters) {
  if (!bridge()) return null;
  return invoke('plugin:dialog|open', { options: { multiple: false, filters: toFilters(filters) } });
}

export async function readDiskFile(path) {
  return invoke('read_file', { path });
}

export async function writeDiskFile(path, content) {
  return invoke('write_file', { path, content });
}

/* ------------------------------------------------------------------ */
/* 主数据文件                                                          */
/* ------------------------------------------------------------------ */

const LS_KEY = 'grade-manager-state';

/** 最近一次读取失败的原因，由界面层取走并提示，避免"打开就是空的"这类静默数据丢失 */
let lastLoadError = null;

export function takeLoadError() {
  const err = lastLoadError;
  lastLoadError = null;
  return err;
}

export async function loadState() {
  const desktop = isDesktop();
  try {
    const raw = desktop ? await invoke('load_data') : localStorage.getItem(LS_KEY);
    if (!raw || raw === 'null') return createDefaultState();
    return normalizeState(JSON.parse(raw));
  } catch (err) {
    console.error('[store] 读取失败，回退默认数据', err);
    lastLoadError = {
      message: `本地数据读取失败，已载入空白数据以避免崩溃。原始数据文件未被修改，请检查后重试。`,
      detail: String((err && err.message) || err),
    };
    return createDefaultState();
  }
}

let saveTimer = null;

/** 防抖保存：连续编辑时只写一次盘 */
export function scheduleSave(getState, onSaved) {
  if (saveTimer) clearTimeout(saveTimer);
  saveTimer = setTimeout(() => saveState(getState(), onSaved), 400);
}

export async function saveState(state, onSaved) {
  const payload = JSON.stringify({ ...state, updatedAt: new Date().toISOString() }, null, 2);
  try {
    if (isDesktop()) await invoke('save_data', { content: payload });
    else localStorage.setItem(LS_KEY, payload);
    if (onSaved) onSaved(null);
  } catch (err) {
    console.error('[store] 保存失败', err);
    if (onSaved) onSaved(err);
  }
}

/** 主数据文件的磁盘路径，用于界面展示 */
export async function getDataPath() {
  if (!isDesktop()) return '浏览器 localStorage（桌面版将保存为本地文件）';
  try {
    return await invoke('data_path');
  } catch {
    return '未知位置';
  }
}

/** 创建一份带时间戳的本地备份（导入前自动调用），返回备份文件路径 */
export async function backupNow() {
  if (!isDesktop()) return '';
  try {
    return await invoke('create_backup');
  } catch {
    return '';
  }
}

/**
 * 上报启动结果到数据目录的 boot-status.json。
 *
 * WebView 内的报错不会出现在终端，因此这是"打包后的应用能否正常启动"唯一的
 * 自动化证据来源，同时也给用户一个可直接查看的排查文件。
 */
export async function reportBoot(ok, detail) {
  if (!isDesktop()) return;
  try {
    await invoke('report_boot', { ok, detail: String(detail || ''), at: new Date().toISOString() });
  } catch {
    /* 上报失败不应影响主流程 */
  }
}

/* ------------------------------------------------------------------ */
/* 结构规范化                                                          */
/* ------------------------------------------------------------------ */

/** 把任意来源的数据（本地文件 / 导入文件）规范化为合法状态，剔除悬空引用 */
export function normalizeState(raw) {
  const base = createDefaultState();
  if (!raw || typeof raw !== 'object') return base;

  const state = {
    version: SCHEMA_VERSION,
    className: typeof raw.className === 'string' && raw.className ? raw.className : base.className,
    subjects: Array.isArray(raw.subjects) ? raw.subjects.map(normalizeSubject).filter(Boolean) : base.subjects,
    students: Array.isArray(raw.students) ? raw.students.map(normalizeStudent).filter(Boolean) : [],
    exams: [],
    scores: {},
    activeExamId: '',
    updatedAt: raw.updatedAt || new Date().toISOString(),
  };

  const subjectIds = new Set(state.subjects.map((s) => s.id));

  state.exams = (Array.isArray(raw.exams) ? raw.exams : [])
    .map((e) => ({
      id: String((e && e.id) || uid('e')),
      name: String((e && e.name) || '未命名考试'),
      date: String((e && e.date) || ''),
      subjectIds: Array.isArray(e && e.subjectIds) ? e.subjectIds.filter((id) => subjectIds.has(id)) : [],
    }))
    .filter((e) => e.name);
  if (state.exams.length === 0) state.exams = base.exams;

  const studentIds = new Set(state.students.map((s) => s.id));
  if (raw.scores && typeof raw.scores === 'object') {
    for (const [examId, byStudent] of Object.entries(raw.scores)) {
      if (!state.exams.some((e) => e.id === examId) || !byStudent) continue;
      const table = {};
      for (const [studentId, bySubject] of Object.entries(byStudent)) {
        if (!studentIds.has(studentId) || !bySubject) continue;
        const row = {};
        for (const [subjectId, value] of Object.entries(bySubject)) {
          if (!subjectIds.has(subjectId)) continue;
          const n = Number(value);
          row[subjectId] = Number.isFinite(n) ? n : null;
        }
        table[studentId] = row;
      }
      state.scores[examId] = table;
    }
  }

  state.activeExamId = state.exams.some((e) => e.id === raw.activeExamId)
    ? raw.activeExamId
    : state.exams[0].id;

  return state;
}

function normalizeSubject(s) {
  if (!s || typeof s.name !== 'string' || !s.name.trim()) return null;
  const fullMark = Number(s.fullMark);
  return {
    id: String(s.id || uid('s')),
    name: s.name.trim(),
    fullMark: Number.isFinite(fullMark) && fullMark > 0 ? fullMark : 100,
  };
}

function normalizeStudent(s) {
  if (!s || typeof s.name !== 'string' || !s.name.trim()) return null;
  const gender = s.gender === '女' ? '女' : s.gender === '男' ? '男' : '';
  return {
    id: String(s.id || uid('st')),
    sid: String(s.sid === undefined || s.sid === null ? '' : s.sid).trim(),
    name: s.name.trim(),
    gender,
    note: String(s.note === undefined || s.note === null ? '' : s.note).trim(),
  };
}
