#include "TntManager.h"
#include "ChunkManager.h"
#include "HostileManager.h"
#include "Sound.h"
#include "HandAnimations.h"
#include "Texture.h"
#include "BlockType.h"
#include "WorldConfig.h"
#include <nds.h>
#include <math.h>
#include <stdlib.h>

#define V(v) floattov16(v)

namespace {
    struct Tnt {
        bool active;
        float x, y, z;            // centro del cubo, en coordenadas de mundo
        float prevX, prevY, prevZ;
        float mx, my, mz;
        bool onGround;
        int fuse;
    };
    Tnt s_tnt[TntManager::MAX_TNT];

    const float HALF = 0.49f;     // la caja de la entidad mide 0,98

    // ¿choca la caja centrada en (x, y, z)? Se prueban las 8 esquinas y los puntos medios de las aristas verticales
    bool blocked(const ChunkManager& cm, float x, float y, float z) {
        for (int a = -1; a <= 1; a += 2)
            for (int b = -1; b <= 1; b += 2)
                for (int c = -1; c <= 1; c += 2)
                    if (cm.isSolidAt(x + a * HALF, y + b * HALF, z + c * HALF)) return true;
        return false;
    }

    // Mueve en un eje de a pasos de 0,25 y, si choca, se queda donde pudo (la mitad, cuatro veces). Devuelve lo que avanzo
    float moveAxis(const ChunkManager& cm, Tnt& t, int axis, float delta) {
        float* p = axis == 0 ? &t.x : (axis == 1 ? &t.y : &t.z);
        float moved = 0.0f;
        while (fabsf(delta - moved) > 1.0e-5f) {
            float step = delta - moved;
            if (step > 0.25f) step = 0.25f; else if (step < -0.25f) step = -0.25f;
            const float old = *p;
            *p = old + step;
            if (blocked(cm, t.x, t.y, t.z)) {
                float s = step;
                *p = old;
                for (int i = 0; i < 4; i++) {
                    s *= 0.5f;
                    *p += s;
                    if (blocked(cm, t.x, t.y, t.z)) *p -= s;
                }
                return *p - (old - moved);   // lo que avanzo en total (sin llegar a donde queria)
            }
            moved += step;
        }
        return moved;
    }

    float brightnessOf(unsigned char level) {
        float f = 1.0f - (float)level / 15.0f;
        return (1.0f - f) / (f * 3.0f + 1.0f) * 0.95f + 0.05f;
    }

    void texCoordCell(int col, int row, int corner) {
        const int u0 = col * 16, v0 = row * 16, u1 = u0 + 16, v1 = v0 + 16;
        switch (corner) {
            case 0: GFX_TEX_COORD = TEXTURE_PACK(inttot16(u0), inttot16(v1)); break;
            case 1: GFX_TEX_COORD = TEXTURE_PACK(inttot16(u1), inttot16(v1)); break;
            case 2: GFX_TEX_COORD = TEXTURE_PACK(inttot16(u1), inttot16(v0)); break;
            case 3: GFX_TEX_COORD = TEXTURE_PACK(inttot16(u0), inttot16(v0)); break;
        }
    }

    // Un cubo de lado 2 * s con las texturas de terrain.png de 'type' (si textured), o liso
    void drawCube(BlockType type, float s, bool textured) {
        TextureCell top, side, bottom;
        getBlockTextures(type, top, side, bottom);
        glBegin(GL_QUADS);
            #define TC(c, k) if (textured) texCoordCell(c.col, c.row, k)
            TC(side, 0); glVertex3v16(V(-s), V(-s), V( s)); TC(side, 1); glVertex3v16(V( s), V(-s), V( s)); TC(side, 2); glVertex3v16(V( s), V( s), V( s)); TC(side, 3); glVertex3v16(V(-s), V( s), V( s));
            TC(side, 0); glVertex3v16(V( s), V(-s), V(-s)); TC(side, 1); glVertex3v16(V(-s), V(-s), V(-s)); TC(side, 2); glVertex3v16(V(-s), V( s), V(-s)); TC(side, 3); glVertex3v16(V( s), V( s), V(-s));
            TC(side, 0); glVertex3v16(V(-s), V(-s), V(-s)); TC(side, 1); glVertex3v16(V(-s), V(-s), V( s)); TC(side, 2); glVertex3v16(V(-s), V( s), V( s)); TC(side, 3); glVertex3v16(V(-s), V( s), V(-s));
            TC(side, 0); glVertex3v16(V( s), V(-s), V( s)); TC(side, 1); glVertex3v16(V( s), V(-s), V(-s)); TC(side, 2); glVertex3v16(V( s), V( s), V(-s)); TC(side, 3); glVertex3v16(V( s), V( s), V( s));
            TC(top, 0);  glVertex3v16(V(-s), V( s), V( s)); TC(top, 1);  glVertex3v16(V( s), V( s), V( s)); TC(top, 2);  glVertex3v16(V( s), V( s), V(-s)); TC(top, 3);  glVertex3v16(V(-s), V( s), V(-s));
            TC(bottom, 0); glVertex3v16(V(-s), V(-s), V(-s)); TC(bottom, 1); glVertex3v16(V( s), V(-s), V(-s)); TC(bottom, 2); glVertex3v16(V( s), V(-s), V( s)); TC(bottom, 3); glVertex3v16(V(-s), V(-s), V( s));
            #undef TC
        glEnd();
    }

