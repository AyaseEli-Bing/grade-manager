/**
 * 门禁检查：模块图完整性。
 *
 * 本项目前端没有打包器，WebView 里不存在 node_modules 解析，也没有构建期报错兜底。
 * 因此导入语句必须同时满足两条：
 *   1) 路径必须以 ./ 或 ../ 开头（裸模块说明符会在生产环境抛解析错误，表现为"初始化失败"白屏）
 *   2) 相对路径指向的文件必须真实存在（否则运行时报"模块找不到"）
 *
 * 用法：node scripts/check-imports.mjs [目标目录，默认 src]
 */

import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const ROOT = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const TARGET = process.argv[2] || path.join(ROOT, 'src');

const walk = (dir) =>
  fs.readdirSync(dir, { withFileTypes: true }).flatMap((entry) =>
    entry.isDirectory() ? walk(path.join(dir, entry.name)) : [path.join(dir, entry.name)]
  );

/** 去掉块注释与整行注释，避免注释里举例的写法被误判 */
function stripComments(source) {
  return source.replace(/\/\*[\s\S]*?\*\//g, '').replace(/^[ \t]*\/\/.*$/gm, '');
}

const SPECIFIER = /\b(?:from|import)\s*\(?\s*["']([^"']+)["']/g;

const bare = [];
const missing = [];

for (const file of walk(TARGET)) {
  if (!/\.(js|mjs)$/.test(file)) continue;
  const lines = stripComments(fs.readFileSync(file, 'utf8')).split('\n');
  lines.forEach((line, index) => {
    SPECIFIER.lastIndex = 0;
    let match;
    while ((match = SPECIFIER.exec(line)) !== null) {
      const spec = match[1];
      const where = `${path.relative(ROOT, file)}:${index + 1}`;
      if (!spec.startsWith('.') && !spec.startsWith('/')) {
        bare.push(`${where}  ${spec}`);
        continue;
      }
      const resolved = path.resolve(path.dirname(file), spec);
      if (!fs.existsSync(resolved)) missing.push(`${where}  ${spec}`);
    }
  });
}

let failed = false;

if (bare.length) {
  failed = true;
  console.log(`  FAIL 发现 ${bare.length} 处裸模块说明符：\n    ${bare.join('\n    ')}`);
  console.log('        → 改为相对路径导入，或直接调用 window.__TAURI_INTERNALS__.invoke');
} else {
  console.log('  PASS 无裸模块说明符');
}

if (missing.length) {
  failed = true;
  console.log(`  FAIL 发现 ${missing.length} 处导入指向不存在的文件：\n    ${missing.join('\n    ')}`);
} else {
  console.log('  PASS 导入路径全部指向真实文件');
}

process.exit(failed ? 1 : 0);

