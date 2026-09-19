/**
 * 考试管理弹窗：新增 / 重命名 / 删除考试，设置考试日期与本科参考科目。
 * 科目的多选为空表示「考全部科目」。
 */

import { icon } from '../icons.js';
import { esc, openModal, toast } from '../ui.js';
import { confirmBox, formModal } from '../form.js';
import { todayStr, uid } from '../store.js';
import { examSubjects } from '../calc.js';

export function openExamManager(ctx) {
  openModal({
    title: '管理考试',
    body: listHtml(ctx),
    width: 540,
    footer: [{ label: '完成', kind: 'secondary', act: 'done' }],
    onAction: (act, trigger, api) => {
      if (act === 'done') {
        api.close();
        return;
      }
      if (act === 'create') {
        editExam(ctx, null, () => api.setBody(listHtml(ctx)));
        return;
      }
      const id = trigger ? trigger.dataset.exam : null;
      if (!id) return;
      if (act === 'edit') editExam(ctx, id, () => api.setBody(listHtml(ctx)));
      else if (act === 'pick') pickExam(ctx, id, () => api.setBody(listHtml(ctx)));
      else if (act === 'delete') removeExam(ctx, id, () => api.setBody(listHtml(ctx)));
    },
  });
}

function listHtml(ctx) {
  const { state } = ctx;
  return `<div class="col">
      <p class="modal-desc">每场考试可以单独指定参考科目；不勾选任何科目表示本次考全部 ${state.subjects.length} 个科目。</p>
      <div class="exam-list">${state.exams.map((exam) => examRowHtml(ctx, exam)).join('')}</div>
      <button type="button" class="btn btn--primary" data-act="create">${icon('plus', 16)}<span>新增考试</span></button>
    </div>`;
}

function examRowHtml(ctx, exam) {
  const { state } = ctx;
  const subjects = examSubjects(state, exam);
  const current = exam.id === state.activeExamId;
  const labels = subjects.length === state.subjects.length ? '全部科目' : subjects.map((s) => s.name).join('、');
  return `<div class="exam-row ${current ? 'is-current' : ''}">
      <div class="exam-row-main">
        <div class="exam-row-name">${esc(exam.name)}${
    current ? '<span class="badge badge--accent" style="margin-left:var(--space-2)">当前</span>' : ''
  }</div>
        <div class="exam-row-meta">${esc(exam.date || '未设置日期')} · ${subjects.length} 科 · ${esc(labels)}</div>
      </div>
      <button type="button" class="btn btn--secondary btn--sm" data-act="pick" data-exam="${esc(exam.id)}" ${current ? 'disabled' : ''}>设为当前</button>
      <button type="button" class="btn btn--ghost btn--sm btn--icon-only" data-act="edit" data-exam="${esc(exam.id)}" aria-label="编辑考试">${icon(
    'pencil',
    15
  )}</button>
      <button type="button" class="btn btn--ghost btn--sm btn--icon-only" data-act="delete" data-exam="${esc(exam.id)}" aria-label="删除考试">${icon(
    'trash',
    15
  )}</button>
    </div>`;
}

async function editExam(ctx, id, refreshBody) {
  const { state } = ctx;
  const exam = id ? state.exams.find((e) => e.id === id) : null;
  const values = await formModal({
    title: exam ? '编辑考试' : '新增考试',
    desc: '参考科目不勾选时，本次考试默认包含全部科目。',
    fields: [
      { name: 'name', label: '考试名称', value: exam ? exam.name : '', required: true, placeholder: '如：第一次月考、期中考试' },
      { name: 'date', label: '考试日期', value: exam ? exam.date : todayStr(), placeholder: 'YYYY-MM-DD', hint: '留空表示暂不设定，日期用于成绩趋势图的排序' },
      {
        name: 'subjectIds',
        label: '参考科目',
        type: 'checks',
        value: exam ? exam.subjectIds : [],
        options: state.subjects.map((s) => ({ value: s.id, label: `${s.name}（${s.fullMark}分）` })),
      },
    ],
    confirmText: exam ? '保存修改' : '新增考试',
  });
  if (!values) return;

  const name = String(values.name).trim();
  const date = String(values.date || '').trim();
  if (date && !/^\d{4}-\d{2}-\d{2}$/.test(date)) {
    toast('日期格式应为 YYYY-MM-DD，例如 2024-10-12', 'error');
    return;
  }
  const next = ctx.clone();
  if (next.exams.some((e) => e.name === name && e.id !== id)) {
    toast(`已存在同名考试「${name}」`, 'error');
    return;
  }

  if (exam) {
    const target = next.exams.find((e) => e.id === id);
    target.name = name;
    target.date = date;
    target.subjectIds = values.subjectIds;
    ctx.setState(next);
    ctx.refresh();
    refreshBody();
    toast(`已更新考试「${name}」`);
    return;
  }

  const created = { id: uid('e'), name, date, subjectIds: values.subjectIds };
  next.exams.push(created);
  next.activeExamId = created.id;
  ctx.setState(next);
  ctx.refresh();
  refreshBody();
  toast(`已新增考试「${name}」并切换为当前考试`);
}

function pickExam(ctx, id, refreshBody) {
  const next = ctx.clone();
  next.activeExamId = id;
  ctx.setState(next);
  ctx.refresh();
  refreshBody();
}

async function removeExam(ctx, id, refreshBody) {
  const { state } = ctx;
  const exam = state.exams.find((e) => e.id === id);
  if (!exam) return;
  if (state.exams.length <= 1) {
    toast('至少需要保留一场考试，无法删除最后一场', 'error');
    return;
  }
  const ok = await confirmBox(`确定删除考试「${exam.name}」吗？该场考试的全部成绩记录将一并清除，且不可恢复。`, {
    title: '删除考试',
    confirmText: '删除考试与成绩',
  });
  if (!ok) return;

  const next = ctx.clone();
  next.exams = next.exams.filter((e) => e.id !== id);
  delete next.scores[id];
  if (next.activeExamId === id) next.activeExamId = next.exams[0].id;
  ctx.setState(next);
  ctx.refresh();
  refreshBody();
  toast(`已删除考试「${exam.name}」及其成绩记录`);
}
