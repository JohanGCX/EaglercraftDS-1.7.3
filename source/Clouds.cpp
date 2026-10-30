#include "Clouds.h"
#include "CloudsData.h"
#include <nds.h>
#include <math.h>

namespace {
    int s_offset = 0;
    const float CLOUD_Y = 108.0f + 0.33f - 0.5f;   // la base (la Beta 108,33; aqui el bloque j cubre j - 0,5 a j + 0,5)
    const float THICK = 4.0f, CELL = 12.0f;
    const int R = 5;

    inline bool cloudAt(int p, int q) {
        p &= 255; q &= 255;
        return (CLOUD_BITS[q * 8 + (p >> 5)] >> (p & 31)) & 1u;
    }

    inline void quad(float x0, float y0, float z0, float x1, float y1, float z1, float x2, float y2, float z2, float x3, float y3, float z3) {
        glVertex3v16(floattov16(x0), floattov16(y0), floattov16(z0));
        glVertex3v16(floattov16(x1), floattov16(y1), floattov16(z1));
        glVertex3v16(floattov16(x2), floattov16(y2), floattov16(z2));
        glVertex3v16(floattov16(x3), floattov16(y3), floattov16(z3));
    }
    inline void shade(float base[3], float k) { glColor3b((int)(base[0] * k * 255.0f), (int)(base[1] * k * 255.0f), (int)(base[2] * k * 255.0f)); }
}

void Clouds::reset() { s_offset = 0; }
void Clouds::tick() { ++s_offset; }

void Clouds::draw(float pt, float ex, float ey, float ez, float sky, float rain, float thunder, bool dsiMode) {
    if (!dsiMode && ey < 80.0f) return;       // desde el suelo, solo en dsi
    if (ey < CLOUD_Y - 60.0f) return;

    // World.func_628_d: el color de las nubes
    float c[3] = { 1.0f, 1.0f, 1.0f };
    if (rain > 0.0f) {
        const float g = 0.6f, k = 1.0f - rain * 0.95f;
        for (int i = 0; i < 3; i++) c[i] = c[i] * k + g * (1.0f - k);
    }
    c[0] *= sky * 0.9f + 0.1f; c[1] *= sky * 0.9f + 0.1f; c[2] *= sky * 0.85f + 0.15f;
    if (thunder > 0.0f) {
        const float g = (c[0] * 0.3f + c[1] * 0.59f + c[2] * 0.11f) * 0.2f, k = 1.0f - thunder * 0.95f;
        for (int i = 0; i < 3; i++) c[i] = c[i] * k + g * (1.0f - k);
    }

    // la celda (P, Q) cubre de P * 12 - desplazamiento a +12 en el mundo; el desplazamiento mueve las nubes 0,03 bloques por tick (el original, hacia -x)
    const float drift = ((float)s_offset + pt) * 0.03f;
    const int pc = (int)floorf((ex + drift) / CELL), qc = (int)floorf((ez + 3.96f) / CELL);
    const float baseY = CLOUD_Y, topY = CLOUD_Y + THICK;
    const bool below = ey < topY, above = ey > baseY;

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glBindTexture(0, 0);
    glPolyFmt(POLY_ALPHA(25) | POLY_CULL_NONE | POLY_ID(44));   // sin POLY_FOG: no se pierden en la niebla
    for (int q = qc - R; q <= qc + R; q++) {
        int p = pc - R;
        while (p <= pc + R) {
            if (!cloudAt(p, q)) { p++; continue; }
            int e = p;
            while (e + 1 <= pc + R && cloudAt(e + 1, q)) e++;
            const int n = e - p + 1;   // la racha p .. e
            const float wx = (float)p * CELL - drift, wz = (float)q * CELL - 3.96f;   // esquina de la racha, en el mundo
            glPushMatrix();
            glTranslatef(wx, baseY, wz);
            glScalef(CELL, THICK, CELL);
            glBegin(GL_QUADS);
            if (below) { shade(c, 0.7f); quad(0, 0, 0, (float)n, 0, 0, (float)n, 0, 1, 0, 0, 1); }
            if (above) { shade(c, 1.0f); quad(0, 1, 0, (float)n, 1, 0, (float)n, 1, 1, 0, 1, 1); }
            // paredes: solo en las celdas cercanas (a 3 cuadros), y solo donde no hay otra nube al lado
            for (int i = p; i <= e; i++) {
                if (abs(i - pc) > 3 || abs(q - qc) > 3) continue;
                const float lx = (float)(i - p);
                if (!cloudAt(i, q - 1)) { shade(c, 0.8f); quad(lx, 0, 0, lx + 1, 0, 0, lx + 1, 1, 0, lx, 1, 0); }
                if (!cloudAt(i, q + 1)) { shade(c, 0.8f); quad(lx, 0, 1, lx + 1, 0, 1, lx + 1, 1, 1, lx, 1, 1); }
            }
            if (abs(p - pc) <= 3 && abs(q - qc) <= 3 && !cloudAt(p - 1, q)) { shade(c, 0.9f); quad(0, 0, 0, 0, 0, 1, 0, 1, 1, 0, 1, 0); }
            if (abs(e - pc) <= 3 && abs(q - qc) <= 3 && !cloudAt(e + 1, q)) { shade(c, 0.9f); quad((float)n, 0, 0, (float)n, 0, 1, (float)n, 1, 1, (float)n, 1, 0); }
            glEnd();
            glPopMatrix(1);
            p = e + 1;
        }
    }
    glPopMatrix(1);
}