    void spawn(int gx, int gy, int gz, int fuse, bool randomMotion) {
        for (int i = 0; i < TntManager::MAX_TNT; i++) {
            Tnt& t = s_tnt[i];
            if (t.active) continue;
            t.active = true;
            t.x = (float)(gx - WorldConfig::SIZE_X / 2) + 0.5f; t.y = (float)(gy - 1); t.z = (float)(gz - WorldConfig::SIZE_Z / 2) + 0.5f;
            t.prevX = t.x; t.prevY = t.y; t.prevZ = t.z;
            const float a = (float)(rand() % 6283) / 1000.0f;   // un giro al azar (Beta lo da en grados al seno: casi no se mueve de lado)
            t.mx = randomMotion ? -sinf(a * 3.14159265f / 180.0f) * 0.02f : 0.0f;
            t.my = randomMotion ? 0.2f : 0.0f;
            t.mz = randomMotion ? -cosf(a * 3.14159265f / 180.0f) * 0.02f : 0.0f;
            t.onGround = false;
            t.fuse = fuse;
            Sound::fuse(t.x, t.y, t.z);
            return;
        }
        // sin sitio: explota en el acto para no perder el bloque
        HostileManager::explodeAt((double)(gx - WorldConfig::SIZE_X / 2) + 0.5, (double)(gy - 1), (double)(gz - WorldConfig::SIZE_Z / 2) + 0.5, 4.0f, false);
    }
}

void TntManager::clear() { for (int i = 0; i < MAX_TNT; i++) s_tnt[i].active = false; }

void TntManager::primeCell(int gx, int gy, int gz) { spawn(gx, gy, gz, 80, true); }

void TntManager::primeFromExplosion(int gx, int gy, int gz) { spawn(gx, gy, gz, rand() % 20 + 10, true); }   // nextInt(80 / 4) + 80 / 8

void TntManager::tick(const ChunkManager& cm) {
    for (int i = 0; i < MAX_TNT; i++) {
        Tnt& t = s_tnt[i];
        if (!t.active) continue;
        t.prevX = t.x; t.prevY = t.y; t.prevZ = t.z;
        t.my -= 0.04f;
        t.onGround = false;
        const float dy = t.my;
        const float movedY = moveAxis(cm, t, 1, dy);
        if (fabsf(movedY - dy) > 1.0e-4f) { if (dy < 0.0f) t.onGround = true; if (dy != 0.0f) t.my = 0.0f; }
        const float dx = t.mx, dz = t.mz;
        if (fabsf(moveAxis(cm, t, 0, dx) - dx) > 1.0e-4f) t.mx = 0.0f;
        if (fabsf(moveAxis(cm, t, 2, dz) - dz) > 1.0e-4f) t.mz = 0.0f;
        t.mx *= 0.98f; t.my *= 0.98f; t.mz *= 0.98f;
        if (t.onGround) { t.mx *= 0.7f; t.mz *= 0.7f; t.my *= -0.5f; }

        if (t.fuse-- <= 0) {
            t.active = false;
            HostileManager::explodeAt((double)t.x, (double)t.y, (double)t.z, 4.0f, false);   // EntityTNTPrimed.explode: createExplosion(fuerza 4)
        }
        if (t.y < -30.0f) t.active = false;   // cayo al vacio
    }
}

void TntManager::draw(const ChunkManager& cm, float playerX, float playerY, float playerZ, float partialTick) {
    const float maxDist = WorldConfig::renderDistance();
    for (int i = 0; i < MAX_TNT; i++) {
        const Tnt& t = s_tnt[i];
        if (!t.active) continue;
        const float x = t.prevX + (t.x - t.prevX) * partialTick;
        const float y = t.prevY + (t.y - t.prevY) * partialTick;
        const float z = t.prevZ + (t.z - t.prevZ) * partialTick;
        const float dx = x - playerX, dy = y - playerY, dz = z - playerZ;
        if (dx * dx + dy * dy + dz * dz > maxDist * maxDist) continue;

        // RenderTNTPrimed: en los ultimos 10 ticks crece hasta 1,3 veces; parpadea en blanco cada 5 ticks, cada vez mas opaco
        const float left = (float)t.fuse - partialTick + 1.0f;
        float scale = 1.0f;
        if (left < 10.0f) {
            float f = 1.0f - left / 10.0f;
            if (f < 0.0f) f = 0.0f; if (f > 1.0f) f = 1.0f;
            f *= f; f *= f;
            scale = 1.0f + f * 0.3f;
        }
        float flash = (1.0f - left / 100.0f) * 0.8f;
        if (flash < 0.0f) flash = 0.0f; if (flash > 1.0f) flash = 1.0f;

        const int v = (int)(brightnessOf(HandAnimations::lightLevelAt(cm, x, y, z)) * 255.0f + 0.5f);
        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
            glTranslatef(x, y, z);
            glScalef(scale, scale, scale);
            glPolyFmt(POLY_ALPHA(31) | POLY_CULL_NONE | POLY_FOG);
            glBindTexture(0, Texture::textureID);
            glColor3b(v, v, v);
            drawCube(BlockType::TNT, 0.49f, true);
            if ((t.fuse / 5) % 2 == 0) {
                const int a = (int)(flash * 31.0f);
                if (a > 0) {
                    GFX_TEX_FORMAT = 0;   // sin textura: el destello es un cubo blanco liso
                    glPolyFmt(POLY_ALPHA(a) | POLY_CULL_NONE | POLY_ID(41));
                    glColor3b(255, 255, 255);
                    drawCube(BlockType::TNT, 0.495f, false);
                    glBindTexture(0, Texture::textureID);
                }
            }
        glPopMatrix(1);
    }
    glPolyFmt(POLY_ALPHA(31) | POLY_CULL_NONE | POLY_FOG);
}
