#include "BoatModel.h"
#include "ModelBox.h"
#include "Texture.h"
#include <nds.h>
#include <math.h>

namespace {
    const float PI_F = 3.14159265f;
    const float RAD_TO_DEG = 180.0f / PI_F;

    struct Quad { s16 px[4], py[4], pz[4]; s16 tu[4], tv[4]; };   // posicion en v16 (1/16 de bloque = 256), textura en 12.4
    struct Part {
        Quad quads[6];
        float pointX, pointY, pointZ;   // punto de rotacion (unidades de modelo)
        float rotX, rotY;               // giros fijos
    };

    const int PARTS = 5;
    Part s_parts[PARTS];
    bool s_ready = false;

    void addBox(Part& part, int tx, int ty, float x, float y, float z, int w, int h, int d) {
        ModelBox::Face faces[6];
        ModelBox::build(faces, tx, ty, x, y, z, w, h, d, 0.0f, 0, 0.125f);
        for (int f = 0; f < 6; f++) {
            for (int k = 0; k < 4; k++) {
                part.quads[f].px[k] = (s16)lroundf(faces[f].v[k].x * 256.0f);
                part.quads[f].py[k] = (s16)lroundf(faces[f].v[k].y * 256.0f);
                part.quads[f].pz[k] = (s16)lroundf(faces[f].v[k].z * 256.0f);
                part.quads[f].tu[k] = (s16)lroundf(faces[f].v[k].u * 16.0f);
                part.quads[f].tv[k] = (s16)lroundf(faces[f].v[k].v * 16.0f);
            }
        }
    }

    float brightnessOf(unsigned char level) {
        float f = 1.0f - (float)level / 15.0f;
        return (1.0f - f) / (f * 3.0f + 1.0f) * 0.95f + 0.05f;
    }

    // ModelRenderer.render: al punto de rotacion, giros Y y X, y las 6 caras
    void drawPart(const Part& p) {
        glPushMatrix();
        glTranslatef(p.pointX / 16.0f, p.pointY / 16.0f, p.pointZ / 16.0f);
        if (p.rotY != 0.0f) glRotatef(p.rotY * RAD_TO_DEG, 0.0f, 1.0f, 0.0f);
        if (p.rotX != 0.0f) glRotatef(p.rotX * RAD_TO_DEG, 1.0f, 0.0f, 0.0f);
        glBegin(GL_QUADS);
        for (int f = 0; f < 6; f++) {
            const Quad& q = p.quads[f];
            for (int k = 0; k < 4; k++) {
                GFX_TEX_COORD = TEXTURE_PACK(q.tu[k], q.tv[k]);
                glVertex3v16(q.px[k], q.py[k], q.pz[k]);
            }
        }
        glEnd();
        glPopMatrix(1);
    }
}

void BoatModel::init() {
    // ModelBoat: largo 24, alto de las paredes 6, ancho 20, altura del fondo 4
    addBox(s_parts[0], 0, 8, -12.0f, -8.0f, -3.0f, 24, 16, 4);   s_parts[0].pointX = 0.0f;   s_parts[0].pointY = 4.0f; s_parts[0].pointZ = 0.0f;  s_parts[0].rotX = PI_F * 0.5f; s_parts[0].rotY = 0.0f;
    addBox(s_parts[1], 0, 0, -10.0f, -7.0f, -1.0f, 20, 6, 2);    s_parts[1].pointX = -11.0f; s_parts[1].pointY = 4.0f; s_parts[1].pointZ = 0.0f;  s_parts[1].rotX = 0.0f;        s_parts[1].rotY = PI_F * 1.5f;
    addBox(s_parts[2], 0, 0, -10.0f, -7.0f, -1.0f, 20, 6, 2);    s_parts[2].pointX = 11.0f;  s_parts[2].pointY = 4.0f; s_parts[2].pointZ = 0.0f;  s_parts[2].rotX = 0.0f;        s_parts[2].rotY = PI_F * 0.5f;
    addBox(s_parts[3], 0, 0, -10.0f, -7.0f, -1.0f, 20, 6, 2);    s_parts[3].pointX = 0.0f;   s_parts[3].pointY = 4.0f; s_parts[3].pointZ = -9.0f; s_parts[3].rotX = 0.0f;        s_parts[3].rotY = PI_F;
    addBox(s_parts[4], 0, 0, -10.0f, -7.0f, -1.0f, 20, 6, 2);    s_parts[4].pointX = 0.0f;   s_parts[4].pointY = 4.0f; s_parts[4].pointZ = 9.0f;  s_parts[4].rotX = 0.0f;        s_parts[4].rotY = 0.0f;
    s_ready = true;
}

// RenderBoat.func_157_a: se coloca en su centro, se gira (180 - yaw), tiembla si lo golpearon y se dibuja el modelo dado vuelta (su Y crece hacia abajo)
void BoatModel::draw(const Boat& b, double x, double y, double z, float yawDeg, float partialTick, unsigned char lightLevel) {
    if (!s_ready || !b.active) return;

    const int v = (int)(brightnessOf(lightLevel) * 255.0f + 0.5f);
    glPolyFmt(POLY_ALPHA(31) | POLY_CULL_NONE | POLY_FOG);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
        glTranslatef((float)x - 4.0f, (float)y - 1.5f, (float)z - 4.0f);
        glRotatef(180.0f - yawDeg, 0.0f, 1.0f, 0.0f);
        const float hit = (float)b.timeSinceHit - partialTick;
        float dmg = (float)b.damage - partialTick;
        if (dmg < 0.0f) dmg = 0.0f;
        if (hit > 0.0f) glRotatef(sinf(hit) * hit * dmg / 10.0f * (float)b.rockDir, 1.0f, 0.0f, 0.0f);

        glColor3b(v, v, v);
        glBindTexture(0, Texture::boatTextureID);
        glScalef(-1.0f, -1.0f, 1.0f);
        for (int i = 0; i < PARTS; i++) drawPart(s_parts[i]);
    glPopMatrix(1);
}
