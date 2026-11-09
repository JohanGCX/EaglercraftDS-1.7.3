#include "Cube.h"

#define V(x) floattov16(x)

static void texCoordCell(TextureCell cell, int corner) {
    int u0 = cell.col * 16;
    int v0 = cell.row * 16;
    int u1 = u0 + 16;
    int v1 = v0 + 16;

    switch (corner) {
        case 0: GFX_TEX_COORD = TEXTURE_PACK(TexUV::lo(u0), TexUV::hi(v1)); break;
        case 1: GFX_TEX_COORD = TEXTURE_PACK(TexUV::hi(u1), TexUV::hi(v1)); break;
        case 2: GFX_TEX_COORD = TEXTURE_PACK(TexUV::hi(u1), TexUV::lo(v0)); break;
        case 3: GFX_TEX_COORD = TEXTURE_PACK(TexUV::lo(u0), TexUV::lo(v0)); break;
    }
}

static void drawBox(float x1, float y1, float z1, float x2, float y2, float z2) {
    glBegin(GL_QUADS);
        glVertex3v16(V(x1), V(y1), V(z2));
        glVertex3v16(V(x2), V(y1), V(z2));
        glVertex3v16(V(x2), V(y2), V(z2));
        glVertex3v16(V(x1), V(y2), V(z2));

        glVertex3v16(V(x2), V(y1), V(z1));
        glVertex3v16(V(x1), V(y1), V(z1));
        glVertex3v16(V(x1), V(y2), V(z1));
        glVertex3v16(V(x2), V(y2), V(z1));

        glVertex3v16(V(x1), V(y1), V(z1));
        glVertex3v16(V(x1), V(y1), V(z2));
        glVertex3v16(V(x1), V(y2), V(z2));
        glVertex3v16(V(x1), V(y2), V(z1));

        glVertex3v16(V(x2), V(y1), V(z2));
        glVertex3v16(V(x2), V(y1), V(z1));
        glVertex3v16(V(x2), V(y2), V(z1));
        glVertex3v16(V(x2), V(y2), V(z2));

        glVertex3v16(V(x1), V(y2), V(z2));
        glVertex3v16(V(x2), V(y2), V(z2));
        glVertex3v16(V(x2), V(y2), V(z1));
        glVertex3v16(V(x1), V(y2), V(z1));

        glVertex3v16(V(x1), V(y1), V(z1));
        glVertex3v16(V(x2), V(y1), V(z1));
        glVertex3v16(V(x2), V(y1), V(z2));
        glVertex3v16(V(x1), V(y1), V(z2));
    glEnd();
}

