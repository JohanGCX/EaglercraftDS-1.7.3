#include "PartialBlocks.h"
#include <stdlib.h>
#include "HostileManager.h"
#include "Particles.h"
#include "Sound.h"
#include "HostileAI.h"
#include "HostileModel.h"
#include "ChunkManager.h"
#include "Chunk.h"
#include "HandAnimations.h"
#include <math.h>
#include <string.h>
#include "TntManager.h"
#include "Options.h"
#include "MobCull.h"
#include "NoteBlocks.h"
#include <stdio.h>

namespace {
    Mob s_mobs[HostileManager::MAX_MOBS];
    Arrow s_arrows[HostileManager::MAX_ARROWS];
    Fireball s_fireballs[HostileManager::MAX_FIREBALLS];
    AnimalAI::Rng s_rng;
    bool s_enabled = false;
    bool s_nether = false;
    int s_spawnTimer = 0;
    int s_held = 0;                  // lo que lleva el jugador: 0 nada, 1 hueso, 2 carne de cerdo
    const int KIND_CAP[H_KIND_COUNT] = { 4, 4, 4, 4, 6, 4, 3, 4, 3 };   // por tipo (zombie, esqueleto, araña, creeper, zombi cerdo, slime, ghast, lobo, calamar)

    // Spawn limits: by default half of the original ones (performance on DS/DSi); Options -> debug options -> oldmobspawnlimit brings back the old ones
    bool oldLimits() { return Options::get().oldMobSpawnLimit; }
    int halfCap(int c) { return oldLimits() ? c : (c + 1) / 2; }
    int monsterCap() { return halfCap(HostileManager::MONSTER_CAP); }
    int kindCap(int kind) { return halfCap(KIND_CAP[kind]); }

    const int DROP_QUEUE = 24;
    HostileManager::Drop s_drops[DROP_QUEUE];
    int s_dropHead = 0, s_dropCount = 0;
    const int HIT_QUEUE = 8;
    HostileManager::PlayerHit s_hits[HIT_QUEUE];
    int s_hitHead = 0, s_hitCount = 0;

    HostileManager::ExplosionListener s_explosionListener = nullptr;

    struct PendingExplosion { double x, y, z; float size; bool flaming; };
    const int MAX_EXPLOSIONS = 24;
    PendingExplosion s_explosions[MAX_EXPLOSIONS];
    int s_explosionCount = 0;

    // El mundo tal como lo ve HostileAI: celdas alineadas al mundo (x e z enteros; el bloque j cubre y de j-0,5 a j+0,5). Igual que el de los animales,
    // mas la luz del cielo y de bloque, la linea de vista y la hora del dia.
    struct HWorld {
        const ChunkManager& cm;
        explicit HWorld(const ChunkManager& c) : cm(c) {}
        mutable int cacheCx = 0x7FFFFFFF, cacheCz = 0x7FFFFFFF;
        mutable Chunk* cacheChunk = nullptr;
        mutable bool cacheReady = false;

        Chunk* chunkAt(int gx, int gz) const {
            const int cx = gx >> 3, cz = gz >> 3;
            if (cx != cacheCx || cz != cacheCz) {
                cacheCx = cx; cacheCz = cz;
                cacheChunk = cm.getChunkByChunkCoord(cx, cz);
                cacheReady = cacheChunk && cacheChunk->isGenerationComplete();
            }
            return cacheReady ? cacheChunk : nullptr;
        }
        BlockType type(int i, int j, int k) const {
            if (j < -1) return BlockType::BEDROCK;
            if (j > 126) return BlockType::AIR;
            const int gx = i + 4, gz = k + 4;
            Chunk* c = chunkAt(gx, gz);
            if (!c) return BlockType::STONE;   // sin chunk, solido: no se cae al vacio
            return c->getBlockType(gx & 7, j + 1, gz & 7);
        }
        float light(int i, int j, int k) const {
            const unsigned char level = HandAnimations::lightLevelAt(cm, (float)i + 0.5f, (float)j, (float)k + 0.5f);
            const float f = 1.0f - (float)level / 15.0f;
            return (1.0f - f) / (f * 3.0f + 1.0f) * 0.95f + 0.05f;
        }
        bool chunkReady(int cx, int cz) const {
            Chunk* c = cm.getChunkByChunkCoord(cx, cz);
            return c && c->isGenerationComplete();
        }
        // Para aparecer mobs: ademas de estar generado, el chunk debe tener ya su luz calculada. Mientras la luz inicial del cielo (o el recalculo) esta
        // pendiente el chunk se ve oscuro, y los hostiles aparecian de dia en esa ventana.
        bool lightReady(int cx, int cz) const {
            Chunk* c = cm.getChunkByChunkCoord(cx, cz);
            return c && c->isGenerationComplete() && !c->isInitialSkyLightPending() && !c->isCacheRecalcPending() && !c->isEmitterReseedPending();
        }
        int rawSky(int i, int j, int k) const {
            if (j > 126) return 15;
            if (j < -1) return 0;
            const int gx = i + 4, gz = k + 4;
            Chunk* c = chunkAt(gx, gz);
            return c ? (int)c->getSkyLight(gx & 7, j + 1, gz & 7) : 15;
        }
        int totalLight(int i, int j, int k) const {
            if (j > 126) return 15 - cm.getSkylightSubtract() > 0 ? 15 - cm.getSkylightSubtract() : 0;
            if (j < -1) return 0;
            const int gx = i + 4, gz = k + 4;
            Chunk* c = chunkAt(gx, gz);
            if (!c) return 15;
            int sky = (int)c->getSkyLight(gx & 7, j + 1, gz & 7) - cm.getSkylightSubtract();
            const int blk = (int)c->getBlockLight(gx & 7, j + 1, gz & 7);
            int lvl = sky > blk ? sky : blk;
            if (lvl < 0) lvl = 0;
            if (lvl > 15) lvl = 15;
            return lvl;
        }
        int blockLight(int i, int j, int k) const {
            if (j > 126 || j < -1) return 0;
            const int gx = i + 4, gz = k + 4;
            Chunk* c = chunkAt(gx, gz);
            return c ? (int)c->getBlockLight(gx & 7, j + 1, gz & 7) : 0;
        }
        BiomeType biomeAt(int i, int k) const { return cm.getBiomeAt((float)i + 0.5f, (float)k + 0.5f); }
        bool canSeeSky(int i, int j, int k) const { return rawSky(i, j, k) >= 14; }
        bool isDay() const { return !s_nether && cm.getSkylightSubtract() < 4; }
        // World.rayTraceBlocks == null: se muestrea el segmento de a 0,25
        bool lineClear(double x0, double y0, double z0, double x1, double y1, double z1) const {
            const double dx = x1 - x0, dy = y1 - y0, dz = z1 - z0;
            const double len = sqrt(dx * dx + dy * dy + dz * dz);
            int steps = (int)ceil(len / 0.25);
            if (steps < 2) return true;
            for (int n = 1; n < steps; n++) {
                const double t = (double)n / (double)steps;
                if (isMaterialSolid(type((int)floor(x0 + dx * t), AnimalAI::cellY(y0 + dy * t), (int)floor(z0 + dz * t)))) return false;
            }
            return true;
        }
    };

