/*
 * This firmware is under MIT License
 * Author: Abdullah Ali
 */
#include "keyboard.h"

static const char* kbRows[] = {
  "1234567890",
  "qwertyuiop",
  "asdfghjkl.",
  "zxcvbnm_+-",
  "^ @#*!?/<>"
};
static const char* kbRowsShift[] = {
  "!@#$%&/()=",
  "QWERTYUIOP",
  "ASDFGHJKL:",
  "ZXCVBNM_+-",
  "^ {|}~'`<>"
};
static const int KB_ROWS = 5;
static const int KB_COLS = 10;

static char kbResult[64];
static char kbPrompt[32];
static int kbCursorX = 0;
static int kbCursorY = 0;
static int kbInputPos = 0;
static int kbMaxLen = 32;
static bool kbShift = false;
static bool kbDone = false;
static bool kbConfirmed = false;
static bool kbIsPassword = false;

static const int KEY_W = 12;
static const int KEY_H = 8;
static const int KB_START_Y = 22;
static const int KB_OFFSET_X = 4;

void keyboard_init(const char* prompt, const char* initialValue, int maxLen, bool isPassword) {
  strncpy(kbPrompt, prompt, 31);
  kbPrompt[31] = '\0';
  strncpy(kbResult, initialValue, 63);
  kbResult[63] = '\0';
  kbInputPos = strlen(kbResult);
  kbMaxLen = (maxLen > 63) ? 63 : maxLen;
  kbCursorX = 0;
  kbCursorY = 0;
  kbShift = false;
  kbDone = false;
  kbConfirmed = false;
  kbIsPassword = isPassword;
}

bool keyboard_handleButton(ButtonID btn) {
  if (kbDone) return true;

  switch (btn) {
    case BTN_ID_UP:
      kbCursorY = (kbCursorY - 1 + KB_ROWS) % KB_ROWS;
      break;

    case BTN_ID_DOWN:
      kbCursorY = (kbCursorY + 1) % KB_ROWS;
      break;

    case BTN_ID_LEFT:
      kbCursorX = (kbCursorX - 1 + KB_COLS) % KB_COLS;
      break;

    case BTN_ID_RIGHT:
      kbCursorX = (kbCursorX + 1) % KB_COLS;
      break;

    case BTN_ID_SELECT: {
      const char* row = kbShift ? kbRowsShift[kbCursorY] : kbRows[kbCursorY];
      char ch = row[kbCursorX];

      if (ch == '<') {
        if (kbInputPos > 0) {
          kbInputPos--;
          kbResult[kbInputPos] = '\0';
        }
      } else if (ch == '>') {
        kbDone = true;
        kbConfirmed = true;
      } else if (ch == '^') {
        kbShift = !kbShift;
      } else if (ch == ' ' && kbCursorX == 1 && kbCursorY == 4) {
        if (kbInputPos < kbMaxLen) {
          kbResult[kbInputPos++] = ' ';
          kbResult[kbInputPos] = '\0';
        }
      } else {
        if (kbInputPos < kbMaxLen) {
          kbResult[kbInputPos++] = ch;
          kbResult[kbInputPos] = '\0';
        }
      }
      break;
    }

    default:
      break;
  }
  return kbDone;
}

void keyboard_draw(U8G2& u8g2) {
  u8g2.setFont(u8g2_font_4x6_tr);
  u8g2.drawStr(0, 6, kbPrompt);

  u8g2.drawFrame(0, 8, 128, 11);
  u8g2.setFont(u8g2_font_5x7_tr);

  if (kbIsPassword) {
    String masked = "";
    for (int i = 0; i < kbInputPos; i++) masked += '*';
    u8g2.drawStr(2, 17, masked.c_str());
    int cx = 2 + kbInputPos * 5;
    if (cx < 126 && (millis() / 500) % 2 == 0) {
      u8g2.drawStr(cx, 17, "_");
    }
  } else {
    int visibleChars = 24;
    int startChar = 0;
    if (kbInputPos > visibleChars) {
      startChar = kbInputPos - visibleChars;
    }
    u8g2.drawStr(2, 17, kbResult + startChar);
    int cx = 2 + (kbInputPos - startChar) * 5;
    if (cx < 126 && (millis() / 500) % 2 == 0) {
      u8g2.drawStr(cx, 17, "_");
    }
  }

  const char** rows = kbShift ? kbRowsShift : kbRows;

  for (int r = 0; r < KB_ROWS; r++) {
    int y = KB_START_Y + r * KEY_H;
    for (int c = 0; c < KB_COLS; c++) {
      int x = KB_OFFSET_X + c * KEY_W;
      char ch = rows[r][c];

      if (r == kbCursorY && c == kbCursorX) {
        u8g2.drawBox(x, y, KEY_W, KEY_H);
        u8g2.setDrawColor(0);
      } else {
        u8g2.drawFrame(x, y, KEY_W, KEY_H);
      }

      char label[4] = {0};
      if (ch == '<') {
        label[0] = '<'; label[1] = '-';
      } else if (ch == '>') {
        label[0] = 'O'; label[1] = 'K';
      } else if (ch == '^') {
        label[0] = 'S'; label[1] = 'H';
      } else if (ch == ' ' && c == 1 && r == 4) {
        label[0] = 'S'; label[1] = 'P';
      } else {
        label[0] = ch;
      }

      u8g2.setFont(u8g2_font_4x6_tr);
      int tw = strlen(label) * 4;
      u8g2.drawStr(x + (KEY_W - tw) / 2, y + 6, label);

      u8g2.setDrawColor(1);
    }
  }

  u8g2.setFont(u8g2_font_4x6_tr);
  if (kbShift) {
    u8g2.drawStr(0, 64, "SH");
  }
  char countBuf[8];
  snprintf(countBuf, 8, "%d/%d", kbInputPos, kbMaxLen);
  u8g2.drawStr(100, 64, countBuf);
}

const char* keyboard_getResult() {
  return kbResult;
}

bool keyboard_wasConfirmed() {
  return kbConfirmed;
}
