/**
 * 界面端到端验收（jsdom 真实 DOM，非静态检查）。
 * 运行：
 *   NODE_PATH=/Users/bing1111/.workbuddy/binaries/node/workspace/node_modules \
 *   /Users/bing1111/.workbuddy/binaries/node/versions/22.22.2-3/bin/node scripts/test-ui.mjs
 *
 * 覆盖：应用启动、四个视图渲染、导航、排名与 KPI、详情抽屉、成绩录入校验与落盘、考试切换、搜索。
 */

import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import { createRequire } from 'node:module';
import { fileURLToPath } from 'node:url';

const require = createRequire(import.meta.url);
const { JSDOM } = require('jsdom');

const ROOT = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const LS_KEY = 'grade-manager-state';

const fixture = () => ({
  version: 1,
  className: '高二(3)班',
  subjects: [
    { id: 's_zh', name: '语文', fullMark: 150 },
    { id: 's_ma', name: '数学', fullMark: 100 },
  ],
  students: [
    { id: 'st_a', sid: '001', name: '陈一鸣', gender: '男', note: '班长' },
    { id: 'st_b', sid: '002', name: '李思远', gender: '男', note: '' },
    { id: 'st_c', sid: '003', name: '王雨桐', gender: '女', note: '课代表' },
  ],
  exams: [
    { id: 'e1', name: '第一次月考', date: '2026-03-10', subjectIds: [] },
    { id: 'e2', name: '期中考试', date: '2026-04-22', subjectIds: [] },
  ],
  scores: {
    e1: { st_a: { s_zh: 120, s_ma: 90 }, st_b: { s_zh: 120, s_ma: 90 }, st_c: { s_zh: 100 } },
    e2: { st_a: { s_zh: 132, s_ma: 96 }, st_b: { s_zh: 118, s_ma: 88 }, st_c: { s_zh: 110, s_ma: 72 } },
  },
  activeExamId: 'e1',
  updatedAt: '2026-03-11T00:00:00.000Z',
});

const tick = (ms = 20) => new Promise((r) => setTimeout(r, ms));
const $ = (sel) => dom.window.document.querySelector(sel);
const $$ = (sel) => Array.from(dom.window.document.querySelectorAll(sel));
const text = (sel) => ($(sel) ? $(sel).textContent.trim() : null);
const click = (node) => node.dispatchEvent(new dom.window.MouseEvent('click', { bubbles: true, cancelable: true }));
const clickSel = (sel) => {
  const node = $(sel);
  assert.ok(node, `找不到可点击元素 ${sel}`);
  click(node);
};
const savedState = () => JSON.parse(dom.window.localStorage.getItem(LS_KEY));
const fire = (node, type) => node.dispatchEvent(new dom.window.Event(type, { bubbles: false }));

let dom;
const cases = [];
const test = (name, fn) => cases.push([name, fn]);

/* ------------------------------------------------------------------ */
/* 启动                                                                */
/* ------------------------------------------------------------------ */

dom = new JSDOM(fs.readFileSync(path.join(ROOT, 'src/index.html'), 'utf8'), {
  url: 'http://localhost:8123/',
  pretendToBeVisual: true,
});
dom.window.localStorage.setItem(LS_KEY, JSON.stringify(fixture()));

global.window = dom.window;
global.document = dom.window.document;
global.localStorage = dom.window.localStorage;
global.HTMLElement = dom.window.HTMLElement;
global.Node = dom.window.Node;
global.Event = dom.window.Event;
global.MouseEvent = dom.window.MouseEvent;
global.requestAnimationFrame = dom.window.requestAnimationFrame.bind(dom.window);
global.getComputedStyle = dom.window.getComputedStyle.bind(dom.window);

const errors = [];
dom.window.addEventListener('error', (e) => errors.push(String(e.message)));

await import('../src/js/app.js');
await tick(60);

/* ------------------------------------------------------------------ */
/* 用例                                                                */
/* ------------------------------------------------------------------ */

test('启动后侧边栏渲染 4 个视图且默认停留在学生管理', () => {
  const items = $$('.nav-item').map((n) => n.textContent.replace(/\s+/g, ' ').trim());
  assert.equal(items.length, 4, '应有 4 个导航项');
  assert.ok(items[0].includes('学生管理'), `首个导航项应为学生管理，实际 ${items[0]}`);
  assert.ok(items.some((t) => t.includes('成绩录入')));
  assert.ok(items.some((t) => t.includes('统计排名')));
  assert.ok(items.some((t) => t.includes('科目管理')));
  assert.ok($('.nav-item.is-active').textContent.includes('学生管理'), '学生管理应为激活态');
});

