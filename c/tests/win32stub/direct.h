/**
 * Win32 自检桩：直接 I/O 头文件。真正的 <direct.h> 提供 _mkdir。
 */
#ifndef GM_WIN32_SELFCHECK_DIRECT_H
#define GM_WIN32_SELFCHECK_DIRECT_H

int _mkdir(const char *path);

#endif
