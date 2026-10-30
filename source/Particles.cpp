#include "Particles.h"
#include "ChunkManager.h"
#include "Chunk.h"
#include "HandAnimations.h"
#include "Texture.h"
#include "Options.h"
#include <nds.h>
#include <math.h>
#include <string.h>

namespace {
    struct P {
        bool active;
        unsigned char kind;
        float x, y, z, px, py, pz;
        float vx, vy, vz;
        int age, maxAge;
        float scale, baseScale, gravity;
        float r, g, b;       // 0..1 (el color de la particula, sin la luz)
        float light;         // 0..1
        unsigned char col, row, jx, jy;   // DIG: la celda de terrain.png y el pedazo de 4 x 4 pixeles
        bool onGround, noClip;
    };
    P s_p[Particles::MAX_PARTICLES];
    unsigned s_rng = 0x9E3779B1u;
    unsigned rnd() { s_rng ^= s_rng << 13; s_rng ^= s_rng >> 17; s_rng ^= s_rng << 5; return s_rng; }
    float rf() { return (float)(rnd() & 0xFFFFFFu) / 16777216.0f; }
    int ri(int n) { return (int)(rnd() % (unsigned)n); }

    float brightnessOf(int level) {
        const float f = 1.0f - (float)level / 15.0f;
        return (1.0f - f) / (f * 3.0f + 1.0f) * 0.95f + 0.05f;
    }

    struct W {
        const ChunkManager& cm;
        explicit W(const ChunkManager& c) : cm(c) {}
        bool solid(float x, float y, float z) const {
            const int i = (int)floorf(x), j = (int)floorf(y + 0.5f), k = (int)floorf(z);
            if (j < 0) return true;
            if (j > 126) return false;
            Chunk* c = cm.getChunkByChunkCoord((i + 4) >> 3, (k + 4) >> 3);
            if (!c || !c->isGenerationComplete()) return true;
            return isMaterialSolid(c->getBlockType((i + 4) & 7, j + 1, (k + 4) & 7));
        }
    };

    P* alloc() {
        int worst = 0; float worstFrac = -1.0f;
        for (int i = 0; i < Particles::MAX_PARTICLES; i++) {
            if (!s_p[i].active) { memset(&s_p[i], 0, sizeof(P)); return &s_p[i]; }
            const float f = (float)s_p[i].age / (float)(s_p[i].maxAge > 0 ? s_p[i].maxAge : 1);
            if (f > worstFrac) { worstFrac = f; worst = i; }
        }
        memset(&s_p[worst], 0, sizeof(P));   // lleno: se reemplaza la mas vieja
        return &s_p[worst];
    }

    // EntityFX(): velocidad al azar normalizada (0,06 de largo aprox) mas lo dado
    void baseMotion(P& p, float vx, float vy, float vz) {
        float mx = vx + (rf() * 2.0f - 1.0f) * 0.4f, my = vy + (rf() * 2.0f - 1.0f) * 0.4f, mz = vz + (rf() * 2.0f - 1.0f) * 0.4f;
        const float f = (rf() + rf() + 1.0f) * 0.15f;
        float l = sqrtf(mx * mx + my * my + mz * mz);
        if (l < 1.0e-4f) l = 1.0f;
        p.vx = mx / l * f * 0.4f; p.vy = my / l * f * 0.4f + 0.1f; p.vz = mz / l * f * 0.4f;
        p.scale = p.baseScale = (rf() * 0.5f + 0.5f) * 2.0f;
        p.maxAge = (int)(4.0f / (rf() * 0.9f + 0.1f));
    }

    void move(P& p, const W& w) {
        p.px = p.x; p.py = p.y; p.pz = p.z;
        p.onGround = false;
        if (p.noClip) { p.x += p.vx; p.y += p.vy; p.z += p.vz; return; }
        float nx = p.x + p.vx;
        if (w.solid(nx, p.y, p.z)) p.vx = 0.0f; else p.x = nx;
        float ny = p.y + p.vy;
        if (w.solid(p.x, ny, p.z)) { if (p.vy < 0.0f) p.onGround = true; p.vy = 0.0f; } else p.y = ny;
        float nz = p.z + p.vz;
        if (w.solid(p.x, p.y, nz)) p.vz = 0.0f; else p.z = nz;
    }

