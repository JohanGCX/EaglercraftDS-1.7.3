#include "Chunk.h"
#include "Options.h"
#include "SignManager.h"
#include <string.h>
#include "Noise.h"
#include "Texture.h"
#include "PerlinNoise.h"
#include <nds.h>
#include <stdio.h>
#include <math.h>
#include "ChunkManager.h"
#include "Biome.h"
#include "PendingDrops.h"
#include "DisplayListBuilder.h"
#include "WorldSave.h"
#include "Profiler.h"
#include "Torch.h"
#include "Liquid.h"
#include "Lakes.h"
#include "FireModel.h"
#include "NetherGen.h"
#include "Chest.h"
#include "ChestStore.h"
#include "PartialBlocks.h"
#include "Redstone.h"
#include "Pistons.h"
#include "Trees.h"
#include "ColorMaps.h"
#include <stdlib.h>
#include <new>
#define V(v) floattov16(v)

// true mientras corre la fase de estructuras (árboles, minerales, cactus).
// Si esas escrituras caen en un chunk vecino ya completo, no se recalcula
// nada por bloque: el vecino recibe un único refresco completo, incremental.
static bool s_structureGenActive = false;

// Presupuesto de TIEMPO para reconstruir bandas, compartido por todos
// los chunks del frame (siempre se permite al menos una banda).
static u32 s_rebuildFrameStart = 0;
static bool s_rebuildDidOne = false;
// Búfer de trabajo: cada banda se arma UNA vez aquí y se copia al tamaño justo (antes se armaba dos veces: una para contar y otra para escribir).
// Si una banda no cabe (6144 words = 24 KB; una banda densa de hojas ronda 5000), se arma de nuevo directo en su búfer final.
static int s_dlAllocFailFrames = 0;   // tras un fallo de memoria, no se reintenta ninguna banda durante unos cuadros
static const u32 DL_SCRATCH_WORDS = 6144;
static u32 s_dlScratch[DL_SCRATCH_WORDS + 1];

static const u32 REBUILD_BUDGET_TICKS = 33514 * 4; // ~4 ms
static u32 s_rebuildBudget = REBUILD_BUDGET_TICKS;   // el presupuesto de este cuadro (Governor lo recorta si el cuadro anterior se pasó)

// glCallList de libnds hace DC_FlushRange en CADA llamada; las listas se vuelcan una sola vez al armarse (flushList) y aqui solo se manda el DMA
static inline void flushList(const u32* list) { DC_FlushRange((const void*)list, (list[0] + 1) * 4); }
static inline void callListFast(const u32* list) {
    const u32 count = *list++;
    while ((DMA_CR(0) | DMA_CR(1) | DMA_CR(2) | DMA_CR(3)) & DMA_BUSY) {}
    DMA_SRC(0) = (uint32)list;
    DMA_DEST(0) = 0x4000400;
    DMA_CR(0) = DMA_FIFO | count;
    while (DMA_CR(0) & DMA_BUSY) {}
}
static int s_lodBuildsThisFrame = 0;

void Chunk::beginRebuildFrame() {
    s_lodBuildsThisFrame = 0;
    if (s_dlAllocFailFrames > 0) s_dlAllocFailFrames--;
    s_rebuildFrameStart = Prof::now();
    s_rebuildDidOne = false;
}

void Chunk::scheduleFullRefresh() {
    beginFullFaceCacheRecalc();
    initialSkyLightPending = true;
    initialSkyLightXi = 0;
    initialSkyLightZi = 0;
    skyInitPhase = 0; skyInitDir = 0;
    skyBaseReady = false;
}

using namespace WorldConfig;

void Chunk::getSaveFilenameFor(int chunkX, int chunkZ, unsigned int seed, char* outPath) {
    char base[64];
    WorldSave::getActiveWorldPath(base);
    sprintf(outPath, "%s/mc_chunk_%u_%d_%d.dat", base, seed, chunkX, chunkZ);
}

void Chunk::getSaveFilenameFor(int chunkX, int chunkZ, char* outPath) {
    char base[64];
    WorldSave::getActiveWorldPath(base);
    sprintf(outPath, "%s/mc_chunk_%d_%d.dat", base, chunkX, chunkZ);
}

void Chunk::getSaveFilename(char* outPath) const {
    getSaveFilenameFor(chunkX, chunkZ, chunkSeed, outPath);
}

static void resetTerrainCachesIfSeedChanged(unsigned int seed);

// Dimensión de los chunks que se crean y se cargan ahora (0 = superficie, -1 = Nether)
static int s_dimension = 0;
void Chunk::setDimension(int dim) { s_dimension = (dim == -1) ? -1 : 0; }
int Chunk::getDimension() { return s_dimension; }

Chunk::Chunk(int chunkX_, int chunkZ_, unsigned int seed) {
    chunkX = chunkX_;
    chunkZ = chunkZ_;
    chunkSeed = seed;
    resetTerrainCachesIfSeedChanged(seed);
    Climate::initIfSeedChanged(seed);

    Noise::setSeed(seed);
    PerlinNoise::init(seed);

    for (int xi = 0; xi < SIZE_X; xi++) {
        for (int yi = 0; yi < SIZE_Y; yi++) {
            for (int zi = 0; zi < SIZE_Z; zi++) {
                blocks[xi][yi][zi] = BlockType::AIR;
            }
        }
    }

    for (int xi = 0; xi < SIZE_X; xi++) {
        for (int zi = 0; zi < SIZE_Z; zi++) {
            columnMinY[xi][zi] = SIZE_Y;
            columnMaxY[xi][zi] = -1;
        }
    }

    genXi = 0;
    genZi = 0;
    generationComplete = false;

    structureGenPhase = 0;
    structureGenAttempt = 0;
    structureGenXi = 0;
    structureGenZi = 0;
    treesPlacedCount = 0;

    if (s_dimension == -1) { // el Nether no tiene cuevas, lagos, árboles ni menas del terreno de la superficie: va directo a lo suyo
        cavesDone = true;
        structureGenPhase = 10; // piedra luminosa, fuego y manantiales de lava antes de dar el chunk por terminado
    }
}

static int s_dlTotalBytes = 0; // memoria total ocupada por display lists de todos los chunks

int Chunk::getDisplayListBytes() { return s_dlTotalBytes; }

void Chunk::freeLodList() {
    if (dlLod) s_dlTotalBytes -= (int)((dlLod[0] + 1) * 4);
    delete[] dlLod;
    dlLod = nullptr;
    dlLodVerts = 0;
    lodDirty = true;
}

static const Chunk* s_lakeOwner = nullptr;   // chunk que tiene en Lakes::res un lago calculado y sin escribir

Chunk::~Chunk() {
    if (s_lakeOwner == this) s_lakeOwner = nullptr;
    freeLodList();
    for (int b = 0; b < DL_BAND_COUNT; b++) {
        if (dlBands[b]) s_dlTotalBytes -= (int)((dlBands[b][0] + 1) * 4);
        delete[] dlBands[b];
        if (dlBands2[b]) s_dlTotalBytes -= (int)((dlBands2[b][0] + 1) * 4);
        delete[] dlBands2[b];
    }
}

void Chunk::freeDisplayLists() {
    for (int b = 0; b < DL_BAND_COUNT; b++) {
        if (dlBands[b]) s_dlTotalBytes -= (int)((dlBands[b][0] + 1) * 4);
        delete[] dlBands[b];
        dlBands[b] = nullptr;
        dlBandVertCount[b] = 0;
        if (dlBands2[b]) s_dlTotalBytes -= (int)((dlBands2[b][0] + 1) * 4);
        delete[] dlBands2[b];
        dlBands2[b] = nullptr;
        dlBandVertCount2[b] = 0;
    }
    displayListEverBuilt = false;
    markDisplayListDirty();
    displayListRebuildBand = 0;
    displayListRebuildHalf = 0;
    displayListDirtyDuringRebuild = false;
}

static u32 s_heavyFrameStart = 0;
static bool s_heavyDidOne = false;
static const u32 HEAVY_BUDGET_TICKS = 33514 * 6; // ~6 ms
static u32 s_heavyBudget = HEAVY_BUDGET_TICKS;

void Chunk::setBudgetPercent(int percent) {
    s_heavyBudget = HEAVY_BUDGET_TICKS / 100 * (u32)percent;
    s_rebuildBudget = REBUILD_BUDGET_TICKS / 100 * (u32)percent;
}

void Chunk::beginHeavyFrame() {
    s_heavyFrameStart = Prof::now();
    s_heavyDidOne = false;
}

bool Chunk::heavyBudgetLeft() {
    return !s_heavyDidOne || (Prof::now() - s_heavyFrameStart) < s_heavyBudget * (u32)WorldConfig::budgetMul();   // modo dsi: el doble de tiempo por cuadro
}

static const u32 POLYFMT_CULLED    = POLY_ALPHA(31) | POLY_CULL_BACK | POLY_FOG;
static const u32 POLYFMT_BOTHSIDES = POLY_ALPHA(31) | POLY_CULL_NONE  | POLY_FOG;
static const u32 POLYFMT_WATER     = POLY_ALPHA(22) | POLY_CULL_NONE  | POLY_FOG | POLY_ID(22);   // agua translucida

static bool getPatchAt(float globalXi, float globalZi, BlockType& outType) {
    const float GRID_SIZE = 12.0f;

    int centerGridX = (int)floorf(globalXi / GRID_SIZE);
    int centerGridZ = (int)floorf(globalZi / GRID_SIZE);

    bool found = false;
    float bestDistSq = 0.0f;
    BlockType bestType = BlockType::AIR;

    for (int gx = centerGridX - 1; gx <= centerGridX + 1; gx++) {
        for (int gz = centerGridZ - 1; gz <= centerGridZ + 1; gz++) {
            unsigned int existsRoll = Noise::hashInt(gx, gz, 210) % 100;
            if (existsRoll >= 15) continue;

            unsigned int typeRoll = Noise::hashInt(gx, gz, 211) % 3;
            if (typeRoll == 0) continue; // aquí había charcos de agua: ahora el agua sale de los lagos (WorldGenLakes)
            BlockType candidateType = (typeRoll == 1) ? BlockType::SAND : BlockType::GRAVEL;

            unsigned int radiusRoll = Noise::hashInt(gx, gz, 212) % 5;
            float radius = 2.0f + (float)radiusRoll;

            unsigned int offsetXRoll = Noise::hashInt(gx, gz, 213) % (int)GRID_SIZE;
            unsigned int offsetZRoll = Noise::hashInt(gx, gz, 214) % (int)GRID_SIZE;
            float centerX = gx * GRID_SIZE + offsetXRoll;
            float centerZ = gz * GRID_SIZE + offsetZRoll;

            float dx = globalXi - centerX;
            float dz = globalZi - centerZ;
            float distSq = dx * dx + dz * dz;
            float dist = sqrtf(distSq);

            float edgeNoise = Noise::valueNoise2D(globalXi * 0.4f, globalZi * 0.4f);
            float radiusMultiplier = 0.7f + edgeNoise * 0.6f;
            float effectiveRadius = radius * radiusMultiplier;

            const float TRANSITION_BAND = 3.0f; // ancho de la franja de mezcla

            bool inside;
            if (dist <= effectiveRadius - TRANSITION_BAND) {
                inside = true; // bien adentro, siempre parche
            } else if (dist > effectiveRadius + TRANSITION_BAND) {
                inside = false; // bien afuera, nunca parche
            } else {
                float t = (dist - (effectiveRadius - TRANSITION_BAND)) / (TRANSITION_BAND * 2.0f);
                unsigned int ditherRoll = Noise::hashInt((int)(globalXi * 4), (int)(globalZi * 4), 220) % 100;

                // Suavizamos la curva de probabilidad (t elevado a una
                // potencia menor que 1) para que la mezcla se sienta
                // más pareja en toda la franja, no solo cerca del centro
                float softT = sqrtf(t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t));
                inside = (float)ditherRoll > softT * 100.0f;
            }

            if (inside) {
                if (!found || distSq < bestDistSq) {
                    found = true;
                    bestDistSq = distSq;
                    bestType = candidateType;
                }
            }
        }
    }

    if (found) {
        outType = bestType;
        return true;
    }
    return false;
}

void Chunk::continueDisplayListRebuild(const ChunkManager* manager, int* rebuildBandBudget) {
    if (s_dlAllocFailFrames > 0) return;
    int processed = 0;
    while (displayListRebuildBand < DL_BAND_COUNT && processed < DL_REBUILD_BANDS_PER_CALL &&
           (!rebuildBandBudget || *rebuildBandBudget > 0)) {
        // Presupuesto de TIEMPO compartido por todos los chunks del frame
        if (s_rebuildDidOne && Prof::now() - s_rebuildFrameStart > s_rebuildBudget) break;

        int band = displayListRebuildBand;
        const int half = displayListRebuildHalf;   // 0 = mitad x baja, 1 = mitad x alta (en la proxima vuelta del bucle / del cuadro)
        u32**  dst      = half ? dlBands2 : dlBands;
        int*   dstVerts = half ? dlBandVertCount2 : dlBandVertCount;
        const int xBegin = half ? SIZE_X / 2 : 0;
        const int xEnd   = half ? SIZE_X : SIZE_X / 2;
        if (half == 0) {
            if (!bandDirty.v[band]) { displayListRebuildBand++; continue; }   // banda limpia: no se toca
            bandDirty.v[band] = 0;   // se limpia antes de armarla: si algo la ensucia mientras tanto, queda marcada otra vez
        }

        // Banda vacía (aire puro): se resuelve gratis, sin gastar presupuesto
        int bandStart = band * DL_BAND_HEIGHT;
        int bandEnd = bandStart + DL_BAND_HEIGHT - 1;
        bool hasContent = false;
        for (int xi = xBegin; xi < xEnd && !hasContent; xi++) {
            for (int zi = 0; zi < SIZE_Z; zi++) {
                if (columnMaxY[xi][zi] >= bandStart && columnMinY[xi][zi] <= bandEnd) {
                    hasContent = true;
                    break;
                }
            }
        }

        if (dst[band]) s_dlTotalBytes -= (int)((dst[band][0] + 1) * 4);
        delete[] dst[band];
        dst[band] = nullptr;
        dstVerts[band] = 0;

        if (!hasContent) {
            if (half == 0) { displayListRebuildHalf = 1; } else { displayListRebuildHalf = 0; displayListRebuildBand++; }
            continue;
        }

        // Media banda con contenido: se construye ENTERA en esta llamada (atómica), en una sola pasada sobre el búfer de trabajo
        DisplayListBuilder scratch(s_dlScratch, DL_SCRATCH_WORDS);
        buildBandGeometry(scratch, band, manager, xBegin, xEnd);
        u32 bodyWords = scratch.finish();

        if (bodyWords > 0) {
            u32* buf = new (std::nothrow) u32[1 + bodyWords];
            if (!buf) {   // sin memoria: se reintenta la misma banda, pero no en cada cuadro ni desde cada chunk (cada intento arma la banda entera)
                bandDirty.v[band] = 1;
                displayListRebuildHalf = 0;
                s_rebuildDidOne = true;
                s_dlAllocFailFrames = 30;
                break;
            }
            if (!scratch.overflowed()) {
                memcpy(buf, s_dlScratch, (1 + bodyWords) * sizeof(u32)); // la palabra 0 ya trae el tamaño
                flushList(buf);
                dstVerts[band] = (int)scratch.getVertexCount();
            } else {
                DisplayListBuilder writer(buf);   // no cupo: se arma otra vez, directo en el búfer de tamaño justo
                buildBandGeometry(writer, band, manager, xBegin, xEnd);
                writer.finish();
                flushList(buf);
                dstVerts[band] = (int)writer.getVertexCount();
            }
            dst[band] = buf;
            s_dlTotalBytes += (int)((1 + bodyWords) * 4);
        }

        s_rebuildDidOne = true;
        if (half == 0) {
            displayListRebuildHalf = 1;   // la otra mitad en la proxima iteracion (si el presupuesto de tiempo no alcanza, queda para el proximo cuadro)
            continue;
        }
        displayListRebuildHalf = 0;
        displayListRebuildBand++;
        processed++;
        s_rebuildDidOne = true;
        if (rebuildBandBudget) (*rebuildBandBudget)--;
    }

    if (displayListRebuildBand >= DL_BAND_COUNT) {
        bool anyDirty = false;   // ¿algo ensució una banda mientras se recorría? se vuelve a pasar solo por esas
        for (int b = 0; b < DL_BAND_COUNT; b++) if (bandDirty.v[b]) { anyDirty = true; break; }
        displayListDirtyDuringRebuild = false;
        if (anyDirty) {
            displayListRebuildBand = 0;
        } else {
            displayListDirty = false;
            displayListRebuildBand = 0;
            displayListEverBuilt = true;
        }
    }
}

// Altura del terreno base de una columna (sin cuevas, menas ni árboles). Es la misma fórmula para generateColumn y para los
// lagos, con una caché: el ruido es caro y los lagos miran una región de 16x16 que comparten varios chunks.
// ---- LÍMITES DE VEGETACIÓN POR CHUNK DE 8 x 8 (lo que más pesa al dibujar y al generar; subilos o bajalos según la DS) ----
// El original tiene unos 2 árboles, 30 matas de pasto alto en llanura y unas pocas flores por chunk de este tamaño.
static const int MAX_TREES_PER_CHUNK = 3;      // árboles (con sus copas, lo más caro)
static const int MAX_TALLGRASS_PER_CHUNK = 10; // matas de pasto alto
static const int MAX_FLOWERS_PER_CHUNK = 4;    // flores (diente de león y rosa)

static const int TERRAIN_SEA_LEVEL = 38;

static int terrainHeightFrom(int gx, int gz, float biomeValue) {
    const int BASE_HEIGHT = 40;
    const int HEIGHT_RANGE = 10;
    const int OCEAN_BASE_HEIGHT = 30;
    const float NOISE_SCALE = 0.2f;
    const float OCEAN_THRESHOLD = 0.25f;
    const float TRANSITION_WIDTH = 0.08f;

    float perlinRaw = PerlinNoise::octaveNoise2D((float)gx * NOISE_SCALE, (float)gz * NOISE_SCALE);
    float noiseValue = perlinRaw;
    if (noiseValue > 1.0f) noiseValue = 1.0f;
    if (noiseValue < -1.0f) noiseValue = -1.0f;

    int base;
    if (biomeValue < OCEAN_THRESHOLD - TRANSITION_WIDTH) {
        base = OCEAN_BASE_HEIGHT;
    } else if (biomeValue > OCEAN_THRESHOLD + TRANSITION_WIDTH) {
        base = BASE_HEIGHT;
    } else {
        float t = (biomeValue - (OCEAN_THRESHOLD - TRANSITION_WIDTH)) / (TRANSITION_WIDTH * 2.0f);
        base = OCEAN_BASE_HEIGHT + (int)(t * (BASE_HEIGHT - OCEAN_BASE_HEIGHT));
    }
    return base + (int)(noiseValue * HEIGHT_RANGE);
}

static struct HeightCache {
    s32 tx[48 * 48];
    s32 tz[48 * 48];
    u8 h[48 * 48];
    HeightCache() { for (int i = 0; i < 48 * 48; i++) { tx[i] = 0x7FFFFFFF; tz[i] = 0; h[i] = 0; } }
} s_heightCache;

static int terrainHeightCached(int gx, int gz, const float* biomeValue) {
    int i = (((gx % 48) + 48) % 48) * 48 + (((gz % 48) + 48) % 48);
    if (s_heightCache.tx[i] == gx && s_heightCache.tz[i] == gz) return s_heightCache.h[i];
    float bv = biomeValue ? *biomeValue
                          : Noise::biomeNoise2D((float)gx * BiomeConfig::BIOME_NOISE_SCALE, (float)gz * BiomeConfig::BIOME_NOISE_SCALE);
    int h = terrainHeightFrom(gx, gz, bv);
    s_heightCache.tx[i] = gx;
    s_heightCache.tz[i] = gz;
    s_heightCache.h[i] = (u8)(h < 0 ? 0 : (h > 255 ? 255 : h));
    return h;
}

// ---- Nether (terreno reducido: NetherGen.h) ----
static struct NetherColCache {
    s32 tx[40 * 40];
    s32 tz[40 * 40];
    NetherGen::Col c[40 * 40];
    NetherColCache() { reset(); }
    void reset() { for (int i = 0; i < 40 * 40; i++) { tx[i] = 0x7FFFFFFF; tz[i] = 0; c[i].floorTop = 0; c[i].ceilBottom = 0; c[i].pillar = 0; c[i].surface = 0; } }
} s_netherCache;

static const NetherGen::Col& netherColumnAt(int gx, int gz) {
    int i = (((gx % 40) + 40) % 40) * 40 + (((gz % 40) + 40) % 40);
    if (s_netherCache.tx[i] != gx || s_netherCache.tz[i] != gz) {
        s_netherCache.c[i] = NetherGen::computeColumn(gx, gz);
        s_netherCache.tx[i] = gx;
        s_netherCache.tz[i] = gz;
    }
    return s_netherCache.c[i];
}

static unsigned int s_cacheSeed = 0;
static bool s_cacheSeedKnown = false;
static void resetTerrainCachesIfSeedChanged(unsigned int seed) {
    if (s_cacheSeedKnown && s_cacheSeed == seed) return;
    s_cacheSeed = seed;
    s_cacheSeedKnown = true;
    s_netherCache.reset();
    NetherGen::resetLattice();
    for (int i = 0; i < 48 * 48; i++) { s_heightCache.tx[i] = 0x7FFFFFFF; s_heightCache.tz[i] = 0; s_heightCache.h[i] = 0; }
}

namespace {
    // Terreno base del Nether como función pura (para mirar a los chunks vecinos sin generarlos)
    struct NetherTerrain {
        BlockType type(int x, int y, int z) const { return NetherGen::baseType(netherColumnAt(x, z), x, y, z); }
        bool isAir(int x, int y, int z) const { return type(x, y, z) == BlockType::AIR; }
    };
}

void Chunk::generateNetherColumn(int xi, int zi) {
    const int gx = chunkX * SIZE_X + xi, gz = chunkZ * SIZE_Z + zi;
    const NetherGen::Col col = netherColumnAt(gx, gz);
    biomeColumn[xi][zi] = BiomeType::GENERIC;
    for (int yi = 0; yi < SIZE_Y; yi++) blocks[xi][yi][zi] = NetherGen::baseType(col, gx, yi, gz);
    highestGeneratedY = SIZE_Y - 1;

    // updateFaceCache fija también los límites de la columna; basta con las celdas que no están enterradas entre sólidos
    for (int yi = 0; yi < SIZE_Y; yi++) {
        BlockType t = blocks[xi][yi][zi];
        if (t == BlockType::AIR) continue;
        bool buried = yi > 0 && yi < SIZE_Y - 1 && isMaterialSolid(blocks[xi][yi - 1][zi]) && isMaterialSolid(blocks[xi][yi + 1][zi]);
        if (!buried) updateFaceCache(xi, yi, zi, nullptr);
    }
}

// Flores y pasto alto en grupos: WorldGenFlowers (64 intentos alrededor de un punto) y WorldGenTallGrass (128 intentos, partiendo de
// la superficie), con las cantidades de ChunkProviderGenerate.populate para un chunk de 16x16 repartidas en chunks de 8x8. Un grupo puede
// alcanzar este chunk desde los 3x3 chunks de alrededor: cada uno se evalúa con su propio azar, sin mirar los chunks vecinos (la altura
// de la superficie sale de la función pura del terreno) y solo se escribe lo que cae aquí, así los grupos no se cortan en la frontera.
// La Beta pone la nieve después de las plantas: en la taiga, una planta reemplaza a la capa de nieve de su celda.
static const int TALLGRASS_CLUSTER_PERCENT = 100; // 100 = todos los grupos del original; los topes de arriba son los que limitan

// Mazmorras (WorldGenDungeons): sala de cobblestone de 5x5 y 4 de alto por dentro con spawner al centro y cofres, solo bajo tierra (cuevas).
// El generador trabaja por chunks de 8x8 sin poder contar con los vecinos, asi que solo se hacen las que caben enteras en este chunk:
// la mas chica del original (radio 2, caja de 7x7) con centro en x, z de 3 o 4. Los cofres llevan el botin de WorldGenDungeons y el
// spawner es decorativo (aun no hay monstruos).
static const int DUNGEON_ATTEMPTS = 6;   // intentos por chunk de 8x8 (el original: 8 por chunk de 16x16 en un rango mucho mas ancho); subir para mas mazmorras
static const int DUNGEON_HEIGHT   = 3;   // var6 del original: el interior va de y a y+3 (4 de alto) y el techo en y+4

// WorldGenDungeons.pickCheckLootItem + el relleno del cofre: 8 intentos, cada uno elige una casilla al azar de las 27 (puede pisar la anterior)
static void fillDungeonChest(Lakes::JRandom& rng, int gx, int gy, int gz) {
    if (ChestStore::find(gx, gy, gz)) return;   // ya existe (mundo regenerado): no se vuelve a llenar
    if (ChestStore::usedCount() >= ChestStore::MAX_CHESTS - 16) return;   // deja sitio a los cofres del jugador (la tabla es de 64)
    ChestData* cd = ChestStore::findOrCreate(gx, gy, gz);
    if (!cd) return;
    for (int n = 0; n < 8; n++) {
        BlockType t = BlockType::AIR; int cnt = 1;
        switch (rng.nextInt(11)) {
            case 0:  t = BlockType::SADDLE; break;
            case 1:  t = BlockType::IRON_INGOT; cnt = rng.nextInt(4) + 1; break;
            case 2:  t = BlockType::BREAD; break;
            case 3:  t = BlockType::WHEAT; cnt = rng.nextInt(4) + 1; break;
            case 4:  t = BlockType::GUNPOWDER; cnt = rng.nextInt(4) + 1; break;
            case 5:  t = BlockType::STRING; cnt = rng.nextInt(4) + 1; break;
            case 6:  t = BlockType::BUCKET; break;
            case 7:  if (rng.nextInt(100) == 0) t = BlockType::GOLDEN_APPLE; break;
            case 8:  if (rng.nextInt(2) == 0) { t = BlockType::REDSTONE; cnt = rng.nextInt(4) + 1; } break;
            case 9:  if (rng.nextInt(10) == 0) t = (rng.nextInt(2) == 0) ? BlockType::RECORD_13 : BlockType::RECORD_CAT; break;
            case 10: t = BlockType::DYE_COCOA; break;
            default: break;
        }
        int slot = rng.nextInt(ChestStore::SLOTS);
        if (t == BlockType::AIR) continue;
        cd->slot[slot].type = (u8)t; cd->slot[slot].count = (u8)cnt; cd->slot[slot].durability = 0;
    }
}

void Chunk::generateDungeons(const ChunkManager* manager) {
    Lakes::JRandom rng(((long long)Noise::hashInt(chunkX, chunkZ, 7300) << 20) ^ (long long)Noise::hashInt(chunkZ + 29, chunkX - 29, 7400));
    const int R = 2;   // radio interior en x y z
    auto solidAt = [&](int xi, int yi, int zi) {
        BlockType t = blocks[xi][yi][zi];
        return t != BlockType::AIR && isLightOpaque(t) && !isLiquidBlock(t);
    };
    for (int attempt = 0; attempt < DUNGEON_ATTEMPTS; attempt++) {
        int cx = 3 + rng.nextInt(2);
        int cz = 3 + rng.nextInt(2);
        int cy = 5 + rng.nextInt(70);
        if (cy + DUNGEON_HEIGHT + 3 >= SIZE_Y) continue;

        // suelo y techo macizos en toda la caja; entre 1 y 5 aberturas al nivel del suelo en el perimetro
        bool ok = true;
        int openings = 0;
        for (int x = cx - R - 1; x <= cx + R + 1 && ok; x++)
            for (int y = cy - 1; y <= cy + DUNGEON_HEIGHT + 1 && ok; y++)
                for (int z = cz - R - 1; z <= cz + R + 1; z++) {
                    if ((y == cy - 1 || y == cy + DUNGEON_HEIGHT + 1) && !solidAt(x, y, z)) { ok = false; break; }
                    bool edge = (x == cx - R - 1 || x == cx + R + 1 || z == cz - R - 1 || z == cz + R + 1);
                    if (edge && y == cy && blocks[x][y][z] == BlockType::AIR && blocks[x][y + 1][z] == BlockType::AIR) openings++;
                }
        if (!ok || openings < 1 || openings > 5) continue;

        // sala: paredes y suelo de cobblestone (el suelo, 3/4 musgoso; el techo se queda con la piedra natural ya comprobada); lo que no es macizo en una pared queda de aire
        for (int x = cx - R - 1; x <= cx + R + 1; x++)
            for (int y = cy + DUNGEON_HEIGHT; y >= cy - 1; y--)
                for (int z = cz - R - 1; z <= cz + R + 1; z++) {
                    bool wall = (x == cx - R - 1 || x == cx + R + 1 || z == cz - R - 1 || z == cz + R + 1 || y == cy - 1);
                    if (!wall) { setBlockDirect(x, y, z, BlockType::AIR, manager); continue; }
                    if (y >= 1 && !solidAt(x, y - 1, z)) { setBlockDirect(x, y, z, BlockType::AIR, manager); continue; }
                    if (solidAt(x, y, z)) {
                        bool mossy = (y == cy - 1) && rng.nextInt(4) != 0;
                        setBlockDirect(x, y, z, mossy ? BlockType::MOSSY_COBBLESTONE : BlockType::COBBLESTONE, manager);
                    }
                }

        // cofres vacios: 2 rondas de 3 intentos, sobre aire con exactamente una pared maciza al lado
        for (int round = 0; round < 2; round++) {
            for (int t = 0; t < 3; t++) {
                int x = cx + rng.nextInt(2 * R + 1) - R;
                int z = cz + rng.nextInt(2 * R + 1) - R;
                if (blocks[x][cy][z] != BlockType::AIR) continue;
                int walls = 0;
                if (blocks[x - 1][cy][z] != BlockType::AIR && solidAt(x - 1, cy, z)) walls++;
                if (blocks[x + 1][cy][z] != BlockType::AIR && solidAt(x + 1, cy, z)) walls++;
                if (blocks[x][cy][z - 1] != BlockType::AIR && solidAt(x, cy, z - 1)) walls++;
                if (blocks[x][cy][z + 1] != BlockType::AIR && solidAt(x, cy, z + 1)) walls++;
                if (walls != 1) continue;
                setBlockDirect(x, cy, z, BlockType::CHEST, manager);
                fillDungeonChest(rng, chunkX * SIZE_X + x, cy, chunkZ * SIZE_Z + z);
                break;
            }
        }

        setBlockDirect(cx, cy, cz, BlockType::MOB_SPAWNER, manager);
        liquidMetaSet(cx, cy, cz, (int)(Noise::hashInt(chunkX * SIZE_X + cx, cy, chunkZ * SIZE_Z + cz) % 4u) == 0 ? 1 : ((int)(Noise::hashInt(chunkX * SIZE_X + cx, cy, chunkZ * SIZE_Z + cz) % 4u) == 3 ? 2 : 0));   // WorldGenDungeons.pickMobSpawner: 1/4 esqueleto (1), 2/4 zombi (0), 1/4 arana (2); va en la metadata (sale de la semilla)
    }
}

