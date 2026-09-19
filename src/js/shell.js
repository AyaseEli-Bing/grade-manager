/**
 * 应用外壳：顶栏、侧边导航、考试切换下拉、保存状态指示。
 * 只负责渲染与事件转发，业务动作由 app.js 提供 handlers。
 */

import { icon } from './icons.js';
import { esc } from './ui.js';

export function renderSidebar(host, { items, current, onNavigate }) {
  host.innerHTML = `
    <div class="nav-group-title">数据视图</div>
    <nav class="nav">
      ${items
        .map(
          (it) => `<button type="button" class="nav-item ${it.id === current ? 'is-active' : ''}" data-nav="${esc(it.id)}">
            ${icon(it.iconName, 16)}
            <span>${esc(it.label)}</span>
            ${it.count === null || it.count === undefined ? '' : `<span class="nav-count">${esc(it.count)}</span>`}
          </button>`
        )
        .join('')}
    </nav>
    <div class="sidebar-foot">
      <span class="save-state" id="save-state"></span>
      <span class="data-path">${icon('database', 12)}<span class="path-text" id="data-path-text"></span></span>
    </div>`;
  host.querySelectorAll('[data-nav]').forEach((node) => {
    node.addEventListener('click', () => onNavigate(node.dataset.nav));
  });
}

/** 启动失败时的兜底界面：显示真实原因并给出重新加载入口，避免白屏无从排查 */
export function renderBootFailure(host, message) {
  host.innerHTML = `<div class="panel"><div class="empty">
      <div class="empty-icon">${icon('alert', 28)}</div>
      <div class="empty-title">应用初始化失败</div>
      <p class="empty-desc">${esc(message)}</p>
      <div class="empty-actions">
        <button type="button" class="btn btn--secondary" data-boot-act="reload">${icon('refresh', 16)}<span>重新加载</span></button>
      </div>
    </div></div>`;
  const reload = host.querySelector('[data-boot-act="reload"]');
  if (reload) reload.addEventListener('click', () => window.location.reload());
}

export function updateSaveState(host, { ok, at, label }) {
  const node = host.querySelector('#save-state');
  if (!node) return;
  node.className = `save-state ${ok ? 'is-ok' : 'is-error'}`;
  node.innerHTML = `${icon(ok ? 'check' : 'alert', 12)}<span>${esc(label)}</span>`;
  node.title = at ? `数据最后写入时间 ${at}` : label;
}

export function updateDataPath(host, path) {
  const node = host.querySelector('#data-path-text');
  if (!node) return;
  node.textContent = path;
  node.parentElement.title = path;
}

/* ------------------------------------------------------------------ */
/* Topbar                                                              */
/* ------------------------------------------------------------------ */

export function renderTopbar(host, { className, examName, examDate, searchPlaceholder, searchValue, onSearch }) {
  host.innerHTML = `
    <div class="brand">
      <div class="brand-mark">${icon('cap', 18)}</div>
      <button type="button" class="brand-class" id="class-name" title="点击修改班级名称">${esc(className)}</button>
    </div>

    <div class="search">
      <span class="search-icon">${icon('search', 16)}</span>
      <input type="search" id="global-search" value="${esc(searchValue)}" placeholder="${esc(searchPlaceholder)}" aria-label="搜索" />
    </div>

    <div class="topbar-actions">
      <div class="exam-select" id="exam-select">
        <button type="button" class="btn btn--secondary exam-trigger" data-exam-toggle aria-haspopup="true" aria-expanded="false" title="${esc(examDate)}">
          ${icon('calendar', 15)}<span class="exam-name">${esc(examName)}</span>${icon('chevron-down', 15)}
        </button>
      </div>
      <button type="button" class="btn btn--secondary btn--sm" data-act="manage-exam">${icon('settings', 15)}<span>管理考试</span></button>
      <span class="divider-v"></span>
      <button type="button" class="btn btn--secondary btn--sm" data-act="import-json" title="从 JSON 备份导入">${icon('upload', 15)}<span>导入</span></button>
      <button type="button" class="btn btn--secondary btn--sm" data-act="export-json" title="导出完整数据为 JSON">${icon('download', 15)}<span>导出</span></button>
      <button type="button" class="btn btn--secondary btn--sm" data-act="export-csv" title="导出成绩表为 CSV">${icon('file-text', 15)}<span>CSV</span></button>
      <button type="button" class="btn btn--secondary btn--sm" data-act="import-csv" title="从 CSV 导入学生名单与成绩">${icon('copy', 15)}<span>名单</span></button>
    </div>`;

  const search = host.querySelector('#global-search');
  let timer = null;
  search.addEventListener('input', () => {
    clearTimeout(timer);
    timer = setTimeout(() => onSearch(search.value), 180);
  });
}

