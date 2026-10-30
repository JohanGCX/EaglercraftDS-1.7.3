#include "GameplayHotbar.h"
#include <nds.h>
#include "ingamesubscreen.h"
#include "InventoryScreen.h"
#include "CraftingScreen.h"

int GameplayHotbar::bgId = -1;

void GameplayHotbar::show() {
    if (bgId == -1) {
        videoSetModeSub(MODE_5_2D | DISPLAY_BG3_ACTIVE);
        vramSetBankC(VRAM_C_SUB_BG);
        bgId = bgInitSub(3, BgType_Bmp16, BgSize_B16_256x256, 0, 0);
    }

    decompress((const void*)ingamesubscreenBitmap, bgGetGfxPtr(bgId), LZ77Vram);

    u16* pixels = (u16*)bgGetGfxPtr(bgId);
    for (int i = 0; i < 256 * 192; i++) {
        pixels[i] |= BIT(15);
    }
}

static bool isInButtonRow(int touchX, int touchY, int buttonIndex) {
    int left = buttonIndex * 64;
    int right = left + 63;
    return touchX >= left && touchX <= right && touchY >= 162 && touchY <= 191;
}

bool GameplayHotbar::isButton1Touched(int touchX, int touchY) {
    return isInButtonRow(touchX, touchY, 0);
}

bool GameplayHotbar::isButton2Touched(int touchX, int touchY) {
    return isInButtonRow(touchX, touchY, 1);
}

bool GameplayHotbar::isButton3Touched(int touchX, int touchY) {
    return isInButtonRow(touchX, touchY, 2);
}

bool GameplayHotbar::isButton4Touched(int touchX, int touchY) {
    return isInButtonRow(touchX, touchY, 3);
}

bool GameplayHotbar::isThrowTouched(int touchX, int touchY) {
    return isButton4Touched(touchX, touchY);
}

bool GameplayHotbar::isInventoryThrowTouched(int touchX, int touchY) {
    return touchX >= 194 && touchX <= 253 && touchY >= 134 && touchY <= 160;
}

// Touch zone y minibuttons medidos sobre ingamesubscreen.png
bool GameplayHotbar::isTouchZone(int touchX, int touchY) {
    return touchX >= 4 && touchX <= 187 && touchY >= 36 && touchY <= 157;
}

bool GameplayHotbar::isMiniButton1Touched(int touchX, int touchY) {
    return touchX >= 210 && touchX <= 233 && touchY >= 46 && touchY <= 67;
}

bool GameplayHotbar::isMiniButton2Touched(int touchX, int touchY) {
    return touchX >= 210 && touchX <= 233 && touchY >= 84 && touchY <= 105;
}

bool GameplayHotbar::isMiniButton3Touched(int touchX, int touchY) {
    return touchX >= 210 && touchX <= 233 && touchY >= 122 && touchY <= 143;
}
