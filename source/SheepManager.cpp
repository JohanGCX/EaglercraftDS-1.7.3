#include "PartialBlocks.h"
#include "SheepManager.h"
#include "ExplosionDamage.h"
#include "SheepAI.h"
#include "SheepModel.h"
#include "AnimalManager.h"
#include "AnimalAI.h"
#include "MobBody.h"
#include "HostileManager.h"
#include "ChunkManager.h"
#include "Chunk.h"
#include "HandAnimations.h"
#include <math.h>

namespace {
    Sheep s_sheep[SheepManager::MAX_SHEEP];
    SheepAI::Rng s_rng;
    bool s_enabled = false;
    int s_spawnTimer = 0;

    const int DROP_QUEUE = 8;   // (los animales llevan su propia cola)
    SheepManager::Drop s_drops[DROP_QUEUE];
    int s_dropHead = 0, s_dropCount = 0;

    // El mundo tal como lo ve SheepAI: celdas alineadas al mundo (x e z enteros; el bloque j cubre y de j-0,5 a j+0,5)
    struct CMWorld {
        const ChunkManager& cm;
        explicit CMWorld(const ChunkManager& c) : cm(c) {}
        // el A* consulta celdas vecinas una y otra vez: se recuerda el último chunk y se calcula con enteros (la celda i del mundo es la celda de
        // rejilla i + 4; cada chunk tiene 8 de ancho)
        mutable int cacheCx = 0x7FFFFFFF, cacheCz = 0x7FFFFFFF;
        mutable Chunk* cacheChunk = nullptr;
        mutable bool cacheReady = false;

        BlockType type(int i, int j, int k) const {
            if (j < -1) return BlockType::BEDROCK;
            if (j > 126) return BlockType::AIR;
            const int gx = i + 4, gz = k + 4;
            const int cx = gx >> 3, cz = gz >> 3;
            if (cx != cacheCx || cz != cacheCz) {
                cacheCx = cx; cacheCz = cz;
                cacheChunk = cm.getChunkByChunkCoord(cx, cz);
                cacheReady = cacheChunk && cacheChunk->isGenerationComplete();
            }
            if (!cacheReady) return BlockType::STONE;   // sin chunk, sólido: no se cae al vacío
            return cacheChunk->getBlockType(gx & 7, j + 1, gz & 7);
        }
        float light(int i, int j, int k) const {
            const unsigned char level = HandAnimations::lightLevelAt(cm, (float)i + 0.5f, (float)j, (float)k + 0.5f);
            const float f = 1.0f - (float)level / 15.0f;
            return (1.0f - f) / (f * 3.0f + 1.0f) * 0.95f + 0.05f;           // brillo como WorldProvider.generateLightBrightnessTable
        }
        bool chunkReady(int cx, int cz) const {
            Chunk* c = cm.getChunkByChunkCoord(cx, cz);
            return c && c->isGenerationComplete();
        }
    };

    // Entity.applyEntityCollision entre todos los mobs a la vez: ovejas, vacas, cerdos, gallinas y los de HostileManager
    const int MAX_BODIES = SheepManager::HOSTILE_BASE + HostileManager::MAX_MOBS;
    int gatherBodies(MobBody* bodies) {
        int n = 0;
        for (int i = 0; i < SheepManager::MAX_SHEEP; i++) {
            Sheep& s = s_sheep[i];
            if (!s.active || s.hs.health <= 0) continue;
            bodies[n].x = &s.x; bodies[n].y = &s.y; bodies[n].z = &s.z; bodies[n].mx = &s.motionX; bodies[n].mz = &s.motionZ;
            bodies[n].w = SheepAI::WIDTH; bodies[n].h = SheepAI::HEIGHT;
            ++n;
        }
        n += AnimalManager::collectBodies(bodies + n, MAX_BODIES - n);
        n += HostileManager::collectBodies(bodies + n, MAX_BODIES - n);
        return n;
    }
    void collideAll() {
        MobBody bodies[MAX_BODIES];
        MobBodies::collide(bodies, gatherBodies(bodies));
    }

