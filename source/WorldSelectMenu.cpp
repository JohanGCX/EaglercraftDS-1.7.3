#include "WorldSelectMenu.h"
#include "WorldSave.h"
#include <nds.h>
#include <string.h>
#include "selectWorld.h"
#include "selectWorldNoWorld.h"
#include "MenuFont.h"
#include <stdio.h>

static int bgId = -1;
static int worldList[100];
static int worldCount = 0;
static int selectedIndex = 0;

// Recuadro negro donde va el nombre, medido sobre la imagen real
static const int LABEL_X0 = 65, LABEL_X1 = 191;
static const int LABEL_Y0 = 108, LABEL_Y1 = 145;

static void clearLabelZone(u16* fb) {
    u16 black = RGB15(0, 0, 0) | BIT(15);
    for (int y = LABEL_Y0; y <= LABEL_Y1; y++) {
        for (int x = LABEL_X0; x <= LABEL_X1; x++) {
            fb[y * 256 + x] = black;
        }
    }
}

static void drawWorldName(int worldNum) {
    if (bgId < 0) return;
    if (worldNum < 0) return; // sin mundos: no hay recuadro que pintar en este fondo

    u16* fb = (u16*)bgGetGfxPtr(bgId);
    clearLabelZone(fb);

    // Nombre propio del mundo (name.dat) o "World N". Hasta 12 letras al tamano de siempre (escala 2); mas largo se achica para caber.
    char name[24];
    if (!WorldSave::readWorldName(worldNum, name, sizeof(name))) snprintf(name, sizeof(name), "World %d", worldNum);

    const int scale = MenuFont::scaleFor((int)strlen(name));
    const int textWidth = MenuFont::width(name, scale);
    const int startX = LABEL_X0 + ((LABEL_X1 - LABEL_X0 + 1) - textWidth) / 2;
    const int startY = LABEL_Y0 + ((LABEL_Y1 - LABEL_Y0 + 1) - 6 * scale) / 2;
    MenuFont::draw16(fb, name, startX, startY, scale, RGB15(31, 31, 31) | BIT(15));
}

static void rebuildWorldList() {
    worldCount = 0;
    for (int n = 1; n <= 100 && worldCount < 100; n++) {
        if (WorldSave::worldSlotExists(n)) {
            worldList[worldCount++] = n;
        }
    }
    if (selectedIndex >= worldCount) selectedIndex = worldCount > 0 ? worldCount - 1 : 0;
}

void WorldSelectMenu::init() {
    videoSetModeSub(MODE_5_2D | DISPLAY_BG3_ACTIVE);
    vramSetBankC(VRAM_C_SUB_BG);
    bgId = bgInitSub(3, BgType_Bmp16, BgSize_B16_256x256, 0, 0);

    rebuildWorldList();
    selectedIndex = 0;

    const void* bitmap = (worldCount > 0) ? (const void*)selectWorldBitmap : (const void*)selectWorldNoWorldBitmap;
    decompress(bitmap, bgGetGfxPtr(bgId), LZ77Vram);

    u16* pixels = (u16*)bgGetGfxPtr(bgId);
    for (int i = 0; i < 256 * 192; i++) {
        pixels[i] |= BIT(15);
    }

    drawWorldName(worldCount > 0 ? worldList[0] : -1);
}

void WorldSelectMenu::refresh() {
    init();
}

void WorldSelectMenu::selectPrevious() {
    if (worldCount == 0) return;
    selectedIndex = (selectedIndex - 1 + worldCount) % worldCount;
    drawWorldName(worldList[selectedIndex]);
}

void WorldSelectMenu::selectNext() {
    if (worldCount == 0) return;
    selectedIndex = (selectedIndex + 1) % worldCount;
    drawWorldName(worldList[selectedIndex]);
}

int WorldSelectMenu::getSelectedWorldNum() {
    if (worldCount == 0) return -1;
    return worldList[selectedIndex];
}

int WorldSelectMenu::getWorldCount() {
    return worldCount;
}

bool WorldSelectMenu::isCreateTouched(int touchX, int touchY) {
    return touchX >= 0 && touchX <= 62 && touchY >= 162 && touchY <= 190;
}

bool WorldSelectMenu::isDeleteTouched(int touchX, int touchY) {
    return touchX >= 66 && touchX <= 125 && touchY >= 162 && touchY <= 190;
}

bool WorldSelectMenu::isPlayTouched(int touchX, int touchY) {
    return touchX >= 130 && touchX <= 189 && touchY >= 162 && touchY <= 190;
}

bool WorldSelectMenu::isBackTouched(int touchX, int touchY) {
    return touchX >= 194 && touchX <= 253 && touchY >= 162 && touchY <= 190;
}

bool WorldSelectMenu::isLeftArrowTouched(int touchX, int touchY) {
    return touchX >= 0 && touchX <= 64 && touchY >= 51 && touchY <= 113;
}

bool WorldSelectMenu::isRightArrowTouched(int touchX, int touchY) {
    return touchX >= 192 && touchX <= 255 && touchY >= 51 && touchY <= 113;
}