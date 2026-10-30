#include "SheepModel.h"
#include "ModelBox.h"
#include "Texture.h"
#include "HostileModel.h"
#include <nds.h>
#include <math.h>

namespace {
    const float PI_F = 3.14159265f;
    const float RAD_TO_DEG = 180.0f / PI_F;

    // EntitySheep.fleeceColorTable
    const float FLEECE[16][3] = {
        {1.0f, 1.0f, 1.0f}, {0.95f, 0.7f, 0.2f}, {0.9f, 0.5f, 0.85f}, {0.6f, 0.7f, 0.95f}, {0.9f, 0.9f, 0.2f}, {0.5f, 0.8f, 0.1f},
        {0.95f, 0.7f, 0.8f}, {0.3f, 0.3f, 0.3f}, {0.6f, 0.6f, 0.6f}, {0.3f, 0.6f, 0.7f}, {0.7f, 0.4f, 0.9f}, {0.2f, 0.4f, 0.8f},
        {0.5f, 0.4f, 0.3f}, {0.4f, 0.5f, 0.2f}, {0.8f, 0.3f, 0.3f}, {0.1f, 0.1f, 0.1f}
    };

    enum { HEAD, BODY, LEG1, LEG2, LEG3, LEG4, PART_COUNT };

    struct Quad { s16 px[4], py[4], pz[4]; s16 tu[4], tv[4]; };   // posición en v16 (1/16 de bloque = 256), textura en 12.4
    struct Part {
        Quad quads[6];
        float pointX, pointY, pointZ;   // punto de rotación (unidades de modelo)
    };

    Part s_skin[PART_COUNT];   // ModelSheep2, textura sheep.png (filas 0 a 31 de la imagen combinada)
    Part s_fur[PART_COUNT];    // ModelSheep1, textura sheep_fur.png (filas 32 a 63)
    bool s_ready = false;

