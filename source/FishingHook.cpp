#include "FishingHook.h"
#include "Weather.h"
#include "Particles.h"
#include "Sound.h"
#include "BlockType.h"
#include "ChunkManager.h"
#include "Chunk.h"
#include "SheepManager.h"
#include <nds.h>
#include <math.h>

namespace {
    const double PI_D = 3.14159265358979323846;

    unsigned int s_seed = 0x5EED1234u;
    unsigned int nextU() { s_seed ^= s_seed << 13; s_seed ^= s_seed >> 17; s_seed ^= s_seed << 5; return s_seed; }
    float nextFloat() { return (float)(nextU() & 0xFFFFFFu) / 16777216.0f; }
    int nextInt(int n) { return (int)(nextU() % (unsigned int)n); }
    double gaussian() { double s = 0.0; for (int i = 0; i < 4; i++) s += (double)nextFloat() * 2.0 - 1.0; return s * 0.866; }

    struct Hook {
        bool active = false;
        double x = 0.0, y = 0.0, z = 0.0, prevX = 0.0, prevY = 0.0, prevZ = 0.0;
        double mx = 0.0, my = 0.0, mz = 0.0;
        bool inGround = false;
        bool hooked = false;          // enganchado a un mob (bobber != null)
        int mobIndex = -1;
        int ticksInGround = 0, ticksInAir = 0, ticksCatchable = 0;
    };
    Hook s_hook;

    inline int cellY(double y) { return (int)floor(y + 0.5); }

