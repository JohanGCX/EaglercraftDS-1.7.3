#include "HostileModel.h"
#include "HostileAI.h"
#include "ModelBox.h"
#include "Texture.h"
#include "Projectiles.h"
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

    // ModelRenderer.addBox + setRotationPoint. mirror: ModelRenderer.mirror (el brazo y la pierna izquierdos usan la textura espejada)
    void make(Part& part, int tx, int ty, float x, float y, float z, int w, int h, int d, float inflate, bool mirror, float px, float py, float pz) {
        ModelBox::Face faces[6];
        ModelBox::build(faces, tx, ty, x, y, z, w, h, d, inflate, 0, 0.125f);   // 1/8 de texel de recorte (TexturedQuad recorta 0,1)
        const float cx = x + (float)w * 0.5f;
        for (int f = 0; f < 6; f++) {
            for (int k = 0; k < 4; k++) {
                float vx = faces[f].v[k].x;
                if (mirror) vx = 2.0f * cx - vx;   // espejar la caja = reflejar sus vertices y dejar la textura como esta
                part.quads[f].px[k] = (s16)lroundf(vx * 256.0f);
                part.quads[f].py[k] = (s16)lroundf(faces[f].v[k].y * 256.0f);
                part.quads[f].pz[k] = (s16)lroundf(faces[f].v[k].z * 256.0f);
                part.quads[f].tu[k] = (s16)lroundf(faces[f].v[k].u * 16.0f);
                part.quads[f].tv[k] = (s16)lroundf(faces[f].v[k].v * 16.0f);
            }
        }
        part.pointX = px; part.pointY = py; part.pointZ = pz;
    }

    // ---- ModelBiped (zombie, zombi cerdo: limbos de 4 x 12 x 4; esqueleto: de 2 x 12 x 2) ----
    enum { B_HEAD, B_BODY, B_RARM, B_LARM, B_RLEG, B_LLEG, B_COUNT };
    Part s_biped[2][B_COUNT];   // [0] zombie / zombi cerdo, [1] esqueleto
    // ---- ModelCreeper ----
    enum { C_HEAD, C_BODY, C_LEG1, C_LEG2, C_LEG3, C_LEG4, C_COUNT };
    Part s_creeper[C_COUNT];
    // ---- ModelSpider ----
    enum { S_HEAD, S_NECK, S_BODY, S_LEG1, S_LEG2, S_LEG3, S_LEG4, S_LEG5, S_LEG6, S_LEG7, S_LEG8, S_COUNT };
    Part s_spider[S_COUNT];
    // ---- ModelGhast: cuerpo de 16 y 9 tentaculos de largo 8 a 14 (los mide Random(1660)) ----
    Part s_ghastBody, s_ghastTent[9];
    // ---- ModelSlime: el de adentro (cuerpo, ojos, boca) y la capa de afuera, translucida ----
    enum { SL_BODY, SL_REYE, SL_LEYE, SL_MOUTH, SL_COUNT };
    Part s_slimeIn[SL_COUNT], s_slimeOut;
    // ---- ModelWolf ----
    enum { W_HEAD, W_BODY, W_MANE, W_LEG1, W_LEG2, W_LEG3, W_LEG4, W_TAIL, W_REAR, W_LEAR, W_SNOUT, W_COUNT };
    Part s_wolf[W_COUNT];
    // ---- ModelSquid ----
    Part s_squidBody, s_squidTent[8];
    bool s_ready = false;
    float s_camYaw = 0.0f, s_camPitch = 0.0f;   // hacia donde mira el jugador (setCamera)

    // WorldProvider.generateLightBrightnessTable (b1.7.3)
    float brightnessOf(unsigned char level) {
        float f = 1.0f - (float)level / 15.0f;
        return (1.0f - f) / (f * 3.0f + 1.0f) * 0.95f + 0.05f;
    }

    void emitQuads(const Part& p) {
        glBegin(GL_QUADS);
        for (int f = 0; f < 6; f++) {
            const Quad& q = p.quads[f];
            for (int k = 0; k < 4; k++) {
                GFX_TEX_COORD = TEXTURE_PACK(q.tu[k], q.tv[k]);
                glVertex3v16(q.px[k], q.py[k], q.pz[k]);
            }
        }
        glEnd();
    }

    // ModelRenderer.renderWithRotation: al punto de rotacion (pasado a mano: el lobo lo mueve al sentarse), giros Y, X y Z (el orden es otro que en render)
    void drawPartRot(const Part& p, float px, float py, float pz, float rx, float ry, float rz) {
        glPushMatrix();
        glTranslatef(px / 16.0f, py / 16.0f, pz / 16.0f);
        if (ry != 0.0f) glRotatef(ry * RAD_TO_DEG, 0.0f, 1.0f, 0.0f);
        if (rx != 0.0f) glRotatef(rx * RAD_TO_DEG, 1.0f, 0.0f, 0.0f);
        if (rz != 0.0f) glRotatef(rz * RAD_TO_DEG, 0.0f, 0.0f, 1.0f);
        emitQuads(p);
        glPopMatrix(1);
    }

    // ModelRenderer.render con el punto de rotacion dado: giros Z, Y y X
    void drawPartAt(const Part& p, float px, float py, float pz, float rx, float ry, float rz) {
        glPushMatrix();
        glTranslatef(px / 16.0f, py / 16.0f, pz / 16.0f);
        if (rz != 0.0f) glRotatef(rz * RAD_TO_DEG, 0.0f, 0.0f, 1.0f);
        if (ry != 0.0f) glRotatef(ry * RAD_TO_DEG, 0.0f, 1.0f, 0.0f);
        if (rx != 0.0f) glRotatef(rx * RAD_TO_DEG, 1.0f, 0.0f, 0.0f);
        emitQuads(p);
        glPopMatrix(1);
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

    // ModelZombie.setRotationAngles (sin ataque con la mano): los brazos al frente y un balanceo leve; las piernas como ModelBiped. ModelSkeleton hereda lo mismo.
    void drawBiped(const Part p[B_COUNT], float swing, float amount, float ticks, float headYaw, float headPitch) {
        drawPart(p[B_HEAD], headPitch, headYaw, 0.0f);
        drawPart(p[B_BODY], 0.0f, 0.0f, 0.0f);
        const float rz = cosf(ticks * 0.09f) * 0.05f + 0.05f;
        const float sx = sinf(ticks * 0.067f) * 0.05f;
        drawPart(p[B_RARM], PI_F * -0.5f + sx, -0.1f, rz);
        drawPart(p[B_LARM], PI_F * -0.5f - sx, 0.1f, -rz);
        drawPart(p[B_RLEG], cosf(swing * 0.6662f) * 1.4f * amount, 0.0f, 0.0f);
        drawPart(p[B_LLEG], cosf(swing * 0.6662f + PI_F) * 1.4f * amount, 0.0f, 0.0f);
    }

    // ModelCreeper.setRotationAngles
    void drawCreeper(float swing, float amount, float headYaw, float headPitch) {
        drawPart(s_creeper[C_HEAD], headPitch, headYaw, 0.0f);
        drawPart(s_creeper[C_BODY], 0.0f, 0.0f, 0.0f);
        const float a = cosf(swing * 0.6662f) * 1.4f * amount;
        const float b = cosf(swing * 0.6662f + PI_F) * 1.4f * amount;
        drawPart(s_creeper[C_LEG1], a, 0.0f, 0.0f);
        drawPart(s_creeper[C_LEG2], b, 0.0f, 0.0f);
        drawPart(s_creeper[C_LEG3], b, 0.0f, 0.0f);
        drawPart(s_creeper[C_LEG4], a, 0.0f, 0.0f);
    }

    // ModelSpider.setRotationAngles. headOnly: solo la cabeza (los ojos brillantes son una zona de la textura de la cabeza)
    void drawSpider(float swing, float amount, float headYaw, float headPitch, bool headOnly) {
        drawPart(s_spider[S_HEAD], headPitch, headYaw, 0.0f);
        if (headOnly) return;
        drawPart(s_spider[S_NECK], 0.0f, 0.0f, 0.0f);
        drawPart(s_spider[S_BODY], 0.0f, 0.0f, 0.0f);
        const float q = PI_F * 0.25f, g = PI_F * 0.125f;
        float z[8] = { -q, q, -q * 0.74f, q * 0.74f, -q * 0.74f, q * 0.74f, -q, q };
        float y[8] = { g * 2.0f, -g * 2.0f, g, -g, -g, g, -g * 2.0f, g * 2.0f };
        const float v10 = -(cosf(swing * 0.6662f * 2.0f + 0.0f) * 0.4f) * amount;
        const float v11 = -(cosf(swing * 0.6662f * 2.0f + PI_F) * 0.4f) * amount;
        const float v12 = -(cosf(swing * 0.6662f * 2.0f + PI_F * 0.5f) * 0.4f) * amount;
        const float v13 = -(cosf(swing * 0.6662f * 2.0f + PI_F * 3.0f / 2.0f) * 0.4f) * amount;
        const float v14 = fabsf(sinf(swing * 0.6662f + 0.0f) * 0.4f) * amount;
        const float v15 = fabsf(sinf(swing * 0.6662f + PI_F) * 0.4f) * amount;
        const float v16 = fabsf(sinf(swing * 0.6662f + PI_F * 0.5f) * 0.4f) * amount;
        const float v17 = fabsf(sinf(swing * 0.6662f + PI_F * 3.0f / 2.0f) * 0.4f) * amount;
        y[0] += v10; y[1] -= v10; y[2] += v11; y[3] -= v11; y[4] += v12; y[5] -= v12; y[6] += v13; y[7] -= v13;
        z[0] += v14; z[1] -= v14; z[2] += v15; z[3] -= v15; z[4] += v16; z[5] -= v16; z[6] += v17; z[7] -= v17;
        for (int i = 0; i < 8; i++) drawPart(s_spider[S_LEG1 + i], 0.0f, y[i], z[i]);
    }

    // ModelGhast.setRotationAngles: los tentaculos se balancean
    void drawGhast(float ticks) {
        drawPart(s_ghastBody, 0.0f, 0.0f, 0.0f);
        for (int i = 0; i < 9; i++) drawPart(s_ghastTent[i], 0.2f * sinf(ticks * 0.3f + (float)i) + 0.4f, 0.0f, 0.0f);
    }

    // ModelSquid.setRotationAngles: todos los tentaculos con el mismo angulo (field_21083_i)
    void drawSquid(float tent) {
        drawPart(s_squidBody, 0.0f, 0.0f, 0.0f);
        for (int i = 0; i < 8; i++) drawPart(s_squidTent[i], tent, (float)((double)i * -PI_F * 2.0 / 8.0 + PI_F * 0.5), 0.0f);
    }

    // ModelWolf.setLivingAnimations + setRotationAngles. tail: EntityWolf.setTailRotation; tilt: la cabeza inclinada (getInterestedAngle)
    void drawWolf(const Mob& m, float swing, float amount, float hy, float hp, float tail, float tilt) {
        const bool sit = m.sitting;
        drawPartRot(s_wolf[W_HEAD], -1.0f, 13.5f, -7.0f, hp, hy, tilt);
        // el cuerpo y la melena
        if (sit) {
            drawPartAt(s_wolf[W_BODY], 0.0f, 18.0f, 0.0f, PI_F * 0.25f, 0.0f, 0.0f);
            drawPartAt(s_wolf[W_MANE], -1.0f, 16.0f, -3.0f, PI_F * 0.4f, 0.0f, 0.0f);
        } else {
            drawPartAt(s_wolf[W_BODY], 0.0f, 14.0f, 2.0f, PI_F * 0.5f, 0.0f, 0.0f);
            drawPartAt(s_wolf[W_MANE], -1.0f, 14.0f, -3.0f, PI_F * 0.5f, 0.0f, 0.0f);
        }
        // las patas
        if (sit) {
            drawPartAt(s_wolf[W_LEG1], -2.5f, 22.0f, 2.0f, PI_F * 3.0f / 2.0f, 0.0f, 0.0f);
            drawPartAt(s_wolf[W_LEG2], 0.5f, 22.0f, 2.0f, PI_F * 3.0f / 2.0f, 0.0f, 0.0f);
            drawPartAt(s_wolf[W_LEG3], -2.49f, 17.0f, -4.0f, PI_F * 1.85f, 0.0f, 0.0f);
            drawPartAt(s_wolf[W_LEG4], 0.51f, 17.0f, -4.0f, PI_F * 1.85f, 0.0f, 0.0f);
        } else {
            const float a = cosf(swing * 0.6662f) * 1.4f * amount, b = cosf(swing * 0.6662f + PI_F) * 1.4f * amount;
            drawPartAt(s_wolf[W_LEG1], -2.5f, 16.0f, 7.0f, a, 0.0f, 0.0f);
            drawPartAt(s_wolf[W_LEG2], 0.5f, 16.0f, 7.0f, b, 0.0f, 0.0f);
            drawPartAt(s_wolf[W_LEG3], -2.5f, 16.0f, -4.0f, b, 0.0f, 0.0f);
            drawPartAt(s_wolf[W_LEG4], 0.5f, 16.0f, -4.0f, a, 0.0f, 0.0f);
        }
        drawPartRot(s_wolf[W_REAR], -1.0f, 13.5f, -7.0f, hp, hy, tilt);
        drawPartRot(s_wolf[W_LEAR], -1.0f, 13.5f, -7.0f, hp, hy, tilt);
        drawPartRot(s_wolf[W_SNOUT], -0.5f, 13.5f, -7.0f, hp, hy, tilt);
        const float wag = m.angry ? 0.0f : cosf(swing * 0.6662f) * 1.4f * amount;
        if (sit) drawPartRot(s_wolf[W_TAIL], -1.0f, 21.0f, 6.0f, tail, wag, 0.0f);
        else drawPartRot(s_wolf[W_TAIL], -1.0f, 12.0f, 8.0f, tail, wag, 0.0f);
    }

    // Render.renderEntityOnFire: capas de llamas (terrain.png (15,1) y (15,2), las animadas del bloque de fuego) apiladas y de cara a la camara, de ancho
    // 1,4 veces el de la entidad. Una capa mas cada 0,45 de altura relativa. (x, y, z): pies de la entidad; yOffset: lo que el original resta (0 aqui)
    void drawFireOverlay(float x, float y, float z, float width, float height) {
        glPolyFmt(POLY_ALPHA(31) | POLY_CULL_NONE);
        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
            glTranslatef(x, y, z);
            const float sc = width * 1.4f;
            glScalef(sc, sc, sc);
            glBindTexture(0, Texture::textureID);
            glColor3b(255, 255, 255);
            // La camara gira con -yaw (main.cpp), asi que +yaw deja el plano de frente con +z hacia el jugador. La Beta usa -playerViewY con su camara
            // girada 180 (orientCamera: yaw + 180): su +z apunta lejos del jugador y el -0,3 de abajo pone las llamas DELANTE del mob. Aqui eso es yaw + 180.
            glRotatef(s_camYaw + 180.0f, 0.0f, 1.0f, 0.0f);
            float h = height / sc;
            glTranslatef(0.0f, 0.0f, -0.3f + (float)((int)h) * 0.02f);
            float halfW = 0.5f, dy = 0.0f, dz = 0.0f;
            int layer = 0;
            const int baseU = 15 * 16, baseV = 16;
            glBegin(GL_QUADS);
            while (h > 0.0f) {
                const int v0i = baseV + ((layer % 2) ? 16 : 0);
                int ua = baseU * 16, ub = (int)((baseU + 15.99f) * 16.0f);
                if ((layer / 2) % 2 == 0) { const int t = ua; ua = ub; ub = t; }   // capas alternas espejadas
                const int va = v0i * 16, vb = (int)((v0i + 15.99f) * 16.0f);
                // Beta: y = 0 - var17, con var17 = yOffset (0 en un mob) que BAJA 0,45 por capa: cada capa queda mas ARRIBA (antes se restaba al reves
                // y las capas se hundian en el suelo)
                GFX_TEX_COORD = TEXTURE_PACK(ub, vb); glVertex3v16(floattov16( halfW), floattov16(dy), floattov16(dz));
                GFX_TEX_COORD = TEXTURE_PACK(ua, vb); glVertex3v16(floattov16(-halfW), floattov16(dy), floattov16(dz));
                GFX_TEX_COORD = TEXTURE_PACK(ua, va); glVertex3v16(floattov16(-halfW), floattov16(1.4f + dy), floattov16(dz));
                GFX_TEX_COORD = TEXTURE_PACK(ub, va); glVertex3v16(floattov16( halfW), floattov16(1.4f + dy), floattov16(dz));
                h -= 0.45f;
                dy += 0.45f;
                halfW *= 0.9f;
                dz += 0.03f;
                ++layer;
            }
            glEnd();
        glPopMatrix(1);
    }

    int textureFor(const Mob& m) {
        switch (m.hkind) {
        case H_ZOMBIE: return Texture::zombieTextureID;
        case H_SKELETON: return Texture::skeletonTextureID;
        case H_SPIDER: return Texture::spiderTextureID;
        case H_CREEPER: return Texture::creeperTextureID;
        case H_SLIME: return Texture::slimeTextureID;
        case H_GHAST: return m.attackCounter > 10 ? Texture::ghastfireTextureID : Texture::ghastTextureID;   // abre la boca para disparar
        case H_WOLF: return m.tamed ? Texture::wolftameTextureID : (m.angry ? Texture::wolfangryTextureID : Texture::wolfTextureID);
        case H_SQUID: return Texture::squidTextureID;
        default: return Texture::pigzombieTextureID;
        }
    }

    void drawBody(const Mob& m, float swing, float amount, float ticks, float hy, float hp) {
        switch (m.hkind) {
        case H_SKELETON: drawBiped(s_biped[1], swing, amount, ticks, hy, hp); break;
        case H_SPIDER:   drawSpider(swing, amount, hy, hp, false); break;
        case H_CREEPER:  drawCreeper(swing, amount, hy, hp); break;
        case H_GHAST:    drawGhast(ticks); break;
        case H_SLIME:    for (int i = 0; i < SL_COUNT; i++) drawPart(s_slimeIn[i], 0.0f, 0.0f, 0.0f); break;
        default:         drawBiped(s_biped[0], swing, amount, ticks, hy, hp); break;
        }
    }
}

