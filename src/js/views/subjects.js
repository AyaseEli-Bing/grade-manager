/**
 * 视图：科目管理 —— 新增 / 编辑 / 删除 / 排序科目。
 * 删除会级联清除该科目在所有考试中的成绩，必须二次确认。
 */

import { icon } from '../icons.js';
import { esc, emptyState, highlight, matches, sortHeader, sortRows, toast } from '../ui.js';
import { confirmBox, formModal } from '../form.js';
import { uid } from '../store.js';
import { examSubjects } from '../calc.js';

const local = { sortKey: 'order', sortDir: 'asc' };

export function render(container, ctx) {
  const { state } = ctx;

  if (state.subjects.length === 0) {
    container.innerHTML = `
      <div class="view-head"><h1 class="view-title">科目管理</h1></div>
      <div class="panel">${emptyState({
        iconName: 'book',
        title: '还没有任何科目',
        desc: '科目决定了成绩录入表的列，每科可以设置独立满分，例如语文数学 150 分、物理化学 100 分。',
        actions: [{ act: 'create', label: '新增科目', iconName: 'plus', kind: 'btn--primary' }],
      })}</div>`;
    const node = container.querySelector('[data-empty-act="create"]');
    if (node) node.addEventListener('click', () => openEditor(ctx, null));
    return;
  }

  const rows = filterRows(state, ctx.search);
  const total = state.subjects.length;

  container.innerHTML = `
    <div class="view-head">
      <h1 class="view-title">科目管理</h1>
      <span class="view-sub">共 ${total} 个科目，列表顺序即成绩录入表的列顺序</span>
    </div>

    <div class="toolbar">
      <button type="button" class="btn btn--primary" data-act="create">${icon('plus', 16)}<span>新增科目</span></button>
      <span class="spacer"></span>
      <span class="toolbar-hint">${
        ctx.search.trim() ? `匹配到 ${rows.length} 个科目` : '使用箭头按钮调整科目顺序，顺序同步反映到录入表与统计表'
      }</span>
    </div>

    ${
      rows.length === 0
        ? `<div class="panel">${emptyState({
            iconName: 'search',
            title: '没有匹配的科目',
            desc: `没有名称包含「${ctx.search.trim()}」的科目，清空搜索即可看到全部 ${total} 个科目。`,
            actions: [{ act: 'clear-search', label: '清空搜索', iconName: 'refresh' }],
          })}</div>`
        : `<div class="panel"><div class="table-scroll"><table class="table">
        <thead>
          <tr>
            <th>序号</th>
            <th>${sortHeader('科目名称', 'name', local.sortKey, local.sortDir)}</th>
            <th class="text-right">${sortHeader('满分', 'fullMark', local.sortKey, local.sortDir)}</th>
            <th class="text-right">${sortHeader('关联考试', 'exams', local.sortKey, local.sortDir)}</th>
            <th class="text-right">排序</th>
            <th class="text-right">操作</th>
          </tr>
        </thead>
        <tbody>${rows.map((row, index) => rowHtml(row, index, state, total, ctx.search)).join('')}</tbody>
      </table></div></div>`
    }`;

  bind(container, ctx, rows);
}

function filterRows(state, search) {
  const rows = state.subjects.map((subject, order) => ({
    subject,
    order,
    exams: relatedExamCount(state, subject.id),
  }));
  const filtered = rows.filter((row) => matches(row.subject.name, search));
  if (local.sortKey === 'order') return local.sortDir === 'desc' ? filtered.slice().reverse() : filtered;
  return sortRows(
    filtered,
    (row, key) => (key === 'name' ? row.subject.name : row[key]),
    local.sortKey,
    local.sortDir
  );
}

function relatedExamCount(state, subjectId) {
  return state.exams.filter((exam) => examSubjects(state, exam).some((s) => s.id === subjectId)).length;
}

function rowHtml(row, index, state, total, search) {
  const { subject } = row;
  const atFirst = row.order === 0;
  const atLast = row.order === total - 1;
  return `<tr data-id="${esc(subject.id)}">
      <td class="col-index">${index + 1}</td>
      <td>${highlight(subject.name, search)}</td>
      <td class="num-cell">${esc(subject.fullMark)}</td>
      <td class="num-cell">${esc(row.exams)}</td>
      <td class="col-act"><div class="row-actions">
        <button type="button" class="btn btn--ghost btn--sm btn--icon-only" data-act="up" ${atFirst ? 'disabled' : ''} aria-label="上移 ${esc(
    subject.name
  )}">${icon('arrow-up', 15)}</button>
        <button type="button" class="btn btn--ghost btn--sm btn--icon-only" data-act="down" ${atLast ? 'disabled' : ''} aria-label="下移 ${esc(
    subject.name
  )}">${icon('arrow-down', 15)}</button>
      </div></td>
      <td class="col-act"><div class="row-actions">
        <button type="button" class="btn btn--ghost btn--sm" data-act="edit">${icon('pencil', 15)}<span>编辑</span></button>
        <button type="button" class="btn btn--ghost btn--sm" data-act="delete">${icon('trash', 15)}<span>删除</span></button>
      </div></td>
    </tr>`;
}

