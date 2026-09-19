/**
 * 桌面链路验收（jsdom + 伪造的 Tauri 桥）。
 *
 * 这是本次「初始化失败」事故的回归测试：
 * 之前的测试只在浏览器降级路径上跑，从未穿过 window.__TAURI_INTERNALS__，
 * 因此漏掉了裸模块说明符导致的生产环境启动崩溃。
 * 本文件把桥"装上"，逐一断言应用到底向 Rust 端发了哪些命令、参数对不对。
 *
 * 运行：
 *   NODE_PATH=/Users/bing1111/.workbuddy/binaries/node/workspace/node_modules \
 *   node scripts/test-desktop.mjs
 */

import assert from 'node:assert/strict';
import { createRequire } from 'node:module';

const require = createRequire(import.meta.url);
const { JSDOM } = require('jsdom');

let dom;
const calls = [];
const disk = new Map();
let storedData = null;
let openPick = null;
let bootReport = null;

dom = new JSDOM('<!doctype html><html><body><div id="toast-root"></div></body></html>', {
  url: 'http://localhost/',
  pretendToBeVisual: true,
});

dom.window.__TAURI_INTERNALS__ = {
  async invoke(cmd, args) {
    calls.push({ cmd, args: args || {} });
    switch (cmd) {
      case 'load_data':
        return storedData === null ? 'null' : storedData;
      case 'save_data':
        storedData = args.content;
        return null;
      case 'data_path':
        return '/Users/fake/Library/Application Support/cn.grade.manager/grades.json';
      case 'create_backup':
        return '/Users/fake/backups/grades-1758000000.json';
      case 'read_file': {
        if (!disk.has(args.path)) throw new Error(`ENOENT: ${args.path}`);
        return disk.get(args.path);
      }
      case 'write_file':
        disk.set(args.path, args.content);
        return null;
      case 'plugin:dialog|save':
        return args.options.defaultPath;
      case 'plugin:dialog|open':
        return openPick;
      case 'report_boot':
        bootReport = args;
        return null;
      default:
        throw new Error(`未预期的后端命令: ${cmd}`);
    }
  },
};

global.window = dom.window;
global.document = dom.window.document;
global.localStorage = dom.window.localStorage;
global.Event = dom.window.Event;
global.MouseEvent = dom.window.MouseEvent;

const store = await import('../src/js/store.js');
const io = await import('../src/js/io.js');

const fixture = () => store.normalizeState({
  className: '高二(3)班',
  subjects: [{ id: 's_zh', name: '语文', fullMark: 150 }],
  students: [{ id: 'st_a', sid: '001', name: '陈一鸣' }],
  exams: [{ id: 'e1', name: '第一次月考', date: '2026-03-10', subjectIds: [] }],
  scores: { e1: { st_a: { s_zh: 120 } } },
  activeExamId: 'e1',
});

const last = () => calls[calls.length - 1];
const find = (cmd) => calls.find((c) => c.cmd === cmd);
const countOf = (cmd) => calls.filter((c) => c.cmd === cmd).length;

const cases = [];
const test = (name, fn) => cases.push([name, fn]);

/* ------------------------------------------------------------------ */

test('桌面桥被识别为桌面环境，且不加载任何外部模块', () => {
  assert.equal(store.isDesktop(), true, '应识别为桌面环境');
  assert.equal(dom.window.__TAURI_INTERNALS__.__usedByProject, undefined, '不应被替换');
});

test('loadState 通过 load_data 命令读取，首次运行返回默认数据', async () => {
  calls.length = 0;
  const state = await store.loadState();
  assert.equal(last().cmd, 'load_data', '应调用 load_data');
  assert.equal(last().args.constructor, Object, '参数应为空对象而非 undefined');
  assert.equal(state.students.length, 0, '无存档时应返回默认空数据');
  assert.equal(state.subjects.length, 6, '默认应带 6 个科目');
  assert.equal(store.takeLoadError(), null, '无存档属正常情况，不应记录为错误');
});

test('loadState 能读回已存档数据并做规范化', async () => {
  calls.length = 0;
  storedData = JSON.stringify({
    className: '高三(1)班',
    subjects: [{ id: 's1', name: '语文', fullMark: 150 }],
    students: [{ id: 'st1', name: '张三' }],
    exams: [{ id: 'e1', name: '期末', subjectIds: [] }],
    scores: { e1: { st1: { s1: 130 }, ghost: { s1: 10 } } },
    activeExamId: 'e1',
  });
  const state = await store.loadState();
  assert.equal(state.className, '高三(1)班');
  assert.equal(state.students.length, 1);
  assert.deepEqual(Object.keys(state.scores.e1), ['st1'], '悬空学生引用应被清理');
});

