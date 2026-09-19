#!/bin/bash
# C 版质量门禁
#   cd c && make verify
# 检查项：
#   1. 逐个源文件编译（任何告警都视为失败）
#   2. 单文件行数不超过 300
#   3. 颜色纪律：不得硬编码 ANSI 转义、不得绕开主题直接操作颜色对
#   4. 逻辑层单元测试
#   5. 终端界面端到端测试（真实 pty + 终端模拟）
#   6. Windows 分支类型自检
set -u
HERE="$(cd "$(dirname "$0")/.." && pwd)"
C_DIR="$HERE/c"
CC="${CC:-cc}"
WARN="-Wall -Wextra -Werror -Wshadow -Wconversion -Wsign-conversion"
PY="/Users/bing1111/.workbuddy/binaries/python/envs/default/bin/python"
NODE="/Users/bing1111/.workbuddy/binaries/node/versions/22.22.2-3/bin/node"
FAIL=0

echo "== 1. 逐个源文件编译（任何告警都视为失败）=="
while IFS= read -r f; do
  printf "  %-24s" "${f#$C_DIR/}"
  if "$CC" -std=c11 $WARN -I "$C_DIR/include" -c "$f" -o /tmp/_gmc.o 2>/tmp/_gmc.err; then
    echo "OK"
  else
    echo "FAIL"
    sed 's/^/      /' /tmp/_gmc.err | head -10
    FAIL=1
  fi
done < <(find "$C_DIR/src" -name '*.c' | sort)
rm -f /tmp/_gmc.o /tmp/_gmc.err

echo "== 2. 单文件行数（上限 300）=="
while IFS= read -r f; do
  n=$(wc -l < "$f" | tr -d ' ')
  if [ "$n" -gt 300 ]; then echo "  FAIL ${f#$C_DIR/} = $n 行"; FAIL=1; else echo "  OK   ${f#$C_DIR/} = $n 行"; fi
done < <(find "$C_DIR/src" "$C_DIR/include" -name '*.c' -o -name '*.h' | sort)

echo "== 3. 界面层颜色纪律 =="
"$NODE" -e '
const fs=require("fs"),path=require("path");
const dir=process.argv[1];
const walk=d=>fs.readdirSync(d,{withFileTypes:true}).flatMap(e=>e.isDirectory()?walk(path.join(d,e.name)):[path.join(d,e.name)]);
const bad=[];
for(const f of walk(dir)){
  if(!/\.(c|h)$/.test(f)) continue;
  fs.readFileSync(f,"utf8").split("\n").forEach((l,i)=>{
    if(/\\033\[|\\x1b\[|\\e\[|\\u001b\[/.test(l)) bad.push(`${path.basename(f)}:${i+1} 硬编码 ANSI 转义`);
    // 主题层：ui.c 定义语义角色并调用 init_pair；win_curses.c 是 Windows 侧对 init_pair 的实现
    if(/\binit_pair\s*\(/.test(l) && !/(ui\.c|win_curses\.c)$/.test(f)) bad.push(`${path.basename(f)}:${i+1} 绕开主题直接 init_pair`);
    if(/\bCOLOR_PAIR\s*\(/.test(l) && !/(ui\.c|ui\.h|win_curses\.h)$/.test(f)) bad.push(`${path.basename(f)}:${i+1} 绕开主题直接 COLOR_PAIR`);
  });
}
console.log(bad.length? "  FAIL\n    "+bad.join("\n    ") : "  PASS 颜色一律走 ui_role()");
process.exit(bad.length?1:0);
' "$C_DIR/src" || FAIL=1

echo "== 4. 逻辑层单元测试 =="
if [ -f "$C_DIR/tests/test_logic.c" ]; then
  (cd "$C_DIR" && make test 2>&1 | tail -2) || FAIL=1
fi

echo "== 5. 终端界面端到端测试（真实 pty）=="
if [ -f "$C_DIR/tests/test_tui.py" ] && [ -x "$PY" ]; then
  (cd "$C_DIR" && "$PY" tests/test_tui.py 2>&1 | tail -2) || FAIL=1
else
  echo "  SKIP 缺少测试脚本或 Python"
fi

echo "== 6. Windows 分支类型自检 =="
bash "$HERE/scripts/check-win.sh" 2>&1 | tail -2 || FAIL=1

echo
if [ "$FAIL" -eq 0 ]; then echo "C 版门禁全部通过"; else echo "存在未通过项，请修复后重跑"; fi
exit "$FAIL"
