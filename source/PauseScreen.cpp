#include "PauseScreen.h"
#include <nds.h>
#include "pauseMenu.h"

static int bgId = -1;

// La imagen puede salir de grit de dos formas segun su .grit: 8 bits con paleta (existe pauseMenuPalLen) o 16 bits sin paleta.
// Este archivo vale para las dos: lo decide el preprocesador con lo que declare el .h generado.
void PauseScreen::show() {
    videoSetModeSub(MODE_5_2D | DISPLAY_BG3_ACTIVE);
    vramSetBankC(VRAM_C_SUB_BG);
#ifdef pauseMenuPalLen
    bgId = bgInitSub(3, BgType_Bmp8, BgSize_B8_256x256, 0, 0);
    dmaCopy(pauseMenuPal, BG_PALETTE_SUB, pauseMenuPalLen);
    decompress(pauseMenuBitmap, bgGetGfxPtr(bgId), LZ77Vram);
#else
    bgId = bgInitSub(3, BgType_Bmp16, BgSize_B16_256x256, 0, 0);
    if (pauseMenuBitmapLen == 256 * 192 * 2) dmaCopy(pauseMenuBitmap, bgGetGfxPtr(bgId), 256 * 192 * 2);   // sin comprimir
    else decompress(pauseMenuBitmap, bgGetGfxPtr(bgId), LZ77Vram);                                          // comprimida (-gzl)
    u16* pixels = (u16*)bgGetGfxPtr(bgId);
    for (int i = 0; i < 256 * 192; i++) pixels[i] |= BIT(15);
#endif
}

// Mismas posiciones que el menu principal (menu_nosave.png): x 51..204, y 38..63, 68..93, 98..123, 128..153
PauseScreen::Button PauseScreen::touched(int touchX, int touchY) {
    if (touchX < 51 || touchX > 204) return BTN_NONE;
    if (touchY >= 38  && touchY <= 63)  return BTN_BACK;
    if (touchY >= 68  && touchY <= 93)  return BTN_OPTIONS;
    if (touchY >= 98  && touchY <= 123) return BTN_SAVE;
    if (touchY >= 128 && touchY <= 153) return BTN_QUIT;
    return BTN_NONE;
}
