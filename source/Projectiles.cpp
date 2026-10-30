#include "Projectiles.h"
#include "BlockType.h"
#include "ChunkManager.h"
#include "Chunk.h"
#include "SheepManager.h"
#include "AnimalManager.h"
#include "HandAnimations.h"
#include "Texture.h"
#include <nds.h>
#include <math.h>

namespace {
    const double PI_D = 3.14159265358979323846;

    // Generador propio (xorshift): los proyectiles no dependen de ningun otro modulo para el azar
    unsigned int s_seed = 0x2F6E2B1u;
    unsigned int nextU() { s_seed ^= s_seed << 13; s_seed ^= s_seed >> 17; s_seed ^= s_seed << 5; return s_seed; }
    float nextFloat() { return (float)(nextU() & 0xFFFFFFu) / 16777216.0f; }
    int nextInt(int n) { return (int)(nextU() % (unsigned int)n); }
    double gaussian() {   // aproximacion de nextGaussian: suma de 4 uniformes (media 0, desviacion ~1)
        double s = 0.0;
        for (int i = 0; i < 4; i++) s += (double)nextFloat() * 2.0 - 1.0;
        return s * 0.866;
    }

    struct Proj {
        bool active = false;
        int kind = 0;
        double x = 0.0, y = 0.0, z = 0.0;
        double prevX = 0.0, prevY = 0.0, prevZ = 0.0;
        double mx = 0.0, my = 0.0, mz = 0.0;
        float yaw = 0.0f, pitch = 0.0f, prevYaw = 0.0f, prevPitch = 0.0f;
        bool inGround = false;
        int shake = 0, ticksInGround = 0, ticksInAir = 0;
        int tileI = 0, tileJ = 0, tileK = 0;     // la celda donde quedo clavada la flecha
        bool pickable = false;                   // doesArrowBelongToPlayer
        bool ownerIsPlayer = false;              // lo disparo el jugador: no lo golpea hasta llevar 5 ticks en el aire
        double ownerX = 0.0, ownerZ = 0.0;
    };
    Proj s_proj[Projectiles::MAX];
    bool s_nextOwnerIsPlayer = false;   // shoot() lo enciende mientras llama a spawn

    inline int cellY(double y) { return (int)floor(y + 0.5); }
    inline int chunkOf(double v) { return (int)floor((v + 4.0) / 8.0); }

