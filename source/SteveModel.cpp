#include "SteveModel.h"
#include "ModelBox.h"
#include "Texture.h"
#include "HandAnimations.h"
#include "BlockType.h"
#include "ArmorShape.h"
#include <nds.h>
#include <math.h>

namespace {
    const float PI_F = 3.14159265f;
    const float RAD_TO_DEG = 180.0f / PI_F;

    enum { HEAD, BODY, ARM_R, ARM_L, LEG_R, LEG_L, PART_COUNT };

    // Caras de ModelBox: 0 = x2, 1 = x1, 2 = arriba, 3 = abajo, 4 = frontal (z1), 5 = trasera (z2)
    struct Quad { s16 px[4], py[4], pz[4]; s16 tu[4], tv[4]; };
    struct Part {
        Quad quads[6];
        unsigned char faceMask;   // bit f encendido = se dibuja la cara f
        float pointX, pointY, pointZ;   // punto de rotacion por defecto (unidades de modelo)
        float rx, ry, rz;               // angulos de este cuadro
        float px, py, pz;               // punto de rotacion de este cuadro
    };

    Part s_part[PART_COUNT];
    Part s_armor1[PART_COUNT];   // armadura con engorde 1,0 (casco, peto, botas: ModelBiped(1.0) de RenderPlayer)
    Part s_armor05[PART_COUNT];  // armadura con engorde 0,5 (pantalones: ModelBiped(0.5))
    bool s_ready = false;

    // estado por tick
    float s_body = 0.0f, s_prevBody = 0.0f;
    float s_limbAmount = 0.0f, s_prevLimbAmount = 0.0f, s_limbSwing = 0.0f;
    int s_ticks = 0;

    float wrap180(float a) {
        while (a >= 180.0f) a -= 360.0f;
        while (a < -180.0f) a += 360.0f;
        return a;
    }

    // ModelRenderer.addBox: mirror = pieza del lado izquierdo (misma textura reflejada)
    void addBox(Part& part, int tx, int ty, float x, float y, float z, int w, int h, int d, bool mirror, unsigned char mask, float inflate = 0.0f) {
        ModelBox::Face faces[6];
        ModelBox::build(faces, tx, ty, x, y, z, w, h, d, inflate, 0, 0.125f);
        for (int f = 0; f < 6; f++) {
            for (int k = 0; k < 4; k++) {
                const float vx = mirror ? -faces[f].v[k].x : faces[f].v[k].x;
                part.quads[f].px[k] = (s16)lroundf(vx * 256.0f);
                part.quads[f].py[k] = (s16)lroundf(faces[f].v[k].y * 256.0f);
                part.quads[f].pz[k] = (s16)lroundf(faces[f].v[k].z * 256.0f);
                part.quads[f].tu[k] = (s16)lroundf(faces[f].v[k].u * 16.0f);
                part.quads[f].tv[k] = (s16)lroundf(faces[f].v[k].v * 16.0f);
            }
        }
        part.faceMask = mask;
    }

    float brightnessOf(unsigned char level) {   // WorldProvider.generateLightBrightnessTable (b1.7.3)
        float f = 1.0f - (float)level / 15.0f;
        return (1.0f - f) / (f * 3.0f + 1.0f) * 0.95f + 0.05f;
    }

    // ModelRenderer.render: al punto de rotacion, giros Z, Y y X, y las caras que se ven
    // Armadura sin textura (la VRAM de texturas esta llena): un color plano por material, sombreado por cara; v = brillo 0 a 255.
    // Su forma sale de ArmorShape.h: solo se dibujan los pedazos opacos de cada cara.
    struct Flat { unsigned char r, g, b; };
    const Flat FLAT_COLORS[4] = { {132, 84, 44}, {208, 208, 212}, {64, 224, 208}, {252, 226, 56} };   // cuero, hierro, diamante, oro
    const int FACE_SHADE[6] = { 204, 204, 255, 128, 166, 166 };   // /255: x2, x1, arriba, abajo, frente, atras

