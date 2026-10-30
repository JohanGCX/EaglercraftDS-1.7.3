#ifndef CUBE_H
#define CUBE_H

#include <nds.h>

struct TextureCell {
    int col;
    int row;
};

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