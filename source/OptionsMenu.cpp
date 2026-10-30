#include "OptionsMenu.h"
#include "Options.h"
#include "TextKeyboard.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static int s_selected = 0;          // 0 = back, 1 = debug large chest, 2 = chunk mode, 3 = default seed (random/custom), 4 = custom default seed
static const int ROW_COUNT = 5;
static const int COLUMNS = 32;      // ancho de la consola

// "->  etiqueta                valor": flecha a la izquierda en la fila elegida, valor alineado a la derecha
static void printRow(bool selected, const char* label, const char* value) {
    char line[COLUMNS + 1];
    memset(line, ' ', COLUMNS);
    line[COLUMNS] = 0;
    if (selected) { line[0] = '-'; line[1] = '>'; }
    int labelLen = (int)strlen(label), valueLen = (int)strlen(value);
    if (labelLen > COLUMNS - 4 - valueLen) labelLen = COLUMNS - 4 - valueLen;
    memcpy(line + 3, label, labelLen);
    memcpy(line + COLUMNS - valueLen, value, valueLen);
    iprintf("%s\n", line);
}

static void draw() {
    consoleClear();
    iprintf("Options\n\n");
    printRow(s_selected == 0, "", "back");
    printRow(s_selected == 1, "debug large chest", Options::get().debugLargeChest ? "true" : "false");
    static const char* const MODE_NAMES[3] = { "fast", "ds", "dsi" };
    printRow(s_selected == 2, "chunk mode", MODE_NAMES[Options::get().chunkMode <= 2 ? Options::get().chunkMode : 1]);
    printRow(s_selected == 3, "default seed", Options::get().seedMode == 1 ? "custom" : "random");
    char seedText[16] = "";
    if (Options::get().customSeedSet) snprintf(seedText, sizeof(seedText), "%u", Options::get().customSeed);
    printRow(s_selected == 4, "custom default seed", seedText);
}

void OptionsMenu::open() {
    s_selected = 0;
    consoleDemoInit();   // consola de la pantalla de abajo
    draw();
}

bool OptionsMenu::update(u32 keysDown) {
    if (keysDown & KEY_B) return true;

    if (keysDown & KEY_UP)   { s_selected = (s_selected + ROW_COUNT - 1) % ROW_COUNT; draw(); }
    if (keysDown & KEY_DOWN) { s_selected = (s_selected + 1) % ROW_COUNT; draw(); }

    if (keysDown & (KEY_A | KEY_LEFT | KEY_RIGHT)) {
        if (s_selected == 0) {
            if (keysDown & KEY_A) return true;
        } else if (s_selected == 1) {
            Options::get().debugLargeChest = !Options::get().debugLargeChest;
            Options::save();
            draw();
        } else if (s_selected == 3) {
            Options::get().seedMode = Options::get().seedMode == 1 ? 0 : 1;
            Options::save();
            draw();
        } else if (s_selected == 4) {
            if (keysDown & KEY_A) {   // A abre el teclado numerico; vacio = sin seed custom
                char buf[12] = "";
                if (Options::get().customSeedSet) snprintf(buf, sizeof(buf), "%u", Options::get().customSeed);
                if (TextKeyboard::edit("Custom default seed", buf, 9, true)) {
                    Options::get().customSeedSet = buf[0] != 0;
                    Options::get().customSeed = buf[0] ? (unsigned int)strtoul(buf, nullptr, 10) : 0;
                    Options::save();
                }
                consoleDemoInit();   // de vuelta a la lista
                draw();
            }
        } else {
            // fast -> ds -> dsi (dsi solo en una DSi); con IZQUIERDA se retrocede. Vale desde el proximo mundo que se abra.
            const int top = isDSiMode() ? 2 : 1;
            int m = Options::get().chunkMode;
            if (m > top) m = top;
            if (keysDown & KEY_LEFT) m = (m + top) % (top + 1);
            else                     m = (m + 1) % (top + 1);
            Options::get().chunkMode = (unsigned char)m;
            Options::save();
            draw();
        }
    }
    return false;
}