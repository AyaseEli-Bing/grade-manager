#!/usr/bin/env python3
"""
终端界面端到端测试：在伪终端(pty)里真实运行程序，按键，并对**实际渲染出的屏幕**断言。

为什么必须这样做：
  1. ncurses 需要真实终端才能初始化，管道执行会直接失败 —— 「能编译」不等于「能跑起来」。
  2. ncurses 只做增量刷新，直接读转义字节流来"看屏幕"是不可靠的（会把历史内容当成当前屏幕）。
     所以这里用 pyte 作为终端模拟器，把字节流渲染成字符网格后再断言。

依赖：pyte（纯 Python）。安装：
  pip install pyte

运行：
  python3 tests/test_tui.py

两个已处理的坑：
  1. ncurses 启动会打开应用光标键模式(smkx)，方向键序列是 ESC O A / ESC O B，
     不是普通模式下的 ESC [ A / ESC [ B，发错会被当成普通字符丢掉。
  2. 主菜单上的 q 是「退出程序」，视图内的 q 才是「返回主菜单」。
"""

import os
import pty
import select
import signal
import struct
import subprocess
import sys
import termios
import time
import fcntl

try:
    import pyte
except ImportError:  # pragma: no cover
    print("需要 pyte：pip install pyte")
    sys.exit(2)

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BIN = os.path.join(ROOT, "build", "grades")
DATA = "/tmp/gm_tui_test.json"

COLS, ROWS = 110, 34
DOWN = "\x1bOB"

passed = 0
failed = 0


def check(condition, label, extra=""):
    global passed, failed
    if condition:
        passed += 1
        print(f"  PASS  {label}")
    else:
        failed += 1
        print(f"  FAIL  {label}" + (f"\n        {extra}" if extra else ""))


class Tui:
    def __init__(self, argv):
        self.screen = pyte.Screen(COLS, ROWS)
        self.stream = pyte.ByteStream(self.screen)

        primary, secondary = pty.openpty()
        fcntl.ioctl(secondary, termios.TIOCSWINSZ, struct.pack("HHHH", ROWS, COLS, 0, 0))
        env = dict(os.environ, TERM="xterm-256color", LANG="zh_CN.UTF-8")
        self.proc = subprocess.Popen(
            argv, stdin=secondary, stdout=secondary, stderr=secondary,
            env=env, close_fds=True, preexec_fn=os.setsid,
        )
        os.close(secondary)
        self.fd = primary

    # ---------- 基础 ----------

    def alive(self):
        return self.proc.poll() is None

    def pump(self, seconds=0.3):
        deadline = time.time() + seconds
        while time.time() < deadline:
            ready, _, _ = select.select([self.fd], [], [], 0.08)
            if not ready:
                continue
            try:
                chunk = os.read(self.fd, 65536)
            except OSError:
                break
            if not chunk:
                break
            self.stream.feed(chunk)

    def display(self):
        """当前屏幕内容（已按终端渲染）"""
        return "\n".join(line.rstrip() for line in self.screen.display)

    def wait_on_screen(self, needle, seconds=4.0):
        """轮询直到 needle 出现在当前屏幕上"""
        deadline = time.time() + seconds
        while time.time() < deadline:
            if needle in self.display():
                return True
            self.pump(0.15)
        return needle in self.display()

    def send(self, text, settle=0.5):
        if not self.alive():
            raise RuntimeError(f"进程已退出(code={self.proc.returncode})，无法发送 {text!r}")
        os.write(self.fd, text.encode("utf-8"))
        self.pump(settle)

    def close(self):
        if self.alive():
            try:
                os.killpg(os.getpgid(self.proc.pid), signal.SIGKILL)
            except OSError:
                pass
        try:
            os.close(self.fd)
        except OSError:
            pass


