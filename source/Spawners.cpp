#include "Spawners.h"
#include "ChunkManager.h"
#include "Chunk.h"
#include "BlockType.h"
#include "HostileManager.h"
#include "HostileModel.h"
#include "HostileAI.h"
#include "HandAnimations.h"
#include "Particles.h"
#include <nds.h>
#include <math.h>

namespace {
    struct Entry { int gx, gy, gz; int delay; bool used; float yaw, prevYaw; };
    const int MAX_ENTRIES = 8;
    Entry s_entries[MAX_ENTRIES];
    int s_scanTimer = 0;
    unsigned s_rng = 0x1B873593u;
    int nextInt(int n) { s_rng ^= s_rng << 13; s_rng ^= s_rng >> 17; s_rng ^= s_rng << 5; return (int)(s_rng % (unsigned)n); }

    Chunk* chunkAt(const ChunkManager& cm, int gx, int gz) {
        Chunk* c = cm.getChunkByChunkCoord(gx >> 3, gz >> 3);
        return (c && c->isGenerationComplete()) ? c : nullptr;
    }
    // el centro del bloque en el mundo: la celda gx cubre x de gx - 4 a gx - 3; la celda yi cubre y de yi - 1,5 a yi - 0,5
    float cx(const Entry& e) { return (float)(e.gx - 4) + 0.5f; }
    float cy(const Entry& e) { return (float)e.gy - 1.0f; }
    float cz(const Entry& e) { return (float)(e.gz - 4) + 0.5f; }
    bool inRange(const Entry& e, float px, float py, float pz) {
        const float dx = px - cx(e), dy = (py - 1.62f) - cy(e), dz = pz - cz(e);
        return dx * dx + dy * dy + dz * dz <= 256.0f;
    }
    int findEntry(int gx, int gy, int gz) {
        for (int i = 0; i < MAX_ENTRIES; i++) if (s_entries[i].used && s_entries[i].gx == gx && s_entries[i].gy == gy && s_entries[i].gz == gz) return i;
        return -1;
    }
}

void Spawners::clear() {
    for (int i = 0; i < MAX_ENTRIES; i++) s_entries[i].used = false;
    s_scanTimer = 0;
}

void Spawners::tick(const ChunkManager& cm, float px, float py, float pz) {
    // cada segundo: busca generadores a menos de 16 bloques (los chunks de alrededor, la franja de altura del jugador)
    if (--s_scanTimer <= 0) {
        s_scanTimer = 20;
        const int pgx = (int)floorf(px) + 4, pgz = (int)floorf(pz) + 4, pyi = (int)floorf(py - 1.62f + 0.5f) + 1;
        int y0 = pyi - 17, y1 = pyi + 17;
        if (y0 < 1) y0 = 1;
        if (y1 > 126) y1 = 126;
        for (int ccx = (pgx - 16) >> 3; ccx <= (pgx + 16) >> 3; ccx++) {
            for (int ccz = (pgz - 16) >> 3; ccz <= (pgz + 16) >> 3; ccz++) {
                Chunk* c = cm.getChunkByChunkCoord(ccx, ccz);
                if (!c || !c->isGenerationComplete()) continue;
                for (int xi = 0; xi < 8; xi++) for (int zi = 0; zi < 8; zi++) for (int y = y0; y <= y1; y++) {
                    if (c->getBlockType(xi, y, zi) != BlockType::MOB_SPAWNER) continue;
                    Entry e = { ccx * 8 + xi, y, ccz * 8 + zi, 20, true, 0.0f, 0.0f };
                    if (!inRange(e, px, py, pz) || findEntry(e.gx, e.gy, e.gz) >= 0) continue;
                    for (int i = 0; i < MAX_ENTRIES; i++) if (!s_entries[i].used) { s_entries[i] = e; break; }
                }
            }
        }
    }

    for (int n = 0; n < MAX_ENTRIES; n++) {
        Entry& e = s_entries[n];
        if (!e.used) continue;
        Chunk* c = chunkAt(cm, e.gx, e.gz);
        if (!c || c->getBlockType(e.gx & 7, e.gy, e.gz & 7) != BlockType::MOB_SPAWNER || !inRange(e, px, py, pz)) { e.used = false; continue; }
        if ((e.delay & 1) == 0) {   // TileEntityMobSpawner.updateEntity: humo y llama en un punto al azar del bloque (cada dos ticks, para gastar menos)
            const float rx = cx(e) - 0.5f + (float)nextInt(1000) / 1000.0f, ry = cy(e) - 0.5f + (float)nextInt(1000) / 1000.0f, rz = cz(e) - 0.5f + (float)nextInt(1000) / 1000.0f;
            Particles::spawn(Particles::SMOKE, rx, ry, rz, 0.0f, 0.0f, 0.0f);
            Particles::spawn(Particles::FLAME, rx, ry, rz, 0.0f, 0.0f, 0.0f);
        }
        e.prevYaw = e.yaw;   // TileEntityMobSpawner.updateEntity: gira 1000 / (delay + 200) grados por tick (el dibujo la multiplica por 10)
        e.yaw += 1000.0f / ((float)(e.delay > 0 ? e.delay : 0) + 200.0f);
        if (e.yaw > 360.0f) { e.yaw -= 360.0f; e.prevYaw -= 360.0f; }
        if (e.delay > 0) { --e.delay; continue; }
        const int kind = c->getLiquidMeta(e.gx & 7, e.gy, e.gz & 7);
        for (int t = 0; t < 4; t++) {
            const int r = HostileManager::spawnerSpawn(cm, kind, e.gx - 4, e.gy - 1, e.gz - 4);
            if (r < 0) { e.delay = 200 + nextInt(600); break; }
            if (r > 0) { e.delay = 200 + nextInt(600); Particles::spawnerBurst(cx(e), cy(e), cz(e)); }
        }
    }
}

void Spawners::draw(const ChunkManager& cm, float partialTick) {
    static Mob probe;
    for (int n = 0; n < MAX_ENTRIES; n++) {
        const Entry& e = s_entries[n];
        if (!e.used) continue;
        Chunk* c = chunkAt(cm, e.gx, e.gz);
        if (!c) continue;
        const int meta = c->getLiquidMeta(e.gx & 7, e.gy, e.gz & 7);
        const int kind = meta == 1 ? 1 : (meta == 2 ? 2 : 0);   // H_ZOMBIE = 0, H_SKELETON = 1, H_SPIDER = 2
        probe = Mob();
        HostileAI::setup(probe, kind, 1);
        probe.active = true;
        probe.x = probe.prevX = 0.0; probe.y = probe.prevY = 0.0; probe.z = probe.prevZ = 0.0;
        probe.yaw = probe.prevYaw = probe.bodyYaw = probe.prevBodyYaw = 0.0f;
        const float spin = (e.prevYaw + (e.yaw - e.prevYaw) * partialTick) * 10.0f;
        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        glTranslatef(cx(e), cy(e) - 0.5f, cz(e));   // la parte baja del bloque (Beta: yCoord)
        glTranslatef(0.0f, 0.4f, 0.0f);
        glRotatef(spin, 0.0f, 1.0f, 0.0f);
        glRotatef(-30.0f, 1.0f, 0.0f, 0.0f);
        glTranslatef(0.0f, -0.4f, 0.0f);
        glScalef(7.0f / 16.0f, 7.0f / 16.0f, 7.0f / 16.0f);
        HostileModel::draw(probe, partialTick, HandAnimations::lightLevelAt(cm, cx(e), cy(e), cz(e)));
        glPopMatrix(1);
    }
}