    void pushDrop(BlockType type, int count, double x, double y, double z) {
        if (count <= 0 || s_dropCount >= DROP_QUEUE) return;
        HostileManager::Drop& d = s_drops[(s_dropHead + s_dropCount) % DROP_QUEUE];
        d.type = type; d.count = count; d.x = (float)x; d.y = (float)y; d.z = (float)z;
        ++s_dropCount;
    }

    void pushHit(int amount, double sx, double sz, bool explosion, double kx, double ky, double kz) {
        if (s_hitCount >= HIT_QUEUE) return;
        HostileManager::PlayerHit& h = s_hits[(s_hitHead + s_hitCount) % HIT_QUEUE];
        h.amount = amount; h.srcX = (float)sx; h.srcZ = (float)sz; h.explosion = explosion; h.kx = (float)kx; h.ky = (float)ky; h.kz = (float)kz;
        ++s_hitCount;
    }

    // EntityLiving.dropFewItems: getDropItemId, nextInt(3) unidades (el esqueleto suelta flechas y huesos)
    void collectDeath(Mob& m) {
        if (!m.diedPending) return;
        m.diedPending = false;
        const double y = m.y;
        switch (m.hkind) {
        case H_ZOMBIE:    pushDrop(BlockType::FEATHER, s_rng.nextInt(3), m.x, y, m.z); break;
        case H_SKELETON:  pushDrop(BlockType::ARROW, s_rng.nextInt(3), m.x, y, m.z); pushDrop(BlockType::BONE, s_rng.nextInt(3), m.x, y, m.z); break;
        case H_SPIDER:    pushDrop(BlockType::STRING, s_rng.nextInt(3), m.x, y, m.z); break;
        case H_CREEPER:   pushDrop(BlockType::GUNPOWDER, s_rng.nextInt(3), m.x, y, m.z); break;
        case H_PIGZOMBIE: pushDrop(BlockType::PORK_COOKED, s_rng.nextInt(3), m.x, y, m.z); break;
        case H_SLIME:     if (m.slimeSize == 1) pushDrop(BlockType::SLIMEBALL, s_rng.nextInt(3), m.x, y, m.z); break;   // getDropItemId: solo el chico
        case H_GHAST:     pushDrop(BlockType::GUNPOWDER, s_rng.nextInt(3), m.x, y, m.z); break;
        case H_SQUID:     pushDrop(BlockType::INK_SAC, s_rng.nextInt(3) + 1, m.x, y, m.z); break;   // EntitySquid.dropFewItems
        default: break;                                                                              // el lobo no suelta nada
        }
    }

    // ---- Explosion (Beta 1.7.3): doExplosionA (rayos, bloques, entidades) y doExplosionB (quita los bloques y suelta el 30%) ----
    // Resistencia a la explosion (Block.getExplosionResistance = blockResistance / 5, casi siempre la dureza)
    float resistanceOf(BlockType t) {
        if (isLiquidBlock(t)) return 100.0f;
        switch (t) {
        case BlockType::BEDROCK: case BlockType::PORTAL: return 1.0e6f;
        case BlockType::OBSIDIAN: return 400.0f;
        case BlockType::BLOCK_IRON: case BlockType::BLOCK_GOLD: case BlockType::BLOCK_DIAMOND: return 6.0f;   // setResistance(10) x3 / 5
        case BlockType::BLOCK_LAPIS: return 3.0f;
        case BlockType::STONE: return 2.0f;
        case BlockType::WOOD_PLANKS: return 1.0f;
        case BlockType::COAL_ORE: case BlockType::IRON_ORE: case BlockType::GOLD_ORE: case BlockType::DIAMOND_ORE: case BlockType::REDSTONE_ORE: case BlockType::LAPIS_ORE:
            return 1.0f;
        default: break;
        }
        const float h = blockHardness(t);
        return h < 0.0f ? 1.0e6f : h;
    }
    // Lo que una explosion no rompe: cofres y hornos (su contenido no se pierde), liquidos, el fondo y el portal
    bool unbreakable(BlockType t) {
        return t == BlockType::AIR || t == BlockType::BEDROCK || t == BlockType::PORTAL || t == BlockType::CHEST || isFurnaceBlock(t) || isDispenserBlock(t) || isLiquidBlock(t) || t == BlockType::OBSIDIAN;
    }

    // World.func_675_a: que parte de la caja se ve desde el centro de la explosion (0 a 1)
    float exposure(const HWorld& w, double ex, double ey, double ez, double x0, double y0, double z0, double x1, double y1, double z1) {
        const double sx = 1.0 / ((x1 - x0) * 2.0 + 1.0), sy = 1.0 / ((y1 - y0) * 2.0 + 1.0), sz = 1.0 / ((z1 - z0) * 2.0 + 1.0);
        int hit = 0, total = 0;
        for (double f = 0.0; f <= 1.0; f += sx) for (double g = 0.0; g <= 1.0; g += sy) for (double h = 0.0; h <= 1.0; h += sz) {
            if (w.lineClear(x0 + (x1 - x0) * f, y0 + (y1 - y0) * g, z0 + (z1 - z0) * h, ex, ey, ez)) ++hit;
            ++total;
        }
        return total > 0 ? (float)hit / (float)total : 0.0f;
    }

