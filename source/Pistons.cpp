#include "Pistons.h"
#include "Redstone.h"
#include "PartialBlocks.h"
#include "ChunkManager.h"
#include "Chunk.h"
#include "PendingDrops.h"
#include "Texture.h"
#include "WorldConfig.h"
#include <math.h>
#include <string.h>

// Coordenadas globales de celda: gx = chunkX * SIZE_X + xi, gy = yi, gz = chunkZ * SIZE_Z + zi (las mismas de Redstone.cpp).
// La celda (gx, gy, gz) ocupa en el mundo X de gx - 4 a gx - 3, Y de gy - 1,5 a gy - 0,5 y Z de gz - 4 a gz - 3.
// Direcciones (PistonBlockTextures): 0 abajo, 1 arriba, 2 -Z, 3 +Z, 4 -X, 5 +X.

namespace {
    const int SIZE_X = WorldConfig::SIZE_X, SIZE_Y = WorldConfig::SIZE_Y, SIZE_Z = WorldConfig::SIZE_Z;

    const ChunkManager* W = nullptr;

    const int DX[6] = { 0, 0, 0, 0, -1, 1 };    // PistonBlockTextures.field_31056_b
    const int DY[6] = { -1, 1, 0, 0, 0, 0 };    // field_31059_c
    const int DZ[6] = { 0, 0, -1, 1, 0, 0 };    // field_31058_d
    const int OPP[6] = { 1, 0, 3, 2, 5, 4 };    // field_31057_a

    bool g_busy = false;   // BlockPistonBase.field_31048_b: mientras se mueve, no se revisa de nuevo

    // ---- Acceso al mundo ----
    int floorDivi(int v, int d) { return v >= 0 ? v / d : -((-v + d - 1) / d); }

    Chunk* chunkOf(int gx, int gz, int& xi, int& zi) {
        const int cx = floorDivi(gx, SIZE_X), cz = floorDivi(gz, SIZE_Z);
        Chunk* c = W->getChunkByChunkCoord(cx, cz);
        if (!c || !c->isGenerationComplete()) return nullptr;
        xi = gx - cx * SIZE_X;
        zi = gz - cz * SIZE_Z;
        return c;
    }

    // Lo que no esta cargado cuenta como piedra
    BlockType T(int x, int y, int z) {
        if (y < 0) return BlockType::STONE;
        if (y >= SIZE_Y) return BlockType::AIR;
        int xi, zi; Chunk* c = chunkOf(x, z, xi, zi);
        return c ? c->getBlockType(xi, y, zi) : BlockType::STONE;
    }

    int M(int x, int y, int z) {
        if (y < 0 || y >= SIZE_Y) return 0;
        int xi, zi; Chunk* c = chunkOf(x, z, xi, zi);
        return c ? c->getLiquidMeta(xi, y, zi) : 0;
    }

    void setM(int x, int y, int z, int m) {   // setBlockMetadata: sin avisar
        if (y < 0 || y >= SIZE_Y) return;
        int xi, zi; Chunk* c = chunkOf(x, z, xi, zi);
        if (c) c->setBlockMeta(xi, y, zi, m, W);
    }

    void setBare(int x, int y, int z, BlockType t, int m) {   // setBlockAndMetadata: cambia el bloque sin avisar
        if (y < 0 || y >= SIZE_Y) return;
        int xi, zi; Chunk* c = chunkOf(x, z, xi, zi);
        if (c) c->setBlockWithMeta(xi, y, zi, t, m, W);
    }

    void setNotify(int x, int y, int z, BlockType t, int m) {   // setBlockAndMetadataWithNotify
        if (y < 0 || y >= SIZE_Y) return;
        int xi, zi; Chunk* c = chunkOf(x, z, xi, zi);
        if (!c) return;
        c->setBlockWithMeta(xi, y, zi, t, m, W);
        c->notifyLiquidNeighbors(xi, y, zi, W);
    }

    void dropItem(int x, int y, int z, BlockType item) {
        if (item == BlockType::AIR) return;
        PendingDrops::push((float)x - SIZE_X / 2.0f + 0.5f, (float)y - 1.0f, (float)z - SIZE_Z / 2.0f + 0.5f, item);
    }