bool Chunk::generatePlantClusters(const ChunkManager* manager) {
    const int step = structureGenAttempt;
    if (step >= 9) return true;

    const int srcX = chunkX - 1 + (step % 3), srcZ = chunkZ - 1 + (step / 3);
    const int SEA_LEVEL = TERRAIN_SEA_LEVEL;

    // el bioma del chunk de origen decide las cantidades (en el original, el del centro del chunk)
    float centerT, centerH;
    Climate::raw(srcX * SIZE_X + SIZE_X / 2, srcZ * SIZE_Z + SIZE_Z / 2, centerT, centerH);
    BiomeType biome = Climate::biomeFor(centerT, centerH);
    int yellowPer16 = 0, grassPer16 = 0;   // ChunkProviderGenerate.populate, por chunk de 16 x 16
    switch (biome) {
        case BiomeType::FOREST:          yellowPer16 = 2; grassPer16 = 2;  break;
        case BiomeType::SEASONAL_FOREST: yellowPer16 = 4; grassPer16 = 2;  break;
        case BiomeType::RAINFOREST:      yellowPer16 = 0; grassPer16 = 10; break;
        case BiomeType::TAIGA:           yellowPer16 = 2; grassPer16 = 1;  break;
        case BiomeType::PLAINS:          yellowPer16 = 3; grassPer16 = 10; break;
        default: break;                                                      // el resto: nada
    }

    Lakes::JRandom rng(((long long)Noise::hashInt(srcX, srcZ, 7100) << 20) ^ (long long)Noise::hashInt(srcZ + 17, srcX - 17, 7200));
    auto per8 = [&](int n) { return n / 4 + (rng.nextInt(4) < (n % 4) ? 1 : 0); };

    // Un grupo solo puede tocar este chunk si su zona de intentos (x y z de -7 a +7 del origen) lo cruza; si no, se salta su azar de un golpe
    // (JRandom::skip hace lo mismo que gastarlo) y los chunks vecinos, que evalúan el mismo grupo, ven exactamente lo mismo
    auto reachesChunk = [&](int ox, int oz) {
        int x0 = chunkX * SIZE_X, z0 = chunkZ * SIZE_Z;
        return ox + 7 >= x0 && ox - 7 <= x0 + SIZE_X - 1 && oz + 7 >= z0 && oz - 7 <= z0 + SIZE_Z - 1;
    };

    auto place = [&](int gx, int gy, int gz, BlockType plant) {
        int xi = gx - chunkX * SIZE_X, zi = gz - chunkZ * SIZE_Z;
        if (xi < 0 || xi >= SIZE_X || zi < 0 || zi >= SIZE_Z || gy < 1 || gy >= SIZE_Y) return;
        const bool isGrass = (plant == BlockType::TALLGRASS);
        if (isGrass ? (tallGrassPlaced >= MAX_TALLGRASS_PER_CHUNK) : (flowersPlaced >= MAX_FLOWERS_PER_CHUNK)) return; // tope del chunk
        BlockType here = blocks[xi][gy][zi];
        if (here != BlockType::AIR && here != BlockType::SNOW_LAYER) return;
        if (!flowerSupportedAt(xi, gy, zi)) return;
        setBlockDirect(xi, gy, zi, plant, manager);
        if (isGrass) tallGrassPlaced++; else flowersPlaced++;
    };

    // WorldGenFlowers.generate
    auto flowerPatch = [&](int ox, int oy, int oz, BlockType plant) {
        if (!reachesChunk(ox, oz) || flowersPlaced >= MAX_FLOWERS_PER_CHUNK) { rng.skip(64 * 6); return; } // 6 llamadas de azar por intento
        for (int n = 0; n < 64; n++) {
            int a = rng.nextInt(8), b = rng.nextInt(8);
            int x = ox + a - b;
            int c = rng.nextInt(4), d = rng.nextInt(4);
            int y = oy + c - d;
            int e = rng.nextInt(8), f = rng.nextInt(8);
            int z = oz + e - f;
            place(x, y, z, plant);
        }
    };

    int yellow = per8(yellowPer16);
    for (int i = 0; i < yellow; i++) {
        int ox = srcX * SIZE_X + rng.nextInt(SIZE_X);
        int oy = rng.nextInt(128);
        int oz = srcZ * SIZE_Z + rng.nextInt(SIZE_Z);
        flowerPatch(ox, oy, oz, BlockType::DANDELION);
    }
    if (rng.nextInt(8) == 0) { // una de cada dos veces por chunk de 16x16
        int ox = srcX * SIZE_X + rng.nextInt(SIZE_X);
        int oy = rng.nextInt(128);
        int oz = srcZ * SIZE_Z + rng.nextInt(SIZE_Z);
        flowerPatch(ox, oy, oz, BlockType::ROSE);
    }

    // WorldGenTallGrass.generate: parte de un punto al azar y baja por el aire hasta el suelo; de ahí salen 128 intentos
    int grass = per8(grassPer16);
    for (int i = 0; i < grass; i++) {
        int ox = srcX * SIZE_X + rng.nextInt(SIZE_X);
        int oy = rng.nextInt(128);
        int oz = srcZ * SIZE_Z + rng.nextInt(SIZE_Z);
        bool keep = rng.nextInt(100) < TALLGRASS_CLUSTER_PERCENT;
        if (!keep) continue;

        const int h = terrainHeightCached(ox, oz, nullptr);
        if (h < SEA_LEVEL) continue;                  // bajo el agua no hay suelo donde plantar
        int cy = (oy <= h) ? oy : h;                  // dentro de la tierra queda donde está; en el aire baja hasta el suelo
        if (!reachesChunk(ox, oz) || tallGrassPlaced >= MAX_TALLGRASS_PER_CHUNK) { rng.skip(128 * 6); continue; }
        for (int n = 0; n < 128; n++) {
            int a = rng.nextInt(8), b = rng.nextInt(8);
            int x = ox + a - b;
            int c = rng.nextInt(4), d = rng.nextInt(4);
            int y = cy + c - d;
            int e = rng.nextInt(8), f = rng.nextInt(8);
            int z = oz + e - f;
            place(x, y, z, BlockType::TALLGRASS);
        }
    }

    // Hongos (WorldGenFlowers con un hongo): marron en 1 de cada 4 chunks de 16x16 y rojo en 1 de cada 8, a una altura al azar (aqui 0-76,
    // la altura del terreno de este mundo). Solo prenden donde hay aire sobre un bloque opaco y bajo la superficie: en las cuevas.
    auto mushroomPlace = [&](int gx, int gy, int gz, BlockType plant) {
        int xi = gx - chunkX * SIZE_X, zi = gz - chunkZ * SIZE_Z;
        if (xi < 0 || xi >= SIZE_X || zi < 0 || zi >= SIZE_Z || gy < 2 || gy >= SIZE_Y) return;
        if (blocks[xi][gy][zi] != BlockType::AIR || !isNormalCube(blocks[xi][gy - 1][zi])) return;
        if (gy > terrainHeightCached(gx, gz, nullptr) - 2) return;
        setBlockDirect(xi, gy, zi, plant, manager);
    };
    auto mushroomPatch = [&](int ox, int oy, int oz, BlockType plant) {
        if (!reachesChunk(ox, oz)) { rng.skip(64 * 6); return; }
        for (int n = 0; n < 64; n++) {
            int a = rng.nextInt(8), b = rng.nextInt(8);
            int x = ox + a - b;
            int c2 = rng.nextInt(4), d2 = rng.nextInt(4);
            int y = oy + c2 - d2;
            int e = rng.nextInt(8), f = rng.nextInt(8);
            int z = oz + e - f;
            mushroomPlace(x, y, z, plant);
        }
    };
    if (rng.nextInt(16) == 0) {
        int ox = srcX * SIZE_X + rng.nextInt(SIZE_X), oy = rng.nextInt(77), oz = srcZ * SIZE_Z + rng.nextInt(SIZE_Z);
        mushroomPatch(ox, oy, oz, BlockType::MUSHROOM_BROWN);
    }
    if (rng.nextInt(32) == 0) {
        int ox = srcX * SIZE_X + rng.nextInt(SIZE_X), oy = rng.nextInt(77), oz = srcZ * SIZE_Z + rng.nextInt(SIZE_Z);
        mushroomPatch(ox, oy, oz, BlockType::MUSHROOM_RED);
    }

    // Calabazas (WorldGenPumpkin): 1 de cada 32 chunks de 16x16 (1 de cada 128 de 8x8), 64 intentos a +-7 en x y z y +-3 en y del origen. Cada una
    // sale en una celda de aire sobre pasto, con una orientacion al azar. El original elige la altura al azar (casi nunca acierta); aqui el origen
    // se toma sobre el suelo de su columna. La orientacion sale de un hash de la celda para no alterar el flujo de azar de los chunks vecinos.
    const int PUMPKIN_ODDS = 128;
    if (rng.nextInt(PUMPKIN_ODDS) == 0) {
        int ox = srcX * SIZE_X + rng.nextInt(SIZE_X), oz = srcZ * SIZE_Z + rng.nextInt(SIZE_Z);
        const int gh = terrainHeightCached(ox, oz, nullptr);
        const int oy = (gh >= SEA_LEVEL) ? gh + 1 : -100;
        if (!reachesChunk(ox, oz)) {
            rng.skip(64 * 6);
        } else {
            for (int n = 0; n < 64; n++) {
                int a = rng.nextInt(8), b = rng.nextInt(8);
                int x = ox + a - b;
                int c2 = rng.nextInt(4), d2 = rng.nextInt(4);
                int y = oy + c2 - d2;
                int e = rng.nextInt(8), f = rng.nextInt(8);
                int z = oz + e - f;
                int xi = x - chunkX * SIZE_X, zi = z - chunkZ * SIZE_Z;
                if (oy < 0 || xi < 0 || xi >= SIZE_X || zi < 0 || zi >= SIZE_Z || y < 1 || y >= SIZE_Y) continue;
                if (blocks[xi][y][zi] != BlockType::AIR || blocks[xi][y - 1][zi] != BlockType::GRASS) continue;
                setBlockDirect(xi, y, zi, BlockType::PUMPKIN, manager);
                liquidMetaSet(xi, y, zi, (int)(Noise::hashInt(x, y * 7 + 3, z) % 4u));   // la cara tallada va en la metadata
            }
        }
    }

    // Caña de azúcar (WorldGenReed): 20 intentos a +-3 del origen; cada uno que cae sobre pasto, tierra o arena con agua pegada a la base y aire
    // encima levanta 2 a 4 cañas. El original elige la altura al azar (casi nunca acierta); aquí se toma la del suelo de cada columna. El azar sale de
    // un hash de la posicion (no del flujo de arriba) para que los chunks vecinos vean exactamente lo mismo.
    const int CANE_ATTEMPTS = 1;
    for (int at = 0; at < CANE_ATTEMPTS; at++) {
        unsigned int hh = Noise::hashInt(srcX, srcZ, 8800 + at * 37);
        int ox = srcX * SIZE_X + (int)(hh % (unsigned)SIZE_X), oz = srcZ * SIZE_Z + (int)((hh >> 8) % (unsigned)SIZE_Z);
        int x0 = chunkX * SIZE_X, z0 = chunkZ * SIZE_Z;
        if (ox + 3 < x0 || ox - 3 > x0 + SIZE_X - 1 || oz + 3 < z0 || oz - 3 > z0 + SIZE_Z - 1) continue;
        for (int n = 0; n < 20; n++) {
            unsigned int h1 = Noise::hashInt(ox * 31 + n, oz * 17, 8900), h2 = Noise::hashInt(ox + n * 13, oz * 29, 8901), h3 = Noise::hashInt(ox, oz * 7 + n, 8902);
            int gx = ox + (int)(h1 % 4u) - (int)((h1 >> 8) % 4u), gz = oz + (int)(h2 % 4u) - (int)((h2 >> 8) % 4u);
            int xi = gx - x0, zi = gz - z0;
            if (xi < 0 || xi >= SIZE_X || zi < 0 || zi >= SIZE_Z) continue;
            const int gh = terrainHeightCached(gx, gz, nullptr);
            if (gh < 1 || gh + 1 >= SIZE_Y || gh > SEA_LEVEL) continue;           // el agua de al lado tiene que llegar hasta la base
            const BlockType ground = blocks[xi][gh][zi];
            if ((ground != BlockType::GRASS && ground != BlockType::DIRT && ground != BlockType::SAND) || blocks[xi][gh + 1][zi] != BlockType::AIR) continue;
            static const int cdx[4] = { -1, 1, 0, 0 }, cdz[4] = { 0, 0, -1, 1 };
            bool wet = false;
            for (int i = 0; i < 4 && !wet; i++) {
                int nxi = xi + cdx[i], nzi = zi + cdz[i];
                if (nxi >= 0 && nxi < SIZE_X && nzi >= 0 && nzi < SIZE_Z) wet = isWaterBlock(blocks[nxi][gh][nzi]);
                else wet = terrainHeightCached(gx + cdx[i], gz + cdz[i], nullptr) < gh;   // fuera del chunk: el agua llega al nivel del mar
            }
            if (!wet) continue;
            const int tall = 2 + (int)(h3 % ((h3 >> 8) % 3u + 1u));
            for (int k = 0; k < tall; k++) {
                int y = gh + 1 + k;
                if (y >= SIZE_Y || blocks[xi][y][zi] != BlockType::AIR) break;
                setBlockDirect(xi, y, zi, BlockType::SUGAR_CANE, manager);
            }
        }
    }

    structureGenAttempt = step + 1;
    return false;
}

// Manantiales de lava, fuego y piedra luminosa del Nether (ChunkProviderHell.populate: WorldGenHellLava, WorldGenFire, WorldGenGlowStone).
// Un paso por llamada: el 0 son los manantiales, y del 1 al 9 los grupos que empiezan en cada uno de los 3x3 chunks que pueden alcanzar a
// este (así quedan completos en la frontera). Las cantidades del original (chunk de 16x16) se reparten en chunks de 8x8.
bool Chunk::generateNetherFeatures(const ChunkManager* manager) {
    NetherTerrain T;
    const int step = structureGenAttempt;

    auto placeIfFree = [&](int gx, int gy, int gz, BlockType t) {
        int xi = gx - chunkX * SIZE_X, zi = gz - chunkZ * SIZE_Z;
        if (xi < 0 || xi >= SIZE_X || zi < 0 || zi >= SIZE_Z || gy < 0 || gy >= SIZE_Y) return;
        if (blocks[xi][gy][zi] != BlockType::AIR) return;
        setBlockDirect(xi, gy, zi, t, manager);
    };

    if (step == 0) {
        // WorldGenHellLava: una pared de netherrack con una sola salida de aire (8 intentos por chunk de 16x16: 2 por chunk de 8x8);
        // solo se prueban celdas del interior del chunk, para no depender de los vecinos
        Lakes::JRandom rng(((long long)Noise::hashInt(chunkX, chunkZ, 6300) << 20) ^ (long long)Noise::hashInt(chunkZ, chunkX, 6400));
        for (int n = 0; n < 2; n++) {
            int xi = 1 + rng.nextInt(SIZE_X - 2);
            int zi = 1 + rng.nextInt(SIZE_Z - 2);
            int yi = rng.nextInt(120) + 4;
            if (yi < 1 || yi >= SIZE_Y - 1) continue;
            if (blocks[xi][yi + 1][zi] != BlockType::NETHERRACK) continue;
            BlockType here = blocks[xi][yi][zi];
            if (here != BlockType::AIR && here != BlockType::NETHERRACK) continue;
            const BlockType around[5] = { blocks[xi - 1][yi][zi], blocks[xi + 1][yi][zi], blocks[xi][yi][zi - 1], blocks[xi][yi][zi + 1], blocks[xi][yi - 1][zi] };
            int rock = 0, air = 0;
            for (int j = 0; j < 5; j++) {
                if (around[j] == BlockType::NETHERRACK) rock++;
                if (around[j] == BlockType::AIR) air++;
            }
            if (rock == 4 && air == 1) setBlockDirect(xi, yi, zi, BlockType::LAVA_FLOW, manager);
        }
        structureGenAttempt = 1;
        return false;
    }

    if (step <= 9) {
        const int k = step - 1;
        const int srcX = chunkX - 1 + (k % 3), srcZ = chunkZ - 1 + (k / 3);
        Lakes::JRandom rng(((long long)Noise::hashInt(srcX, srcZ, 6100) << 20) ^ (long long)Noise::hashInt(srcZ + 31, srcX - 31, 6200));

        // WorldGenFire (en el original, unos 3 parches por chunk de 16x16, con el origen al azar). Aquí el suelo tiene una sola altura por
        // columna, así que el origen se toma pegado al suelo y se deja un parche cada 3 o 4 chunks de origen: da unas 2 o 3 llamas por chunk
        if (rng.nextInt(10) < 3) {
            int ox = srcX * SIZE_X + rng.nextInt(SIZE_X);
            int oz = srcZ * SIZE_Z + rng.nextInt(SIZE_Z);
            const NetherGen::Col& col = netherColumnAt(ox, oz);
            if (col.floorTop >= NetherGen::LAVA_LEVEL) {
                int oy = col.floorTop + rng.nextInt(3);
                NetherGen::firePatch(T, rng, ox, oy, oz, [&](int x, int y, int z) { placeIfFree(x, y, z, BlockType::FIRE); });
            }
        }

        // WorldGenGlowStone1 y 2 (el mismo código): un grupo colgado del techo, en 3 de cada 5 chunks de origen. El origen se toma
        // justo bajo el techo de la columna, porque en el original casi todos los orígenes al azar caen fuera del techo
        if (rng.nextInt(5) < 3) {
            int ox = srcX * SIZE_X + rng.nextInt(SIZE_X);
            int oz = srcZ * SIZE_Z + rng.nextInt(SIZE_Z);
            const NetherGen::Col& col = netherColumnAt(ox, oz);
            int oy = col.ceilBottom - 1;
            if (!col.pillar && oy > col.floorTop && oy >= NetherGen::LAVA_LEVEL) {
                static NetherGen::Glow glow;
                if (NetherGen::glowCluster(T, rng, ox, oy, oz, glow)) {
                    for (int i = 0; i < 15; i++)
                        for (int j = 0; j < 12; j++)
                            for (int q = 0; q < 15; q++)
                                if (glow.cell[i][j][q]) placeIfFree(ox - 7 + i, oy - 11 + j, oz - 7 + q, BlockType::GLOWSTONE);
                }
            }
        }
        structureGenAttempt = step + 1;
        return false;
    }
    return true;
}

void Chunk::ensureClimate() {
    if (climateReady) return;
    lat.fill(chunkX * SIZE_X, chunkZ * SIZE_Z);
    climateReady = true;
}

// Bioma de la Beta (temperatura y humedad -> BiomeGenBase.getBiome) y colores del pasto y de las hojas de la columna
void Chunk::computeColumnBiome(int xi, int zi) {
    ensureClimate();
    const int gx = chunkX * SIZE_X + xi, gz = chunkZ * SIZE_Z + zi;
    float t, h;
    lat.at(gx, gz, t, h);
    BiomeType b = Climate::biomeFor(t, h);
    if (terrainHeightCached(gx, gz, nullptr) < TERRAIN_SEA_LEVEL) b = BiomeType::OCEAN; // marca de columna bajo el agua
    biomeColumn[xi][zi] = b;

    // El color sale del mapa en el clima de la propia columna (el clima ya cambia suave de una columna a otra)
    u8 r, g, bl;
    ColorMaps::grass(t, h, r, g, bl);
    grassTint[xi][zi][0] = r; grassTint[xi][zi][1] = g; grassTint[xi][zi][2] = bl;
    ColorMaps::foliage(t, h, r, g, bl);
    foliageTint[xi][zi][0] = r; foliageTint[xi][zi][1] = g; foliageTint[xi][zi][2] = bl;
}

// Los chunks que se cargan del disco no pasan por generateColumn: el bioma y los colores se vuelven a calcular (son función del clima)
void Chunk::computeBiomeData() {
    for (int xi = 0; xi < SIZE_X; xi++)
        for (int zi = 0; zi < SIZE_Z; zi++) {
            if (s_dimension == 0) computeColumnBiome(xi, zi);
            else biomeColumn[xi][zi] = BiomeType::GENERIC;
        }
}

// Color con el que se pinta un bloque en esta columna: el pasto y el pasto alto, del mapa del pasto; las hojas de roble, del de las
// hojas; las de abedul y abeto tienen un color fijo (BlockLeaves). Lo demás, sin tinte.
void Chunk::columnTint(BlockType type, int xi, int zi, u8& r, u8& g, u8& b) const {
    switch (type) {
        case BlockType::GRASS:
        case BlockType::TALLGRASS:
        case BlockType::SUGAR_CANE:
            r = grassTint[xi][zi][0]; g = grassTint[xi][zi][1]; b = grassTint[xi][zi][2];
            break;
        case BlockType::OAK_LEAVES:
            r = foliageTint[xi][zi][0]; g = foliageTint[xi][zi][1]; b = foliageTint[xi][zi][2];
            break;
        default:
            getGrassTint(type, false, r, g, b); // abedul, abeto y lo que no se tiñe
            break;
    }
}

void Chunk::generateColumn(int xi, int zi) {
    if (s_dimension == -1) { generateNetherColumn(xi, zi); return; }

    const int SEA_LEVEL = TERRAIN_SEA_LEVEL;
    const int DIRT_LAYER_DEPTH = 3;

    float globalXi = chunkX * SIZE_X + xi;
    float globalZi = chunkZ * SIZE_Z + zi;

    // El ruido de bioma de siempre solo da las cuencas oceánicas del terreno (altura); el bioma sale del clima (temperatura y humedad)
    float biomeValue = Noise::biomeNoise2D(globalXi * BiomeConfig::BIOME_NOISE_SCALE,
                                             globalZi * BiomeConfig::BIOME_NOISE_SCALE);
    computeColumnBiome(xi, zi);
    BiomeType biome = biomeColumn[xi][zi];   // OCEAN si la columna queda bajo el agua

    BlockType patchType_ = BlockType::AIR;
    bool isPatch = (biome != BiomeType::DESERT && biome != BiomeType::OCEAN) &&
                   getPatchAt(globalXi, globalZi, patchType_);

    const float OCEAN_THRESHOLD = 0.25f;
    int height = terrainHeightCached((int)globalXi, (int)globalZi, &biomeValue);

    if (height > highestGeneratedY) highestGeneratedY = height;

    // --- Fix 2: playas ---
    bool isUnderwater = (height < SEA_LEVEL);

    BlockType surfaceType = BlockType::GRASS;
    if (isUnderwater) {
        surfaceType = BlockType::SAND; // piso de océano/lago
    } else if (biome == BiomeType::DESERT) {
        surfaceType = BlockType::SAND;
    } else {
        const int BEACH_CHECK_RADIUS = 6;
        bool nearOcean = false;

        for (int dx = -BEACH_CHECK_RADIUS; dx <= BEACH_CHECK_RADIUS && !nearOcean; dx += 3) {
            for (int dz = -BEACH_CHECK_RADIUS; dz <= BEACH_CHECK_RADIUS && !nearOcean; dz += 3) {
                float checkX = globalXi + dx;
                float checkZ = globalZi + dz;
                float checkBiomeValue = Noise::biomeNoise2D(checkX * BiomeConfig::BIOME_NOISE_SCALE,
                                                               checkZ * BiomeConfig::BIOME_NOISE_SCALE);
                if (checkBiomeValue < OCEAN_THRESHOLD) {
                    nearOcean = true;
                }
            }
        }

        if (nearOcean) {
            surfaceType = BlockType::SAND;
        }
    }

    if (isPatch && patchType_ == BlockType::SAND) surfaceType = BlockType::SAND;
    if (isPatch && patchType_ == BlockType::GRAVEL) surfaceType = BlockType::GRAVEL;

    for (int yi = 0; yi <= height && yi < SIZE_Y; yi++) {
        if (yi == height) {
            blocks[xi][yi][zi] = surfaceType;
        } else if (yi >= height - DIRT_LAYER_DEPTH) {
            blocks[xi][yi][zi] = (surfaceType == BlockType::GRASS) ? BlockType::DIRT : surfaceType;
        } else if (biome == BiomeType::DESERT && yi == height - DIRT_LAYER_DEPTH - 1) {
            blocks[xi][yi][zi] = BlockType::SANDSTONE;
        } else {
            blocks[xi][yi][zi] = BlockType::STONE;
        }
    }

    // Capa de bedrock (replaceBlocksForBiome de la Beta): y=0 siempre, y=1..4 con 80/60/40/20%
    for (int yi = 0; yi < 5 && yi < SIZE_Y; yi++) {
        if (yi == 0 || (int)(Noise::hashInt((int)globalXi, (int)globalZi, 900 + yi) % 5u) >= yi) {
            blocks[xi][yi][zi] = BlockType::BEDROCK;
        }
    }

    // Relleno de agua: SOLO por altura real de esta columna, ya no
    // por clasificación de bioma — elimina la discontinuidad
    int waterTop = isUnderwater ? SEA_LEVEL : -1;
    if (waterTop >= 0) {
        for (int yi = height + 1; yi <= waterTop && yi < SIZE_Y; yi++) {
            blocks[xi][yi][zi] = BlockType::WATER;
            if (yi > highestGeneratedY) highestGeneratedY = yi;
        }
    }

    // Nieve (ChunkProviderGenerate.populate): donde la temperatura del lugar, bajada por la altura (la Beta usa 64 de nivel del mar y aquí
    // es 38), es menor a 0,5. Sobre cualquier suelo sólido (pasto, arena, grava) y nunca bajo el agua. Las flores y el pasto alto se
    // colocan en grupos más adelante (generatePlantClusters) y reemplazan la nieve de su celda, como en la Beta.
    float colTemp, colHumid;
    climateAt((int)globalXi, (int)globalZi, colTemp, colHumid);
    // Hielo: el agua de la superficie se congela donde la nieve cubriria la tierra (misma temperatura, bajada por la altura)
    if (isUnderwater && waterTop >= 0 && waterTop < SIZE_Y && blocks[xi][waterTop][zi] == BlockType::WATER &&
        (colTemp - (float)(waterTop - SEA_LEVEL) / 64.0f * 0.3f) < 0.5f) {
        blocks[xi][waterTop][zi] = BlockType::ICE;
    }
    bool snowy = !isUnderwater && height + 1 < SIZE_Y && isMaterialSolid(blocks[xi][height][zi]) &&
                 (colTemp - (float)(height - SEA_LEVEL) / 64.0f * 0.3f) < 0.5f;
    if (snowy) {
        blocks[xi][height + 1][zi] = BlockType::SNOW_LAYER;
        if (height + 1 > highestGeneratedY) highestGeneratedY = height + 1;
    } else if (biome == BiomeType::DESERT && blocks[xi][height][zi] == BlockType::SAND &&
               height + 1 < SIZE_Y && blocks[xi][height + 1][zi] == BlockType::AIR) {
        unsigned int bushRoll = Noise::hashInt((int)globalXi, (int)globalZi, 1) % 20;
        if (bushRoll == 0) {
            blocks[xi][height + 1][zi] = BlockType::DEAD_BUSH;
            if (height + 1 > highestGeneratedY) highestGeneratedY = height + 1;
        }
    }

    for (int yi = 0; yi <= height && yi < SIZE_Y; yi++) {
        updateFaceCache(xi, yi, zi, nullptr);
    }
    for (int yi = height + 1; yi <= highestGeneratedY && yi < SIZE_Y; yi++) {
        if (blocks[xi][yi][zi] != BlockType::AIR) {
            updateFaceCache(xi, yi, zi, nullptr);
        }
    }
}

// Cuevas de Beta 1.7.3 (ver Caves.h). Devuelve true cuando este chunk ya está excavado.
bool Chunk::generateCaves(const ChunkManager* manager) {
    (void)manager;
    bool worked = false;
    if (!Caves::prepare(chunkX, chunkZ, worked)) {
        if (worked) s_heavyDidOne = true;
        return false; // todavía se están simulando las celdas vecinas
    }
    for (;;) {
        bool finished = Caves::carveStep(blocks, chunkX, chunkZ, caveCursor, 1);
        s_heavyDidOne = true;
        if (finished) return true;
        if (!heavyBudgetLeft()) return false;
    }
}

// Lagos de Beta 1.7.3 (WorldGenLakes). Cada chunk evalúa los lagos que empiezan en los 3x3 chunks que pueden alcanzarlo y
// solo escribe lo que cae dentro de él, así los lagos (de hasta 16 bloques) quedan completos y sin costuras.
namespace {
    // Terreno base por altura, sin cuevas ni árboles (los lagos se hacen antes que los árboles, como en populate)
    struct LakeTerrain {
        bool lava;
        int cell(int gx, int gy, int gz) const { // 0 = aire, 1 = sólido, 2 = agua del mar
            if (gy < 0 || gy >= WorldConfig::SIZE_Y) return 0;
            int h = terrainHeightCached(gx, gz, nullptr);
            if (gy <= h) return 1;
            if (h < TERRAIN_SEA_LEVEL && gy <= TERRAIN_SEA_LEVEL) return 2;
            return 0;
        }
        bool isAir(int x, int y, int z) const { return cell(x, y, z) == 0; }
        bool isSolid(int x, int y, int z) const { return cell(x, y, z) == 1; }
        bool isLiquid(int x, int y, int z) const { return cell(x, y, z) == 2; }
        bool isSameLiquid(int x, int y, int z) const { return !lava && cell(x, y, z) == 2; }
    };
}

bool Chunk::generateLakes(const ChunkManager* manager) {
    const int UNITS = 9 * 2;
    // Probabilidad de intentar un lago por chunk de origen: 1 de cada N. El original (chunk de 16x16) usa 4 para el agua y 8 para la
    // lava; aquí el chunk tiene la cuarta parte del área, así que 16 y 32. Bajalos para tener más lagos.
    const int WATER_LAKE_ODDS = 16;
    const int LAVA_LAKE_ODDS = 32;
    const int COLUMNS_PER_CALL = 8;   // columnas de terreno que se calculan por llamada (el ruido es caro)
    static Lakes::Result res;

    while (lakeUnit < UNITS) {
        int src = lakeUnit / 2;
        bool lava = (lakeUnit & 1) != 0;
        int srcX = chunkX - 2 + (src % 3);   // un lago de 16 sale hasta 22 bloques del origen del chunk: alcanza a 3 chunks
        int srcZ = chunkZ - 2 + (src / 3);

        long long seed = ((long long)Noise::hashInt(srcX, srcZ, 4100 + (lava ? 1 : 0)) << 20) ^
                         (long long)Noise::hashInt(srcZ + 77, srcX - 77, 4200 + (lava ? 1 : 0));
        Lakes::JRandom rng(seed);

        if (rng.nextInt(lava ? LAVA_LAKE_ODDS : WATER_LAKE_ODDS) != 0) { lakeUnit++; lakeWarm = 0; continue; }
        int ox = srcX * SIZE_X + rng.nextInt(SIZE_X) + 8;
        int oz = srcZ * SIZE_Z + rng.nextInt(SIZE_Z) + 8;
        int oy;
        if (!lava) {
            oy = rng.nextInt(77);                          // rand(128) del original, escalado a este terreno (x0,6)
        } else {
            oy = rng.nextInt(rng.nextInt(72) + 5);         // rand(rand(120) + 8), escalado
            if (!(oy < TERRAIN_SEA_LEVEL || rng.nextInt(10) == 0)) { lakeUnit++; lakeWarm = 0; continue; }
        }

        // Primero se calculan (y quedan en la caché) las 256 columnas de terreno de la región, de a pocas por llamada
        int x0 = ox - 8, z0 = oz - 8;
        if (lakeWarm < 256) {
            int n = 0;
            while (lakeWarm < 256 && n < COLUMNS_PER_CALL) {
                terrainHeightCached(x0 + (lakeWarm >> 4), z0 + (lakeWarm & 15), nullptr);
                lakeWarm++; n++;
            }
            s_heavyDidOne = true;
            return false;
        }

        if (!lakeBuilt) {
            if (s_lakeOwner && s_lakeOwner != this) return false;   // otro chunk espera escribir su lago: la tabla de resultado es compartida
            LakeTerrain terrain;
            terrain.lava = lava;
            const bool ok = Lakes::build(terrain, rng, lava, ox, oy, oz, res);
            s_heavyDidOne = true;
            if (!ok) { lakeUnit++; lakeWarm = 0; return false; }
            lakeBuilt = true; s_lakeOwner = this;   // la escritura va en la siguiente llamada (antes: calculo y escritura juntos, ~12 ms)
            return false;
        }
        lakeBuilt = false; s_lakeOwner = nullptr;
        {
            struct Writer {
                Chunk* c; const ChunkManager* m; BlockType liquid;
                int baseX, baseZ;
                int top(int gx, int gz) { return terrainHeightCached(gx, gz, nullptr); }
                bool inside(int gx, int gy, int gz, int& xi, int& zi) const {
                    xi = gx - baseX; zi = gz - baseZ;
                    return xi >= 0 && xi < SIZE_X && zi >= 0 && zi < SIZE_Z && gy >= 0 && gy < SIZE_Y;
                }
                bool isDirt(int gx, int gy, int gz) {
                    int xi, zi;
                    return inside(gx, gy, gz, xi, zi) && c->blocks[xi][gy][zi] == BlockType::DIRT;
                }
                void put(int gx, int gy, int gz, int kind) {
                    int xi, zi;
                    if (!inside(gx, gy, gz, xi, zi)) return;
                    BlockType t = kind == 0 ? BlockType::AIR : (kind == 1 ? liquid : (kind == 2 ? BlockType::GRASS : BlockType::STONE));
                    if (c->blocks[xi][gy][zi] == t) return;
                    if (kind == 3 && !isMaterialSolid(c->blocks[xi][gy][zi])) return; // la cáscara solo reemplaza lo sólido (no tapa cuevas)
                    c->setBlockDirect(xi, gy, zi, t, m);
                }
            };
            Writer w{ this, manager, lava ? BlockType::LAVA : BlockType::WATER, chunkX * SIZE_X, chunkZ * SIZE_Z };
            Lakes::apply(w, res, lava);
        }
        lakeUnit++;
        lakeWarm = 0;
        s_heavyDidOne = true;
        return false; // un lago por llamada
    }
    return true;
}

// Manantiales de Beta 1.7.3 (WorldGenLiquids): agua y lava metidas en una pared de piedra con una sola salida de aire.
// Solo se prueban celdas del interior del chunk (1 a 6) para no depender de los chunks vecinos. La fuente queda como corriente
// y empieza a fluir sola en cuanto el chunk entra en juego.
void Chunk::generateSprings(const ChunkManager* manager) {
    Lakes::JRandom rng(((long long)Noise::hashInt(chunkX, chunkZ, 5100) << 20) ^ (long long)Noise::hashInt(chunkZ, chunkX, 5200));
    // 50 intentos de agua y 20 de lava por chunk de 16x16: 12,5 y 5 por chunk de 8x8
    const int attempts[2] = { 12 + (rng.nextInt(2) == 0 ? 1 : 0), 5 };

    for (int pass = 0; pass < 2; pass++) {
        const bool lava = (pass == 1);
        for (int k = 0; k < attempts[pass]; k++) {
            int xi = 1 + rng.nextInt(SIZE_X - 2);
            int zi = 1 + rng.nextInt(SIZE_Z - 2);
            // alturas del original, escaladas a este terreno (x0,6)
            int yi = lava ? rng.nextInt(rng.nextInt(rng.nextInt(67) + 5) + 5)
                          : rng.nextInt(rng.nextInt(72) + 5);
            if (yi < 1 || yi >= SIZE_Y - 1) continue;

            if (blocks[xi][yi + 1][zi] != BlockType::STONE) continue;
            if (blocks[xi][yi - 1][zi] != BlockType::STONE) continue;
            BlockType here = blocks[xi][yi][zi];
            if (here != BlockType::AIR && here != BlockType::STONE) continue;

            int stones = 0, airs = 0;
            const BlockType sides[4] = { blocks[xi - 1][yi][zi], blocks[xi + 1][yi][zi], blocks[xi][yi][zi - 1], blocks[xi][yi][zi + 1] };
            for (int j = 0; j < 4; j++) {
                if (sides[j] == BlockType::STONE) stones++;
                if (sides[j] == BlockType::AIR) airs++;
            }
            if (stones == 3 && airs == 1) {
                setBlockDirect(xi, yi, zi, lava ? BlockType::LAVA_FLOW : BlockType::WATER_FLOW, manager);
            }
        }
    }
}

void Chunk::update(const ChunkManager* manager) {
    if (generationComplete) {
        { PROF(Prof::FACES); continueFaceCacheRecalc(manager); }
        { PROF(Prof::SKY);   continueInitialSkyLight(manager); continueEmitterReseed(manager); }
        return;
    }

    if (genXi < SIZE_X) {
        if (s_dimension == 0 && !climateReady) {   // el clima del chunk se mide en 4 cuadros de ~2,5 ms (antes: 16 puntos de golpe, ~10 ms en la primera columna)
            PROFSTEP("climate");
            lat.fillRow(chunkX * SIZE_X, chunkZ * SIZE_Z, climateRow);
            if (++climateRow >= 4) climateReady = true;
            s_heavyDidOne = true;
            return;
        }
        PROF(Prof::GEN_COL);
        // Hasta 3 columnas por cuadro mientras quede presupuesto de tiempo (antes una sola: el terreno de cada chunk tardaba 64 cuadros).
        // La primera siempre se hace; las demás solo si no se pasó de los ~6 ms.
        int cols = 0;
        u32 lastColCost = 0;
        do {
            const u32 colStart = Prof::now();
            { PROFSTEP("column"); generateColumn(genXi, genZi); }
            s_heavyDidOne = true;
            if (!cavesDone) { // las cuevas se van simulando mientras se generan las columnas
                bool caveWork;
                PROFSTEP("caves prep");
                Caves::prepare(chunkX, chunkZ, caveWork);
            }
            genZi++;
            if (genZi >= SIZE_Z) {
                genZi = 0;
                genXi++;
            }
            cols++;
            lastColCost = Prof::now() - colStart;
        } while (genXi < SIZE_X && cols < WorldConfig::genColsPerFrame() && heavyBudgetLeft()
                 && (Prof::now() - s_heavyFrameStart) + lastColCost < s_heavyBudget * (u32)WorldConfig::budgetMul());   // no empieza otra columna si no le cabe (antes 2 columnas de 10 ms = 20+)
        return;
    }

    PROF(Prof::GEN_STRUCT);
    if (!cavesDone) {
        PROFSTEP("caves");
        if (generateCaves(manager)) cavesDone = true;
        return; // un paso por cuadro; árboles, menas y cactus empiezan cuando terminan las cuevas
    }
    s_structureGenActive = true;
    // Terreno terminado — repartimos las estructuras en varios frames
    // separados, en vez de las 3 juntas de golpe
    switch (structureGenPhase) {
        case 0: {
            PROFSTEP("lakes");
            if (generateLakes(manager)) { structureGenPhase = 1; structureGenXi = 0; structureGenZi = 0; }
            break;
        }
        case 1: {
            PROFSTEP("trees");
            bool done;
            generateTrees(manager, structureGenXi, structureGenZi, structureGenXi, structureGenZi, done);
            if (done) { structureGenPhase = 2; structureGenXi = 0; structureGenZi = 0; }
            break;
        }
        case 2: {
            PROFSTEP("ores");
            bool done;
            generateOres(manager, structureGenAttempt, structureGenAttempt, done);
            if (done) { structureGenPhase = 3; structureGenXi = 0; structureGenZi = 0; structureGenAttempt = 0; }
            break;
        }
        case 3: {
            PROFSTEP("cactus");
            bool done;
            generateCactus(manager, structureGenXi, structureGenZi, structureGenXi, structureGenZi, done);
            if (done) { structureGenPhase = 4; }
            break;
        }
        case 4: {
            PROFSTEP("springs");
            generateSprings(manager);
            structureGenPhase = 6;
            structureGenAttempt = 0;
            break;
        }
        case 6: {
            PROFSTEP("plants");
            if (generatePlantClusters(manager)) structureGenPhase = 7;
            break;
        }
        case 7: {
            PROFSTEP("dungeons");
            generateDungeons(manager);
            structureGenPhase = 5;
            break;
        }
        case 10: {
            PROFSTEP("nether");
            if (generateNetherFeatures(manager)) structureGenPhase = 5;
            break;
        }
        case 5: {
            PROFSTEP("closing");
            generationComplete = true;
	    rebuildAllOccupancy();
            beginFullFaceCacheRecalc();
            initialSkyLightPending = true;
            initialSkyLightXi = 0;
            initialSkyLightZi = 0;
	    skyInitPhase = 0; skyInitDir = 0;
   	    skyBaseReady = false;
            break;
        }
    }
    s_structureGenActive = false;
}