    void explode(const ChunkManager& cm, HWorld& w, const AnimalAI::PlayerPos& pl, double ex, double ey, double ez, float size, bool flaming) {
        static unsigned char grid[21][21][21];
        memset(grid, 0, sizeof(grid));
        Sound::explosion((float)ex, (float)ey, (float)ez);
        Particles::explosion((float)ex, (float)ey, (float)ez, size);
        const double cy = ey + 0.5;   // y alineada a las celdas de la Beta (la celda j cubre de j-0,5 a j+0,5 en el mundo)
        const int ci = (int)floor(ex), cj = (int)floor(cy), ck = (int)floor(ez);

        const int N = 16;
        for (int a = 0; a < N; a++) for (int b = 0; b < N; b++) for (int c = 0; c < N; c++) {
            if (!(a == 0 || a == N - 1 || b == 0 || b == N - 1 || c == 0 || c == N - 1)) continue;
            double dx = (double)((float)a / ((float)N - 1.0f) * 2.0f - 1.0f);
            double dy = (double)((float)b / ((float)N - 1.0f) * 2.0f - 1.0f);
            double dz = (double)((float)c / ((float)N - 1.0f) * 2.0f - 1.0f);
            const double l = sqrt(dx * dx + dy * dy + dz * dz);
            dx /= l; dy /= l; dz /= l;
            float len = size * (0.7f + s_rng.nextFloat() * 0.6f);
            double x = ex, y = cy, z = ez;
            for (; len > 0.0f; len -= 0.3f * (12.0f / 16.0f)) {
                const int i = (int)floor(x), j = (int)floor(y), k = (int)floor(z);
                const BlockType t = w.type(i, j, k);
                if (t != BlockType::AIR) len -= (resistanceOf(t) + 0.3f) * 0.3f;
                if (len > 0.0f && t != BlockType::AIR) {
                    const int di = i - ci + 10, dj = j - cj + 10, dk = k - ck + 10;
                    if (di >= 0 && di < 21 && dj >= 0 && dj < 21 && dk >= 0 && dk < 21) grid[di][dj][dk] = 1;
                }
                x += dx * 0.3; y += dy * 0.3; z += dz * 0.3;
            }
        }

        // entidades: el jugador y los mobs en un radio del doble de la fuerza
        const float D = size * 2.0f;
        if (pl.valid) {
            const double dx = pl.x - ex, dy = pl.y - ey, dz = pl.z - ez;
            const double d = sqrt(dx * dx + dy * dy + dz * dz);
            if (d / (double)D <= 1.0 && d > 1.0e-6) {
                const double e = (double)exposure(w, ex, ey, ez, pl.x - 0.3, pl.y - 1.62, pl.z - 0.3, pl.x + 0.3, pl.y + 0.18, pl.z + 0.3);
                const double v = (1.0 - d / (double)D) * e;
                const int dmg = (int)((v * v + v) / 2.0 * 8.0 * (double)D + 1.0);
                pushHit(dmg, ex, ez, true, dx / d * v, dy / d * v, dz / d * v);
            }
        }
        for (int n = 0; n < HostileManager::MAX_MOBS; n++) {
            Mob& m = s_mobs[n];
            if (!m.active || m.hs.health <= 0) continue;
            const double dx = m.x - ex, dy = m.y - ey, dz = m.z - ez;
            const double d = sqrt(dx * dx + dy * dy + dz * dz);
            if (d / (double)D > 1.0 || d < 1.0e-6) continue;
            const double e = (double)exposure(w, ex, ey, ez, m.x - m.width * 0.5, m.y, m.z - m.width * 0.5, m.x + m.width * 0.5, m.y + m.height, m.z + m.width * 0.5);
            const double v = (1.0 - d / (double)D) * e;
            const int dmg = (int)((v * v + v) / 2.0 * 8.0 * (double)D + 1.0);
            AnimalAI::damage(m, dmg, true, ex, ez);
            m.motionX += dx / d * v; m.motionY += dy / d * v; m.motionZ += dz / d * v;
            collectDeath(m);
        }

        if (s_explosionListener) s_explosionListener(cm, ex, ey, ez, size);   // animales y ovejas

        // bloques: se quitan y el 30% suelta lo que daria al picarlos
        for (int di = 0; di < 21; di++) for (int dj = 0; dj < 21; dj++) for (int dk = 0; dk < 21; dk++) {
            if (!grid[di][dj][dk]) continue;
            const int i = ci + di - 10, j = cj + dj - 10, k = ck + dk - 10;
            const BlockType t = w.type(i, j, k);
            if (unbreakable(t)) continue;
            if (t == BlockType::TNT) {   // BlockTNT.onBlockDestroyedByExplosion: no se rompe, se enciende con la mecha corta
                cm.setBlockGlobal(i + 4, j + 1, k + 4, BlockType::AIR);
                TntManager::primeFromExplosion(i + 4, j + 1, k + 4);
                continue;
            }
            if (t == BlockType::NOTE_BLOCK) NoteBlocks::remove(i + 4, j + 1, k + 4);
            const bool noDrop = t == BlockType::OAK_LEAVES || t == BlockType::BIRCH_LEAVES || t == BlockType::SPRUCE_LEAVES || t == BlockType::TALLGRASS;
            if (!noDrop && s_rng.nextFloat() < 0.3f) pushDrop(getDropType(t), 1, (double)i + 0.5, (double)j - 0.3, (double)k + 0.5);
            cm.setBlockGlobal(i + 4, j + 1, k + 4, BlockType::AIR);
        }

        // Explosion.doExplosionB con fuego (bola del ghast): sobre cada bloque quitado que quedo en el aire con un bloque solido debajo, 1 de cada 3 se enciende
        if (flaming) {
            for (int di = 0; di < 21; di++) for (int dj = 0; dj < 21; dj++) for (int dk = 0; dk < 21; dk++) {
                if (!grid[di][dj][dk] || s_rng.nextInt(3) != 0) continue;
                const int i = ci + di - 10, j = cj + dj - 10, k = ck + dk - 10;
                if (w.type(i, j, k) == BlockType::AIR && isNormalCube(w.type(i, j - 1, k))) cm.setBlockGlobal(i + 4, j + 1, k + 4, BlockType::FIRE);
            }
        }
    }

    bool addMob(int kind, double x, double y, double z, int slimeSize = 1);

    struct Sink {
        void playerHit(int amount, double sx, double sz) { pushHit(amount, sx, sz, false, 0.0, 0.0, 0.0); }
        void addArrow(const Arrow& a) {
            for (int i = 0; i < HostileManager::MAX_ARROWS; i++) if (!s_arrows[i].active) { s_arrows[i] = a; return; }
        }
        void addFireball(const Fireball& f) {
            for (int i = 0; i < HostileManager::MAX_FIREBALLS; i++) if (!s_fireballs[i].active) { s_fireballs[i] = f; return; }
        }
        void explode(Mob& m, float size) {
            if (s_explosionCount < MAX_EXPLOSIONS) { s_explosions[s_explosionCount++] = { m.x, m.y, m.z, size, false }; }
        }
        void explodeAt(double x, double y, double z, float size, bool flaming) {
            if (s_explosionCount < MAX_EXPLOSIONS) { s_explosions[s_explosionCount++] = { x, y, z, size, flaming }; }
        }
        // EntitySlime.setEntityDead: al morir uno grande salen 4 de la mitad de tamano, repartidos en cuadro
        void splitSlime(const Mob& m) {
            const int size = m.slimeSize;
            for (int n = 0; n < 4; n++) {
                const double ox = ((double)(n % 2) - 0.5) * (double)size / 4.0, oz = ((double)(n / 2) - 0.5) * (double)size / 4.0;
                addMob(H_SLIME, m.x + ox, m.y + 0.5, m.z + oz, size / 2);
            }
        }
        // la bola de fuego choca con la caja (agrandada 0,3) de un mob (los ghasts no: la bola sale de uno)
        bool mobHit(double x0, double y0, double z0, double x1, double y1, double z1) {
            for (int i = 0; i < HostileManager::MAX_MOBS; i++) {
                const Mob& m = s_mobs[i];
                if (!m.active || m.hs.health <= 0 || m.hkind == H_GHAST) continue;
                const double hw = m.width * 0.5 + 0.3;
                if (HostileAI::segmentHitsBox(x0, y0, z0, x1, y1, z1, m.x - hw, m.y - 0.3, m.z - hw, m.x + hw, m.y + m.height + 0.3, m.z + hw)) return true;
            }
            return false;
        }
        int heldKind() { return s_held; }
    };