    // ---- Bloques en movimiento (TileEntityPiston) ----
    struct Moving {
        bool active;
        int x, y, z;
        BlockType type;      // el bloque que lleva (storedBlockID)
        int meta;            // su metadata (storedMetadata)
        int facing;          // field_31025_c
        bool extending;      // field_31024_i
        bool source;         // field_31023_j: es el piston que se esta replegando
        float progress, last;   // field_31022_k, field_31020_l
    };
    const int MAX_MOVING = 128;
    Moving g_m[MAX_MOVING];

    Moving* findAt(int x, int y, int z) {
        for (int i = 0; i < MAX_MOVING; i++)
            if (g_m[i].active && g_m[i].x == x && g_m[i].y == y && g_m[i].z == z) return &g_m[i];
        return nullptr;
    }

    // BlockPistonMoving.func_31036_a: el TileEntityPiston nuevo (progreso 0)
    void addMoving(int x, int y, int z, BlockType t, int meta, int facing, bool extending, bool source) {
        Moving* e = findAt(x, y, z);
        if (!e) {
            for (int i = 0; i < MAX_MOVING; i++) if (!g_m[i].active) { e = &g_m[i]; break; }
        }
        if (!e) return;   // sin lugar: el bloque queda como PISTON_MOVING huerfano y se limpia solo
        e->active = true; e->x = x; e->y = y; e->z = z;
        e->type = t; e->meta = meta; e->facing = facing; e->extending = extending; e->source = source;
        e->progress = 0.0f; e->last = 0.0f;
    }

    // TileEntityPiston.func_31011_l: termina el movimiento ya y deja el bloque en su lugar
    void forceFinish(Moving* e) {
        if (e->last < 1.0f) {
            e->last = e->progress = 1.0f;
            e->active = false;
            const int x = e->x, y = e->y, z = e->z;
            const BlockType t = e->type; const int m = e->meta;
            if (T(x, y, z) == BlockType::PISTON_MOVING) setNotify(x, y, z, t, m);
        }
    }

    // ---- Reglas de movimiento ----
    // Block.getMobilityFlag: 0 se empuja, 1 se destruye al empujarlo, 2 no se empuja (material inmovil o con TileEntity)
    int mobility(BlockType t) {
        switch (t) {
            case BlockType::BEDROCK: case BlockType::OBSIDIAN: case BlockType::PORTAL: case BlockType::MOB_SPAWNER:
            case BlockType::CHEST:
            case BlockType::PISTON_HEAD: case BlockType::PISTON_MOVING:
            case BlockType::SIGN_POST: case BlockType::SIGN_WALL:
                return 2;
            case BlockType::TALLGRASS: case BlockType::DEAD_BUSH: case BlockType::ROSE: case BlockType::DANDELION:
            case BlockType::SAPLING_OAK: case BlockType::SAPLING_SPRUCE: case BlockType::SAPLING_BIRCH:
            case BlockType::MUSHROOM_RED: case BlockType::MUSHROOM_BROWN: case BlockType::SUGAR_CANE:
            case BlockType::SNOW_LAYER: case BlockType::FIRE:
            case BlockType::LEVER: case BlockType::BUTTON: case BlockType::REDSTONE_WIRE:
                return 1;
            default:
                break;
        }
        if (isFurnaceBlock(t) || isDispenserBlock(t) || t == BlockType::NOTE_BLOCK || t == BlockType::JUKEBOX) return 2;   // bloques con tile entity: no se empujan
        if (isLiquidBlock(t) || isTorchBlock(t) || isRedstoneTorch(t) || isRepeaterBlock(t)) return 1;
        return 0;
    }

    bool dropsNothing(BlockType t) {
        return t == BlockType::TALLGRASS || t == BlockType::DEAD_BUSH || t == BlockType::SNOW_LAYER || t == BlockType::FIRE || isLiquidBlock(t);
    }

    // BlockPistonBase.canPushBlock
    bool canPush(BlockType t, int x, int y, int z, bool allowDestroy) {
        if (t == BlockType::OBSIDIAN) return false;
        if (isPistonBase(t)) {
            if (M(x, y, z) & 8) return false;   // un piston extendido no se mueve
        } else {
            if (blockHardness(t) < 0.0f) return false;
            const int mob = mobility(t);
            if (mob == 2) return false;
            if (!allowDestroy && mob == 1) return false;
        }
        return true;
    }

