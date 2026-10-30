#include "HotbarIcons.h"
#include "GameplayHotbar.h"
#include "BlockType.h"
#include "InventoryScreen.h"
#include "terrain.h"
#include "items.h"
#include "AnimItems.h"
#include <nds.h>

extern bool inventoryOpen;

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

static void drawSlotIcon(int slotIndex, BlockType type) {
    TextureCell top, side, bottom;
    getBlockTextures(type, top, side, bottom);

    bool isLeaves = isLeavesBlock(type);

    u8 tintR, tintG, tintB;
    if (isLeaves) {
        getGrassTint(type, false, tintR, tintG, tintB); // hojas sí necesitan tinte (textura gris)
    } else {
        tintR = tintG = tintB = 255; // pasto/tierra/piedra/madera: sin tinte, la textura ya está bien tal cual
    }

    int slotX = 4 + slotIndex * 28;
    int slotY = 4;

    u16* fb = (u16*)bgGetGfxPtr(GameplayHotbar::getBgId());

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

static void drawSelectionBox(int slotIndex) {
    u16* fb = (u16*)bgGetGfxPtr(GameplayHotbar::getBgId());

    // Mismo origen que los íconos (4px desde arriba/izquierda), con un
    // pequeño margen alrededor del ícono de 23x23
    const int PADDING = 2;   // margen entre el ícono y el recuadro
    const int THICKNESS = 5; // grosor real del borde

    int boxX = 4 + slotIndex * 28 - PADDING;
    int boxY = 4 - PADDING;
    int boxSize = 24 + PADDING * 2;

    u16 white = RGB15(24, 24, 24) | BIT(15);

    for (int t = 0; t < THICKNESS; t++) {
        int x1 = boxX - t;
        int y1 = boxY - t;
        int size = boxSize + t * 2;

        for (int x = 0; x < size; x++) {
            fb[y1 * 256 + (x1 + x)] = white;
            fb[(y1 + size - 1) * 256 + (x1 + x)] = white;
        }
        for (int y = 0; y < size; y++) {
            fb[(y1 + y) * 256 + x1] = white;
            fb[(y1 + y) * 256 + (x1 + size - 1)] = white;
        }
    }
}

// Fuente numérica propia, 4x6 píxeles por dígito (bits: 1=encendido)
static const unsigned char digitFont[10][6] = {
    {0b0110, 0b1001, 0b1001, 0b1001, 0b1001, 0b0110}, // 0
    {0b0010, 0b0110, 0b0010, 0b0010, 0b0010, 0b0111}, // 1
    {0b0110, 0b1001, 0b0001, 0b0010, 0b0100, 0b1111}, // 2
    {0b1111, 0b0001, 0b0110, 0b0001, 0b1001, 0b0110}, // 3
    {0b0001, 0b0011, 0b0101, 0b1001, 0b1111, 0b0001}, // 4
    {0b1111, 0b1000, 0b1110, 0b0001, 0b1001, 0b0110}, // 5
    {0b0011, 0b0100, 0b1000, 0b1110, 0b1001, 0b0110}, // 6
    {0b1111, 0b0001, 0b0010, 0b0100, 0b0100, 0b0100}, // 7
    {0b0110, 0b1001, 0b0110, 0b1001, 0b1001, 0b0110}, // 8
    {0b0110, 0b1001, 0b0111, 0b0001, 0b0010, 0b1100}, // 9
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

static void drawSlotCount(int slotIndex, int count) {
    if (count <= 1) return; // fiel al original: no se muestra si es 1

    u16* fb = (u16*)bgGetGfxPtr(GameplayHotbar::getBgId());
    u16 white = RGB15(31, 31, 31) | BIT(15);
    u16 black = RGB15(0, 0, 0) | BIT(15);

    // Convertimos el número a dígitos individuales (máximo 2 dígitos
    // por ahora, ya que no tenemos límite de stack todavía)
    int digits[3];
    int numDigits = 0;
    int temp = count;
    do {
        digits[numDigits++] = temp % 10;
        temp /= 10;
    } while (temp > 0 && numDigits < 3);

    const int DIGIT_WIDTH = 5; // 4px de dígito + 1px de espacio
    int textWidth = numDigits * DIGIT_WIDTH - 1;

    int slotRight = 4 + slotIndex * 28 + 24;
    int slotBottom = 4 + 24;

    int startX = slotRight - textWidth;
    int startY = slotBottom - 6;

    // Sombra negra (1px abajo/derecha) para legibilidad, estilo clásico
    for (int i = numDigits - 1; i >= 0; i--) {
        int digit = digits[i];
        int dx = startX + (numDigits - 1 - i) * DIGIT_WIDTH;
        drawDigit(fb, digit, dx + 1, startY + 1, black);
    }
    for (int i = numDigits - 1; i >= 0; i--) {
        int digit = digits[i];
        int dx = startX + (numDigits - 1 - i) * DIGIT_WIDTH;
        drawDigit(fb, digit, dx, startY, white);
    }
}

static void drawDurabilityBar(int slotX, int slotY, int size, int durability, int maxDurability) {
    if (durability >= maxDurability) return; // llena = sin barra, como el original

    u16* fb = (u16*)bgGetGfxPtr(GameplayHotbar::getBgId());

    float fraction = (float)durability / (float)maxDurability;
    if (fraction < 0.0f) fraction = 0.0f;
    if (fraction > 1.0f) fraction = 1.0f;

    int barWidth = size - 2;
    int filledWidth = (int)(fraction * barWidth);

    int barX = slotX + 1;
    int barY = slotY + size - 3;

    u16 darkBg = RGB15(3, 3, 3) | BIT(15);

    // Fila de fondo (oscura, ancho completo)
    for (int x = 0; x < barWidth; x++) {
        fb[barY * 256 + (barX + x)] = darkBg;
    }

    // Fila de color (verde -> rojo según cuánto queda)
    u8 r = (u8)(31 * (1.0f - fraction));
    u8 g = (u8)(31 * fraction);
    u16 fillColor = RGB15(r, g, 0) | BIT(15);

    for (int x = 0; x < filledWidth; x++) {
        fb[(barY + 1) * 256 + (barX + x)] = fillColor;
    }
}

void HotbarIcons::render(const Inventory& inventory, int selectedSlot) {
    GameplayHotbar::show();

    for (int i = 0; i < Inventory::HOTBAR_SIZE; i++) {
        if (!inventory.isSlotEmpty(i)) {
            drawSlotIcon(i, inventory.getSlotType(i));
            drawSlotCount(i, inventory.getSlotCount(i));

            if (hasDurability(inventory.getSlotType(i))) {
                int sx = 4 + i * 28;
                int sy = 4;
                drawDurabilityBar(sx, sy, 24, inventory.getSlotDurability(i), getToolMaxDurability(inventory.getSlotType(i)));
            }
        }
    }

    drawSelectionBox(selectedSlot);
}