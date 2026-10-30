#include "Raycaster.h"
#include "ChunkManager.h"
#include "Chunk.h"
#include "Torch.h"
#include "PartialBlocks.h"
#include <math.h>

// Face codes internos, mismos que el algoritmo real de World.java:
// 0=BOTTOM, 1=TOP, 2=NORTH, 3=SOUTH, 4=WEST, 5=EAST
static BlockFace faceFromCode(int code) {
    switch (code) {
        case 0: return BlockFace::BOTTOM;
        case 1: return BlockFace::TOP;
        case 2: return BlockFace::NORTH;
        case 3: return BlockFace::SOUTH;
        case 4: return BlockFace::WEST;
        case 5: return BlockFace::EAST;
        default: return BlockFace::NONE;
    }
}

static const float Y_ALIGN_OFFSET = 1.5f; // compensa que Y no está alineada como X/Z

// La antorcha no ocupa la celda entera: el rayo tiene que cortar su caja (BlockTorch.collisionRayTrace).
// Si no la corta, el rayo sigue de largo hacia lo que haya detrás. La celda (cellX, cellY, cellZ) va de
// cellX a cellX+1 en X y Z, y de cellY-1.5 a cellY-0.5 en Y del mundo.
static bool torchBoxHit(BlockType t, int cellX, int cellY, int cellZ,
                        float ox, float oy, float oz, float ex, float ey, float ez,
                        BlockFace& face, float& hx, float& hy, float& hz) {
    float s[3] = { ox - (float)cellX, (oy + Y_ALIGN_OFFSET) - (float)cellY, oz - (float)cellZ };
    float e[3] = { ex - (float)cellX, (ey + Y_ALIGN_OFFSET) - (float)cellY, ez - (float)cellZ };
    Torch::RayHit r = Torch::rayVsBox(Torch::boundsForMeta(torchMeta(t)), s, e);
    if (!r.hit) return false;
    face = faceFromCode(r.face);
    hx = r.x + (float)cellX;
    hy = r.y + (float)cellY - Y_ALIGN_OFFSET;
    hz = r.z + (float)cellZ;
    return true;
}

// Puertas, trampilla y cama tampoco llenan la celda: el rayo tiene que cortar su caja o sigue de largo
static bool partialBoxHit(const ChunkManager& cm, BlockType t, Chunk* c, int xi, int yi, int zi, int cellX, int cellY, int cellZ,
                          float ox, float oy, float oz, float ex, float ey, float ez,
                          BlockFace& face, float& hx, float& hy, float& hz) {
    float bd[6];
    Partial::bounds(t, c->getLiquidMeta(xi, yi, zi), bd);
    float s[3] = { ox - (float)cellX, (oy + Y_ALIGN_OFFSET) - (float)cellY, oz - (float)cellZ };
    float e[3] = { ex - (float)cellX, (ey + Y_ALIGN_OFFSET) - (float)cellY, ez - (float)cellZ };
    float h[3];
    BlockFace f = BlockFace::NONE;
    if (!Partial::rayVsBox(bd, s, e, f, h)) return false;
    face = f;
    hx = h[0] + (float)cellX;
    hy = h[1] + (float)cellY - Y_ALIGN_OFFSET;
    hz = h[2] + (float)cellZ;
    (void)cm;
    return true;
}

// ¿la celda cuenta como objetivo del rayo? Los líquidos solo se detectan para el balde vacío, y solo las fuentes
// (BlockFluid.canCollideCheck: devuelve verdadero únicamente con el indicador del balde y metadata 0)
static bool cellPresent(const ChunkManager& cm, bool hitLiquidSources, float x, float y, float z) {
    if (cm.isBlockPresentAt(x, y, z)) return true;
    return hitLiquidSources && cm.isLiquidSourceAt(x, y, z);
}

