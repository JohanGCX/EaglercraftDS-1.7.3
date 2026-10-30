#include "NewWorldMenu.h"
#include "TextKeyboard.h"
#include "Sound.h"
#include "MenuFont.h"
#include <nds.h>
#include <stdlib.h>
#include <string.h>
#include "createWorldMenu.h"

static int bgId = -1;
static char s_name[NewWorldMenu::MAX_NAME_LEN + 1];
static char s_seed[NewWorldMenu::MAX_SEED_DIGITS + 1];

// Recuadros negros de createWorldMenu.png (medidos sobre la imagen) y botones de abajo
static const int NAME_X0 = 66, NAME_X1 = 189, NAME_Y0 = 48, NAME_Y1 = 73;
static const int SEED_X0 = 66, SEED_X1 = 189, SEED_Y0 = 105, SEED_Y1 = 130;
static const int BTN_Y0 = 162, BTN_Y1 = 190;

// La imagen puede salir de grit de dos formas segun su .grit: 8 bits con paleta (existe createWorldMenuPalLen) o 16 bits sin paleta
// (como selectWorld). Este archivo vale para las dos: lo decide el preprocesador con lo que declare el .h generado.
#ifdef createWorldMenuPalLen
#define NWM_PALETTED 1
#else
#define NWM_PALETTED 0
#endif

#if NWM_PALETTED
static u8 s_white = 255, s_black = 0;

// Indice de la paleta mas cercano a un color (la paleta de la imagen se arma con grit, sus indices no son fijos)
static u8 nearestIndex(int r, int g, int b) {
    const u16* pal = (const u16*)createWorldMenuPal;
    const int n = (int)(createWorldMenuPalLen / 2);
    int best = 0, bestD = 0x7FFFFFFF;
    for (int i = 0; i < n; i++) {
        const int pr = pal[i] & 31, pg = (pal[i] >> 5) & 31, pb = (pal[i] >> 10) & 31;
        const int d = (pr - r) * (pr - r) + (pg - g) * (pg - g) + (pb - b) * (pb - b);
        if (d < bestD) { bestD = d; best = i; }
    }
    return (u8)best;
}
#endif

static void drawField(int x0, int x1, int y0, int y1, const char* text) {
    u16* vram = (u16*)bgGetGfxPtr(bgId);
    const int scale = text[0] ? MenuFont::scaleFor((int)strlen(text)) : 2;
    const int ty = y0 + ((y1 - y0 + 1) - 6 * scale) / 2;
#if NWM_PALETTED
    MenuFont::fill8(vram, x0, y0, x1, y1, s_black);
    if (text[0]) MenuFont::draw8(vram, text, x0 + 3, ty, scale, s_white);
#else
    MenuFont::fill16(vram, x0, y0, x1, y1, RGB15(0, 0, 0) | BIT(15));
    if (text[0]) MenuFont::draw16(vram, text, x0 + 3, ty, scale, RGB15(31, 31, 31) | BIT(15));
#endif
}

static void show() {
    videoSetModeSub(MODE_5_2D | DISPLAY_BG3_ACTIVE);
    vramSetBankC(VRAM_C_SUB_BG);
#if NWM_PALETTED
    bgId = bgInitSub(3, BgType_Bmp8, BgSize_B8_256x256, 0, 0);
    dmaCopy(createWorldMenuPal, BG_PALETTE_SUB, createWorldMenuPalLen);
    decompress(createWorldMenuBitmap, bgGetGfxPtr(bgId), LZ77Vram);
    s_white = nearestIndex(31, 31, 31);
    s_black = nearestIndex(0, 0, 0);
#else
    bgId = bgInitSub(3, BgType_Bmp16, BgSize_B16_256x256, 0, 0);
    if (createWorldMenuBitmapLen == 256 * 192 * 2) dmaCopy(createWorldMenuBitmap, bgGetGfxPtr(bgId), 256 * 192 * 2);   // sin comprimir
    else decompress(createWorldMenuBitmap, bgGetGfxPtr(bgId), LZ77Vram);                                              // comprimida (-gzl)
    u16* pixels = (u16*)bgGetGfxPtr(bgId);
    for (int i = 0; i < 256 * 192; i++) pixels[i] |= BIT(15);   // como la seleccion de mundo: el bit 15 hace opacos los pixeles
#endif
    drawField(NAME_X0, NAME_X1, NAME_Y0, NAME_Y1, s_name);
    drawField(SEED_X0, SEED_X1, SEED_Y0, SEED_Y1, s_seed);
}

void NewWorldMenu::open() {
    s_name[0] = 0;
    s_seed[0] = 0;
    show();
}

NewWorldMenu::Result NewWorldMenu::update(unsigned int keysDown) {
    if (keysDown & KEY_B) { Sound::uiClick(); return RES_CANCEL; }
    if (keysDown & KEY_START) { Sound::uiClick(); return RES_CREATE; }
    if (!(keysDown & KEY_TOUCH)) return RES_NONE;

    touchPosition t;
    touchRead(&t);

    if (t.py >= BTN_Y0 && t.py <= BTN_Y1) Sound::uiClick();
    if (t.py >= BTN_Y0 && t.py <= BTN_Y1) return (t.px < 128) ? RES_CREATE : RES_CANCEL;

    if (t.px >= NAME_X0 && t.px <= NAME_X1 && t.py >= NAME_Y0 - 8 && t.py <= NAME_Y1 + 3) {
        Sound::uiClick();
        TextKeyboard::edit("World name", s_name, MAX_NAME_LEN, false);
        show();
    } else if (t.px >= SEED_X0 && t.px <= SEED_X1 && t.py >= SEED_Y0 - 8 && t.py <= SEED_Y1 + 3) {
        Sound::uiClick();
        TextKeyboard::edit("Seed", s_seed, MAX_SEED_DIGITS, true);
        show();
    }
    return RES_NONE;
}

const char* NewWorldMenu::getName() { return s_name; }

bool NewWorldMenu::getSeed(unsigned int& outSeed) {
    if (!s_seed[0]) return false;
    outSeed = (unsigned int)strtoul(s_seed, nullptr, 10);
    return true;
}