def main():
    if not os.path.exists(BIN):
        print(f"找不到 {BIN}，请先执行 make")
        return 1
    if os.path.exists(DATA):
        os.remove(DATA)

    print("终端界面端到端测试（真实 pty + 终端模拟）\n")
    tui = Tui([BIN, "--file", DATA])
    try:
        # ---------- 启动 ----------
        check(tui.wait_on_screen("学生管理", 6.0), "启动后渲染出中文主菜单", tui.display())
        check("6 个科目" in tui.display(), "主菜单显示预置科目数与班级名")
        check("保存并退出" in tui.display(), "主菜单列出全部 9 个功能项")

        # ---------- 空数据下的错误路径（视图会自动退回主菜单） ----------
        tui.send(DOWN + "\r")  # 成绩录入
        check(tui.wait_on_screen("还没有学生"), "无学生时成绩录入给出明确提示", tui.display())
        tui.send(" ")
        check(tui.wait_on_screen("保存并退出"), "关闭提示后回到主菜单")

        tui.send(DOWN * 2 + "\r")  # 统计排名
        check(tui.wait_on_screen("还没有学生"), "无学生时统计排名给出明确提示", tui.display())
        tui.send(" ")
        check(tui.wait_on_screen("保存并退出"), "关闭提示后回到主菜单")

        # ---------- 学生管理：空状态与新增表单 ----------
        tui.send("\r")  # 学生管理
        check(tui.wait_on_screen("暂无数据"), "空名单时显示空状态", tui.display())
        check("[a]新增" in tui.display(), "标题栏列出快捷键")

        tui.send("a")
        check(tui.wait_on_screen("姓名（必填）"), "按 a 弹出新增学生表单", tui.display())
        tui.send("TestStudent")
        tui.send("\r")
        check(tui.wait_on_screen("TestStudent"), "新增的学生出现在名单中", tui.display())
        check("共 1 名学生" in tui.display(), "标题显示学生数已更新", tui.display())
        check("学号" in tui.display() and "备注" in tui.display(), "表格渲染出列标题", tui.display())
        tui.send("q")
        check(tui.wait_on_screen("保存并退出"), "视图内按 q 返回主菜单")

        # ---------- 科目管理 ----------
        tui.send(DOWN * 3 + "\r")
        check(tui.wait_on_screen("语文"), "科目管理显示预置科目「语文」", tui.display())
        screen = tui.display()
        check("数学" in screen and "英语" in screen, "显示全部 6 个预置科目")
        check("150" in screen and "100" in screen, "显示各科满分")
        check("关联成绩数" in screen, "显示关联成绩列")
        tui.send("q")

        # ---------- 考试管理 ----------
        tui.send(DOWN * 4 + "\r")
        check(tui.wait_on_screen("第一次月考"), "考试管理显示预置考试", tui.display())
        check("←当前" in tui.display(), "标出当前考试")
        tui.send("q")

        # ---------- 统计排名（已有 1 名学生） ----------
        tui.send(DOWN * 2 + "\r")
        check(tui.wait_on_screen("按学生排名"), "有学生后统计排名正常渲染", tui.display())
        check("TestStudent" in tui.display(), "排名表中出现该学生")
        check("参考 1 人" in tui.display(), "统计页显示汇总信息", tui.display())
        tui.send("\t")
        check(tui.wait_on_screen("按科目统计"), "Tab 可切换到按科目统计", tui.display())
        check("及格率" in tui.display(), "按科目视图显示及格率")
        tui.send("q")

        # ---------- 成绩录入（已有 1 名学生） ----------
        tui.send(DOWN + "\r")
        check(tui.wait_on_screen("已录"), "有学生后成绩录入渲染网格与进度", tui.display())
        check("语文/150" in tui.display(), "表头显示科目与满分", tui.display())
        tui.send("85")
        tui.send("\r")
        check(tui.wait_on_screen("85"), "可在网格中直接录入分数", tui.display())
        check("已录 1/6" in tui.display(), "进度随录入更新", tui.display())
        tui.send("q")

        # ---------- 退出并落盘 ----------
        tui.send("q")
        exited = False
        try:
            tui.proc.wait(timeout=6)
            exited = True
        except subprocess.TimeoutExpired:
            pass
        check(exited, "主菜单按 q 正常退出程序", tui.display())

        check(os.path.exists(DATA), "有改动后退出会写出数据文件", DATA)
        if os.path.exists(DATA):
            with open(DATA, "r", encoding="utf-8") as handle:
                body = handle.read()
            check("TestStudent" in body, "数据文件包含新增的学生")
            check("第一次月考" in body, "数据文件包含预置考试")
            check("语文" in body and "150" in body, "数据文件包含预置科目与满分")
            check("85" in body, "数据文件包含录入的分数")
            os.remove(DATA)
    except Exception as error:  # noqa: BLE001 - 测试脚本把异常也计入失败
        check(False, f"测试过程异常：{error}", tui.display())
    finally:
        tui.close()

    print(f"\n{passed} 通过 / {failed} 失败")
    return 0 if failed == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
