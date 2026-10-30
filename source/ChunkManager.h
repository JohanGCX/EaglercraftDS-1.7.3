#ifndef CHUNKMANAGER_H
#define CHUNKMANAGER_H

#include "Chunk.h"

class ChunkManager {
public:
    // Capacidad maxima (modo dsi: ventana de 7 x 7 + reserva); el tamano real de cada modo se fija en el constructor
    static const int MAX_RADIUS = WorldConfig::MAX_VIEW_RADIUS;
    static const int CAP_CHUNKS = (2 * MAX_RADIUS + 1) * (2 * MAX_RADIUS + 1) + 3;
    static const int POOL_SPARE = 2;
    int chunkCount() const { return maxChunks; }
    int poolKB() const { return (poolSize * (int)sizeof(Chunk)) / 1024; }   // RAM fija de todos los chunks del pozo (la consola la muestra)
    bool isSolidAt(float x, float y, float z) const;
    void drawAll(float playerX, float playerY, float playerZ, float lookDirX, float lookDirY, float lookDirZ);
    ChunkManager(unsigned int seed, int startChunkX = 0, int startChunkZ = 0);
    ~ChunkManager();

    // Llamar cada frame: genera columnas pendientes de todos los
    // chunks activos, y asegura que el chunk del jugador + sus 8
    // vecinos existan
    void update(float playerX, float playerZ);
    void refreshNeighborFace(float worldX, float worldY, float worldZ) const;

    // Retorna el chunk que contiene la posición de mundo dada, o
    // nullptr si no está cargado
    Chunk* getChunkAt(float worldX, float worldZ) const;

    // true solo cuando TODOS los chunks activos terminaron de generar
    bool isGenerationComplete() const;

    Chunk* getCenterChunk() const { return chunks[centerIndex]; }
    unsigned int getSeed() const { return seed; }
    int saveModifiedChunks() const;
    bool isBlockPresentAt(float x, float y, float z) const;
    void forceFullRecalculation() const;
    BlockType getBlockTypeAt(float x, float y, float z) const;
    BiomeType getBiomeAt(float worldX, float worldZ) const;
    int heavyWorkRotationStart = 0;
    int pendingLoadSlots[CAP_CHUNKS];
    int pendingLoadCount = 0;
    int pendingSaveCount = 0;
    bool awaitingLoad[CAP_CHUNKS];
    bool borderRefreshPending[CAP_CHUNKS]; // un vecino acaba de completarse: falta recalcular las caras de la orilla (de a un chunk por cuadro)
    Chunk* pendingSaveChunks[CAP_CHUNKS] = {};
    bool getColumnOccupancy(float worldX, float worldZ, unsigned int outBits[4]) const;
    
    void propagateBlockLight(float worldX, float worldY, float worldZ, u8 level) const;
    void removeBlockLight(float worldX, float worldY, float worldZ) const;
    void propagateSkyColumn(float worldX, float worldZ) const;

    Chunk* getChunkByChunkCoord(int cx, int cz) const;

    void relightNewAirCell(float worldX, float worldY, float worldZ) const;
    void removeSkyLightAt(float worldX, float worldY, float worldZ) const;

    int getSkylightSubtract() const { return skylightSubtract; }
    void setSkylightSubtract(int v);

    void seedSkyRange(int bx, int bz, int yFrom, int yTo) const;

