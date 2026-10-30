#include "PartialBlocks.h"
#include "AnimalManager.h"
#include "HostileManager.h"
#include "AnimalAI.h"
#include "AnimalModel.h"
#include "ChunkManager.h"
#include "Chunk.h"
#include "HandAnimations.h"
#include "ExplosionDamage.h"
#include <math.h>

namespace {
    Animal s_animals[AnimalManager::MAX_ANIMALS];
    AnimalAI::Rng s_rng;
    bool s_enabled = false;
    int s_riding = -1;                 // indice del cerdo que monta el jugador
    bool s_forcedPending = false; float s_forcedX = 0, s_forcedY = 0, s_forcedZ = 0;
    int s_spawnTimer = 0;

    const int DROP_QUEUE = 16;
    AnimalManager::Drop s_drops[DROP_QUEUE];
    int s_dropHead = 0, s_dropCount = 0;

    // El mundo tal como lo ve AnimalAI: celdas alineadas al mundo (x e z enteros; el bloque j cubre y de j-0,5 a j+0,5)
    struct CMWorld {
        const ChunkManager& cm;
        explicit CMWorld(const ChunkManager& c) : cm(c) {}
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
            if (!cacheReady) return BlockType::STONE;   // sin chunk, solido: no se cae al vacio
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

    void pushDrop(BlockType type, int count, double x, double y, double z) {
        if (count <= 0 || s_dropCount >= DROP_QUEUE) return;
        AnimalManager::Drop& d = s_drops[(s_dropHead + s_dropCount) % DROP_QUEUE];
        d.type = type; d.count = count; d.x = (float)x; d.y = (float)y; d.z = (float)z;
        ++s_dropCount;
    }

    // EntityLiving.dropFewItems: de 0 a 2 del objeto de cada animal (cuero, carne cruda de cerdo, plumas)
    void collectDeath(Animal& a) {
        if (!a.diedPending) return;
        a.diedPending = false;
        const BlockType t = a.kind == KIND_COW ? BlockType::LEATHER : (a.kind == KIND_PIG ? BlockType::PORK_RAW : BlockType::FEATHER);
        const BlockType drop = (t == BlockType::PORK_RAW && a.fire > 0) ? BlockType::PORK_COOKED : t;   // EntityPig.getDropItemId: cocida si murio ardiendo
        pushDrop(drop, s_rng.nextInt(3), a.x, a.y, a.z);
    }

    int countKind(int kind) {
        int n = 0;
        for (int i = 0; i < AnimalManager::MAX_ANIMALS; i++) if (s_animals[i].active && s_animals[i].kind == kind) ++n;
        return n;
    }

