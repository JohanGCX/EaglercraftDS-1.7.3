#include "InventoryScreen.h"
#include "GameplayHotbar.h"
#include "BlockType.h"
#include "terrain.h"
#include "inventoryMenu.h"
#include "items.h"
#include "AnimItems.h"
#include "CraftingScreen.h"

#include <nds.h>

int InventoryScreen::bgId = -1;

// Reutilizamos la misma imagen base de la hotbar como fondo (ya tiene
// el marco de los 9 slots superiores dibujado) — el resto de la
// grilla se dibuja encima directamente en VRAM
static u8 getTerrainPixelIndex(int px, int py) {
    const u8* data = (const u8*)terrainBitmap;
    return data[py * 256 + px];
}

static u8 getItemsPixelIndex(int px, int py) {
    const int anim = AnimItems::pixelOverride(px, py);   // brujula y reloj: el cuadro de ahora
    if (anim >= 0) return (u8)anim;
    const u8* data = (const u8*)itemsBitmap;
    return data[py * 256 + px];
}

static void drawIconAt(int slotX, int slotY, BlockType type) {
    TextureCell top, side, bottom;
    getBlockTextures(type, top, side, bottom);
    if (type == BlockType::JUKEBOX || isPistonBase(type)) side = top;   // el icono muestra la cara de arriba: la ranura del disco y el frente del piston (normal o pegajoso)

    bool isLeaves = isLeavesBlock(type);

    u8 tintR, tintG, tintB;
    if (isLeaves) {
        getGrassTint(type, false, tintR, tintG, tintB);
    } else {
        tintR = tintG = tintB = 255;
    }

    u16* fb = (u16*)bgGetGfxPtr(InventoryScreen::getBgId());

    bool fromItems = usesItemAtlas(type);

    // Boton y placas de presion: la textura se encoge para imitar en 2D la forma del bloque colocado
    int x0 = 0, x1 = 24, y0 = 0, y1 = 24;
    if (type == BlockType::BUTTON)            { x0 = 6; x1 = 18; y0 = 8;  y1 = 16; }   // rectangulo de 12 x 8
    else if (isPlateBlock(type))              { x0 = 2; x1 = 22; y0 = 11; y1 = 14; }   // 20 x 3
    const int w = x1 - x0, h = y1 - y0;

    for (int dy = y0; dy < y1; dy++) {
        int srcY = side.row * 16 + ((dy - y0) * 16) / h;
        for (int dx = x0; dx < x1; dx++) {
            if (!iconShapeVisible(type, dx, dy, 24)) continue;   // losas, escaleras y valla: silueta propia
            int srcX = side.col * 16 + ((dx - x0) * 16) / w;

            u8 index = fromItems ? getItemsPixelIndex(srcX, srcY) : getTerrainPixelIndex(srcX, srcY);
            if (index == 0) continue; // transparente: se ve el fondo del slot
            u16 pal = fromItems ? itemsPal[index] : terrainPal[index];

            int r5 = pal & 0x1F;
            int g5 = (pal >> 5) & 0x1F;
            int b5 = (pal >> 10) & 0x1F;

            r5 = (r5 * tintR) / 255;
            g5 = (g5 * tintG) / 255;
            b5 = (b5 * tintB) / 255;

            u16 color = RGB15(r5, g5, b5) | BIT(15);
            fb[(slotY + dy) * 256 + (slotX + dx)] = color;
        }
    }
}

