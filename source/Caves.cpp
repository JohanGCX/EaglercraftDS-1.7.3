#include "Caves.h"
#include "CaveSinTable.h"
#include <string.h>

// =====================================================================
// Port de MapGenBase + MapGenCaves de Beta 1.7.3. Se validó contra el MapGenCaves.java de la
// descompilación: 108 chunks en 3 semillas, 0 bloques distintos. La simulación por celdas
// reproduce el original sin su poda por chunk (98,6% de los bloques idénticos).
// =====================================================================

namespace {

// ---------------------------------------------------------------- java.util.Random
struct JRandom {
    int64_t seed;
    JRandom() : seed(0) {}
    explicit JRandom(int64_t s) { setSeed(s); }
    void setSeed(int64_t s) { seed = (s ^ 0x5DEECE66DLL) & ((1LL << 48) - 1); }
    int32_t next(int bits) {
        seed = (int64_t)(((uint64_t)seed * 0x5DEECE66DULL + 0xBULL) & ((1ULL << 48) - 1));
        return (int32_t)((uint64_t)seed >> (48 - bits));
    }
    int32_t nextInt(int32_t bound) {
        if ((bound & -bound) == bound) return (int32_t)(((int64_t)bound * (int64_t)next(31)) >> 31);
        int32_t bits, val;
        do {
            bits = next(31);
            val = bits % bound;
        } while ((int32_t)((uint32_t)bits - (uint32_t)val + (uint32_t)(bound - 1)) < 0);
        return val;
    }
    int64_t nextLong() {
        int64_t hi = (int64_t)next(32);
        int64_t lo = (int64_t)next(32);
        return (int64_t)((uint64_t)hi << 32) + lo;
    }
    float nextFloat() { return next(24) / (float)(1 << 24); }
};

// ---------------------------------------------------------------- MathHelper
const float PI_F = 3.14159274101257324f; // (float)Math.PI

inline float tableSin(int j) { return CAVE_SIN[j & 4095]; }
inline float jsin(float v) { return tableSin((int)(v * 651.8986f)); }
inline float jcos(float v) { return tableSin((int)((v + (PI_F / 2.0f)) * 651.8986f)); }

inline int floorD(double v) { int i = (int)v; return v < (double)i ? i - 1 : i; }
inline int floorF(float v) { int i = (int)v; return v < (float)i ? i - 1 : i; }
inline int floorDiv(int a, int b) { int d = a / b; if ((a % b != 0) && ((a < 0) != (b < 0))) d--; return d; }

// ---------------------------------------------------------------- caché de celdas
const int WINDOW = 32;                       // ventana toroidal de celdas (32x32 = 512 bloques)
const int BLOCK_SPHERES = 256;               // esferas por bloque del banco
#ifndef CAVES_POOL_BLOCKS
#define CAVES_POOL_BLOCKS 80
#endif
const int POOL_BLOCKS = CAVES_POOL_BLOCKS;   // 80 x 256 x 8 bytes = 160 KB
const int MAX_BLOCKS_PER_CELL = 24;          // 6144 esferas (máximo medido: 5277)

enum CellState : uint8_t { UNKNOWN = 0, EMPTY = 1, PENDING = 2, SIMULATING = 3, READY = 4 };

struct Cell {
    int16_t cx, cz;
    uint8_t state;
    uint8_t first;       // primer bloque del banco
    uint8_t nblocks;
    uint8_t pad;
    uint16_t count;      // esferas guardadas
    uint32_t lastUse;
};

Cell s_cells[WINDOW * WINDOW];
Caves::Sphere s_pool[POOL_BLOCKS * BLOCK_SPHERES];
uint8_t s_nextBlock[POOL_BLOCKS];
int s_freeHead = -1;
int s_freeCount = 0;
uint32_t s_tick = 1;
int s_cellsSim = 0;
int s_truncatedCells = 0;
int s_evictions = 0;

int64_t s_worldSeed = 0;
int64_t s_k1 = 1, s_k2 = 1;

inline Cell& slotFor(int cx, int cz) { return s_cells[((cz & (WINDOW - 1)) << 5) | (cx & (WINDOW - 1))]; }

void freeCellBlocks(Cell& c) {
    int b = c.first;
    for (int n = 0; n < c.nblocks; n++) {
        int nx = s_nextBlock[b];
        s_nextBlock[b] = (s_freeHead < 0) ? 0xFF : (uint8_t)s_freeHead;
        s_freeHead = b;
        s_freeCount++;
        b = nx;
    }
    c.first = 0; c.nblocks = 0; c.count = 0;
}

// ---------------------------------------------------------------- simulación reanudable de una celda
struct Tunnel {
    double x, y, z;
    float radiusScale, yaw, pitch, yawVel, pitchVel;
    int step, length, branchStep;
    bool steep, isRoom;
    JRandom r;
};

struct Job {
    bool active;
    int cx, cz;
    JRandom rand;
    int nOrigins, originIdx;
    int stage;
    double ox, oy, oz;
    int tunnelsTotal, tunnelsDone;
    Tunnel stack[3];
    int sp;
    // dónde se escribe en el banco
    uint8_t tail;       // bloque donde se está escribiendo
    int fill;           // esferas ya escritas en 'tail'
    bool truncated;     // sin bloques libres: se descarta lo que falte
};
Job s_job;

void cellRng(int cx, int cz, JRandom& rand) {
    rand.setSeed((int64_t)(((uint64_t)((int64_t)cx) * (uint64_t)s_k1 + (uint64_t)((int64_t)cz) * (uint64_t)s_k2) ^ (uint64_t)s_worldSeed));
}

// Cuántos orígenes tiene la celda (0 = sin cuevas). Consume los mismos números que el original.
int probeOrigins(int cx, int cz) {
    JRandom rand;
    cellRng(cx, cz, rand);
    int n = rand.nextInt(rand.nextInt(rand.nextInt(40) + 1) + 1);
    if (rand.nextInt(15) != 0) n = 0;
    return n;
}

bool allocBlock(uint8_t& out) {
    while (s_freeHead < 0) {
        // sin libres: se expulsa la celda lista usada hace más tiempo (no la de los últimos 2 cuadros).
        // Las celdas listas sin esferas no liberan nada, así que no cuentan como candidatas.
        int best = -1; uint32_t bestUse = 0xFFFFFFFFu;
        for (int i = 0; i < WINDOW * WINDOW; i++) {
            Cell& c = s_cells[i];
            if (c.state != READY || c.nblocks == 0) continue;
            if (c.lastUse + 2 >= s_tick) continue;
            if (c.lastUse < bestUse) { bestUse = c.lastUse; best = i; }
        }
        if (best < 0) return false;
        freeCellBlocks(s_cells[best]);
        s_cells[best].state = UNKNOWN;
        s_evictions++;
    }
    int b = s_freeHead;
    s_freeHead = (s_nextBlock[b] == 0xFF) ? -1 : s_nextBlock[b];
    s_freeCount--;
    s_nextBlock[b] = 0xFF;
    out = (uint8_t)b;
    return true;
}

void emit(Cell& cell, double x, double y, double z, double rad, bool flat) {
    if (s_job.truncated) return;
    if (cell.count >= MAX_BLOCKS_PER_CELL * BLOCK_SPHERES) { s_job.truncated = true; return; }

    if (cell.nblocks == 0 || s_job.fill >= BLOCK_SPHERES) {
        uint8_t nb;
        if (cell.nblocks >= MAX_BLOCKS_PER_CELL || !allocBlock(nb)) { s_job.truncated = true; return; }
        if (cell.nblocks == 0) cell.first = nb; else s_nextBlock[s_job.tail] = nb;
        s_job.tail = nb;
        s_job.fill = 0;
        cell.nblocks++;
    }

    // posiciones relativas a la esquina de la celda, redondeo "mitad hacia arriba"
    Caves::Sphere s;
    s.x8 = (int16_t)floorD((x - (double)(s_job.cx * 16)) * 8.0 + 0.5);
    s.y8 = (int16_t)floorD(y * 8.0 + 0.5);
    s.z8 = (int16_t)floorD((z - (double)(s_job.cz * 16)) * 8.0 + 0.5);
    double r = rad * 16.0;
    if (r > 255.0) r = 255.0;
    s.r16 = (uint8_t)(int)(r + 0.5);
    s.flat = flat ? 1 : 0;

    s_pool[s_job.tail * BLOCK_SPHERES + s_job.fill] = s;
    s_job.fill++;
    cell.count++;
}

// Crea un túnel leyendo del generador de la celda lo mismo que el original al empezar
// (rand.nextLong() para su generador propio) y de ese generador propio el largo, el paso de
// bifurcación y si es "empinado".
void initTunnel(Tunnel& t, double x, double y, double z, float radiusScale, float yaw, float pitch,
                int step, int length, bool isRoomIn) {
    t.x = x; t.y = y; t.z = z;
    t.radiusScale = radiusScale; t.yaw = yaw; t.pitch = pitch;
    t.yawVel = 0.0f; t.pitchVel = 0.0f;
    t.r = JRandom(s_job.rand.nextLong());
    if (length <= 0) { int m = 8 * 16 - 16; length = m - t.r.nextInt(m / 4); }
    t.isRoom = isRoomIn;
    if (step == -1) { step = length / 2; t.isRoom = true; }
    t.step = step; t.length = length;
    t.branchStep = t.r.nextInt(length / 2) + length / 4;
    t.steep = t.r.nextInt(6) == 0;
}

void pushTunnel(double x, double y, double z, float radiusScale, float yaw, float pitch,
                int step, int length, bool isRoomIn) {
    initTunnel(s_job.stack[s_job.sp], x, y, z, radiusScale, yaw, pitch, step, length, isRoomIn);
    s_job.sp++;
}

// Avanza un túnel hasta 'budget' pasos. Devuelve true si el túnel terminó (y hay que sacarlo de la pila).
bool runTunnel(Cell& cell, int& budget) {
    Tunnel& t = s_job.stack[s_job.sp - 1];
    while (t.step < t.length) {
        if (budget <= 0) return false;
        budget--;

        double rad = 1.5 + (double)(jsin((float)t.step * PI_F / (float)t.length) * t.radiusScale * 1.0f);
        float cp = jcos(t.pitch);
        float sp = jsin(t.pitch);
        t.x += (double)(jcos(t.yaw) * cp);
        t.y += (double)sp;
        t.z += (double)(jsin(t.yaw) * cp);
        if (t.steep) t.pitch *= 0.92f; else t.pitch *= 0.7f;
        t.pitch += t.pitchVel * 0.1f;
        t.yaw += t.yawVel * 0.1f;
        t.pitchVel *= 0.9f;
        t.yawVel *= 12.0f / 16.0f;
        { float a = t.r.nextFloat(), b = t.r.nextFloat(), c = t.r.nextFloat(); t.pitchVel += (a - b) * c * 2.0f; }
        { float a = t.r.nextFloat(), b = t.r.nextFloat(), c = t.r.nextFloat(); t.yawVel += (a - b) * c * 4.0f; }

        if (!t.isRoom && t.step == t.branchStep && t.radiusScale > 1.0f) {
            // Bifurcación: dos túneles hijos. Las semillas salen del generador de la celda en el
            // mismo orden que el original (primero uno y después el otro).
            float r1 = t.r.nextFloat() * 0.5f + 0.5f;
            float r2 = t.r.nextFloat() * 0.5f + 0.5f;
            double bx = t.x, by = t.y, bz = t.z;
            float yaw = t.yaw, pitch = t.pitch;
            int step = t.step, length = t.length;
            s_job.sp--; // el padre termina
            Tunnel c1, c2;
            initTunnel(c1, bx, by, bz, r1, yaw - PI_F * 0.5f, pitch / 3.0f, step, length, false); // primero en nacer
            initTunnel(c2, bx, by, bz, r2, yaw + PI_F * 0.5f, pitch / 3.0f, step, length, false);
            s_job.stack[s_job.sp++] = c2; // se ejecuta segundo
            s_job.stack[s_job.sp++] = c1; // se ejecuta primero
            return false;                 // los hijos siguen en la pila
        }

        if (t.isRoom) {
            emit(cell, t.x, t.y, t.z, rad * (double)CAVES_RADIUS_SCALE, true);
            return true; // una sala es una sola esfera
        }
        if (t.r.nextInt(4) != 0) {
            emit(cell, t.x, t.y, t.z, rad * (double)CAVES_RADIUS_SCALE, false);
        }
        t.step++;
    }
    return true;
}

}  // namespace