bool Chunk::isGenerationComplete() const {
    return generationComplete;
}

float Chunk::gridToWorldX(int xi) const {
    return chunkX * SIZE_X + xi - SIZE_X / 2.0f + 0.5f;
}

float Chunk::gridToWorldY(int yi) const {
    return -1.0f + yi * 1.0f;
}

float Chunk::gridToWorldZ(int zi) const {
    return chunkZ * SIZE_Z + zi - SIZE_Z / 2.0f + 0.5f;
}

bool Chunk::worldToGrid(float worldX, float worldY, float worldZ,
                         int& outXi, int& outYi, int& outZi) const {
    int xi = (int)(worldX - chunkX * SIZE_X + SIZE_X / 2.0f);
    int yi = (int)(worldY + 1.0f + 0.5f);
    int zi = (int)(worldZ - chunkZ * SIZE_Z + SIZE_Z / 2.0f);

    if (xi < 0 || xi >= SIZE_X) return false;
    if (yi < 0 || yi >= SIZE_Y) return false;
    if (zi < 0 || zi >= SIZE_Z) return false;

    outXi = xi;
    outYi = yi;
    outZi = zi;
    return true;
}

bool Chunk::isSolidAt(float worldX, float worldY, float worldZ) const {
    int xi, yi, zi;
    if (!worldToGrid(worldX, worldY, worldZ, xi, yi, zi)) return false;

    BlockType type = blocks[xi][yi][zi];
    const float fx = worldX - (gridToWorldX(xi) - 0.5f), fy = worldY - (gridToWorldY(yi) - 0.5f), fz = worldZ - (gridToWorldZ(zi) - 0.5f);
    if (yi > 0 && blocks[xi][yi - 1][zi] == BlockType::FENCE && fy < 0.5f) {   // la valla llega a 1,5 de alto: su mitad de arriba esta en la celda de encima
        float bd[6]; Partial::fenceBox(liquidMetaGet(xi, yi - 1, zi), bd);
        if (fx >= bd[0] && fx <= bd[3] && fz >= bd[2] && fz <= bd[5]) return true;
    }
    if (isPartialBlock(type)) {   // puertas, trampilla, cama, pastel, losas, escaleras y valla: solo cuenta el punto que cae dentro de su caja (el cartel no estorba)
        if (!Partial::hasCollision(type)) return false;
        float boxes[9][6]; const int nb = Partial::collisionBoxes(type, liquidMetaGet(xi, yi, zi), boxes);
        for (int k = 0; k < nb; k++) {
            const float* bd = boxes[k];
            if (fx >= bd[0] && fx <= bd[3] && fy >= bd[1] && fy < bd[4] && fz >= bd[2] && fz <= bd[5]) return true;   // arriba estricto: parado justo sobre la losa no choca
        }
        return false;
    }
    return type != BlockType::AIR && type != BlockType::TALLGRASS &&
           !isLiquidBlock(type) && !isTorchBlock(type) && type != BlockType::FIRE && type != BlockType::PORTAL &&
           type != BlockType::SNOW_LAYER && type != BlockType::DEAD_BUSH &&
           !isCrossPlant(type);
}

bool Chunk::isSolidGrid(int xi, int yi, int zi) const {
    if (xi < 0 || xi >= SIZE_X) return false;
    if (yi < 0 || yi >= SIZE_Y) return false;
    if (zi < 0 || zi >= SIZE_Z) return false;

    BlockType type = blocks[xi][yi][zi];
    return type != BlockType::AIR && type != BlockType::TALLGRASS && !isLiquidBlock(type) &&
           !isTorchBlock(type) && type != BlockType::FIRE && type != BlockType::PORTAL && type != BlockType::SNOW_LAYER && type != BlockType::DEAD_BUSH &&
           !isCrossPlant(type) && !isPartialBlock(type) && type != BlockType::GLASS && type != BlockType::ICE;
}

static void drawCrossQuad(TextureCell cell, u8 tintR, u8 tintG, u8 tintB) {
    int u0 = cell.col * 16, v0 = cell.row * 16;
    int u1 = u0 + 16, v1 = v0 + 16;

    glColor3b(tintR, tintG, tintB);

    glBegin(GL_QUADS);
        GFX_TEX_COORD = TEXTURE_PACK(inttot16(u0), inttot16(v1)); glVertex3v16(floattov16(-0.5f), floattov16(-0.5f), floattov16(-0.5f));
        GFX_TEX_COORD = TEXTURE_PACK(inttot16(u1), inttot16(v1)); glVertex3v16(floattov16( 0.5f), floattov16(-0.5f), floattov16( 0.5f));
        GFX_TEX_COORD = TEXTURE_PACK(inttot16(u1), inttot16(v0)); glVertex3v16(floattov16( 0.5f), floattov16( 0.5f), floattov16( 0.5f));
        GFX_TEX_COORD = TEXTURE_PACK(inttot16(u0), inttot16(v0)); glVertex3v16(floattov16(-0.5f), floattov16( 0.5f), floattov16(-0.5f));

        GFX_TEX_COORD = TEXTURE_PACK(inttot16(u0), inttot16(v1)); glVertex3v16(floattov16(-0.5f), floattov16(-0.5f), floattov16( 0.5f));
        GFX_TEX_COORD = TEXTURE_PACK(inttot16(u1), inttot16(v1)); glVertex3v16(floattov16( 0.5f), floattov16(-0.5f), floattov16(-0.5f));
        GFX_TEX_COORD = TEXTURE_PACK(inttot16(u1), inttot16(v0)); glVertex3v16(floattov16( 0.5f), floattov16( 0.5f), floattov16(-0.5f));
        GFX_TEX_COORD = TEXTURE_PACK(inttot16(u0), inttot16(v0)); glVertex3v16(floattov16(-0.5f), floattov16( 0.5f), floattov16( 0.5f));
    glEnd();
}

static void drawCenteredCrossQuad(TextureCell cell, u8 tintR, u8 tintG, u8 tintB) {
    int u0 = cell.col * 16, v0 = cell.row * 16;
    int u1 = u0 + 16, v1 = v0 + 16;

    glColor3b(tintR, tintG, tintB);

    glBegin(GL_QUADS);
        GFX_TEX_COORD = TEXTURE_PACK(inttot16(u0), inttot16(v1)); glVertex3v16(floattov16(-0.5f), floattov16(-0.5f), floattov16(0));
        GFX_TEX_COORD = TEXTURE_PACK(inttot16(u1), inttot16(v1)); glVertex3v16(floattov16( 0.5f), floattov16(-0.5f), floattov16(0));
        GFX_TEX_COORD = TEXTURE_PACK(inttot16(u1), inttot16(v0)); glVertex3v16(floattov16( 0.5f), floattov16( 0.5f), floattov16(0));
        GFX_TEX_COORD = TEXTURE_PACK(inttot16(u0), inttot16(v0)); glVertex3v16(floattov16(-0.5f), floattov16( 0.5f), floattov16(0));

        GFX_TEX_COORD = TEXTURE_PACK(inttot16(u0), inttot16(v1)); glVertex3v16(floattov16(0), floattov16(-0.5f), floattov16(-0.5f));
        GFX_TEX_COORD = TEXTURE_PACK(inttot16(u1), inttot16(v1)); glVertex3v16(floattov16(0), floattov16(-0.5f), floattov16( 0.5f));
        GFX_TEX_COORD = TEXTURE_PACK(inttot16(u1), inttot16(v0)); glVertex3v16(floattov16(0), floattov16( 0.5f), floattov16( 0.5f));
        GFX_TEX_COORD = TEXTURE_PACK(inttot16(u0), inttot16(v0)); glVertex3v16(floattov16(0), floattov16( 0.5f), floattov16(-0.5f));
    glEnd();
}

// Tipos que el camino LOD no dibuja (hojas, plantas, antorchas...)
static inline bool lodSkipTop(BlockType t) {
    return t == BlockType::TALLGRASS || isTorchBlock(t) || t == BlockType::FIRE || t == BlockType::PORTAL || t == BlockType::CACTUS ||
           t == BlockType::OAK_LEAVES || t == BlockType::BIRCH_LEAVES || t == BlockType::SNOW_LAYER || t == BlockType::SPRUCE_LEAVES ||
           t == BlockType::DEAD_BUSH || isCrossPlant(t) || isPartialBlock(t);
}

static void drawLodTopQuad(TextureCell cell, u8 tintR, u8 tintG, u8 tintB, float s = 1.0f) {
    const float h = 0.5f * s;
    int u0 = cell.col * 16, v0 = cell.row * 16;
    int u1 = u0 + 16, v1 = v0 + 16;

    glBegin(GL_QUADS);
        glColor3b(tintR, tintG, tintB);
        GFX_TEX_COORD = TEXTURE_PACK(inttot16(u0), inttot16(v1)); glVertex3v16(floattov16(-h), floattov16(0.5f), floattov16( h));
        GFX_TEX_COORD = TEXTURE_PACK(inttot16(u1), inttot16(v1)); glVertex3v16(floattov16( h), floattov16(0.5f), floattov16( h));
        GFX_TEX_COORD = TEXTURE_PACK(inttot16(u1), inttot16(v0)); glVertex3v16(floattov16( h), floattov16(0.5f), floattov16(-h));
        GFX_TEX_COORD = TEXTURE_PACK(inttot16(u0), inttot16(v0)); glVertex3v16(floattov16(-h), floattov16(0.5f), floattov16(-h));
    glEnd();
}

static u32 packTexCoordCell(TextureCell cell, int corner) {
    int u0 = cell.col * 16, v0 = cell.row * 16;
    int u1 = u0 + 16, v1 = v0 + 16;
    switch (corner) {
        case 0: return TEXTURE_PACK(inttot16(u0), inttot16(v1));
        case 1: return TEXTURE_PACK(inttot16(u1), inttot16(v1));
        case 2: return TEXTURE_PACK(inttot16(u1), inttot16(v0));
        default: return TEXTURE_PACK(inttot16(u0), inttot16(v0));
    }
}

// Arma la lista del camino LOD: la cara superior de cada columna (o de cada bloque de 2 x 2 si coarse). Las posiciones son relativas al origen del chunk
// en X y Z (se pone con una traslacion antes de llamarla) y absolutas en Y; cada quad lleva su propia traslacion dentro de la lista.
bool Chunk::buildLodList(bool coarse) {
    DisplayListBuilder b(s_dlScratch, DL_SCRATCH_WORDS);
    const int lodStep = coarse ? 2 : 1;
    const float qh = coarse ? 1.0f : 0.5f;
    for (int gxi = 0; gxi < SIZE_X; gxi += lodStep) {
        for (int gzi = 0; gzi < SIZE_Z; gzi += lodStep) {
            int xi = gxi, zi = gzi;
            float lx = (float)gxi, lz = (float)gzi;
            if (coarse) {
                int bestY = -1;
                for (int a = 0; a < 2; a++) {
                    for (int bb = 0; bb < 2; bb++) {
                        const int cy = columnMaxY[gxi + a][gzi + bb];
                        if (cy < columnMinY[gxi + a][gzi + bb] || cy <= bestY) continue;
                        if (lodSkipTop(blocks[gxi + a][cy][gzi + bb])) continue;
                        bestY = cy; xi = gxi + a; zi = gzi + bb;
                    }
                }
                if (bestY < 0) continue;
                lx += 0.5f; lz += 0.5f;   // centro del bloque de 2 x 2
            }
            const int colMaxY = columnMaxY[xi][zi];
            if (colMaxY < columnMinY[xi][zi]) continue;

            const BlockType topType = blocks[xi][colMaxY][zi];
            if (topType == BlockType::TALLGRASS || isTorchBlock(topType) || topType == BlockType::FIRE || topType == BlockType::PORTAL || topType == BlockType::CACTUS ||
                topType == BlockType::OAK_LEAVES || topType == BlockType::BIRCH_LEAVES ||
                topType == BlockType::SNOW_LAYER || topType == BlockType::SPRUCE_LEAVES ||
                topType == BlockType::DEAD_BUSH || topType == BlockType::ROSE ||
                isCrossPlant(topType) || isPartialBlock(topType)) continue;

            TextureCell lodTop, lodSide, lodBottom;
            getBlockTextures(topType, lodTop, lodSide, lodBottom);
            u8 tr, tg, tb;
            columnTint(topType, xi, zi, tr, tg, tb);

            const int u0 = lodTop.col * 16, v0 = lodTop.row * 16, u1 = u0 + 16, v1 = v0 + 16;
            const v16 h = (v16)(qh * 4096.0f), top = 2048;
            b.mtxPush();
            b.mtxTrans((int)(lx * 4096.0f), (int)(gridToWorldY(colMaxY) * 4096.0f), (int)(lz * 4096.0f));
            b.begin(GL_QUADS);
            b.color3b(tr, tg, tb);
            b.texCoordPacked(TEXTURE_PACK(inttot16(u0), inttot16(v1))); b.vertex(-h, top,  h);
            b.texCoordPacked(TEXTURE_PACK(inttot16(u1), inttot16(v1))); b.vertex( h, top,  h);
            b.texCoordPacked(TEXTURE_PACK(inttot16(u1), inttot16(v0))); b.vertex( h, top, -h);
            b.texCoordPacked(TEXTURE_PACK(inttot16(u0), inttot16(v0))); b.vertex(-h, top, -h);
            b.end();
            b.mtxPop(1);
        }
    }
    const u32 words = b.finish();
    freeLodList();
    if (b.overflowed() || words == 0) { lodDirty = false; lodCoarse = coarse; return words == 0; }   // vacia: no hay nada que dibujar
    u32* buf = new (std::nothrow) u32[1 + words];
    if (!buf) return false;
    memcpy(buf, s_dlScratch, (1 + words) * sizeof(u32));
    flushList(buf);
    dlLod = buf;
    dlLodVerts = (int)b.getVertexCount();
    s_dlTotalBytes += (int)((1 + words) * 4);
    lodDirty = false;
    lodCoarse = coarse;
    return true;
}

static u8 packLightWithSubtract(u8 sky, u8 blk, const ChunkManager* manager) {
    if (manager) {
        int adjustedSky = (int)sky - manager->getSkylightSubtract();
        sky = (adjustedSky < 0) ? 0 : (u8)adjustedSky;
    }
    return (sky << 4) | blk;
}

u8 Chunk::getFaceLightRaw(int nxi, int nyi, int nzi, const ChunkManager* manager) const {
    if (nyi < 0 || nyi >= SIZE_Y) return 0x00;

    if (nxi >= 0 && nxi < SIZE_X && nzi >= 0 && nzi < SIZE_Z) {
        return packLightWithSubtract(getSkyLight(nxi, nyi, nzi), getBlockLight(nxi, nyi, nzi), manager);
    }

    if (!manager) return 0xFF;

    float worldX = gridToWorldX(nxi);
    float worldY = gridToWorldY(nyi);
    float worldZ = gridToWorldZ(nzi);

    Chunk* nc = manager->getChunkAt(worldX, worldZ);
    if (!nc) return 0xFF;

    int lxi, lyi, lzi;
    if (!nc->worldToGrid(worldX, worldY, worldZ, lxi, lyi, lzi)) return 0xFF;

    return packLightWithSubtract(nc->getSkyLight(lxi, lyi, lzi), nc->getBlockLight(lxi, lyi, lzi), manager);
}

static void applyLight(u8 raw, u8 baseR, u8 baseG, u8 baseB, u8& outR, u8& outG, u8& outB) {
    u8 sky = (raw >> 4) & 0x0F;
    u8 blk = raw & 0x0F;
    u8 level = sky > blk ? sky : blk;
    bool warm = blk > sky;

    static const u8 LIGHT_CURVE[16] = {
        51, 55, 59, 63, 68, 74, 80, 88,
        96, 107, 119, 134, 153, 177, 210, 255
    };
    u8 factor = LIGHT_CURVE[level];

    // WorldProviderHell.generateLightBrightnessTable: el Nether tiene un mínimo de brillo de 0,1 en vez de 0,05; esta tabla es la
    // diferencia entre las dos curvas de la Beta (0,05 * (1 - g) en una escala de 0 a 255) y se suma a la curva de este juego
    if (s_dimension == -1) {
        static const u8 NETHER_BOOST[16] = { 13, 13, 12, 12, 12, 11, 11, 10, 10, 9, 9, 8, 6, 5, 3, 0 };
        unsigned int boosted = (unsigned int)factor + NETHER_BOOST[level];
        factor = boosted > 255 ? 255 : (u8)boosted;
    }

    u16 r = (u16)baseR * factor / 255;
    u16 g = (u16)baseG * factor / 255;
    u16 bl = (u16)baseB * factor / 255;

    if (warm) {
        u16 warmBoost = (255 - factor) / 4;
        r  = (r + warmBoost > 255) ? 255 : r + warmBoost;
        g  = (g + warmBoost/2 > 255) ? 255 : g + warmBoost/2;
        bl = (bl > warmBoost/2) ? bl - warmBoost/2 : 0;
    }

    outR = (u8)r; outG = (u8)g; outB = (u8)bl;
}

void Chunk::snowifyTreeTop(const ChunkManager* manager, int xi, int zi, int maxRadius, int baseY, int maxHeight) {
    int scanTopY = baseY + maxHeight;
    if (scanTopY >= SIZE_Y) scanTopY = SIZE_Y - 1;

    for (int dx = -maxRadius; dx <= maxRadius; dx++) {
        for (int dz = -maxRadius; dz <= maxRadius; dz++) {
            int nlXi = xi + dx;
            int nlZi = zi + dz;
            bool inThisChunk = (nlXi >= 0 && nlXi < SIZE_X && nlZi >= 0 && nlZi < SIZE_Z);

            float columnWorldX = gridToWorldX(nlXi);
            float columnWorldZ = gridToWorldZ(nlZi);

            // El bioma que importa es el de ESTA columna puntual, no
            // el del origen del árbol — una copa puede asomarse fuera
            // del límite de la taiga, o meterse adentro desde afuera.
            BiomeType columnBiome;
            if (inThisChunk) {
                columnBiome = biomeColumn[nlXi][nlZi];
            } else if (manager) {
                columnBiome = manager->getBiomeAt(columnWorldX, columnWorldZ);
            } else {
                continue;
            }
            if (columnBiome != BiomeType::TAIGA) continue;

            for (int ly = scanTopY; ly >= baseY; ly--) {
                float worldY = gridToWorldY(ly);
                BlockType foundType = BlockType::AIR;

                if (inThisChunk) {
                    if (ly < 0 || ly >= SIZE_Y) continue;
                    foundType = blocks[nlXi][ly][nlZi];
                } else if (manager) {
                    Chunk* neighbor = manager->getChunkAt(columnWorldX, columnWorldZ);
                    if (!neighbor) continue;
                    int nXi, nYi, nZi;
                    if (!neighbor->worldToGrid(columnWorldX, worldY, columnWorldZ, nXi, nYi, nZi)) continue;
                    foundType = neighbor->getBlockType(nXi, nYi, nZi);
                } else {
                    continue;
                }

                if (foundType == BlockType::AIR) continue;

                bool isTreeTop = (foundType == BlockType::SPRUCE_LEAVES || foundType == BlockType::SPRUCE_LOG ||
                                   foundType == BlockType::OAK_LEAVES   || foundType == BlockType::OAK_LOG ||
                                   foundType == BlockType::BIRCH_LEAVES || foundType == BlockType::BIRCH_LOG);

                if (isTreeTop) {
                    int snowLy = ly + 1;
                    float snowWorldY = gridToWorldY(snowLy);

                    if (inThisChunk) {
                        if (snowLy >= 0 && snowLy < SIZE_Y && blocks[nlXi][snowLy][nlZi] == BlockType::AIR) {
                            setBlockDirect(nlXi, snowLy, nlZi, BlockType::SNOW_LAYER, manager);
                        }
                    } else if (manager) {
                        Chunk* neighbor = manager->getChunkAt(columnWorldX, columnWorldZ);
                        if (neighbor) {
                            int nXi, nYi, nZi;
                            if (neighbor->worldToGrid(columnWorldX, snowWorldY, columnWorldZ, nXi, nYi, nZi)) {
                                if (neighbor->getBlockType(nXi, nYi, nZi) == BlockType::AIR) {
                                    neighbor->setBlockDirect(nXi, nYi, nZi, BlockType::SNOW_LAYER, manager);
                                }
                            }
                        }
                    }
                }
                break;
            }
        }
    }
}


static void emitSnowLayer(DisplayListBuilder& b, float lx, float ly, float lz,
                           TextureCell cell, u8 lightRaw) {
    int u0 = cell.col * 16, v0 = cell.row * 16;
    int u1 = u0 + 16, v1 = v0 + 16;

    u8 r, g, bl; applyLight(lightRaw, 255, 255, 255, r, g, bl);

    const float topY = -0.5f + (2.0f / 16.0f); // 2/16 de bloque de alto

    b.begin(GL_QUADS);
    b.color3b(r, g, bl);

    b.texCoordPacked(TEXTURE_PACK(inttot16(u0), inttot16(v1))); b.vertex(V(lx-0.5f), V(ly+topY), V(lz+0.5f));
    b.texCoordPacked(TEXTURE_PACK(inttot16(u1), inttot16(v1))); b.vertex(V(lx+0.5f), V(ly+topY), V(lz+0.5f));
    b.texCoordPacked(TEXTURE_PACK(inttot16(u1), inttot16(v0))); b.vertex(V(lx+0.5f), V(ly+topY), V(lz-0.5f));
    b.texCoordPacked(TEXTURE_PACK(inttot16(u0), inttot16(v0))); b.vertex(V(lx-0.5f), V(ly+topY), V(lz-0.5f));

    b.texCoordPacked(TEXTURE_PACK(inttot16(u0), inttot16(v1))); b.vertex(V(lx-0.5f), V(ly-0.5f), V(lz+0.5f));
    b.texCoordPacked(TEXTURE_PACK(inttot16(u1), inttot16(v1))); b.vertex(V(lx+0.5f), V(ly-0.5f), V(lz+0.5f));
    b.texCoordPacked(TEXTURE_PACK(inttot16(u1), inttot16(v0))); b.vertex(V(lx+0.5f), V(ly+topY), V(lz+0.5f));
    b.texCoordPacked(TEXTURE_PACK(inttot16(u0), inttot16(v0))); b.vertex(V(lx-0.5f), V(ly+topY), V(lz+0.5f));

    b.texCoordPacked(TEXTURE_PACK(inttot16(u0), inttot16(v1))); b.vertex(V(lx+0.5f), V(ly-0.5f), V(lz-0.5f));
    b.texCoordPacked(TEXTURE_PACK(inttot16(u1), inttot16(v1))); b.vertex(V(lx-0.5f), V(ly-0.5f), V(lz-0.5f));
    b.texCoordPacked(TEXTURE_PACK(inttot16(u1), inttot16(v0))); b.vertex(V(lx-0.5f), V(ly+topY), V(lz-0.5f));
    b.texCoordPacked(TEXTURE_PACK(inttot16(u0), inttot16(v0))); b.vertex(V(lx+0.5f), V(ly+topY), V(lz-0.5f));

    b.texCoordPacked(TEXTURE_PACK(inttot16(u0), inttot16(v1))); b.vertex(V(lx-0.5f), V(ly-0.5f), V(lz-0.5f));
    b.texCoordPacked(TEXTURE_PACK(inttot16(u1), inttot16(v1))); b.vertex(V(lx-0.5f), V(ly-0.5f), V(lz+0.5f));
    b.texCoordPacked(TEXTURE_PACK(inttot16(u1), inttot16(v0))); b.vertex(V(lx-0.5f), V(ly+topY), V(lz+0.5f));
    b.texCoordPacked(TEXTURE_PACK(inttot16(u0), inttot16(v0))); b.vertex(V(lx-0.5f), V(ly+topY), V(lz-0.5f));

    b.texCoordPacked(TEXTURE_PACK(inttot16(u0), inttot16(v1))); b.vertex(V(lx+0.5f), V(ly-0.5f), V(lz+0.5f));
    b.texCoordPacked(TEXTURE_PACK(inttot16(u1), inttot16(v1))); b.vertex(V(lx+0.5f), V(ly-0.5f), V(lz-0.5f));
    b.texCoordPacked(TEXTURE_PACK(inttot16(u1), inttot16(v0))); b.vertex(V(lx+0.5f), V(ly+topY), V(lz-0.5f));
    b.texCoordPacked(TEXTURE_PACK(inttot16(u0), inttot16(v0))); b.vertex(V(lx+0.5f), V(ly+topY), V(lz+0.5f));

    b.end();
}

// Estas tres calcan exactamente el winding/UV de Cube::draw y de
// drawCrossQuad/drawCenteredCrossQuad, pero escriben a un
// DisplayListBuilder en vez de llamar a GL directo, y usan
// coordenadas LOCALES dentro de la banda (lx,ly,lz) en vez de
// coordenadas de mundo — así los vértices siempre caben en v16.
static void emitCube(DisplayListBuilder& b, float lx, float ly, float lz,
                      TextureCell topCell, TextureCell sideCell, TextureCell bottomCell,
                      FaceMask faces, u8 tintR, u8 tintG, u8 tintB, bool tintAllFaces,
                      u8 lightSouth, u8 lightNorth, u8 lightWest, u8 lightEast,
                      u8 lightBottom, u8 lightTop, float topSink = 0.0f,
                      int frontDir = -1, TextureCell frontCell = {0, 0},
                      float sideInset = 0.0f, const TextureCell* sideOverride = nullptr) {
    // Coordenadas en punto fijo 4.12 (4096 = 1 bloque), una sola vez por bloque
    const v16 H = 2048; // medio bloque
    v16 cx = (v16)((int)lx << 12);
    v16 cy = (v16)((int)ly << 12);
    v16 cz = (v16)((int)lz << 12);
    v16 x0 = cx - H, x1 = cx + H;
    v16 y0 = cy - H, y1 = cy + H;
    v16 z0 = cz - H, z1 = cz + H;
    v16 yTop = (topSink != 0.0f) ? (v16)(y1 - (v16)(topSink * 4096.0f)) : y1;

    // Costados metidos hacia adentro (cactus): se trasladan, no se achican
    const v16 inset = (v16)(sideInset * 4096.0f);
    const v16 zS = z1 - inset, zN = z0 + inset, xW = x0 + inset, xE = x1 - inset;

    u8 sideR = tintAllFaces ? tintR : 255;
    u8 sideG = tintAllFaces ? tintG : 255;
    u8 sideB = tintAllFaces ? tintB : 255;

    // sideOverride (cofre): una textura distinta para cada cara lateral, en el orden sur, norte, oeste, este
    const TextureCell cSouth = sideOverride ? sideOverride[0] : ((frontDir == 0) ? frontCell : sideCell);
    const TextureCell cNorth = sideOverride ? sideOverride[1] : ((frontDir == 1) ? frontCell : sideCell);
    const TextureCell cWest  = sideOverride ? sideOverride[2] : ((frontDir == 2) ? frontCell : sideCell);
    const TextureCell cEast  = sideOverride ? sideOverride[3] : ((frontDir == 3) ? frontCell : sideCell);

    b.begin(GL_QUADS);

    if (faces.south) {
        u8 r, g, bl; applyLight(lightSouth, sideR, sideG, sideB, r, g, bl);
        b.color3b(r, g, bl);
        b.texCoordPacked(packTexCoordCell(cSouth, 0)); b.vertex(x0, y0, zS);
        b.texCoordPacked(packTexCoordCell(cSouth, 1)); b.vertex(x1, y0, zS);
        b.texCoordPacked(packTexCoordCell(cSouth, 2)); b.vertex(x1, y1, zS);
        b.texCoordPacked(packTexCoordCell(cSouth, 3)); b.vertex(x0, y1, zS);
    }
    if (faces.north) {
        u8 r, g, bl; applyLight(lightNorth, sideR, sideG, sideB, r, g, bl);
        b.color3b(r, g, bl);
        b.texCoordPacked(packTexCoordCell(cNorth, 0)); b.vertex(x1, y0, zN);
        b.texCoordPacked(packTexCoordCell(cNorth, 1)); b.vertex(x0, y0, zN);
        b.texCoordPacked(packTexCoordCell(cNorth, 2)); b.vertex(x0, y1, zN);
        b.texCoordPacked(packTexCoordCell(cNorth, 3)); b.vertex(x1, y1, zN);
    }
    if (faces.west) {
        u8 r, g, bl; applyLight(lightWest, sideR, sideG, sideB, r, g, bl);
        b.color3b(r, g, bl);
        b.texCoordPacked(packTexCoordCell(cWest, 0)); b.vertex(xW, y0, z0);
        b.texCoordPacked(packTexCoordCell(cWest, 1)); b.vertex(xW, y0, z1);
        b.texCoordPacked(packTexCoordCell(cWest, 2)); b.vertex(xW, y1, z1);
        b.texCoordPacked(packTexCoordCell(cWest, 3)); b.vertex(xW, y1, z0);
    }
    if (faces.east) {
        u8 r, g, bl; applyLight(lightEast, sideR, sideG, sideB, r, g, bl);
        b.color3b(r, g, bl);
        b.texCoordPacked(packTexCoordCell(cEast, 0)); b.vertex(xE, y0, z1);
        b.texCoordPacked(packTexCoordCell(cEast, 1)); b.vertex(xE, y0, z0);
        b.texCoordPacked(packTexCoordCell(cEast, 2)); b.vertex(xE, y1, z0);
        b.texCoordPacked(packTexCoordCell(cEast, 3)); b.vertex(xE, y1, z1);
    }
    if (faces.bottom) {
        u8 r, g, bl; applyLight(lightBottom, sideR, sideG, sideB, r, g, bl);
        b.color3b(r, g, bl);
        b.texCoordPacked(packTexCoordCell(bottomCell, 0)); b.vertex(x0, y0, z0);
        b.texCoordPacked(packTexCoordCell(bottomCell, 1)); b.vertex(x1, y0, z0);
        b.texCoordPacked(packTexCoordCell(bottomCell, 2)); b.vertex(x1, y0, z1);
        b.texCoordPacked(packTexCoordCell(bottomCell, 3)); b.vertex(x0, y0, z1);
    }
    b.end();

    if (faces.top) {
        u8 r, g, bl; applyLight(lightTop, tintR, tintG, tintB, r, g, bl);
        b.begin(GL_QUADS);
        b.color3b(r, g, bl);
        b.texCoordPacked(packTexCoordCell(topCell, 0)); b.vertex(x0, yTop, z1);
        b.texCoordPacked(packTexCoordCell(topCell, 1)); b.vertex(x1, yTop, z1);
        b.texCoordPacked(packTexCoordCell(topCell, 2)); b.vertex(x1, yTop, z0);
        b.texCoordPacked(packTexCoordCell(topCell, 3)); b.vertex(x0, yTop, z0);
        b.end();
    }
}

static void emitCrossQuad(DisplayListBuilder& b, float lx, float ly, float lz,
                           TextureCell cell, u8 tintR, u8 tintG, u8 tintB, u8 lightRaw) {
    int u0 = cell.col * 16, v0 = cell.row * 16;
    int u1 = u0 + 16, v1 = v0 + 16;

    u8 r, g, bl; applyLight(lightRaw, tintR, tintG, tintB, r, g, bl);

    b.begin(GL_QUADS);
    b.color3b(r, g, bl);

    b.texCoordPacked(TEXTURE_PACK(inttot16(u0), inttot16(v1))); b.vertex(V(lx-0.5f), V(ly-0.5f), V(lz-0.5f));
    b.texCoordPacked(TEXTURE_PACK(inttot16(u1), inttot16(v1))); b.vertex(V(lx+0.5f), V(ly-0.5f), V(lz+0.5f));
    b.texCoordPacked(TEXTURE_PACK(inttot16(u1), inttot16(v0))); b.vertex(V(lx+0.5f), V(ly+0.5f), V(lz+0.5f));
    b.texCoordPacked(TEXTURE_PACK(inttot16(u0), inttot16(v0))); b.vertex(V(lx-0.5f), V(ly+0.5f), V(lz-0.5f));

    b.texCoordPacked(TEXTURE_PACK(inttot16(u0), inttot16(v1))); b.vertex(V(lx-0.5f), V(ly-0.5f), V(lz+0.5f));
    b.texCoordPacked(TEXTURE_PACK(inttot16(u1), inttot16(v1))); b.vertex(V(lx+0.5f), V(ly-0.5f), V(lz-0.5f));
    b.texCoordPacked(TEXTURE_PACK(inttot16(u1), inttot16(v0))); b.vertex(V(lx+0.5f), V(ly+0.5f), V(lz-0.5f));
    b.texCoordPacked(TEXTURE_PACK(inttot16(u0), inttot16(v0))); b.vertex(V(lx-0.5f), V(ly+0.5f), V(lz+0.5f));
    b.end();
}

static void emitCenteredCrossQuad(DisplayListBuilder& b, float lx, float ly, float lz,
                                   TextureCell cell, u8 tintR, u8 tintG, u8 tintB, u8 lightRaw) {
    int u0 = cell.col * 16, v0 = cell.row * 16;
    int u1 = u0 + 16, v1 = v0 + 16;

    u8 r, g, bl; applyLight(lightRaw, tintR, tintG, tintB, r, g, bl);

    b.begin(GL_QUADS);
    b.color3b(r, g, bl);

    b.texCoordPacked(TEXTURE_PACK(inttot16(u0), inttot16(v1))); b.vertex(V(lx-0.5f), V(ly-0.5f), V(lz));
    b.texCoordPacked(TEXTURE_PACK(inttot16(u1), inttot16(v1))); b.vertex(V(lx+0.5f), V(ly-0.5f), V(lz));
    b.texCoordPacked(TEXTURE_PACK(inttot16(u1), inttot16(v0))); b.vertex(V(lx+0.5f), V(ly+0.5f), V(lz));
    b.texCoordPacked(TEXTURE_PACK(inttot16(u0), inttot16(v0))); b.vertex(V(lx-0.5f), V(ly+0.5f), V(lz));

    b.texCoordPacked(TEXTURE_PACK(inttot16(u0), inttot16(v1))); b.vertex(V(lx), V(ly-0.5f), V(lz-0.5f));
    b.texCoordPacked(TEXTURE_PACK(inttot16(u1), inttot16(v1))); b.vertex(V(lx), V(ly-0.5f), V(lz+0.5f));
    b.texCoordPacked(TEXTURE_PACK(inttot16(u1), inttot16(v0))); b.vertex(V(lx), V(ly+0.5f), V(lz+0.5f));
    b.texCoordPacked(TEXTURE_PACK(inttot16(u0), inttot16(v0))); b.vertex(V(lx), V(ly+0.5f), V(lz-0.5f));
    b.end();
}

