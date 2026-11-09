#include <nds.h>
#include <stdio.h>
#include <fat.h>
#include <filesystem.h>
#include <math.h>
#include "Renderer.h"
#include "ChunkManager.h"
#include "Player.h"
#include "Raycaster.h"
#include "Furnace.h"
#include "Texture.h"
#include "TextureAnim.h"
#include "Health.h"
#include "HealthHud.h"
#include "GameOverScreen.h"
#include "Options.h"
#include "SettingsMenu.h"
#include "PauseScreen.h"
#include "DebugChests.h"
#include "SheepModel.h"
#include "SheepManager.h"
#include "MobCull.h"
#include "AnimalManager.h"
#include "Maps.h"
#include "Weather.h"
#include "Spawners.h"
#include "Sky.h"
#include "StartupSplash.h"
#include "Clouds.h"
#include "Particles.h"
#include "Sound.h"
#include "Music.h"
#include "TexturePack.h"
#include "TexturePackMenu.h"
#include "Skins.h"
#include "Trees.h"
#include "Controls.h"
#include "WorldConfig.h"
#include "MinecartManager.h"
#include "MinecartModel.h"
#include "BoatManager.h"
#include "PaintingManager.h"
#include "SignManager.h"
#include "TntManager.h"
#include "Projectiles.h"
#include "FishingHook.h"
#include "AnimItems.h"
#include "Dispenser.h"
#include "NoteBlocks.h"
#include "BoatModel.h"
#include "AnimalModel.h"
#include "HostileModel.h"
#include "HostileManager.h"
#include "Governor.h"
#include "Biome.h"
#include "Noise.h"
#include "SubScreenMenu.h"
#include "TopScreenMenu.h"
#include "WorldConfig.h"
#include "WorldSave.h"
#include "Liquid.h"
#include "ChestStore.h"
#include "BlockType.h"
#include "DroppedItem.h"
#include "GameplayHotbar.h"
#include "HotbarIcons.h"
#include "FallingBlocks.h"
#include "SteveModel.h"
#include "Inventory.h"
#include "LoadingScreen.h"
#include "InventoryScreen.h"
#include "CraftingScreen.h"
#include "Recipes.h"
#include "PendingDrops.h"
#include "WorldSelectMenu.h"
#include "NewWorldMenu.h"
#include "Profiler.h"
#include "HandAnimations.h"
#include "PartialBlocks.h"
#include "Redstone.h"
#include "Pistons.h"
#include "Sleeping.h"

constexpr bool HEALTH_DAMAGE_DEFAULT = true;   // el daño arranca activado (en el juego: consola abierta con X y B lo alterna)
constexpr bool USE_WORLD_VIEW_BOBBING = true;  // el mismo balanceo también en la cámara del mundo

// Color del fondo y de la niebla del Nether, en la escala de 5 bits de la DS (0 a 31). La Beta usa (0.2, 0.03, 0.03), o sea (6, 1, 1):
// un marrón rojizo muy oscuro. Subí el rojo (y un poco el verde) para un Nether más rojizo.
constexpr int NETHER_SKY_R = 6, NETHER_SKY_G = 1, NETHER_SKY_B = 1;

enum GameState {
    STATE_MENU,
    STATE_WORLD_SELECT,
    STATE_CREATE_WORLD,
    STATE_LOADING,
    STATE_PLAYING,
    STATE_OPTIONS,
    STATE_TEXTUREPACKS
};

static void findLandChunkOffset(unsigned int seed, int& outChunkX, int& outChunkZ) {
    Noise::setSeed(seed);

    outChunkX = 0;
    outChunkZ = 0;

    float centerBiomeX = (float)(WorldConfig::SIZE_X / 2) * BiomeConfig::BIOME_NOISE_SCALE;
    float centerBiomeZ = (float)(WorldConfig::SIZE_Z / 2) * BiomeConfig::BIOME_NOISE_SCALE;
    float centerValue = Noise::biomeNoise2D(centerBiomeX, centerBiomeZ);

    if (BiomeConfig::getBiomeFromNoise(centerValue) != BiomeType::OCEAN) {
        return; // el origen ya es tierra, no hace falta buscar más
    }

    const int SEARCH_RADIUS_CHUNKS = 50;
    for (int radius = 1; radius <= SEARCH_RADIUS_CHUNKS; radius++) {
        for (int dx = -radius; dx <= radius; dx++) {
            for (int dz = -radius; dz <= radius; dz++) {
                float globalXi = dx * WorldConfig::SIZE_X + WorldConfig::SIZE_X / 2;
                float globalZi = dz * WorldConfig::SIZE_Z + WorldConfig::SIZE_Z / 2;

                float biomeValue = Noise::biomeNoise2D(globalXi * BiomeConfig::BIOME_NOISE_SCALE,
                                                         globalZi * BiomeConfig::BIOME_NOISE_SCALE);

                if (BiomeConfig::getBiomeFromNoise(biomeValue) != BiomeType::OCEAN) {
                    outChunkX = dx;
                    outChunkZ = dz;
                    return;
                }
            }
        }
    }
    // Si no se encontró nada en 50 chunks de radio (extremadamente
    // improbable), nos quedamos en el origen tal como estaba
}

static void getFaceOffset(BlockFace face, int& dx, int& dy, int& dz) {
    dx = dy = dz = 0;
    switch (face) {
        case BlockFace::NORTH:  dz = -1; break;
        case BlockFace::SOUTH:  dz = 1; break;
        case BlockFace::EAST:   dx = 1; break;
        case BlockFace::WEST:   dx = -1; break;
        case BlockFace::TOP:    dy = 1; break;
        case BlockFace::BOTTOM: dy = -1; break;
        default: break;
    }
}

// El frente del horno mira hacia el jugador (Player: forward = (-sin yaw, -cos yaw))
static int furnaceFrontDirFromYaw(float yawDeg) {
    float r = yawDeg * 3.14159265f / 180.0f;
    float s = sinf(r), c = cosf(r);
    if (fabsf(c) >= fabsf(s)) return (c > 0.0f) ? 0 : 1; // sur / norte
    return (s > 0.0f) ? 3 : 2;                           // este / oeste
}

bool inventoryOpen = false;
bool craftingOpen = false;

// Redstone: las cajas de las entidades que pisan las placas de presion (jugador, ovejas y objetos sueltos)
struct RedstoneEntityCtx { Player* const* player; DroppedItem* items; int itemCount; };
static void redstoneEnumerateEntities(void* user, Redstone::EntitySink sink, void* ctx) {
    const RedstoneEntityCtx* e = (const RedstoneEntityCtx*)user;
    const Player* p = *e->player;
    if (p) sink(ctx, p->x - 0.3f, p->y - 1.62f, p->z - 0.3f, p->x + 0.3f, p->y + 0.18f, p->z + 0.3f, false);
    float sb[12][6];
    const int n = SheepManager::collectBoxes(sb, 12);
    for (int i = 0; i < n; i++) sink(ctx, sb[i][0], sb[i][1], sb[i][2], sb[i][3], sb[i][4], sb[i][5], false);
    float cb[MinecartManager::MAX_CARTS][6];
    const int cn = MinecartManager::collectBoxes(cb, MinecartManager::MAX_CARTS);
    for (int i = 0; i < cn; i++) sink(ctx, cb[i][0], cb[i][1], cb[i][2], cb[i][3], cb[i][4], cb[i][5], true);   // las vagonetas pisan las placas de madera (no son mobs)
    for (int i = 0; i < e->itemCount; i++) {
        if (!e->items[i].isActive()) continue;
        const float x = e->items[i].getX(), y = e->items[i].getY(), z = e->items[i].getZ();
        sink(ctx, x - 0.125f, y - 0.125f, z - 0.125f, x + 0.125f, y + 0.125f, z + 0.125f, true);   // EntityItem: caja de 0,25
    }
}

// Redstone: las cajas de las vagonetas (el riel detector)
static void redstoneEnumerateMinecarts(void* user, Redstone::EntitySink sink, void* ctx) {
    (void)user;
    float cb[MinecartManager::MAX_CARTS][6];
    const int cn = MinecartManager::collectBoxes(cb, MinecartManager::MAX_CARTS);
    for (int i = 0; i < cn; i++) sink(ctx, cb[i][0], cb[i][1], cb[i][2], cb[i][3], cb[i][4], cb[i][5], false);
}

static void throwOneFromSlot(Inventory& inventory, DroppedItem droppedItems[], int maxItems,
                              int slotIndex, Player* player, bool wholeStack = false) {
    if (inventory.isSlotEmpty(slotIndex)) return;

    BlockType typeToThrow = inventory.getSlotType(slotIndex);
    bool tool = hasDurability(typeToThrow);
    int toolDurability = tool ? inventory.getSlotDurability(slotIndex) : -1;
    int amount = (wholeStack && !tool) ? inventory.getSlotCount(slotIndex) : 1;

    float dirX, dirY, dirZ;
    player->getLookDirection(dirX, dirY, dirZ);

    float velX = dirX * 0.3f;
    float velZ = dirZ * 0.3f;
    float velY = dirY * 0.3f + 0.1f;

    float throwX = player->x + dirX * 0.5f;
    float throwY = player->y - 0.3f;
    float throwZ = player->z + dirZ * 0.5f;

    for (int i = 0; i < maxItems; i++) {
        if (!droppedItems[i].isActive()) {
            droppedItems[i].spawn(throwX, throwY, throwZ, typeToThrow, true, velX, velY, velZ, toolDurability, amount);
            inventory.removeFromSlot(slotIndex, amount);
            return;
        }
    }
    // sin lugar en la lista de ítems sueltos: no se tira nada (antes el ítem se perdía)
}

static void drawBreakingOverlay(Chunk* chunk, int xi, int yi, int zi, float progress) {
    int stage = (int)(progress * 10.0f);
    if (stage > 9) stage = 9;
    if (stage < 0) return;

    // la grieta sigue la caja de seleccion (puerta, antorcha, nieve...), no la celda entera
    float bd[6];
    if (!chunk->getSelectionBounds(xi, yi, zi, bd)) return;

    const float e = 0.006f;   // un poco mas grande que la caja: el buffer de profundidad de la DS es de 15 bits
    const float x0 = bd[0] - 0.5f - e, y0 = bd[1] - 0.5f - e, z0 = bd[2] - 0.5f - e;
    const float x1 = bd[3] - 0.5f + e, y1 = bd[4] - 0.5f + e, z1 = bd[5] - 0.5f + e;

    float worldX = chunk->gridToWorldX(xi);
    float worldY = chunk->gridToWorldY(yi);
    float worldZ = chunk->gridToWorldZ(zi);

    const int u0 = stage * 16, v0 = 15 * 16;
    const int u1 = u0 + 16, v1 = v0 + 16;

    glBindTexture(0, Texture::textureID);
    glColor3b(255, 255, 255);
    glPolyFmt(POLY_ALPHA(31) | POLY_CULL_NONE | POLY_ID(5));   // sin culling: se ve desde cualquier lado y el orden de los vertices no importa

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
        glTranslatef(worldX, worldY, worldZ);
        glBegin(GL_QUADS);
#define BV(U, V, X, Y, Z) GFX_TEX_COORD = TEXTURE_PACK(inttot16(U), inttot16(V)); glVertex3v16(floattov16(X), floattov16(Y), floattov16(Z));
            BV(u0, v1, x0, y0, z1) BV(u1, v1, x1, y0, z1) BV(u1, v0, x1, y1, z1) BV(u0, v0, x0, y1, z1)   // +Z
            BV(u1, v1, x0, y0, z0) BV(u0, v1, x1, y0, z0) BV(u0, v0, x1, y1, z0) BV(u1, v0, x0, y1, z0)   // -Z
            BV(u0, v1, x0, y0, z0) BV(u1, v1, x0, y0, z1) BV(u1, v0, x0, y1, z1) BV(u0, v0, x0, y1, z0)   // -X
            BV(u0, v1, x1, y0, z1) BV(u1, v1, x1, y0, z0) BV(u1, v0, x1, y1, z0) BV(u0, v0, x1, y1, z1)   // +X
            BV(u0, v0, x0, y1, z1) BV(u1, v0, x1, y1, z1) BV(u1, v1, x1, y1, z0) BV(u0, v1, x0, y1, z0)   // arriba
            BV(u0, v0, x0, y0, z0) BV(u1, v0, x1, y0, z0) BV(u1, v1, x1, y0, z1) BV(u0, v1, x0, y0, z1)   // abajo
#undef BV
        glEnd();
    glPopMatrix(1);

    glPolyFmt(POLY_ALPHA(31) | POLY_CULL_NONE | POLY_FOG);   // el formato base de Renderer::setupProjection
}

// Velo del portal (GuiIngame.renderPortalOverlay): la textura del portal a pantalla completa, más opaca cuanto más tiempo llevás
// adentro (alfa = t^4 * 0,8 + 0,2). Sin animación ni la ondulación de cámara del original.
static void drawPortalOverlay(float timeInPortal) {
    float v = timeInPortal;
    if (v < 1.0f) {
        v *= v;
        v *= v;
        v = v * 0.8f + 0.2f;
    }
    int alpha = (int)(v * 30.0f);
    if (alpha < 1) alpha = 1;
    if (alpha > 30) alpha = 30;

    const int u0 = 14 * 16, v0 = 0; // celda (14,0) del atlas del terreno
    const int u1 = u0 + 16, v1 = v0 + 16;

    glPolyFmt(POLY_ALPHA(alpha) | POLY_CULL_NONE | POLY_ID(7)); // sin niebla; con otro ID para mezclarse con lo que hay detrás
    glBindTexture(0, Texture::textureID);
    glColor3b(255, 255, 255);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
        glLoadIdentity();
        glTranslatef(0.0f, 0.0f, -0.15f); // pegado a la cámara, como la mira
        const float hw = 0.16f, hh = 0.12f; // un poco más que la mitad de la pantalla a esa distancia
        glBegin(GL_QUADS);
            GFX_TEX_COORD = TEXTURE_PACK(inttot16(u0), inttot16(v1)); glVertex3v16(floattov16(-hw), floattov16(-hh), floattov16(0));
            GFX_TEX_COORD = TEXTURE_PACK(inttot16(u1), inttot16(v1)); glVertex3v16(floattov16( hw), floattov16(-hh), floattov16(0));
            GFX_TEX_COORD = TEXTURE_PACK(inttot16(u1), inttot16(v0)); glVertex3v16(floattov16( hw), floattov16( hh), floattov16(0));
            GFX_TEX_COORD = TEXTURE_PACK(inttot16(u0), inttot16(v0)); glVertex3v16(floattov16(-hw), floattov16( hh), floattov16(0));
        glEnd();
    glPopMatrix(1);

    glPolyFmt(POLY_ALPHA(31) | POLY_CULL_NONE | POLY_FOG); // el formato base de Renderer::setupProjection
}

// ItemRenderer.renderFireInFirstPerson: dos llamas (terrain.png 31 y 47) a los lados de la pantalla, con alfa 0,9, mientras el jugador arde.
// Las texturas animadas del original no existen aqui: cada llama alterna entre las dos celdas y se espeja cada pocos ticks para que parpadee.
static void drawFireOverlay(u32 frames) {
    const int t = (int)(frames / 3);   // ticks de mundo
    // (1 << 11): los poligonos translucidos tambien escriben profundidad. Con glFlush(0) el hardware ordena los translucidos por su cuenta, y el agua
    // (translucida) a veces se dibujaba despues y tapaba las llamas. Ahora el agua que cae detras de las llamas no pasa la prueba de profundidad.
    glPolyFmt(POLY_ALPHA(28) | POLY_CULL_NONE | POLY_ID(15) | (1 << 11));
    glBindTexture(0, Texture::textureID);
    glColor3b(255, 255, 255);
    for (int k = 0; k < 2; k++) {
        const int cellRow = 1 + ((k + (t >> 1)) & 1);   // fila 1 (celda 31) o fila 2 (celda 47)
        int u0 = 15 * 16, u1 = u0 + 16;
        if (((t >> 2) + k) & 1) { int tmp = u0; u0 = u1; u1 = tmp; }   // espejo
        const int v0 = cellRow * 16, v1 = v0 + 16;
        const float sgn = (float)(k * 2 - 1);
        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
            glLoadIdentity();
            glTranslatef(0.0f, 0.0f, -0.15f);
            glTranslatef(-sgn * 0.24f * 0.3f, -0.3f * 0.3f, 0.0f);   // la escena del original (1 bloque a 0,5) llevada al plano de 0,15
            glRotateY(sgn * 10.0f);
            const float h = 0.15f;   // medio lado: 1 * 0,3 / 2
            glBegin(GL_QUADS);
                GFX_TEX_COORD = TEXTURE_PACK(inttot16(u1), inttot16(v1)); glVertex3v16(floattov16(-h), floattov16(-h), floattov16(0));
                GFX_TEX_COORD = TEXTURE_PACK(inttot16(u0), inttot16(v1)); glVertex3v16(floattov16( h), floattov16(-h), floattov16(0));
                GFX_TEX_COORD = TEXTURE_PACK(inttot16(u0), inttot16(v0)); glVertex3v16(floattov16( h), floattov16( h), floattov16(0));
                GFX_TEX_COORD = TEXTURE_PACK(inttot16(u1), inttot16(v0)); glVertex3v16(floattov16(-h), floattov16( h), floattov16(0));
            glEnd();
        glPopMatrix(1);
    }
    glPolyFmt(POLY_ALPHA(31) | POLY_CULL_NONE | POLY_FOG);
}

// GuiGameOver.drawScreen: drawGradientRect(0, 0, ancho, alto, 0x60500000, 0xA0803030), un degradado a rojo vino sobre toda la pantalla de arriba.
// La DS admite una sola transparencia por polígono: se usa la promedio (0x80 = 16/31); el degradado de color sí se conserva.
static void drawDeathOverlay() {
    glPolyFmt(POLY_ALPHA(16) | POLY_CULL_NONE | POLY_ID(11));
    glBindTexture(0, 0); // sin textura, color plano

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
        glLoadIdentity();
        glTranslatef(0.0f, 0.0f, -0.15f); // pegado a la cámara, como la mira
        const float hw = 0.16f, hh = 0.12f;
        glBegin(GL_QUADS);
            glColor3b(0x80, 0x30, 0x30); glVertex3v16(floattov16(-hw), floattov16(-hh), floattov16(0)); // abajo: 0xA0803030
            glColor3b(0x80, 0x30, 0x30); glVertex3v16(floattov16( hw), floattov16(-hh), floattov16(0));
            glColor3b(0x50, 0x00, 0x00); glVertex3v16(floattov16( hw), floattov16( hh), floattov16(0)); // arriba: 0x60500000
            glColor3b(0x50, 0x00, 0x00); glVertex3v16(floattov16(-hw), floattov16( hh), floattov16(0));
        glEnd();
    glPopMatrix(1);

    glPolyFmt(POLY_ALPHA(31) | POLY_CULL_NONE | POLY_FOG); // el formato base de Renderer::setupProjection
}

// Velo de la cama (GuiIngame): 0x101020 con hasta 220/255 de alfa mientras duermes; amount de 0 a 1 (Sleeping::overlayAmount)
static void drawSleepOverlay(float amount) {
    int alpha = (int)(amount * 220.0f / 255.0f * 31.0f + 0.5f);
    if (alpha < 1) return;   // alfa 0 en la DS dibuja solo las aristas
    if (alpha > 31) alpha = 31;
    glPolyFmt(POLY_ALPHA(alpha) | POLY_CULL_NONE | POLY_ID(12));
    glBindTexture(0, 0); // sin textura, color plano
    glColor3b(0x10, 0x10, 0x20);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
        glLoadIdentity();
        glTranslatef(0.0f, 0.0f, -0.15f); // pegado a la cámara, como la mira
        const float hw = 0.16f, hh = 0.12f;
        glBegin(GL_QUADS);
            glVertex3v16(floattov16(-hw), floattov16(-hh), floattov16(0));
            glVertex3v16(floattov16( hw), floattov16(-hh), floattov16(0));
            glVertex3v16(floattov16( hw), floattov16( hh), floattov16(0));
            glVertex3v16(floattov16(-hw), floattov16( hh), floattov16(0));
        glEnd();
    glPopMatrix(1);

    glPolyFmt(POLY_ALPHA(31) | POLY_CULL_NONE | POLY_FOG); // el formato base de Renderer::setupProjection
}

// Opcion inventoryMap (como la Legacy Edition): el jugador siempre lleva un mapa, de preferencia en la ultima casilla de la barra (slot 9)
static void ensureInventoryMap(Inventory* inv) {
    if (!inv || !Options::get().inventoryMap) return;
    for (int i = 0; i < Inventory::TOTAL_SIZE; i++)
        if (!inv->isSlotEmpty(i) && inv->getSlotType(i) == BlockType::MAP) return;
    int slot = -1;
    if (inv->isSlotEmpty(Inventory::HOTBAR_SIZE - 1)) slot = Inventory::HOTBAR_SIZE - 1;
    else for (int i = 0; i < Inventory::TOTAL_SIZE && slot < 0; i++) if (inv->isSlotEmpty(i)) slot = i;
    if (slot < 0) return;
    inv->setSlotDirect(slot, BlockType::MAP, 1);
    inv->setSlotDurability(slot, 0);   // sin asignar: se le da su numero la primera vez que se lleva en la mano
}

// Una flecha del jugador o del dispensador pega al jugador (EntityPlayer.attackEntityFrom): vida, invulnerabilidad y empuje. Devuelve false si no hizo dano.
struct ArrowHurtCtx { HealthState* hs; Player** player; };
static bool arrowHurtsPlayer(void* c, int amount, float srcX, float srcZ, bool hasSource) {
    ArrowHurtCtx* a = (ArrowHurtCtx*)c;
    const bool newHit = !((float)a->hs->heartsLife > (float)Health::HEARTS_HALVES_LIFE / 2.0f);
    if (!Health::attack(*a->hs, amount)) return false;
    Sound::playerHurt();
    if (newHit && hasSource && *a->player) (*a->player)->knockBack(srcX, srcZ);
    return true;
}

// Ciclo de dia y noche de la Beta 1.7.3: 24000 ticks de mundo a 20 por segundo (20 minutos). El juego corre a 60 cuadros por segundo, asi que
// gameTimeTicks cuenta cuadros (3 por tick de mundo). WorldProvider.calculateCelestialAngle + World.calculateSkylightSubtracted.
static const u32 FRAMES_PER_WORLD_TICK = 3;
static const u32 WORLD_DAY_TICKS = 24000;
static const u32 CYCLE_TICKS = WORLD_DAY_TICKS * FRAMES_PER_WORLD_TICK;
static const u32 DEBUG_NOON_TICKS = 6000 * FRAMES_PER_WORLD_TICK;        // para la depuracion (B): mediodia
static const u32 DEBUG_MIDNIGHT_TICKS = 18000 * FRAMES_PER_WORLD_TICK;   // y medianoche

static float computeCelestialAngle(u32 frames) {
    const float t = (float)(frames % CYCLE_TICKS) / (float)FRAMES_PER_WORLD_TICK;
    float a = t / (float)WORLD_DAY_TICKS - 0.25f;
    if (a < 0.0f) a += 1.0f;
    if (a > 1.0f) a -= 1.0f;
    const float raw = a;
    a = 1.0f - (float)((cos((double)a * 3.14159265358979) + 1.0) / 2.0);
    return raw + (a - raw) / 3.0f;
}

// brillo del cielo 0..1 (el cos(angulo)*2+0.5 recortado de World.calculateSkylightSubtracted y WorldProvider.func_4096_a)
static float computeSkyBrightness(u32 frames) {
    float f = cosf(computeCelestialAngle(frames) * 3.14159265f * 2.0f) * 2.0f + 0.5f;
    if (f < 0.0f) f = 0.0f;
    if (f > 1.0f) f = 1.0f;
    return f;
}

static float computeNightFactor(u32 frames) { return 1.0f - computeSkyBrightness(frames); }

// World.calculateSkylightSubtracted: 0 (dia) a 11 (noche cerrada)
static int computeSkylightSubtract(u32 frames) {
    const float wf = (1.0f - Weather::rain(1.0f) * 5.0f / 16.0f) * (1.0f - Weather::thunder(1.0f) * 5.0f / 16.0f);
    int v = (int)((1.0f - computeSkyBrightness(frames) * wf) * 11.0f);
    if (v < 0) v = 0;
    if (v > 11) v = 11;
    return v;
}