    struct FlatQuad { s16 x[4], y[4], z[4]; unsigned char face; };
    const int MAX_FLAT = 14;
    FlatQuad s_flat[2][PART_COUNT][MAX_FLAT];   // [0] = engorde 1,0 (forma de armor_*_1), [1] = engorde 0,5 (forma de armor_*_2)
    int s_flatCount[2][PART_COUNT];

    void drawFlatPart(const Part& p, const FlatQuad* q, int n, const Flat& c, int v, float gb) {
        glPushMatrix();
        glTranslatef(p.px / 16.0f, p.py / 16.0f, p.pz / 16.0f);
        if (p.rz != 0.0f) glRotatef(p.rz * RAD_TO_DEG, 0.0f, 0.0f, 1.0f);
        if (p.ry != 0.0f) glRotatef(p.ry * RAD_TO_DEG, 0.0f, 1.0f, 0.0f);
        if (p.rx != 0.0f) glRotatef(p.rx * RAD_TO_DEG, 1.0f, 0.0f, 0.0f);
        glBegin(GL_QUADS);
        for (int i = 0; i < n; i++) {
            const int s = v * FACE_SHADE[q[i].face] / 255;
            glColor3b((u8)(c.r * s / 255), (u8)(c.g * s / 255 * gb), (u8)(c.b * s / 255 * gb));
            for (int k = 0; k < 4; k++) glVertex3v16(q[i].x[k], q[i].y[k], q[i].z[k]);
        }
        glEnd();
        glPopMatrix(1);
    }

    // Convierte los rectangulos de texeles opacos de una pieza en cuadrilateros de su caja (interpolando las esquinas de la cara)
    void buildFlat(int set, int part, const ArmorRect* r, int n) {
        int count = 0;
        const Part& p = (set == 0 ? s_armor1 : s_armor05)[part];
        for (int i = 0; i < n && count < MAX_FLAT; i++) {
            const Quad& f = p.quads[r[i].face];
            // esquinas de la cara: k0 = (s 1, t 0), k1 = (0, 0), k2 = (0, 1), k3 = (1, 1)
            const float sa = (float)(r[i].u - r[i].fu) / r[i].fw, sb = (float)(r[i].u + r[i].w - r[i].fu) / r[i].fw;
            const float ta = (float)(r[i].v - r[i].fv) / r[i].fh, tb = (float)(r[i].v + r[i].h - r[i].fv) / r[i].fh;
            const float SS[4] = { sb, sa, sa, sb }, TT[4] = { ta, ta, tb, tb };
            FlatQuad& o = s_flat[set][part][count++];
            o.face = r[i].face;
            for (int k = 0; k < 4; k++) {
                const float s = SS[k], t = TT[k];
                const float w0 = s * (1 - t), w1 = (1 - s) * (1 - t), w2 = (1 - s) * t, w3 = s * t;
                o.x[k] = (s16)lroundf(w0 * f.px[0] + w1 * f.px[1] + w2 * f.px[2] + w3 * f.px[3]);
                o.y[k] = (s16)lroundf(w0 * f.py[0] + w1 * f.py[1] + w2 * f.py[2] + w3 * f.py[3]);
                o.z[k] = (s16)lroundf(w0 * f.pz[0] + w1 * f.pz[1] + w2 * f.pz[2] + w3 * f.pz[3]);
            }
        }
        s_flatCount[set][part] = count;
    }

    void drawPart(const Part& p) {
        glPushMatrix();
        glTranslatef(p.px / 16.0f, p.py / 16.0f, p.pz / 16.0f);
        if (p.rz != 0.0f) glRotatef(p.rz * RAD_TO_DEG, 0.0f, 0.0f, 1.0f);
        if (p.ry != 0.0f) glRotatef(p.ry * RAD_TO_DEG, 0.0f, 1.0f, 0.0f);
        if (p.rx != 0.0f) glRotatef(p.rx * RAD_TO_DEG, 1.0f, 0.0f, 0.0f);
        glBegin(GL_QUADS);
        for (int f = 0; f < 6; f++) {
            if (!(p.faceMask & (1 << f))) continue;
            const Quad& q = p.quads[f];
            for (int k = 0; k < 4; k++) {
                GFX_TEX_COORD = TEXTURE_PACK(q.tu[k], q.tv[k]);
                glVertex3v16(q.px[k], q.py[k], q.pz[k]);
            }
        }
        glEnd();
        glPopMatrix(1);
    }

