#include "StartupSplash.h"
#include <nds.h>
#include "eagteklogo.h"
#include "mojanglogo.h"

namespace {
    const int LOGO_FRAMES = 60;   // 1 segundo por logo, cambio de golpe

    void showLogo(const unsigned int* bitmap, const unsigned short* pal, unsigned int palLen, bool hold) {
        swiWaitForVBlank();   // la imagen cambia al empezar el cuadro, sin cortes a medias
        videoSetMode(MODE_5_2D | DISPLAY_BG3_ACTIVE);
        vramSetBankB(VRAM_B_MAIN_BG_0x06000000);
        const int id = bgInit(3, BgType_Bmp8, BgSize_B8_256x256, 0, 0);
        dmaCopy(pal, BG_PALETTE, palLen);
        decompress(bitmap, bgGetGfxPtr(id), LZ77Vram);   // directo a la VRAM: la imagen no queda en la RAM
        if (hold) for (int f = 0; f < LOGO_FRAMES; f++) swiWaitForVBlank();
    }
}

void StartupSplash::show() {
    // pantalla de abajo: sin capas, solo el color de fondo (blanco)
    videoSetModeSub(MODE_0_2D);
    setBackdropColorSub(RGB15(31, 31, 31));

    showLogo((const unsigned int*)eagteklogoBitmap, eagteklogoPal, eagteklogoPalLen, true);
    showLogo((const unsigned int*)mojanglogoBitmap, mojanglogoPal, mojanglogoPalLen, true);
    // sin restaurar nada: el logo de Mojang y la pantalla blanca siguen ahi mientras se inicia libfat y se cargan las texturas
}

void StartupSplash::finish() {
    setBackdropColorSub(0);
    consoleDemoInit();   // la consola de abajo como estaba antes del splash
}
