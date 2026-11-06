#ifndef CHUNK_H
#define CHUNK_H

#include "Cube.h"
#include "BlockFace.h"
#include "BlockType.h"
#include "WorldConfig.h"
#include "Biome.h"
#include "Climate.h"
#include "Caves.h"

class DisplayListBuilder;

class Chunk {
public:
    Chunk(int chunkX, int chunkZ, unsigned int seed);
    ~Chunk();
    void draw(float playerX, float playerY, float playerZ,
              float lookDirX, float lookDirY, float lookDirZ, const class ChunkManager* manager = nullptr,
              int* vertBudget = nullptr, int* rebuildBandBudget = nullptr);
    void update(const class ChunkManager* manager = nullptr);
    int highestGeneratedY = 0;
    bool refreshBorderFaces(const class ChunkManager* manager, int maxColumns = 1000);   // true = termino todo el borde (si no, sigue en la proxima llamada)
    int borderRefreshCursor = 0;
    void updateFaceCache(int xi, int yi, int zi, const class ChunkManager* manager);
    void setBlockDirect(int xi, int yi, int zi, BlockType type, const class ChunkManager* manager);
    // Bloques parciales (puertas, trampilla, cama): ponen el bloque con su metadata y cambian solo la metadata (ver PartialBlocks.h)
    void setBlockWithMeta(int xi, int yi, int zi, BlockType type, int meta, const class ChunkManager* manager);
    void setBlockMeta(int xi, int yi, int zi, int meta, const class ChunkManager* manager);
    void setMetaQuiet(int xi, int yi, int zi, int meta);   // solo cambia la metadata (marca de hoja, etapa del retoño): no reconstruye geometria

    bool isSolidAt(float x, float y, float z) const;
    MC_HOT bool worldToGrid(float worldX, float worldY, float worldZ,
                      int& outXi, int& outYi, int& outZi) const;
    float gridToWorldX(int xi) const;
    float gridToWorldY(int yi) const;
    float gridToWorldZ(int zi) const;

    void drawOutlineAt(int xi, int yi, int zi);
    bool getSelectionBounds(int xi, int yi, int zi, float bd[6]) const;   // caja de seleccion dentro de la celda (0 a 1); la usan el contorno negro y la grieta
    void breakBlockAt(int xi, int yi, int zi, const class ChunkManager* manager);
    void placeBlockAt(int xi, int yi, int zi, BlockFace face, const class ChunkManager* manager);

    bool isGenerationComplete() const;

    bool saveToFile(const char* path) const;
    bool loadFromFile(const char* path);   // lee el archivo (etapa 1); el chunk queda "cargando" hasta que continueLoad() devuelva true
    bool continueLoad();                   // etapa 2, repartida en cuadros: clima, bioma y colores, ocupacion. true = el chunk quedo completo

    int getHighestSolidY(int xi, int zi) const;
    // Inline: it is called from many .cpp files, and a real function call cost more than the lookup itself.
    // One unsigned compare per axis covers both the negative and the too-big case.
    BlockType getBlockType(int xi, int yi, int zi) const {
        if ((unsigned)xi >= (unsigned)WorldConfig::SIZE_X || (unsigned)yi >= (unsigned)WorldConfig::SIZE_Y || (unsigned)zi >= (unsigned)WorldConfig::SIZE_Z) return BlockType::AIR;
        return blocks[xi][yi][zi];
    }

    int getChunkX() const { return chunkX; }
    int getChunkZ() const { return chunkZ; }
    bool isModified() const { return modified && generationComplete; }   // un chunk a medio generar NUNCA se guarda: al recargarlo quedaba "completo" pero vacio
    void getSaveFilename(char* outPath) const;
    static void getSaveFilenameFor(int chunkX, int chunkZ, char* outPath);
    void recalculateFullFaceCache(const class ChunkManager* manager);