    // ModelBiped.setRotationAngles
    void setAngles(float limbSwing, float limbAmount, float ticks, float headYawDeg, float headPitchDeg, float swing, bool sneak, bool holding, bool riding) {
        for (int i = 0; i < PART_COUNT; i++) {
            Part& p = s_part[i];
            p.rx = p.ry = p.rz = 0.0f;
            p.px = p.pointX; p.py = p.pointY; p.pz = p.pointZ;
        }
        Part& head = s_part[HEAD]; Part& body = s_part[BODY];
        Part& rArm = s_part[ARM_R]; Part& lArm = s_part[ARM_L];
        Part& rLeg = s_part[LEG_R]; Part& lLeg = s_part[LEG_L];

        head.ry = headYawDeg / RAD_TO_DEG;
        head.rx = headPitchDeg / RAD_TO_DEG;

        rArm.rx = cosf(limbSwing * 0.6662f + PI_F) * 2.0f * limbAmount * 0.5f;
        lArm.rx = cosf(limbSwing * 0.6662f) * 2.0f * limbAmount * 0.5f;
        rLeg.rx = cosf(limbSwing * 0.6662f) * 1.4f * limbAmount;
        lLeg.rx = cosf(limbSwing * 0.6662f + PI_F) * 1.4f * limbAmount;

        if (riding) {   // ModelBiped.isRiding: sentado, con las piernas hacia delante y abiertas un poco
            rArm.rx += PI_F * -0.2f; lArm.rx += PI_F * -0.2f;
            rLeg.rx = PI_F * -0.4f; lLeg.rx = PI_F * -0.4f;
            rLeg.ry = PI_F * 0.1f; lLeg.ry = PI_F * -0.1f;
        }

        if (holding) rArm.rx = rArm.rx * 0.5f - 0.31415927f;   // brazo levantado con un objeto

        if (swing > 0.0f) {   // el golpe: el cuerpo gira y el brazo derecho baja
            body.ry = sinf(sqrtf(swing) * PI_F * 2.0f) * 0.2f;
            rArm.pz = sinf(body.ry) * 5.0f;  rArm.px = -cosf(body.ry) * 5.0f;
            lArm.pz = -sinf(body.ry) * 5.0f; lArm.px = cosf(body.ry) * 5.0f;
            rArm.ry += body.ry; lArm.ry += body.ry; lArm.rx += body.ry;
            float f6 = 1.0f - swing; f6 *= f6; f6 *= f6; f6 = 1.0f - f6;
            const float f7 = sinf(f6 * PI_F);
            const float f8 = sinf(swing * PI_F) * -(head.rx - 0.7f) * 0.75f;
            rArm.rx -= f7 * 1.2f + f8;
            rArm.ry += body.ry * 2.0f;
            rArm.rz = sinf(swing * PI_F) * -0.4f;
        }

        if (sneak) {
            body.rx = 0.5f;
            rArm.rx += 0.4f; lArm.rx += 0.4f;
            rLeg.pz = 4.0f; lLeg.pz = 4.0f;
            rLeg.py = 9.0f; lLeg.py = 9.0f;
            head.py = 1.0f;
        }

        // balanceo en reposo
        rArm.rz += cosf(ticks * 0.09f) * 0.05f + 0.05f;
        lArm.rz -= cosf(ticks * 0.09f) * 0.05f + 0.05f;
        rArm.rx += sinf(ticks * 0.067f) * 0.05f;
        lArm.rx -= sinf(ticks * 0.067f) * 0.05f;
    }
}

