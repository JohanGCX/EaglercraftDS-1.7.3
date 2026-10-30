#include "MinecartManager.h"
#include "MinecartAI.h"
#include "MinecartModel.h"
#include "ChunkManager.h"
#include "Chunk.h"
#include "ChestStore.h"
#include "HandAnimations.h"
#include "WorldConfig.h"
#include <math.h>
#include <stdio.h>

namespace {
    Minecart s_carts[MinecartManager::MAX_CARTS];
    int s_nextUid = 1;
    int s_riding = -1;
    unsigned int s_rng = 12345u;

    bool s_forcedPending = false;
    float s_forcedX = 0.0f, s_forcedY = 0.0f, s_forcedZ = 0.0f;

    const int DROP_QUEUE = 48;
    MinecartManager::Drop s_drops[DROP_QUEUE];
    int s_dropHead = 0, s_dropCount = 0;

    const double OX = 4.0, OY = 1.5, OZ = 4.0;   // mundo = rejilla - (4, 1,5, 4)

    int nextInt(int n) { s_rng = s_rng * 1664525u + 1013904223u; return (int)((s_rng >> 16) % (unsigned)n); }

    // El mundo tal como lo ve MinecartAI: celdas de rejilla (gx, gy, gz)
    struct CartWorld {
        const ChunkManager& cm;
        explicit CartWorld(const ChunkManager& c) : cm(c) {}
        mutable int cacheCx = 0x7FFFFFFF, cacheCz = 0x7FFFFFFF;
        mutable Chunk* cacheChunk = nullptr;
        mutable bool cacheReady = false;

        Chunk* chunkFor(int gx, int gz) const {
            const int cx = gx >> 3, cz = gz >> 3;
            if (cx != cacheCx || cz != cacheCz) {
                cacheCx = cx; cacheCz = cz;
                cacheChunk = cm.getChunkByChunkCoord(cx, cz);
                cacheReady = cacheChunk && cacheChunk->isGenerationComplete();
            }
            return cacheReady ? cacheChunk : nullptr;
        }
        BlockType type(int x, int y, int z) const {
            if (y < 0) return BlockType::BEDROCK;
            if (y >= WorldConfig::SIZE_Y) return BlockType::AIR;
            Chunk* c = chunkFor(x, z);
            if (!c) return BlockType::STONE;   // sin chunk, solido
            return c->getBlockType(x & 7, y, z & 7);
        }
        int meta(int x, int y, int z) const {
            if (y < 0 || y >= WorldConfig::SIZE_Y) return 0;
            Chunk* c = chunkFor(x, z);
            return c ? c->getLiquidMeta(x & 7, y, z & 7) : 0;
        }
        bool normal(int x, int y, int z) const { return isNormalCube(type(x, y, z)); }
        bool solid(int x, int y, int z) const { return isMaterialSolid(type(x, y, z)); }
        bool ready(int x, int z) const { return chunkFor(x, z) != nullptr; }
    };

    void pushDrop(BlockType type, int count, int durability, double gx, double gy, double gz) {
        if (s_dropCount >= DROP_QUEUE) return;
        MinecartManager::Drop& d = s_drops[(s_dropHead + s_dropCount) % DROP_QUEUE];
        d.type = type; d.count = count; d.durability = durability;
        d.x = (float)(gx - OX); d.y = (float)(gy - OY); d.z = (float)(gz - OZ);
        ++s_dropCount;
    }

    void eyeOf(const Minecart& c, float& x, float& y, float& z) {   // getMountedYOffset (-0,3) + la altura del jugador sentado (1,62 - 0,5)
        x = (float)(c.x - OX); y = (float)(c.y + 0.82 - OY); z = (float)(c.z - OZ);
    }

    void dismountIndex(int i, float& ex, float& ey, float& ez) {
        const Minecart& c = s_carts[i];
        ex = (float)(c.x - OX);
        ey = (float)((c.y - MinecartAI::Y_OFFSET) + MinecartAI::HEIGHT + 1.62 - OY);   // sobre la vagoneta: Entity.mountEntity(null)
        ez = (float)(c.z - OZ);
        s_carts[i].ridden = false;
        s_riding = -1;
    }

