#include "DroppedItem.h"
#include "PartialBlocks.h"
#include "Texture.h"
#include "ChunkManager.h"
#include "Chunk.h"
#include "BlockType.h"
#include <nds.h>
#include <math.h>
#include <stdlib.h>
#include "WorldConfig.h"

#define V(v) floattov16(v)

static void texCoordCell(int col, int row, int corner) {
    int u0 = col * 16;
    int v0 = row * 16;
    int u1 = u0 + 16;
    int v1 = v0 + 16;

    switch (corner) {
        case 0: GFX_TEX_COORD = TEXTURE_PACK(TexUV::lo(u0), TexUV::hi(v1)); break;
        case 1: GFX_TEX_COORD = TEXTURE_PACK(TexUV::hi(u1), TexUV::hi(v1)); break;
        case 2: GFX_TEX_COORD = TEXTURE_PACK(TexUV::hi(u1), TexUV::lo(v0)); break;
        case 3: GFX_TEX_COORD = TEXTURE_PACK(TexUV::lo(u0), TexUV::lo(v0)); break;
    }
}

void DroppedItem::spawn(float spawnX, float spawnY, float spawnZ, BlockType blockType, bool isThrown,
                         float throwVelX, float throwVelY, float throwVelZ, int durabilityIn, int stackCount) {
    active = true;
    x = spawnX;
    y = spawnY;
    z = spawnZ;
    type = blockType;
    durability = durabilityIn;
    count = stackCount < 1 ? 1 : stackCount;
    health = 5;
    pickupDelayTicks = isThrown ? THROWN_PICKUP_DELAY_TICKS : PICKUP_DELAY_TICKS;

    prevX = x;
    prevY = y;
    prevZ = z;

    rotationPhase = (float)(rand() % 360);
    prevRotationPhase = rotationPhase;

    bobPhase = (float)(rand() % 100) / 100.0f * (2.0f * 3.14159265f);
    ageTicks = 0;

    settling = true;

    if (isThrown) {
        velocityX = throwVelX;
        velocityY = throwVelY;
        velocityZ = throwVelZ;

        // Jitter aleatorio adicional, confirmado por código real
        float jitterAngle = ((float)(rand() % 1000) / 1000.0f) * 2.0f * 3.14159265f;
        float jitterMag = 0.02f * ((float)(rand() % 1000) / 1000.0f);

        velocityX += cosf(jitterAngle) * jitterMag;
        velocityZ += sinf(jitterAngle) * jitterMag;

        float r1 = (float)(rand() % 1000) / 1000.0f;
        float r2 = (float)(rand() % 1000) / 1000.0f;
        velocityY += (r1 - r2) * 0.1f;
    } else {
        velocityX = ((float)(rand() % 200) / 1000.0f) - 0.1f;
        velocityY = 0.1f;
        velocityZ = ((float)(rand() % 200) / 1000.0f) - 0.1f;
    }

    pickingUp = false;
    pickupAnimTicks = 0;
    pickupCompleteFlag = false;
}

void DroppedItem::startPickupAnimation(float targetX, float targetY, float targetZ) {
    if (pickingUp) return;

    pickingUp = true;
    pickupAnimTicks = 0;
    pickupTargetX = targetX;
    pickupTargetY = targetY;
    pickupTargetZ = targetZ;

    settling = false;
    velocityX = velocityY = velocityZ = 0.0f;
}

bool DroppedItem::consumePickupComplete() {
    if (pickupCompleteFlag) {
        pickupCompleteFlag = false;
        return true;
    }
    return false;
}