RaycastHit Raycaster::cast(const ChunkManager& chunkManager,
                            float originX, float originY, float originZ,
                            float dirX, float dirY, float dirZ,
                            float maxDistance, bool hitLiquidSources) {
    RaycastHit result;
    result.hit = false;
    result.chunk = nullptr;
    result.face = BlockFace::NONE;

    float px = originX, py = originY, pz = originZ;

    float endX = originX + dirX * maxDistance;
    float endY = originY + dirY * maxDistance;
    float endZ = originZ + dirZ * maxDistance;


    int targetXi = (int)floorf(endX);
    int targetYi = (int)floorf(endY + Y_ALIGN_OFFSET);
    int targetZi = (int)floorf(endZ);

    int curXi = (int)floorf(px);
    int curYi = (int)floorf(py + Y_ALIGN_OFFSET);
    int curZi = (int)floorf(pz);

    if (cellPresent(chunkManager, hitLiquidSources, px, py, pz)) {
        Chunk* c = chunkManager.getChunkAt(px, pz);
        if (c) {
            int sxi = 0, syi = 0, szi = 0;
            c->worldToGrid(px, py, pz, sxi, syi, szi);
            BlockType st = c->getBlockType(sxi, syi, szi);
            BlockFace tf = BlockFace::NONE; float thx = px, thy = py, thz = pz;
            const bool okShape = (isPartialBlock(st) || st == BlockType::SNOW_LAYER)
                ? partialBoxHit(chunkManager, st, c, sxi, syi, szi, curXi, curYi, curZi, originX, originY, originZ, endX, endY, endZ, tf, thx, thy, thz)
                : (!isTorchBlock(st) || torchBoxHit(st, curXi, curYi, curZi, originX, originY, originZ, endX, endY, endZ, tf, thx, thy, thz));
            if (okShape) {
                result.hit = true;
                result.chunk = c;
                result.hitX = thx; result.hitY = thy; result.hitZ = thz;
                result.blockXi = sxi; result.blockYi = syi; result.blockZi = szi;
                result.face = tf;
                return result;
            }
        }
    }

    int maxIterations = 200;
    while (maxIterations-- >= 0) {
        if (curXi == targetXi && curYi == targetYi && curZi == targetZi) {
            return result;
        }

        bool activeX = true, activeY = true, activeZ = true;
        float boundX = 999.0f, boundY = 999.0f, boundZ = 999.0f;

        if (targetXi > curXi) boundX = (float)curXi + 1.0f;
        else if (targetXi < curXi) boundX = (float)curXi + 0.0f;
        else activeX = false;

        if (targetYi > curYi) boundY = (float)curYi + 1.0f;
        else if (targetYi < curYi) boundY = (float)curYi + 0.0f;
        else activeY = false;

        if (targetZi > curZi) boundZ = (float)curZi + 1.0f;
        else if (targetZi < curZi) boundZ = (float)curZi + 0.0f;
        else activeZ = false;

        float remainX = endX - px;
        float remainY = (endY + Y_ALIGN_OFFSET) - (py + Y_ALIGN_OFFSET);
        float remainZ = endZ - pz;

        float tX = 999.0f, tY = 999.0f, tZ = 999.0f;
        if (activeX) tX = (boundX - px) / remainX;
        if (activeY) tY = (boundY - (py + Y_ALIGN_OFFSET)) / remainY;
        if (activeZ) tZ = (boundZ - pz) / remainZ;

        int faceCode;

        if (tX < tY && tX < tZ) {
            faceCode = (targetXi > curXi) ? 4 : 5;
            px = boundX;
            py += remainY * tX;
            pz += remainZ * tX;
        } else if (tY < tZ) {
            faceCode = (targetYi > curYi) ? 0 : 1;
            px += remainX * tY;
            py = boundY - Y_ALIGN_OFFSET;
            pz += remainZ * tY;
        } else {
            faceCode = (targetZi > curZi) ? 2 : 3;
            px += remainX * tZ;
            py += remainY * tZ;
            pz = boundZ;
        }

        curXi = (int)floorf(px);
        if (faceCode == 5) curXi--;

        curYi = (int)floorf(py + Y_ALIGN_OFFSET);
        if (faceCode == 1) curYi--;

        curZi = (int)floorf(pz);
        if (faceCode == 3) curZi--;

        float testWorldX = (float)curXi + 0.5f;
        float testWorldY = (float)curYi - 1.0f; // usa la misma fórmula que gridToWorldY
        float testWorldZ = (float)curZi + 0.5f;

        if (cellPresent(chunkManager, hitLiquidSources, testWorldX, testWorldY, testWorldZ)) {
            Chunk* c = chunkManager.getChunkAt(testWorldX, testWorldZ);
            if (c) {
                int bxi = 0, byi = 0, bzi = 0;
                c->worldToGrid(testWorldX, testWorldY, testWorldZ, bxi, byi, bzi);
                BlockType bt = c->getBlockType(bxi, byi, bzi);
                BlockFace hitFace = faceFromCode(faceCode);
                float hhx = px, hhy = py, hhz = pz;
                // una antorcha solo cuenta si el rayo corta su caja; si no, se sigue de largo
                const bool okShape = (isPartialBlock(bt) || bt == BlockType::SNOW_LAYER)
                    ? partialBoxHit(chunkManager, bt, c, bxi, byi, bzi, curXi, curYi, curZi, originX, originY, originZ, endX, endY, endZ, hitFace, hhx, hhy, hhz)
                    : (!isTorchBlock(bt) || torchBoxHit(bt, curXi, curYi, curZi, originX, originY, originZ, endX, endY, endZ, hitFace, hhx, hhy, hhz));
                if (okShape) {
                    result.hit = true;
                    result.chunk = c;
                    result.hitX = hhx; result.hitY = hhy; result.hitZ = hhz;
                    result.blockXi = bxi; result.blockYi = byi; result.blockZi = bzi;
                    result.face = hitFace;
                    return result;
                }
            }
        }
    }

    return result;
}