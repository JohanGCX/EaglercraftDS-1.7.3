#include "Liquid.h"
#include "Redstone.h"
#include "PartialBlocks.h"
#include "FallingBlocks.h"
#include "Teleporter.h"
#include "PendingDrops.h"
#include "ChunkManager.h"
#include "Trees.h"
#include "Governor.h"
#include "WorldConfig.h"
#include "BlockType.h"
#include "Profiler.h"
#include "Caves.h"
#include "Options.h"
#include <malloc.h>
#include <new>
#include <math.h>
#include <stdio.h>

static inline int floorDiv(int a, int b) {
    int d = a / b;
    int r = a % b;
    if (r != 0 && ((r < 0) != (b < 0))) d--;
    return d;
}

// Which chunks of the square window are loaded. In dsi (radius 3) the 4 corner chunks are skipped: from the centre chunk they are ~34 blocks away,
// past where the fog reaches full density (85 % of the 25-block render distance), so they were generated, lit and kept in RAM without ever being seen.
// The window becomes a rounded square of 45 chunks instead of 49 (about 124 KB of RAM and their generation time saved). The other modes keep the square:
// their corners are close enough to be visible.
static inline bool cellInWindow(int dx, int dz, int r) {
    if (dx < -r || dx > r || dz < -r || dz > r) return false;
    if (r >= 3 && (dx == -r || dx == r) && (dz == -r || dz == r)) return false;
    return true;
}

ChunkManager::ChunkManager(unsigned int seed_, int startChunkX, int startChunkZ) {
    seed = seed_;
    Caves::reset(seed); // caché de cuevas nuevo para este mundo
    skyPendingCount = 0; // la luz pendiente del mundo anterior no vale para este
    Liquid::resetTicks(); // sin ticks de líquidos pendientes de otro mundo
    FallingBlocks::clear(); // ni bloques cayendo del mundo anterior

    // Modo de chunks de Options (fast / ds / dsi). El dsi solo vale en una DSi; si no alcanza la RAM para el pozo de chunks
    // (o queda muy poca libre despues) se baja al modo anterior.
    int mode = Options::get().chunkMode;
    if (mode > 2) mode = 1;
    if (mode == 2 && !isDSiMode()) mode = 1;
    for (;;) {
        const int radius = (mode == 0) ? 1 : (mode == 1 ? 2 : 3);
        windowRadius = radius;
        windowSide = 2 * radius + 1;
        windowCount = 0;
        for (int dx = -radius; dx <= radius; dx++) for (int dz = -radius; dz <= radius; dz++) if (cellInWindow(dx, dz, radius)) windowCount++;
        maxChunks = windowCount + (radius > 1 ? 3 : 7);
        poolSize = maxChunks + POOL_SPARE;

        poolCount = 0;
        for (int p = 0; p < poolSize; p++) {
            Chunk* ch = new (std::nothrow) Chunk(0, 0, seed);
            if (!ch) break;
            pool[poolCount] = ch;
            poolInUse[poolCount] = false;
            poolCount++;
        }
        struct mallinfo mi = mallinfo();
        const long freeKB = ((long)(getHeapLimit() - getHeapEnd()) + (long)mi.fordblks) / 1024;
        const long needKB = (mode == 2) ? 700 : 0;   // margen para las listas de dibujo
        if (mode == 0 || (poolCount >= poolSize && freeKB >= needKB)) break;
        for (int p = 0; p < poolCount; p++) delete pool[p];
        poolCount = 0;
        mode--;
    }
    WorldConfig::setChunkMode(windowRadius, mode == 0 ? 1 : (mode == 1 ? 3 : 4), 1);   // dsi: 4 columnas por cuadro con el mismo tiempo que ds (el doble estiraba los cuadros)

    for (int i = 0; i < maxChunks; i++) {
        chunks[i] = nullptr;
        slotUsed[i] = false;
        wasComplete[i] = false;
        awaitingLoad[i] = false;
        loadingBiome[i] = false;
        borderRefreshPending[i] = false;
    }

    centerIndex = -1;
    lastUsedChunk = nullptr;

    fillPending = windowRadius > 1;   // el resto de la ventana se pide en el primer update tras terminar el 3 x 3
    for (int dx = -1; dx <= 1; dx++) {
        for (int dz = -1; dz <= 1; dz++) {
            int slot = findOrCreateSlot(startChunkX + dx, startChunkZ + dz);
            if (dx == 0 && dz == 0) {
                centerIndex = slot;
            }
        }
    }
}

ChunkManager::~ChunkManager() {
    // chunks[] y la cola de guardado apuntan dentro del pool: se borra solo el pool
    for (int p = 0; p < poolCount; p++) {
        delete pool[p];
    }
}

int ChunkManager::worldToChunkCoord(float worldPos) const {
    return (int)floorf(worldPos / WorldConfig::SIZE_X + 0.5f);
}

Chunk* ChunkManager::acquireChunk(int cx, int cz) {
    for (int attempt = 0; attempt < 2; attempt++) {
        for (int p = 0; p < poolCount; p++) {
            if (!poolInUse[p]) {
                poolInUse[p] = true;
                pool[p]->~Chunk();                 // libera sus display lists
                new (pool[p]) Chunk(cx, cz, seed); // reusa la MISMA memoria
                return pool[p];
            }
        }
        flushOnePendingSave(); // no hay libres: se guarda uno ya y se reutiliza
    }
    return nullptr;
}

void ChunkManager::releaseChunk(Chunk* c) {
    if (lastUsedChunk == c) lastUsedChunk = nullptr;
    if (lastLightChunk == c) lastLightChunk = nullptr;
    for (int p = 0; p < poolCount; p++) {
        if (pool[p] == c) {
            poolInUse[p] = false;
            c->freeDisplayLists();
            return;
        }
    }
}

void ChunkManager::flushOnePendingSave() {
    PROFSTEP("chunk save");
    if (pendingSaveCount <= 0) return;
    pendingSaveCount--;
    Chunk* toSave = pendingSaveChunks[pendingSaveCount];
    char oldFilename[96];
    toSave->getSaveFilename(oldFilename);
    toSave->saveToFile(oldFilename);
    releaseChunk(toSave);
}

int ChunkManager::findOrCreateSlot(int cx, int cz, const int* protectX, const int* protectZ, int protectCount) {
    // Si ya existe, retornamos su índice
    for (int i = 0; i < maxChunks; i++) {
        if (slotUsed[i] && chunkPosX[i] == cx && chunkPosZ[i] == cz) {
            return i;
        }
    }

    // Buscamos un slot libre
    for (int i = 0; i < maxChunks; i++) {
        if (!slotUsed[i]) {
            chunks[i] = acquireChunk(cx, cz);
            if (!chunks[i]) return -1;

            chunkPosX[i] = cx;
            chunkPosZ[i] = cz;
            slotUsed[i] = true;

            // La carga de archivo se ENCOLA, no ocurre aquí — evita
            // ráfagas de varias aperturas de archivo en el mismo frame
            if (pendingLoadCount < maxChunks) {
                pendingLoadSlots[pendingLoadCount] = i;
                pendingLoadCount++;
		awaitingLoad[i] = true; loadingBiome[i] = false; 
		borderRefreshPending[i] = false;
            }

            return i;
        }
    }

    // No hay slots libres: liberamos el chunk cargado más lejano del
    // nuevo centro (cx, cz), asumiendo que ya no es vecino relevante
    int farthestIndex = -1;
    int farthestDistSq = -1;

    for (int i = 0; i < maxChunks; i++) {
        if (i == centerIndex) continue;

        bool isProtected = false;
        for (int p = 0; p < protectCount; p++) {
            if (chunkPosX[i] == protectX[p] && chunkPosZ[i] == protectZ[p]) { isProtected = true; break; }
        }
        if (isProtected) continue;

        int ddx = chunkPosX[i] - cx;
        int ddz = chunkPosZ[i] - cz;
        int distSq = ddx * ddx + ddz * ddz;

        if (distSq > farthestDistSq) {
            farthestDistSq = distSq;
            farthestIndex = i;
        }
    }

    if (farthestIndex == -1) return -1;

    if (lastUsedChunk == chunks[farthestIndex]) {
        lastUsedChunk = nullptr;
    }
    if (lastLightChunk == chunks[farthestIndex]) {
        lastLightChunk = nullptr;
    }

    // Si tiene cambios sin guardar, el guardado real se DIFIERE (cola
    // de abajo, uno por frame) en vez de escribirse acá mismo — evitar
    // ráfagas de varias escrituras de archivo en el mismo frame,
    // mismo motivo que ya tiene la cola de carga.
    if (chunks[farthestIndex]->isModified() && pendingSaveCount < maxChunks) {
        pendingSaveChunks[pendingSaveCount++] = chunks[farthestIndex];
    } else {
        releaseChunk(chunks[farthestIndex]);
    }

    // El chunk nuevo se crea YA (barato, sin E/S) — igual que el
    // camino de slot libre, arranca a generar terreno fresco de
    // inmediato mientras su archivo guardado (si existe) se carga
    // más adelante, en cola, sin bloquear este frame.
    chunks[farthestIndex] = acquireChunk(cx, cz);
    if (!chunks[farthestIndex]) { slotUsed[farthestIndex] = false; return -1; }

    if (pendingLoadCount < maxChunks) {
        pendingLoadSlots[pendingLoadCount] = farthestIndex;
        pendingLoadCount++;
        awaitingLoad[farthestIndex] = true; loadingBiome[farthestIndex] = false;
        borderRefreshPending[farthestIndex] = false;
    }

    chunkPosX[farthestIndex] = cx;
    chunkPosZ[farthestIndex] = cz;
    wasComplete[farthestIndex] = false;

    return farthestIndex;
}

