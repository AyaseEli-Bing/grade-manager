/**
 * 平台差异层的实现。Windows 分支刻意只使用 Win7 就有的 API：
 * GetEnvironmentVariableA / CreateDirectoryA / MoveFileExA。
 */
#include "compat.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#define GM_SEP '\\'
#else
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#define GM_SEP '/'
#endif

void gm_localtime(const time_t *when, struct tm *out) {
#ifdef _WIN32
  localtime_s(out, when);
#else
  localtime_r(when, out);
#endif
}

void gm_gmtime(const time_t *when, struct tm *out) {
#ifdef _WIN32
  gmtime_s(out, when);
#else
  gmtime_r(when, out);
#endif
}

char *gm_strdup(const char *text) {
  if (!text) text = "";
  size_t len = strlen(text) + 1;
  char *copy = malloc(len);
  if (copy) memcpy(copy, text, len);
  return copy;
}

int gm_mkdir(const char *path) {
#ifdef _WIN32
  return _mkdir(path);
#else
  return mkdir(path, 0700);
#endif
}

static int is_dir(const char *path) {
#ifdef _WIN32
  DWORD attr = GetFileAttributesA(path);
  return attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY);
#else
  struct stat info;
  return stat(path, &info) == 0 && S_ISDIR(info.st_mode);
#endif
}

int gm_mkdirs(const char *path) {
  if (!path || !path[0]) return -1;
  if (is_dir(path)) return 0;

  char work[1024];
  snprintf(work, sizeof(work), "%s", path);

  /* 逐段创建，兼容 / 与 \ 两种分隔符 */
  for (char *p = work + 1; *p; p++) {
    if (*p != '/' && *p != GM_SEP) continue;
    if (*p == '/' && GM_SEP == '\\') {
      *p = GM_SEP; /* 统一成当前平台的分隔符，避免混合分隔符在某些 API 下失败 */
    }
    char saved = *p;
    *p = '\0';
    if (work[0] && !is_dir(work)) gm_mkdir(work);
    *p = saved;
  }
  if (!is_dir(work)) gm_mkdir(work);
  return is_dir(work) ? 0 : -1;
}

int gm_replace_file(const char *src, const char *dest) {
#ifdef _WIN32
  /* Windows 的 rename 在目标已存在时会失败，必须用 MoveFileEx 才能原子替换 */
  if (MoveFileExA(src, dest, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) return 0;
  return -1;
#else
  return rename(src, dest);
#endif
}

int gm_default_data_path(char *out, size_t outsz) {
  if (!out || outsz == 0) return -1;
#ifdef _WIN32
  const char *appdata = getenv("APPDATA");
  if (appdata && appdata[0]) {
    snprintf(out, outsz, "%s\\cn.grade.manager\\grades.json", appdata);
    return 0;
  }
  snprintf(out, outsz, "grades.json");
  return 0;
#else
  const char *home = getenv("HOME");
  if (!home) home = ".";
#ifdef __APPLE__
  snprintf(out, outsz, "%s/Library/Application Support/cn.grade.manager/grades.json", home);
#else
  snprintf(out, outsz, "%s/.local/share/cn.grade.manager/grades.json", home);
#endif
  return 0;
#endif
}