    // BlockPistonBase.func_31041_f: ¿le llega energia? (incluye la conexion "casi" de los pistones: tambien por la celda de arriba)
    bool shouldBePowered(int x, int y, int z, int d) {
        auto P = [&](int px, int py, int pz, int s) { return Redstone::indirectPowerAt(*W, px, py, pz, s); };
        return (d != 0 && P(x, y - 1, z, 0)) || (d != 1 && P(x, y + 1, z, 1)) || (d != 2 && P(x, y, z - 1, 2)) ||
               (d != 3 && P(x, y, z + 1, 3)) || (d != 5 && P(x + 1, y, z, 5)) || (d != 4 && P(x - 1, y, z, 4)) ||
               P(x, y, z, 0) || P(x, y + 2, z, 1) || P(x, y + 1, z - 1, 2) || P(x, y + 1, z + 1, 3) ||
               P(x - 1, y + 1, z, 4) || P(x + 1, y + 1, z, 5);
    }

    // BlockPistonBase.func_31045_h: ¿se puede extender? (hasta 12 bloques en fila y al final aire o algo que se destruye)
    bool canExtend(int x, int y, int z, int d) {
        int px = x + DX[d], py = y + DY[d], pz = z + DZ[d];
        for (int n = 0; n < 13; n++) {
            if (py <= 0 || py >= 127) return false;
            const BlockType t = T(px, py, pz);
            if (t != BlockType::AIR) {
                if (!canPush(t, px, py, pz, true)) return false;
                if (mobility(t) != 1) {
                    if (n == 12) return false;
                    px += DX[d]; py += DY[d]; pz += DZ[d];
                    continue;
                }
            }
            return true;
        }
        return true;
    }

    // BlockPistonBase.func_31047_i: extender (case 0 de playBlock)
    bool doExtend(int x, int y, int z, int d, bool sticky) {
        int px = x + DX[d], py = y + DY[d], pz = z + DZ[d];
        for (int n = 0; n < 13; n++) {
            if (py <= 0 || py >= 127) return false;
            const BlockType t = T(px, py, pz);
            if (t != BlockType::AIR) {
                if (!canPush(t, px, py, pz, true)) return false;
                if (mobility(t) != 1) {
                    if (n == 12) return false;
                    px += DX[d]; py += DY[d]; pz += DZ[d];
                    continue;
                }
                if (!dropsNothing(t)) dropItem(px, py, pz, getDropType(t));   // dropBlockAsItem del bloque que se destruye
                setNotify(px, py, pz, BlockType::AIR, 0);
            }
            break;
        }
        const BlockType self = sticky ? BlockType::PISTON_STICKY : BlockType::PISTON_BASE;
        while (px != x || py != y || pz != z) {
            const int qx = px - DX[d], qy = py - DY[d], qz = pz - DZ[d];
            const BlockType qt = T(qx, qy, qz);
            const int qm = M(qx, qy, qz);
            if (qt == self && qx == x && qy == y && qz == z) {
                const int hm = d | (sticky ? 8 : 0);
                setBare(px, py, pz, BlockType::PISTON_MOVING, hm);
                addMoving(px, py, pz, BlockType::PISTON_HEAD, hm, d, true, false);
            } else {
                setBare(px, py, pz, BlockType::PISTON_MOVING, qm);
                addMoving(px, py, pz, qt, qm, d, true, false);
            }
            px = qx; py = qy; pz = qz;
        }
        return true;
    }