    // EntityMinecart.attackEntityFrom (al pasar de 40) + setEntityDead: la vagoneta se rompe y suelta lo suyo
    void breakCart(int i) {
        Minecart& c = s_carts[i];
        if (s_riding == i) {
            float ex, ey, ez;
            dismountIndex(i, ex, ey, ez);
            s_forcedPending = true; s_forcedX = ex; s_forcedY = ey; s_forcedZ = ez;
        }
        pushDrop(BlockType::MINECART, 1, -1, c.x, c.y, c.z);
        if (c.type == 1) {
            ChestData* cd = ChestStore::find(c.uid, MinecartManager::CHEST_KEY_Y, 0);
            if (cd) {
                for (int s = 0; s < ChestStore::SLOTS; s++) {
                    if (cd->slot[s].count > 0) pushDrop((BlockType)cd->slot[s].type, cd->slot[s].count, cd->slot[s].durability, c.x, c.y + 0.3, c.z);
                }
                ChestStore::remove(c.uid, MinecartManager::CHEST_KEY_Y, 0);
            }
            pushDrop(BlockType::CHEST, 1, -1, c.x, c.y, c.z);
        } else if (c.type == 2) {
            pushDrop(BlockType::FURNACE, 1, -1, c.x, c.y, c.z);
        }
        c.active = false;
    }

    // EntityMinecart.applyEntityCollision: 'self' es la que recibe el choque, 'other' quien choca
    void cartCollision(Minecart& self, Minecart& other) {
        double dx = other.x - self.x, dz = other.z - self.z;
        double d2 = dx * dx + dz * dz;
        if (d2 < 1.0e-4) return;
        const double d = sqrt(d2);
        dx /= d; dz /= d;
        double k = 1.0 / d;
        if (k > 1.0) k = 1.0;
        dx *= k; dz *= k;
        dx *= 0.1; dz *= 0.1;
        dx *= 0.5; dz *= 0.5;
        const double a = other.x - self.x, b = other.z - self.z;
        double t = a * other.motionZ + b * other.prevX;   // (sic) asi esta en el original: usa prevPosX
        t *= t;
        if (t > 5.0) return;
        double mx = other.motionX + self.motionX, mz = other.motionZ + self.motionZ;
        if (other.type == 2 && self.type != 2) {
            self.motionX *= 0.2; self.motionZ *= 0.2;
            self.motionX += other.motionX - dx; self.motionZ += other.motionZ - dz;
            other.motionX *= 0.7; other.motionZ *= 0.7;
        } else if (other.type != 2 && self.type == 2) {
            other.motionX *= 0.2; other.motionZ *= 0.2;
            other.motionX += self.motionX + dx; other.motionZ += self.motionZ + dz;
            self.motionX *= 0.7; self.motionZ *= 0.7;
        } else {
            mx /= 2.0; mz /= 2.0;
            self.motionX *= 0.2; self.motionZ *= 0.2;
            self.motionX += mx - dx; self.motionZ += mz - dz;
            other.motionX *= 0.2; other.motionZ *= 0.2;
            other.motionX += mx + dx; other.motionZ += mz + dz;
        }
    }
}

void MinecartManager::clear() {
    for (int i = 0; i < MAX_CARTS; i++) s_carts[i] = Minecart();
    s_nextUid = 1;
    s_riding = -1;
    s_forcedPending = false;
    s_dropHead = s_dropCount = 0;
}

int MinecartManager::count() {
    int n = 0;
    for (int i = 0; i < MAX_CARTS; i++) if (s_carts[i].active) ++n;
    return n;
}

bool MinecartManager::spawn(const ChunkManager& cm, int gx, int gy, int gz, int type) {
    CartWorld w(cm);
    if (!isRailBlock(w.type(gx, gy, gz))) return false;
    for (int i = 0; i < MAX_CARTS; i++) {
        if (s_carts[i].active) continue;
        Minecart& c = s_carts[i];
        c = Minecart();
        c.active = true;
        c.uid = s_nextUid++;
        c.type = type;
        MinecartAI::setPosition(c, (double)gx + 0.5, (double)gy + 0.5 + MinecartAI::Y_OFFSET, (double)gz + 0.5);
        c.prevX = c.x; c.prevY = c.y; c.prevZ = c.z;
        return true;
    }
    return false;
}

