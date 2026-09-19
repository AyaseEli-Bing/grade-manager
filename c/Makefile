# 班级成绩管理系统（终端版）—— 纯 C，零第三方库
#
#   make           编译为 build/grades
#   make run       编译并运行
#   make test      编译并运行逻辑层测试
#   make asan      用 AddressSanitizer 编译并跑测试（内存安全验证）
#   make wincheck  做 Windows 平台分支的类型自检（本机无 SDK，用手写桩）
#   make win       交叉编译出 Windows 可执行文件（需要 Zig 或 MinGW-w64）
#   make verify    跑质量门禁
#   make clean     清理产物
#
# 平台差异：
#   POSIX（macOS / Linux）用 ncurses + -lncurses
#   Windows（含 Win7）用自带的 Win32 控制台后端，不依赖任何外部库

CC      ?= cc
CFLAGS  ?= -std=c11 -O2
WARNINGS = -Wall -Wextra -Werror -Wshadow -Wconversion -Wsign-conversion
INC      = -I include

# ---------- 源文件分组 ----------

# 逻辑层：不依赖任何界面库，测试只需要这一组
SRC_LOGIC = src/compat.c \
            src/model.c \
            src/json.c \
            src/json_parse.c \
            src/json_scan_string.c \
            src/json_write.c \
            src/calc.c \
            src/io_csv.c \
            src/persist.c \
            src/persist_json.c

SRC_UI = src/ui.c \
         src/ui_chart.c \
         src/ui_widgets.c \
         src/ui_list.c \
         src/views_common.c \
         src/view_students.c \
         src/view_scores.c \
         src/view_stats.c \
         src/view_stats_detail.c \
         src/view_subjects.c \
         src/view_exams.c

SRC_MAIN = src/main.c

# ---------- 平台选择 ----------

ifeq ($(OS),Windows_NT)
  PLATFORM_SRC = src/win_curses.c src/win_refresh.c src/win_draw.c src/win_input.c
  PLATFORM_LIBS =
else
  PLATFORM_SRC =
  PLATFORM_LIBS = -lncurses
endif

SRC      = $(SRC_LOGIC) $(SRC_UI) $(SRC_MAIN) $(PLATFORM_SRC)
TEST_SRC = $(SRC_LOGIC) tests/test_logic.c

OBJ  = $(SRC:src/%.c=build/%.o)
BIN  = build/grades
TEST_BIN = build/test-logic

# Windows 交叉编译用的 Zig（见 c/README.md）
ZIG ?= $(HOME)/.local/toolchains/zig-0.16.0/zig

.PHONY: all run test asan wincheck win verify clean dirs

all: dirs $(BIN)

dirs:
	@mkdir -p build

$(BIN): $(OBJ)
	$(CC) $(CFLAGS) $(WARNINGS) $(OBJ) -o $@ $(PLATFORM_LIBS) -lm

build/%.o: src/%.c
	$(CC) $(CFLAGS) $(WARNINGS) $(INC) -c $< -o $@

run: all
	./$(BIN)

test: dirs
	$(CC) $(CFLAGS) $(WARNINGS) $(INC) $(TEST_SRC) -o $(TEST_BIN) -lm
	./$(TEST_BIN)

asan: dirs
	$(CC) -std=c11 -g -O1 -fsanitize=address,undefined -fno-omit-frame-pointer \
		$(WARNINGS) $(INC) $(TEST_SRC) -o build/test-asan -lm
	./build/test-asan

# 本机没有 Windows SDK，用 tests/win32stub 下手写的最小 Win32 声明做类型自检。
# 这只能抓语法与类型错误，不能替代在 Windows 上的真实编译。
wincheck:
	@bash ../scripts/check-win.sh

# 交叉编译出 Windows 可执行文件。
# -Wl,--major/minor-subsystem-version 让 PE 头声明最低支持 Windows 7。
win: dirs
	@if [ ! -x "$(ZIG)" ]; then echo "找不到 Zig：$(ZIG)"; echo "见 c/README.md 的「Windows 构建」一节"; exit 1; fi
	$(ZIG) cc -target x86_64-windows-gnu -std=c11 -O2 $(WARNINGS) \
		-Wl,--major-subsystem-version,6 -Wl,--minor-subsystem-version,1 \
		-I include $(SRC) -o build/grades-win64.exe -lm
	@echo "已生成 build/grades-win64.exe（PE 最低子系统版本 6.1 = Windows 7）"

verify:
	@bash ../scripts/verify-c.sh

clean:
	rm -rf build
