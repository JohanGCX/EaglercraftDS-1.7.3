#include "TexturePackMenu.h"
#include "TexturePack.h"
#include "Skins.h"
#include "Sound.h"
#include <stdio.h>
#include <string.h>

namespace {
    const int MAX_PACKS = 24;
    const int COLUMNS = 32;
    const int VISIBLE = 9;

    TexturePack::Entry s_packs[MAX_PACKS];
    int s_count = 0;
    int s_sel = 0, s_top = 0;            // fila 0 = Default, fila i = s_packs[i - 1]
    const char* s_msg = "";
    bool s_skins = false;                // lista de skins en vez de texture packs

    const char* active() { return s_skins ? Skins::activeName() : TexturePack::activeName(); }

    int rows() { return s_count + 1; }

    bool isActive(int row) {
        const char* a = active();
        return row == 0 ? a[0] == 0 : strcmp(a, s_packs[row - 1].name) == 0;
    }

    // "->*Titulo": flecha en la fila elegida, * en el pack que se esta usando
    void printRow(int row) {
        char line[COLUMNS + 1];
        memset(line, ' ', COLUMNS);
        line[COLUMNS] = 0;
        if (row == s_sel) { line[0] = '-'; line[1] = '>'; }
        if (isActive(row)) line[2] = '*';
        const char* t = row == 0 ? "Default" : s_packs[row - 1].title;
        int n = (int)strlen(t);
        if (n > COLUMNS - 3) n = COLUMNS - 3;
        memcpy(line + 3, t, (size_t)n);
        iprintf("%s", line);   // 32 columnas: la consola salta sola a la linea siguiente
    }

    void draw() {
        if (s_sel < s_top) s_top = s_sel;
        if (s_sel >= s_top + VISIBLE) s_top = s_sel - VISIBLE + 1;
        if (s_top > rows() - VISIBLE) s_top = rows() - VISIBLE;
        if (s_top < 0) s_top = 0;
        consoleClear();
        iprintf("%s\n\n", s_skins ? "Skins" : "Texture packs");
        iprintf("%s\n", s_top > 0 ? "                              ^" : "");
        for (int r = s_top; r < s_top + VISIBLE && r < rows(); r++) printRow(r);
        iprintf("%s\n", s_top + VISIBLE < rows() ? "                              v" : "");
        if (s_sel == 0) iprintf(s_skins ? "Steve, the game's own skin\n" : "The game's own textures\n");
        else iprintf("%.31s\n", s_packs[s_sel - 1].name);
        if (s_count == 0) iprintf(s_skins ? "Put skins (.png, 64x32 or 64x64)\nin /mcdsefiles/skins/\n" : "Put packs (.zip) in\n/mcdsefiles/texturepacks/\n");
        if (s_msg[0]) iprintf("\n%s\n", s_msg);
        else iprintf("\nA: use   B: back\n");
    }
}

void TexturePackMenu::open(bool skins) {
    consoleDemoInit();
    s_skins = skins;
    s_count = skins ? Skins::scan(s_packs, MAX_PACKS) : TexturePack::scan(s_packs, MAX_PACKS);
    s_sel = 0;
    for (int r = 1; r < rows(); r++) if (isActive(r)) s_sel = r;
    s_top = 0;
    s_msg = "";
    draw();
}

bool TexturePackMenu::update(u32 keysDown) {
    if (keysDown & (KEY_A | KEY_B)) Sound::uiClick();
    if (keysDown & KEY_B) return true;
    if (keysDown & KEY_UP)   { s_sel = (s_sel + rows() - 1) % rows(); s_msg = ""; draw(); }
    if (keysDown & KEY_DOWN) { s_sel = (s_sel + 1) % rows(); s_msg = ""; draw(); }
    if ((keysDown & KEY_A) && !isActive(s_sel)) {
        consoleClear();
        iprintf("%s\n\nLoading...\n\nThe first time it is used\nit is converted for the DS.\nThis takes a few seconds.\n", s_skins ? "Skins" : "Texture packs");
        const char* name = s_sel == 0 ? "" : s_packs[s_sel - 1].name;
        const bool ok = s_skins ? Skins::apply(name) : TexturePack::apply(name);
        if (!ok) {
            static char err[64];
            snprintf(err, sizeof(err), "Could not load it:\n%s", s_skins ? Skins::lastError() : TexturePack::lastError());
            s_msg = err;
        } else s_msg = "";
        draw();
    }
    return false;
}
