/**
 * 逻辑层验收脚本（不依赖浏览器）。
 * 运行：/Users/bing1111/.workbuddy/binaries/node/versions/22.22.2-3/bin/node scripts/test-logic.mjs
 * 覆盖：排名/总分/平均分、单科统计、录入进度、及格率、趋势、数据规范化、CSV 解析。
 */

import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import {
  buildRanking,
  classPassRate,
  entryProgress,
  examSubjects,
  round2,
  studentTrend,
  subjectStats,
} from '../src/js/calc.js';
import { createDefaultState, normalizeState } from '../src/js/store.js';
import { parseCsvIntoStudents } from '../src/js/io.js';
import { mergeState } from '../src/js/transfer.js';

let passed = 0;
const cases = [];
const ROOT = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
function test(name, fn) {
  cases.push([name, fn]);
}

/* ---------------- 公共测试数据 ---------------- */

function fixture() {
  return {
    version: 1,
    className: '高二(3)班',
    subjects: [
      { id: 's_zh', name: '语文', fullMark: 150 },
      { id: 's_ma', name: '数学', fullMark: 100 },
    ],
    students: [
      { id: 'st_a', sid: '001', name: '陈一鸣', gender: '男', note: '' },
      { id: 'st_b', sid: '002', name: '李思远', gender: '男', note: '' },
      { id: 'st_c', sid: '003', name: '王雨桐', gender: '女', note: '' },
    ],
    exams: [
      { id: 'e1', name: '第一次月考', date: '2026-03-10', subjectIds: [] },
      { id: 'e2', name: '期中考试', date: '2026-04-22', subjectIds: [] },
    ],
    scores: {
      e1: {
        st_a: { s_zh: 120, s_ma: 90 },
        st_b: { s_zh: 120, s_ma: 90 },
        st_c: { s_zh: 100 },
      },
      e2: {
        st_a: { s_zh: 132, s_ma: 96 },
        st_b: { s_zh: 118, s_ma: 88 },
        st_c: { s_zh: 110, s_ma: 72 },
      },
    },
    activeExamId: 'e1',
    updatedAt: '2026-03-11T00:00:00.000Z',
  };
}

/* ---------------- 排名与总分 ---------------- */

test('总分只累计已录科目，缺考科目单独计数', () => {
  const state = fixture();
  const rows = buildRanking(state, 'e1');
  const c = rows.find((r) => r.student.id === 'st_c');
  assert.equal(c.total, 100, '缺考数学不应按 0 分计入总分');
  assert.equal(c.counted, 1, '已录科目数应为 1');
  assert.equal(c.missing, 1, '缺考科目数应为 1');
  assert.equal(c.average, 100, '平均分应只除以已录科目数');
});

test('同分同名次，后续名次跳号（1,1,3）', () => {
  const rows = buildRanking(fixture(), 'e1');
  const ranks = rows.map((r) => r.rank);
  assert.deepEqual(ranks.slice().sort(), [1, 1, 3]);
  assert.equal(rows[0].rank, 1);
  assert.equal(rows[1].rank, 1);
  assert.equal(rows[2].rank, 3);
});

test('得分率以参考科目满分合计为分母', () => {
  const rows = buildRanking(fixture(), 'e1');
  const a = rows.find((r) => r.student.id === 'st_a');
  assert.equal(a.rate, round2((210 / 250) * 100));
});

test('未录任何成绩的学生总分与平均分为 0 且不占名次', () => {
  const state = fixture();
  state.students.push({ id: 'st_d', sid: '004', name: '赵无成绩', gender: '', note: '' });
  const rows = buildRanking(state, 'e1');
  const d = rows.find((r) => r.student.id === 'st_d');
  assert.equal(d.total, 0);
  assert.equal(d.counted, 0);
  assert.equal(d.missing, 2);
  assert.equal(d.rank, 4, '应排在最后一位');
});

/* ---------------- 单科统计 ---------------- */

test('单科统计：平均/最高/最低/及格率/优秀率', () => {
  const stats = subjectStats(fixture(), 'e1', 's_ma');
  assert.equal(stats.count, 2);
  assert.equal(stats.average, 90);
  assert.equal(stats.max, 90);
  assert.equal(stats.min, 90);
  assert.equal(stats.passRate, 100);
  assert.equal(stats.excellentRate, 100);
});