/** 顶栏班级名就地编辑 */
export function bindClassNameEditor(host, { value, onCommit }) {
  const trigger = host.querySelector('#class-name');
  if (!trigger) return;
  const start = () => {
    const input = document.createElement('input');
    input.className = 'brand-class-input';
    input.value = value();
    input.maxLength = 24;
    trigger.replaceWith(input);
    input.focus();
    input.select();
    let finished = false;
    const finish = (commit) => {
      if (finished) return;
      finished = true;
      const next = input.value.trim();
      input.replaceWith(trigger);
      trigger.textContent = value();
      if (commit && next && next !== value()) onCommit(next);
    };
    input.addEventListener('blur', () => finish(true));
    input.addEventListener('keydown', (e) => {
      if (e.key === 'Enter') {
        e.preventDefault();
        finish(true);
      } else if (e.key === 'Escape') {
        e.preventDefault();
        finish(false);
      }
    });
  };
  trigger.addEventListener('click', start);
}

/* ------------------------------------------------------------------ */
/* Exam dropdown                                                       */
/* ------------------------------------------------------------------ */

export function openExamMenu(anchor, { exams, activeExamId, counts, onPick, onManage }) {
  const existing = anchor.parentElement.querySelector('.popover');
  if (existing) {
    existing.remove();
    setExamMenuOpen(anchor, false);
    return;
  }
  const menu = document.createElement('div');
  menu.className = 'popover popover--right';
  menu.innerHTML = `
    <div class="popover-label">切换当前考试</div>
    ${exams
      .map(
        (e) => `<button type="button" class="popover-item ${e.id === activeExamId ? 'is-active' : ''}" data-exam="${esc(e.id)}">
          <span>${esc(e.name)}</span><span class="item-sub">${esc(counts[e.id] ?? '')}</span>
        </button>`
      )
      .join('')}
    <div class="popover-sep"></div>
    <button type="button" class="popover-item" data-manage>${icon('settings', 15)}<span>管理考试</span></button>`;

  anchor.parentElement.appendChild(menu);
  setExamMenuOpen(anchor, true);

  const close = () => {
    menu.remove();
    setExamMenuOpen(anchor, false);
  };

  menu.addEventListener('click', (event) => {
    const pick = event.target.closest('[data-exam]');
    const manage = event.target.closest('[data-manage]');
    if (!pick && !manage) return;
    close();
    if (pick) onPick(pick.dataset.exam);
    else onManage();
  });

  const onDocClick = (event) => {
    if (!menu.contains(event.target) && !anchor.contains(event.target)) {
      close();
      document.removeEventListener('mousedown', onDocClick);
    }
  };
  setTimeout(() => document.addEventListener('mousedown', onDocClick), 0);
}

function setExamMenuOpen(anchor, open) {
  anchor.setAttribute('aria-expanded', open ? 'true' : 'false');
}

export function closeExamMenus(host) {
  host.querySelectorAll('.popover').forEach((menu) => {
    menu.remove();
    const trigger = host.querySelector('[data-exam-toggle]');
    if (trigger) setExamMenuOpen(trigger, false);
  });
}