void DroppedItem::update(const ChunkManager& chunkManager) {
    if (!active) return;

    prevX = x;
    prevY = y;
    prevZ = z;
    prevRotationPhase = rotationPhase;

    if (pickingUp) {
        pickupAnimTicks++;

        float t = (float)pickupAnimTicks / (float)PICKUP_ANIM_DURATION;
        if (t > 1.0f) t = 1.0f;

        x = x + (pickupTargetX - x) * t;
        y = y + (pickupTargetY - y) * t;
        z = z + (pickupTargetZ - z) * t;

        if (pickupAnimTicks >= PICKUP_ANIM_DURATION) {
            active = false;
            pickingUp = false;
            pickupCompleteFlag = true;
        }

        rotationPhase += ROTATION_SPEED;
        if (rotationPhase >= 360.0f) rotationPhase -= 360.0f;

        return;
    }

    // Si el chunk de abajo no esta cargado (por ejemplo, el jugador reaparecio lejos de donde murio) el objeto espera quieto y sin envejecer
    {
        Chunk* below = chunkManager.getChunkByChunkCoord((int)floorf((x + 4.0f) / 8.0f), (int)floorf((z + 4.0f) / 8.0f));
        if (!below || !below->isGenerationComplete()) return;
    }

    ageTicks++;
    if (ageTicks >= DESPAWN_TICKS) {
        active = false;
        return;
    }

    // Entity.setOnFireFromLava: la lava hace 4 de daño por tick (y 1 más por quemarse) a un ítem de 5 de vida: se pierde al instante.
    // Entity.moveEntity + World.isBoundingBoxBurning: un ítem dentro de un bloque de fuego recibe 1 de daño por tick (muere en 5 ticks).
    // Si es una pila, desaparece entera (es una sola entidad).
    if (chunkManager.lavaInBox(x - 0.05f, y + 0.1f, z - 0.05f, x + 0.05f, y + 0.15f, z + 0.05f)) {
        active = false;
        return;
    }
    if (chunkManager.fireInBox(x - 0.124f, y + 0.001f, z - 0.124f, x + 0.124f, y + 0.249f, z + 0.124f)) {
        health--;
        if (health <= 0) {
            active = false;
            return;
        }
    }

    if (settling) {
        velocityY = (velocityY - ITEM_GRAVITY) * ITEM_DRAG;
        velocityX *= ITEM_DRAG;
        velocityZ *= ITEM_DRAG;

        float targetX = x + velocityX;
        float targetY = y + velocityY;
        float targetZ = z + velocityZ;

        if (chunkManager.isSolidAt(targetX, targetY, targetZ)) {
            settling = false;
            velocityX = 0.0f;
            velocityY = 0.0f;
            velocityZ = 0.0f;
        } else {
            x = targetX;
            y = targetY;
            z = targetZ;
        }
    } else {
        if (!chunkManager.isSolidAt(x, y - 0.05f, z)) {
            settling = true;
        }
    }

    rotationPhase += ROTATION_SPEED;
    if (rotationPhase >= 360.0f) rotationPhase -= 360.0f;

    bobPhase += BOB_SPEED;
    if (bobPhase >= 2.0f * 3.14159265f) bobPhase -= 2.0f * 3.14159265f;
}