    int countAlive() { int n = 0; for (int i = 0; i < HostileManager::MAX_MOBS; i++) if (s_mobs[i].active) ++n; return n; }

    bool addMob(int kind, double x, double y, double z, int slimeSize) {
        for (int i = 0; i < HostileManager::MAX_MOBS; i++) {
            if (s_mobs[i].active) continue;
            Mob& m = s_mobs[i];
            HostileAI::setup(m, kind, slimeSize);
            m.active = true;
            m.x = m.prevX = x; m.y = m.prevY = y; m.z = m.prevZ = z;
            m.yaw = m.prevYaw = m.bodyYaw = m.prevBodyYaw = s_rng.nextFloat() * 360.0f;
            if (kind == H_SLIME) m.slimeJump = s_rng.nextInt(20) + 10;
            if (kind == H_SQUID) { m.sqStep = 1.0f / (s_rng.nextFloat() + 1.0f) * 0.2f; m.sqPhase = s_rng.nextFloat() * 6.2831853f; }
            if (kind == H_GHAST) { m.wayX = x; m.wayY = y; m.wayZ = z; }
            return true;
        }
        return false;
    }

    // ---- lobos domados guardados ----
    struct SavedWolf { double x, y, z; float yaw; int health; bool sitting; };
    const int MAX_SAVED_WOLVES = 8;
    SavedWolf s_pendingWolves[MAX_SAVED_WOLVES];
    int s_pendingCount = 0;

    // Un sitio libre junto al jugador (anillo de radio 2 a 4, con suelo debajo y dos celdas libres). Devuelve false si no hay ninguno
    bool spotNearPlayer(HWorld& w, const AnimalAI::PlayerPos& pl, double& ox, double& oy, double& oz) {
        const int pi = (int)floor(pl.x), pk = (int)floor(pl.z), pj = AnimalAI::cellY(pl.y - 1.62 + 1.0e-6);
        for (int r = 2; r <= 4; r++) {
            for (int di = -r; di <= r; di++) for (int dk = -r; dk <= r; dk++) {
                if ((di < 0 ? -di : di) != r && (dk < 0 ? -dk : dk) != r) continue;   // solo el borde del cuadro
                for (int dj = 0; dj >= -1; dj--) {
                    const int i = pi + di, k = pk + dk, j = pj + dj;
                    if (!w.lightReady(AnimalAI::chunkOf((double)i), AnimalAI::chunkOf((double)k))) continue;
                    if (HostileAI::airSpawnSpot(w, i, j, k)) { ox = (double)i + 0.5; oy = (double)j - 0.5; oz = (double)k + 0.5; return true; }
                }
            }
        }
        return false;
    }

    void spawnPendingWolves(HWorld& w, const AnimalAI::PlayerPos& pl, int pcx, int pcz) {
        int keep = 0;
        for (int n = 0; n < s_pendingCount; n++) {
            const SavedWolf& sw = s_pendingWolves[n];
            double x = sw.x, y = sw.y, z = sw.z;
            const int cx = AnimalAI::chunkOf(x), cz = AnimalAI::chunkOf(z);
            const bool here = abs(cx - pcx) <= 3 && abs(cz - pcz) <= 3 && w.chunkReady(cx, cz);
            bool placed = here;
            if (!here && pl.valid) placed = spotNearPlayer(w, pl, x, y, z);        // lejos del jugador: aparece a su lado
            if (placed && addMob(H_WOLF, x, y, z)) {
                for (int i = 0; i < HostileManager::MAX_MOBS; i++) {                 // el recien agregado es el ultimo activo con ese lugar
                    Mob& m = s_mobs[i];
                    if (!m.active || m.hkind != H_WOLF || m.tamed || fabs(m.x - x) > 1.0e-6 || fabs(m.z - z) > 1.0e-6) continue;
                    m.tamed = true; m.sitting = sw.sitting; m.hs.health = sw.health; m.hs.prevHealth = sw.health;
                    m.yaw = m.prevYaw = m.bodyYaw = m.prevBodyYaw = sw.yaw;
                    break;
                }
            } else {
                s_pendingWolves[keep++] = sw;   // todavia no se puede (chunk sin generar o sin sitio): se intenta en el proximo tick
            }
        }
        s_pendingCount = keep;
    }

    // ---- SpawnerAnimals.performSpawning: un tipo, un punto al azar, hasta 3 racimos de 4 pasos de +-5 en horizontal (la altura no cambia) ----
    int categoryCount(int category) {   // 0 monstruos, 1 lobos, 2 calamares
        int n = 0;
        for (int i = 0; i < HostileManager::MAX_MOBS; i++) {
            if (!s_mobs[i].active) continue;
            const int k = s_mobs[i].hkind;
            if (category == 0 ? k <= H_GHAST : (category == 1 ? k == H_WOLF : k == H_SQUID)) ++n;
        }
        return n;
    }
    int kindCount(int kind) { int n = 0; for (int i = 0; i < HostileManager::MAX_MOBS; i++) if (s_mobs[i].active && s_mobs[i].hkind == kind) ++n; return n; }