// Modelo de la antorcha de Beta 1.7.3 (RenderBlocks.renderTorchAtAngle): 5 quads, siempre a brillo completo
// (renderBlockTorch fuerza brillo 1.0 en los bloques que emiten luz).
static void emitTorch(DisplayListBuilder& b, float lx, float ly, float lz, int meta, TextureCell cell = TextureCell{0, 5}, u8 cr = 255, u8 cg = 255, u8 cb = 255) {
    Torch::Vtx v[Torch::MODEL_VERTS];
    Torch::buildModel(meta, v);   // cell: la textura (por defecto la de getBlockTextures(TORCH)); cr, cg, cb: el color

    b.begin(GL_QUADS);
    b.color3b(cr, cg, cb);
    for (int i = 0; i < Torch::MODEL_VERTS; i++) {
        float px = lx + v[i].x, py = ly + v[i].y, pz = lz + v[i].z;
        // VERTEX10 llega hasta +7.98: en la última columna la parte de la antorcha de pared que se mete dentro
        // del bloque de apoyo (oculta) se recorta en vez de dar la vuelta
        if (px > 7.98f) px = 7.98f;
        if (pz > 7.98f) pz = 7.98f;
        b.texCoordPacked(TEXTURE_PACK(floattot16((float)(cell.col * 16) + v[i].u), floattot16((float)(cell.row * 16) + v[i].v)));
        b.vertex(V(px), V(py), V(pz));
    }
    b.end();
}

// ---------------------------------------------------------------------------------------------------------
// Líquidos de Beta 1.7.3
// ---------------------------------------------------------------------------------------------------------

// BlockFluid.shouldSideBeRendered: una cara lateral o inferior se dibuja si el vecino no es opaco
static inline bool hidesLiquidFace(BlockType t) { return isLightOpaque(t) && !isLiquidBlock(t); }

static u8 brighterRaw(u8 a, u8 b) {
    int la = ((a >> 4) > (a & 15)) ? (a >> 4) : (a & 15);
    int lb = ((b >> 4) > (b & 15)) ? (b >> 4) : (b & 15);
    return lb > la ? b : a;
}

int Chunk::metaNear(int xi, int yi, int zi, const ChunkManager* manager) const {
    if (yi < 0 || yi >= SIZE_Y) return 0;
    if (xi >= 0 && xi < SIZE_X && zi >= 0 && zi < SIZE_Z) return liquidMetaGet(xi, yi, zi);
    if (!manager) return 0;
    float wx = gridToWorldX(xi), wy = gridToWorldY(yi), wz = gridToWorldZ(zi);
    Chunk* n = manager->getChunkAt(wx, wz);
    int nx, ny, nz;
    if (!n || !n->isGenerationComplete() || !n->worldToGrid(wx, wy, wz, nx, ny, nz)) return 0;
    return n->liquidMetaGet(nx, ny, nz);
}

// Escritura de una celda por la simulación de líquidos. Misma masa (quieta <-> corriente, cambio de nivel): solo cambia
// la altura, no hay que recalcular caras ni luz. Cualquier otro cambio pasa por setBlockDirect.
void Chunk::putLiquidCell(int xi, int yi, int zi, BlockType type, int meta, const ChunkManager* manager) {
    if (xi < 0 || xi >= SIZE_X || yi < 0 || yi >= SIZE_Y || zi < 0 || zi >= SIZE_Z) return;
    BlockType oldType = blocks[xi][yi][zi];

    if (oldType == BlockType::FIRE && type == BlockType::FIRE) { // la edad del fuego cambia: ni geometría ni luz
        liquidMetaSet(xi, yi, zi, meta);
        modified = true;
        return;
    }

    if (isLiquidBlock(oldType) && isLiquidBlock(type) && liquidMaterial(oldType) == liquidMaterial(type)) {
        blocks[xi][yi][zi] = type;
        liquidMetaSet(xi, yi, zi, meta);
        modified = true;
    } else {
        setBlockDirect(xi, yi, zi, type, manager);
        liquidMetaSet(xi, yi, zi, meta);
        if (manager && generationComplete && isLightOpaque(oldType) && !isLightOpaque(type)) {
            // la lava (opaca a la luz) se fue: el cielo vuelve a entrar
            float wx = gridToWorldX(xi), wy = gridToWorldY(yi), wz = gridToWorldZ(zi);
            manager->propagateSkyColumn(wx, wz);
            manager->relightNewAirCell(wx, wy, wz);
        }
    }
    markBandRangeDirty(yi - 1, yi + 1);

    // la altura de las esquinas depende de las celdas vecinas, también las del chunk de al lado
    if (manager) {
        float wx = gridToWorldX(xi), wz = gridToWorldZ(zi);
        if (xi == 0)          { Chunk* n = manager->getChunkAt(wx - 1.0f, wz); if (n && n != this) n->markBandRangeDirty(yi - 1, yi + 1); }
        if (xi == SIZE_X - 1) { Chunk* n = manager->getChunkAt(wx + 1.0f, wz); if (n && n != this) n->markBandRangeDirty(yi - 1, yi + 1); }
        if (zi == 0)          { Chunk* n = manager->getChunkAt(wx, wz - 1.0f); if (n && n != this) n->markBandRangeDirty(yi - 1, yi + 1); }
        if (zi == SIZE_Z - 1) { Chunk* n = manager->getChunkAt(wx, wz + 1.0f); if (n && n != this) n->markBandRangeDirty(yi - 1, yi + 1); }
    }
}

void Chunk::notifyLiquidNeighbors(int xi, int yi, int zi, const ChunkManager* manager) {
    if (!manager || !generationComplete) return;
    manager->liquidBlockChanged(chunkX * SIZE_X + xi, yi, chunkZ * SIZE_Z + zi);
}

// RenderBlocks.renderBlockFluids + getFluidHeight. La superficie sigue el nivel de cada esquina. No hay animación
// en esta versión: textura fija, sin girar con la corriente. (lx, ly, lz) es el centro de la celda.
void Chunk::emitLiquidCell(DisplayListBuilder& b, int xi, int yi, int zi, float lx, float ly, float lz, const ChunkManager* manager) {
    const BlockType self = blocks[xi][yi][zi];
    const int mat = liquidMaterial(self);

    auto typeAt = [&](int x, int y, int z) { return typeNear(x, y, z, manager, BlockType::STONE); };
    auto faceVisible = [&](int x, int y, int z, bool top) {
        BlockType t = typeAt(x, y, z);
        if (liquidMaterial(t) == mat) return false;
        return top ? true : !hidesLiquidFace(t);
    };
    const bool showTop = faceVisible(xi, yi + 1, zi, true);
    const bool showBottom = faceVisible(xi, yi - 1, zi, false);
    const bool showSide[4] = { faceVisible(xi, yi, zi - 1, false), faceVisible(xi, yi, zi + 1, false),
                               faceVisible(xi - 1, yi, zi, false), faceVisible(xi + 1, yi, zi, false) };
    if (!showTop && !showBottom && !showSide[0] && !showSide[1] && !showSide[2] && !showSide[3]) return;

    // altura de la esquina cuyo bloque "propio" es (cx, cz): promedio de las 4 celdas que la tocan
    auto corner = [&](int cx, int cz) -> float {
        int count = 0; float sum = 0.0f;
        for (int k = 0; k < 4; k++) {
            int nx = cx - (k & 1), nz = cz - ((k >> 1) & 1);
            if (liquidMaterial(typeAt(nx, yi + 1, nz)) == mat) return 1.0f;
            BlockType t = typeAt(nx, yi, nz);
            if (liquidMaterial(t) != mat) {
                if (!isMaterialSolid(t)) { sum += 1.0f; count++; }
            } else {
                int m = metaNear(nx, yi, nz, manager);
                if (m >= 8 || m == 0) { sum += Liquid::percentAir(m) * 10.0f; count += 10; }
                sum += Liquid::percentAir(m);
                count++;
            }
        }
        return 1.0f - sum / (float)count;
    };
    const float h00 = corner(xi, zi), h01 = corner(xi, zi + 1), h11 = corner(xi + 1, zi + 1), h10 = corner(xi + 1, zi);

    TextureCell tTop, tSide, tBottom;
    getBlockTextures(self, tTop, tSide, tBottom);
    const float u0 = (float)(tSide.col * 16), v0 = (float)(tSide.row * 16);
    const float u1 = u0 + 15.99f, vEnd = v0 + 15.99f;

    u8 lTop = 0, lBot = 0, lSide[4] = { 0, 0, 0, 0 };
    if (!b.isCounting()) { // la luz solo cambia el color, no la cantidad de palabras
        const int dx[4] = { 0, 0, -1, 1 }, dz[4] = { -1, 1, 0, 0 };
        lTop = brighterRaw(getFaceLightRaw(xi, yi, zi, manager), getFaceLightRaw(xi, yi + 1, zi, manager));
        lBot = brighterRaw(getFaceLightRaw(xi, yi - 1, zi, manager), getFaceLightRaw(xi, yi, zi, manager));
        for (int s = 0; s < 4; s++)
            lSide[s] = brighterRaw(getFaceLightRaw(xi + dx[s], yi, zi + dz[s], manager), getFaceLightRaw(xi + dx[s], yi + 1, zi + dz[s], manager));
    }

    auto color = [&](u8 raw) { u8 r, g, bl; applyLight(raw, 255, 255, 255, r, g, bl); b.color3b(r, g, bl); };
    auto vtx = [&](float px, float py, float pz, float u, float v) {
        b.texCoordPacked(TEXTURE_PACK(floattot16(u), floattot16(v)));
        b.vertex(V(px), V(py), V(pz));
    };

    const float x0 = lx - 0.5f, x1 = lx + 0.5f, z0 = lz - 0.5f, z1 = lz + 0.5f, yb = ly - 0.5f;

    b.polyFormat(isWaterBlock(self) ? POLYFMT_WATER : POLYFMT_BOTHSIDES);
    b.begin(GL_QUADS);

    if (showTop) {
        color(lTop);
        vtx(x0, yb + h00, z0, u0, v0);
        vtx(x0, yb + h01, z1, u0, v0 + 16.0f);
        vtx(x1, yb + h11, z1, u0 + 16.0f, v0 + 16.0f);
        vtx(x1, yb + h10, z0, u0 + 16.0f, v0);
    }
    if (showBottom) {
        color(lBot);
        vtx(x0, yb, z0, u0, v0);
        vtx(x1, yb, z0, u0 + 16.0f, v0);
        vtx(x1, yb, z1, u0 + 16.0f, v0 + 16.0f);
        vtx(x0, yb, z1, u0, v0 + 16.0f);
    }

    // lados: -Z, +Z, -X, +X. Cada cara va de la altura de una esquina a la de la otra; la textura no se estira
    // (el origen de v baja según qué tanto falta para llenar la celda)
    for (int s = 0; s < 4; s++) {
        if (!showSide[s]) continue;
        float ax, az, bx, bz, hA, hB;
        if (s == 0)      { ax = x0; az = z0; bx = x1; bz = z0; hA = h00; hB = h10; }
        else if (s == 1) { ax = x1; az = z1; bx = x0; bz = z1; hA = h11; hB = h01; }
        else if (s == 2) { ax = x0; az = z1; bx = x0; bz = z0; hA = h01; hB = h00; }
        else             { ax = x1; az = z0; bx = x1; bz = z1; hA = h10; hB = h11; }
        color(lSide[s]);
        vtx(ax, yb + hA, az, u0, v0 + (1.0f - hA) * 16.0f);
        vtx(bx, yb + hB, bz, u1, v0 + (1.0f - hB) * 16.0f);
        vtx(bx, yb,      bz, u1, vEnd);
        vtx(ax, yb,      az, u0, vEnd);
    }

    b.end();
    b.polyFormat(POLYFMT_CULLED);
}

// Fuego de Beta 1.7.3 (RenderBlocks.renderBlockFire, ver FireModel.h). Sin animación: la primera capa es la celda (15,1) del
// atlas del terreno y la segunda la (15,2). Siempre a brillo completo (el fuego ilumina con 15).
void Chunk::emitFireCell(DisplayListBuilder& b, int xi, int yi, int zi, float lx, float ly, float lz, const ChunkManager* manager) {
    const int gx = chunkX * SIZE_X + xi, gy = yi, gz = chunkZ * SIZE_Z + zi;
    auto cellAt = [&](int ax, int ay, int az) { return typeNear(xi + (ax - gx), yi + (ay - gy), zi + (az - gz), manager, BlockType::STONE); };
    auto canCatch = [&](int ax, int ay, int az) { return fireEncourage(cellAt(ax, ay, az)) > 0; };
    auto isNormal = [&](int ax, int ay, int az) { return isNormalCube(cellAt(ax, ay, az)); };
    const float ox = lx - 0.5f, oy = ly - 0.5f, oz = lz - 0.5f; // esquina mínima de la celda

    b.polyFormat(POLYFMT_BOTHSIDES);
    b.begin(GL_QUADS);
    b.color3b(255, 255, 255);
    fireModel(gx, gy, gz, 31, canCatch, isNormal, [&](double x, double y, double z, double u, double v) {
        float px = ox + (float)(x - gx), py = oy + (float)(y - gy), pz = oz + (float)(z - gz);
        if (px > 7.98f) px = 7.98f; // VERTEX10 llega hasta +7,98 (igual que la antorcha)
        if (pz > 7.98f) pz = 7.98f;
        b.texCoordPacked(TEXTURE_PACK(floattot16((float)(u * 256.0)), floattot16((float)(v * 256.0))));
        b.vertex(V(px), V(py), V(pz));
    });
    b.end();
    b.polyFormat(POLYFMT_CULLED);
}

// Portal de Beta 1.7.3 (BlockPortal): un plano de 4/16 de grosor que corre a lo largo del marco, con la misma textura en sus dos
// caras (setBlockBoundsBasedOnState y shouldSideBeRendered: solo se dibujan las dos caras grandes). Sin animación: un cuadro.
void Chunk::emitPortalCell(DisplayListBuilder& b, int xi, int yi, int zi, float lx, float ly, float lz, const ChunkManager* manager) {
    // si hay portal en x-1 o x+1 el plano corre a lo largo de X (delgado en Z); si no, a lo largo de Z (delgado en X)
    const bool alongX = typeNear(xi - 1, yi, zi, manager, BlockType::AIR) == BlockType::PORTAL ||
                        typeNear(xi + 1, yi, zi, manager, BlockType::AIR) == BlockType::PORTAL;

    TextureCell tTop, tSide, tBottom;
    getBlockTextures(BlockType::PORTAL, tTop, tSide, tBottom);
    const float u0 = (float)(tSide.col * 16), v0 = (float)(tSide.row * 16), u1 = u0 + 16.0f, v1 = v0 + 16.0f;

    u8 r = 255, g = 255, bl = 255;
    if (!b.isCounting()) applyLight(getFaceLightRaw(xi, yi, zi, manager), 255, 255, 255, r, g, bl);

    auto vtx = [&](float px, float py, float pz, float u, float v) {
        if (px > 7.98f) px = 7.98f; // VERTEX10 llega hasta +7,98
        if (pz > 7.98f) pz = 7.98f;
        b.texCoordPacked(TEXTURE_PACK(floattot16(u), floattot16(v)));
        b.vertex(V(px), V(py), V(pz));
    };

    const float t = 2.0f / 16.0f; // 0,5 +- 2/16 a cada lado del centro de la celda
    b.polyFormat(POLYFMT_BOTHSIDES);
    b.begin(GL_QUADS);
    b.color3b(r, g, bl);
    for (int side = -1; side <= 1; side += 2) {
        if (alongX) {
            const float z = lz + t * side;
            vtx(lx - 0.5f, ly - 0.5f, z, u0, v1);
            vtx(lx + 0.5f, ly - 0.5f, z, u1, v1);
            vtx(lx + 0.5f, ly + 0.5f, z, u1, v0);
            vtx(lx - 0.5f, ly + 0.5f, z, u0, v0);
        } else {
            const float x = lx + t * side;
            vtx(x, ly - 0.5f, lz - 0.5f, u0, v1);
            vtx(x, ly - 0.5f, lz + 0.5f, u1, v1);
            vtx(x, ly + 0.5f, lz + 0.5f, u1, v0);
            vtx(x, ly + 0.5f, lz - 0.5f, u0, v0);
        }
    }
    b.end();
    b.polyFormat(POLYFMT_CULLED);
}

// Caja parcial (puerta, trampilla, cama): bd = {minX, minY, minZ, maxX, maxY, maxZ} dentro de la celda (0 a 1). Las coordenadas de textura
// salen de la posicion de cada vertice, asi una hoja delgada usa solo la franja de la textura que le toca (como RenderBlocks).
// skipMask: bit n = no dibujar la cara n (0 abajo, 1 arriba, 2 norte -Z, 3 sur +Z, 4 oeste -X, 5 este +X). topUV: UV propias de la cara de arriba.
static void emitPartialBox(DisplayListBuilder& b, float lx, float ly, float lz, const float bd[6],
                           const TextureCell cells[6], const bool flip[6], int skipMask, u8 lightRaw, const signed char* topUV) {
    u8 r, g, bl; applyLight(lightRaw, 255, 255, 255, r, g, bl);
    const float x0 = lx - 0.5f + bd[0], x1 = lx - 0.5f + bd[3];
    const float y0 = ly - 0.5f + bd[1], y1 = ly - 0.5f + bd[4];
    const float z0 = lz - 0.5f + bd[2], z1 = lz - 0.5f + bd[5];
    auto uv = [&](int f, float uf, float vf) -> u32 {
        if (flip[f]) uf = 1.0f - uf;
        return TEXTURE_PACK(floattot16((float)(cells[f].col * 16) + uf * 16.0f), floattot16((float)(cells[f].row * 16) + vf * 16.0f));
    };
    b.begin(GL_QUADS);
    b.color3b(r, g, bl);
    if (!(skipMask & 1)) {   // abajo
        b.texCoordPacked(uv(0, bd[0], 1.0f - bd[2])); b.vertex(V(x0), V(y0), V(z0));
        b.texCoordPacked(uv(0, bd[3], 1.0f - bd[2])); b.vertex(V(x1), V(y0), V(z0));
        b.texCoordPacked(uv(0, bd[3], 1.0f - bd[5])); b.vertex(V(x1), V(y0), V(z1));
        b.texCoordPacked(uv(0, bd[0], 1.0f - bd[5])); b.vertex(V(x0), V(y0), V(z1));
    }
    if (!(skipMask & 2)) {   // arriba
        if (topUV) {   // esquinas (maxX,maxZ), (maxX,minZ), (minX,minZ), (minX,maxZ)
            b.texCoordPacked(uv(1, (float)topUV[6], (float)topUV[7])); b.vertex(V(x0), V(y1), V(z1));
            b.texCoordPacked(uv(1, (float)topUV[0], (float)topUV[1])); b.vertex(V(x1), V(y1), V(z1));
            b.texCoordPacked(uv(1, (float)topUV[2], (float)topUV[3])); b.vertex(V(x1), V(y1), V(z0));
            b.texCoordPacked(uv(1, (float)topUV[4], (float)topUV[5])); b.vertex(V(x0), V(y1), V(z0));
        } else {
            b.texCoordPacked(uv(1, bd[0], bd[5])); b.vertex(V(x0), V(y1), V(z1));
            b.texCoordPacked(uv(1, bd[3], bd[5])); b.vertex(V(x1), V(y1), V(z1));
            b.texCoordPacked(uv(1, bd[3], bd[2])); b.vertex(V(x1), V(y1), V(z0));
            b.texCoordPacked(uv(1, bd[0], bd[2])); b.vertex(V(x0), V(y1), V(z0));
        }
    }
    if (!(skipMask & 4)) {   // norte (-Z)
        b.texCoordPacked(uv(2, 1.0f - bd[3], 1.0f - bd[1])); b.vertex(V(x1), V(y0), V(z0));
        b.texCoordPacked(uv(2, 1.0f - bd[0], 1.0f - bd[1])); b.vertex(V(x0), V(y0), V(z0));
        b.texCoordPacked(uv(2, 1.0f - bd[0], 1.0f - bd[4])); b.vertex(V(x0), V(y1), V(z0));
        b.texCoordPacked(uv(2, 1.0f - bd[3], 1.0f - bd[4])); b.vertex(V(x1), V(y1), V(z0));
    }
    if (!(skipMask & 8)) {   // sur (+Z)
        b.texCoordPacked(uv(3, bd[0], 1.0f - bd[1])); b.vertex(V(x0), V(y0), V(z1));
        b.texCoordPacked(uv(3, bd[3], 1.0f - bd[1])); b.vertex(V(x1), V(y0), V(z1));
        b.texCoordPacked(uv(3, bd[3], 1.0f - bd[4])); b.vertex(V(x1), V(y1), V(z1));
        b.texCoordPacked(uv(3, bd[0], 1.0f - bd[4])); b.vertex(V(x0), V(y1), V(z1));
    }
    if (!(skipMask & 16)) {   // oeste (-X)
        b.texCoordPacked(uv(4, bd[2], 1.0f - bd[1])); b.vertex(V(x0), V(y0), V(z0));
        b.texCoordPacked(uv(4, bd[5], 1.0f - bd[1])); b.vertex(V(x0), V(y0), V(z1));
        b.texCoordPacked(uv(4, bd[5], 1.0f - bd[4])); b.vertex(V(x0), V(y1), V(z1));
        b.texCoordPacked(uv(4, bd[2], 1.0f - bd[4])); b.vertex(V(x0), V(y1), V(z0));
    }
    if (!(skipMask & 32)) {   // este (+X)
        b.texCoordPacked(uv(5, 1.0f - bd[5], 1.0f - bd[1])); b.vertex(V(x1), V(y0), V(z1));
        b.texCoordPacked(uv(5, 1.0f - bd[2], 1.0f - bd[1])); b.vertex(V(x1), V(y0), V(z0));
        b.texCoordPacked(uv(5, 1.0f - bd[2], 1.0f - bd[4])); b.vertex(V(x1), V(y1), V(z0));
        b.texCoordPacked(uv(5, 1.0f - bd[5], 1.0f - bd[4])); b.vertex(V(x1), V(y1), V(z1));
    }
    b.end();
}

// Caja girada alrededor del eje vertical de la celda (el modelo del cartel: SignModel con la rotacion de TileEntitySignRenderer).
// bx = {minX, minY, minZ, maxX, maxY, maxZ}: X y Z relativos al centro de la celda, Y desde su base; yawDeg gira como glRotatef(yaw, 0, 1, 0).
// Las caras se dibujan con la misma orientacion de vertices que emitPartialBox; la textura de cada cara toma de la celda el trozo de su tamano.
static void emitYawBox(DisplayListBuilder& b, float lx, float ly, float lz, const float bx[6], float yawDeg, TextureCell cell, u8 lightRaw) {
    u8 r, g, bl; applyLight(lightRaw, 255, 255, 255, r, g, bl);
    const float a = yawDeg * (3.14159265f / 180.0f);
    const float sn = sinf(a), cs = cosf(a);
    const float yb = ly - 0.5f;
    auto P = [&](float x, float y, float z, float uf, float vf, float wF, float hF) {
        const float rx = x * cs + z * sn, rz = -x * sn + z * cs;
        b.texCoordPacked(TEXTURE_PACK(floattot16((float)(cell.col * 16) + uf * wF * 16.0f), floattot16((float)(cell.row * 16) + vf * hF * 16.0f)));
        b.vertex(V(lx + rx), V(yb + y), V(lz + rz));
    };
    const float x0 = bx[0], y0 = bx[1], z0 = bx[2], x1 = bx[3], y1 = bx[4], z1 = bx[5];
    const float dx = x1 - x0, dy = y1 - y0, dz = z1 - z0;
    b.begin(GL_QUADS);
    b.color3b(r, g, bl);
    // abajo
    P(x0, y0, z0, 0, 1, dx, dz); P(x1, y0, z0, 1, 1, dx, dz); P(x1, y0, z1, 1, 0, dx, dz); P(x0, y0, z1, 0, 0, dx, dz);
    // arriba
    P(x0, y1, z1, 0, 1, dx, dz); P(x1, y1, z1, 1, 1, dx, dz); P(x1, y1, z0, 1, 0, dx, dz); P(x0, y1, z0, 0, 0, dx, dz);
    // norte (-Z)
    P(x1, y0, z0, 0, 1, dx, dy); P(x0, y0, z0, 1, 1, dx, dy); P(x0, y1, z0, 1, 0, dx, dy); P(x1, y1, z0, 0, 0, dx, dy);
    // sur (+Z)
    P(x0, y0, z1, 0, 1, dx, dy); P(x1, y0, z1, 1, 1, dx, dy); P(x1, y1, z1, 1, 0, dx, dy); P(x0, y1, z1, 0, 0, dx, dy);
    // oeste (-X)
    P(x0, y0, z0, 0, 1, dz, dy); P(x0, y0, z1, 1, 1, dz, dy); P(x0, y1, z1, 1, 0, dz, dy); P(x0, y1, z0, 0, 0, dz, dy);
    // este (+X)
    P(x1, y0, z1, 0, 1, dz, dy); P(x1, y0, z0, 1, 1, dz, dy); P(x1, y1, z0, 1, 0, dz, dy); P(x1, y1, z1, 0, 0, dz, dy);
    b.end();
}