void ChunkManager::update(float playerX, float playerZ) {
    auto needsHeavyWork = [this](int i) {
	if (awaitingLoad[i]) return false;
        return !chunks[i]->isGenerationComplete() || chunks[i]->isCacheRecalcPending() || chunks[i]->isInitialSkyLightPending() || chunks[i]->isEmitterReseedPending();
    };
    if (centerIndex >= 0) {
        int ccx = chunkPosX[centerIndex];
        int ccz = chunkPosZ[centerIndex];
        for (int i = 0; i < maxChunks; i++) {
            if (!slotUsed[i]) continue;
            int dx = chunkPosX[i] - ccx;
            int dz = chunkPosZ[i] - ccz;
            chunks[i]->setInWindow(cellInWindow(dx, dz, windowRadius));
        }
    }

    if (pendingLoadCount > 0) {
        pendingLoadCount--;
        int i = pendingLoadSlots[pendingLoadCount];

        if (slotUsed[i]) {
            char filename[96];
            Chunk::getSaveFilenameFor(chunkPosX[i], chunkPosZ[i], seed, filename);
            if (chunks[i]->loadFromFile(filename)) {   // etapa 1: abrir y leer. Bioma y colores se calculan despues, repartidos (abajo)
                loadingBiome[i] = true;                 // awaitingLoad sigue en true hasta que termine
            } else {
                awaitingLoad[i] = false;                // sin archivo: el chunk se genera de la semilla
            }
        }
    }

    if (pendingSaveCount > 0) {
        flushOnePendingSave();
    }

    // Cuántos chunks distintos reciben su turno normal de trabajo
    // este frame — 1 es muy lento en total, 9 (todos) causa los
    // picos originales. Ajustable a prueba y error.
    const int MAX_CONCURRENT_HEAVY_CHUNKS = 3;

    int processedHeavy = 0;
    Chunk::beginHeavyFrame();
    Caves::beginFrame();

    // Un chunk recien leido del disco termina su bioma/colores en varios cuadros (el mas cercano primero); los demas esperan su turno
    if (!Governor::skipHeavy()) {
        const int ccx = centerIndex >= 0 ? chunkPosX[centerIndex] : 0;
        const int ccz = centerIndex >= 0 ? chunkPosZ[centerIndex] : 0;
        int best = -1, bestKey = 0x7FFFFFFF;
        for (int i = 0; i < maxChunks; i++) {
            if (!slotUsed[i] || !loadingBiome[i]) continue;
            const int ddx = chunkPosX[i] - ccx, ddz = chunkPosZ[i] - ccz;
            const int key = ddx * ddx + ddz * ddz;
            if (key < bestKey) { bestKey = key; best = i; }
        }
        if (best >= 0 && chunks[best]->continueLoad()) {
            loadingBiome[best] = false;
            awaitingLoad[best] = false;
            chunks[best]->beginFullFaceCacheRecalc();
        }
    }

    // Primero los chunks más cercanos al jugador: al llegar a terreno nuevo, el de debajo de los pies se completa antes que los del borde
    // de la ventana (antes se rotaba entre todos por igual y con 25 chunks el de los pies tardaba mucho)
    int heavyOrder[CAP_CHUNKS], heavyKey[CAP_CHUNKS], heavyCount = 0;
    {
        const int ccx = centerIndex >= 0 ? chunkPosX[centerIndex] : 0;
        const int ccz = centerIndex >= 0 ? chunkPosZ[centerIndex] : 0;
        for (int i = 0; i < maxChunks; i++) {
            if (!slotUsed[i] || !needsHeavyWork(i)) continue;
            const int ddx = chunkPosX[i] - ccx, ddz = chunkPosZ[i] - ccz;
            const int key = ddx * ddx + ddz * ddz;
            int p = heavyCount++;
            while (p > 0 && heavyKey[p - 1] > key) { heavyKey[p] = heavyKey[p - 1]; heavyOrder[p] = heavyOrder[p - 1]; p--; }
            heavyKey[p] = key; heavyOrder[p] = i;
        }
    }

    for (int k = 0; k < heavyCount; k++) {
        int i = heavyOrder[k];
        if (processedHeavy >= MAX_CONCURRENT_HEAVY_CHUNKS || Governor::skipHeavy()) continue;   // Governor: tras un cuadro lento se salta el trabajo pesado
        if (!Chunk::heavyBudgetLeft()) break;

        chunks[i]->update(this);
        processedHeavy++;

        bool isCompleteNow = chunks[i]->isGenerationComplete();
        if (isCompleteNow && !wasComplete[i]) {
            int cx = chunkPosX[i];
            int cz = chunkPosZ[i];

            int neighborOffsets[4][2] = {{1,0}, {-1,0}, {0,1}, {0,-1}};
            for (int n = 0; n < 4; n++) {
                for (int j = 0; j < maxChunks; j++) {
                    if (slotUsed[j] &&
                        chunkPosX[j] == cx + neighborOffsets[n][0] &&
                        chunkPosZ[j] == cz + neighborOffsets[n][1] &&
                        chunks[j]->isGenerationComplete()) {
                        borderRefreshPending[j] = true;   // antes: los 4 vecinos de golpe en este cuadro (~9 ms cada uno)
                    }
                }
            }
        }
        wasComplete[i] = isCompleteNow;
    }

    if (!Governor::skipHeavy()) continueSkyFlood();

    // Un vecino por cuadro (28 columnas, ~9 ms). Varias terminaciones seguidas sobre el mismo vecino se resuelven con un solo recálculo.
    if (!Governor::skipHeavy()) {
        for (int k = 0; k < maxChunks; k++) {
            int j = (heavyWorkRotationStart + k) % maxChunks;
            if (!slotUsed[j] || !borderRefreshPending[j]) continue;
            if (chunks[j]->isGenerationComplete()) {
                PROFSTEP("neighbors");
                if (chunks[j]->refreshBorderFaces(this, 7)) borderRefreshPending[j] = false;   // 7 columnas por cuadro (antes las 28 de golpe: ~12-19 ms)
                break;
            }
            borderRefreshPending[j] = false;
        }
    }

    // Chunks SIN trabajo pesado (ya completos, sin recálculo
    // pendiente) igual reciben su update normal, es barato
    for (int i = 0; i < maxChunks; i++) {
        if (!slotUsed[i]) continue;
        if (needsHeavyWork(i)) continue; // ya se procesó arriba si le tocaba
        // Un chunk que espera su carga no cuenta como "con trabajo pesado" (needsHeavyWork da false), pero tampoco es un chunk terminado: llamarle
        // update acá lo hacía generar columnas fuera del presupuesto, uno por chunk nuevo (3 al cruzar un borde: cuadros de 38 a 50 ms)
        if (awaitingLoad[i]) continue;

        chunks[i]->update(this);
        wasComplete[i] = chunks[i]->isGenerationComplete();
    }

    heavyWorkRotationStart = (heavyWorkRotationStart + 1) % maxChunks;

    // Histéresis: en vez de recentrar apenas el chunk "matemático" del
    // jugador cambia (que puede oscilar frame a frame si está parado
    // justo en el límite entre dos chunks), solo recentramos cuando el
    // jugador se alejó claramente del centro actual — más de medio
    // chunk MÁS un margen. Evita el parpadeo de expulsar/recrear
    // chunks en cadena al caminar justo sobre un borde.
    const float RECENTER_MARGIN = 1.0f;

    bool needsRecenter = (centerIndex == -1);
    if (fillPending && isGenerationComplete()) {   // el 3 x 3 inicial ya está: se crea el resto de la ventana (ya con el juego en marcha)
        fillPending = false;
        needsRecenter = true;
    }
    if (!needsRecenter) {
        float currentCenterX = chunkPosX[centerIndex] * WorldConfig::SIZE_X;
        float currentCenterZ = chunkPosZ[centerIndex] * WorldConfig::SIZE_Z;
        needsRecenter = fabsf(playerX - currentCenterX) > (WorldConfig::SIZE_X / 2.0f + RECENTER_MARGIN) ||
                        fabsf(playerZ - currentCenterZ) > (WorldConfig::SIZE_Z / 2.0f + RECENTER_MARGIN);
    }

    if (needsRecenter) {
        int playerChunkX = worldToChunkCoord(playerX);
        int playerChunkZ = worldToChunkCoord(playerZ);

        // Calculamos las 9 posiciones objetivo (ventana 3x3, radio 1)
        // ANTES de pedir ninguna, así ningún findOrCreateSlot
        // individual puede expulsar un chunk que otra de las 9 va a
        // necesitar dentro de este mismo recentrado.
        int targetX[CAP_CHUNKS], targetZ[CAP_CHUNKS];
        int idx = 0;
        for (int dx = -windowRadius; dx <= windowRadius; dx++) {
            for (int dz = -windowRadius; dz <= windowRadius; dz++) {
                if (!cellInWindow(dx, dz, windowRadius)) continue;   // dsi: no corners
                targetX[idx] = playerChunkX + dx;
                targetZ[idx] = playerChunkZ + dz;
                idx++;
            }
        }

        for (int t = 0; t < windowCount; t++) {
            int slot = findOrCreateSlot(targetX[t], targetZ[t], targetX, targetZ, windowCount);
            if (targetX[t] == playerChunkX && targetZ[t] == playerChunkZ) {
                centerIndex = slot;
            }
        }
    }
}