test('分数段分布覆盖全部有效分数且人数守恒', () => {
  const stats = subjectStats(fixture(), 'e2', 's_zh');
  const total = stats.distribution.reduce((sum, bin) => sum + bin.count, 0);
  assert.equal(total, stats.count, '各分数段人数之和应等于有效分数个数');
  assert.equal(stats.distribution.length, 5);
  const ratios = round2(stats.distribution.reduce((sum, bin) => sum + bin.ratio, 0) * 100);
  assert.equal(ratios, 100, '各分数段占比之和应为 100%');
});

test('无人录分时单科统计返回零值而非 NaN', () => {
  const state = fixture();
  state.subjects.push({ id: 's_bi', name: '生物', fullMark: 100 });
  const stats = subjectStats(state, 'e1', 's_bi');
  assert.equal(stats.count, 0);
  assert.equal(stats.average, 0);
  assert.equal(stats.passRate, 0);
  assert.deepEqual(stats.distribution, []);
});

/* ---------------- 进度与及格率 ---------------- */

test('录入进度按学生数 × 科目数计算', () => {
  const state = fixture();
  const subjects = examSubjects(state, state.exams[0]);
  const progress = entryProgress(state, 'e1', subjects);
  assert.equal(progress.total, 6);
  assert.equal(progress.done, 5);
  assert.equal(progress.percent, 83.3);
});

test('全班及格率按已录分数计算', () => {
  const state = fixture();
  const subjects = examSubjects(state, state.exams[0]);
  const rate = classPassRate(subjects, buildRanking(state, 'e1'));
  assert.equal(round2(rate), 100);
});

test('及格率在低分场景下正确下降', () => {
  const state = fixture();
  state.scores.e1.st_a.s_zh = 40;
  state.scores.e1.st_a.s_ma = 30;
  const subjects = examSubjects(state, state.exams[0]);
  const rate = round2(classPassRate(subjects, buildRanking(state, 'e1')));
  assert.equal(rate, round2((3 / 5) * 100), '5 个已录分数中应有 3 个及格');
});

/* ---------------- 趋势 ---------------- */

test('趋势按考试日期升序返回，且不遗漏无成绩的考试', () => {
  const trend = studentTrend(fixture(), 'st_a');
  assert.equal(trend.length, 2);
  assert.deepEqual(trend.map((t) => t.exam), ['第一次月考', '期中考试']);
  assert.equal(trend[0].total, 210);
  assert.equal(trend[1].total, 228);
  assert.equal(trend[1].rank, 1);
});

/* ---------------- 数据规范化 ---------------- */

test('规范化剔除悬空学生、未知科目与非法考试引用', () => {
  const dirty = {
    className: '',
    subjects: [{ id: 's1', name: '语文', fullMark: 150 }, { id: 's2', name: '', fullMark: 100 }],
    students: [{ id: 'st1', name: '张三' }, { name: '' }],
    exams: [{ id: 'e1', name: '月考', subjectIds: ['s1', 'ghost'] }],
    scores: { e1: { st1: { s1: 100, ghost: 50 }, ghost: { s1: 10 } }, ghostExam: { st1: { s1: 1 } } },
    activeExamId: 'not-exist',
  };
  const clean = normalizeState(dirty);
  assert.equal(clean.className, createDefaultState().className, '空班名应回退默认值');
  assert.equal(clean.subjects.length, 1, '空名科目应被剔除');
  assert.equal(clean.students.length, 1, '无姓名学生应被剔除');
  assert.deepEqual(clean.exams[0].subjectIds, ['s1'], '未知科目引用应被剔除');
  assert.deepEqual(Object.keys(clean.scores), ['e1'], '未知考试的整表应被剔除');
  assert.deepEqual(Object.keys(clean.scores.e1), ['st1'], '未知学生的行应被剔除');
  assert.deepEqual(Object.keys(clean.scores.e1.st1), ['s1'], '未知科目的格子应被剔除');
  assert.equal(clean.activeExamId, 'e1', '非法 activeExamId 应回退到首场考试');
});

test('规范化可接受 null / 字符串等异常输入', () => {
  assert.equal(normalizeState(null).students.length, 0);
  assert.equal(normalizeState('oops').subjects.length, createDefaultState().subjects.length);
  assert.equal(normalizeState(42).exams.length, 1);
});

