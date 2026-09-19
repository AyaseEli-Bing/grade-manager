/**
 * 应用入口：状态管理、视图路由、顶栏 / 侧边栏装配、导入导出入口。
 * 业务动作下发给各视图模块，本文件只做装配与协调。
 */

import { getDataPath, loadState, reportBoot, saveState, scheduleSave, takeLoadError } from './store.js';
import { captureInputFocus, restoreInputFocus, toast } from './ui.js';
import { examName } from './calc.js';
import {
  bindClassNameEditor,
  openExamMenu,
  renderBootFailure,
  renderSidebar,
  renderTopbar,
  updateDataPath,
  updateSaveState,
} from './shell.js';
import { cloneState, handleExportCsv, handleExportJson, handleImportCsvStudents, handleImportJson } from './transfer.js';
import { openExamManager } from './views/exams.js';
import { render as renderScores } from './views/scores.js';
import { render as renderStats } from './views/stats.js';
import { render as renderStudents } from './views/students.js';
import { render as renderSubjects } from './views/subjects.js';

const VIEWS = [
  { id: 'students', label: '学生管理', iconName: 'users', render: renderStudents, placeholder: '搜索学号或姓名', count: (s) => s.students.length },
  { id: 'scores', label: '成绩录入', iconName: 'pencil', render: renderScores, placeholder: '搜索要录入的学生', count: (s) => recordCount(s, s.activeExamId) },
  { id: 'stats', label: '统计排名', iconName: 'chart', render: renderStats, placeholder: '搜索学生查看排名', count: null },
  { id: 'subjects', label: '科目管理', iconName: 'book', render: renderSubjects, placeholder: '搜索科目名称', count: (s) => s.subjects.length },
];

let state = null;
let currentId = 'students';
let search = '';

const sidebar = () => document.getElementById('sidebar');
const topbar = () => document.getElementById('topbar');
const viewHost = () => document.getElementById('view');
const currentView = () => VIEWS.find((v) => v.id === currentId);

/* ------------------------------------------------------------------ */
/* 视图上下文：所有视图模块通过它读写状态                                */
/* ------------------------------------------------------------------ */

const ctx = {
  get state() {
    return state;
  },
  get activeExam() {
    return state.exams.find((e) => e.id === state.activeExamId) || state.exams[0] || null;
  },
  get search() {
    return search;
  },
  setSearch(value) {
    search = value;
    syncGlobalSearch(value);
    renderView();
  },
  setState(next) {
    state = next;
    persist();
    renderSidebarNow();
  },
  clone() {
    return cloneState(state);
  },
  refresh() {
    renderView();
  },
};

/* ------------------------------------------------------------------ */
/* 渲染                                                                */
/* ------------------------------------------------------------------ */

function renderAll() {
  renderSidebarNow();
  renderTopbarNow();
  renderView();
}

const SEARCH_SELECTOR = 'input[data-view-search]';

/** 视图重渲染会重建 DOM，此处统一保存并恢复搜索框焦点，避免输入被打断 */
function renderView() {
  const host = viewHost();
  const snapshot = captureInputFocus(host, SEARCH_SELECTOR);
  currentView().render(host, ctx);
  restoreInputFocus(host, snapshot, SEARCH_SELECTOR);
}

function renderSidebarNow() {
  renderSidebar(sidebar(), {
    items: VIEWS.map((v) => ({ id: v.id, label: v.label, iconName: v.iconName, count: v.count ? v.count(state) : null })),
    current: currentId,
    onNavigate: navigate,
  });
  updateSavePath();
}

function renderTopbarNow() {
  const exam = ctx.activeExam;
  renderTopbar(topbar(), {
    className: state.className,
    examName: exam ? exam.name : '暂无考试',
    examDate: exam && exam.date ? `考试日期 ${exam.date}` : '尚未设置考试日期',
    searchPlaceholder: currentView().placeholder,
    searchValue: search,
    onSearch: (value) => {
      search = value;
      renderView();
    },
  });
  bindTopbarActions();
  bindExamSwitcher();
  bindClassNameEditor(topbar(), {
    value: () => state.className,
    onCommit: (name) => {
      const next = cloneState(state);
      next.className = name;
      state = next;
      persist();
      toast(`班级名称已更新为「${name}」`);
    },
  });
}

/* ------------------------------------------------------------------ */
/* 路由                                                                */
/* ------------------------------------------------------------------ */

function navigate(id) {
  if (!VIEWS.some((v) => v.id === id)) return;
  currentId = id;
  search = '';
  renderAll();
}

