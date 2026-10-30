#include "PaintingManager.h"
#include "Texture.h"
#include "Chunk.h"
#include "ChunkManager.h"
#include "WorldConfig.h"
#include "HandAnimations.h"
#include <nds.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace {
    // EnumArt: tamano y desplazamiento en kz.png (256 x 256) en pixeles
    struct Art { const char* title; int sx, sy, ox, oy; };
    const Art ARTS[25] = {
        {"Kebab", 16, 16, 0, 0},   {"Aztec", 16, 16, 16, 0},  {"Alban", 16, 16, 32, 0},  {"Aztec2", 16, 16, 48, 0}, {"Bomb", 16, 16, 64, 0},
        {"Plant", 16, 16, 80, 0},  {"Wasteland", 16, 16, 96, 0}, {"Pool", 32, 16, 0, 32}, {"Courbet", 32, 16, 32, 32}, {"Sea", 32, 16, 64, 32},
        {"Sunset", 32, 16, 96, 32}, {"Creebet", 32, 16, 128, 32}, {"Wanderer", 16, 32, 0, 64}, {"Graham", 16, 32, 16, 64}, {"Match", 32, 32, 0, 128},
        {"Bust", 32, 32, 32, 128}, {"Stage", 32, 32, 64, 128}, {"Void", 32, 32, 96, 128}, {"SkullAndRoses", 32, 32, 128, 128}, {"Fighters", 64, 32, 0, 96},
        {"Pointer", 64, 64, 0, 192}, {"Pigscene", 64, 64, 64, 192}, {"BurningSkull", 64, 64, 128, 192}, {"Skeleton", 64, 48, 192, 64}, {"DonkeyKong", 64, 48, 192, 112}
    };
    const int ART_COUNT = 25;

    struct Painting {
        bool active;
        int gx, gy, gz;          // bloque clicado (xPosition, yPosition, zPosition del original)
        int dir, art;
        int ticks;
        double px, py, pz;       // posicion del centro (coordenadas de la Beta)
        double minX, minY, minZ, maxX, maxY, maxZ;
    };
    Painting s_p[PaintingManager::MAX_PAINTINGS];
    PaintingManager::Drop s_drops[8];
    int s_dropCount = 0;
    bool s_unknown = false;   // la comprobacion toco un chunk que no esta cargado

    // mundo (render) = Beta - (4, 1,5, 4); la celda (gx, gy, gz) de la Beta es la misma que la global de este juego
    const double OX = 4.0, OY = 1.5, OZ = 4.0;

    BlockType typeAt(const ChunkManager& cm, int gx, int gy, int gz) {
        if (gy < 0 || gy >= WorldConfig::SIZE_Y) return BlockType::AIR;
        Chunk* c = cm.getChunkByChunkCoord(gx >> 3, gz >> 3);
        if (!c || !c->isGenerationComplete()) { s_unknown = true; return BlockType::AIR; }
        return c->getBlockType(gx & 7, gy, gz & 7);
    }

    float halfSnap(int size) { return (size == 32 || size == 64) ? 0.5f : 0.0f; }   // EntityPainting.func_411_c

    // EntityPainting.func_412_b: posicion y caja segun la direccion y el motivo
    void compute(Painting& p) {
        const Art& a = ARTS[p.art];
        float sx = (float)a.sx, sy = (float)a.sy, sz = (float)a.sx;
        if (p.dir != 0 && p.dir != 2) sx = 0.5f; else sz = 0.5f;
        sx /= 32.0f; sy /= 32.0f; sz /= 32.0f;
        float x = (float)p.gx + 0.5f, y = (float)p.gy + 0.5f, z = (float)p.gz + 0.5f;
        const float off = 9.0f / 16.0f;
        if (p.dir == 0) z -= off;
        if (p.dir == 1) x -= off;
        if (p.dir == 2) z += off;
        if (p.dir == 3) x += off;
        if (p.dir == 0) x -= halfSnap(a.sx);
        if (p.dir == 1) z += halfSnap(a.sx);
        if (p.dir == 2) x += halfSnap(a.sx);
        if (p.dir == 3) z -= halfSnap(a.sx);
        y += halfSnap(a.sy);
        const float g = -(0.1f / 16.0f);
        p.px = x; p.py = y; p.pz = z;
        p.minX = x - sx - g; p.minY = y - sy - g; p.minZ = z - sz - g;
        p.maxX = x + sx + g; p.maxY = y + sy + g; p.maxZ = z + sz + g;
    }

    // EntityPainting.func_410_i: nada solido delante, la pared detras entera y ningun otro cuadro encima
    bool isValid(const ChunkManager& cm, const Painting& p, int self) {
        s_unknown = false;
        bool ok = true;
        const int i0 = (int)floor(p.minX), i1 = (int)floor(p.maxX), j0 = (int)floor(p.minY), j1 = (int)floor(p.maxY), k0 = (int)floor(p.minZ), k1 = (int)floor(p.maxZ);
        for (int i = i0; i <= i1 && ok; i++) for (int j = j0; j <= j1 && ok; j++) for (int k = k0; k <= k1 && ok; k++)
            if (isMaterialSolid(typeAt(cm, i, j, k))) ok = false;
        if (ok) {
            const Art& a = ARTS[p.art];
            const int w = a.sx / 16, h = a.sy / 16;
            int x0 = p.gx, z0 = p.gz;
            if (p.dir == 0 || p.dir == 2) x0 = (int)floor(p.px - (double)((float)a.sx / 32.0f));
            else z0 = (int)floor(p.pz - (double)((float)a.sx / 32.0f));
            const int y0 = (int)floor(p.py - (double)((float)a.sy / 32.0f));
            for (int u = 0; u < w && ok; u++) for (int v = 0; v < h && ok; v++) {
                const BlockType t = (p.dir == 0 || p.dir == 2) ? typeAt(cm, x0 + u, y0 + v, p.gz) : typeAt(cm, p.gx, y0 + v, z0 + u);
                if (!isMaterialSolid(t)) ok = false;
            }
        }
        if (ok) {
            for (int i = 0; i < PaintingManager::MAX_PAINTINGS; i++) {
                const Painting& o = s_p[i];
                if (!o.active || i == self) continue;
                if (p.minX < o.maxX && p.maxX > o.minX && p.minY < o.maxY && p.maxY > o.minY && p.minZ < o.maxZ && p.maxZ > o.minZ) { ok = false; break; }
            }
        }
        return s_unknown ? true : ok;
    }

    void pushDrop(const Painting& p) {
        if (s_dropCount >= 8) return;
        PaintingManager::Drop& d = s_drops[s_dropCount++];
        d.type = BlockType::PAINTING; d.count = 1; d.durability = -1;
        d.x = (float)(p.px - OX); d.y = (float)(p.py - OY); d.z = (float)(p.pz - OZ);
    }

    float brightnessOf(unsigned char level) {
        float f = 1.0f - (float)level / 15.0f;
        return (1.0f - f) / (f * 3.0f + 1.0f) * 0.95f + 0.05f;
    }

    inline void vtx(int u, int v, int x, int y, int z) {
        GFX_TEX_COORD = TEXTURE_PACK((s16)u, (s16)v);
        glVertex3v16((s16)x, (s16)y, (s16)z);
    }
}