    // BlockPistonBase.playBlock
    void playBlock(int x, int y, int z, int ev, int d) {
        const BlockType self = T(x, y, z);
        const bool sticky = (self == BlockType::PISTON_STICKY);
        g_busy = true;
        if (ev == 0) {
            if (doExtend(x, y, z, d, sticky)) setNotify(x, y, z, self, d | 8);
        } else if (ev == 1) {
            const int hx = x + DX[d], hy = y + DY[d], hz = z + DZ[d];
            Moving* he = findAt(hx, hy, hz);
            if (he) forceFinish(he);
            setBare(x, y, z, BlockType::PISTON_MOVING, d);
            addMoving(x, y, z, self, d, d, false, true);
            if (sticky) {
                const int qx = x + DX[d] * 2, qy = y + DY[d] * 2, qz = z + DZ[d] * 2;
                BlockType qt = T(qx, qy, qz);
                int qm = M(qx, qy, qz);
                bool pulled = false;
                if (qt == BlockType::PISTON_MOVING) {
                    Moving* qe = findAt(qx, qy, qz);
                    if (qe && qe->facing == d && qe->extending) {
                        const BlockType st = qe->type; const int sm = qe->meta;
                        forceFinish(qe);
                        qt = st; qm = sm;
                        pulled = true;
                    }
                }
                if (pulled || qt == BlockType::AIR || !canPush(qt, qx, qy, qz, false) || (mobility(qt) != 0 && !isPistonBase(qt))) {
                    if (!pulled) {
                        g_busy = false;
                        setNotify(hx, hy, hz, BlockType::AIR, 0);
                        g_busy = true;
                    }
                } else {
                    g_busy = false;
                    setNotify(qx, qy, qz, BlockType::AIR, 0);
                    g_busy = true;
                    setBare(hx, hy, hz, BlockType::PISTON_MOVING, qm);
                    addMoving(hx, hy, hz, qt, qm, d, false, false);
                }
            } else {
                g_busy = false;
                setNotify(hx, hy, hz, BlockType::AIR, 0);
                g_busy = true;
            }
        }
        g_busy = false;
    }

    // BlockPistonBase.func_31043_h
    void check(int x, int y, int z) {
        const int m = M(x, y, z);
        const int d = m & 7;
        if (d > 5) return;   // metadata 7 = recien puesto
        const bool powered = shouldBePowered(x, y, z, d);
        if (powered && !(m & 8)) {
            if (canExtend(x, y, z, d)) {
                setM(x, y, z, d | 8);
                playBlock(x, y, z, 0, d);
            }
        } else if (!powered && (m & 8)) {
            setM(x, y, z, d);
            playBlock(x, y, z, 1, d);
        }
    }

    // BlockPistonBase.func_31039_c: hacia donde mira el piston segun donde esta el jugador
    int placeFacing(int gx, int gy, int gz, float yawDeg, float eyeX, float eyeY, float eyeZ) {
        const float cellX = (float)gx - SIZE_X / 2.0f, cellY = (float)gy - 1.5f, cellZ = (float)gz - SIZE_Z / 2.0f;   // esquina minima
        if (fabsf(eyeX - cellX) < 2.0f && fabsf(eyeZ - cellZ) < 2.0f) {
            const float v = eyeY + 0.2f;   // posY + 1.82 - yOffset de Beta
            if (v - cellY > 2.0f) return 1;
            if (cellY - v > 0.0f) return 0;
        }
        const float yawB = 180.0f - yawDeg;   // el giro de Beta
        const int q = (int)floorf(yawB * 4.0f / 360.0f + 0.5f) & 3;
        return q == 0 ? 2 : (q == 1 ? 5 : (q == 2 ? 3 : 4));
    }
}

