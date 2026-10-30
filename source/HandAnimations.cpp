#include "HandAnimations.h"
#include "PartialBlocks.h"
#include "ChunkManager.h"
#include "Texture.h"
#include "Maps.h"
#include "MapBg.h"
#include <nds.h>
#include <math.h>

// =====================================================================
// Port de la mano en primera persona de Minecraft Beta 1.7.3. Fuentes:
//   ItemRenderer        renderItemInFirstPerson, renderItem, updateEquippedItem
//   EntityRenderer      func_4135_b (renderHand), setupViewBobbing
//   RenderPlayer        drawFirstPersonHand
//   ModelBiped          bipedRightArm (caja, textura y pose)
//   ModelRenderer       addBox, render (orden de rotaciones)
//   TexturedQuad        UV de cada cara y su normal
//   RenderBlocks        renderBlockOnInventory, renderItemIn3d
//   RenderHelper        luces del ítem (valores del RenderHelper original)
//   EntityPlayer/EntityLiving/Entity  swing de 8 ticks y balanceo
//   Minecraft           clickMouse (cuándo se dispara el swing)
// =====================================================================

namespace {

const float kPi = 3.14159265f;

// ---- Estado por tick ----
// Swing (EntityPlayer.updatePlayerActionState / EntityLiving)
bool  isSwinging = false;
int   swingProgressInt = 0;
float swingProgress = 0.0f;
float prevSwingProgress = 0.0f;

// Subir/bajar al cambiar de ítem (ItemRenderer.updateEquippedItem)
float equippedProgress = 0.0f;
float prevEquippedProgress = 0.0f;
BlockType toRenderType = BlockType::AIR;
int toRenderDurability = 0;
int toRenderSlot = -1;

// Clics (Minecraft.runTick / clickMouse)
int ticksRan = 0;
int mouseTicksRan = 0;
int leftClickCounter = 0;
bool pendingAttackPress = false;
bool pendingAttackTargeting = false;
bool pendingPlace = false;
bool pendingPlaceEmptied = false;

// Balanceo (Entity.distanceWalkedModified, EntityPlayer.field_774_f / field_9328_R)
float distanceWalked = 0.0f, prevDistanceWalked = 0.0f;
float bobAmount = 0.0f, prevBobAmount = 0.0f;
float camPitch = 0.0f, prevCamPitch = 0.0f;

// Comprime la profundidad de la mano contra el plano cercano (NDC z' = s*z + s - 1),
// para que quede siempre delante del mundo: emula el glClear(GL_DEPTH_BUFFER_BIT)
// que el original hace antes de dibujar la mano y que el DS no puede hacer a mitad de frame.
const float kDepthSquash = 1.0f / 8.0f;
const m4x4 kDepthSquashMatrix = {{
    inttof32(1), 0, 0, 0,
    0, inttof32(1), 0, 0,
    0, 0, floattof32(kDepthSquash), 0,
    0, 0, floattof32(kDepthSquash - 1.0f), inttof32(1)
}};

// ---- Utilidades ----

// WorldProvider.generateLightBrightnessTable (b1.7.3)
float brightnessOf(u8 level) {
    float f = 1.0f - (float)level / 15.0f;
    return (1.0f - f) / (f * 3.0f + 1.0f) * 0.95f + 0.05f;
}

int to5(float v) {
    int i = (int)(v * 31.0f + 0.5f);
    return i < 0 ? 0 : (i > 31 ? 31 : i);
}

v10 toV10(float v) {
    if (v > 0.998f) v = 0.998f;
    if (v < -1.0f) v = -1.0f;
    return floattov10(v);
}

inline void tex(float u, float v) {
    GFX_TEX_COORD = TEXTURE_PACK(floattot16(u), floattot16(v)); // en texels
}

inline void vtx(float x, float y, float z) {
    glVertex3v16(floattov16(x), floattov16(y), floattov16(z));
}

inline void nrm(float x, float y, float z) {
    glNormal(NORMAL_PACK(toV10(x), toV10(y), toV10(z)));
}

// GL_COLOR_MATERIAL con el RenderHelper original: ambiente global 0.4 y difusa 0.6.
// El DS no tiene ambiente global: cada una de las 2 luces aporta 0.2 de ambiente.
void setMaterial(float r, float g, float b) {
    glMaterialf(GL_DIFFUSE,  RGB15(to5(r * 0.6f), to5(g * 0.6f), to5(b * 0.6f)));
    glMaterialf(GL_AMBIENT,  RGB15(to5(r * 0.2f), to5(g * 0.2f), to5(b * 0.2f)));
    glMaterialf(GL_SPECULAR, RGB15(0, 0, 0));
    glMaterialf(GL_EMISSION, RGB15(0, 0, 0));
}

// RenderHelper.enableStandardItemLighting: luz 0 hacia (0.2, 1, -0.7), luz 1 hacia
// (-0.2, 1, 0.7). En OpenGL GL_POSITION es la dirección HACIA la luz; en el DS el
// vector dice hacia dónde viaja la luz, por eso va con el signo invertido.
void setupItemLights() {
    float l0x = 0.2f, l0y = 1.0f, l0z = -0.7f;
    float l1x = -0.2f, l1y = 1.0f, l1z = 0.7f;
    float n0 = sqrtf(l0x * l0x + l0y * l0y + l0z * l0z);
    float n1 = sqrtf(l1x * l1x + l1y * l1y + l1z * l1z);

    glLight(0, RGB15(31, 31, 31), toV10(-l0x / n0), toV10(-l0y / n0), toV10(-l0z / n0));
    glLight(1, RGB15(31, 31, 31), toV10(-l1x / n1), toV10(-l1y / n1), toV10(-l1z / n1));
}

float swingProgressAt(float pt) {
    // EntityLiving.getSwingProgress
    float d = swingProgress - prevSwingProgress;
    if (d < 0.0f) d += 1.0f;
    return prevSwingProgress + d * pt;
}

void swingItem() {
    // EntityPlayer.swingItem
    swingProgressInt = -1;
    isSwinging = true;
}

void clickAttack(bool targeting) {
    // Minecraft.clickMouse(0)
    if (leftClickCounter > 0) return;
    swingItem();
    if (!targeting) leftClickCounter = 10;
}

void applyViewBobbing(float pt) {
    // EntityRenderer.setupViewBobbing
    float d3 = distanceWalked - prevDistanceWalked;
    float d4 = -(distanceWalked + d3 * pt);
    float b = prevBobAmount + (bobAmount - prevBobAmount) * pt;
    float cp = prevCamPitch + (camPitch - prevCamPitch) * pt;

    glTranslatef(sinf(d4 * kPi) * b * 0.5f, -fabsf(cosf(d4 * kPi) * b), 0.0f);
    glRotatef(sinf(d4 * kPi) * b * 3.0f, 0.0f, 0.0f, 1.0f);
    glRotatef(fabsf(cosf(d4 * kPi - 0.2f) * b) * 5.0f, 1.0f, 0.0f, 0.0f);
    glRotatef(cp, 1.0f, 0.0f, 0.0f);
}

// ---- Brazo: ModelBiped.bipedRightArm + RenderPlayer.drawFirstPersonHand ----
struct P3 { float x, y, z; };

void drawFirstPersonArm() {
    const float S = 1.0f / 16.0f;

    // addBox(-3, -2, -2, 4, 12, 4), textura en (40, 16)
    const float x1 = -3.0f, y1 = -2.0f, z1 = -2.0f;
    const float x2 = 1.0f,  y2 = 10.0f, z2 = 2.0f;
    const P3 v20 = {x1, y1, z1}, v12 = {x2, y1, z1}, v13 = {x2, y2, z1}, v14 = {x1, y2, z1};
    const P3 v15 = {x1, y1, z2}, v16 = {x2, y1, z2}, v17 = {x2, y2, z2}, v18 = {x1, y2, z2};

    const int tx = 40, ty = 16, w = 4, h = 12, d = 4;
    struct Face { P3 v[4]; int u1, vv1, u2, vv2; };
    const Face faces[6] = {
        {{v16, v12, v13, v17}, tx + d + w,     ty + d, tx + d + w + d,     ty + d + h},
        {{v20, v15, v18, v14}, tx + 0,         ty + d, tx + d,             ty + d + h},
        {{v16, v15, v20, v12}, tx + d,         ty + 0, tx + d + w,         ty + d},
        {{v13, v14, v18, v17}, tx + d + w,     ty + 0, tx + d + w + w,     ty + d},
        {{v12, v20, v14, v13}, tx + d,         ty + d, tx + d + w,         ty + d + h},
        {{v15, v16, v17, v18}, tx + d + w + d, ty + d, tx + d + w + d + w, ty + d + h},
    };

    // ModelRenderer.render: punto de rotación (-5, 2, 0) y, con la pose de
    // drawFirstPersonHand, solo rotateAngleZ = cos(0)*0.05 + 0.05 = 0.1 rad
    glPushMatrix();
    glTranslatef(-5.0f * S, 2.0f * S, 0.0f);
    glRotatef(0.1f * (180.0f / kPi), 0.0f, 0.0f, 1.0f);

    glBindTexture(0, Texture::skinTextureID);
    glBegin(GL_QUADS);
    for (int f = 0; f < 6; f++) {
        const Face& q = faces[f];

        // TexturedQuad.draw: normal = (v1 - v2) x (v1 - v0)
        float ax = q.v[1].x - q.v[2].x, ay = q.v[1].y - q.v[2].y, az = q.v[1].z - q.v[2].z;
        float bx = q.v[1].x - q.v[0].x, by = q.v[1].y - q.v[0].y, bz = q.v[1].z - q.v[0].z;
        float nx = ay * bz - az * by, ny = az * bx - ax * bz, nz = ax * by - ay * bx;
        float nl = sqrtf(nx * nx + ny * ny + nz * nz);
        if (nl > 0.0f) { nx /= nl; ny /= nl; nz /= nl; }
        nrm(nx, ny, nz);

        // TexturedQuad: v0=(u2,v1) v1=(u1,v1) v2=(u1,v2) v3=(u2,v2), con un margen de 0,1 texel
        const float m = 0.1f;
        const float uv[4][2] = {
            {q.u2 - m, q.vv1 + m}, {q.u1 + m, q.vv1 + m},
            {q.u1 + m, q.vv2 - m}, {q.u2 - m, q.vv2 - m},
        };
        for (int k = 0; k < 4; k++) {
            tex(uv[k][0], uv[k][1]);
            vtx(q.v[k].x * S, q.v[k].y * S, q.v[k].z * S);
        }
    }
    glEnd();
    glPopMatrix(1);
}

// ---- Bloque en la mano: RenderBlocks.renderBlockOnInventory ----

// Color de inventario de las hojas (ColorizerFoliage: basic, birch, pine)
void blockRenderColor(BlockType t, float& r, float& g, float& b) {
    r = g = b = 1.0f;
    if (t == BlockType::OAK_LEAVES)    { r = 72 / 255.0f;  g = 181 / 255.0f; b = 24 / 255.0f; }
    if (t == BlockType::BIRCH_LEAVES)  { r = 128 / 255.0f; g = 167 / 255.0f; b = 85 / 255.0f; }
    if (t == BlockType::SPRUCE_LEAVES) { r = 97 / 255.0f;  g = 153 / 255.0f; b = 97 / 255.0f; }
}

// Texturas por lado: 0 abajo, 1 arriba, 2 -Z, 3 +Z, 4 -X, 5 +X (getBlockTextureFromSide)
void blockSideTextures(BlockType t, TextureCell out[6]) {
    TextureCell top, side, bottom;
    getBlockTextures(t, top, side, bottom);
    out[0] = bottom;
    out[1] = top;
    for (int i = 2; i < 6; i++) out[i] = side;

    if (isFurnaceBlock(t)) {
        out[0] = out[1] = FURNACE_TOP_CELL;
        for (int i = 2; i < 6; i++) out[i] = FURNACE_SIDE_CELL;
        out[3] = furnaceIsLit(t) ? FURNACE_FRONT_LIT : FURNACE_FRONT_OFF; // el frente es el lado 3
    }
    if (isPumpkinBlock(t)) {   // BlockPumpkin.getBlockTextureFromSide: lado 3 = cara tallada
        out[0] = out[1] = PUMPKIN_TOP_CELL;
        for (int i = 2; i < 6; i++) out[i] = PUMPKIN_SIDE_CELL;
        out[3] = isJackBlock(t) ? JACK_FRONT_CELL : PUMPKIN_FRONT_CELL;
    }
    if (t == BlockType::CRAFTING_TABLE) {
        out[3] = out[5] = {11, 3}; // BlockWorkbench: los lados 3 y 5 usan la otra textura lateral
    }
}

void drawBlockInHand(BlockType t, float brightness) {
    TextureCell tc[6];
    blockSideTextures(t, tc);

    float r, g, b;
    blockRenderColor(t, r, g, b);

    // El cactus (renderType 13) se dibuja con los costados metidos 1/16
    const float in = (t == BlockType::CACTUS) ? (1.0f / 16.0f) : 0.0f;

    glBindTexture(0, Texture::textureID);
    if (t == BlockType::TRAPDOOR) glScalef(1.0f, 3.0f / 16.0f, 1.0f);   // la trampilla cerrada es una losa de 3/16
    else if (t == BlockType::BUTTON) glScalef(6.0f / 16.0f, 4.0f / 16.0f, 2.0f / 16.0f);   // el boton: caja de 6 x 4 x 2
    else if (isPlateBlock(t)) glScalef(14.0f / 16.0f, 2.0f / 16.0f, 14.0f / 16.0f);   // placas de presion: losa baja
    glTranslatef(-0.5f, -0.5f, -0.5f);
    setMaterial(r * brightness, g * brightness, b * brightness);

    auto uv = [](const TextureCell& c, float& u0, float& u1, float& v0, float& v1) {
        u0 = c.col * 16.0f; u1 = c.col * 16.0f + 15.99f;
        v0 = c.row * 16.0f; v1 = c.row * 16.0f + 15.99f;
    };
    float u0, u1, v0, v1;

    // Losas, escaleras y valla: varias cajas dentro de la celda (RenderBlocks.renderBlockOnInventory usa las cajas del bloque)
    float shape[9][6] = { { 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f } };
    int shapeCount = 1;
    if (isStructureBlock(t) && t != BlockType::LADDER) shapeCount = Partial::shapeBoxes(t, t == BlockType::FENCE ? 12 : 0, shape);
    for (int sb = 0; sb < shapeCount; sb++) {
    glPushMatrix();
    glTranslatef(shape[sb][0], shape[sb][1], shape[sb][2]);
    glScalef(shape[sb][3] - shape[sb][0], shape[sb][4] - shape[sb][1], shape[sb][5] - shape[sb][2]);

    glBegin(GL_QUADS);

    // renderBottomFace (y = 0)
    uv(tc[0], u0, u1, v0, v1);
    nrm(0.0f, -1.0f, 0.0f);
    tex(u0, v1); vtx(0, 0, 1);
    tex(u0, v0); vtx(0, 0, 0);
    tex(u1, v0); vtx(1, 0, 0);
    tex(u1, v1); vtx(1, 0, 1);
    glEnd();

    // renderTopFace (y = 1). El pasto lleva el tinte de bioma solo arriba (como el mundo)
    if (t == BlockType::GRASS) {
        u8 tr, tg, tb;
        getGrassTint(t, false, tr, tg, tb);
        setMaterial(tr / 255.0f * brightness, tg / 255.0f * brightness, tb / 255.0f * brightness);
    }
    glBegin(GL_QUADS);
    uv(tc[1], u0, u1, v0, v1);
    nrm(0.0f, 1.0f, 0.0f);
    tex(u1, v1); vtx(1, 1, 1);
    tex(u1, v0); vtx(1, 1, 0);
    tex(u0, v0); vtx(0, 1, 0);
    tex(u0, v1); vtx(0, 1, 1);
    glEnd();
    if (t == BlockType::GRASS) {
        setMaterial(r * brightness, g * brightness, b * brightness);
    }

    glBegin(GL_QUADS);
    // renderEastFace (-Z)
    uv(tc[2], u0, u1, v0, v1);
    nrm(0.0f, 0.0f, -1.0f);
    tex(u1, v0); vtx(0, 1, in);
    tex(u0, v0); vtx(1, 1, in);
    tex(u0, v1); vtx(1, 0, in);
    tex(u1, v1); vtx(0, 0, in);

    // renderWestFace (+Z)
    uv(tc[3], u0, u1, v0, v1);
    nrm(0.0f, 0.0f, 1.0f);
    tex(u0, v0); vtx(0, 1, 1.0f - in);
    tex(u0, v1); vtx(0, 0, 1.0f - in);
    tex(u1, v1); vtx(1, 0, 1.0f - in);
    tex(u1, v0); vtx(1, 1, 1.0f - in);

    // renderNorthFace (-X)
    uv(tc[4], u0, u1, v0, v1);
    nrm(-1.0f, 0.0f, 0.0f);
    tex(u1, v0); vtx(in, 1, 1);
    tex(u0, v0); vtx(in, 1, 0);
    tex(u0, v1); vtx(in, 0, 0);
    tex(u1, v1); vtx(in, 0, 1);

    // renderSouthFace (+X)
    uv(tc[5], u0, u1, v0, v1);
    nrm(1.0f, 0.0f, 0.0f);
    tex(u0, v1); vtx(1.0f - in, 0, 1);
    tex(u1, v1); vtx(1.0f - in, 0, 0);
    tex(u1, v0); vtx(1.0f - in, 1, 0);
    tex(u0, v0); vtx(1.0f - in, 1, 1);
    glEnd();
    glPopMatrix(1);
    }
}

// ---- Ítem 2D extruido: ItemRenderer.renderItem (rama sin renderItemIn3d) ----
void drawItem2D(BlockType t, float brightness) {
    TextureCell top, side, bottom;
    getBlockTextures(t, top, side, bottom);
    glBindTexture(0, usesItemAtlas(t) ? Texture::itemsTextureID : Texture::textureID);

    // var5..var8 del original, en texels
    const float u0 = side.col * 16.0f, u1 = side.col * 16.0f + 15.99f;
    const float v0 = side.row * 16.0f, v1 = side.row * 16.0f + 15.99f;
    const float th = 1.0f / 16.0f;   // grosor
    const float eps = 0.5f;          // 0.001953125 * 256

    glTranslatef(0.0f, -0.3f, 0.0f);
    glScalef(1.5f, 1.5f, 1.5f);
    glRotatef(50.0f, 0.0f, 1.0f, 0.0f);
    glRotatef(335.0f, 0.0f, 0.0f, 1.0f);
    glTranslatef(-15.0f / 16.0f, -1.0f / 16.0f, 0.0f);

    setMaterial(brightness, brightness, brightness);

    glBegin(GL_QUADS);
    // cara frontal
    nrm(0.0f, 0.0f, 1.0f);
    tex(u1, v1); vtx(0, 0, 0);
    tex(u0, v1); vtx(1, 0, 0);
    tex(u0, v0); vtx(1, 1, 0);
    tex(u1, v0); vtx(0, 1, 0);

    // cara trasera
    nrm(0.0f, 0.0f, -1.0f);
    tex(u1, v0); vtx(0, 1, -th);
    tex(u0, v0); vtx(1, 1, -th);
    tex(u0, v1); vtx(1, 0, -th);
    tex(u1, v1); vtx(0, 0, -th);

    // tiras laterales (-X)
    nrm(-1.0f, 0.0f, 0.0f);
    for (int i = 0; i < 16; i++) {
        float f = i / 16.0f;
        float u = u1 + (u0 - u1) * f - eps;
        float x = f;
        tex(u, v1); vtx(x, 0, -th);
        tex(u, v1); vtx(x, 0, 0);
        tex(u, v0); vtx(x, 1, 0);
        tex(u, v0); vtx(x, 1, -th);
    }

    // tiras laterales (+X)
    nrm(1.0f, 0.0f, 0.0f);
    for (int i = 0; i < 16; i++) {
        float f = i / 16.0f;
        float u = u1 + (u0 - u1) * f - eps;
        float x = f + 1.0f / 16.0f;
        tex(u, v0); vtx(x, 1, -th);
        tex(u, v0); vtx(x, 1, 0);
        tex(u, v1); vtx(x, 0, 0);
        tex(u, v1); vtx(x, 0, -th);
    }

    // tiras (+Y)
    nrm(0.0f, 1.0f, 0.0f);
    for (int i = 0; i < 16; i++) {
        float f = i / 16.0f;
        float v = v1 + (v0 - v1) * f - eps;
        float y = f + 1.0f / 16.0f;
        tex(u1, v); vtx(0, y, 0);
        tex(u0, v); vtx(1, y, 0);
        tex(u0, v); vtx(1, y, -th);
        tex(u1, v); vtx(0, y, -th);
    }

    // tiras (-Y)
    nrm(0.0f, -1.0f, 0.0f);
    for (int i = 0; i < 16; i++) {
        float f = i / 16.0f;
        float v = v1 + (v0 - v1) * f - eps;
        float y = f;
        tex(u0, v); vtx(1, y, 0);
        tex(u1, v); vtx(0, y, 0);
        tex(u1, v); vtx(0, y, -th);
        tex(u0, v); vtx(1, y, -th);
    }
    glEnd();
}

// ItemRenderer.renderItem
// ---- El mapa en las manos (ItemRenderer.renderItemInFirstPerson, rama del mapa): dos brazos sostienen el mapa por abajo. Todo en colores planos (sin textura:
// la VRAM esta llena): el fondo de pergamino sale de MapBg.h (la forma de mapbg.png) y el mapa se dibuja en una cuadricula de tiras del mismo color ----
const int MAP_MAX_QUADS = 900;

void mapQuad(float x0, float y0, float x1, float y1, float z, int r, int g, int b) {
    glColor3b((u8)r, (u8)g, (u8)b);
    vtx(x0, y1, z); vtx(x1, y1, z); vtx(x1, y0, z); vtx(x0, y0, z);
}

// Cuantas tiras necesita la cuadricula de 'grid' x 'grid' (para elegir la mas fina que cabe en el presupuesto de poligonos)
int mapRunCount(const u8* px, int grid) {
    const int k = 128 / grid;
    int n = 0;
    for (int j = 0; j < grid; j++) {
        int i = 0;
        while (i < grid) {
            const u8 val = px[(i * k + k / 2) + (j * k + k / 2) * 128];
            int run = 1;
            while (i + run < grid && px[((i + run) * k + k / 2) + (j * k + k / 2) * 128] == val) run++;
            if (val / 4 > 0) n++;
            i += run;
        }
    }
    return n;
}

void drawMapHeld(int mapId, float sw, float eq, float pitchDS, float yawDS, float eyeB, float posX, float posZ, int dim) {
    // ItemRenderer.renderItemInFirstPerson, rama de Item.mapItem, tal cual (pitch de la Beta = -pitchDS)
    const float a = sinf(sw * kPi);
    const float b = sinf(sqrtf(sw) * kPi);
    float v = 1.0f + pitchDS / 45.0f + 0.1f;
    if (v < 0.0f) v = 0.0f;
    if (v > 1.0f) v = 1.0f;
    v = -cosf(v * kPi) * 0.5f + 0.5f;

    glPushMatrix();
    glTranslatef(-b * 0.4f, sinf(sqrtf(sw) * kPi * 2.0f) * 0.2f, -a * 0.2f);
    glTranslatef(0.0f, -(1.0f - eq) * 1.2f - v * 0.5f + 0.04f, -0.9f * 0.8f);
    glRotatef(90.0f, 0.0f, 1.0f, 0.0f);
    glRotatef(v * -85.0f, 0.0f, 0.0f, 1.0f);

    // los dos brazos (RenderPlayer.drawFirstPersonHand)
    glPolyFmt(POLY_ALPHA(31) | POLY_CULL_NONE | POLY_FORMAT_LIGHT0 | POLY_FORMAT_LIGHT1);
    setMaterial(eyeB, eyeB, eyeB);
    for (int k = 0; k < 2; k++) {
        const int side = k * 2 - 1;
        glPushMatrix();
        glTranslatef(0.0f, -0.6f, 1.1f * (float)side);
        glRotatef((float)(-45 * side), 1.0f, 0.0f, 0.0f);
        glRotatef(-90.0f, 0.0f, 0.0f, 1.0f);
        glRotatef(59.0f, 0.0f, 0.0f, 1.0f);
        glRotatef((float)(-65 * side), 0.0f, 1.0f, 0.0f);
        drawFirstPersonArm();
        glPopMatrix(1);
    }

    const float a2 = sinf(sw * sw * kPi);
    glRotatef(-a2 * 20.0f, 0.0f, 1.0f, 0.0f);
    glRotatef(-b * 20.0f, 0.0f, 0.0f, 1.0f);
    glRotatef(-b * 80.0f, 1.0f, 0.0f, 0.0f);
    glScalef(0.38f, 0.38f, 0.38f);
    glRotatef(90.0f, 0.0f, 1.0f, 0.0f);
    glRotatef(180.0f, 0.0f, 0.0f, 1.0f);
    glTranslatef(-1.0f, -1.0f, 0.0f);
    // el original escala por 1/64 y dibuja en pixeles del mapa (-7..135); aqui la division va en cada vertice para no salirse del rango de v16.
    // La cara visible mira hacia -z: lo que va encima se acerca con z negativa (mas que en el original, por la precision de la DS)
    const float P = 1.0f / 64.0f;
    const int sc = (int)(eyeB * 255.0f + 0.5f);
    glPolyFmt(POLY_ALPHA(31) | POLY_CULL_NONE);
    glBindTexture(0, 0);
    glBegin(GL_QUADS);
    // mapbg.png (-7..135, la v de la textura crece con la y) con su forma rota y su borde oscuro
    for (int i = 0; i < MAPBG_COUNT; i++) {
        const MapBgRect& r = MAPBG_RECTS[i];
        const int cr = r.c == 1 ? 214 : 153, cg = r.c == 1 ? 190 : 135, cb = r.c == 1 ? 150 : 108;
        const float x0 = -7.0f + r.x * 142.0f / 64.0f, x1 = -7.0f + (r.x + r.w) * 142.0f / 64.0f;
        const float y0 = -7.0f + r.y * 142.0f / 64.0f, y1 = -7.0f + (r.y + r.h) * 142.0f / 64.0f;
        mapQuad(x0 * P, y0 * P, x1 * P, y1 * P, 0.0f, cr * sc / 255, cg * sc / 255, cb * sc / 255);
    }
    // MapItemRenderer: el pixel (x, y) va de x a x+1 y de y a y+1
    const u8* px = Maps::pixels(mapId);
    if (px) {
        int grid = 64;
        if (mapRunCount(px, 64) > MAP_MAX_QUADS) grid = 32;
        const int k = 128 / grid;
        for (int j = 0; j < grid; j++) {
            int i = 0;
            while (i < grid) {
                const u8 val = px[(i * k + k / 2) + (j * k + k / 2) * 128];
                int run = 1;
                while (i + run < grid && px[((i + run) * k + k / 2) + (j * k + k / 2) * 128] == val) run++;
                int r, g, bl;
                if (Maps::colorOf(val, r, g, bl))
                    mapQuad((float)(i * k) * P, (float)(j * k) * P, (float)((i + run) * k) * P, (float)((j + 1) * k) * P, -0.008f, r * sc / 255, g * sc / 255, bl * sc / 255);
                i += run;
            }
        }
    }
    glEnd();

    // MapData.func_28169_a + MapItemRenderer: la flecha del jugador (mapicons 0) en (x/2 + 64, y/2 + 64), girada en 16 pasos (yaw de la Beta = 180 - yawDS)
    float mx, my;
    if (px && Maps::playerMarker(mapId, posX, posZ, dim, mx, my)) {
        const int bx = (int)(mx * 2.0f + 0.5f), by = (int)(my * 2.0f + 0.5f);
        const float cx = (float)bx / 2.0f + 64.0f, cy = (float)by / 2.0f + 64.0f;
        const int rot = (int)((180.0f - yawDS) * 16.0f / 360.0f + 0.5f) & 15;
        const float yq = (180.0f - (float)rot * 22.5f) * (kPi / 180.0f);
        const float dx = -sinf(yq), dy = -cosf(yq);     // hacia donde mira, en pixeles del mapa (x este, y sur)
        const float qx = -dy, qy = dx;
        glBegin(GL_TRIANGLES);
        for (int pass = 0; pass < 2; pass++) {
            const float kk = pass == 0 ? 1.4f : 1.0f, z = pass == 0 ? -0.016f : -0.024f;
            if (pass == 0) glColor3b(0, 0, 0); else glColor3b((u8)sc, (u8)sc, (u8)sc);
            vtx((cx + dx * 4.0f * kk) * P, (cy + dy * 4.0f * kk) * P, z);
            vtx((cx - dx * 3.0f * kk + qx * 3.0f * kk) * P, (cy - dy * 3.0f * kk + qy * 3.0f * kk) * P, z);
            vtx((cx - dx * 3.0f * kk - qx * 3.0f * kk) * P, (cy - dy * 3.0f * kk - qy * 3.0f * kk) * P, z);
        }
        glEnd();
    }
    glPopMatrix(1);
}

void renderItem(BlockType t, float eyeBrightness, float bodyBrightness) {
    glPushMatrix();
    if (!isFlatInHand(t)) {
        // renderItemIn3d: el color sale de getEntityBrightness (luz a la altura del cuerpo)
        drawBlockInHand(t, bodyBrightness);
    } else {
        drawItem2D(t, eyeBrightness);
    }
    glPopMatrix(1);
}

} // namespace

