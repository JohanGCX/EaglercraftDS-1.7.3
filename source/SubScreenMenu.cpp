#include "SubScreenMenu.h"
#include <nds.h>
#include "menu_nosave.h"

static int bgId = -1;

// menu_nosave.png tiene 75 colores: va en 8 bits con paleta y comprimido (LZ77), unos 10 KB en memoria en vez de 96 KB.
// Se descomprime directo en la VRAM.
void SubScreenMenu::init(bool hasSave) {
    (void)hasSave;
    videoSetModeSub(MODE_5_2D | DISPLAY_BG3_ACTIVE);
    vramSetBankC(VRAM_C_SUB_BG);
    bgId = bgInitSub(3, BgType_Bmp8, BgSize_B8_256x256, 0, 0);

    dmaCopy(menu_nosavePal, BG_PALETTE_SUB, menu_nosavePalLen);
    decompress(menu_nosaveBitmap, bgGetGfxPtr(bgId), LZ77Vram);
}

// Botones de menu_nosave.png (x 51..204): Play game y 38..63, Skins 68..93, Texture Packs 98..123, Options 128..153
bool SubScreenMenu::isNewLoadTouched(int touchX, int touchY) {
    return touchX >= 51 && touchX <= 204 && touchY >= 38 && touchY <= 63;
}

bool SubScreenMenu::isOptionsTouched(int touchX, int touchY) {
    return touchX >= 51 && touchX <= 204 && touchY >= 128 && touchY <= 153;
}