/* ------------------------------------------------------------------ */
/* 顶栏交互                                                            */
/* ------------------------------------------------------------------ */

function bindTopbarActions() {
  topbar()
    .querySelectorAll('[data-act]')
    .forEach((node) => node.addEventListener('click', () => onTopbarAction(node.dataset.act)));
}

async function onTopbarAction(act) {
  if (act === 'manage-exam') {
    openExamManager(ctx);
    return;
  }
  if (act === 'export-json') {
    await handleExportJson(state);
    return;
  }
  if (act === 'export-csv') {
    await handleExportCsv(state);
    return;
  }
  if (act === 'import-csv') {
    const result = await handleImportCsvStudents(state);
    if (!result) return;
    if (result.added === 0 && result.merged === 0) {
      toast('CSV 中没有可导入的新记录', 'info');
      return;
    }
    state = result.state;
    persist();
    renderAll();
    toast(`导入完成：新增 ${result.added} 名学生${result.merged ? `，合并已存在 ${result.merged} 名` : ''}${result.hasScores ? '，并写入各科成绩' : ''}`);
    return;
  }
  if (act === 'import-json') {
    const result = await handleImportJson(state);
    if (!result) return;
    state = result.state;
    persist();
    renderAll();
    if (result.mode === 'replace') {
      toast('已用备份文件替换全部数据');
    } else {
      const r = result.report || { students: 0, subjects: 0, exams: 0, scores: 0 };
      toast(`合并完成：新增 ${r.students} 名学生、${r.subjects} 个科目、${r.exams} 场考试，更新 ${r.scores} 条成绩`);
    }
  }
}

function bindExamSwitcher() {
  const trigger = topbar().querySelector('[data-exam-toggle]');
  if (!trigger) return;
  trigger.addEventListener('click', () => {
    openExamMenu(trigger, {
      exams: state.exams,
      activeExamId: state.activeExamId,
      counts: Object.fromEntries(state.exams.map((e) => [e.id, `${recordCount(state, e.id)} 条成绩`])),
      onPick: (id) => {
        state = { ...state, activeExamId: id };
        persist();
        renderAll();
        toast(`已切换到「${examName(state, id)}」`);
      },
      onManage: () => openExamManager(ctx),
    });
  });
}

/* ------------------------------------------------------------------ */
/* 持久化与状态指示                                                     */
/* ------------------------------------------------------------------ */

function persist() {
  setSaveLabel(true, '正在保存…', null);
  scheduleSave(
    () => state,
    (err) => {
      if (err) {
        setSaveLabel(false, '保存失败 · 请检查写入权限', null);
        toast('数据保存失败，请检查数据目录的写入权限', 'error');
        return;
      }
      const at = clock();
      setSaveLabel(true, `已保存 · ${at}`, at);
    }
  );
}

function setSaveLabel(ok, label, at) {
  const host = sidebar();
  if (host) updateSaveState(host, { ok, at, label });
}

function updateSavePath() {
  getDataPath()
    .then((path) => updateDataPath(sidebar(), path))
    .catch(() => updateDataPath(sidebar(), '未知位置'));
}

function recordCount(target, examId) {
  const table = (target && target.scores && target.scores[examId]) || {};
  return Object.values(table).reduce((sum, row) => sum + Object.keys(row).length, 0);
}

function clock() {
  const d = new Date();
  const p = (n) => String(n).padStart(2, '0');
  return `${p(d.getHours())}:${p(d.getMinutes())}:${p(d.getSeconds())}`;
}

/** 视图内搜索时同步顶栏搜索框，两处入口保持一致 */
function syncGlobalSearch(value) {
  const node = document.getElementById('global-search');
  if (node && node.value !== value && document.activeElement !== node) node.value = value;
}

/* ------------------------------------------------------------------ */
/* 启动                                                                */
/* ------------------------------------------------------------------ */

async function boot() {
  state = await loadState();
  const loadError = takeLoadError();
  renderAll();
  setSaveLabel(true, '数据已载入', null);
  updateSavePath();
  if (loadError) {
    console.error('[boot] 数据读取异常', loadError.detail);
    toast(loadError.message, 'error', 8000);
  }
  reportBoot(true, loadError ? `ok-with-warning: ${loadError.detail}` : 'ok');

  window.addEventListener('beforeunload', () => {
    saveState(state);
  });
  document.addEventListener('visibilitychange', () => {
    if (document.hidden) saveState(state);
  });
}

boot().catch((err) => {
  const message = String((err && err.message) || err);
  console.error('[boot] 初始化失败', err);
  reportBoot(false, message);
  renderBootFailure(viewHost(), message);
});