test('规范化结果可安全序列化并再次读回（幂等）', () => {
  const once = normalizeState(fixture());
  const twice = normalizeState(JSON.parse(JSON.stringify(once)));
  assert.deepEqual(twice, once);
});

/* ---------------- CSV 解析 ---------------- */

test('CSV 解析：中文表头、引号内逗号、缺失单元格、未知科目列', () => {
  const csv = [
    '学号,姓名,性别,备注,语文,数学,地理',
    '001,陈一鸣,男,"走读, 需乘车",120,90,88',
    '002,"李,思远",男,,118,88,',
    '003,王雨桐,女,课代表,110,,70',
    ',,空行应被跳过,,,,',
  ].join('\n');
  const result = parseCsvIntoStudents(fixture(), csv);
  assert.equal(result.ok, true);
  assert.equal(result.students.length, 3, '无姓名的行应被跳过');
  assert.equal(result.students[0].note, '走读, 需乘车', '引号内的逗号不应拆分单元格');
  assert.equal(result.students[1].name, '李,思远', '引号内的姓名应完整保留');
  assert.equal(result.hasScores, true);
  assert.equal(result.scores[result.students[0].id].s_zh, 120);
  assert.equal(result.scores[result.students[2].id].s_ma, null, '空单元格应记为缺考');
  assert.equal(
    Object.keys(result.scores[result.students[0].id]).length,
    2,
    'CSV 中不存在的科目（地理）不应写入'
  );
});

test('CSV 解析：缺姓名列直接拒绝并给出原因', () => {
  const result = parseCsvIntoStudents(fixture(), '学号,分数\n001,90');
  assert.equal(result.ok, false);
  assert.match(result.error, /姓名/);
});

test('CSV 解析：只有名单没有成绩列时 hasScores 为 false', () => {
  const result = parseCsvIntoStudents(fixture(), '学号,姓名\n001,陈一鸣\n002,李思远');
  assert.equal(result.ok, true);
  assert.equal(result.students.length, 2);
  assert.equal(result.hasScores, false);
});

/* ---------------- 数据合并（导入 JSON 的去重规则） ---------------- */

test('合并导入：科目按名称去重、学生按学号+姓名去重、考试按名称去重', () => {
  const base = fixture();
  const incoming = {
    version: 1,
    className: '高三(2)班',
    subjects: [
      { id: 'x_zh', name: '语文', fullMark: 150 }, // 同名 → 复用现有
      { id: 'x_bi', name: '生物', fullMark: 100 }, // 新增
    ],
    students: [
      { id: 'x_a', sid: '001', name: '陈一鸣', gender: '男', note: '' }, // 学号+姓名相同 → 复用
      { id: 'x_e', sid: '009', name: '周新来', gender: '女', note: '转学生' }, // 新增
    ],
    exams: [
      { id: 'x_e1', name: '第一次月考', date: '2026-03-10', subjectIds: ['x_zh', 'x_bi'] }, // 同名 → 复用
      { id: 'x_e3', name: '期末考试', date: '2026-06-28', subjectIds: ['x_bi'] }, // 新增
    ],
    scores: {
      x_e1: { x_a: { x_zh: 140, x_bi: 92 }, x_e: { x_zh: 100 } },
      x_e3: { x_e: { x_bi: 88 } },
    },
    activeExamId: 'x_e1',
  };

  const { state, report } = mergeState(base, incoming);

  assert.equal(state.subjects.length, 3, '科目应由 2 个增至 3 个（生物）');
  assert.equal(report.subjects, 1);
  assert.equal(state.students.length, 4, '学生应由 3 名增至 4 名（周新来）');
  assert.equal(report.students, 1);
  assert.equal(state.exams.length, 3, '考试应由 2 场增至 3 场（期末考试）');
  assert.equal(report.exams, 1);

  // 同名考试的既有成绩被覆盖，同一学生不存在两条记录
  const e1 = state.scores.e1;
  assert.equal(Object.keys(e1).length, 4, 'e1 下应有 4 名学生（原 3 名 + 新 1 名）');
  assert.equal(e1.st_a.s_zh, 140, '同名考试的成绩应按 学生+科目 覆盖为 140');
  assert.equal(e1.st_a.s_ma, 90, '未在导入文件中出现的科目成绩应保留');

  // 新考试引用被正确映射到既有科目 id
  const biologyId = state.subjects.find((s) => s.name === '生物').id;
  const newExam = state.exams.find((e) => e.name === '期末考试');
  assert.deepEqual(newExam.subjectIds, [biologyId], '新考试的科目引用应映射到合并后的科目 id');
  const newStudent = state.students.find((s) => s.name === '周新来');
  assert.equal(state.scores[newExam.id][newStudent.id][biologyId], 88, '新考试的新生成绩应写入');
  assert.equal(newStudent.note, '转学生', '新增学生的备注应完整保留');
});

