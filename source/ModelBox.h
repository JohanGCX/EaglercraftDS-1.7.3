#ifndef MODELBOX_H
#define MODELBOX_H

#include <math.h>

// Port de ModelRenderer.addBox + TexturedQuad (Beta 1.7.3), sin libnds: arma las 6 caras de una caja con sus vértices y coordenadas de textura.
// Posiciones en unidades de modelo (1/16 de bloque); texturas en texeles.
namespace ModelBox {
    struct Vertex { float x, y, z; float u, v; };
    struct Face { Vertex v[4]; };

    // x, y, z: esquina mínima; w, h, d: tamaño; inflate: engorde (ModelRenderer.addBox var7); tx, ty: origen en la textura; vOffset: filas que
    // se suman a la v (la lana va 32 filas más abajo en la imagen combinada). uvInset: el 0,1 de texel que TexturedQuad recorta en cada borde.
    inline void build(Face out[6], int tx, int ty, float x, float y, float z, int w, int h, int d, float inflate, int vOffset, float uvInset) {
        float x2 = x + (float)w, y2 = y + (float)h, z2 = z + (float)d;
        const float x1 = x - inflate, y1 = y - inflate, z1 = z - inflate;
        x2 += inflate; y2 += inflate; z2 += inflate;
        const float c[8][3] = { {x1, y1, z1}, {x2, y1, z1}, {x2, y2, z1}, {x1, y2, z1}, {x1, y1, z2}, {x2, y1, z2}, {x2, y2, z2}, {x1, y2, z2} };

        struct Def { int idx[4]; int u1, v1, u2, v2; };
        const Def defs[6] = {
            { {5, 1, 2, 6}, tx + d + w,         ty + d, tx + d + w + d,     ty + d + h },   // cara x2
            { {0, 4, 7, 3}, tx + 0,             ty + d, tx + d,             ty + d + h },   // cara x1
            { {5, 4, 0, 1}, tx + d,             ty + 0, tx + d + w,         ty + d },       // cara y1
            { {2, 3, 7, 6}, tx + d + w,         ty + 0, tx + d + w + w,     ty + d },       // cara y2
            { {1, 0, 3, 2}, tx + d,             ty + d, tx + d + w,         ty + d + h },   // cara z1
            { {4, 5, 6, 7}, tx + d + w + d,     ty + d, tx + d + w + d + w, ty + d + h },   // cara z2
        };
        for (int f = 0; f < 6; f++) {
            const Def& q = defs[f];
            const float u1 = (float)q.u1 + uvInset, u2 = (float)q.u2 - uvInset;
            const float v1 = (float)(q.v1 + vOffset) + uvInset, v2 = (float)(q.v2 + vOffset) - uvInset;
            const float us[4] = { u2, u1, u1, u2 };   // TexturedQuad: vértice 0 = (u2, v1), 1 = (u1, v1), 2 = (u1, v2), 3 = (u2, v2)
            const float vs[4] = { v1, v1, v2, v2 };
            for (int k = 0; k < 4; k++) out[f].v[k] = { c[q.idx[k]][0], c[q.idx[k]][1], c[q.idx[k]][2], us[k], vs[k] };
        }
    }
}

#endif