void ChunkManager::setSkylightSubtract(int v) {
    if (v == skylightSubtract) return;
    skylightSubtract = v;
    for (int i = 0; i < maxChunks; i++) {
        if (slotUsed[i]) chunks[i]->markLightDirty();
    }
}

Chunk* ChunkManager::getChunkAt(float worldX, float worldZ) const {
    int cx = worldToChunkCoord(worldX);
    int cz = worldToChunkCoord(worldZ);

    if (lastUsedChunk && lastUsedChunkX == cx && lastUsedChunkZ == cz) {
        return lastUsedChunk;
    }

    for (int i = 0; i < maxChunks; i++) {
        if (slotUsed[i] && chunkPosX[i] == cx && chunkPosZ[i] == cz) {
            lastUsedChunk = chunks[i];
            lastUsedChunkX = cx;
            lastUsedChunkZ = cz;
            return chunks[i];
        }
    }

    return nullptr;
}

bool ChunkManager::isGenerationComplete() const {
    for (int i = 0; i < maxChunks; i++) {
        if (slotUsed[i] && !chunks[i]->isGenerationComplete()) {
            return false;
        }
    }
    return true;
}

bool ChunkManager::isSolidAt(float x, float y, float z) const {
    Chunk* c = getChunkAt(x, z);

    if (!c) return true;
    if (!c->isGenerationComplete()) return true;

    return c->isSolidAt(x, y, z);
}

void ChunkManager::drawAll(float playerX, float playerY, float playerZ, float lookDirX, float lookDirY, float lookDirZ) {
    const float CHUNK_RADIUS = 6.0f;
    const float maxDist = WorldConfig::renderDistance() + CHUNK_RADIUS;
    const float maxDistSq = maxDist * maxDist;

    // Juntamos los índices de los chunks que van a dibujarse este
    // frame junto con su distancia al jugador, para poder ordenarlos
    // de más cerca a más lejos: así, si el presupuesto de polígonos
    // se llena a mitad de frame (chequeo dentro de Chunk::draw), lo
    // que se pierde son los chunks más LEJANOS, no bloques al azar
    // cerca del jugador.
    int drawOrder[CAP_CHUNKS];
    float drawDistSq[CAP_CHUNKS];
    int drawCount = 0;

    for (int i = 0; i < maxChunks; i++) {
        if (!slotUsed[i]) continue;
        if (!chunks[i]->isGenerationComplete()) continue;
        if (chunks[i]->isCacheRecalcPending() && !chunks[i]->hasBuiltDisplayList()) continue;

        float centerX = chunkPosX[i] * WorldConfig::SIZE_X;
        float centerZ = chunkPosZ[i] * WorldConfig::SIZE_Z;

        float dx = centerX - playerX;
        float dz = centerZ - playerZ;
        float distSq = dx * dx + dz * dz;

        if (distSq > maxDistSq) continue;

        // Para decidir INCLUSIÓN usamos distancia real al jugador
        // (arriba), pero para el ORDEN usamos distancia en grilla de
        // chunks al centro actual — son números enteros que solo
        // cambian en un recentrado real (ya protegido por histéresis),
        // así que el orden queda estable frame a frame y no se ve
        // afectado por el bamboleo normal de cámara/colisión, que es
        // lo que causaba que el chunk cortado por el presupuesto
        // cambiara sin ningún motivo real.
        int gdx = chunkPosX[i] - chunkPosX[centerIndex];
        int gdz = chunkPosZ[i] - chunkPosZ[centerIndex];
        float sortKey = (float)(gdx * gdx + gdz * gdz);

        drawOrder[drawCount] = i;
        drawDistSq[drawCount] = sortKey;
        drawCount++;
    }

    // Insertion sort por distancia ascendente — con máximo 9 chunks
    // no hace falta nada más elaborado.
    for (int a = 1; a < drawCount; a++) {
        int idx = drawOrder[a];
        float dist = drawDistSq[a];
        int b = a - 1;
        while (b >= 0 && drawDistSq[b] > dist) {
            drawOrder[b + 1] = drawOrder[b];
            drawDistSq[b + 1] = drawDistSq[b];
            b--;
        }
        drawOrder[b + 1] = idx;
        drawDistSq[b + 1] = dist;
    }

    int vertBudget = Options::get().renderQuality == 2 ? 7000 : (Options::get().renderQuality == 1 ? 11000 : 15000); // deja margen para brazo/mirilla/overlay/ítems tirados
    int rebuildBandBudget = Governor::skipHeavy() ? 0 : 64; // 64 ya no limita en la práctica: manda el presupuesto de tiempo; 0 = Governor salta las listas este cuadro
    Chunk::beginRebuildFrame();

    for (int k = 0; k < drawCount; k++) {
        int i = drawOrder[k];
        chunks[i]->draw(playerX, playerY, playerZ, lookDirX, lookDirY, lookDirZ, this, &vertBudget, &rebuildBandBudget);
    }
}

void ChunkManager::refreshNeighborFace(float worldX, float worldY, float worldZ) const {
    Chunk* c = getChunkAt(worldX, worldZ);
    if (!c) return;

    int xi, yi, zi;
    if (!c->worldToGrid(worldX, worldY, worldZ, xi, yi, zi)) return;

    c->recomputeColumn(xi, zi, this);
}

Chunk* ChunkManager::getChunkByChunkCoord(int cx, int cz) const {
    if (lastLightChunk && lastLightChunkX == cx && lastLightChunkZ == cz) {
        return lastLightChunk;
    }
    for (int i = 0; i < maxChunks; i++) {
        if (slotUsed[i] && chunkPosX[i] == cx && chunkPosZ[i] == cz) {
            lastLightChunk = chunks[i];
            lastLightChunkX = cx;
            lastLightChunkZ = cz;
            return chunks[i];
        }
    }
    return nullptr;
}