test('合并导入不会因同名而重复创建学号相同但姓名不同的学生', () => {
  const base = fixture();
  const incoming = {
    subjects: [],
    students: [{ id: 'x', sid: '001', name: '陈一鸣（转学）', gender: '', note: '' }],
    exams: [],
    scores: {},
  };
  const { state } = mergeState(base, incoming);
  assert.equal(state.students.length, 4, '学号相同但姓名不同应视为两名学生');
});

test('合并导入：空科目满分兜底为 100，考试日期缺失时补当天', () => {
  const base = fixture();
  const incoming = {
    subjects: [{ id: 'x_g', name: '地理', fullMark: 0 }],
    students: [],
    exams: [{ id: 'x_e', name: '摸底考', date: '', subjectIds: [] }],
    scores: {},
  };
  const { state } = mergeState(base, incoming);
  const subject = state.subjects.find((s) => s.name === '地理');
  assert.equal(subject.fullMark, 100, '非正满分应兜底为 100');
  assert.match(state.exams.find((e) => e.name === '摸底考').date, /^\d{4}-\d{2}-\d{2}$/, '缺失日期应补当天');
});

/* ---------------- 随包发布的示例数据 ---------------- */

test('示例数据可被正确规范化并算出完整排名', () => {
  const file = path.join(ROOT, 'examples/示例数据.json');
  assert.ok(fs.existsSync(file), '示例数据文件应存在');
  const state = normalizeState(JSON.parse(fs.readFileSync(file, 'utf8')));

  assert.equal(state.students.length, 16, '示例数据应有 16 名学生');
  assert.equal(state.exams.length, 3, '示例数据应有 3 场考试');
  assert.equal(state.subjects.length, 6, '示例数据应有 6 个科目');
  assert.equal(state.activeExamId, 'e_2');

  for (const exam of state.exams) {
    const rows = buildRanking(state, exam.id);
    assert.equal(rows.length, 16, `「${exam.name}」应排出 16 行`);
    assert.equal(rows[0].rank, 1, `「${exam.name}」第一名名次应为 1`);
    for (let i = 1; i < rows.length; i++) {
      const prev = rows[i - 1];
      const cur = rows[i];
      assert.ok(prev.total >= cur.total, `「${exam.name}」应按总分降序排列`);
      assert.ok(cur.rank >= prev.rank, `「${exam.name}」名次不应随总分下降而前进`);
      if (cur.total === prev.total) assert.equal(cur.rank, prev.rank, '同分应并列同名次');
      else assert.ok(cur.rank > prev.rank, '总分更低者名次必须更靠后');
    }
    for (const row of rows) {
      assert.ok(row.total > 0, `${row.student.name} 的总分应大于 0`);
      assert.ok(row.rate > 0 && row.rate <= 100, `${row.student.name} 的得分率应在 (0,100] 内`);
      assert.equal(row.counted + row.missing, 6, '已录与缺考科目数之和应等于参考科目数');
    }
  }

  const first = buildRanking(state, 'e_1');
  const absent = first.filter((r) => r.missing > 0);
  assert.equal(absent.length, 1, '示例数据应保留恰好一条缺考记录用于演示缺考规则');
  assert.ok(absent[0].average > 0, '缺考学生的平均分应按已录科目计算，不应被 0 分拖低');
});

/* ---------------- 执行 ---------------- */

let failed = 0;
for (const [name, fn] of cases) {
  try {
    fn();
    passed++;
    console.log(`  PASS  ${name}`);
  } catch (err) {
    failed++;
    console.log(`  FAIL  ${name}\n        ${err.message}`);
  }
}
console.log(`\n${passed} 通过 / ${failed} 失败 / 共 ${cases.length} 项`);
process.exit(failed === 0 ? 0 : 1);
