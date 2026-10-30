#include "CraftingScreen.h"
#include "Furnace.h"
#include "subcrafting.h"
#include "topcrafting1.h"
#include "Recipes.h"
#include "ItemOrder.h"
#include "BlockType.h"
#include "terrain.h"
#include "items.h"
#include "AnimItems.h"
#include "topcrafting2.h"
#include "GameplayHotbar.h"
#include "InventoryScreen.h"
#include "topfurnace1.h"
#include "topfurnace2.h"
#include "normalchest.h"
#include "dispensergui.h"
#include "largechest.h"
#include "ChestStore.h"
#include <nds.h>

int CraftingScreen::bgId = -1;

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

static void drawIconAt(int slotX, int slotY, int size, BlockType type) {
    TextureCell top, side, bottom;
    getBlockTextures(type, top, side, bottom);

    bool fromItems = usesItemAtlas(type);
    bool isLeaves = isLeavesBlock(type);

    u8 tintR, tintG, tintB;
    if (isLeaves) {
        getGrassTint(type, false, tintR, tintG, tintB);
    } else {
        tintR = tintG = tintB = 255;
    }

    u16* fb = (u16*)bgGetGfxPtr(CraftingScreen::getBgId());

    // Boton y placas de presion: la textura se encoge para imitar en 2D la forma del bloque colocado
    int x0 = 0, x1 = size, y0 = 0, y1 = size;
    if (type == BlockType::BUTTON)            { x0 = size * 6 / 24; x1 = size * 18 / 24; y0 = size * 8 / 24;  y1 = size * 16 / 24; }
    else if (isPlateBlock(type))              { x0 = size * 2 / 24; x1 = size * 22 / 24; y0 = size * 11 / 24; y1 = size * 11 / 24 + (size >= 24 ? 3 : 2); }
    const int w = x1 - x0, h = y1 - y0;

    for (int dy = y0; dy < y1; dy++) {
        int srcY = side.row * 16 + ((dy - y0) * 16) / h;
        for (int dx = x0; dx < x1; dx++) {
            if (!iconShapeVisible(type, dx, dy, size)) continue;   // losas, escaleras y valla: silueta propia
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
    for (int row = 0; row < 6; row++) {
        unsigned char bits = digitFont[digit][row];
        for (int col = 0; col < 4; col++) {
            if (bits & (1 << (3 - col))) {
                int x = px + col;
                int y = py + row;
                if (x >= 0 && x < 256 && y >= 0 && y < 192) fb[y * 256 + x] = color;
            }
        }
    }
}

// "pagina - total" entre los botones next y craft (solo si hay mas de una pagina)
static void drawPageIndicator(int page, int pages) {
    u16* fb = (u16*)bgGetGfxPtr(CraftingScreen::getBgId());
    const u16 white = RGB15(31, 31, 31) | BIT(15);
    const u16 black = RGB15(0, 0, 0) | BIT(15);
    int x = 138;
    const int y = 10;
    int vals[2] = { page, pages };
    for (int n = 0; n < 2; n++) {
        int digits[3], nd = 0, t = vals[n];
        do { digits[nd++] = t % 10; t /= 10; } while (t > 0 && nd < 3);
        for (int i = nd - 1; i >= 0; i--) { drawDigit(fb, digits[i], x + 1, y + 1, black); drawDigit(fb, digits[i], x, y, white); x += 5; }
        if (n == 0) { for (int k = 0; k < 4; k++) { fb[(y + 3) * 256 + x + 1 + k] = white; } x += 8; }
    }
}

static void drawCountAt(int slotX, int slotY, int size, int count) {
    if (count <= 1) return;

    u16* fb = (u16*)bgGetGfxPtr(CraftingScreen::getBgId());
    u16 white = RGB15(31, 31, 31) | BIT(15);
    u16 black = RGB15(0, 0, 0) | BIT(15);

    int digits[3];
    int numDigits = 0;
    int temp = count;
    do { digits[numDigits++] = temp % 10; temp /= 10; } while (temp > 0 && numDigits < 3);

    const int DIGIT_WIDTH = 5;
    int textWidth = numDigits * DIGIT_WIDTH - 1;
    int startX = (slotX + size) - textWidth;
    int startY = (slotY + size) - 6;

    for (int i = numDigits - 1; i >= 0; i--) {
        int dx = startX + (numDigits - 1 - i) * DIGIT_WIDTH;
        drawDigit(fb, digits[i], dx + 1, startY + 1, black);
    }
    for (int i = numDigits - 1; i >= 0; i--) {
        int dx = startX + (numDigits - 1 - i) * DIGIT_WIDTH;
        drawDigit(fb, digits[i], dx, startY, white);
    }
}

static void drawSelectionBoxAt(int slotX, int slotY, int size, u16 color) {
    u16* fb = (u16*)bgGetGfxPtr(CraftingScreen::getBgId());
    const int PADDING = 2;
    const int THICKNESS = 5;

    int boxX = slotX - PADDING;
    int boxY = slotY - PADDING;
    int boxSize = size + PADDING * 2;

    for (int t = 0; t < THICKNESS; t++) {
        int x1 = boxX - t, y1 = boxY - t;
        int s = boxSize + t * 2;
        for (int x = 0; x < s; x++) {
            fb[y1 * 256 + (x1 + x)] = color;
            fb[(y1 + s - 1) * 256 + (x1 + x)] = color;
        }
        for (int y = 0; y < s; y++) {
            fb[(y1 + y) * 256 + x1] = color;
            fb[(y1 + y) * 256 + (x1 + s - 1)] = color;
        }
    }
}

static void getRecipeSlotPosition(int slotIndex, int& outX, int& outY) {
    int row = slotIndex / 9;
    int col = slotIndex % 9;
    outX = 4 + col * 28;
    outY = 41 + row * 28;
}

// Mismas coordenadas que la mesa de crafteo: medio-arriba, medio-abajo y resultado
static void getFurnaceSlotPosition(int slot, int& outX, int& outY) {
    if (slot == 0)      { outX = 57;  outY = 56;  }
    else if (slot == 1) { outX = 57;  outY = 112; }
    else                { outX = 189; outY = 84;  }
}

static void drawDurabilityBarAt(int slotX, int slotY, int size, int durability, int maxDurability) {
    if (maxDurability <= 0 || durability >= maxDurability) return;

    u16* fb = (u16*)bgGetGfxPtr(CraftingScreen::getBgId());

    float fraction = (float)durability / (float)maxDurability;
    if (fraction < 0.0f) fraction = 0.0f;
    if (fraction > 1.0f) fraction = 1.0f;

    int barWidth = size - 2;
    int filledWidth = (int)(fraction * barWidth);
    int barX = slotX + 1;
    int barY = slotY + size - 3;

    u16 darkBg = RGB15(3, 3, 3) | BIT(15);
    for (int x = 0; x < barWidth; x++) fb[barY * 256 + (barX + x)] = darkBg;

    u8 r = (u8)(31 * (1.0f - fraction));
    u8 g = (u8)(31 * fraction);
    u16 fillColor = RGB15(r, g, 0) | BIT(15);
    for (int x = 0; x < filledWidth; x++) fb[(barY + 1) * 256 + (barX + x)] = fillColor;
}

int CraftingScreen::visibleRecipeIndices[36];
static const int PAGE_SIZE = 36;
static int s_page = 0;
static int s_pages = 1;
int CraftingScreen::visibleCount = 0;

void CraftingScreen::show(const Inventory& inventory, int selectedRecipe, bool usingTable) {
    if (bgId == -1) {
        videoSetModeSub(MODE_5_2D | DISPLAY_BG3_ACTIVE);
        vramSetBankC(VRAM_C_SUB_BG);
        bgId = bgInitSub(3, BgType_Bmp16, BgSize_B16_256x256, 0, 0);
        GameplayHotbar::invalidate();
        InventoryScreen::invalidate();
    }

    decompress((const void*)subcraftingBitmap, bgGetGfxPtr(bgId), LZ77Vram);   // comprimida (-gzl): se descomprime directo a la VRAM
    u16* pixels = (u16*)bgGetGfxPtr(bgId);
    for (int i = 0; i < 256 * 192; i++) pixels[i] |= BIT(15);

    visibleCount = 0;
    int selectedVisibleSlot = -1;

    // Paginas de 36 recetas (4 filas de 9): primero se cuentan las que se pueden hacer para saber cuantas paginas hay
    // Las recetas se recorren en el orden del original (por id del resultado), no en el del arreglo
    static int s_order[RECIPE_COUNT];
    static bool s_orderReady = false;
    if (!s_orderReady) {
        for (int i = 0; i < RECIPE_COUNT; i++) s_order[i] = i;
        ItemOrder::sortStable(s_order, RECIPE_COUNT, [](int r) { return ItemOrder::key(RECIPES[r].result); });
        s_orderReady = true;
    }
    int total = 0;
    for (int k = 0; k < RECIPE_COUNT; k++) {
        const int i = s_order[k];
        if (RECIPES[i].requiresTable && !usingTable) continue;
        if (canCraft(inventory, RECIPES[i])) total++;
    }
    s_pages = (total + PAGE_SIZE - 1) / PAGE_SIZE;
    if (s_pages < 1) s_pages = 1;
    if (s_page >= s_pages) s_page = s_pages - 1;
    if (s_page < 0) s_page = 0;
    int seen = 0;

    for (int k = 0; k < RECIPE_COUNT; k++) {
        const int i = s_order[k];
        if (RECIPES[i].requiresTable && !usingTable) continue; // solo se filtran si NO estamos en mesa
        if (!canCraft(inventory, RECIPES[i])) continue;
        if (seen++ < s_page * PAGE_SIZE) continue;      // paginas anteriores
        if (visibleCount >= PAGE_SIZE) break;            // pagina llena

        int sx, sy;
        getRecipeSlotPosition(visibleCount, sx, sy);
        drawIconAt(sx, sy, 24, RECIPES[i].result);
        drawCountAt(sx, sy, 24, RECIPES[i].resultCount);

        visibleRecipeIndices[visibleCount] = i;

        if (selectedRecipe == i) {
            selectedVisibleSlot = visibleCount; // guardamos, no dibujamos todavía
        }

        visibleCount++;
    }

    // El recuadro se dibuja AL FINAL, después de todos los íconos,
    // para que quede siempre por encima
    if (selectedVisibleSlot != -1) {
        int sx, sy;
        getRecipeSlotPosition(selectedVisibleSlot, sx, sy);
        u16 gray = RGB15(24, 24, 24) | BIT(15);
        drawSelectionBoxAt(sx, sy, 24, gray);
    }

    if (s_pages > 1) drawPageIndicator(s_page + 1, s_pages);
}

void CraftingScreen::resetPage() { s_page = 0; }

bool CraftingScreen::changePage(int delta) {
    const int np = s_page + delta;
    if (np < 0 || np >= s_pages) return false;
    s_page = np;
    return true;
}

void CraftingScreen::showTop(int selectedRecipe, bool usingTable) {
    videoSetMode(MODE_5_2D | DISPLAY_BG3_ACTIVE);
    vramSetBankB(VRAM_B_MAIN_BG_0x06000000);
    int bgIdTop = bgInit(3, BgType_Bmp16, BgSize_B16_256x256, 0, 0);

    // Si hay una receta seleccionada, la plantilla depende de LO QUE
    // ESA RECETA necesita (requiresTable), no de si estamos en una
    // mesa físicamente — solo cuando no hay nada seleccionado usamos
    // el contexto (usingTable) como respaldo para la pantalla en blanco
    bool showTableLayout = usingTable;
    if (selectedRecipe >= 0 && selectedRecipe < RECIPE_COUNT) {
        showTableLayout = RECIPES[selectedRecipe].requiresTable;
    }

    const void* bitmap = showTableLayout ? (const void*)topcrafting2Bitmap : (const void*)topcrafting1Bitmap;
    decompress(bitmap, bgGetGfxPtr(bgIdTop), LZ77Vram);

    u16* pixels = (u16*)bgGetGfxPtr(bgIdTop);
    for (int i = 0; i < 256 * 192; i++) pixels[i] |= BIT(15);

    if (selectedRecipe < 0 || selectedRecipe >= RECIPE_COUNT) return;

    const Recipe& r = RECIPES[selectedRecipe];

    int bgIdSaved = bgId;
    bgId = bgIdTop;

    if (showTableLayout) {
        const int cellX3x3[3] = {29, 57, 85};
        const int cellY3x3[3] = {56, 84, 112};

        for (int c = 0; c < 9; c++) {
            if (r.pattern[c] == RecipeCell::EMPTY) continue;
            BlockType iconType = (r.pattern[c] == RecipeCell::INGREDIENT_A) ? r.altA[0] : (r.pattern[c] == RecipeCell::INGREDIENT_B) ? r.altB[0] : (r.pattern[c] == RecipeCell::INGREDIENT_C) ? r.altC[0] : r.altD[0];
            int col = c % 3;
            int row = c / 3;
            drawIconAt(cellX3x3[col], cellY3x3[row], 24, iconType);
        }
    } else {
        const int cellX[4] = {43, 71, 43, 71};
        const int cellY[4] = {70, 70, 98, 98};

        for (int c = 0; c < 4; c++) {
            if (r.pattern[c] == RecipeCell::EMPTY) continue;
            BlockType iconType = (r.pattern[c] == RecipeCell::INGREDIENT_A) ? r.altA[0] : (r.pattern[c] == RecipeCell::INGREDIENT_B) ? r.altB[0] : (r.pattern[c] == RecipeCell::INGREDIENT_C) ? r.altC[0] : r.altD[0];
            drawIconAt(cellX[c], cellY[c], 24, iconType);
        }
    }

    drawIconAt(189, 84, 24, r.result);
    drawCountAt(189, 84, 24, r.resultCount);

    bgId = bgIdSaved;
}

// Animacion del horno: topfurnace1 trae el fueguito y la flecha en magenta (255,0,255). Esas zonas se rellenan con el gris de las casillas
// y encima se pintan, como cuadros planos, la llama (baja un pixel al gastarse el combustible) y la flecha (avanza hacia el resultado).
static const int FLAME_X0 = 55,  FLAME_X1 = 80,  FLAME_Y0 = 82, FLAME_Y1 = 107;   // 26 filas
static const int ARROW_X0 = 127, ARROW_X1 = 158, ARROW_Y0 = 82, ARROW_Y1 = 108;   // 32 columnas
static int s_flameRows = -1, s_arrowCols = -1;                                      // lo que quedo dibujado
static int s_furnaceSel = -1;                                                       // casilla con el recuadro de seleccion
static int s_furnaceBg = -1;                                                        // fondo de arriba mientras el horno esta abierto

static void paintFurnaceAnim(const u16* src, u16* px, int flameRows, int arrowCols);

static void furnaceAnimValues(const FurnaceData* d, int& flameRows, int& arrowCols) {
    flameRows = 0; arrowCols = 0;
    if (!d) return;
    const int rowsMax = FLAME_Y1 - FLAME_Y0 + 1, colsMax = ARROW_X1 - ARROW_X0 + 1;
    if (d->burnTime > 0 && d->burnTimeMax > 0) {
        flameRows = (d->burnTime * rowsMax + d->burnTimeMax - 1) / d->burnTimeMax;   // hacia arriba: encendido siempre muestra al menos 1
        if (flameRows > rowsMax) flameRows = rowsMax;
    }
    if (d->cookTime > 0) {
        arrowCols = d->cookTime * colsMax / Furnace::COOK_TIME;
        if (arrowCols > colsMax) arrowCols = colsMax;
    }
}

// Solo repinta los dos rectangulos de la animacion (unos 1500 pixeles), sin tocar el resto: no parpadea
void CraftingScreen::updateFurnaceAnim(const FurnaceData* data) {
    if (s_furnaceBg < 0 || !data || data->burnTime <= 0) return;   // en reposo no hay animacion (topfurnace2)
    int f, a;
    furnaceAnimValues(data, f, a);
    paintFurnaceAnim((const u16*)topfurnace1Bitmap, (u16*)bgGetGfxPtr(s_furnaceBg), f, a);
    s_flameRows = f; s_arrowCols = a;
}

bool CraftingScreen::furnaceAnimStale(const FurnaceData* data) {
    int f, a;
    furnaceAnimValues(data, f, a);
    return f != s_flameRows || a != s_arrowCols;
}

// Pinta solo la llama y la flecha en dst. La mascara sale de src (la imagen original), asi que sirve tanto en la pantalla completa como
// para actualizar solo esos dos rectangulos sin redibujar todo (que parpadea).
static void paintFurnaceAnim(const u16* src, u16* px, int flameRows, int arrowCols) {
    const u16 gray   = src[70 * 256 + 60] | BIT(15);        // interior de una casilla: el mismo gris
    const u16 yellow = RGB15(31, 28, 0) | BIT(15);
    const u16 white  = RGB15(31, 31, 31) | BIT(15);
    // El recuadro de seleccion (5 px de grosor, 2 de margen) invade la zona de la llama: ahi no se pinta
    int bx0 = 0, by0 = 0, bx1 = -1, by1 = -1;
    if (s_furnaceSel >= 0 && s_furnaceSel <= 2) {
        int sx, sy;
        getFurnaceSlotPosition(s_furnaceSel, sx, sy);
        bx0 = sx - 6; by0 = sy - 6; bx1 = sx + 30; by1 = sy + 30;
    }
    for (int y = FLAME_Y0; y <= FLAME_Y1; y++) {
        for (int x = FLAME_X0; x <= FLAME_X1; x++) {
            const u16 p = src[y * 256 + x];
            if ((p & 0x7FFF) != 0x7C1F && p != 0) continue;   // solo la zona magenta (o transparente)
            if (x >= bx0 && x < bx1 && y >= by0 && y < by1) continue;
            px[y * 256 + x] = (y > FLAME_Y1 - flameRows) ? yellow : gray;
        }
    }
    for (int y = ARROW_Y0; y <= ARROW_Y1; y++) {
        for (int x = ARROW_X0; x <= ARROW_X1; x++) {
            const u16 p = src[y * 256 + x];
            if ((p & 0x7FFF) != 0x7C1F && p != 0) continue;
            if (x >= bx0 && x < bx1 && y >= by0 && y < by1) continue;
            px[y * 256 + x] = (x < ARROW_X0 + arrowCols) ? white : gray;
        }
    }
}

void CraftingScreen::showFurnaceTop(bool lit, const FurnaceData* data, int selectedSlot) {
    (void)lit;
    // Con el horno sin encender se usa topfurnace2 (sin animacion); encendido, topfurnace1 con la llama y la flecha animadas
    const bool idle = !data || data->burnTime <= 0;
    videoSetMode(MODE_5_2D | DISPLAY_BG3_ACTIVE);
    vramSetBankB(VRAM_B_MAIN_BG_0x06000000);
    int bgIdTop = bgInit(3, BgType_Bmp16, BgSize_B16_256x256, 0, 0);

    dmaCopy(idle ? (const void*)topfurnace2Bitmap : (const void*)topfurnace1Bitmap, bgGetGfxPtr(bgIdTop), 256 * 192 * 2);

    u16* pixels = (u16*)bgGetGfxPtr(bgIdTop);
    int flameRows, arrowCols;
    furnaceAnimValues(data, flameRows, arrowCols);
    s_furnaceSel = selectedSlot;
    if (!idle) paintFurnaceAnim((const u16*)topfurnace1Bitmap, pixels, flameRows, arrowCols);
    s_furnaceBg = bgIdTop;
    s_flameRows = flameRows; s_arrowCols = arrowCols;
    for (int i = 0; i < 256 * 192; i++) pixels[i] |= BIT(15);

    int bgIdSaved = bgId;
    bgId = bgIdTop;

    if (data) {
        for (int s = 0; s < 3; s++) {
            const FurnaceSlot& fs = data->slot[s];
            if (fs.empty || fs.count <= 0) continue;
            int sx, sy;
            getFurnaceSlotPosition(s, sx, sy);
            drawIconAt(sx, sy, 24, fs.type);
            drawCountAt(sx, sy, 24, fs.count);
            if (hasDurability(fs.type)) {
                drawDurabilityBarAt(sx, sy, 24, fs.durability, getToolMaxDurability(fs.type));
            }
        }
    }

    // El cuadrito blanco va al final, por encima de todo (mismo estilo que el inventario)
    if (selectedSlot >= 0 && selectedSlot <= 2) {
        int sx, sy;
        getFurnaceSlotPosition(selectedSlot, sx, sy);
        drawSelectionBoxAt(sx, sy, 24, RGB15(31, 31, 31) | BIT(15));
    }

    bgId = bgIdSaved;
}

// Casillas del menú del cofre (los dos PNG tienen la misma separación de 28 px): la cuadrícula de 9 x 3 empieza en y=56 y la de 9 x 6, en y=14
static void getDispenserSlotPosition(int slot, int& outX, int& outY) {
    outX = 88 + (slot % 3) * 28;
    outY = 56 + (slot / 3) * 28;
}

static void getChestSlotPosition(int rows, int slot, int& outX, int& outY) {
    outX = 4 + (slot % 9) * 28;
    outY = (rows == 3 ? 56 : 14) + (slot / 9) * 28;
}

// Menú del cofre en la pantalla de arriba: la cuadrícula (de un cofre o de uno doble) con su contenido y el cuadrito de la casilla elegida
void CraftingScreen::showChestTop(const ChestView& view, int selectedSlot) {
    videoSetMode(MODE_5_2D | DISPLAY_BG3_ACTIVE);
    vramSetBankB(VRAM_B_MAIN_BG_0x06000000);
    int bgIdTop = bgInit(3, BgType_Bmp16, BgSize_B16_256x256, 0, 0);

    const void* bitmap = view.dispenser ? (const void*)dispenserguiBitmap : ((view.rows == 6) ? (const void*)largechestBitmap : (const void*)normalchestBitmap);
    decompress(bitmap, bgGetGfxPtr(bgIdTop), LZ77Vram);

    u16* pixels = (u16*)bgGetGfxPtr(bgIdTop);
    for (int i = 0; i < 256 * 192; i++) pixels[i] |= BIT(15);

    int bgIdSaved = bgId;
    bgId = bgIdTop;

    for (int i = 0; i < view.rows * 9; i++) {
        const ChestSlot* cs = view.slotAt(i);
        if (!cs || cs->count == 0) continue;
        int sx, sy;
        if (view.dispenser) getDispenserSlotPosition(i, sx, sy); else getChestSlotPosition(view.rows, i, sx, sy);
        BlockType t = (BlockType)cs->type;
        drawIconAt(sx, sy, 24, t);
        drawCountAt(sx, sy, 24, cs->count);
        if (hasDurability(t)) drawDurabilityBarAt(sx, sy, 24, cs->durability, getToolMaxDurability(t));
    }

    if (selectedSlot >= 0 && selectedSlot < view.rows * 9) {
        int sx, sy;
        if (view.dispenser) getDispenserSlotPosition(selectedSlot, sx, sy); else getChestSlotPosition(view.rows, selectedSlot, sx, sy);
        drawSelectionBoxAt(sx, sy, 24, RGB15(31, 31, 31) | BIT(15));
    }

    bgId = bgIdSaved;
}

bool CraftingScreen::isPreviousTouched(int touchX, int touchY) {
    return touchX >= 2 && touchX <= 61 && touchY >= 2 && touchY <= 28;
}

bool CraftingScreen::isNextTouched(int touchX, int touchY) {
    return touchX >= 66 && touchX <= 125 && touchY >= 2 && touchY <= 28;
}

bool CraftingScreen::isCraftTouched(int touchX, int touchY) {
    return touchX >= 194 && touchX <= 253 && touchY >= 2 && touchY <= 28;
}

int CraftingScreen::getTouchedRecipeSlot(int touchX, int touchY) {
    if (touchY < 41 || touchY > 41 + 28 * 4) return -1;
    int row = (touchY - 41) / 28;
    int col = (touchX - 4) / 28;
    if (row < 0 || row >= 4 || col < 0 || col >= 9) return -1;

    int visibleSlot = row * 9 + col;
    if (visibleSlot >= visibleCount) return -1;

    return visibleRecipeIndices[visibleSlot];
}