void ChunkManager::floodFillSkyLight(LightNode* seeds, int seedCount) const {
    static const int OFFSETS[6][3] = {
        {1,0,0}, {-1,0,0}, {0,1,0}, {0,-1,0}, {0,0,1}, {0,0,-1}
    };

    // Chunks de 8 x 8: el chunk y la posición local salen con desplazamientos (floorDiv eran 12 divisiones por software en cada paso) y se
    // recuerda el último chunk consultado, porque casi todos los vecinos caen en el mismo
    static_assert(WorldConfig::SIZE_X == 8 && WorldConfig::SIZE_Z == 8, "floodFillSkyLight asume chunks de 8 x 8");
    int cacheCx = 0x7FFFFFFF, cacheCz = 0;
    Chunk* cacheChunk = nullptr;

    int head = 0, tail = 0;
    for (int s = 0; s < seedCount && tail < LIGHT_QUEUE_CAPACITY; s++) {
        lightQueue[tail++] = seeds[s];
    }

    int steps = 0;
    while (head != tail) {
        if (++steps > SKY_FLOOD_STEPS_PER_CALL) break; // tanda agotada: lo que queda se guarda abajo para los próximos cuadros
        LightNode node = lightQueue[head++];
        if (head >= LIGHT_QUEUE_CAPACITY) head = 0;

        if (node.level <= 1) continue;

        for (int d = 0; d < 6; d++) {
            int nx = node.bx + OFFSETS[d][0];
            int ny = node.by + OFFSETS[d][1];
            int nz = node.bz + OFFSETS[d][2];
            if (ny < 0 || ny >= WorldConfig::SIZE_Y) continue;

            int ncx = nx >> 3;   // piso, también con negativos (desplazamiento aritmético)
            int ncz = nz >> 3;
            Chunk* nc;
            if (ncx == cacheCx && ncz == cacheCz) {
                nc = cacheChunk;
            } else {
                nc = getChunkByChunkCoord(ncx, ncz);
                cacheCx = ncx; cacheCz = ncz; cacheChunk = nc;
            }
            if (!nc) continue;

            int lxi = nx & 7;
            int lzi = nz & 7;

            const BlockType nbType = nc->getBlockType(lxi, ny, lzi);
            if (isLightOpaque(nbType)) continue;

            const int dimmed = (int)node.level - 1 - lightDim(nbType);
            if (dimmed <= 0) continue;
            u8 newLevel = (u8)dimmed;
            if (nc->getSkyLight(lxi, ny, lzi) >= newLevel) continue;

            nc->setSkyLight(lxi, ny, lzi, newLevel);

            int nextTail = (tail + 1) % LIGHT_QUEUE_CAPACITY;
            if (nextTail == head) continue;
            lightQueue[tail] = { nx, ny, nz, newLevel };
            tail = nextTail;
        }
    }

    // Lo que no alcanzó en esta tanda queda pendiente (si el pendiente se llena, se pierde, como antes con el corte)
    while (head != tail && skyPendingCount < LIGHT_QUEUE_CAPACITY) {
        skyPending[skyPendingCount++] = lightQueue[head];
        head = (head + 1) % LIGHT_QUEUE_CAPACITY;
    }
}

void ChunkManager::continueSkyFlood() const {
    if (skyPendingCount == 0) return;
    PROFSTEP("pending light");
    const int TAKE = 64;
    LightNode seeds[TAKE];
    int take = skyPendingCount < TAKE ? skyPendingCount : TAKE;
    for (int i = 0; i < take; i++) seeds[i] = skyPending[skyPendingCount - take + i];
    skyPendingCount -= take;
    floodFillSkyLight(seeds, take);   // lo que sobre vuelve a quedar pendiente
}

void ChunkManager::seedSkyRange(int bx, int bz, int yFrom, int yTo) const {
    if (yFrom < 0) yFrom = 0;
    if (yTo > WorldConfig::SIZE_Y) yTo = WorldConfig::SIZE_Y;
    if (yFrom >= yTo) return;

    int cx = floorDiv(bx, WorldConfig::SIZE_X);
    int cz = floorDiv(bz, WorldConfig::SIZE_Z);
    Chunk* c = getChunkByChunkCoord(cx, cz);
    if (!c) return;

    int lxi = bx - cx * WorldConfig::SIZE_X;
    int lzi = bz - cz * WorldConfig::SIZE_Z;

    LightNode seeds[WorldConfig::SIZE_Y];
    int n = 0;
    for (int y = yFrom; y < yTo; y++) {
        u8 lvl = c->getSkyLight(lxi, y, lzi);
        if (lvl > 1) seeds[n++] = { bx, y, bz, lvl };
    }
    floodFillSkyLight(seeds, n);
}

void ChunkManager::propagateSkyColumn(float worldX, float worldZ) const {
    Chunk* c = getChunkAt(worldX, worldZ);
    if (!c) return;

    int xi, yi, zi;
    float topY = c->gridToWorldY(WorldConfig::SIZE_Y - 1);
    if (!c->worldToGrid(worldX, topY, worldZ, xi, yi, zi)) return;

    int bx = c->getChunkX() * WorldConfig::SIZE_X + xi;
    int bz = c->getChunkZ() * WorldConfig::SIZE_Z + zi;

    static const int OFFSETS[6][3] = {
        {1,0,0}, {-1,0,0}, {0,1,0}, {0,-1,0}, {0,0,1}, {0,0,-1}
    };

    LightNode seeds[WorldConfig::SIZE_Y];
    int seedCount = 0;
    int relightCount = 0;
    int relightProbes = 0; // presupuesto de sondeos a vecinos, no de celdas
    bool blocked = false;
    int colLevel = 15;

    for (int y = WorldConfig::SIZE_Y - 1; y >= 0; y--) {
        if (!blocked && isLightOpaque(c->getBlockType(xi, y, zi))) {
            blocked = true;
        }

        if (!blocked) {
            colLevel -= lightDim(c->getBlockType(xi, y, zi));   // agua y hielo oscurecen
            if (colLevel < 0) colLevel = 0;
            c->setSkyLight(xi, y, zi, (u8)colLevel);
            seeds[seedCount++] = { bx, y, bz, (u8)colLevel };
        } else {
            u8 oldLevel = c->getSkyLight(xi, y, zi);
            if (oldLevel > 0) {
                c->setSkyLight(xi, y, zi, 0);

                if (relightProbes < MAX_FLOOD_STEPS_PER_CALL) {
                    for (int d = 0; d < 6; d++) {
                        relightProbes++;
                        int nx = bx + OFFSETS[d][0];
                        int ny = y + OFFSETS[d][1];
                        int nz = bz + OFFSETS[d][2];
                        if (ny < 0 || ny >= WorldConfig::SIZE_Y) continue;

                        int ncx = floorDiv(nx, WorldConfig::SIZE_X);
                        int ncz = floorDiv(nz, WorldConfig::SIZE_Z);
                        Chunk* nc = getChunkByChunkCoord(ncx, ncz);
                        if (!nc) continue;

                        int lxi = nx - ncx * WorldConfig::SIZE_X;
                        int lzi = nz - ncz * WorldConfig::SIZE_Z;

                        u8 neighborLevel = nc->getSkyLight(lxi, ny, lzi);
                        if (neighborLevel > 0 && relightCount < LIGHT_QUEUE_CAPACITY) {
                            relightSeeds[relightCount++] = { nx, ny, nz, neighborLevel };
                        }
                    }
                }
            }
        }
    }

    floodFillSkyLight(seeds, seedCount);
    floodFillSkyLight(relightSeeds, relightCount);
}

void ChunkManager::relightNewAirCell(float worldX, float worldY, float worldZ) const {
    Chunk* c = getChunkAt(worldX, worldZ);
    if (!c) return;

    int xi, yi, zi;
    if (!c->worldToGrid(worldX, worldY, worldZ, xi, yi, zi)) return;

    int bx = c->getChunkX() * WorldConfig::SIZE_X + xi;
    int bz = c->getChunkZ() * WorldConfig::SIZE_Z + zi;

    static const int OFFSETS[6][3] = {
        {1,0,0}, {-1,0,0}, {0,1,0}, {0,-1,0}, {0,0,1}, {0,0,-1}
    };

    u8 bestSky = 0, bestBlock = 0;
    for (int d = 0; d < 6; d++) {
        int nx = bx + OFFSETS[d][0];
        int ny = yi + OFFSETS[d][1];
        int nz = bz + OFFSETS[d][2];
        if (ny < 0 || ny >= WorldConfig::SIZE_Y) continue;

        int ncx = floorDiv(nx, WorldConfig::SIZE_X);
        int ncz = floorDiv(nz, WorldConfig::SIZE_Z);
        Chunk* nc = getChunkByChunkCoord(ncx, ncz);
        if (!nc) continue;

        int lxi = nx - ncx * WorldConfig::SIZE_X;
        int lzi = nz - ncz * WorldConfig::SIZE_Z;

        u8 s = nc->getSkyLight(lxi, ny, lzi);
        u8 bl = nc->getBlockLight(lxi, ny, lzi);
        if (s > bestSky) bestSky = s;
        if (bl > bestBlock) bestBlock = bl;
    }

    // "Heredar" del vecino más brillante, con el decaimiento normal
    // de -1, y desde ahí inundar hacia afuera con el mecanismo ya
    // existente — así la luz entra de costado por la nueva abertura,
    // no solo desde arriba.
    if (bestSky > 1) {
        u8 newLevel = bestSky - 1;
        if (c->getSkyLight(xi, yi, zi) < newLevel) {
            c->setSkyLight(xi, yi, zi, newLevel);
            LightNode seed = { bx, yi, bz, newLevel };
            floodFillSkyLight(&seed, 1);
        }
    }
    if (bestBlock > 1) {
        u8 newLevel = bestBlock - 1;
        if (c->getBlockLight(xi, yi, zi) < newLevel) {
            c->setBlockLight(xi, yi, zi, newLevel);
            LightNode seed = { bx, yi, bz, newLevel };
            floodFillBlockLight(&seed, 1);
        }
    }
}