// Misma fuente numérica ya usada en HotbarIcons
static const unsigned char digitFont[10][6] = {
    {0b0110, 0b1001, 0b1001, 0b1001, 0b1001, 0b0110},
    {0b0010, 0b0110, 0b0010, 0b0010, 0b0010, 0b0111},
    {0b0110, 0b1001, 0b0001, 0b0010, 0b0100, 0b1111},
    {0b1111, 0b0001, 0b0110, 0b0001, 0b1001, 0b0110},
    {0b0001, 0b0011, 0b0101, 0b1001, 0b1111, 0b0001},
    {0b1111, 0b1000, 0b1110, 0b0001, 0b1001, 0b0110},
    {0b0011, 0b0100, 0b1000, 0b1110, 0b1001, 0b0110},
    {0b1111, 0b0001, 0b0010, 0b0100, 0b0100, 0b0100},
    {0b0110, 0b1001, 0b0110, 0b1001, 0b1001, 0b0110},
    {0b0110, 0b1001, 0b0111, 0b0001, 0b0010, 0b1100},
};

static void drawDigit(u16* fb, int digit, int px, int py, u16 color) {
    if (digit < 0 || digit > 9) return;
    for (int row = 0; row < 6; row++) {
        unsigned char bits = digitFont[digit][row];
        for (int col = 0; col < 4; col++) {
            if (bits & (1 << (3 - col))) {
                int x = px + col;
                int y = py + row;
                if (x >= 0 && x < 256 && y >= 0 && y < 192) {
                    fb[y * 256 + x] = color;
                }
            }
        }
    }
}

static void drawCountAt(int slotX, int slotY, int count) {
    if (count <= 1) return;

    u16* fb = (u16*)bgGetGfxPtr(InventoryScreen::getBgId());
    u16 white = RGB15(31, 31, 31) | BIT(15);
    u16 black = RGB15(0, 0, 0) | BIT(15);

    int digits[3];
    int numDigits = 0;
    int temp = count;
    do {
        digits[numDigits++] = temp % 10;
        temp /= 10;
    } while (temp > 0 && numDigits < 3);

    const int DIGIT_WIDTH = 5;
    int textWidth = numDigits * DIGIT_WIDTH - 1;

    int startX = (slotX + 24) - textWidth;
    int startY = (slotY + 24) - 6;

    for (int i = numDigits - 1; i >= 0; i--) {
        int dx = startX + (numDigits - 1 - i) * DIGIT_WIDTH;
        drawDigit(fb, digits[i], dx + 1, startY + 1, black);
    }
    for (int i = numDigits - 1; i >= 0; i--) {
        int dx = startX + (numDigits - 1 - i) * DIGIT_WIDTH;
        drawDigit(fb, digits[i], dx, startY, white);
    }
}

static void drawSelectionBoxAt(int slotX, int slotY, u16 color) {
    u16* fb = (u16*)bgGetGfxPtr(InventoryScreen::getBgId());
    const int PADDING = 2;
    const int THICKNESS = 5;

    int boxX = slotX - PADDING;
    int boxY = slotY - PADDING;
    int boxSize = 24 + PADDING * 2;

    for (int t = 0; t < THICKNESS; t++) {
        int x1 = boxX - t;
        int y1 = boxY - t;
        int size = boxSize + t * 2;

        for (int x = 0; x < size; x++) {
            fb[y1 * 256 + (x1 + x)] = color;
            fb[(y1 + size - 1) * 256 + (x1 + x)] = color;
        }
        for (int y = 0; y < size; y++) {
            fb[(y1 + y) * 256 + x1] = color;
            fb[(y1 + y) * 256 + (x1 + size - 1)] = color;
        }
    }
}

// Coordenadas base de cada slot (0-8 hotbar, 9-35 inventario extendido)
static void getSlotPosition(int slotIndex, int& outX, int& outY) {
    if (slotIndex >= Inventory::ARMOR_BASE) {   // fila de armadura (casco, peto, pantalones, botas), abajo a la izquierda
        outX = 4 + (slotIndex - Inventory::ARMOR_BASE) * 28;
        outY = 133;
    } else if (slotIndex < 9) {
        outX = 4 + slotIndex * 28;
        outY = 4;
    } else {
        int extIndex = slotIndex - 9;
        int row = extIndex / 9;
        int col = extIndex % 9;
        outX = 4 + col * 28;
        outY = 41 + row * 28; // 12px de separación tras la hotbar, luego 28px por fila
    }
}