    // true si se creo al menos un mob. (px, py, pz): los ojos del jugador
    bool spawnPack(const ChunkManager& cm, HWorld& w, int kind, double px, double py, double pz, int pcx, int pcz) {
        const bool waterMob = kind == H_SQUID;
        const int category = kind == H_SQUID ? 2 : (kind == H_WOLF ? 1 : 0);
        const int categoryCap = category == 0 ? monsterCap() : halfCap(category == 1 ? HostileManager::WOLF_CAP : HostileManager::SQUID_CAP);
        const int packMax = kind == H_GHAST ? 1 : (kind == H_WOLF ? 8 : 4);
        const int feetJ = AnimalAI::cellY(py - 1.62);
        const long long seed = (long long)cm.getSeed();
        for (int attempt = 0; attempt < 12; attempt++) {
            const int cx = pcx + s_rng.nextInt(7) - 3, cz = pcz + s_rng.nextInt(7) - 3;
            if (!w.chunkReady(cx, cz)) continue;
            const int i0 = cx * 8 - 4 + s_rng.nextInt(8), k0 = cz * 8 - 4 + s_rng.nextInt(8);
            int j0 = feetJ + s_rng.nextInt(41) - 20;
            if (j0 < 0) j0 = 0;
            if (j0 > 124) j0 = 124;
            const BlockType t0 = w.type(i0, j0, k0);
            if (waterMob) { if (!isWaterBlock(t0) || isNormalCube(w.type(i0, j0 + 1, k0))) continue; }
            else if (t0 != BlockType::AIR) continue;                                   // el punto de partida: aire (y no un bloque normal)
            if (kind == H_WOLF) {                                                      // la lista de criaturas del bioma: solo bosque y taiga traen lobos
                const BiomeType b = w.biomeAt(i0, k0);
                if (b != BiomeType::FOREST && b != BiomeType::TAIGA) continue;
            }
            int made = 0;
            for (int cluster = 0; cluster < 3; cluster++) {
                int i = i0, k = k0;
                for (int step = 0; step < 4; step++) {
                    i += s_rng.nextInt(6) - s_rng.nextInt(6);
                    k += s_rng.nextInt(6) - s_rng.nextInt(6);
                    if (!w.lightReady(AnimalAI::chunkOf((double)i), AnimalAI::chunkOf((double)k))) continue;
                    if (waterMob ? !HostileAI::waterSpawnSpot(w, i, j0, k) : !HostileAI::airSpawnSpot(w, i, j0, k)) continue;
                    const double x = (double)i + 0.5, y = (double)j0 - 0.5, z = (double)k + 0.5;
                    const double dx = x - px, dy = y - (py - 1.62), dz = z - pz;
                    if (dx * dx + dy * dy + dz * dz < 144.0) continue;                 // Beta: ningun jugador a menos de 24 (aqui 12: el mundo cargado es mucho mas chico)
                    if (countAlive() >= HostileManager::MAX_MOBS || categoryCount(category) >= categoryCap || kindCount(kind) >= kindCap(kind)) return made > 0;
                    // EntityLiving.getCanSpawnHere: no encima de otro mob
                    bool taken = false;
                    for (int n = 0; n < HostileManager::MAX_MOBS && !taken; n++) {
                        const Mob& o = s_mobs[n];
                        if (o.active && fabs(o.x - x) < (o.width + 0.6) * 0.5 && fabs(o.z - z) < (o.width + 0.6) * 0.5 && o.y < y + 1.0 && o.y + o.height > y) taken = true;
                    }
                    if (taken) continue;
                    if (!HostileAI::canSpawnHere(kind, w, s_rng, s_nether, seed, i, j0, k, x, y, z)) continue;
                    int slimeSize = 1;
                    if (kind == H_SLIME) slimeSize = 1 << s_rng.nextInt(3);            // EntitySlime(): 1 << nextInt(3)
                    if (!addMob(kind, x, y, z, slimeSize)) return made > 0;
                    ++made;
                    if (made >= packMax) return true;
                }
            }
            if (made > 0) return true;
        }
        return false;
    }
}

void HostileManager::clear(unsigned int seed) {
    for (int i = 0; i < MAX_MOBS; i++) s_mobs[i] = Mob();
    for (int i = 0; i < MAX_ARROWS; i++) s_arrows[i] = Arrow();
    for (int i = 0; i < MAX_FIREBALLS; i++) s_fireballs[i] = Fireball();
    s_held = 0;
    s_rng = AnimalAI::Rng(seed ^ 0x2545F491u);
    s_spawnTimer = 40;               // el primero aparece a los 2 segundos
    s_dropHead = s_dropCount = 0;
    s_hitHead = s_hitCount = 0;
    s_explosionCount = 0;
}

bool HostileManager::sleepSpawn(const ChunkManager& cm, float px, float pz, int bedI, int bedJ, int bedK, int offX, int offZ) {
    if (!s_enabled || s_nether) return false;
    HWorld w(cm);
    static const int LIST[3] = { H_SPIDER, H_ZOMBIE, H_SKELETON };   // nightSpawnEntities
    static Mob s_probe;
    const long long seed = (long long)cm.getSeed();
    const double posY = (double)bedJ + 0.9375;
    for (int attempt = 0; attempt < 20; attempt++) {
        const int i = (int)floor((double)px) + s_rng.nextInt(32) - s_rng.nextInt(32);
        const int k = (int)floor((double)pz) + s_rng.nextInt(32) - s_rng.nextInt(32);
        int j0 = bedJ + s_rng.nextInt(16) - s_rng.nextInt(16);
        if (j0 < 1) j0 = 1; else if (j0 > 126) j0 = 126;
        const int kind = LIST[s_rng.nextInt(3)];
        if (!w.lightReady(AnimalAI::chunkOf((double)i), AnimalAI::chunkOf((double)k))) continue;
        int j = j0;
        while (j > 2 && !isNormalCube(w.type(i, j - 1, k))) --j;
        while (!HostileAI::airSpawnSpot(w, i, j, k) && j < j0 + 16 && j < 126) ++j;
        if (!(j < j0 + 16 && j < 126)) continue;
        const double x = (double)i + 0.5, y = (double)j - 0.5, z = (double)k + 0.5;
        if (!HostileAI::canSpawnHere(kind, w, s_rng, false, seed, i, j, k, x, y, z)) continue;   // getCanSpawnHere (luz, sitio libre)
        s_probe = Mob();
        HostileAI::setup(s_probe, kind, 1);
        s_probe.x = x; s_probe.y = y; s_probe.z = z;
        if (!AnimalAI::findPath(s_probe, w, bedI, bedJ, bedK, 32.0f) || s_probe.pathLen <= 1) continue;
        const int e = s_probe.pathLen - 1;
        if (!(fabs((double)s_probe.pathX[e] - (double)px) < 1.5 && fabs((double)s_probe.pathZ[e] - (double)pz) < 1.5 && fabs((double)s_probe.pathY[e] - posY) < 1.5)) continue;
        // BlockBed.getNearestEmptyChunkCoordinates(..., 1): la segunda celda con suelo macizo y dos de aire, en los 3 x 3 de cada mitad de la cama
        int skip = 1, sx = i, sj = j + 1, sz = k;
        bool got = false;
        for (int h = 0; h <= 1 && !got; h++) {
            const int x0 = bedI - offX * h - 1, z0 = bedK - offZ * h - 1;
            for (int xx = x0; xx <= x0 + 2 && !got; xx++) for (int zz = z0; zz <= z0 + 2 && !got; zz++) {
                if (isNormalCube(w.type(xx, bedJ - 1, zz)) && w.type(xx, bedJ, zz) == BlockType::AIR && w.type(xx, bedJ + 1, zz) == BlockType::AIR) {
                    if (skip <= 0) { sx = xx; sj = bedJ; sz = zz; got = true; } else --skip;
                }
            }
        }
        if (addMob(kind, (double)sx + 0.5, (double)sj - 0.5, (double)sz + 0.5, 1)) return true;
        return false;   // sin sitio en la lista de mobs: la noche pasa como si no hubiera aparecido nada
    }
    return false;
}

void HostileManager::setNether(bool nether) { s_nether = nether; }
// TileEntityMobSpawner.updateEntity (un intento): (ci, cj, ck) = celda del generador en celdas de mob. kind: 0 zombi, 1 esqueleto, 2 arana.
// Devuelve -1 si ya hay 6 o mas de su clase a su alrededor (caja de +-8, +-4, +-8), 1 si aparecio uno, 0 si no hubo sitio (la Beta lo reintenta en el siguiente intento)
static bool s_peacefulMode = false;
void HostileManager::setPeaceful(bool peaceful) { s_peacefulMode = peaceful; }