    // Líquidos (agua y lava de Beta 1.7.3)
    void tickLiquids();                                       // una vez por tick del juego (20 Hz)
    void liquidBlockChanged(int gx, int gy, int gz) const;    // un bloque cambió: avisa a sus 6 vecinos
    // Empuje de las corrientes sobre una caja del mundo (World.handleMaterialAcceleration); mat 1 = agua, 2 = lava
    bool liquidAcceleration(float minX, float minY, float minZ, float maxX, float maxY, float maxZ, int mat, float out[3]) const;
    // Pone una fuente de líquido como el balde: setBlockAndMetadataWithNotify(corriente, nivel 0)
    void placeLiquidSource(int gx, int gy, int gz, BlockType flowingType) const;
    bool setBlockGlobal(int gx, int gy, int gz, BlockType type) const; // pone un bloque (con avisos a los vecinos) en una celda global cargada
    bool ignite(int gx, int gy, int gz) const; // mechero: fuego en la celda si ahí hay aire
    bool extinguishFire(int gx, int gy, int gz) const; // World.onBlockHit: apaga el fuego de esa celda
    // Teleporter: busca el portal más cercano (en los chunks cargados) o construye uno, y da dónde queda el jugador (ojos)
    bool teleportToPortal(float worldX, float eyeY, float worldZ, unsigned int randSeed, float& outX, float& outEyeY, float& outZ) const;
    // Balde (ItemBucket.onItemRightClick): coordenadas de celda global (gx = chunkX*8 + xi, gy = yi, gz = chunkZ*8 + zi)
    bool isLiquidSourceAt(float x, float y, float z) const;               // fuente (nivel 0) de agua o lava
    BlockType bucketTakeSource(int gx, int gy, int gz) const;             // vacío -> lleno; AIR si no era una fuente
    bool bucketPlaceSource(int gx, int gy, int gz, BlockType flowingType) const; // lleno -> pone la fuente
    bool lavaInBox(float minX, float minY, float minZ, float maxX, float maxY, float maxZ) const; // World.isMaterialInBB
    bool anyLiquidInBox(float minX, float minY, float minZ, float maxX, float maxY, float maxZ) const; // World.getIsAnyLiquid
    bool portalInBox(float minX, float minY, float minZ, float maxX, float maxY, float maxZ) const;
    bool fireInBox(float minX, float minY, float minZ, float maxX, float maxY, float maxZ) const;      // World.isBoundingBoxBurning (fuego)    // Entity.doBlockCollisions: ¿toca un portal?
    bool typeInBox(BlockType t, float minX, float minY, float minZ, float maxX, float maxY, float maxZ) const; // ¿hay algún bloque de ese tipo en la caja?
    bool isInsideOfMaterial(float worldX, float testWorldY, float worldZ, bool lava) const;   // Entity.isInsideOfMaterial (agua o lava), con la altura de la superficie
    bool insideOpaqueBlock(float worldX, float testWorldY, float worldZ) const;                // Entity.isEntityInsideOpaqueBlock (jugador de 0,6 de ancho)

private:
    Chunk* chunks[CAP_CHUNKS];
    int chunkPosX[CAP_CHUNKS]; // coordenadas de chunk de cada slot
    int chunkPosZ[CAP_CHUNKS];
    bool slotUsed[CAP_CHUNKS];
    bool wasComplete[CAP_CHUNKS];

    int centerIndex; // índice del chunk donde está el jugador actualmente

    unsigned int seed;

    int worldToChunkCoord(float worldPos) const;
    int findOrCreateSlot(int cx, int cz, const int* protectX = nullptr, const int* protectZ = nullptr, int protectCount = 0);
    mutable Chunk* lastUsedChunk = nullptr;
    mutable int lastUsedChunkX = 0;
    mutable int lastUsedChunkZ = 0;
    mutable Chunk* lastLightChunk = nullptr;
    mutable int lastLightChunkX = 0;
    mutable int lastLightChunkZ = 0;

    struct LightNode { int bx, by, bz; u8 level; };
    static const int LIGHT_QUEUE_CAPACITY = 4096;
    static const int MAX_FLOOD_STEPS_PER_CALL = 400;
    // La luz del cielo se inunda en tandas chicas: lo que no entra en una tanda queda pendiente y sigue en los próximos cuadros
    // (antes una sola tanda de 400 pasos tardaba 25 a 80 ms en el bosque, y lo que sobraba se perdía)
    static const int SKY_FLOOD_STEPS_PER_CALL = 80;
    mutable LightNode skyPending[LIGHT_QUEUE_CAPACITY];
    mutable int skyPendingCount = 0;
    mutable LightNode lightQueue[LIGHT_QUEUE_CAPACITY];
    mutable LightNode relightSeeds[LIGHT_QUEUE_CAPACITY];

    void floodFillBlockLight(LightNode* seeds, int seedCount) const;
    void floodFillSkyLight(LightNode* seeds, int seedCount) const;
    void continueSkyFlood() const;   // una tanda de lo pendiente (se llama una vez por cuadro)

    int skylightSubtract = 0;

    Chunk* pool[CAP_CHUNKS + POOL_SPARE];
    bool poolInUse[CAP_CHUNKS + POOL_SPARE];
    int poolCount = 0;
    int poolSize = 0;
    int windowRadius = 1, windowSide = 3, windowCount = 9, maxChunks = 16;
    bool loadingBiome[CAP_CHUNKS] = {};   // leido del disco, calculando bioma y colores en varios cuadros (awaitingLoad sigue en true)
    bool fillPending = false;   // los chunks de la ventana más allá de los primeros 3 x 3 se crean ya con el juego en marcha, no en la pantalla de carga
    Chunk* acquireChunk(int cx, int cz);
    void releaseChunk(Chunk* c);
    void flushOnePendingSave();
};

#endif