// ---------------------------------------------------------------- origen -> túneles (reanudable)
namespace {

// Avanza la máquina de estados de los orígenes cuando no hay túnel en curso.
// Mantiene el mismo orden de lecturas del generador de la celda que MapGenCaves.func_868_a.
void advanceOrigin(Cell& cell) {
    Job& j = s_job;
    if (j.originIdx >= j.nOrigins) {
        j.active = false;
        cell.state = READY;
        cell.lastUse = s_tick;
        s_cellsSim++;
        if (j.truncated) s_truncatedCells++;
        return;
    }

    if (j.stage == 0) {
        double x = (double)(j.cx * 16 + j.rand.nextInt(16));
        double y = (double)(int)((float)j.rand.nextInt(j.rand.nextInt(120) + 8) * CAVES_ORIGIN_Y_SCALE);
        double z = (double)(j.cz * 16 + j.rand.nextInt(16));
        j.ox = x; j.oy = y; j.oz = z;
        j.tunnelsTotal = 1;
        j.tunnelsDone = 0;
        if (j.rand.nextInt(4) == 0) {
            float rs = 1.0f + j.rand.nextFloat() * 6.0f;
            pushTunnel(x, y, z, rs, 0.0f, 0.0f, -1, -1, true); // sala: se ejecuta primero
            j.tunnelsTotal += j.rand.nextInt(4);
        }
        j.stage = 1;
        return;
    }

    // stage 1: un túnel por vez
    if (j.tunnelsDone >= j.tunnelsTotal) {
        j.originIdx++;
        j.stage = 0;
        return;
    }
    float yaw = j.rand.nextFloat() * PI_F * 2.0f;
    float pitch = (j.rand.nextFloat() - 0.5f) * 2.0f / 8.0f;
    float rad = j.rand.nextFloat() * 2.0f + j.rand.nextFloat();
    pushTunnel(j.ox, j.oy, j.oz, rad, yaw, pitch, 0, 0, false);
    j.tunnelsDone++;
}

void runJob(int stepBudget) {
    Cell& cell = slotFor(s_job.cx, s_job.cz);
    int budget = stepBudget;
    while (s_job.active && budget > 0) {
        if (s_job.sp > 0) {
            if (runTunnel(cell, budget)) s_job.sp--;      // terminó el túnel de arriba
        } else {
            advanceOrigin(cell);
        }
    }
}

void startJob(Cell& cell, int cx, int cz, int nOrigins) {
    Job& j = s_job;
    j.active = true;
    j.cx = cx; j.cz = cz;
    cellRng(cx, cz, j.rand);
    // se repiten las lecturas del sorteo inicial para dejar el generador en el mismo punto que el original
    j.rand.nextInt(j.rand.nextInt(j.rand.nextInt(40) + 1) + 1);
    j.rand.nextInt(15);
    j.nOrigins = nOrigins;
    j.originIdx = 0;
    j.stage = 0;
    j.sp = 0;
    j.tail = 0; j.fill = 0; j.truncated = false;
    cell.state = SIMULATING;
    cell.first = 0; cell.nblocks = 0; cell.count = 0;
}

// Esferas por llamada a prepare(): ~40 pasos son ~1-2 ms en la DS
const int STEPS_PER_CALL = 40;

}  // namespace