void ChunkManager::removeSkyLightAt(float worldX, float worldY, float worldZ) const {
    Chunk* startChunk = getChunkAt(worldX, worldZ);
    if (!startChunk) return;

    int xi, yi, zi;
    if (!startChunk->worldToGrid(worldX, worldY, worldZ, xi, yi, zi)) return;

    u8 oldLevel = startChunk->getSkyLight(xi, yi, zi);
    if (oldLevel == 0) return;

    int bx = startChunk->getChunkX() * WorldConfig::SIZE_X + xi;
    int bz = startChunk->getChunkZ() * WorldConfig::SIZE_Z + zi;

    startChunk->setSkyLight(xi, yi, zi, 0);

    static const int OFFSETS[6][3] = {
        {1,0,0}, {-1,0,0}, {0,1,0}, {0,-1,0}, {0,0,1}, {0,0,-1}
    };

    int head = 0, tail = 0;
    lightQueue[tail++] = { bx, yi, bz, oldLevel };
    int relightCount = 0;

    int steps = 0;
    while (head != tail) {
        if (++steps > MAX_FLOOD_STEPS_PER_CALL) break;
        LightNode node = lightQueue[head++];
        if (head >= LIGHT_QUEUE_CAPACITY) head = 0;

        for (int d = 0; d < 6; d++) {
            int nx = node.bx + OFFSETS[d][0];
            int ny = node.by + OFFSETS[d][1];
            int nz = node.bz + OFFSETS[d][2];
            if (ny < 0 || ny >= WorldConfig::SIZE_Y) continue;

            int ncx = floorDiv(nx, WorldConfig::SIZE_X);
            int ncz = floorDiv(nz, WorldConfig::SIZE_Z);
            Chunk* nc = getChunkByChunkCoord(ncx, ncz);
            if (!nc) continue;

            int lxi = nx - ncx * WorldConfig::SIZE_X;
            int lzi = nz - ncz * WorldConfig::SIZE_Z;

            u8 neighborLevel = nc->getSkyLight(lxi, ny, lzi);
            if (neighborLevel == 0) continue;

            if (neighborLevel < node.level) {
                nc->setSkyLight(lxi, ny, lzi, 0);

                int nextTail = (tail + 1) % LIGHT_QUEUE_CAPACITY;
                if (nextTail == head) continue;
                lightQueue[tail] = { nx, ny, nz, neighborLevel };
                tail = nextTail;
            } else if (relightCount < LIGHT_QUEUE_CAPACITY) {
                relightSeeds[relightCount++] = { nx, ny, nz, neighborLevel };
            }
        }
    }

    floodFillSkyLight(relightSeeds, relightCount);
}

// Núcleo compartido: inunda hacia afuera desde una o varias semillas
// ya escritas en el mundo con su nivel correcto, sin cruzar bloques
// opacos, sin sobrescribir nada que ya tenga luz igual o mayor. Lo
// usan tanto colocar una antorcha (1 semilla) como la fase 2 de
// removeBlockLight (varias semillas a la vez).
void ChunkManager::floodFillBlockLight(LightNode* seeds, int seedCount) const {
    static const int OFFSETS[6][3] = {
        {1,0,0}, {-1,0,0}, {0,1,0}, {0,-1,0}, {0,0,1}, {0,0,-1}
    };

    // Igual que la del cielo: sin divisiones por software y recordando el último chunk (eran ~30 ms por cada 400 pasos)
    int cacheCx = 0x7FFFFFFF, cacheCz = 0;
    Chunk* cacheChunk = nullptr;

    int head = 0, tail = 0;
    for (int s = 0; s < seedCount && tail < LIGHT_QUEUE_CAPACITY; s++) {
        lightQueue[tail++] = seeds[s];
    }

    int steps = 0;
    while (head != tail) {
        if (++steps > MAX_FLOOD_STEPS_PER_CALL) break; // presupuesto agotado: cortar, no trabar
        LightNode node = lightQueue[head++];
        if (head >= LIGHT_QUEUE_CAPACITY) head = 0;

        if (node.level <= 1) continue;

        for (int d = 0; d < 6; d++) {
            int nx = node.bx + OFFSETS[d][0];
            int ny = node.by + OFFSETS[d][1];
            int nz = node.bz + OFFSETS[d][2];
            if (ny < 0 || ny >= WorldConfig::SIZE_Y) continue;

            int ncx = nx >> 3;
            int ncz = nz >> 3;
            Chunk* nc;
            if (ncx == cacheCx && ncz == cacheCz) {
                nc = cacheChunk;
            } else {
                nc = getChunkByChunkCoord(ncx, ncz);
                cacheCx = ncx; cacheCz = ncz; cacheChunk = nc;
            }
            if (!nc) continue; // vecino no cargado: se retoma si carga después

            int lxi = nx & 7;
            int lzi = nz & 7;

            if (isLightOpaque(nc->getBlockType(lxi, ny, lzi))) continue;

            u8 newLevel = node.level - 1;
            if (nc->getBlockLight(lxi, ny, lzi) >= newLevel) continue;

            nc->setBlockLight(lxi, ny, lzi, newLevel);

            int nextTail = (tail + 1) % LIGHT_QUEUE_CAPACITY;
            if (nextTail == head) continue; // cola llena (caso extremo): se descarta este nodo
            lightQueue[tail] = { nx, ny, nz, newLevel };
            tail = nextTail;
        }
    }
}

void ChunkManager::propagateBlockLight(float worldX, float worldY, float worldZ, u8 level) const {
    Chunk* startChunk = getChunkAt(worldX, worldZ);
    if (!startChunk) return;

    int xi, yi, zi;
    if (!startChunk->worldToGrid(worldX, worldY, worldZ, xi, yi, zi)) return;

    int bx = startChunk->getChunkX() * WorldConfig::SIZE_X + xi;
    int bz = startChunk->getChunkZ() * WorldConfig::SIZE_Z + zi;

    startChunk->setBlockLight(xi, yi, zi, level);

    LightNode seed = { bx, yi, bz, level };
    floodFillBlockLight(&seed, 1);
}

void ChunkManager::removeBlockLight(float worldX, float worldY, float worldZ) const {
    Chunk* startChunk = getChunkAt(worldX, worldZ);
    if (!startChunk) return;

    int xi, yi, zi;
    if (!startChunk->worldToGrid(worldX, worldY, worldZ, xi, yi, zi)) return;

    u8 oldLevel = startChunk->getBlockLight(xi, yi, zi);
    if (oldLevel == 0) return;

    int bx = startChunk->getChunkX() * WorldConfig::SIZE_X + xi;
    int bz = startChunk->getChunkZ() * WorldConfig::SIZE_Z + zi;

    startChunk->setBlockLight(xi, yi, zi, 0);

    static const int OFFSETS[6][3] = {
        {1,0,0}, {-1,0,0}, {0,1,0}, {0,-1,0}, {0,0,1}, {0,0,-1}
    };

    // Fase 1: apagar todo lo que dependía de esta fuente, juntando de
    // paso los "bordes" (vecinos con luz propia independiente) como
    // semillas para la fase 2.
    int head = 0, tail = 0;
    lightQueue[tail++] = { bx, yi, bz, oldLevel };
    int relightCount = 0;

    int steps = 0;
    while (head != tail) {
        if (++steps > MAX_FLOOD_STEPS_PER_CALL) break;
        LightNode node = lightQueue[head++];
        if (head >= LIGHT_QUEUE_CAPACITY) head = 0;

        for (int d = 0; d < 6; d++) {
            int nx = node.bx + OFFSETS[d][0];
            int ny = node.by + OFFSETS[d][1];
            int nz = node.bz + OFFSETS[d][2];
            if (ny < 0 || ny >= WorldConfig::SIZE_Y) continue;

            int ncx = floorDiv(nx, WorldConfig::SIZE_X);
            int ncz = floorDiv(nz, WorldConfig::SIZE_Z);
            Chunk* nc = getChunkByChunkCoord(ncx, ncz);
            if (!nc) continue;

            int lxi = nx - ncx * WorldConfig::SIZE_X;
            int lzi = nz - ncz * WorldConfig::SIZE_Z;

            u8 neighborLevel = nc->getBlockLight(lxi, ny, lzi);
            if (neighborLevel == 0) continue;

            if (neighborLevel < node.level) {
                // Dependía de esta fuente: apagar y seguir la cadena.
                nc->setBlockLight(lxi, ny, lzi, 0);

                int nextTail = (tail + 1) % LIGHT_QUEUE_CAPACITY;
                if (nextTail == head) continue;
                lightQueue[tail] = { nx, ny, nz, neighborLevel };
                tail = nextTail;
            } else if (relightCount < LIGHT_QUEUE_CAPACITY) {
                // Tiene luz propia (de otra fuente): semilla para
                // re-propagar en la fase 2, sin tocarlo ahora.
                relightSeeds[relightCount++] = { nx, ny, nz, neighborLevel };
            }
        }
    }

    // Fase 2: re-inundar desde todos los bordes recolectados, de una
    // sola pasada (inundación multi-fuente).
    floodFillBlockLight(relightSeeds, relightCount);
}

