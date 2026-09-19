/**
 * 视图：成绩录入 —— 网格化批量录入、越界自动钳制、实时总分与平均分。
 */

import { icon } from '../icons.js';
import { esc, emptyState, fmtNum, highlight, matches, toast } from '../ui.js';
import { entryProgress, examSubjects, round2 } from '../calc.js';

export function render(container, ctx) {
  const exam = ctx.activeExam;
  const { state } = ctx;

  if (!exam) {
    container.innerHTML = shell('成绩录入', '') + emptyStateInPanel('calendar', '当前没有可录入的考试', '请先在顶栏点击「管理考试」创建一场考试，再回到这里录入分数。');
    return;
  }
  if (state.students.length === 0) {
    container.innerHTML =
      shell('成绩录入', exam.name) +
      emptyStateInPanel('users', '还没有学生，无法录入成绩', '请先到「学生管理」新增学生或从 CSV 导入名单，之后即可在本页按科目录入分数。');
    return;
  }
  const subjects = examSubjects(state, exam);
  if (subjects.length === 0) {
    container.innerHTML =
      shell('成绩录入', exam.name) +
      emptyStateInPanel('book', `「${exam.name}」没有参考科目`, '请到「科目管理」至少创建一个科目，或在本场考试的设置中勾选参考科目。');
    return;
  }

  const rows = state.students.filter((s) => matches(s.name, ctx.search) || matches(s.sid, ctx.search));
  const progress = entryProgress(state, exam.id, subjects);

  container.innerHTML = `
    <div class="view-head">
      <h1 class="view-title">成绩录入</h1>
      <span class="view-sub">当前考试：${esc(exam.name)}${exam.date ? ` · ${esc(exam.date)}` : ''}</span>
    </div>

    <div class="panel">
      <div class="panel-head">
        <span class="panel-title">${icon('pencil', 16)}</span>
        <span class="grow">参考科目 ${subjects.length} 科 · 满分合计 ${subjects.reduce((sum, s) => sum + s.fullMark, 0)} 分</span>
        <span class="row">
          <span class="progress"><span class="progress-fill" id="score-progress" style="width:${progress.ratio}%"></span></span>
          <span class="progress-text" id="score-progress-text">${progress.done} / ${progress.total} · ${progress.percent}%</span>
        </span>
      </div>
      <div class="table-scroll">
        ${
          rows.length === 0
            ? emptyState({
                iconName: 'search',
                title: '没有匹配的学生',
                desc: `没有姓名或学号包含「${ctx.search.trim()}」的学生，清空搜索即可继续录入。`,
                actions: [{ act: 'clear-search', label: '清空搜索', iconName: 'refresh' }],
              })
            : `<table class="table scores-table">
          <thead>
            <tr>
              <th class="sticky-col sticky-left col-student">学生</th>
              ${subjects
                .map(
                  (subject) =>
                    `<th class="col-score"><span class="th-stacked"><span>${esc(subject.name)}</span><span class="th-sub">满分 ${esc(
                      subject.fullMark
                    )}</span></span></th>`
                )
                .join('')}
              <th class="sticky-col sticky-right-2 col-total">总分</th>
              <th class="sticky-col sticky-right col-avg">平均分</th>
            </tr>
          </thead>
          <tbody>${rows.map((student, r) => rowHtml(ctx, student, subjects, r, exam)).join('')}</tbody>
          <tfoot>${footerHtml(ctx, rows, subjects, exam)}</tfoot>
        </table>`
        }
      </div>
    </div>`;

  bind(container, ctx, rows, subjects, exam);
}

function shell(title, sub) {
  return `<div class="view-head"><h1 class="view-title">${esc(title)}</h1>${
    sub ? `<span class="view-sub">当前考试：${esc(sub)}</span>` : ''
  }</div>`;
}

function emptyStateInPanel(iconName, title, desc) {
  return `<div class="panel">${emptyState({ iconName, title, desc })}</div>`;
}