test('顶栏展示班级名与当前考试名，且图标全部是内联 SVG', () => {
  assert.equal(text('#class-name'), '高二(3)班');
  assert.ok($('.exam-trigger').textContent.includes('第一次月考'), '应显示当前考试名');
  assert.ok($$('.topbar svg.icon').length >= 8, '顶栏应渲染多个 SVG 图标');
  assert.equal($$('img').length, 0, '不应出现位图图标');
});

test('学生管理表格渲染 3 名学生', () => {
  const rows = $$('tbody tr');
  assert.equal(rows.length, 3);
  assert.ok(text('tbody').includes('陈一鸣'));
  assert.ok(text('tbody').includes('王雨桐'));
  assert.ok(text('tbody').includes('班长'), '备注列应渲染');
});

test('侧边栏底部显示数据保存位置', () => {
  const pathText = text('#data-path-text');
  assert.ok(pathText && pathText.length > 0, '应展示数据文件路径或 localStorage 说明');
});

test('学生管理搜索为实时过滤（防抖后生效）', async () => {
  const input = $('[data-view-search]');
  input.value = '李';
  fire(input, 'input');
  await tick(240);
  assert.equal($$('tbody tr').length, 1, '输入「李」后应只剩 1 行');
  assert.ok(text('tbody').includes('李思远'));
  const input2 = $('[data-view-search]');
  input2.value = '';
  fire(input2, 'input');
  await tick(240);
  assert.equal($$('tbody tr').length, 3, '清空搜索后应恢复 3 行');
});

test('切换到统计排名：KPI 卡片数值正确', () => {
  clickSel('.nav-item[data-nav="stats"]');
  assert.equal($$('.kpi-card').length, 4, '应有 4 张 KPI 卡片');
  const values = $$('.kpi-value').map((n) => n.textContent.replace(/\s+/g, '').trim());
  assert.equal(values[0], '3人', `参考人数应为 3 人，实际 ${values[0]}`);
  assert.equal(values[1], '103.33分', `全班平均分应为 103.33 分（(105+105+100)/3），实际 ${values[1]}`);
  assert.equal(values[2], '210分', `最高总分应为 210 分，实际 ${values[2]}`);
  assert.equal(values[3], '100%', `全科及格率应为 100%，实际 ${values[3]}`);
});

test('按学生排名：名次、总分、平均分、得分率渲染正确', () => {
  const rows = $$('tbody tr[data-open]');
  assert.equal(rows.length, 3, '应渲染 3 名学生的排名行');
  const first = rows[0];
  const cells = Array.from(first.querySelectorAll('td')).map((td) => td.textContent.trim());
  assert.equal(cells[0], '1', '首行名次应为 1');
  assert.ok(first.querySelector('.rank--1'), '首名应使用 rank--1 徽章样式');
  assert.ok(cells.includes('210'), '应显示总分 210');
  assert.ok(cells.includes('105'), '应显示平均分 105');
  assert.ok(cells.includes('84%'), '应显示得分率 84%');
  const third = rows[2];
  assert.ok(third.querySelector('.rank--3'), '第三名应使用 rank--3 奖牌样式');
  assert.equal(third.querySelectorAll('.rank').length, 1, '每行应有且仅有一个名次徽章');
});

test('按学生排名：名次徽章使用奖牌 Token 而非 emoji', () => {
  const badge = $('.rank--1');
  assert.ok(badge, '应存在 rank--1 徽章');
  assert.equal(badge.querySelectorAll('svg').length, 0, '名次徽章不应内嵌图标');
  assert.equal(/[\u{1F000}-\u{1FAFF}]/u.test(dom.window.document.body.textContent), false, '页面文本不应含 emoji');
});

test('点击学生行打开详情抽屉，含趋势折线图与历次考试列表', () => {
  click($('tbody tr[data-open]'));
  const drawer = $('.drawer');
  assert.ok(drawer, '应打开详情抽屉');
  assert.ok(drawer.textContent.includes('陈一鸣'));
  assert.ok(drawer.querySelector('.chart-svg'), '应渲染手写 SVG 趋势图');
  assert.equal(drawer.querySelectorAll('.chart-line').length >= 1, true, '应有折线路径');
  assert.equal(drawer.querySelectorAll('.trend-row').length, 2, '应有 2 条历次考试记录');
  assert.ok(drawer.textContent.includes('期中考试'));
  clickSel('[data-drawer="close"]');
  assert.equal($('.drawer'), null, '点击关闭后抽屉应消失');
});