    // ModelRenderer.addBox: las 6 caras con sus vértices en v16 y su textura en 12.4
    void addBox(Part& part, int tx, int ty, float x, float y, float z, int w, int h, int d, float inflate, int vOffset) {
        ModelBox::Face faces[6];
        ModelBox::build(faces, tx, ty, x, y, z, w, h, d, inflate, vOffset, 0.125f);   // 1/8 de texel de recorte (TexturedQuad recorta 0,1)
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

    // WorldProvider.generateLightBrightnessTable (b1.7.3)
    float brightnessOf(unsigned char level) {
        float f = 1.0f - (float)level / 15.0f;
        return (1.0f - f) / (f * 3.0f + 1.0f) * 0.95f + 0.05f;
    }

    // ModelRenderer.render: al punto de rotación, giros Z, Y y X, y las 6 caras
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

    // los 6 trozos con los ángulos que da ModelQuadruped.setRotationAngles
    void drawModel(const Part parts[PART_COUNT], float limbSwing, float limbSwingAmount, float headYawRad, float headPitchRad) {
        const float swing = limbSwingAmount * 1.4f;
        const float a = cosf(limbSwing * 0.6662f) * swing;
        const float b = cosf(limbSwing * 0.6662f + PI_F) * swing;
        drawPart(parts[HEAD], headPitchRad, headYawRad, 0.0f);
        drawPart(parts[BODY], PI_F * 0.5f, 0.0f, 0.0f);
        drawPart(parts[LEG1], a, 0.0f, 0.0f);
        drawPart(parts[LEG2], b, 0.0f, 0.0f);
        drawPart(parts[LEG3], b, 0.0f, 0.0f);
        drawPart(parts[LEG4], a, 0.0f, 0.0f);
    }
}

void SheepModel::init() {
    // Piel: ModelSheep2 = ModelQuadruped(12, 0) con cabeza y cuerpo propios
    addBox(s_skin[HEAD], 0, 0, -3.0f, -4.0f, -6.0f, 6, 6, 8, 0.0f, 0);   s_skin[HEAD].pointX = 0.0f;  s_skin[HEAD].pointY = 6.0f;  s_skin[HEAD].pointZ = -8.0f;
    addBox(s_skin[BODY], 28, 8, -4.0f, -10.0f, -7.0f, 8, 16, 6, 0.0f, 0); s_skin[BODY].pointX = 0.0f;  s_skin[BODY].pointY = 5.0f;  s_skin[BODY].pointZ = 2.0f;
    const float legX[4] = { -3.0f, 3.0f, -3.0f, 3.0f }, legZ[4] = { 7.0f, 7.0f, -5.0f, -5.0f };
    for (int i = 0; i < 4; i++) {
        addBox(s_skin[LEG1 + i], 0, 16, -2.0f, 0.0f, -2.0f, 4, 12, 4, 0.0f, 0);   // patas de 12 de alto
        s_skin[LEG1 + i].pointX = legX[i]; s_skin[LEG1 + i].pointY = 12.0f; s_skin[LEG1 + i].pointZ = legZ[i];
    }

    // Lana: ModelSheep1, cajas engordadas (0,6 cabeza, 1,75 cuerpo, 0,5 patas de 6 de alto), textura 32 filas más abajo
    addBox(s_fur[HEAD], 0, 0, -3.0f, -4.0f, -4.0f, 6, 6, 6, 0.6f, 32);    s_fur[HEAD].pointX = 0.0f;  s_fur[HEAD].pointY = 6.0f;  s_fur[HEAD].pointZ = -8.0f;
    addBox(s_fur[BODY], 28, 8, -4.0f, -10.0f, -7.0f, 8, 16, 6, 1.75f, 32); s_fur[BODY].pointX = 0.0f;  s_fur[BODY].pointY = 5.0f;  s_fur[BODY].pointZ = 2.0f;
    for (int i = 0; i < 4; i++) {
        addBox(s_fur[LEG1 + i], 0, 16, -2.0f, 0.0f, -2.0f, 4, 6, 4, 0.5f, 32);
        s_fur[LEG1 + i].pointX = legX[i]; s_fur[LEG1 + i].pointY = 12.0f; s_fur[LEG1 + i].pointZ = legZ[i];
    }
    s_ready = true;
}

// RenderLiving.doRenderLiving + RenderSheep: se coloca, se gira el cuerpo, se da vuelta el modelo (su Y crece hacia abajo) y se dibuja la piel y,
// encima, la lana teñida con el color de la oveja
void SheepModel::draw(const Sheep& s, float partialTick, unsigned char lightLevel) {
    if (!s_ready || !s.active) return;

    const float t = partialTick;
    const float x = (float)(s.prevX + (s.x - s.prevX) * t);
    const float y = (float)(s.prevY + (s.y - s.prevY) * t);
    const float z = (float)(s.prevZ + (s.z - s.prevZ) * t);
    const float bodyYaw = s.prevBodyYaw + (s.bodyYaw - s.prevBodyYaw) * t;
    const float headYaw = (s.prevYaw + (s.yaw - s.prevYaw) * t) - bodyYaw;     // la cabeza gira respecto del cuerpo
    const float pitch = s.prevPitch + (s.pitch - s.prevPitch) * t;
    float amount = s.prevLimbSwingAmount + (s.limbSwingAmount - s.prevLimbSwingAmount) * t;
    const float swing = s.limbSwing - s.limbSwingAmount * (1.0f - t);
    if (amount > 1.0f) amount = 1.0f;

    const float brightness = brightnessOf(lightLevel);
    const int v = (int)(brightness * 255.0f + 0.5f);
    const bool hurt = s.hs.hurtTime > 0 || s.hs.deathTime > 0;      // se pone roja al recibir daño y al morir
    const float gb = hurt ? 0.45f : 1.0f;
    const float fireR = (!hurt && s.fire > 0) ? 0.35f : 1.0f;   // ardiendo: tono anaranjado (menos azul y verde)

    glPolyFmt(POLY_ALPHA(31) | POLY_CULL_NONE | POLY_FOG);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
        glTranslatef(x, y, z);
        glRotatef(180.0f - bodyYaw, 0.0f, 1.0f, 0.0f);
        if (s.hs.deathTime > 0) {   // RenderLiving.rotateCorpse: se tumba de costado en el curso de 20 ticks
            float f = ((float)s.hs.deathTime + t - 1.0f) / 20.0f * 1.6f;
            f = sqrtf(f < 0.0f ? 0.0f : f);
            if (f > 1.0f) f = 1.0f;
            glRotatef(f * 90.0f, 0.0f, 0.0f, 1.0f);
        }
        glScalef(-1.0f, -1.0f, 1.0f);
        glTranslatef(0.0f, -24.0f * 0.0625f - 0.0078125f, 0.0f);

        glBindTexture(0, Texture::sheepTextureID);
        glColor3b(v, (int)(v * gb * (fireR < 1.0f ? 0.65f : 1.0f)), (int)(v * gb * fireR));
        drawModel(s_skin, swing, amount, headYaw / RAD_TO_DEG, pitch / RAD_TO_DEG);

        if (!s.sheared) {
            const int c = s.fleeceColor & 15;
            glColor3b((int)(v * FLEECE[c][0]), (int)(v * FLEECE[c][1] * gb), (int)(v * FLEECE[c][2] * gb));
            drawModel(s_fur, swing, amount, headYaw / RAD_TO_DEG, pitch / RAD_TO_DEG);
        }
    glPopMatrix(1);
    if (s.fire > 0 && s.hs.deathTime == 0) HostileModel::drawFire(x, y, z, 0.9f, 1.3f);
}