void PaintingManager::clear() {
    memset(s_p, 0, sizeof(s_p));
    s_dropCount = 0;
}

bool PaintingManager::place(const ChunkManager& cm, int gx, int gy, int gz, int dir) {
    if (dir < 0 || dir > 3) return false;
    int slot = -1;
    for (int i = 0; i < MAX_PAINTINGS; i++) if (!s_p[i].active) { slot = i; break; }
    if (slot < 0) return false;
    Painting c;
    memset(&c, 0, sizeof(c));
    c.gx = gx; c.gy = gy; c.gz = gz; c.dir = dir;
    int ok[ART_COUNT], n = 0;
    for (int a = 0; a < ART_COUNT; a++) {
        c.art = a;
        compute(c);
        if (isValid(cm, c, -1)) ok[n++] = a;
    }
    if (n == 0) return false;
    c.art = ok[rand() % n];
    compute(c);
    c.active = true;
    s_p[slot] = c;
    return true;
}

void PaintingManager::tick(const ChunkManager& cm) {
    for (int i = 0; i < MAX_PAINTINGS; i++) {
        Painting& p = s_p[i];
        if (!p.active) continue;
        if (p.ticks++ < 100) continue;
        p.ticks = 0;
        if (!isValid(cm, p, i)) { pushDrop(p); p.active = false; }
    }
}