// ---------------------------------------------------------------------------------------------------------
// Redstone (RenderBlocks.renderBlockRedstoneWire, renderBlockLever, renderBlockRepeater y la antorcha de redstone)
// ---------------------------------------------------------------------------------------------------------
void Chunk::emitRedstoneCell(DisplayListBuilder& b, int xi, int yi, int zi, float lx, float ly, float lz, const ChunkManager* manager) {
    const BlockType type = blocks[xi][yi][zi];
    const int meta = liquidMetaGet(xi, yi, zi);
    const u8 lightRaw = packLightWithSubtract(getSkyLight(xi, yi, zi), getBlockLight(xi, yi, zi), manager);

    auto vtx = [&](float px, float py, float pz, float u, float v) {
        b.texCoordPacked(TEXTURE_PACK(floattot16(u), floattot16(v)));
        b.vertex(V(px), V(py), V(pz));
    };

    // ---- Antorcha de redstone: el mismo modelo de la antorcha, con su textura; encendida va a brillo completo ----
    if (isRedstoneTorch(type)) {
        const bool on = (type == BlockType::TORCH_REDSTONE_ON);
        const TextureCell cell = on ? TextureCell{3, 6} : TextureCell{3, 7};
        u8 r = 255, g = 255, bl = 255;
        if (!on) applyLight(lightRaw, 255, 255, 255, r, g, bl);
        b.polyFormat(POLYFMT_BOTHSIDES);
        emitTorch(b, lx, ly, lz, (meta >= 1 && meta <= 5) ? meta : 5, cell, r, g, bl);
        b.polyFormat(POLYFMT_CULLED);
        return;
    }

    // ---- Riel (RenderBlocks.renderBlockMinecartTrack): un cuadrado plano a 1/16 de altura; en las pendientes un lado sube 1 ----
    if (isRailBlock(type)) {
        int shape = meta;
        TextureCell cell = TextureCell{0, 8};
        if (type == BlockType::RAIL) { if (meta >= 6) cell = TextureCell{0, 7}; }
        else {
            shape &= 7;
            cell = (type == BlockType::RAIL_DETECTOR) ? TextureCell{3, 12} : ((meta & 8) ? TextureCell{3, 11} : TextureCell{3, 10});
        }
        const float u0 = (float)(cell.col * 16), u1 = u0 + 15.99f, v0 = (float)(cell.row * 16), v1 = v0 + 15.99f;
        const float ox = lx - 0.5f, oy = ly - 0.5f, oz = lz - 0.5f;
        float xa = ox + 1.0f, xb = ox + 1.0f, xc = ox, xd = ox;
        float za = oz, zb = oz + 1.0f, zc = oz + 1.0f, zd = oz;
        const float y0 = oy + 1.0f / 16.0f;
        float ya = y0, yb = y0, yc = y0, yd = y0;
        if (shape != 1 && shape != 2 && shape != 3 && shape != 7) {
            if (shape == 8) {
                xa = xb = ox; xc = xd = ox + 1.0f;
                za = zd = oz + 1.0f; zb = zc = oz;
            } else if (shape == 9) {
                xa = xd = ox; xb = xc = ox + 1.0f;
                za = zb = oz; zc = zd = oz + 1.0f;
            }
        } else {
            xa = xd = ox + 1.0f; xb = xc = ox;
            za = zb = oz + 1.0f; zc = zd = oz;
        }
        if (shape == 2 || shape == 4) { ya += 1.0f; yd += 1.0f; }
        else if (shape == 3 || shape == 5) { yb += 1.0f; yc += 1.0f; }
        u8 r, g, bl; applyLight(lightRaw, 255, 255, 255, r, g, bl);
        b.polyFormat(POLYFMT_BOTHSIDES);
        b.begin(GL_QUADS);
        b.color3b(r, g, bl);
        vtx(xa, ya, za, u1, v0);
        vtx(xb, yb, zb, u1, v1);
        vtx(xc, yc, zc, u0, v1);
        vtx(xd, yd, zd, u0, v0);
        b.end();
        b.polyFormat(POLYFMT_CULLED);
        return;
    }

    // ---- Cable ----
    if (type == BlockType::REDSTONE_WIRE) {
        auto tAt = [&](int dx, int dy, int dz) { return typeNear(xi + dx, yi + dy, zi + dz, manager, BlockType::STONE); };
        auto nrm = [&](int dx, int dy, int dz) { return isNormalCube(tAt(dx, dy, dz)); };
        auto provider = [&](int dx, int dy, int dz, int d) -> bool {   // BlockRedstoneWire.isPowerProviderOrWire
            const BlockType t = tAt(dx, dy, dz);
            if (t == BlockType::REDSTONE_WIRE) return true;
            if (t == BlockType::AIR) return false;
            if (t == BlockType::LEVER || t == BlockType::BUTTON || isPlateBlock(t) || isRedstoneTorch(t) || t == BlockType::RAIL_DETECTOR) return true;
            if (isRepeaterBlock(t)) {
                static const int OPP[4] = { 2, 3, 0, 1 };
                return d == OPP[metaNear(xi + dx, yi + dy, zi + dz, manager) & 3];
            }
            return false;
        };
        bool w = provider(-1, 0, 0, 1) || (!nrm(-1, 0, 0) && provider(-1, -1, 0, -1));
        bool e = provider(1, 0, 0, 3) || (!nrm(1, 0, 0) && provider(1, -1, 0, -1));
        bool n = provider(0, 0, -1, 2) || (!nrm(0, 0, -1) && provider(0, -1, -1, -1));
        bool s = provider(0, 0, 1, 0) || (!nrm(0, 0, 1) && provider(0, -1, 1, -1));
        const bool roofFree = !nrm(0, 1, 0);
        if (roofFree) {
            if (nrm(-1, 0, 0) && provider(-1, 1, 0, -1)) w = true;
            if (nrm(1, 0, 0) && provider(1, 1, 0, -1)) e = true;
            if (nrm(0, 0, -1) && provider(0, 1, -1, -1)) n = true;
            if (nrm(0, 0, 1) && provider(0, 1, 1, -1)) s = true;
        }

        const float f = (float)meta / 15.0f;
        float cr = f * 0.6f + 0.4f;
        if (meta == 0) cr = 0.3f;
        float cg = f * f * 0.7f - 0.5f, cb = f * f * 0.6f - 0.7f;
        if (cg < 0.0f) cg = 0.0f;
        if (cb < 0.0f) cb = 0.0f;
        u8 r1, g1, b1, r2, g2, b2;
        applyLight(lightRaw, (u8)(cr * 255.0f), (u8)(cg * 255.0f), (u8)(cb * 255.0f), r1, g1, b1);
        applyLight(lightRaw, 255, 255, 255, r2, g2, b2);

        const float ox = lx - 0.5f, oy = ly - 0.5f, oz = lz - 0.5f;
        const float U = 4.0f * 16.0f, Vt = 10.0f * 16.0f;   // terrain.png (164): la cruz; (165): la linea; una fila mas abajo, el borde claro
        int shape = 0;
        if ((w || e) && !n && !s) shape = 1;
        if ((n || s) && !e && !w) shape = 2;

        b.polyFormat(POLYFMT_BOTHSIDES);
        b.begin(GL_QUADS);
        {
            float x0 = ox, x1 = ox + 1.0f, z0 = oz, z1 = oz + 1.0f;
            float u0 = U, u1 = U + 16.0f, v0 = Vt, v1 = Vt + 16.0f;
            if (shape != 0) { u0 = U + 16.0f; u1 = U + 32.0f; }
            if (shape == 0 && (w || e || n || s)) {
                if (!w) { x0 += 5.0f / 16.0f; u0 += 5.0f; }
                if (!e) { x1 -= 5.0f / 16.0f; u1 -= 5.0f; }
                if (!n) { z0 += 5.0f / 16.0f; v0 += 5.0f; }
                if (!s) { z1 -= 5.0f / 16.0f; v1 -= 5.0f; }
            }
            const float y = oy + 0.015625f;
            for (int pass = 0; pass < 2; pass++) {
                const float dv = pass ? 16.0f : 0.0f;   // el borde claro va una fila mas abajo
                if (pass == 0) b.color3b(r1, g1, b1); else b.color3b(r2, g2, b2);
                if (shape == 2) {
                    vtx(x1, y, z1, u1, v1 + dv); vtx(x1, y, z0, u0, v1 + dv); vtx(x0, y, z0, u0, v0 + dv); vtx(x0, y, z1, u1, v0 + dv);
                } else {
                    vtx(x1, y, z1, u1, v1 + dv); vtx(x1, y, z0, u1, v0 + dv); vtx(x0, y, z0, u0, v0 + dv); vtx(x0, y, z1, u0, v1 + dv);
                }
            }
        }
        if (roofFree) {   // cables que suben por la pared de un bloque vecino
            const float wu0 = U + 16.0f, wu1 = U + 32.0f, wv0 = Vt, wv1 = Vt + 16.0f;
            const float top = oy + 1.0f, bot = oy;
            for (int pass = 0; pass < 2; pass++) {
                const float dv = pass ? 16.0f : 0.0f;
                if (pass == 0) b.color3b(r1, g1, b1); else b.color3b(r2, g2, b2);
                if (nrm(-1, 0, 0) && tAt(-1, 1, 0) == BlockType::REDSTONE_WIRE) {
                    const float x = ox + 0.015625f;
                    vtx(x, top, oz + 1.0f, wu1, wv0 + dv); vtx(x, bot, oz + 1.0f, wu0, wv0 + dv); vtx(x, bot, oz, wu0, wv1 + dv); vtx(x, top, oz, wu1, wv1 + dv);
                }
                if (nrm(1, 0, 0) && tAt(1, 1, 0) == BlockType::REDSTONE_WIRE) {
                    const float x = ox + 1.0f - 0.015625f;
                    vtx(x, bot, oz + 1.0f, wu0, wv1 + dv); vtx(x, top, oz + 1.0f, wu1, wv1 + dv); vtx(x, top, oz, wu1, wv0 + dv); vtx(x, bot, oz, wu0, wv0 + dv);
                }
                if (nrm(0, 0, -1) && tAt(0, 1, -1) == BlockType::REDSTONE_WIRE) {
                    const float z = oz + 0.015625f;
                    vtx(ox + 1.0f, bot, z, wu0, wv1 + dv); vtx(ox + 1.0f, top, z, wu1, wv1 + dv); vtx(ox, top, z, wu1, wv0 + dv); vtx(ox, bot, z, wu0, wv0 + dv);
                }
                if (nrm(0, 0, 1) && tAt(0, 1, 1) == BlockType::REDSTONE_WIRE) {
                    const float z = oz + 1.0f - 0.015625f;
                    vtx(ox + 1.0f, top, z, wu1, wv0 + dv); vtx(ox + 1.0f, bot, z, wu0, wv0 + dv); vtx(ox, bot, z, wu0, wv1 + dv); vtx(ox, top, z, wu1, wv1 + dv);
                }
            }
        }
        b.end();
        b.polyFormat(POLYFMT_CULLED);
        return;
    }

    // ---- Palanca: base de cobblestone y la manija inclinada ----
    if (type == BlockType::LEVER) {
        const int v = meta & 7;
        const bool on = (meta & 8) != 0;
        const float a = 0.25f, c = 3.0f / 16.0f;
        float bd[6];   // RenderBlocks.renderBlockLever: la caja de la base
        if (v == 5)      { bd[0] = 0.5f - c; bd[1] = 0.0f; bd[2] = 0.5f - a; bd[3] = 0.5f + c; bd[4] = c; bd[5] = 0.5f + a; }
        else if (v == 6) { bd[0] = 0.5f - a; bd[1] = 0.0f; bd[2] = 0.5f - c; bd[3] = 0.5f + a; bd[4] = c; bd[5] = 0.5f + c; }
        else if (v == 4) { bd[0] = 0.5f - c; bd[1] = 0.5f - a; bd[2] = 1.0f - c; bd[3] = 0.5f + c; bd[4] = 0.5f + a; bd[5] = 1.0f; }
        else if (v == 3) { bd[0] = 0.5f - c; bd[1] = 0.5f - a; bd[2] = 0.0f; bd[3] = 0.5f + c; bd[4] = 0.5f + a; bd[5] = c; }
        else if (v == 2) { bd[0] = 1.0f - c; bd[1] = 0.5f - a; bd[2] = 0.5f - c; bd[3] = 1.0f; bd[4] = 0.5f + a; bd[5] = 0.5f + c; }
        else             { bd[0] = 0.0f; bd[1] = 0.5f - a; bd[2] = 0.5f - c; bd[3] = c; bd[4] = 0.5f + a; bd[5] = 0.5f + c; }
        const TextureCell cobble = TextureCell{0, 1};
        TextureCell cells[6] = { cobble, cobble, cobble, cobble, cobble, cobble };
        bool flip[6] = { false, false, false, false, false, false };
        emitPartialBox(b, lx, ly, lz, bd, cells, flip, 0, lightRaw, nullptr);

        // la manija: 8 vertices, 2/16 de ancho y 10/16 de largo, girados como el original
        float px[8], py[8], pz[8];
        const float hw = 1.0f / 16.0f, hl = 10.0f / 16.0f;
        const float cx[8] = { -hw, hw, hw, -hw, -hw, hw, hw, -hw };
        const float cz[8] = { -hw, -hw, hw, hw, -hw, -hw, hw, hw };
        const float PI_F = 3.14159265f;
        auto rotX = [](float& y, float& z, float ang) { const float cs = cosf(ang), sn = sinf(ang); const float ny = y * cs + z * sn, nz = z * cs - y * sn; y = ny; z = nz; };
        auto rotY = [](float& x, float& z, float ang) { const float cs = cosf(ang), sn = sinf(ang); const float nx = x * cs + z * sn, nz = z * cs - x * sn; x = nx; z = nz; };
        for (int i = 0; i < 8; i++) {
            float x = cx[i], y = (i < 4) ? 0.0f : hl, z = cz[i];
            if (on) { z -= 1.0f / 16.0f; rotX(y, z, PI_F * 2.0f / 9.0f); }
            else    { z += 1.0f / 16.0f; rotX(y, z, -(PI_F * 2.0f / 9.0f)); }
            if (v == 6) rotY(x, z, PI_F * 0.5f);
            if (v < 5) {
                y -= 0.375f;
                rotX(y, z, PI_F * 0.5f);
                if (v == 3) rotY(x, z, PI_F);
                if (v == 2) rotY(x, z, PI_F * 0.5f);
                if (v == 1) rotY(x, z, -PI_F * 0.5f);
                y += 0.5f;
            } else {
                y += 2.0f / 16.0f;
            }
            px[i] = lx + x; py[i] = (ly - 0.5f) + y; pz[i] = lz + z;   // el centro de la celda en x y z; y desde la base
        }
        static const int FACE[6][4] = { {0, 1, 2, 3}, {7, 6, 5, 4}, {1, 0, 4, 5}, {2, 1, 5, 6}, {3, 2, 6, 7}, {0, 3, 7, 4} };
        const float U = 0.0f, Vt = 6.0f * 16.0f;   // terrain.png (96)
        u8 r, g, bl; applyLight(lightRaw, 255, 255, 255, r, g, bl);
        b.polyFormat(POLYFMT_BOTHSIDES);
        b.begin(GL_QUADS);
        b.color3b(r, g, bl);
        for (int fc = 0; fc < 6; fc++) {
            const float u0 = U + 7.0f, u1 = U + 9.0f - 0.01f, v0 = Vt + 6.0f, v1 = Vt + (fc < 2 ? 8.0f : 16.0f) - 0.01f;
            vtx(px[FACE[fc][0]], py[FACE[fc][0]], pz[FACE[fc][0]], u0, v1);
            vtx(px[FACE[fc][1]], py[FACE[fc][1]], pz[FACE[fc][1]], u1, v1);
            vtx(px[FACE[fc][2]], py[FACE[fc][2]], pz[FACE[fc][2]], u1, v0);
            vtx(px[FACE[fc][3]], py[FACE[fc][3]], pz[FACE[fc][3]], u0, v0);
        }
        b.end();
        b.polyFormat(POLYFMT_CULLED);
        return;
    }

    // ---- Repetidor: losa con la flecha y dos antorchitas ----
    if (isRepeaterBlock(type)) {
        const bool on = (type == BlockType::REPEATER_ON);
        const int dir = meta & 3, delay = (meta & 12) >> 2;
        float bd[6]; Partial::bounds(type, meta, bd);
        TextureCell cells[6];
        cells[0] = on ? TextureCell{3, 6} : TextureCell{3, 7};   // abajo: 99 / 115
        cells[1] = on ? TextureCell{3, 9} : TextureCell{3, 8};   // arriba: 147 / 131
        for (int i = 2; i < 6; i++) cells[i] = TextureCell{5, 0};
        bool flip[6] = { false, false, false, false, false, false };
        static const signed char TOP_UV[4][8] = {
            { 1, 1, 1, 0, 0, 0, 0, 1 },
            { 1, 0, 0, 0, 0, 1, 1, 1 },
            { 0, 0, 0, 1, 1, 1, 1, 0 },
            { 0, 1, 1, 1, 1, 0, 0, 0 },
        };
        emitPartialBox(b, lx, ly, lz, bd, cells, flip, 0, lightRaw, TOP_UV[dir]);

        static const float STEP[4] = { -0.0625f, 0.0625f, 0.1875f, 0.3125f };   // BlockRedstoneRepeater.field_22024_a
        float ax = 0.0f, az = 0.0f, bx = 0.0f, bz = 0.0f;
        switch (dir) {
            case 0: bz = -0.3125f; az = STEP[delay]; break;
            case 1: bx = 0.3125f;  ax = -STEP[delay]; break;
            case 2: bz = 0.3125f;  az = -STEP[delay]; break;
            default: bx = -0.3125f; ax = STEP[delay]; break;
        }
        const TextureCell tcell = on ? TextureCell{3, 6} : TextureCell{3, 7};
        u8 r = 255, g = 255, bl = 255;
        if (!on) applyLight(lightRaw, 255, 255, 255, r, g, bl);
        b.polyFormat(POLYFMT_BOTHSIDES);
        emitTorch(b, lx + ax, ly - 0.1875f, lz + az, 5, tcell, r, g, bl);
        emitTorch(b, lx + bx, ly - 0.1875f, lz + bz, 5, tcell, r, g, bl);
        b.polyFormat(POLYFMT_CULLED);
        return;
    }

    // ---- Boton y placas: una caja con la textura de piedra o de tablones ----
    float bd[6]; Partial::bounds(type, meta, bd);
    TextureCell c = (type == BlockType::PLATE_WOOD) ? TextureCell{4, 0} : TextureCell{1, 0};
    TextureCell cells[6] = { c, c, c, c, c, c };
    bool flip[6] = { false, false, false, false, false, false };
    emitPartialBox(b, lx, ly, lz, bd, cells, flip, 0, lightRaw, nullptr);
}

void Chunk::emitPartialCell(DisplayListBuilder& b, int xi, int yi, int zi, float lx, float ly, float lz, const ChunkManager* manager) {
    const BlockType type = blocks[xi][yi][zi];
    const int meta = liquidMetaGet(xi, yi, zi);
    if (isRedstoneComponent(type)) { emitRedstoneCell(b, xi, yi, zi, lx, ly, lz, manager); return; }
    if (type == BlockType::CROPS) {   // RenderBlocks.renderBlockCrops: cuatro planos en # (a 0,25 y 0,75), 1/16 mas abajo, visibles por los dos lados; la textura es 88 + etapa
        const int stage = meta < 0 ? 7 : (meta > 7 ? 7 : meta);
        const int u0 = (8 + stage) * 16, v0 = 5 * 16, u1 = u0 + 16, v1 = v0 + 16;
        u8 cr, cg, cb; applyLight(packLightWithSubtract(getSkyLight(xi, yi, zi), getBlockLight(xi, yi, zi), manager), 255, 255, 255, cr, cg, cb);
        const float yb = ly - 0.5f - 1.0f / 16.0f, yt = ly + 0.5f - 1.0f / 16.0f;
        b.polyFormat(POLYFMT_BOTHSIDES);
        b.begin(GL_QUADS);
        b.color3b(cr, cg, cb);
        for (int p = 0; p < 4; p++) {
            const bool alongZ = p < 2;                                   // los dos primeros planos son x = cte; los otros z = cte
            const float off = (p & 1) ? 0.25f : -0.25f;
            const float xa = alongZ ? lx + off : lx - 0.5f, xb = alongZ ? lx + off : lx + 0.5f;
            const float za = alongZ ? lz - 0.5f : lz + off, zb = alongZ ? lz + 0.5f : lz + off;
            b.texCoordPacked(TEXTURE_PACK(inttot16(u0), inttot16(v1))); b.vertex(V(xa), V(yb), V(za));
            b.texCoordPacked(TEXTURE_PACK(inttot16(u1), inttot16(v1))); b.vertex(V(xb), V(yb), V(zb));
            b.texCoordPacked(TEXTURE_PACK(inttot16(u1), inttot16(v0))); b.vertex(V(xb), V(yt), V(zb));
            b.texCoordPacked(TEXTURE_PACK(inttot16(u0), inttot16(v0))); b.vertex(V(xa), V(yt), V(za));
        }
        b.end();
        b.polyFormat(POLYFMT_CULLED);
        return;
    }
    if (isPistonBlock(type)) {   // piston, cabeza del piston y bloque en movimiento (este se dibuja aparte, cada cuadro: Pistons::draw)
        if (type == BlockType::PISTON_MOVING) return;
        struct Ctx { DisplayListBuilder* b; float x, y, z; } ctx = { &b, lx - 0.5f, ly - 0.5f, lz - 0.5f };
        auto quad = [](void* c, const float p[12], const float uv[8]) {
            Ctx* k = (Ctx*)c;
            for (int i = 0; i < 4; i++) {
                k->b->texCoordPacked(TEXTURE_PACK(floattot16(uv[i * 2]), floattot16(uv[i * 2 + 1])));
                k->b->vertex(V(k->x + p[i * 3]), V(k->y + p[i * 3 + 1]), V(k->z + p[i * 3 + 2]));
            }
        };
        u8 pr, pg, pb; applyLight(packLightWithSubtract(getSkyLight(xi, yi, zi), getBlockLight(xi, yi, zi), manager), 255, 255, 255, pr, pg, pb);
        b.begin(GL_QUADS);
        b.color3b(pr, pg, pb);
        if (type == BlockType::PISTON_HEAD) Pistons::emitHead(quad, &ctx, meta, true, -1, 0.0f, 0.0f, 0.0f);
        else Pistons::emitBase(quad, &ctx, type, meta, false, 0.0f, 0.0f, 0.0f);
        b.end();
        return;
    }
    if (isSignBlock(type)) {   // SignModel:
        if (SignManager::isRegistered(getChunkX() * WorldConfig::SIZE_X + xi, yi, getChunkZ() * WorldConfig::SIZE_Z + zi)) return;   // lo dibuja SignManager con sign.png y texto tablero de 24 x 12 x 2 y poste de 2 x 14 x 2, a escala 2/3 (1 unidad = 1/24 de bloque)
        const u8 light = packLightWithSubtract(getSkyLight(xi, yi, zi), getBlockLight(xi, yi, zi), manager);
        const float k = 1.0f / 24.0f;
        const TextureCell boardCell = TextureCell{4, 0};   // tablones; el original usa /item/sign.png
        const TextureCell stickCell = TextureCell{4, 1};   // tronco
        float board[6], stick[6];
        float yaw;
        if (type == BlockType::SIGN_POST) {
            yaw = -(float)meta * 22.5f;   // glRotatef(-meta * 360 / 16)
            board[0] = -12.0f * k; board[1] = 14.0f * k;        board[2] = -1.0f * k; board[3] = 12.0f * k; board[4] = 26.0f * k; board[5] = 1.0f * k;
            stick[0] = -1.0f * k;  stick[1] = 0.0f;              stick[2] = -1.0f * k; stick[3] = 1.0f * k;  stick[4] = 14.0f * k; stick[5] = 1.0f * k;
            emitYawBox(b, lx, ly, lz, stick, yaw, stickCell, light);
        } else {
            float fy = 0.0f;
            if (meta == 2) fy = 180.0f; else if (meta == 4) fy = 90.0f; else if (meta == 5) fy = -90.0f;
            yaw = -fy;
            // glTranslatef(0, -5/16, -7/16) dentro del giro: el tablero queda pegado a la pared
            const float oy = 12.0f / 16.0f * (2.0f / 3.0f) - 5.0f / 16.0f, oz = -7.0f / 16.0f;
            board[0] = -12.0f * k; board[1] = oy + 2.0f * k;  board[2] = oz - 1.0f * k; board[3] = 12.0f * k; board[4] = oy + 14.0f * k; board[5] = oz + 1.0f * k;
        }
        emitYawBox(b, lx, ly, lz, board, yaw, boardCell, light);
        return;
    }
    if (isStructureBlock(type)) {   // losa, escalera, escalera de mano y valla
        const u8 sLight = packLightWithSubtract(getSkyLight(xi, yi, zi), getBlockLight(xi, yi, zi), manager);
        TextureCell sTop, sSide, sBottom;
        getBlockTextures(type, sTop, sSide, sBottom);
        if (type == BlockType::LADDER) {   // RenderBlocks.renderBlockLadder: un plano a 0,05 de la pared, visible por los dos lados
            u8 lr, lg, lb; applyLight(sLight, 255, 255, 255, lr, lg, lb);
            const float cx = lx, cy = ly, cz = lz, o = 0.05f;
            const float u0 = (float)(sSide.col * 16), v0 = (float)(sSide.row * 16), u1 = u0 + 16.0f, v1 = v0 + 16.0f;
            float p[4][3];   // esquinas: abajo-a, abajo-b, arriba-b, arriba-a
            switch (meta) {
                case 2: { const float z = cz + 0.5f - o; p[0][0] = cx - 0.5f; p[1][0] = cx + 0.5f; p[2][0] = cx + 0.5f; p[3][0] = cx - 0.5f; for (int i = 0; i < 4; i++) p[i][2] = z; break; }
                case 3: { const float z = cz - 0.5f + o; p[0][0] = cx + 0.5f; p[1][0] = cx - 0.5f; p[2][0] = cx - 0.5f; p[3][0] = cx + 0.5f; for (int i = 0; i < 4; i++) p[i][2] = z; break; }
                case 4: { const float x = cx + 0.5f - o; p[0][2] = cz + 0.5f; p[1][2] = cz - 0.5f; p[2][2] = cz - 0.5f; p[3][2] = cz + 0.5f; for (int i = 0; i < 4; i++) p[i][0] = x; break; }
                default: { const float x = cx - 0.5f + o; p[0][2] = cz - 0.5f; p[1][2] = cz + 0.5f; p[2][2] = cz + 0.5f; p[3][2] = cz - 0.5f; for (int i = 0; i < 4; i++) p[i][0] = x; break; }
            }
            p[0][1] = cy - 0.5f; p[1][1] = cy - 0.5f; p[2][1] = cy + 0.5f; p[3][1] = cy + 0.5f;
            b.polyFormat(POLYFMT_BOTHSIDES);
            b.begin(GL_QUADS);
            b.color3b(lr, lg, lb);
            const float uu[4] = { u0, u1, u1, u0 }, vv[4] = { v1, v1, v0, v0 };
            for (int i = 0; i < 4; i++) {
                b.texCoordPacked(TEXTURE_PACK(floattot16(uu[i]), floattot16(vv[i])));
                b.vertex(V(p[i][0]), V(p[i][1]), V(p[i][2]));
            }
            b.end();
            b.polyFormat(POLYFMT_CULLED);
            return;
        }
        float boxes[9][6];
        const int nb = Partial::shapeBoxes(type, meta, boxes);
        const TextureCell sCells[6] = { sBottom, sTop, sSide, sSide, sSide, sSide };
        const bool sFlip[6] = { false, false, false, false, false, false };
        const bool floorSolid = yi > 0 && isNormalCube(blocks[xi][yi - 1][zi]);
        for (int k = 0; k < nb; k++) {
            int sSkip = 0;
            if (boxes[k][1] <= 0.0f && floorSolid) sSkip |= 1;       // la cara de abajo contra el suelo no se ve
            if (isStairsBlock(type) && k == 1) sSkip |= 1;            // el escalon se apoya en la mitad de abajo
            emitPartialBox(b, lx, ly, lz, boxes[k], sCells, sFlip, sSkip, sLight, nullptr);
        }
        return;
    }
    float bd[6]; Partial::bounds(type, meta, bd);
    TextureCell cells[6];
    bool flip[6] = { false, false, false, false, false, false };
    int skip = 0;
    const signed char* topUV = nullptr;
    for (int f = 0; f < 6; f++) {
        if (isDoorBlock(type)) {
            cells[f] = Partial::doorCell(type, meta, f, flip[f]);
        } else if (type == BlockType::BED_BLOCK) {
            bool sk;
            cells[f] = Partial::bedCell(meta, f, flip[f], sk);
            if (sk) skip |= (1 << f);
        } else if (type == BlockType::CAKE_BLOCK) {   // BlockCake.getBlockTextureFromSideAndMetadata
            cells[f] = (f == 1) ? TextureCell{9, 7} : (f == 0) ? TextureCell{12, 7} : (f == 4 && meta > 0) ? TextureCell{11, 7} : TextureCell{10, 7};
        } else if (type == BlockType::FARMLAND) {   // BlockFarmland.getBlockTextureFromSideAndMetadata: arriba seca (87) o mojada (86), el resto tierra
            cells[f] = (f == 1) ? (meta > 0 ? TextureCell{6, 5} : TextureCell{7, 5}) : TextureCell{2, 0};
        } else {
            cells[f] = TextureCell{4, 5};   // trampilla
        }
    }
    if (type == BlockType::BED_BLOCK) topUV = Partial::bedTopUV(meta & 3);
    const u8 lightRaw = packLightWithSubtract(getSkyLight(xi, yi, zi), getBlockLight(xi, yi, zi), manager);
    emitPartialBox(b, lx, ly, lz, bd, cells, flip, skip, lightRaw, topUV);
}

void Chunk::buildBandGeometry(DisplayListBuilder& b, int bandIndex, const ChunkManager* manager, int xBegin, int xEnd) {
    int bandStart = bandIndex * DL_BAND_HEIGHT;
    int bandEnd = bandStart + DL_BAND_HEIGHT; // exclusivo
    b.polyFormat(POLYFMT_CULLED);

    for (int xi = xBegin; xi < xEnd; xi++) {
        for (int zi = 0; zi < SIZE_Z; zi++) {
            int colMinY = columnMinY[xi][zi];
            int colMaxY = columnMaxY[xi][zi];
            if (colMaxY < colMinY) continue;

            int yStart = colMinY > bandStart ? colMinY : bandStart;
            int yEnd   = colMaxY < (bandEnd - 1) ? colMaxY : (bandEnd - 1);
            if (yStart > yEnd) continue;


            for (int yi = yStart; yi <= yEnd; yi++) {
                BlockType type = blocks[xi][yi][zi];
                if (type == BlockType::AIR) continue;

                float lx = (float)xi;
                float ly = (float)(yi - bandStart);
                float lz = (float)zi;

                if (type == BlockType::TALLGRASS || type == BlockType::DEAD_BUSH ||
                    isCrossPlant(type)) {
                    u8 tintR, tintG, tintB;
                    columnTint(type, xi, zi, tintR, tintG, tintB);
                    TextureCell tgTop, tgSide, tgBottom;
                    getBlockTextures(type, tgTop, tgSide, tgBottom);
                    u8 lightRaw = packLightWithSubtract(getSkyLight(xi, yi, zi), getBlockLight(xi, yi, zi), manager);
                    b.polyFormat(POLYFMT_BOTHSIDES);
                    emitCrossQuad(b, lx, ly, lz, tgSide, tintR, tintG, tintB, lightRaw);
                    b.polyFormat(POLYFMT_CULLED);
                    continue;
                }

                if (type == BlockType::FIRE) {
                    emitFireCell(b, xi, yi, zi, lx, ly, lz, manager);
                    continue;
                }

                if (type == BlockType::PORTAL) {
                    emitPortalCell(b, xi, yi, zi, lx, ly, lz, manager);
                    continue;
                }

                if (isTorchBlock(type)) {
                    b.polyFormat(POLYFMT_BOTHSIDES);
                    emitTorch(b, lx, ly, lz, torchMeta(type));
                    b.polyFormat(POLYFMT_CULLED);
                    continue;
                }

                if (isLiquidBlock(type)) {
                    emitLiquidCell(b, xi, yi, zi, lx, ly, lz, manager);
                    continue;
                }

                if (isPartialBlock(type)) {
                    emitPartialCell(b, xi, yi, zi, lx, ly, lz, manager);
                    continue;
                }

                if (type == BlockType::SNOW_LAYER) {
                    TextureCell snowCell = {2, 4};
                    u8 lightRaw = packLightWithSubtract(getSkyLight(xi, yi, zi), getBlockLight(xi, yi, zi), manager);
                    emitSnowLayer(b, lx, ly, lz, snowCell, lightRaw);
                    continue;
                }

                FaceMask faces = faceCache[xi][yi][zi];
                if (!faces.top && !faces.bottom && !faces.north &&
                    !faces.south && !faces.east && !faces.west) {
                    continue;
                }

                TextureCell top, side, bottom;
                getBlockTextures(type, top, side, bottom);

                u8 tintR, tintG, tintB;
                columnTint(type, xi, zi, tintR, tintG, tintB);

                bool isLeaves = (type == BlockType::OAK_LEAVES || type == BlockType::BIRCH_LEAVES || type == BlockType::SPRUCE_LEAVES);

                float topSink = 0.0f;
                if (type == BlockType::WATER) {
                    bool waterAbove = (yi + 1 < SIZE_Y) && (blocks[xi][yi + 1][zi] == BlockType::WATER);
                    if (!waterAbove) topSink = 0.125f;
                }

                u8 lightSouth = 0, lightNorth = 0, lightWest = 0;
                u8 lightEast = 0, lightBottom = 0, lightTop = 0;
                if (!b.isCounting()) { // la luz solo afecta el color, no la cantidad de palabras
                    lightSouth  = getFaceLightRaw(xi, yi, zi + 1, manager);
                    lightNorth  = getFaceLightRaw(xi, yi, zi - 1, manager);
                    lightWest   = getFaceLightRaw(xi - 1, yi, zi, manager);
                    lightEast   = getFaceLightRaw(xi + 1, yi, zi, manager);
                    lightBottom = getFaceLightRaw(xi, yi - 1, zi, manager);
                    lightTop    = getFaceLightRaw(xi, yi + 1, zi, manager);
                }

                if (type == BlockType::WATER || type == BlockType::GLASS || type == BlockType::ICE) b.polyFormat(POLYFMT_BOTHSIDES);
                int frontDir = -1;
                TextureCell frontCell = {0, 0};
                if (isFurnaceBlock(type)) {
                    top = FURNACE_TOP_CELL;
                    bottom = FURNACE_TOP_CELL;
                    side = FURNACE_SIDE_CELL;
                    frontDir = furnaceFrontDir(type);
                    frontCell = furnaceIsLit(type) ? FURNACE_FRONT_LIT : FURNACE_FRONT_OFF;
                } else if (isDispenserBlock(type)) {
                    top = {14, 3};
                    bottom = {14, 3};
                    side = {13, 2};
                    frontDir = dispenserFrontDir(type);
                    frontCell = DISPENSER_FRONT_CELL;
                } else if (isPumpkinBlock(type)) {
                    top = PUMPKIN_TOP_CELL;
                    bottom = PUMPKIN_TOP_CELL;
                    side = PUMPKIN_SIDE_CELL;
                    frontDir = liquidMetaGet(xi, yi, zi) & 3;
                    frontCell = isJackBlock(type) ? JACK_FRONT_CELL : PUMPKIN_FRONT_CELL;
                }

                // Cofre: el frente y las mitades de un doble se deciden mirando a los vecinos (BlockChest.getBlockTexture)
                TextureCell chestSides[4];
                const TextureCell* sideOverride = nullptr;
                if (type == BlockType::CHEST) {
                    struct Around {
                        const Chunk* c; int xi, yi, zi; const ChunkManager* m;
                        BlockType at(int dx, int dz) const { return c->typeNear(xi + dx, yi, zi + dz, m, BlockType::STONE); }
                    } around = { this, xi, yi, zi, manager };
                    chestSides[0] = Chest::cellOfIndex(Chest::sideIndex(around, 3)); // sur
                    chestSides[1] = Chest::cellOfIndex(Chest::sideIndex(around, 2)); // norte
                    chestSides[2] = Chest::cellOfIndex(Chest::sideIndex(around, 4)); // oeste
                    chestSides[3] = Chest::cellOfIndex(Chest::sideIndex(around, 5)); // este
                    sideOverride = chestSides;
                }

                // Mesa de crafteo (BlockWorkbench.getBlockTextureFromSide): las caras 2 (norte) y 4 (oeste) llevan la textura 60 (col 12, fila 3)
                // y las caras 3 (sur) y 5 (este) la 59 (col 11, fila 3)
                TextureCell tableSides[4];
                if (type == BlockType::CRAFTING_TABLE) {
                    tableSides[0] = {11, 3}; // sur
                    tableSides[1] = {12, 3}; // norte
                    tableSides[2] = {12, 3}; // oeste
                    tableSides[3] = {11, 3}; // este
                    sideOverride = tableSides;
                }

                float sideInset = (type == BlockType::CACTUS) ? (1.0f / 16.0f) : 0.0f;
                emitCube(b, lx, ly, lz, top, side, bottom, faces, tintR, tintG, tintB, isLeaves,
                         lightSouth, lightNorth, lightWest, lightEast, lightBottom, lightTop, topSink,
                         frontDir, frontCell, sideInset, sideOverride);
                if (type == BlockType::WATER || type == BlockType::GLASS || type == BlockType::ICE) b.polyFormat(POLYFMT_CULLED);
            }
        }
    }
    b.polyFormat(POLYFMT_BOTHSIDES);
}

static inline int chunkFloorDiv(int a, int b) {
    int d = a / b;
    if ((a % b != 0) && ((a < 0) != (b < 0))) d--;
    return d;
}

void Chunk::draw(float playerX, float playerY, float playerZ,
                  float lookDirX, float lookDirY, float lookDirZ, const ChunkManager* manager,
                  int* vertBudget, int* rebuildBandBudget) {
    if (inWindow && displayListDirty && generationComplete && !cacheRecalcPending && !initialSkyLightPending &&
        (!rebuildBandBudget || *rebuildBandBudget > 0)) {
        { PROF(Prof::DL_REBUILD); PROFSTEP("list"); continueDisplayListRebuild(manager, rebuildBandBudget); }
    }

    glBindTexture(0, Texture::textureID);
    const float maxDistSq = WorldConfig::renderDistance() * WorldConfig::renderDistance();
    const float LOD_DIST_RAW = WorldConfig::renderDistance() * 0.5f;
    // El detalle completo (bloques con todas sus caras) llega como mucho a 8,5 en cualquier modo: lo que el modo dsi agrega es solo LOD (1 quad por columna)
    const float detailCap = Options::get().renderQuality == 2 ? 4.5f : (Options::get().renderQuality == 1 ? 6.5f : 8.5f);   // opcion render: mas LOD cerca = menos poligonos
    const float LOD_DIST = LOD_DIST_RAW < detailCap ? LOD_DIST_RAW : detailCap;
    const float LOD_DIST_SQ = LOD_DIST * LOD_DIST;

    float chunkMinX = gridToWorldX(0) - 0.5f;
    float chunkMaxX = gridToWorldX(SIZE_X - 1) + 0.5f;
    float chunkMinZ = gridToWorldZ(0) - 0.5f;
    float chunkMaxZ = gridToWorldZ(SIZE_Z - 1) + 0.5f;

    float closestX = playerX < chunkMinX ? chunkMinX : (playerX > chunkMaxX ? chunkMaxX : playerX);
    float closestZ = playerZ < chunkMinZ ? chunkMinZ : (playerZ > chunkMaxZ ? chunkMaxZ : playerZ);

    float cdx = closestX - playerX;
    float cdz = closestZ - playerZ;
    float chunkDistSq = cdx * cdx + cdz * cdz;

    if (chunkDistSq > LOD_DIST_SQ || (displayListDirty && !displayListEverBuilt) ||
        (initialSkyLightPending && !displayListEverBuilt)) {
        // ----- Camino LOD: todo el chunk a la vez, solo cara superior -----
        if (chunkDistSq > maxDistSq) return;

        {   // lista cacheada del chunk entero: una llamada en vez de recorrer las 64 columnas cada cuadro
            ProfScope lodProf(Prof::DL_LOD);
            const bool coarseNow = chunkDistSq > 14.0f * 14.0f;
            const float ccx = (chunkMinX + chunkMaxX) * 0.5f - playerX, ccz = (chunkMinZ + chunkMaxZ) * 0.5f - playerZ;
            if (ccx * lookDirX + ccz * lookDirZ < -6.0f) return;   // el chunk entero queda detras de la camara
            const bool canBuild = generationComplete && rebuildBandBudget && *rebuildBandBudget > 0 && s_lodBuildsThisFrame < 1 && s_dlAllocFailFrames == 0;
            if ((lodDirty || lodCoarse != coarseNow) && canBuild) { ++s_lodBuildsThisFrame; buildLodList(coarseNow); }
            if (dlLod || (!lodDirty && lodCoarse == coarseNow)) {   // lista lista (o chunk sin nada que dibujar); si esta desactualizada se usa igual hasta poder rearmarla
                if (dlLod) {
                    if (vertBudget) {
                        if (*vertBudget < dlLodVerts) return;
                        *vertBudget -= dlLodVerts;
                    }
                    glPushMatrix();
                    glTranslate3f32(floattof32(gridToWorldX(0)), 0, floattof32(gridToWorldZ(0)));
                    callListFast(dlLod);
                    glPopMatrix(1);
                }
                return;
            }
        }

        const float HALF_ANGLE_COS = 0.1736f;
        const float HALF_ANGLE_COS_SQ = HALF_ANGLE_COS * HALF_ANGLE_COS;
        const float NEAR_RADIUS = 3.0f;

        glPushMatrix();
        float curX = 0.0f, curY = 0.0f, curZ = 0.0f;

        // Chunks lejanos (mas de 14 de distancia): un solo quad de 2 x 2 columnas, con la columna mas alta que se pueda dibujar.
        // Son 16 quads en vez de 64 por chunk: lo que suma el modo dsi en distancia casi no cuesta dibujo.
        const bool coarse = chunkDistSq > 14.0f * 14.0f;
        const int lodStep = coarse ? 2 : 1;
        for (int gxi = 0; gxi < SIZE_X; gxi += lodStep) {
            for (int gzi = 0; gzi < SIZE_Z; gzi += lodStep) {
                int xi = gxi, zi = gzi;
                float worldX = gridToWorldX(gxi);
                float worldZ = gridToWorldZ(gzi);
                if (coarse) {
                    int bestY = -1;
                    for (int a = 0; a < 2; a++) {
                        for (int b = 0; b < 2; b++) {
                            const int cy = columnMaxY[gxi + a][gzi + b];
                            if (cy < columnMinY[gxi + a][gzi + b] || cy <= bestY) continue;
                            if (lodSkipTop(blocks[gxi + a][cy][gzi + b])) continue;
                            bestY = cy; xi = gxi + a; zi = gzi + b;
                        }
                    }
                    if (bestY < 0) continue;
                    worldX += 0.5f; worldZ += 0.5f;   // centro del bloque de 2 x 2
                }
                float dx = worldX - playerX;
                float dz = worldZ - playerZ;
                float horizDistSq = dx * dx + dz * dz;
                if (horizDistSq > maxDistSq) continue;

                int colMaxY = columnMaxY[xi][zi];
                if (colMaxY < columnMinY[xi][zi]) continue;

                BlockType topType = blocks[xi][colMaxY][zi];
		if (topType == BlockType::TALLGRASS || isTorchBlock(topType) || topType == BlockType::FIRE || topType == BlockType::PORTAL || topType == BlockType::CACTUS ||
  		topType == BlockType::OAK_LEAVES || topType == BlockType::BIRCH_LEAVES ||
   		topType == BlockType::SNOW_LAYER || topType == BlockType::SPRUCE_LEAVES ||
   		topType == BlockType::DEAD_BUSH || topType == BlockType::ROSE ||
   		isCrossPlant(topType) || isPartialBlock(topType)) continue;

                float worldY = gridToWorldY(colMaxY);
                float dy = worldY - playerY;
                float distSq = horizDistSq + dy * dy;
                if (distSq > maxDistSq) continue;

                if (distSq > NEAR_RADIUS * NEAR_RADIUS) {
                    float forwardDot = dx * lookDirX + dy * lookDirY + dz * lookDirZ;
                    if (forwardDot < 0.0f) continue;
                    if (forwardDot * forwardDot < HALF_ANGLE_COS_SQ * distSq) continue;
                }

                TextureCell lodTop, lodSide, lodBottom;
                getBlockTextures(topType, lodTop, lodSide, lodBottom);

                u8 lodTintR, lodTintG, lodTintB;
                columnTint(topType, xi, zi, lodTintR, lodTintG, lodTintB);

                if (vertBudget) {
                    if (*vertBudget <= 0) continue; // presupuesto agotado: se salta el resto de columnas LOD de este chunk
                    *vertBudget -= 4; // 4 vértices por quad, igual que el camino detallado
                }

                glTranslatef(worldX - curX, worldY - curY, worldZ - curZ);
                curX = worldX; curY = worldY; curZ = worldZ;
                drawLodTopQuad(lodTop, lodTintR, lodTintG, lodTintB, coarse ? 2.0f : 1.0f);
            }
        }
        glPopMatrix(1);
        return;
    }

    // ----- Camino detallado: display lists por banda, ya precalculados -----
    if (chunkDistSq > maxDistSq) return;

    float originX = gridToWorldX(0);
    float originZ = gridToWorldZ(0);

    // ----- Descarte por campo de visión, en enteros (1 unidad = 1/16 de bloque) -----
    // Cono de 58°: diagonal real de la cámara (~49°) + margen para balanceo.
    const int CONE_COS = 136;  // cos(58°) * 256
    const int CONE_SIN = 217;  // sin(58°) * 256
    const int BAND_R = 100;    // radio de una banda 8x2x8 (~5.74 bloques) + margen.
                               // Si cambiás DL_BAND_HEIGHT, este valor hay que recalcularlo.

    int eyeX = (int)(playerX * 16.0f);
    int eyeY = (int)(playerY * 16.0f);
    int eyeZ = (int)(playerZ * 16.0f);
    int dirX = (int)(lookDirX * 256.0f);
    int dirY = (int)(lookDirY * 256.0f);
    int dirZ = (int)(lookDirZ * 256.0f);
    int centerX = (int)((gridToWorldX(0) + gridToWorldX(SIZE_X - 1)) * 8.0f);
    int centerZ = (int)((gridToWorldZ(0) + gridToWorldZ(SIZE_Z - 1)) * 8.0f);
    int baseY16 = (int)(gridToWorldY(0) * 16.0f) + DL_BAND_HEIGHT * 8 - 8; // centro de la banda 0

    const u32 cullStart = Prof::now();
    u32 callTicks = 0;
    const int originX32 = floattof32(originX);
    const int originZ32 = floattof32(originZ);
    const int baseY32 = floattof32(gridToWorldY(0));
    for (int band = 0; band < DL_BAND_COUNT; band++) {
        if (!dlBands[band] && !dlBands2[band]) continue;

        int vx = centerX - eyeX;
        int vy = baseY16 + band * DL_BAND_HEIGHT * 16 - eyeY;
        int vz = centerZ - eyeZ;
        int v2 = vx * vx + vy * vy + vz * vz;
        if (v2 > BAND_R * BAND_R) { // si la cámara está dentro de la banda, siempre se dibuja
            int d = (vx * dirX + vy * dirY + vz * dirZ) >> 8; // distancia a lo largo de la mirada
            int e2 = v2 - d * d;
            if (e2 < 0) e2 = 0;
            int e = sqrt32(e2);                               // distancia perpendicular a la mirada
            if (((e * CONE_COS - d * CONE_SIN) >> 8) > BAND_R) continue; // fuera del cono
        }

        const int bandVerts = dlBandVertCount[band] + dlBandVertCount2[band];
        if (vertBudget && bandVerts > *vertBudget) continue;

        const u32 callStart = Prof::now();
        glPushMatrix();
            glTranslate3f32(originX32, baseY32 + band * DL_BAND_HEIGHT * 4096, originZ32);
            if (dlBands[band]) callListFast(dlBands[band]);
            if (dlBands2[band]) callListFast(dlBands2[band]);
        glPopMatrix(1);
        callTicks += Prof::now() - callStart;

        if (vertBudget) *vertBudget -= bandVerts;
    }
    Prof::add(Prof::DL_CALL, callTicks);
    Prof::add(Prof::DL_CULL, (Prof::now() - cullStart) - callTicks);
}