int main(void) {
    // DSi mode: ask for the 133 MHz ARM9 clock (SCFG_CLK bit 0). If the launcher already left it on this changes nothing; on a regular DS it is skipped.
    // All frame budgets are measured with the bus timers (33.5 MHz), which do not change, so the same budget simply does more work.
#ifndef MC_NO_DSI_BOOST
    if (isDSiMode()) *(volatile u16*)0x04004004 |= 1;
#endif
    consoleDemoInit();

    Renderer renderer;
    renderer.init();
    renderer.setupProjection();

    StartupSplash::show();   // logos de Eagtek y Mojang (1 s cada uno) al arrancar, antes de cargar nada

    u32 gameTimeTicks = 0;
    int dlMaxKB = 0;
    int lastSkylightStep = -1;

    // Dimensiones: 0 = superficie, -1 = Nether. Quedarse 4 s dentro de un portal (EntityPlayerSP.onLivingUpdate) lleva al otro mundo.
    int currentDimension = 0;
    float timeInPortal = 0.0f;
    int timeUntilPortal = 20;      // EntityPlayer.timeUntilPortal arranca en 20
    bool inPortalFlag = false;
    unsigned char chunkModeAtOptions = 0;
    bool reloadChunkModeRequested = false;
    bool arrivalPending = false;   // al terminar de cargar el mundo nuevo, el jugador aparece en (arrivalX, arrivalZ)
    HealthState healthState;       // vida del jugador (Health.h)
    bool healthDamageEnabled = HEALTH_DAMAGE_DEFAULT;
    bool hudHeadInWater = false;   // la cabeza bajo el agua: muestra las burbujas
    float deathCamX = 0.0f, deathCamY = 0.0f, deathCamZ = 0.0f;   // donde estaban los ojos al morir: la camara se queda ahi, fija
    float trampleWalked = 0.0f, tramplePrevX = 0.0f, tramplePrevZ = 0.0f;   // Entity.distanceWalkedModified: cada paso de mas de 1 sobre tierra de cultivo la puede pisotear
    int trampleNextStep = 1;
    bool tramplePrevValid = false;
    bool deathHandled = false;     // la muerte ya se procesó (el inventario se suelta una sola vez)
    bool respawnPending = false;   // al terminar de cargar, el jugador aparece en el punto de aparición del mundo
    float arrivalX = 0.0f, arrivalZ = 0.0f;

    // Dormir (EntityPlayer.sleeping / sleepTimer / bedChunkCoordinates / playerSpawnCoordinate). Las celdas son globales (gx, gy, gz) de la mitad de la cama con el bit 8
    bool sleeping = false;
    int sleepTimer = 0;                   // sube a 100 durmiendo (velo y noche saltada) y a 110 al despertar (el velo se va)
    int bedGx = 0, bedGy = 0, bedGz = 0;  // la cama donde se duerme
    int bedDir = 0;                       // su direccion (metadata & 3)
    int sleepLastHealth = 20;             // para despertar al recibir dano
    bool bedSpawnSet = false;             // hay punto de reaparicion en una cama (bed.dat)
    float worldSpawnX = 0.0f, worldSpawnZ = 0.0f;   // punto de aparicion del mundo: a lo que apunta la brujula
    bool worldSpawnSet = false;
    int bedSpawnGx = 0, bedSpawnGy = 0, bedSpawnGz = 0;
    bool respawnAtBed = false;            // el mundo que se esta cargando para reaparecer esta centrado en la cama

    bool controlsInverted = false;
    bool hasSavedWorld = false;
    bool consoleVisible = false;
    bool autoJumpEnabled = true;
    int controlMode = 0;
    bool thirdPerson = false;     // camara en tercera persona (minibutton2), vista trasera de Beta 1.7.3
    bool paused = false;          // menu de pausa abierto: el juego se detiene (GuiIngameMenu.doesGuiPauseGame); siguen la musica y la carga de chunks
    int pauseView = 0;            // 0 = cerrado, 1 = menu de pausa, 2 = opciones (consola)
    bool saveRequested = false;   // boton Save game del menu de pausa
    bool quitRequested = false;   // boton Quit to title del menu de pausa

    Chunk* miningChunk = nullptr;
    int miningXi = -1, miningYi = -1, miningZi = -1;
    int miningChunkX = 0, miningChunkZ = 0;
    float miningDamage = 0.0f;   // PlayerControllerSP.curBlockDamage: 0 a 1, se rompe al llegar a 1 (ver getBlockStrength en BlockType.h)
    int miningHitWait = 0;       // PlayerControllerSP.blockHitWait: pausa de 5 ticks tras romper un bloque

    unsigned int canaryBeforeSlot = 0xDEADBEEF;
    int selectedSlot = 0;
    unsigned int canaryAfterSlot = 0xCAFEBABE;

    int craftingSelectedRecipe = -1;
    int inventorySelectedSlot = -1;
    bool craftingUsingTable = false;
    bool furnaceOpen = false;
    FurnaceData* openFurnace = nullptr;
    int furnaceSelectedSlot = 0;
    bool chestOpen = false;       // menú del cofre abierto (la pantalla de arriba muestra el cofre; la de abajo, el inventario)
    ChestView chestView;
    int chestSelectedSlot = 0;
    int furnaceLastLeftSlot = 0;
    bool furnaceLitOpen = false;

    static int prevSelectedSlotDiag = 0;
    int prevDurabilityDiag[Inventory::TOTAL_SIZE] = {0};
    bool diagInitialized = false;
    bool selectedSlotChangedByTouch = false;

    if (!fatInitDefault()) {
        iprintf("ERROR: libfat init failed\n");
    } else {
        iprintf("libfat initialized\n");
    }
    // NitroFS: archivos metidos dentro del .nds (carpeta nitrofiles/ del repo, por ahora la musica). Si falla, la musica solo se busca en la SD
    if (!nitroFSInit(nullptr)) iprintf("NitroFS not available\n");
    Prof::init();
    WorldSave::ensureDirectoryStructure();
    Options::load();
    Sound::init();   // los menus tambien suenan
    Controls::load();   // perfiles de controles (controlsprofile.dat)
    autoJumpEnabled = Options::get().autoJump;
    controlMode = Options::get().controlMode;
    WorldSave::recoverInterruptedSession();

    if (Texture::loadItemsAtlas()) {
        iprintf("Items loaded OK\n");
    } else {
        iprintf("ERROR loading items\n");
    }

    if (Texture::loadTerrainAtlas()) {
        iprintf("Terrain loaded OK\n");
    } else {
        iprintf("ERROR loading terrain\n");
    }

    if (Texture::loadIconsAtlas()) {
        iprintf("Icons loaded OK\n");
    } else {
        iprintf("ERROR loading icons\n");
    }
    SheepModel::init();
    MinecartModel::init();
    BoatModel::init();
    AnimalModel::init();
    HostileModel::init();
    SteveModel::init();
    TextureAnim::init(); // fuego y portal animados (el atlas del terreno ya está cargado)
    AnimItems::init();   // brujula y reloj animados (el atlas de items ya esta cargado)
    TexturePack::boot();   // el texture pack elegido (terrain e items); si ya no esta o fallo, vuelve a Default

    Skins::boot();   // la skin elegida se escribe sobre char.png antes de subirla; si ya no esta o fallo, vuelve a Default
    if (Texture::loadSkinAtlas()) {
        iprintf("Skin loaded OK\n");
    } else {
        iprintf("ERROR loading skin\n");
    }

    // Las texturas de entidades van al final: asi, al soltarlas para el panorama del menu, quedan juntas con el espacio libre (32 KB seguidos)
    if (Texture::loadSheepAtlas()) {
        iprintf("Sheep loaded OK\n");
    } else {
        iprintf("ERROR loading sheep\n");
    }
    if (Texture::loadCartAtlas()) {
        iprintf("Minecart loaded OK\n");
    } else {
        iprintf("ERROR loading minecart\n");
    }
    if (Texture::loadBoatAtlas()) {
        iprintf("Boat loaded OK\n");
    } else {
        iprintf("ERROR loading boat\n");
    }
    if (Texture::loadAnimalAtlases()) {
        iprintf("Animals loaded OK\n");
    } else {
        iprintf("ERROR loading animals\n");
    }
    if (Texture::loadHostileAtlases()) {
        iprintf("Hostiles loaded OK\n");
    } else {
        iprintf("ERROR loading hostiles\n");
    }

    StartupSplash::finish();   // todo cargado: la pantalla de abajo vuelve a la normalidad y el menu toma las dos pantallas
    SubScreenMenu::init(WorldSave::worldSlotExists(1));
    TopScreenMenu::init();

    GameState state = STATE_MENU;

    ChunkManager* chunkManager = nullptr;
    Player* player = nullptr;

    const int MAX_DROPPED_ITEMS = 48; // al morir se sueltan hasta 36 pilas del inventario de golpe
    DroppedItem droppedItems[MAX_DROPPED_ITEMS];
    RedstoneEntityCtx redstoneCtx = { &player, droppedItems, MAX_DROPPED_ITEMS };

    unsigned int canaryBeforeInventoryPtr = 0xFEEDFACE;
    Inventory* inventory = nullptr;
    unsigned int canaryAfterInventoryPtr = 0xBAADF00D;

    unsigned int entropyCounter = 0;
    int frameCounter = 0;
    int renderFrameCounter = 0;

    auto startWorld = [&](bool loadExisting, int worldNum, const char* newName, bool useCustomSeed, unsigned int customSeed) {
        state = STATE_LOADING;
        TopScreenMenu::suspend();
        LoadingScreen::show();

        if (loadExisting) {
            WorldSave::beginSessionLoad(worldNum);
        } else {
            WorldSave::beginSessionNew();
            WorldSave::writeActiveWorldName(newName);   // nombre propio (vacio = World N)
        }

        char metaPath[80];
        WorldSave::getBaseFilePath("meta.dat", metaPath);

        static WorldMetadata loadedMeta;
        hasSavedWorld = loadExisting && WorldSave::loadMetadata(loadedMeta, metaPath);

        // Terrain generator: a new world uses the current one (or the original one with debug options -> oldterraingenerator); a saved world keeps
        // the one it was made with (meta.dat written before this field existed reads it as 0 = original generator), so old worlds do not get seams
        // between saved and newly generated chunks.
        {
            const int newWorldGen = Options::get().oldTerrainGenerator ? 0 : WorldConfig::CURRENT_TERRAIN_GEN;
            int tg = loadExisting ? (hasSavedWorld ? loadedMeta.terrainGen : 0) : newWorldGen;
            if (tg < 0 || tg > WorldConfig::CURRENT_TERRAIN_GEN) tg = 0;
            WorldConfig::setTerrainGen(tg);
        }

        unsigned int worldSeed;
        inventory = new Inventory();

        // Dimensión guardada (dim.dat: -1 = Nether). Un mundo sin ese archivo está en la superficie.
        currentDimension = 0;
        if (hasSavedWorld) {
            char dimPath[80];
            WorldSave::getBaseFilePath("dim.dat", dimPath);
            FILE* dimFile = fopen(dimPath, "rb");
            if (dimFile) {
                int savedDim = 0;
                if (fread(&savedDim, sizeof(int), 1, dimFile) == 1 && savedDim == -1) currentDimension = -1;
                fclose(dimFile);
            }
        }
        WorldSave::setActiveDimension(currentDimension);
        Chunk::setDimension(currentDimension);
        Liquid::setHell(currentDimension == -1);
        timeInPortal = 0.0f;
        timeUntilPortal = 20;
        inPortalFlag = false;
        arrivalPending = false;
        healthState = HealthState();
        deathHandled = false;
        respawnPending = false;
        sleeping = false; sleepTimer = 0; respawnAtBed = false;
        bedSpawnSet = false;
        worldSpawnSet = false;
        AnimItems::reset();
        if (hasSavedWorld) {
            char healthPath[80];
            WorldSave::getBaseFilePath("health.dat", healthPath);
            FILE* healthFile = fopen(healthPath, "rb");
            if (healthFile) {
                int h = 0;
                if (fread(&h, sizeof(int), 1, healthFile) == 1 && h >= 1 && h <= Health::MAX_HEALTH) { healthState.health = h; healthState.prevHealth = h; }
                fclose(healthFile);
            }
            // bed.dat: el punto de reaparicion en una cama (flag, gx, gy, gz)
            char bedPath[80];
            WorldSave::getBaseFilePath("bed.dat", bedPath);
            FILE* bedFile = fopen(bedPath, "rb");
            if (bedFile) {
                int v[4] = { 0, 0, 0, 0 };
                if (fread(v, sizeof(int), 4, bedFile) == 4 && v[0] == 1) { bedSpawnSet = true; bedSpawnGx = v[1]; bedSpawnGy = v[2]; bedSpawnGz = v[3]; }
                fclose(bedFile);
            }
        }

        if (hasSavedWorld) {
            worldSeed = loadedMeta.seed;
        } else if (useCustomSeed) {
            worldSeed = customSeed;   // la que se escribio en Create world, o la "custom default seed" de Options
        } else {
            touchPosition seedTouch;
            touchRead(&seedTouch);
            worldSeed = entropyCounter ^ (seedTouch.px * 7919) ^ (seedTouch.py * 104729);
        }

        int startChunkX, startChunkZ;
        findLandChunkOffset(worldSeed, startChunkX, startChunkZ);
        if (currentDimension == -1) { // un mundo guardado dentro del Nether: la ventana de chunks arranca donde está el jugador
            startChunkX = (int)floorf(loadedMeta.playerX / (float)WorldConfig::SIZE_X + 0.5f);
            startChunkZ = (int)floorf(loadedMeta.playerZ / (float)WorldConfig::SIZE_Z + 0.5f);
        }
        chunkManager = new ChunkManager(worldSeed, startChunkX, startChunkZ);
        renderer.setupProjection(); glMatrixMode(GL_MODELVIEW);   // la niebla y el plano lejano dependen del modo de chunks que acaba de fijar el ChunkManager

        player = new Player();
        inventoryOpen = false;
        craftingOpen = false;
        inventorySelectedSlot = -1;
        craftingSelectedRecipe = -1;
        craftingUsingTable = false;
	furnaceOpen = false;
        openFurnace = nullptr;
        chestOpen = false;
        Furnace::clearAll();
        ChestStore::clearAll();
MinecartManager::clear();
BoatManager::clear(); Maps::reset(); Weather::reset(); Spawners::clear(); Clouds::reset(); Particles::clear();
PaintingManager::clear();
SignManager::clear();
HostileManager::clearWolves();
TntManager::clear();
Projectiles::clear(); FishingHook::clear();
Dispenser::clearQueue();
NoteBlocks::clearAll();
        if (hasSavedWorld) {
            char furnPath[80];
            WorldSave::getActiveFilePath("furnaces.dat", furnPath);
            Furnace::loadFromFile(furnPath);
            char chestPath[80];
            WorldSave::getActiveFilePath("chests.dat", chestPath);
            ChestStore::loadFromFile(chestPath);
{ char cartPath[80]; WorldSave::getActiveFilePath("carts.dat", cartPath); MinecartManager::loadFromFile(cartPath); }
{ char boatPath[80]; WorldSave::getActiveFilePath("boats.dat", boatPath); BoatManager::loadFromFile(boatPath); }
Weather::load();
{ char paintPath[80]; WorldSave::getActiveFilePath("paintings.dat", paintPath); PaintingManager::loadFromFile(paintPath); }
{ char signPath[80]; WorldSave::getActiveFilePath("signs.dat", signPath); SignManager::loadFromFile(signPath); }
{ char wolfPath[80]; WorldSave::getActiveFilePath("wolves.dat", wolfPath); HostileManager::loadWolves(wolfPath); }
{ char notePath[80]; WorldSave::getActiveFilePath("notes.dat", notePath); NoteBlocks::loadFromFile(notePath); }
        }
        if (hasSavedWorld) {
            player->x = loadedMeta.playerX;
            player->y = loadedMeta.playerY;
            player->z = loadedMeta.playerZ;
            player->yaw = loadedMeta.playerYaw;
            player->pitch = loadedMeta.playerPitch;
            gameTimeTicks = loadedMeta.gameTimeTicks;
            for (int i = 0; i < Inventory::TOTAL_SIZE; i++) {
                inventory->setSlotDirect(i, (BlockType)loadedMeta.hotbarType[i], loadedMeta.hotbarCount[i]);
                inventory->setSlotDurability(i, loadedMeta.hotbarDurability[i]);
            }
            if (loadedMeta.spawnSet) { worldSpawnX = loadedMeta.spawnX; worldSpawnZ = loadedMeta.spawnZ; worldSpawnSet = true; }   // los mundos viejos lo fijan en el primer tick
            for (int i = 0; i < Inventory::ARMOR_SLOTS; i++) {   // armadura puesta (campos al final de WorldMetadata; los mundos viejos traen ceros)
                inventory->setSlotDirect(Inventory::ARMOR_BASE + i, (BlockType)loadedMeta.armorType[i], loadedMeta.armorType[i] ? 1 : 0);
                inventory->setSlotDurability(Inventory::ARMOR_BASE + i, loadedMeta.armorDurability[i]);
            }
        } else {
            gameTimeTicks = 0; // mundo nuevo: siempre arranca de día, no hereda la hora de la sesión anterior
            // el inventario arranca vacío: lo de depuración ahora viene en los cofres grandes (opción "debug large chest")
            ensureInventoryMap(inventory);   // inventoryMap solo aplica al crear un mundo desde cero
        }

	lastSkylightStep = -1;
	HandAnimations::reset(); SteveModel::reset(player ? player->yaw : 0.0f);

        frameCounter = 0;
        dlMaxKB = 0;
        Prof::resetMinRam();
    };

    // Viaje por el portal (Minecraft.usePortal): x y z se dividen por 8 al ir al Nether y se multiplican por 8 al volver; la altura no
    // cambia. Se guarda lo de la dimensión actual, se crea el mundo de la otra y se espera a que se genere (pantalla de carga).
    // Falta Teleporter: por ahora el jugador aparece en la superficie del terreno, sin buscar ni construir un portal de llegada.
    bool keepEntitiesOnLoad = false;   // tras recargar por cambio de chunk mode: conservar mobs e items
    bool reloadWorldPending = false;   // recarga en la misma dimension y posicion (cambio de chunk mode en partida)
    auto changeDimension = [&](bool reload = false) {
        const int newDim = reload ? currentDimension : ((currentDimension == -1) ? 0 : -1);
        const unsigned int seed = chunkManager->getSeed();
        float nx = player->x, nz = player->z;
        if (!reload) { if (newDim == -1) { nx /= 8.0f; nz /= 8.0f; } else { nx *= 8.0f; nz *= 8.0f; } }

        state = STATE_LOADING;
        TopScreenMenu::suspend();
        LoadingScreen::show();

        chunkManager->saveModifiedChunks();
        char furnPath[80];
        WorldSave::getActiveFilePath("furnaces.dat", furnPath);
        Furnace::saveToFile(furnPath);
        char chestPath[80];
        WorldSave::getActiveFilePath("chests.dat", chestPath);
        ChestStore::saveToFile(chestPath);
{ char cartPath[80]; WorldSave::getActiveFilePath("carts.dat", cartPath); MinecartManager::saveToFile(cartPath); }
{ char boatPath[80]; WorldSave::getActiveFilePath("boats.dat", boatPath); BoatManager::saveToFile(boatPath); Maps::flush(); Weather::save(); }
{ char paintPath[80]; WorldSave::getActiveFilePath("paintings.dat", paintPath); PaintingManager::saveToFile(paintPath); }
{ char signPath[80]; WorldSave::getActiveFilePath("signs.dat", signPath); SignManager::saveToFile(signPath); }
{ char wolfPath[80]; WorldSave::getActiveFilePath("wolves.dat", wolfPath); HostileManager::saveWolves(wolfPath); }
{ char notePath[80]; WorldSave::getActiveFilePath("notes.dat", notePath); NoteBlocks::saveToFile(notePath); }

        delete chunkManager;
        chunkManager = nullptr;
        if (!reload) for (int i = 0; i < MAX_DROPPED_ITEMS; i++) droppedItems[i] = DroppedItem();
        PendingDrop discarded;
        while (PendingDrops::pop(discarded)) {}
        Furnace::clearAll();
        ChestStore::clearAll();
MinecartManager::clear();
BoatManager::clear(); Maps::reset(); Weather::reset(); Spawners::clear(); Clouds::reset(); Particles::clear();
PaintingManager::clear();
SignManager::clear();
HostileManager::clearWolves();
TntManager::clear();
Projectiles::clear(); FishingHook::clear();
Dispenser::clearQueue();
NoteBlocks::clearAll();
        furnaceOpen = false;
        openFurnace = nullptr;
        chestOpen = false;
        miningChunk = nullptr;
        miningDamage = 0.0f; miningHitWait = 0;

        currentDimension = newDim;
        WorldSave::setActiveDimension(newDim);
        WorldSave::ensureDimensionFolder();
        Chunk::setDimension(newDim);
        Liquid::setHell(newDim == -1);

        const int cx = (int)floorf(nx / (float)WorldConfig::SIZE_X + 0.5f);
        const int cz = (int)floorf(nz / (float)WorldConfig::SIZE_Z + 0.5f);
        chunkManager = new ChunkManager(seed, cx, cz);
        renderer.setupProjection(); glMatrixMode(GL_MODELVIEW);   // la niebla y el plano lejano dependen del modo de chunks que acaba de fijar el ChunkManager

        WorldSave::getActiveFilePath("furnaces.dat", furnPath);
        Furnace::loadFromFile(furnPath);
        WorldSave::getActiveFilePath("chests.dat", chestPath);
        ChestStore::loadFromFile(chestPath);
{ char cartPath[80]; WorldSave::getActiveFilePath("carts.dat", cartPath); MinecartManager::loadFromFile(cartPath); }
{ char boatPath[80]; WorldSave::getActiveFilePath("boats.dat", boatPath); BoatManager::loadFromFile(boatPath); }
Weather::load();
{ char paintPath[80]; WorldSave::getActiveFilePath("paintings.dat", paintPath); PaintingManager::loadFromFile(paintPath); }
{ char signPath[80]; WorldSave::getActiveFilePath("signs.dat", signPath); SignManager::loadFromFile(signPath); }
{ char wolfPath[80]; WorldSave::getActiveFilePath("wolves.dat", wolfPath); HostileManager::loadWolves(wolfPath); }
{ char notePath[80]; WorldSave::getActiveFilePath("notes.dat", notePath); NoteBlocks::loadFromFile(notePath); }

        player->x = nx;
        player->z = nz;
        arrivalPending = !reload;
        reloadWorldPending = reload;
        arrivalX = nx;
        arrivalZ = nz;
        timeInPortal = 0.0f;
        timeUntilPortal = 10; // Minecraft.usePortal: timeUntilPortal = 10 al viajar
        inPortalFlag = false;
        HandAnimations::reset(); SteveModel::reset(player ? player->yaw : 0.0f);
        lastSkylightStep = -1;
        frameCounter = 0;
    };

    // Minecraft.respawn: si el mundo no permite reaparecer (el Nether) el jugador pasa primero a la superficie; luego aparece en el punto de aparición
    // del mundo, el mismo que se busca al crear uno. Se guarda lo hecho, se vuelve a crear el mundo y se espera a que se genere (pantalla de carga).
    auto respawnPlayer = [&]() {
        const unsigned int seed = chunkManager->getSeed();
        state = STATE_LOADING;
        TopScreenMenu::suspend();
        LoadingScreen::show();

        chunkManager->saveModifiedChunks();
        char furnPath[80];
        WorldSave::getActiveFilePath("furnaces.dat", furnPath);
        Furnace::saveToFile(furnPath);
        char chestPath[80];
        WorldSave::getActiveFilePath("chests.dat", chestPath);
        ChestStore::saveToFile(chestPath);
{ char cartPath[80]; WorldSave::getActiveFilePath("carts.dat", cartPath); MinecartManager::saveToFile(cartPath); }
{ char boatPath[80]; WorldSave::getActiveFilePath("boats.dat", boatPath); BoatManager::saveToFile(boatPath); Maps::flush(); Weather::save(); }
{ char paintPath[80]; WorldSave::getActiveFilePath("paintings.dat", paintPath); PaintingManager::saveToFile(paintPath); }
{ char signPath[80]; WorldSave::getActiveFilePath("signs.dat", signPath); SignManager::saveToFile(signPath); }
{ char wolfPath[80]; WorldSave::getActiveFilePath("wolves.dat", wolfPath); HostileManager::saveWolves(wolfPath); }
{ char notePath[80]; WorldSave::getActiveFilePath("notes.dat", notePath); NoteBlocks::saveToFile(notePath); }

        delete chunkManager;
        chunkManager = nullptr;
        // Entity en el mundo: los objetos que soltaste al morir se quedan donde cayeron (esperan quietos hasta que su chunk se cargue). Solo se pierden los del Nether, porque se reaparece en la superficie
        if (currentDimension != 0) { for (int i = 0; i < MAX_DROPPED_ITEMS; i++) droppedItems[i] = DroppedItem(); }
        PendingDrop discarded;
        while (PendingDrops::pop(discarded)) {}
        Furnace::clearAll();
        ChestStore::clearAll();
MinecartManager::clear();
BoatManager::clear(); Maps::reset(); Weather::reset(); Spawners::clear(); Clouds::reset(); Particles::clear();
PaintingManager::clear();
SignManager::clear();
HostileManager::clearWolves();
TntManager::clear();
Projectiles::clear(); FishingHook::clear();
Dispenser::clearQueue();
NoteBlocks::clearAll();
        furnaceOpen = false;
        openFurnace = nullptr;
        chestOpen = false;
        miningChunk = nullptr;
        miningDamage = 0.0f; miningHitWait = 0;

        currentDimension = 0;
        WorldSave::setActiveDimension(0);
        WorldSave::ensureDimensionFolder();
        Chunk::setDimension(0);
        Liquid::setHell(false);

        int startChunkX, startChunkZ;
        findLandChunkOffset(seed, startChunkX, startChunkZ);
        respawnAtBed = false;
        if (bedSpawnSet) {   // Minecraft.respawn: junto a la cama donde dormiste (si sigue ahi); la ventana de chunks arranca en su chunk
            startChunkX = (int)floorf((float)bedSpawnGx / (float)WorldConfig::SIZE_X);
            startChunkZ = (int)floorf((float)bedSpawnGz / (float)WorldConfig::SIZE_Z);
            respawnAtBed = true;
        }
        chunkManager = new ChunkManager(seed, startChunkX, startChunkZ);
        renderer.setupProjection(); glMatrixMode(GL_MODELVIEW);   // la niebla y el plano lejano dependen del modo de chunks que acaba de fijar el ChunkManager

        WorldSave::getActiveFilePath("furnaces.dat", furnPath);
        Furnace::loadFromFile(furnPath);
        WorldSave::getActiveFilePath("chests.dat", chestPath);
        ChestStore::loadFromFile(chestPath);
{ char cartPath[80]; WorldSave::getActiveFilePath("carts.dat", cartPath); MinecartManager::loadFromFile(cartPath); }
{ char boatPath[80]; WorldSave::getActiveFilePath("boats.dat", boatPath); BoatManager::loadFromFile(boatPath); }
Weather::load();
{ char paintPath[80]; WorldSave::getActiveFilePath("paintings.dat", paintPath); PaintingManager::loadFromFile(paintPath); }
{ char signPath[80]; WorldSave::getActiveFilePath("signs.dat", signPath); SignManager::loadFromFile(signPath); }
{ char wolfPath[80]; WorldSave::getActiveFilePath("wolves.dat", wolfPath); HostileManager::loadWolves(wolfPath); }
{ char notePath[80]; WorldSave::getActiveFilePath("notes.dat", notePath); NoteBlocks::loadFromFile(notePath); }

        delete player;
        player = new Player();       // cuerpo entero otra vez, quieto; el inventario sigue siendo el que quedó tras soltarlo todo
        healthState = HealthState(); // vida llena
        sleeping = false; sleepTimer = 0;
        deathHandled = false;
        respawnPending = true;
        timeInPortal = 0.0f;
        timeUntilPortal = 20;
        inPortalFlag = false;
        arrivalPending = false;
        HandAnimations::reset(); SteveModel::reset(player ? player->yaw : 0.0f);
        lastSkylightStep = -1;
        frameCounter = 0;
    };

    // Bloque en una celda global (STONE si el chunk no está cargado)
    auto typeAtCell = [&](int gx, int gy, int gz) -> BlockType {
        if (gy < 0 || gy >= WorldConfig::SIZE_Y) return BlockType::AIR;
        const int SX = WorldConfig::SIZE_X, SZ = WorldConfig::SIZE_Z;
        int cx = (gx >= 0) ? gx / SX : -((-gx + SX - 1) / SX);
        int cz = (gz >= 0) ? gz / SZ : -((-gz + SZ - 1) / SZ);
        Chunk* c = chunkManager->getChunkByChunkCoord(cx, cz);
        if (!c || !c->isGenerationComplete()) return BlockType::STONE;
        return c->getBlockType(gx - cx * SX, gy, gz - cz * SZ);
    };

    // Metadata de una celda global (0 si el chunk no está cargado)
    auto metaAtCell = [&](int gx, int gy, int gz) -> int {
        if (gy < 0 || gy >= WorldConfig::SIZE_Y) return 0;
        const int SX = WorldConfig::SIZE_X, SZ = WorldConfig::SIZE_Z;
        int cx = (gx >= 0) ? gx / SX : -((-gx + SX - 1) / SX);
        int cz = (gz >= 0) ? gz / SZ : -((-gz + SZ - 1) / SZ);
        Chunk* c = chunkManager->getChunkByChunkCoord(cx, cz);
        if (!c || !c->isGenerationComplete()) return 0;
        return c->getLiquidMeta(gx - cx * SX, gy, gz - cz * SZ);
    };

    // EntityPlayer.wakeUpPlayer: donde queda el jugador al levantarse (los ojos). Junto a la cama si hay sitio; si la cama ya no esta, donde estaba su celda.
    auto computeWakeSpot = [&](float& nx, float& ny, float& nz) {
        nx = player->x; ny = player->y; nz = player->z;
        int ox, oy, oz;
        if (typeAtCell(bedGx, bedGy, bedGz) == BlockType::BED_BLOCK) {
            if (!Sleeping::nearestEmpty(typeAtCell, bedGx, bedGy, bedGz, bedDir, ox, oy, oz)) { ox = bedGx; oy = bedGy + 1; oz = bedGz; }   // sin sitio: encima de la cama
        } else {
            ox = bedGx; oy = bedGy; oz = bedGz;
        }
        Sleeping::standingEye(ox, oy, oz, nx, ny, nz);
    };

    // EntityPlayer.wakeUpPlayer(resetTimer, updateFlag, setSpawn): resetTimer = el velo se va de golpe; setSpawn = la cama pasa a ser el punto de reaparicion
    auto wakeUp = [&](bool resetTimer, bool setSpawn) {
        if (!sleeping) return;
        float nx, ny, nz;
        computeWakeSpot(nx, ny, nz);
        sleeping = false;
        sleepTimer = resetTimer ? 0 : Sleeping::FULL_ASLEEP;
        if (setSpawn) { bedSpawnSet = true; bedSpawnGx = bedGx; bedSpawnGy = bedGy; bedSpawnGz = bedGz; }
        player->teleportTo(nx, ny, nz);
    };

    // BlockBed.blockActivated + EntityPlayer.sleepInBedAt. (gx, gy, gz): la celda de cama que se tocó
    auto useBed = [&](int gx, int gy, int gz) {
        int meta = metaAtCell(gx, gy, gz);
        if (!(meta & 8)) {   // se actua sobre la mitad con el bit 8
            int dx, dz;
            Partial::bedOffset(meta & 3, dx, dz);
            gx += dx; gz += dz;
            if (typeAtCell(gx, gy, gz) != BlockType::BED_BLOCK) return;
            meta = metaAtCell(gx, gy, gz);
        }
        const int dir = meta & 3;

        if (currentDimension == -1) {   // !canRespawnHere: la cama explota (fuerza 5, con fuego)
            int dx, dz;
            Partial::bedOffset(dir, dx, dz);
            chunkManager->setBlockGlobal(gx, gy, gz, BlockType::AIR);
            gx += dx; gz += dz;   // la Beta mueve aqui el centro de la explosion
            if (typeAtCell(gx, gy, gz) == BlockType::BED_BLOCK) chunkManager->setBlockGlobal(gx, gy, gz, BlockType::AIR);
            HostileManager::explodeAt((double)(gx - WorldConfig::SIZE_X / 2) + 0.5, (double)(gy - 1), (double)(gz - WorldConfig::SIZE_Z / 2) + 0.5, 5.0f, true);
            return;
        }

        // sleepInBedAt: NOT_POSSIBLE_NOW de dia (sin chat no hay mensaje) y TOO_FAR_AWAY a mas de 3 de ancho, 2 de alto
        if (sleeping || healthState.dead) return;
        if (computeSkylightSubtract(gameTimeTicks) < 4) return;
        if (fabsf(player->x - (float)(gx - 4)) > 3.0f || fabsf((player->y + 0.5f) - (float)(gy - 1)) > 2.0f || fabsf(player->z - (float)(gz - 4)) > 3.0f) return;

        float ex, ey, ez;
        Sleeping::eyePosition(gx, gy, gz, dir, ex, ey, ez);
        sleeping = true;
        sleepTimer = 0;
        bedGx = gx; bedGy = gy; bedGz = gz; bedDir = dir;
        sleepLastHealth = healthState.health;
        player->teleportTo(ex, ey, ez);
    };

    // BlockChest.blockActivated: abre el cofre (y su pareja, si es doble). No se abre con un bloque sólido encima del cofre o de su pareja.
    // Opción "debug large chest": dos cofres grandes con todos los bloques y objetos, cerca del jugador y en su chunk (ver DebugChests.h).
    // En el océano, o si no hay un sitio llano, no se genera nada.
    auto placeDebugChests = [&]() {
        const int SX = WorldConfig::SIZE_X, SZ = WorldConfig::SIZE_Z;
        const int pgx = (int)floorf(player->x + 4.0f), pgz = (int)floorf(player->z + 4.0f);
        const int x0 = (int)floorf((float)pgx / (float)SX) * SX, z0 = (int)floorf((float)pgz / (float)SZ) * SZ;

        DebugChests::Site sites[4];
        if (!DebugChests::findSite(typeAtCell, x0, z0, SX, SZ, pgx, pgz, 3.0f, nullptr, sites[0])) return;
        int siteCount = 1;
        if (DebugChests::findSite(typeAtCell, x0, z0, SX, SZ, pgx, pgz, 5.0f, &sites[0], sites[1])) siteCount = 2;
        if (siteCount == 2 && DebugChests::findSite(typeAtCell, x0, z0, SX, SZ, pgx, pgz, 7.0f, &sites[0], sites[2], &sites[1])) siteCount = 3;
        if (siteCount == 3 && DebugChests::findSite(typeAtCell, x0, z0, SX, SZ, pgx, pgz, 9.0f, &sites[0], sites[3], &sites[1], &sites[2])) siteCount = 4;

        BlockType items[DebugChests::MAX_ITEMS];
        const int itemCount = DebugChests::buildItemList(items);

        for (int s = 0; s < siteCount; s++) {
            const DebugChests::Site& st = sites[s];
            const int hx = st.gx + (st.alongX ? 1 : 0), hz = st.gz + (st.alongX ? 0 : 1);

            // la mitad baja del terreno se rellena con tierra hasta la altura de los cofres
            for (int y = st.groundA + 1; y < st.gy; y++) chunkManager->setBlockGlobal(st.gx, y, st.gz, BlockType::DIRT);
            for (int y = st.groundB + 1; y < st.gy; y++) chunkManager->setBlockGlobal(hx, y, hz, BlockType::DIRT);

            chunkManager->setBlockGlobal(st.gx, st.gy, st.gz, BlockType::CHEST);
            chunkManager->setBlockGlobal(hx, st.gy, hz, BlockType::CHEST);

            // part[0] (casillas 0..26) es la mitad de menor x o z, como en el menú del cofre doble
            ChestData* halves[2] = { ChestStore::findOrCreate(st.gx, st.gy, st.gz), ChestStore::findOrCreate(hx, st.gy, hz) };
            for (int i = 0; i < 54; i++) {
                const int itemIndex = s * 54 + i;
                if (itemIndex >= itemCount || !halves[i / 27]) continue;
                const BlockType t = items[itemIndex];
                ChestSlot& slot = halves[i / 27]->slot[i % 27];
                slot.type = (u8)t;
                slot.count = (u8)maxStackFor(t);   // pilas completas; herramientas y baldes, de a uno
                slot.durability = (u16)(hasDurability(t) ? getToolMaxDurability(t) : 0);
            }
        }
    };

    auto openChest = [&](int gx, int gy, int gz) {
        static const int D[4][2] = { {-1, 0}, {1, 0}, {0, -1}, {0, 1} };
        if (isNormalCube(typeAtCell(gx, gy + 1, gz))) return;
        for (int i = 0; i < 4; i++) {
            if (typeAtCell(gx + D[i][0], gy, gz + D[i][1]) == BlockType::CHEST &&
                isNormalCube(typeAtCell(gx + D[i][0], gy + 1, gz + D[i][1]))) return;
        }

        ChestData* self = ChestStore::findOrCreate(gx, gy, gz);
        if (!self) return;
        ChestView v;
        v.rows = 3;
        v.part[0] = self;
        v.part[1] = nullptr;
        for (int i = 0; i < 4; i++) {
            if (typeAtCell(gx + D[i][0], gy, gz + D[i][1]) != BlockType::CHEST) continue;
            ChestData* other = ChestStore::findOrCreate(gx + D[i][0], gy, gz + D[i][1]);
            if (!other) return;
            v.rows = 6;
            // InventoryLargeChest: arriba va el cofre de menor x o z (el del oeste o el del norte)
            if (i == 0 || i == 2) { v.part[0] = other; v.part[1] = self; }
            else                  { v.part[0] = self;  v.part[1] = other; }
        }

        chestView = v;
        chestOpen = true;
        chestSelectedSlot = 0;
        craftingOpen = false;
        inventoryOpen = true; // la pantalla de abajo reutiliza el inventario
        inventorySelectedSlot = -1;
        CraftingScreen::showChestTop(chestView, chestSelectedSlot);
        InventoryScreen::show(*inventory, selectedSlot, -1);
    };

    // BlockDispenser.blockActivated: el menu del dispensador (9 casillas en 3 x 3 arriba, el inventario abajo)
    auto openDispenser = [&](int gx, int gy, int gz) {
        ChestData* self = ChestStore::findOrCreate(gx, gy, gz);
        if (!self) return;
        ChestView v;
        v.rows = 1;
        v.dispenser = true;
        v.part[0] = self;
        v.part[1] = nullptr;
        chestView = v;
        chestOpen = true;
        chestSelectedSlot = 0;
        craftingOpen = false;
        inventoryOpen = true;
        inventorySelectedSlot = -1;
        CraftingScreen::showChestTop(chestView, chestSelectedSlot);
        InventoryScreen::show(*inventory, selectedSlot, -1);
    };

    // EntityMinecart.interact (vagoneta con cofre): el menu del cofre con las 27 casillas de la vagoneta (guardadas en ChestStore con su identificador)
    auto openCartChest = [&](int uid) {
        if (uid < 0) return;
        ChestData* self = ChestStore::findOrCreate(uid, MinecartManager::CHEST_KEY_Y, 0);
        if (!self) return;
        ChestView v;
        v.rows = 3;
        v.part[0] = self;
        v.part[1] = nullptr;
        chestView = v;
        chestOpen = true;
        chestSelectedSlot = 0;
        craftingOpen = false;
        inventoryOpen = true;
        inventorySelectedSlot = -1;
        CraftingScreen::showChestTop(chestView, chestSelectedSlot);
        InventoryScreen::show(*inventory, selectedSlot, -1);
    };

    // BlockChest.onBlockRemoval: lo que había en una casilla sale como ítems sueltos (pilas de 10 a 30); lo que no entra en la lista
    // de ítems va al inventario y, si tampoco cabe, se pierde (igual que con el horno)
    auto spillStack = [&](BlockType type, int count, int durability, float bx, float by, float bz) {
        const bool tool = hasDurability(type);
        int left = count;
        while (left > 0) {
            int freeIdx = -1;
            for (int i = 0; i < MAX_DROPPED_ITEMS; i++) {
                if (!droppedItems[i].isActive()) { freeIdx = i; break; }
            }
            if (freeIdx == -1) break;
            int piece = tool ? 1 : (10 + rand() % 21);
            if (piece > left) piece = left;
            if (tool) droppedItems[freeIdx].spawn(bx, by, bz, type, true, 0.0f, 0.1f, 0.0f, durability);
            else      droppedItems[freeIdx].spawn(bx, by, bz, type, false, 0.0f, 0.0f, 0.0f, -1, piece);
            left -= piece;
        }
        if (tool) {
            while (left > 0 && inventory->addToolWithDurability(type, durability)) left--;
        } else if (left > 0) {
            bool usedNewSlot;
            left = inventory->addItem(type, left, usedNewSlot);
        }
    };

    auto closeFurnaceMenu = [&]() {
        furnaceOpen = false;
        openFurnace = nullptr;
        chestOpen = false;
        inventoryOpen = false;
        inventorySelectedSlot = -1;
        videoSetMode(MODE_0_3D);
        if (consoleVisible) { consoleDemoInit(); } else { HotbarIcons::render(*inventory, selectedSlot); }
    };

    while (pmMainLoop()) {
	u32 frameStartTicks = Prof::now();
        entropyCounter++;

        scanKeys();
        if (state == STATE_PLAYING && player && chunkManager) {
            Music::setListener(player->x, player->y, player->z, player->yaw);   // el disco de la jukebox suena desde su bloque
            int rgx, rgy, rgz;
            if (Music::recordCell(rgx, rgy, rgz)) {   // la jukebox desaparecio sin sacar el disco (TNT...): se calla, como BlockJukeBox.onBlockRemoval
                const int rcx = rgx >= 0 ? rgx / WorldConfig::SIZE_X : -((-rgx + WorldConfig::SIZE_X - 1) / WorldConfig::SIZE_X);
                const int rcz = rgz >= 0 ? rgz / WorldConfig::SIZE_Z : -((-rgz + WorldConfig::SIZE_Z - 1) / WorldConfig::SIZE_Z);
                if (chunkManager->getChunkByChunkCoord(rcx, rcz) && Partial::jukeboxRecord(*chunkManager, rgx, rgy, rgz) == BlockType::AIR) Music::stopRecordAt(rgx, rgy, rgz);
            }
        }
        Music::update(state == STATE_PLAYING);   // background music (streams from /mcdsefiles/music); stops by itself outside the game
        u32 keys_down = keysDown();
        u32 keys_held = keysHeld();

        // El panorama vive en el menú principal, en opciones y en la selección de mundo: en cualquier otro estado se libera (y el juego recupera su VRAM y su proyección)
        if (state != STATE_MENU && state != STATE_OPTIONS && state != STATE_TEXTUREPACKS && state != STATE_WORLD_SELECT && state != STATE_CREATE_WORLD) TopScreenMenu::suspend();

        if (state == STATE_MENU) {
            TopScreenMenu::tick();
            touchPosition menuTouch;
            touchRead(&menuTouch);
            bool menuTouching = (keys_down & KEY_TOUCH) != 0;

            if (menuTouching && SubScreenMenu::isNewLoadTouched(menuTouch.px, menuTouch.py)) {
                Sound::uiClick();
                state = STATE_WORLD_SELECT;
                WorldSelectMenu::init();
            } else if (menuTouching && SubScreenMenu::isOptionsTouched(menuTouch.px, menuTouch.py)) {
                Sound::uiClick();
                state = STATE_OPTIONS;
                SettingsMenu::open();
            } else if (menuTouching && SubScreenMenu::isTexturePacksTouched(menuTouch.px, menuTouch.py)) {
                Sound::uiClick();
                state = STATE_TEXTUREPACKS;
                TexturePackMenu::open(false);
            } else if (menuTouching && SubScreenMenu::isSkinsTouched(menuTouch.px, menuTouch.py)) {
                Sound::uiClick();
                state = STATE_TEXTUREPACKS;   // el mismo menu de lista, con las skins
                TexturePackMenu::open(true);
            }

            swiWaitForVBlank();
            continue;
        }

        if (state == STATE_TEXTUREPACKS) {
            TopScreenMenu::tick();
            if (TexturePackMenu::update(keys_down)) {
                state = STATE_MENU;
                SubScreenMenu::init(WorldSave::worldSlotExists(1));
            }

            swiWaitForVBlank();
            continue;
        }

        if (state == STATE_OPTIONS) {
            TopScreenMenu::tick();
            if (SettingsMenu::update(keys_down)) {
                autoJumpEnabled = Options::get().autoJump;
                controlMode = Options::get().controlMode;
                state = STATE_MENU;
                SubScreenMenu::init(WorldSave::worldSlotExists(1));
            }

            swiWaitForVBlank();
            continue;
        }

        if (state == STATE_WORLD_SELECT) {
            TopScreenMenu::tick();
            bool wsTouchDown = (keys_down & KEY_TOUCH) != 0;

            if (wsTouchDown) {
                touchPosition wsTouch;
                touchRead(&wsTouch);

                if (WorldSelectMenu::isLeftArrowTouched(wsTouch.px, wsTouch.py)) { Sound::uiClick();
                    WorldSelectMenu::selectPrevious();
                } else if (WorldSelectMenu::isRightArrowTouched(wsTouch.px, wsTouch.py)) { Sound::uiClick();
                    WorldSelectMenu::selectNext();
                } else if (WorldSelectMenu::isBackTouched(wsTouch.px, wsTouch.py)) { Sound::uiClick();
                    state = STATE_MENU;
                    SubScreenMenu::init(WorldSave::worldSlotExists(1));
                    TopScreenMenu::init();
                } else if (WorldSelectMenu::isDeleteTouched(wsTouch.px, wsTouch.py)) { Sound::uiClick();
                    int selected = WorldSelectMenu::getSelectedWorldNum();
                    if (selected > 0) {
                        char worldPath[64];
                        WorldSave::getWorldFolderPath(selected, worldPath);
                        WorldSave::deleteWorld(worldPath);
                        WorldSelectMenu::refresh();
                    }
                } else if (WorldSelectMenu::isCreateTouched(wsTouch.px, wsTouch.py)) { Sound::uiClick();
                    state = STATE_CREATE_WORLD;
                    NewWorldMenu::open();
                } else if (WorldSelectMenu::isPlayTouched(wsTouch.px, wsTouch.py)) { Sound::uiClick();
                    int selected = WorldSelectMenu::getSelectedWorldNum();
                    if (selected > 0) {
                        startWorld(true, selected, nullptr, false, 0);
                    }
                }
            }

            swiWaitForVBlank();
            continue;
        }

        if (state == STATE_CREATE_WORLD) {
            TopScreenMenu::tick();
            const NewWorldMenu::Result cwResult = NewWorldMenu::update(keys_down);
            if (cwResult == NewWorldMenu::RES_CANCEL) {
                state = STATE_WORLD_SELECT;
                WorldSelectMenu::init();
            } else if (cwResult == NewWorldMenu::RES_CREATE) {
                // Seed: la escrita aqui; si se dejo vacia y las opciones piden una custom, esa; si no, aleatoria
                unsigned int cwSeed = 0;
                bool cwUseSeed = NewWorldMenu::getSeed(cwSeed);
                if (!cwUseSeed && Options::get().seedMode == 1 && Options::get().customSeedSet) {
                    cwUseSeed = true;
                    cwSeed = Options::get().customSeed;
                }
                startWorld(false, 0, NewWorldMenu::getName(), cwUseSeed, cwSeed);
            }
            swiWaitForVBlank();
            continue;
        }

        if (state == STATE_LOADING) {
            Governor::setEnabled(false);       // cargando: lo que importa es terminar
            Chunk::setBudgetPercent(100);
            chunkManager->update(player->x, player->z);

            if (chunkManager->isGenerationComplete()) {
                if (arrivalPending) {
                    // Llegada por un portal (Teleporter): el portal más cercano en los chunks cargados o, si no hay, uno nuevo
                    arrivalPending = false;
                    float tx, ty, tz;
                    if (chunkManager->teleportToPortal(arrivalX, player->y, arrivalZ, (unsigned int)gameTimeTicks * 2654435761u + 12345u, tx, ty, tz)) {
                        player->teleportTo(tx, ty, tz);
                    } else {
                        player->x = arrivalX;
                        player->z = arrivalZ;
                    }
                } else if (reloadWorldPending) {
                    reloadWorldPending = false; keepEntitiesOnLoad = true;   // cambio de chunk mode en partida: el jugador sigue donde estaba
                } else if (!hasSavedWorld || respawnPending) {
                    const bool wasRespawn = respawnPending;
                    respawnPending = false;
                    Chunk* center = chunkManager->getCenterChunk();

                    int spawnXi = WorldConfig::SIZE_X / 2;
                    int spawnZi = WorldConfig::SIZE_Z / 2;

                    float centerWorldX = center->gridToWorldX(spawnXi);
                    float centerWorldZ = center->gridToWorldZ(spawnZi);

                    auto isRealLand = [&](float testX, float testZ) -> bool {
                        if (chunkManager->getBiomeAt(testX, testZ) == BiomeType::OCEAN) return false;

                        Chunk* c = chunkManager->getChunkAt(testX, testZ);
                        if (!c) return false;

                        int cxi, cyi, czi;
                        float probeY = c->gridToWorldY(0);
                        if (!c->worldToGrid(testX, probeY, testZ, cxi, cyi, czi)) return false;

                        int highestY = c->getHighestSolidY(cxi, czi);
                        if (highestY < 0) return false;

                        BlockType surfaceType = c->getBlockType(cxi, highestY, czi);
                        return !isLiquidBlock(surfaceType);
                    };

                    bool foundLand = false;
                    float foundWorldX = centerWorldX;
                    float foundWorldZ = centerWorldZ;

                    if (isRealLand(centerWorldX, centerWorldZ)) {
                        foundLand = true;
                    } else {
                        const int SEARCH_RADIUS_BLOCKS = 10;
                        for (int radius = 1; radius <= SEARCH_RADIUS_BLOCKS && !foundLand; radius++) {
                            for (int dx = -radius; dx <= radius && !foundLand; dx++) {
                                for (int dz = -radius; dz <= radius && !foundLand; dz++) {
                                    float testWorldX = centerWorldX + dx;
                                    float testWorldZ = centerWorldZ + dz;

                                    if (isRealLand(testWorldX, testWorldZ)) {
                                        foundWorldX = testWorldX;
                                        foundWorldZ = testWorldZ;
                                        foundLand = true;
                                    }
                                }
                            }
                        }
                    }

                    Chunk* spawnChunk = chunkManager->getChunkAt(foundWorldX, foundWorldZ);
                    if (spawnChunk) {
                        int sxi, syi, szi;
                        float probeY = spawnChunk->gridToWorldY(0);
                        if (spawnChunk->worldToGrid(foundWorldX, probeY, foundWorldZ, sxi, syi, szi)) {
                            int highestY = spawnChunk->getHighestSolidY(sxi, szi);
                            if (highestY >= 0) {
                                float spawnWorldY = spawnChunk->gridToWorldY(highestY);
                                player->y = spawnWorldY + 0.5f + 1.62f;
                                player->x = foundWorldX;
                                player->z = foundWorldZ;
                            }
                        }
                    }
                    if (!wasRespawn) { worldSpawnX = player->x; worldSpawnZ = player->z; worldSpawnSet = true; }   // mundo nuevo: la brujula apunta aqui
                    if (!wasRespawn && Options::get().debugLargeChest) placeDebugChests();

                    if (wasRespawn && respawnAtBed) {
                        // Minecraft.respawn + EntityPlayer.func_25060_a: junto a la cama si sigue en pie y tiene un sitio libre al lado; si no, al punto del mundo
                        respawnAtBed = false;
                        int ox, oy, oz;
                        if (typeAtCell(bedSpawnGx, bedSpawnGy, bedSpawnGz) == BlockType::BED_BLOCK &&
                            Sleeping::nearestEmpty(typeAtCell, bedSpawnGx, bedSpawnGy, bedSpawnGz, metaAtCell(bedSpawnGx, bedSpawnGy, bedSpawnGz) & 3, ox, oy, oz)) {
                            float bx, by, bz;
                            Sleeping::standingEye(ox, oy, oz, bx, by, bz);
                            player->teleportTo(bx, by, bz);
                        } else {
                            bedSpawnSet = false;   // la cama ya no esta (tile.bed.notValid): se vuelve a cargar para aparecer en el punto del mundo
                            respawnPlayer();
                            continue;
                        }
                    }
                }
		chunkManager->forceFullRecalculation();
        if (!keepEntitiesOnLoad) SheepManager::clear((unsigned int)chunkManager->getSeed() ^ (unsigned int)gameTimeTicks);   // sin ovejas al entrar; aparecen solas
        keepEntitiesOnLoad = false;
        Pistons::reset();
        Redstone::reset();                                                                         // sin ticks de redstone pendientes del mundo anterior
        Redstone::setEntityEnumerator(redstoneEnumerateEntities, &redstoneCtx);
        Redstone::setMinecartEnumerator(redstoneEnumerateMinecarts, nullptr);
        SheepManager::setActive(currentDimension == 0);                                            // ninguna en el Nether

                state = STATE_PLAYING;

                videoSetMode(MODE_0_3D);
                if (consoleVisible) { consoleDemoInit(); } else { HotbarIcons::render(*inventory, selectedSlot); } // antes: GameplayHotbar::show();
                consoleVisible = false;;
            }

            swiWaitForVBlank();
            continue;
        }

        // --- STATE_PLAYING ---
        // Game Over (GuiGameOver): Respawn reaparece; Title menu sale al menú sin guardar, con lo que el mundo vuelve a antes de morir
        bool titleMenuRequested = false;
        if (healthState.dead && (keys_down & KEY_TOUCH)) {
            touchPosition goTouch;
            touchRead(&goTouch);
            if (GameOverScreen::isRespawnTouched(goTouch.px, goTouch.py)) { Sound::uiClick(); respawnPlayer(); continue; }
            if (GameOverScreen::isTitleTouched(goTouch.px, goTouch.py)) { Sound::uiClick(); titleMenuRequested = true; }
        }
        static bool touchSuppressed = false;   // el toque que abre o cierra la debug screen no debe hacer nada mas hasta soltar
        if (!(keys_held & KEY_TOUCH)) touchSuppressed = false;
        // Menu de pausa (START o minibutton1): el juego no se congela. Si el jugador muere con el menu abierto, lo reemplaza la pantalla de Game Over.
        if (healthState.dead) { paused = false; pauseView = 0; }
        {
            touchPosition pTouch;
            touchRead(&pTouch);
            const bool canPause = !inventoryOpen && !craftingOpen && !furnaceOpen && !chestOpen;
            // Debug screen: minibutton3 la abre; con ella abierta, tocar cualquier parte de la pantalla la cierra
            if (!paused && !healthState.dead && (keys_down & KEY_TOUCH)) {
                if (consoleVisible) {
                    consoleVisible = false;
                    GameplayHotbar::invalidate();   // render() recrea el fondo de la hotbar en la pantalla de abajo; la de arriba no se toca
                    HotbarIcons::render(*inventory, selectedSlot);
                    touchSuppressed = true;
                } else if (canPause && GameplayHotbar::isMiniButton3Touched(pTouch.px, pTouch.py)) {
                    Sound::uiClick();
                    consoleVisible = true;
                    Prof::flushSpikes();   // al abrir la consola (un momento quieto) se guardan los picos anotados
                    consoleDemoInit();
                    touchSuppressed = true;
                }
            }
            if ((keys_down & KEY_TOUCH) && !paused && !healthState.dead && canPause && !consoleVisible && !touchSuppressed &&
                GameplayHotbar::isMiniButton2Touched(pTouch.px, pTouch.py)) {
                thirdPerson = !thirdPerson;   // minibutton2: tercera persona
                Sound::uiClick();
            }
            const bool miniPause = (keys_down & KEY_TOUCH) && !paused && !touchSuppressed && GameplayHotbar::isMiniButton1Touched(pTouch.px, pTouch.py);
            const bool closeNow = paused && pauseView == 1 && (keys_down & KEY_START);
            auto closePause = [&]() {
                paused = false; pauseView = 0;
                if (consoleVisible) { consoleDemoInit(); }
                else {
                    GameplayHotbar::invalidate();   // la imagen de pausa es de 8 bits: render() vuelve a crear el fondo de la hotbar (16 bits) sin tocar la pantalla de arriba
                    HotbarIcons::render(*inventory, selectedSlot);
                }
            };
            if (!healthState.dead && !paused && canPause && ((keys_down & KEY_START) || miniPause)) {
                // The debug screen and the pause menu share the bottom screen: with the debug screen open, its print every 30 frames (and every
                // consoleDemoInit while it is "visible") wrote over the pause image and the Options list, so the menu looked frozen. Pausing closes it.
                consoleVisible = false;
                paused = true; pauseView = 1;
                Sound::uiClick();
                PauseScreen::show();
            } else if (closeNow) {
                Sound::uiClick();
                closePause();
            } else if (paused && pauseView == 1 && (keys_down & KEY_TOUCH)) {
                PauseScreen::Button pb = PauseScreen::touched(pTouch.px, pTouch.py);
                if (pb != PauseScreen::BTN_NONE) Sound::uiClick();
                if (pb == PauseScreen::BTN_BACK) closePause();
                else if (pb == PauseScreen::BTN_OPTIONS) { pauseView = 2; chunkModeAtOptions = Options::get().chunkMode; SettingsMenu::open(); }
                else if (pb == PauseScreen::BTN_SAVE) saveRequested = true;
                else if (pb == PauseScreen::BTN_QUIT) quitRequested = true;
            } else if (paused && pauseView == 2) {
                if (SettingsMenu::update(keys_down)) {
                    autoJumpEnabled = Options::get().autoJump;
                    controlMode = Options::get().controlMode;
                    if (Options::get().chunkMode != chunkModeAtOptions) reloadChunkModeRequested = true;   // se aplica al instante: se recarga el mundo con el modo nuevo
                    pauseView = 1;
                    PauseScreen::show();
                }
            }
        }
        if (reloadChunkModeRequested && !quitRequested && !titleMenuRequested) {
            reloadChunkModeRequested = false;
            paused = false; pauseView = 0;
            changeDimension(true);
            continue;
        }
        if (quitRequested || titleMenuRequested) {
            quitRequested = false; paused = false; pauseView = 0;
            Prof::flushSpikes();   // se guardan los picos anotados antes de salir
            state = STATE_MENU;

            WorldSave::endSession();

            delete chunkManager;
            delete player;
            delete inventory;
            chunkManager = nullptr;
            player = nullptr;
            inventory = nullptr;

            SubScreenMenu::init(WorldSave::worldSlotExists(1));
            TopScreenMenu::init();

            continue;
        }

        Governor::setEnabled(true);
        Chunk::setBudgetPercent(Governor::budgetPercent());   // si el cuadro anterior se pasó, los presupuestos se recortan
        { PROF(Prof::UPDATE); chunkManager->update(player->x, player->z); }

        touchPosition touchXY;
        touchRead(&touchXY);
        bool touching = !healthState.dead && !paused && !touchSuppressed && !consoleVisible && (keys_held & KEY_TOUCH) != 0; // muerto: la pantalla de abajo es la de Game Over

        // Un toque que empezo en la zona de camara y se arrastra sobre un boton no lo activa (igual que los minibotones, que solo cuentan al tocar)
        static bool camDragTouch = false;
        if (keys_down & KEY_TOUCH) camDragTouch = !inventoryOpen && !craftingOpen && controlMode != 1 && GameplayHotbar::isTouchZone(touchXY.px, touchXY.py);
        if (!(keys_held & KEY_TOUCH)) camDragTouch = false;

        bool btn1Touched = touching && !camDragTouch && GameplayHotbar::isButton1Touched(touchXY.px, touchXY.py);
        bool btn3Touched = touching && !camDragTouch && GameplayHotbar::isButton3Touched(touchXY.px, touchXY.py);
        static bool btn3WasTouched = false;
        if (btn3Touched && !btn3WasTouched && !inventoryOpen && !craftingOpen) { Sound::uiClick(); player->setSneak(!player->isSneaking()); }   // boton Crouch: alterna
        btn3WasTouched = btn3Touched;
	bool btn2Touched = touching && !camDragTouch && GameplayHotbar::isButton2Touched(touchXY.px, touchXY.py);

        static bool btn1WasTouched = false;
        if (btn1Touched && !btn1WasTouched) Sound::uiClick();
        if (btn1Touched && !btn1WasTouched && (furnaceOpen || chestOpen)) {
            closeFurnaceMenu();
        } else if (btn1Touched && !btn1WasTouched) {
            craftingOpen = !craftingOpen;
            if (craftingOpen) inventoryOpen = false;

            if (craftingOpen) {
                craftingSelectedRecipe = -1;
                CraftingScreen::resetPage();
		craftingUsingTable = false;
                CraftingScreen::showTop(craftingSelectedRecipe, craftingUsingTable);
                CraftingScreen::show(*inventory, craftingSelectedRecipe, craftingUsingTable);
            } else {
                videoSetMode(MODE_0_3D);
                if (consoleVisible) {
                    consoleDemoInit();
                } else {
                    if (consoleVisible) { consoleDemoInit(); } else { HotbarIcons::render(*inventory, selectedSlot); }
                }
            }
        }
        btn1WasTouched = btn1Touched;

	if (craftingOpen) {
            static bool craftTapWasTouching = false;
            if (touching && !craftTapWasTouching) {
                if (CraftingScreen::isPreviousTouched(touchXY.px, touchXY.py) || CraftingScreen::isNextTouched(touchXY.px, touchXY.py)) {
                    Sound::uiClick();
                    if (CraftingScreen::changePage(CraftingScreen::isNextTouched(touchXY.px, touchXY.py) ? 1 : -1))
                        CraftingScreen::show(*inventory, craftingSelectedRecipe, craftingUsingTable);
                } else if (CraftingScreen::isCraftTouched(touchXY.px, touchXY.py)) {
                    Sound::uiClick();
                    if (craftingSelectedRecipe != -1 && canCraft(*inventory, RECIPES[craftingSelectedRecipe])) {
                        doCraft(*inventory, RECIPES[craftingSelectedRecipe]);

                        if (!canCraft(*inventory, RECIPES[craftingSelectedRecipe])) {
                            craftingSelectedRecipe = -1;
                        }

                        CraftingScreen::showTop(craftingSelectedRecipe, craftingUsingTable);
                        CraftingScreen::show(*inventory, craftingSelectedRecipe, craftingUsingTable);
                    }
                } else {
                    int tapped = CraftingScreen::getTouchedRecipeSlot(touchXY.px, touchXY.py);
                    if (tapped != -1) {
                        craftingSelectedRecipe = tapped;
                        CraftingScreen::showTop(craftingSelectedRecipe, craftingUsingTable);
                        CraftingScreen::show(*inventory, craftingSelectedRecipe, craftingUsingTable);
                    }
                }
            }
            craftTapWasTouching = touching;
        }

        static bool btn2WasTouched = false;
        if (btn2Touched && !btn2WasTouched) Sound::uiClick();
        if (btn2Touched && !btn2WasTouched && (furnaceOpen || chestOpen)) {
            closeFurnaceMenu();
        } else if (btn2Touched && !btn2WasTouched) {
            inventoryOpen = !inventoryOpen;
            if (inventoryOpen) craftingOpen = false;
            inventorySelectedSlot = -1;

            if (inventoryOpen) {
                LoadingScreen::showTopDirtOnly(); // reutilizamos el fondo de tierra, esta vez solo nos interesa la pantalla SUPERIOR
                InventoryScreen::show(*inventory, selectedSlot, inventorySelectedSlot);
            } else {
                videoSetMode(MODE_0_3D);
                if (consoleVisible) {
                    consoleDemoInit();
                } else {
                    if (consoleVisible) { consoleDemoInit(); } else { HotbarIcons::render(*inventory, selectedSlot); }
                }
            }
        }
        btn2WasTouched = btn2Touched;

	const bool uiBlockingBase = inventoryOpen || craftingOpen || paused || healthState.dead; // muerto: sin controles
	bool uiBlocking = uiBlockingBase || sleeping;   // dormido tampoco se actua; mirar, agacharse y la tercera persona quedan libres (para ver al jugador tendido)

        // Levantarse (GuiSleepMP "Leave Bed"): cualquier direccion de la cruceta, o A en el modo normal (EntityPlayer.wakeUpPlayer(false, true, true))
        if (sleeping && !uiBlockingBase) {
            const u32 leaveKeys = (u32)(Controls::key(Controls::FORWARD) | Controls::key(Controls::BACK) | Controls::key(Controls::STRAFE_LEFT) | Controls::key(Controls::STRAFE_RIGHT) | Controls::key(Controls::JUMP));
            if (keys_down & leaveKeys) wakeUp(false, true);
        }

        if (!uiBlockingBase) {
            // camara por botones: vale en los dos perfiles (en el normal vienen sin asignar y la camara va por touch)
            player->handleButtonLook((keys_held & Controls::key(Controls::LOOK_LEFT)) != 0, (keys_held & Controls::key(Controls::LOOK_RIGHT)) != 0,
                                     (keys_held & Controls::key(Controls::LOOK_UP)) != 0, (keys_held & Controls::key(Controls::LOOK_DOWN)) != 0);
            if (controlMode == 1) {
                player->handleTouch(false, touchXY.px, touchXY.py);
            } else {
                bool inGroundArea = GameplayHotbar::isTouchZone(touchXY.px, touchXY.py);
                if (touching && inGroundArea) {
                    player->handleTouch(touching, touchXY.px, touchXY.py);
                } else {
                    player->handleTouch(false, touchXY.px, touchXY.py);
                }
            }
        } else {
            player->handleTouch(false, touchXY.px, touchXY.py); // resetea estado, evita saltos al cerrar
        }

	if (inventoryOpen && !furnaceOpen && !chestOpen) {
            static bool invTapWasTouching = false;
            if (touching && !invTapWasTouching) {
                int tapped = InventoryScreen::getTouchedSlot(touchXY.px, touchXY.py);
                if (tapped != -1) {
                    if (inventorySelectedSlot == -1 && !inventory->isSlotEmpty(tapped)) {
                        inventorySelectedSlot = tapped;
                        InventoryScreen::show(*inventory, selectedSlot, inventorySelectedSlot);
                    } else {
                        inventory->swapOrMerge(inventorySelectedSlot, tapped);
                        inventorySelectedSlot = -1;
                        InventoryScreen::show(*inventory, selectedSlot, inventorySelectedSlot);
                    }
                }
            }
            invTapWasTouching = touching;
        }

	if (furnaceOpen) {
            // Si el horno abierto fue destruido con el menú abierto, se cierra el menú
            if (!openFurnace || !openFurnace->used) {
                closeFurnaceMenu();
            } else {
                int prevSlot = furnaceSelectedSlot;
                if (keys_down & KEY_UP)    furnaceSelectedSlot = Furnace::SLOT_INGREDIENT;
                if (keys_down & KEY_DOWN)  furnaceSelectedSlot = Furnace::SLOT_FUEL;
                if (keys_down & KEY_RIGHT) furnaceSelectedSlot = Furnace::SLOT_RESULT;
                if ((keys_down & KEY_LEFT) && furnaceSelectedSlot == Furnace::SLOT_RESULT) {
                    furnaceSelectedSlot = furnaceLastLeftSlot;
                }
                if (furnaceSelectedSlot != Furnace::SLOT_RESULT) furnaceLastLeftSlot = furnaceSelectedSlot;

                if (furnaceSelectedSlot != prevSlot) {
                    CraftingScreen::showFurnaceTop(furnaceLitOpen, openFurnace, furnaceSelectedSlot);
                }

                static bool furnaceTapWasTouching = false;
                if (touching && !furnaceTapWasTouching) {
                    int tapped = InventoryScreen::getTouchedSlot(touchXY.px, touchXY.py);
                    if (tapped != -1 && tapped < Inventory::ARMOR_BASE &&
                        Furnace::transferWithInventory(*inventory, tapped, *openFurnace, furnaceSelectedSlot)) {
                        CraftingScreen::showFurnaceTop(furnaceLitOpen, openFurnace, furnaceSelectedSlot);
                        InventoryScreen::show(*inventory, selectedSlot, -1);
                    }
                }
                furnaceTapWasTouching = touching;
            }
        }

	if (chestOpen) {
            // Si el cofre abierto (o su pareja) se rompió con el menú abierto, el menú se cierra
            bool intact = true;
            for (int p = 0; p < 2; p++) {
                ChestData* part = chestView.part[p];
                if (!part) continue;
                if (!part->used) intact = false;
                else if (chestView.dispenser) { if (!isDispenserBlock(typeAtCell(part->gx, part->gy, part->gz))) intact = false; }
                else if (part->gy != MinecartManager::CHEST_KEY_Y && typeAtCell(part->gx, part->gy, part->gz) != BlockType::CHEST) intact = false;   // el cofre de una vagoneta no es un bloque
            }
            if (!intact) {
                closeFurnaceMenu();
            } else {
                const int total = chestView.dispenser ? 9 : chestView.rows * 9;
                int prevSlot = chestSelectedSlot;
                const int cols = chestView.dispenser ? 3 : 9;   // el dispensador es una cuadricula de 3 x 3
                if ((keys_down & KEY_LEFT)  && chestSelectedSlot % cols > 0)          chestSelectedSlot--;
                if ((keys_down & KEY_RIGHT) && chestSelectedSlot % cols < cols - 1)   chestSelectedSlot++;
                if ((keys_down & KEY_UP)    && chestSelectedSlot >= cols)             chestSelectedSlot -= cols;
                if ((keys_down & KEY_DOWN)  && chestSelectedSlot + cols < total)      chestSelectedSlot += cols;
                if (chestSelectedSlot != prevSlot) CraftingScreen::showChestTop(chestView, chestSelectedSlot);

                static bool chestTapWasTouching = false;
                if (touching && !chestTapWasTouching) {
                    int tapped = InventoryScreen::getTouchedSlot(touchXY.px, touchXY.py);
                    ChestSlot* cs = chestView.slotAt(chestSelectedSlot);
                    if (tapped != -1 && tapped < Inventory::ARMOR_BASE && cs && ChestStore::transferWithInventory(*inventory, tapped, *cs)) {
                        CraftingScreen::showChestTop(chestView, chestSelectedSlot);
                        InventoryScreen::show(*inventory, selectedSlot, -1);
                    }
                }
                chestTapWasTouching = touching;
            }
        }

        if (!uiBlockingBase && !consoleVisible && (keys_down & Controls::key(Controls::SNEAK))) {
            player->setSneak(!player->isSneaking());   // el boton de agacharse del perfil alterna (en xyba va sin asignar: se agacha con el boton tactil)
        }

        // Depuración: con la consola abierta y controles normales, B alterna dia / noche al instante (para probar los mobs hostiles).
        // Noche = medianoche; dia = mediodia.
        if (!uiBlocking && consoleVisible && Options::get().debugScreenOptions && (keys_down & KEY_B)) {
            const bool isNight = computeNightFactor(gameTimeTicks) > 0.5f;
            gameTimeTicks = isNight ? DEBUG_NOON_TICKS : DEBUG_MIDNIGHT_TICKS;
        }

        // Depuración: con la consola abierta y controles normales, Y alterna tormenta / despejado al instante (para probar el clima)
        if (!uiBlocking && consoleVisible && Options::get().debugScreenOptions && (keys_down & KEY_Y)) Weather::debugToggle();

        if (!uiBlockingBase && !consoleVisible && (keys_down & Controls::key(Controls::THIRD_PERSON))) {
            thirdPerson = !thirdPerson;   // en modo normal, Y alterna la tercera persona (tirar un objeto: boton Throw)
        }

        // Depuración: con la consola abierta, X (en cualquier modo de controles) enciende o apaga el daño. La barra y la regeneración siguen igual.
        if (!uiBlocking && consoleVisible && Options::get().debugScreenOptions && Options::get().difficulty == 0 && (keys_down & KEY_X)) {
            healthDamageEnabled = !healthDamageEnabled;
        }

	static bool throwWasTouched = false;
        bool throwTouchedNow = touching && !camDragTouch && !uiBlocking && GameplayHotbar::isThrowTouched(touchXY.px, touchXY.py);
        if (throwTouchedNow && !throwWasTouched) {
            Sound::uiClick();
            throwOneFromSlot(*inventory, droppedItems, MAX_DROPPED_ITEMS, selectedSlot, player);
            if (consoleVisible) { consoleDemoInit(); } else { HotbarIcons::render(*inventory, selectedSlot); }
        }
        throwWasTouched = throwTouchedNow;

	if (inventoryOpen) {
            static bool invThrowWasTouched = false;
            bool invThrowTouchedNow = touching && GameplayHotbar::isInventoryThrowTouched(touchXY.px, touchXY.py);
            if (invThrowTouchedNow && !invThrowWasTouched && inventorySelectedSlot != -1) {
                Sound::uiClick();
                throwOneFromSlot(*inventory, droppedItems, MAX_DROPPED_ITEMS, inventorySelectedSlot, player, true);
                InventoryScreen::show(*inventory, selectedSlot, inventorySelectedSlot);
            }
            invThrowWasTouched = invThrowTouchedNow;
        }

        static int hotbarRowTouchFrames = 0;
        bool hotbarRowTouched = touching && touchXY.py <= 29;

        if (hotbarRowTouched) {
            hotbarRowTouchFrames++;
        } else {
            hotbarRowTouchFrames = 0;
        }

        if (hotbarRowTouched && hotbarRowTouchFrames == 2 && !uiBlocking) {
            int tappedSlot = touchXY.px / 28;
            if (tappedSlot >= 0 && tappedSlot < Inventory::HOTBAR_SIZE && tappedSlot != selectedSlot) {
                selectedSlot = tappedSlot;
                selectedSlotChangedByTouch = true;
                if (consoleVisible) { consoleDemoInit(); } else { HotbarIcons::render(*inventory, selectedSlot); }
            }
        }

        // Salto por touch — solo tiene sentido en modo botones, ya que
        // en modo normal esa zona ya se usa para mirar
        bool touchJumpHeld = false;
        if (!uiBlocking && controlMode == 1) {
            bool inGroundArea = GameplayHotbar::isTouchZone(touchXY.px, touchXY.py);
            touchJumpHeld = touching && inGroundArea;
        }

        player->handleInput(paused ? 0 : Controls::gameHeld(keys_held), *chunkManager);   // en pausa la camara tampoco se mueve

        float partialTick = frameCounter / 3.0f;   // en pausa frameCounter no avanza: la interpolacion queda fija y la imagen quieta

        float bobX, bobY, bobRoll;
        player->getViewBob(partialTick, bobX, bobY, bobRoll);
        const float hurtRoll = Health::hurtCameraRoll(healthState, partialTick) + Health::deathCameraRoll(healthState, partialTick);

        // Game over: el FOV se achica (EntityRenderer.getFOVModifier). Se escalan x e y del espacio de la cámara, que equivale a achicar el ángulo.
        // No se toca la pila de proyección (tiene un solo nivel): la matriz de la cámara se reinicia en cada cuadro y el zoom no se acumula.
        float deathZoom = 1.0f;
        if (healthState.dead) {
            const float f = Health::deathFovFactor(healthState, partialTick);
            const float tan35 = 0.70020753f; // tan(35 grados): la mitad del FOV de 70
            deathZoom = tan35 / tanf(atanf(tan35) / f);
        } // EntityRenderer.hurtCameraEffect

        if (USE_WORLD_VIEW_BOBBING) {
            player->applyCamera(partialTick, 0.0f); // mantiene lo que Player calcula internamente
            float rx, ry, rz;
            player->getRenderPosition(partialTick, rx, ry, rz);

            // Mismo orden que EntityRenderer: balanceo primero, después orientar la cámara
            glMatrixMode(GL_MODELVIEW);
            glLoadIdentity();
            if (deathZoom != 1.0f) glScalef(deathZoom, deathZoom, 1.0f);
            glRotatef(hurtRoll, 0.0f, 0.0f, 1.0f); // el temblor por daño va antes del balanceo, como en EntityRenderer.orientCamera
            if (sleeping && !thirdPerson) {
                // EntityRenderer.orientCamera dormido: la vista queda fija mirando a lo largo de la cama (hacia los pies), 0,12 sobre los ojos; no se puede girar
                glTranslatef(0.0f, 0.3f, 0.0f);
                glRotatef((float)(bedDir * 90), 0.0f, 1.0f, 0.0f);
                glTranslatef(0.0f, -0.42f, 0.0f);
                glTranslatef(-rx, -ry, -rz);
            } else {
                if (healthState.dead) {   // muerto: la camara queda quieta donde moriste (sin balanceo, ni caida, ni seguir al cuerpo)
                    glRotatef(-player->pitch, 1.0f, 0.0f, 0.0f);
                    glRotatef(-player->yaw, 0.0f, 1.0f, 0.0f);
                    glTranslatef(-deathCamX, -deathCamY, -deathCamZ);
                } else {
                HandAnimations::applyWorldViewBobbing(partialTick);
                if (thirdPerson) glTranslatef(0.0f, 0.0f, -player->thirdPersonDistance(*chunkManager, rx, ry - player->getEyeDrop(partialTick), rz));   // la camara se aleja detras de los ojos
                glRotatef(-player->pitch, 1.0f, 0.0f, 0.0f);
                glRotatef(-player->yaw, 0.0f, 1.0f, 0.0f);
                glTranslatef(-rx, -(ry - player->getEyeDrop(partialTick)), -rz);
                }
            }
        } else {
            player->applyCamera(partialTick, bobRoll + hurtRoll);

            glMatrixMode(GL_MODELVIEW);
            glTranslatef(bobX, bobY, 0.0f);
        }

        float dirX, dirY, dirZ;
        player->getLookDirection(dirX, dirY, dirZ);

        const float eyeCastY = player->y - player->getEyeDrop(1.0f);   // agachado, los ojos bajan
        RaycastHit hit = Raycaster::cast(*chunkManager, player->x, eyeCastY, player->z,
                                          dirX, dirY, dirZ, 5.0f);

        int sheepTarget = -1;
        int boatTarget = -1;   // el bote apuntado
        int cartTarget = -1;   // la vagoneta apuntada (si esta mas cerca que el bloque y que la oveja)
        int paintTarget = -1;  // el cuadro apuntado
        if (!uiBlocking) {
            float blockDist = 1.0e9f;
            if (hit.hit) {
                const float hx = hit.hitX - player->x, hy = hit.hitY - eyeCastY, hz = hit.hitZ - player->z;
                blockDist = sqrtf(hx * hx + hy * hy + hz * hz);
            }
            float sheepDist = 0.0f;
            sheepTarget = SheepManager::pick(player->x, eyeCastY, player->z, dirX, dirY, dirZ, 3.0f, sheepDist);
            if (sheepTarget >= 0 && sheepDist >= blockDist) sheepTarget = -1;
            float cartDist = 0.0f;
            cartTarget = MinecartManager::pick(player->x, eyeCastY, player->z, dirX, dirY, dirZ, 3.0f, cartDist);
            if (cartTarget >= 0 && (cartDist >= blockDist || (sheepTarget >= 0 && cartDist >= sheepDist))) cartTarget = -1;
            else if (cartTarget >= 0) sheepTarget = -1;
            float boatDist = 0.0f;
            boatTarget = BoatManager::pick(player->x, eyeCastY, player->z, dirX, dirY, dirZ, 3.0f, boatDist);
            if (boatTarget >= 0 && (boatDist >= blockDist || (sheepTarget >= 0 && boatDist >= sheepDist) || (cartTarget >= 0 && boatDist >= cartDist))) boatTarget = -1;
            else if (boatTarget >= 0) { sheepTarget = -1; cartTarget = -1; }
            float paintDist = 0.0f;
            paintTarget = PaintingManager::pick(player->x, eyeCastY, player->z, dirX, dirY, dirZ, 3.0f, paintDist);
            if (paintTarget >= 0 && (paintDist >= blockDist || (sheepTarget >= 0 && paintDist >= sheepDist) || (cartTarget >= 0 && paintDist >= cartDist) || (boatTarget >= 0 && paintDist >= boatDist))) paintTarget = -1;
            else if (paintTarget >= 0) { sheepTarget = -1; cartTarget = -1; boatTarget = -1; }
        }

        // PlayerController.clickBlock -> World.onBlockHit: al hacer clic sobre un bloque, el fuego que hay delante de esa cara se apaga
        if (!uiBlocking && hit.hit && (keys_down & Controls::key(Controls::BREAK))) {
            int xdx, xdy, xdz;
            getFaceOffset(hit.face, xdx, xdy, xdz);
            chunkManager->extinguishFire(hit.chunk->getChunkX() * WorldConfig::SIZE_X + hit.blockXi + xdx, hit.blockYi + xdy,
                                         hit.chunk->getChunkZ() * WorldConfig::SIZE_Z + hit.blockZi + xdz);
        }

        u32 breakKey = Controls::key(Controls::BREAK);
        u32 placeKey = Controls::key(Controls::PLACE);

        bool breakHeld = !uiBlocking && ((keys_held & breakKey) != 0) && sheepTarget < 0 && cartTarget < 0 && boatTarget < 0 && paintTarget < 0;   // apuntando a una oveja o una vagoneta no se pica el bloque de atrás
	if (!uiBlocking && (keys_down & breakKey)) {
            HandAnimations::onAttackPressed(hit.hit || sheepTarget >= 0 || cartTarget >= 0 || boatTarget >= 0 || paintTarget >= 0);
            if (paintTarget >= 0) {
                // EntityPainting.attackEntityFrom: cualquier golpe lo suelta como objeto; la herramienta se gasta como al golpear a cualquier entidad
                const BlockType held = inventory->isSlotEmpty(selectedSlot) ? BlockType::AIR : inventory->getSlotType(selectedSlot);
                PaintingManager::attack(paintTarget);
                if (isTool(held) && !isHoeTool(held)) {
                    inventory->reduceDurability(selectedSlot, isSword(held) ? 1 : 2);
                    if (consoleVisible) { consoleDemoInit(); } else { HotbarIcons::render(*inventory, selectedSlot); }
                }
            } else if (sheepTarget >= 0) {
                // EntityPlayer.attackTargetEntityWithCurrentItem: daño según lo que lleva en la mano; ItemSword gasta 1 de durabilidad por golpe y las demás herramientas 2
                const BlockType held = inventory->isSlotEmpty(selectedSlot) ? BlockType::AIR : inventory->getSlotType(selectedSlot);
                SheepManager::attack(sheepTarget, getDamageVsEntity(held), player->x, player->z);
                if (isTool(held) && !isHoeTool(held)) {   // la azada (ItemHoe) no gasta al golpear
                    inventory->reduceDurability(selectedSlot, isSword(held) ? 1 : 2);
                    if (consoleVisible) { consoleDemoInit(); } else { HotbarIcons::render(*inventory, selectedSlot); }
                }
            } else if (boatTarget >= 0) {
                // EntityBoat.attackEntityFrom: igual que la vagoneta (+10 por punto de dano, se rompe al pasar de 40)
                const BlockType held = inventory->isSlotEmpty(selectedSlot) ? BlockType::AIR : inventory->getSlotType(selectedSlot);
                BoatManager::attack(boatTarget, getDamageVsEntity(held));
                if (isTool(held) && !isHoeTool(held)) {   // la azada (ItemHoe) no gasta al golpear
                    inventory->reduceDurability(selectedSlot, isSword(held) ? 1 : 2);
                    if (consoleVisible) { consoleDemoInit(); } else { HotbarIcons::render(*inventory, selectedSlot); }
                }
            } else if (cartTarget >= 0) {
                // EntityMinecart.attackEntityFrom: cada golpe suma 10 por punto de daño; al pasar de 40 se rompe
                const BlockType held = inventory->isSlotEmpty(selectedSlot) ? BlockType::AIR : inventory->getSlotType(selectedSlot);
                MinecartManager::attack(cartTarget, getDamageVsEntity(held));
                if (isTool(held) && !isHoeTool(held)) {   // la azada (ItemHoe) no gasta al golpear
                    inventory->reduceDurability(selectedSlot, isSword(held) ? 1 : 2);
                    if (consoleVisible) { consoleDemoInit(); } else { HotbarIcons::render(*inventory, selectedSlot); }
                }
            } else if (hit.hit) {
                // Block.onBlockClicked: la puerta, la trampilla y el pastel responden tambien al clic izquierdo (BlockDoor y BlockTrapDoor llaman a blockActivated)
                const BlockType ct = hit.chunk->getBlockType(hit.blockXi, hit.blockYi, hit.blockZi);
                if (ct == BlockType::REDSTONE_ORE) {   // BlockRedstoneOre.onBlockClicked: se enciende al golpearla
                    Redstone::lightOre(*chunkManager, hit.chunk->getChunkX() * WorldConfig::SIZE_X + hit.blockXi, hit.blockYi, hit.chunk->getChunkZ() * WorldConfig::SIZE_Z + hit.blockZi);
                }
                if (isDoorBlock(ct) || ct == BlockType::TRAPDOOR || ct == BlockType::CAKE_BLOCK || ct == BlockType::LEVER || ct == BlockType::BUTTON) {   // BlockLever y BlockButton.onBlockClicked
                    const int cgx = hit.chunk->getChunkX() * WorldConfig::SIZE_X + hit.blockXi;
                    const int cgz = hit.chunk->getChunkZ() * WorldConfig::SIZE_Z + hit.blockZi;
                    if (ct == BlockType::CAKE_BLOCK) {
                        if (healthState.health < Health::MAX_HEALTH && !healthState.dead) {
                            Health::heal(healthState, 3);
                            Partial::eatCake(*chunkManager, cgx, hit.blockYi, cgz);
                        }
                    } else {
                        Partial::activate(*chunkManager, cgx, hit.blockYi, cgz);
                    }
                }
            }
        }

        if (!breakHeld || !hit.hit) {
            miningChunk = nullptr;
            miningDamage = 0.0f; miningHitWait = 0;
        }

        // EntityMinecart.interact: la vagoneta normal te sube, la de cofre abre su cofre y la de horno acepta carbon (y se empuja lejos de ti)
        if (!uiBlocking && (keys_down & placeKey) && cartTarget >= 0) {
            const bool coal = !inventory->isSlotEmpty(selectedSlot) && inventory->getSlotType(selectedSlot) == BlockType::COAL;
            const int r = MinecartManager::interact(cartTarget, coal, player->x, player->z);
            if (r & MinecartManager::INTERACT_MOUNTED) player->setSneak(false);
            if (r & MinecartManager::INTERACT_COAL_USED) {
                inventory->removeOneFromSlot(selectedSlot);
                if (consoleVisible) { consoleDemoInit(); } else { HotbarIcons::render(*inventory, selectedSlot); }
            }
            if ((r & MinecartManager::INTERACT_CHEST) && !chestOpen) openCartChest(MinecartManager::chestUid(cartTarget));
        }

        // EntityBoat.interact: el bote te sube
        if (!uiBlocking && (keys_down & placeKey) && boatTarget >= 0 && !MinecartManager::isRiding() && !BoatManager::isRiding() && !AnimalManager::isRiding()) {
            if (BoatManager::interact(boatTarget)) player->setSneak(false);
        }

        // ItemBoat.onItemRightClick: un rayo de 5 bloques (que tambien se detiene en el agua) y el bote aparece donde cae
        bool boatPlaced = false;
        if (!uiBlocking && (keys_down & placeKey) && sheepTarget < 0 && cartTarget < 0 && boatTarget < 0 && paintTarget < 0 && !inventory->isSlotEmpty(selectedSlot) && inventory->getSlotType(selectedSlot) == BlockType::BOAT) {
            if (BoatManager::spawnFromRay(*chunkManager, player->x, eyeCastY, player->z, dirX, dirY, dirZ)) {
                inventory->removeOneFromSlot(selectedSlot);
                HandAnimations::onBlockPlaced(inventory->isSlotEmpty(selectedSlot));
                if (consoleVisible) { consoleDemoInit(); } else { HotbarIcons::render(*inventory, selectedSlot); }
                boatPlaced = true;
            }
        }

        // ItemBow.onItemRightClick: gasta una flecha del inventario y la dispara; ItemSnowball / ItemEgg: se gasta el objeto y se lanza. Un bloque que se activa (mesa, cofre, horno...) gana el clic
        if (!uiBlocking && (keys_down & placeKey) && !healthState.dead && cartTarget < 0 && boatTarget < 0 && !boatPlaced && !inventory->isSlotEmpty(selectedSlot) &&
            (inventory->getSlotType(selectedSlot) == BlockType::BOW || inventory->getSlotType(selectedSlot) == BlockType::SNOWBALL || inventory->getSlotType(selectedSlot) == BlockType::EGG || inventory->getSlotType(selectedSlot) == BlockType::FISHING_ROD)) {
            const BlockType heldP = inventory->getSlotType(selectedSlot);
            bool blockUsed = false;
            if (hit.hit) {
                const BlockType ht = hit.chunk->getBlockType(hit.blockXi, hit.blockYi, hit.blockZi);
                blockUsed = ht == BlockType::CRAFTING_TABLE || ht == BlockType::CHEST || isDispenserBlock(ht) || ht == BlockType::NOTE_BLOCK || ht == BlockType::JUKEBOX || isFurnaceBlock(ht) ||
                            ht == BlockType::REDSTONE_ORE || Partial::isInteractive(ht);
            }
            if (!blockUsed) {
                bool fired = false;
                if (heldP == BlockType::BOW) {
                    int arrowSlot = -1;
                    for (int q = 0; q < Inventory::TOTAL_SIZE; q++) {   // InventoryPlayer.consumeInventoryItem: la primera flecha, empezando por la barra
                        if (!inventory->isSlotEmpty(q) && inventory->getSlotType(q) == BlockType::ARROW) { arrowSlot = q; break; }
                    }
                    if (arrowSlot >= 0 && Projectiles::shoot(Projectiles::ARROW, player->x, eyeCastY, player->z, player->yaw, player->pitch)) {
                        inventory->removeOneFromSlot(arrowSlot);
                        fired = true;
                    }
                } else if (heldP == BlockType::FISHING_ROD) {
                    // ItemFishingRod.onItemRightClick: con anzuelo, lo recoge (y gasta la cana segun lo que paso); sin anzuelo, lo lanza
                    if (FishingHook::active()) {
                        const FishingHook::Catch c = FishingHook::reel(player->x, eyeCastY, player->z);
                        if (c.damage > 0) inventory->reduceDurability(selectedSlot, c.damage);
                        if (c.fish) {
                            for (int i = 0; i < MAX_DROPPED_ITEMS; i++) {
                                if (droppedItems[i].isActive()) continue;
                                droppedItems[i].spawn(c.x, c.y + 0.3f, c.z, BlockType::FISH_RAW, true, c.vx, c.vy, c.vz, -1);
                                break;
                            }
                        }
                    } else {
                        FishingHook::cast(player->x, eyeCastY, player->z, player->yaw, player->pitch);
                    }
                    fired = true;
                } else if (Projectiles::shoot(heldP == BlockType::SNOWBALL ? Projectiles::SNOWBALL : Projectiles::EGG, player->x, eyeCastY, player->z, player->yaw, player->pitch)) {
                    inventory->removeOneFromSlot(selectedSlot);
                    fired = true;
                }
                if (fired) {
                    if (heldP != BlockType::BOW) HandAnimations::onBlockPlaced(inventory->isSlotEmpty(selectedSlot));   // el arco no balancea la mano
                    if (consoleVisible) { consoleDemoInit(); } else { HotbarIcons::render(*inventory, selectedSlot); }
                    boatPlaced = true;   // el clic ya se uso: no sigue a colocar bloques
                }
            }
        }

        // EntitySheep.interact: tijeras en la mano sobre una oveja con lana = esquilarla (2 a 4 lanas, gasta 1 de durabilidad). Apuntando a una oveja no se coloca nada.
        if (!uiBlocking && (keys_down & placeKey) && sheepTarget >= 0) {
            const BlockType held = inventory->isSlotEmpty(selectedSlot) ? BlockType::AIR : inventory->getSlotType(selectedSlot);
            // EntityPig.interact: un cerdo con silla te sube; si no, ItemSaddle.saddleEntity: la silla en la mano ensilla al cerdo (gasta 1)
            if (!AnimalManager::isRiding() && SheepManager::mount(sheepTarget)) {
                player->setSneak(false);
            } else if (held == BlockType::SADDLE && SheepManager::saddle(sheepTarget)) {
                inventory->removeOneFromSlot(selectedSlot);
                if (consoleVisible) { consoleDemoInit(); } else { HotbarIcons::render(*inventory, selectedSlot); }
            }
            if (held == BlockType::SHEARS && SheepManager::shear(sheepTarget)) {
                inventory->reduceDurability(selectedSlot, 1);
                if (consoleVisible) { consoleDemoInit(); } else { HotbarIcons::render(*inventory, selectedSlot); }
            }
            // ItemDye.saddleEntity: un tinte sobre una oveja con lana de otro color le cambia el color (gasta 1)
            { const int dd = dyeDamage(held); if (dd >= 0 && SheepManager::dye(sheepTarget, 15 - dd)) { inventory->removeOneFromSlot(selectedSlot); if (consoleVisible) { consoleDemoInit(); } else { HotbarIcons::render(*inventory, selectedSlot); } } }
            // EntityCow.interact: un balde vacio sobre una vaca se llena de leche
            if (held == BlockType::BUCKET && SheepManager::milk(sheepTarget)) {
                inventory->setSlotDirect(selectedSlot, BlockType::MILK_BUCKET, 1);
                if (consoleVisible) { consoleDemoInit(); } else { HotbarIcons::render(*inventory, selectedSlot); }
            }
            // EntityWolf.interact: un hueso intenta domar al lobo salvaje; carne de cerdo cura al domado; cualquier otra cosa sienta o para al domado
            if (SheepManager::interact(sheepTarget, held) == 1) {
                inventory->removeOneFromSlot(selectedSlot);
                if (consoleVisible) { consoleDemoInit(); } else { HotbarIcons::render(*inventory, selectedSlot); }
            }
        }

        // ItemFood.onItemRightClick: cura segun el alimento (cerdo crudo 3, cocido 8, pan 5, sopa 10, galleta 1); se come aunque la vida este llena (sin hambre en la Beta)
        if (!uiBlocking && (keys_down & placeKey) && sheepTarget < 0 && cartTarget < 0 && boatTarget < 0 && paintTarget < 0 && !healthState.dead && !inventory->isSlotEmpty(selectedSlot)) {
            const BlockType foodT = inventory->getSlotType(selectedSlot);
            int foodHeal = 0;
            switch (foodT) {
                case BlockType::PORK_RAW: foodHeal = 3; break;
                case BlockType::PORK_COOKED: foodHeal = 8; break;
                case BlockType::BREAD: foodHeal = 5; break;
                case BlockType::MUSHROOM_STEW: foodHeal = 10; break;
                case BlockType::COOKIE: foodHeal = 1; break;
                case BlockType::APPLE: foodHeal = 4; break;
                case BlockType::GOLDEN_APPLE: foodHeal = 42; break;
                case BlockType::FISH_RAW: foodHeal = 2; break;
                case BlockType::FISH_COOKED: foodHeal = 5; break;
                default: break;
            }
            if (foodHeal > 0) {
                Health::heal(healthState, foodHeal);
                if (foodT == BlockType::MUSHROOM_STEW) inventory->setSlotDirect(selectedSlot, BlockType::BOWL, 1);   // ItemSoup: devuelve el tazon vacio
                else inventory->removeOneFromSlot(selectedSlot);
                if (consoleVisible) { consoleDemoInit(); } else { HotbarIcons::render(*inventory, selectedSlot); }
            }
        }

        if (!uiBlocking && (keys_down & placeKey) && sheepTarget < 0 && cartTarget < 0 && boatTarget < 0 && paintTarget < 0 && hit.hit && hit.chunk->getBlockType(hit.blockXi, hit.blockYi, hit.blockZi) == BlockType::REDSTONE_ORE) {
            // BlockRedstoneOre.blockActivated: se enciende y el clic sigue (se puede poner un bloque encima)
            Redstone::lightOre(*chunkManager, hit.chunk->getChunkX() * WorldConfig::SIZE_X + hit.blockXi, hit.blockYi, hit.chunk->getChunkZ() * WorldConfig::SIZE_Z + hit.blockZi);
        }

        if (!uiBlocking && (keys_down & placeKey) && sheepTarget < 0 && cartTarget < 0 && boatTarget < 0 && paintTarget < 0 && !boatPlaced) {
            if (hit.hit && hit.chunk->getBlockType(hit.blockXi, hit.blockYi, hit.blockZi) == BlockType::CRAFTING_TABLE) {
                craftingOpen = true;
                inventoryOpen = false;
                craftingSelectedRecipe = -1;
                CraftingScreen::resetPage();
                craftingUsingTable = true; // nuevo — ver más abajo

                CraftingScreen::showTop(craftingSelectedRecipe, craftingUsingTable);
                CraftingScreen::show(*inventory, craftingSelectedRecipe, craftingUsingTable);
            } else if (hit.hit && hit.chunk->getBlockType(hit.blockXi, hit.blockYi, hit.blockZi) == BlockType::CHEST) {
                if (!chestOpen) {
                    openChest(hit.chunk->getChunkX() * WorldConfig::SIZE_X + hit.blockXi, hit.blockYi,
                              hit.chunk->getChunkZ() * WorldConfig::SIZE_Z + hit.blockZi);
                }
            } else if (hit.hit && isDispenserBlock(hit.chunk->getBlockType(hit.blockXi, hit.blockYi, hit.blockZi))) {
                if (!chestOpen) {
                    openDispenser(hit.chunk->getChunkX() * WorldConfig::SIZE_X + hit.blockXi, hit.blockYi,
                                  hit.chunk->getChunkZ() * WorldConfig::SIZE_Z + hit.blockZi);
                }
            } else if (hit.hit && hit.chunk->getBlockType(hit.blockXi, hit.blockYi, hit.blockZi) == BlockType::NOTE_BLOCK) {   // BlockNote.blockActivated: sube la nota y suena
                const int ngx = hit.chunk->getChunkX() * WorldConfig::SIZE_X + hit.blockXi, ngz = hit.chunk->getChunkZ() * WorldConfig::SIZE_Z + hit.blockZi;
                NoteBlocks::changePitch(ngx, hit.blockYi, ngz);
                NoteBlocks::trigger(*chunkManager, ngx, hit.blockYi, ngz);
            } else if (hit.hit && hit.chunk->getBlockType(hit.blockXi, hit.blockYi, hit.blockZi) == BlockType::JUKEBOX &&
                       (Partial::jukeboxRecord(*chunkManager, hit.chunk->getChunkX() * WorldConfig::SIZE_X + hit.blockXi, hit.blockYi, hit.chunk->getChunkZ() * WorldConfig::SIZE_Z + hit.blockZi) != BlockType::AIR ||
                        (!inventory->isSlotEmpty(selectedSlot) && isRecord(inventory->getSlotType(selectedSlot))))) {
                // ItemRecord.onItemUse / BlockJukeBox.blockActivated: con un disco en la mano lo mete; si ya tiene uno, lo saca (el disco suena con Music::playRecord)
                const int jgx = hit.chunk->getChunkX() * WorldConfig::SIZE_X + hit.blockXi, jgz = hit.chunk->getChunkZ() * WorldConfig::SIZE_Z + hit.blockZi;
                const BlockType inside = Partial::jukeboxEject(*chunkManager, jgx, hit.blockYi, jgz);
                if (inside != BlockType::AIR) {
                    spillStack(inside, 1, -1, hit.chunk->gridToWorldX(hit.blockXi), hit.chunk->gridToWorldY(hit.blockYi) + 0.9f, hit.chunk->gridToWorldZ(hit.blockZi));
                } else if (Partial::jukeboxInsert(*chunkManager, jgx, hit.blockYi, jgz, inventory->getSlotType(selectedSlot))) {
                    inventory->removeOneFromSlot(selectedSlot);
                    HandAnimations::onBlockPlaced(inventory->isSlotEmpty(selectedSlot));
                    if (consoleVisible) { consoleDemoInit(); } else { HotbarIcons::render(*inventory, selectedSlot); }
                }
            } else if (hit.hit && isFurnaceBlock(hit.chunk->getBlockType(hit.blockXi, hit.blockYi, hit.blockZi))) {
                if (!furnaceOpen) {
                    BlockType fType = hit.chunk->getBlockType(hit.blockXi, hit.blockYi, hit.blockZi);
                    int fgx = hit.chunk->getChunkX() * WorldConfig::SIZE_X + hit.blockXi;
                    int fgy = hit.blockYi;
                    int fgz = hit.chunk->getChunkZ() * WorldConfig::SIZE_Z + hit.blockZi;

                    openFurnace = Furnace::findOrCreate(fgx, fgy, fgz);
                    if (openFurnace) {
                        furnaceOpen = true;
                        furnaceLitOpen = furnaceIsLit(fType);
                        furnaceSelectedSlot = Furnace::SLOT_INGREDIENT;
                        furnaceLastLeftSlot = Furnace::SLOT_INGREDIENT;
                        craftingOpen = false;
                        inventoryOpen = true; // la pantalla de abajo reutiliza el inventario
                        inventorySelectedSlot = -1;

                        CraftingScreen::showFurnaceTop(furnaceLitOpen, openFurnace, furnaceSelectedSlot);
                        InventoryScreen::show(*inventory, selectedSlot, -1);
                    }
                }
            } else if (hit.hit && Partial::isInteractive(hit.chunk->getBlockType(hit.blockXi, hit.blockYi, hit.blockZi))) {
                // Block.blockActivated: abrir o cerrar la puerta de madera o la trampilla. El clic se gasta aunque no haga nada (puerta de hierro, cama)
                const int pgx = hit.chunk->getChunkX() * WorldConfig::SIZE_X + hit.blockXi;
                const int pgz = hit.chunk->getChunkZ() * WorldConfig::SIZE_Z + hit.blockZi;
                if (hit.chunk->getBlockType(hit.blockXi, hit.blockYi, hit.blockZi) == BlockType::CAKE_BLOCK) {
                    // BlockCake.eatCakeSlice: solo si falta vida; cura 3 (1 corazon y medio) y quita una porcion
                    if (healthState.health < Health::MAX_HEALTH && !healthState.dead) {
                        Health::heal(healthState, 3);
                        Partial::eatCake(*chunkManager, pgx, hit.blockYi, pgz);
                    }
                } else if (hit.chunk->getBlockType(hit.blockXi, hit.blockYi, hit.blockZi) == BlockType::BED_BLOCK) {
                    useBed(pgx, hit.blockYi, pgz);   // BlockBed.blockActivated: dormir (de noche) o, en el Nether, explotar
                } else {
                    Partial::activate(*chunkManager, pgx, hit.blockYi, pgz);
                }
                HandAnimations::onBlockPlaced(false);   // swingItem: el brazo golpea al abrir o cerrar
            } else if (hit.hit && !inventory->isSlotEmpty(selectedSlot) && isPistonBase(inventory->getSlotType(selectedSlot))) {
                // ItemPiston: la direccion sale de donde esta el jugador (BlockPistonBase.func_31039_c)
                if (Pistons::useItem(*chunkManager, inventory->getSlotType(selectedSlot),
                                     hit.chunk->getChunkX() * WorldConfig::SIZE_X + hit.blockXi, hit.blockYi,
                                     hit.chunk->getChunkZ() * WorldConfig::SIZE_Z + hit.blockZi, hit.face, player->yaw,
                                     player->x, player->y, player->z)) {
                    inventory->removeOneFromSlot(selectedSlot);
                    HandAnimations::onBlockPlaced(inventory->isSlotEmpty(selectedSlot));
                    if (consoleVisible) { consoleDemoInit(); } else { HotbarIcons::render(*inventory, selectedSlot); }
                }
            } else if (hit.hit && !inventory->isSlotEmpty(selectedSlot) && isMinecartItem(inventory->getSlotType(selectedSlot))) {
                // ItemMinecart.onItemUse: solo sobre un riel
                const BlockType heldC = inventory->getSlotType(selectedSlot);
                const int ctype = (heldC == BlockType::MINECART) ? 0 : (heldC == BlockType::MINECART_CHEST ? 1 : 2);
                if (MinecartManager::spawn(*chunkManager, hit.chunk->getChunkX() * WorldConfig::SIZE_X + hit.blockXi, hit.blockYi,
                                           hit.chunk->getChunkZ() * WorldConfig::SIZE_Z + hit.blockZi, ctype)) {
                    inventory->removeOneFromSlot(selectedSlot);
                    HandAnimations::onBlockPlaced(inventory->isSlotEmpty(selectedSlot));
                    if (consoleVisible) { consoleDemoInit(); } else { HotbarIcons::render(*inventory, selectedSlot); }
                }
            } else if (hit.hit && !inventory->isSlotEmpty(selectedSlot) &&
                       (inventory->getSlotType(selectedSlot) == BlockType::REDSTONE || isRailBlock(inventory->getSlotType(selectedSlot)) || inventory->getSlotType(selectedSlot) == BlockType::LEVER ||
                        inventory->getSlotType(selectedSlot) == BlockType::BUTTON || isPlateBlock(inventory->getSlotType(selectedSlot)) ||
                        inventory->getSlotType(selectedSlot) == BlockType::TORCH_REDSTONE_ON || inventory->getSlotType(selectedSlot) == BlockType::REPEATER_ITEM)) {
                // polvo, palanca, boton, placas, antorcha de redstone y repetidor: Redstone::useItem
                if (Redstone::useItem(*chunkManager, inventory->getSlotType(selectedSlot),
                                      hit.chunk->getChunkX() * WorldConfig::SIZE_X + hit.blockXi, hit.blockYi,
                                      hit.chunk->getChunkZ() * WorldConfig::SIZE_Z + hit.blockZi, hit.face, player->yaw)) {
                    inventory->removeOneFromSlot(selectedSlot);
                    HandAnimations::onBlockPlaced(inventory->isSlotEmpty(selectedSlot));
                    if (consoleVisible) { consoleDemoInit(); } else { HotbarIcons::render(*inventory, selectedSlot); }
                }
            } else if (hit.hit && !inventory->isSlotEmpty(selectedSlot) &&
                       (inventory->getSlotType(selectedSlot) == BlockType::DOOR_WOOD_ITEM || inventory->getSlotType(selectedSlot) == BlockType::DOOR_IRON_ITEM ||
                        inventory->getSlotType(selectedSlot) == BlockType::BED_ITEM || inventory->getSlotType(selectedSlot) == BlockType::TRAPDOOR ||
                        inventory->getSlotType(selectedSlot) == BlockType::SIGN_ITEM || inventory->getSlotType(selectedSlot) == BlockType::CAKE_ITEM ||
                        isStructureBlock(inventory->getSlotType(selectedSlot)))) {
                // ItemDoor, ItemBed, la trampilla, losas, escaleras, escalera de mano y valla: se ponen con su propia logica (orientacion, apoyo)
                bool structureBlocked = false;   // losas, escaleras y vallas no se ponen donde estan el jugador o una oveja (la escalera de mano no estorba)
                const BlockType heldS = inventory->getSlotType(selectedSlot);
                if (isStructureBlock(heldS) && heldS != BlockType::LADDER) {
                    const BlockType clickedS = hit.chunk->getBlockType(hit.blockXi, hit.blockYi, hit.blockZi);
                    const bool mergeS = isSlabBlock(heldS) && hit.face == BlockFace::TOP && clickedS == heldS;
                    if (!mergeS) {
                        int sdx, sdy, sdz;
                        getFaceOffset(hit.face, sdx, sdy, sdz);
                        if (clickedS == BlockType::SNOW_LAYER) { sdx = sdy = sdz = 0; }
                        const float swx = hit.chunk->gridToWorldX(hit.blockXi + sdx), swy = hit.chunk->gridToWorldY(hit.blockYi + sdy), swz = hit.chunk->gridToWorldZ(hit.blockZi + sdz);
                        structureBlocked = player->wouldOverlapBlock(swx, swy, swz) || SheepManager::overlapsBlock(swx, swy, swz);
                    }
                }
                if (!structureBlocked && Partial::useItem(*chunkManager, inventory->getSlotType(selectedSlot),
                                     hit.chunk->getChunkX() * WorldConfig::SIZE_X + hit.blockXi, hit.blockYi,
                                     hit.chunk->getChunkZ() * WorldConfig::SIZE_Z + hit.blockZi, hit.face, player->yaw)) {
                    inventory->removeOneFromSlot(selectedSlot);
                    HandAnimations::onBlockPlaced(inventory->isSlotEmpty(selectedSlot));
                    if (SignManager::runPendingEdit()) GameplayHotbar::invalidate();   // cartel recien puesto: pide el texto con el teclado; luego render() recrea el fondo de abajo
                    if (consoleVisible) { consoleDemoInit(); } else { HotbarIcons::render(*inventory, selectedSlot); }
                }
            } else if (hit.hit && !inventory->isSlotEmpty(selectedSlot) && inventory->getSlotType(selectedSlot) == BlockType::FLINT_AND_STEEL) {
                // ItemFlintAndSteel.onItemUse: fuego en la celda de la cara apuntada si ahí hay aire; gasta 1 siempre
                int fdx, fdy, fdz;
                getFaceOffset(hit.face, fdx, fdy, fdz);
                chunkManager->ignite(hit.chunk->getChunkX() * WorldConfig::SIZE_X + hit.blockXi + fdx, hit.blockYi + fdy,
                                     hit.chunk->getChunkZ() * WorldConfig::SIZE_Z + hit.blockZi + fdz);
                inventory->reduceDurability(selectedSlot, 1);
                HandAnimations::onBlockPlaced(inventory->isSlotEmpty(selectedSlot));
                if (consoleVisible) { consoleDemoInit(); } else { HotbarIcons::render(*inventory, selectedSlot); }
            } else if (hit.hit && !inventory->isSlotEmpty(selectedSlot) && isHoeTool(inventory->getSlotType(selectedSlot))) {
                // ItemHoe.onItemUse: tierra o cesped libre arriba -> tierra de cultivo; gasta 1 y el brazo golpea solo si funciono
                if (Partial::useHoe(*chunkManager, hit.chunk->getChunkX() * WorldConfig::SIZE_X + hit.blockXi, hit.blockYi,
                                    hit.chunk->getChunkZ() * WorldConfig::SIZE_Z + hit.blockZi, hit.face)) {
                    inventory->reduceDurability(selectedSlot, 1);
                    HandAnimations::onBlockPlaced(inventory->isSlotEmpty(selectedSlot));
                    if (consoleVisible) { consoleDemoInit(); } else { HotbarIcons::render(*inventory, selectedSlot); }
                }
            } else if (hit.hit && !inventory->isSlotEmpty(selectedSlot) && inventory->getSlotType(selectedSlot) == BlockType::SEEDS) {
                // ItemSeeds.onItemUse: solo por la cara de arriba de tierra de cultivo con aire encima; gasta una semilla
                if (Partial::useSeeds(*chunkManager, hit.chunk->getChunkX() * WorldConfig::SIZE_X + hit.blockXi, hit.blockYi,
                                      hit.chunk->getChunkZ() * WorldConfig::SIZE_Z + hit.blockZi, hit.face)) {
                    inventory->removeOneFromSlot(selectedSlot);
                    HandAnimations::onBlockPlaced(inventory->isSlotEmpty(selectedSlot));
                    if (consoleVisible) { consoleDemoInit(); } else { HotbarIcons::render(*inventory, selectedSlot); }
                }
            } else if (hit.hit && !inventory->isSlotEmpty(selectedSlot) && inventory->getSlotType(selectedSlot) == BlockType::BONE_MEAL) {
                // ItemDye.onItemUse con polvo de hueso: cultivo (crece) o pasto (esparce hierba y flores)
                if (Partial::useBoneMeal(*chunkManager, hit.chunk->getChunkX() * WorldConfig::SIZE_X + hit.blockXi, hit.blockYi,
                                         hit.chunk->getChunkZ() * WorldConfig::SIZE_Z + hit.blockZi)) {
                    inventory->removeOneFromSlot(selectedSlot);
                    HandAnimations::onBlockPlaced(inventory->isSlotEmpty(selectedSlot));
                    if (consoleVisible) { consoleDemoInit(); } else { HotbarIcons::render(*inventory, selectedSlot); }
                }
            } else if (hit.hit && !inventory->isSlotEmpty(selectedSlot) && inventory->getSlotType(selectedSlot) == BlockType::PAINTING) {
                // ItemPainting.onItemUse: en una pared (no arriba ni abajo) cuelga un cuadro con un motivo al azar entre los que caben
                int pdir = -1;
                if (hit.face == BlockFace::NORTH) pdir = 0; else if (hit.face == BlockFace::WEST) pdir = 1; else if (hit.face == BlockFace::SOUTH) pdir = 2; else if (hit.face == BlockFace::EAST) pdir = 3;
                if (pdir >= 0 && PaintingManager::place(*chunkManager, hit.chunk->getChunkX() * WorldConfig::SIZE_X + hit.blockXi, hit.blockYi,
                                                        hit.chunk->getChunkZ() * WorldConfig::SIZE_Z + hit.blockZi, pdir)) {
                    inventory->removeOneFromSlot(selectedSlot);
                    HandAnimations::onBlockPlaced(inventory->isSlotEmpty(selectedSlot));
                    if (consoleVisible) { consoleDemoInit(); } else { HotbarIcons::render(*inventory, selectedSlot); }
                }
            } else if (!inventory->isSlotEmpty(selectedSlot) && isBucket(inventory->getSlotType(selectedSlot))) {
                // ItemBucket.onItemRightClick: rayo de 5 bloques; el balde vacío detecta las fuentes de líquido y el lleno las ignora
                BlockType bucket = inventory->getSlotType(selectedSlot);
                RaycastHit bhit = Raycaster::cast(*chunkManager, player->x, player->y, player->z,
                                                  dirX, dirY, dirZ, 5.0f, bucket == BlockType::BUCKET);
                if (bhit.hit) {
                    int bgx = bhit.chunk->getChunkX() * WorldConfig::SIZE_X + bhit.blockXi;
                    int bgy = bhit.blockYi;
                    int bgz = bhit.chunk->getChunkZ() * WorldConfig::SIZE_Z + bhit.blockZi;
                    bool changed = false;
                    if (bucket == BlockType::BUCKET) {
                        BlockType filled = chunkManager->bucketTakeSource(bgx, bgy, bgz);
                        if (filled != BlockType::AIR) {
                            inventory->setSlotDirect(selectedSlot, filled, 1);
                            changed = true;
                        }
                    } else {
                        int bdx, bdy, bdz;
                        getFaceOffset(bhit.face, bdx, bdy, bdz);
                        BlockType flowing = (bucket == BlockType::WATER_BUCKET) ? BlockType::WATER_FLOW : BlockType::LAVA_FLOW;
                        if (chunkManager->bucketPlaceSource(bgx + bdx, bgy + bdy, bgz + bdz, flowing)) {
                            inventory->setSlotDirect(selectedSlot, BlockType::BUCKET, 1);
                            changed = true;
                        }
                    }
                    if (changed) {
                        if (consoleVisible) { consoleDemoInit(); } else { HotbarIcons::render(*inventory, selectedSlot); }
                    }
                }
            } else if (!inventory->isSlotEmpty(selectedSlot) && hit.hit) {
                BlockType typeToPlace = inventory->getSlotType(selectedSlot);
                if (isPlaceable(typeToPlace)) {
                int dx, dy, dz;
                getFaceOffset(hit.face, dx, dy, dz);
		if (hit.chunk->getBlockType(hit.blockXi, hit.blockYi, hit.blockZi) == BlockType::SNOW_LAYER) { dx = dy = dz = 0; }

                float targetWorldX = hit.chunk->gridToWorldX(hit.blockXi + dx);
                float targetWorldY = hit.chunk->gridToWorldY(hit.blockYi + dy);
                float targetWorldZ = hit.chunk->gridToWorldZ(hit.blockZi + dz);

                bool cactusRuleOk = true;
                if (typeToPlace == BlockType::CACTUS) {
                    float belowX = targetWorldX;
                    float belowY = targetWorldY - 1.0f;
                    float belowZ = targetWorldZ;
                    BlockType belowType = chunkManager->getBlockTypeAt(belowX, belowY, belowZ);
                    bool belowOk = (belowType == BlockType::SAND || belowType == BlockType::CACTUS);

                    bool sidesOk = true;
                    float checkOffsets[4][2] = {{1,0}, {-1,0}, {0,1}, {0,-1}};
                    for (int i = 0; i < 4 && sidesOk; i++) {
                        float checkX = targetWorldX + checkOffsets[i][0];
                        float checkZ = targetWorldZ + checkOffsets[i][1];
                        BlockType sideType = chunkManager->getBlockTypeAt(checkX, targetWorldY, checkZ);
                        if (isMaterialSolid(sideType)) sidesOk = false; // BlockCactus.canBlockStay: cuenta el material sólido, no solo el aire
                    }

                    cactusRuleOk = belowOk && sidesOk;
                }

                // la antorcha no tiene caja de colisión (getCollisionBoundingBoxFromPool == null): se puede poner donde estás parado
                if (cactusRuleOk && (isTorchBlock(typeToPlace) || (!player->wouldOverlapBlock(targetWorldX, targetWorldY, targetWorldZ) && !SheepManager::overlapsBlock(targetWorldX, targetWorldY, targetWorldZ)))) {
                    BlockType typeToPlace = inventory->getSlotType(selectedSlot);
                    BlockType placedType = typeToPlace;
                    if (typeToPlace == BlockType::SUGAR_CANE_ITEM) placedType = BlockType::SUGAR_CANE;   // el objeto pone el bloque
                    // BlockPumpkin.onBlockPlacedBy: la cara tallada mira hacia el jugador
                    Chunk::s_placeMeta = (typeToPlace == BlockType::PUMPKIN || typeToPlace == BlockType::JACK_O_LANTERN) ? furnaceFrontDirFromYaw(player->yaw) : (isLeavesBlock(typeToPlace) ? 8 : -1);   // ItemLeaves.getPlacedBlockMetadata: damage | 8 (la hoja puesta se revisa y cae si no hay tronco)
                    if (typeToPlace == BlockType::DISPENSER) placedType = makeDispenserBlock(furnaceFrontDirFromYaw(player->yaw));   // BlockDispenser.onBlockPlacedBy: el frente mira al jugador
                    if (typeToPlace == BlockType::FURNACE) {
                        // TEMPORAL: con la consola abierta (X) se coloca encendido, para probar la textura
                        placedType = makeFurnaceBlock(furnaceFrontDirFromYaw(player->yaw), false);
                    }
                    const bool placedOk = hit.chunk->placeBlockAt(hit.blockXi, hit.blockYi, hit.blockZi, hit.face, placedType, chunkManager);
                    Chunk::s_placeMeta = -1;
                    if (placedOk) {
                        Sound::blockPlace(placedType, hit.chunk->gridToWorldX(hit.blockXi), hit.chunk->gridToWorldY(hit.blockYi), hit.chunk->gridToWorldZ(hit.blockZi));
                        inventory->removeOneFromSlot(selectedSlot);
			HandAnimations::onBlockPlaced(inventory->isSlotEmpty(selectedSlot));
                        if (consoleVisible) { consoleDemoInit(); } else { HotbarIcons::render(*inventory, selectedSlot); }
                    }
                    }
                }
            }
	}

        float lookDirX, lookDirY, lookDirZ;
        player->getLookDirection(lookDirX, lookDirY, lookDirZ);

        // Pausa (Minecraft.isGamePaused): sin avanzar frameCounter nunca llega el tick de 20 Hz (mobs, tiempo, clima, liquidos, redstone,
        // particulas, mano...) y la hora no corre. Se sigue dibujando la misma escena; al volver todo sigue donde quedo, sin ticks atrasados.
        if (!paused) frameCounter++;
        renderFrameCounter++;
        bool shouldRender = (renderFrameCounter % 3) == 0;

        if (!paused) gameTimeTicks = (gameTimeTicks + 1) % CYCLE_TICKS;
        int skylightStep = computeSkylightSubtract(gameTimeTicks);
        if (skylightStep != lastSkylightStep) {
            lastSkylightStep = skylightStep;
            chunkManager->setSkylightSubtract(skylightStep);
        }

        int skyR, skyG, skyB;
        if (currentDimension == -1) {
            // WorldProviderHell.func_4096_a: el fondo y la niebla del Nether son siempre (0.2, 0.03, 0.03), sin ciclo de día y noche
            skyR = NETHER_SKY_R; skyG = NETHER_SKY_G; skyB = NETHER_SKY_B;
        } else {
            float nightFactor = computeNightFactor(gameTimeTicks);
            // EntityRenderer.updateFogColor: con lluvia el cielo se vuelve gris (mezcla hacia un gris oscuro) y con tormenta oscurece mas; el rayo lo ilumina
            const float rn = Weather::rain(partialTick), th = Weather::thunder(partialTick);
            float br = (1.0f - nightFactor) * (1.0f - rn * 0.45f) * (1.0f - th * 0.4f);
            float fl = Weather::flash(partialTick);
            skyR = (int)(20 * br + 11 * fl); skyG = (int)(20 * br + 11 * fl); skyB = (int)(31 * br + 0 * fl);
            if (skyR > 31) skyR = 31; if (skyG > 31) skyG = 31; if (skyB > 31) skyB = 31;
            if (rn > 0.0f) { skyB = skyB * 3 / 4 + (int)(skyG * 1 / 4); skyG = skyG * 9 / 10; }
        }
        glClearColor(skyR, skyG, skyB, 31);
        glFogColor(skyR, skyG, skyB, 31);

	if (shouldRender) {
            // La mano va PRIMERO — así nunca se pierde aunque el terreno
            // sature el presupuesto de polígonos del frame (la GPU descarta
            // en silencio lo que venga DESPUÉS del límite, no lo que ya se mandó).
            {
                HandAnimations::RenderInput hr;
                hr.partialTicks = partialTick;
                hr.pitch = player->pitch;
                hr.yaw = player->yaw;
                hr.posX = player->x; hr.posZ = player->z; hr.dimension = currentDimension;
                hr.eyeLight = HandAnimations::lightLevelAt(*chunkManager, player->x, player->y, player->z);
                // getEntityBrightness: pies + 1.8 * 0.66 = ojos - 0.432
                hr.bodyLight = HandAnimations::lightLevelAt(*chunkManager, player->x, player->y - 0.432f, player->z);
                if (!healthState.dead && !thirdPerson && !sleeping) HandAnimations::render(hr);
            }

            HealthHud::draw(healthState, healthState.armorPoints, hudHeadInWater);   // barra de armadura sobre los corazones

	    if (!healthState.dead && !sleeping) {
            glMatrixMode(GL_MODELVIEW);
            glPushMatrix();
            glLoadIdentity();
            glTranslatef(0.0f, 0.0f, -0.15f); // muy cerca de la cámara

            glColor3b(255, 255, 255);
            glBindTexture(0, 0); // sin textura, color plano

            const float halfLen = 0.006f;
            const float halfThick = 0.0006f;

            glBegin(GL_QUADS);
                // Línea horizontal
                glVertex3v16(floattov16(-halfLen), floattov16(-halfThick), floattov16(0));
                glVertex3v16(floattov16( halfLen), floattov16(-halfThick), floattov16(0));
                glVertex3v16(floattov16( halfLen), floattov16( halfThick), floattov16(0));
                glVertex3v16(floattov16(-halfLen), floattov16( halfThick), floattov16(0));

                // Línea vertical
                glVertex3v16(floattov16(-halfThick), floattov16(-halfLen), floattov16(0));
                glVertex3v16(floattov16( halfThick), floattov16(-halfLen), floattov16(0));
                glVertex3v16(floattov16( halfThick), floattov16( halfLen), floattov16(0));
                glVertex3v16(floattov16(-halfThick), floattov16( halfLen), floattov16(0));
            glEnd();
        glPopMatrix(1);
            }

            { PROF(Prof::DRAW_ALL); chunkManager->drawAll(player->x, player->y, player->z, lookDirX, lookDirY, lookDirZ); }
            const u32 entsStartTicks = Prof::now();   // lo que se dibuja despues del terreno (mobs, objetos, particulas, clima, HUD...)

            if (breakHeld && hit.hit && miningChunk != nullptr && miningDamage > 0.0f) { // sin grietas en la bedrock (su fuerza es 0)
                float progress = miningDamage < 1.0f ? miningDamage : 0.999f;
                drawBreakingOverlay(miningChunk, miningXi, miningYi, miningZi, progress);
            }

            for (int i = 0; i < MAX_DROPPED_ITEMS; i++) {
                droppedItems[i].draw(player->x, player->y, player->z, player->yaw, partialTick);
            }
            FallingBlocks::draw(player->x, player->y, player->z, partialTick);
            Pistons::draw(player->x, player->y, player->z, partialTick);   // bloques que se estan moviendo

            MobCull::set(player->x, player->y, player->z, lookDirX, lookDirY, lookDirZ, WorldConfig::renderDistance() + 2.0f);   // mobs off screen are not drawn
            SheepManager::setCamera(player->yaw, player->pitch);
            SheepManager::draw(*chunkManager, partialTick);
            MinecartManager::draw(*chunkManager, partialTick);
            BoatManager::draw(*chunkManager, partialTick);
            if (currentDimension == 0) Spawners::draw(*chunkManager, partialTick);
            PaintingManager::draw(*chunkManager, player->x, player->y, player->z);
            SignManager::draw(*chunkManager, player->x, player->y, player->z);
            TntManager::draw(*chunkManager, player->x, player->y, player->z, partialTick);
            Particles::draw(*chunkManager, player->x, player->y, player->z, player->yaw, player->pitch);
            if (currentDimension == 0) Clouds::draw(partialTick, player->x, player->y, player->z, computeSkyBrightness(gameTimeTicks), Weather::rain(partialTick), Weather::thunder(partialTick), WorldConfig::viewRadius() >= 3);
            if (currentDimension == 0 && WorldConfig::viewRadius() >= 2) Sky::draw(player->x, player->y, player->z, computeCelestialAngle(gameTimeTicks), Weather::rain(partialTick), skyR, skyG, skyB);   // sol, luna y resplandor: solo en ds y dsi
            if (currentDimension == 0) Weather::draw(partialTick, player->x, player->y, player->z, player->yaw, computeSkyBrightness(gameTimeTicks));
            Projectiles::draw(*chunkManager, partialTick, player->yaw, player->pitch);   // flechas (modelo 3D), bolas de nieve y huevos
            if (FishingHook::active()) {   // el anzuelo y su linea
                float frx, fry, frz;
                player->getRenderPosition(partialTick, frx, fry, frz);
                FishingHook::draw(*chunkManager, partialTick, player->yaw, player->pitch, frx, fry, frz, player->yaw, player->pitch, HandAnimations::getSwingProgress(partialTick), thirdPerson);
            }

            if (thirdPerson) {   // el jugador (ModelBiped); muerto, la caja mide 0,2 y los ojos quedan a 0,1 del suelo
                float rx, ry, rz;
                player->getRenderPosition(partialTick, rx, ry, rz);
                const float feetY = ry - (sleeping ? 0.2f : (healthState.dead ? 0.1f : 1.62f));   // dormido la caja mide 0,2 y los ojos quedan a 0,2 de los pies
                const bool emptyHeld = inventory->isSlotEmpty(selectedSlot);
                signed char armorMats[4];
                for (int a = 0; a < 4; a++) {
                    const int as = Inventory::ARMOR_BASE + a;
                    armorMats[a] = (!inventory->isSlotEmpty(as) && isArmor(inventory->getSlotType(as))) ? (signed char)armorMaterial(inventory->getSlotType(as)) : (signed char)-1;
                }
                SteveModel::draw(rx, feetY, rz, player->yaw, player->pitch, partialTick, HandAnimations::getSwingProgress(partialTick),
                                 HandAnimations::lightLevelAt(*chunkManager, rx, ry - 0.432f, rz),
                                 healthState.hurtTime > 0 || healthState.deathTime > 0, healthState.deathTime, player->isSneaking(),
                                 emptyHeld ? BlockType::AIR : inventory->getSlotType(selectedSlot),
                                 MinecartManager::isRiding() || BoatManager::isRiding() || AnimalManager::isRiding(),
                                 sleeping ? bedDir : -1, armorMats);
            }

            if (hit.hit && sheepTarget < 0 && cartTarget < 0 && boatTarget < 0 && paintTarget < 0 && !sleeping) {
                hit.chunk->drawOutlineAt(hit.blockXi, hit.blockYi, hit.blockZi);
            }

            if (healthState.fire > 0 && !thirdPerson && !sleeping) drawFireOverlay(gameTimeTicks);   // ItemRenderer.renderOverlays: isBurning
            if (healthState.dead) drawDeathOverlay();
            if (sleepTimer > 0) drawSleepOverlay(Sleeping::overlayAmount(sleepTimer, partialTick) * (thirdPerson ? 0.4f : 1.0f));   // GuiIngame: el velo oscuro al dormir y al despertar (en tercera persona mas claro, para ver al jugador tendido)
            if (timeInPortal > 0.0f) drawPortalOverlay(timeInPortal);

            Prof::add(Prof::ENTS, Prof::now() - entsStartTicks);
            glFlush(0);
        }

	// --- Diagnóstico de límites de la GPU (visible con el toggle de consola, tecla X) ---
        static int gfxOverflowCount = 0;
        static int gfxUnderflowCount = 0;
        if (GFX_CONTROL & GL_POLY_OVERFLOW)   gfxOverflowCount++;
        if (GFX_CONTROL & GL_COLOR_UNDERFLOW) gfxUnderflowCount++;
        GFX_CONTROL |= GL_POLY_OVERFLOW | GL_COLOR_UNDERFLOW; // se limpian escribiendo 1
	static int rdLinesMin = 46;
        int rdLines = (*(vu32*)0x04000320) & 0x3F; // GFX_RDLINES_COUNT: margen mínimo del frame anterior
        if (rdLines < rdLinesMin) rdLinesMin = rdLines;
        

	if (canaryBeforeSlot != 0xDEADBEEF || canaryAfterSlot != 0xCAFEBABE ||
            canaryBeforeInventoryPtr != 0xFEEDFACE || canaryAfterInventoryPtr != 0xBAADF00D) {
            consoleDemoInit();
            iprintf("CANARY BROKEN!\n");
            iprintf("before slot: %x\n", canaryBeforeSlot);
            iprintf("after slot: %x\n", canaryAfterSlot);
            iprintf("before inv: %x\n", canaryBeforeInventoryPtr);
            iprintf("after inv: %x\n", canaryAfterInventoryPtr);
            iprintf("selectedSlot: %d\n", selectedSlot);
            swiWaitForVBlank();
            swiWaitForVBlank();
            swiWaitForVBlank();
        }

	if (diagInitialized) {
                if (selectedSlot != prevSelectedSlotDiag && !selectedSlotChangedByTouch) {
                    // Blindaje: algo cambió selectedSlot sin pasar por
                    // un toque real de la hotbar. En vez de investigar
                    // la causa exacta (no es obvia), lo revertimos acá
                    // mismo, en silencio, sin frenar el frame — nada
                    // que no sea un toque real de la hotbar puede
                    // hacer que el slot cambie de verdad.
                    selectedSlot = prevSelectedSlotDiag;
                }

                for (int i = 0; i < Inventory::TOTAL_SIZE; i++) {
                    if (!inventory->isSlotEmpty(i) && hasDurability(inventory->getSlotType(i))) {
                        int curDur = inventory->getSlotDurability(i);
                        int prevDur = prevDurabilityDiag[i];
                        if (prevDur > 3 && curDur == 1) {
                            consoleDemoInit();
                            iprintf("DIAG: durability slot %d\n", i);
                            iprintf("  dropped from %d to %d\n", prevDur, curDur);
                            iprintf("  mining active: %d\n", (miningChunk != nullptr));
                            swiWaitForVBlank();
                        }
                    }
                }
            }

            selectedSlotChangedByTouch = false;

            for (int i = 0; i < Inventory::TOTAL_SIZE; i++) {
                prevDurabilityDiag[i] = inventory->isSlotEmpty(i) ? 0 : inventory->getSlotDurability(i);
            }
            prevSelectedSlotDiag = selectedSlot; // <-- esta línea faltaba
            diagInitialized = true;

        Prof::add(Prof::FRAME, Prof::now() - frameStartTicks);
        Governor::onFrameEnd(Prof::now() - frameStartTicks);
        Prof::setContext((int)floorf(player->x), (int)floorf(player->y), (int)floorf(player->z), currentDimension);
        Prof::endFrame();
        static int profPrintCounter = 0;
        if (consoleVisible && !paused && ++profPrintCounter >= 30) {   // never over the pause menu
            profPrintCounter = 0;
            if (Options::get().debugCoordsOnly) {
                iprintf("\x1b[2J");   // solo coordenadas: sin rendimiento ni memoria
            } else {
                Prof::print();
                iprintf("underflow %d\noverflow  %d\nRDLINES min %d\n",
                        gfxUnderflowCount, gfxOverflowCount, rdLinesMin);
                int dlKB0 = Chunk::getDisplayListBytes() / 1024;
                if (dlKB0 > dlMaxKB) dlMaxKB = dlKB0;
                iprintf("DL %d KB (max %d)\nChunks %dx%dK=%dK\n", dlKB0, dlMaxKB, chunkManager->chunkCount(), (int)(sizeof(Chunk) / 1024), chunkManager->poolKB());
                iprintf("Health %d/20  damage %s (X) night (B)\n", healthState.health, healthDamageEnabled ? "ON" : "OFF");
            }
            rdLinesMin = 46;
            { // coordenadas: en el Nether 1 bloque equivale a 8 de la superficie (la ultima linea sin salto para no desplazar la consola)
                const int cx = (int)floorf(player->x), cz = (int)floorf(player->z);
                iprintf("XYZ %d %d %d\n", cx, (int)floorf(player->y - 1.62f + 1.5f), cz);
                if (currentDimension == -1) iprintf("Overworld: %d %d\n", cx * 8, cz * 8);
                else iprintf("Nether: %d %d\n", (int)floorf(player->x / 8.0f), (int)floorf(player->z / 8.0f));
                iprintf("Seed %u  terrain gen %d", chunkManager->getSeed(), WorldConfig::terrainGen());   // la ultima linea, sin salto; gen 0 = original, 1 = sin mesetas
            }
        }

        {
            // Ritmo del juego: el motor dibuja 1 de cada 3 cuadros y hace un tick cada 3. Si el cuadro que dibuja se pasa de 16,7 ms, el
            // swiWaitForVBlank lo redondea a 2 vblanks y el tick completo dura 4 en vez de 3 (el juego va al 75 %). Esa demora se anota como deuda y los
            // cuadros siguientes (que no dibujan) se saltan la espera hasta saldarla: el tick vuelve a durar 50 ms mientras el promedio alcance.
            static u32 s_lagTicks = 0;
            const u32 FRAME_TICKS = 560190;   // 33513982 / 59,8261 Hz
            const u32 work = Prof::now() - frameStartTicks;
            bool skipWait = false;
            if (work >= FRAME_TICKS) {
                s_lagTicks += ((work + FRAME_TICKS - 1) / FRAME_TICKS) * FRAME_TICKS - FRAME_TICKS;
                if (s_lagTicks > 4 * FRAME_TICKS) s_lagTicks = 4 * FRAME_TICKS;
            } else if (!shouldRender && s_lagTicks >= FRAME_TICKS - work) {
                s_lagTicks -= FRAME_TICKS - work;
                skipWait = true;
            }
            if (!skipWait) {
                swiWaitForVBlank();
                if (!paused) TextureAnim::onVBlank(); // al empezar el VBlank el 3D no dibuja: se puede escribir en la VRAM de texturas (en pausa, fuego y portal quietos)
            }
        }

        if (frameCounter >= 3) {
            PROF(Prof::LOGIC);
            frameCounter = 0;

            u32 keysForPhysics = uiBlocking ? 0 : Controls::gameHeld(keys_held);
            if (!uiBlocking && controlMode == 1 && touchJumpHeld) keysForPhysics |= KEY_A;   // xyba: la zona tactil tambien salta (ademas del boton que se asigne)
            // EntityPlayerSP.onLivingUpdate: dentro de un portal el contador sube 0,0125 por tick (80 ticks = 4 s); al llegar a 1 se viaja.
            // Fuera del portal baja de a 0,05. La bandera la fija el tick anterior (Entity.setInPortal).
            {
                bool travelNow = false;
                if (inPortalFlag) {
                    timeInPortal += 0.0125f;
                    if (timeInPortal >= 1.0f) { timeInPortal = 1.0f; timeUntilPortal = 10; travelNow = true; }
                    inPortalFlag = false;
                } else {
                    if (timeInPortal > 0.0f) timeInPortal -= 0.05f;
                    if (timeInPortal < 0.0f) timeInPortal = 0.0f;
                }
                if (timeUntilPortal > 0) --timeUntilPortal;
                if (travelNow) { changeDimension(); continue; }
            }

            // EntityPlayer.onUpdate + World.tick: dormido el contador sube hasta 100; sin cama o de dia te levantas; con todos dormidos del todo la noche pasa.
            // Al despertar el contador sigue hasta 110 (el velo se va)
            if (sleeping) {
                if (sleepTimer < Sleeping::FULL_ASLEEP) ++sleepTimer;
                if (typeAtCell(bedGx, bedGy, bedGz) != BlockType::BED_BLOCK) {
                    wakeUp(true, false);                                           // isInBed: la cama ya no esta
                } else if (computeSkylightSubtract(gameTimeTicks) < 4) {
                    wakeUp(false, true);                                           // isDaytime: amanecio
                } else if (sleepTimer >= Sleeping::FULL_ASLEEP) {
                    int offX, offZ;
                    Partial::bedOffset(bedDir, offX, offZ);
                    if (HostileManager::sleepSpawn(*chunkManager, player->x, player->z, bedGx - 4, bedGy - 1, bedGz - 4, offX, offZ)) {
                        wakeUp(true, false);                                       // performSleepSpawning: aparecio un monstruo, te despiertas y la noche sigue
                    } else {
                        gameTimeTicks = 0;                                         // setWorldTime(siguiente multiplo de 24000): amanece
                        wakeUp(false, true);                                       // wakeUpAllPlayers
                        Weather::clearAfterSleep();                                // wakeUpAllPlayers -> clearWeather: se acaba la lluvia
                    }
                }
            } else if (sleepTimer > 0) {
                if (++sleepTimer >= Sleeping::FADE_END) sleepTimer = 0;
            }

            if (!MinecartManager::isRiding() && !BoatManager::isRiding() && !AnimalManager::isRiding() && !sleeping) player->physicsTick(keysForPhysics, *chunkManager, autoJumpEnabled);   // en una vagoneta el jugador va sentado y en la cama tendido: no se mueve solo
            Sound::init();
            healthState.isPlayer = true; healthState.difficulty = Options::get().difficulty;   // el daño depende de la dificultad
            Sound::setListener(player->x, player->y, player->z, player->yaw);
            // Entity.moveEntity -> Block.onEntityWalking: la distancia caminada (x 0,6) pasa de 1 en 1 y el bloque bajo los pies (0,2 mas abajo) lo pisa; la tierra de cultivo vuelve a tierra 1 de cada 4
            if (!sleeping && !healthState.dead && !MinecartManager::isRiding() && !BoatManager::isRiding() && !AnimalManager::isRiding()) {
                const float tdx = player->x - tramplePrevX, tdz = player->z - tramplePrevZ;
                if (tramplePrevValid && tdx * tdx + tdz * tdz < 4.0f && !(player->isOnGround() && player->isSneaking())) {
                    trampleWalked += sqrtf(tdx * tdx + tdz * tdz) * 0.6f;
                    if (trampleWalked > (float)trampleNextStep) {
                        ++trampleNextStep;
                        const int tgx = (int)floorf(player->x + 4.0f), tgz = (int)floorf(player->z + 4.0f);
                        const int tgy = (int)floorf(player->y - 1.62f - 0.2f + 1.5f);
                        const BlockType underFoot = typeAtCell(tgx, tgy, tgz);
                        if (player->isOnGround()) Sound::footstep(underFoot);
                        if (underFoot == BlockType::FARMLAND && (rand() & 3) == 0) Partial::trample(*chunkManager, tgx, tgy, tgz);
                    }
                }
                tramplePrevX = player->x; tramplePrevZ = player->z; tramplePrevValid = true;
            } else {
                tramplePrevValid = false;
            }
            { PROFSTEP("t liquids"); chunkManager->tickLiquids(); } // agua y lava: un tick del mundo por cada tick del juego
            Redstone::tick(*chunkManager); // redstone: ticks programados y placas de presion
            { PROFSTEP("t pistons"); Pistons::tick(*chunkManager); }  // bloques empujados por pistones
            if (player->isOnGround() && !player->isSneaking() && (player->getTickMoveX() != 0.0f || player->getTickMoveZ() != 0.0f)) {
                // Entity.moveEntity -> BlockRedstoneOre.onEntityWalking: la mena que se pisa se enciende
                const int ogx = (int)floorf(player->x + 4.0f), ogz = (int)floorf(player->z + 4.0f);
                const int ogy = (int)floorf(player->y - 1.62f + 1.5f - 0.2f);
                Redstone::lightOre(*chunkManager, ogx, ogy, ogz);
            }
            FallingBlocks::update(*chunkManager); // arena y grava cayendo
            { PROFSTEP("t tnt"); TntManager::tick(*chunkManager); }       // TNT encendido: cae, parpadea y explota
            {   // flechas, bolas de nieve y huevos en vuelo; la flecha clavada del jugador se recoge al pisarla
                // el jugador tambien es blanco de sus flechas (a los 5 ticks de vuelo) y de las del dispensador; dormido la caja mide 0,2
                ArrowHurtCtx arrowCtx = { &healthState, &player };
                Projectiles::PlayerTarget arrowTarget;
                arrowTarget.valid = !healthState.dead;
                arrowTarget.hurt = &arrowHurtsPlayer;
                arrowTarget.ctx = &arrowCtx;
                {
                    const float hw = sleeping ? 0.1f : 0.3f, feet = sleeping ? player->y - 0.2f : player->y - 1.62f, top = sleeping ? player->y : player->y + 0.18f;
                    arrowTarget.box[0] = player->x - hw; arrowTarget.box[1] = feet; arrowTarget.box[2] = player->z - hw;
                    arrowTarget.box[3] = player->x + hw; arrowTarget.box[4] = top;  arrowTarget.box[5] = player->z + hw;
                }
                if (!worldSpawnSet) { worldSpawnX = player->x; worldSpawnZ = player->z; worldSpawnSet = true; }
                AnimItems::tick(player->x, player->z, player->yaw, worldSpawnX, worldSpawnZ, computeCelestialAngle(gameTimeTicks));
                // clima: lluvia/nieve/tormenta (solo en la superficie); el rayo daña al jugador a menos de 3 bloques (EntityLightningBolt -> onStruckByLightning: 5 de daño y fuego)
                Clouds::tick();
                { PROFSTEP("t weather"); Weather::tick(*chunkManager, player->x, player->y, player->z, currentDimension == 0); }
                { PROFSTEP("t particles"); Particles::tick(*chunkManager, player->x, player->y, player->z); }
                healthState.isPlayer = true; healthState.difficulty = Options::get().difficulty;
                HostileManager::setPeaceful(Options::get().difficulty == 0);   // pacifico: sin monstruos
                if (Options::get().difficulty == 0 && !healthState.dead && healthState.health < Health::MAX_HEALTH) {   // EntityPlayer: en pacifico regenera 1 de vida por segundo
                    static int peacefulTimer = 0;
                    if (++peacefulTimer >= 20) { peacefulTimer = 0; Health::heal(healthState, 1); }
                }
                if (currentDimension == 0 && !healthState.dead) Spawners::tick(*chunkManager, player->x, player->y, player->z);   // generadores de monstruos de las mazmorras
                {
                    float sx, sy, sz;
                    while (Weather::popStrike(sx, sy, sz)) {
                        const float ddx = player->x - sx, ddz = player->z - sz;
                        Sound::thunder(sx, sy, sz);
                        SheepManager::lightning(sx, sy, sz);   // ovejas, animales (el cerdo se vuelve zombi cerdo) y hostiles cercanos
                        if (!healthState.dead && ddx * ddx + ddz * ddz < 9.0f && player->y > sy - 3.0f) {
                            if (Health::attack(healthState, 5)) Sound::playerHurt();
                            if (healthState.fire <= 0) healthState.fire = 300;
                        }
                    }
                }
                // ItemMap.onUpdate: el mapa de la mano se asigna (primera vez) y explora el terreno de alrededor
                if (!inventory->isSlotEmpty(selectedSlot) && inventory->getSlotType(selectedSlot) == BlockType::MAP && !healthState.dead) {
                    int mapId = inventory->getSlotDurability(selectedSlot);
                    if (mapId <= 0) {
                        mapId = Maps::create((int)floorf(player->x), (int)floorf(player->z), currentDimension);
                        if (mapId > 0) inventory->setSlotDurability(selectedSlot, mapId);
                    }
                    if (mapId > 0) Maps::tick(*chunkManager, mapId, player->x, player->z, currentDimension);
                }
                AnimItems::setInventoryAnimated(Options::get().clockCompassInvUpdate);
                if (AnimItems::takeChanged() && Options::get().clockCompassInvUpdate && !uiBlocking && !consoleVisible) {   // la brujula o el reloj de la barra cambiaron de cuadro: se vuelve a dibujar
                    bool animInBar = false;
                    for (int q = 0; q < Inventory::HOTBAR_SIZE; q++)
                        if (!inventory->isSlotEmpty(q) && (inventory->getSlotType(q) == BlockType::COMPASS || inventory->getSlotType(q) == BlockType::CLOCK)) animInBar = true;
                    if (animInBar) HotbarIcons::render(*inventory, selectedSlot);
                }
                FishingHook::tick(*chunkManager, player->x, player->y - player->getEyeDrop(1.0f), player->z,
                                  !inventory->isSlotEmpty(selectedSlot) && inventory->getSlotType(selectedSlot) == BlockType::FISHING_ROD, !healthState.dead);
                const int pickedArrows = Projectiles::tick(*chunkManager, player->x, player->y, player->z, !healthState.dead && inventory->canAccept(BlockType::ARROW), &arrowTarget);
                if (pickedArrows > 0) {
                    bool usedNewSlot = false;
                    inventory->addItem(BlockType::ARROW, pickedArrows, usedNewSlot);
                    if (consoleVisible) { consoleDemoInit(); } else { HotbarIcons::render(*inventory, selectedSlot); }
                }
            }
            NoteBlocks::setListener(player->x, player->y, player->z);
            NoteBlocks::tickSound();               // corta las notas que ya duraron lo suyo
            {   // lo que expulsa un dispensador sale como objeto suelto
                Dispenser::Drop dd;
                while (Dispenser::popDrop(dd)) {
                    for (int i = 0; i < MAX_DROPPED_ITEMS; i++) {
                        if (!droppedItems[i].isActive()) {
                            droppedItems[i].spawn(dd.x, dd.y, dd.z, dd.type, true, dd.vx, dd.vy, dd.vz, dd.durability);
                            break;
                        }
                    }
                }
            }

            // Vida (Health.h): un tick del jugador con lo que tiene alrededor
            {
                const float px = player->x, py = player->y, pz = player->z;
                const float eyeY = py + 0.12f;   // EntityPlayer.getEyeHeight
                Health::Env env;
                env.inWater = player->wasInWater();
                env.inLava = player->wasInLava();
                env.headInWater = chunkManager->isInsideOfMaterial(px, eyeY, pz, false);
                env.insideBlock = chunkManager->insideOpaqueBlock(px, eyeY, pz, sleeping ? 0.2f : 0.6f);   // sleepInBedAt: setSize(0.2, 0.2). Con la almohada contra una pared, el cuerpo de 0,6 asomaba 0,2 dentro del bloque y asfixiaba
                env.burningBox = chunkManager->fireInBox(px - 0.299f, py - 1.619f, pz - 0.299f, px + 0.299f, py + 0.179f, pz + 0.299f)
                              || chunkManager->lavaInBox(px - 0.299f, py - 1.619f, pz - 0.299f, px + 0.299f, py + 0.179f, pz + 0.299f);
                // el cactus de la Beta tiene la caja 1/16 más chica: se lo toca aunque se quede a unos centímetros
                env.cactus = chunkManager->typeInBox(BlockType::CACTUS, px - 0.45f, py - 1.77f, pz - 0.45f, px + 0.45f, py + 0.17f, pz + 0.45f);
                env.fallLanded = player->takeFallDistance();
                healthState.enabled = healthDamageEnabled || Options::get().difficulty != 0;   // el toggle de daño solo vale en pacifico
                healthState.armorPoints = inventory->getTotalArmorValue();   // la armadura puesta reduce el daño (EntityPlayer.damageEntity)
                static bool wasInWater = false;
                if (env.inWater && !wasInWater) Sound::splash(player->x, player->y - 1.0f, player->z, 0.8f);
                wasInWater = env.inWater;
                if (env.headInWater && (gameTimeTicks % 12) == 0)   // burbujas de aire frente a la cara
                    Particles::spawn(Particles::BUBBLE, player->x + ((rand() & 255) / 255.0f - 0.5f) * 0.4f - sinf(player->yaw * 0.0174532925f) * 0.4f, player->y - 0.2f, player->z + ((rand() & 255) / 255.0f - 0.5f) * 0.4f - cosf(player->yaw * 0.0174532925f) * 0.4f, 0.0f, 0.0f, 0.0f);
                const int hpBeforeTick = healthState.health;
                Health::tick(healthState, env);
                if (healthState.health < hpBeforeTick) Sound::playerHurt();   // caida, fuego, lava, ahogo, cactus...
                hudHeadInWater = env.headInWater;
            }

            // EntityPlayer.onDeath: suelta todo el inventario (cada pila al azar, como dropPlayerItemWithRandomChoice), queda caído en el suelo
            // y la pantalla de abajo pasa a Game Over
            if (healthState.dead && !deathHandled) {
                deathHandled = true;
                paused = false; pauseView = 0;
                if (furnaceOpen || chestOpen) closeFurnaceMenu();
                inventoryOpen = false;
                craftingOpen = false;
                consoleVisible = false;
                videoSetMode(MODE_0_3D);

                for (int s = 0; s < Inventory::SLOT_COUNT; s++) {   // tambien la armadura puesta
                    if (inventory->isSlotEmpty(s)) continue;
                    const BlockType t = inventory->getSlotType(s);
                    const bool tool = hasDurability(t);
                    const int durability = tool ? inventory->getSlotDurability(s) : -1;
                    const int amount = tool ? 1 : inventory->getSlotCount(s);
                    const float angle = (float)(rand() % 6283) / 1000.0f;
                    const float force = (float)(rand() % 500) / 1000.0f;
                    for (int i = 0; i < MAX_DROPPED_ITEMS; i++) {
                        if (!droppedItems[i].isActive()) {
                            droppedItems[i].spawn(player->x, player->y - 0.18f, player->z, t, true,
                                                  -sinf(angle) * force, 0.2f, cosf(angle) * force, durability, amount);
                            inventory->removeFromSlot(s, amount);
                            break;
                        }
                    }
                }
                deathCamX = player->x; deathCamY = player->y; deathCamZ = player->z;
                player->setDead();
                GameOverScreen::show();
            }

            // Ovejas: movimiento, aparición y desaparición (SheepManager); lo que sueltan al morir pasa a ser ítems en el suelo
            SheepManager::setHeld(inventory->isSlotEmpty(selectedSlot) ? BlockType::AIR : inventory->getSlotType(selectedSlot));
            { PROFSTEP("t mobs"); SheepManager::tick(*chunkManager, player->x, player->y, player->z, !healthState.dead); }
            if (!healthState.dead && !AnimalManager::isRiding()) {   // empuje mutuo con todos los mobs (Entity.applyEntityCollision): no se amontonan en el mismo punto
                float pvx, pvz;
                SheepManager::pushWithPlayer(player->x, player->y, player->z, pvx, pvz);
                if (pvx != 0.0f || pvz != 0.0f) player->addVelocity(pvx, 0.0f, pvz);
            }
            // Golpes de los hostiles (puño del zombie, flecha, explosion): vida, invulnerabilidad y empuje del jugador (EntityLiving.attackEntityFrom + knockBack)
            {
                HostileManager::PlayerHit ph;
                while (HostileManager::popPlayerHit(ph)) {
                    const bool newHit = !((float)healthState.heartsLife > (float)Health::HEARTS_HALVES_LIFE / 2.0f);
                    if (Health::attack(healthState, ph.amount)) {
                        if (!ph.explosion) Sound::playerHurt();
                        if (newHit) player->knockBack(ph.srcX, ph.srcZ);
                        if (ph.explosion) player->addVelocity(ph.kx, ph.ky, ph.kz);
                    }
                }
            }
            // InventoryPlayer.damageArmor: cada golpe recibido desgasta las piezas puestas
            for (int w = 0; w < healthState.armorWearCount; w++) inventory->damageArmor(healthState.armorWear[w]);
            healthState.armorWearCount = 0;
            // EntityPlayer.attackEntityFrom: cualquier dano despierta a quien duerme (isSleeping -> wakeUpPlayer(true, true, false))
            if (sleeping && healthState.health < sleepLastHealth) wakeUp(true, false);
            sleepLastHealth = healthState.health;
            // Vagonetas: fisica sobre los rieles; el jugador que va dentro se coloca donde le toca (agacharse = bajarse); lo que sueltan pasa a ser items
            { PROFSTEP("t carts"); MinecartManager::tick(*chunkManager, player->x, player->y, player->z); }
            {
                float rx, ry, rz;
                if (MinecartManager::takeForcedDismount(rx, ry, rz)) {
                    player->teleportTo(rx, ry, rz);
                } else if (MinecartManager::isRiding()) {
                    if (player->isSneaking()) {
                        if (MinecartManager::dismount(rx, ry, rz)) { player->setSneak(false); player->teleportTo(rx, ry, rz); }
                    } else if (MinecartManager::riderEye(rx, ry, rz)) {
                        player->rideTo(rx, ry, rz);
                    }
                }
                MinecartManager::Drop cartDrop;
                while (MinecartManager::popDrop(cartDrop)) spillStack(cartDrop.type, cartDrop.count, cartDrop.durability, cartDrop.x, cartDrop.y, cartDrop.z);
            }
            // Cerdos con silla: el cerdo va a lo suyo (en la Beta no se dirige); agacharse = bajarse; si muere o el jugador muere, te baja
            {
                float rx, ry, rz;
                if (AnimalManager::takeForcedDismount(rx, ry, rz)) {
                    player->teleportTo(rx, ry, rz);
                } else if (AnimalManager::isRiding()) {
                    if (player->isSneaking() || healthState.dead) {
                        if (AnimalManager::dismount(rx, ry, rz)) { player->setSneak(false); player->teleportTo(rx, ry, rz); }
                    } else if (AnimalManager::riderEye(rx, ry, rz)) {
                        player->rideTo(rx, ry, rz);
                    }
                }
            }
            // Botes: flotan; el jinete los empuja con las teclas (EntityBoat: motion += motionDelJinete * 0,2); agacharse = bajarse
            {
                float rx, ry, rz;
                float inX = 0.0f, inZ = 0.0f;
                if (BoatManager::isRiding() && !uiBlocking) {
                    const float yr = player->yaw * (3.14159265f / 180.0f);
                    float fi = 0.0f, si = 0.0f;
                    { const u32 gh = Controls::gameHeld(keys_held);
                    if (gh & KEY_UP)    fi += 1.0f;
                    if (gh & KEY_DOWN)  fi -= 1.0f;
                    if (gh & KEY_LEFT)  si -= 1.0f;
                    if (gh & KEY_RIGHT) si += 1.0f; }
                    const float mg = sqrtf(fi * fi + si * si);
                    if (mg > 1.0f) { fi /= mg; si /= mg; }
                    inX = -sinf(yr) * fi + cosf(yr) * si;
                    inZ = -cosf(yr) * fi - sinf(yr) * si;
                }
                BoatManager::tick(*chunkManager, inX, inZ);
                PaintingManager::tick(*chunkManager);
                { PaintingManager::Drop pd; while (PaintingManager::popDrop(pd)) spillStack(pd.type, pd.count, pd.durability, pd.x, pd.y, pd.z); }
                if (BoatManager::takeForcedDismount(rx, ry, rz)) {
                    player->teleportTo(rx, ry, rz);
                } else if (BoatManager::isRiding()) {
                    if (player->isSneaking()) {
                        if (BoatManager::dismount(rx, ry, rz)) { player->setSneak(false); player->teleportTo(rx, ry, rz); }
                    } else if (BoatManager::riderEye(rx, ry, rz)) {
                        player->rideTo(rx, ry, rz);
                    }
                }
                BoatManager::Drop boatDrop;
                while (BoatManager::popDrop(boatDrop)) spillStack(boatDrop.type, boatDrop.count, boatDrop.durability, boatDrop.x, boatDrop.y, boatDrop.z);
            }
            {
                SheepManager::Drop sheepDrop;
                while (SheepManager::popDrop(sheepDrop)) {
                    for (int n = 0; n < sheepDrop.count; n++) {
                        for (int i = 0; i < MAX_DROPPED_ITEMS; i++) {
                            if (droppedItems[i].isActive()) continue;
                            float vx = (float)(rand() % 200 - 100) / 1000.0f, vy = 0.2f, vz = (float)(rand() % 200 - 100) / 1000.0f, yOff = 0.3f;
                            if (sheepDrop.shear) {   // al esquilar: sale a 1 de altura y con un poco más de empuje
                                yOff = 1.0f;
                                vy += (float)(rand() % 50) / 1000.0f;
                                vx += (float)((rand() % 1000) - (rand() % 1000)) / 10000.0f;
                                vz += (float)((rand() % 1000) - (rand() % 1000)) / 10000.0f;
                            }
                            droppedItems[i].spawn(sheepDrop.x, sheepDrop.y + yOff, sheepDrop.z, sheepDrop.type, true, vx, vy, vz, -1);
                            break;
                        }
                    }
                }
            }

            // Entity.doBlockCollisions -> BlockPortal.onEntityCollidedWithBlock -> Entity.setInPortal
            if (chunkManager->portalInBox(player->x - 0.299f, player->y - 1.619f, player->z - 0.299f,
                                          player->x + 0.299f, player->y + 0.179f, player->z + 0.299f)) {
                if (timeUntilPortal > 0) timeUntilPortal = 10; else inPortalFlag = true;
            }

	    {
                HandAnimations::TickInput hin;
                bool emptySlot = inventory->isSlotEmpty(selectedSlot);
                hin.heldType = emptySlot ? BlockType::AIR : inventory->getSlotType(selectedSlot);
                hin.heldDurability = emptySlot ? 0 : inventory->getSlotDurability(selectedSlot);
                hin.heldSlot = selectedSlot;
                hin.attackHeld = !uiBlocking && ((keys_held & breakKey) != 0);
                hin.targetingBlock = hit.hit;
                hin.velX = player->getVelX();
                hin.velY = player->getVelY();
                hin.velZ = player->getVelZ();
                hin.movedX = player->getTickMoveX();
                hin.movedZ = player->getTickMoveZ();
                hin.onGround = player->isOnGround();
                hin.sneaking = player->isSneaking();
                HandAnimations::tick(hin);
                {   // Montado (bote, vagoneta o cerdo) no camina: la Beta mueve al jinete con updateRiderPosition despues de onUpdate, asi que su
                    // limbSwing no ve desplazamiento y brazos y piernas quedan quietos en la pose de sentado (ModelBiped.isRiding)
                    const bool ridingNow = BoatManager::isRiding() || MinecartManager::isRiding() || AnimalManager::isRiding();
                    SteveModel::tick(player->yaw, ridingNow ? 0.0f : player->getTickMoveX(), ridingNow ? 0.0f : player->getTickMoveZ(),
                                     HandAnimations::getSwingProgress(1.0f), player->isSneaking());
                }
            }

            Furnace::tickAll(*chunkManager);
            if (furnaceOpen && openFurnace && openFurnace->dirty) {
                furnaceLitOpen = openFurnace->burnTime > 0;
                CraftingScreen::showFurnaceTop(furnaceLitOpen, openFurnace, furnaceSelectedSlot);
            } else if (furnaceOpen && openFurnace && CraftingScreen::furnaceAnimStale(openFurnace)) {
                CraftingScreen::updateFurnaceAnim(openFurnace);   // solo llama y flecha, sin redibujar la pantalla
            }

            for (int i = 0; i < MAX_DROPPED_ITEMS; i++) {
                droppedItems[i].update(*chunkManager);

                // con el inventario lleno el ítem se queda en el suelo, en vez de perderse
                if (!healthState.dead && droppedItems[i].canBePickedUp(player->x, player->y - 1.62f, player->z) &&
                    inventory->canAccept(droppedItems[i].getType())) {
                    droppedItems[i].startPickupAnimation(player->x, player->y - 1.0f, player->z);
                }

                if (droppedItems[i].consumePickupComplete()) {
                    BlockType pickedType = droppedItems[i].getType();
                    int pickedCount = droppedItems[i].getCount();

                    bool added;
                    if (hasDurability(pickedType)) {
                        added = inventory->addToolWithDurability(pickedType, droppedItems[i].getDurability());
                    } else {
                        bool usedNewSlot;
                        int remaining = inventory->addItem(pickedType, pickedCount, usedNewSlot);
                        added = (remaining < pickedCount);
                        if (remaining > 0) { // no entró toda la pila: el resto vuelve al suelo, junto al jugador
                            droppedItems[i].spawn(player->x, player->y - 1.0f, player->z, pickedType, true,
                                                  0.0f, 0.0f, 0.0f, -1, remaining);
                        }
                    }

                    if (added) {
                        if (inventoryOpen) InventoryScreen::show(*inventory, selectedSlot, inventorySelectedSlot);
                        else if (craftingOpen) CraftingScreen::show(*inventory, craftingSelectedRecipe, craftingUsingTable);
                        else if (consoleVisible) { consoleDemoInit(); } else { HotbarIcons::render(*inventory, selectedSlot); }
                    }
                }
            }

	    // Los ítems sueltos que quedan quietos cerca de otro igual se juntan en una sola pila
            for (int i = 0; i < MAX_DROPPED_ITEMS; i++) {
                if (!droppedItems[i].isActive()) continue;
                for (int j = i + 1; j < MAX_DROPPED_ITEMS; j++) {
                    if (droppedItems[i].canMergeWith(droppedItems[j])) droppedItems[i].absorb(droppedItems[j]);
                }
            }

	    PendingDrop pd;
            while (PendingDrops::pop(pd)) {
                for (int i = 0; i < MAX_DROPPED_ITEMS; i++) {
                    if (!droppedItems[i].isActive()) {
                        droppedItems[i].spawn(pd.x, pd.y, pd.z, pd.type);
                        break;
                    }
                }
            }

            if (breakHeld && hit.hit) {
                bool sameTarget = (miningChunk == hit.chunk &&
                                      miningChunkX == hit.chunk->getChunkX() &&
                                      miningChunkZ == hit.chunk->getChunkZ() &&
                                      miningXi == hit.blockXi &&
                                      miningYi == hit.blockYi &&
                                      miningZi == hit.blockZi);

                // PlayerControllerSP.sendBlockRemoving: tras romper un bloque hay una pausa de 5 ticks; al cambiar de bloque solo se fija el nuevo
                // objetivo (sin sumar daño ese tick); con el mismo bloque se suma la fuerza de este tick. clickBlock: si un golpe alcanza (fuerza >= 1:
                // plantas, antorchas...) el bloque se rompe en el acto, sin pausa.
                bool breakNow = false;
                bool clickInstant = false;
                if (miningHitWait > 0) {
                    --miningHitWait;
                } else if (!sameTarget) {
                    miningChunk = hit.chunk;
                    miningChunkX = hit.chunk->getChunkX();
                    miningChunkZ = hit.chunk->getChunkZ();
                    miningXi = hit.blockXi;
                    miningYi = hit.blockYi;
                    miningZi = hit.blockZi;
                    miningDamage = 0.0f;

                    BlockType targetType = hit.chunk->getBlockType(hit.blockXi, hit.blockYi, hit.blockZi);
                    BlockType equippedType = inventory->isSlotEmpty(selectedSlot) ? BlockType::AIR : inventory->getSlotType(selectedSlot);
                    if (targetType == BlockType::NOTE_BLOCK) {   // BlockNote.onBlockClicked: un golpe la hace sonar
                        NoteBlocks::trigger(*chunkManager, hit.chunk->getChunkX() * WorldConfig::SIZE_X + hit.blockXi, hit.blockYi, hit.chunk->getChunkZ() * WorldConfig::SIZE_Z + hit.blockZi);
                    }
                    if (getBlockStrength(targetType, equippedType, player->isOnGround(), hudHeadInWater) >= 1.0f) {
                        breakNow = true;
                        clickInstant = true;
                    }
                } else {
                    BlockType targetType = miningChunk->getBlockType(miningXi, miningYi, miningZi);
                    BlockType equippedType = inventory->isSlotEmpty(selectedSlot) ? BlockType::AIR : inventory->getSlotType(selectedSlot);
                    if (targetType != BlockType::AIR) {
                        miningDamage += getBlockStrength(targetType, equippedType, player->isOnGround(), hudHeadInWater);
                        {   // addBlockHitEffects: una particula sobre la cara apuntada cada 4 pasos
                            static int s_hitFx = 0;
                            if (++s_hitFx >= 4) {
                                s_hitFx = 0;
                                int hdx, hdy, hdz;
                                getFaceOffset(hit.face, hdx, hdy, hdz);
                                Particles::blockHit(targetType, miningChunk->gridToWorldX(miningXi), miningChunk->gridToWorldY(miningYi), miningChunk->gridToWorldZ(miningZi), hdx, hdy, hdz);
                            }
                        }
                        if (miningDamage >= 1.0f) breakNow = true;
                    }
                }

                if (breakNow) {
                    BlockType brokenType = miningChunk->getBlockType(miningXi, miningYi, miningZi);
                    bool tntIgnite = false;
                    const int brokenGx = miningChunk->getChunkX() * WorldConfig::SIZE_X + miningXi;
                    const int brokenGz = miningChunk->getChunkZ() * WorldConfig::SIZE_Z + miningZi;
                    Sound::blockBreak(brokenType, miningChunk->gridToWorldX(miningXi), miningChunk->gridToWorldY(miningYi), miningChunk->gridToWorldZ(miningZi));
                    Particles::blockBreak(brokenType, miningChunk->gridToWorldX(miningXi), miningChunk->gridToWorldY(miningYi), miningChunk->gridToWorldZ(miningZi));
                    if (brokenType == BlockType::NOTE_BLOCK) NoteBlocks::remove(brokenGx, miningYi, brokenGz);
                    if (brokenType == BlockType::JUKEBOX) {   // BlockJukeBox.onBlockRemoval: el disco sale suelto
                        const BlockType rec = Partial::jukeboxEject(*chunkManager, brokenGx, miningYi, brokenGz);
                        if (rec != BlockType::AIR) spillStack(rec, 1, -1, miningChunk->gridToWorldX(miningXi), miningChunk->gridToWorldY(miningYi) + 0.5f, miningChunk->gridToWorldZ(miningZi));
                    }
                    if (isDispenserBlock(brokenType)) {   // BlockDispenser.onBlockRemoval: el contenido sale como objetos sueltos
                        ChestData* dd = ChestStore::find(brokenGx, miningYi, brokenGz);
                        if (dd) {
                            const float dbx = miningChunk->gridToWorldX(miningXi), dby = miningChunk->gridToWorldY(miningYi), dbz = miningChunk->gridToWorldZ(miningZi);
                            for (int s = 0; s < Dispenser::SLOTS; s++) {
                                if (dd->slot[s].count == 0) continue;
                                spillStack((BlockType)dd->slot[s].type, dd->slot[s].count, dd->slot[s].durability, dbx, dby, dbz);
                            }
                            ChestStore::remove(brokenGx, miningYi, brokenGz);
                        }
                    }
		    if (brokenType == BlockType::CHEST) {
                        int cgx = miningChunk->getChunkX() * WorldConfig::SIZE_X + miningXi;
                        int cgz = miningChunk->getChunkZ() * WorldConfig::SIZE_Z + miningZi;
                        ChestData* cd = ChestStore::find(cgx, miningYi, cgz);
                        if (cd) {
                            float cbx = miningChunk->gridToWorldX(miningXi);
                            float cby = miningChunk->gridToWorldY(miningYi);
                            float cbz = miningChunk->gridToWorldZ(miningZi);
                            for (int s = 0; s < ChestStore::SLOTS; s++) {
                                if (cd->slot[s].count == 0) continue;
                                spillStack((BlockType)cd->slot[s].type, cd->slot[s].count, cd->slot[s].durability, cbx, cby, cbz);
                            }
                            ChestStore::remove(cgx, miningYi, cgz);
                        }
                    }
		    if (isFurnaceBlock(brokenType)) {
                        int fgx = miningChunk->getChunkX() * WorldConfig::SIZE_X + miningXi;
                        int fgz = miningChunk->getChunkZ() * WorldConfig::SIZE_Z + miningZi;
                        FurnaceData* fd = Furnace::find(fgx, miningYi, fgz);
                        if (fd) {
                            float bx = miningChunk->gridToWorldX(miningXi);
                            float by = miningChunk->gridToWorldY(miningYi);
                            float bz = miningChunk->gridToWorldZ(miningZi);

                            for (int s = 0; s < 3; s++) {
                                FurnaceSlot& fs = fd->slot[s];
                                if (fs.empty || fs.count <= 0) continue;

                                bool tool = hasDurability(fs.type);
                                int left = fs.count;

                                // Primero como ítems sueltos (la lista es de 16, así que no siempre alcanza)
                                while (left > 0) {
                                    int freeIdx = -1;
                                    for (int i = 0; i < MAX_DROPPED_ITEMS; i++) {
                                        if (!droppedItems[i].isActive()) { freeIdx = i; break; }
                                    }
                                    if (freeIdx == -1) break;

                                    int chunk = tool ? 1 : (10 + rand() % 21); // pilas de 10 a 30, como el original
                                    if (chunk > left) chunk = left;
                                    if (tool) droppedItems[freeIdx].spawn(bx, by, bz, fs.type, true, 0.0f, 0.1f, 0.0f, fs.durability);
                                    else      droppedItems[freeIdx].spawn(bx, by, bz, fs.type, false, 0.0f, 0.0f, 0.0f, -1, chunk);
                                    left -= chunk;
                                }

                                // Lo que no entró va directo al inventario; si tampoco hay lugar, se pierde
                                if (tool) {
                                    while (left > 0 && inventory->addToolWithDurability(fs.type, fs.durability)) left--;
                                } else if (left > 0) {
                                    bool usedNewSlot;
                                    left = inventory->addItem(fs.type, left, usedNewSlot);
                                }
                            }

                            Furnace::remove(fgx, miningYi, fgz);
                        }
                    }

                    bool equippedIsPickaxe = !inventory->isSlotEmpty(selectedSlot) &&
                                               isPickaxe(inventory->getSlotType(selectedSlot));

                    bool shouldDrop = (brokenType != BlockType::OAK_LEAVES &&
                                        brokenType != BlockType::BIRCH_LEAVES &&
                                        brokenType != BlockType::SPRUCE_LEAVES &&
                                        brokenType != BlockType::TALLGRASS &&
                                        brokenType != BlockType::SNOW_LAYER &&
                                        brokenType != BlockType::DEAD_BUSH &&
                                        brokenType != BlockType::GLASS);

                    if (brokenType == BlockType::STONE && !equippedIsPickaxe) {
                        shouldDrop = false;
                    }

                    BlockType equippedNow = inventory->isSlotEmpty(selectedSlot) ? BlockType::AIR : inventory->getSlotType(selectedSlot);
                    if (requiresPickaxe(brokenType) && pickaxeTier(equippedNow) < minPickaxeTier(brokenType)) {
                        shouldDrop = false;
                    }
                    if (brokenType == BlockType::MOB_SPAWNER || brokenType == BlockType::BOOKSHELF || brokenType == BlockType::ICE) shouldDrop = false;   // quantityDropped = 0 (estanteria, hielo)
                    if (brokenType == BlockType::SNOW_BLOCK && !canHarvestBlock(brokenType, equippedNow)) shouldDrop = false;   // el bloque de nieve solo suelta con pala   // BlockMobSpawner.quantityDropped = 0
                    if (brokenType == BlockType::TNT && equippedNow == BlockType::FLINT_AND_STEEL) { shouldDrop = false; tntIgnite = true; }   // BlockTNT.onBlockClicked: con el mechero el golpe lo enciende
                    // la mitad de arriba de la puerta y el pie de la cama no sueltan nada: lo suelta la otra mitad
                    if (isPartialBlock(brokenType) && !Partial::dropsOnMining(brokenType, miningChunk->getLiquidMeta(miningXi, miningYi, miningZi))) shouldDrop = false;
                    if (isLeavesBlock(brokenType) && equippedNow == BlockType::SHEARS) shouldDrop = true; // BlockLeaves.harvestBlock: con tijeras suelta la hoja
                    if (brokenType == BlockType::SNOW_LAYER && canHarvestBlock(brokenType, equippedNow)) shouldDrop = true;   // BlockSnow.harvestBlock: con pala suelta una bola de nieve

                    if (brokenType == BlockType::CROPS) {   // BlockCrops.dropBlockAsItemWithChance: el trigo (maduro) y hasta 3 semillas
                        int cropWheat, cropSeeds;
                        Partial::cropDrops(miningChunk->getLiquidMeta(miningXi, miningYi, miningZi), cropWheat, cropSeeds);
                        for (int q = 0; q < cropWheat + cropSeeds; q++) {
                            for (int i = 0; i < MAX_DROPPED_ITEMS; i++) {
                                if (!droppedItems[i].isActive()) {
                                    droppedItems[i].spawn(miningChunk->gridToWorldX(miningXi), miningChunk->gridToWorldY(miningYi),
                                                          miningChunk->gridToWorldZ(miningZi), q < cropWheat ? BlockType::WHEAT : BlockType::SEEDS);
                                    break;
                                }
                            }
                        }
                    }
                    BlockType dropType = getDropType(brokenType);
                    if (brokenType == BlockType::TALLGRASS && (rand() % 8) == 0) { shouldDrop = true; dropType = BlockType::SEEDS; }   // BlockTallGrass.idDropped: 1 de 8 suelta semillas
                    if (brokenType == BlockType::SNOW_LAYER) dropType = BlockType::SNOWBALL;
                    if (isLeavesBlock(brokenType)) {   // BlockLeaves.quantityDropped: 1 de 20 suelta su retoño (con tijeras, la hoja)
                        if (equippedNow == BlockType::SHEARS) dropType = brokenType;
                        else if (rand() % 20 == 0) { shouldDrop = true; dropType = Trees::saplingOfLeaves(brokenType); }
                        else shouldDrop = false;
                    }
                    if (brokenType == BlockType::GRAVEL && (rand() % 10) == 0) dropType = BlockType::FLINT; // BlockGravel.idDropped
                    if (brokenType == BlockType::STONE) {
                        dropType = BlockType::COBBLESTONE;
                    }

                    if (brokenType == BlockType::GLOWSTONE && shouldDrop) {
                        int dustCount = 2 + (rand() % 3); // BlockGlowStone.quantityDropped: 2 + nextInt(3)
                        for (int c = 0; c < dustCount; c++) {
                            for (int i = 0; i < MAX_DROPPED_ITEMS; i++) {
                                if (!droppedItems[i].isActive()) {
                                    droppedItems[i].spawn(miningChunk->gridToWorldX(miningXi), miningChunk->gridToWorldY(miningYi),
                                                          miningChunk->gridToWorldZ(miningZi), BlockType::GLOWSTONE_DUST);
                                    break;
                                }
                            }
                        }
                    } else if ((brokenType == BlockType::REDSTONE_ORE || brokenType == BlockType::REDSTONE_ORE_LIT || brokenType == BlockType::LAPIS_ORE || brokenType == BlockType::CLAY || brokenType == BlockType::SNOW_BLOCK) && shouldDrop) {
                        // BlockRedstoneOre.quantityDropped: 4 + nextInt(2); BlockOreLapis: 4 + nextInt(5); BlockClay: 4
                        int oreCount = (brokenType == BlockType::CLAY || brokenType == BlockType::SNOW_BLOCK) ? 4 : (brokenType == BlockType::REDSTONE_ORE || brokenType == BlockType::REDSTONE_ORE_LIT) ? 4 + (rand() % 2) : 4 + (rand() % 5);
                        for (int c = 0; c < oreCount; c++) {
                            for (int i = 0; i < MAX_DROPPED_ITEMS; i++) {
                                if (!droppedItems[i].isActive()) {
                                    droppedItems[i].spawn(miningChunk->gridToWorldX(miningXi), miningChunk->gridToWorldY(miningYi),
                                                          miningChunk->gridToWorldZ(miningZi), dropType);
                                    break;
                                }
                            }
                        }
                    } else if (brokenType == BlockType::COAL_ORE && equippedIsPickaxe) {
                        int coalCount = 1 + (rand() % 3);
                        for (int c = 0; c < coalCount; c++) {
                            for (int i = 0; i < MAX_DROPPED_ITEMS; i++) {
                                if (!droppedItems[i].isActive()) {
                                    float dropX = miningChunk->gridToWorldX(miningXi);
                                    float dropY = miningChunk->gridToWorldY(miningYi);
                                    float dropZ = miningChunk->gridToWorldZ(miningZi);
                                    droppedItems[i].spawn(dropX, dropY, dropZ, BlockType::COAL);
                                    break;
                                }
                            }
                        }
                    } else if (brokenType != BlockType::COAL_ORE && shouldDrop) {
                        float dropX = miningChunk->gridToWorldX(miningXi);
                        float dropY = miningChunk->gridToWorldY(miningYi);
                        float dropZ = miningChunk->gridToWorldZ(miningZi);
                        const int dropQty = isDoubleSlabBlock(brokenType) ? 2 : 1;   // BlockStep.quantityDropped: la losa doble suelta 2
                        for (int q = 0; q < dropQty; q++) {
                            for (int i = 0; i < MAX_DROPPED_ITEMS; i++) {
                                if (!droppedItems[i].isActive()) {
                                    droppedItems[i].spawn(dropX, dropY, dropZ, dropType);
                                    break;
                                }
                            }
                        }
                    }

                    if (!inventory->isSlotEmpty(selectedSlot) && isTool(inventory->getSlotType(selectedSlot)) && !isHoeTool(inventory->getSlotType(selectedSlot)) &&
                        inventory->getSlotType(selectedSlot) != BlockType::FLINT_AND_STEEL && // el mechero no se gasta al picar
                        (inventory->getSlotType(selectedSlot) != BlockType::SHEARS || isLeavesBlock(brokenType))) { // las tijeras solo al romper hojas
                        inventory->reduceDurability(selectedSlot, isSword(inventory->getSlotType(selectedSlot)) ? 2 : 1);
                        if (consoleVisible) { consoleDemoInit(); } else { HotbarIcons::render(*inventory, selectedSlot); }
                    }

                    miningChunk->breakBlockAt(miningXi, miningYi, miningZi, chunkManager);
                    if (brokenType == BlockType::ICE && miningYi > 0) {   // BlockIce.harvestBlock: si debajo hay suelo o liquido, queda agua
                        const BlockType belowIce = miningChunk->getBlockType(miningXi, miningYi - 1, miningZi);
                        if (isMaterialSolid(belowIce) || isLiquidBlock(belowIce)) chunkManager->setBlockGlobal(brokenGx, miningYi, brokenGz, BlockType::WATER);
                    }
                    if (tntIgnite) TntManager::primeCell(brokenGx, miningYi, brokenGz);
                    miningChunk = nullptr;
                    miningDamage = 0.0f;
                    miningHitWait = clickInstant ? 0 : 5;
                    
                }
            }
        }

        if (saveRequested && !healthState.dead) {
            saveRequested = false;
            Prof::flushSpikes(); // muerto no se guarda: el mundo debe poder volver a antes de morir
            chunkManager->saveModifiedChunks();

            static WorldMetadata meta;
            meta.seed = chunkManager->getSeed();
            float savX = player->x, savY = player->y, savZ = player->z;
            if (sleeping) computeWakeSpot(savX, savY, savZ);   // EntityPlayer.readEntityFromNBT: al cargar un mundo guardado durmiendo, te levantas junto a la cama
            meta.playerX = savX;
            meta.playerY = savY;
            meta.playerZ = savZ;
            meta.playerYaw = player->yaw;
            meta.playerPitch = player->pitch;
            meta.gameTimeTicks = gameTimeTicks;
            meta.spawnX = worldSpawnX; meta.spawnZ = worldSpawnZ; meta.spawnSet = worldSpawnSet ? 1 : 0;
            meta.terrainGen = WorldConfig::terrainGen();

            for (int i = 0; i < Inventory::TOTAL_SIZE; i++) {
                if (inventory->isSlotEmpty(i)) {
                    meta.hotbarType[i] = 0;
                    meta.hotbarCount[i] = 0;
                    meta.hotbarDurability[i] = 0; // nuevo
                } else {
                    meta.hotbarType[i] = (int)inventory->getSlotType(i);
                    meta.hotbarCount[i] = inventory->getSlotCount(i);
                    meta.hotbarDurability[i] = inventory->getSlotDurability(i); // nuevo
                }
            }
            for (int i = 0; i < Inventory::ARMOR_SLOTS; i++) {
                const int a = Inventory::ARMOR_BASE + i;
                meta.armorType[i] = inventory->isSlotEmpty(a) ? 0 : (int)inventory->getSlotType(a);
                meta.armorDurability[i] = inventory->isSlotEmpty(a) ? 0 : inventory->getSlotDurability(a);
            }

            char metaPath[80];
            WorldSave::getBaseFilePath("meta.dat", metaPath);
            WorldSave::saveMetadata(meta, metaPath);

            { // dimensión actual: dim.dat vive en la carpeta del mundo, no en la de la dimensión
                char dimPath[80];
                WorldSave::getBaseFilePath("dim.dat", dimPath);
                FILE* dimFile = fopen(dimPath, "wb");
                if (dimFile) { int d = currentDimension; fwrite(&d, sizeof(int), 1, dimFile); fclose(dimFile); }
            }

            { // cama de reaparicion: bed.dat (flag, gx, gy, gz), también en la carpeta del mundo
                char bedPath[80];
                WorldSave::getBaseFilePath("bed.dat", bedPath);
                FILE* bedFile = fopen(bedPath, "wb");
                if (bedFile) { int v[4] = { bedSpawnSet ? 1 : 0, bedSpawnGx, bedSpawnGy, bedSpawnGz }; fwrite(v, sizeof(int), 4, bedFile); fclose(bedFile); }
            }

            { // vida del jugador: health.dat, también en la carpeta del mundo
                char healthPath[80];
                WorldSave::getBaseFilePath("health.dat", healthPath);
                FILE* healthFile = fopen(healthPath, "wb");
                if (healthFile) { int h = healthState.health; fwrite(&h, sizeof(int), 1, healthFile); fclose(healthFile); }
            }

            char furnPath[80];
            WorldSave::getActiveFilePath("furnaces.dat", furnPath);
            Furnace::saveToFile(furnPath);
            char chestPath[80];
            WorldSave::getActiveFilePath("chests.dat", chestPath);
            ChestStore::saveToFile(chestPath);
{ char cartPath[80]; WorldSave::getActiveFilePath("carts.dat", cartPath); MinecartManager::saveToFile(cartPath); }
{ char boatPath[80]; WorldSave::getActiveFilePath("boats.dat", boatPath); BoatManager::saveToFile(boatPath); Maps::flush(); Weather::save(); }
{ char paintPath[80]; WorldSave::getActiveFilePath("paintings.dat", paintPath); PaintingManager::saveToFile(paintPath); }
{ char signPath[80]; WorldSave::getActiveFilePath("signs.dat", signPath); SignManager::saveToFile(signPath); }
{ char wolfPath[80]; WorldSave::getActiveFilePath("wolves.dat", wolfPath); HostileManager::saveWolves(wolfPath); }
{ char notePath[80]; WorldSave::getActiveFilePath("notes.dat", notePath); NoteBlocks::saveToFile(notePath); }

            WorldSave::commitSession();
        }
        saveRequested = false;
    }

    return 0;
}