// =====================================================================

void HandAnimations::reset() {
    isSwinging = false;
    swingProgressInt = 0;
    swingProgress = prevSwingProgress = 0.0f;
    equippedProgress = prevEquippedProgress = 0.0f;
    toRenderType = BlockType::AIR;
    toRenderDurability = 0;
    toRenderSlot = -1;
    ticksRan = mouseTicksRan = leftClickCounter = 0;
    pendingAttackPress = pendingPlace = false;
    distanceWalked = prevDistanceWalked = 0.0f;
    bobAmount = prevBobAmount = 0.0f;
    camPitch = prevCamPitch = 0.0f;
}

void HandAnimations::onAttackPressed(bool targetingBlock) {
    pendingAttackPress = true;
    pendingAttackTargeting = targetingBlock;
}

void HandAnimations::onBlockPlaced(bool stackEmptied) {
    pendingPlace = true;
    pendingPlaceEmptied = stackEmptied;
}

void HandAnimations::tick(const TickInput& in) {
    // --- 1) ItemRenderer.updateEquippedItem ---
    prevEquippedProgress = equippedProgress;

    bool same;
    if (in.heldType == BlockType::AIR && toRenderType == BlockType::AIR) {
        same = true;
    } else if (in.heldType == toRenderType && in.heldSlot == toRenderSlot) {
        same = true; // mismo "objeto" (la durabilidad puede bajar mientras se usa)
        toRenderDurability = in.heldDurability;
    } else if (in.heldType == toRenderType && in.heldType != BlockType::AIR &&
               (!isTool(in.heldType) || in.heldDurability == toRenderDurability)) {
        same = true; // mismo id y daño en otro slot: no baja
        toRenderSlot = in.heldSlot;
    } else {
        same = false;
    }

    float delta = (same ? 1.0f : 0.0f) - equippedProgress;
    if (delta < -0.4f) delta = -0.4f;
    if (delta > 0.4f) delta = 0.4f;
    equippedProgress += delta;
    if (equippedProgress < 0.1f) {
        toRenderType = in.heldType;
        toRenderDurability = in.heldDurability;
        toRenderSlot = in.heldSlot;
    }

    // --- 2) Minecraft.runTick: clics ---
    ticksRan++;
    if (pendingAttackPress) {
        clickAttack(pendingAttackTargeting);
        mouseTicksRan = ticksRan;
        pendingAttackPress = false;
    }
    if (pendingPlace) {
        swingItem();
        if (!pendingPlaceEmptied) equippedProgress = 0.0f; // func_9449_b: baja y vuelve a subir
        mouseTicksRan = ticksRan;
        pendingPlace = false;
    }
    if (leftClickCounter > 0) leftClickCounter--;
    if (in.attackHeld && (ticksRan - mouseTicksRan) >= 5) { // ticksPerSecond / 4
        clickAttack(in.targetingBlock);
        mouseTicksRan = ticksRan;
    }
    if (!in.attackHeld) leftClickCounter = 0;

    // --- 3) Actualización de la entidad ---
    prevSwingProgress = swingProgress;
    prevDistanceWalked = distanceWalked;
    prevBobAmount = bobAmount;
    prevCamPitch = camPitch;

    // EntityPlayer.updatePlayerActionState
    if (isSwinging) {
        swingProgressInt++;
        if (swingProgressInt >= 8) {
            swingProgressInt = 0;
            isSwinging = false;
        }
    } else {
        swingProgressInt = 0;
    }
    swingProgress = (float)swingProgressInt / 8.0f;

    // Entity.moveEntity
    // Agachado en el suelo no suma distancia: el balanceo de la mano se cancela (Entity.moveEntity: !(sneaking && onGround))
    if (!(in.sneaking && in.onGround))
        distanceWalked += sqrtf(in.movedX * in.movedX + in.movedZ * in.movedZ) * 0.6f;

    // EntityPlayer.onLivingUpdate
    float h = sqrtf(in.velX * in.velX + in.velZ * in.velZ);
    if (h > 0.1f) h = 0.1f;
    if (!in.onGround) h = 0.0f;
    float p = atanf(-in.velY * 0.2f) * 15.0f;
    if (in.onGround) p = 0.0f;
    bobAmount += (h - bobAmount) * 0.4f;
    camPitch += (p - camPitch) * 0.8f;
}

