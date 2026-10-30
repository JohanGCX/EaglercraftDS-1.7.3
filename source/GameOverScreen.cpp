#include "GameOverScreen.h"
#include <nds.h>
#include "gameover.h"

static int bgId = -1;

// gameover.png tiene 58 colores: va en 8 bits con paleta y comprimido (LZ77), unos 10 KB en memoria en vez de los 96 KB del bitmap de 16 bits.
// Se descomprime directo en la VRAM; la imagen no queda en la RAM.
void GameOverScreen::show() {
    videoSetModeSub(MODE_5_2D | DISPLAY_BG3_ACTIVE);
    vramSetBankC(VRAM_C_SUB_BG);
    bgId = bgInitSub(3, BgType_Bmp8, BgSize_B8_256x256, 0, 0);

    dmaCopy(gameoverPal, BG_PALETTE_SUB, gameoverPalLen);
    decompress(gameoverBitmap, bgGetGfxPtr(bgId), LZ77Vram);
}

// Botones medidos en gameover.png: Respawn en y 83..104 y Title menu en y 129..150, ambos de x 51 a 202
bool GameOverScreen::isRespawnTouched(int touchX, int touchY) {
    return touchX >= 51 && touchX <= 202 && touchY >= 83 && touchY <= 104;
}

bool GameOverScreen::isTitleTouched(int touchX, int touchY) {
    return touchX >= 51 && touchX <= 202 && touchY >= 129 && touchY <= 150;
}