    void update(P& p, const W& w) {
        if (p.age++ >= p.maxAge) { p.active = false; return; }
        switch (p.kind) {
            case Particles::SMOKE: case Particles::EXPLODE: {
                p.vy += 0.004f;
                const float oy = p.y;
                move(p, w);
                if (p.y == oy) { p.vx *= 1.1f; p.vz *= 1.1f; }
                p.vx *= 0.96f; p.vy *= 0.96f; p.vz *= 0.96f;
                if (p.onGround) { p.vx *= 0.7f; p.vz *= 0.7f; }
                float f = (float)p.age / (float)p.maxAge * 32.0f; if (f > 1.0f) f = 1.0f;
                p.scale = p.baseScale * f;
                break;
            }
            case Particles::SNOW: {
                p.vy -= 0.03f;
                move(p, w);
                p.vx *= 0.99f; p.vy *= 0.99f; p.vz *= 0.99f;
                if (p.onGround) { p.vx *= 0.7f; p.vz *= 0.7f; }
                float f = (float)p.age / (float)p.maxAge * 32.0f; if (f > 1.0f) f = 1.0f;
                p.scale = p.baseScale * f;
                break;
            }
            case Particles::FLAME: {
                move(p, w);
                p.vx *= 0.96f; p.vy *= 0.96f; p.vz *= 0.96f;
                const float f = (float)p.age / (float)p.maxAge;
                p.scale = p.baseScale * (1.0f - f * f * 0.5f);
                break;
            }
            case Particles::SPLASH: {
                p.vy -= 0.04f * 0.5f;
                move(p, w);
                p.vx *= 0.98f; p.vy *= 0.98f; p.vz *= 0.98f;
                if (p.onGround) p.active = false;
                break;
            }
            case Particles::LAVA: {
                p.vy -= 0.03f;
                move(p, w);
                p.vx *= 0.999f; p.vz *= 0.999f;
                if (p.onGround) { p.vx *= 0.7f; p.vz *= 0.7f; }
                const float f = (float)p.age / (float)p.maxAge;
                p.scale = p.baseScale * (1.0f - f);
                break;
            }
            case Particles::BUBBLE: {
                p.vy += 0.002f;
                move(p, w);
                p.vx *= 0.85f; p.vy *= 0.85f; p.vz *= 0.85f;
                break;
            }
            case Particles::HEART: {   // sube despacio y se encoge al final
                p.noClip = true;
                move(p, w);
                p.vx *= 0.96f; p.vy *= 0.96f; p.vz *= 0.96f;
                const float f = (float)p.age / (float)p.maxAge;
                p.scale = p.baseScale * (f < 0.8f ? 1.0f : (1.0f - f) * 5.0f);
                break;
            }
            case Particles::REDDUST: {
                move(p, w);
                p.vx *= 0.96f; p.vy *= 0.96f; p.vz *= 0.96f;
                const float f = (float)p.age / (float)p.maxAge;
                p.scale = p.baseScale * (1.0f - f * 0.5f);
                break;
            }
            default: {   // DIG: EntityFX.onUpdate
                p.vy -= 0.04f * p.gravity;
                move(p, w);
                p.vx *= 0.98f; p.vy *= 0.98f; p.vz *= 0.98f;
                if (p.onGround) { p.vx *= 0.7f; p.vz *= 0.7f; }
                break;
            }
        }
    }
}

void Particles::clear() { memset(s_p, 0, sizeof(s_p)); }

