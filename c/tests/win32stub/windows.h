/**
 * 【仅用于本机类型自检，不是可用的 Windows 头文件】
 *
 * 本机没有 Windows SDK，也没有 MinGW 交叉工具链，为了让 win_*.c 能在 macOS 上
 * 被 clang 编译一遍（抓语法、类型、结构体字段、参数顺序之类的错误），这里按
 * 微软官方文档手写了用到的那部分 Win32 声明。
 *
 * 局限必须说清楚：本文件由我手写，如果我把某个函数签名写错了，自检会"通过"但
 * 真实 Windows 下编译失败。因此它只能作为第一道筛子，**不能替代在 Windows 上的真实编译**。
 * 真正的验证命令见 c/README.md 的「Windows 构建」一节。
 */
#ifndef GM_WIN32_SELFCHECK_STUB_H
#define GM_WIN32_SELFCHECK_STUB_H

#include <stddef.h>
#include <wchar.h>

typedef void *HANDLE;
typedef void *HWND;
typedef unsigned long DWORD;
typedef int BOOL;
typedef unsigned short WORD;
typedef unsigned char BYTE;
typedef short SHORT;
typedef long LONG;
typedef unsigned int UINT;
typedef wchar_t WCHAR;

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

/* ---------- 句柄与常量 ---------- */
#define STD_INPUT_HANDLE ((DWORD)-10)
#define STD_OUTPUT_HANDLE ((DWORD)-11)
#define STD_ERROR_HANDLE ((DWORD)-12)
#define INVALID_HANDLE_VALUE ((HANDLE)(long)-1)

#define ENABLE_PROCESSED_INPUT 0x0001
#define ENABLE_WINDOW_INPUT 0x0008
#define ENABLE_MOUSE_INPUT 0x0010
#define ENABLE_QUICK_EDIT_MODE 0x0040
#define ENABLE_EXTENDED_FLAGS 0x0080
#define ENABLE_PROCESSED_OUTPUT 0x0001
#define ENABLE_WRAP_AT_EOL_OUTPUT 0x0002
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004

#define FOREGROUND_BLUE 0x0001
#define FOREGROUND_GREEN 0x0002
#define FOREGROUND_RED 0x0004
#define FOREGROUND_INTENSITY 0x0008
#define BACKGROUND_BLUE 0x0010
#define BACKGROUND_GREEN 0x0020
#define BACKGROUND_RED 0x0040
#define BACKGROUND_INTENSITY 0x0080
#define COMMON_LVB_UNDERSCORE 0x8000
#define COMMON_LVB_REVERSE_VIDEO 0x4000

#define FILE_ATTRIBUTE_DIRECTORY 0x0010
#define INVALID_FILE_ATTRIBUTES ((DWORD)-1)
#define MOVEFILE_REPLACE_EXISTING 0x00000001
#define MOVEFILE_WRITE_THROUGH 0x00000008

#define CP_UTF8 65001

#define KEY_EVENT 0x0001
#define WINDOW_BUFFER_SIZE_EVENT 0x0004

#define LEFT_CTRL_PRESSED 0x0008
#define RIGHT_CTRL_PRESSED 0x0004

#define VK_BACK 0x08
#define VK_RETURN 0x0D
#define VK_ESCAPE 0x1B
#define VK_PRIOR 0x21
#define VK_NEXT 0x22
#define VK_END 0x23
#define VK_HOME 0x24
#define VK_LEFT 0x25
#define VK_UP 0x26
#define VK_RIGHT 0x27
#define VK_DOWN 0x28

/* ---------- 结构体 ---------- */
typedef struct {
  SHORT X;
  SHORT Y;
} COORD;

typedef struct {
  SHORT Left;
  SHORT Top;
  SHORT Right;
  SHORT Bottom;
} SMALL_RECT;

typedef struct {
  union {
    WCHAR UnicodeChar;
    char AsciiChar;
  } Char;
  WORD Attributes;
} CHAR_INFO;

typedef struct {
  COORD dwSize;
  COORD dwCursorPosition;
  WORD wAttributes;
  SMALL_RECT srWindow;
  COORD dwMaximumWindowSize;
} CONSOLE_SCREEN_BUFFER_INFO;

typedef struct {
  DWORD dwSize;
  BOOL bVisible;
} CONSOLE_CURSOR_INFO;

typedef struct {
  BOOL bKeyDown;
  WORD wRepeatCount;
  WORD wVirtualKeyCode;
  WORD wVirtualScanCode;
  union {
    WCHAR UnicodeChar;
    char AsciiChar;
  } uChar;
  DWORD dwControlKeyState;
} KEY_EVENT_RECORD;

typedef struct {
  COORD dwSize;
} WINDOW_BUFFER_SIZE_RECORD;

/* 真实 windows.h 里事件是 Event 联合体的成员，这里保持一致 */
typedef struct {
  WORD EventType;
  union {
    KEY_EVENT_RECORD KeyEvent;
    WINDOW_BUFFER_SIZE_RECORD WindowBufferSizeEvent;
  } Event;
} INPUT_RECORD;

/* ---------- 控制台 API ---------- */
HANDLE GetStdHandle(DWORD std_handle);
BOOL GetConsoleMode(HANDLE handle, DWORD *mode);
BOOL SetConsoleMode(HANDLE handle, DWORD mode);
BOOL GetConsoleScreenBufferInfo(HANDLE handle, CONSOLE_SCREEN_BUFFER_INFO *info);
BOOL WriteConsoleOutputW(HANDLE handle, const CHAR_INFO *buffer, COORD buffer_size, COORD buffer_coord,
                         SMALL_RECT *region);
BOOL ReadConsoleInputW(HANDLE handle, INPUT_RECORD *buffer, DWORD length, DWORD *read);
BOOL GetConsoleCursorInfo(HANDLE handle, CONSOLE_CURSOR_INFO *info);
BOOL SetConsoleCursorInfo(HANDLE handle, const CONSOLE_CURSOR_INFO *info);
BOOL SetConsoleCursorPosition(HANDLE handle, COORD position);
BOOL SetConsoleTitleA(const char *title);
BOOL SetConsoleOutputCP(UINT codepage);
UINT GetConsoleOutputCP(void);

int WideCharToMultiByte(UINT codepage, DWORD flags, const wchar_t *wide, int wide_len, char *multi,
                        int multi_len, const char *default_char, BOOL *used_default);

/* ---------- 文件与时间 ---------- */
DWORD GetFileAttributesA(const char *path);
BOOL MoveFileExA(const char *from, const char *to, DWORD flags);

struct tm;
int localtime_s(struct tm *out, const long *when);
int gmtime_s(struct tm *out, const long *when);

#endif /* GM_WIN32_SELFCHECK_STUB_H */