int ChunkManager::saveModifiedChunks() const {
    int savedCount = 0;

    for (int i = 0; i < maxChunks; i++) {
        if (!slotUsed[i]) continue;
        if (!chunks[i]->isModified()) continue;

        char filename[96]; // antes 32
        chunks[i]->getSaveFilename(filename);

        if (chunks[i]->saveToFile(filename)) {
            savedCount++;
        }
    }

    return savedCount;
}

bool ChunkManager::isBlockPresentAt(float x, float y, float z) const {
    Chunk* c = getChunkAt(x, z);
    if (!c) return false;
    if (!c->isGenerationComplete()) return false;

    int xi, yi, zi;
    if (!c->worldToGrid(x, y, z, xi, yi, zi)) return false;

    BlockType type = c->getBlockType(xi, yi, zi);
    // el fuego no tiene caja (isCollidable() == false) y el portal es un plano fino sin colisión: el rayo los atraviesa
    return type != BlockType::AIR && !isLiquidBlock(type) && type != BlockType::FIRE && type != BlockType::PORTAL;
}

void ChunkManager::forceFullRecalculation() const {
    for (int i = 0; i < maxChunks; i++) {
        if (!slotUsed[i]) continue;
        if (!chunks[i]->isGenerationComplete()) continue;

        chunks[i]->recalculateFullFaceCache(this);
    }
}

BlockType ChunkManager::getBlockTypeAt(float x, float y, float z) const {
    Chunk* c = getChunkAt(x, z);
    if (!c) return BlockType::AIR;
    if (!c->isGenerationComplete()) return BlockType::AIR;

    int xi, yi, zi;
    if (!c->worldToGrid(x, y, z, xi, yi, zi)) return BlockType::AIR;

    return c->getBlockType(xi, yi, zi);
}

BiomeType ChunkManager::getBiomeAt(float worldX, float worldZ) const {
    Chunk* c = getChunkAt(worldX, worldZ);
    if (!c) return BiomeType::OCEAN; // sin datos, tratamos como agua para no spawnear ahí

    float safeY = c->gridToWorldY(0);
    int xi, yi, zi;
    if (!c->worldToGrid(worldX, safeY, worldZ, xi, yi, zi)) return BiomeType::OCEAN;

    return c->getBiomeAt(xi, zi);
}

bool ChunkManager::getColumnOccupancy(float worldX, float worldZ, unsigned int outBits[4]) const {
    Chunk* c = getChunkAt(worldX, worldZ);
    if (!c || !c->isGenerationComplete()) {
        outBits[0] = outBits[1] = outBits[2] = outBits[3] = 0xFFFFFFFF; // sin datos = sólido, conservador (mismo criterio que isSolidAt)
        return false;
    }

    int xi, yi, zi;
    float safeY = c->gridToWorldY(0);
    if (!c->worldToGrid(worldX, safeY, worldZ, xi, yi, zi)) {
        outBits[0] = outBits[1] = outBits[2] = outBits[3] = 0xFFFFFFFF;
        return false;
    }

    c->getColumnOccupancyBits(xi, zi, outBits);
    return true;
}

// =====================================================================================================================
// Líquidos de Beta 1.7.3: el ChunkManager es el mundo de la simulación (celdas globales: gx = chunkX*8 + xi, gy = yi,
// gz = chunkZ*8 + zi). Una celda sin cargar cuenta como piedra, así que nada fluye hacia ella.
// =====================================================================================================================
namespace {
    unsigned int s_liquidRng = 12345u;

    struct LiquidWorld {
        ChunkManager* m;

        static int floorDiv(int v, int d) { return v >= 0 ? v / d : -((-v + d - 1) / d); }

        Chunk* chunkOf(int gx, int gz, int& xi, int& zi) const {
            int cx = floorDiv(gx, WorldConfig::SIZE_X), cz = floorDiv(gz, WorldConfig::SIZE_Z);
            Chunk* c = m->getChunkByChunkCoord(cx, cz);
            if (!c || !c->isGenerationComplete()) return nullptr;
            xi = gx - cx * WorldConfig::SIZE_X;
            zi = gz - cz * WorldConfig::SIZE_Z;
            return c;
        }
        BlockType getType(int gx, int gy, int gz) {
            if (gy < 0) return BlockType::STONE;
            if (gy >= WorldConfig::SIZE_Y) return BlockType::AIR;
            int xi, zi; Chunk* c = chunkOf(gx, gz, xi, zi);
            return c ? c->getBlockType(xi, gy, zi) : BlockType::STONE;
        }
        int getMeta(int gx, int gy, int gz) {
            if (gy < 0 || gy >= WorldConfig::SIZE_Y) return 0;
            int xi, zi; Chunk* c = chunkOf(gx, gz, xi, zi);
            return c ? c->getLiquidMeta(xi, gy, zi) : 0;
        }
        bool inRange(int gx, int gy, int gz) {
            if (gy < 0 || gy >= WorldConfig::SIZE_Y) return false;
            int xi, zi; return chunkOf(gx, gz, xi, zi) != nullptr;
        }
        void putCell(int gx, int gy, int gz, BlockType t, int meta) {
            int xi, zi; Chunk* c = chunkOf(gx, gz, xi, zi);
            if (c) c->putLiquidCell(xi, gy, zi, t, meta, m);
        }
        void pushTick(int gx, int gy, int gz, BlockType t, int delay) {
            int xi, zi; if (chunkOf(gx, gz, xi, zi)) Liquid::pushTick(gx, gy, gz, t, delay);
        }
        int randInt(int n) {
            s_liquidRng = s_liquidRng * 1664525u + 1013904223u;
            return (int)((s_liquidRng >> 8) % (unsigned int)n);
        }
        // flowIntoBlock: el agua rompe lo que desplaza y suelta lo que soltaría
        void dropItem(int gx, int gy, int gz, BlockType old) {
            BlockType drop = BlockType::AIR;
            if (isTorchBlock(old)) drop = BlockType::TORCH;
            else if (old == BlockType::CROPS) drop = BlockType::SEEDS;   // el agua arrastra el cultivo (se suelta una semilla; la Beta suelta segun la etapa)
            else if (isCrossPlant(old) || old == BlockType::CACTUS) drop = getDropType(old);   // el cactus suelta su ítem al romperse solo o al desplazarlo el agua
            if (drop == BlockType::AIR) return;
            PendingDrops::push((float)gx - WorldConfig::SIZE_X / 2.0f + 0.5f, (float)gy - 1.0f,
                               (float)gz - WorldConfig::SIZE_Z / 2.0f + 0.5f, drop);
        }
        void otherNeighbor(int gx, int gy, int gz) {
            int xi, zi; Chunk* c = chunkOf(gx, gz, xi, zi);
            if (c && gy >= 0 && gy < WorldConfig::SIZE_Y && isTorchBlock(c->getBlockType(xi, gy, zi))) c->checkTorchSupport(xi, gy, zi, m);
        }
        void mixEffect() {}   // sonido y humo
        // BlockSand: el bloque pasa a ser una entidad que cae (centro del cubo en coordenadas de mundo)
        bool spawnFalling(int gx, int gy, int gz, BlockType t) {
            return FallingBlocks::spawn((float)gx - WorldConfig::SIZE_X / 2.0f + 0.5f, (float)gy - 1.0f, (float)gz - WorldConfig::SIZE_Z / 2.0f + 0.5f, t);
        }
    };

    const int LIQUID_TICKS_PER_STEP = 24;   // tope por tick del juego; lo que sobra espera al siguiente, en el mismo orden
}