    // El mundo en celdas alineadas, igual que los proyectiles
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
            Chunk* c = chunkAt(i + 4, k + 4);
            if (!c) return BlockType::STONE;
            return c->getBlockType((i + 4) & 7, j + 1, (k + 4) & 7);
        }
        bool blocking(int i, int j, int k) const { return isMaterialSolid(type(i, j, k)); }
        bool ready(double x, double z) const {
            Chunk* c = cm.getChunkByChunkCoord((int)floor((x + 4.0) / 8.0), (int)floor((z + 4.0) / 8.0));
            return c && c->isGenerationComplete();
        }
    };

    // Cuanto del tramo de y (abajo, arriba) del hook esta bajo la superficie del agua de su celda (World.isAABBInMaterial(water)): la superficie de un agua con aire arriba
    // queda 1/9 mas baja que el techo de la celda
    bool waterAt(const World& w, double x, double yLow, double yHigh, double z) {
        const int i = (int)floor(x), k = (int)floor(z);
        for (int j = cellY(yLow); j <= cellY(yHigh); j++) {
            if (!isWaterBlock(w.type(i, j, k))) continue;
            const double top = isWaterBlock(w.type(i, j + 1, k)) ? (double)j + 0.5 : (double)j + 0.5 - 1.0 / 9.0;
            if (yLow < top) return true;
        }
        return false;
    }

    void step(Hook& h, const World& w) {
        h.prevX = h.x; h.prevY = h.y; h.prevZ = h.z;
        if (h.hooked) {   // EntityFish.onUpdate con bobber: sigue al mob (x, 80% de su altura, z); si murio o desaparecio, se suelta y sigue volando
            float ax, ay, az;
            if (SheepManager::anchor(h.mobIndex, ax, ay, az)) { h.x = ax; h.y = ay; h.z = az; return; }
            h.hooked = false; h.mobIndex = -1; h.ticksInAir = 0;
            return;
        }
        if (h.inGround) {
            if (++h.ticksInGround >= 1200) h.active = false;
            return;
        }
        ++h.ticksInAir;

        // bloques: se recorre el segmento de a 0,1 y, si toca uno solido, el anzuelo se clava (no se mueve ese tick)
        const double len = sqrt(h.mx * h.mx + h.my * h.my + h.mz * h.mz);
        int steps = (int)ceil(len / 0.1);
        if (steps < 1) steps = 1;
        for (int n = 1; n <= steps; n++) {
            const double t = (double)n / (double)steps;
            if (w.blocking((int)floor(h.x + h.mx * t), cellY(h.y + h.my * t), (int)floor(h.z + h.mz * t))) { h.inGround = true; return; }
        }
        // mobs: la Beta agranda 0,3 la caja del mob; SheepManager::pick usa 0,1, asi que alcanza con el rayo central
        {
            double dx = h.mx, dy = h.my, dz = h.mz;
            const double l = sqrt(dx * dx + dy * dy + dz * dz);
            if (l > 1.0e-6 && h.ticksInAir >= 5) {
                float hd = 0.0f;
                const int mob = SheepManager::pick((float)h.x, (float)h.y, (float)h.z, (float)(dx / l), (float)(dy / l), (float)(dz / l), (float)l, hd);
                if (mob >= 0) {
                    SheepManager::attack(mob, 0, (float)h.x - (float)h.mx, (float)h.z - (float)h.mz);   // attackEntityFrom(angler, 0): solo el empujon
                    h.x += (dx / l) * hd; h.y += (dy / l) * hd; h.z += (dz / l) * hd;
                    h.hooked = true; h.mobIndex = mob;
                    return;
                }
            }
        }

        h.x += h.mx; h.y += h.my; h.z += h.mz;
        float drag = 0.92f;   // 0,5 si toca el suelo o una pared: aqui no ocurre, porque un bloque lo clava antes

        // flotacion: 5 tramos de la caja de 0,25 (la caja nace en y y llega a y + 0,25)
        double inWater = 0.0;
        for (int s = 0; s < 5; s++) {
            const double lo = h.y + 0.25 * (double)s / 5.0, hi = h.y + 0.25 * (double)(s + 1) / 5.0;
            if (waterAt(w, h.x, lo, hi, h.z)) inWater += 1.0 / 5.0;
        }
        static bool s_wet = false;
        if (inWater > 0.0 && !s_wet) {   // cae al agua: salpicadura
            for (int k = 0; k < 5; k++) Particles::spawn(Particles::SPLASH, (float)h.x, (float)h.y + 0.1f, (float)h.z, 0.0f, 0.0f, 0.0f, 0.8f);
            Sound::splash((float)h.x, (float)h.y, (float)h.z, 0.5f);
        }
        s_wet = inWater > 0.0;
        if (inWater > 0.0) {
            if (h.ticksCatchable > 0) {
                --h.ticksCatchable;
            } else if (nextInt(Weather::canRainAtWorld((float)h.x, (float)h.y + 1.0f, (float)h.z) ? 300 : 500) == 0) {            // pica (1 de 300 si llueve sobre el)
                h.ticksCatchable = nextInt(30) + 10;
                h.my -= 0.2;
                for (int k = 0; k < 4; k++) Particles::spawn(Particles::BUBBLE, (float)h.x, (float)h.y, (float)h.z, 0.0f, 0.0f, 0.0f);
                Sound::splash((float)h.x, (float)h.y, (float)h.z, 0.4f);   // el pique
            }
        }
        if (h.ticksCatchable > 0) h.my -= (double)(nextFloat() * nextFloat() * nextFloat()) * 0.2;
        h.my += 0.04 * (inWater * 2.0 - 1.0);
        if (inWater > 0.0) { drag = (float)((double)drag * 0.9); h.my *= 0.8; }
        h.mx *= drag; h.my *= drag; h.mz *= drag;
    }

    inline void rotX(double& y, double& z, double a) { const double c = cos(a), s = sin(a), ny = y * c + z * s, nz = z * c - y * s; y = ny; z = nz; }   // Vec3D.rotateAroundX
    inline void rotY(double& x, double& z, double a) { const double c = cos(a), s = sin(a), nx = x * c + z * s, nz = z * c - x * s; x = nx; z = nz; }   // Vec3D.rotateAroundY

    // Donde sale la linea: RenderFish. yawB / pitchB: los angulos de la Beta (yaw = 180 - yaw de Player; pitch positivo = mira abajo)
    void handPos(double ex, double ey, double ez, float playerYaw, float playerPitch, float swing, bool third, double& hx, double& hy, double& hz) {
        const double yawB = (180.0 - (double)playerYaw) * PI_D / 180.0, pitchB = -(double)playerPitch * PI_D / 180.0;
        if (third) {
            hx = ex - cos(yawB) * 0.35 - sin(yawB) * 0.85;
            hy = ey - 0.45;
            hz = ez - sin(yawB) * 0.35 + cos(yawB) * 0.85;
            return;
        }
        const double sw = sin(sqrt((double)swing) * PI_D);
        double vx = -0.5, vy = 0.03, vz = 0.8;
        rotX(vy, vz, -pitchB);
        rotY(vx, vz, -yawB);
        rotY(vx, vz, sw * 0.5);
        rotX(vy, vz, -sw * 0.7);
        hx = ex + vx; hy = ey + vy; hz = ez + vz;
    }

    float brightnessOf(unsigned char level) {
        const float f = 1.0f - (float)level / 15.0f;
        return (1.0f - f) / (f * 3.0f + 1.0f) * 0.95f + 0.05f;
    }
}