void Cube::draw(TextureCell topCell, TextureCell sideCell, TextureCell bottomCell, FaceMask faces,
                 u8 tintR, u8 tintG, u8 tintB, bool tintAllFaces) {
    glBegin(GL_QUADS);
        glColor3b(tintAllFaces ? tintR : 255, tintAllFaces ? tintG : 255, tintAllFaces ? tintB : 255);

        // Cara frontal (+Z) — SOUTH
        if (faces.south) {
            texCoordCell(sideCell, 0); glVertex3v16(V(-0.5), V(-0.5), V( 0.5));
            texCoordCell(sideCell, 1); glVertex3v16(V( 0.5), V(-0.5), V( 0.5));
            texCoordCell(sideCell, 2); glVertex3v16(V( 0.5), V( 0.5), V( 0.5));
            texCoordCell(sideCell, 3); glVertex3v16(V(-0.5), V( 0.5), V( 0.5));
        }

        // Cara trasera (-Z) — NORTH
        if (faces.north) {
            texCoordCell(sideCell, 0); glVertex3v16(V( 0.5), V(-0.5), V(-0.5));
            texCoordCell(sideCell, 1); glVertex3v16(V(-0.5), V(-0.5), V(-0.5));
            texCoordCell(sideCell, 2); glVertex3v16(V(-0.5), V( 0.5), V(-0.5));
            texCoordCell(sideCell, 3); glVertex3v16(V( 0.5), V( 0.5), V(-0.5));
        }

        // Cara izquierda (-X) — WEST
        if (faces.west) {
            texCoordCell(sideCell, 0); glVertex3v16(V(-0.5), V(-0.5), V(-0.5));
            texCoordCell(sideCell, 1); glVertex3v16(V(-0.5), V(-0.5), V( 0.5));
            texCoordCell(sideCell, 2); glVertex3v16(V(-0.5), V( 0.5), V( 0.5));
            texCoordCell(sideCell, 3); glVertex3v16(V(-0.5), V( 0.5), V(-0.5));
        }

        // Cara derecha (+X) — EAST
        if (faces.east) {
            texCoordCell(sideCell, 0); glVertex3v16(V( 0.5), V(-0.5), V( 0.5));
            texCoordCell(sideCell, 1); glVertex3v16(V( 0.5), V(-0.5), V(-0.5));
            texCoordCell(sideCell, 2); glVertex3v16(V( 0.5), V( 0.5), V(-0.5));
            texCoordCell(sideCell, 3); glVertex3v16(V( 0.5), V( 0.5), V( 0.5));
        }

        // Cara inferior (-Y) — BOTTOM
        if (faces.bottom) {
            texCoordCell(bottomCell, 0); glVertex3v16(V(-0.5), V(-0.5), V(-0.5));
            texCoordCell(bottomCell, 1); glVertex3v16(V( 0.5), V(-0.5), V(-0.5));
            texCoordCell(bottomCell, 2); glVertex3v16(V( 0.5), V(-0.5), V( 0.5));
            texCoordCell(bottomCell, 3); glVertex3v16(V(-0.5), V(-0.5), V( 0.5));
        }
    glEnd();

    // Cara superior (+Y) — TOP, en un glBegin separado para poder
    // cambiar el color justo antes, sin afectar las demás caras
    if (faces.top) {
        glBegin(GL_QUADS);
            glColor3b(tintR, tintG, tintB);
            texCoordCell(topCell, 0); glVertex3v16(V(-0.5), V( 0.5), V( 0.5));
            texCoordCell(topCell, 1); glVertex3v16(V( 0.5), V( 0.5), V( 0.5));
            texCoordCell(topCell, 2); glVertex3v16(V( 0.5), V( 0.5), V(-0.5));
            texCoordCell(topCell, 3); glVertex3v16(V(-0.5), V( 0.5), V(-0.5));
        glEnd();
    }
}

void Cube::drawOutline() {
    const float s = 0.5f;
    const float e = 0.005f;  // antes 0.02f — margen mucho más ajustado a la cara real
    const float t = 0.008f;  // antes 0.025f — arista más delgada, más fiel al grosor real
    const float o = s + e;

    glColor3b(0, 0, 0);

    for (int sy = -1; sy <= 1; sy += 2) {
        for (int sz = -1; sz <= 1; sz += 2) {
            float y0 = sy * o;
            float z0 = sz * o;
            drawBox(-o, y0 - t, z0 - t, o, y0 + t, z0 + t);
        }
    }

    for (int sx = -1; sx <= 1; sx += 2) {
        for (int sz = -1; sz <= 1; sz += 2) {
            float x0 = sx * o;
            float z0 = sz * o;
            drawBox(x0 - t, -o, z0 - t, x0 + t, o, z0 + t);
        }
    }

    for (int sx = -1; sx <= 1; sx += 2) {
        for (int sy = -1; sy <= 1; sy += 2) {
            float x0 = sx * o;
            float y0 = sy * o;
            drawBox(x0 - t, y0 - t, -o, x0 + t, y0 + t, o);
        }
    }
}

void Cube::drawOutlineBox(float x0, float y0, float z0, float x1, float y1, float z1) {
    const float e = 0.005f;  // mismo margen y grosor que drawOutline
    const float t = 0.008f;
    const float X0 = x0 - e, X1 = x1 + e, Y0 = y0 - e, Y1 = y1 + e, Z0 = z0 - e, Z1 = z1 + e;

    glColor3b(0, 0, 0);

    const float ys[2] = { Y0, Y1 }, zs[2] = { Z0, Z1 }, xs[2] = { X0, X1 };
    for (int i = 0; i < 2; i++) {
        for (int j = 0; j < 2; j++) {
            drawBox(X0, ys[i] - t, zs[j] - t, X1, ys[i] + t, zs[j] + t); // aristas a lo largo de X
            drawBox(xs[i] - t, Y0, zs[j] - t, xs[i] + t, Y1, zs[j] + t); // a lo largo de Y
            drawBox(xs[i] - t, ys[j] - t, Z0, xs[i] + t, ys[j] + t, Z1); // a lo largo de Z
        }
    }
}