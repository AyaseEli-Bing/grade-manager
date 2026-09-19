/**
 * 手写 SVG 图表。项目禁止引入任何图表库，全部使用该 leaders。
 * 颜色一律通过 CSS class 引用 design token，不在 SVG 里写色值。
 */

const escapeNum = (n) => String(Math.round(n * 100) / 100);

/**
 * 折线图：学生多次考试的总分趋势。
 * @param {{label:string, sub?:string, value:number|null}[]} points
 */
export function lineChart(points, { width = 344, height = 168, unit = '' } = {}) {
  const usable = points.filter((p) => p.value !== null && Number.isFinite(Number(p.value)));
  if (usable.length === 0) {
    return '<p class="dim">该学生在各次考试中尚无成绩记录，无法绘制趋势。</p>';
  }

  const pad = { top: 20, right: 18, bottom: 26, left: 38 };
  const plotW = width - pad.left - pad.right;
  const plotH = height - pad.top - pad.bottom;

  const values = usable.map((p) => Number(p.value));
  let min = Math.min(...values);
  let max = Math.max(...values);
  if (min === max) {
    min = Math.min(min, min - 10);
    max = max + 10;
  } else {
    const headroom = (max - min) * 0.12;
    min -= headroom;
    max += headroom;
  }

  const plotX = (i) => {
    if (points.length === 1) return pad.left + plotW / 2;
    return pad.left + (plotW * i) / (points.length - 1);
  };
  const plotY = (v) => pad.top + plotH - ((Number(v) - min) / (max - min)) * plotH;

  const gridRows = [0, 0.5, 1];
  const grid = gridRows
    .map((r) => {
      const y = pad.top + plotH * r;
      const v = max - (max - min) * r;
      return `<line class="chart-grid" x1="${pad.left}" y1="${y}" x2="${pad.left + plotW}" y2="${y}" />
        <text class="chart-axis-text" x="${pad.left - 6}" y="${y + 3}" text-anchor="end">${escapeNum(v)}</text>`;
    })
    .join('');

  const segments = [];
  let current = [];
  points.forEach((p, i) => {
    if (p.value === null || !Number.isFinite(Number(p.value))) {
      if (current.length) segments.push(current);
      current = [];
      return;
    }
    current.push([plotX(i), plotY(p.value)]);
  });
  if (current.length) segments.push(current);

  const paths = segments
    .map((seg) => {
      if (seg.length === 1) {
        const [x, y] = seg[0];
        return `<line class="chart-line" x1="${x}" y1="${y}" x2="${x}" y2="${y}" stroke-linecap="round" />`;
      }
      return `<path class="chart-line" d="${seg.map((p, i) => `${i === 0 ? 'M' : 'L'}${p[0]} ${p[1]}`).join(' ')}" />`;
    })
    .join('');

  const dots = points
    .map((p, i) => {
      if (p.value === null || !Number.isFinite(Number(p.value))) return '';
      const x = plotX(i);
      const y = plotY(p.value);
      const last = i === points.length - 1;
      return `<circle class="chart-dot ${last ? 'chart-dot-last' : ''}" cx="${x}" cy="${y}" r="${last ? 4 : 3}" />
        <text class="chart-dot-text" x="${x}" y="${y - 8}" text-anchor="middle">${escapeNum(p.value)}${unit}</text>`;
    })
    .join('');

  const labels = points
    .map((p, i) => {
      const x = plotX(i);
      const step = points.length > 6 ? Math.ceil(points.length / 6) : 1;
      if (i % step !== 0 && i !== points.length - 1) return '';
      const text = p.label.length > 5 ? `${p.label.slice(0, 5)}…` : p.label;
      return `<text class="chart-axis-text" x="${x}" y="${height - 8}" text-anchor="middle">${text}</text>`;
    })
    .join('');

  return `<div class="chart-frame"><svg class="chart-svg" viewBox="0 0 ${width} ${height}" role="img" aria-label="总分趋势折线图">
      ${grid}${paths}${dots}${labels}
    </svg></div>`;
}

/**
 * 横向分数段条形图（纯 DIV + 宽度百分比，颜色走 --dist-* token）。
 * @param {{label:string,count:number,ratio:number}[]} bins
 */
export function distBars(bins) {
  if (!bins || bins.length === 0) return '';
  return `<div class="dist">${bins
    .map((bin, i) => {
      const pct = Math.round((bin.ratio || 0) * 1000) / 10;
      const tone = i === 0 ? 'fail' : `b${i}`;
      return `<div class="dist-row">
        <span class="dist-label">${bin.label}</span>
        <span class="dist-track"><span class="dist-fill dist-fill--${tone}" style="width:${Math.max(pct, bin.count > 0 ? 3 : 0)}%"></span></span>
        <span class="dist-meta">${bin.count} 人 · ${pct}%</span>
      </div>`;
    })
    .join('')}</div>`;
}