    void pushDrop(BlockType type, int count, double x, double y, double z, bool shear = false) {
        if (s_dropCount >= DROP_QUEUE) return;
        SheepManager::Drop& d = s_drops[(s_dropHead + s_dropCount) % DROP_QUEUE];
        d.type = type; d.count = count; d.x = (float)x; d.y = (float)y; d.z = (float)z; d.shear = shear;
        ++s_dropCount;
    }

    // EntitySheep.dropFewItems: una lana del color de la oveja, si no está esquilada
    void collectDeath(Sheep& s) {
        if (!s.diedPending) return;
        s.diedPending = false;
        if (!s.sheared) pushDrop(woolType(s.fleeceColor), 1, s.x, s.y, s.z);
    }
}

void SheepManager::clear(unsigned int seed) {
    for (int i = 0; i < MAX_SHEEP; i++) s_sheep[i] = Sheep();
    s_rng = SheepAI::Rng(seed ^ 0x9E3779B9u);
    s_spawnTimer = 260;              // la primera aparece a los 2 segundos
    s_dropHead = s_dropCount = 0;
    AnimalManager::clear(seed);
    HostileManager::clear(seed);
}

void SheepManager::explosionDamage(const ChunkManager& cm, double ex, double ey, double ez, float size) {
    AnimalManager::explosionDamage(cm, ex, ey, ez, size);
    CMWorld w(cm);
    for (int i = 0; i < MAX_SHEEP; i++) {
        Sheep& s = s_sheep[i];
        if (!s.active || s.hs.health <= 0) continue;
        int dmg; double kx, ky, kz;
        if (!ExplosionDamage::compute(w, ex, ey, ez, size, s.x, s.y, s.z, SheepAI::WIDTH, SheepAI::HEIGHT, dmg, kx, ky, kz)) continue;
        SheepAI::damage(s, dmg, true, ex, ez);
        s.motionX += kx; s.motionY += ky; s.motionZ += kz;
        collectDeath(s);
    }
}

void SheepManager::setActive(bool active) { HostileManager::setExplosionListener(&SheepManager::explosionDamage); s_enabled = active; AnimalManager::setActive(active); HostileManager::setNether(!active); HostileManager::setActive(true); }   // los hostiles van en las dos dimensiones

int SheepManager::count() {
    int n = 0;
    for (int i = 0; i < MAX_SHEEP; i++) if (s_sheep[i].active) ++n;
    return n + AnimalManager::count() + HostileManager::count();
}

