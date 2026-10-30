#include "Trees.h"
#include "ChunkManager.h"
#include "Chunk.h"
#include "WorldConfig.h"
#include "PendingDrops.h"
#include <math.h>
#include <stdlib.h>

namespace {
    const int SY = WorldConfig::SIZE_Y;   // 128, como el original

    struct Rng {
        unsigned s;
        explicit Rng(unsigned seed) : s(seed ? seed : 1u) {}
        unsigned step() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
        int nextInt(int n) { return (int)(step() % (unsigned)n); }
        float nextFloat() { return (float)(step() & 0xFFFFFFu) / 16777216.0f; }
    };

    struct W {
        const ChunkManager& cm;
        explicit W(const ChunkManager& c) : cm(c) {}
        Chunk* chunk(int gx, int gz, int& xi, int& zi) const {
            Chunk* c = cm.getChunkByChunkCoord(gx >> 3, gz >> 3);
            if (!c || !c->isGenerationComplete()) return nullptr;
            xi = gx & 7; zi = gz & 7;
            return c;
        }
        BlockType get(int gx, int gy, int gz) const {   // lo que no esta cargado cuenta como piedra: ahi no crece nada
            if (gy < 0) return BlockType::STONE;
            if (gy >= SY) return BlockType::AIR;
            int xi, zi; Chunk* c = chunk(gx, gz, xi, zi);
            return c ? c->getBlockType(xi, gy, zi) : BlockType::STONE;
        }
        void set(int gx, int gy, int gz, BlockType t) const {
            if (gy < 0 || gy >= SY) return;
            int xi, zi; Chunk* c = chunk(gx, gz, xi, zi);
            if (c) c->setBlockDirect(xi, gy, zi, t, &cm);
        }
    };

    bool isLog(BlockType t) { return t == BlockType::OAK_LOG || t == BlockType::BIRCH_LOG || t == BlockType::SPRUCE_LOG; }
    bool freeOrLeaves(BlockType t) { return t == BlockType::AIR || isLeavesBlock(t); }   // el hueco donde cabe el arbol

    // WorldGenTrees (roble) y WorldGenForest (abedul): mismo molde, cambian la altura base y los bloques
    bool genRound(const W& w, Rng& r, int x, int y, int z, int heightBase, BlockType leaf, BlockType log) {
        const int h = r.nextInt(3) + heightBase;
        if (!(y >= 1 && y + h + 1 <= 128)) return false;
        bool ok = true;
        for (int yy = y; yy <= y + 1 + h; ++yy) {
            int rad = 1;
            if (yy == y) rad = 0;
            if (yy >= y + 1 + h - 2) rad = 2;
            for (int xx = x - rad; xx <= x + rad && ok; ++xx)
                for (int zz = z - rad; zz <= z + rad && ok; ++zz) {
                    if (yy >= 0 && yy < 128) { if (!freeOrLeaves(w.get(xx, yy, zz))) ok = false; }
                    else ok = false;
                }
        }
        if (!ok) return false;
        const BlockType below = w.get(x, y - 1, z);
        if (!((below == BlockType::GRASS || below == BlockType::DIRT) && y < 128 - h - 1)) return false;
        w.set(x, y - 1, z, BlockType::DIRT);
        for (int yy = y - 3 + h; yy <= y + h; ++yy) {
            const int dy = yy - (y + h);
            const int rad = 1 - dy / 2;
            for (int xx = x - rad; xx <= x + rad; ++xx) {
                const int dx = xx - x;
                for (int zz = z - rad; zz <= z + rad; ++zz) {
                    const int dz = zz - z;
                    if ((abs(dx) != rad || abs(dz) != rad || (r.nextInt(2) != 0 && dy != 0)) && !isLightOpaque(w.get(xx, yy, zz)))
                        w.set(xx, yy, zz, leaf);
                }
            }
        }
        for (int k = 0; k < h; ++k) {
            const BlockType t = w.get(x, y + k, z);
            if (freeOrLeaves(t)) w.set(x, y + k, z, log);
        }
        return true;
    }