void ChunkManager::tickLiquids() {
    Liquid::advanceTime();
    LiquidWorld lw{ this };
    Liquid::Sim<LiquidWorld> sim(lw);
    int gx, gy, gz; BlockType t;
    for (int n = 0; n < LIQUID_TICKS_PER_STEP && Liquid::popDue(gx, gy, gz, t); n++) {
        sim.runScheduled(gx, gy, gz, t);
    }

    // Ticks aleatorios (World.updateBlocksAndPlayCaveSounds): en el original son 80 celdas por chunk de 16x16x128 y por tick; aquí,
    // 20 por chunk de 8x8x128. La lava quieta puede prender fuego a lo inflamable que tiene encima (BlockStationary.updateTick).
    static unsigned int s_randomTickRng = 2463534242u;
    for (int i = 0; i < maxChunks; i++) {
        Chunk* c = chunks[i];
        if (!slotUsed[i] || !c || !c->isGenerationComplete()) continue;
        for (int k = 0; k < 20; k++) {
            s_randomTickRng = s_randomTickRng * 1664525u + 1013904223u;
            unsigned int r = s_randomTickRng >> 8;
            int xi = (int)(r % WorldConfig::SIZE_X);
            int yi = (int)((r / WorldConfig::SIZE_X) % WorldConfig::SIZE_Y);
            int zi = (int)((r / (WorldConfig::SIZE_X * WorldConfig::SIZE_Y)) % WorldConfig::SIZE_Z);
            const BlockType rt = c->getBlockType(xi, yi, zi);
            if (rt == BlockType::LAVA) {
                sim.lavaRandomTick(c->getChunkX() * WorldConfig::SIZE_X + xi, yi, c->getChunkZ() * WorldConfig::SIZE_Z + zi);
            } else if (rt == BlockType::CROPS) {   // BlockCrops.updateTick: se mantiene en pie y crece
                Partial::cropsTick(*this, c->getChunkX() * WorldConfig::SIZE_X + xi, yi, c->getChunkZ() * WorldConfig::SIZE_Z + zi);
            } else if (isSapling(rt)) {   // BlockSapling.updateTick: crece con luz suficiente
                Trees::saplingTick(*this, c->getChunkX() * WorldConfig::SIZE_X + xi, yi, c->getChunkZ() * WorldConfig::SIZE_Z + zi);
            } else if (isLeavesBlock(rt)) {   // BlockLeaves.updateTick: las marcadas se caen si no hay tronco cerca
                if (c->getLiquidMeta(xi, yi, zi) & 8) Trees::leafTick(*this, c->getChunkX() * WorldConfig::SIZE_X + xi, yi, c->getChunkZ() * WorldConfig::SIZE_Z + zi);
            } else if (rt == BlockType::ICE || rt == BlockType::SNOW_LAYER || rt == BlockType::SNOW_BLOCK) {
                // BlockIce / BlockSnow / BlockSnowBlock.updateTick: con luz de bloque mayor a 11 (el hielo resta su opacidad 3: mayor a 8) se derriten
                const int meltLight = (rt == BlockType::ICE) ? 8 : 11;
                if ((int)c->getBlockLight(xi, yi, zi) > meltLight) {
                    const int mgx = c->getChunkX() * WorldConfig::SIZE_X + xi, mgz = c->getChunkZ() * WorldConfig::SIZE_Z + zi;
                    if (rt == BlockType::SNOW_BLOCK) {   // dropBlockAsItem: 4 bolas de nieve
                        for (int q = 0; q < 4; q++)
                            PendingDrops::push((float)mgx - WorldConfig::SIZE_X / 2.0f + 0.5f, (float)yi - 1.0f, (float)mgz - WorldConfig::SIZE_Z / 2.0f + 0.5f, BlockType::SNOWBALL);
                    }
                    setBlockGlobal(mgx, yi, mgz, rt == BlockType::ICE ? BlockType::WATER : BlockType::AIR);
                }
            } else if (rt == BlockType::FARMLAND) {   // BlockFarmland.updateTick: se hidrata o se seca (1 de cada 5 ticks aleatorios)
                s_randomTickRng = s_randomTickRng * 1664525u + 1013904223u;
                Partial::farmlandTick(*this, c->getChunkX() * WorldConfig::SIZE_X + xi, yi, c->getChunkZ() * WorldConfig::SIZE_Z + zi, (int)((s_randomTickRng >> 8) % 5u));
            } else if (Redstone::isRandomTicking(rt) || isRepeaterBlock(rt)) {   // botones, placas, antorchas, mena encendida y repetidores (tickOnLoad del original)
                Redstone::randomTick(*this, c->getChunkX() * WorldConfig::SIZE_X + xi, yi, c->getChunkZ() * WorldConfig::SIZE_Z + zi, rt);
            }
        }
    }
}

void ChunkManager::liquidBlockChanged(int gx, int gy, int gz) const {
    Redstone::blockChanged(*this, gx, gy, gz);   // la redstone avisa a sus vecinos (cables, antorchas, puertas...)
    LiquidWorld lw{ const_cast<ChunkManager*>(this) };
    Liquid::Sim<LiquidWorld> sim(lw);
    sim.notifyNeighbors(gx, gy, gz);
    // el bloque recien puesto por el jugador: BlockSand.onBlockAdded (los vecinos ya se avisaron arriba)
    const BlockType self = lw.getType(gx, gy, gz);
    if (self == BlockType::SAND || self == BlockType::GRAVEL) lw.pushTick(gx, gy, gz, self, Liquid::tickRateOf(self));
}

// El mundo va desplazado respecto de las celdas de la simulación: x' = x + 4, y' = y + 1.5, z' = z + 4
bool ChunkManager::liquidAcceleration(float minX, float minY, float minZ, float maxX, float maxY, float maxZ,
                                      int mat, float out[3]) const {
    LiquidWorld lw{ const_cast<ChunkManager*>(this) };
    Liquid::Sim<LiquidWorld> sim(lw);
    double v[3];
    bool any = sim.materialAcceleration(minX + 4.0, minY + 1.5, minZ + 4.0, maxX + 4.0, maxY + 1.5, maxZ + 4.0, mat, v);
    out[0] = (float)v[0]; out[1] = (float)v[1]; out[2] = (float)v[2];
    return any;
}

bool ChunkManager::lavaInBox(float minX, float minY, float minZ, float maxX, float maxY, float maxZ) const {
    LiquidWorld lw{ const_cast<ChunkManager*>(this) };
    int x0 = (int)floorf(minX + 4.0f), x1 = (int)floorf(maxX + 4.0f + 1.0f);
    int y0 = (int)floorf(minY + 1.5f), y1 = (int)floorf(maxY + 1.5f + 1.0f);
    int z0 = (int)floorf(minZ + 4.0f), z1 = (int)floorf(maxZ + 4.0f + 1.0f);
    for (int x = x0; x < x1; x++)
        for (int y = y0; y < y1; y++)
            for (int z = z0; z < z1; z++)
                if (isLavaBlock(lw.getType(x, y, z))) return true;
    return false;
}

void ChunkManager::placeLiquidSource(int gx, int gy, int gz, BlockType flowingType) const {
    LiquidWorld lw{ const_cast<ChunkManager*>(this) };
    Liquid::Sim<LiquidWorld> sim(lw);
    sim.setBlockAndMetadataWithNotify(gx, gy, gz, flowingType, 0);
}

// World.getIsAnyLiquid: ¿alguna celda que toca la caja es agua o lava? (mismo cambio de coordenadas que lavaInBox)
bool ChunkManager::anyLiquidInBox(float minX, float minY, float minZ, float maxX, float maxY, float maxZ) const {
    LiquidWorld lw{ const_cast<ChunkManager*>(this) };
    int x0 = (int)floorf(minX + 4.0f), x1 = (int)floorf(maxX + 4.0f + 1.0f);
    int y0 = (int)floorf(minY + 1.5f), y1 = (int)floorf(maxY + 1.5f + 1.0f);
    int z0 = (int)floorf(minZ + 4.0f), z1 = (int)floorf(maxZ + 4.0f + 1.0f);
    for (int x = x0; x < x1; x++)
        for (int y = y0; y < y1; y++)
            for (int z = z0; z < z1; z++)
                if (isLiquidBlock(lw.getType(x, y, z))) return true;
    return false;
}

bool ChunkManager::isLiquidSourceAt(float x, float y, float z) const {
    Chunk* c = getChunkAt(x, z);
    if (!c || !c->isGenerationComplete()) return false;
    int xi, yi, zi;
    if (!c->worldToGrid(x, y, z, xi, yi, zi)) return false;
    return isLiquidBlock(c->getBlockType(xi, yi, zi)) && c->getLiquidMeta(xi, yi, zi) == 0;
}

// ItemBucket: con el balde vacío, solo las fuentes se toman (los riachuelos no); la celda queda en aire
BlockType ChunkManager::bucketTakeSource(int gx, int gy, int gz) const {
    LiquidWorld lw{ const_cast<ChunkManager*>(this) };
    BlockType t = lw.getType(gx, gy, gz);
    if (!isLiquidBlock(t) || lw.getMeta(gx, gy, gz) != 0) return BlockType::AIR;
    Liquid::Sim<LiquidWorld> sim(lw);
    sim.setBlockWithNotify(gx, gy, gz, BlockType::AIR);
    return isWaterBlock(t) ? BlockType::WATER_BUCKET : BlockType::LAVA_BUCKET;
}