test('按科目统计：切换到科目视图并渲染统计卡与分布条', () => {
  clickSel('.seg-item[data-tab="subjects"]');
  const cards = $$('.subject-card');
  assert.equal(cards.length, 2, '应有 2 张科目统计卡');
  assert.ok(cards[0].textContent.includes('语文'));
  const stats = Array.from(cards[0].querySelectorAll('.stat-value')).map((n) => n.textContent.trim());
  assert.equal(stats.length, 5, '每科应展示 5 项指标');
  assert.equal(stats[0], '113.33', `语文平均分应为 113.33，实际 ${stats[0]}`);
  assert.equal(stats[1], '120', '语文最高分应为 120');
  assert.equal(stats[2], '100', '语文最低分应为 100');
  assert.equal(cards[0].querySelectorAll('.dist-row').length, 5, '应有 5 个分数段');
  assert.ok(cards[0].querySelector('.dist-fill'), '分数段应有条形填充');
  clickSel('.seg-item[data-tab="students"]');
  assert.ok($$('tbody tr[data-open]').length === 3, '切回按学生应恢复排名表');
});

test('成绩录入：单元格数量等于学生数 × 科目数', () => {
  clickSel('.nav-item[data-nav="scores"]');
  assert.equal($$('.score-input').length, 6, '3 名学生 × 2 个科目应为 6 个输入格');
  assert.ok(text('.progress-text').includes('83.3%'), `录入进度应为 83.3%，实际 ${text('.progress-text')}`);
});

test('成绩录入：正常分数写入后总分与平均分实时更新', () => {
  const input = $$('.score-input').find((n) => n.dataset.id === 'st_c' && n.dataset.sid === 's_ma');
  assert.ok(input, '应找到王雨桐的数学输入格');
  input.value = '88';
  fire(input, 'blur');
  const row = input.closest('tr');
  const cells = Array.from(row.querySelectorAll('[data-cell]')).map((n) => n.textContent.trim());
  assert.deepEqual(cells, ['188', '94'], `总分应为 188、平均分 94，实际 ${cells.join('/')}`);
  assert.equal(input.value, '88', '合法分数不应被改写');
  assert.equal(input.classList.contains('is-excellent'), false, '88 分未达 90% 不应标记优秀');
  assert.equal(input.classList.contains('is-fail'), false, '88 分已过 60% 不应标记不及格');
});

test('成绩录入：分数着色阈值按满分比例判定', () => {
  const input = $$('.score-input').find((n) => n.dataset.id === 'st_c' && n.dataset.sid === 's_ma');
  input.value = '95';
  fire(input, 'blur');
  assert.ok(input.classList.contains('is-excellent'), '95 分（≥90% 满分）应标记为优秀');
  input.value = '50';
  fire(input, 'blur');
  assert.ok(input.classList.contains('is-fail'), '50 分（<60% 满分）应标记为不及格');
  input.value = '88';
  fire(input, 'blur');
  assert.equal(input.classList.contains('is-excellent') || input.classList.contains('is-fail'), false, '中间分段不应着色');
});

test('成绩录入：超出满分自动钳制并提示，不写入非法值', () => {
  const input = $$('.score-input').find((n) => n.dataset.id === 'st_b' && n.dataset.sid === 's_ma');
  input.value = '999';
  fire(input, 'blur');
  assert.equal(input.value, '100', '超出满分的输入应被钳制为满分 100');
  assert.ok(input.classList.contains('is-invalid'), '应标记为非法输入');
  assert.ok($('.toast--error'), '应弹出错误提示');
  assert.ok($('.toast--error').textContent.includes('超出'), '提示文案应说明超出范围');
});

test('成绩录入：清空单元格视为缺考，总分不按 0 分计入', () => {
  const input = $$('.score-input').find((n) => n.dataset.id === 'st_c' && n.dataset.sid === 's_ma');
  input.value = '';
  fire(input, 'blur');
  const cells = Array.from(input.closest('tr').querySelectorAll('[data-cell]')).map((n) => n.textContent.trim());
  assert.deepEqual(cells, ['100', '100'], `缺考后总分应为 100、平均分 100，实际 ${cells.join('/')}`);
});

