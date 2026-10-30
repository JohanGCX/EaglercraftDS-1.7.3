#include "FallingBlocks.h"
#include "ChunkManager.h"
#include "Texture.h"
#include "PendingDrops.h"
#include "WorldConfig.h"
#include <nds.h>
#include <math.h>

#define V(v) floattov16(v)

namespace {
    struct Falling {
        bool active;
        BlockType type;
        float x, y, z;       // centro del cubo
        float prevY;
        float vy;
        int fallTime;
    };
    const int MAX_FALLING = 12;
    Falling s_f[MAX_FALLING];

    const float GRAVITY = 0.04f;   // Entity: motionY -= 0.04 por tick, luego * 0.98
    const float DRAG = 0.98f;

    void texCoordCell(int col, int row, int corner) {
        int u0 = col * 16, v0 = row * 16, u1 = u0 + 16, v1 = v0 + 16;
        switch (corner) {
            case 0: GFX_TEX_COORD = TEXTURE_PACK(inttot16(u0), inttot16(v1)); break;
            case 1: GFX_TEX_COORD = TEXTURE_PACK(inttot16(u1), inttot16(v1)); break;
            case 2: GFX_TEX_COORD = TEXTURE_PACK(inttot16(u1), inttot16(v0)); break;
            case 3: GFX_TEX_COORD = TEXTURE_PACK(inttot16(u0), inttot16(v0)); break;
        }
    }

    bool placeable(BlockType t) {   // Block.canPlaceBlockAt: aire, liquido, fuego o nieve
        return t == BlockType::AIR || isLiquidBlock(t) || t == BlockType::FIRE || t == BlockType::SNOW_LAYER;
    }
}

void FallingBlocks::clear() {
    for (int i = 0; i < MAX_FALLING; i++) s_f[i].active = false;
}

bool FallingBlocks::spawn(float cx, float cy, float cz, BlockType type) {
    for (int i = 0; i < MAX_FALLING; i++) {
        if (s_f[i].active) continue;
        s_f[i].active = true;
        s_f[i].type = type;
        s_f[i].x = cx; s_f[i].y = cy; s_f[i].z = cz;
        s_f[i].prevY = cy;
        s_f[i].vy = 0.0f;
        s_f[i].fallTime = 0;
        return true;
    }
    return false;
}

void FallingBlocks::update(ChunkManager& cm) {
    for (int i = 0; i < MAX_FALLING; i++) {
        Falling& e = s_f[i];
        if (!e.active) continue;

        e.prevY = e.y;
        e.fallTime++;
        e.vy -= GRAVITY;
        const float dy = e.vy;
        e.vy *= DRAG;

        // se mueve en pasos de a lo sumo medio bloque para no atravesar un suelo delgado
        int steps = (int)(fabsf(dy) / 0.5f) + 1;
        bool landed = false;
        int solidCell = 0;
        for (int s = 0; s < steps && !landed; s++) {
            e.y += dy / steps;
            const float probe = e.y - 0.5f - 0.01f;   // justo debajo de la base del cubo
            if (cm.isSolidAt(e.x, probe, e.z)) {
                landed = true;
                solidCell = (int)floorf(probe + 1.5f);
            }
        }

        const int gx = (int)floorf(e.x + WorldConfig::SIZE_X / 2.0f);
        const int gz = (int)floorf(e.z + WorldConfig::SIZE_Z / 2.0f);

        if (landed) {
            const int cell = solidCell + 1;   // la celda que queda justo encima del suelo
            e.y = (float)cell - 1.0f;
            e.active = false;
            if (placeable(cm.getBlockTypeAt(e.x, e.y, e.z)) && cm.setBlockGlobal(gx, cell, gz, e.type)) continue;
            PendingDrops::push(e.x, e.y, e.z, e.type);   // no cabe: suelta el item (EntityFallingSand)
        } else if (e.fallTime > 100 || e.y < -3.0f) {
            e.active = false;
            PendingDrops::push(e.x, e.y < -1.0f ? 1.0f : e.y, e.z, e.type);
        }
    }
}

void FallingBlocks::draw(float playerX, float playerY, float playerZ, float partialTick) {
    const float maxDist = WorldConfig::renderDistance();
    for (int i = 0; i < MAX_FALLING; i++) {
        const Falling& e = s_f[i];
        if (!e.active) continue;

        const float ry = e.prevY + (e.y - e.prevY) * partialTick;
        const float dx = e.x - playerX, dy = ry - playerY, dz = e.z - playerZ;
        if (dx * dx + dy * dy + dz * dz > maxDist * maxDist) continue;

        TextureCell top, side, bottom;
        getBlockTextures(e.type, top, side, bottom);

        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
            glTranslatef(e.x, ry, e.z);
            glBindTexture(0, Texture::textureID);
            const float s = 0.49f;   // la caja de la entidad mide 0,98
            glBegin(GL_QUADS);
                glColor3b(255, 255, 255);
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

                texCoordCell(top.col, top.row, 0); glVertex3v16(V(-s), V( s), V( s));
                texCoordCell(top.col, top.row, 1); glVertex3v16(V( s), V( s), V( s));
                texCoordCell(top.col, top.row, 2); glVertex3v16(V( s), V( s), V(-s));
                texCoordCell(top.col, top.row, 3); glVertex3v16(V(-s), V( s), V(-s));

                texCoordCell(bottom.col, bottom.row, 0); glVertex3v16(V(-s), V(-s), V(-s));
                texCoordCell(bottom.col, bottom.row, 1); glVertex3v16(V( s), V(-s), V(-s));
                texCoordCell(bottom.col, bottom.row, 2); glVertex3v16(V( s), V(-s), V( s));
                texCoordCell(bottom.col, bottom.row, 3); glVertex3v16(V(-s), V(-s), V( s));
            glEnd();
        glPopMatrix(1);
    }
}