void HostileModel::init() {
    // ModelBiped: cabeza, cuerpo, brazos y piernas (la izquierda espejada)
    for (int t = 0; t < 2; t++) {
        Part* p = s_biped[t];
        make(p[B_HEAD], 0, 0,   -4.0f, -8.0f, -4.0f, 8, 8, 8, 0.0f, false,   0.0f, 0.0f, 0.0f);
        make(p[B_BODY], 16, 16, -4.0f, 0.0f, -2.0f, 8, 12, 4, 0.0f, false,   0.0f, 0.0f, 0.0f);
        if (t == 0) {   // ModelZombie: limbos de 4
            make(p[B_RARM], 40, 16, -3.0f, -2.0f, -2.0f, 4, 12, 4, 0.0f, false, -5.0f, 2.0f, 0.0f);
            make(p[B_LARM], 40, 16, -1.0f, -2.0f, -2.0f, 4, 12, 4, 0.0f, true,   5.0f, 2.0f, 0.0f);
            make(p[B_RLEG], 0, 16,  -2.0f, 0.0f, -2.0f, 4, 12, 4, 0.0f, false, -2.0f, 12.0f, 0.0f);
            make(p[B_LLEG], 0, 16,  -2.0f, 0.0f, -2.0f, 4, 12, 4, 0.0f, true,   2.0f, 12.0f, 0.0f);
        } else {        // ModelSkeleton: limbos de 2
            make(p[B_RARM], 40, 16, -1.0f, -2.0f, -1.0f, 2, 12, 2, 0.0f, false, -5.0f, 2.0f, 0.0f);
            make(p[B_LARM], 40, 16, -1.0f, -2.0f, -1.0f, 2, 12, 2, 0.0f, true,   5.0f, 2.0f, 0.0f);
            make(p[B_RLEG], 0, 16,  -1.0f, 0.0f, -1.0f, 2, 12, 2, 0.0f, false, -2.0f, 12.0f, 0.0f);
            make(p[B_LLEG], 0, 16,  -1.0f, 0.0f, -1.0f, 2, 12, 2, 0.0f, true,   2.0f, 12.0f, 0.0f);
        }
    }
    // ModelCreeper (var2 = 4)
    make(s_creeper[C_HEAD], 0, 0,   -4.0f, -8.0f, -4.0f, 8, 8, 8, 0.0f, false,  0.0f, 4.0f, 0.0f);
    make(s_creeper[C_BODY], 16, 16, -4.0f, 0.0f, -2.0f, 8, 12, 4, 0.0f, false,  0.0f, 4.0f, 0.0f);
    make(s_creeper[C_LEG1], 0, 16,  -2.0f, 0.0f, -2.0f, 4, 6, 4, 0.0f, false,  -2.0f, 16.0f,  4.0f);
    make(s_creeper[C_LEG2], 0, 16,  -2.0f, 0.0f, -2.0f, 4, 6, 4, 0.0f, false,   2.0f, 16.0f,  4.0f);
    make(s_creeper[C_LEG3], 0, 16,  -2.0f, 0.0f, -2.0f, 4, 6, 4, 0.0f, false,  -2.0f, 16.0f, -4.0f);
    make(s_creeper[C_LEG4], 0, 16,  -2.0f, 0.0f, -2.0f, 4, 6, 4, 0.0f, false,   2.0f, 16.0f, -4.0f);
    // ModelSpider (var2 = 15)
    make(s_spider[S_HEAD], 32, 4,   -4.0f, -4.0f, -8.0f, 8, 8, 8, 0.0f, false,   0.0f, 15.0f, -3.0f);
    make(s_spider[S_NECK], 0, 0,    -3.0f, -3.0f, -3.0f, 6, 6, 6, 0.0f, false,   0.0f, 15.0f, 0.0f);
    make(s_spider[S_BODY], 0, 12,   -5.0f, -4.0f, -6.0f, 10, 8, 12, 0.0f, false, 0.0f, 15.0f, 9.0f);
    const float lz[4] = { 2.0f, 1.0f, 0.0f, -1.0f };
    for (int i = 0; i < 4; i++) {
        make(s_spider[S_LEG1 + i * 2],     18, 0, -15.0f, -1.0f, -1.0f, 16, 2, 2, 0.0f, false, -4.0f, 15.0f, lz[i]);
        make(s_spider[S_LEG1 + i * 2 + 1], 18, 0,  -1.0f, -1.0f, -1.0f, 16, 2, 2, 0.0f, false,  4.0f, 15.0f, lz[i]);
    }
    // ModelGhast (var1 = -16)
    make(s_ghastBody, 0, 0, -8.0f, -8.0f, -8.0f, 16, 16, 16, 0.0f, false, 0.0f, 8.0f, 0.0f);
    {
        HostileAI::JavaRandom rnd(1660LL);
        for (int i = 0; i < 9; i++) {
            const float tx = (((float)(i % 3) - (float)(i / 3 % 2) * 0.5f + 0.25f) / 2.0f * 2.0f - 1.0f) * 5.0f;
            const float tz = ((float)(i / 3) / 2.0f * 2.0f - 1.0f) * 5.0f;
            const int len = rnd.nextInt(7) + 8;
            make(s_ghastTent[i], 0, 0, -1.0f, 0.0f, -1.0f, 2, len, 2, 0.0f, false, tx, 15.0f, tz);
        }
    }
    // ModelSlime(16) por dentro, ModelSlime(0) por fuera
    make(s_slimeIn[SL_BODY],  0, 16, -3.0f, 17.0f, -3.0f, 6, 6, 6, 0.0f, false, 0.0f, 0.0f, 0.0f);
    make(s_slimeIn[SL_REYE], 32, 0,  -3.25f, 18.0f, -3.5f, 2, 2, 2, 0.0f, false, 0.0f, 0.0f, 0.0f);
    make(s_slimeIn[SL_LEYE], 32, 4,  1.25f, 18.0f, -3.5f, 2, 2, 2, 0.0f, false, 0.0f, 0.0f, 0.0f);
    make(s_slimeIn[SL_MOUTH], 32, 8, 0.0f, 21.0f, -3.5f, 1, 1, 1, 0.0f, false, 0.0f, 0.0f, 0.0f);
    make(s_slimeOut, 0, 0, -4.0f, 16.0f, -4.0f, 8, 8, 8, 0.0f, false, 0.0f, 0.0f, 0.0f);
    // ModelWolf
    make(s_wolf[W_HEAD],  0, 0,   -3.0f, -3.0f, -2.0f, 6, 6, 4, 0.0f, false, -1.0f, 13.5f, -7.0f);
    make(s_wolf[W_BODY], 18, 14,  -4.0f, -2.0f, -3.0f, 6, 9, 6, 0.0f, false, 0.0f, 14.0f, 2.0f);
    make(s_wolf[W_MANE], 21, 0,   -4.0f, -3.0f, -3.0f, 8, 6, 7, 0.0f, false, -1.0f, 14.0f, 2.0f);
    for (int i = 0; i < 4; i++) make(s_wolf[W_LEG1 + i], 0, 18, -1.0f, 0.0f, -1.0f, 2, 8, 2, 0.0f, false, 0.0f, 0.0f, 0.0f);
    make(s_wolf[W_TAIL],  9, 18,  -1.0f, 0.0f, -1.0f, 2, 8, 2, 0.0f, false, -1.0f, 12.0f, 8.0f);
    make(s_wolf[W_REAR], 16, 14,  -3.0f, -5.0f, 0.0f, 2, 2, 1, 0.0f, false, -1.0f, 13.5f, -7.0f);
    make(s_wolf[W_LEAR], 16, 14,   1.0f, -5.0f, 0.0f, 2, 2, 1, 0.0f, false, -1.0f, 13.5f, -7.0f);
    make(s_wolf[W_SNOUT], 0, 10,  -2.0f, 0.0f, -5.0f, 3, 3, 4, 0.0f, false, -0.5f, 13.5f, -7.0f);
    // ModelSquid (var1 = -16)
    make(s_squidBody, 0, 0, -6.0f, -8.0f, -6.0f, 12, 16, 12, 0.0f, false, 0.0f, 8.0f, 0.0f);
    for (int i = 0; i < 8; i++) {
        const double a = (double)i * 3.14159265358979 * 2.0 / 8.0;
        make(s_squidTent[i], 48, 0, -1.0f, 0.0f, -1.0f, 2, 18, 2, 0.0f, false, (float)cos(a) * 5.0f, 15.0f, (float)sin(a) * 5.0f);
    }
    s_ready = true;
}