bool PaintingManager::popDrop(Drop& out) {
    if (s_dropCount == 0) return false;
    out = s_drops[--s_dropCount];
    return true;
}

int PaintingManager::pick(float ox, float oy, float oz, float dx, float dy, float dz, float maxDist, float& hitDist) {
    int best = -1;
    double bestDist = (double)maxDist + 1.0;
    const double o[3] = { (double)ox + OX, (double)oy + OY, (double)oz + OZ }, d[3] = { dx, dy, dz };
    for (int i = 0; i < MAX_PAINTINGS; i++) {
        const Painting& p = s_p[i];
        if (!p.active) continue;
        const double g = 0.1;   // Entity.collisionBorderSize
        const double lo[3] = { p.minX - g, p.minY - g, p.minZ - g }, hi[3] = { p.maxX + g, p.maxY + g, p.maxZ + g };
        double t0 = 0.0, t1 = (double)maxDist;
        bool miss = false;
        for (int a = 0; a < 3 && !miss; a++) {
            if (fabs(d[a]) < 1e-9) { if (o[a] < lo[a] || o[a] > hi[a]) miss = true; continue; }
            double ta = (lo[a] - o[a]) / d[a], tb = (hi[a] - o[a]) / d[a];
            if (ta > tb) { const double t = ta; ta = tb; tb = t; }
            if (ta > t0) t0 = ta;
            if (tb < t1) t1 = tb;
            if (t0 > t1) miss = true;
        }
        if (!miss && t0 < bestDist) { bestDist = t0; best = i; }
    }
    if (best >= 0) hitDist = (float)bestDist;
    return best;
}

void PaintingManager::attack(int index) {
    if (index < 0 || index >= MAX_PAINTINGS || !s_p[index].active) return;
    pushDrop(s_p[index]);
    s_p[index].active = false;
}

// RenderPainting: una cara de cada 16 x 16 pixeles de Beta con su propia luz; el borde sale de la zona de tablones de kz.png (192..208, 0..16)
void PaintingManager::draw(const ChunkManager& cm, float px, float py, float pz) {
    (void)py;
    bool bound = false;
    for (int i = 0; i < MAX_PAINTINGS; i++) {
        const Painting& p = s_p[i];
        if (!p.active) continue;
        const float cx = (float)(p.px - OX), cy = (float)(p.py - OY), cz = (float)(p.pz - OZ);
        const float dx = px - cx, dz = pz - cz;
        if (dx * dx + dz * dz > 26.0f * 26.0f) continue;
        const float nx = p.dir == 1 ? -1.0f : (p.dir == 3 ? 1.0f : 0.0f), nz = p.dir == 0 ? -1.0f : (p.dir == 2 ? 1.0f : 0.0f);
        if (dx * nx + dz * nz <= 0.0f) continue;   // por detras no se ve
        Chunk* c = cm.getChunkByChunkCoord(p.gx >> 3, p.gz >> 3);
        if (!c || !c->isGenerationComplete()) continue;

        const Art& a = ARTS[p.art];
        const float ang = (float)p.dir * 90.0f * 3.14159265f / 180.0f;
        const float sn = sinf(ang), cs = cosf(ang);
        if (!bound) { glBindTexture(0, Texture::paintingTextureID); bound = true; }   // sin textura (no cupo en la VRAM): id 0 = blanco plano con la luz del lugar
        glPolyFmt(POLY_ALPHA(31) | POLY_CULL_NONE | POLY_FOG | POLY_ID(21));
        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        glTranslatef(cx, cy, cz);
        glRotatef((float)p.dir * 90.0f, 0.0f, 1.0f, 0.0f);

        const int w = a.sx, h = a.sy;
        glBegin(GL_QUADS);
        for (int u = 0; u < w / 16; u++) {
            for (int v = 0; v < h / 16; v++) {
                const int x12 = -w / 2 + (u + 1) * 16, x13 = -w / 2 + u * 16;      // pixeles del modelo (x12 > x13)
                const int y14 = -h / 2 + (v + 1) * 16, y15 = -h / 2 + v * 16;
                const float lx = (float)(x12 + x13) * 0.5f / 16.0f, ly = (float)(y14 + y15) * 0.5f / 16.0f;
                const float wx = cx + lx * cs, wz = cz - lx * sn;
                const int lvl = HandAnimations::lightLevelAt(cm, wx, cy + ly, wz);
                const int br = (int)(brightnessOf((unsigned char)lvl) * 255.0f + 0.5f);
                glColor3b(br, br, br);
                // u en pixeles de kz.png x PAINTING_UV = texeles de la textura reducida x 16
                const int u16 = (a.ox + w - u * 16) * PAINTING_UV, u17 = (a.ox + w - (u + 1) * 16) * PAINTING_UV;
                const int v18 = (a.oy + h - v * 16) * PAINTING_UV, v19 = (a.oy + h - (v + 1) * 16) * PAINTING_UV;
                vtx(u17, v18, x12 * 256, y15 * 256, -128);
                vtx(u16, v18, x13 * 256, y15 * 256, -128);
                vtx(u16, v19, x13 * 256, y14 * 256, -128);
                vtx(u17, v19, x12 * 256, y14 * 256, -128);
            }
        }
        // bordes (1/16 de bloque de grosor): arriba, abajo, izquierda y derecha, con los tablones de kz.png
        glColor3b(120, 120, 120);
        const int xl = (-w / 2) * 256, xr = (w / 2) * 256, yb = (-h / 2) * 256, yt = (h / 2) * 256;
        const int B0 = 192 * PAINTING_UV, B1 = 208 * PAINTING_UV, BM = 192 * PAINTING_UV + PAINTING_UV / 2, BV = PAINTING_UV / 2, BE = 16 * PAINTING_UV;
        vtx(B0, BV, xr, yt, -128); vtx(B1, BV, xl, yt, -128); vtx(B1, BV, xl, yt, 128); vtx(B0, BV, xr, yt, 128);   // arriba
        vtx(B0, BV, xr, yb, 128);  vtx(B1, BV, xl, yb, 128);  vtx(B1, BV, xl, yb, -128); vtx(B0, BV, xr, yb, -128); // abajo
        vtx(BM, 0, xr, yt, 128);  vtx(BM, BE, xr, yb, 128); vtx(BM, BE, xr, yb, -128); vtx(BM, 0, xr, yt, -128); // lado +x del modelo
        vtx(BM, 0, xl, yt, -128); vtx(BM, BE, xl, yb, -128); vtx(BM, BE, xl, yb, 128); vtx(BM, 0, xl, yt, 128);   // lado -x
        glEnd();
        glPopMatrix(1);
    }
}