// ItemBucket: con el balde lleno se pone una fuente donde hay aire o algo que no es sólido (reemplaza sin soltar)
bool ChunkManager::bucketPlaceSource(int gx, int gy, int gz, BlockType flowingType) const {
    LiquidWorld lw{ const_cast<ChunkManager*>(this) };
    if (!lw.inRange(gx, gy, gz)) return false;
    if (isMaterialSolid(lw.getType(gx, gy, gz))) return false;
    if (Liquid::isHell() && isWaterBlock(flowingType)) return true; // en el Nether el agua se evapora: el balde se vacía y no queda nada
    Liquid::Sim<LiquidWorld> sim(lw);
    sim.setBlockAndMetadataWithNotify(gx, gy, gz, flowingType, 0);
    return true;
}

// ItemFlintAndSteel.onItemUse: el fuego solo se pone donde hay aire (si el fuego no puede quedarse, se apaga solo)
bool ChunkManager::ignite(int gx, int gy, int gz) const {
    LiquidWorld lw{ const_cast<ChunkManager*>(this) };
    if (!lw.inRange(gx, gy, gz) || lw.getType(gx, gy, gz) != BlockType::AIR) return false;
    Liquid::Sim<LiquidWorld> sim(lw);
    sim.setBlockWithNotify(gx, gy, gz, BlockType::FIRE);
    return true;
}

// ¿alguna celda que toca la caja es un portal? (mismo cambio de coordenadas que lavaInBox)
bool ChunkManager::portalInBox(float minX, float minY, float minZ, float maxX, float maxY, float maxZ) const {
    LiquidWorld lw{ const_cast<ChunkManager*>(this) };
    int x0 = (int)floorf(minX + 4.0f), x1 = (int)floorf(maxX + 4.0f + 1.0f);
    int y0 = (int)floorf(minY + 1.5f), y1 = (int)floorf(maxY + 1.5f + 1.0f);
    int z0 = (int)floorf(minZ + 4.0f), z1 = (int)floorf(maxZ + 4.0f + 1.0f);
    for (int x = x0; x < x1; x++)
        for (int y = y0; y < y1; y++)
            for (int z = z0; z < z1; z++)
                if (lw.getType(x, y, z) == BlockType::PORTAL) return true;
    return false;
}

// Teleporter (Minecraft.usePortal -> Teleporter.func_4107_a). El mundo de este juego va desplazado respecto de las celdas del original:
// x' = x + 4, y' = y + 1.5, z' = z + 4. La búsqueda se limita a las columnas de los chunks cargados (las demás no tienen portal ni aire).
bool ChunkManager::teleportToPortal(float worldX, float eyeY, float worldZ, unsigned int randSeed, float& outX, float& outEyeY, float& outZ) const {
    LiquidWorld lw{ const_cast<ChunkManager*>(this) };
    Liquid::Sim<LiquidWorld> sim(lw);

    Teleporter::Bounds b = { 0x7FFFFFFF, -0x7FFFFFFF, 0x7FFFFFFF, -0x7FFFFFFF };
    for (int i = 0; i < maxChunks; i++) {
        Chunk* c = chunks[i];
        if (!slotUsed[i] || !c || !c->isGenerationComplete()) continue;
        int x0 = c->getChunkX() * WorldConfig::SIZE_X, z0 = c->getChunkZ() * WorldConfig::SIZE_Z;
        if (x0 < b.x0) b.x0 = x0;
        if (x0 + WorldConfig::SIZE_X - 1 > b.x1) b.x1 = x0 + WorldConfig::SIZE_X - 1;
        if (z0 < b.z0) b.z0 = z0;
        if (z0 + WorldConfig::SIZE_Z - 1 > b.z1) b.z1 = z0 + WorldConfig::SIZE_Z - 1;
    }
    if (b.x0 > b.x1 || b.z0 > b.z1) return false;

    // usePortal vuelve a fijar la posición con setLocationAndAngles(x, posY, z): a los ojos se les suma otra vez la altura de los ojos
    const double posX = (double)worldX + 4.0, posY = (double)eyeY + 1.5 + 1.62, posZ = (double)worldZ + 4.0;
    Lakes::JRandom rng((long long)randSeed);
    Teleporter::Spot spot;
    if (!Teleporter::placeInPortal(sim, rng, b, posX, posY, posZ, spot)) return false;

    outX = (float)(spot.x - 4.0);
    outEyeY = (float)(spot.y - 1.5) + 1.62f;   // spot.y son los pies
    outZ = (float)(spot.z - 4.0);
    return true;
}

// World.onBlockHit: el fuego de la celda se quita (setBlockWithNotify a aire)
bool ChunkManager::extinguishFire(int gx, int gy, int gz) const {
    LiquidWorld lw{ const_cast<ChunkManager*>(this) };
    if (!lw.inRange(gx, gy, gz) || lw.getType(gx, gy, gz) != BlockType::FIRE) return false;
    Liquid::Sim<LiquidWorld> sim(lw);
    sim.setBlockWithNotify(gx, gy, gz, BlockType::AIR);
    return true;
}

// ¿alguna celda que toca la caja tiene fuego? (mismo cambio de coordenadas que portalInBox)
bool ChunkManager::fireInBox(float minX, float minY, float minZ, float maxX, float maxY, float maxZ) const {
    LiquidWorld lw{ const_cast<ChunkManager*>(this) };
    int x0 = (int)floorf(minX + 4.0f), x1 = (int)floorf(maxX + 4.0f + 1.0f);
    int y0 = (int)floorf(minY + 1.5f), y1 = (int)floorf(maxY + 1.5f + 1.0f);
    int z0 = (int)floorf(minZ + 4.0f), z1 = (int)floorf(maxZ + 4.0f + 1.0f);
    for (int x = x0; x < x1; x++)
        for (int y = y0; y < y1; y++)
            for (int z = z0; z < z1; z++)
                if (lw.getType(x, y, z) == BlockType::FIRE) return true;
    return false;
}

bool ChunkManager::typeInBox(BlockType t, float minX, float minY, float minZ, float maxX, float maxY, float maxZ) const {
    LiquidWorld lw{ const_cast<ChunkManager*>(this) };
    int x0 = (int)floorf(minX + 4.0f), x1 = (int)floorf(maxX + 4.0f) + 1;
    int y0 = (int)floorf(minY + 1.5f), y1 = (int)floorf(maxY + 1.5f) + 1;
    int z0 = (int)floorf(minZ + 4.0f), z1 = (int)floorf(maxZ + 4.0f) + 1;
    for (int x = x0; x < x1; x++)
        for (int y = y0; y < y1; y++)
            for (int z = z0; z < z1; z++)
                if (lw.inRange(x, y, z) && lw.getType(x, y, z) == t) return true;
    return false;
}

// Entity.isInsideOfMaterial: la celda donde está el punto de prueba (los ojos + 0,12) es del material y el punto queda por debajo de la
// superficie del líquido (BlockFluid.getPercentAir: nivel del agua en esa celda; una fuente llega al borde de arriba)
bool ChunkManager::isInsideOfMaterial(float worldX, float testWorldY, float worldZ, bool lava) const {
    LiquidWorld lw{ const_cast<ChunkManager*>(this) };
    const int gx = (int)floorf(worldX + 4.0f), gz = (int)floorf(worldZ + 4.0f);
    const float mcY = testWorldY + 1.5f;
    const int gy = (int)floorf(mcY);
    if (!lw.inRange(gx, gy, gz)) return false;
    const BlockType t = lw.getType(gx, gy, gz);
    if (lava ? !isLavaBlock(t) : !isWaterBlock(t)) return false;
    int meta = lw.getMeta(gx, gy, gz);
    if (meta >= 8) meta = 0;                                  // agua que cae: celda llena
    const float percentAir = (float)(meta + 1) / 9.0f;
    const float level = (float)(gy + 1) - (percentAir - 1.0f / 9.0f);
    return mcY < level;
}

// Entity.isEntityInsideOpaqueBlock: 8 puntos alrededor de la cabeza (0,54 de ancho, 0,1 de alto) dentro de un bloque cúbico y sólido
bool ChunkManager::insideOpaqueBlock(float worldX, float testWorldY, float worldZ) const {
    for (int i = 0; i < 8; i++) {
        const float dx = ((float)((i >> 0) % 2) - 0.5f) * 0.6f * 0.9f;
        const float dy = ((float)((i >> 1) % 2) - 0.5f) * 0.1f;
        const float dz = ((float)((i >> 2) % 2) - 0.5f) * 0.6f * 0.9f;
        if (isNormalCube(getBlockTypeAt(worldX + dx, testWorldY + dy, worldZ + dz))) return true;
    }
    return false;
}

// Pone un bloque en una celda global (la usan los cofres de depuración al crear un mundo)
bool ChunkManager::setBlockGlobal(int gx, int gy, int gz, BlockType type) const {
    LiquidWorld lw{ const_cast<ChunkManager*>(this) };
    if (!lw.inRange(gx, gy, gz)) return false;
    Liquid::Sim<LiquidWorld> sim(lw);
    sim.setBlockWithNotify(gx, gy, gz, type);
    return true;
}