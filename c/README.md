# 班级成绩管理系统 · C 终端版

纯 C 实现的全屏终端界面版本，零第三方运行库依赖。与仓库里的图形版（Tauri + HTML）
**共用同一个数据文件和同一套 JSON / CSV 格式**，两个版本可以随时切换着用同一份数据。

- **macOS / Linux**：链接系统自带的 ncurses
- **Windows（含 Win7）**：使用自带的 Win32 控制台后端，不需要 ncurses，也不需要任何额外运行库

---

## 一、为什么要有 C 版

图形版用的是 Tauri + WebView2，而 **WebView2 已不支持 Windows 7**（最后一个支持 Win7 的版本停在
109，2023 年 1 月之后微软不再提供）。所以要在 Win7 上用，只能走原生程序这条路。

C 版直接在控制台绘制汉字界面，只依赖系统自带的控制台 API，Win7 上开箱可用。

---

## 二、编译与运行

### macOS / Linux

```bash
cd c
make          # 生成 build/grades
make run      # 编译并运行
```

### Windows（MinGW-w64 或 Zig）

Windows 上不需要 ncurses，源码会自动切到 Win32 控制台后端（`src/win_curses.c` 等）。
关键是让链接器把 PE 的最低子系统版本标成 6.1（即 Windows 7）：

```bat
:: MinGW-w64
gcc -std=c11 -O2 -Wall -Wextra -I include src\compat.c src\model.c src\json.c ^
    src\json_parse.c src\json_scan_string.c src\json_write.c src\calc.c ^
    src\io_csv.c src\persist.c src\persist_json.c ^
    src\ui.c src\ui_chart.c src\ui_widgets.c src\ui_list.c ^
    src\views_common.c src\view_students.c src\view_scores.c ^
    src\view_stats.c src\view_stats_detail.c src\view_subjects.c src\view_exams.c ^
    src\win_curses.c src\win_refresh.c src\win_draw.c src\win_input.c ^
    src\main.c -o grades.exe -lm ^
    -Wl,--major-subsystem-version,6 -Wl,--minor-subsystem-version,1
```

用 Zig 交叉编译（在任意平台上都能出 Windows 可执行文件）：

```bash
cd c
make win        # 需要 ZIG=/path/to/zig，产物 build/grades-win64.exe
```

> Windows 下**不要**把 `src/win_*.c` 换掉或删掉，它们是这一版唯一没有替代品的部分。
> 反过来，POSIX 下 `make` 不会编译 `src/win_*.c`，它们被平台变量自动排除。

### 数据文件位置

| 平台 | 路径 |
|------|------|
| macOS | `~/Library/Application Support/cn.grade.manager/grades.json` |
| Linux | `~/.local/share/cn.grade.manager/grades.json` |
| Windows | `%APPDATA%\cn.grade.manager\grades.json` |

这个路径**与图形版完全一致**。也可以用命令行参数指定别的文件：

```bash
./grades --file /path/to/grades.json
```

---

## 三、操作方式

主菜单 9 项，上下键选择、Enter 进入。**视图内按 `q` 返回主菜单，主菜单按 `q` 退出程序。**

| 视图 | 快捷键 |
|------|--------|
| 学生管理 | `a` 新增 · `e` 编辑 · `d` 删除 · `i` 导入 CSV · `/` 筛选 |
| 成绩录入 | `0-9` 直接输入 · `←→` 换列 · `Enter` 编辑该格 · `d` 清空（记缺考） · `/` 筛选 |
| 统计排名 | `Tab` 在「按学生排名 / 按科目统计」间切换 · `Enter` 看详情（趋势折线图 / 分数段分布） · `/` 筛选 |
| 科目管理 | `a` 新增 · `e` 编辑 · `d` 删除 · `u`/`j` 上移下移 |
| 考试管理 | `a` 新增 · `e` 编辑 · `s` 设置参考科目 · `空格` 设为当前 · `d` 删除 |

两条业务规则与图形版一致：

1. **缺考不按 0 分计入总分**。成绩格子留空表示缺考，总分与平均分只统计已录科目。
2. **同分同名次并跳号**。两人并列第 1 名时，下一位是第 3 名。

---

## 四、项目结构

```
c/
├── include/                 接口契约
│   ├── model.h              数据模型：班级 / 科目 / 学生 / 考试 / 成绩
│   ├── json.h               迷你 JSON 解析器对外接口
│   ├── json_internal.h      JSON 模块内部（拆分文件用）
│   ├── calc.h               纯计算层接口
│   ├── ui.h                 界面原语（按平台切到 curses.h 或 win_curses.h）
│   ├── win_curses.h         Win32 控制台后端对外接口（curses 兼容）
│   ├── win_internal.h       Win32 后端内部共享
│   ├── compat.h             平台差异层
│   └── views.h              视图层接口
├── src/
│   ├── model.c              数据结构增删改查
│   ├── json*.c              迷你 JSON 解析与序列化（解析 / 字符串 / 序列化 / 核心）
│   ├── calc.c               总分 / 平均分 / 排名 / 单科统计 / 趋势
│   ├── io_csv.c             CSV 导入导出
│   ├── persist.c            数据文件读写（原子保存）
│   ├── persist_json.c       JSON ⇄ 数据模型转换与载入清洗
│   ├── compat.c             时间函数 / strdup / 原子替换文件 / 默认路径
│   ├── ui.c                 主题（唯一允许定义颜色的地方）、页眉页脚、文本度量
│   ├── ui_chart.c           字符条形图与折线图
│   ├── ui_widgets.c         表单 / 确认框 / 提示框 / 输入框
│   ├── ui_list.c            可滚动列表（四个视图的主干）
│   ├── view_*.c             五个视图
│   ├── win_curses.c         Win32 后端：设备初始化、窗口与字符网格
│   ├── win_refresh.c        Win32 后端：刷新上屏
│   ├── win_draw.c           Win32 后端：绘制原语
│   ├── win_input.c          Win32 后端：键盘输入
│   └── main.c               入口、主菜单、导入导出、退出保存
└── tests/
    ├── test_logic.c         逻辑层单元测试（89 项）
    ├── test_tui.py          终端界面端到端测试（35 项，真实 pty）
    └── win32stub/           手写的最小 Win32 声明，仅供本机类型自检
```