    // El mundo en celdas alineadas (x e z enteros; el bloque j cubre y de j-0,5 a j+0,5), igual que el de los mobs
    struct World {
        const ChunkManager& cm;
        explicit World(const ChunkManager& c) : cm(c) {}
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
            if (!c) return BlockType::STONE;   // sin chunk, solido
            return c->getBlockType(gx & 7, j + 1, gz & 7);
        }
        bool chunkReady(int cx, int cz) const {
            Chunk* c = cm.getChunkByChunkCoord(cx, cz);
            return c && c->isGenerationComplete();
        }
        bool blocking(int i, int j, int k) const { return isMaterialSolid(type(i, j, k)); }
    };

    // EntityArrow.setArrowHeading (igual para la bola de nieve y el huevo)
    void setHeading(Proj& p, double dx, double dy, double dz, float speed, float spread) {
        double len = sqrt(dx * dx + dy * dy + dz * dz);
        if (len < 1.0e-9) len = 1.0;
        dx /= len; dy /= len; dz /= len;
        dx += gaussian() * 0.0075 * (double)spread;
        dy += gaussian() * 0.0075 * (double)spread;
        dz += gaussian() * 0.0075 * (double)spread;
        dx *= (double)speed; dy *= (double)speed; dz *= (double)speed;
        p.mx = dx; p.my = dy; p.mz = dz;
        const double h = sqrt(dx * dx + dz * dz);
        p.yaw = p.prevYaw = (float)(atan2(dx, dz) * 180.0 / PI_D);
        p.pitch = p.prevPitch = (float)(atan2(dy, h) * 180.0 / PI_D);
        p.ticksInGround = 0;
    }

    // El mob mas cercano al segmento. La Beta agranda la caja del mob 0,3 para los proyectiles; SheepManager::pick usa 0,1, asi que se tiran cinco rayos
    // paralelos (el central y cuatro a 0,2 alrededor) y se queda con el primero que pega. Devuelve el indice de SheepManager o -1.
    int pickMob(double x0, double y0, double z0, double x1, double y1, double z1, float& dist) {
        double dx = x1 - x0, dy = y1 - y0, dz = z1 - z0;
        const double len = sqrt(dx * dx + dy * dy + dz * dz);
        if (len < 1.0e-6) return -1;
        dx /= len; dy /= len; dz /= len;
        // base perpendicular al rayo
        double ux = dy * 0.0 - dz * 1.0, uy = dz * 0.0 - dx * 0.0, uz = dx * 1.0 - dy * 0.0;   // n x (0, 1, 0)
        double ul = sqrt(ux * ux + uy * uy + uz * uz);
        if (ul < 1.0e-3) { ux = 0.0; uy = 0.0; uz = 1.0; ul = 1.0; }                              // casi vertical
        ux /= ul; uy /= ul; uz /= ul;
        const double vx = dy * uz - dz * uy, vy = dz * ux - dx * uz, vz = dx * uy - dy * ux;       // n x u
        static const double OFF[5][2] = { { 0.0, 0.0 }, { 0.2, 0.0 }, { -0.2, 0.0 }, { 0.0, 0.2 }, { 0.0, -0.2 } };
        int best = -1;
        float bestD = 1.0e9f;
        for (int r = 0; r < 5; r++) {
            const double ox = ux * OFF[r][0] + vx * OFF[r][1], oy = uy * OFF[r][0] + vy * OFF[r][1], oz = uz * OFF[r][0] + vz * OFF[r][1];
            float hd = 0.0f;
            const int idx = SheepManager::pick((float)(x0 + ox), (float)(y0 + oy), (float)(z0 + oz), (float)dx, (float)dy, (float)dz, (float)len, hd);
            if (idx >= 0 && hd < bestD) { bestD = hd; best = idx; }
        }
        if (best >= 0) dist = bestD;
        return best;
    }

    // El segmento (x0, y0, z0) -> (x1, y1, z1) contra una caja (AxisAlignedBB.func_1169_a): t de 0 a 1 donde entra. Si empieza adentro cuenta como golpe en t = 0.
    bool segmentVsBox(double x0, double y0, double z0, double x1, double y1, double z1, const double mn[3], const double mx[3], double& tHit) {
        const double s[3] = { x0, y0, z0 }, e[3] = { x1, y1, z1 };
        double tmin = 0.0, tmax = 1.0;
        for (int a = 0; a < 3; a++) {
            const double d = e[a] - s[a];
            if (fabs(d) < 1.0e-12) {
                if (s[a] < mn[a] || s[a] > mx[a]) return false;
            } else {
                double t1 = (mn[a] - s[a]) / d, t2 = (mx[a] - s[a]) / d;
                if (t1 > t2) { const double tt = t1; t1 = t2; t2 = tt; }
                if (t1 > tmin) tmin = t1;
                if (t2 < tmax) tmax = t2;
                if (tmin > tmax) return false;
            }
        }
        tHit = tmin;
        return true;
    }

    // EntityEgg: al romperse, 1 de 8 suelta una gallina (y 1 de 32 de esas, cuatro)
    void eggBreak(double x, double y, double z) {
        if (nextInt(8) != 0) return;
        const int n = nextInt(32) == 0 ? 4 : 1;
        for (int i = 0; i < n; i++) AnimalManager::spawnChicken(x, y, z);
    }

    // EntityArrow.onUpdate / EntitySnowball.onUpdate / EntityEgg.onUpdate
    void step(Proj& p, const World& w, const Projectiles::PlayerTarget* pt) {
        p.prevX = p.x; p.prevY = p.y; p.prevZ = p.z;
        p.prevYaw = p.yaw; p.prevPitch = p.pitch;
        if (p.shake > 0) --p.shake;

        if (p.inGround) {
            if (w.blocking(p.tileI, p.tileJ, p.tileK)) {
                if (++p.ticksInGround >= 1200) p.active = false;            // desaparece a los 60 s
            } else {                                                        // se rompio el bloque: vuelve a caer
                p.inGround = false;
                p.mx *= (double)nextFloat() * 0.2;
                p.my *= (double)nextFloat() * 0.2;
                p.mz *= (double)nextFloat() * 0.2;
                p.ticksInGround = 0; p.ticksInAir = 0;
            }
            return;
        }

        ++p.ticksInAir;
        const double tx = p.x + p.mx, ty = p.y + p.my, tz = p.z + p.mz;
        // bloques: se recorre el segmento de a 0,1 y se frena en el primero que bloquea
        const double len = sqrt(p.mx * p.mx + p.my * p.my + p.mz * p.mz);
        int steps = (int)ceil(len / 0.1);
        if (steps < 1) steps = 1;
        bool hitBlock = false;
        double hx = p.x, hy = p.y, hz = p.z;
        int bi = 0, bj = 0, bk = 0;
        for (int n = 1; n <= steps; n++) {
            const double t = (double)n / (double)steps;
            const double qx = p.x + p.mx * t, qy = p.y + p.my * t, qz = p.z + p.mz * t;
            const int i = (int)floor(qx), j = cellY(qy), k = (int)floor(qz);
            if (w.blocking(i, j, k)) { hitBlock = true; bi = i; bj = j; bk = k; break; }
            hx = qx; hy = qy; hz = qz;
        }
        const double ex = hitBlock ? hx : tx, ey = hitBlock ? hy : ty, ez = hitBlock ? hz : tz;

        // el jugador (si es un blanco): la Beta agranda su caja 0,3 y se queda con la entidad mas cercana de todas
        bool hitPlayer = false;
        float playerDist = 1.0e9f;
        if (pt && pt->valid && pt->hurt && (!p.ownerIsPlayer || p.ticksInAir >= 5)) {
            const double mn[3] = { (double)pt->box[0] - 0.3, (double)pt->box[1] - 0.3, (double)pt->box[2] - 0.3 };
            const double mx[3] = { (double)pt->box[3] + 0.3, (double)pt->box[4] + 0.3, (double)pt->box[5] + 0.3 };
            double th = 0.0;
            if (segmentVsBox(p.x, p.y, p.z, ex, ey, ez, mn, mx, th)) {
                hitPlayer = true;
                const double sx = ex - p.x, sy = ey - p.y, sz = ez - p.z;
                playerDist = (float)(th * sqrt(sx * sx + sy * sy + sz * sz));
            }
        }

        // mobs: el primero que toca el segmento
        float hd = 0.0f;
        int mob = pickMob(p.x, p.y, p.z, ex, ey, ez, hd);
        if (mob >= 0 && hitPlayer) {
            if (hd <= playerDist) hitPlayer = false; else mob = -1;       // gana el que esta mas cerca
        }

        if (hitPlayer) {
            hitBlock = false;
            if (p.kind == Projectiles::ARROW) {
                // EntityArrow: attackEntityFrom(owner, 4). Una flecha tuya que te cae encima te empuja hacia un lado al azar; la del dispensador no tiene atacante
                float srcX = (float)p.ownerX, srcZ = (float)p.ownerZ;
                if (p.ownerIsPlayer) {
                    srcX = (float)p.x + (nextFloat() - nextFloat()) * 0.01f;
                    srcZ = (float)p.z + (nextFloat() - nextFloat()) * 0.01f;
                }
                if (pt->hurt(pt->ctx, 4, srcX, srcZ, p.ownerIsPlayer)) { p.active = false; return; }
                p.mx *= -0.1; p.my *= -0.1; p.mz *= -0.1;                   // sigue invulnerable: rebota
                p.yaw += 180.0f; p.prevYaw += 180.0f;
                p.ticksInAir = 0;
            } else {
                if (p.kind == Projectiles::EGG) eggBreak(p.x, p.y, p.z);   // 0 de dano: solo se rompe
                p.active = false;
                return;
            }
        } else if (mob >= 0) {
            hitBlock = false;                                               // en ese tick manda el mob (como en la Beta)
            if (p.kind == Projectiles::ARROW) {
                if (SheepManager::attack(mob, 4, (float)p.ownerX, (float)p.ownerZ)) { p.active = false; return; }
                p.mx *= -0.1; p.my *= -0.1; p.mz *= -0.1;                   // no le hizo dano (todavia invulnerable): rebota
                p.yaw += 180.0f; p.prevYaw += 180.0f;
                p.ticksInAir = 0;
            } else {
                SheepManager::attack(mob, 0, (float)p.ownerX, (float)p.ownerZ);   // 0 de dano: solo el empujon
                if (p.kind == Projectiles::EGG) eggBreak(p.x, p.y, p.z);
                p.active = false;
                return;
            }
        }

        if (hitBlock) {
            if (p.kind != Projectiles::ARROW) {                             // la bola de nieve y el huevo se rompen
                if (p.kind == Projectiles::EGG) eggBreak(hx, hy, hz);
                p.active = false;
                return;
            }
            const double dx = hx - p.x, dy = hy - p.y, dz = hz - p.z;
            const double l = sqrt(dx * dx + dy * dy + dz * dz);
            p.tileI = bi; p.tileJ = bj; p.tileK = bk;
            p.mx = dx; p.my = dy; p.mz = dz;
            if (l > 1.0e-6) { p.x -= dx / l * 0.05; p.y -= dy / l * 0.05; p.z -= dz / l * 0.05; }
            p.x += p.mx; p.y += p.my; p.z += p.mz;
            p.inGround = true;
            p.shake = 7;
            return;
        }

        p.x = tx; p.y = ty; p.z = tz;
        const double h = sqrt(p.mx * p.mx + p.mz * p.mz);
        float newYaw = (float)(atan2(p.mx, p.mz) * 180.0 / PI_D);
        float newPitch = (float)(atan2(p.my, h) * 180.0 / PI_D);
        while (newPitch - p.prevPitch < -180.0f) p.prevPitch -= 360.0f;
        while (newPitch - p.prevPitch >= 180.0f) p.prevPitch += 360.0f;
        while (newYaw - p.prevYaw < -180.0f) p.prevYaw -= 360.0f;
        while (newYaw - p.prevYaw >= 180.0f) p.prevYaw += 360.0f;
        p.pitch = p.prevPitch + (newPitch - p.prevPitch) * 0.2f;
        p.yaw = p.prevYaw + (newYaw - p.prevYaw) * 0.2f;
        const bool wet = isWaterBlock(w.type((int)floor(p.x), cellY(p.y), (int)floor(p.z)));
        const double drag = wet ? 0.8 : 0.99;
        p.mx *= drag; p.my *= drag; p.mz *= drag;
        p.my -= 0.03;
    }

    float brightnessOf(unsigned char level) {   // WorldProvider.generateLightBrightnessTable (b1.7.3)
        const float f = 1.0f - (float)level / 15.0f;
        return (1.0f - f) / (f * 3.0f + 1.0f) * 0.95f + 0.05f;
    }

    // Sprite de items.png (columna, fila) que mira al jugador, a la mitad de tamano (RenderSnowball)
    void drawSprite(float x, float y, float z, int col, int row, float camYaw, float camPitch) {
        glPolyFmt(POLY_ALPHA(31) | POLY_CULL_NONE);
        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
            glTranslatef(x, y, z);
            glScalef(0.5f, 0.5f, 0.5f);
            glRotatef(camYaw, 0.0f, 1.0f, 0.0f);      // contra el giro de la camara (-yaw, -pitch): el sprite queda de frente
            glRotatef(camPitch, 1.0f, 0.0f, 0.0f);
            glBindTexture(0, Texture::itemsTextureID);
            glColor3b(255, 255, 255);
            const int u0 = col * 16, v0 = row * 16, u1 = u0 + 16, v1 = v0 + 16;
            glBegin(GL_QUADS);
                GFX_TEX_COORD = TEXTURE_PACK(inttot16(u0), inttot16(v1)); glVertex3v16(floattov16(-0.5f), floattov16(-0.25f), 0);
                GFX_TEX_COORD = TEXTURE_PACK(inttot16(u1), inttot16(v1)); glVertex3v16(floattov16( 0.5f), floattov16(-0.25f), 0);
                GFX_TEX_COORD = TEXTURE_PACK(inttot16(u1), inttot16(v0)); glVertex3v16(floattov16( 0.5f), floattov16( 0.75f), 0);
                GFX_TEX_COORD = TEXTURE_PACK(inttot16(u0), inttot16(v0)); glVertex3v16(floattov16(-0.5f), floattov16( 0.75f), 0);
            glEnd();
        glPopMatrix(1);
    }
}