void FishingHook::clear() { s_hook = Hook(); }
bool FishingHook::active() { return s_hook.active; }

bool FishingHook::cast(double eyeX, double eyeY, double eyeZ, float yaw, float pitch) {
    Hook& h = s_hook;
    h = Hook();
    h.active = true;
    const double yr = (double)yaw * PI_D / 180.0, pr = (double)pitch * PI_D / 180.0;
    double dx = -sin(yr) * cos(pr), dy = sin(pr), dz = -cos(yr) * cos(pr);
    h.x = eyeX + cos(yr) * 0.16; h.y = eyeY - 0.1; h.z = eyeZ - sin(yr) * 0.16;   // EntityFish(world, player)
    h.prevX = h.x; h.prevY = h.y; h.prevZ = h.z;
    dx += gaussian() * 0.0075; dy += gaussian() * 0.0075; dz += gaussian() * 0.0075;   // func_4042_a(motion, 1.5, 1.0)
    h.mx = dx * 1.5; h.my = dy * 1.5; h.mz = dz * 1.5;
    return true;
}

FishingHook::Catch FishingHook::reel(double eyeX, double eyeY, double eyeZ) {
    Catch c = { 0, false, 0, 0, 0, 0, 0, 0 };
    Hook& h = s_hook;
    if (!h.active) return c;
    const double dx = eyeX - h.x, dy = eyeY - h.y, dz = eyeZ - h.z;
    const double dist = sqrt(dx * dx + dy * dy + dz * dz);
    if (h.hooked) {
        c.damage = 3;   // EntityFish.catchFish con bobber: tira del mob hacia el jugador
        SheepManager::pull(h.mobIndex, (float)(dx * 0.1), (float)(dy * 0.1 + sqrt(dist) * 0.08), (float)(dz * 0.1));
    } else if (h.ticksCatchable > 0) {
        c.fish = true; c.damage = 1;
        c.x = (float)h.x; c.y = (float)h.y; c.z = (float)h.z;
        c.vx = (float)(dx * 0.1); c.vy = (float)(dy * 0.1 + sqrt(dist) * 0.08); c.vz = (float)(dz * 0.1);
    }
    if (h.inGround) c.damage = 2;
    h = Hook();
    return c;
}

void FishingHook::tick(const ChunkManager& cm, double eyeX, double eyeY, double eyeZ, bool holdingRod, bool alive) {
    Hook& h = s_hook;
    if (!h.active) return;
    const double dx = h.x - eyeX, dy = h.y - eyeY, dz = h.z - eyeZ;
    World w(cm);
    if (!alive || !holdingRod || dx * dx + dy * dy + dz * dz > 1024.0 || !w.ready(h.x, h.z) || h.y < -40.0 || h.y > 300.0) { h = Hook(); return; }
    step(h, w);
}