static void drawDurabilityBar(int slotX, int slotY, int size, int durability, int maxDurability) {
    if (durability >= maxDurability) return;

    u16* fb = (u16*)bgGetGfxPtr(InventoryScreen::getBgId());

    float fraction = (float)durability / (float)maxDurability;
    if (fraction < 0.0f) fraction = 0.0f;
    if (fraction > 1.0f) fraction = 1.0f;

    int barWidth = size - 2;
    int filledWidth = (int)(fraction * barWidth);

    int barX = slotX + 1;
    int barY = slotY + size - 3;

    u16 darkBg = RGB15(3, 3, 3) | BIT(15);
    for (int x = 0; x < barWidth; x++) {
        fb[barY * 256 + (barX + x)] = darkBg;
    }

    u8 r = (u8)(31 * (1.0f - fraction));
    u8 g = (u8)(31 * fraction);
    u16 fillColor = RGB15(r, g, 0) | BIT(15);
    for (int x = 0; x < filledWidth; x++) {
        fb[(barY + 1) * 256 + (barX + x)] = fillColor;
    }
}

void InventoryScreen::show(const Inventory& inventory, int heldSlot, int swapSelectedSlot) {
    if (bgId == -1) {
        videoSetModeSub(MODE_5_2D | DISPLAY_BG3_ACTIVE);
        vramSetBankC(VRAM_C_SUB_BG);
        bgId = bgInitSub(3, BgType_Bmp16, BgSize_B16_256x256, 0, 0);
        GameplayHotbar::invalidate();
        CraftingScreen::invalidate();
    }

    decompress((const void*)inventoryMenuBitmap, bgGetGfxPtr(bgId), LZ77Vram);

    u16* pixels = (u16*)bgGetGfxPtr(bgId);
    for (int i = 0; i < 256 * 192; i++) {
        pixels[i] |= BIT(15);
    }

    for (int i = 0; i < Inventory::SLOT_COUNT; i++) {
        if (!inventory.isSlotEmpty(i)) {
            int sx, sy;
            getSlotPosition(i, sx, sy);
            drawIconAt(sx, sy, inventory.getSlotType(i));       // sin el 24
            drawCountAt(sx, sy, inventory.getSlotCount(i));     // sin el 24

            if (hasDurability(inventory.getSlotType(i))) {
                drawDurabilityBar(sx, sy, 24, inventory.getSlotDurability(i), getToolMaxDurability(inventory.getSlotType(i)));
            }
        }
    }

    u16 grayColor = RGB15(24, 24, 24) | BIT(15);
    int hx, hy;
    getSlotPosition(heldSlot, hx, hy);
    drawSelectionBoxAt(hx, hy, grayColor);

    if (swapSelectedSlot != -1) {
        u16 whiteColor = RGB15(31, 31, 31) | BIT(15);
        int sx, sy;
        getSlotPosition(swapSelectedSlot, sx, sy);
        drawSelectionBoxAt(sx, sy, whiteColor);
    }
}

int InventoryScreen::getTouchedSlot(int touchX, int touchY) {
    // Hotbar (fila superior)
    if (touchY >= 4 && touchY <= 27) {
        int col = (touchX - 4) / 28;
        if (col >= 0 && col < 9) return col;
    }

    // Inventario extendido (3 filas de 9)
    if (touchY >= 41 && touchY <= 41 + 28 * 3) {
        int row = (touchY - 41) / 28;
        int col = (touchX - 4) / 28;
        if (row >= 0 && row < 3 && col >= 0 && col < 9) {
            return 9 + row * 9 + col;
        }
    }

    // Fila de armadura: 4 slots
    if (touchY >= 133 && touchY <= 133 + 24) {
        int col = (touchX - 4) / 28;
        if (touchX >= 4 && col >= 0 && col < Inventory::ARMOR_SLOTS) return Inventory::ARMOR_BASE + col;
    }

    return -1;
}