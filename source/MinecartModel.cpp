#include "MinecartModel.h"
#include "ModelBox.h"
#include "Texture.h"
#include "BlockType.h"
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

    const int PARTS = 6;
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

    // ModelRenderer.render: al punto de rotacion, giros Z, Y y X, y las 6 caras
    void drawPart(const Part& p, float yOverride) {
        glPushMatrix();
        glTranslatef(p.pointX / 16.0f, (yOverride != 0.0f ? yOverride : p.pointY) / 16.0f, p.pointZ / 16.0f);
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

    // El bloque de dentro (RenderBlocks.renderBlockOnInventory): un cubo de lado 1 centrado en el origen, con las celdas de terrain.png
    void cubeFace(const TextureCell& c, const float v[4][3], int level) {
        const int u0 = c.col * 256 + 2, u1 = c.col * 256 + 254, v0 = c.row * 256 + 2, v1 = c.row * 256 + 254;
        (void)level;
        const int us[4] = { u0, u1, u1, u0 }, vs[4] = { v0, v0, v1, v1 };
        for (int k = 0; k < 4; k++) {
            GFX_TEX_COORD = TEXTURE_PACK(us[k], vs[k]);
            glVertex3v16(floattov16(v[k][0]), floattov16(v[k][1]), floattov16(v[k][2]));
        }
    }

    void drawBlock(BlockType t) {
        TextureCell top, side, bottom, front;
        if (t == BlockType::CHEST) { top = bottom = {9, 1}; side = {10, 1}; front = {11, 1}; }
        else { top = bottom = FURNACE_TOP_CELL; side = FURNACE_SIDE_CELL; front = FURNACE_FRONT_OFF; }
        const float h = 0.5f;
        glBegin(GL_QUADS);
        { const float f[4][3] = { { -h, -h, h }, { h, -h, h }, { h, -h, -h }, { -h, -h, -h } }; cubeFace(bottom, f, 0); }   // abajo
        { const float f[4][3] = { { -h, h, -h }, { h, h, -h }, { h, h, h }, { -h, h, h } }; cubeFace(top, f, 1); }          // arriba
        { const float f[4][3] = { { h, h, -h }, { -h, h, -h }, { -h, -h, -h }, { h, -h, -h } }; cubeFace(side, f, 2); }     // -Z
        { const float f[4][3] = { { -h, h, h }, { h, h, h }, { h, -h, h }, { -h, -h, h } }; cubeFace(front, f, 3); }        // +Z (frente)
        { const float f[4][3] = { { -h, h, -h }, { -h, h, h }, { -h, -h, h }, { -h, -h, -h } }; cubeFace(side, f, 4); }     // -X
        { const float f[4][3] = { { h, h, h }, { h, h, -h }, { h, -h, -h }, { h, -h, h } }; cubeFace(side, f, 5); }         // +X
        glEnd();
    }
}

void MinecartModel::init() {
    // ModelMinecart: var1 = 20 (largo), var2 = 8 (alto de las paredes), var3 = 16 (ancho), var4 = 4
    addBox(s_parts[0], 0, 10, -10.0f, -8.0f, -1.0f, 20, 16, 2);    s_parts[0].pointX = 0.0f;  s_parts[0].pointY = 4.0f; s_parts[0].pointZ = 0.0f;  s_parts[0].rotX = PI_F * 0.5f;  s_parts[0].rotY = 0.0f;
    addBox(s_parts[1], 0, 0, -8.0f, -9.0f, -1.0f, 16, 8, 2);       s_parts[1].pointX = -9.0f; s_parts[1].pointY = 4.0f; s_parts[1].pointZ = 0.0f;  s_parts[1].rotX = 0.0f;         s_parts[1].rotY = PI_F * 1.5f;
    addBox(s_parts[2], 0, 0, -8.0f, -9.0f, -1.0f, 16, 8, 2);       s_parts[2].pointX = 9.0f;  s_parts[2].pointY = 4.0f; s_parts[2].pointZ = 0.0f;  s_parts[2].rotX = 0.0f;         s_parts[2].rotY = PI_F * 0.5f;
    addBox(s_parts[3], 0, 0, -8.0f, -9.0f, -1.0f, 16, 8, 2);       s_parts[3].pointX = 0.0f;  s_parts[3].pointY = 4.0f; s_parts[3].pointZ = -7.0f; s_parts[3].rotX = 0.0f;         s_parts[3].rotY = PI_F;
    addBox(s_parts[4], 0, 0, -8.0f, -9.0f, -1.0f, 16, 8, 2);       s_parts[4].pointX = 0.0f;  s_parts[4].pointY = 4.0f; s_parts[4].pointZ = 7.0f;  s_parts[4].rotX = 0.0f;         s_parts[4].rotY = 0.0f;
    addBox(s_parts[5], 44, 10, -9.0f, -7.0f, -1.0f, 18, 14, 1);    s_parts[5].pointX = 0.0f;  s_parts[5].pointY = 4.1f; s_parts[5].pointZ = 0.0f;  s_parts[5].rotX = -PI_F * 0.5f; s_parts[5].rotY = 0.0f;
    s_ready = true;
}

// RenderMinecart.func_152_a: se coloca sobre el riel, se gira, se inclina con la pendiente, tiembla si lo golpearon, lleva su bloque (si es de cofre o de
// horno) y se dibuja el modelo dado vuelta (su Y crece hacia abajo)
void MinecartModel::draw(const Minecart& c, const MinecartAI::Pose& pose, float partialTick, unsigned char lightLevel) {
    if (!s_ready || !c.active) return;

    const int v = (int)(brightnessOf(lightLevel) * 255.0f + 0.5f);
    glPolyFmt(POLY_ALPHA(31) | POLY_CULL_NONE | POLY_FOG);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
        glTranslatef((float)pose.x - 4.0f, (float)pose.y - 1.5f, (float)pose.z - 4.0f);
        glRotatef(180.0f - pose.yawDeg, 0.0f, 1.0f, 0.0f);
        glRotatef(-pose.pitchDeg, 0.0f, 0.0f, 1.0f);
        const float hit = (float)c.timeSinceHit - partialTick;
        float dmg = (float)c.damage - partialTick;
        if (dmg < 0.0f) dmg = 0.0f;
        if (hit > 0.0f) glRotatef(sinf(hit) * hit * dmg / 10.0f * (float)c.rockDir, 1.0f, 0.0f, 0.0f);

        glColor3b(v, v, v);
        if (c.type != 0) {
            glBindTexture(0, Texture::textureID);
            glPushMatrix();
                glScalef(0.75f, 0.75f, 0.75f);
                glTranslatef(0.0f, 5.0f / 16.0f, 0.0f);
                glRotatef(90.0f, 0.0f, 1.0f, 0.0f);
                drawBlock(c.type == 1 ? BlockType::CHEST : BlockType::FURNACE);
            glPopMatrix(1);
        }

        glBindTexture(0, Texture::cartTextureID);
        glScalef(-1.0f, -1.0f, 1.0f);
        for (int i = 0; i < PARTS; i++) drawPart(s_parts[i], 0.0f);
    glPopMatrix(1);
}
