#!/bin/bash
# 质量门禁：语法 / P0-emoji / P0-裸色值 / 单文件行数
# 用法：bash scripts/verify.sh
set -u
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
NODE="/Users/bing1111/.workbuddy/binaries/node/versions/22.22.2-3/bin/node"
SRC="$ROOT/src"
FAIL=0

echo "== 1. JS 语法检查 =="
while IFS= read -r f; do
  cp "$f" /tmp/_chk.mjs
  if "$NODE" --check /tmp/_chk.mjs 2>/tmp/_chk.err; then
    echo "  OK   ${f#$ROOT/}"
  else
    echo "  FAIL ${f#$ROOT/}"; cat /tmp/_chk.err; FAIL=1
  fi
done < <(find "$SRC" -name '*.js' -type f)
rm -f /tmp/_chk.mjs /tmp/_chk.err

echo "== 2. P0-1 emoji 图标扫描 =="
# emoji 全文件扫描；箭头类符号仅扫 js/html（CSS 注释中的 → 属正常排版符号）
"$NODE" -e '
const fs=require("fs"),path=require("path");
const emoji=/[\u{1F000}-\u{1FAFF}\u{2600}-\u{27BF}\u{FE0F}\u{2B00}-\u{2BFF}\u{2705}\u{274C}\u{2728}\u{2764}\u{2049}\u{203C}]/u;
const arrow=/[\u2190-\u21FF\u2B05\u2B06\u2B07]/u;
const walk=d=>fs.readdirSync(d,{withFileTypes:true}).flatMap(e=>e.isDirectory()?walk(path.join(d,e.name)):[path.join(d,e.name)]);
const bad=[];
for(const f of walk(process.argv[1])){
  if(!/\.(js|html|css)$/.test(f)) continue;
  const lines=fs.readFileSync(f,"utf8").split("\n");
  lines.forEach((l,i)=>{
    if(emoji.test(l)) bad.push(`${f}:${i+1} emoji`);
    else if(!/\.css$/.test(f) && arrow.test(l)) bad.push(`${f}:${i+1} 箭头符号（应改用 icons.js）`);
  });
}
console.log(bad.length? "  FAIL\n    "+bad.join("\n    ") : "  PASS 无 emoji / 无箭头符号冒充图标");
process.exit(bad.length?1:0);
' "$SRC" || FAIL=1

echo "== 3. P0-3 裸色值扫描（仅 design-tokens.css 允许裸色值）=="
"$NODE" -e '
const fs=require("fs"),path=require("path");
const walk=d=>fs.readdirSync(d,{withFileTypes:true}).flatMap(e=>e.isDirectory()?walk(path.join(d,e.name)):[path.join(d,e.name)]);
const bad=[];
for(const f of walk(process.argv[1])){
  if(!/\.(js|html)$/.test(f)) continue;
  if(/\.css$/.test(f) && f.endsWith("design-tokens.css")) continue;
  const lines=fs.readFileSync(f,"utf8").split("\n");
  lines.forEach((l,i)=>{
    const m=l.match(/#[0-9a-fA-F]{3,8}\b/g);
    if(m){ const illegal=m.filter(c=>!/^#(fff|ffffff|000|000000)$/i.test(c)); if(illegal.length) bad.push(`${f}:${i+1} ${illegal.join(",")}`); }
    if(/rgb\(\s*\d/.test(l)) bad.push(`${f}:${i+1} rgb()`);
  });
}
const css=walk(process.argv[1]).filter(f=>/\.css$/.test(f)&&!f.endsWith("design-tokens.css"));
for(const f of css){
  const lines=fs.readFileSync(f,"utf8").split("\n");
  lines.forEach((l,i)=>{
    const m=l.match(/#[0-9a-fA-F]{3,8}\b/g);
    if(m){ const illegal=m.filter(c=>!/^#(fff|ffffff|000|000000)$/i.test(c)); if(illegal.length) bad.push(`${f}:${i+1} ${illegal.join(",")}`); }
  });
}
console.log(bad.length? "  FAIL\n    "+bad.join("\n    ") : "  PASS 无硬编码颜色");
process.exit(bad.length?1:0);
' "$SRC" || FAIL=1

echo "== 4. 单文件行数（上限 300）=="
while IFS= read -r f; do
  n=$(wc -l < "$f" | tr -d ' ')
  if [ "$n" -gt 300 ]; then echo "  FAIL ${f#$ROOT/} = $n 行"; FAIL=1; else echo "  OK   ${f#$ROOT/} = $n 行"; fi
done < <(find "$SRC" -name '*.js' -type f)

echo "== 5. 模块图完整性（无打包器环境下唯一的导入检查手段）=="
"$NODE" "$ROOT/scripts/check-imports.mjs" "$SRC" || FAIL=1

echo
if [ "$FAIL" -eq 0 ]; then echo "门禁全部通过"; else echo "存在未通过项，请修复后重跑"; fi
exit "$FAIL"