void DroppedItem::draw(float playerX, float playerY, float playerZ, float playerYaw, float partialTick) const {
    if (!active) return;

    float renderX = prevX + (x - prevX) * partialTick;
    float renderY = prevY + (y - prevY) * partialTick;
    float renderZ = prevZ + (z - prevZ) * partialTick;

    float rotDelta = rotationPhase - prevRotationPhase;
    if (rotDelta > 180.0f) rotDelta -= 360.0f;
    if (rotDelta < -180.0f) rotDelta += 360.0f;
    float renderRotation = prevRotationPhase + rotDelta * partialTick;

    float dx = renderX - playerX;
    float dy = renderY - playerY;
    float dz = renderZ - playerZ;
    float distSq = dx * dx + dy * dy + dz * dz;

    float maxDist = WorldConfig::renderDistance();
    if (distSq > maxDist * maxDist) return;

    float bobAmount = BOB_MIN + (BOB_MAX - BOB_MIN) * (0.5f + 0.5f * sinf(bobPhase));

    TextureCell top, side, bottom;
    getBlockTextures(type, top, side, bottom);

    u8 tintR, tintG, tintB;
    getGrassTint(type, false, tintR, tintG, tintB);
    const bool leavesBlock = type == BlockType::OAK_LEAVES || type == BlockType::BIRCH_LEAVES || type == BlockType::SPRUCE_LEAVES;   // las hojas se tiñen en las 6 caras (el cesped solo arriba)
    const u8 sideR = leavesBlock ? tintR : 255, sideG = leavesBlock ? tintG : 255, sideB = leavesBlock ? tintB : 255;

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
        glTranslatef(renderX, renderY + bobAmount, renderZ);
        if (!isFlatInHand(type)) glRotatef(renderRotation, 0.0f, 1.0f, 0.0f); // los planos miran al jugador dentro del bucle
        float itemScale = isFlatInHand(type) ? FLAT_ITEM_SCALE : ITEM_SCALE;
        glScalef(itemScale, itemScale, itemScale);

	// RenderItem: una pila se dibuja como 1, 2, 3 o 4 copias, con pequeños desplazamientos
        int copies = 1;
        if (count > 1) copies = 2;
        if (count > 5) copies = 3;
        if (count > 20) copies = 4;
        const bool flat = isFlatInHand(type);
        unsigned int seed = 187u; // fija: las copias no se mueven entre cuadros
        auto rnd = [&seed]() { seed = seed * 1664525u + 1013904223u; return (float)((seed >> 8) & 0xFFFF) / 65535.0f * 2.0f - 1.0f; };

        for (int copy = 0; copy < copies; copy++) {
            glPushMatrix();
            if (copy > 0) {
                float k = flat ? 0.3f : 0.2f / itemScale;
                glTranslatef(rnd() * k, rnd() * k, rnd() * k);
            }
            if (flat) glRotatef(playerYaw, 0.0f, 1.0f, 0.0f);

        if (isFlatInHand(type)) {
            bool fromItems = usesItemAtlas(type); // dentro de la rama plana, decide el atlas correcto
            glBindTexture(0, fromItems ? Texture::itemsTextureID : Texture::textureID);
            glColor3b(255, 255, 255);

            const float s = 0.5f;
            int u0 = side.col * 16, v0 = side.row * 16;
            int u1 = u0 + 16, v1 = v0 + 16;

            glBegin(GL_QUADS);
		GFX_TEX_COORD = TEXTURE_PACK(TexUV::lo(u0), TexUV::hi(v1)); glVertex3v16(V(-s), V(-0.25f), V(0));
                GFX_TEX_COORD = TEXTURE_PACK(TexUV::hi(u1), TexUV::hi(v1)); glVertex3v16(V( s), V(-0.25f), V(0));
                GFX_TEX_COORD = TEXTURE_PACK(TexUV::hi(u1), TexUV::lo(v0)); glVertex3v16(V( s), V( 0.75f), V(0));
                GFX_TEX_COORD = TEXTURE_PACK(TexUV::lo(u0), TexUV::lo(v0)); glVertex3v16(V(-s), V( 0.75f), V(0));
            glEnd();
        } else {
            // Bloque normal — cubo completo, sin cambios respecto a lo que ya tenías
            glBindTexture(0, Texture::textureID);

            // Boton y placas de presion: caja baja apoyada en la base del cubo (mismas medidas que en la mano)
            if (type == BlockType::BUTTON || isPlateBlock(type)) {
                const float sx = (type == BlockType::BUTTON) ? 6.0f / 16.0f : 14.0f / 16.0f;
                const float sy = (type == BlockType::BUTTON) ? 4.0f / 16.0f : 2.0f / 16.0f;
                const float sz = (type == BlockType::BUTTON) ? 2.0f / 16.0f : 14.0f / 16.0f;
                glTranslatef(0.0f, -(1.0f - sy) * 0.5f, 0.0f);
                glScalef(sx, sy, sz);
            }

            const float s = 0.5f;

            // Losas, escaleras y valla: varias cajas dentro de la celda (la escalera de mano es plana y no llega aqui)
            float shape[9][6] = { { 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f } };
            int shapeCount = 1;
            if (isStructureBlock(type) && type != BlockType::LADDER) shapeCount = Partial::shapeBoxes(type, type == BlockType::FENCE ? 12 : 0, shape);
            for (int sb = 0; sb < shapeCount; sb++) {
            glPushMatrix();
            if (isStructureBlock(type)) {
                const float bsx = shape[sb][3] - shape[sb][0], bsy = shape[sb][4] - shape[sb][1], bsz = shape[sb][5] - shape[sb][2];
                const float ox = (shape[sb][0] + shape[sb][3]) * 0.5f - 0.5f, oz = (shape[sb][2] + shape[sb][5]) * 0.5f - 0.5f;
                const float oy = isSlabBlock(type) ? 0.0f : (shape[sb][1] + shape[sb][4]) * 0.5f - 0.5f;   // la losa se centra para que gire sobre si misma
                glTranslatef(ox, oy, oz);
                glScalef(bsx, bsy, bsz);
            }

            glBegin(GL_QUADS);
                glColor3b(sideR, sideG, sideB);
                texCoordCell(side.col, side.row, 0); glVertex3v16(V(-s), V(-s), V( s));
                texCoordCell(side.col, side.row, 1); glVertex3v16(V( s), V(-s), V( s));
                texCoordCell(side.col, side.row, 2); glVertex3v16(V( s), V( s), V( s));
                texCoordCell(side.col, side.row, 3); glVertex3v16(V(-s), V( s), V( s));

                texCoordCell(side.col, side.row, 0); glVertex3v16(V( s), V(-s), V(-s));
                texCoordCell(side.col, side.row, 1); glVertex3v16(V(-s), V(-s), V(-s));
                texCoordCell(side.col, side.row, 2); glVertex3v16(V(-s), V( s), V(-s));
                texCoordCell(side.col, side.row, 3); glVertex3v16(V( s), V( s), V(-s));

                texCoordCell(side.col, side.row, 0); glVertex3v16(V(-s), V(-s), V(-s));
                texCoordCell(side.col, side.row, 1); glVertex3v16(V(-s), V(-s), V( s));
                texCoordCell(side.col, side.row, 2); glVertex3v16(V(-s), V( s), V( s));
                texCoordCell(side.col, side.row, 3); glVertex3v16(V(-s), V( s), V(-s));

                texCoordCell(side.col, side.row, 0); glVertex3v16(V( s), V(-s), V( s));
                texCoordCell(side.col, side.row, 1); glVertex3v16(V( s), V(-s), V(-s));
                texCoordCell(side.col, side.row, 2); glVertex3v16(V( s), V( s), V(-s));
                texCoordCell(side.col, side.row, 3); glVertex3v16(V( s), V( s), V( s));

                glColor3b(tintR, tintG, tintB);
                texCoordCell(top.col, top.row, 0); glVertex3v16(V(-s), V( s), V( s));
                texCoordCell(top.col, top.row, 1); glVertex3v16(V( s), V( s), V( s));
                texCoordCell(top.col, top.row, 2); glVertex3v16(V( s), V( s), V(-s));
                texCoordCell(top.col, top.row, 3); glVertex3v16(V(-s), V( s), V(-s));

                glColor3b(sideR, sideG, sideB);
                texCoordCell(bottom.col, bottom.row, 0); glVertex3v16(V(-s), V(-s), V(-s));
                texCoordCell(bottom.col, bottom.row, 1); glVertex3v16(V( s), V(-s), V(-s));
                texCoordCell(bottom.col, bottom.row, 2); glVertex3v16(V( s), V(-s), V( s));
                texCoordCell(bottom.col, bottom.row, 3); glVertex3v16(V(-s), V(-s), V( s));
            glEnd();
            glPopMatrix(1);
            }                  // cierra el for de cajas
            }                  // cierra el else (bloque normal)

            glPopMatrix(1);    // cierra la copia
        }                      // cierra el for de las copias

    glPopMatrix(1);            // la matriz de afuera (la del glPushMatrix inicial)
}                              // cierra draw

bool DroppedItem::canBePickedUp(float playerX, float playerFeetY, float playerZ) const {
    if (!active) return false;
    if (pickingUp) return false;
    if (ageTicks < pickupDelayTicks) return false; // antes: PICKUP_DELAY_TICKS fijo

    float dx = playerX - x;
    float dz = playerZ - z;
    float horizDistSq = dx * dx + dz * dz;
    if (horizDistSq > (PICKUP_RADIUS_H * PICKUP_RADIUS_H)) return false;

    float playerHeadY = playerFeetY + PLAYER_HEIGHT;
    if (y < playerFeetY - 0.2f || y > playerHeadY + 0.2f) return false;

    return true;
}