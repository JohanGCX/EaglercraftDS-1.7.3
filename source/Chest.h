#ifndef CHEST_H
#define CHEST_H

#include "BlockType.h"

// =====================================================================
// Cofre de Beta 1.7.3 (BlockChest): sin orientación guardada. Qué cara es el frente, y si el cofre es la mitad izquierda o derecha
// de uno doble, se decide al dibujar y al colocar mirando a los vecinos. T debe ofrecer BlockType at(dx, dz): el bloque que está
// a (dx, dz) del cofre, en su misma altura.
// Caras laterales: 2 = norte (-z), 3 = sur (+z), 4 = oeste (-x), 5 = este (+x).
// =====================================================================
namespace Chest {

const int TEX_BASE = 26;   // terrain.png: 25 = arriba y abajo, 26 = lado, 27 = frente, 41/42 = frente del doble, 57/58 = fondo del doble

inline TextureCell cellOfIndex(int idx) { TextureCell c; c.col = idx % 16; c.row = idx / 16; return c; }

// BlockChest.getBlockTexture (caras laterales): índice de textura en terrain.png
template <class T>
int sideIndex(T& t, int face) {
    const int B = TEX_BASE;
    const BlockType n = t.at(0, -1), s = t.at(0, 1), w = t.at(-1, 0), e = t.at(1, 0);
    const bool cn = n == BlockType::CHEST, cs = s == BlockType::CHEST, cw = w == BlockType::CHEST, ce = e == BlockType::CHEST;
    auto opq = [](BlockType b) { return isLightOpaque(b); };   // Block.opaqueCubeLookup

    if (!cn && !cs) {
        if (!cw && !ce) {                       // cofre solo: el frente mira al lado opuesto de la pared
            int front = 3;
            if (opq(n) && !opq(s)) front = 3;
            if (opq(s) && !opq(n)) front = 2;
            if (opq(w) && !opq(e)) front = 5;
            if (opq(e) && !opq(w)) front = 4;
            return face == front ? B + 1 : B;
        } else if (face != 4 && face != 5) {    // doble a lo largo de X: caras norte y sur
            int off = 0;
            if (cw) off = -1;
            const int px = cw ? -1 : 1;
            const BlockType a = t.at(px, -1), b = t.at(px, 1);
            if (face == 3) off = -1 - off;
            int front = 3;
            if ((opq(n) || opq(a)) && !opq(s) && !opq(b)) front = 3;
            if ((opq(s) || opq(b)) && !opq(n) && !opq(a)) front = 2;
            return (face == front ? B + 16 : B + 32) + off;
        } else {
            return B;                           // las puntas del doble
        }
    } else if (face != 2 && face != 3) {        // doble a lo largo de Z: caras oeste y este
        int off = 0;
        if (cn) off = -1;
        const int pz = cn ? -1 : 1;
        const BlockType a = t.at(-1, pz), b = t.at(1, pz);
        if (face == 4) off = -1 - off;
        int front = 5;
        if ((opq(w) || opq(a)) && !opq(e) && !opq(b)) front = 5;
        if ((opq(e) || opq(b)) && !opq(w) && !opq(a)) front = 4;
        return (face == front ? B + 16 : B + 32) + off;
    }
    return B;
}

// BlockChest.canPlaceBlockAt: a lo sumo un cofre al lado, y ese no puede ser ya parte de un doble (no hay cofres triples)
template <class T>
bool canPlaceAt(T& t) {
    static const int D[4][2] = { {-1, 0}, {1, 0}, {0, -1}, {0, 1} };
    int count = 0;
    for (int i = 0; i < 4; i++) if (t.at(D[i][0], D[i][1]) == BlockType::CHEST) count++;
    if (count > 1) return false;
    for (int i = 0; i < 4; i++) {
        if (t.at(D[i][0], D[i][1]) != BlockType::CHEST) continue;
        for (int j = 0; j < 4; j++) {
            if (t.at(D[i][0] + D[j][0], D[i][1] + D[j][1]) == BlockType::CHEST) return false;
        }
    }
    return true;
}

}  // namespace Chest

#endif
