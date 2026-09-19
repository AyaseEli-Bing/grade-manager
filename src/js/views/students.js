/**
 * 视图：学生管理 —— 名单维护、CSV 导入、批量删除。
 */

import { icon } from '../icons.js';
import { esc, emptyState, highlight, matches, sortHeader, sortRows, toast } from '../ui.js';
import { confirmBox, formModal } from '../form.js';
import { handleImportCsvStudents } from '../transfer.js';
import { uid } from '../store.js';

const local = { sortKey: 'sid', sortDir: 'asc', selected: new Set() };

export function render(container, ctx) {
  const { state } = ctx;

  if (state.students.length === 0) {
    container.innerHTML = `
      <div class="view-head"><h1 class="view-title">学生管理</h1></div>
      <div class="panel">${emptyState({
        iconName: 'users',
        title: '暂无学生数据',
        desc: '点击下方新增学生开始录入，或从 CSV 名单批量导入（支持同时带入学号、性别、备注与成绩列）。',
        actions: [
          { act: 'create', label: '新增学生', iconName: 'user-plus', kind: 'btn--primary' },
          { act: 'import-csv', label: '从 CSV 导入', iconName: 'upload' },
        ],
      })}</div>`;
    const create = container.querySelector('[data-empty-act="create"]');
    if (create) create.addEventListener('click', () => openEditor(ctx, null));
    const importBtn = container.querySelector('[data-empty-act="import-csv"]');
    if (importBtn) importBtn.addEventListener('click', () => importCsv(ctx));
    return;
  }

  const rows = sortRows(
    state.students.filter((s) => matches(s.name, ctx.search) || matches(s.sid, ctx.search)),
    (student, key) => student[key],
    local.sortKey,
    local.sortDir
  );
  const visibleIds = rows.map((s) => s.id);
  const selected = visibleIds.filter((id) => local.selected.has(id));
  const allChecked = rows.length > 0 && selected.length === rows.length;

  container.innerHTML = `
    <div class="view-head">
      <h1 class="view-title">学生管理</h1>
      <span class="view-sub">共 ${state.students.length} 名学生${ctx.search.trim() ? `，当前筛选 ${rows.length} 名` : ''}</span>
    </div>

    <div class="toolbar">
      <div class="search">
        <span class="search-icon">${icon('search', 16)}</span>
        <input type="search" data-view-search value="${esc(ctx.search)}" placeholder="按学号或姓名筛选" aria-label="筛选学生" />
      </div>
      <button type="button" class="btn btn--primary" data-act="create">${icon('user-plus', 16)}<span>新增学生</span></button>
      <button type="button" class="btn btn--secondary" data-act="import-csv">${icon('upload', 16)}<span>导入 CSV</span></button>
      <span class="spacer"></span>
      <span class="toolbar-hint">${selected.length ? `已选中 ${selected.length} 人` : '勾选左侧复选框可批量删除'}</span>
      <button type="button" class="btn btn--danger btn--sm" data-act="bulk-delete" ${selected.length ? '' : 'disabled'}>${icon(
        'trash',
        15
      )}<span>批量删除</span></button>
    </div>

    ${
      rows.length === 0
        ? `<div class="panel">${emptyState({
            iconName: 'search',
            title: '没有匹配的学生',
            desc: `没有学号或姓名包含「${ctx.search.trim()}」的学生，清空搜索即可看到全部 ${state.students.length} 名学生。`,
            actions: [{ act: 'clear-search', label: '清空搜索', iconName: 'refresh' }],
          })}</div>`
        : `<div class="panel"><div class="table-scroll"><table class="table">
        <thead>
          <tr>
            <th class="col-check"><span class="checkbox ${allChecked ? 'is-checked' : ''}" data-act="toggle-all" role="checkbox" tabindex="0" aria-checked="${allChecked}">${icon(
              'check',
              12
            )}</span></th>
            <th>序号</th>
            <th>${sortHeader('学号', 'sid', local.sortKey, local.sortDir)}</th>
            <th>${sortHeader('姓名', 'name', local.sortKey, local.sortDir)}</th>
            <th>性别</th>
            <th>备注</th>
            <th class="text-right">操作</th>
          </tr>
        </thead>
        <tbody>${rows.map((student, index) => rowHtml(student, index, ctx)).join('')}</tbody>
      </table></div></div>`
    }`;

  bind(container, ctx, visibleIds, selected.length);
}