    void beginFullFaceCacheRecalc();
    void continueFaceCacheRecalc(const class ChunkManager* manager);
    void continueInitialSkyLight(const class ChunkManager* manager);
    bool isCacheRecalcPending() const { return cacheRecalcPending; }
    bool placeBlockAt(int xi, int yi, int zi, BlockFace face, BlockType typeToPlace, const class ChunkManager* manager);
    static int s_placeMeta;   // metadata que se le pone al bloque colocado (-1 = ninguna): la cara tallada de la calabaza
    bool flowerSupportedAt(int xi, int yi, int zi) const;
    bool plantSupportedAt(BlockType plant, int xi, int yi, int zi, const class ChunkManager* manager) const;
    static void getSaveFilenameFor(int chunkX, int chunkZ, unsigned int seed, char* outPath);
    bool isForestAt(int xi, int zi) const;
    void generateTrees(const class ChunkManager* manager, int startXi, int startZi, int& outXi, int& outZi, bool& done);
    void generateOres(const class ChunkManager* manager, int startAttempt, int& outAttempt, bool& done);
    void generateCactus(const class ChunkManager* manager, int startXi, int startZi, int& outXi, int& outZi, bool& done);
    void generateSpruceTree1(const class ChunkManager* manager, int xi, int zi, int highestY);
    bool generateBigOakTree(const class ChunkManager* manager, int xi, int zi, int highestY);
    BlockType bigTreeGetBlock(const class ChunkManager* manager, int wx, int wy, int wz) const;
    void bigTreeSetBlock(const class ChunkManager* manager, int wx, int wy, int wz, BlockType type);
    void generateSpruceTree2(const class ChunkManager* manager, int xi, int zi, int highestY);
    void snowifyTreeTop(const class ChunkManager* manager, int xi, int zi, int maxRadius, int baseY, int maxHeight);

    int structureGenPhase = 0;
    int structureGenXi = 0;
    int structureGenZi = 0;
    int structureGenAttempt = 0;

    struct TreePos { int xi; int zi; };
    TreePos treesPlaced[16];
    int treesPlacedCount = 0;

    void checkTorchSupport(int xi, int yi, int zi, const class ChunkManager* manager);
    BiomeType getBiomeAt(int xi, int zi) const;
    void getColumnOccupancyBits(int xi, int zi, unsigned int outBits[4]) const;
    void recomputeColumn(int xi, int zi, const class ChunkManager* manager);
    void recomputeColumnRange(int xi, int zi, int yiLo, int yiHi, const class ChunkManager* manager); // igual, pero ensucia solo esas alturas
    bool isInitialSkyLightPending() const { return initialSkyLightPending; }
    bool hasBuiltDisplayList() const { return displayListEverBuilt; }
    static int getDisplayListBytes();

    int getSkyBottom(int xi, int zi) const { return skyBottom[xi][zi]; }
    bool isSkyBaseReady() const { return skyBaseReady; }
    static void beginRebuildFrame();
    void scheduleFullRefresh();
    static void beginHeavyFrame();
    static void setBudgetPercent(int percent);   // escala los presupuestos de tiempo de este cuadro (Governor): 100 = normales
    static bool heavyBudgetLeft();
    void freeDisplayLists();
    void setInWindow(bool v) {
        if (v == inWindow) return;
        inWindow = v;
        if (!v) freeDisplayLists();
        else markDisplayListDirty();
    }

    // Toda la lista del chunk. Es lo que hace falta cuando cambia mucho (generación, vecinos, luz inicial)...
    void markDisplayListDirty() {
        for (int b = 0; b < DL_BAND_COUNT; b++) bandDirty.v[b] = 1;
        displayListDirty = true;
        lodDirty = true;
    }
    // ...y esto, cuando cambia una zona: solo las bandas que tocan las alturas yiLo a yiHi. Antes un bloque roto, un cambio de luz o un
    // chunk vecino nuevo reconstruían las 64 bandas del chunk entero.
    void markBandRangeDirty(int yiLo, int yiHi) {
        if (yiLo < 0) yiLo = 0;
        if (yiHi >= WorldConfig::SIZE_Y) yiHi = WorldConfig::SIZE_Y - 1;
        for (int b = yiLo / DL_BAND_HEIGHT; b <= yiHi / DL_BAND_HEIGHT; b++) bandDirty.v[b] = 1;
        displayListDirty = true;
        lodDirty = true;
    }
    void markLightDirty() { markDisplayListDirty(); }