function rowHtml(ctx, student, subjects, rowIndex, exam) {
  const table = ctx.state.scores[exam.id] || {};
  const row = table[student.id] || {};
  const cells = subjects
    .map((subject, colIndex) => {
      const raw = row[subject.id];
      const value = raw === null || raw === undefined ? '' : String(raw);
      return `<td class="score-cell"><input class="score-input ${toneClass(raw, subject.fullMark)}"
          inputmode="decimal" autocomplete="off" value="${esc(value)}" placeholder="—"
          data-id="${esc(student.id)}" data-sid="${esc(subject.id)}" data-full="${esc(subject.fullMark)}"
          data-r="${rowIndex}" data-c="${colIndex}" aria-label="${esc(student.name)} 的 ${esc(subject.name)} 成绩" /></td>`;
    })
    .join('');
  const totals = computeTotals(row, subjects);
  return `<tr data-id="${esc(student.id)}">
      <td class="sticky-col sticky-left col-student">
        <div class="row">
          <span class="col-index">${rowIndex + 1}</span>
          <span class="student-cell">
            <span class="student-name">${highlight(student.name, ctx.search)}</span>
            <span class="th-sub">${highlight(student.sid || '未填学号', ctx.search)}</span>
          </span>
        </div>
      </td>
      ${cells}
      <td class="sticky-col sticky-right-2 col-total num-cell" data-cell="total">${fmtNum(totals.total)}</td>
      <td class="sticky-col sticky-right col-avg num-cell" data-cell="average">${fmtNum(totals.average)}</td>
    </tr>`;
}

function footerHtml(ctx, rows, subjects, exam) {
  const table = ctx.state.scores[exam.id] || {};
  const cells = subjects
    .map((subject) => {
      let entered = 0;
      let missing = 0;
      for (const student of rows) {
        const value = (table[student.id] || {})[subject.id];
        if (value === null || value === undefined) missing++;
        else entered++;
      }
      return `<td class="num-cell" data-foot="${esc(subject.id)}">${entered} / ${missing}</td>`;
    })
    .join('');
  return `<tr>
      <td class="sticky-col sticky-left">已录入 / 缺考</td>
      ${cells}
      <td class="sticky-col sticky-right-2 col-total"></td>
      <td class="sticky-col sticky-right col-avg"></td>
    </tr>`;
}

function toneClass(value, fullMark) {
  if (value === null || value === undefined) return '';
  const n = Number(value);
  if (!Number.isFinite(n)) return '';
  if (n >= fullMark * 0.9) return 'is-excellent';
  if (n < fullMark * 0.6) return 'is-fail';
  return '';
}

function computeTotals(row, subjects) {
  let total = 0;
  let counted = 0;
  for (const subject of subjects) {
    const value = row[subject.id];
    if (value === null || value === undefined) continue;
    if (!Number.isFinite(Number(value))) continue;
    total += Number(value);
    counted++;
  }
  return { total: round2(total), average: counted ? round2(total / counted) : null };
}

function bind(container, ctx, rows, subjects, exam) {
  container.querySelectorAll('[data-empty-act]').forEach((node) => {
    node.addEventListener('click', () => ctx.setSearch(''));
  });

  container.querySelectorAll('.score-input').forEach((input) => {
    input.addEventListener('input', () => {
      const cleaned = input.value.replace(/[^\d.]/g, '').replace(/(\..*)\./g, '$1');
      if (cleaned !== input.value) input.value = cleaned;
      input.classList.remove('is-excellent', 'is-fail');
    });

    input.addEventListener('keydown', (event) => {
      const r = Number(input.dataset.r);
      const c = Number(input.dataset.c);
      if (event.key === 'Enter' || event.key === 'ArrowDown') {
        event.preventDefault();
        focusCell(container, r + 1, c, rows.length);
      } else if (event.key === 'ArrowUp') {
        event.preventDefault();
        focusCell(container, r - 1, c, rows.length);
      }
    });

    input.addEventListener('blur', () => commit(ctx, input, container, exam, subjects));
    input.addEventListener('focus', () => input.select());
  });
}