void Projectiles::clear() {
    for (int i = 0; i < MAX; i++) s_proj[i] = Proj();
}

bool Projectiles::spawn(int kind, double x, double y, double z, double dx, double dy, double dz, float speed, float spread, bool pickable, double ownerX, double ownerZ) {
    for (int i = 0; i < MAX; i++) {
        if (s_proj[i].active) continue;
        Proj& p = s_proj[i];
        p = Proj();
        p.active = true;
        p.kind = kind;
        p.x = p.prevX = x; p.y = p.prevY = y; p.z = p.prevZ = z;
        p.pickable = pickable;
        p.ownerIsPlayer = s_nextOwnerIsPlayer;
        p.ownerX = ownerX; p.ownerZ = ownerZ;
        setHeading(p, dx, dy, dz, speed, spread);
        return true;
    }
    // sin lugar: se reemplaza la flecha clavada mas vieja (no la que esta volando)
    int oldest = -1, most = -1;
    for (int i = 0; i < MAX; i++) {
        if (s_proj[i].kind == ARROW && s_proj[i].inGround && s_proj[i].ticksInGround > most) { most = s_proj[i].ticksInGround; oldest = i; }
    }
    if (oldest < 0) return false;
    s_proj[oldest].active = false;
    return spawn(kind, x, y, z, dx, dy, dz, speed, spread, pickable, ownerX, ownerZ);
}