bool Chunk::getSelectionBounds(int xi, int yi, int zi, float bd[6]) const {
    bd[0] = 0.0f; bd[1] = 0.0f; bd[2] = 0.0f; bd[3] = 1.0f; bd[4] = 1.0f; bd[5] = 1.0f;
    if (xi < 0 || xi >= SIZE_X || yi < 0 || yi >= SIZE_Y || zi < 0 || zi >= SIZE_Z) return false;
    const BlockType t = blocks[xi][yi][zi];
    if (isTorchBlock(t)) {   // BlockTorch.collisionRayTrace: caja chica
        Torch::Box bx = Torch::boundsForMeta(torchMeta(t));
        bd[0] = bx.x0; bd[1] = bx.y0; bd[2] = bx.z0; bd[3] = bx.x1; bd[4] = bx.y1; bd[5] = bx.z1;
    } else if (isPartialBlock(t) || t == BlockType::SNOW_LAYER) {
        Partial::bounds(t, liquidMetaGet(xi, yi, zi), bd);
    }
    return true;
}

void Chunk::drawOutlineAt(int xi, int yi, int zi) {
    if (xi < 0 || xi >= SIZE_X) return;
    if (yi < 0 || yi >= SIZE_Y) return;
    if (zi < 0 || zi >= SIZE_Z) return;

    float worldX = gridToWorldX(xi);
    float worldY = gridToWorldY(yi);
    float worldZ = gridToWorldZ(zi);

    glBindTexture(0, 0);

    glPushMatrix();
        glTranslatef(worldX, worldY, worldZ);
        float bd[6]; getSelectionBounds(xi, yi, zi, bd);   // antorcha, puertas, nieve, etc. se seleccionan por su caja, no por la celda entera
        if (bd[0] == 0.0f && bd[1] == 0.0f && bd[2] == 0.0f && bd[3] == 1.0f && bd[4] == 1.0f && bd[5] == 1.0f) cube.drawOutline();
        else cube.drawOutlineBox(bd[0] - 0.5f, bd[1] - 0.5f, bd[2] - 0.5f, bd[3] - 0.5f, bd[4] - 0.5f, bd[5] - 0.5f);
    glPopMatrix(1);
}

void Chunk::breakBlockAt(int xi, int yi, int zi, const ChunkManager* manager) {
    if (xi < 0 || xi >= SIZE_X) return;
    if (yi < 0 || yi >= SIZE_Y) return;
    if (zi < 0 || zi >= SIZE_Z) return;

    BlockType oldType = blocks[xi][yi][zi];
    const int oldMeta = liquidMetaGet(xi, yi, zi);
    blocks[xi][yi][zi] = BlockType::AIR;
    modified = true;
    rebuildColumnOccupancy(xi, zi);
    if (manager && generationComplete && (isRedstoneComponent(oldType) || oldType == BlockType::PISTON_HEAD)) Redstone::blockRemoved(*manager, chunkX * SIZE_X + xi, yi, chunkZ * SIZE_Z + zi, oldType, oldMeta);   // Block.onBlockRemoval
    if (manager && generationComplete && (isLeavesBlock(oldType) || oldType == BlockType::OAK_LOG || oldType == BlockType::BIRCH_LOG || oldType == BlockType::SPRUCE_LOG))
        Trees::onBlockRemoved(*manager, chunkX * SIZE_X + xi, yi, chunkZ * SIZE_Z + zi, oldType);   // marca las hojas que deben revisarse

    // Si justo encima había pasto alto, se cae también (sin soporte)
    if (yi + 1 < SIZE_Y && blocks[xi][yi + 1][zi] == BlockType::TALLGRASS) {
        breakBlockAt(xi, yi + 1, zi, manager);
    }
    
    if (yi + 1 < SIZE_Y && (blocks[xi][yi + 1][zi] == BlockType::TALLGRASS || blocks[xi][yi + 1][zi] == BlockType::DEAD_BUSH ||
                             isCrossPlant(blocks[xi][yi + 1][zi]))) {
        const BlockType upType = blocks[xi][yi + 1][zi];
        const float upX = gridToWorldX(xi), upY = gridToWorldY(yi + 1), upZ = gridToWorldZ(zi);
        breakBlockAt(xi, yi + 1, zi, manager);
        if (isSapling(upType) || upType == BlockType::SUGAR_CANE || upType == BlockType::MUSHROOM_RED || upType == BlockType::MUSHROOM_BROWN)
            PendingDrops::push(upX, upY, upZ, getDropType(upType));   // la caña y los hongos sin apoyo se sueltan como objeto
    }

    if (yi + 1 < SIZE_Y && blocks[xi][yi + 1][zi] == BlockType::CACTUS) {
        BlockType droppedType = blocks[xi][yi + 1][zi];
        float dropX = gridToWorldX(xi);
        float dropY = gridToWorldY(yi + 1);
        float dropZ = gridToWorldZ(zi);

        breakBlockAt(xi, yi + 1, zi, manager);
        PendingDrops::push(dropX, dropY, dropZ, droppedType);
    }

    if (yi + 1 < SIZE_Y && blocks[xi][yi + 1][zi] == BlockType::SNOW_LAYER) {
        breakBlockAt(xi, yi + 1, zi, manager);
    }

    // onNeighborBlockChange de BlockTorch: los 6 vecinos que sean antorchas revisan si les quedó su apoyo.
    // Ahora también se avisa a las de los chunks vecinos (antes solo a las del mismo chunk).
    const int tdx[6] = {-1, 1, 0, 0, 0, 0};
    const int tdy[6] = {0, 0, -1, 1, 0, 0};
    const int tdz[6] = {0, 0, 0, 0, -1, 1};
    for (int i = 0; i < 6; i++) {
        int nxi = xi + tdx[i], nyi = yi + tdy[i], nzi = zi + tdz[i];
        if (nyi < 0 || nyi >= SIZE_Y) continue;
        if (nxi >= 0 && nxi < SIZE_X && nzi >= 0 && nzi < SIZE_Z) {
            if (isTorchBlock(blocks[nxi][nyi][nzi])) {
                checkTorchSupport(nxi, nyi, nzi, manager);
            }
        } else if (manager) {
            Chunk* nc = manager->getChunkAt(gridToWorldX(nxi), gridToWorldZ(nzi));
            int cxi, cyi, czi;
            if (nc && nc != this && nc->isGenerationComplete() &&
                nc->worldToGrid(gridToWorldX(nxi), gridToWorldY(nyi), gridToWorldZ(nzi), cxi, cyi, czi) &&
                isTorchBlock(nc->blocks[cxi][cyi][czi])) {
                nc->checkTorchSupport(cxi, cyi, czi, manager);
            }
        }
    }

    updateFaceCache(xi, yi, zi, manager);
    updateFaceCache(xi + 1, yi, zi, manager);
    updateFaceCache(xi - 1, yi, zi, manager);
    updateFaceCache(xi, yi + 1, zi, manager);
    updateFaceCache(xi, yi - 1, zi, manager);
    updateFaceCache(xi, yi, zi + 1, manager);
    updateFaceCache(xi, yi, zi - 1, manager);

    if (manager) {
        float worldX = gridToWorldX(xi);
        float worldY = gridToWorldY(yi);
        float worldZ = gridToWorldZ(zi);
        if (xi == 0)          manager->refreshNeighborFace(worldX - 1.0f, worldY, worldZ);
        if (xi == SIZE_X - 1) manager->refreshNeighborFace(worldX + 1.0f, worldY, worldZ);
        if (zi == 0)          manager->refreshNeighborFace(worldX, worldY, worldZ - 1.0f);
        if (zi == SIZE_Z - 1) manager->refreshNeighborFace(worldX, worldY, worldZ + 1.0f);

        if (blockLightEmission(oldType) > 0) {
            manager->removeBlockLight(worldX, worldY, worldZ);
        }
        manager->propagateSkyColumn(worldX, worldZ);
        manager->relightNewAirCell(worldX, worldY, worldZ);
    }

    notifyLiquidNeighbors(xi, yi, zi, manager); // un hueco al lado del agua o la lava la despierta

    // puertas, trampillas y camas de alrededor revisan si les quedo su apoyo (soltando su objeto si no)
    if (manager) Partial::neighborsChanged(*manager, chunkX * SIZE_X + xi, yi, chunkZ * SIZE_Z + zi);
}

void Chunk::setBlockWithMeta(int xi, int yi, int zi, BlockType type, int meta, const ChunkManager* manager) {
    if (xi < 0 || xi >= SIZE_X || yi < 0 || yi >= SIZE_Y || zi < 0 || zi >= SIZE_Z) return;
    setBlockDirect(xi, yi, zi, type, manager);
    liquidMetaSet(xi, yi, zi, meta & 15);   // setBlockDirect la dejo en 0
    if (generationComplete) modified = true;
    markBandRangeDirty(yi - 1, yi + 1);
}

void Chunk::setBlockMeta(int xi, int yi, int zi, int meta, const ChunkManager* manager) {
    (void)manager;
    if (xi < 0 || xi >= SIZE_X || yi < 0 || yi >= SIZE_Y || zi < 0 || zi >= SIZE_Z) return;
    liquidMetaSet(xi, yi, zi, meta & 15);
    if (generationComplete) modified = true;
    markBandRangeDirty(yi - 1, yi + 1);
}

void Chunk::setMetaQuiet(int xi, int yi, int zi, int meta) {
    if (xi < 0 || xi >= SIZE_X || yi < 0 || yi >= SIZE_Y || zi < 0 || zi >= SIZE_Z) return;
    liquidMetaSet(xi, yi, zi, meta & 15);
    if (generationComplete) modified = true;
}

void Chunk::setBlockDirect(int xi, int yi, int zi, BlockType type, const ChunkManager* manager) {
    if (xi < 0 || xi >= SIZE_X) return;
    if (yi < 0 || yi >= SIZE_Y) return;
    if (zi < 0 || zi >= SIZE_Z) return;

    BlockType oldType = blocks[xi][yi][zi];
    const int oldMeta = liquidMetaGet(xi, yi, zi);
    blocks[xi][yi][zi] = type;
    liquidMetaSet(xi, yi, zi, 0); // setBlock del original: la metadata vuelve a 0
    if (manager && generationComplete && oldType != type && (isRedstoneComponent(oldType) || oldType == BlockType::PISTON_HEAD)) Redstone::blockRemoved(*manager, chunkX * SIZE_X + xi, yi, chunkZ * SIZE_Z + zi, oldType, oldMeta);
    if (manager && generationComplete && !s_structureGenActive && oldType != type && (isLeavesBlock(oldType) || oldType == BlockType::OAK_LOG || oldType == BlockType::BIRCH_LOG || oldType == BlockType::SPRUCE_LOG))
        Trees::onBlockRemoved(*manager, chunkX * SIZE_X + xi, yi, chunkZ * SIZE_Z + zi, oldType);   // Block.onBlockRemoval de troncos y hojas
    if (generationComplete) modified = true;   // lo que se escribe mientras el chunk se genera (arboles, menas, lagos) sale de la semilla: no hay que guardarlo

    if (yi > highestGeneratedY) highestGeneratedY = yi;
    if (!generationComplete || s_structureGenActive) {
        // Se saltea el recálculo de caras, pero la ocupación por bits SÍ se
        // mantiene al día (es un solo bit): la leen el recálculo de caras
        // posterior y los chunks vecinos.
        unsigned int mask = 1u << (yi % 32);
        if (isSolidGrid(xi, yi, zi)) occupancy[xi][zi][yi / 32] |= mask;
        else                         occupancy[xi][zi][yi / 32] &= ~mask;
        if (generationComplete) scheduleFullRefresh();
        return;
    }

    // Recalculamos la columna que cambió y sus 4 vecinas dentro de
    // este mismo chunk (su visibilidad de cara pudo haber cambiado)
    // (solo ensucian las bandas de las alturas yi - 1 a yi + 1: antes un bloque roto reconstruía las 64 bandas del chunk)
    recomputeColumnRange(xi, zi, yi - 1, yi + 1, manager);
    if (xi + 1 < SIZE_X) recomputeColumnRange(xi + 1, zi, yi - 1, yi + 1, manager);
    if (xi - 1 >= 0)     recomputeColumnRange(xi - 1, zi, yi - 1, yi + 1, manager);
    if (zi + 1 < SIZE_Z) recomputeColumnRange(xi, zi + 1, yi - 1, yi + 1, manager);
    if (zi - 1 >= 0)     recomputeColumnRange(xi, zi - 1, yi - 1, yi + 1, manager);

    if (manager) {
        float worldX = gridToWorldX(xi);
        float worldZ = gridToWorldZ(zi);
        float probeY = gridToWorldY(0);

        if (xi == 0)          manager->refreshNeighborFace(worldX - 1.0f, probeY, worldZ);
        if (xi == SIZE_X - 1) manager->refreshNeighborFace(worldX + 1.0f, probeY, worldZ);
        if (zi == 0)          manager->refreshNeighborFace(worldX, probeY, worldZ - 1.0f);
        if (zi == SIZE_Z - 1) manager->refreshNeighborFace(worldX, probeY, worldZ + 1.0f);

        // Durante la GENERACIÓN inicial (árboles, minerales, cactus)
        // esto se llama decenas de veces por chunk — la luz recién se
        // calcula UNA vez, completa, cuando el chunk termina de
        // generarse. Solo en ediciones de gameplay (chunk ya
        // completo) vale la pena recalcular en el momento.
        if (generationComplete) {
            float worldY = gridToWorldY(yi);

            int oldEmit = blockLightEmission(oldType);
            int newEmit = blockLightEmission(type);
            if (oldEmit > 0 && newEmit == 0) {
                manager->removeBlockLight(worldX, worldY, worldZ);
            }
            if (newEmit > 0) {
                mayHaveEmitters = true;
                manager->propagateBlockLight(worldX, worldY, worldZ, (u8)newEmit);
            }
            if (isTorchBlock(type)) {
                checkTorchSupport(xi, yi, zi, manager);
            }

            if (isLightOpaque(type)) {
                manager->removeSkyLightAt(worldX, worldY, worldZ);
            }
        }
    }
}

bool Chunk::flowerSupportedAt(int xi, int yi, int zi) const {
    if (xi < 0 || xi >= SIZE_X || zi < 0 || zi >= SIZE_Z) return false;
    if (yi < 1 || yi >= SIZE_Y) return false;
    BlockType below = blocks[xi][yi - 1][zi];
    return below == BlockType::GRASS || below == BlockType::DIRT;
}

// Soporte de cada planta (canBlockStay / canPlaceBlockAt de la Beta): flores sobre pasto o tierra; hongos sobre un bloque opaco con luz menor a 13;
// caña sobre otra caña, o sobre pasto, tierra o arena con agua pegada a la base.
bool Chunk::plantSupportedAt(BlockType plant, int xi, int yi, int zi, const ChunkManager* manager) const {
    if (plant == BlockType::WEB) return xi >= 0 && xi < SIZE_X && zi >= 0 && zi < SIZE_Z && yi >= 0 && yi < SIZE_Y;   // BlockWeb se pone en cualquier lado
    if (xi < 0 || xi >= SIZE_X || zi < 0 || zi >= SIZE_Z) return false;
    if (yi < 1 || yi >= SIZE_Y) return false;
    const BlockType below = blocks[xi][yi - 1][zi];
    if (plant == BlockType::MUSHROOM_RED || plant == BlockType::MUSHROOM_BROWN) {
        if (!isNormalCube(below)) return false;
        int sky = (int)getSkyLight(xi, yi, zi) - (manager ? manager->getSkylightSubtract() : 0);
        int lvl = sky > (int)getBlockLight(xi, yi, zi) ? sky : (int)getBlockLight(xi, yi, zi);
        return lvl < 13;
    }
    if (plant == BlockType::SUGAR_CANE) {
        if (below == BlockType::SUGAR_CANE) return true;
        if (below != BlockType::GRASS && below != BlockType::DIRT && below != BlockType::SAND) return false;
        static const int dx[4] = { -1, 1, 0, 0 }, dz[4] = { 0, 0, -1, 1 };
        for (int i = 0; i < 4; i++) {
            const int nx = xi + dx[i], nz = zi + dz[i];
            BlockType t = BlockType::AIR;
            if (nx >= 0 && nx < SIZE_X && nz >= 0 && nz < SIZE_Z) {
                t = blocks[nx][yi - 1][nz];
            } else if (manager) {
                Chunk* nc = manager->getChunkAt(gridToWorldX(nx), gridToWorldZ(nz));
                int cx, cy, cz;
                if (nc && nc->isGenerationComplete() && nc->worldToGrid(gridToWorldX(nx), gridToWorldY(yi - 1), gridToWorldZ(nz), cx, cy, cz))
                    t = nc->getBlockType(cx, cy, cz);
            }
            if (isWaterBlock(t)) return true;
        }
        return false;
    }
    return below == BlockType::GRASS || below == BlockType::DIRT || below == BlockType::FARMLAND;   // BlockFlower.canThisPlantGrowOnThisBlockID
}

static inline bool placementTargetFree(BlockType t) {
    return t == BlockType::AIR || isLiquidBlock(t) || t == BlockType::SNOW_LAYER || t == BlockType::FIRE;
}

int Chunk::s_placeMeta = -1;

bool Chunk::placeBlockAt(int xi, int yi, int zi, BlockFace face, BlockType typeToPlace, const ChunkManager* manager) {
    if (isTorchBlock(typeToPlace)) {
        return placeTorchFromClick(xi, yi, zi, face, manager); // ItemBlock + BlockTorch de la Beta
    }

    // La nieve se reemplaza en el lugar
    if (xi >= 0 && xi < SIZE_X && yi >= 0 && yi < SIZE_Y && zi >= 0 && zi < SIZE_Z &&
        blocks[xi][yi][zi] == BlockType::SNOW_LAYER) {
        if (isCrossPlant(typeToPlace) && !plantSupportedAt(typeToPlace, xi, yi, zi, manager)) return false;
        if (isPumpkinBlock(typeToPlace) && !(yi > 0 && isNormalCube(blocks[xi][yi - 1][zi]))) return false;   // BlockPumpkin.canPlaceBlockAt
        if (typeToPlace == BlockType::CHEST && !chestPlacementOk(xi, yi, zi, manager)) return false;
        setBlockDirect(xi, yi, zi, typeToPlace, manager);
        notifyLiquidNeighbors(xi, yi, zi, manager);
        if (manager) Partial::neighborsChanged(*manager, chunkX * SIZE_X + xi, yi, chunkZ * SIZE_Z + zi);   // las vallas de alrededor se unen
        return true;
    }

    int targetXi = xi;
    int targetYi = yi;
    int targetZi = zi;

    switch (face) {
        case BlockFace::NORTH:  targetZi -= 1; break;
        case BlockFace::SOUTH:  targetZi += 1; break;
        case BlockFace::EAST:   targetXi += 1; break;
        case BlockFace::WEST:   targetXi -= 1; break;
        case BlockFace::TOP:    targetYi += 1; break;
        case BlockFace::BOTTOM: targetYi -= 1; break;
        default: return false;
    }

    if (targetYi < 0 || targetYi >= SIZE_Y) return false;

    if (targetXi >= 0 && targetXi < SIZE_X && targetZi >= 0 && targetZi < SIZE_Z) {
	if (!placementTargetFree(blocks[targetXi][targetYi][targetZi])) return false;
        if (isCrossPlant(typeToPlace) && !plantSupportedAt(typeToPlace, targetXi, targetYi, targetZi, manager)) return false;
        if (isPumpkinBlock(typeToPlace) && !(targetYi > 0 && isNormalCube(blocks[targetXi][targetYi - 1][targetZi]))) return false;
        if (typeToPlace == BlockType::CHEST && !chestPlacementOk(targetXi, targetYi, targetZi, manager)) return false;
        setBlockDirect(targetXi, targetYi, targetZi, typeToPlace, manager);
        if (s_placeMeta >= 0) liquidMetaSet(targetXi, targetYi, targetZi, s_placeMeta);
        notifyLiquidNeighbors(targetXi, targetYi, targetZi, manager);
        if (manager) Partial::neighborsChanged(*manager, chunkX * SIZE_X + targetXi, targetYi, chunkZ * SIZE_Z + targetZi);
        return true;
    }

    if (!manager) return false;

    float worldX = gridToWorldX(targetXi);
    float worldY = gridToWorldY(targetYi);
    float worldZ = gridToWorldZ(targetZi);

    Chunk* neighbor = manager->getChunkAt(worldX, worldZ);
    if (!neighbor) return false;

    int nXi, nYi, nZi;
    if (!neighbor->worldToGrid(worldX, worldY, worldZ, nXi, nYi, nZi)) return false;

    if (!placementTargetFree(neighbor->getBlockType(nXi, nYi, nZi))) return false;
    if (isCrossPlant(typeToPlace) && !neighbor->plantSupportedAt(typeToPlace, nXi, nYi, nZi, manager)) return false;
    if (isPumpkinBlock(typeToPlace) && !(nYi > 0 && isNormalCube(neighbor->getBlockType(nXi, nYi - 1, nZi)))) return false;
    if (typeToPlace == BlockType::CHEST && !neighbor->chestPlacementOk(nXi, nYi, nZi, manager)) return false;

    neighbor->setBlockDirect(nXi, nYi, nZi, typeToPlace, manager);
    if (s_placeMeta >= 0) neighbor->liquidMetaSet(nXi, nYi, nZi, s_placeMeta);
    neighbor->notifyLiquidNeighbors(nXi, nYi, nZi, manager);
    Partial::neighborsChanged(*manager, neighbor->chunkX * SIZE_X + nXi, nYi, neighbor->chunkZ * SIZE_Z + nZi);
    return true;
}

// BlockChest.canPlaceBlockAt: a lo sumo un cofre al lado, que no sea ya parte de un doble
bool Chunk::chestPlacementOk(int xi, int yi, int zi, const ChunkManager* manager) const {
    struct Around {
        const Chunk* c; int xi, yi, zi; const ChunkManager* m;
        BlockType at(int dx, int dz) const { return c->typeNear(xi + dx, yi, zi + dz, m, BlockType::AIR); }
    } around = { this, xi, yi, zi, manager };
    return Chest::canPlaceAt(around);
}

int Chunk::getHighestSolidY(int xi, int zi) const {
    if (xi < 0 || xi >= SIZE_X) return -1;
    if (zi < 0 || zi >= SIZE_Z) return -1;

    for (int yi = SIZE_Y - 1; yi >= 0; yi--) {
        if (blocks[xi][yi][zi] != BlockType::AIR && blocks[xi][yi][zi] != BlockType::SNOW_LAYER) {
            return yi;
        }
    }

    return -1;
}

bool Chunk::saveToFile(const char* path) const {
    if (!generationComplete) return false;   // nunca escribir un chunk incompleto
    FILE* file = fopen(path, "wb");
    if (!file) return false;

    size_t totalBytes = sizeof(blocks);
    size_t written = fwrite(blocks, 1, totalBytes, file);
    // al final, el nivel de los líquidos (los archivos viejos no lo tienen: se leen como fuentes)
    if (written == totalBytes) fwrite(liquidMeta, 1, sizeof(liquidMeta), file);

    fclose(file);

    return written == totalBytes;
}

bool Chunk::loadFromFile(const char* path) {
    // (cada parte con su etiqueta, para ver en el registro de picos si lo caro es abrir el archivo, leerlo o recalcular)
    FILE* file;
    { PROFSTEP("load: open"); file = fopen(path, "rb"); }
    if (!file) return false;

    size_t totalBytes = sizeof(blocks);
    size_t bytesRead;
    {
        PROFSTEP("load: read");
        bytesRead = fread(blocks, 1, totalBytes, file);
        if (bytesRead == totalBytes) {
            size_t metaRead = fread(liquidMeta, 1, sizeof(liquidMeta), file);
            if (metaRead != sizeof(liquidMeta)) memset(liquidMeta, 0, sizeof(liquidMeta));
        }
        fclose(file);
    }

    if (bytesRead != totalBytes) return false;

    {   // archivo danado (guardado incompleto de una version anterior): si la capa y=0 es toda aire se descarta y el chunk se regenera de la semilla
        bool any = false;
        for (int x = 0; x < SIZE_X && !any; x++)
            for (int z = 0; z < SIZE_Z; z++)
                if (blocks[x][0][z] != BlockType::AIR) { any = true; break; }
        if (!any) { remove(path); return false; }
    }

    loadStaging = true;      // el bioma y los colores (19 a 46 ms juntos) se calculan despues, repartidos en varios cuadros
    loadBiomeIdx = 0;
    return true;
}

bool Chunk::continueLoad() {
    if (!loadStaging) return generationComplete;
    if (s_dimension == 0) {
        if (!climateReady) {   // la malla de clima en 4 cuadros, igual que al generar
            PROFSTEP("load: climate");
            lat.fillRow(chunkX * SIZE_X, chunkZ * SIZE_Z, climateRow);
            if (++climateRow >= 4) climateReady = true;
            s_heavyDidOne = true;
            return false;
        }
        {
            PROFSTEP("load: biome");
            do {
                computeColumnBiome(loadBiomeIdx / SIZE_Z, loadBiomeIdx % SIZE_Z);
                loadBiomeIdx++;
                s_heavyDidOne = true;
            } while (loadBiomeIdx < SIZE_X * SIZE_Z && heavyBudgetLeft());
        }
        if (loadBiomeIdx < SIZE_X * SIZE_Z) return false;
    } else {
        computeBiomeData();   // fuera del mundo normal no hay clima: bioma generico, barato
    }
    loadStaging = false;
    generationComplete = true;
    { PROFSTEP("load: occupancy"); rebuildAllOccupancy(); }
    initialSkyLightPending = true;
    initialSkyLightXi = 0;
    initialSkyLightZi = 0;
    skyInitPhase = 0; skyInitDir = 0;
    skyBaseReady = false;
    return true;
}

void Chunk::updateFaceCache(int xi, int yi, int zi, const ChunkManager* manager) {
    markDisplayListDirty();
    if (xi < 0 || xi >= SIZE_X || yi < 0 || yi >= SIZE_Y || zi < 0 || zi >= SIZE_Z) return;
    if (isTorchBlock(blocks[xi][yi][zi]) || blocks[xi][yi][zi] == BlockType::FIRE || blocks[xi][yi][zi] == BlockType::PORTAL || isPartialBlock(blocks[xi][yi][zi])) {
        FaceMask tFaces = {};
        tFaces.top = true;
        faceCache[xi][yi][zi] = tFaces;
        if (yi < columnMinY[xi][zi]) columnMinY[xi][zi] = yi;
        if (yi > columnMaxY[xi][zi]) columnMaxY[xi][zi] = yi;
        return;
    }

    if (xi < 0 || xi >= SIZE_X) return;
    if (yi < 0 || yi >= SIZE_Y) return;
    if (zi < 0 || zi >= SIZE_Z) return;

    FaceMask faces;

    faces.top    = !isSolidGrid(xi, yi + 1, zi);
    faces.bottom = !isSolidGrid(xi, yi - 1, zi);

    float worldX = gridToWorldX(xi);
    float worldY = gridToWorldY(yi);
    float worldZ = gridToWorldZ(zi);

    if (zi - 1 >= 0) {
        faces.north = !isSolidGrid(xi, yi, zi - 1);
    } else if (manager) {
        faces.north = !manager->isSolidAt(worldX, worldY, worldZ - 1.0f);
    } else {
        faces.north = true;
    }

    if (zi + 1 < SIZE_Z) {
        faces.south = !isSolidGrid(xi, yi, zi + 1);
    } else if (manager) {
        faces.south = !manager->isSolidAt(worldX, worldY, worldZ + 1.0f);
    } else {
        faces.south = true;
    }

    if (xi + 1 < SIZE_X) {
        faces.east = !isSolidGrid(xi + 1, yi, zi);
    } else if (manager) {
        faces.east = !manager->isSolidAt(worldX + 1.0f, worldY, worldZ);
    } else {
        faces.east = true;
    }

    if (xi - 1 >= 0) {
        faces.west = !isSolidGrid(xi - 1, yi, zi);
    } else if (manager) {
        faces.west = !manager->isSolidAt(worldX - 1.0f, worldY, worldZ);
    } else {
        faces.west = true;
    }

    if (yi == 0) {
        faces.bottom = false;
    }

    faceCache[xi][yi][zi] = faces;
    if (faces.top || faces.bottom || faces.north || faces.south || faces.east || faces.west) {
        if (yi < columnMinY[xi][zi]) columnMinY[xi][zi] = yi;
        if (yi > columnMaxY[xi][zi]) columnMaxY[xi][zi] = yi;
    }
}

// Un chunk vecino acaba de terminar de generarse: las caras de la orilla pueden cambiar. Antes cada columna ensuciaba la lista del chunk entero,
// y los 4 vecinos de cada chunk nuevo se reconstruían completos. Ahora se compara y solo se ensucian las bandas donde de verdad cambió una cara.
static bool sameFaces(const FaceMask& a, const FaceMask& b) {
    return a.top == b.top && a.bottom == b.bottom && a.north == b.north && a.south == b.south && a.east == b.east && a.west == b.west;
}

bool Chunk::refreshBorderFaces(const ChunkManager* manager, int maxColumns) {
    auto refresh = [&](int xi, int zi) {
        FaceMask before[SIZE_Y];
        for (int y = 0; y < SIZE_Y; y++) before[y] = faceCache[xi][y][zi];
        rebuildColumnOccupancy(xi, zi);
        computeColumnFacesBitwise(xi, zi, manager);
        int lo = SIZE_Y, hi = -1;
        for (int y = 0; y < SIZE_Y; y++) {
            if (!sameFaces(before[y], faceCache[xi][y][zi])) { if (y < lo) lo = y; if (y > hi) hi = y; }
        }
        if (hi >= 0) markBandRangeDirty(lo, hi);
    };
    // Columnas del borde numeradas: [0,SIZE_X) = z 0, luego z max, luego x 0, luego x max. Se reparte en varios cuadros.
    const int total = 2 * SIZE_X + 2 * SIZE_Z;
    int done = 0;
    while (borderRefreshCursor < total && done < maxColumns) {
        int c = borderRefreshCursor++;
        done++;
        if (c < SIZE_X) refresh(c, 0);
        else if (c < 2 * SIZE_X) refresh(c - SIZE_X, SIZE_Z - 1);
        else if (c < 2 * SIZE_X + SIZE_Z) refresh(0, c - 2 * SIZE_X);
        else refresh(SIZE_X - 1, c - 2 * SIZE_X - SIZE_Z);
    }
    if (borderRefreshCursor >= total) { borderRefreshCursor = 0; return true; }
    return false;
}