void HandAnimations::applyWorldViewBobbing(float partialTicks) {
    applyViewBobbing(partialTicks);
}

void HandAnimations::render(const RenderInput& in) {
    float pt = in.partialTicks;
    float eq = prevEquippedProgress + (equippedProgress - prevEquippedProgress) * pt;
    float sw = swingProgressAt(pt);

    float eyeB = brightnessOf(in.eyeLight);
    float bodyB = brightnessOf(in.bodyLight);

    // --- Proyección: la del mundo en el original (FOV 70, aspecto de la pantalla,
    //     plano cercano 0.05), con la profundidad comprimida contra el plano cercano ---
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glMultMatrix4x4(&kDepthSquashMatrix);
    gluPerspective(70.0f, 256.0f / 192.0f, 0.05f, 80.0f);

    // --- EntityRenderer.func_4135_b: identidad + balanceo ---
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    applyViewBobbing(pt);

    // --- Luces del ítem, orientadas con pitch y yaw como en renderItemInFirstPerson.
    //     Beta: rotationPitch = -pitch, rotationYaw = 180 - yaw (sin el +180 de la cámara) ---
    glPushMatrix();
    glRotatef(-in.pitch, 1.0f, 0.0f, 0.0f);
    glRotatef(180.0f - in.yaw, 0.0f, 1.0f, 0.0f);
    setupItemLights();
    glPopMatrix(1);

    glPolyFmt(POLY_ALPHA(31) | POLY_CULL_NONE | POLY_FORMAT_LIGHT0 | POLY_FORMAT_LIGHT1);

    float a = sinf(sw * kPi);
    float b = sinf(sqrtf(sw) * kPi);
    float a2 = sinf(sw * sw * kPi);

    if (toRenderType == BlockType::MAP) {
        drawMapHeld(toRenderDurability, sw, eq, in.pitch, in.yaw, eyeB, in.posX, in.posZ, in.dimension);
    } else if (toRenderType != BlockType::AIR) {
        // --- renderItemInFirstPerson, rama con ítem (el brazo no se dibuja) ---
        glPushMatrix();
        glTranslatef(-b * 0.4f, sinf(sqrtf(sw) * kPi * 2.0f) * 0.2f, -a * 0.2f);
        glTranslatef(0.7f * 0.8f, -0.65f * 0.8f - (1.0f - eq) * 0.6f, -0.9f * 0.8f);
        glRotatef(45.0f, 0.0f, 1.0f, 0.0f);
        glRotatef(-a2 * 20.0f, 0.0f, 1.0f, 0.0f);
        glRotatef(-b * 20.0f, 0.0f, 0.0f, 1.0f);
        glRotatef(-b * 80.0f, 1.0f, 0.0f, 0.0f);
        glScalef(0.4f, 0.4f, 0.4f);
        if (toRenderType == BlockType::FISHING_ROD) glRotatef(180.0f, 0.0f, 1.0f, 0.0f);   // Item.shouldRotateAroundWhenRendering (la cana)
        renderItem(toRenderType, eyeB, bodyB);
        glPopMatrix(1);
    } else {
        // --- renderItemInFirstPerson, rama de la mano vacía ---
        glPushMatrix();
        glTranslatef(-b * 0.3f, sinf(sqrtf(sw) * kPi * 2.0f) * 0.4f, -a * 0.4f);
        glTranslatef(0.8f * 0.8f, -0.75f * 0.8f - (1.0f - eq) * 0.6f, -0.9f * 0.8f);
        glRotatef(45.0f, 0.0f, 1.0f, 0.0f);
        glRotatef(b * 70.0f, 0.0f, 1.0f, 0.0f);
        glRotatef(-a2 * 20.0f, 0.0f, 0.0f, 1.0f);
        glTranslatef(-1.0f, 3.6f, 3.5f);
        glRotatef(120.0f, 0.0f, 0.0f, 1.0f);
        glRotatef(200.0f, 1.0f, 0.0f, 0.0f);
        glRotatef(-135.0f, 0.0f, 1.0f, 0.0f);
        glTranslatef(5.6f, 0.0f, 0.0f);
        setMaterial(eyeB, eyeB, eyeB);
        drawFirstPersonArm();
        glPopMatrix(1);
    }

    glPopMatrix(1);                 // modelview
    glMatrixMode(GL_PROJECTION);
    glPopMatrix(1);                 // proyección del mundo
    glMatrixMode(GL_MODELVIEW);

    // El resto del frame (LOD, ítems tirados, contorno) usa el formato global de Renderer
    glPolyFmt(POLY_ALPHA(31) | POLY_CULL_NONE | POLY_FOG);
}