bool Projectiles::shoot(int kind, double eyeX, double eyeY, double eyeZ, float yaw, float pitch) {
    s_nextOwnerIsPlayer = true;   // lo que sale de aqui lo dispara el jugador (spawn lo marca y lo apaga)
    // Player::getLookDirection: la direccion sale del yaw y el pitch del jugador
    const double yr = (double)yaw * PI_D / 180.0, pr = (double)pitch * PI_D / 180.0;
    const double dx = -sin(yr) * cos(pr), dy = sin(pr), dz = -cos(yr) * cos(pr);
    // la Beta usa otro yaw (180 - yaw): posX -= cos(yawB) * 0,16, posZ -= sin(yawB) * 0,16, posY -= 0,1
    const double sx = eyeX + cos(yr) * 0.16, sz = eyeZ - sin(yr) * 0.16, sy = eyeY - 0.1;
    const bool ok = spawn(kind, sx, sy, sz, dx, dy, dz, 1.5f, 1.0f, kind == ARROW, eyeX, eyeZ);
    s_nextOwnerIsPlayer = false;
    return ok;
}

int Projectiles::tick(const ChunkManager& cm, float px, float py, float pz, bool canPickUp, const PlayerTarget* target) {
    World w(cm);
    int picked = 0;
    const double feet = (double)py - 1.62;
    for (int i = 0; i < MAX; i++) {
        Proj& p = s_proj[i];
        if (!p.active) continue;
        if (!(fabs(p.x) < 1.0e6 && fabs(p.z) < 1.0e6 && p.y > -40.0 && p.y < 300.0) || !w.chunkReady(chunkOf(p.x), chunkOf(p.z))) { p.active = false; continue; }
        step(p, w, target);
        // EntityArrow.onCollideWithPlayer: la flecha clavada del jugador se recoge al pisarla (la caja del jugador se agranda 1 en horizontal)
        if (p.active && p.kind == ARROW && p.inGround && p.pickable && p.shake <= 0 && canPickUp) {
            if (fabs(p.x - (double)px) < 1.3 && fabs(p.z - (double)pz) < 1.3 && p.y > feet - 0.3 && p.y < feet + 2.1) {
                p.active = false;
                ++picked;
            }
        }
    }
    return picked;
}

