#include "AnimalModel.h"
#include "ModelBox.h"
#include "Texture.h"
#include "HostileModel.h"
#include <nds.h>
#include <math.h>

namespace {
    const float PI_F = 3.14159265f;
    const float RAD_TO_DEG = 180.0f / PI_F;

    struct Quad { s16 px[4], py[4], pz[4]; s16 tu[4], tv[4]; };   // posicion en v16 (1/16 de bloque = 256), textura en 12.4
    struct Part {
        Quad quads[6];
        float pointX, pointY, pointZ;   // punto de rotacion (unidades de modelo)
    };

    // ModelRenderer.addBox + setRotationPoint
    void make(Part& part, int tx, int ty, float x, float y, float z, int w, int h, int d, float px, float py, float pz) {
        ModelBox::Face faces[6];
        ModelBox::build(faces, tx, ty, x, y, z, w, h, d, 0.0f, 0, 0.125f);   // 1/8 de texel de recorte (TexturedQuad recorta 0,1)
        for (int f = 0; f < 6; f++) {
            for (int k = 0; k < 4; k++) {
                part.quads[f].px[k] = (s16)lroundf(faces[f].v[k].x * 256.0f);
                part.quads[f].py[k] = (s16)lroundf(faces[f].v[k].y * 256.0f);
                part.quads[f].pz[k] = (s16)lroundf(faces[f].v[k].z * 256.0f);
                part.quads[f].tu[k] = (s16)lroundf(faces[f].v[k].u * 16.0f);
                part.quads[f].tv[k] = (s16)lroundf(faces[f].v[k].v * 16.0f);
            }
        }
        part.pointX = px; part.pointY = py; part.pointZ = pz;
    }

    // ---- ModelCow (sobre ModelQuadruped(12, 0)) ----
    enum { C_HEAD, C_HORN1, C_HORN2, C_UDDER, C_BODY, C_LEG1, C_LEG2, C_LEG3, C_LEG4, C_COUNT };
    Part s_cow[C_COUNT];
    // ---- ModelPig (ModelQuadruped(6, 0)) ----
    enum { P_HEAD, P_BODY, P_LEG1, P_LEG2, P_LEG3, P_LEG4, P_COUNT };
    Part s_pig[P_COUNT];
    // ---- ModelChicken ----
    enum { K_HEAD, K_BILL, K_CHIN, K_BODY, K_RLEG, K_LLEG, K_RWING, K_LWING, K_COUNT };
    Part s_chicken[K_COUNT];
    Part s_saddleSeat, s_saddleStrap;   // la silla del cerdo: color plano, sin textura (forma de saddle.png aproximada con 2 cajas)
    bool s_ready = false;

    // WorldProvider.generateLightBrightnessTable (b1.7.3)
    float brightnessOf(unsigned char level) {
        float f = 1.0f - (float)level / 15.0f;
        return (1.0f - f) / (f * 3.0f + 1.0f) * 0.95f + 0.05f;
    }

