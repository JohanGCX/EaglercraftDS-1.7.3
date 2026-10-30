#include "TextKeyboard.h"
#include "TopScreenMenu.h"
#include "Sound.h"
#include <nds.h>
#include <stdio.h>
#include <string.h>

namespace {
    enum Kind { K_CHAR = 0, K_SHIFT, K_BACK, K_SPACE, K_OK, K_CANCEL, K_CLEAR };
    struct Key { u8 col, row, w; char ch; u8 kind; };   // posicion en celdas de la consola (8 x 8 pixeles)

    Key s_keys[48];
    int s_keyCount = 0;
    bool s_shift = false;

    void addKey(int col, int row, int w, char ch, int kind) {
        Key k; k.col = (u8)col; k.row = (u8)row; k.w = (u8)w; k.ch = ch; k.kind = (u8)kind;
        s_keys[s_keyCount++] = k;
    }

    void build(bool numeric) {
        s_keyCount = 0;
        if (numeric) {
            const char* d = "123456789";
            for (int r = 0; r < 3; r++)
                for (int c = 0; c < 3; c++) addKey(7 + 6 * c, 6 + 3 * r, 5, d[r * 3 + c], K_CHAR);
            addKey(7, 15, 5, 0, K_BACK);
            addKey(13, 15, 5, '0', K_CHAR);
            addKey(19, 15, 5, 0, K_CLEAR);
        } else {
            const char* r0 = "1234567890";
            const char* r1 = "qwertyuiop";
            const char* r2 = "asdfghjkl";
            const char* r3 = "zxcvbnm";
            for (int i = 0; i < 10; i++) addKey(1 + 3 * i, 6, 3, r0[i], K_CHAR);
            for (int i = 0; i < 10; i++) addKey(1 + 3 * i, 9, 3, r1[i], K_CHAR);
            for (int i = 0; i < 9; i++)  addKey(2 + 3 * i, 12, 3, r2[i], K_CHAR);
            addKey(1, 15, 3, 0, K_SHIFT);
            for (int i = 0; i < 7; i++)  addKey(4 + 3 * i, 15, 3, r3[i], K_CHAR);
            addKey(25, 15, 3, 0, K_BACK);
            addKey(1, 18, 3, '-', K_CHAR);
            addKey(4, 18, 3, '_', K_CHAR);
            addKey(7, 18, 3, '.', K_CHAR);
            addKey(10, 18, 12, ' ', K_SPACE);
        }
        addKey(1, 21, 14, 0, K_OK);
        addKey(17, 21, 14, 0, K_CANCEL);
    }

    char keyChar(const Key& k) {
        if (s_shift && k.ch >= 'a' && k.ch <= 'z') return (char)(k.ch - 'a' + 'A');
        return k.ch;
    }

    void drawKey(const Key& k) {
        char label[16];
        switch (k.kind) {
            case K_SHIFT:  strcpy(label, "^"); break;
            case K_BACK:   strcpy(label, "<"); break;
            case K_SPACE:  strcpy(label, "space"); break;
            case K_OK:     strcpy(label, "OK"); break;
            case K_CANCEL: strcpy(label, "Cancel"); break;
            case K_CLEAR:  strcpy(label, "C"); break;
            default:       label[0] = keyChar(k); label[1] = 0; break;
        }
        char line[40];
        const int inner = k.w - 2, len = (int)strlen(label);
        const int pad = (inner - len) / 2;
        line[0] = '[';
        for (int i = 0; i < inner; i++) line[1 + i] = ' ';
        memcpy(line + 1 + pad, label, len);
        line[1 + inner] = ']';
        line[2 + inner] = 0;
        iprintf("\x1b[%d;%dH%s", k.row + 1, k.col + 1, line);
    }

    void drawText(const char* text) {
        char line[48];
        snprintf(line, sizeof(line), "> %s_", text);
        iprintf("\x1b[3;1H%-31s", line);
    }

    void drawAll(const char* title, const char* text) {
        consoleClear();
        iprintf("\x1b[1;1H%s", title);
        drawText(text);
        for (int i = 0; i < s_keyCount; i++) drawKey(s_keys[i]);
    }

    void finish(char* buf, const char* text, bool numeric) {
        strcpy(buf, text);
        if (!numeric) {   // sin espacios al final
            int n = (int)strlen(buf);
            while (n > 0 && buf[n - 1] == ' ') buf[--n] = 0;
        }
    }
}

bool TextKeyboard::edit(const char* title, char* buf, int maxLen, bool numeric, bool menuTick) {
    if (maxLen > 30) maxLen = 30;
    consoleDemoInit();
    char text[32];
    strncpy(text, buf, maxLen);
    text[maxLen] = 0;
    s_shift = false;
    build(numeric);
    drawAll(title, text);
    swiWaitForVBlank();

    while (pmMainLoop()) {
        if (menuTick) TopScreenMenu::tick();   // el panorama de arriba sigue girando
        scanKeys();
        const u32 down = keysDown();
        if (down & KEY_B) return false;
        if (down & KEY_START) { finish(buf, text, numeric); return true; }

        if (down & KEY_TOUCH) {
            touchPosition t;
            touchRead(&t);
            for (int i = 0; i < s_keyCount; i++) {
                const Key& k = s_keys[i];
                if (t.px < k.col * 8 || t.px >= (k.col + k.w) * 8 || t.py < (k.row - 1) * 8 || t.py >= (k.row + 2) * 8) continue;

                Sound::uiClick();
                const int len = (int)strlen(text);
                switch (k.kind) {
                    case K_OK:     finish(buf, text, numeric); return true;
                    case K_CANCEL: return false;
                    case K_SHIFT:  s_shift = !s_shift; for (int j = 0; j < s_keyCount; j++) drawKey(s_keys[j]); break;
                    case K_BACK:   if (len > 0) { text[len - 1] = 0; drawText(text); } break;
                    case K_CLEAR:  text[0] = 0; drawText(text); break;
                    case K_SPACE:  if (len > 0 && len < maxLen) { text[len] = ' '; text[len + 1] = 0; drawText(text); } break;
                    default:       if (len < maxLen) { text[len] = keyChar(k); text[len + 1] = 0; drawText(text); } break;
                }
                break;
            }
        }
        swiWaitForVBlank();
    }
    return false;
}