void Projectiles::draw(const ChunkManager& cm, float partialTick, float camYaw, float camPitch) {
    const float t = partialTick;
    for (int i = 0; i < MAX; i++) {
        const Proj& p = s_proj[i];
        if (!p.active) continue;
        const float x = (float)(p.prevX + (p.x - p.prevX) * t);
        const float y = (float)(p.prevY + (p.y - p.prevY) * t);
        const float z = (float)(p.prevZ + (p.z - p.prevZ) * t);
        if (p.kind == ARROW) {
            const float yaw = p.prevYaw + (p.yaw - p.prevYaw) * t;
            const float pitch = p.prevPitch + (p.pitch - p.prevPitch) * t;
            drawArrowModel(x, y, z, yaw, pitch, (float)p.shake - t, HandAnimations::lightLevelAt(cm, x, y, z));
        } else if (p.kind == SNOWBALL) {
            drawSprite(x, y, z, 14, 0, camYaw, camPitch);   // items.png (14,0)
        } else {
            drawSprite(x, y, z, 12, 0, camYaw, camPitch);   // items.png (12,0)
        }
    }
}

// RenderArrow (Beta 1.7.3) con arrows.png (32 x 32): un tubo de 16 x 5 texels en cruz (dos planos) y la tapa de la cola de 5 x 5. La Beta mete 0,05625 por texel
// (el tubo mide 16 * 0,05625 = 0,9 bloques) y desplaza el modelo 4 texels para que el centro de la entidad quede a 4 de la punta.
void Projectiles::drawArrowModel(float x, float y, float z, float yaw, float pitch, float shake, unsigned char level) {
    const float S = 0.05625f;
    const int v = (int)(brightnessOf(level) * 255.0f + 0.5f);

    glPolyFmt(POLY_ALPHA(31) | POLY_CULL_NONE | POLY_FOG);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
        glTranslatef(x, y, z);
        glRotatef(yaw - 90.0f, 0.0f, 1.0f, 0.0f);
        glRotatef(pitch, 0.0f, 0.0f, 1.0f);
        if (shake > 0.0f) glRotatef(-sinf(shake * 3.0f) * shake, 0.0f, 0.0f, 1.0f);   // tiembla al clavarse
        glRotatef(45.0f, 1.0f, 0.0f, 0.0f);
        glBindTexture(0, Texture::signFontTextureID);   // el modelo de la flecha esta pintado en el atlas de letras (Texture.cpp)
        glColor3b(v, v, v);

        // la tapa de la cola (x = -7 texels), de 5 x 5 en (0, 26) del atlas de letras
        glBegin(GL_QUADS);
            GFX_TEX_COORD = TEXTURE_PACK(inttot16(0), inttot16(26)); glVertex3v16(floattov16((-7.0f - 4.0f) * S), floattov16(-2.0f * S), floattov16(-2.0f * S));
            GFX_TEX_COORD = TEXTURE_PACK(inttot16(5), inttot16(26)); glVertex3v16(floattov16((-7.0f - 4.0f) * S), floattov16(-2.0f * S), floattov16( 2.0f * S));
            GFX_TEX_COORD = TEXTURE_PACK(inttot16(5), inttot16(31)); glVertex3v16(floattov16((-7.0f - 4.0f) * S), floattov16( 2.0f * S), floattov16( 2.0f * S));
            GFX_TEX_COORD = TEXTURE_PACK(inttot16(0), inttot16(31)); glVertex3v16(floattov16((-7.0f - 4.0f) * S), floattov16( 2.0f * S), floattov16(-2.0f * S));
        glEnd();

        // el tubo, de 16 x 5 texels en (0, 20) del atlas de letras: dos planos que se cruzan en el eje de la flecha (la Beta dibuja cuatro, dos pares iguales de espaldas)
        for (int pass = 0; pass < 2; pass++) {
            glRotatef(90.0f, 1.0f, 0.0f, 0.0f);
            glBegin(GL_QUADS);
                GFX_TEX_COORD = TEXTURE_PACK(inttot16(0),  inttot16(20)); glVertex3v16(floattov16((-8.0f - 4.0f) * S), floattov16(-2.0f * S), 0);
                GFX_TEX_COORD = TEXTURE_PACK(inttot16(16), inttot16(20)); glVertex3v16(floattov16(( 8.0f - 4.0f) * S), floattov16(-2.0f * S), 0);
                GFX_TEX_COORD = TEXTURE_PACK(inttot16(16), inttot16(25)); glVertex3v16(floattov16(( 8.0f - 4.0f) * S), floattov16( 2.0f * S), 0);
                GFX_TEX_COORD = TEXTURE_PACK(inttot16(0),  inttot16(25)); glVertex3v16(floattov16((-8.0f - 4.0f) * S), floattov16( 2.0f * S), 0);
            glEnd();
        }
    glPopMatrix(1);
}
