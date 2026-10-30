#include "TopScreenMenu.h"
#include <nds.h>
#include "topgenericbg.h"
#include "mclogomenu.h"   // el logo con transparencia (índice 0), para ponerlo encima del panorama
#include "MenuPanorama.h"
#include "Renderer.h"
#include "Texture.h"

static int bgId = -1;
static bool s_panorama = false;      // el panorama está en pantalla
static bool s_panoramaFailed = false; // no hubo VRAM: se queda el logo estático hasta la próxima vez que se entre al menú
static float s_ticks = 0.0f;         // tiempo del panorama en ticks de juego (20 por segundo); sigue donde quedó al volver al menú

void TopScreenMenu::init() {
    if (s_panorama) return; // el panorama sigue en pantalla (menú, opciones, selección de mundo): no se vuelve al logo estático

    videoSetMode(MODE_5_2D | DISPLAY_BG3_ACTIVE);
    vramSetBankB(VRAM_B_MAIN_BG_0x06000000); // antes VRAM_B_MAIN_BG (offset incorrecto)
    bgId = bgInit(3, BgType_Bmp16, BgSize_B16_256x256, 0, 0);

    decompress(topgenericbgBitmap, bgGetGfxPtr(bgId), LZ77Vram);

    u16* pixels = (u16*)bgGetGfxPtr(bgId);
    for (int i = 0; i < 256 * 192; i++) {
        pixels[i] |= BIT(15);
    }
}

static bool startPanorama() {
    Texture::unloadEntityTextures();   // sin esto, el panorama (32 KB seguidos) ya no cabe en la VRAM
    if (!MenuPanorama::load()) return false;

    // 3D en la capa 0 y el logo (8 bits, índice 0 transparente) en la capa 3, por delante
    videoSetMode(MODE_5_3D | DISPLAY_BG3_ACTIVE);
    vramSetBankB(VRAM_B_MAIN_BG_0x06000000);
    int id = bgInit(3, BgType_Bmp8, BgSize_B8_256x256, 0, 0);
    bgSetPriority(id, 0);
    REG_BG0CNT = BG_PRIORITY(1);

    dmaCopy(mclogomenuBitmap, bgGetGfxPtr(id), mclogomenuBitmapLen);
    dmaCopy(mclogomenuPal, BG_PALETTE, mclogomenuPalLen);
    return true;
}

void TopScreenMenu::tick() {
    if (!s_panorama) {
        if (s_panoramaFailed) return;
        if (!startPanorama()) { s_panoramaFailed = true; return; }
        s_panorama = true;
    }

    MenuPanorama::draw(s_ticks);
    glFlush(0);
    s_ticks += 1.0f / 3.0f;   // 60 cuadros por segundo -> 20 ticks por segundo, como el original
}

void TopScreenMenu::suspend() {
    s_panoramaFailed = false;
    if (!s_panorama) { Texture::reloadEntityTextures(); return; }

    s_panorama = false;
    MenuPanorama::unload();
    Texture::reloadEntityTextures();
    Renderer renderer;           // sin datos propios: sirve para repetir lo que main.cpp hizo al arrancar
    renderer.setupProjection();  // el juego espera su proyección y su formato de polígono de siempre
    init();
}