namespace Pistons {

void reset() {
    memset(g_m, 0, sizeof(g_m));
    g_busy = false;
}

void tick(const ChunkManager& cm) {
    W = &cm;
    const int n = MAX_MOVING;
    for (int i = 0; i < n; i++) {
        if (!g_m[i].active) continue;
        Moving& e = g_m[i];
        if (T(e.x, e.y, e.z) != BlockType::PISTON_MOVING) { e.active = false; continue; }   // otro bloque lo reemplazo
        e.last = e.progress;
        if (e.last >= 1.0f) {   // TileEntityPiston.updateEntity: ya llego, se pone el bloque
            const int x = e.x, y = e.y, z = e.z;
            const BlockType t = e.type; const int m = e.meta;
            e.active = false;
            setNotify(x, y, z, t, m);
        } else {
            e.progress += 0.5f;
            if (e.progress >= 1.0f) e.progress = 1.0f;
        }
    }
}

void bounds(BlockType t, int meta, float b[6]) {
    b[0] = 0.0f; b[1] = 0.0f; b[2] = 0.0f; b[3] = 1.0f; b[4] = 1.0f; b[5] = 1.0f;
    const int d = meta & 7;
    if (isPistonBase(t)) {   // BlockPistonBase.setBlockBoundsBasedOnState
        if (!(meta & 8)) return;
        switch (d) {
            case 0: b[1] = 0.25f; break;
            case 1: b[4] = 0.75f; break;
            case 2: b[2] = 0.25f; break;
            case 3: b[5] = 0.75f; break;
            case 4: b[0] = 0.25f; break;
            case 5: b[3] = 0.75f; break;
            default: break;
        }
    } else if (t == BlockType::PISTON_HEAD) {   // BlockPistonExtension.setBlockBoundsBasedOnState: la placa
        switch (d) {
            case 0: b[4] = 0.25f; break;
            case 1: b[1] = 0.75f; break;
            case 2: b[5] = 0.25f; break;
            case 3: b[2] = 0.75f; break;
            case 4: b[3] = 0.25f; break;
            case 5: b[0] = 0.75f; break;
            default: break;
        }
    } else {   // PISTON_MOVING: no se puede apuntar
        b[0] = b[1] = b[2] = 0.5f; b[3] = b[4] = b[5] = 0.5f;
    }
}

void baseNeighborChanged(const ChunkManager& cm, int x, int y, int z) {
    W = &cm;
    if (!g_busy) check(x, y, z);
}

void baseAdded(const ChunkManager& cm, int x, int y, int z) {
    W = &cm;
    if (g_busy) return;
    if (findAt(x, y, z)) return;   // sigue en movimiento
    check(x, y, z);
}

void headNeighborChanged(const ChunkManager& cm, int x, int y, int z) {
    W = &cm;
    const int d = M(x, y, z) & 7;
    if (d > 5) return;
    const int bx = x - DX[d], by = y - DY[d], bz = z - DZ[d];
    const BlockType bt = T(bx, by, bz);
    if (!isPistonBase(bt)) setNotify(x, y, z, BlockType::AIR, 0);
    else baseNeighborChanged(cm, bx, by, bz);
}

void headRemoved(const ChunkManager& cm, int x, int y, int z, int meta) {
    W = &cm;
    const int d = meta & 7;
    if (d > 5) return;
    const int o = OPP[d];
    const int bx = x + DX[o], by = y + DY[o], bz = z + DZ[o];
    const BlockType bt = T(bx, by, bz);
    if (isPistonBase(bt) && (M(bx, by, bz) & 8)) {
        dropItem(bx, by, bz, bt);
        setNotify(bx, by, bz, BlockType::AIR, 0);
    }
}

void cleanupOrphan(const ChunkManager& cm, int x, int y, int z) {
    W = &cm;
    if (T(x, y, z) != BlockType::PISTON_MOVING || findAt(x, y, z)) return;
    setNotify(x, y, z, BlockType::AIR, 0);
}

bool useItem(const ChunkManager& cm, BlockType item, int gx, int gy, int gz, BlockFace face, float yawDeg,
             float eyeX, float eyeY, float eyeZ) {
    W = &cm;
    if (!isPistonBase(item)) return false;
    int tx = gx, ty = gy, tz = gz;
    if (T(gx, gy, gz) != BlockType::SNOW_LAYER) {
        switch (face) {
            case BlockFace::BOTTOM: ty--; break;
            case BlockFace::TOP:    ty++; break;
            case BlockFace::NORTH:  tz--; break;
            case BlockFace::SOUTH:  tz++; break;
            case BlockFace::WEST:   tx--; break;
            case BlockFace::EAST:   tx++; break;
            default: return false;
        }
    }
    if (ty < 0 || ty >= SIZE_Y) return false;
    const BlockType here = T(tx, ty, tz);
    const bool free = here == BlockType::AIR || here == BlockType::TALLGRASS || here == BlockType::DEAD_BUSH || here == BlockType::SNOW_LAYER ||
                      here == BlockType::FIRE || isLiquidBlock(here);
    if (!free) return false;
    const int d = placeFacing(tx, ty, tz, yawDeg, eyeX, eyeY, eyeZ);
    setNotify(tx, ty, tz, item, d);   // el aviso de la celda nueva revisa si le llega energia (onBlockAdded)
    return true;
}

}

// ---------------------------------------------------------------------------------------------------------
// Geometria (la misma para el mapa y para los bloques en movimiento)
// ---------------------------------------------------------------------------------------------------------
namespace {
    using Pistons::QuadFn;