    // WorldGenTaiga2 (el abeto del retoño)
    bool genSpruce(const W& w, Rng& r, int x, int y, int z) {
        const int h = r.nextInt(4) + 6;
        const int a = 1 + r.nextInt(2);
        const int b = h - a;
        const int c = 2 + r.nextInt(2);
        if (!(y >= 1 && y + h + 1 <= 128)) return false;
        bool ok = true;
        for (int yy = y; yy <= y + 1 + h && ok; ++yy) {
            const int rad = (yy - y < a) ? 0 : c;
            for (int xx = x - rad; xx <= x + rad && ok; ++xx)
                for (int zz = z - rad; zz <= z + rad && ok; ++zz) {
                    if (yy >= 0 && yy < 128) { if (!freeOrLeaves(w.get(xx, yy, zz))) ok = false; }
                    else ok = false;
                }
        }
        if (!ok) return false;
        const BlockType below = w.get(x, y - 1, z);
        if (!((below == BlockType::GRASS || below == BlockType::DIRT) && y < 128 - h - 1)) return false;
        w.set(x, y - 1, z, BlockType::DIRT);
        int rad = r.nextInt(2), k1 = 1, k2 = 0;
        for (int i = 0; i <= b; ++i) {
            const int yy = y + h - i;
            for (int xx = x - rad; xx <= x + rad; ++xx) {
                const int dx = xx - x;
                for (int zz = z - rad; zz <= z + rad; ++zz) {
                    const int dz = zz - z;
                    if ((abs(dx) != rad || abs(dz) != rad || rad <= 0) && !isLightOpaque(w.get(xx, yy, zz)))
                        w.set(xx, yy, zz, BlockType::SPRUCE_LEAVES);
                }
            }
            if (rad >= k1) { rad = k2; k2 = 1; ++k1; if (k1 > c) k1 = c; }
            else ++rad;
        }
        const int trim = r.nextInt(3);
        for (int k = 0; k < h - trim; ++k) {
            const BlockType t = w.get(x, y + k, z);
            if (freeOrLeaves(t)) w.set(x, y + k, z, BlockType::SPRUCE_LOG);
        }
        return true;
    }

    // WorldGenBigTree (el roble grande: 1 de cada 10)
    struct BigTree {
        static const int AX[6];
        const W& w;
        Rng rnd;
        int basePos[3];
        int heightLimit = 0, height = 0;
        double heightAttenuation = 0.618, branchSlope = 0.381, scaleWidth = 1.0, leafDensity = 1.0;
        int heightLimitLimit = 12, leafDistanceLimit = 4;
        int (*nodes)[4] = nullptr;
        int nodeCount = 0;

        BigTree(const W& world, unsigned seed) : w(world), rnd(seed) {}
        ~BigTree() { delete[] nodes; }

        bool freeCell(BlockType t) const { return freeOrLeaves(t); }

        // func_528_a: tamaño de la capa de hojas segun la altura
        float layerSize(int layer) const {
            if ((double)layer < (double)((float)heightLimit) * 0.3) return -1.618f;
            const float var2 = (float)heightLimit / 2.0f;
            const float var3 = (float)heightLimit / 2.0f - (float)layer;
            float var4;
            if (var3 == 0.0f) var4 = var2;
            else if (fabsf(var3) >= var2) var4 = 0.0f;
            else var4 = sqrtf(var2 * var2 - var3 * var3);
            return var4 * 0.5f;
        }
        float leafSize(int layer) const {
            return (layer >= 0 && layer < leafDistanceLimit) ? ((layer != 0 && layer != leafDistanceLimit - 1) ? 3.0f : 2.0f) : -1.0f;
        }

        // func_524_a: -1 si la linea esta libre; si no, a cuantos pasos se topa
        int checkLine(const int* from, const int* to) const {
            int d[3] = { 0, 0, 0 };
            int major = 0;
            for (int i = 0; i < 3; ++i) { d[i] = to[i] - from[i]; if (abs(d[i]) > abs(d[major])) major = i; }
            if (d[major] == 0) return -1;
            const int a1 = AX[major], a2 = AX[major + 3];
            const int sgn = d[major] > 0 ? 1 : -1;
            const double s1 = (double)d[a1] / (double)d[major], s2 = (double)d[a2] / (double)d[major];
            int pos[3];
            int step = 0;
            const int end = d[major] + sgn;
            for (; step != end; step += sgn) {
                pos[major] = from[major] + step;
                pos[a1] = (int)floor((double)from[a1] + (double)step * s1);
                pos[a2] = (int)floor((double)from[a2] + (double)step * s2);
                if (!freeCell(w.get(pos[0], pos[1], pos[2]))) break;
            }
            return step == end ? -1 : abs(step);
        }