void Chunk::recalculateFullFaceCache(const ChunkManager* manager) {
    for (int xi = 0; xi < SIZE_X; xi++) {
        for (int zi = 0; zi < SIZE_Z; zi++) {
            recomputeColumn(xi, zi, manager);
        }
    }
}

BlockType Chunk::getBlockType(int xi, int yi, int zi) const {
    if (xi < 0 || xi >= SIZE_X) return BlockType::AIR;
    if (yi < 0 || yi >= SIZE_Y) return BlockType::AIR;
    if (zi < 0 || zi >= SIZE_Z) return BlockType::AIR;
    return blocks[xi][yi][zi];
}

void Chunk::beginFullFaceCacheRecalc() {
    cacheRecalcPending = true;
    cacheRecalcXi = 0;
    cacheRecalcZi = 0;
}

void Chunk::continueInitialSkyLight(const ChunkManager* manager) {
    if (!initialSkyLightPending || !manager) return;
    PROFSTEP(skyInitPhase == 0 ? "luz cielo A" : "luz cielo B");

    if (skyInitPhase == 0) {
        // Fase A: luz 15 en línea recta hasta el primer opaco, sin inundar
        int processed = 0;
        while (initialSkyLightXi < SIZE_X && processed < SKY_BASE_BATCH && heavyBudgetLeft()) {
            s_heavyDidOne = true;
            int xi = initialSkyLightXi, zi = initialSkyLightZi;
            int y = SIZE_Y - 1;
            int skyLevel = 15;
            while (y >= 0 && !isLightOpaque(blocks[xi][y][zi])) {
                skyLevel -= lightDim(blocks[xi][y][zi]);   // agua y hielo oscurecen
                if (skyLevel < 0) skyLevel = 0;
                lightData[xi][y][zi] = (lightData[xi][y][zi] & 0x0F) | (u8)(skyLevel << 4);
                y--;
            }
            skyBottom[xi][zi] = (u8)(y + 1);
            for (int yb = y; yb >= 0; yb--) {
                lightData[xi][yb][zi] &= 0x0F; // apaga el cielo bajo el primer opaco (la fase B lo repone donde corresponda)
            }

            initialSkyLightZi++;
            if (initialSkyLightZi >= SIZE_Z) { initialSkyLightZi = 0; initialSkyLightXi++; }
            processed++;
        }
        if (initialSkyLightXi >= SIZE_X) {
            skyInitPhase = 1;
            skyBaseReady = true;
            initialSkyLightXi = 0;
            initialSkyLightZi = 0;
            markDisplayListDirty(); // una sola vez, no por celda
        }
        return;
    }

    // Fase B: solo se inunda donde hay desnivel de luz con un vecino
    // Una DIRECCIÓN por paso, no una columna entera: cada dirección inunda hasta MAX_FLOOD_STEPS_PER_CALL pasos (~10 ms) y una columna son 4
    // (los 20 a 39 ms de 'luz cielo B' del registro de picos). Si una dirección no tiene nada que inundar sale gratis y sigue con la próxima.
    int processed = 0;
    while (initialSkyLightXi < SIZE_X && processed < SKYLIGHT_INIT_BATCH * 4 && heavyBudgetLeft()) {
        spreadSkyFromColumn(initialSkyLightXi, initialSkyLightZi, manager, skyInitDir);
        s_heavyDidOne = true;
        processed++;
        if (++skyInitDir >= 4) {
            skyInitDir = 0;
            reseedColumnEmitters(initialSkyLightXi, initialSkyLightZi, manager); // antorchas y hornos encendidos
            initialSkyLightZi++;
            if (initialSkyLightZi >= SIZE_Z) { initialSkyLightZi = 0; initialSkyLightXi++; }
        }
    }
    if (initialSkyLightXi >= SIZE_X) {
        initialSkyLightPending = false;

        // Los emisores de los chunks vecinos ya cargados también tienen que iluminar este
        for (int dx = -1; dx <= 1; dx++) {
            for (int dz = -1; dz <= 1; dz++) {
                if (dx == 0 && dz == 0) continue;
                Chunk* n = manager->getChunkByChunkCoord(chunkX + dx, chunkZ + dz);
                if (n && n->hasEmitters()) n->requestEmitterReseed();   // antes: las 64 columnas del vecino de golpe (100 ms o más con lava)
            }
        }
    }
}

void Chunk::reseedColumnEmitters(int xi, int zi, const ChunkManager* manager) {
    for (int yi = 0; yi < SIZE_Y; yi++) {
        // updateTick de BlockTorch: una antorcha sin orientar (metadata 0, de un mundo viejo) se orienta sola
        if (blocks[xi][yi][zi] == BlockType::TORCH) reorientLegacyTorch(xi, yi, zi, manager);
        if (isFlowingLiquid(blocks[xi][yi][zi]) || blocks[xi][yi][zi] == BlockType::FIRE) { // tickOnLoad: las corrientes y el fuego retoman su cálculo
            Liquid::pushTick(chunkX * SIZE_X + xi, yi, chunkZ * SIZE_Z + zi, blocks[xi][yi][zi], Liquid::tickRateOf(blocks[xi][yi][zi]));
        }
        int level = blockLightEmission(blocks[xi][yi][zi]);
        if (level > 0) {
            mayHaveEmitters = true;
            if (isLavaBlock(blocks[xi][yi][zi])) {
                // un lago de lava tiene cientos de celdas: solo las que tocan algo transparente inundan de luz
                static const int NX[6] = { -1, 1, 0, 0, 0, 0 }, NY[6] = { 0, 0, -1, 1, 0, 0 }, NZ[6] = { 0, 0, 0, 0, -1, 1 };
                bool enclosed = true;
                for (int k = 0; k < 6 && enclosed; k++)
                    if (!isLightOpaque(typeNear(xi + NX[k], yi + NY[k], zi + NZ[k], manager, BlockType::AIR))) enclosed = false;
                if (enclosed) { setBlockLight(xi, yi, zi, (u8)level); continue; }
            }
            manager->propagateBlockLight(gridToWorldX(xi), gridToWorldY(yi), gridToWorldZ(zi), (u8)level);
        }
    }
}

void Chunk::continueEmitterReseed(const ChunkManager* manager) {
    if (emitterReseedColumn < 0 || !manager) return;
    PROFSTEP("emitters");
    int processed = 0;
    while (emitterReseedColumn < SIZE_X * SIZE_Z && processed < 1 && heavyBudgetLeft()) {
        reseedColumnEmitters(emitterReseedColumn / SIZE_Z, emitterReseedColumn % SIZE_Z, manager);
        s_heavyDidOne = true;
        processed++;
        emitterReseedColumn++;
    }
    if (emitterReseedColumn >= SIZE_X * SIZE_Z) emitterReseedColumn = -1;
}

void Chunk::reseedEmitters(const ChunkManager* manager) {
    if (!generationComplete || !manager) return;
    for (int xi = 0; xi < SIZE_X; xi++) {
        for (int zi = 0; zi < SIZE_Z; zi++) {
            reseedColumnEmitters(xi, zi, manager);
        }
    }
}

void Chunk::spreadSkyFromColumn(int xi, int zi, const ChunkManager* manager, int onlyDir) {
    static const int DIRS[4][2] = { {1, 0}, {-1, 0}, {0, 1}, {0, -1} };
    int own = skyBottom[xi][zi];
    int bx = chunkX * SIZE_X + xi;
    int bz = chunkZ * SIZE_Z + zi;

    for (int d = 0; d < 4; d++) {
        if (onlyDir >= 0 && d != onlyDir) continue;
        int nxi = xi + DIRS[d][0];
        int nzi = zi + DIRS[d][1];
        int nb;
        bool otherChunk = false;

        if (nxi >= 0 && nxi < SIZE_X && nzi >= 0 && nzi < SIZE_Z) {
            nb = skyBottom[nxi][nzi];
        } else {
            otherChunk = true;
            int ncx = chunkX + (nxi < 0 ? -1 : (nxi >= SIZE_X ? 1 : 0));
            int ncz = chunkZ + (nzi < 0 ? -1 : (nzi >= SIZE_Z ? 1 : 0));
            Chunk* nc = manager->getChunkByChunkCoord(ncx, ncz);
            if (!nc || !nc->isSkyBaseReady()) continue;
            nb = nc->getSkyBottom((nxi + SIZE_X) % SIZE_X, (nzi + SIZE_Z) % SIZE_Z);
        }

        if (nb > own) {
            // nuestra columna está iluminada donde la vecina está a oscuras
            manager->seedSkyRange(bx, bz, own, nb);
        } else if (otherChunk && own > nb) {
            // la luz del chunk vecino entra a esta columna
            manager->seedSkyRange(bx + DIRS[d][0], bz + DIRS[d][1], nb, own);
        }
    }
}

void Chunk::continueFaceCacheRecalc(const ChunkManager* manager) {
    if (!cacheRecalcPending) return;
    PROFSTEP("faces");

    // Como cada columna se resuelve con operaciones de bits, un lote
    // por frame mucho más generoso (todas las columnas del chunk de
    // una sola vez, 64 en total) sigue siendo barato
    const int COLUMN_BATCH = 16;
    int processed = 0;

    while (processed < COLUMN_BATCH) {
        if (cacheRecalcXi >= SIZE_X) {
            cacheRecalcPending = false;
            return;
        }

        if (!heavyBudgetLeft()) return;
        recomputeColumn(cacheRecalcXi, cacheRecalcZi, manager);
        s_heavyDidOne = true;
        processed++;

        cacheRecalcZi++;
        if (cacheRecalcZi >= SIZE_Z) {
            cacheRecalcZi = 0;
            cacheRecalcXi++;
        }
    }
}

bool Chunk::isForestAt(int xi, int zi) const {
    if (xi < 0 || xi >= SIZE_X) return false;
    if (zi < 0 || zi >= SIZE_Z) return false;
    return biomeColumn[xi][zi] == BiomeType::FOREST; // antes: return isForest = (...)
}

BiomeType Chunk::getBiomeAt(int xi, int zi) const {
    if (xi < 0 || xi >= SIZE_X) return BiomeType::GENERIC;
    if (zi < 0 || zi >= SIZE_Z) return BiomeType::GENERIC;
    return biomeColumn[xi][zi];
}

void Chunk::generateSpruceTree1(const ChunkManager* manager, int xi, int zi, int highestY) {
    int globalXi = chunkX * SIZE_X + xi;
    int globalZi = chunkZ * SIZE_Z + zi;
    int baseY = highestY + 1;

    int height = 7 + (int)(Noise::hashInt(globalXi, globalZi, 20) % 5);            // 7-11
    int coneTopY = height - (int)(Noise::hashInt(globalXi, globalZi, 21) % 2) - 3;
    int maxSpread = height - coneTopY;
    int maxRadius = 1 + (int)(Noise::hashInt(globalXi, globalZi, 22) % (maxSpread + 1));

    for (int dy = 0; dy < height - 1; dy++) {
        setBlockDirect(xi, baseY + dy, zi, BlockType::SPRUCE_LOG, manager);
    }

    int radius = 0;
    for (int ly = height; ly >= coneTopY; ly--) {
        int worldLy = baseY + ly;

        for (int dx = -radius; dx <= radius; dx++) {
            for (int dz = -radius; dz <= radius; dz++) {
                int nlXi = xi + dx;
                int nlZi = zi + dz;

                float worldX = gridToWorldX(nlXi);
                float worldY = gridToWorldY(worldLy);
                float worldZ = gridToWorldZ(nlZi);

                if (nlXi >= 0 && nlXi < SIZE_X && nlZi >= 0 && nlZi < SIZE_Z) {
                    if (worldLy >= 0 && worldLy < SIZE_Y && blocks[nlXi][worldLy][nlZi] == BlockType::AIR) {
                        setBlockDirect(nlXi, worldLy, nlZi, BlockType::SPRUCE_LEAVES, manager);
                    }
                } else if (manager) {
                    Chunk* neighbor = manager->getChunkAt(worldX, worldZ);
                    if (neighbor) {
                        int nXi, nYi, nZi;
                        if (neighbor->worldToGrid(worldX, worldY, worldZ, nXi, nYi, nZi)) {
                            if (neighbor->getBlockType(nXi, nYi, nZi) == BlockType::AIR) {
                                neighbor->setBlockDirect(nXi, nYi, nZi, BlockType::SPRUCE_LEAVES, manager);
                            }
                        }
                    }
                }
            }
        }

        if (radius >= 1 && ly == coneTopY + 1) {
            radius--;
        } else if (radius < maxRadius) {
            radius++;
        }
    }
}

void Chunk::generateSpruceTree2(const ChunkManager* manager, int xi, int zi, int highestY) {
    int globalXi = chunkX * SIZE_X + xi;
    int globalZi = chunkZ * SIZE_Z + zi;
    int baseY = highestY + 1;

    int height = 6 + (int)(Noise::hashInt(globalXi, globalZi, 23) % 4);            // 6-9
    int trimTop = 1 + (int)(Noise::hashInt(globalXi, globalZi, 24) % 2);
    int leafLayers = height - trimTop;
    int maxRadius = 2 + (int)(Noise::hashInt(globalXi, globalZi, 25) % 2);         // 2-3

    int radius = (int)(Noise::hashInt(globalXi, globalZi, 26) % 2);
    int growCounter = 1;
    int growFlag = 0;

    for (int ring = 0; ring <= leafLayers; ring++) {
        int worldLy = baseY + height - ring;

        for (int dx = -radius; dx <= radius; dx++) {
            for (int dz = -radius; dz <= radius; dz++) {
                int nlXi = xi + dx;
                int nlZi = zi + dz;

                float worldX = gridToWorldX(nlXi);
                float worldY = gridToWorldY(worldLy);
                float worldZ = gridToWorldZ(nlZi);

                if (nlXi >= 0 && nlXi < SIZE_X && nlZi >= 0 && nlZi < SIZE_Z) {
                    if (worldLy >= 0 && worldLy < SIZE_Y && blocks[nlXi][worldLy][nlZi] == BlockType::AIR) {
                        setBlockDirect(nlXi, worldLy, nlZi, BlockType::SPRUCE_LEAVES, manager);
                    }
                } else if (manager) {
                    Chunk* neighbor = manager->getChunkAt(worldX, worldZ);
                    if (neighbor) {
                        int nXi, nYi, nZi;
                        if (neighbor->worldToGrid(worldX, worldY, worldZ, nXi, nYi, nZi)) {
                            if (neighbor->getBlockType(nXi, nYi, nZi) == BlockType::AIR) {
                                neighbor->setBlockDirect(nXi, nYi, nZi, BlockType::SPRUCE_LEAVES, manager);
                            }
                        }
                    }
                }
            }
        }

        if (radius >= growCounter) {
            radius = growFlag;
            growFlag = 1;
            growCounter++;
            if (growCounter > maxRadius) growCounter = maxRadius;
        } else {
            radius++;
        }
    }

    int trunkTrim = (int)(Noise::hashInt(globalXi, globalZi, 27) % 3);
    int trunkHeight = height - trunkTrim;
    for (int dy = 0; dy < trunkHeight; dy++) {
        setBlockDirect(xi, baseY + dy, zi, BlockType::SPRUCE_LOG, manager);
    }
}

// ---- Roble grande (WorldGenBigTree real, porteado) ----

struct BigTreeBranch {
    int x, y, z;   // punta de la rama, donde va el racimo de hojas
    int trunkY;    // altura en el tronco de donde sale esa rama
};

struct BigTreeRng {
    unsigned int gx, gz;
    int salt;
    BigTreeRng(int gx_, int gz_, int startSalt) : gx((unsigned int)gx_), gz((unsigned int)gz_), salt(startSalt) {}
    float nextFloat() {
        unsigned int h = Noise::hashInt((int)gx, (int)gz, salt++);
        return (float)(h & 0x7fffffff) / (float)0x7fffffff;
    }
};

static const int BIGTREE_AXIS_A[3] = {2, 0, 0};
static const int BIGTREE_AXIS_B[3] = {1, 2, 1};

BlockType Chunk::bigTreeGetBlock(const ChunkManager* manager, int wx, int wy, int wz) const {
    int lyi = wy + 1;
    if (lyi < 0 || lyi >= SIZE_Y) return BlockType::AIR;

    int lxi = wx - chunkX * SIZE_X + SIZE_X / 2;
    int lzi = wz - chunkZ * SIZE_Z + SIZE_Z / 2;
    if (lxi >= 0 && lxi < SIZE_X && lzi >= 0 && lzi < SIZE_Z) {
        return blocks[lxi][lyi][lzi];
    }
    if (!manager) return BlockType::AIR;

    int ncx = chunkFloorDiv(wx + SIZE_X / 2, SIZE_X);
    int ncz = chunkFloorDiv(wz + SIZE_Z / 2, SIZE_Z);
    Chunk* nc = manager->getChunkByChunkCoord(ncx, ncz);
    if (!nc) return BlockType::AIR;
    return nc->getBlockType(wx - ncx * SIZE_X + SIZE_X / 2, lyi, wz - ncz * SIZE_Z + SIZE_Z / 2);
}

void Chunk::bigTreeSetBlock(const ChunkManager* manager, int wx, int wy, int wz, BlockType type) {
    int lyi = wy + 1;
    if (lyi < 0 || lyi >= SIZE_Y) return;

    int lxi = wx - chunkX * SIZE_X + SIZE_X / 2;
    int lzi = wz - chunkZ * SIZE_Z + SIZE_Z / 2;
    if (lxi >= 0 && lxi < SIZE_X && lzi >= 0 && lzi < SIZE_Z) {
        setBlockDirect(lxi, lyi, lzi, type, manager);
        return;
    }
    if (!manager) return;

    int ncx = chunkFloorDiv(wx + SIZE_X / 2, SIZE_X);
    int ncz = chunkFloorDiv(wz + SIZE_Z / 2, SIZE_Z);
    Chunk* nc = manager->getChunkByChunkCoord(ncx, ncz);
    if (!nc) return;
    nc->setBlockDirect(wx - ncx * SIZE_X + SIZE_X / 2, lyi, wz - ncz * SIZE_Z + SIZE_Z / 2, type, manager);
}

static float bigTreeEnvelope(float totalHeight, int layer) {
    if ((float)layer < totalHeight * 0.3f) return -1.618f;
    float half = totalHeight / 2.0f;
    float diff = totalHeight / 2.0f - (float)layer;
    float r;
    if (diff == 0.0f) r = half;
    else if (fabsf(diff) >= half) r = 0.0f;
    else r = sqrtf(half * half - diff * diff);
    return r * 0.5f;
}

static float bigTreeLeafLayerRadius(int layer, int clusterHeight) {
    if (layer < 0 || layer >= clusterHeight) return -1.0f;
    if (layer != 0 && layer != clusterHeight - 1) return 3.0f;
    return 2.0f;
}

static int bigTreeClearance(Chunk* self, const ChunkManager* manager, const int p1[3], const int p2[3]) {
    int d[3];
    int major = 0;
    for (int i = 0; i < 3; i++) {
        d[i] = p2[i] - p1[i];
        if (abs(d[i]) > abs(d[major])) major = i;
    }
    if (d[major] == 0) return -1;

    int a1 = BIGTREE_AXIS_A[major];
    int a2 = BIGTREE_AXIS_B[major];
    int dir = (d[major] > 0) ? 1 : -1;

    float slopeA1 = (float)d[a1] / (float)d[major];
    float slopeA2 = (float)d[a2] / (float)d[major];

    int endExclusive = d[major] + dir;
    int i = 0;
    int pos[3];

    while (i != endExclusive) {
        pos[major] = p1[major] + i;
        pos[a1] = (int)floorf((float)p1[a1] + (float)i * slopeA1);
        pos[a2] = (int)floorf((float)p1[a2] + (float)i * slopeA2);

        BlockType t = self->bigTreeGetBlock(manager, pos[0], pos[1], pos[2]);
        if (t != BlockType::AIR && t != BlockType::OAK_LEAVES) break;

        i += dir;
    }

    return (i == endExclusive) ? -1 : abs(i);
}

static void bigTreePlaceSphereLayer(Chunk* self, const ChunkManager* manager, int cx, int cy, int cz, float radius, int axis, BlockType blockType) {
    if (radius < 0.0f) return;
    int r = (int)(radius + 0.618f);
    int r4 = (int)(radius * radius * 4.0f + 0.5f); // (2·radio)², una sola vez
    int a1 = BIGTREE_AXIS_A[axis];
    int a2 = BIGTREE_AXIS_B[axis];

    int center[3] = {cx, cy, cz};
    int pos[3];

    for (int da = -r; da <= r; da++) {
        for (int db = -r; db <= r; db++) {
            int ea = 2 * (da < 0 ? -da : da) + 1;
            int eb = 2 * (db < 0 ? -db : db) + 1;
            if (ea * ea + eb * eb > r4) continue;

            pos[a1] = center[a1] + da;
            pos[a2] = center[a2] + db;
            pos[axis] = center[axis];

            BlockType existing = self->bigTreeGetBlock(manager, pos[0], pos[1], pos[2]);
            if (existing != BlockType::AIR && existing != BlockType::OAK_LEAVES) continue;

            self->bigTreeSetBlock(manager, pos[0], pos[1], pos[2], blockType);
        }
    }
}

static void bigTreePlaceLeafCluster(Chunk* self, const ChunkManager* manager, int x, int yBase, int z, int clusterHeight) {
    for (int dy = 0; dy < clusterHeight; dy++) {
        float radius = bigTreeLeafLayerRadius(dy, clusterHeight);
        bigTreePlaceSphereLayer(self, manager, x, yBase + dy, z, radius, 1, BlockType::OAK_LEAVES);
    }
}

static void bigTreeDrawLine(Chunk* self, const ChunkManager* manager, const int p1[3], const int p2[3], BlockType blockType) {
    int d[3];
    int major = 0;
    for (int i = 0; i < 3; i++) {
        d[i] = p2[i] - p1[i];
        if (abs(d[i]) > abs(d[major])) major = i;
    }
    if (d[major] == 0) return;

    int a1 = BIGTREE_AXIS_A[major];
    int a2 = BIGTREE_AXIS_B[major];
    int dir = (d[major] > 0) ? 1 : -1;

    float slopeA1 = (float)d[a1] / (float)d[major];
    float slopeA2 = (float)d[a2] / (float)d[major];

    int endExclusive = d[major] + dir;
    int pos[3];

    for (int i = 0; i != endExclusive; i += dir) {
        pos[major] = (int)floorf((float)(p1[major] + i) + 0.5f);
        pos[a1] = (int)floorf((float)p1[a1] + (float)i * slopeA1 + 0.5f);
        pos[a2] = (int)floorf((float)p1[a2] + (float)i * slopeA2 + 0.5f);
        self->bigTreeSetBlock(manager, pos[0], pos[1], pos[2], blockType);
    }
}

static void bigTreeComputeBranches(Chunk* self, const ChunkManager* manager, BigTreeRng& rng,
                                    int baseX, int baseY, int baseZ, float totalHeight,
                                    BigTreeBranch outBranches[], int& outCount, int maxBranches) {
    float branchTopHeight = totalHeight * 0.618f;
    if (branchTopHeight >= totalHeight) branchTopHeight = totalHeight - 1.0f;

    int branchesPerLayer = (int)(1.382f + powf(totalHeight / 13.0f, 2.0f));
    if (branchesPerLayer < 1) branchesPerLayer = 1;

    const int LEAF_CLUSTER_HEIGHT = 4;

    int y = baseY + (int)totalHeight - LEAF_CLUSTER_HEIGHT;
    int topLeafY = baseY + (int)branchTopHeight;

    outCount = 0;
    if (outCount < maxBranches) {
        outBranches[outCount].x = baseX;
        outBranches[outCount].y = y;
        outBranches[outCount].z = baseZ;
        outBranches[outCount].trunkY = topLeafY;
        outCount++;
    }
    y--;

    int layer = y - baseY;

    while (layer >= 0) {
        float envelopeRadius = bigTreeEnvelope(totalHeight, layer);
        if (envelopeRadius < 0.0f) {
            y--;
            layer--;
            continue;
        }

        for (int b = 0; b < branchesPerLayer; b++) {
            float dist = envelopeRadius * (rng.nextFloat() + 0.328f);
            float angle = rng.nextFloat() * 2.0f * 3.14159f;

            int tipX = (int)floorf(dist * sinf(angle) + (float)baseX + 0.5f);
            int tipZ = (int)floorf(dist * cosf(angle) + (float)baseZ + 0.5f);

            int tipLow[3]  = {tipX, y, tipZ};
            int tipHigh[3] = {tipX, y + LEAF_CLUSTER_HEIGHT, tipZ};

            if (bigTreeClearance(self, manager, tipLow, tipHigh) == -1) {
                int trunkPoint[3] = {baseX, baseY, baseZ};
                float horizDist = sqrtf(powf(fabsf((float)(baseX - tipX)), 2.0f) + powf(fabsf((float)(baseZ - tipZ)), 2.0f));
                float slopeDrop = horizDist * 0.381f;
                if ((float)y - slopeDrop > (float)topLeafY) {
                    trunkPoint[1] = topLeafY;
                } else {
                    trunkPoint[1] = (int)((float)y - slopeDrop);
                }

                if (bigTreeClearance(self, manager, trunkPoint, tipLow) == -1) {
                    if (outCount < maxBranches) {
                        outBranches[outCount].x = tipX;
                        outBranches[outCount].y = y;
                        outBranches[outCount].z = tipZ;
                        outBranches[outCount].trunkY = trunkPoint[1];
                        outCount++;
                    }
                }
            }
        }

        y--;
        layer--;
    }
}

static int bigTreeCheckClearance(Chunk* self, const ChunkManager* manager, int baseX, int baseY, int baseZ, int requestedHeight) {
    BlockType below = self->bigTreeGetBlock(manager, baseX, baseY - 1, baseZ);
    if (below != BlockType::GRASS && below != BlockType::DIRT) return -1;

    int p1[3] = {baseX, baseY, baseZ};
    int p2[3] = {baseX, baseY + requestedHeight - 1, baseZ};
    int hit = bigTreeClearance(self, manager, p1, p2);
    if (hit == -1) return requestedHeight;
    if (hit < 6) return -1;
    return hit;
}

bool Chunk::generateBigOakTree(const ChunkManager* manager, int xi, int zi, int highestY) {
    int globalXi = chunkX * SIZE_X + xi;
    int globalZi = chunkZ * SIZE_Z + zi;

    int baseX = (int)floorf(gridToWorldX(xi));
    int baseZ = (int)floorf(gridToWorldZ(zi));
    int baseY = (int)gridToWorldY(highestY + 1);

    BigTreeRng rng(globalXi, globalZi, 100);

    int requestedHeight = 5 + (int)(Noise::hashInt(globalXi, globalZi, 28) % 12);
    int height = bigTreeCheckClearance(this, manager, baseX, baseY, baseZ, requestedHeight);
    if (height < 0) return false;

    BigTreeBranch branches[48];
    int branchCount = 0;
    bigTreeComputeBranches(this, manager, rng, baseX, baseY, baseZ, (float)height, branches, branchCount, 48);

    for (int i = 0; i < branchCount; i++) {
        bigTreePlaceLeafCluster(this, manager, branches[i].x, branches[i].y, branches[i].z, 4);
    }

    int trunkBase[3] = {baseX, baseY, baseZ};
    int trunkTop[3]  = {baseX, baseY + height, baseZ};
    bigTreeDrawLine(this, manager, trunkBase, trunkTop, BlockType::OAK_LOG);

    for (int i = 0; i < branchCount; i++) {
        int relY = branches[i].trunkY - baseY;
        if ((float)relY < (float)height * 0.2f) continue;

        int from[3] = {baseX, branches[i].trunkY, baseZ};
        int to[3]   = {branches[i].x, branches[i].y, branches[i].z};
        bigTreeDrawLine(this, manager, from, to, BlockType::OAK_LOG);
    }

    return true;
}