    // Vectores de las caras (como los de Chunk::emitPartialBox): u crece hacia U, v crece hacia V; la "arriba" de la textura es -V
    const int FACE_U[6][3] = { {1, 0, 0}, {1, 0, 0}, {-1, 0, 0}, {1, 0, 0}, {0, 0, 1}, {0, 0, -1} };
    const int FACE_V[6][3] = { {0, 0, -1}, {0, 0, 1}, {0, -1, 0}, {0, -1, 0}, {0, -1, 0}, {0, -1, 0} };

    void cross3(const int a[3], const int b[3], int o[3]) {
        o[0] = a[1] * b[2] - a[2] * b[1];
        o[1] = a[2] * b[0] - a[0] * b[2];
        o[2] = a[0] * b[1] - a[1] * b[0];
    }

    // Coordenadas de textura (0 a 1) de un punto p (0 a 1 dentro de la celda) de la cara f, con la "arriba" de la textura hacia la direccion d (o sin girar si d < 0
    // o si la cara es perpendicular a d). Equivale a los field_31087_g... de RenderBlocks, que giran las texturas del piston
    void faceUV(int f, int d, const float p[3], float& s, float& t) {
        float u, v;
        switch (f) {
            case 0: u = p[0];        v = 1.0f - p[2]; break;
            case 1: u = p[0];        v = p[2];        break;
            case 2: u = 1.0f - p[0]; v = 1.0f - p[1]; break;
            case 3: u = p[0];        v = 1.0f - p[1]; break;
            case 4: u = p[2];        v = 1.0f - p[1]; break;
            default: u = 1.0f - p[2]; v = 1.0f - p[1]; break;
        }
        s = u; t = v;
        if (d < 0) return;
        // eje de la cara f: 0,1 -> Y (1); 2,3 -> Z (2); 4,5 -> X (0)
        const int faceAxis = (f < 2) ? 1 : (f < 4 ? 2 : 0);
        const int dAxis = (d < 2) ? 1 : (d < 4 ? 2 : 0);
        if (faceAxis == dAxis) return;
        const int pAxis = 3 - faceAxis - dAxis;
        const bool dPos = (d & 1) != 0;   // 1, 3 y 5 son hacia +
        const float a = dPos ? p[dAxis] : 1.0f - p[dAxis];   // 0 atras, 1 adelante
        t = 1.0f - a;
        // el otro eje, con el sentido que conserve el giro de la cara (sin espejo)
        int Tv[3] = { 0, 0, 0 };
        Tv[dAxis] = dPos ? -1 : 1;                           // hacia donde crece t
        int E[3] = { 0, 0, 0 }; E[pAxis] = 1;
        int cuv[3], cst[3];
        cross3(FACE_U[f], FACE_V[f], cuv);
        cross3(E, Tv, cst);
        const int dot = cuv[0] * cst[0] + cuv[1] * cst[1] + cuv[2] * cst[2];
        s = (dot >= 0) ? p[pAxis] : 1.0f - p[pAxis];
    }

    // Una caja con una textura (indice de terrain.png) por cara. Mismo orden de vertices que Chunk::emitPartialBox
    void box(QuadFn fn, void* ctx, const float bd[6], const int tex[6], int d, float ox, float oy, float oz) {
        const float x0 = bd[0], y0 = bd[1], z0 = bd[2], x1 = bd[3], y1 = bd[4], z1 = bd[5];
        // vertices de cada cara (x, y, z) en el orden de emitPartialBox
        const float V[6][4][3] = {
            { {x0, y0, z0}, {x1, y0, z0}, {x1, y0, z1}, {x0, y0, z1} },   // abajo
            { {x0, y1, z1}, {x1, y1, z1}, {x1, y1, z0}, {x0, y1, z0} },   // arriba
            { {x1, y0, z0}, {x0, y0, z0}, {x0, y1, z0}, {x1, y1, z0} },   // -Z
            { {x0, y0, z1}, {x1, y0, z1}, {x1, y1, z1}, {x0, y1, z1} },   // +Z
            { {x0, y0, z0}, {x0, y0, z1}, {x0, y1, z1}, {x0, y1, z0} },   // -X
            { {x1, y0, z1}, {x1, y0, z0}, {x1, y1, z0}, {x1, y1, z1} },   // +X
        };
        for (int f = 0; f < 6; f++) {
            const int idx = tex[f];
            if (idx < 0) continue;
            const float cu = (float)((idx & 15) * 16), cv = (float)((idx >> 4) * 16);
            float p[12], uv[8];
            for (int k = 0; k < 4; k++) {
                float s, t;
                faceUV(f, d, V[f][k], s, t);
                p[k * 3] = V[f][k][0] + ox; p[k * 3 + 1] = V[f][k][1] + oy; p[k * 3 + 2] = V[f][k][2] + oz;
                uv[k * 2] = cu + s * 16.0f; uv[k * 2 + 1] = cv + t * 16.0f;
            }
            fn(ctx, p, uv);
        }
    }