void Particles::spawn(Kind kind, float x, float y, float z, float vx, float vy, float vz, float scale) {
    if (!Options::get().particles) return;
    P& p = *alloc();
    p.active = true; p.kind = (unsigned char)kind;
    p.x = p.px = x; p.y = p.py = y; p.z = p.pz = z;
    p.gravity = 1.0f; p.light = 1.0f;
    baseMotion(p, 0.0f, 0.0f, 0.0f);
    switch (kind) {
        case SMOKE: {   // EntitySmokeFX
            p.vx = p.vx * 0.1f + vx; p.vy = p.vy * 0.1f + vy; p.vz = p.vz * 0.1f + vz;
            p.r = p.g = p.b = rf() * 0.3f;
            p.baseScale = 1.0f * 0.75f * scale; p.scale = p.baseScale;
            p.maxAge = (int)((int)(8.0f / (rf() * 0.8f + 0.2f)) * scale);
            break;
        }
        case EXPLODE: {   // EntityExplodeFX: humo gris claro
            p.vx = p.vx * 0.1f + vx; p.vy = p.vy * 0.1f + vy; p.vz = p.vz * 0.1f + vz;
            p.r = p.g = p.b = 0.5f + rf() * 0.4f;
            p.baseScale = (rf() * 0.5f + 0.5f) * 2.0f * 0.75f * scale; p.scale = p.baseScale;
            p.maxAge = (int)(8.0f / (rf() * 0.8f + 0.2f)) + 2;
            break;
        }
        case SNOW: {   // EntitySnowShovelFX
            p.vx = p.vx * 0.1f + vx; p.vy = p.vy * 0.1f + vy; p.vz = p.vz * 0.1f + vz;
            p.r = p.g = p.b = 1.0f - rf() * 0.3f;
            p.baseScale = 0.75f * scale; p.scale = p.baseScale;
            p.maxAge = (int)(8.0f / (rf() * 0.8f + 0.2f));
            break;
        }
        case FLAME: {   // EntityFlameFX: se mueve casi nada, sin choque
            p.vx = p.vx * 0.01f + vx; p.vy = p.vy * 0.01f + vy; p.vz = p.vz * 0.01f + vz;
            p.r = 1.0f; p.g = 0.72f; p.b = 0.2f;
            p.baseScale = p.scale = (rf() * 0.5f + 0.5f) * 2.0f * scale;
            p.maxAge = (int)(8.0f / (rf() * 0.8f + 0.2f)) + 4;
            p.noClip = true;
            break;
        }
        case SPLASH: {   // EntitySplashFX / lluvia: una gota que salta y cae
            p.vx = vx * 0.5f + (rf() - 0.5f) * 0.06f; p.vy = 0.12f + rf() * 0.08f + vy; p.vz = vz * 0.5f + (rf() - 0.5f) * 0.06f;
            p.r = 0.3f; p.g = 0.45f; p.b = 1.0f;
            p.baseScale = p.scale = 0.5f * scale;
            p.maxAge = 20;
            break;
        }
        case LAVA: {   // EntityLavaFX: chispa naranja que sube y cae
            p.vx = (rf() * 2.0f - 1.0f) * 0.08f; p.vy = rf() * 0.2f + 0.1f; p.vz = (rf() * 2.0f - 1.0f) * 0.08f;
            p.r = 1.0f; p.g = 0.45f; p.b = 0.05f;
            p.baseScale = p.scale = (rf() * 0.5f + 0.5f) * 0.9f * scale;
            p.maxAge = (int)(16.0f / (rf() * 0.8f + 0.2f));
            break;
        }
        case BUBBLE: {
            p.vx = p.vx * 0.2f + vx; p.vy = p.vy * 0.2f + vy; p.vz = p.vz * 0.2f + vz;
            p.r = 0.6f; p.g = 0.78f; p.b = 1.0f;
            p.baseScale = p.scale = 0.3f * scale;
            p.maxAge = (int)(8.0f / (rf() * 0.8f + 0.2f));
            break;
        }
        case HEART: {   // EntityHeartFX: rojo, sube
            p.vx = (rf() - 0.5f) * 0.02f; p.vy = 0.05f + rf() * 0.03f; p.vz = (rf() - 0.5f) * 0.02f;
            p.r = 1.0f; p.g = 0.15f; p.b = 0.25f;
            p.baseScale = p.scale = 1.4f * scale;
            p.maxAge = 20 + ri(8);
            p.noClip = true;
            break;
        }
        case REDDUST: {   // EntityReddustFX
            p.vx = p.vx * 0.1f + vx; p.vy = p.vy * 0.1f + vy; p.vz = p.vz * 0.1f + vz;
            p.r = 1.0f; p.g = 0.05f + rf() * 0.1f; p.b = 0.05f;
            p.baseScale = p.scale = 0.75f * scale;
            p.maxAge = (int)(8.0f / (rf() * 0.8f + 0.2f));
            break;
        }
        default: break;
    }
}

