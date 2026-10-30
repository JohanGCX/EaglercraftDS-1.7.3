#include "MenuPanorama.h"
#include <nds.h>
#include <math.h>
#include "panorama.h"   // grit: panoramaBitmap, panoramaPal (256 x 128, 8 bits)

static int s_tex = -1;

// Celdas de 64 x 64 dentro de la textura de 256 x 128: los 4 costados en la fila de arriba; el techo y el piso en la de abajo
static const int CELL_X[6] = { 0, 64, 128, 192, 0, 64 };
static const int CELL_Y[6] = { 0,  0,   0,   0, 64, 64 };

bool MenuPanorama::load() {
    if (s_tex >= 0) return true;

    int id = 0;
    glGenTextures(1, &id);
    glBindTexture(0, id);
    int ok = glTexImage2D(0, 0, GL_RGB256, TEXTURE_SIZE_256, TEXTURE_SIZE_128, 0, TEXGEN_TEXCOORD, (u8*)panoramaBitmap);
    if (!ok) {              // sin VRAM libre para 32 KB
        glDeleteTextures(1, &id);
        return false;
    }
    glColorTableEXT(0, 0, 256, 0, 0, (u16*)panoramaPal);
    s_tex = id;
    return true;
}

void MenuPanorama::unload() {
    if (s_tex < 0) return;
    int id = s_tex;
    glDeleteTextures(1, &id);
    s_tex = -1;
}

// GuiMainMenu.renderSkybox de la versión original, con la misma cadena de matrices: giro de 180 grados en X (para ver el cubo desde
// adentro), inclinación sin(t / 400) * 25 + 20 grados, y giro de -0,1 grados por tick. Sin el desenfoque de 64 pasadas (no existe en DS).
// El campo de visión es de 90 grados en vertical (el original usaba 120 en una vista cuadrada).
void MenuPanorama::draw(float ticks) {
    if (s_tex < 0) return;

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(90, 256.0 / 192.0, 0.1, 10.0);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glPolyFmt(POLY_ALPHA(31) | POLY_CULL_NONE | POLY_ID(0)); // sin niebla: el cubo está a 1 bloque de la cámara
    glColor3b(255, 255, 255);
    glBindTexture(0, s_tex);

    glRotatef(180.0f, 1.0f, 0.0f, 0.0f);
    glRotatef(sinf(ticks / 400.0f) * 25.0f + 20.0f, 1.0f, 0.0f, 0.0f);
    glRotatef(-ticks * 0.1f, 0.0f, 1.0f, 0.0f);

    const v16 S = 4096;      // 1 en punto fijo 4.12
    for (int j = 0; j < 6; j++) {
        glPushMatrix();
        if (j == 1) glRotatef(90.0f, 0.0f, 1.0f, 0.0f);
        if (j == 2) glRotatef(180.0f, 0.0f, 1.0f, 0.0f);
        if (j == 3) glRotatef(-90.0f, 0.0f, 1.0f, 0.0f);
        if (j == 4) glRotatef(90.0f, 1.0f, 0.0f, 0.0f);
        if (j == 5) glRotatef(-90.0f, 1.0f, 0.0f, 0.0f);

        // coordenadas de textura en 12.4 (1/16 de texel), con un cuarto de texel de margen para no leer la celda vecina
        const int u0 = CELL_X[j] * 16 + 4, u1 = (CELL_X[j] + 64) * 16 - 4;
        const int v0 = CELL_Y[j] * 16 + 4, v1 = (CELL_Y[j] + 64) * 16 - 4;

        glBegin(GL_QUADS);
            GFX_TEX_COORD = TEXTURE_PACK(u0, v0); glVertex3v16(-S, -S, S);
            GFX_TEX_COORD = TEXTURE_PACK(u1, v0); glVertex3v16( S, -S, S);
            GFX_TEX_COORD = TEXTURE_PACK(u1, v1); glVertex3v16( S,  S, S);
            GFX_TEX_COORD = TEXTURE_PACK(u0, v1); glVertex3v16(-S,  S, S);
        glEnd();
        glPopMatrix(1);
    }
}