    u8 getBlockLight(int xi, int yi, int zi) const { return lightData[xi][yi][zi] & 0x0F; }
    u8 getSkyLight(int xi, int yi, int zi) const { return (lightData[xi][yi][zi] >> 4) & 0x0F; }
    void setBlockLight(int xi, int yi, int zi, u8 v) {
        lightData[xi][yi][zi] = (lightData[xi][yi][zi] & 0xF0) | (v & 0x0F);
        markBandRangeDirty(yi - 1, yi + 1); // una luz distinta cambia el color de las caras de las celdas vecinas
    }
    void setSkyLight(int xi, int yi, int zi, u8 v) {
        lightData[xi][yi][zi] = (lightData[xi][yi][zi] & 0x0F) | ((v & 0x0F) << 4);
        markBandRangeDirty(yi - 1, yi + 1);
    }
    void reseedEmitters(const class ChunkManager* manager);

    // Dimensión de los chunks que se crean y se cargan ahora: 0 = superficie, -1 = Nether (la fija main.cpp al entrar a un mundo o al viajar)
    static void setDimension(int dim);
    static int getDimension();

    // Líquidos: nivel 0..7 (+8 si cae) en 4 bits por celda
    int  getLiquidMeta(int xi, int yi, int zi) const { return liquidMetaGet(xi, yi, zi); }
    void putLiquidCell(int xi, int yi, int zi, BlockType type, int meta, const class ChunkManager* manager);
    void notifyLiquidNeighbors(int xi, int yi, int zi, const class ChunkManager* manager);
    bool hasEmitters() const { return mayHaveEmitters; }
    // Re-sembrar los emisores de un chunk (lava, antorchas) ya no se hace de golpe: se pide y se reparte, unas columnas por cuadro
    void requestEmitterReseed() { if (mayHaveEmitters) emitterReseedColumn = 0; }
    bool isEmitterReseedPending() const { return emitterReseedColumn >= 0; }
    void continueEmitterReseed(const class ChunkManager* manager);

private:
    Cube cube;
    static constexpr int DL_BAND_HEIGHT = 2; // WorldConfig::SIZE_Y debe ser múltiplo de esto. Era 4: las bandas de 4 capas tardaban 20 a 44 ms en reconstruirse en el bosque
    static constexpr int DL_BAND_COUNT = WorldConfig::SIZE_Y / DL_BAND_HEIGHT;
    u32* dlBands[DL_BAND_COUNT] = {};    // mitad x < SIZE_X/2 de cada banda
    u32* dlBands2[DL_BAND_COUNT] = {};   // mitad x >= SIZE_X/2 (cada mitad se arma en un cuadro distinto: ~la mitad del pico)
    int dlBandVertCount2[DL_BAND_COUNT] = {};
    int displayListRebuildHalf = 0;
    int dlBandVertCount[DL_BAND_COUNT] = {};
    struct BandFlags {   // qué bandas hay que reconstruir (al principio, todas)
        u8 v[DL_BAND_COUNT];
        BandFlags() { for (int i = 0; i < DL_BAND_COUNT; i++) v[i] = 1; }
    } bandDirty;
    bool displayListDirty = true;
    // Lista de dibujo del camino LOD (la capa superior de todo el chunk, una sola llamada en vez de 64 quads por cuadro con aritmetica de punto flotante por software)
    u32* dlLod = nullptr;
    int dlLodVerts = 0;
    bool lodDirty = true;
    bool lodCoarse = false;   // con que detalle se armo (2 x 2 columnas por quad cuando esta lejos)
    bool buildLodList(bool coarse);
    void freeLodList();
    bool displayListDirtyDuringRebuild = false;
    bool displayListEverBuilt = false;
    u8 lightData[WorldConfig::SIZE_X][WorldConfig::SIZE_Y][WorldConfig::SIZE_Z] = {};

    void continueDisplayListRebuild(const class ChunkManager* manager, int* rebuildBandBudget);
    void buildBandGeometry(DisplayListBuilder& b, int bandIndex, const class ChunkManager* manager, int xBegin = 0, int xEnd = WorldConfig::SIZE_X);
    int displayListRebuildBand = 0;
    static constexpr int DL_REBUILD_BANDS_PER_CALL = 2;