        // func_522_a: pone una linea de bloques
        void placeLine(const int* from, const int* to, BlockType t) const {
            int d[3] = { 0, 0, 0 };
            int major = 0;
            for (int i = 0; i < 3; ++i) { d[i] = to[i] - from[i]; if (abs(d[i]) > abs(d[major])) major = i; }
            if (d[major] == 0) return;
            const int a1 = AX[major], a2 = AX[major + 3];
            const int sgn = d[major] > 0 ? 1 : -1;
            const double s1 = (double)d[a1] / (double)d[major], s2 = (double)d[a2] / (double)d[major];
            int pos[3];
            for (int step = 0, end = d[major] + sgn; step != end; step += sgn) {
                pos[major] = (int)floor((double)(from[major] + step) + 0.5);
                pos[a1] = (int)floor((double)from[a1] + (double)step * s1 + 0.5);
                pos[a2] = (int)floor((double)from[a2] + (double)step * s2 + 0.5);
                w.set(pos[0], pos[1], pos[2], t);
            }
        }

        // func_523_a: una capa circular de hojas
        void leafLayer(int x, int y, int z, float size) const {
            const int rad = (int)((double)size + 0.618);
            for (int a = -rad; a <= rad; ++a)
                for (int b = -rad; b <= rad; ++b) {
                    const double dist = sqrt(pow((double)abs(a) + 0.5, 2.0) + pow((double)abs(b) + 0.5, 2.0));
                    if (dist > (double)size) continue;
                    const int px = x + a, pz = z + b;
                    const BlockType t = w.get(px, y, pz);
                    if (t == BlockType::AIR || isLeavesBlock(t)) w.set(px, y, pz, BlockType::OAK_LEAVES);
                }
        }

        void makeNodes() {   // func_521_a
            height = (int)((double)heightLimit * heightAttenuation);
            if (height >= heightLimit) height = heightLimit - 1;
            int per = (int)(1.382 + pow(scaleWidth * (double)heightLimit / 13.0, 2.0));
            if (per < 1) per = 1;
            int (*tmp)[4] = new int[per * heightLimit][4];
            int y = basePos[1] + heightLimit - leafDistanceLimit;
            int n = 1;
            const int trunkTop = basePos[1] + height;
            int layer = y - basePos[1];
            tmp[0][0] = basePos[0]; tmp[0][1] = y; tmp[0][2] = basePos[2]; tmp[0][3] = trunkTop;
            --y;
            while (layer >= 0) {
                const float size = layerSize(layer);
                if (size < 0.0f) { --y; --layer; continue; }
                for (int k = 0; k < per; ++k) {
                    const double dist = leafDensity * (double)size * ((double)rnd.nextFloat() + 0.328);
                    const double ang = (double)rnd.nextFloat() * 2.0 * 3.14159;
                    const int px = (int)floor(dist * sin(ang) + (double)basePos[0] + 0.5);
                    const int pz = (int)floor(dist * cos(ang) + (double)basePos[2] + 0.5);
                    int top[3] = { px, y, pz };
                    int bot[3] = { px, y + leafDistanceLimit, pz };
                    if (checkLine(top, bot) != -1) continue;
                    int start[3] = { basePos[0], basePos[1], basePos[2] };
                    const double hd = sqrt(pow((double)abs(basePos[0] - top[0]), 2.0) + pow((double)abs(basePos[2] - top[2]), 2.0));
                    const double drop = hd * branchSlope;
                    if ((double)top[1] - drop > (double)trunkTop) start[1] = trunkTop;
                    else start[1] = (int)((double)top[1] - drop);
                    if (checkLine(start, top) == -1) {
                        tmp[n][0] = px; tmp[n][1] = y; tmp[n][2] = pz; tmp[n][3] = start[1];
                        ++n;
                    }
                }
                --y; --layer;
            }
            nodes = new int[n][4];
            nodeCount = n;
            for (int i = 0; i < n; ++i) for (int j = 0; j < 4; ++j) nodes[i][j] = tmp[i][j];
            delete[] tmp;
        }