int HostileManager::spawnerSpawn(const ChunkManager& cm, int mobKind, int ci, int cj, int ck) {
    if (!s_enabled || s_nether || s_peacefulMode) return 0;
    const int kind = mobKind == 0 ? H_ZOMBIE : (mobKind == 1 ? H_SKELETON : H_SPIDER);
    int near = 0;
    for (int n = 0; n < MAX_MOBS; n++) {
        const Mob& o = s_mobs[n];
        if (o.active && o.hkind == kind && fabs(o.x - (ci + 0.5)) <= 8.5 && fabs(o.z - (ck + 0.5)) <= 8.5 && fabs((o.y + 0.5) - (double)cj) <= 4.5) ++near;
    }
    if (near >= 6) return -1;
    if (countAlive() >= MAX_MOBS || categoryCount(0) >= monsterCap()) return 0;   // the spawner also respects the monster limit
    HWorld w(cm);
    const double fx = (double)ci + (double)(s_rng.nextFloat() - s_rng.nextFloat()) * 4.0;
    const double fz = (double)ck + (double)(s_rng.nextFloat() - s_rng.nextFloat()) * 4.0;
    const int i = (int)floor(fx), k = (int)floor(fz), j = cj + s_rng.nextInt(3) - 1;
    if (!w.lightReady(AnimalAI::chunkOf((double)i), AnimalAI::chunkOf((double)k))) return 0;
    if (!HostileAI::airSpawnSpot(w, i, j, k)) return 0;
    const double x = fx, y = (double)j - 0.5, z = fz;
    if (!HostileAI::canSpawnHere(kind, w, s_rng, false, (long long)cm.getSeed(), i, j, k, x, y, z)) return 0;
    return addMob(kind, x, y, z) ? 1 : 0;
}

void HostileManager::setActive(bool active) { s_enabled = active; }
int HostileManager::count() { return countAlive(); }

void HostileManager::tick(const ChunkManager& cm, float px, float py, float pz, bool playerAlive) {
    if (!s_enabled) return;
    HWorld w(cm);
    const int pcx = AnimalAI::chunkOf(px), pcz = AnimalAI::chunkOf(pz);
    const AnimalAI::PlayerPos pl = { (double)px, (double)py, (double)pz, playerAlive };
    Sink sink;

    for (int i = 0; i < MAX_MOBS; i++) {
        Mob& m = s_mobs[i];
        if (!m.active) continue;
        if (!(fabs(m.x) < 1.0e6 && fabs(m.z) < 1.0e6 && m.y > -20.0 && m.y < 200.0)) { m.active = false; continue; }   // coordenadas rotas
        if (s_peacefulMode && m.hkind != H_WOLF && m.hkind != H_SQUID) { m.active = false; continue; }   // pacifico: los monstruos desaparecen
        if (m.hkind == H_WOLF && m.tamed && pl.valid) {   // el domado no desaparece: si se queda lejos o en un chunk que ya no esta, aparece junto al dueño
            const int wcx = AnimalAI::chunkOf(m.x), wcz = AnimalAI::chunkOf(m.z);
            if (abs(wcx - pcx) > 3 || abs(wcz - pcz) > 3 || !w.chunkReady(wcx, wcz)) {
                double tx, ty, tz;
                if (!spotNearPlayer(w, pl, tx, ty, tz)) continue;   // sin sitio: espera quieto
                m.x = m.prevX = tx; m.y = m.prevY = ty; m.z = m.prevZ = tz;
                m.motionX = m.motionY = m.motionZ = 0.0; m.pathLen = 0; m.fallDistance = 0.0f;
            }
        } else if (AnimalAI::shouldDespawn(m, w, pcx, pcz)) { m.active = false; continue; }
        // Far away and calm: the AI runs every other tick (the ghast shoots from far, the tamed wolf follows its owner, a chasing or dying mob keeps full speed)
        if (m.hkind != H_GHAST && !m.tamed && !m.hasTarget && m.hs.health > 0 && m.hs.hurtTime == 0 && MobCull::skipFar(m.x, m.z, (double)px, (double)pz, i)) {
            MobCull::freeze(m);
            m.prevSquish = m.squish; m.prevAttackCounter = m.attackCounter; m.prevInterest = m.interest;
            m.sqPrevPitch = m.sqPitch; m.sqPrevRoll = m.sqRoll; m.sqPrevTent = m.sqTent;
            continue;
        }
        HostileAI::tick(m, w, s_rng, pl, sink);
        if (m.active && m.hkind != H_GHAST) Partial::mobWalked(cm, m);   // pisotea la tierra de cultivo (el ghast vuela)
        collectDeath(m);
    }
    // (el empuje entre mobs -Entity.applyEntityCollision- lo hace SheepManager con todos a la vez: ovejas, animales y estos)

    if (s_pendingCount > 0) spawnPendingWolves(w, pl, pcx, pcz);

    for (int i = 0; i < MAX_FIREBALLS; i++) {
        Fireball& f = s_fireballs[i];
        if (!f.active) continue;
        if (!(fabs(f.x) < 1.0e6 && fabs(f.z) < 1.0e6 && f.y > -40.0 && f.y < 300.0) || !w.chunkReady(AnimalAI::chunkOf(f.x), AnimalAI::chunkOf(f.z))) { f.active = false; continue; }
        HostileAI::fireballTick(f, w, pl, sink);
    }

    for (int i = 0; i < MAX_ARROWS; i++) {
        Arrow& a = s_arrows[i];
        if (!a.active) continue;
        if (!(fabs(a.x) < 1.0e6 && fabs(a.z) < 1.0e6 && a.y > -40.0 && a.y < 300.0) || !w.chunkReady(AnimalAI::chunkOf(a.x), AnimalAI::chunkOf(a.z))) { a.active = false; continue; }
        HostileAI::arrowTick(a, w, s_rng, pl, sink);
    }

    for (int i = 0; i < s_explosionCount; i++) explode(cm, w, pl, s_explosions[i].x, s_explosions[i].y, s_explosions[i].z, s_explosions[i].size, s_explosions[i].flaming);
    s_explosionCount = 0;

    // SpawnerAnimals: un ciclo cada 0,5 a 1 segundo. Monstruos de la lista del bioma (peso 10 cada uno), y las criaturas que trae este modulo: lobos (bosque y
    // taiga, peso 2 contra 40 de los animales) y calamares (agua). El Nether solo tiene ghast y zombi cerdo (peso 10 y 10) y ni lobos ni calamares.
    if (playerAlive && --s_spawnTimer <= 0) {
        s_spawnTimer = 10 + s_rng.nextInt(10);
        if (!s_peacefulMode && categoryCount(0) < monsterCap()) {
            int kind;
            if (s_nether) kind = s_rng.nextInt(2) == 0 ? H_GHAST : H_PIGZOMBIE;
            else { static const int LIST[5] = { H_SPIDER, H_ZOMBIE, H_SKELETON, H_CREEPER, H_SLIME }; kind = LIST[s_rng.nextInt(5)]; }
            spawnPack(cm, w, kind, (double)px, (double)py, (double)pz, pcx, pcz);
        }
        if (!s_nether) {
            if (categoryCount(1) < halfCap(2) && s_rng.nextInt(4) == 0) spawnPack(cm, w, H_WOLF, (double)px, (double)py, (double)pz, pcx, pcz);
            if (categoryCount(2) < halfCap(SQUID_CAP) && s_rng.nextInt(2) == 0) spawnPack(cm, w, H_SQUID, (double)px, (double)py, (double)pz, pcx, pcz);
        }
    }
}

