/**
 * 通用 UI 原语：转义 / 数值格式化 / 排序 / 搜索高亮 / Toast / Modal 容器。
 * 无第三方依赖；图标统一来自 icons.js。
 */

import { icon } from './icons.js';

const ESCAPE_MAP = {
  '&': '&amp;',
  '<': '&lt;',
  '>': '&gt;',
  '"': '&quot;',
  "'": '&#39;',
};

/** HTML 转义：所有写入 innerHTML 的文本都必须过这一层 */
export function esc(value) {
  return String(value ?? '').replace(/[&<>"']/g, (c) => ESCAPE_MAP[c]);
}

/** 数值格式化：空值统一显示占位符 */
export function fmtNum(value, suffix = '') {
  if (value === null || value === undefined || value === '' || !Number.isFinite(Number(value))) return '—';
  return `${Math.round(Number(value) * 100) / 100}${suffix}`;
}

/** 百分比格式化：入参已是 0-100 */
export function fmtPct(value) {
  if (value === null || value === undefined || !Number.isFinite(Number(value))) return '—';
  return `${Math.round(Number(value) * 10) / 10}%`;
}

/**
 * 通用排序：数值比大小、字符串走中文 collator，空值恒排末尾（不随升降序翻转）。
 */
export function sortRows(rows, accessor, key, dir) {
  const factor = dir === 'desc' ? -1 : 1;
  const collator = new Intl.Collator('zh-Hans-CN');
  return rows.slice().sort((a, b) => {
    const va = accessor(a, key);
    const vb = accessor(b, key);
    const na = va === null || va === undefined || va === '';
    const nb = vb === null || vb === undefined || vb === '';
    if (na && nb) return 0;
    if (na) return 1;
    if (nb) return -1;
    if (typeof va === 'number' && typeof vb === 'number') return (va - vb) * factor;
    return collator.compare(String(va), String(vb)) * factor;
  });
}

/** 关键词命中判定：空关键词恒命中 */
export function matches(text, keyword) {
  const kw = String(keyword ?? '').trim().toLowerCase();
  if (!kw) return true;
  return String(text ?? '').toLowerCase().includes(kw);
}

/** 搜索关键词高亮，返回已转义的 HTML 片段 */
export function highlight(text, keyword) {
  const source = String(text ?? '');
  const kw = String(keyword ?? '').trim();
  if (!kw) return esc(source);
  const hay = source.toLowerCase();
  const needle = kw.toLowerCase();
  let out = '';
  let from = 0;
  for (;;) {
    const at = hay.indexOf(needle, from);
    if (at < 0) return out + esc(source.slice(from));
    out += esc(source.slice(from, at)) + `<mark class="hl">${esc(source.slice(at, at + needle.length))}</mark>`;
    from = at + needle.length;
  }
}

/** 可排序表头按钮 */
export function sortHeader(label, key, currentKey, currentDir) {
  const sorted = currentKey === key;
  const arrow = sorted ? (currentDir === 'asc' ? 'sort-asc' : 'sort-desc') : 'sort-desc';
  return `<button type="button" class="th-sort ${sorted ? 'is-sorted' : ''}" data-sort="${esc(key)}">
      <span>${esc(label)}</span>${icon(arrow, 14)}
    </button>`;
}

/** 空状态块 */
export function emptyState({ iconName = 'info', title, desc, actions = [] }) {
  return `<div class="empty">
      <div class="empty-icon">${icon(iconName, 28)}</div>
      <div class="empty-title">${esc(title)}</div>
      <p class="empty-desc">${esc(desc)}</p>
      <div class="empty-actions">${actions
        .map(
          (a) =>
            `<button type="button" class="btn ${a.kind || 'btn--secondary'}" data-empty-act="${esc(a.act)}">${icon(
              a.iconName,
              16
            )}<span>${esc(a.label)}</span></button>`
        )
        .join('')}</div>
    </div>`;
}

/* ------------------------------------------------------------------ */
/* 焦点保持（视图整体重渲染时避免打断用户输入）                          */
/* ------------------------------------------------------------------ */

/** 记录焦点元素的光标位置；不在指定元素上时返回 null */
export function captureInputFocus(host, selector) {
  const node = document.activeElement;
  if (!node || !host.contains(node) || typeof node.matches !== 'function' || !node.matches(selector)) return null;
  return { start: node.selectionStart, end: node.selectionEnd };
}

export function restoreInputFocus(host, snapshot, selector) {
  if (!snapshot) return;
  const node = host.querySelector(selector);
  if (!node) return;
  node.focus();
  try {
    node.setSelectionRange(snapshot.start, snapshot.end);
  } catch {
    /* 部分输入类型不支持选区，忽略即可 */
  }
}

/* ------------------------------------------------------------------ */
/* Toast                                                               */
/* ------------------------------------------------------------------ */

const TOAST_ICON = { success: 'check', error: 'alert', info: 'info' };

export function toast(message, type = 'success', duration = 2600) {
  const root = document.getElementById('toast-root');
  if (!root) return;
  const kind = type === 'error' || type === 'info' ? type : 'success';
  const node = document.createElement('div');
  node.className = `toast toast--${kind}`;
  node.innerHTML = `${icon(TOAST_ICON[kind], 16)}<span></span>`;
  node.lastElementChild.textContent = String(message ?? '');
  root.appendChild(node);
  requestAnimationFrame(() => node.classList.add('is-in'));
  setTimeout(() => {
    node.classList.remove('is-in');
    setTimeout(() => node.remove(), 220);
  }, duration);
}

/* ------------------------------------------------------------------ */
/* Modal 容器（业务表单见 form.js）                                     */
/* ------------------------------------------------------------------ */

const BTN_KIND = {
  primary: 'btn--primary',
  secondary: 'btn--secondary',
  ghost: 'btn--ghost',
  danger: 'btn--danger',
};

/**
 * 打开模态框，返回 { root, setBody, close }。
 * 支持 ESC 关闭、点击遮罩关闭、首个可聚焦元素自动获得焦点。
 */
export function openModal({ title, body = '', footer = [], width, onAction, onClose, scrimClose = true }) {
  const host = document.getElementById('modal-root');
  const scrim = document.createElement('div');
  scrim.className = 'modal-scrim';

  const dialog = document.createElement('div');
  dialog.className = 'modal';
  dialog.setAttribute('role', 'dialog');
  dialog.setAttribute('aria-modal', 'true');
  if (width) dialog.style.width = typeof width === 'number' ? `${width}px` : width;

  const renderBody = (html) => {
    const target = dialog.querySelector('.modal-body');
    if (target) target.innerHTML = html;
  };

  dialog.innerHTML = `
    <div class="modal-head">
      <h2 class="modal-title">${esc(title)}</h2>
      <button type="button" class="btn btn--ghost btn--sm btn--icon-only" data-act="close" aria-label="关闭">${icon('x', 16)}</button>
    </div>
    <div class="modal-body">${body}</div>
    ${
      footer.length
        ? `<div class="modal-foot">${footer
            .map(
              (f) =>
                `<button type="button" class="btn ${BTN_KIND[f.kind] || BTN_KIND.secondary}" data-act="${esc(f.act)}">${
                  f.iconName ? icon(f.iconName, 16) : ''
                }<span>${esc(f.label)}</span></button>`
            )
            .join('')}</div>`
        : ''
    }`;

  scrim.appendChild(dialog);
  host.appendChild(scrim);

  let closed = false;
  const close = () => {
    if (closed) return;
    closed = true;
    document.removeEventListener('keydown', onKey, true);
    scrim.remove();
    if (onClose) onClose();
  };

  const onKey = (event) => {
    if (event.key === 'Escape') {
      event.stopPropagation();
      close();
    }
  };

  scrim.addEventListener('mousedown', (event) => {
    if (scrimClose && event.target === scrim) close();
  });

  dialog.addEventListener('click', (event) => {
    const trigger = event.target.closest('[data-act]');
    if (!trigger) return;
    const act = trigger.dataset.act;
    if (act === 'close') {
      close();
      return;
    }
    if (onAction) onAction(act, trigger, { root: dialog, setBody: renderBody, close });
  });

  document.addEventListener('keydown', onKey, true);

  const first =
    dialog.querySelector('.modal-body input:not([type="checkbox"]), .modal-body select, .modal-body textarea') ||
    dialog.querySelector('.modal-foot button[data-act="ok"]') ||
    dialog.querySelector('.modal-head button[data-act="close"]');
  if (first) setTimeout(() => first.focus(), 0);

  return { root: dialog, scrim, setBody: renderBody, close };
}