void SheepManager::tick(const ChunkManager& cm, float px, float py, float pz, bool playerAlive) {
    HostileManager::tick(cm, px, py, pz, playerAlive);   // los hostiles corren en las dos dimensiones
    if (!s_enabled) { collideAll(); return; }
    AnimalManager::tick(cm, px, py, pz);
    CMWorld w(cm);
    const int pcx = SheepAI::chunkOf(px), pcz = SheepAI::chunkOf(pz);
    const SheepAI::PlayerPos pl = { (double)px, (double)py, (double)pz, true };

    int alive = 0;
    for (int i = 0; i < MAX_SHEEP; i++) {
        Sheep& s = s_sheep[i];
        if (!s.active) continue;
        if (!(fabs(s.x) < 1.0e6 && fabs(s.z) < 1.0e6 && s.y > -20.0 && s.y < 200.0)) { s.active = false; continue; }   // coordenadas rotas (NaN o fuera del mundo): ocupaban un cupo para siempre
        if (SheepAI::shouldDespawn(s, w, pcx, pcz)) { s.active = false; continue; }   // fuera de los 9 chunks: se borra al instante
        SheepAI::tick(s, w, s_rng, pl);
        if (s.active) Partial::mobWalked(cm, s);   // pisotea la tierra de cultivo
        collectDeath(s);
        if (s.active) ++alive;
    }

    // Autorreparacion: si pasan 60 s sin un solo bicho, se renueva el generador aleatorio y el temporizador (lo mismo que hace entrar de nuevo al mundo)
    static int s_noneTicks = 0;
    if (alive > 0) s_noneTicks = 0;
    else if (++s_noneTicks >= 1200) { s_noneTicks = 0; s_rng = SheepAI::Rng(s_rng.s * 2654435761u + 0x9E3779B9u); s_spawnTimer = 1; }
    if (alive < MAX_SHEEP && --s_spawnTimer <= 0) {
        s_spawnTimer = 300 + s_rng.nextInt(200);   // un intento cada 15 a 25 segundos
        double x, y, z;
        if (SheepAI::findSpawn(w, s_rng, pcx, pcz, (double)px, (double)pz, x, y, z)) {
            const double fx = x, fy = y, fz = z;
            for (int member = 0; member < 4; member++) {   // una manada de hasta 4 (getMaxSpawnedInChunk); caben las que dejen los 2 cupos
                if (member > 0 && !AnimalAI::packSpot(w, s_rng, (int)floor(fx), AnimalAI::cellY(fy), (int)floor(fz), (double)px, (double)pz, x, y, z)) break;
                int slot = -1;
                for (int i = 0; i < MAX_SHEEP; i++) if (!s_sheep[i].active) { slot = i; break; }
                if (slot < 0) break;
                Sheep& s = s_sheep[slot];
                s = Sheep();
                s.active = true;
                s.x = s.prevX = x; s.y = s.prevY = y; s.z = s.prevZ = z;
                s.yaw = s.prevYaw = s.bodyYaw = s.prevBodyYaw = s_rng.nextFloat() * 360.0f;
                s.fleeceColor = SheepAI::randomFleeceColor(s_rng);
            }
        }
    }
    collideAll();
}

bool SheepManager::popDrop(Drop& out) {
    if (s_dropCount == 0) {
        AnimalManager::Drop ad;
        if (AnimalManager::popDrop(ad)) {
            out.type = ad.type; out.count = ad.count; out.x = ad.x; out.y = ad.y; out.z = ad.z; out.shear = false;
            return true;
        }
        HostileManager::Drop hd;
        if (!HostileManager::popDrop(hd)) return false;
        out.type = hd.type; out.count = hd.count; out.x = hd.x; out.y = hd.y; out.z = hd.z; out.shear = false;
        return true;
    }
    out = s_drops[s_dropHead];
    s_dropHead = (s_dropHead + 1) % DROP_QUEUE;
    --s_dropCount;
    return true;
}

void SheepManager::setCamera(float yaw, float pitch) { HostileManager::setCamera(yaw, pitch); }

void SheepManager::draw(const ChunkManager& cm, float partialTick) {
    AnimalManager::draw(cm, partialTick);
    HostileManager::draw(cm, partialTick);
    for (int i = 0; i < MAX_SHEEP; i++) {
        const Sheep& s = s_sheep[i];
        if (!s.active) continue;
        SheepModel::draw(s, partialTick, HandAnimations::lightLevelAt(cm, (float)s.x, (float)s.y + 0.9f, (float)s.z));
    }
}

int SheepManager::pick(float ox, float oy, float oz, float dx, float dy, float dz, float maxDist, float& hitDist) {
    int best = -1;
    double bestDist = (double)maxDist + 1.0;
    for (int i = 0; i < MAX_SHEEP; i++) {
        const Sheep& s = s_sheep[i];
        if (!s.active || s.hs.health <= 0) continue;
        const double d = SheepAI::rayHit(s, ox, oy, oz, dx, dy, dz, (double)maxDist);
        if (d >= 0.0 && d < bestDist) { bestDist = d; best = i; }
    }
    if (best >= 0) hitDist = (float)bestDist;
    float ad = 0.0f;
    const int ai = AnimalManager::pick(ox, oy, oz, dx, dy, dz, maxDist, ad);
    if (ai >= 0 && (best < 0 || (double)ad < bestDist)) { hitDist = ad; bestDist = (double)ad; best = ANIMAL_BASE + ai; }
    float hd = 0.0f;
    const int hi = HostileManager::pick(ox, oy, oz, dx, dy, dz, maxDist, hd);
    if (hi >= 0 && (best < 0 || (double)hd < bestDist)) { hitDist = hd; return HOSTILE_BASE + hi; }
    return best;
}