test('存档损坏时回退默认数据并记录可提示的错误，不抛异常', async () => {
  calls.length = 0;
  storedData = '{ 这不是合法 JSON';
  const state = await store.loadState();
  assert.equal(state.students.length, 0, '应回退为空数据而非崩溃');
  const err = store.takeLoadError();
  assert.ok(err, '应记录读取错误供界面提示');
  assert.match(err.message, /读取失败/);
  assert.ok(err.detail.length > 0, '应携带原始错误详情便于排查');
  assert.equal(store.takeLoadError(), null, '错误只能被取走一次');
  storedData = null;
});

test('saveState 通过 save_data 命令落盘，内容为可解析的 JSON', async () => {
  calls.length = 0;
  storedData = null;
  await store.saveState(fixture());
  assert.equal(last().cmd, 'save_data', '应调用 save_data');
  assert.equal(typeof last().args.content, 'string');
  const parsed = JSON.parse(last().args.content);
  assert.equal(parsed.className, '高二(3)班');
  assert.equal(parsed.students[0].name, '陈一鸣');
  assert.ok(parsed.updatedAt, '落盘时应刷新 updatedAt');
});

test('保存失败时把错误交给回调，不抛出到上层', async () => {
  const original = dom.window.__TAURI_INTERNALS__.invoke;
  dom.window.__TAURI_INTERNALS__.invoke = async (cmd) => {
    if (cmd === 'save_data') throw new Error('磁盘只读');
    return original(cmd, {});
  };
  let captured = 'untouched';
  await store.saveState(fixture(), (err) => {
    captured = err;
  });
  assert.ok(captured instanceof Error, '应通过回调上报错误');
  assert.equal(captured.message, '磁盘只读');
  dom.window.__TAURI_INTERNALS__.invoke = original;
});

test('getDataPath 返回真实数据文件路径用于界面展示', async () => {
  const path = await store.getDataPath();
  assert.match(path, /grades\.json$/);
});

test('backupNow 调用 create_backup 并返回备份路径', async () => {
  calls.length = 0;
  const path = await store.backupNow();
  assert.equal(last().cmd, 'create_backup');
  assert.match(path, /backups\/grades-\d+\.json$/);
});

/* ---------------- 导出 ---------------- */

test('导出 JSON：先弹系统另存为对话框，再写入所选路径', async () => {
  calls.length = 0;
  disk.clear();
  const ok = await io.exportJson(fixture());
  assert.equal(ok, true, '未取消时应返回 true');

  const dialog = find('plugin:dialog|save');
  assert.ok(dialog, '应调用系统另存为对话框');
  assert.equal(dialog.args.options.filters[0].extensions[0], 'json', '应限定 json 扩展名');
  assert.match(dialog.args.options.defaultPath, /\.json$/, '默认文件名应带扩展名');
  assert.ok(dialog.args.options.defaultPath.includes('高二(3)班'), '默认文件名应含班级名');

  const write = find('write_file');
  assert.ok(write, '应向所选路径写入内容');
  assert.equal(write.args.path, dialog.args.options.defaultPath, '写入路径应等于用户选择');
  assert.equal(JSON.parse(write.args.content).students[0].name, '陈一鸣');
});

test('导出 CSV：内容带 BOM、含表头与排名列，并写入所选路径', async () => {
  calls.length = 0;
  disk.clear();
  const ok = await io.exportCsv(fixture(), 'current');
  assert.equal(ok, true);

  const dialog = find('plugin:dialog|save');
  assert.equal(dialog.args.options.filters[0].extensions[0], 'csv', '应限定 csv 扩展名');
  const content = find('write_file').args.content;
  assert.equal(content.charCodeAt(0), 0xfeff, '应以 BOM 开头以兼容 Excel');
  const lines = content.slice(1).split('\n');
  assert.equal(lines[0], '学号,姓名,语文,总分,平均分,得分率,排名');
  assert.equal(lines[1], '001,陈一鸣,120,120,120,80%,1');
});

test('用户在对话框中取消时不写文件', async () => {
  const original = dom.window.__TAURI_INTERNALS__.invoke;
  dom.window.__TAURI_INTERNALS__.invoke = async (cmd, args) => {
    if (cmd === 'plugin:dialog|save') return null; // 模拟取消
    return original(cmd, args);
  };
  calls.length = 0;
  assert.equal(await io.exportJson(fixture()), false, '取消应返回 false');
  assert.equal(find('write_file'), undefined, '取消后不应写文件');
  dom.window.__TAURI_INTERNALS__.invoke = original;
});

/* ---------------- 导入 ---------------- */