void SteveModel::init() {
    // Las 6 caras de cada pieza (ModelBox.render del original): tambien la que antes se omitia, para que la skin se vea completa desde cualquier lado.
    const unsigned char MASK = 0x3F;
    const unsigned char HEAD_MASK = MASK, BODY_MASK = MASK, ARM_MASK = MASK, LEG_MASK = MASK;

    addBox(s_part[HEAD],  0,  0, -4.0f, -8.0f, -4.0f, 8,  8, 8, false, HEAD_MASK); s_part[HEAD].pointX = 0.0f;  s_part[HEAD].pointY = 0.0f;  s_part[HEAD].pointZ = 0.0f;
    addBox(s_part[BODY], 16, 16, -4.0f,  0.0f, -2.0f, 8, 12, 4, false, BODY_MASK); s_part[BODY].pointX = 0.0f;  s_part[BODY].pointY = 0.0f;  s_part[BODY].pointZ = 0.0f;
    addBox(s_part[ARM_R], 40, 16, -3.0f, -2.0f, -2.0f, 4, 12, 4, false, ARM_MASK); s_part[ARM_R].pointX = -5.0f; s_part[ARM_R].pointY = 2.0f;  s_part[ARM_R].pointZ = 0.0f;
    addBox(s_part[ARM_L], 40, 16, -3.0f, -2.0f, -2.0f, 4, 12, 4, true,  ARM_MASK); s_part[ARM_L].pointX = 5.0f;  s_part[ARM_L].pointY = 2.0f;  s_part[ARM_L].pointZ = 0.0f;
    addBox(s_part[LEG_R],  0, 16, -2.0f,  0.0f, -2.0f, 4, 12, 4, false, LEG_MASK); s_part[LEG_R].pointX = -2.0f; s_part[LEG_R].pointY = 12.0f; s_part[LEG_R].pointZ = 0.0f;
    addBox(s_part[LEG_L],  0, 16, -2.0f,  0.0f, -2.0f, 4, 12, 4, true,  LEG_MASK); s_part[LEG_L].pointX = 2.0f;  s_part[LEG_L].pointY = 12.0f; s_part[LEG_L].pointZ = 0.0f;
    // Las dos variantes de la armadura: mismas cajas, engordadas
    const Part* base[PART_COUNT];
    for (int i = 0; i < PART_COUNT; i++) base[i] = &s_part[i];
    static const int TX[PART_COUNT] = { 0, 16, 40, 40, 0, 0 }, TY[PART_COUNT] = { 0, 16, 16, 16, 16, 16 };
    static const float BX[PART_COUNT] = { -4, -4, -3, -3, -2, -2 }, BY[PART_COUNT] = { -8, 0, -2, -2, 0, 0 }, BZ[PART_COUNT] = { -4, -2, -2, -2, -2, -2 };
    static const int BW[PART_COUNT] = { 8, 8, 4, 4, 4, 4 }, BH[PART_COUNT] = { 8, 12, 12, 12, 12, 12 }, BD[PART_COUNT] = { 8, 4, 4, 4, 4, 4 };
    for (int k = 0; k < 2; k++) {
        Part* set = k ? s_armor05 : s_armor1;
        const float inf = k ? 0.5f : 1.0f;
        for (int i = 0; i < PART_COUNT; i++) {
            addBox(set[i], TX[i], TY[i], BX[i], BY[i], BZ[i], BW[i], BH[i], BD[i], i == ARM_L || i == LEG_L, MASK, inf);
            set[i].pointX = base[i]->pointX; set[i].pointY = base[i]->pointY; set[i].pointZ = base[i]->pointZ;
        }
    }
    // La forma de cada pieza: casco y peto con la de armor_*_1, pantalones con la de armor_*_2, botas con la parte baja de las piernas de armor_*_1
    buildFlat(0, HEAD, ARMOR_1_HEAD, ARMOR_1_HEAD_N);
    buildFlat(0, BODY, ARMOR_1_BODY, ARMOR_1_BODY_N);
    buildFlat(0, ARM_R, ARMOR_1_ARM, ARMOR_1_ARM_N);
    buildFlat(0, ARM_L, ARMOR_1_ARM, ARMOR_1_ARM_N);
    buildFlat(0, LEG_R, ARMOR_1_LEG, ARMOR_1_LEG_N);
    buildFlat(0, LEG_L, ARMOR_1_LEG, ARMOR_1_LEG_N);
    buildFlat(1, BODY, ARMOR_2_BODY, ARMOR_2_BODY_N);
    buildFlat(1, LEG_R, ARMOR_2_LEG, ARMOR_2_LEG_N);
    buildFlat(1, LEG_L, ARMOR_2_LEG, ARMOR_2_LEG_N);
    s_ready = true;
}

