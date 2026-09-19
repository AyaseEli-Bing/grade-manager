/**
 * 平台差异层 —— 把 Windows 与 POSIX 的差别收拢在这一个文件里。
 *
 * 之所以需要它：Win7 时代的 MSVCRT 没有 localtime_r / strdup / 原子重命名等，
 * 业务代码直接调用这些函数会在 Windows 上编译失败或行为不一致。
 */
#ifndef GM_COMPAT_H
#define GM_COMPAT_H

#include <stddef.h>
#include <time.h>

/** 本地时间拆解（POSIX localtime_r / Windows localtime_s） */
void gm_localtime(const time_t *when, struct tm *out);

/** UTC 时间拆解（POSIX gmtime_r / Windows gmtime_s） */
void gm_gmtime(const time_t *when, struct tm *out);

/** 复制字符串，内存不足返回 NULL */
char *gm_strdup(const char *text);

/** 递归创建目录，成功或已存在返回 0。mode 仅在 POSIX 下有意义 */
int gm_mkdirs(const char *path);

/** 用 src 原子替换 dest（Windows 走 MoveFileEx，POSIX 走 rename） */
int gm_replace_file(const char *src, const char *dest);

/** 应用数据目录下的数据文件默认路径；返回 0 表示成功 */
int gm_default_data_path(char *out, size_t outsz);

/** 创建目录（单层） */
int gm_mkdir(const char *path);

#endif /* GM_COMPAT_H */