void Chunk::generateTrees(const ChunkManager* manager, int startXi, int startZi, int& outXi, int& outZi, bool& done) {
    const int BATCH_SIZE = 6;

    int processed = 0;
    int xi = startXi;
    int zi = startZi;

    while (xi < SIZE_X) {
        while (zi < SIZE_Z) {
            if (treesPlacedCount >= MAX_TREES_PER_CHUNK) {
                outXi = SIZE_X;
                outZi = 0;
                done = true;
                return;
            }

            int globalXi = chunkX * SIZE_X + xi;
            int globalZi = chunkZ * SIZE_Z + zi;

            // ChunkProviderGenerate.populate: la cantidad de árboles depende del bioma (bosque, selva y taiga muchos; bosque estacional
            // la mitad; desierto, tundra y llanura ninguno; el resto casi ninguno)
            const BiomeType colBiome = biomeColumn[xi][zi];
            const int treeDenom = BiomeConfig::treeAttemptDenominator(colBiome);
            bool forest = (treeDenom > 0 && treeDenom <= 64); // biomas con bosque: los árboles guardan distancia entre sí
            bool taiga = (colBiome == BiomeType::TAIGA);

            bool attempt = treeDenom > 0 && (Noise::hashInt(globalXi, globalZi, 10) % (unsigned int)treeDenom) == 0;

            if (attempt) {
                bool skip = false;

                if (forest) {
                    unsigned int spacingRoll = Noise::hashInt(globalXi, globalZi, 11) % 3;
                    int minSpacing = 3 + (int)spacingRoll; // la copa tiene radio 2: de 3 a 5 evita que se pisen

                    for (int p = 0; p < treesPlacedCount; p++) {
                        int ddx = treesPlaced[p].xi - xi;
                        int ddz = treesPlaced[p].zi - zi;
                        if (ddx * ddx + ddz * ddz < minSpacing * minSpacing) {
                            skip = true;
                            break;
                        }
                    }
                }

                if (!skip) {
                    int highestY = getHighestSolidY(xi, zi);
                    if (highestY >= 0 && blocks[xi][highestY][zi] == BlockType::GRASS) {
                        int trunkHeight = 4 + (int)(Noise::hashInt(globalXi, globalZi, 12) % 3);

                        bool clearAbove = true;
                        for (int dy = 1; dy <= trunkHeight + 2; dy++) {
                            if (highestY + dy >= SIZE_Y) { clearAbove = false; break; }
                            BlockType aboveType = blocks[xi][highestY + dy][zi];
                            if (aboveType != BlockType::AIR && aboveType != BlockType::SNOW_LAYER) { clearAbove = false; break; }
                        }

                        if (clearAbove) {
                            if (taiga) {
                                bool useTaiga1 = (Noise::hashInt(globalXi, globalZi, 19) % 3) == 0;
                                if (useTaiga1) {
                                    generateSpruceTree1(manager, xi, zi, highestY);
                                } else {
                                    generateSpruceTree2(manager, xi, zi, highestY);
                                }

                                snowifyTreeTop(manager, xi, zi, 5, highestY + 1, 12);

                                if (treesPlacedCount < 16) {
                                    treesPlaced[treesPlacedCount].xi = xi;
                                    treesPlaced[treesPlacedCount].zi = zi;
                                    treesPlacedCount++;
                                }
                            } else {
                                // BiomeGen*.getRandomWorldGenForTrees: selva 1 de cada 3 grande, bosque algo menos, el resto 1 de cada 10
                                unsigned int bigDenom = (colBiome == BiomeType::RAINFOREST) ? 3u : ((colBiome == BiomeType::FOREST) ? 4u : 10u);
                                bool useBigTree = (Noise::hashInt(globalXi, globalZi, 28) % bigDenom) == 0;
                                if (useBigTree && generateBigOakTree(manager, xi, zi, highestY)) {
                                    snowifyTreeTop(manager, xi, zi, 6, highestY + 1, 20);

                                    if (treesPlacedCount < 16) {
                                        treesPlaced[treesPlacedCount].xi = xi;
                                        treesPlaced[treesPlacedCount].zi = zi;
                                        treesPlacedCount++;
                                    }
                                } else {
                                bool isBirch = (colBiome == BiomeType::FOREST) && (Noise::hashInt(globalXi, globalZi, 13) % 5) == 0; // solo el bosque tiene abedules (1 de cada 5)
                                BlockType logType = isBirch ? BlockType::BIRCH_LOG : BlockType::OAK_LOG;
                                BlockType leafType = isBirch ? BlockType::BIRCH_LEAVES : BlockType::OAK_LEAVES;

                                for (int dy = 1; dy <= trunkHeight; dy++) {
                                    setBlockDirect(xi, highestY + dy, zi, logType, manager);
                                }

                                int topLogY = highestY + trunkHeight;

                                for (int layerOffset = -2; layerOffset <= 1; layerOffset++) {
                                    int ly = topLogY + layerOffset;
                                    bool isTopmost = (layerOffset == 1);
                                    int radius = (layerOffset <= -1) ? 2 : 1;

                                    for (int dx = -radius; dx <= radius; dx++) {
                                        for (int dz = -radius; dz <= radius; dz++) {
                                            bool isCorner = (abs(dx) == radius && abs(dz) == radius);
                                            if (isCorner) {
                                                int nlXi2 = xi + dx;
                                                int nlZi2 = zi + dz;
                                                int cornerGlobalXi = chunkX * SIZE_X + nlXi2;
                                                int cornerGlobalZi = chunkZ * SIZE_Z + nlZi2;
                                                unsigned int cornerRoll = Noise::hashInt(cornerGlobalXi, cornerGlobalZi, 14 + ly) % 2;
                                                if (isTopmost || cornerRoll == 0) continue;
                                            }

                                            int nlXi = xi + dx;
                                            int nlZi = zi + dz;

                                            float worldX = gridToWorldX(nlXi);
                                            float worldY = gridToWorldY(ly);
                                            float worldZ = gridToWorldZ(nlZi);

                                            if (nlXi >= 0 && nlXi < SIZE_X && nlZi >= 0 && nlZi < SIZE_Z) {
                                                if (ly >= 0 && ly < SIZE_Y && blocks[nlXi][ly][nlZi] == BlockType::AIR) {
                                                    setBlockDirect(nlXi, ly, nlZi, leafType, manager);
                                                }
                                            } else if (manager) {
                                                Chunk* neighbor = manager->getChunkAt(worldX, worldZ);
                                                if (neighbor) {
                                                    int nXi, nYi, nZi;
                                                    if (neighbor->worldToGrid(worldX, worldY, worldZ, nXi, nYi, nZi)) {
                                                        if (neighbor->getBlockType(nXi, nYi, nZi) == BlockType::AIR) {
                                                            neighbor->setBlockDirect(nXi, nYi, nZi, leafType, manager);
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }

                                snowifyTreeTop(manager, xi, zi, 3, highestY + 1, trunkHeight + 3);

                                if (treesPlacedCount < 16) {
                                    treesPlaced[treesPlacedCount].xi = xi;
                                    treesPlaced[treesPlacedCount].zi = zi;
                                    treesPlacedCount++;
                                }
                            }
                        }
                    }
		    }
                }
            }

            zi++;
            processed++;
            s_heavyDidOne = true;
            if (processed >= BATCH_SIZE || !heavyBudgetLeft()) {
                outXi = xi;
                outZi = zi;
                done = false;
                return;
            }
        }
        zi = 0;
        xi++;
    }

    outXi = SIZE_X;
    outZi = 0;
    done = true;
}

// ---------------------------------------------------------------------
// Vetas de mena: port de WorldGenMinable (Beta 1.7.3)
// ---------------------------------------------------------------------
static const float ORE_SIN32[33] = {
0.000000f,0.098017f,0.195090f,0.290285f,0.382683f,0.471397f,0.555570f,0.634393f,0.707107f,0.773010f,0.831470f,0.881921f,0.923880f,0.956940f,0.980785f,0.995185f,1.000000f,0.995185f,0.980785f,0.956940f,0.923880f,0.881921f,0.831470f,0.773010f,0.707107f,0.634393f,0.555570f,0.471397f,0.382683f,0.290285f,0.195090f,0.098017f,0.000000f
};
static const float ORE_COS32[32] = {
1.000000f,0.995185f,0.980785f,0.956940f,0.923880f,0.881921f,0.831470f,0.773010f,0.707107f,0.634393f,0.555570f,0.471397f,0.382683f,0.290285f,0.195090f,0.098017f,0.000000f,-0.098017f,-0.195090f,-0.290285f,-0.382683f,-0.471397f,-0.555570f,-0.634393f,-0.707107f,-0.773010f,-0.831470f,-0.881921f,-0.923880f,-0.956940f,-0.980785f,-0.995185f
};

struct OreRng {
    unsigned int s;
    unsigned int next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
};

static inline int oreFloorInt(float v) {
    int i = (int)v;
    return (v < (float)i) ? i - 1 : i;
}

// Una línea de n+1 esferas entre dos puntos, con radio (sin(i*pi/n)+1)*r0+1 y
// r0 = rand * n/16. Solo se colocan los bloques que caen DENTRO del chunk destino
// (ox, oz = su origen en bloques globales), así cada chunk puede evaluar también
// las vetas que empiezan en sus vecinos y todas quedan completas, sin costuras.
//   n    = tamaño de la veta (4, 8, 16 o 32: la tabla de senos usa 32/n)
//   yBase  = se suma a la altura de origen (arcilla: arranca cerca del fondo del agua)
//   yExtra = si es mayor que 0, se suma un segundo dado rand(yExtra) a la altura de origen
//   srcW = ancho de un chunk (de donde sale el origen de la veta)
//   clipW/clipD = ventana donde se colocan bloques (el chunk destino)
// Devuelve la cantidad de pruebas de celda hechas (0 = veta descartada de entrada).
template <typename PlaceFn>
static int oreVeinBlob(unsigned int seed, int n, const float* sinTab, int yRange, int yExtra, int yBase, int srcX, int srcZ, int srcW,
                        int ox, int oz, int clipW, int sizeY, int clipD,
                        int topY, PlaceFn place) {
    OreRng rng;
    rng.s = seed | 1u;
    for (int w = 0; w < 4; w++) rng.next(); // descarta los primeros valores

    int angleIdx = (int)(rng.next() % 32u);
    float sn = ORE_SIN32[angleIdx];
    float cs = ORE_COS32[angleIdx];
    float half = (float)n * 0.125f; // n / 8

    float cx = (float)(srcX * srcW + (int)(rng.next() % (unsigned)srcW));
    float cz = (float)(srcZ * srcW + (int)(rng.next() % (unsigned)srcW));
    int baseY = yBase + (int)(rng.next() % (unsigned)yRange);
    if (yExtra > 0) baseY += (int)(rng.next() % (unsigned)yExtra);   // dos dados (lapislazuli): la altura se concentra en el medio

    float x1 = cx + sn * half, x2 = cx - sn * half;
    float z1 = cz + cs * half, z2 = cz - cs * half;
    float y1 = (float)(baseY + (int)(rng.next() % 3u) + 2);
    float y2 = (float)(baseY + (int)(rng.next() % 3u) + 2);

    // Descarte rápido: radio máximo posible = ((2n/16)+1)/2
    float rMaxHalf = ((float)n * 0.125f + 1.0f) * 0.5f;
    float xmin = x1 < x2 ? x1 : x2, xmax = x1 < x2 ? x2 : x1;
    float zmin = z1 < z2 ? z1 : z2, zmax = z1 < z2 ? z2 : z1;
    float ymin = y1 < y2 ? y1 : y2;
    if (oreFloorInt(xmax + rMaxHalf) < ox || oreFloorInt(xmin - rMaxHalf) >= ox + clipW) return 0;
    if (oreFloorInt(zmax + rMaxHalf) < oz || oreFloorInt(zmin - rMaxHalf) >= oz + clipD) return 0;
    if (oreFloorInt(ymin - rMaxHalf) > topY) return 0; // toda la veta por encima del terreno

    int tests = 0;

    for (int i = 0; i <= n; i++) {
        float t = (float)i / (float)n;
        float ccx = x1 + (x2 - x1) * t;
        float ccy = y1 + (y2 - y1) * t;
        float ccz = z1 + (z2 - z1) * t;

        float r0 = (float)(rng.next() & 0xFFFFu) * (1.0f / 65536.0f) * (float)n * 0.0625f; // rand * n/16
        float r = (sinTab[i] + 1.0f) * r0 + 1.0f;   // sinTab[i] = sin(i * pi / n)
        float hr = r * 0.5f;
        float inv = 1.0f / hr;

        int minX = oreFloorInt(ccx - hr), maxX = oreFloorInt(ccx + hr);
        int minY = oreFloorInt(ccy - hr), maxY = oreFloorInt(ccy + hr);
        int minZ = oreFloorInt(ccz - hr), maxZ = oreFloorInt(ccz + hr);

        if (minX < ox) minX = ox;
        if (maxX > ox + clipW - 1) maxX = ox + clipW - 1;
        if (minZ < oz) minZ = oz;
        if (maxZ > oz + clipD - 1) maxZ = oz + clipD - 1;
        if (minY < 0) minY = 0;
        if (maxY > sizeY - 1) maxY = sizeY - 1;

        for (int x = minX; x <= maxX; x++) {
            float dx = ((float)x + 0.5f - ccx) * inv;
            float dx2 = dx * dx;
            if (dx2 >= 1.0f) continue;
            for (int y = minY; y <= maxY; y++) {
                float dy = ((float)y + 0.5f - ccy) * inv;
                float dxy2 = dx2 + dy * dy;
                if (dxy2 >= 1.0f) continue;
                for (int z = minZ; z <= maxZ; z++) {
                    float dz = ((float)z + 0.5f - ccz) * inv;
                    tests++;
                    if (dxy2 + dz * dz < 1.0f) {
                        place(x - ox, y, z - oz);
                    }
                }
            }
        }
    }
    return tests;
}

void Chunk::generateOres(const ChunkManager* manager, int startAttempt, int& outAttempt, bool& done) {
    // Menas como en Beta 1.7.3 (ChunkProviderGenerate.populate + WorldGenMinable). Intentos por chunk de 16x16 en el original; aqui los chunks
    // son de 8x8, o sea 1/4 de eso por origen (lo que es menos de 1 se resuelve con "odds": 1 de cada N origenes intenta una veta):
    //   carbon 20 (tam. 16, altura 0-127), hierro 20 (tam. 8, 0-63), oro 2 (tam. 8, 0-31), redstone 8 (tam. 7, 0-15),
    //   diamante 1 (tam. 7, 0-15), lapislazuli 1 (tam. 6, rand(16)+rand(16)).
    // Alturas: carbon, hierro y oro escaladas al nivel del mar de este terreno (38 en vez de 63); redstone, diamante y lapislazuli quedan
    // junto a la bedrock, sin escalar, como el diamante.
    static float sinTab16[17], sinTab8[9], sinTab32[33];             // sin(i * pi / n), a partir de la tabla de 32 pasos
    static bool sinInit = false;
    if (!sinInit) {
        for (int i = 0; i <= 16; i++) sinTab16[i] = ORE_SIN32[i * 2];
        for (int i = 0; i <= 8; i++)  sinTab8[i] = ORE_SIN32[i * 4];
        for (int i = 0; i <= 32; i++) sinTab32[i] = ORE_SIN32[i];
        sinInit = true;
    }
    static const float sinTab7[8] = { 0.000000f, 0.433884f, 0.781831f, 0.974928f, 0.974928f, 0.781831f, 0.433884f, 0.000000f };
    static const float sinTab6[7] = { 0.000000f, 0.500000f, 0.866025f, 1.000000f, 0.866025f, 0.500000f, 0.000000f };

    enum OreKind { KIND_NORMAL, KIND_CLAY };   // la arcilla solo reemplaza arena bajo el agua
    struct OreDef { BlockType type; int size; int yRange; int yExtra; int count; int odds; const float* sinTab; int yBase; OreKind kind; };
    // El orden y el indice de cada mena fijan la semilla: carbon 0, hierro 1 y diamante 2 no cambian (los chunks ya generados siguen igual)
    const OreDef ORES[9] = {
        { BlockType::COAL_ORE,     16, 77, 0, 5, 1, sinTab16, 0, KIND_NORMAL },
        { BlockType::IRON_ORE,      8, 39, 0, 5, 1, sinTab8,  0, KIND_NORMAL },
        { BlockType::DIAMOND_ORE,   7, 16, 0, 1, 4, sinTab7,  0, KIND_NORMAL },
        { BlockType::GOLD_ORE,      8, 19, 0, 1, 2, sinTab8,  0, KIND_NORMAL },
        { BlockType::REDSTONE_ORE,  7, 16, 0, 2, 1, sinTab7,  0, KIND_NORMAL },
        { BlockType::LAPIS_ORE,     6, 16, 16, 1, 4, sinTab6, 0, KIND_NORMAL },
        // Parches de tierra (20 por chunk de 16x16, tam. 32) y de grava (10), a cualquier altura bajo tierra (0-127 escalado a 77)
        { BlockType::DIRT,         32, 77, 0, 5, 1, sinTab32, 0, KIND_NORMAL },
        { BlockType::GRAVEL,       32, 77, 0, 5, 2, sinTab32, 0, KIND_NORMAL },
        // Arcilla (WorldGenClay, tam. 32): en el original 10 intentos que solo valen si el origen cae dentro del agua (~1,2 por chunk de
        // 16x16 con agua). Aqui 1 de cada 3 origenes, con la altura entre el fondo y la superficie del agua, y solo reemplaza arena bajo agua.
        { BlockType::CLAY,         32, 17, 0, 1, 3, sinTab32, TERRAIN_SEA_LEVEL - 17, KIND_CLAY },
    };
    const int UNITS_PER_SRC = 5 + 5 + 1 + 1 + 2 + 1 + 5 + 5 + 1;   // suma de count
    const int UNITS = 9 * UNITS_PER_SRC;               // 9 chunks de origen
    const int MAX_VEINS_PER_CALL = 1;          // vetas con trabajo real por llamada

    int worked = 0;
    int unit = startAttempt;

    while (unit < UNITS) {
        int src = unit / UNITS_PER_SRC;
        int rest = unit % UNITS_PER_SRC;
        int ore = 0, k = rest;
        while (k >= ORES[ore].count) { k -= ORES[ore].count; ore++; }
        const OreDef& def = ORES[ore];

        int srcX = chunkX + (src % 3) - 1;
        int srcZ = chunkZ + (src / 3) - 1;
        unsigned int seed = Noise::hashInt(srcX * 92821 + k * 12007, srcZ * 68917 + ore * 7919, 3100 + ore);
        const BlockType oreType = def.type;

        // este origen no intenta veta (el diamante conserva su sal original, 3300)
        if (def.odds > 1 && Noise::hashInt(srcX, srcZ, 3300 + (ore == 2 ? 0 : ore)) % (unsigned)def.odds != 0u) { unit++; continue; }

        int tests = oreVeinBlob(seed, def.size, def.sinTab, def.yRange, def.yExtra, def.yBase,
                                srcX, srcZ, SIZE_X,
                                chunkX * SIZE_X, chunkZ * SIZE_Z, SIZE_X, SIZE_Y, SIZE_Z,
                                highestGeneratedY,
                                [&](int xi, int yi, int zi) {
                                    if (def.kind == KIND_CLAY) {
                                        // WorldGenClay: solo arena, y con agua encima (hasta 4 celdas de arena/arcilla de por medio)
                                        if (blocks[xi][yi][zi] != BlockType::SAND) return;
                                        int up = yi + 1;
                                        while (up < SIZE_Y && up <= yi + 4 && (blocks[xi][up][zi] == BlockType::SAND || blocks[xi][up][zi] == BlockType::CLAY)) up++;
                                        if (up >= SIZE_Y || !isWaterBlock(blocks[xi][up][zi])) return;
                                        setBlockDirect(xi, yi, zi, oreType, manager);
                                    } else if (blocks[xi][yi][zi] == BlockType::STONE) {
                                        setBlockDirect(xi, yi, zi, oreType, manager);
                                    }
                                });
        unit++;

        if (tests > 0) {
            worked++;
            s_heavyDidOne = true;
            if (worked >= MAX_VEINS_PER_CALL || !heavyBudgetLeft()) {
                outAttempt = unit;
                done = false;
                return;
            }
        }
    }

    outAttempt = UNITS;
    done = true;
}

void Chunk::generateCactus(const ChunkManager* manager, int startXi, int startZi, int& outXi, int& outZi, bool& done) {
    const int BATCH_SIZE = 8;

    int processed = 0;
    int xi = startXi, zi = startZi;

    while (xi < SIZE_X) {
        while (zi < SIZE_Z) {
            if (biomeColumn[xi][zi] == BiomeType::DESERT) {
                int globalXi = chunkX * SIZE_X + xi;
                int globalZi = chunkZ * SIZE_Z + zi;

                unsigned int attempt = Noise::hashInt(globalXi, globalZi, 300) % 40;
                if (attempt == 0) {
                    int highestY = getHighestSolidY(xi, zi);
                    if (highestY >= 0 && blocks[xi][highestY][zi] == BlockType::SAND) {
                        unsigned int heightRoll = Noise::hashInt(globalXi, globalZi, 301) % 4;
                        int cactusHeight = 1 + (int)heightRoll;

                        bool clear = true;
                        for (int dy = 1; dy <= cactusHeight; dy++) {
                            if (highestY + dy >= SIZE_Y || blocks[xi][highestY + dy][zi] != BlockType::AIR) {
                                clear = false;
                                break;
                            }
                        }

                        if (clear) {
                            const int cdx[4] = {1, -1, 0, 0};
                            const int cdz[4] = {0, 0, 1, -1};
                            bool neighborsClear = true;

                            for (int i = 0; i < 4 && neighborsClear; i++) {
                                int nxi = xi + cdx[i];
                                int nzi = zi + cdz[i];

                                if (nxi >= 0 && nxi < SIZE_X && nzi >= 0 && nzi < SIZE_Z) {
                                    if (blocks[nxi][highestY + 1][nzi] != BlockType::AIR) {
                                        neighborsClear = false;
                                    }
                                } else if (manager) {
                                    float checkWorldX = gridToWorldX(nxi);
                                    float checkWorldY = gridToWorldY(highestY + 1);
                                    float checkWorldZ = gridToWorldZ(nzi);
                                    if (manager->isBlockPresentAt(checkWorldX, checkWorldY, checkWorldZ)) {
                                        neighborsClear = false;
                                    }
                                }
                            }

                            if (neighborsClear) {
                                for (int dy = 1; dy <= cactusHeight; dy++) {
                                    setBlockDirect(xi, highestY + dy, zi, BlockType::CACTUS, manager);
                                }
                            }
                        }
                    }
                }
            }

            zi++;
            processed++;
            s_heavyDidOne = true;
            if (processed >= BATCH_SIZE || !heavyBudgetLeft()) {
                outXi = xi;
                outZi = zi;
                done = false;
                return;
            }
        }
        zi = 0;
        xi++;
    }

    outXi = SIZE_X;
    outZi = 0;
    done = true;
}

// Bloque en una posición local de este chunk, que puede caer en un chunk vecino. 'unknown' es lo que se devuelve
// si el vecino no está cargado.
BlockType Chunk::typeNear(int xi, int yi, int zi, const ChunkManager* manager, BlockType unknown) const {
    if (yi < 0) return BlockType::BEDROCK;
    if (yi >= SIZE_Y) return BlockType::AIR;
    if (xi >= 0 && xi < SIZE_X && zi >= 0 && zi < SIZE_Z) return blocks[xi][yi][zi];
    if (!manager) return unknown;

    float wx = gridToWorldX(xi), wy = gridToWorldY(yi), wz = gridToWorldZ(zi);
    Chunk* n = manager->getChunkAt(wx, wz);
    if (!n || !n->isGenerationComplete()) return unknown;
    int nx, ny, nz;
    if (!n->worldToGrid(wx, wy, wz, nx, ny, nz)) return unknown;
    return n->blocks[nx][ny][nz];
}

// BlockTorch.onNeighborBlockChange: la antorcha se cae si no le queda ningún apoyo o si le falta SU apoyo.
// Un vecino sin cargar se da por apoyado, para que no se caigan antorchas en el borde de la ventana de chunks.
void Chunk::checkTorchSupport(int xi, int yi, int zi, const ChunkManager* manager) {
    BlockType t = blocks[xi][yi][zi];
    if (!isTorchBlock(t)) return;

    if (t == BlockType::TORCH) { // metadata 0: todavía sin orientar
        reorientLegacyTorch(xi, yi, zi, manager);
        return;
    }

    auto at = [&](int x, int y, int z) { return typeNear(x, y, z, manager, BlockType::STONE); };
    if (!Torch::mustDrop(at, xi, yi, zi, torchMeta(t))) return;

    float dropX = gridToWorldX(xi);
    float dropY = gridToWorldY(yi);
    float dropZ = gridToWorldZ(zi);

    breakBlockAt(xi, yi, zi, manager);
    PendingDrops::push(dropX, dropY, dropZ, BlockType::TORCH);
}

// BlockTorch.updateTick / onBlockAdded para metadata 0: se orienta con la prioridad -X, +X, -Z, +Z, piso;
// si no tiene ningún apoyo, se cae. Si algún vecino que hay que mirar no está cargado todavía, se decide más tarde.
void Chunk::reorientLegacyTorch(int xi, int yi, int zi, const ChunkManager* manager) {
    if (blocks[xi][yi][zi] != BlockType::TORCH) return;

    auto atAir   = [&](int x, int y, int z) { return typeNear(x, y, z, manager, BlockType::AIR); };
    auto atStone = [&](int x, int y, int z) { return typeNear(x, y, z, manager, BlockType::STONE); };
    int metaA = Torch::autoMeta(atAir, xi, yi, zi);
    int metaS = Torch::autoMeta(atStone, xi, yi, zi);
    if (metaA != metaS) return; // depende de un chunk sin cargar

    if (metaA == 0) { // dropTorchIfCantStay
        float dropX = gridToWorldX(xi);
        float dropY = gridToWorldY(yi);
        float dropZ = gridToWorldZ(zi);
        breakBlockAt(xi, yi, zi, manager);
        PendingDrops::push(dropX, dropY, dropZ, BlockType::TORCH);
        return;
    }
    setBlockDirect(xi, yi, zi, torchFromMeta(metaA), manager);
}

// ItemBlock.onItemUse + BlockTorch.canPlaceBlockAt/onBlockAdded/onBlockPlaced. (xi,yi,zi) es la celda DESTINO de
// esta antorcha y faceCode la cara clickeada (0 abajo, 1 arriba, 2 -Z, 3 +Z, 4 -X, 5 +X).
bool Chunk::tryPlaceTorch(int xi, int yi, int zi, int faceCode, const ChunkManager* manager) {
    if (xi < 0 || xi >= SIZE_X || yi < 0 || yi >= SIZE_Y || zi < 0 || zi >= SIZE_Z) return false;

    // World.canBlockBePlacedAt: en el destino solo se reemplaza aire, agua o la capa de nieve
    BlockType here = blocks[xi][yi][zi];
    if (here != BlockType::AIR && !isLiquidBlock(here) && here != BlockType::SNOW_LAYER && here != BlockType::FIRE) return false;

    auto at = [&](int x, int y, int z) { return typeNear(x, y, z, manager, BlockType::AIR); };
    if (!Torch::canPlaceAt(at, xi, yi, zi)) return false;

    int meta = Torch::autoMeta(at, xi, yi, zi);
    meta = Torch::metaForPlacement(at, xi, yi, zi, faceCode, meta);
    setBlockDirect(xi, yi, zi, torchFromMeta(meta), manager);
    notifyLiquidNeighbors(xi, yi, zi, manager);
    return true;
}

// (xi,yi,zi) es el bloque CLICKEADO. Calcula la celda destino igual que ItemBlock (la nieve se reemplaza en su
// lugar y cuenta como cara 0) y la delega al chunk que la contiene.
bool Chunk::placeTorchFromClick(int xi, int yi, int zi, BlockFace face, const ChunkManager* manager) {
    if (xi >= 0 && xi < SIZE_X && yi >= 0 && yi < SIZE_Y && zi >= 0 && zi < SIZE_Z &&
        blocks[xi][yi][zi] == BlockType::SNOW_LAYER) {
        return tryPlaceTorch(xi, yi, zi, 0, manager);
    }

    int targetXi = xi, targetYi = yi, targetZi = zi, faceCode;
    switch (face) {
        case BlockFace::BOTTOM: targetYi -= 1; faceCode = 0; break;
        case BlockFace::TOP:    targetYi += 1; faceCode = 1; break;
        case BlockFace::NORTH:  targetZi -= 1; faceCode = 2; break;
        case BlockFace::SOUTH:  targetZi += 1; faceCode = 3; break;
        case BlockFace::WEST:   targetXi -= 1; faceCode = 4; break;
        case BlockFace::EAST:   targetXi += 1; faceCode = 5; break;
        default: return false;
    }
    if (targetYi < 0 || targetYi >= SIZE_Y) return false;

    if (targetXi >= 0 && targetXi < SIZE_X && targetZi >= 0 && targetZi < SIZE_Z) {
        return tryPlaceTorch(targetXi, targetYi, targetZi, faceCode, manager);
    }

    if (!manager) return false;
    float worldX = gridToWorldX(targetXi), worldY = gridToWorldY(targetYi), worldZ = gridToWorldZ(targetZi);
    Chunk* neighbor = manager->getChunkAt(worldX, worldZ);
    if (!neighbor || !neighbor->isGenerationComplete()) return false;
    int nXi, nYi, nZi;
    if (!neighbor->worldToGrid(worldX, worldY, worldZ, nXi, nYi, nZi)) return false;
    return neighbor->tryPlaceTorch(nXi, nYi, nZi, faceCode, manager);
}

static void shiftUp1(const unsigned int in[4], unsigned int out[4]) {
    out[0] = in[0] << 1;
    out[1] = (in[1] << 1) | (in[0] >> 31);
    out[2] = (in[2] << 1) | (in[1] >> 31);
    out[3] = (in[3] << 1) | (in[2] >> 31);
}

static void shiftDown1(const unsigned int in[4], unsigned int out[4]) {
    out[3] = in[3] >> 1;
    out[2] = (in[2] >> 1) | (in[3] << 31);
    out[1] = (in[1] >> 1) | (in[2] << 31);
    out[0] = (in[0] >> 1) | (in[1] << 31);
}

void Chunk::rebuildAllOccupancy() {
    for (int xi = 0; xi < SIZE_X; xi++) {
        for (int zi = 0; zi < SIZE_Z; zi++) {
            rebuildColumnOccupancy(xi, zi);
        }
    }
}

void Chunk::rebuildColumnOccupancy(int xi, int zi) {
    occupancy[xi][zi][0] = 0;
    occupancy[xi][zi][1] = 0;
    occupancy[xi][zi][2] = 0;
    occupancy[xi][zi][3] = 0;

    for (int yi = 0; yi < SIZE_Y; yi++) {
        BlockType t = blocks[xi][yi][zi];
        bool solid = (t != BlockType::AIR && t != BlockType::TALLGRASS &&
                      !isLiquidBlock(t) && !isTorchBlock(t) && t != BlockType::FIRE && t != BlockType::PORTAL && t != BlockType::SNOW_LAYER &&
                      t != BlockType::DEAD_BUSH && !isCrossPlant(t) && !isPartialBlock(t) &&
                      t != BlockType::GLASS && t != BlockType::ICE);
	if (solid) {
            occupancy[xi][zi][yi / 32] |= (1u << (yi % 32));
        }
    }
}

void Chunk::computeColumnFacesBitwise(int xi, int zi, const ChunkManager* manager) {
    unsigned int* occ = occupancy[xi][zi];

    unsigned int aboveMask[4], belowMask[4];
    shiftDown1(occ, aboveMask); // bit yi = ocupación real de yi+1
    shiftUp1(occ, belowMask);   // bit yi = ocupación real de yi-1

    unsigned int topMask[4], bottomMask[4];
    for (int w = 0; w < 4; w++) {
        topMask[w]    = occ[w] & ~aboveMask[w];
        bottomMask[w] = occ[w] & ~belowMask[w];
    }
    bottomMask[0] &= ~1u; // yi=0 nunca muestra cara inferior, regla existente

    // Vecino en la misma dirección — dentro del chunk, o consultando
    // al ChunkManager si estamos justo en el borde
    unsigned int northNeighbor[4] = {0,0,0,0};
    unsigned int southNeighbor[4] = {0,0,0,0};
    unsigned int eastNeighbor[4]  = {0,0,0,0};
    unsigned int westNeighbor[4]  = {0,0,0,0};

    if (zi - 1 >= 0) {
        for (int w = 0; w < 4; w++) northNeighbor[w] = occupancy[xi][zi - 1][w];
    } else if (manager) {
        float wx = gridToWorldX(xi), wz = gridToWorldZ(zi) - 1.0f;
        manager->getColumnOccupancy(wx, wz, northNeighbor);
    }

    if (zi + 1 < SIZE_Z) {
        for (int w = 0; w < 4; w++) southNeighbor[w] = occupancy[xi][zi + 1][w];
    } else if (manager) {
        float wx = gridToWorldX(xi), wz = gridToWorldZ(zi) + 1.0f;
        manager->getColumnOccupancy(wx, wz, southNeighbor);
    }

    if (xi + 1 < SIZE_X) {
        for (int w = 0; w < 4; w++) eastNeighbor[w] = occupancy[xi + 1][zi][w];
    } else if (manager) {
        float wx = gridToWorldX(xi) + 1.0f, wz = gridToWorldZ(zi);
        manager->getColumnOccupancy(wx, wz, eastNeighbor);
    }

    if (xi - 1 >= 0) {
        for (int w = 0; w < 4; w++) westNeighbor[w] = occupancy[xi - 1][zi][w];
    } else if (manager) {
        float wx = gridToWorldX(xi) - 1.0f, wz = gridToWorldZ(zi);
        manager->getColumnOccupancy(wx, wz, westNeighbor);
    }

    unsigned int northMask[4], southMask[4], eastMask[4], westMask[4];
    for (int w = 0; w < 4; w++) {
        northMask[w] = occ[w] & ~northNeighbor[w];
        southMask[w] = occ[w] & ~southNeighbor[w];
        eastMask[w]  = occ[w] & ~eastNeighbor[w];
        westMask[w]  = occ[w] & ~westNeighbor[w];
    }

    // Rango de columna: CUALQUIER bloque no-aire cuenta (no solo los
    // que tienen cara calculada por bits) — así agua/tallgrass/antorcha
    // que queden por encima del terreno sólido normal no se pierden
    columnMinY[xi][zi] = SIZE_Y;
    columnMaxY[xi][zi] = -1;
    for (int yi = 0; yi < SIZE_Y; yi++) {
        if (blocks[xi][yi][zi] != BlockType::AIR) {
            if (yi < columnMinY[xi][zi]) columnMinY[xi][zi] = yi;
            if (yi > columnMaxY[xi][zi]) columnMaxY[xi][zi] = yi;
        }
    }

    // Volcamos el resultado a faceCache (mantiene compatible el resto
    // del código que ya lo consume) y a sideFaceMask (para drawGreedy)
    for (int yi = 0; yi < SIZE_Y; yi++) {
        if (isTorchBlock(blocks[xi][yi][zi]) || blocks[xi][yi][zi] == BlockType::FIRE || blocks[xi][yi][zi] == BlockType::PORTAL) {
            FaceMask tf = {};
            tf.top = true;
            faceCache[xi][yi][zi] = tf;
            continue;
        }

        int w = yi / 32;
        unsigned int bit = (1u << (yi % 32));

        FaceMask f;
        f.top    = (topMask[w]    & bit) != 0;
        f.bottom = (bottomMask[w] & bit) != 0;
        f.north  = (northMask[w]  & bit) != 0;
        f.south  = (southMask[w]  & bit) != 0;
        f.east   = (eastMask[w]   & bit) != 0;
        f.west   = (westMask[w]   & bit) != 0;

        faceCache[xi][yi][zi] = f;
    }

    auto isWaterFaceVisible = [](BlockType neighborType) {
        return neighborType == BlockType::AIR || neighborType == BlockType::TALLGRASS || isTorchBlock(neighborType) || neighborType == BlockType::FIRE || neighborType == BlockType::PORTAL;
    };

    for (int yi = columnMinY[xi][zi]; yi <= columnMaxY[xi][zi] && yi >= 0; yi++) {
        if (blocks[xi][yi][zi] != BlockType::WATER) continue;

        FaceMask wf;
        float worldX = gridToWorldX(xi);
        float worldY = gridToWorldY(yi);
        float worldZ = gridToWorldZ(zi);

        wf.top    = (yi + 1 >= SIZE_Y) || isWaterFaceVisible(blocks[xi][yi + 1][zi]);
        wf.bottom = (yi == 0) ? false : isWaterFaceVisible(blocks[xi][yi - 1][zi]);

        if (zi + 1 < SIZE_Z) wf.south = isWaterFaceVisible(blocks[xi][yi][zi + 1]);
        else if (manager) wf.south = isWaterFaceVisible(manager->getBlockTypeAt(worldX, worldY, worldZ + 1.0f));
        else wf.south = true;

        if (zi - 1 >= 0) wf.north = isWaterFaceVisible(blocks[xi][yi][zi - 1]);
        else if (manager) wf.north = isWaterFaceVisible(manager->getBlockTypeAt(worldX, worldY, worldZ - 1.0f));
        else wf.north = true;

        if (xi + 1 < SIZE_X) wf.east = isWaterFaceVisible(blocks[xi + 1][yi][zi]);
        else if (manager) wf.east = isWaterFaceVisible(manager->getBlockTypeAt(worldX + 1.0f, worldY, worldZ));
        else wf.east = true;

        if (xi - 1 >= 0) wf.west = isWaterFaceVisible(blocks[xi - 1][yi][zi]);
        else if (manager) wf.west = isWaterFaceVisible(manager->getBlockTypeAt(worldX - 1.0f, worldY, worldZ));
        else wf.west = true;

        faceCache[xi][yi][zi] = wf;
    }

    // Caras del vidrio: se ven contra todo lo que NO sea un cubo opaco, y nunca contra otro vidrio
    auto isGlassFaceVisible = [](BlockType self, BlockType n) {
        if (n == self) return false;   // nunca contra el mismo material (vidrio con vidrio, hielo con hielo)
        switch (n) {
            case BlockType::GLASS:
            case BlockType::ICE:
            case BlockType::WEB:
            case BlockType::AIR:
            case BlockType::TALLGRASS:
            case BlockType::WATER:
            case BlockType::TORCH:
            case BlockType::TORCH_W:
            case BlockType::TORCH_E:
            case BlockType::TORCH_N:
            case BlockType::TORCH_S:
            case BlockType::TORCH_F:
            case BlockType::FIRE:
            case BlockType::PORTAL:
            case BlockType::SNOW_LAYER:
            case BlockType::DEAD_BUSH:
            case BlockType::ROSE:
            case BlockType::DANDELION:
            case BlockType::SAPLING_OAK: case BlockType::SAPLING_SPRUCE: case BlockType::SAPLING_BIRCH:
            case BlockType::MUSHROOM_RED:
            case BlockType::MUSHROOM_BROWN:
            case BlockType::SUGAR_CANE:
            case BlockType::OAK_LEAVES:
            case BlockType::BIRCH_LEAVES:
            case BlockType::SPRUCE_LEAVES:
            case BlockType::CACTUS:
                return true;
            default:
                return isPartialBlock(n); // puertas, trampilla y cama no tapan el vidrio; un bloque opaco si
        }
    };

    for (int yi = columnMinY[xi][zi]; yi <= columnMaxY[xi][zi] && yi >= 0; yi++) {
        const BlockType glassSelf = blocks[xi][yi][zi];
        if (glassSelf != BlockType::GLASS && glassSelf != BlockType::ICE) continue;

        FaceMask gf;
        float worldX = gridToWorldX(xi);
        float worldY = gridToWorldY(yi);
        float worldZ = gridToWorldZ(zi);

        gf.top    = (yi + 1 >= SIZE_Y) || isGlassFaceVisible(glassSelf, blocks[xi][yi + 1][zi]);
        gf.bottom = (yi == 0) ? false : isGlassFaceVisible(glassSelf, blocks[xi][yi - 1][zi]);

        if (zi + 1 < SIZE_Z) gf.south = isGlassFaceVisible(glassSelf, blocks[xi][yi][zi + 1]);
        else if (manager) gf.south = isGlassFaceVisible(glassSelf, manager->getBlockTypeAt(worldX, worldY, worldZ + 1.0f));
        else gf.south = true;

        if (zi - 1 >= 0) gf.north = isGlassFaceVisible(glassSelf, blocks[xi][yi][zi - 1]);
        else if (manager) gf.north = isGlassFaceVisible(glassSelf, manager->getBlockTypeAt(worldX, worldY, worldZ - 1.0f));
        else gf.north = true;

        if (xi + 1 < SIZE_X) gf.east = isGlassFaceVisible(glassSelf, blocks[xi + 1][yi][zi]);
        else if (manager) gf.east = isGlassFaceVisible(glassSelf, manager->getBlockTypeAt(worldX + 1.0f, worldY, worldZ));
        else gf.east = true;

        if (xi - 1 >= 0) gf.west = isGlassFaceVisible(glassSelf, blocks[xi - 1][yi][zi]);
        else if (manager) gf.west = isGlassFaceVisible(glassSelf, manager->getBlockTypeAt(worldX - 1.0f, worldY, worldZ));
        else gf.west = true;

        faceCache[xi][yi][zi] = gf;
    }
}

void Chunk::getColumnOccupancyBits(int xi, int zi, unsigned int outBits[4]) const {
    if (xi < 0 || xi >= SIZE_X || zi < 0 || zi >= SIZE_Z) {
        outBits[0] = outBits[1] = outBits[2] = outBits[3] = 0xFFFFFFFF; // fuera de rango = sólido, conservador
        return;
    }
    outBits[0] = occupancy[xi][zi][0];
    outBits[1] = occupancy[xi][zi][1];
    outBits[2] = occupancy[xi][zi][2];
    outBits[3] = occupancy[xi][zi][3];
}

void Chunk::recomputeColumn(int xi, int zi, const ChunkManager* manager) {
    markDisplayListDirty();
    rebuildColumnOccupancy(xi, zi);
    computeColumnFacesBitwise(xi, zi, manager);
}

void Chunk::recomputeColumnRange(int xi, int zi, int yiLo, int yiHi, const ChunkManager* manager) {
    markBandRangeDirty(yiLo, yiHi);
    rebuildColumnOccupancy(xi, zi);
    computeColumnFacesBitwise(xi, zi, manager);
}