void MinecartManager::tick(const ChunkManager& cm, float px, float py, float pz) {
    CartWorld w(cm);
    for (int i = 0; i < MAX_CARTS; i++) {
        Minecart& c = s_carts[i];
        if (!c.active) continue;
        if (!w.ready(MinecartAI::fl(c.x), MinecartAI::fl(c.z))) { c.prevX = c.x; c.prevY = c.y; c.prevZ = c.z; continue; }   // su chunk no esta: queda quieta

        MinecartAI::tick(c, w, nextInt(4));
        if (c.y < -32.0) {   // cayo al vacio
            if (s_riding == i) { float ex, ey, ez; dismountIndex(i, ex, ey, ez); s_forcedPending = true; s_forcedX = ex; s_forcedY = ey; s_forcedZ = ez; }
            c.active = false;
            continue;
        }

        // choques con otras vagonetas (ancho + 0,2 a cada lado)
        const MinecartAI::Box a = MinecartAI::bodyBox(c);
        for (int j = 0; j < MAX_CARTS; j++) {
            if (j == i || !s_carts[j].active) continue;
            const MinecartAI::Box b = MinecartAI::bodyBox(s_carts[j]);
            if (b.maxX > a.minX - 0.2 && b.minX < a.maxX + 0.2 && b.maxZ > a.minZ - 0.2 && b.minZ < a.maxZ + 0.2 && b.maxY > a.minY && b.minY < a.maxY)
                cartCollision(s_carts[j], c);
        }

        // el jugador empuja la vagoneta al chocar con ella (Entity.applyEntityCollision desde EntityLiving.onLivingUpdate)
        if (s_riding != i) {
            const double ex = (double)px + OX, ez = (double)pz + OZ, feet = (double)py - 1.62 + OY;
            if (feet < a.maxY && feet + 1.8 > a.minY && ex + 0.5 > a.minX && ex - 0.5 < a.maxX && ez + 0.5 > a.minZ && ez - 0.5 < a.maxZ) {
                double dx = ex - c.x, dz = ez - c.z;
                double d2 = dx * dx + dz * dz;
                if (d2 >= 1.0e-4) {
                    const double d = sqrt(d2);
                    dx /= d; dz /= d;
                    double k = 1.0 / d;
                    if (k > 1.0) k = 1.0;
                    dx *= k * 0.1 * 0.5; dz *= k * 0.1 * 0.5;
                    c.motionX -= dx; c.motionZ -= dz;
                }
            }
        }
    }
}

bool MinecartManager::popDrop(Drop& out) {
    if (s_dropCount == 0) return false;
    out = s_drops[s_dropHead];
    s_dropHead = (s_dropHead + 1) % DROP_QUEUE;
    --s_dropCount;
    return true;
}

void MinecartManager::draw(const ChunkManager& cm, float partialTick) {
    CartWorld w(cm);
    for (int i = 0; i < MAX_CARTS; i++) {
        const Minecart& c = s_carts[i];
        if (!c.active) continue;
        const MinecartAI::Pose p = MinecartAI::renderPose(c, w, partialTick);
        MinecartModel::draw(c, p, partialTick, HandAnimations::lightLevelAt(cm, (float)(p.x - OX), (float)(p.y - OY) + 0.3f, (float)(p.z - OZ)));
    }
}

bool MinecartManager::isRiding() { return s_riding >= 0 && s_carts[s_riding].active; }

bool MinecartManager::riderEye(float& x, float& y, float& z) {
    if (!isRiding()) return false;
    eyeOf(s_carts[s_riding], x, y, z);
    return true;
}

bool MinecartManager::dismount(float& ex, float& ey, float& ez) {
    if (!isRiding()) return false;
    dismountIndex(s_riding, ex, ey, ez);
    return true;
}

bool MinecartManager::takeForcedDismount(float& ex, float& ey, float& ez) {
    if (!s_forcedPending) return false;
    s_forcedPending = false;
    ex = s_forcedX; ey = s_forcedY; ez = s_forcedZ;
    return true;
}

int MinecartManager::pick(float ox, float oy, float oz, float dx, float dy, float dz, float maxDist, float& hitDist) {
    int best = -1;
    double bestDist = (double)maxDist + 1.0;
    for (int i = 0; i < MAX_CARTS; i++) {
        const Minecart& c = s_carts[i];
        if (!c.active || i == s_riding) continue;
        const double d = MinecartAI::rayHit(c, (double)ox + OX, (double)oy + OY, (double)oz + OZ, (double)dx, (double)dy, (double)dz, (double)maxDist);
        if (d >= 0.0 && d < bestDist) { bestDist = d; best = i; }
    }
    if (best >= 0) hitDist = (float)bestDist;
    return best;
}

