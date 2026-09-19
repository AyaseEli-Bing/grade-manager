#!/usr/bin/env python3
"""
Windows PE 可执行文件检查器 —— 用来验证交叉编译产物是否真的面向 Windows 7。

本机是 macOS，无法直接运行 .exe，但可以从 PE 头里读出关键事实：
  1. 是不是合法的 PE32+ 可执行文件（而不是误编成 Mach-O）
  2. 目标架构（x86_64 / i386）
  3. 子系统是不是「控制台」（3），否则双击不会有窗口
  4. **最低子系统版本是不是 6.1（Windows 7）** —— 若标成 10.0，Win7 会直接拒绝启动
  5. **依赖哪些系统 DLL** —— 只能依赖 Win7 就存在的（kernel32 / msvcrt / user32 …）
     如果出现 api-ms-win-core-* 这类 UCRT 转发 DLL，就说明用了 UCRT（Win10+ 才自带）

    python3 tests/inspect_pe.py build/grades-win64.exe
"""

import struct
import sys

# Windows 7 自带、无需任何前置安装的系统 DLL
WIN7_SAFE_DLLS = {
    "kernel32.dll", "user32.dll", "advapi32.dll", "msvcrt.dll", "ntdll.dll",
    "shell32.dll", "gdi32.dll", "ole32.dll", "oleaut32.dll", "ws2_32.dll",
    "comdlg32.dll", "shlwapi.dll",
}

# UCRT（通用 C 运行时）。Windows 10/11 是系统自带；Windows 7 需要先安装
# 「Universal C Runtime 更新」（KB2999226，通常随 Windows Update 或
# Visual C++ 2015-2022 可再发行组件一起装上）。
UCRT_DLLS_PREFIX = "api-ms-win-crt-"
UCRT_EXACT = {"ucrtbase.dll"}

SUBSYSTEM_NAMES = {2: "Windows GUI", 3: "Windows 控制台", 9: "Windows CE GUI"}
MACHINE_NAMES = {0x014C: "i386 (32 位)", 0x8664: "x86_64 (64 位)", 0xAA64: "ARM64"}


def rva_to_offset(sections, rva):
    for name, vaddr, vsize, raw_ptr, raw_size in sections:
        if vaddr <= rva < vaddr + max(vsize, raw_size):
            return raw_ptr + (rva - vaddr)
    return None


def cstring(data, offset, limit=260):
    if offset is None or offset >= len(data):
        return None
    end = data.find(b"\0", offset, min(offset + limit, len(data)))
    if end < 0:
        return None
    return data[offset:end].decode("ascii", errors="replace")


