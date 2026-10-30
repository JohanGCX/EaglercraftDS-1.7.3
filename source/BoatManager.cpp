#include "BoatManager.h"
#include "BoatAI.h"
#include "BoatModel.h"
#include "ChunkManager.h"
#include "Chunk.h"
#include "HandAnimations.h"
#include "WorldConfig.h"
#include <math.h>
#include <stdio.h>

namespace {
    Boat s_boats[BoatManager::MAX_BOATS];
    int s_nextUid = 1;
    int s_riding = -1;
    double s_riderMX = 0.0, s_riderMZ = 0.0;   // movimiento del jinete (lo que EntityBoat lee de riddenByEntity.motionX / motionZ)

    bool s_forcedPending = false;
    float s_forcedX = 0.0f, s_forcedY = 0.0f, s_forcedZ = 0.0f;

    const int DROP_QUEUE = 48;
    BoatManager::Drop s_drops[DROP_QUEUE];
    int s_dropHead = 0, s_dropCount = 0;

    const double OX = 4.0, OY = 1.5, OZ = 4.0;   // mundo = rejilla - (4, 1,5, 4)
    const double PI_D = 3.14159265358979323846;

    // El mundo tal como lo ve BoatAI: celdas de rejilla (gx, gy, gz)
    struct BoatWorld {
        const ChunkManager& cm;
        explicit BoatWorld(const ChunkManager& c) : cm(c) {}
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
        bool solid(int x, int y, int z) const { return isMaterialSolid(type(x, y, z)); }
        bool ready(int x, int z) const { return chunkFor(x, z) != nullptr; }
    };

    void pushDrop(BlockType type, int count, double gx, double gy, double gz) {
        if (s_dropCount >= DROP_QUEUE) return;
        BoatManager::Drop& d = s_drops[(s_dropHead + s_dropCount) % DROP_QUEUE];
        d.type = type; d.count = count; d.durability = -1;
        d.x = (float)(gx - OX); d.y = (float)(gy - OY); d.z = (float)(gz - OZ);
        ++s_dropCount;
    }

    // EntityBoat.updateRiderPosition: el jinete va a 0,4 hacia la proa
    void eyeOf(const Boat& b, float& x, float& y, float& z) {
        const double yr = (double)b.yaw * PI_D / 180.0;
        x = (float)(b.x + cos(yr) * 0.4 - OX);
        y = (float)(b.y + BoatAI::MOUNTED_Y - OY);
        z = (float)(b.z + sin(yr) * 0.4 - OZ);
    }

    void dismountIndex(int i, float& ex, float& ey, float& ez) {
        const Boat& b = s_boats[i];
        ex = (float)(b.x - OX);
        ey = (float)((b.y - BoatAI::Y_OFFSET) + BoatAI::HEIGHT + 1.62 - OY);   // sobre el bote: Entity.mountEntity(null)
        ez = (float)(b.z - OZ);
        s_boats[i].ridden = false;
        s_riding = -1;
        s_riderMX = s_riderMZ = 0.0;
    }

    void forceDismount(int i) {
        if (s_riding != i) return;
        float ex, ey, ez;
        dismountIndex(i, ex, ey, ez);
        s_forcedPending = true; s_forcedX = ex; s_forcedY = ey; s_forcedZ = ez;
    }

    // setEntityDead con lo que suelta: 3 tablas y 2 palos
    void breakBoat(int i) {
        Boat& b = s_boats[i];
        forceDismount(i);
        pushDrop(BlockType::WOOD_PLANKS, 3, b.x, b.y, b.z);
        pushDrop(BlockType::STICK, 2, b.x, b.y, b.z);
        b.active = false;
    }

    // Entity.applyEntityCollision: 'self' recibe el choque de 'other' (se empujan 0,05 en sentidos opuestos)
    void bump(Boat& self, Boat& other) {
        double dx = other.x - self.x, dz = other.z - self.z;
        double d = fabs(dx) > fabs(dz) ? fabs(dx) : fabs(dz);
        if (d < 0.01) return;
        d = sqrt(d);
        dx /= d; dz /= d;
        double k = 1.0 / d;
        if (k > 1.0) k = 1.0;
        dx *= k * 0.05; dz *= k * 0.05;
        self.motionX -= dx; self.motionZ -= dz;
        other.motionX += dx; other.motionZ += dz;
    }

    // Beta: el rayo del bote se detiene en cualquier bloque (salvo aire) y en el agua quieta (fuente); el agua que corre no lo detiene
    bool rayStops(BlockType t, int meta) {
        if (t == BlockType::AIR) return false;
        if (isWaterBlock(t) || isLavaBlock(t)) return meta == 0;
        return true;
    }
}

