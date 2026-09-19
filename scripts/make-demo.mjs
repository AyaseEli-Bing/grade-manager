/**
 * 生成示例数据（可复现，不使用随机源）。
 * 产出：
 *   examples/示例数据.json        —— 可直接在应用内「导入」使用，含 3 场考试
 *   examples/成绩导入模板.csv     —— 可直接在「学生管理 → 导入 CSV」使用
 *
 * 运行：node scripts/make-demo.mjs
 */

import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const ROOT = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const OUT = path.join(ROOT, 'examples');
fs.mkdirSync(OUT, { recursive: true });

const SUBJECTS = [
  { id: 's_zh', name: '语文', fullMark: 150 },
  { id: 's_ma', name: '数学', fullMark: 150 },
  { id: 's_en', name: '英语', fullMark: 150 },
  { id: 's_ph', name: '物理', fullMark: 100 },
  { id: 's_ch', name: '化学', fullMark: 100 },
  { id: 's_bi', name: '生物', fullMark: 100 },
];

const NAMES = [
  '陈嘉禾', '李思远', '王雨桐', '张明轩', '刘子涵', '赵一鸣',
  '孙悦宁', '周瑾瑜', '吴承泽', '郑婉清', '黄浩然', '徐若彤',
  '何知远', '林晚舟', '罗慕言', '谢云舒',
];

const students = NAMES.map((name, index) => ({
  id: `st_${String(index + 1).padStart(2, '0')}`,
  sid: `2026${String(index + 1).padStart(3, '0')}`,
  name,
  gender: index % 2 === 0 ? '男' : '女',
  note: index === 0 ? '班长' : index === 2 ? '学习委员' : index === 5 ? '课代表' : '',
}));

const EXAMS = [
  { id: 'e_1', name: '第一次月考', date: '2026-03-12', subjectIds: [] },
  { id: 'e_2', name: '期中考试', date: '2026-04-25', subjectIds: [] },
  { id: 'e_3', name: '第二次月考', date: '2026-05-30', subjectIds: [] },
];

/** 确定性伪随机：以字符串种子生成 0~1 的稳定值 */
function seeded(seed) {
  let h = 2166136261;
  for (let i = 0; i < seed.length; i++) {
    h ^= seed.charCodeAt(i);
    h = Math.imul(h, 16777619);
  }
  return ((h >>> 0) % 100000) / 100000;
}

const scores = {};
EXAMS.forEach((exam, examIndex) => {
  scores[exam.id] = {};
  students.forEach((student, studentIndex) => {
    const row = {};
    // 每个学生有一个稳定的"学力基線"，随考试场次小幅进步
    const base = 0.52 + seeded(`${student.id}-base`) * 0.4;
    const growth = examIndex * 0.015;
    for (const subject of SUBJECTS) {
      const wobble = (seeded(`${student.id}-${subject.id}-${exam.id}`) - 0.5) * 0.16;
      let ratio = base + growth + wobble;
      ratio = Math.min(0.99, Math.max(0.32, ratio));
      // 极端值：让第 1 名与第 12 名拉开差距，排名才有看点
      if (studentIndex === 0) ratio = Math.min(0.99, ratio + 0.1);
      if (studentIndex === students.length - 1) ratio = Math.max(0.3, ratio - 0.1);
      row[subject.id] = Math.round(subject.fullMark * ratio);
    }
    scores[exam.id][student.id] = row;
  });

  // 制造两处缺考，验证缺考不计入总分的规则
  if (examIndex === 0) delete scores[exam.id][students[4].id].s_bi;
  if (examIndex === 2) delete scores[exam.id][students[9].id].s_ph;
});

const state = {
  version: 1,
  className: '高二(3)班',
  subjects: SUBJECTS,
  students,
  exams: EXAMS,
  scores,
  activeExamId: 'e_2',
  updatedAt: new Date().toISOString(),
};

fs.writeFileSync(path.join(OUT, '示例数据.json'), JSON.stringify(state, null, 2), 'utf8');

const header = ['学号', '姓名', '性别', '备注', ...SUBJECTS.map((s) => s.name)].join(',');
const lines = [header];
for (const student of students) {
  lines.push(
    [
      student.sid,
      student.name,
      student.gender,
      student.note,
      ...SUBJECTS.map((s) => scores.e_2[student.id][s.id] ?? ''),
    ].join(',')
  );
}
fs.writeFileSync(path.join(OUT, '成绩导入模板.csv'), `\uFEFF${lines.join('\n')}`, 'utf8');

console.log(`已生成 examples/示例数据.json（${students.length} 名学生 / ${EXAMS.length} 场考试 / ${SUBJECTS.length} 个科目）`);
console.log('已生成 examples/成绩导入模板.csv（含期中考试成绩，可直接导入）');
