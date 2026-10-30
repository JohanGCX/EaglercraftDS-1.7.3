#include "Dispenser.h"
#include "ChestStore.h"
#include "Chunk.h"
#include "ChunkManager.h"
#include "WorldConfig.h"
#include "Projectiles.h"
#include <math.h>
#include <stdlib.h>

namespace {
    const int QUEUE = 16;
    Dispenser::Drop s_queue[QUEUE];
    int s_head = 0, s_count = 0;

    BlockType typeAtGrid(const ChunkManager& cm, int gx, int gy, int gz) {
        if (gy < 0 || gy >= WorldConfig::SIZE_Y) return BlockType::AIR;
        Chunk* c = cm.getChunkByChunkCoord(gx >> 3, gz >> 3);
        if (!c || !c->isGenerationComplete()) return BlockType::AIR;
        return c->getBlockType(gx & 7, gy, gz & 7);
    }

    float gauss() {   // aproximacion de nextGaussian: suma de 4 uniformes (media 0, desviacion ~1)
        float s = 0.0f;
        for (int i = 0; i < 4; i++) s += (float)(rand() % 2001) / 1000.0f - 1.0f;
        return s * 0.866f;
    }
}

void Dispenser::dispense(const ChunkManager& cm, int gx, int gy, int gz) {
    const BlockType t = typeAtGrid(cm, gx, gy, gz);
    if (!isDispenserBlock(t)) return;
    ChestData* cd = ChestStore::find(gx, gy, gz);
    if (!cd) return;

    // TileEntityDispenser.getRandomStackFromInventory: una casilla ocupada al azar (todas con la misma probabilidad)
    int pick = -1, seen = 1;
    for (int i = 0; i < SLOTS; i++) {
        if (cd->slot[i].count == 0) continue;
        if (rand() % seen++ == 0) pick = i;
    }
    if (pick < 0 || s_count >= QUEUE) return;   // vacio: Beta solo hace el "clic" (sonido 1001)

    ChestSlot& s = cd->slot[pick];
    const BlockType item = (BlockType)s.type;
    const int durability = isTool(item) ? (int)s.durability : -1;

    // frente: 0 sur (+Z), 1 norte (-Z), 2 oeste (-X), 3 este (+X)
    float dx = 0.0f, dz = 0.0f;
    switch (dispenserFrontDir(t)) { case 0: dz = 1.0f; break; case 1: dz = -1.0f; break; case 2: dx = -1.0f; break; default: dx = 1.0f; break; }

    // BlockDispenser.dispenseItem: la flecha, el huevo y la bola de nieve salen como proyectiles (velocidad 1,1, desvio 6, un poco hacia arriba)
    if (item == BlockType::ARROW || item == BlockType::EGG || item == BlockType::SNOWBALL) {
        const int kind = item == BlockType::ARROW ? Projectiles::ARROW : (item == BlockType::EGG ? Projectiles::EGG : Projectiles::SNOWBALL);
        const double cx = (double)(gx - WorldConfig::SIZE_X / 2) + 0.5, cz = (double)(gz - WorldConfig::SIZE_Z / 2) + 0.5;
        if (!Projectiles::spawn(kind, cx + dx * 0.6, (double)(gy - 1), cz + dz * 0.6, dx, 0.1, dz, 1.1f, 6.0f, true, cx, cz)) return;   // sin lugar: no se gasta
        if (--s.count == 0) { s.type = 0; s.durability = 0; }
        return;
    }
    if (--s.count == 0) { s.type = 0; s.durability = 0; }

    Drop& d = s_queue[(s_head + s_count) % QUEUE];
    ++s_count;
    d.x = (float)(gx - WorldConfig::SIZE_X / 2) + dx * 0.6f + 0.5f;
    d.y = (float)(gy - 1) - 0.3f;
    d.z = (float)(gz - WorldConfig::SIZE_Z / 2) + dz * 0.6f + 0.5f;
    const float speed = (float)(rand() % 1000) / 10000.0f + 0.2f;
    d.vx = dx * speed + gauss() * 0.045f;
    d.vy = 0.2f + gauss() * 0.045f;
    d.vz = dz * speed + gauss() * 0.045f;
    d.type = item;
    d.durability = durability;
}

bool Dispenser::popDrop(Drop& out) {
    if (s_count == 0) return false;
    out = s_queue[s_head];
    s_head = (s_head + 1) % QUEUE;
    --s_count;
    return true;
}

void Dispenser::clearQueue() { s_head = s_count = 0; }