// paintings.dat: 'MPNT', version 1, cantidad y por cuadro: gx, gy, gz (int32), direccion y motivo (1 byte cada uno)
bool PaintingManager::saveToFile(const char* path) {
    FILE* f = fopen(path, "wb");
    if (!f) return false;
    int n = 0;
    for (int i = 0; i < MAX_PAINTINGS; i++) if (s_p[i].active) n++;
    fputc('M', f); fputc('P', f); fputc('N', f); fputc('T', f);
    fputc(1, f);
    fputc(n, f);
    for (int i = 0; i < MAX_PAINTINGS; i++) {
        const Painting& p = s_p[i];
        if (!p.active) continue;
        const int v[3] = { p.gx, p.gy, p.gz };
        fwrite(v, 4, 3, f);
        fputc(p.dir, f); fputc(p.art, f);
    }
    fclose(f);
    return true;
}

bool PaintingManager::loadFromFile(const char* path) {
    clear();
    FILE* f = fopen(path, "rb");
    if (!f) return false;
    char m[4];
    if (fread(m, 1, 4, f) != 4 || m[0] != 'M' || m[1] != 'P' || m[2] != 'N' || m[3] != 'T' || fgetc(f) != 1) { fclose(f); return false; }
    const int n = fgetc(f);
    int slot = 0;
    for (int i = 0; i < n && slot < MAX_PAINTINGS; i++) {
        int v[3];
        if (fread(v, 4, 3, f) != 3) break;
        const int dir = fgetc(f), art = fgetc(f);
        if (dir < 0 || dir > 3 || art < 0 || art >= ART_COUNT) continue;
        Painting& p = s_p[slot++];
        memset(&p, 0, sizeof(p));
        p.gx = v[0]; p.gy = v[1]; p.gz = v[2]; p.dir = dir; p.art = art; p.ticks = (i * 7) % 100;
        compute(p);
        p.active = true;
    }
    fclose(f);
    return true;
}