void Particles::blockBreak(BlockType type, float cx, float cy, float cz) {
    if (!Options::get().particles) return;
    TextureCell top, side, bottom;
    getBlockTextures(type, top, side, bottom);
    const bool leaves = isLeavesBlock(type) || type == BlockType::TALLGRASS;   // textura gris: se tine como el follaje
    for (int n = 0; n < 14; n++) {   // la Beta: 4 x 4 x 4 = 64; aqui 14
        P& p = *alloc();
        p.active = true; p.kind = DIG;
        const float ox = rf() - 0.5f, oy = rf() - 0.5f, oz = rf() - 0.5f;
        p.x = p.px = cx + ox; p.y = p.py = cy + oy; p.z = p.pz = cz + oz;
        baseMotion(p, ox, oy, oz);
        p.scale = p.baseScale = p.baseScale * 0.5f;
        p.gravity = 1.0f; p.light = 1.0f;
        p.r = p.g = p.b = 0.6f;
        if (leaves) { u8 tr, tg, tb; getGrassTint(type, false, tr, tg, tb); p.r = 0.6f * tr / 255.0f; p.g = 0.6f * tg / 255.0f; p.b = 0.6f * tb / 255.0f; }
        p.col = (unsigned char)bottom.col; p.row = (unsigned char)bottom.row;
        p.jx = (unsigned char)ri(12); p.jy = (unsigned char)ri(12);
    }
}

void Particles::blockHit(BlockType type, float cx, float cy, float cz, int dx, int dy, int dz) {
    if (!Options::get().particles) return;
    TextureCell top, side, bottom;
    getBlockTextures(type, top, side, bottom);
    P& p = *alloc();
    p.active = true; p.kind = DIG;
    float ox = (rf() - 0.5f) * 0.8f, oy = (rf() - 0.5f) * 0.8f, oz = (rf() - 0.5f) * 0.8f;
    if (dx) ox = 0.6f * (float)dx; if (dy) oy = 0.6f * (float)dy; if (dz) oz = 0.6f * (float)dz;
    p.x = p.px = cx + ox; p.y = p.py = cy + oy; p.z = p.pz = cz + oz;
    baseMotion(p, 0.0f, 0.0f, 0.0f);
    p.vx *= 0.2f; p.vy = (p.vy - 0.1f) * 0.2f + 0.1f; p.vz *= 0.2f;   // func_407_b(0.2)
    p.scale = p.baseScale = p.baseScale * 0.5f * 0.6f;
    p.gravity = 1.0f; p.light = 1.0f;
    p.r = p.g = p.b = 0.6f;
    if (isLeavesBlock(type) || type == BlockType::TALLGRASS) { u8 tr, tg, tb; getGrassTint(type, false, tr, tg, tb); p.r = 0.6f * tr / 255.0f; p.g = 0.6f * tg / 255.0f; p.b = 0.6f * tb / 255.0f; }
    p.col = (unsigned char)bottom.col; p.row = (unsigned char)bottom.row;
    p.jx = (unsigned char)ri(12); p.jy = (unsigned char)ri(12);
}

void Particles::explosion(float x, float y, float z, float size) {
    if (!Options::get().particles) return;
    const float r = size * 0.5f;
    for (int n = 0; n < 10; n++) {
        const float dx = (rf() - 0.5f) * 2.0f * r, dy = (rf() - 0.5f) * 2.0f * r, dz = (rf() - 0.5f) * 2.0f * r;
        spawn(EXPLODE, x + dx, y + dy, z + dz, dx * 0.1f, dy * 0.1f, dz * 0.1f, 1.4f);
    }
    for (int n = 0; n < 6; n++) {
        const float dx = (rf() - 0.5f) * 2.0f * r, dy = (rf() - 0.5f) * 2.0f * r, dz = (rf() - 0.5f) * 2.0f * r;
        spawn(SMOKE, x + dx, y + dy, z + dz, dx * 0.05f, dy * 0.05f + 0.02f, dz * 0.05f, 1.8f);
    }
}