void SteveModel::reset(float yaw) {
    s_body = s_prevBody = yaw;
    s_limbAmount = s_prevLimbAmount = s_limbSwing = 0.0f;
    s_ticks = 0;
}

// EntityLiving.onUpdate: el cuerpo gira hacia donde camina (o hacia donde mira si golpea) y la cabeza no pasa de 75 grados de el
void SteveModel::tick(float yaw, float movedX, float movedZ, float swingProgressNow, bool sneak) {
    (void)sneak;
    s_prevBody = s_body;
    s_prevLimbAmount = s_limbAmount;

    const float dist = sqrtf(movedX * movedX + movedZ * movedZ);
    float f = dist * 4.0f;
    if (f > 1.0f) f = 1.0f;
    s_limbAmount += (f - s_limbAmount) * 0.4f;
    s_limbSwing += s_limbAmount;

    // los angulos son los de Player: delante = (-sen yaw, -cos yaw)
    float target = s_body;
    if (dist > 0.05f) target = atan2f(-movedX, -movedZ) * RAD_TO_DEG;
    if (swingProgressNow > 0.0f) target = yaw;
    s_body += wrap180(target - s_body) * 0.3f;

    float diff = wrap180(yaw - s_body);
    const bool wide = diff < -90.0f || diff >= 90.0f;
    (void)wide;
    if (diff < -75.0f) diff = -75.0f;
    if (diff >= 75.0f) diff = 75.0f;
    s_body = yaw - diff;
    if (diff * diff > 2500.0f) s_body += diff * 0.2f;

    s_ticks++;
}