    u32 canaryBeforeBlocks = 0xDEAD0001;
    BlockType blocks[WorldConfig::SIZE_X][WorldConfig::SIZE_Y][WorldConfig::SIZE_Z];
    u32 canaryAfterBlocks = 0xDEAD0002;
    u32 canaryBeforeFaceCache = 0xDEAD0003;
    FaceMask faceCache[WorldConfig::SIZE_X][WorldConfig::SIZE_Y][WorldConfig::SIZE_Z];
    u32 canaryAfterFaceCache = 0xDEAD0004;
	
    int chunkX, chunkZ;

    int genXi, genZi;
    bool generationComplete;

    void generateColumn(int xi, int zi);
    bool isSolidGrid(int xi, int yi, int zi) const;

    int columnMinY[WorldConfig::SIZE_X][WorldConfig::SIZE_Z];
    int columnMaxY[WorldConfig::SIZE_X][WorldConfig::SIZE_Z];
    bool modified = false;

    bool cacheRecalcPending = false;
    int cacheRecalcXi = 0, cacheRecalcZi = 0;
    bool inWindow = true;
    void rebuildAllOccupancy();

    static constexpr int CACHE_RECALC_BATCH = 8;
    unsigned int chunkSeed;
    BiomeType biomeColumn[WorldConfig::SIZE_X][WorldConfig::SIZE_Z];
    u8 grassTint[WorldConfig::SIZE_X][WorldConfig::SIZE_Z][3];     // BlockGrass.colorMultiplier: promedio de las 3 x 3 columnas de alrededor
    u8 foliageTint[WorldConfig::SIZE_X][WorldConfig::SIZE_Z][3];   // BlockLeaves.colorMultiplier
    Climate::Lattice lat;                                          // temperatura y humedad medidas en una malla sobre este chunk
    bool climateReady = false;
    bool loadStaging = false;   // leido del disco, falta calcular bioma y colores
    int loadBiomeIdx = 0;
    u8 tallGrassPlaced = 0, flowersPlaced = 0;                    // topes de vegetación por chunk (generatePlantClusters)

    u8 skyBottom[WorldConfig::SIZE_X][WorldConfig::SIZE_Z] = {}; // Y más baja con cielo directo
    int skyInitPhase = 0;
    int skyInitDir = 0;   // fase B: por cuál de las 4 direcciones de la columna se va (cada una inunda hasta MAX_FLOOD_STEPS_PER_CALL pasos)
    int climateRow = 0;   // filas de la malla de clima ya medidas (de 4)
    bool skyBaseReady = false;
    static constexpr int SKY_BASE_BATCH = 16; // columnas por llamada en la fase A (barata)
    void spreadSkyFromColumn(int xi, int zi, const class ChunkManager* manager, int onlyDir = -1);

    u32 canaryBeforeOccupancy = 0xDEAD0005;
    unsigned int occupancy[WorldConfig::SIZE_X][WorldConfig::SIZE_Z][4] = {};
    u32 canaryAfterOccupancy = 0xDEAD0006;
    void rebuildColumnOccupancy(int xi, int zi);
    void computeColumnFacesBitwise(int xi, int zi, const class ChunkManager* manager);
    u8 getFaceLightRaw(int nxi, int nyi, int nzi, const class ChunkManager* manager) const;
    bool initialSkyLightPending = false;
    bool checkMemoryCanaries() const {
        return canaryBeforeBlocks == 0xDEAD0001 && canaryAfterBlocks == 0xDEAD0002 &&
               canaryBeforeFaceCache == 0xDEAD0003 && canaryAfterFaceCache == 0xDEAD0004 &&
               canaryBeforeOccupancy == 0xDEAD0005 && canaryAfterOccupancy == 0xDEAD0006;
    }
    int initialSkyLightXi = 0, initialSkyLightZi = 0;
    static constexpr int SKYLIGHT_INIT_BATCH = 1; // columnas por frame