bool SheepManager::attack(int index, int damage, float attackerX, float attackerZ) {
    if (index >= HOSTILE_BASE) return HostileManager::attack(index - HOSTILE_BASE, damage, attackerX, attackerZ);
    if (index >= ANIMAL_BASE) return AnimalManager::attack(index - ANIMAL_BASE, damage, attackerX, attackerZ);
    if (index < 0 || index >= MAX_SHEEP || !s_sheep[index].active) return false;
    Sheep& s = s_sheep[index];
    const bool hurt = SheepAI::damage(s, damage, true, (double)attackerX, (double)attackerZ);
    collectDeath(s);
    return hurt;
}

void SheepManager::lightning(double x, double y, double z) {
    AnimalManager::lightning(x, y, z);
    HostileManager::lightning(x, y, z);
    for (int i = 0; i < MAX_SHEEP; i++) {
        Sheep& s = s_sheep[i];
        if (!s.active || s.hs.health <= 0) continue;
        if (fabs(s.x - x) > 3.0 + SheepAI::WIDTH * 0.5 || fabs(s.z - z) > 3.0 + SheepAI::WIDTH * 0.5 || s.y < y - 3.0 - SheepAI::HEIGHT || s.y > y + 9.0) continue;
        SheepAI::damage(s, 5, false, 0.0, 0.0);
        if (s.fire <= 0) s.fire = 300;
        collectDeath(s);
    }
}

bool SheepManager::anchor(int index, float& x, float& y, float& z) {
    if (index >= HOSTILE_BASE) return HostileManager::anchor(index - HOSTILE_BASE, x, y, z);
    if (index >= ANIMAL_BASE) return AnimalManager::anchor(index - ANIMAL_BASE, x, y, z);
    if (index < 0 || index >= MAX_SHEEP || !s_sheep[index].active || s_sheep[index].hs.health <= 0) return false;
    const Sheep& s = s_sheep[index];
    x = (float)s.x; y = (float)(s.y + SheepAI::HEIGHT * 0.8); z = (float)s.z;
    return true;
}

void SheepManager::pull(int index, float vx, float vy, float vz) {
    if (index >= HOSTILE_BASE) { HostileManager::pull(index - HOSTILE_BASE, vx, vy, vz); return; }
    if (index >= ANIMAL_BASE) { AnimalManager::pull(index - ANIMAL_BASE, vx, vy, vz); return; }
    if (index < 0 || index >= MAX_SHEEP || !s_sheep[index].active) return;
    Sheep& s = s_sheep[index];
    s.motionX += vx; s.motionY += vy; s.motionZ += vz;
}

bool SheepManager::overlapsBlock(float bx, float by, float bz) {
    if (AnimalManager::overlapsBlock(bx, by, bz) || HostileManager::overlapsBlock(bx, by, bz)) return true;
    for (int i = 0; i < MAX_SHEEP; i++)
        if (s_sheep[i].active && SheepAI::overlapsBlock(s_sheep[i], bx, by, bz)) return true;
    return false;
}