**分层规则**：`calc` / `json` / `io_csv` / `persist` 不依赖任何界面代码，
所以 `make test` 只链接这几个文件就能跑，不需要终端。

---

## 五、测试与门禁

```bash
make test      # 逻辑层单元测试（89 项）
make asan      # 用 AddressSanitizer + UBSan 再跑一遍（内存安全）
make wincheck  # Windows 分支类型自检
make verify    # 全部质量门禁
```

### 三层验证

| 层 | 工具 | 覆盖 |
|----|------|------|
| 逻辑层 | `tests/test_logic.c` | JSON 解析与转义、数据清洗、排名规则、单科统计、CSV 引号解析、文件往返 |
| 界面层 | `tests/test_tui.py` | 在真实伪终端里运行程序、按键、用终端模拟器渲染屏幕后断言 |
| Windows 分支 | `scripts/check-win.sh` | 用手写 Win32 声明编译 Windows 代码，抓语法 / 类型 / 字段 / 参数顺序错误 |

界面层测试需要 `pyte`：

```bash
pip install pyte
python3 tests/test_tui.py
```

**为什么界面测试要用伪终端 + 终端模拟器**：ncurses 需要真实终端才能初始化，管道执行会直接失败；
而 ncurses 只做增量刷新，直接读转义字节流来"看屏幕"会把历史内容误当成当前屏幕。
所以必须用 pyte 把字节流渲染成字符网格之后再断言。

### 质量门禁（`make verify`）

1. 逐个源文件编译，**任何告警都算失败**（`-Wall -Wextra -Werror -Wshadow -Wconversion -Wsign-conversion`）
2. 单文件不超过 300 行
3. 颜色纪律：不得硬编码 ANSI 转义，不得绕开主题层直接操作颜色
4. 逻辑层单元测试
5. 终端界面端到端测试
6. Windows 分支类型自检

---

## 六、Windows 7 兼容性的真实状态（请务必读完）

这一节要把话说清楚，避免你基于错误的预期部署。

### 已经做到的

- **不使用任何 Win10+ 才有的 API**。用到的全部是 Win7 就存在的：
  `GetStdHandle` / `GetConsoleScreenBufferInfo` / `WriteConsoleOutputW` /
  `ReadConsoleInputW` / `SetConsoleMode` / `MoveFileExA` / `GetFileAttributesA` 等。
- **不依赖 ANSI / VT 转义序列**。Win7 控制台不支持 VT，所以定位与上色全部走
  `WriteConsoleOutputW` 的 `CHAR_INFO`（字符 + 属性一次性写入）。
- **中文走宽字符路径**。输出用 `WriteConsoleOutputW`，输入用 `ReadConsoleInputW` +
  `WideCharToMultiByte(CP_UTF8)`。字节流 UTF-8 输出在 Win7 控制台下不可靠（即使 `chcp 65001`），
  因此刻意避开。
- **链接时把 PE 最低子系统版本标成 6.1**（`-Wl,--major-subsystem-version,6 -Wl,--minor-subsystem-version,1`），
  否则系统可能直接拒绝启动。
- 界面渲染是**整块写入字符网格**，因此没有逐字符输出的闪烁问题。

### 还没验证的（重要）

- **没有在真实的 Windows（更别说 Win7）上编译和运行过。**
  本机是 macOS，既没有 Windows SDK 也没有 MinGW，Zig 工具链因网络原因未能下载成功。
  我做到的替代验证是用手写的最小 Win32 声明（`tests/win32stub/`）把 Windows 分支
  编译了一遍，能抓语法 / 类型 / 结构体字段 / 参数顺序错误，**但抓不到与真实 `windows.h` 声明的差异** ——
  万一我把某个 Win32 函数的签名写错了，自检会"通过"而真实编译失败。
- **Win7 控制台的中文输入（IME）体验未知。** 控制台对输入法的支持在 Win7 上历来不佳。
  如果录入中文姓名遇到困难，最稳的办法是用 Excel 整理成 CSV，
  再用「学生管理 → `i` 导入 CSV」批量导入 —— 这条路不经过键盘输入法。

### 因此建议的验证顺序

1. 在 Windows 上先按上面的命令编译，确认零报错；
2. 运行后先看主菜单与「科目管理」，确认中文没有变成乱码或方块；
3. 再试「学生管理 → `a` 新增学生」，确认中文输入是否可用；
4. 最后再导入真实班级数据。

任一步出问题，把编译输出或屏幕现象发回来即可，我按实际情况改。

---

## 七、许可证

与仓库根目录一致，MIT。