test('导入 JSON：打开对话框、读取文件、解析为合法状态', async () => {
  calls.length = 0;
  const payload = {
    className: '高三(2)班',
    subjects: [{ id: 's_zh', name: '语文', fullMark: 150 }],
    students: [{ id: 'st_a', sid: '001', name: '陈一鸣' }, { id: 'st_b', sid: '002', name: '李思远' }],
    exams: [{ id: 'e1', name: '期中考试', date: '2026-04-22', subjectIds: [] }],
    scores: { e1: { st_a: { s_zh: 132 }, st_b: { s_zh: 118 } } },
    activeExamId: 'e1',
  };
  disk.set('/tmp/备份.json', JSON.stringify(payload));
  openPick = '/tmp/备份.json';

  const result = await io.importJson();
  assert.equal(find('plugin:dialog|open').args.options.multiple, false, '应限定单选');
  assert.equal(find('plugin:dialog|open').args.options.filters[0].extensions[0], 'json');
  assert.equal(find('read_file').args.path, '/tmp/备份.json', '读取路径应等于用户选择');
  assert.equal(result.ok, true);
  assert.equal(result.state.className, '高三(2)班');
  assert.equal(result.state.students.length, 2);
});

test('导入 JSON：文件内容非法时返回可展示的错误而非抛异常', async () => {
  disk.set('/tmp/坏文件.json', 'not json at all');
  openPick = '/tmp/坏文件.json';
  const result = await io.importJson();
  assert.equal(result.ok, false);
  assert.match(result.error, /解析失败/);
});

test('导入 JSON：用户取消时返回 null 且不读取文件', async () => {
  calls.length = 0;
  openPick = null;
  assert.equal(await io.importJson(), null);
  assert.equal(find('read_file'), undefined, '取消后不应读取文件');
});

test('导入 CSV：读取真实文件并按科目列解析出学生与成绩', async () => {
  calls.length = 0;
  disk.set('/tmp/名单.csv', '\uFEFF学号,姓名,性别,备注,语文\n001,陈一鸣,男,班长,120\n002,李思远,男,,118\n');
  openPick = '/tmp/名单.csv';

  const state = fixture();
  const result = await io.importCsv(state);
  assert.equal(find('plugin:dialog|open').args.options.filters[0].extensions[0], 'csv');
  assert.equal(result.ok, true);
  assert.equal(result.students.length, 2);
  assert.equal(result.students[0].note, '班长');
  assert.equal(result.hasScores, true);
  assert.equal(result.scores[result.students[0].id].s_zh, 120, '语文成绩应写入');
});

test('读取失败（文件被删/无权限）时不返回半成品数据', async () => {
  openPick = '/tmp/不存在.json';
  const original = dom.window.__TAURI_INTERNALS__.invoke;
  dom.window.__TAURI_INTERNALS__.invoke = async (cmd, args) => {
    if (cmd === 'read_file') throw new Error('EACCES: permission denied');
    return original(cmd, args);
  };
  await assert.rejects(() => io.importJson(), /permission denied/, '应把底层错误抛出交给界面层提示');
  dom.window.__TAURI_INTERNALS__.invoke = original;
});

test('reportBoot 上报启动结果，作为「打包后能否启动」的自动化证据', async () => {
  calls.length = 0;
  await store.reportBoot(true, 'ok');
  assert.equal(last().cmd, 'report_boot', '应调用 report_boot');
  assert.equal(last().args.ok, true);
  assert.equal(last().args.detail, 'ok');
  assert.match(last().args.at, /^\d{4}-\d{2}-\d{2}T/, '应带上 ISO 时间戳');

  await store.reportBoot(false, 'Failed to resolve module specifier');
  assert.equal(bootReport.ok, false);
  assert.match(bootReport.detail, /module specifier/, '失败详情应原样上报，便于排查');
});

test('启动上报失败不阻断主流程', async () => {
  const original = dom.window.__TAURI_INTERNALS__.invoke;
  dom.window.__TAURI_INTERNALS__.invoke = async (cmd, args) => {
    if (cmd === 'report_boot') throw new Error('只读文件系统');
    return original(cmd, args);
  };
  await store.reportBoot(true, 'ok');
  dom.window.__TAURI_INTERNALS__.invoke = original;
  assert.ok(true, '不应抛出异常');
});

test('全程未向后端发送任何未知命令', () => {
  const known = new Set([
    'load_data', 'save_data', 'data_path', 'create_backup', 'read_file', 'write_file', 'report_boot',
    'plugin:dialog|open', 'plugin:dialog|save',
  ]);
  const unknown = calls.map((c) => c.cmd).filter((cmd) => !known.has(cmd));
  assert.deepEqual(unknown, [], `出现未预期命令：${unknown.join(', ')}`);
});

test('所有命令的参数名与 Rust 侧形参一致（单复数、大小写）', () => {
  for (const call of calls) {
    if (call.cmd === 'read_file' || call.cmd === 'write_file') {
      const keys = Object.keys(call.args);
      assert.ok(keys.includes('path'), `${call.cmd} 参数名应为 path`);
      assert.equal(keys.includes('filePath') || keys.includes('file'), false, `${call.cmd} 不应使用 filePath/file`);
    }
    if (call.cmd === 'save_data') {
      assert.ok(Object.keys(call.args).includes('content'), 'save_data 参数名应为 content');
    }
    if (call.cmd.startsWith('plugin:dialog|')) {
      assert.ok(Object.keys(call.args).includes('options'), '对话框参数应包在 options 内');
    }
  }
});

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
