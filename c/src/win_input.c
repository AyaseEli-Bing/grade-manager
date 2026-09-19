/**
 * Win32 后端的键盘输入：把控制台输入记录翻译成 curses 风格的单字节/特殊键码。
 *
 * 中文输入的处理方式：控制台给出的是 UTF-16 码元，这里转成 UTF-8 字节后
 * 逐个返回，这样视图层那套「按字节累积到缓冲区」的逻辑无需任何改动即可工作。
 */
#include "win_curses.h"
#include "win_internal.h"

#ifdef _WIN32

#include <string.h>
#include <windows.h>

/* 一个多字节字符可能被拆成多次 getch 返回，这里做缓冲 */
static unsigned char pending[8];
static int pending_len = 0;
static int pending_at = 0;

/** 把 UTF-16 码元转成 UTF-8 存入 pending，返回首字节 */
static int queue_utf8(wchar_t ch) {
  pending_len = 0;
  pending_at = 0;

  if (ch < 0x80) {
    pending[pending_len++] = (unsigned char)ch;
  } else {
    if (!WideCharToMultiByte(CP_UTF8, 0, &ch, 1, (char *)pending, (int)sizeof(pending), NULL, NULL)) {
      /* 转换失败（例如孤立代理项）时丢弃，避免卡死 */
      return ERR;
    }
    pending_len = (int)strlen((const char *)pending);
  }
  if (pending_len <= 0) return ERR;
  pending_at = 1;
  return pending[0];
}

static int map_virtual_key(WORD vk) {
  switch (vk) {
    case VK_UP: return KEY_UP;
    case VK_DOWN: return KEY_DOWN;
    case VK_LEFT: return KEY_LEFT;
    case VK_RIGHT: return KEY_RIGHT;
    case VK_HOME: return KEY_HOME;
    case VK_END: return KEY_END;
    case VK_PRIOR: return KEY_PPAGE;
    case VK_NEXT: return KEY_NPAGE;
    case VK_BACK: return KEY_BACKSPACE;
    case VK_RETURN: return '\n';
    case VK_ESCAPE: return 27 /* Esc */;
    default: return -1;
  }
}

int wgetch(WINDOW *win) {
  (void)win;

  /* 先把上次没发完的多字节字符补完 */
  if (pending_at < pending_len) return pending[pending_at++];

  HANDLE input = GetStdHandle(STD_INPUT_HANDLE);
  if (input == INVALID_HANDLE_VALUE) return 'q'; /* 无输入设备：直接返回退出键，避免死循环 */

  for (;;) {
    INPUT_RECORD record;
    DWORD read = 0;
    if (!ReadConsoleInputW(input, &record, 1, &read) || read == 0) return ERR;

    if (record.EventType == WINDOW_BUFFER_SIZE_EVENT) {
      gm_win_refresh_size();
      return KEY_RESIZE;
    }
    if (record.EventType != KEY_EVENT) continue;

    const KEY_EVENT_RECORD *key = &record.Event.KeyEvent;
    if (!key->bKeyDown) continue;

    /* Ctrl+C：已关闭 ENABLE_PROCESSED_INPUT，需要自己当成退出处理 */
    if ((key->dwControlKeyState & (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED)) &&
        (key->wVirtualKeyCode == 'C' || key->wVirtualKeyCode == 'c')) {
      return 27 /* Esc */;
    }

    int mapped = map_virtual_key(key->wVirtualKeyCode);
    if (mapped >= 0) return mapped;

    /* 普通字符（含中文等宽字符） */
    if (key->uChar.UnicodeChar != 0) {
      int first = queue_utf8(key->uChar.UnicodeChar);
      if (first != ERR) return first;
    }
  }
}

int getch(void) { return wgetch(stdscr); }

#else
typedef int gm_win_input_unused;
#endif /* _WIN32 */