function rowHtml(student, index, ctx) {
  const checked = local.selected.has(student.id);
  const gender = student.gender
    ? `<span class="badge ${student.gender === '女' ? 'badge--warn' : 'badge--accent'}">${esc(student.gender)}</span>`
    : '<span class="dim">未填写</span>';
  return `<tr data-id="${esc(student.id)}" class="${checked ? 'is-selected' : ''}">
      <td class="col-check"><span class="checkbox ${checked ? 'is-checked' : ''}" data-act="toggle" role="checkbox" tabindex="0" aria-checked="${checked}">${icon(
    'check',
    12
  )}</span></td>
      <td class="col-index">${index + 1}</td>
      <td class="num-cell">${highlight(student.sid || '未填写', ctx.search)}</td>
      <td>${highlight(student.name, ctx.search)}</td>
      <td>${gender}</td>
      <td class="dim">${highlight(student.note || '', ctx.search)}</td>
      <td class="col-act"><div class="row-actions">
        <button type="button" class="btn btn--ghost btn--sm" data-act="edit">${icon('pencil', 15)}<span>编辑</span></button>
        <button type="button" class="btn btn--ghost btn--sm" data-act="delete">${icon('trash', 15)}<span>删除</span></button>
      </div></td>
    </tr>`;
}

function bind(container, ctx, visibleIds, selectedCount) {
  const searchInput = container.querySelector('[data-view-search]');
  if (searchInput) {
    let timer = null;
    searchInput.addEventListener('input', () => {
      clearTimeout(timer);
      const value = searchInput.value;
      timer = setTimeout(() => ctx.setSearch(value), 180);
    });
  }

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

  const createBtn = container.querySelector('[data-act="create"]');
  if (createBtn) createBtn.addEventListener('click', () => openEditor(ctx, null));
  const importBtn = container.querySelector('[data-act="import-csv"]');
  if (importBtn) importBtn.addEventListener('click', () => importCsv(ctx));

  container.querySelectorAll('[data-empty-act]').forEach((node) => {
    node.addEventListener('click', () => {
      if (node.dataset.emptyAct === 'create') openEditor(ctx, null);
      else if (node.dataset.emptyAct === 'import-csv') importCsv(ctx);
      else ctx.setSearch('');
    });
  });

  const toggleAll = container.querySelector('[data-act="toggle-all"]');
  if (toggleAll) {
    toggleAll.addEventListener('click', () => {
      const allSelected = visibleIds.length > 0 && visibleIds.every((id) => local.selected.has(id));
      if (allSelected) visibleIds.forEach((id) => local.selected.delete(id));
      else visibleIds.forEach((id) => local.selected.add(id));
      ctx.refresh();
    });
  }

  const bulk = container.querySelector('[data-act="bulk-delete"]');
  if (bulk) bulk.addEventListener('click', () => bulkDelete(ctx, visibleIds));

  container.querySelectorAll('tbody tr').forEach((tr) => {
    const id = tr.dataset.id;
    const check = tr.querySelector('[data-act="toggle"]');
    check.addEventListener('click', () => {
      if (local.selected.has(id)) local.selected.delete(id);
      else local.selected.add(id);
      ctx.refresh();
    });
    tr.querySelector('[data-act="edit"]').addEventListener('click', () => openEditor(ctx, id));
    tr.querySelector('[data-act="delete"]').addEventListener('click', () => removeStudent(ctx, id));
  });

  void selectedCount;
}