    // ModelRenderer.render: al punto de rotacion, giros Z, Y y X, y las 6 caras
    void drawPart(const Part& p, float rx, float ry, float rz) {
        glPushMatrix();
        glTranslatef(p.pointX / 16.0f, p.pointY / 16.0f, p.pointZ / 16.0f);
        if (rz != 0.0f) glRotatef(rz * RAD_TO_DEG, 0.0f, 0.0f, 1.0f);
        if (ry != 0.0f) glRotatef(ry * RAD_TO_DEG, 0.0f, 1.0f, 0.0f);
        if (rx != 0.0f) glRotatef(rx * RAD_TO_DEG, 1.0f, 0.0f, 0.0f);
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

    // ModelQuadruped.setRotationAngles: las patas 1 y 4 van con 'a' y la 2 y 3 con 'b'
    void legs(const Part& l1, const Part& l2, const Part& l3, const Part& l4, float limbSwing, float limbSwingAmount) {
        const float swing = limbSwingAmount * 1.4f;
        const float a = cosf(limbSwing * 0.6662f) * swing;
        const float b = cosf(limbSwing * 0.6662f + PI_F) * swing;
        drawPart(l1, a, 0.0f, 0.0f);
        drawPart(l2, b, 0.0f, 0.0f);
        drawPart(l3, b, 0.0f, 0.0f);
        drawPart(l4, a, 0.0f, 0.0f);
    }

    void drawCow(float limbSwing, float limbSwingAmount, float headYaw, float headPitch) {
        drawPart(s_cow[C_HEAD], headPitch, headYaw, 0.0f);
        drawPart(s_cow[C_BODY], PI_F * 0.5f, 0.0f, 0.0f);
        legs(s_cow[C_LEG1], s_cow[C_LEG2], s_cow[C_LEG3], s_cow[C_LEG4], limbSwing, limbSwingAmount);
        drawPart(s_cow[C_HORN1], headPitch, headYaw, 0.0f);
        drawPart(s_cow[C_HORN2], headPitch, headYaw, 0.0f);
        drawPart(s_cow[C_UDDER], PI_F * 0.5f, 0.0f, 0.0f);
    }

    // Caras de la caja: 0 x2, 1 x1, 2 arriba (y1), 3 abajo, 4 z1, 5 z2; sombreado fijo por cara como la armadura
    void drawFlat(const Part& p, int r, int g, int b, int v, float gb) {
        static const int SHADE[6] = { 204, 204, 255, 128, 166, 166 };
        glPushMatrix();
        glTranslatef(p.pointX / 16.0f, p.pointY / 16.0f, p.pointZ / 16.0f);
        glRotatef(90.0f, 1.0f, 0.0f, 0.0f);
        glBegin(GL_QUADS);
        for (int f = 0; f < 6; f++) {
            const int s = v * SHADE[f] / 255;
            glColor3b((u8)(r * s / 255), (u8)(g * s / 255 * gb), (u8)(b * s / 255 * gb));
            for (int k = 0; k < 4; k++) glVertex3v16(p.quads[f].px[k], p.quads[f].py[k], p.quads[f].pz[k]);
        }
        glEnd();
        glPopMatrix(1);
    }

    void drawPig(float limbSwing, float limbSwingAmount, float headYaw, float headPitch) {
        drawPart(s_pig[P_HEAD], headPitch, headYaw, 0.0f);
        drawPart(s_pig[P_BODY], PI_F * 0.5f, 0.0f, 0.0f);
        legs(s_pig[P_LEG1], s_pig[P_LEG2], s_pig[P_LEG3], s_pig[P_LEG4], limbSwing, limbSwingAmount);
    }

    // ModelChicken.setRotationAngles: la cabeza sube con el pitch NEGADO; las alas giran en Z (wing = RenderChicken.getWingRotation)
    void drawChicken(float limbSwing, float limbSwingAmount, float wing, float headYaw, float headPitch) {
        const float hx = -headPitch;
        drawPart(s_chicken[K_HEAD], hx, headYaw, 0.0f);
        drawPart(s_chicken[K_BILL], hx, headYaw, 0.0f);
        drawPart(s_chicken[K_CHIN], hx, headYaw, 0.0f);
        drawPart(s_chicken[K_BODY], PI_F * 0.5f, 0.0f, 0.0f);
        const float swing = limbSwingAmount * 1.4f;
        drawPart(s_chicken[K_RLEG], cosf(limbSwing * 0.6662f) * swing, 0.0f, 0.0f);
        drawPart(s_chicken[K_LLEG], cosf(limbSwing * 0.6662f + PI_F) * swing, 0.0f, 0.0f);
        drawPart(s_chicken[K_RWING], 0.0f, 0.0f, wing);
        drawPart(s_chicken[K_LWING], 0.0f, 0.0f, -wing);
    }
}

void AnimalModel::init() {
    // Vaca: cabeza (0,0), cuernos (22,0), ubre (52,0), cuerpo (18,4), patas (0,16) de 12 de alto. Las patas ya llevan los desplazamientos de ModelCow
    make(s_cow[C_HEAD],  0, 0,  -4.0f, -4.0f, -6.0f, 8, 8, 6,   0.0f, 4.0f, -8.0f);
    make(s_cow[C_HORN1], 22, 0, -4.0f, -5.0f, -4.0f, 1, 3, 1,   0.0f, 3.0f, -7.0f);
    make(s_cow[C_HORN2], 22, 0,  3.0f, -5.0f, -4.0f, 1, 3, 1,   0.0f, 3.0f, -7.0f);
    make(s_cow[C_UDDER], 52, 0, -2.0f, -3.0f,  0.0f, 4, 6, 2,   0.0f, 14.0f, 6.0f);
    make(s_cow[C_BODY],  18, 4, -6.0f, -10.0f, -7.0f, 12, 18, 10, 0.0f, 5.0f, 2.0f);
    make(s_cow[C_LEG1],  0, 16, -2.0f, 0.0f, -2.0f, 4, 12, 4,   -4.0f, 12.0f, 7.0f);
    make(s_cow[C_LEG2],  0, 16, -2.0f, 0.0f, -2.0f, 4, 12, 4,    4.0f, 12.0f, 7.0f);
    make(s_cow[C_LEG3],  0, 16, -2.0f, 0.0f, -2.0f, 4, 12, 4,   -4.0f, 12.0f, -6.0f);
    make(s_cow[C_LEG4],  0, 16, -2.0f, 0.0f, -2.0f, 4, 12, 4,    4.0f, 12.0f, -6.0f);

    // Cerdo: ModelQuadruped(6, 0): cabeza (0,0), cuerpo (28,8), patas (0,16) de 6 de alto
    make(s_pig[P_HEAD],  0, 0,  -4.0f, -4.0f, -8.0f, 8, 8, 8,   0.0f, 12.0f, -6.0f);
    make(s_pig[P_BODY],  28, 8, -5.0f, -10.0f, -7.0f, 10, 16, 8, 0.0f, 11.0f, 2.0f);
    make(s_pig[P_LEG1],  0, 16, -2.0f, 0.0f, -2.0f, 4, 6, 4,   -3.0f, 18.0f, 7.0f);
    make(s_pig[P_LEG2],  0, 16, -2.0f, 0.0f, -2.0f, 4, 6, 4,    3.0f, 18.0f, 7.0f);
    make(s_pig[P_LEG3],  0, 16, -2.0f, 0.0f, -2.0f, 4, 6, 4,   -3.0f, 18.0f, -5.0f);
    make(s_pig[P_LEG4],  0, 16, -2.0f, 0.0f, -2.0f, 4, 6, 4,    3.0f, 18.0f, -5.0f);

    // Silla (RenderPig + ModelPig(0.5), saddle.png): el asiento cubre la mitad de arriba del lomo y una correa le da la vuelta al cuerpo
    make(s_saddleSeat,  0, 0, -5.0f, -7.0f, -3.0f, 10, 7, 4,  0.0f, 11.0f, 2.0f);
    make(s_saddleStrap, 0, 0, -5.0f, -3.0f, -7.0f, 10, 1, 8,  0.0f, 11.0f, 2.0f);
    // inflate 0,5 (128 en v16): cada vertice se aleja del centro de su caja
    {
        Part* ps[2] = { &s_saddleSeat, &s_saddleStrap };
        for (int i = 0; i < 2; i++) {
            int mx = 0, my = 0, mz = 0;
            for (int f = 0; f < 6; f++) for (int k = 0; k < 4; k++) { mx += ps[i]->quads[f].px[k]; my += ps[i]->quads[f].py[k]; mz += ps[i]->quads[f].pz[k]; }
            mx /= 24; my /= 24; mz /= 24;
            for (int f = 0; f < 6; f++) for (int k = 0; k < 4; k++) {
                Quad& q = ps[i]->quads[f];
                q.px[k] = (s16)(q.px[k] + (q.px[k] > mx ? 128 : -128));
                q.py[k] = (s16)(q.py[k] + (q.py[k] > my ? 128 : -128));
                q.pz[k] = (s16)(q.pz[k] + (q.pz[k] > mz ? 128 : -128));
            }
        }
    }

    // Gallina (var1 = 16)
    make(s_chicken[K_HEAD],  0, 0,   -2.0f, -6.0f, -2.0f, 4, 6, 3,   0.0f, 15.0f, -4.0f);
    make(s_chicken[K_BILL],  14, 0,  -2.0f, -4.0f, -4.0f, 4, 2, 2,   0.0f, 15.0f, -4.0f);
    make(s_chicken[K_CHIN],  14, 4,  -1.0f, -2.0f, -3.0f, 2, 2, 2,   0.0f, 15.0f, -4.0f);
    make(s_chicken[K_BODY],  0, 9,   -3.0f, -4.0f, -3.0f, 6, 8, 6,   0.0f, 16.0f, 0.0f);
    make(s_chicken[K_RLEG],  26, 0,  -1.0f, 0.0f, -3.0f, 3, 5, 3,   -2.0f, 19.0f, 1.0f);
    make(s_chicken[K_LLEG],  26, 0,  -1.0f, 0.0f, -3.0f, 3, 5, 3,    1.0f, 19.0f, 1.0f);
    make(s_chicken[K_RWING], 24, 13,  0.0f, 0.0f, -3.0f, 1, 4, 6,   -4.0f, 13.0f, 0.0f);
    make(s_chicken[K_LWING], 24, 13, -1.0f, 0.0f, -3.0f, 1, 4, 6,    4.0f, 13.0f, 0.0f);
    s_ready = true;
}

// RenderLiving.doRenderLiving: se coloca, se gira el cuerpo, se da vuelta el modelo (su Y crece hacia abajo) y se dibuja
void AnimalModel::draw(const Animal& a, float partialTick, unsigned char lightLevel) {
    if (!s_ready || !a.active) return;

    const float t = partialTick;
    const float x = (float)(a.prevX + (a.x - a.prevX) * t);
    const float y = (float)(a.prevY + (a.y - a.prevY) * t);
    const float z = (float)(a.prevZ + (a.z - a.prevZ) * t);
    const float bodyYaw = a.prevBodyYaw + (a.bodyYaw - a.prevBodyYaw) * t;
    const float headYaw = (a.prevYaw + (a.yaw - a.prevYaw) * t) - bodyYaw;     // la cabeza gira respecto del cuerpo
    const float pitch = a.prevPitch + (a.pitch - a.prevPitch) * t;
    float amount = a.prevLimbSwingAmount + (a.limbSwingAmount - a.prevLimbSwingAmount) * t;
    const float swing = a.limbSwing - a.limbSwingAmount * (1.0f - t);
    if (amount > 1.0f) amount = 1.0f;

    const int v = (int)(brightnessOf(lightLevel) * 255.0f + 0.5f);
    const bool hurt = a.hs.hurtTime > 0 || a.hs.deathTime > 0;      // se pone roja al recibir dano y al morir
    const float gb = hurt ? 0.45f : 1.0f;
    const float fireR = (!hurt && a.fire > 0) ? 0.35f : 1.0f;   // ardiendo: tono anaranjado (menos azul y verde)

    glPolyFmt(POLY_ALPHA(31) | POLY_CULL_NONE | POLY_FOG);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
        glTranslatef(x, y, z);
        glRotatef(180.0f - bodyYaw, 0.0f, 1.0f, 0.0f);
        if (a.hs.deathTime > 0) {   // RenderLiving.rotateCorpse: se tumba de costado en el curso de 20 ticks
            float f = ((float)a.hs.deathTime + t - 1.0f) / 20.0f * 1.6f;
            f = sqrtf(f < 0.0f ? 0.0f : f);
            if (f > 1.0f) f = 1.0f;
            glRotatef(f * 90.0f, 0.0f, 0.0f, 1.0f);
        }
        glScalef(-1.0f, -1.0f, 1.0f);
        glTranslatef(0.0f, -24.0f * 0.0625f - 0.0078125f, 0.0f);
        glColor3b(v, (int)(v * gb * (fireR < 1.0f ? 0.65f : 1.0f)), (int)(v * gb * fireR));

        const float hy = headYaw / RAD_TO_DEG, hp = pitch / RAD_TO_DEG;
        if (a.kind == KIND_COW) {
            glBindTexture(0, Texture::cowTextureID);
            drawCow(swing, amount, hy, hp);
        } else if (a.kind == KIND_PIG) {
            glBindTexture(0, Texture::pigTextureID);
            drawPig(swing, amount, hy, hp);
            if (a.saddled) {
                glBindTexture(0, 0);
                drawFlat(s_saddleSeat, 150, 98, 52, v, gb);
                drawFlat(s_saddleStrap, 96, 62, 34, v, gb);
            }
        } else {
            glBindTexture(0, Texture::chickenTextureID);
            // RenderChicken.getWingRotation
            const float phase = a.wingPrevPhase + (a.wingPhase - a.wingPrevPhase) * t;
            const float dest = a.wingPrevDest + (a.wingDest - a.wingPrevDest) * t;
            drawChicken(swing, amount, (sinf(phase) + 1.0f) * dest, hy, hp);
        }
    glPopMatrix(1);
    if (a.fire > 0 && a.hs.deathTime == 0) HostileModel::drawFire(x, y, z, a.width, a.height);
}