    // Una cara de la franja del brazo: dos bordes largos, de la base (u = 0) a la placa (u = largo)
    void shaft(QuadFn fn, void* ctx, int d, float len, int tex, float ox, float oy, float oz) {
        const int axis = (d < 2) ? 1 : (d < 4 ? 2 : 0);
        const bool pos = (d & 1) != 0;
        const float plate = pos ? 0.75f : 0.25f;                 // donde empieza la placa (cara de adentro)
        const float baseEnd = pos ? plate - len : plate + len;   // el otro extremo del brazo
        const float lo = 6.0f / 16.0f, hi = 10.0f / 16.0f;
        const float C[4][2] = { {lo, lo}, {lo, hi}, {hi, hi}, {hi, lo} };
        const int a1 = (axis == 0) ? 1 : 0;                      // los dos ejes de la seccion
        const int a2 = (axis == 2) ? 1 : 2;
        const float cu = (float)((tex & 15) * 16), cv = (float)((tex >> 4) * 16);
        for (int i = 0; i < 4; i++) {
            const float* c0 = C[i]; const float* c1 = C[(i + 1) & 3];
            float pt[4][3];
            // vertices: c0/base, c0/placa, c1/placa, c1/base
            const float along[4] = { baseEnd, plate, plate, baseEnd };
            const float* cc[4] = { c0, c0, c1, c1 };
            for (int k = 0; k < 4; k++) {
                pt[k][axis] = along[k];
                pt[k][a1] = cc[k][0];
                pt[k][a2] = cc[k][1];
            }
            const float uLen = len * 16.0f;
            const float U[4] = { 0.0f, uLen, uLen, 0.0f };
            const float Vv[4] = { 0.0f, 0.0f, 3.99f, 3.99f };
            for (int pass = 0; pass < 2; pass++) {   // las dos orientaciones, para que se vea de cualquier lado
                float p[12], uv[8];
                for (int k = 0; k < 4; k++) {
                    const int kk = pass == 0 ? k : 3 - k;
                    p[k * 3] = pt[kk][0] + ox; p[k * 3 + 1] = pt[kk][1] + oy; p[k * 3 + 2] = pt[kk][2] + oz;
                    uv[k * 2] = cu + U[kk]; uv[k * 2 + 1] = cv + Vv[kk];
                }
                fn(ctx, p, uv);
            }
        }
    }
}

namespace Pistons {

void emitBase(QuadFn fn, void* ctx, BlockType t, int meta, bool forceExtended, float ox, float oy, float oz) {
    const int d = meta & 7;
    const bool ext = forceExtended || (meta & 8) != 0;
    float bd[6];
    bounds(t, ext ? (meta | 8) : (meta & 7), bd);
    int tex[6];
    for (int f = 0; f < 6; f++) {
        if (f == d) tex[f] = ext ? 110 : (t == BlockType::PISTON_STICKY ? 106 : 107);
        else if (f == OPP[d < 6 ? d : 0]) tex[f] = 109;
        else tex[f] = 108;
    }
    box(fn, ctx, bd, tex, d < 6 ? d : -1, ox, oy, oz);
}

void emitHead(QuadFn fn, void* ctx, int meta, bool longArm, int frontTexOverride, float ox, float oy, float oz) {
    const int d = meta & 7;
    if (d > 5) return;
    const bool sticky = (meta & 8) != 0;
    float bd[6];
    bounds(BlockType::PISTON_HEAD, meta, bd);
    int tex[6];
    for (int f = 0; f < 6; f++) {
        if (f == d) tex[f] = frontTexOverride >= 0 ? frontTexOverride : (sticky ? 106 : 107);
        else if (f == OPP[d]) tex[f] = 107;
        else tex[f] = 108;
    }
    box(fn, ctx, bd, tex, d, ox, oy, oz);
    shaft(fn, ctx, d, longArm ? 1.0f : 0.5f, 108, ox, oy, oz);
}

void emitBox(QuadFn fn, void* ctx, BlockType t, int meta, float ox, float oy, float oz) {
    TextureCell top, side, bottom;
    getBlockTextures(t, top, side, bottom);
    float bd[6];
    if (isPartialBlock(t)) Partial::bounds(t, meta, bd);
    else { bd[0] = bd[1] = bd[2] = 0.0f; bd[3] = bd[4] = bd[5] = 1.0f; }
    const int tb = bottom.row * 16 + bottom.col, tt = top.row * 16 + top.col, ts = side.row * 16 + side.col;
    const int tex[6] = { tb, tt, ts, ts, ts, ts };
    box(fn, ctx, bd, tex, -1, ox, oy, oz);
}

// ---------------------------------------------------------------------------------------------------------
// Bloques en movimiento: se dibujan cada cuadro (TileEntityRendererPiston)
// ---------------------------------------------------------------------------------------------------------
namespace {
    struct DrawCtx { float x, y, z; };   // esquina minima de la celda en el mundo

