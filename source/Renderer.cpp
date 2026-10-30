#include "Renderer.h"
#include "WorldConfig.h"

void Renderer::init() {
    vramSetBankA(VRAM_A_TEXTURE);
    vramSetBankD(VRAM_D_TEXTURE);       // banco D en vez de B — libre, nadie lo reutiliza dinámicamente
    vramSetBankF(VRAM_F_TEX_PALETTE_SLOT0);
    vramSetBankG(VRAM_G_TEX_PALETTE_SLOT1);

    videoSetMode(MODE_0_3D);
    glInit();
    glDisable(GL_ANTIALIAS);
    glEnable(GL_TEXTURE_2D);
    glClearColor(0, 0, 20, 31);
    glClearDepth(GL_MAX_DEPTH);
    glViewport(0, 0, 255, 191);
}

void Renderer::setupProjection() {
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    const float NEAR_PLANE = 0.1f, FAR_PLANE = 40.0f;
    gluPerspective(70, 256.0 / 192.0, NEAR_PLANE, FAR_PLANE);

    glPolyFmt(POLY_ALPHA(31) | POLY_CULL_NONE | POLY_FOG);

    // La niebla sale de la distancia de render del modo (fast / ds / dsi): empieza a 60 % de la distancia y llega al maximo a 85 %,
    // asi el borde del mundo queda tapado en cualquier modo. La profundidad del hardware (15 bits) no es lineal: se calcula con la
    // misma formula de la proyeccion. 32 escalones de niebla, cada uno de (0x400 >> shift) unidades de profundidad.
    const float rd = WorldConfig::renderDistance();
    const float F = FAR_PLANE, N = NEAR_PLANE;
    float d0 = ((F + N) / (F - N) - 2.0f * F * N / ((F - N) * (rd * 0.60f))) * 0.5f + 0.5f;
    float d1 = ((F + N) / (F - N) - 2.0f * F * N / ((F - N) * (rd * 0.85f))) * 0.5f + 0.5f;
    d0 *= 32767.0f; d1 *= 32767.0f;
    const float need = (d1 - d0) / 32.0f;
    int shift = 0;
    while (shift < 10 && (float)(0x400 >> (shift + 1)) >= need) shift++;   // el escalon mas fino que aun cubre el tramo
    int offset = (int)d0;
    if (offset < 0) offset = 0;
    if (offset > 32767) offset = 32767;
    glFogShift(shift);
    glFogOffset(offset);

    for (int i = 0; i < 32; i++) {
        int density = (i * 127) / 31;
        glFogDensity(i, density);
    }

    glFogColor(20, 20, 31, 31); // se sobreescribe cada frame en main.cpp según la hora
    glEnable(GL_FOG);
}