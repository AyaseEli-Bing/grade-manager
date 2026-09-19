#!/bin/bash
# Windows 平台分支的类型自检。
#
# 本机既没有 Windows SDK，也没有 MinGW 交叉工具链，因此用 c/tests/win32stub/
# 下手写的最小 Win32 声明，把 Windows 分支的源码用本机 clang 编译一遍。
#
# 能抓到：语法错误、类型不匹配、结构体字段名写错、参数顺序错、隐式转换告警、
#         平台分支（#ifdef _WIN32）引用了不存在的函数。
# 抓不到：与真实 windows.h 声明的差异。所以它只是第一道筛子，
#         真实编译必须在 Windows 上做（见 c/README.md）。
set -u
HERE="$(cd "$(dirname "$0")/.." && pwd)"
C_DIR="$HERE/c"
CC="${CC:-cc}"
WARN="-Wall -Wextra -Werror -Wshadow -Wconversion -Wsign-conversion"
FAIL=0

echo "== Windows 分支类型自检（基于手写 Win32 桩，非真实 SDK）=="

# Windows 后端（依赖 windows.h）
for f in compat win_curses win_refresh win_draw win_input; do
  printf "  %-20s" "src/$f.c"
  if "$CC" -std=c11 -D_WIN32 -I "$C_DIR/include" -I "$C_DIR/tests/win32stub" $WARN \
      -c "$C_DIR/src/$f.c" -o /tmp/_gmwin.o 2>/tmp/_gmwin.err; then
    echo "OK"
  else
    echo "FAIL"
    sed 's/^/      /' /tmp/_gmwin.err | head -10
    FAIL=1
  fi
done

# 界面层与视图层：验证 ui.h 的平台分支能正确切到 mini-curses
for f in ui ui_chart ui_widgets ui_list views_common view_students view_scores \
         view_stats view_stats_detail view_subjects view_exams main; do
  printf "  %-20s" "src/$f.c"
  if "$CC" -std=c11 -D_WIN32 -I "$C_DIR/include" -I "$C_DIR/tests/win32stub" \
      -Wall -Wextra -Werror -Wshadow -c "$C_DIR/src/$f.c" -o /tmp/_gmwin.o 2>/tmp/_gmwin.err; then
    echo "OK"
  else
    echo "FAIL"
    sed 's/^/      /' /tmp/_gmwin.err | head -10
    FAIL=1
  fi
done

rm -f /tmp/_gmwin.o /tmp/_gmwin.err

echo
if [ "$FAIL" -eq 0 ]; then
  echo "Windows 分支自检通过（注意：这不等同于在真实 Windows SDK 下编译通过）"
else
  echo "Windows 分支自检失败"
fi
exit "$FAIL"