void HostileManager::setExplosionListener(ExplosionListener l) { s_explosionListener = l; }

void HostileManager::explodeAt(double x, double y, double z, float size, bool flaming) {
    if (s_explosionCount < MAX_EXPLOSIONS) s_explosions[s_explosionCount++] = { x, y, z, size, flaming };
}

void HostileManager::clearWolves() { s_pendingCount = 0; }

// wolves.dat: 'MWLF', version 1, cantidad, y por lobo: x, y, z (double), giro, vida y si esta sentado
bool HostileManager::saveWolves(const char* path) {
    SavedWolf all[MAX_SAVED_WOLVES];
    int n = 0;
    for (int i = 0; i < MAX_MOBS && n < MAX_SAVED_WOLVES; i++) {
        const Mob& m = s_mobs[i];
        if (!m.active || m.hkind != H_WOLF || !m.tamed || m.hs.health <= 0) continue;
        all[n++] = { m.x, m.y, m.z, m.yaw, m.hs.health, m.sitting };
    }
    for (int i = 0; i < s_pendingCount && n < MAX_SAVED_WOLVES; i++) all[n++] = s_pendingWolves[i];
    FILE* f = fopen(path, "wb");
    if (!f) return false;
    fputc('M', f); fputc('W', f); fputc('L', f); fputc('F', f);
    fputc(1, f);
    fputc(n, f);
    for (int i = 0; i < n; i++) {
        fwrite(&all[i].x, 8, 1, f); fwrite(&all[i].y, 8, 1, f); fwrite(&all[i].z, 8, 1, f);
        fwrite(&all[i].yaw, 4, 1, f);
        const int health = all[i].health;
        fwrite(&health, 4, 1, f);
        fputc(all[i].sitting ? 1 : 0, f);
    }
    fclose(f);
    return true;
}

bool HostileManager::loadWolves(const char* path) {
    s_pendingCount = 0;
    FILE* f = fopen(path, "rb");
    if (!f) return false;
    char magic[4];
    if (fread(magic, 1, 4, f) != 4 || magic[0] != 'M' || magic[1] != 'W' || magic[2] != 'L' || magic[3] != 'F' || fgetc(f) != 1) { fclose(f); return false; }
    const int n = fgetc(f);
    for (int i = 0; i < n && i < MAX_SAVED_WOLVES; i++) {
        SavedWolf w;
        int health = 0;
        if (fread(&w.x, 8, 1, f) != 1 || fread(&w.y, 8, 1, f) != 1 || fread(&w.z, 8, 1, f) != 1 || fread(&w.yaw, 4, 1, f) != 1 || fread(&health, 4, 1, f) != 1) break;
        const int sit = fgetc(f);
        if (!(fabs(w.x) < 1.0e6 && fabs(w.z) < 1.0e6 && w.y > -20.0 && w.y < 200.0) || health <= 0 || health > 20) continue;
        w.health = health; w.sitting = sit == 1;
        s_pendingWolves[s_pendingCount++] = w;
    }
    fclose(f);
    return true;
}

bool HostileManager::popDrop(Drop& out) {
    if (s_dropCount == 0) return false;
    out = s_drops[s_dropHead];
    s_dropHead = (s_dropHead + 1) % DROP_QUEUE;
    --s_dropCount;
    return true;
}

bool HostileManager::popPlayerHit(PlayerHit& out) {
    if (s_hitCount == 0) return false;
    out = s_hits[s_hitHead];
    s_hitHead = (s_hitHead + 1) % HIT_QUEUE;
    --s_hitCount;
    return true;
}

void HostileManager::setCamera(float yaw, float pitch) { HostileModel::setCamera(yaw, pitch); }

void HostileManager::draw(const ChunkManager& cm, float partialTick) {
    for (int i = 0; i < MAX_MOBS; i++) {
        const Mob& m = s_mobs[i];
        if (!m.active) continue;
        const float r = (float)(m.width > m.height ? m.width : m.height) * (m.hkind == H_SLIME ? (float)m.slimeSize : 1.0f) + 0.5f;
        if (!MobCull::visible((float)m.x, (float)(m.y + m.height * 0.5), (float)m.z, r)) continue;   // off screen or too far: not drawn
        HostileModel::draw(m, partialTick, HandAnimations::lightLevelAt(cm, (float)m.x, (float)m.y + (float)(m.height * 0.5), (float)m.z));
    }
    for (int i = 0; i < MAX_ARROWS; i++) {
        const Arrow& a = s_arrows[i];
        if (!a.active || !MobCull::visible((float)a.x, (float)a.y, (float)a.z, 0.6f)) continue;
        HostileModel::drawArrow(a, partialTick, HandAnimations::lightLevelAt(cm, (float)a.x, (float)a.y, (float)a.z));
    }
    for (int i = 0; i < MAX_FIREBALLS; i++) {
        const Fireball& f = s_fireballs[i];
        if (!f.active) continue;
        HostileModel::drawFireball(f, partialTick);
    }
}

int HostileManager::pick(float ox, float oy, float oz, float dx, float dy, float dz, float maxDist, float& hitDist) {
    int best = -1;
    double bestDist = (double)maxDist + 1.0;
    for (int i = 0; i < MAX_MOBS; i++) {
        const Mob& m = s_mobs[i];
        if (!m.active || m.hs.health <= 0) continue;
        const double d = AnimalAI::rayHit(m, ox, oy, oz, dx, dy, dz, (double)maxDist);
        if (d >= 0.0 && d < bestDist) { bestDist = d; best = i; }
    }
    if (best >= 0) hitDist = (float)bestDist;
    return best;
}

void HostileManager::lightning(double x, double y, double z) {
    for (int i = 0; i < MAX_MOBS; i++) {
        Mob& m = s_mobs[i];
        if (!m.active || m.hs.health <= 0) continue;
        if (fabs(m.x - x) > 3.0 + m.width * 0.5 || fabs(m.z - z) > 3.0 + m.width * 0.5 || m.y < y - 3.0 - m.height || m.y > y + 9.0) continue;
        if (m.hkind == H_GHAST || m.hkind == H_PIGZOMBIE) continue;   // inmunes al fuego
        AnimalAI::damage(m, 5, false, 0.0, 0.0);
        if (m.fire <= 0) m.fire = 300;
        collectDeath(m);
    }
}

bool HostileManager::spawnPigZombie(double x, double y, double z) { return addMob(H_PIGZOMBIE, x, y, z); }

bool HostileManager::anchor(int index, float& x, float& y, float& z) {
    if (index < 0 || index >= MAX_MOBS || !s_mobs[index].active || s_mobs[index].hs.health <= 0) return false;
    const Mob& m = s_mobs[index];
    x = (float)m.x; y = (float)(m.y + m.height * 0.8); z = (float)m.z;
    return true;
}

