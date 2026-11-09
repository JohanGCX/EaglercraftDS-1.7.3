#ifndef CUBE_H
#define CUBE_H

#include <nds.h>

struct TextureCell {
    int col;
    int row;
};

// Texture coordinates inside one 16 x 16 cell of an atlas (terrain.png, items.png), in the DS 12.4 format (1 texel = 16 units).
// Every cell edge is pulled 1/8 of a texel towards the inside of the cell: the GPU's interpolation and rounding could otherwise land
// exactly on the first texel of the NEXT cell and show a thin line of an unrelated texture on the edge of a block (for example a pixel
// of the Nether portal on the yellow flower, its neighbour in terrain.png). 1/8 of a texel is invisible; the mob models already do it.
namespace TexUV {
    const int INSET = 2;                                                    // 12.4 units (1/8 texel)
    inline int lo(int texel) { return (texel << 4) + INSET; }               // a cell's left / top edge (u0, v0)
    inline int hi(int texel) { return (texel << 4) - INSET; }               // a cell's right / bottom edge (u1 = u0 + 16, v1 = v0 + 16)
    // any coordinate (in texels) of a quad that uses the cell starting at cellMin, kept inside that cell
    inline int clampIn(float texel, int cellMin) {
        const int t = (int)(texel * 16.0f), a = (cellMin << 4) + INSET, b = ((cellMin + 16) << 4) - INSET;
        return t < a ? a : (t > b ? b : t);
    }
}

struct FaceMask {
    bool top : 1;
    bool bottom : 1;
    bool north : 1;
    bool south : 1;
    bool east : 1;
    bool west : 1;
    FaceMask() : top(true), bottom(true), north(true), south(true), east(true), west(true) {}
};

class Cube {
public:
    void draw(TextureCell topCell, TextureCell sideCell, TextureCell bottomCell, FaceMask faces,
              u8 tintR, u8 tintG, u8 tintB, bool tintAllFaces = false);
    void drawOutline();
    // Contorno de una caja cualquiera, en coordenadas relativas al centro de la celda (la antorcha)
    void drawOutlineBox(float x0, float y0, float z0, float x1, float y1, float z1);
};

#endif