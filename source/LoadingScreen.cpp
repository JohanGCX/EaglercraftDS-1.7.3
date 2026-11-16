#include "LoadingScreen.h"
#include <nds.h>
#include "genericbg.h"
#include "topgenericbg.h"

static void copyBitmapAndForceOpaque(int bgId, const void* bitmapData) {
    dmaCopy(bitmapData, bgGetGfxPtr(bgId), 256 * 192 * 2);

    u16* pixels = (u16*)bgGetGfxPtr(bgId);
    for (int i = 0; i < 256 * 192; i++) {
        pixels[i] |= BIT(15);
    }
}

// topgenericbg va comprimido (-gzl): se descomprime directo a la VRAM
static void decompressTopOpaque(int bgId) {
    decompress((const void*)topgenericbgBitmap, bgGetGfxPtr(bgId), LZ77Vram);
    u16* pixels = (u16*)bgGetGfxPtr(bgId);
    for (int i = 0; i < 256 * 192; i++) pixels[i] |= BIT(15);
}

void LoadingScreen::show() {
    // Pantalla superior: fondo genérico (antes mclogo)
    videoSetMode(MODE_5_2D | DISPLAY_BG3_ACTIVE);
    vramSetBankB(VRAM_B_MAIN_BG_0x06000000);
    BG_PALETTE[0] = 0;   // fondo (backdrop) negro: mientras se escribe la imagen se ve negro, no el magenta del color 0 de otra paleta
    int bgIdTop = bgInit(3, BgType_Bmp16, BgSize_B16_256x256, 0, 0);
    decompressTopOpaque(bgIdTop);

    // Pantalla inferior: fondo genérico
    videoSetModeSub(MODE_5_2D | DISPLAY_BG3_ACTIVE);
    vramSetBankC(VRAM_C_SUB_BG);
    BG_PALETTE_SUB[0] = 0;   // fondo (backdrop) negro: mientras se escribe la imagen se ve negro, no el magenta del color 0 de otra paleta
    int bgIdSub = bgInitSub(3, BgType_Bmp16, BgSize_B16_256x256, 0, 0);
    copyBitmapAndForceOpaque(bgIdSub, (const void*)genericbgBitmap);
}

void LoadingScreen::showTopDirtOnly() {
    videoSetMode(MODE_5_2D | DISPLAY_BG3_ACTIVE);
    vramSetBankB(VRAM_B_MAIN_BG_0x06000000);
    BG_PALETTE[0] = 0;   // fondo (backdrop) negro: mientras se escribe la imagen se ve negro, no el magenta del color 0 de otra paleta
    int bgIdTop = bgInit(3, BgType_Bmp16, BgSize_B16_256x256, 0, 0);
    decompressTopOpaque(bgIdTop); // antes: genericbgBitmap
}