function bind(container, ctx, rows) {
  container.querySelectorAll('[data-sort]').forEach((node) => {
    node.addEventListener('click', () => {
      const key = node.dataset.sort;
      if (local.sortKey === key) local.sortDir = local.sortDir === 'asc' ? 'desc' : 'asc';
      else {
        local.sortKey = key;
        local.sortDir = 'asc';
      }
      ctx.refresh();
    });
  });

  container.querySelector('[data-act="create"]').addEventListener('click', () => openEditor(ctx, null));

  container.querySelectorAll('[data-empty-act]').forEach((node) => {
    node.addEventListener('click', () => {
      if (node.dataset.emptyAct === 'create') openEditor(ctx, null);
      else ctx.setSearch('');
    });
  });

  container.querySelectorAll('tbody tr').forEach((tr) => {
    const id = tr.dataset.id;
    tr.querySelector('[data-act="edit"]').addEventListener('click', () => openEditor(ctx, id));
    tr.querySelector('[data-act="delete"]').addEventListener('click', () => removeSubject(ctx, id));
    const up = tr.querySelector('[data-act="up"]');
    if (up) up.addEventListener('click', () => move(ctx, id, -1));
    const down = tr.querySelector('[data-act="down"]');
    if (down) down.addEventListener('click', () => move(ctx, id, 1));
  });

  void rows;
}

function move(ctx, id, delta) {
  const next = ctx.clone();
  const from = next.subjects.findIndex((s) => s.id === id);
  const to = from + delta;
  if (from < 0 || to < 0 || to >= next.subjects.length) return;
  const moved = next.subjects[from];
  next.subjects.splice(from, 1);
  next.subjects.splice(to, 0, moved);
  ctx.setState(next);
  ctx.refresh();
}

async function openEditor(ctx, id) {
  const { state } = ctx;
  const subject = id ? state.subjects.find((s) => s.id === id) : null;
  const usedExams = subject ? relatedExamCount(state, subject.id) : 0;
  const values = await formModal({
    title: subject ? '编辑科目' : '新增科目',
    desc: subject
      ? `当前有 ${usedExams} 场考试包含「${subject.name}」。调整满分不会改动已录入的分数。`
      : '每科可以设置独立满分，新增后会立刻出现在所有考试的成绩录入表中。',
    fields: [
      {
        name: 'name',
        label: '科目名称',
        value: subject ? subject.name : '',
        required: true,
        placeholder: '如：语文、数学、英语',
      },
      {
        name: 'fullMark',
        label: '满分',
        type: 'number',
        value: subject ? subject.fullMark : 100,
        min: 1,
        max: 1000,
        required: true,
        hint: '取值 1 到 1000，常见为 100 或 150',
      },
    ],
    confirmText: subject ? '保存修改' : '新增科目',
  });
  if (!values) return;

  const name = String(values.name).trim();
  const next = ctx.clone();
  if (next.subjects.some((s) => s.name === name && s.id !== id)) {
    toast(`已存在同名科目「${name}」`, 'error');
    return;
  }

  if (subject) {
    const target = next.subjects.find((s) => s.id === id);
    target.name = name;
    target.fullMark = values.fullMark;
    ctx.setState(next);
    ctx.refresh();
    toast(`已更新科目「${name}」`);
    return;
  }

  next.subjects.push({ id: uid('s'), name, fullMark: values.fullMark });
  ctx.setState(next);
  ctx.refresh();
  toast(`已新增科目「${name}」，满分 ${values.fullMark}`);
}

async function removeSubject(ctx, id) {
  const { state } = ctx;
  const subject = state.subjects.find((s) => s.id === id);
  if (!subject) return;

  let records = 0;
  for (const table of Object.values(state.scores || {})) {
    for (const row of Object.values(table || {})) {
      if (row[subject.id] !== undefined && row[subject.id] !== null) records++;
    }
  }

  const ok = await confirmBox(
    `确定删除科目「${subject.name}」吗？将同时清除该科目在所有考试中的 ${records} 条成绩记录，且不可恢复。`,
    { title: '删除科目', confirmText: '删除科目与成绩' }
  );
  if (!ok) return;

  const next = ctx.clone();
  next.subjects = next.subjects.filter((s) => s.id !== id);
  for (const exam of next.exams) {
    exam.subjectIds = (exam.subjectIds || []).filter((sid) => sid !== id);
  }
  for (const examId of Object.keys(next.scores || {})) {
    for (const studentId of Object.keys(next.scores[examId] || {})) {
      delete next.scores[examId][studentId][subject.id];
    }
  }
  ctx.setState(next);
  ctx.refresh();
  toast(`已删除科目「${subject.name}」及 ${records} 条成绩记录`);
}