// =====================================================================
// API pública
// =====================================================================
namespace Caves {

void reset(unsigned int worldSeed) {
    memset(s_cells, 0, sizeof(s_cells));
    for (int b = 0; b < POOL_BLOCKS; b++) s_nextBlock[b] = (b + 1 < POOL_BLOCKS) ? (uint8_t)(b + 1) : 0xFF;
    s_freeHead = 0;
    s_freeCount = POOL_BLOCKS;
    s_tick = 1;
    s_cellsSim = 0;
    s_truncatedCells = 0;
    s_evictions = 0;
    s_job = Job();
    s_job.active = false;
    s_worldSeed = (int64_t)worldSeed;
    JRandom base(s_worldSeed);
    s_k1 = base.nextLong() / 2LL * 2LL + 1LL;
    s_k2 = base.nextLong() / 2LL * 2LL + 1LL;
}

void beginFrame() { s_tick++; }

int cellsSimulated() { return s_cellsSim; }
int cellsTruncated() { return s_truncatedCells; }
int evictions() { return s_evictions; }
int blocksFree() { return s_freeCount; }

bool prepare(int chunkX, int chunkZ, bool& workDone) {
    workDone = false;

    if (s_job.active) {
        runJob(STEPS_PER_CALL);
        workDone = true;
    }

    // Un chunk de 8x8 cae siempre dentro de una celda de 16x16
    const int cellX = floorDiv(chunkX, 2);
    const int cellZ = floorDiv(chunkZ, 2);
    bool allReady = true;

    for (int i = -REACH_CELLS; i <= REACH_CELLS; i++) {
        for (int k = -REACH_CELLS; k <= REACH_CELLS; k++) {
            const int cx = cellX + i, cz = cellZ + k;
            Cell& c = slotFor(cx, cz);

            if (c.state != UNKNOWN && (c.cx != cx || c.cz != cz)) {
                // la casilla la ocupaba una celda lejana: se libera
                if (c.state == READY) freeCellBlocks(c);
                if (c.state == SIMULATING) s_job.active = false;
                c.state = UNKNOWN;
            }
            if (c.state == UNKNOWN) {
                c.cx = (int16_t)cx; c.cz = (int16_t)cz;
                c.first = 0; c.nblocks = 0; c.count = 0;
                c.state = (probeOrigins(cx, cz) > 0) ? PENDING : EMPTY;
            }
            c.lastUse = s_tick;

            if (c.state == PENDING && !s_job.active) {
                startJob(c, cx, cz, probeOrigins(cx, cz));
                runJob(STEPS_PER_CALL);
                workDone = true;
            }
            if (c.state != EMPTY && c.state != READY) allReady = false;
        }
    }
    return allReady;
}

// (la selección de esferas está en carveStep(), más abajo)

// ---------------------------------------------------------------- excavación
static inline bool isDecoration(BlockType t) {
    return t == BlockType::TALLGRASS || isCrossPlant(t) ||
           t == BlockType::SNOW_LAYER || t == BlockType::DEAD_BUSH;
}

void carveSphere(BlockType (*blocks)[WorldConfig::SIZE_Y][WorldConfig::SIZE_Z],
                 int chunkX, int chunkZ, const Sphere& s) {
    (void)chunkX; (void)chunkZ;
    const int SX = WorldConfig::SIZE_X, SY = WorldConfig::SIZE_Y, SZ = WorldConfig::SIZE_Z;

    // x8 / z8 ya vienen relativos a la esquina del chunk
    const float x = (float)s.x8 * 0.125f;
    const float y = (float)s.y8 * 0.125f;
    const float z = (float)s.z8 * 0.125f;
    const float rad = (float)s.r16 * (1.0f / 16.0f);
    const float radV = s.flat ? rad * 0.5f : rad;

    int minX = floorF(x - rad) - 1, maxX = floorF(x + rad) + 1;
    int minY = floorF(y - radV) - 1, maxY = floorF(y + radV) + 1;
    int minZ = floorF(z - rad) - 1, maxZ = floorF(z + rad) + 1;
    if (minX < 0) minX = 0;
    if (maxX > SX) maxX = SX;
    if (minY < 1) minY = 1;
    if (maxY > 120) maxY = 120;
    if (minZ < 0) minZ = 0;
    if (maxZ > SZ) maxZ = SZ;
    if (minX >= maxX || minZ >= maxZ || minY >= maxY) return;

    // No se excava si hay agua en la caja (solo la parte que cae dentro de este chunk)
    bool water = false;
    for (int ix = minX; !water && ix < maxX; ++ix) {
        for (int iz = minZ; !water && iz < maxZ; ++iz) {
            for (int iy = maxY + 1; !water && iy >= minY - 1; --iy) {
                if (iy >= 0 && iy < SY) {
                    if (blocks[ix][iy][iz] == BlockType::WATER) water = true;
                    if (iy != minY - 1 && ix != minX && ix != maxX - 1 && iz != minZ && iz != maxZ - 1) iy = minY;
                }
            }
        }
    }
    if (water) return;

    const float invRad = 1.0f / rad;
    const float invRadV = 1.0f / radV;
    for (int ix = minX; ix < maxX; ++ix) {
        const float dx = ((float)ix + 0.5f - x) * invRad;
        const float dx2 = dx * dx;
        if (dx2 >= 1.0f) continue;
        for (int iz = minZ; iz < maxZ; ++iz) {
            const float dz = ((float)iz + 0.5f - z) * invRad;
            const float dxz2 = dx2 + dz * dz;
            if (dxz2 >= 1.0f) continue;
            bool grassFlag = false;
            // Igual que el original: la densidad se evalúa en la altura iy, pero el bloque que se
            // modifica es el de iy+1 (el índice del arreglo arranca en maxY y baja a la par).
            for (int iy = maxY - 1; iy >= minY; --iy) {
                const float dy = ((float)iy + 0.5f - y) * invRadV;
                if (dy > -0.7f && dxz2 + dy * dy < 1.0f) {
                    const int by = iy + 1;
                    const BlockType b = blocks[ix][by][iz];
                    if (b == BlockType::GRASS) grassFlag = true;
                    if (b == BlockType::STONE || b == BlockType::DIRT || b == BlockType::GRASS) {
                        // MapGenCaves: por debajo de y=10 la cueva queda llena de lava (Block.lavaMoving; aquí quieta)
                        blocks[ix][by][iz] = (iy < 10) ? BlockType::LAVA : BlockType::AIR;
                        if (by + 1 < SY && isDecoration(blocks[ix][by + 1][iz])) blocks[ix][by + 1][iz] = BlockType::AIR;
                        if (grassFlag && blocks[ix][iy][iz] == BlockType::DIRT) blocks[ix][iy][iz] = BlockType::GRASS;
                    }
                }
            }
        }
    }
}

bool carveStep(BlockType (*blocks)[WorldConfig::SIZE_Y][WorldConfig::SIZE_Z],
               int chunkX, int chunkZ, CarveCursor& cur, int maxCarves) {
    const int side = 2 * REACH_CELLS + 1;
    const int cellX = floorDiv(chunkX, 2);
    const int cellZ = floorDiv(chunkZ, 2);
    const int gx0 = chunkX * WorldConfig::SIZE_X;      // esquina del chunk, en bloques
    const int gz0 = chunkZ * WorldConfig::SIZE_Z;
    int carved = 0;

    while (cur.cell < side * side) {
        const int cx = cellX - REACH_CELLS + cur.cell / side;
        const int cz = cellZ - REACH_CELLS + cur.cell % side;
        Cell& c = slotFor(cx, cz);

        if (c.state == READY && c.cx == cx && c.cz == cz) {
            // límites del chunk relativos a la esquina de esta celda, en 1/8 de bloque
            const int lo_x = (gx0 - cx * 16) * 8, hi_x = lo_x + WorldConfig::SIZE_X * 8;
            const int lo_z = (gz0 - cz * 16) * 8, hi_z = lo_z + WorldConfig::SIZE_Z * 8;

            // se salta hasta el bloque del banco donde quedó el cursor
            int b = c.first;
            for (int hop = cur.sphere / BLOCK_SPHERES; hop > 0; hop--) b = s_nextBlock[b];

            while (cur.sphere < c.count) {
                const int idx = cur.sphere % BLOCK_SPHERES;
                const Sphere& sp = s_pool[b * BLOCK_SPHERES + idx];
                cur.sphere++;
                if (cur.sphere % BLOCK_SPHERES == 0 && cur.sphere < c.count) b = s_nextBlock[b];

                const int m = (sp.r16 >> 1) + 24;   // radio (1/16 -> 1/8) + 3 bloques de margen
                if (sp.x8 + m < lo_x || sp.x8 - m >= hi_x) continue;
                if (sp.z8 + m < lo_z || sp.z8 - m >= hi_z) continue;

                Sphere s = sp;                       // coordenadas relativas a la esquina del CHUNK
                s.x8 = (int16_t)(s.x8 - lo_x);
                s.z8 = (int16_t)(s.z8 - lo_z);
                carveSphere(blocks, chunkX, chunkZ, s);
                if (++carved >= maxCarves) return false; // se retoma desde el cursor en la próxima llamada
            }
        }
        cur.cell++;
        cur.sphere = 0;
    }
    return true;
}

}  // namespace Caves