test('编辑结果自动落盘（防抖 400ms 后写入）', async () => {
  await tick(520);
  const stored = savedState();
  assert.equal(stored.scores.e1.st_c.s_ma, undefined, '被清空的数学成绩不应留在存档里');
  assert.equal(stored.scores.e1.st_b.s_ma, 100, '被钳制的成绩应以 100 落盘');
});

test('科目管理视图渲染 2 个科目及其满分', () => {
  clickSel('.nav-item[data-nav="subjects"]');
  const rows = $$('tbody tr');
  assert.equal(rows.length, 2);
  assert.ok(text('tbody').includes('语文'));
  assert.ok(text('tbody').includes('150'), '应显示语文满分 150');
});

test('考试切换：下拉列出全部考试并可切换当前考试', async () => {
  clickSel('[data-exam-toggle]');
  const items = $$('.popover-item[data-exam]');
  assert.equal(items.length, 2, '下拉应列出 2 场考试');
  assert.ok($('.popover-item.is-active'), '当前考试应有选中态');
  assert.ok($('.popover-item[data-exam="e1"] .item-sub').textContent.includes('条成绩'), '下拉应显示每场考试的成绩条数');
  click(items.find((n) => n.dataset.exam === 'e2'));
  assert.ok($('.exam-trigger').textContent.includes('期中考试'), '切换后顶栏应显示期中考试');
  assert.equal($('.popover'), null, '选择后下拉应关闭');
  await tick(520);
  assert.equal(savedState().activeExamId, 'e2', '切换结果应落盘');
});

test('切换考试后统计视图按新考试重新计算', () => {
  clickSel('.nav-item[data-nav="stats"]');
  assert.equal($$('.kpi-card').length, 4, '仍应正常渲染 4 张 KPI');
  const values = $$('.kpi-value').map((n) => n.textContent.replace(/\s+/g, '').trim());
  assert.equal(values[0], '3人', `期中考试参考人数应为 3 人，实际 ${values[0]}`);
  assert.equal(values[2], '228分', `期中考试最高总分应为 228 分，实际 ${values[2]}`);
  assert.equal(values[3], '100%', `期中考试及格率应为 100%，实际 ${values[3]}`);
});

test('导出 CSV：内容含 BOM、表头与逐行成绩，字段与界面一致', async () => {
  let captured = null;
  class BlobSpy {
    constructor(parts, options) {
      captured = parts.join('');
      this.type = options && options.type;
    }
  }
  const originBlob = global.Blob;
  global.Blob = BlobSpy;
  dom.window.URL.createObjectURL = () => 'blob:stub';
  dom.window.URL.revokeObjectURL = () => {};
  const originURL = global.URL;
  global.URL = dom.window.URL;

  try {
    const { exportCsv } = await import('../src/js/io.js');
    const { loadState } = await import('../src/js/store.js');
    const current = await loadState(); // 此时当前考试已切换为「期中考试」
    assert.equal(await exportCsv(current, 'current'), true);

    assert.ok(captured, '应生成导出内容');
    assert.equal(captured.charCodeAt(0), 0xfeff, 'CSV 应以 BOM 开头以兼容 Excel 中文');
    const lines = captured.slice(1).split('\n').filter(Boolean);
    assert.equal(lines[0], '学号,姓名,语文,数学,总分,平均分,得分率,排名');
    assert.equal(lines[1], '001,陈一鸣,132,96,228,114,91.2%,1', `首行数据不符：${lines[1]}`);
    assert.equal(lines[2], '002,李思远,118,88,206,103,82.4%,2', `次行数据不符：${lines[2]}`);
    assert.equal(lines[3], '003,王雨桐,110,72,182,91,72.8%,3', `第三行数据不符：${lines[3]}`);
  } finally {
    global.Blob = originBlob;
    global.URL = originURL;
  }
});

test('运行过程中没有未捕获的脚本错误', () => {
  assert.deepEqual(errors, [], `出现脚本错误：${errors.join(' | ')}`);
});

/* ------------------------------------------------------------------ */
/* 执行                                                                */
/* ------------------------------------------------------------------ */

let passed = 0;
let failed = 0;
for (const [name, fn] of cases) {
  try {
    await fn();
    passed++;
    console.log(`  PASS  ${name}`);
  } catch (err) {
    failed++;
    console.log(`  FAIL  ${name}\n        ${err.message.split('\n')[0]}`);
  }
}
console.log(`\n${passed} 通过 / ${failed} 失败 / 共 ${cases.length} 项`);
process.exit(failed === 0 ? 0 : 1);