u8 HandAnimations::lightLevelAt(const ChunkManager& cm, float x, float y, float z) {
    Chunk* c = cm.getChunkAt(x, z);
    if (!c) return 15;

    int xi, yi, zi;
    if (!c->worldToGrid(x, y, z, xi, yi, zi)) return 15;

    int sky = (int)c->getSkyLight(xi, yi, zi) - cm.getSkylightSubtract();
    int blk = (int)c->getBlockLight(xi, yi, zi);
    int lvl = sky > blk ? sky : blk;
    if (lvl < 0) lvl = 0;
    if (lvl > 15) lvl = 15;
    return (u8)lvl;
}

void HandAnimations::setupWorldItemLights() {
    setupItemLights();
}

// RenderBiped.renderEquippedItems: la matriz ya esta en la mano (brazo + translate(-0.0625, 0.4375, 0.0625))
void HandAnimations::renderItemThirdPerson(BlockType t, u8 light) {
    const float b = brightnessOf(light);
    glPolyFmt(POLY_ALPHA(31) | POLY_CULL_NONE | POLY_FORMAT_LIGHT0 | POLY_FORMAT_LIGHT1);
    glPushMatrix();
    if (!isFlatInHand(t)) {                       // bloque 3D
        float f = 0.5f;
        glTranslatef(0.0f, 0.1875f, -0.3125f);
        f *= 0.75f;
        glRotatef(20.0f, 1.0f, 0.0f, 0.0f);
        glRotatef(45.0f, 0.0f, 1.0f, 0.0f);
        glScalef(-f, -f, f);
    } else if ((isTool(t) && t != BlockType::FLINT_AND_STEEL && t != BlockType::SHEARS) || isSword(t) || t == BlockType::STICK || t == BlockType::FISHING_ROD) {   // isFull3D (el mechero y las tijeras no lo son)
        const float f = 0.625f;
        if (t == BlockType::FISHING_ROD) { glRotatef(180.0f, 0.0f, 0.0f, 1.0f); glTranslatef(0.0f, -0.125f, 0.0f); }   // shouldRotateAroundWhenRendering
        glTranslatef(0.0f, 0.1875f, 0.0f);
        glScalef(f, -f, f);   // Beta: scale(f, -f, f)
        glRotatef(-100.0f, 1.0f, 0.0f, 0.0f);
        glRotatef(45.0f, 0.0f, 1.0f, 0.0f);
    } else {                                      // objeto plano comun (antorcha, flor, comida...)
        const float f = 0.375f;
        glTranslatef(0.25f, 0.1875f, -0.1875f);
        glScalef(f, f, f);
        glRotatef(60.0f, 0.0f, 0.0f, 1.0f);
        glRotatef(-90.0f, 1.0f, 0.0f, 0.0f);
        glRotatef(20.0f, 0.0f, 0.0f, 1.0f);
    }
    renderItem(t, b, b);
    glPopMatrix(1);
    glPolyFmt(POLY_ALPHA(31) | POLY_CULL_NONE | POLY_FOG);
}

float HandAnimations::getSwingProgress(float partialTicks) {
    return swingProgressAt(partialTicks);
}
