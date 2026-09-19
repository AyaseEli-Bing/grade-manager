/**
 * 视图：统计排名 —— KPI 概览、按学生排名、按科目统计、个人成绩趋势抽屉。
 * 数据来自 calc.js 的纯计算函数，图表由 charts.js 手写 SVG 提供。
 */

import { icon } from '../icons.js';
import { esc, emptyState, fmtNum, fmtPct, highlight, matches, sortHeader, sortRows } from '../ui.js';
import { buildRanking, classPassRate, examSubjects, round2, subjectStats } from '../calc.js';
import { distBars } from '../charts.js';
import { drawerHtml, kpiHtml, panelOnly, rankBadge } from './stats-parts.js';

const local = { tab: 'students', sortKey: 'total', sortDir: 'desc', openId: null };

export function render(container, ctx) {
  const exam = ctx.activeExam;
  const { state } = ctx;

  if (!exam) {
    container.innerHTML = panelOnly(
      '统计排名',
      emptyState({
        iconName: 'calendar',
        title: '还没有任何考试',
        desc: '统计与排名基于一场具体考试。请先在顶栏点击「管理考试」创建考试，再到「成绩录入」填写分数。',
      })
    );
    return;
  }
  if (state.students.length === 0) {
    container.innerHTML = panelOnly(
      `统计排名 · ${exam.name}`,
      emptyState({
        iconName: 'users',
        title: '还没有学生，暂时无法统计',
        desc: '请先到「学生管理」新增学生或从 CSV 导入名单，录入成绩后这里会自动生成总分、平均分与班级排名。',
      })
    );
    return;
  }
  const subjects = examSubjects(state, exam);
  if (subjects.length === 0) {
    container.innerHTML = panelOnly(
      `统计排名 · ${exam.name}`,
      emptyState({
        iconName: 'book',
        title: '本场考试没有参考科目',
        desc: '请到「科目管理」创建科目，或在本场考试的设置中勾选参考科目后再查看统计。',
      })
    );
    return;
  }

  const all = buildRanking(state, exam.id);
  const rows = all.filter((r) => matches(r.student.name, ctx.search) || matches(r.student.sid, ctx.search));
  const graded = all.filter((r) => r.counted > 0);
  const ranked = graded.slice().sort((a, b) => b.total - a.total);
  const fullSum = subjects.reduce((sum, s) => sum + s.fullMark, 0);

  const kpis = [
    {
      iconName: 'users',
      tone: 'users',
      label: '参考人数',
      value: rows.length,
      unit: '人',
      sub: `已录 ${graded.length} 人 · 待录 ${all.length - graded.length} 人`,
    },
    {
      iconName: 'chart',
      tone: 'chart',
      label: '全班平均分',
      value: graded.length ? round2(graded.reduce((sum, r) => sum + r.average, 0) / graded.length) : 0,
      unit: '分',
      sub: '按人均各科平均分计算',
    },
    {
      iconName: 'award',
      tone: 'award',
      label: '最高总分',
      value: ranked.length ? ranked[0].total : 0,
      unit: '分',
      sub: ranked.length ? `${ranked[0].student.name} · 满分 ${fullSum}` : '尚无成绩',
    },
    {
      iconName: 'trending',
      tone: '',
      label: '全科及格率',
      value: round2(classPassRate(subjects, all)),
      unit: '%',
      sub: '已录分数达到满分 60% 的比例',
    },
  ];

  container.innerHTML = `
    <div class="view-head">
      <h1 class="view-title">统计排名</h1>
      <span class="view-sub">${esc(exam.name)}${exam.date ? ` · ${esc(exam.date)}` : ''} · 参考 ${subjects.length} 科 · 满分合计 ${fullSum} 分</span>
    </div>

    <div class="kpi-grid">${kpis.map(kpiHtml).join('')}</div>

    <div class="toolbar">
      <div class="search">
        <span class="search-icon">${icon('search', 16)}</span>
        <input type="search" data-view-search value="${esc(ctx.search)}" placeholder="按姓名或学号筛选学生" aria-label="筛选统计对象" />
      </div>
      <div class="segmented" role="tablist">
        <button type="button" class="seg-item ${local.tab === 'students' ? 'is-active' : ''}" data-tab="students" role="tab">${icon(
    'users',
    15
  )}<span>按学生排名</span></button>
        <button type="button" class="seg-item ${local.tab === 'subjects' ? 'is-active' : ''}" data-tab="subjects" role="tab">${icon(
    'book',
    15
  )}<span>按科目统计</span></button>
      </div>
      <span class="spacer"></span>
      <span class="toolbar-hint">${
        local.tab === 'students' ? '点击任意学生行可查看其历次考试成绩趋势' : '条形长度表示该分数段人数占比'
      }</span>
    </div>

    ${local.tab === 'students' ? studentsTable(rows, subjects, ctx) : subjectsGrid(state, exam, subjects, all)}

    ${local.openId ? drawerHtml(state, exam, subjects, local.openId) : ''}`;

  bind(container, ctx);
  restoreViewFocus(container);
}

/* ------------------------------------------------------------------ */
/* 按学生排名                                                          */
/* ------------------------------------------------------------------ */

const accessor = (row, key) => {
  if (key.startsWith('sub:')) return row.scores[key.slice(4)];
  if (key === 'sid') return row.student.sid;
  if (key === 'name') return row.student.name;
  return row[key];
};