// RenderLiving.doRenderLiving: se coloca, se gira el cuerpo, se da vuelta el modelo (su Y crece hacia abajo) y se dibuja
void HostileModel::draw(const Mob& m, float partialTick, unsigned char lightLevel) {
    if (!s_ready || !m.active) return;

    const float t = partialTick;
    const float x = (float)(m.prevX + (m.x - m.prevX) * t);
    const float y = (float)(m.prevY + (m.y - m.prevY) * t);
    const float z = (float)(m.prevZ + (m.z - m.prevZ) * t);
    const float bodyYaw = m.prevBodyYaw + (m.bodyYaw - m.prevBodyYaw) * t;
    const float headYaw = (m.prevYaw + (m.yaw - m.prevYaw) * t) - bodyYaw;
    const float pitch = m.prevPitch + (m.pitch - m.prevPitch) * t;
    float amount = m.prevLimbSwingAmount + (m.limbSwingAmount - m.prevLimbSwingAmount) * t;
    const float swing = m.limbSwing - m.limbSwingAmount * (1.0f - t);
    if (amount > 1.0f) amount = 1.0f;
    const float ticks = (float)m.ticksExisted + t;

    const float brightness = brightnessOf(lightLevel);
    const int v = (int)(brightness * 255.0f + 0.5f);
    const bool hurt = m.hs.hurtTime > 0 || m.hs.deathTime > 0;      // roja al recibir dano y al morir
    // ardiendo: tono anaranjado
    float gr = 1.0f, bl = 1.0f;
    if (hurt) { gr = 0.45f; bl = 0.45f; }
    else if (m.fire > 0) { gr = 0.65f; bl = 0.35f; }

    float sx = 1.0f, sy = 1.0f, flash = 0.0f;
    if (m.hkind == H_CREEPER) {   // RenderCreeper.updateCreeperScale
        flash = ((float)m.lastIgnite + (float)(m.ignite - m.lastIgnite) * t) / 28.0f;
        const float w = 1.0f + sinf(flash * 100.0f) * flash * 0.01f;
        float f = flash < 0.0f ? 0.0f : (flash > 1.0f ? 1.0f : flash);
        f *= f; f *= f;
        sx = (1.0f + f * 0.4f) * w;
        sy = (1.0f + f * 0.1f) / w;
    }

    glPolyFmt(POLY_ALPHA(31) | POLY_CULL_NONE | POLY_FOG);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
        glTranslatef(x, y, z);
        if (m.hkind == H_SQUID) {   // RenderSquid.rotateCorpse: se inclina y rueda sobre si mismo al nadar
            glTranslatef(0.0f, 0.5f, 0.0f);
            glRotatef(180.0f - bodyYaw, 0.0f, 1.0f, 0.0f);
            glRotatef(m.sqPrevPitch + (m.sqPitch - m.sqPrevPitch) * t, 1.0f, 0.0f, 0.0f);
            glRotatef(m.sqPrevRoll + (m.sqRoll - m.sqPrevRoll) * t, 0.0f, 1.0f, 0.0f);
            glTranslatef(0.0f, -1.2f, 0.0f);
        } else {
            glRotatef(180.0f - bodyYaw, 0.0f, 1.0f, 0.0f);
            if (m.hs.deathTime > 0) {   // RenderLiving.rotateCorpse: se tumba en el curso de 20 ticks (la arana, de espaldas: 180)
                float f = ((float)m.hs.deathTime + t - 1.0f) / 20.0f * 1.6f;
                f = sqrtf(f < 0.0f ? 0.0f : f);
                if (f > 1.0f) f = 1.0f;
                glRotatef(f * (m.hkind == H_SPIDER ? 180.0f : 90.0f), 0.0f, 0.0f, 1.0f);
            }
        }
        glScalef(-1.0f, -1.0f, 1.0f);
        if (m.hkind == H_CREEPER) glScalef(sx, sy, sx);
        if (m.hkind == H_GHAST) {   // RenderGhast: se estira al cargar el disparo
            float c = ((float)m.prevAttackCounter + (float)(m.attackCounter - m.prevAttackCounter) * t) / 20.0f;
            if (c < 0.0f) c = 0.0f;
            const float k = 1.0f / (c * c * c * c * c * 2.0f + 1.0f);
            const float gy = (8.0f + k) / 2.0f, gxz = (8.0f + 1.0f / k) / 2.0f;
            glScalef(gxz, gy, gxz);
        }
        if (m.hkind == H_SLIME) {   // RenderSlime.scaleSlime: se aplasta y se estira al saltar
            const float size = (float)m.slimeSize;
            const float q = (m.prevSquish + (m.squish - m.prevSquish) * t) / (size * 0.5f + 1.0f);
            const float k = 1.0f / (q + 1.0f);
            glScalef(k * size, 1.0f / k * size, k * size);
        }
        glTranslatef(0.0f, -24.0f * 0.0625f - 0.0078125f, 0.0f);

        const float hy = headYaw / RAD_TO_DEG, hp = pitch / RAD_TO_DEG;
        glBindTexture(0, textureFor(m));
        glColor3b(v, (int)(v * gr), (int)(v * bl));
        if (m.hkind == H_WOLF) {
            float tail = 3.14159265f * 0.2f;
            if (m.angry) tail = 3.14159265f * 0.49f;
            else if (m.tamed) tail = (0.55f - (float)(20 - m.hs.health) * 0.02f) * 3.14159265f;
            const float tilt = (m.prevInterest + (m.interest - m.prevInterest) * t) * 0.15f * 3.14159265f;
            drawWolf(m, swing, amount, hy, hp, tail, tilt);
        } else if (m.hkind == H_SQUID) {
            drawSquid(m.sqPrevTent + (m.sqTent - m.sqPrevTent) * t);
        } else {
            drawBody(m, swing, amount, ticks, hy, hp);
        }

        // la capa de afuera del slime (translucida)
        if (m.hkind == H_SLIME) {
            glPolyFmt(POLY_ALPHA(20) | POLY_CULL_NONE | POLY_FOG | POLY_ID(14));
            drawPart(s_slimeOut, 0.0f, 0.0f, 0.0f);
        }

        // ojos brillantes de la arana (RenderSpider.setSpiderEyeBrightness): mas visibles cuanto mas oscuro; se dibujan apenas agrandados para no pelear con la profundidad
        if (m.hkind == H_SPIDER && m.hs.deathTime == 0) {
            const int a = (int)((1.0f - brightness) * 0.5f * 31.0f + 0.5f);
            if (a >= 1) {
                glPolyFmt(POLY_ALPHA(a > 31 ? 31 : a) | POLY_CULL_NONE | POLY_ID(13));
                glBindTexture(0, Texture::spidereyesTextureID);
                glColor3b(255, 255, 255);
                glPushMatrix();
                glTranslatef(0.0f, 15.0f / 16.0f, -3.0f / 16.0f);
                glScalef(1.004f, 1.004f, 1.004f);
                glTranslatef(0.0f, -15.0f / 16.0f, 3.0f / 16.0f);
                drawSpider(swing, amount, hy, hp, true);
                glPopMatrix(1);
            }
        }

        // el creeper a punto de explotar parpadea en blanco (RenderCreeper.updateCreeperColorMultiplier)
        if (m.hkind == H_CREEPER && m.hs.deathTime == 0 && ((int)(flash * 10.0f)) % 2 != 0) {
            int a = (int)(flash * 0.2f * 255.0f);
            if (a > 255) a = 255;
            int al = (a * 31 + 127) / 255;
            if (al < 1) al = 1;
            glPolyFmt(POLY_ALPHA(al) | POLY_CULL_NONE | POLY_ID(12));
            glBindTexture(0, 0);
            glColor3b(255, 255, 255);
            glPushMatrix();
            glTranslatef(0.0f, 0.5f, 0.0f);
            glScalef(1.01f, 1.01f, 1.01f);
            glTranslatef(0.0f, -0.5f, 0.0f);
            drawCreeper(swing, amount, hy, hp);
            glPopMatrix(1);
        }
    glPopMatrix(1);

    // Render.doRenderShadowAndFire: el fuego animado sobre el que arde (el ghast es inmune)
    if (m.fire > 0 && m.hkind != H_GHAST) drawFireOverlay(x, y, z, (float)m.width, (float)m.height);
}