    bool mayHaveEmitters = false;
    int emitterReseedColumn = -1;   // próxima columna (0 a 63) de la re-siembra pendiente; -1 = nada pendiente
    void reseedColumnEmitters(int xi, int zi, const class ChunkManager* manager);

    // Cuevas: se excavan después del terreno y antes de árboles y menas
    bool cavesDone = false;
    Caves::CarveCursor caveCursor;
    bool generateCaves(const class ChunkManager* manager);

    // Lagos y manantiales de líquido (WorldGenLakes y WorldGenLiquids de Beta 1.7.3)
    int lakeUnit = 0;   // 3x3 chunks de origen x 2 (agua y lava)
    bool lakeBuilt = false;   // el lago ya se calculo (Lakes::build) y falta escribirlo (Lakes::apply), en otra llamada
    int lakeWarm = 0;   // columnas del terreno de la región del lago ya calculadas
    bool generateLakes(const class ChunkManager* manager);
    void generateSprings(const class ChunkManager* manager);

    // Antorchas de Beta 1.7.3 (BlockTorch + ItemBlock)
    BlockType typeNear(int xi, int yi, int zi, const class ChunkManager* manager, BlockType unknown) const;
    bool tryPlaceTorch(int xi, int yi, int zi, int faceCode, const class ChunkManager* manager);
    bool placeTorchFromClick(int xi, int yi, int zi, BlockFace face, const class ChunkManager* manager);
    void reorientLegacyTorch(int xi, int yi, int zi, const class ChunkManager* manager);

    // Líquidos (BlockFluid, RenderBlocks.renderBlockFluids)
    static constexpr int LIQUID_META_BYTES = WorldConfig::SIZE_X * WorldConfig::SIZE_Y * WorldConfig::SIZE_Z / 2;
    u8 liquidMeta[LIQUID_META_BYTES] = {};
    int liquidMetaGet(int xi, int yi, int zi) const {
        int i = (xi * WorldConfig::SIZE_Y + yi) * WorldConfig::SIZE_Z + zi;
        return (liquidMeta[i >> 1] >> ((i & 1) * 4)) & 0x0F;
    }
    void liquidMetaSet(int xi, int yi, int zi, int v) {
        int i = (xi * WorldConfig::SIZE_Y + yi) * WorldConfig::SIZE_Z + zi;
        int sh = (i & 1) * 4;
        liquidMeta[i >> 1] = (u8)((liquidMeta[i >> 1] & ~(0x0F << sh)) | ((v & 0x0F) << sh));
    }
    int metaNear(int xi, int yi, int zi, const class ChunkManager* manager) const;
    void emitLiquidCell(DisplayListBuilder& b, int xi, int yi, int zi, float lx, float ly, float lz, const class ChunkManager* manager);
    void emitFireCell(DisplayListBuilder& b, int xi, int yi, int zi, float lx, float ly, float lz, const class ChunkManager* manager);
    void emitPortalCell(DisplayListBuilder& b, int xi, int yi, int zi, float lx, float ly, float lz, const class ChunkManager* manager);
    void emitPartialCell(DisplayListBuilder& b, int xi, int yi, int zi, float lx, float ly, float lz, const class ChunkManager* manager);
    void emitRedstoneCell(DisplayListBuilder& b, int xi, int yi, int zi, float lx, float ly, float lz, const class ChunkManager* manager);
    void generateNetherColumn(int xi, int zi);
    bool generateNetherFeatures(const class ChunkManager* manager);
    bool generatePlantClusters(const class ChunkManager* manager);
    void generateDungeons(const class ChunkManager* manager);   // mazmorras (WorldGenDungeons) que caben enteras en este chunk
    void ensureClimate();
    void climateAt(int gx, int gz, float& t, float& h) const { lat.at(gx, gz, t, h); }
    void computeColumnBiome(int xi, int zi);   // bioma del clima + colores del pasto y de las hojas de una columna
    void computeBiomeData();                   // lo mismo para todas las columnas (chunks cargados del disco)
    void columnTint(BlockType type, int xi, int zi, u8& r, u8& g, u8& b) const;
    bool chestPlacementOk(int xi, int yi, int zi, const class ChunkManager* manager) const;
};

#endif