    void setup(Animal& a, int kind) {
        a.kind = kind;
        if (kind == KIND_COW)          { a.width = 0.9; a.height = 1.3; }
        else if (kind == KIND_PIG)     { a.width = 0.9; a.height = 0.9; }
        else { a.width = 0.3; a.height = 0.4; a.hs.health = 4; a.hs.prevHealth = 4; a.noFall = true; }   // EntityChicken: health = 4
    }
}

void AnimalManager::clear(unsigned int seed) {
    for (int i = 0; i < MAX_ANIMALS; i++) s_animals[i] = Animal();
    s_rng = AnimalAI::Rng(seed ^ 0x7F4A7C15u);
    s_spawnTimer = 200;               // el primero aparece a los 4 segundos
    s_dropHead = s_dropCount = 0;
    s_riding = -1; s_forcedPending = false;
}

void AnimalManager::explosionDamage(const ChunkManager& cm, double ex, double ey, double ez, float size) {
    CMWorld w(cm);
    for (int i = 0; i < MAX_ANIMALS; i++) {
        Animal& a = s_animals[i];
        if (!a.active || a.hs.health <= 0) continue;
        int dmg; double kx, ky, kz;
        if (!ExplosionDamage::compute(w, ex, ey, ez, size, a.x, a.y, a.z, a.width, a.height, dmg, kx, ky, kz)) continue;
        AnimalAI::damage(a, dmg, true, ex, ez);
        a.motionX += kx; a.motionY += ky; a.motionZ += kz;
        collectDeath(a);
    }
}

void AnimalManager::setActive(bool active) { s_enabled = active; }

int AnimalManager::count() {
    int n = 0;
    for (int i = 0; i < MAX_ANIMALS; i++) if (s_animals[i].active) ++n;
    return n;
}

void AnimalManager::tick(const ChunkManager& cm, float px, float py, float pz) {
    if (!s_enabled) return;
    CMWorld w(cm);
    const int pcx = AnimalAI::chunkOf(px), pcz = AnimalAI::chunkOf(pz);
    const AnimalAI::PlayerPos pl = { (double)px, (double)py, (double)pz, true };

    int alive = 0;
    for (int i = 0; i < MAX_ANIMALS; i++) {
        Animal& a = s_animals[i];
        if (!a.active) continue;
        if (!(fabs(a.x) < 1.0e6 && fabs(a.z) < 1.0e6 && a.y > -20.0 && a.y < 200.0)) { a.active = false; continue; }   // coordenadas rotas (NaN o fuera del mundo): ocupaban un cupo para siempre
        if (AnimalAI::shouldDespawn(a, w, pcx, pcz)) { a.active = false; continue; }   // fuera de los 9 chunks: se borra al instante
        AnimalAI::tick(a, w, s_rng, pl);
        if (a.active) Partial::mobWalked(cm, a);   // pisotea la tierra de cultivo
        collectDeath(a);
        if (a.eggPending) { a.eggPending = false; pushDrop(BlockType::EGG, 1, a.x, a.y + 0.2, a.z); }   // EntityChicken: dropItem(egg)
        if (a.active) ++alive;
    }

    // El cerdo montado murio o desaparecio: el jugador se baja encima de donde estaba
    if (s_riding >= 0) {
        const Animal& r = s_animals[s_riding];
        if (!r.active || r.hs.health <= 0) {
            s_forcedPending = true; s_forcedX = (float)r.x; s_forcedY = (float)(r.y + r.height + 1.62); s_forcedZ = (float)r.z;
            s_riding = -1;
        }
    }

    // Autorreparacion: si pasan 60 s sin un solo bicho, se renueva el generador aleatorio y el temporizador (lo mismo que hace entrar de nuevo al mundo)
    static int s_noneTicks = 0;
    if (alive > 0) s_noneTicks = 0;
    else if (++s_noneTicks >= 1200) { s_noneTicks = 0; s_rng = AnimalAI::Rng(s_rng.s * 2654435761u + 0x9E3779B9u); s_spawnTimer = 1; }
    if (alive < ANIMAL_SPAWN_CAP && --s_spawnTimer <= 0) {
        s_spawnTimer = 300 + s_rng.nextInt(200);   // un intento cada 15 a 25 segundos (en la Beta la poblacion crece despacio)
        // BiomeGenBase.spawnableCreatureList: se elige entre los tipos que todavia no llegaron a su tope, segun su peso (cerdo 10, gallina 10, vaca 8; la oveja
        // -peso 12- la maneja SheepManager)
        static const int WEIGHT[KIND_COUNT] = { 8, 10, 10 };
        int open[KIND_COUNT], n = 0, total = 0;
        for (int k = 0; k < KIND_COUNT; k++) if (countKind(k) < MAX_PER_KIND) { open[n++] = k; total += WEIGHT[k]; }
        double x, y, z;
        if (n > 0 && AnimalAI::findSpawn(w, s_rng, pcx, pcz, (double)px, (double)pz, x, y, z)) {
            int roll = s_rng.nextInt(total), kind = open[0];
            for (int q = 0; q < n; q++) { roll -= WEIGHT[open[q]]; if (roll < 0) { kind = open[q]; break; } }
            const double fx = x, fy = y, fz = z;
            // una manada de hasta 4 del mismo tipo (EntityLiving.getMaxSpawnedInChunk): el primero y 3 mas cerca, si caben
            for (int member = 0; member < 4; member++) {
                if (member > 0) {
                    if (countKind(kind) >= MAX_PER_KIND || !AnimalAI::packSpot(w, s_rng, (int)floor(fx), AnimalAI::cellY(fy), (int)floor(fz), (double)px, (double)pz, x, y, z)) break;
                }
                int slot = -1;
                for (int i = 0; i < MAX_ANIMALS; i++) if (!s_animals[i].active) { slot = i; break; }
                if (slot < 0) break;
                Animal& a = s_animals[slot];
                a = Animal();
                a.active = true;
                setup(a, kind);
                a.x = a.prevX = x; a.y = a.prevY = y; a.z = a.prevZ = z;
                a.yaw = a.prevYaw = a.bodyYaw = a.prevBodyYaw = s_rng.nextFloat() * 360.0f;
                a.eggTimer = s_rng.nextInt(6000) + 6000;     // EntityChicken: timeUntilNextEgg
            }
        }
    }
}

bool AnimalManager::spawnChicken(double x, double y, double z) {
    int slot = -1;
    for (int i = 0; i < MAX_ANIMALS; i++) if (!s_animals[i].active) { slot = i; break; }
    if (slot < 0) return false;
    Animal& a = s_animals[slot];
    a = Animal();
    a.active = true;
    setup(a, KIND_CHICKEN);
    a.x = a.prevX = x; a.y = a.prevY = y; a.z = a.prevZ = z;
    a.yaw = a.prevYaw = a.bodyYaw = a.prevBodyYaw = s_rng.nextFloat() * 360.0f;
    a.eggTimer = s_rng.nextInt(6000) + 6000;
    return true;
}

bool AnimalManager::popDrop(Drop& out) {
    if (s_dropCount == 0) return false;
    out = s_drops[s_dropHead];
    s_dropHead = (s_dropHead + 1) % DROP_QUEUE;
    --s_dropCount;
    return true;
}

void AnimalManager::draw(const ChunkManager& cm, float partialTick) {
    for (int i = 0; i < MAX_ANIMALS; i++) {
        const Animal& a = s_animals[i];
        if (!a.active) continue;
        AnimalModel::draw(a, partialTick, HandAnimations::lightLevelAt(cm, (float)a.x, (float)a.y + (float)(a.height * 0.5), (float)a.z));
    }
}

int AnimalManager::pick(float ox, float oy, float oz, float dx, float dy, float dz, float maxDist, float& hitDist) {
    int best = -1;
    double bestDist = (double)maxDist + 1.0;
    for (int i = 0; i < MAX_ANIMALS; i++) {
        const Animal& a = s_animals[i];
        if (!a.active || a.hs.health <= 0 || i == s_riding) continue;
        const double d = AnimalAI::rayHit(a, ox, oy, oz, dx, dy, dz, (double)maxDist);
        if (d >= 0.0 && d < bestDist) { bestDist = d; best = i; }
    }
    if (best >= 0) hitDist = (float)bestDist;
    return best;
}

bool AnimalManager::attack(int index, int damage, float attackerX, float attackerZ) {
    if (index < 0 || index >= MAX_ANIMALS || !s_animals[index].active) return false;
    Animal& a = s_animals[index];
    const bool hurt = AnimalAI::damage(a, damage, true, (double)attackerX, (double)attackerZ);
    collectDeath(a);
    return hurt;
}

void AnimalManager::lightning(double x, double y, double z) {
    for (int i = 0; i < MAX_ANIMALS; i++) {
        Animal& a = s_animals[i];
        if (!a.active || a.hs.health <= 0) continue;
        if (fabs(a.x - x) > 3.0 + a.width * 0.5 || fabs(a.z - z) > 3.0 + a.width * 0.5 || a.y < y - 3.0 - a.height || a.y > y + 9.0) continue;
        if (a.kind == KIND_PIG) {   // EntityPig.onStruckByLightning: desaparece y aparece un zombi cerdo en su lugar
            if (HostileManager::spawnPigZombie(a.x, a.y, a.z)) { a.active = false; continue; }
        }
        AnimalAI::damage(a, 5, false, 0.0, 0.0);
        if (a.fire <= 0) a.fire = 300;
        collectDeath(a);
    }
}

bool AnimalManager::anchor(int index, float& x, float& y, float& z) {
    if (index < 0 || index >= MAX_ANIMALS || !s_animals[index].active || s_animals[index].hs.health <= 0) return false;
    const Animal& a = s_animals[index];
    x = (float)a.x; y = (float)(a.y + a.height * 0.8); z = (float)a.z;
    return true;
}

void AnimalManager::pull(int index, float vx, float vy, float vz) {
    if (index < 0 || index >= MAX_ANIMALS || !s_animals[index].active) return;
    Animal& a = s_animals[index];
    a.motionX += vx; a.motionY += vy; a.motionZ += vz;
}

bool AnimalManager::saddle(int index) {
    if (index < 0 || index >= MAX_ANIMALS || !s_animals[index].active) return false;
    Animal& a = s_animals[index];
    if (a.kind != KIND_PIG || a.hs.health <= 0 || a.saddled) return false;
    a.saddled = true;
    return true;
}

bool AnimalManager::mount(int index) {
    if (s_riding >= 0 || index < 0 || index >= MAX_ANIMALS || !s_animals[index].active) return false;
    const Animal& a = s_animals[index];
    if (a.kind != KIND_PIG || !a.saddled || a.hs.health <= 0) return false;
    s_riding = index;
    return true;
}

bool AnimalManager::isRiding() { return s_riding >= 0 && s_animals[s_riding].active; }

bool AnimalManager::riderEye(float& x, float& y, float& z) {
    if (!isRiding()) return false;
    const Animal& a = s_animals[s_riding];
    x = (float)a.x; z = (float)a.z;
    y = (float)(a.y + a.height * 0.75 + 1.62 - 0.5);   // getMountedYOffset + (yOffset del jugador - 0,5)
    return true;
}

bool AnimalManager::dismount(float& x, float& y, float& z) {
    if (!isRiding()) return false;
    const Animal& a = s_animals[s_riding];
    x = (float)a.x; y = (float)(a.y + a.height + 1.62); z = (float)a.z;
    s_riding = -1;
    return true;
}

bool AnimalManager::takeForcedDismount(float& x, float& y, float& z) {
    if (!s_forcedPending) return false;
    s_forcedPending = false;
    x = s_forcedX; y = s_forcedY; z = s_forcedZ;
    return true;
}

bool AnimalManager::milk(int index) {
    if (index < 0 || index >= MAX_ANIMALS || !s_animals[index].active) return false;
    return s_animals[index].kind == KIND_COW && s_animals[index].hs.health > 0;
}

bool AnimalManager::overlapsBlock(float bx, float by, float bz) {
    for (int i = 0; i < MAX_ANIMALS; i++)
        if (s_animals[i].active && AnimalAI::overlapsBlock(s_animals[i], bx, by, bz)) return true;
    return false;
}

int AnimalManager::collectBodies(MobBody* out, int maxN) {
    int n = 0;
    for (int i = 0; i < MAX_ANIMALS && n < maxN; i++) {
        Animal& a = s_animals[i];
        if (!a.active || a.hs.health <= 0) continue;
        out[n].x = &a.x; out[n].y = &a.y; out[n].z = &a.z; out[n].mx = &a.motionX; out[n].mz = &a.motionZ; out[n].w = a.width; out[n].h = a.height;
        ++n;
    }
    return n;
}

int AnimalManager::collectBoxes(float out[][6], int maxN) {
    int n = 0;
    for (int i = 0; i < MAX_ANIMALS && n < maxN; i++) {
        if (!s_animals[i].active) continue;
        const Animal& a = s_animals[i];
        const double h = a.width * 0.5;
        out[n][0] = (float)(a.x - h); out[n][1] = (float)a.y; out[n][2] = (float)(a.z - h);
        out[n][3] = (float)(a.x + h); out[n][4] = (float)(a.y + a.height); out[n][5] = (float)(a.z + h);
        n++;
    }
    return n;
}