function studentsTable(rows, subjects, ctx) {
  if (rows.length === 0) {
    return `<div class="panel">${emptyState({
      iconName: 'search',
      title: '没有匹配的学生',
      desc: `没有姓名或学号包含「${ctx.search.trim()}」的学生，清空搜索即可看到完整排名。`,
      actions: [{ act: 'clear-search', label: '清空搜索', iconName: 'refresh' }],
    })}</div>`;
  }

  const sorted = sortRows(rows, accessor, local.sortKey, local.sortDir);
  return `<div class="panel"><div class="table-scroll"><table class="table">
      <thead><tr>
        <th>${sortHeader('排名', 'rank', local.sortKey, local.sortDir)}</th>
        <th>${sortHeader('学号', 'sid', local.sortKey, local.sortDir)}</th>
        <th>${sortHeader('姓名', 'name', local.sortKey, local.sortDir)}</th>
        ${subjects
          .map((s) => `<th class="text-right">${sortHeader(s.name, `sub:${s.id}`, local.sortKey, local.sortDir)}</th>`)
          .join('')}
        <th class="text-right">${sortHeader('总分', 'total', local.sortKey, local.sortDir)}</th>
        <th class="text-right">${sortHeader('平均分', 'average', local.sortKey, local.sortDir)}</th>
        <th class="text-right">${sortHeader('得分率', 'rate', local.sortKey, local.sortDir)}</th>
      </tr></thead>
      <tbody>${sorted
        .map(
          (row) => `<tr data-open="${esc(row.student.id)}" class="is-clickable ${
            local.openId === row.student.id ? 'is-selected' : ''
          }">
          <td>${rankBadge(row)}</td>
          <td class="num-cell">${highlight(row.student.sid || '未填写', ctx.search)}</td>
          <td>${highlight(row.student.name, ctx.search)}</td>
          ${subjects
            .map(
              (s) =>
                `<td class="text-right num-cell ${row.scores[s.id] === null ? 'dim' : ''}">${fmtNum(row.scores[s.id])}</td>`
            )
            .join('')}
          <td class="text-right num-cell">${fmtNum(row.total)}</td>
          <td class="text-right num-cell">${fmtNum(row.average)}</td>
          <td class="text-right num-cell">${fmtPct(row.rate)}</td>
        </tr>`
        )
        .join('')}</tbody>
    </table></div></div>`;
}

/* ------------------------------------------------------------------ */
/* 按科目统计                                                          */
/* ------------------------------------------------------------------ */

function subjectsGrid(state, exam, subjects, rows) {
  return `<div class="subject-grid">${subjects
    .map((subject) => {
      const stats = subjectStats(state, exam.id, subject.id);
      return `<div class="subject-card">
        <div class="subject-card-head">
          <span class="subject-card-name">${esc(subject.name)}</span>
          <span class="badge badge--neutral">满分 ${esc(subject.fullMark)}</span>
          <span class="grow"></span>
          <span class="dim">${stats.count} / ${rows.length} 人已录</span>
        </div>
        <div class="stat-grid">
          <div class="stat-cell"><span class="stat-label">平均分</span><span class="stat-value">${fmtNum(stats.average)}</span></div>
          <div class="stat-cell"><span class="stat-label">最高分</span><span class="stat-value">${fmtNum(stats.max)}</span></div>
          <div class="stat-cell"><span class="stat-label">最低分</span><span class="stat-value">${fmtNum(stats.min)}</span></div>
          <div class="stat-cell"><span class="stat-label">及格率</span><span class="stat-value ${
            stats.passRate >= 60 ? 'is-pass' : 'is-low'
          }">${fmtPct(stats.passRate)}</span></div>
          <div class="stat-cell"><span class="stat-label">优秀率</span><span class="stat-value">${fmtPct(
            stats.excellentRate
          )}</span></div>
        </div>
        ${distBars(stats.distribution)}
      </div>`;
    })
    .join('')}</div>`;
}

/* ------------------------------------------------------------------ */
/* 交互绑定                                                            */
/* ------------------------------------------------------------------ */

function bind(container, ctx) {
  const searchInput = container.querySelector('[data-view-search]');
  if (searchInput) {
    let timer = null;
    searchInput.addEventListener('input', () => {
      clearTimeout(timer);
      const value = searchInput.value;
      timer = setTimeout(() => ctx.setSearch(value), 180);
    });
  }

  container.querySelectorAll('[data-tab]').forEach((node) => {
    node.addEventListener('click', () => {
      local.tab = node.dataset.tab;
      local.openId = null;
      ctx.refresh();
    });
  });

  container.querySelectorAll('[data-sort]').forEach((node) => {
    node.addEventListener('click', () => {
      const key = node.dataset.sort;
      if (local.sortKey === key) local.sortDir = local.sortDir === 'asc' ? 'desc' : 'asc';
      else {
        local.sortKey = key;
        local.sortDir = key === 'name' || key === 'sid' ? 'asc' : 'desc';
      }
      ctx.refresh();
    });
  });

  container.querySelectorAll('[data-open]').forEach((row) => {
    row.addEventListener('click', () => {
      local.openId = local.openId === row.dataset.open ? null : row.dataset.open;
      ctx.refresh();
    });
  });

  const closeButton = container.querySelector('[data-drawer="close"]');
  if (closeButton) {
    closeButton.addEventListener('click', () => {
      local.openId = null;
      ctx.refresh();
    });
  }

  container.querySelectorAll('[data-empty-act]').forEach((node) => {
    node.addEventListener('click', () => ctx.setSearch(''));
  });
}

function restoreViewFocus(container) {
  const node = document.activeElement;
  if (!node || !node.matches || !node.matches('[data-view-search]')) return;
  const caret = node.selectionStart;
  const next = container.querySelector('[data-view-search]');
  if (!next || next === node) return;
  next.focus();
  try {
    next.setSelectionRange(caret, caret);
  } catch {
    /* 忽略不支持选区的输入类型 */
  }
}
