#include "MenuBackground.h"
#include "Texture.h"
#include <nds.h>

#define V(x) floattov16(x)

static void texCoordCell(int col, int row, int corner) {
    int u0 = col * 16;
    int v0 = row * 16;
    int u1 = u0 + 16;
    int v1 = v0 + 16;

    switch (corner) {
        case 0: GFX_TEX_COORD = TEXTURE_PACK(inttot16(u0), inttot16(v1)); break;
        case 1: GFX_TEX_COORD = TEXTURE_PACK(inttot16(u1), inttot16(v1)); break;
        case 2: GFX_TEX_COORD = TEXTURE_PACK(inttot16(u1), inttot16(v0)); break;
        case 3: GFX_TEX_COORD = TEXTURE_PACK(inttot16(u0), inttot16(v0)); break;
    }
}

void MenuBackground::draw() {
    const int TILES_X = 14;
    const int TILES_Y = 10;
    const float TILE_SIZE = 1.0f;
    const float DEPTH = -6.0f;

    // Celda de tierra en terrain.png (col=2, row=0)
    const int DIRT_COL = 2;
    const int DIRT_ROW = 0;

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
        glLoadIdentity();

        glBindTexture(0, Texture::textureID);
        glColor3b(80, 70, 60);

        float startX = -(TILES_X * TILE_SIZE) / 2.0f;
        float startY = -(TILES_Y * TILE_SIZE) / 2.0f;

        glBegin(GL_QUADS);
            for (int i = 0; i < TILES_X; i++) {
                for (int j = 0; j < TILES_Y; j++) {
                    float x1 = startX + i * TILE_SIZE;
                    float x2 = x1 + TILE_SIZE;
                    float y1 = startY + j * TILE_SIZE;
                    float y2 = y1 + TILE_SIZE;

                    texCoordCell(DIRT_COL, DIRT_ROW, 0); glVertex3v16(V(x1), V(y1), V(DEPTH));
                    texCoordCell(DIRT_COL, DIRT_ROW, 1); glVertex3v16(V(x2), V(y1), V(DEPTH));
                    texCoordCell(DIRT_COL, DIRT_ROW, 2); glVertex3v16(V(x2), V(y2), V(DEPTH));
                    texCoordCell(DIRT_COL, DIRT_ROW, 3); glVertex3v16(V(x1), V(y2), V(DEPTH));
                }
            }
        glEnd();

    glPopMatrix(1);
}