void MinecartManager::attack(int index, int damage) {
    if (index < 0 || index >= MAX_CARTS || !s_carts[index].active) return;
    Minecart& c = s_carts[index];
    c.rockDir = -c.rockDir;
    c.timeSinceHit = 10;
    c.damage += damage * 10;
    if (c.damage > 40) breakCart(index);
}

int MinecartManager::interact(int index, bool holdingCoal, float playerX, float playerZ) {
    if (index < 0 || index >= MAX_CARTS || !s_carts[index].active) return 0;
    Minecart& c = s_carts[index];
    if (c.type == 0) {
        if (c.ridden) return 0;
        c.ridden = true;
        s_riding = index;
        return INTERACT_MOUNTED;
    }
    if (c.type == 1) return INTERACT_CHEST;
    int r = 0;
    if (holdingCoal) { c.fuel += 1200; r |= INTERACT_COAL_USED; }
    c.pushX = c.x - ((double)playerX + OX);
    c.pushZ = c.z - ((double)playerZ + OZ);
    return r;
}

int MinecartManager::chestUid(int index) {
    if (index < 0 || index >= MAX_CARTS || !s_carts[index].active) return -1;
    return s_carts[index].uid;
}

int MinecartManager::collectBoxes(float out[][6], int maxN) {
    int n = 0;
    for (int i = 0; i < MAX_CARTS && n < maxN; i++) {
        if (!s_carts[i].active) continue;
        const MinecartAI::Box b = MinecartAI::bodyBox(s_carts[i]);
        out[n][0] = (float)(b.minX - OX); out[n][1] = (float)(b.minY - OY); out[n][2] = (float)(b.minZ - OZ);
        out[n][3] = (float)(b.maxX - OX); out[n][4] = (float)(b.maxY - OY); out[n][5] = (float)(b.maxZ - OZ);
        n++;
    }
    return n;
}

// ---- Guardado: carts.dat ----
namespace {
    void putI32(FILE* f, int v) { fwrite(&v, 4, 1, f); }
    bool getI32(FILE* f, int& v) { return fread(&v, 4, 1, f) == 1; }
}

bool MinecartManager::saveToFile(const char* path) {
    FILE* f = fopen(path, "wb");
    if (!f) return false;
    fputc('M', f); fputc('C', f); fputc('R', f); fputc('T', f);
    fputc(1, f);   // version del formato
    putI32(f, s_nextUid);
    putI32(f, count());
    for (int i = 0; i < MAX_CARTS; i++) {
        const Minecart& c = s_carts[i];
        if (!c.active) continue;
        putI32(f, c.uid);
        putI32(f, c.type);
        fwrite(&c.x, 8, 1, f); fwrite(&c.y, 8, 1, f); fwrite(&c.z, 8, 1, f);
        fwrite(&c.yaw, 4, 1, f);
        putI32(f, c.flipped ? 1 : 0);
        putI32(f, c.fuel);
        fwrite(&c.pushX, 8, 1, f); fwrite(&c.pushZ, 8, 1, f);
    }
    fclose(f);
    return true;
}

bool MinecartManager::loadFromFile(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) return false;
    char magic[4];
    if (fread(magic, 1, 4, f) != 4 || magic[0] != 'M' || magic[1] != 'C' || magic[2] != 'R' || magic[3] != 'T' || fgetc(f) != 1) { fclose(f); return false; }
    int nextUid = 1, n = 0;
    if (!getI32(f, nextUid) || !getI32(f, n)) { fclose(f); return false; }
    s_nextUid = nextUid < 1 ? 1 : nextUid;
    for (int k = 0; k < n && k < MAX_CARTS; k++) {
        Minecart& c = s_carts[k];
        c = Minecart();
        int flipped = 0;
        if (!getI32(f, c.uid) || !getI32(f, c.type) ||
            fread(&c.x, 8, 1, f) != 1 || fread(&c.y, 8, 1, f) != 1 || fread(&c.z, 8, 1, f) != 1 ||
            fread(&c.yaw, 4, 1, f) != 1 || !getI32(f, flipped) || !getI32(f, c.fuel) ||
            fread(&c.pushX, 8, 1, f) != 1 || fread(&c.pushZ, 8, 1, f) != 1) { c = Minecart(); break; }
        c.active = true;
        c.flipped = flipped != 0;
        c.prevX = c.x; c.prevY = c.y; c.prevZ = c.z;
        c.prevYaw = c.yaw;
    }
    fclose(f);
    return true;
}