void Particles::spawnerBurst(float cx, float cy, float cz) {
    if (!Options::get().particles) return;
    for (int n = 0; n < 6; n++) {
        const float x = cx + (rf() - 0.5f) * 2.0f, y = cy + (rf() - 0.5f) * 2.0f, z = cz + (rf() - 0.5f) * 2.0f;
        spawn(SMOKE, x, y, z, 0.0f, 0.0f, 0.0f);
        spawn(FLAME, x, y, z, 0.0f, 0.0f, 0.0f);
    }
}

void Particles::tick(const ChunkManager& cm, float px, float py, float pz) {
    W w(cm);
    for (int i = 0; i < MAX_PARTICLES; i++) {
        P& p = s_p[i];
        if (!p.active) continue;
        update(p, w);
        if (p.active && ((p.age + i) % 5) == 0) p.light = brightnessOf(HandAnimations::lightLevelAt(cm, p.x, p.y, p.z));
    }

    if (!Options::get().particles) return;

    // randomDisplayTick: puntos al azar a 8 bloques del jugador (la Beta mira 1000 a 16: aqui menos, y las particulas se ponen mas pocas)
    const int ci = (int)floorf(px), cj = (int)floorf(py - 1.62f + 0.5f), ck = (int)floorf(pz);
    for (int n = 0; n < 48; n++) {
        const int i = ci + ri(17) - 8, j = cj + ri(9) - 4, k = ck + ri(17) - 8;
        if (j < 0 || j > 126) continue;
        Chunk* c = cm.getChunkByChunkCoord((i + 4) >> 3, (k + 4) >> 3);
        if (!c || !c->isGenerationComplete()) continue;
        const int xi = (i + 4) & 7, zi = (k + 4) & 7;
        const BlockType t = c->getBlockType(xi, j + 1, zi);
        if (t == BlockType::AIR) continue;
        const float bx = (float)i + 0.5f, by = (float)j, bz = (float)k + 0.5f;   // centro de la celda
        if (isTorchBlock(t) || t == BlockType::TORCH_REDSTONE_ON) {
            float x = bx, y = by + 0.2f, z = bz;   // la Beta: y + 0,7 desde el piso de la celda
            if (t == BlockType::TORCH_W) { x -= 0.27f; y += 0.22f; }
            else if (t == BlockType::TORCH_E) { x += 0.27f; y += 0.22f; }
            else if (t == BlockType::TORCH_N) { z -= 0.27f; y += 0.22f; }
            else if (t == BlockType::TORCH_S) { z += 0.27f; y += 0.22f; }
            if (t == BlockType::TORCH_REDSTONE_ON) {
                spawn(REDDUST, x + (rf() - 0.5f) * 0.2f, y + (rf() - 0.5f) * 0.2f, z + (rf() - 0.5f) * 0.2f, 0.0f, 0.0f, 0.0f);
            } else {
                spawn(SMOKE, x, y, z, 0.0f, 0.0f, 0.0f);
                spawn(FLAME, x, y, z, 0.0f, 0.0f, 0.0f);
            }
        } else if (t == BlockType::FIRE) {
            if (ri(3) == 0) spawn(SMOKE, (float)i + rf(), (float)j - 0.5f + rf(), (float)k + rf(), 0.0f, 0.0f, 0.0f, 2.5f);
        } else if (isLavaBlock(t)) {
            if (c->getBlockType(xi, j + 2, zi) == BlockType::AIR && ri(25) == 0) spawn(LAVA, (float)i + rf(), (float)j + 0.5f, (float)k + rf(), 0.0f, 0.0f, 0.0f);
        } else if (t == BlockType::FURNACE_LIT_S || t == BlockType::FURNACE_LIT_W || t == BlockType::FURNACE_LIT_N || t == BlockType::FURNACE_LIT_E) {
            const float d = 0.52f, r = rf() * 0.6f - 0.3f, h = rf() * 6.0f / 16.0f - 0.5f + 0.1f;
            float x = bx, z = bz;
            if (t == BlockType::FURNACE_LIT_W) { x -= d; z += r; }
            else if (t == BlockType::FURNACE_LIT_E) { x += d; z += r; }
            else if (t == BlockType::FURNACE_LIT_N) { z -= d; x += r; }
            else { z += d; x += r; }
            spawn(SMOKE, x, by + h, z, 0.0f, 0.0f, 0.0f);
            spawn(FLAME, x, by + h, z, 0.0f, 0.0f, 0.0f);
        }
    }
}