        bool validLocation() {   // func_519_e
            int a[3] = { basePos[0], basePos[1], basePos[2] };
            int b[3] = { basePos[0], basePos[1] + heightLimit - 1, basePos[2] };
            const BlockType below = w.get(basePos[0], basePos[1] - 1, basePos[2]);
            if (below != BlockType::GRASS && below != BlockType::DIRT) return false;
            const int r = checkLine(a, b);
            if (r == -1) return true;
            if (r < 6) return false;
            heightLimit = r;
            return true;
        }

        bool generate(int x, int y, int z) {
            basePos[0] = x; basePos[1] = y; basePos[2] = z;
            if (heightLimit == 0) heightLimit = 5 + rnd.nextInt(heightLimitLimit);
            if (!validLocation()) return false;
            makeNodes();
            for (int i = 0; i < nodeCount; ++i) {               // func_518_b: las hojas de cada nodo
                for (int yy = nodes[i][1]; yy < nodes[i][1] + leafDistanceLimit; ++yy)
                    leafLayer(nodes[i][0], yy, nodes[i][2], leafSize(yy - nodes[i][1]));
            }
            { int a[3] = { basePos[0], basePos[1], basePos[2] }; int b[3] = { basePos[0], basePos[1] + height, basePos[2] }; placeLine(a, b, BlockType::OAK_LOG); }   // el tronco
            for (int i = 0; i < nodeCount; ++i) {               // func_525_d: ramas
                int from[3] = { basePos[0], nodes[i][3], basePos[2] };
                int to[3] = { nodes[i][0], nodes[i][1], nodes[i][2] };
                if ((double)(from[1] - basePos[1]) >= (double)heightLimit * 0.2) placeLine(from, to, BlockType::OAK_LOG);
            }
            return true;
        }
    };
    const int BigTree::AX[6] = { 2, 0, 0, 1, 2, 1 };

    // Pone o quita la marca de revision (meta 8) de una hoja sin volver a dibujar el chunk
    void markLeaf(const W& w, int gx, int gy, int gz) {
        int xi, zi; Chunk* c = w.chunk(gx, gz, xi, zi);
        if (!c || gy < 0 || gy >= SY) return;
        if (!isLeavesBlock(c->getBlockType(xi, gy, zi))) return;
        const int m = c->getLiquidMeta(xi, gy, zi);
        if (!(m & 8)) c->setMetaQuiet(xi, gy, zi, m | 8);
    }
}

BlockType Trees::saplingOfLeaves(BlockType leaves) {
    return leaves == BlockType::SPRUCE_LEAVES ? BlockType::SAPLING_SPRUCE : (leaves == BlockType::BIRCH_LEAVES ? BlockType::SAPLING_BIRCH : BlockType::SAPLING_OAK);
}

bool Trees::grow(const ChunkManager& cm, int gx, int gy, int gz) {
    W w(cm);
    int xi, zi; Chunk* c = w.chunk(gx, gz, xi, zi);
    if (!c || gy < 0 || gy >= SY) return false;
    const BlockType sap = c->getBlockType(xi, gy, zi);
    if (!isSapling(sap)) return false;
    Rng r((unsigned)rand() * 2654435761u + (unsigned)(gx * 73856093) + (unsigned)(gz * 19349663) + 12345u);
    c->setBlockDirect(xi, gy, zi, BlockType::AIR, &cm);   // BlockSapling.growTree: se quita antes de intentar
    bool ok;
    if (sap == BlockType::SAPLING_SPRUCE) ok = genSpruce(w, r, gx, gy, gz);
    else if (sap == BlockType::SAPLING_BIRCH) ok = genRound(w, r, gx, gy, gz, 5, BlockType::BIRCH_LEAVES, BlockType::BIRCH_LOG);
    else {
        if (r.nextInt(10) == 0) { BigTree big(w, r.step()); ok = big.generate(gx, gy, gz); }
        else ok = genRound(w, r, gx, gy, gz, 4, BlockType::OAK_LEAVES, BlockType::OAK_LOG);
    }
    if (!ok) c->setBlockWithMeta(xi, gy, zi, sap, 0, &cm);   // sin sitio: vuelve el retoño (etapa 0)
    return ok;
}