void BoatManager::clear() {
    for (int i = 0; i < MAX_BOATS; i++) s_boats[i] = Boat();
    s_nextUid = 1;
    s_riding = -1;
    s_riderMX = s_riderMZ = 0.0;
    s_forcedPending = false;
    s_dropHead = s_dropCount = 0;
}

int BoatManager::count() {
    int n = 0;
    for (int i = 0; i < MAX_BOATS; i++) if (s_boats[i].active) ++n;
    return n;
}

bool BoatManager::spawnFromRay(const ChunkManager& cm, float ox, float oy, float oz, float dx, float dy, float dz) {
    BoatWorld w(cm);
    const double len = sqrt((double)dx * dx + (double)dy * dy + (double)dz * dz);
    if (len < 1.0e-6) return false;
    const double sx = (double)dx / len, sy = (double)dy / len, sz = (double)dz / len;
    double px = (double)ox + OX, py = (double)oy + OY, pz = (double)oz + OZ;

    int hx = 0, hy = 0, hz = 0;
    bool hit = false;
    for (double t = 0.0; t <= 5.0; t += 0.05) {
        const int cx = BoatAI::fl(px + sx * t), cy = BoatAI::fl(py + sy * t), cz = BoatAI::fl(pz + sz * t);
        if (cy < 0 || cy >= WorldConfig::SIZE_Y || !w.ready(cx, cz)) continue;
        if (rayStops(w.type(cx, cy, cz), w.meta(cx, cy, cz))) { hx = cx; hy = cy; hz = cz; hit = true; break; }
    }
    if (!hit) return false;
    if (w.type(hx, hy, hz) == BlockType::SNOW_LAYER) --hy;   // ItemBoat: sobre la nieve, el bote va sobre el suelo de debajo

    for (int i = 0; i < MAX_BOATS; i++) {
        if (s_boats[i].active) continue;
        Boat& b = s_boats[i];
        b = Boat();
        b.active = true;
        b.uid = s_nextUid++;
        b.x = (double)hx + 0.5; b.y = (double)hy + 1.0 + BoatAI::Y_OFFSET; b.z = (double)hz + 0.5;
        b.prevX = b.x; b.prevY = b.y; b.prevZ = b.z;
        return true;
    }
    return false;
}

void BoatManager::tick(const ChunkManager& cm, float inX, float inZ) {
    BoatWorld w(cm);

    // Entity.updateRidden pone el movimiento del jinete en 0 cada tick; luego su onLivingUpdate le suma UN tick de entrada (moveFlying 0,02 * 0,98)
    // y moveEntityWithHeading lo frena (x 0,8 dentro del agua). Eso es lo que lee EntityBoat: no se acumula.
    if (isRiding()) {
        s_riderMX = (double)inX * 0.98 * 0.02 * 0.8;
        s_riderMZ = (double)inZ * 0.98 * 0.02 * 0.8;
    } else {
        s_riderMX = s_riderMZ = 0.0;
    }

    for (int i = 0; i < MAX_BOATS; i++) {
        Boat& b = s_boats[i];
        if (!b.active) continue;
        if (!w.ready(BoatAI::fl(b.x), BoatAI::fl(b.z))) { b.prevX = b.x; b.prevY = b.y; b.prevZ = b.z; b.prevYaw = b.yaw; continue; }   // su chunk no esta: queda quieto

        const bool carries = (s_riding == i);
        if (BoatAI::tick(b, w, carries, s_riderMX, s_riderMZ)) { breakBoat(i); continue; }
        if (b.y < -32.0) {   // cayo al vacio
            forceDismount(i);
            b.active = false;
            continue;
        }

        // choques con otros botes (la caja, 0,2 mas ancha a cada lado)
        const BoatAI::Box a = BoatAI::bodyBox(b);
        for (int j = 0; j < MAX_BOATS; j++) {
            if (j == i || !s_boats[j].active) continue;
            const BoatAI::Box o = BoatAI::bodyBox(s_boats[j]);
            if (o.maxX > a.minX - 0.2 && o.minX < a.maxX + 0.2 && o.maxZ > a.minZ - 0.2 && o.minZ < a.maxZ + 0.2 && o.maxY > a.minY && o.minY < a.maxY)
                bump(s_boats[j], b);
        }
    }
}

bool BoatManager::popDrop(Drop& out) {
    if (s_dropCount == 0) return false;
    out = s_drops[s_dropHead];
    s_dropHead = (s_dropHead + 1) % DROP_QUEUE;
    --s_dropCount;
    return true;
}