void Particles::draw(const ChunkManager& cm, float ex, float ey, float ez, float yawDeg, float pitchDeg) {
    (void)cm;
    if (!Options::get().particles) return;
    const float yr = yawDeg * 0.0174532925f, pr = pitchDeg * 0.0174532925f;
    const float rx = cosf(yr), rz = -sinf(yr);                                   // la derecha de la camara
    const float ux = sinf(pr) * sinf(yr), uy = cosf(pr), uz = sinf(pr) * cosf(yr);   // y arriba
    glMatrixMode(GL_MODELVIEW);
    for (int pass = 0; pass < 2; pass++) {   // 0: color plano, 1: terrain.png (bloques)
        bool began = false;
        for (int i = 0; i < MAX_PARTICLES; i++) {
            const P& p = s_p[i];
            if (!p.active || (p.kind == DIG) != (pass == 1)) continue;
            const float dx = p.x - ex, dy = p.y - ey, dz = p.z - ez;
            if (dx * dx + dy * dy + dz * dz > 40.0f * 40.0f) continue;
            if (!began) {
                began = true;
                glBindTexture(0, pass == 1 ? Texture::textureID : 0);
                glPolyFmt(POLY_ALPHA(31) | POLY_CULL_NONE | POLY_FOG | POLY_ID(46 + pass));
            }
            float k = p.light, cr = p.r, cg = p.g, cb = p.b;
            if (p.kind == FLAME) {   // EntityFlameFX.getEntityBrightness: arde a pleno y se apaga con la edad; el color pasa de amarillo a rojo oscuro
                const float f = (float)p.age / (float)p.maxAge;
                k = 1.0f; cr = 1.0f - 0.25f * f; cg = 0.78f - 0.6f * f; cb = 0.25f - 0.2f * f;
            }
            glColor3b((int)(cr * k * 255.0f), (int)(cg * k * 255.0f), (int)(cb * k * 255.0f));
            const float s = 0.1f * p.scale;
            glPushMatrix();
            glTranslatef(p.x, p.y, p.z);
            glBegin(GL_QUADS);
            if (pass == 1) {
                const int tu = p.col * 16 + p.jx, tv = p.row * 16 + p.jy;
                const int u0 = tu * 16, u1 = (tu + 4) * 16 - 1, v0 = tv * 16, v1 = (tv + 4) * 16 - 1;
                GFX_TEX_COORD = TEXTURE_PACK((s16)u0, (s16)v1); glVertex3v16(floattov16(-rx * s - ux * s), floattov16(-uy * s), floattov16(-rz * s - uz * s));
                GFX_TEX_COORD = TEXTURE_PACK((s16)u0, (s16)v0); glVertex3v16(floattov16(-rx * s + ux * s), floattov16(uy * s), floattov16(-rz * s + uz * s));
                GFX_TEX_COORD = TEXTURE_PACK((s16)u1, (s16)v0); glVertex3v16(floattov16(rx * s + ux * s), floattov16(uy * s), floattov16(rz * s + uz * s));
                GFX_TEX_COORD = TEXTURE_PACK((s16)u1, (s16)v1); glVertex3v16(floattov16(rx * s - ux * s), floattov16(-uy * s), floattov16(rz * s - uz * s));
            } else {
                glVertex3v16(floattov16(-rx * s - ux * s), floattov16(-uy * s), floattov16(-rz * s - uz * s));
                glVertex3v16(floattov16(-rx * s + ux * s), floattov16(uy * s), floattov16(-rz * s + uz * s));
                glVertex3v16(floattov16(rx * s + ux * s), floattov16(uy * s), floattov16(rz * s + uz * s));
                glVertex3v16(floattov16(rx * s - ux * s), floattov16(-uy * s), floattov16(rz * s - uz * s));
            }
            glEnd();
            glPopMatrix(1);
        }
    }
}