function focusCell(container, r, c, rowCount) {
  if (r < 0 || r >= rowCount) return;
  const next = container.querySelector(`.score-input[data-r="${r}"][data-c="${c}"]`);
  if (next) {
    next.focus();
    next.select();
  }
}

function commit(ctx, input, container, exam, subjects) {
  const studentId = input.dataset.id;
  const subjectId = input.dataset.sid;
  const fullMark = Number(input.dataset.full);
  const student = ctx.state.students.find((s) => s.id === studentId);
  const subject = subjects.find((s) => s.id === subjectId);
  const raw = String(input.value).trim();

  let value = null;
  let invalid = false;
  let clamped = false;
  if (raw !== '') {
    const n = Number(raw);
    if (!Number.isFinite(n)) {
      invalid = true;
    } else if (n < 0) {
      clamped = true;
      value = 0;
    } else if (n > fullMark) {
      clamped = true;
      value = fullMark;
    } else {
      value = round2(n);
    }
  }

  const next = ctx.clone();
  if (!next.scores[exam.id]) next.scores[exam.id] = {};
  const row = next.scores[exam.id][studentId] || {};
  if (value === null) delete row[subjectId];
  else row[subjectId] = value;
  next.scores[exam.id][studentId] = row;
  ctx.setState(next);

  input.classList.remove('is-excellent', 'is-fail', 'is-invalid');
  input.value = value === null ? '' : String(value);
  if (value !== null) {
    const tone = toneClass(value, fullMark);
    if (tone) input.classList.add(tone);
  }

  if (clamped) {
    input.classList.add('is-invalid');
    setTimeout(() => input.classList.remove('is-invalid'), 1400);
    toast(`${student ? student.name : '该学生'}的「${subject ? subject.name : ''}」成绩超出 0 到 ${fullMark} 的范围，已修正为 ${value}`, 'error');
  } else if (invalid) {
    input.classList.add('is-invalid');
    setTimeout(() => input.classList.remove('is-invalid'), 1400);
    toast('只接受数字成绩，非数字输入已被忽略', 'error');
  }

  patchRow(container, next, exam, subjects, input);
  patchFoot(container, next, exam, subjects);
  patchProgress(container, entryProgress(next, exam.id, subjects));
}

function patchRow(container, state, exam, subjects, input) {
  const tr = input.closest('tr');
  if (!tr) return;
  const row = (state.scores[exam.id] || {})[tr.dataset.id] || {};
  const totals = computeTotals(row, subjects);
  const totalCell = tr.querySelector('[data-cell="total"]');
  const avgCell = tr.querySelector('[data-cell="average"]');
  if (totalCell) totalCell.textContent = fmtNum(totals.total);
  if (avgCell) avgCell.textContent = fmtNum(totals.average);
}

function patchFoot(container, state, exam, subjects) {
  const table = state.scores[exam.id] || {};
  const bodyRows = Array.from(container.querySelectorAll('tbody tr'));
  for (const subject of subjects) {
    const cell = container.querySelector(`[data-foot="${subject.id}"]`);
    if (!cell) continue;
    let entered = 0;
    let missing = 0;
    for (const tr of bodyRows) {
      const value = (table[tr.dataset.id] || {})[subject.id];
      if (value === null || value === undefined) missing++;
      else entered++;
    }
    cell.textContent = `${entered} / ${missing}`;
  }
}

function patchProgress(container, progress) {
  const fill = container.querySelector('#score-progress');
  const text = container.querySelector('#score-progress-text');
  if (fill) fill.style.width = `${progress.ratio}%`;
  if (text) text.textContent = `${progress.done} / ${progress.total} · ${progress.percent}%`;
}