void HostileManager::pull(int index, float vx, float vy, float vz) {
    if (index < 0 || index >= MAX_MOBS || !s_mobs[index].active) return;
    Mob& m = s_mobs[index];
    m.motionX += vx; m.motionY += vy; m.motionZ += vz;
}

bool HostileManager::attack(int index, int damage, float attackerX, float attackerZ) {
    if (index < 0 || index >= MAX_MOBS || !s_mobs[index].active) return false;
    Mob& m = s_mobs[index];
    if (m.hkind == H_WOLF) m.sitting = false;                            // EntityWolf.attackEntityFrom: se levanta
    const bool hurt = AnimalAI::damage(m, damage, true, (double)attackerX, (double)attackerZ);
    if (hurt && m.hkind == H_WOLF) {
        if (!m.tamed && !m.angry) {                                      // el jugador lo golpea: se enoja, y toda la manada de 16 bloques con el
            m.angry = true; m.hasTarget = true; m.pathLen = 0;
            for (int i = 0; i < MAX_MOBS; i++) {
                Mob& o = s_mobs[i];
                if (!o.active || o.hkind != H_WOLF || o.tamed || o.hasTarget) continue;
                if (fabs(o.x - m.x) > 16.0 || fabs(o.z - m.z) > 16.0 || fabs(o.y - m.y) > 4.0) continue;
                o.hasTarget = true; o.angry = true; o.pathLen = 0;
            }
        }
    } else if (hurt && m.hs.health > 0 && m.hkind != H_SQUID && m.hkind != H_GHAST && m.hkind != H_SLIME) {
        m.hasTarget = true;                                              // EntityMob.attackEntityFrom: playerToAttack = el que golpea
        m.pathLen = 0;
        if (m.hkind == H_PIGZOMBIE) {                                    // EntityPigZombie.attackEntityFrom: se enojan todos los de 32 bloques
            for (int i = 0; i < MAX_MOBS; i++) {
                Mob& o = s_mobs[i];
                if (!o.active || o.hkind != H_PIGZOMBIE) continue;
                const double dx = o.x - m.x, dy = o.y - m.y, dz = o.z - m.z;
                if (dx * dx + dy * dy + dz * dz > 1024.0) continue;
                o.anger = 400 + s_rng.nextInt(400);
                o.hasTarget = true; o.pathLen = 0;
            }
            m.anger = 400 + s_rng.nextInt(400);
        }
    }
    collectDeath(m);
    return hurt;
}

static float rf01() { return (float)(rand() & 1023) / 1023.0f; }

// EntityWolf.interact
int HostileManager::interact(int index, int held) {
    if (index < 0 || index >= MAX_MOBS || !s_mobs[index].active) return 0;
    Mob& m = s_mobs[index];
    if (m.hkind != H_WOLF || m.hs.health <= 0) return 0;
    if (!m.tamed) {
        if (held == 1 && !m.angry) {                                     // un hueso: 1 de cada 3 lo doma
            if (s_rng.nextInt(3) == 0) {
                m.tamed = true; m.pathLen = 0; m.sitting = true; m.hs.health = 20; m.hs.prevHealth = 20; m.hasTarget = false;
                for (int k = 0; k < 7; k++) Particles::spawn(Particles::HEART, (float)m.x + (rf01() - 0.5f) * 0.8f, (float)m.y + 0.8f + rf01() * 0.5f, (float)m.z + (rf01() - 0.5f) * 0.8f, 0.0f, 0.0f, 0.0f);
            } else {
                for (int k = 0; k < 7; k++) Particles::spawn(Particles::SMOKE, (float)m.x + (rf01() - 0.5f) * 0.8f, (float)m.y + 0.8f + rf01() * 0.5f, (float)m.z + (rf01() - 0.5f) * 0.8f, 0.0f, 0.0f, 0.0f);
            }
            return 1;
        }
        return 0;
    }
    if (held == 2 && m.hs.health < 20) {                                 // carne de cerdo cruda: cura 3
        m.hs.health += 3; if (m.hs.health > 20) m.hs.health = 20;
        return 1;
    }
    m.sitting = !m.sitting;                                              // el dueño (el unico jugador) lo sienta o lo para
    m.isJumping = false; m.pathLen = 0;
    return 2;
}

void HostileManager::setHeldKind(int held) { s_held = held; }

int HostileManager::collectBodies(MobBody* out, int maxN) {
    int n = 0;
    for (int i = 0; i < MAX_MOBS && n < maxN; i++) {
        Mob& m = s_mobs[i];
        if (!m.active || m.hs.health <= 0) continue;
        out[n].x = &m.x; out[n].y = &m.y; out[n].z = &m.z; out[n].mx = &m.motionX; out[n].mz = &m.motionZ; out[n].w = m.width; out[n].h = m.height;
        ++n;
    }
    return n;
}

bool HostileManager::overlapsBlock(float bx, float by, float bz) {
    for (int i = 0; i < MAX_MOBS; i++)
        if (s_mobs[i].active && AnimalAI::overlapsBlock(s_mobs[i], bx, by, bz)) return true;
    return false;
}

int HostileManager::collectBoxes(float out[][6], int maxN) {
    int n = 0;
    for (int i = 0; i < MAX_MOBS && n < maxN; i++) {
        if (!s_mobs[i].active) continue;
        const Mob& m = s_mobs[i];
        out[n][0] = (float)(m.x - m.width * 0.5); out[n][1] = (float)m.y; out[n][2] = (float)(m.z - m.width * 0.5);
        out[n][3] = (float)(m.x + m.width * 0.5); out[n][4] = (float)(m.y + m.height); out[n][5] = (float)(m.z + m.width * 0.5);
        n++;
    }
    return n;
}

// El jugador y los mobs se empujan entre si (Entity.applyEntityCollision, en los dos sentidos). (px, py, pz): los ojos del jugador. Devuelve el empuje
// que recibe el jugador en vx, vz (se suma a su velocidad). Llamar una vez por tick de fisica (20 Hz).
void HostileManager::pushWithPlayer(float px, float py, float pz, float& vx, float& vz) {
    vx = vz = 0.0f;
    if (!s_enabled) return;
    const double feet = (double)py - 1.62, top = feet + 1.8;
    for (int i = 0; i < MAX_MOBS; i++) {
        Mob& m = s_mobs[i];
        if (!m.active || m.hs.health <= 0) continue;
        const double hx = 0.3 + 0.2 + m.width * 0.5;
        if (fabs(m.x - px) >= hx || fabs(m.z - pz) >= hx) continue;
        if (m.y >= top || m.y + m.height <= feet) continue;
        double pvx = 0.0, pvz = 0.0;
        // el mob empuja al jugador y el jugador al mob (dos veces, una por cada onLivingUpdate)
        for (int r = 0; r < 2; r++) MobBodies::pushApart(m.x, m.z, (double)px, (double)pz, pvx, pvz, m.motionX, m.motionZ);
        vx += (float)pvx; vz += (float)pvz;
    }
}