async function bulkDelete(ctx, visibleIds) {
  const ids = visibleIds.filter((id) => local.selected.has(id));
  if (ids.length === 0) return;
  const ok = await confirmBox(
    `确定删除选中的 ${ids.length} 名学生吗？他们在所有考试中的成绩记录将一并清除，且不可恢复。`,
    { title: '批量删除学生', confirmText: `删除 ${ids.length} 名学生` }
  );
  if (!ok) return;
  applyRemoval(ctx, ids);
  local.selected.clear();
  ctx.refresh();
  toast(`已删除 ${ids.length} 名学生及其成绩记录`);
}

async function removeStudent(ctx, id) {
  const { state } = ctx;
  const student = state.students.find((s) => s.id === id);
  if (!student) return;
  const ok = await confirmBox(`确定删除学生「${student.name}」吗？该生在所有考试中的成绩记录将一并清除，且不可恢复。`, {
    title: '删除学生',
  });
  if (!ok) return;
  applyRemoval(ctx, [id]);
  local.selected.delete(id);
  ctx.refresh();
  toast(`已删除学生「${student.name}」`);
}

function applyRemoval(ctx, ids) {
  const set = new Set(ids);
  const next = ctx.clone();
  next.students = next.students.filter((s) => !set.has(s.id));
  for (const examId of Object.keys(next.scores || {})) {
    for (const id of ids) delete next.scores[examId][id];
  }
  ctx.setState(next);
}

async function openEditor(ctx, id) {
  const { state } = ctx;
  const student = id ? state.students.find((s) => s.id === id) : null;
  const values = await formModal({
    title: student ? '编辑学生信息' : '新增学生',
    desc: student ? '学号用于在 CSV 导出与导入去重，建议保持唯一。' : '姓名为必填项，学号建议填写以便导入时自动去重。',
    fields: [
      { name: 'name', label: '姓名', value: student ? student.name : '', required: true, placeholder: '如：张明轩' },
      { name: 'sid', label: '学号', value: student ? student.sid : '', placeholder: '如：20240101' },
      {
        name: 'gender',
        label: '性别',
        type: 'select',
        value: student ? student.gender || '' : '',
        options: [
          { value: '', label: '不限' },
          { value: '男', label: '男' },
          { value: '女', label: '女' },
        ],
      },
      { name: 'note', label: '备注', type: 'textarea', value: student ? student.note : '', placeholder: '如：走读 / 课代表 / 需要补考' },
    ],
    confirmText: student ? '保存修改' : '新增学生',
  });
  if (!values) return;

  const name = String(values.name).trim();
  const sid = String(values.sid || '').trim();
  const next = ctx.clone();
  const duplicate = next.students.find((s) => s.name === name && s.sid === sid && s.id !== id);
  if (duplicate) {
    toast(`学号 ${sid} 的「${name}」已在名单中`, 'error');
    return;
  }

  if (student) {
    const target = next.students.find((s) => s.id === id);
    target.name = name;
    target.sid = sid;
    target.gender = values.gender || '';
    target.note = String(values.note || '').trim();
    ctx.setState(next);
    ctx.refresh();
    toast(`已更新「${name}」的信息`);
    return;
  }

  const created = { id: uid('st'), sid, name, gender: values.gender || '', note: String(values.note || '').trim() };
  next.students.push(created);
  ctx.setState(next);
  ctx.refresh();
  toast(`已新增学生「${name}」`);
}

async function importCsv(ctx) {
  const result = await handleImportCsvStudents(ctx.state);
  if (!result) return;
  if (result.added === 0 && result.merged === 0) {
    toast('CSV 中没有可导入的新记录', 'info');
    return;
  }
  ctx.setState(result.state);
  ctx.refresh();
  toast(
    `导入完成：新增 ${result.added} 名${result.merged ? `，合并已存在 ${result.merged} 名` : ''}${
      result.hasScores ? '，并写入各科成绩' : ''
    }`
  );
}