void FishingHook::draw(const ChunkManager& cm, float partialTick, float camYaw, float camPitch, double eyeX, double eyeY, double eyeZ, float playerYaw, float playerPitch, float swing, bool thirdPerson) {
    const Hook& h = s_hook;
    if (!h.active) return;
    const float t = partialTick;
    const double x = h.prevX + (h.x - h.prevX) * t, y = h.prevY + (h.y - h.prevY) * t, z = h.prevZ + (h.z - h.prevZ) * t;

    // el flotador (RenderFish): la celda 1 de particles.png (8 x 8, el sprite de 4 x 7 centrado) en un cuadro de 0,5 que mira a la camara.
    // Sin textura (la VRAM esta llena): cada pixel del original es un cuadrito de color plano; los pixeles seguidos del mismo color van en un solo cuadro.
    (void)cm;
    glPolyFmt(POLY_ALPHA(31) | POLY_CULL_NONE);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
        glTranslatef((float)x, (float)y, (float)z);
        glScalef(0.5f, 0.5f, 0.5f);
        glRotatef(camYaw, 0.0f, 1.0f, 0.0f);
        glRotatef(camPitch, 1.0f, 0.0f, 0.0f);
        glBindTexture(0, 0);
        {
            // fila 0 = arriba. W blanco (211), G gris (145), R rojo (211,42,42), r rojo oscuro (137,20,20), K negro (el gancho), '.' transparente
            static const char* const SPRITE[7] = { ".WWG", ".RRr", ".WGG", "..K.", "..K.", "K.K.", ".KK." };
            const float u = 0.125f;          // un pixel de la celda de 8 (la celda mide 1 antes del escalado)
            const float ox = -0.5f + 2.0f * u, oy = 0.5f;   // columnas 2 a 5 de la celda, desde la fila de arriba
            glBegin(GL_QUADS);
            for (int r = 0; r < 7; r++) {
                for (int c = 0; c < 4; ) {
                    const char k = SPRITE[r][c];
                    int e = c + 1;
                    while (e < 4 && SPRITE[r][e] == k) e++;
                    if (k != '.') {
                        int cr = 0, cg = 0, cb = 0;
                        if (k == 'W') { cr = cg = cb = 211; }
                        else if (k == 'G') { cr = cg = cb = 145; }
                        else if (k == 'R') { cr = 211; cg = 42; cb = 42; }
                        else if (k == 'r') { cr = 137; cg = 20; cb = 20; }
                        glColor3b(cr, cg, cb);
                        const float x0 = ox + (float)c * u, x1 = ox + (float)e * u, y0 = oy - (float)(r + 1) * u, y1 = oy - (float)r * u;
                        glVertex3v16(floattov16(x0), floattov16(y0), 0); glVertex3v16(floattov16(x1), floattov16(y0), 0);
                        glVertex3v16(floattov16(x1), floattov16(y1), 0); glVertex3v16(floattov16(x0), floattov16(y1), 0);
                    }
                    c = e;
                }
            }
            glEnd();
        }
    glPopMatrix(1);

    // la linea: 16 tramos desde la mano hasta el anzuelo, en una curva (y = (s * s + s) / 2), en negro
    double px, py, pz, ex = eyeX, ey = eyeY, ez = eyeZ;
    {   // la posicion del jugador a este cuadro no se interpola aqui: los ojos de este tick bastan
        handPos(ex, ey, ez, playerYaw, playerPitch, swing, thirdPerson, px, py, pz);
    }
    const double ddx = px - x, ddy = py - (y + 0.25), ddz = pz - z;
    glPolyFmt(POLY_ALPHA(31) | POLY_CULL_NONE);
    glBindTexture(0, 0);
    glColor3b(0, 0, 0);
    float lx = 0, ly = 0, lz = 0;
    for (int i = 0; i <= 16; i++) {
        const double s = (double)i / 16.0;
        const float cx = (float)(x + ddx * s), cy = (float)(y + ddy * (s * s + s) * 0.5 + 0.25), cz = (float)(z + ddz * s);
        if (i > 0) {   // cada tramo con su propio origen: las coordenadas de v16 no pasan de 8
            const float dx = cx - lx, dy = cy - ly, dz = cz - lz, e = 0.008f;   // grosor: dos cuadros finos en cruz
            glPushMatrix();
            glTranslatef(lx, ly, lz);
            glBegin(GL_QUADS);
                glVertex3v16(floattov16(0.0f), floattov16(-e), floattov16(0.0f)); glVertex3v16(floattov16(dx), floattov16(dy - e), floattov16(dz));
                glVertex3v16(floattov16(dx), floattov16(dy + e), floattov16(dz)); glVertex3v16(floattov16(0.0f), floattov16(e), floattov16(0.0f));
                glVertex3v16(floattov16(-e), floattov16(0.0f), floattov16(-e)); glVertex3v16(floattov16(dx - e), floattov16(dy), floattov16(dz - e));
                glVertex3v16(floattov16(dx + e), floattov16(dy), floattov16(dz + e)); glVertex3v16(floattov16(e), floattov16(0.0f), floattov16(e));
            glEnd();
            glPopMatrix(1);
        }
        lx = cx; ly = cy; lz = cz;
    }
}
