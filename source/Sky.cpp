#include "Sky.h"
#include "Texture.h"
#include <nds.h>
#include <math.h>

#define SKY_GLOW 0   // resplandor del amanecer/atardecer desactivado: en la DS se ve como un plano grande sobre el horizonte (1 para volver a probarlo)

namespace {
    const float PI_F = 3.14159265f;
    // La Beta dibuja a 100 bloques; el plano lejano de la DS esta a 40, asi que todo va a 30 (escala 0,3). Los vertices van en v16 (maximo 8),
    // por eso se dividen entre 16 y la matriz los multiplica de vuelta (16 * 0,3 = 4,8).
    const float K = 0.3f * 16.0f;
    const float K_GLOW = 0.25f * 16.0f;   // el abanico llega a ~126 bloques: a 0,3 pasaba el plano lejano (40) y se recortaba en un cuadro recto
    inline int v(float x) { return floattov16(x / 16.0f); }
    inline void vert(float x, float y, float z) { glVertex3v16(v(x), v(y), v(z)); }
    inline void uv(int u, int w) { GFX_TEX_COORD = TEXTURE_PACK(u << 4, w << 4); }
}

void Sky::draw(float ex, float ey, float ez, float angle, float rain, int skyR, int skyG, int skyB) {
    const float th = angle * PI_F * 2.0f;
    const float c = cosf(th);   // altura del sol sobre el horizonte (-1 a 1); la luna esta en -c

    glMatrixMode(GL_MODELVIEW);

    // WorldProvider.calcSunriseSunsetColors + el abanico de RenderGlobal.renderSky: del color del resplandor en el centro al color del cielo en el borde
    if (SKY_GLOW && c >= -0.4f && c <= 0.4f) {
        const float f = c / 0.4f * 0.5f + 0.5f;
        float a = 1.0f - (1.0f - sinf(f * PI_F)) * 0.99f;
        a *= a;
        const float gr = f * 0.3f + 0.7f, gg = f * f * 0.7f + 0.2f, gb = 0.2f;
        const int cr = (int)((skyR * (1.0f - a) + gr * 31.0f * a) * 8.0f), cg = (int)((skyG * (1.0f - a) + gg * 31.0f * a) * 8.0f), cb = (int)((skyB * (1.0f - a) + gb * 31.0f * a) * 8.0f);
        glPushMatrix();
        glTranslatef(ex, ey, ez);
        glScalef(K_GLOW, K_GLOW, K_GLOW);
        glRotatef(90.0f, 1.0f, 0.0f, 0.0f);
        glRotatef(sinf(th) < 0.0f ? 180.0f : 0.0f, 0.0f, 0.0f, 1.0f);
        glRotatef(90.0f, 0.0f, 0.0f, 1.0f);
        glBindTexture(0, 0);
        glPolyFmt(POLY_ALPHA(30) | POLY_CULL_NONE | POLY_ID(40));   // 30 de 31: no escribe profundidad y se mezcla con el fondo
        glBegin(GL_TRIANGLES);
        float px[17], py[17], pz[17];
        for (int i = 0; i <= 16; i++) {
            const float t = (float)i * PI_F * 2.0f / 16.0f, s = sinf(t), co = cosf(t);
            px[i] = s * 120.0f; py[i] = co * 120.0f; pz[i] = -co * 40.0f * a;
        }
        for (int i = 0; i < 16; i++) {
            glColor3b(cr > 255 ? 255 : cr, cg > 255 ? 255 : cg, cb > 255 ? 255 : cb); vert(0.0f, 100.0f, 0.0f);
            glColor3b(skyR * 8, skyG * 8, skyB * 8); vert(px[i], py[i], pz[i]);
            glColor3b(skyR * 8, skyG * 8, skyB * 8); vert(px[i + 1], py[i + 1], pz[i + 1]);
        }
        glEnd();
        glPopMatrix(1);
    }

    int alpha = (int)(30.0f * (1.0f - rain) + 0.5f);   // la lluvia los apaga (color4f(1,1,1,1 - lluvia))
    if (alpha < 1) return;

    // el sol y la luna se esconden poco antes de ponerse: debajo del horizonte el borde del mundo (sin terreno lejano) dejaria verlos
    const bool showSun = c > -0.1f && Texture::skySunTextureID != 0;
    const bool showMoon = c < 0.1f && Texture::skyMoonTextureID != 0;
    if (!showSun && !showMoon) return;

    glPolyFmt(POLY_ALPHA(alpha) | POLY_CULL_NONE | POLY_ID(41));
    glColor3b(255, 255, 255);
    if (showSun) {
        glPushMatrix();
        glTranslatef(ex, ey, ez);
        glScalef(K, K, K);
        glRotatef(-90.0f, 0.0f, 1.0f, 0.0f);
        glRotatef(angle * 360.0f, 1.0f, 0.0f, 0.0f);
        glBindTexture(0, Texture::skySunTextureID);
        glBegin(GL_QUADS);
        uv(0, 0);   vert(-30.0f, 100.0f, -30.0f);
        uv(32, 0);  vert(30.0f, 100.0f, -30.0f);
        uv(32, 32); vert(30.0f, 100.0f, 30.0f);
        uv(0, 32);  vert(-30.0f, 100.0f, 30.0f);
        glEnd();
        glPopMatrix(1);
    }
    if (showMoon) {
        glPolyFmt(POLY_ALPHA(alpha) | POLY_CULL_NONE | POLY_ID(42));
        glPushMatrix();
        glTranslatef(ex, ey, ez);
        glScalef(K, K, K);
        glRotatef(-90.0f, 0.0f, 1.0f, 0.0f);
        glRotatef(angle * 360.0f, 1.0f, 0.0f, 0.0f);
        glBindTexture(0, Texture::skyMoonTextureID);
        glBegin(GL_QUADS);
        uv(32, 32); vert(-20.0f, -100.0f, 20.0f);
        uv(0, 32);  vert(20.0f, -100.0f, 20.0f);
        uv(0, 0);   vert(20.0f, -100.0f, -20.0f);
        uv(32, 0);  vert(-20.0f, -100.0f, -20.0f);
        glEnd();
        glPopMatrix(1);
    }
    glPolyFmt(POLY_ALPHA(31) | POLY_CULL_NONE | POLY_FOG);   // el formato base de Renderer::setupProjection
}