    void quadImmediate(void* c, const float p[12], const float uv[8]) {
        const DrawCtx* k = (const DrawCtx*)c;
        for (int i = 0; i < 4; i++) {
            GFX_TEX_COORD = TEXTURE_PACK(floattot16(uv[i * 2]), floattot16(uv[i * 2 + 1]));
            glVertex3v16(floattov16(k->x + p[i * 3]), floattov16(k->y + p[i * 3 + 1]), floattov16(k->z + p[i * 3 + 2]));
        }
    }
}

void draw(float playerX, float playerY, float playerZ, float partialTick) {
    const float maxDist = WorldConfig::renderDistance();
    for (int i = 0; i < MAX_MOVING; i++) {
        const Moving& e = g_m[i];
        if (!e.active) continue;
        float cx = (float)e.x - SIZE_X / 2.0f, cy = (float)e.y - 1.5f, cz = (float)e.z - SIZE_Z / 2.0f;
        const float dx = cx + 0.5f - playerX, dy = cy + 0.5f - playerY, dz = cz + 0.5f - playerZ;
        if (dx * dx + dy * dy + dz * dz > maxDist * maxDist) continue;

        float p = e.last + (e.progress - e.last) * partialTick;   // TileEntityPiston.func_31008_a
        if (p > 1.0f) p = 1.0f;
        if (p >= 1.0f) continue;   // ya llego: el mapa lo dibuja
        const float off = e.extending ? (p - 1.0f) : (1.0f - p);
        const int d = e.facing > 5 ? 0 : e.facing;
        const float ox = off * (float)DX[d], oy = off * (float)DY[d], oz = off * (float)DZ[d];

        DrawCtx ctx; ctx.x = cx; ctx.y = cy; ctx.z = cz;
        glBindTexture(0, Texture::textureID);
        glColor3b(255, 255, 255);
        glBegin(GL_QUADS);
        if (e.type == BlockType::PISTON_HEAD) {
            if (p < 0.5f) emitHead(quadImmediate, &ctx, e.meta, false, -1, ox, oy, oz);   // brazo corto al principio
            else          emitHead(quadImmediate, &ctx, e.meta, true, -1, ox, oy, oz);
        } else if (e.source && !e.extending && isPistonBase(e.type)) {
            // el piston que se repliega: la cabeza vuelve hacia la base, que se queda quieta
            const int hm = d | (e.type == BlockType::PISTON_STICKY ? 8 : 0);
            emitHead(quadImmediate, &ctx, hm, p < 0.5f, e.type == BlockType::PISTON_STICKY ? 106 : 107, ox, oy, oz);
            emitBase(quadImmediate, &ctx, e.type, e.meta, true, 0.0f, 0.0f, 0.0f);
        } else if (isPistonBase(e.type)) {
            emitBase(quadImmediate, &ctx, e.type, e.meta, false, ox, oy, oz);
        } else {
            emitBox(quadImmediate, &ctx, e.type, e.meta, ox, oy, oz);
        }
        glEnd();
    }
}

}