void BoatManager::draw(const ChunkManager& cm, float partialTick) {
    for (int i = 0; i < MAX_BOATS; i++) {
        const Boat& b = s_boats[i];
        if (!b.active) continue;
        const double x = b.prevX + (b.x - b.prevX) * (double)partialTick;
        const double y = b.prevY + (b.y - b.prevY) * (double)partialTick;
        const double z = b.prevZ + (b.z - b.prevZ) * (double)partialTick;
        const float yaw = b.prevYaw + (b.yaw - b.prevYaw) * partialTick;
        BoatModel::draw(b, x, y, z, yaw, partialTick, HandAnimations::lightLevelAt(cm, (float)(x - OX), (float)(y - OY), (float)(z - OZ)));
    }
}

bool BoatManager::isRiding() { return s_riding >= 0 && s_boats[s_riding].active; }

bool BoatManager::riderEye(float& x, float& y, float& z) {
    if (!isRiding()) return false;
    eyeOf(s_boats[s_riding], x, y, z);
    return true;
}

bool BoatManager::dismount(float& ex, float& ey, float& ez) {
    if (!isRiding()) return false;
    dismountIndex(s_riding, ex, ey, ez);
    return true;
}

bool BoatManager::takeForcedDismount(float& ex, float& ey, float& ez) {
    if (!s_forcedPending) return false;
    s_forcedPending = false;
    ex = s_forcedX; ey = s_forcedY; ez = s_forcedZ;
    return true;
}

int BoatManager::pick(float ox, float oy, float oz, float dx, float dy, float dz, float maxDist, float& hitDist) {
    int best = -1;
    double bestDist = (double)maxDist + 1.0;
    for (int i = 0; i < MAX_BOATS; i++) {
        const Boat& b = s_boats[i];
        if (!b.active || i == s_riding) continue;
        const double d = BoatAI::rayHit(b, (double)ox + OX, (double)oy + OY, (double)oz + OZ, (double)dx, (double)dy, (double)dz, (double)maxDist);
        if (d >= 0.0 && d < bestDist) { bestDist = d; best = i; }
    }
    if (best >= 0) hitDist = (float)bestDist;
    return best;
}

// EntityBoat.attackEntityFrom: cada golpe suma 10 por punto de dano; al pasar de 40 se rompe
void BoatManager::attack(int index, int damage) {
    if (index < 0 || index >= MAX_BOATS || !s_boats[index].active) return;
    Boat& b = s_boats[index];
    b.rockDir = -b.rockDir;
    b.timeSinceHit = 10;
    b.damage += damage * 10;
    if (b.damage > 40) breakBoat(index);
}

bool BoatManager::interact(int index) {
    if (index < 0 || index >= MAX_BOATS || !s_boats[index].active) return false;
    Boat& b = s_boats[index];
    if (b.ridden) return false;
    b.ridden = true;
    s_riding = index;
    s_riderMX = s_riderMZ = 0.0;
    return true;
}

// ---- Guardado: boats.dat ----
namespace {
    void putI32(FILE* f, int v) { fwrite(&v, 4, 1, f); }
    bool getI32(FILE* f, int& v) { return fread(&v, 4, 1, f) == 1; }
}

bool BoatManager::saveToFile(const char* path) {
    FILE* f = fopen(path, "wb");
    if (!f) return false;
    fputc('M', f); fputc('B', f); fputc('O', f); fputc('T', f);
    fputc(1, f);   // version del formato
    putI32(f, s_nextUid);
    putI32(f, count());
    for (int i = 0; i < MAX_BOATS; i++) {
        const Boat& b = s_boats[i];
        if (!b.active) continue;
        putI32(f, b.uid);
        fwrite(&b.x, 8, 1, f); fwrite(&b.y, 8, 1, f); fwrite(&b.z, 8, 1, f);
        fwrite(&b.yaw, 4, 1, f);
    }
    fclose(f);
    return true;
}

bool BoatManager::loadFromFile(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) return false;
    char magic[4];
    if (fread(magic, 1, 4, f) != 4 || magic[0] != 'M' || magic[1] != 'B' || magic[2] != 'O' || magic[3] != 'T' || fgetc(f) != 1) { fclose(f); return false; }
    int nextUid = 1, n = 0;
    if (!getI32(f, nextUid) || !getI32(f, n)) { fclose(f); return false; }
    s_nextUid = nextUid < 1 ? 1 : nextUid;
    for (int k = 0; k < n && k < MAX_BOATS; k++) {
        Boat& b = s_boats[k];
        b = Boat();
        if (!getI32(f, b.uid) || fread(&b.x, 8, 1, f) != 1 || fread(&b.y, 8, 1, f) != 1 || fread(&b.z, 8, 1, f) != 1 || fread(&b.yaw, 4, 1, f) != 1) { b = Boat(); break; }
        b.active = true;
        b.prevX = b.x; b.prevY = b.y; b.prevZ = b.z;
        b.prevYaw = b.yaw;
    }
    fclose(f);
    return true;
}
