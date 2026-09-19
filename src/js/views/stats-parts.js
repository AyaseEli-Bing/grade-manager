/**
 * 统计视图的纯渲染片段：KPI 卡片、名次徽章、学生详情抽屉、历次考试列表。
 * 只负责把已算好的数据变成 HTML，不含交互绑定。
 */

import { icon } from '../icons.js';
import { esc, fmtNum, fmtPct } from '../ui.js';
import { buildRanking, round2, studentTrend } from '../calc.js';
import { lineChart } from '../charts.js';

export function panelOnly(title, body) {
  return `<div class="view-head"><h1 class="view-title">${esc(title)}</h1></div><div class="panel">${body}</div>`;
}

export function kpiHtml({ iconName, tone, label, value, unit, sub }) {
  return `<div class="kpi-card">
      <div class="kpi-icon ${tone ? `kpi-icon--${tone}` : ''}">${icon(iconName, 16)}</div>
      <div class="kpi-body">
        <span class="kpi-label">${esc(label)}</span>
        <span class="kpi-value">${esc(value)}<span class="kpi-unit">${esc(unit)}</span></span>
        <span class="kpi-sub">${esc(sub)}</span>
      </div>
    </div>`;
}

export function rankBadge(row) {
  if (row.counted === 0) return '<span class="rank rank--plain">—</span>';
  const cls = row.rank <= 3 ? `rank--${row.rank}` : 'rank--plain';
  return `<span class="rank ${cls}" title="总分 ${fmtNum(row.total)}">${row.rank}</span>`;
}

export function drawerHtml(state, exam, subjects, studentId) {
  const student = state.students.find((s) => s.id === studentId);
  if (!student) return '';

  const trend = studentTrend(state, studentId).filter((t) => t.total !== null);
  const rows = buildRanking(state, exam.id);
  const me = rows.find((r) => r.student.id === studentId);

  return `<aside class="drawer" role="complementary" aria-label="学生成绩详情">
      <div class="drawer-head">
        <span class="drawer-title">${esc(student.name)}${student.sid ? ` · ${esc(student.sid)}` : ''}</span>
        <button type="button" class="btn btn--ghost btn--sm btn--icon-only" data-drawer="close" aria-label="关闭详情">${icon('x', 16)}</button>
      </div>
      <div class="drawer-body">
        <div class="drawer-section-title">${icon('file-text', 15)}<span>本次考试各科成绩</span></div>
        ${me ? scoreBlock(me, subjects, rows.length) : '<p class="dim">该生在本场考试中尚无成绩记录。</p>'}

        <div class="drawer-section-title">${icon('trending', 15)}<span>总分趋势</span></div>
        ${lineChart(trend.map((t) => ({ label: t.exam, value: t.total })), { width: 340, height: 160 })}

        <div class="drawer-section-title">${icon('calendar', 15)}<span>历次考试记录</span></div>
        ${trendListHtml(trend)}
      </div>
    </aside>`;
}

function scoreBlock(me, subjects, classSize) {
  return `<div class="stat-grid">
      ${subjects
        .map((s) => `<div class="stat-cell"><span class="stat-label">${esc(s.name)}</span><span class="stat-value">${fmtNum(me.scores[s.id])}</span></div>`)
        .join('')}
    </div>
    <div class="row row-wrap">
      <span class="badge badge--accent">总分 ${fmtNum(me.total)}</span>
      <span class="badge badge--neutral">平均分 ${fmtNum(me.average)}</span>
      <span class="badge badge--neutral">得分率 ${fmtPct(me.rate)}</span>
      <span class="badge ${me.rank <= 3 ? 'badge--warn' : 'badge--neutral'}">班级第 ${me.rank} 名 / ${classSize} 人</span>
    </div>`;
}

function trendListHtml(trend) {
  if (trend.length === 0) return '<p class="dim">暂无历史考试记录，录入多场考试后即可看到变化趋势。</p>';
  const ordered = trend.slice().reverse();
  return `<div class="trend-list">${ordered
    .map((item, index) => {
      const previous = ordered[index + 1];
      const delta = previous ? round2(item.total - previous.total) : null;
      const tone = delta === null || delta === 0 ? 'is-flat' : delta > 0 ? 'is-up' : 'is-down';
      const arrow = delta === null || delta === 0 ? null : delta > 0 ? 'arrow-up' : 'arrow-down';
      return `<div class="trend-row">
        <span class="trend-exam" title="${esc(item.exam)}">${esc(item.exam)}${
        item.date ? `<span class="th-sub"> · ${esc(item.date)}</span>` : ''
      }</span>
        <span class="num">${fmtNum(item.total)}</span>
        <span class="dim">第 ${item.rank} 名</span>
        <span class="trend-delta ${tone}">${arrow ? icon(arrow, 12) : ''}${
        delta === null ? '首次' : `${delta > 0 ? '+' : ''}${delta}`
      }</span>
      </div>`;
    })
    .join('')}</div>`;
}