void Trees::saplingTick(const ChunkManager& cm, int gx, int gy, int gz) {
    W w(cm);
    int xi, zi; Chunk* c = w.chunk(gx, gz, xi, zi);
    if (!c || gy < 0 || gy >= SY - 1 || !isSapling(c->getBlockType(xi, gy, zi))) return;
    int sky = (int)c->getSkyLight(xi, gy + 1, zi) - cm.getSkylightSubtract();
    int lvl = sky > (int)c->getBlockLight(xi, gy + 1, zi) ? sky : (int)c->getBlockLight(xi, gy + 1, zi);
    if (lvl < 9 || rand() % 30 != 0) return;
    const int meta = c->getLiquidMeta(xi, gy, zi);
    if (!(meta & 8)) c->setMetaQuiet(xi, gy, zi, meta | 8);   // primera etapa
    else grow(cm, gx, gy, gz);
}

void Trees::leafTick(const ChunkManager& cm, int gx, int gy, int gz) {
    W w(cm);
    int xi, zi; Chunk* c = w.chunk(gx, gz, xi, zi);
    if (!c || gy < 0 || gy >= SY) return;
    const BlockType self = c->getBlockType(xi, gy, zi);
    if (!isLeavesBlock(self)) return;
    const int meta = c->getLiquidMeta(xi, gy, zi);
    if (!(meta & 8)) return;
    int tx, tz;
    if (!w.chunk(gx - 5, gz - 5, tx, tz) || !w.chunk(gx + 5, gz - 5, tx, tz) || !w.chunk(gx - 5, gz + 5, tx, tz) || !w.chunk(gx + 5, gz + 5, tx, tz)) return;   // vecinos sin cargar: se revisa despues

    // BlockLeaves.updateTick: distancia en pasos, por hojas, hasta un tronco (0); busca a 4 pasos como mucho
    static signed char d[9][9][9];
    for (int a = -4; a <= 4; a++)
        for (int b = -4; b <= 4; b++)
            for (int e = -4; e <= 4; e++) {
                const BlockType t = w.get(gx + a, gy + b, gz + e);
                d[a + 4][b + 4][e + 4] = isLog(t) ? 0 : (isLeavesBlock(t) ? -2 : -1);
            }
    for (int step = 1; step <= 4; step++)
        for (int a = 0; a < 9; a++)
            for (int b = 0; b < 9; b++)
                for (int e = 0; e < 9; e++) {
                    if (d[a][b][e] != step - 1) continue;
                    if (a > 0 && d[a - 1][b][e] == -2) d[a - 1][b][e] = (signed char)step;
                    if (a < 8 && d[a + 1][b][e] == -2) d[a + 1][b][e] = (signed char)step;
                    if (b > 0 && d[a][b - 1][e] == -2) d[a][b - 1][e] = (signed char)step;
                    if (b < 8 && d[a][b + 1][e] == -2) d[a][b + 1][e] = (signed char)step;
                    if (e > 0 && d[a][b][e - 1] == -2) d[a][b][e - 1] = (signed char)step;
                    if (e < 8 && d[a][b][e + 1] == -2) d[a][b][e + 1] = (signed char)step;
                }
    if (d[4][4][4] >= 0) {
        c->setMetaQuiet(xi, gy, zi, meta & ~8);   // sigue unida a un arbol
    } else {
        if (rand() % 20 == 0)   // BlockLeaves.quantityDropped: 1 de cada 20 suelta su retoño
            PendingDrops::push((float)gx - WorldConfig::SIZE_X / 2.0f + 0.5f, (float)gy - 1.0f, (float)gz - WorldConfig::SIZE_Z / 2.0f + 0.5f, saplingOfLeaves(self));
        c->breakBlockAt(xi, gy, zi, &cm);   // (marca a las vecinas, como removeLeaves)
    }
}

void Trees::onBlockRemoved(const ChunkManager& cm, int gx, int gy, int gz, BlockType old) {
    int rad;
    if (isLog(old)) rad = 4;
    else if (isLeavesBlock(old)) rad = 1;
    else return;
    W w(cm);
    for (int a = -rad; a <= rad; a++)
        for (int b = -rad; b <= rad; b++)
            for (int e = -rad; e <= rad; e++)
                if (a || b || e) markLeaf(w, gx + a, gy + b, gz + e);
}