int SheepManager::collectBoxes(float out[][6], int maxN) {
    int n = 0;
    for (int i = 0; i < MAX_SHEEP && n < maxN; i++) {
        if (!s_sheep[i].active) continue;
        const Sheep& s = s_sheep[i];
        out[n][0] = (float)(s.x - SheepAI::HALF); out[n][1] = (float)s.y; out[n][2] = (float)(s.z - SheepAI::HALF);
        out[n][3] = (float)(s.x + SheepAI::HALF); out[n][4] = (float)(s.y + SheepAI::HEIGHT); out[n][5] = (float)(s.z + SheepAI::HALF);
        n++;
    }
    if (n < maxN) n += AnimalManager::collectBoxes(out + n, maxN - n);
    if (n < maxN) n += HostileManager::collectBoxes(out + n, maxN - n);
    return n;
}

// El jugador y todos los mobs se empujan entre si (Entity.applyEntityCollision, en los dos sentidos). (px, py, pz): los ojos del jugador. Devuelve en vx, vz el empuje que
// recibe el jugador. Una vez por tick de fisica.
void SheepManager::pushWithPlayer(float px, float py, float pz, float& vx, float& vz) {
    vx = vz = 0.0f;
    MobBody bodies[MAX_BODIES];
    const int n = gatherBodies(bodies);
    const double feet = (double)py - 1.62, top = feet + 1.8;
    for (int i = 0; i < n; i++) {
        const MobBody& b = bodies[i];
        const double hx = 0.3 + 0.2 + b.w * 0.5;
        if (fabs(*b.x - px) >= hx || fabs(*b.z - pz) >= hx) continue;
        if (*b.y >= top || *b.y + b.h <= feet) continue;
        double pvx = 0.0, pvz = 0.0;
        for (int r = 0; r < 2; r++) MobBodies::pushApart(*b.x, *b.z, (double)px, (double)pz, pvx, pvz, *b.mx, *b.mz);   // el mob empuja al jugador y el jugador al mob (uno por cada onLivingUpdate)
        vx += (float)pvx; vz += (float)pvz;
    }
}

int SheepManager::interact(int index, BlockType held) {
    if (index < HOSTILE_BASE) return 0;
    const int h = held == BlockType::BONE ? 1 : ((held == BlockType::PORK_RAW || held == BlockType::PORK_COOKED) ? 2 : 0);
    return HostileManager::interact(index - HOSTILE_BASE, h);
}

void SheepManager::setHeld(BlockType held) {
    HostileManager::setHeldKind(held == BlockType::BONE ? 1 : ((held == BlockType::PORK_RAW || held == BlockType::PORK_COOKED) ? 2 : 0));
}

bool SheepManager::saddle(int index) {
    return index >= ANIMAL_BASE && index < HOSTILE_BASE && AnimalManager::saddle(index - ANIMAL_BASE);
}

bool SheepManager::mount(int index) {
    return index >= ANIMAL_BASE && index < HOSTILE_BASE && AnimalManager::mount(index - ANIMAL_BASE);
}

bool SheepManager::milk(int index) {
    return index >= ANIMAL_BASE && index < HOSTILE_BASE && AnimalManager::milk(index - ANIMAL_BASE);
}

bool SheepManager::dye(int index, int fleeceColor) {
    if (index < 0 || index >= MAX_SHEEP || !s_sheep[index].active) return false;
    Sheep& s = s_sheep[index];
    if (s.sheared || s.hs.health <= 0 || s.fleeceColor == fleeceColor) return false;
    s.fleeceColor = fleeceColor & 15;
    return true;
}

// EntitySheep.interact: con tijeras en la mano y la oveja con lana: queda esquilada y suelta 2 + nextInt(3) lanas de su color (cada una por separado,
// a un bloque de altura). La oveja esquilada no recupera la lana (el crecimiento al comer pasto es de una versión posterior).
bool SheepManager::shear(int index) {
    if (index < 0 || index >= MAX_SHEEP || !s_sheep[index].active) return false;
    Sheep& s = s_sheep[index];
    if (s.sheared || s.hs.health <= 0) return false;
    s.sheared = true;
    const int n = 2 + s_rng.nextInt(3);
    for (int i = 0; i < n; i++) pushDrop(woolType(s.fleeceColor), 1, s.x, s.y + 1.0, s.z, true);
    return true;
}