void SteveModel::draw(float feetX, float feetY, float feetZ, float yaw, float pitch, float partialTick, float swingProgress,
                      unsigned char lightLevel, bool hurt, int deathTime, bool sneak, BlockType held, bool riding, int sleepDir, const signed char* armor) {
    if (!s_ready) return;
    const float t = partialTick;
    const bool sleeping = sleepDir >= 0;
    if (sleeping) sneak = false;   // acostado no se agacha

    const float body = s_prevBody + wrap180(s_body - s_prevBody) * t;
    float headYaw = wrap180(body - yaw);                         // en el modelo (MC) el sentido del giro es el contrario al de Player
    float headPitch = -pitch;                                    // Player: positivo = mira arriba; ModelBiped: positivo = mira abajo
    float amount = s_prevLimbAmount + (s_limbAmount - s_prevLimbAmount) * t;
    if (amount > 1.0f) amount = 1.0f;
    float swing = s_limbSwing - s_limbAmount * (1.0f - t);
    if (sleeping) { headYaw = 0.0f; headPitch = 0.0f; amount = 0.0f; swing = 0.0f; }   // tendido, con la cabeza recta y sin pasos

    setAngles(swing, amount, (float)s_ticks + t, headYaw, headPitch, sleeping ? 0.0f : swingProgress, sneak, held != BlockType::AIR, riding && !sleeping);

    const int v = (int)(brightnessOf(lightLevel) * 255.0f + 0.5f);
    const float gb = hurt ? 0.45f : 1.0f;

    glPolyFmt(POLY_ALPHA(31) | POLY_CULL_NONE | POLY_FOG);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
        if (held != BlockType::AIR) HandAnimations::setupWorldItemLights();   // con solo la camara puesta
        if (sleeping) {
            // RenderPlayer.func_22016_b / func_22017_a: el modelo se corre 1,8 hacia los pies y se acuesta: giro de la cama, 90 sobre Z y 270
            static const float SHIFT_X[4] = { 0.0f, 1.8f, 0.0f, -1.8f };
            static const float SHIFT_Z[4] = { -1.8f, 0.0f, 1.8f, 0.0f };
            static const float ORIENT[4] = { 90.0f, 0.0f, 270.0f, 180.0f };   // EntityPlayer.getBedOrientationInDegrees
            const int d = sleepDir & 3;
            glTranslatef(feetX + SHIFT_X[d], feetY, feetZ + SHIFT_Z[d]);
            glRotatef(ORIENT[d], 0.0f, 1.0f, 0.0f);
            glRotatef(90.0f, 0.0f, 0.0f, 1.0f);
            glRotatef(270.0f, 0.0f, 1.0f, 0.0f);
        } else {
        glTranslatef(feetX, feetY - (sneak ? 0.125f : 0.0f), feetZ);
        glRotatef(body, 0.0f, 1.0f, 0.0f);   // el frente del modelo (-Z) mira hacia (-sen, -cos) del angulo
        if (deathTime > 0) {                 // RenderLiving.rotateCorpse
            float f = ((float)deathTime + t - 1.0f) / 20.0f * 1.6f;
            f = sqrtf(f < 0.0f ? 0.0f : f);
            if (f > 1.0f) f = 1.0f;
            glRotatef(f * 90.0f, 0.0f, 0.0f, 1.0f);
        }
        }
        glScalef(-1.0f, -1.0f, 1.0f);
        glScalef(0.9375f, 0.9375f, 0.9375f);   // RenderPlayer.preRenderCallback
        glTranslatef(0.0f, -24.0f * 0.0625f - 0.0078125f, 0.0f);

        glBindTexture(0, Texture::skinTextureID);
        glColor3b(v, (int)(v * gb), (int)(v * gb));
        for (int i = 0; i < PART_COUNT; i++) drawPart(s_part[i]);

        if (armor) {   // RenderPlayer.setArmorModel: cada pieza con las partes que le tocan (color plano por material)
            glBindTexture(0, 0);
            for (int slot = 0; slot < 4; slot++) {
                if (armor[slot] < 0) continue;
                Part* set = (slot == 2) ? s_armor05 : s_armor1;
                for (int i = 0; i < PART_COUNT; i++) {
                    bool show = false;
                    switch (slot) {
                        case 0: show = (i == HEAD); break;
                        case 1: show = (i == BODY || i == ARM_R || i == ARM_L); break;
                        case 2: show = (i == BODY || i == LEG_R || i == LEG_L); break;
                        default: show = (i == LEG_R || i == LEG_L); break;
                    }
                    if (!show) continue;
                    Part& a = set[i]; const Part& b = s_part[i];
                    a.rx = b.rx; a.ry = b.ry; a.rz = b.rz; a.px = b.px; a.py = b.py; a.pz = b.pz;
                    const int fs = (slot == 2) ? 1 : 0;
                    drawFlatPart(a, s_flat[fs][i], s_flatCount[fs][i], FLAT_COLORS[armor[slot] & 3], v, gb);
                }
            }
        }

        if (held != BlockType::AIR) {   // RenderBiped.renderEquippedItems: del brazo derecho a la mano
            const Part& a = s_part[ARM_R];
            glPushMatrix();
            glTranslatef(a.px / 16.0f, a.py / 16.0f, a.pz / 16.0f);
            if (a.rz != 0.0f) glRotatef(a.rz * RAD_TO_DEG, 0.0f, 0.0f, 1.0f);
            if (a.ry != 0.0f) glRotatef(a.ry * RAD_TO_DEG, 0.0f, 1.0f, 0.0f);
            if (a.rx != 0.0f) glRotatef(a.rx * RAD_TO_DEG, 1.0f, 0.0f, 0.0f);
            glTranslatef(-0.0625f, 0.4375f, 0.0625f);
            HandAnimations::renderItemThirdPerson(held, lightLevel);
            glPopMatrix(1);
        }
    glPopMatrix(1);
}