// RenderArrow: la flecha vuela con la punta hacia donde va y tiembla un poco al clavarse; el modelo 3D (arrows.png) es el de Projectiles
void HostileModel::drawArrow(const Arrow& a, float partialTick, unsigned char lightLevel) {
    if (!a.active) return;
    const float t = partialTick;
    const float x = (float)(a.prevX + (a.x - a.prevX) * t);
    const float y = (float)(a.prevY + (a.y - a.prevY) * t);
    const float z = (float)(a.prevZ + (a.z - a.prevZ) * t);
    const float yaw = a.prevYaw + (a.yaw - a.prevYaw) * t;
    const float pitch = a.prevPitch + (a.pitch - a.prevPitch) * t;
    Projectiles::drawArrowModel(x, y, z, yaw, pitch, (float)a.shake - t, lightLevel);
}

// RenderFireball: el sprite de la bola de nieve (items.png (14,0)) al doble de tamano, siempre de cara a la camara
void HostileModel::drawFire(float x, float y, float z, float width, float height) { drawFireOverlay(x, y, z, width, height); }

void HostileModel::setCamera(float yaw, float pitch) { s_camYaw = yaw; s_camPitch = pitch; }

void HostileModel::drawFireball(const Fireball& f, float partialTick) {
    if (!f.active) return;
    const float t = partialTick;
    const float x = (float)(f.prevX + (f.x - f.prevX) * t);
    const float y = (float)(f.prevY + (f.y - f.prevY) * t);
    const float z = (float)(f.prevZ + (f.z - f.prevZ) * t);
    glPolyFmt(POLY_ALPHA(31) | POLY_CULL_NONE);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
        glTranslatef(x, y, z);
        glScalef(2.0f, 2.0f, 2.0f);
        glRotatef(s_camYaw, 0.0f, 1.0f, 0.0f);      // contra el giro de la camara (-yaw, -pitch): el sprite queda de frente
        glRotatef(s_camPitch, 1.0f, 0.0f, 0.0f);
        glBindTexture(0, Texture::itemsTextureID);
        glColor3b(255, 255, 255);
        const int u0 = 14 * 16, v0 = 0, u1 = u0 + 16, v1 = v0 + 16;
        glBegin(GL_QUADS);
            GFX_TEX_COORD = TEXTURE_PACK(inttot16(u0), inttot16(v1)); glVertex3v16(floattov16(-0.5f), floattov16(-0.25f), 0);
            GFX_TEX_COORD = TEXTURE_PACK(inttot16(u1), inttot16(v1)); glVertex3v16(floattov16( 0.5f), floattov16(-0.25f), 0);
            GFX_TEX_COORD = TEXTURE_PACK(inttot16(u1), inttot16(v0)); glVertex3v16(floattov16( 0.5f), floattov16( 0.75f), 0);
            GFX_TEX_COORD = TEXTURE_PACK(inttot16(u0), inttot16(v0)); glVertex3v16(floattov16(-0.5f), floattov16( 0.75f), 0);
        glEnd();
    glPopMatrix(1);
    drawFireOverlay(x, y, z, 1.0f, 1.0f);   // EntityFireball arde (fire = 1): las llamas animadas van sobre la bola, como en la Beta
}