def main():
    if len(sys.argv) < 2:
        print("用法: python3 tests/inspect_pe.py <exe>")
        return 2

    path = sys.argv[1]
    with open(path, "rb") as handle:
        data = handle.read()

    problems = []
    print(f"检查文件: {path}  ({len(data)} 字节)\n")

    # ---------- DOS 头 ----------
    if data[:2] != b"MZ":
        print("  ✗ 不是 PE 文件（缺少 MZ 头）")
        return 1
    pe_offset = struct.unpack_from("<I", data, 0x3C)[0]
    if data[pe_offset:pe_offset + 4] != b"PE\0\0":
        print("  ✗ 不是 PE 文件（缺少 PE 签名）")
        return 1
    print("  ✓ 合法的 PE 文件")

    # ---------- COFF 头 ----------
    machine, n_sections, _, _, _, opt_size, _ = struct.unpack_from("<HHIIIHH", data, pe_offset + 4)
    print(f"  ✓ 目标架构: {MACHINE_NAMES.get(machine, hex(machine))}")
    if machine != 0x8664:
        problems.append(f"架构不是 x86_64（{hex(machine)}）")

    opt_offset = pe_offset + 24
    magic = struct.unpack_from("<H", data, opt_offset)[0]
    if magic != 0x20B:
        print(f"  ✗ 期望 PE32+（0x20B），实际 {hex(magic)}")
        return 1
    print("  ✓ PE32+ 可执行格式")

    # ---------- 可选头 ----------
    major_os = struct.unpack_from("<H", data, opt_offset + 40)[0]
    minor_os = struct.unpack_from("<H", data, opt_offset + 42)[0]
    subsystem = struct.unpack_from("<H", data, opt_offset + 68)[0]
    major_sub = struct.unpack_from("<H", data, opt_offset + 48)[0]
    minor_sub = struct.unpack_from("<H", data, opt_offset + 50)[0]
    n_dirs = struct.unpack_from("<I", data, opt_offset + 108)[0]

    print(f"  {'✓' if subsystem == 3 else '✗'} 子系统: {SUBSYSTEM_NAMES.get(subsystem, subsystem)}"
          f"（{subsystem}）")
    if subsystem != 3:
        problems.append(f"子系统不是控制台（{subsystem}），双击可能没有窗口")

    sub_version = (major_sub, minor_sub)
    print(f"  {'✓' if sub_version <= (6, 1) else '✗'} 最低子系统版本: {major_sub}.{minor_sub}"
          f"  →  {'Windows 7 可运行' if sub_version <= (6, 1) else '⚠️ 高于 6.1，Windows 7 将拒绝启动'}")
    if sub_version > (6, 1):
        problems.append(f"最低子系统版本 {major_sub}.{minor_sub} 高于 6.1，Win7 无法启动")

    if major_os:
        print(f"  · 最低操作系统版本: {major_os}.{minor_os}（信息性字段）")

    # ---------- 节表 ----------
    sections = []
    sect_offset = opt_offset + opt_size
    for i in range(n_sections):
        base = sect_offset + i * 40
        name = data[base:base + 8].rstrip(b"\0").decode("ascii", errors="replace")
        vsize, vaddr, raw_size, raw_ptr = struct.unpack_from("<IIII", data, base + 8)
        sections.append((name, vaddr, vsize, raw_ptr, raw_size))
    print(f"  · 节区: {', '.join(s[0] for s in sections)}")

    # ---------- 导入表 ----------
    if n_dirs < 2:
        print("  ✗ 没有导入表")
        return 1
    # PE32+ 可选头的数据目录从偏移 112 开始：112=导出表，120=导入表
    import_rva, import_size = struct.unpack_from("<II", data, opt_offset + 120)
    dlls = []
    if import_rva and import_size:
        offset = rva_to_offset(sections, import_rva)
        if offset:
            for i in range(import_size // 20):
                entry = offset + i * 20
                name_rva = struct.unpack_from("<I", data, entry + 12)[0]
                if name_rva == 0:
                    break
                name = cstring(data, rva_to_offset(sections, name_rva))
                if name:
                    dlls.append(name)

    print(f"\n  依赖的系统 DLL（{len(dlls)} 个）:")
    risky = []
    uses_ucrt = False
    for dll in sorted({d.lower() for d in dlls}):
        if dll in WIN7_SAFE_DLLS:
            print(f"    ✓ {dll:<44} Win7 自带")
        elif dll.startswith(UCRT_DLLS_PREFIX) or dll in UCRT_EXACT:
            uses_ucrt = True
            print(f"    ⚠ {dll:<44} UCRT，Win7 需先装 KB2999226")
        else:
            risky.append(dll)
            print(f"    ✗ {dll:<44} 未知来源，需人工确认")

    if risky:
        problems.append("依赖了未知来源的 DLL: " + ", ".join(sorted(risky)))

    # ---------- 结论 ----------
    print()
    if problems:
        print("结论: 存在问题")
        for issue in problems:
            print(f"  ✗ {issue}")
        return 1

    if uses_ucrt:
        print("结论: 检查通过，但有一处部署前提需要知道")
        print("  · 这是面向 Windows 7 的 x86_64 控制台程序（PE 子系统版本 6.0 ≤ 6.1，Win7 不会拒绝启动）")
        print("  · Windows 10 / 11 上开箱即用")
        print("  · Windows 7 上需要先安装 UCRT 更新 KB2999226（一般随 Windows Update，")
        print("    或安装 Visual C++ 2015-2022 可再发行组件即可）")
        print("  · 若要求 Win7 上绝对零前置依赖，需在 Windows 上用 MSVCRT 运行时的")
        print("    MinGW-w64 重新编译（详见 c/README.md 的「Windows 7 兼容性」一节）")
        return 0

    print("结论: 全部检查通过 —— 面向 Windows 7 的 x86_64 控制台程序，且无额外运行时依赖")
    return 0


if __name__ == "__main__":
    sys.exit(main())
