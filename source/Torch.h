#ifndef TORCH_H
#define TORCH_H

#include <math.h>
#include "BlockType.h"

// =====================================================================
// Antorcha de Minecraft Beta 1.7.3 (BlockTorch + ItemBlock + RenderBlocks.renderTorchAtAngle).
//
// La metadata del original (1..5) va en el TIPO de bloque e indica dónde está el bloque que la sostiene:
//     meta 1 = bloque de -X   (TORCH_W)      meta 2 = bloque de +X   (TORCH_E)
//     meta 3 = bloque de -Z   (TORCH_N)      meta 4 = bloque de +Z   (TORCH_S)
//     meta 5 = bloque de abajo (TORCH, la de piso; también las antorchas de mundos viejos)
//
// Todo lo de este archivo es lógica pura (sin Chunk ni libnds), así que se prueba en el host contra una
// copia literal del Java. Chunk.cpp, Raycaster.cpp y los tests usan exactamente este código.
// =====================================================================

namespace Torch {

// Cara del bloque clickeado, con los mismos códigos que Raycaster y AxisAlignedBB:
// 0 = abajo, 1 = arriba, 2 = -Z (NORTH), 3 = +Z (SOUTH), 4 = -X (WEST), 5 = +X (EAST). -1 = ninguna.
const int FACE_NONE = -1;

// BlockTorch.func_31032_h: cubo normal, o valla (que este juego no tiene)
template <class BlockAt>
inline bool floorSupport(BlockAt at, int x, int y, int z) {
    return isNormalCube(at(x, y, z));
}

// BlockTorch.canPlaceBlockAt: alcanza con que haya algún apoyo, sin importar la orientación
template <class BlockAt>
inline bool canPlaceAt(BlockAt at, int x, int y, int z) {
    if (isNormalCube(at(x - 1, y, z))) return true;
    if (isNormalCube(at(x + 1, y, z))) return true;
    if (isNormalCube(at(x, y, z - 1))) return true;
    if (isNormalCube(at(x, y, z + 1))) return true;
    return floorSupport(at, x, y - 1, z);
}

// BlockTorch.onBlockAdded: orientación automática. Prioridad: -X, +X, -Z, +Z y recién después el piso.
// Devuelve 0 si no hay ningún apoyo (en el original la antorcha se cae en ese caso).
template <class BlockAt>
inline int autoMeta(BlockAt at, int x, int y, int z) {
    if (isNormalCube(at(x - 1, y, z))) return 1;
    if (isNormalCube(at(x + 1, y, z))) return 2;
    if (isNormalCube(at(x, y, z - 1))) return 3;
    if (isNormalCube(at(x, y, z + 1))) return 4;
    if (floorSupport(at, x, y - 1, z)) return 5;
    return 0;
}

// ItemBlock.onItemUse + BlockTorch.onBlockPlaced: la cara clickeada puede cambiar la orientación,
// pero solo si esa cara ofrece un apoyo válido. 'meta' es la orientación ya elegida por autoMeta.
template <class BlockAt>
inline int metaForPlacement(BlockAt at, int x, int y, int z, int clickedFace, int meta) {
    if (clickedFace == 1 && floorSupport(at, x, y - 1, z)) meta = 5;
    if (clickedFace == 2 && isNormalCube(at(x, y, z + 1))) meta = 4;
    if (clickedFace == 3 && isNormalCube(at(x, y, z - 1))) meta = 3;
    if (clickedFace == 4 && isNormalCube(at(x + 1, y, z))) meta = 2;
    if (clickedFace == 5 && isNormalCube(at(x - 1, y, z))) meta = 1;
    return meta;
}

// BlockTorch.onNeighborBlockChange: la antorcha se cae si no queda ningún apoyo, o si falta SU apoyo.
// (Si su apoyo está, canPlaceAt es verdadero, así que se reduce a mirar solo el apoyo propio.)
template <class BlockAt>
inline bool mustDrop(BlockAt at, int x, int y, int z, int meta) {
    if (!canPlaceAt(at, x, y, z)) return true;
    if (meta == 1 && !isNormalCube(at(x - 1, y, z))) return true;
    if (meta == 2 && !isNormalCube(at(x + 1, y, z))) return true;
    if (meta == 3 && !isNormalCube(at(x, y, z - 1))) return true;
    if (meta == 4 && !isNormalCube(at(x, y, z + 1))) return true;
    if (meta == 5 && !floorSupport(at, x, y - 1, z)) return true;
    return false;
}

// ---------------------------------------------------------------------------------------------
// Caja de selección: BlockTorch.collisionRayTrace + Block.collisionRayTrace + AxisAlignedBB.func_1169_a
// ---------------------------------------------------------------------------------------------
struct Box { float x0, y0, z0, x1, y1, z1; };

inline Box boundsForMeta(int meta) {
    float s = 0.15f;
    if (meta == 1) return { 0.0f,        0.2f, 0.5f - s,    s * 2.0f,    0.8f, 0.5f + s };
    if (meta == 2) return { 1.0f - s * 2.0f, 0.2f, 0.5f - s, 1.0f,       0.8f, 0.5f + s };
    if (meta == 3) return { 0.5f - s,    0.2f, 0.0f,        0.5f + s,    0.8f, s * 2.0f };
    if (meta == 4) return { 0.5f - s,    0.2f, 1.0f - s * 2.0f, 0.5f + s, 0.8f, 1.0f };
    s = 0.1f;
    return { 0.5f - s, 0.0f, 0.5f - s, 0.5f + s, 0.6f, 0.5f + s };
}

struct RayHit { bool hit; int face; float x, y, z; }; // x,y,z en coordenadas de la celda (0..1)

// Vec3D.getIntermediateWithXValue / Y / Z: punto del segmento a-b donde la coordenada vale 'v', o nada
inline bool intermediate(const float a[3], const float b[3], int axis, float v, float out[3]) {
    float d0 = b[0] - a[0], d1 = b[1] - a[1], d2 = b[2] - a[2];
    float d = (axis == 0) ? d0 : (axis == 1 ? d1 : d2);
    if (d * d < 1.0E-7f) return false;
    float t = (v - a[axis]) / d;
    if (t < 0.0f || t > 1.0f) return false;
    out[0] = a[0] + d0 * t; out[1] = a[1] + d1 * t; out[2] = a[2] + d2 * t;
    return true;
}

// Block.collisionRayTrace: el segmento start->end (en coordenadas de la celda) contra la caja.
// Devuelve la intersección más cercana a 'start' y la cara: 0 abajo, 1 arriba, 2 -Z, 3 +Z, 4 -X, 5 +X.
inline RayHit rayVsBox(const Box& b, const float start[3], const float end[3]) {
    float p[6][3]; bool ok[6];
    ok[0] = intermediate(start, end, 0, b.x0, p[0]);   // cara 4 (-X)
    ok[1] = intermediate(start, end, 0, b.x1, p[1]);   // cara 5 (+X)
    ok[2] = intermediate(start, end, 1, b.y0, p[2]);   // cara 0 (abajo)
    ok[3] = intermediate(start, end, 1, b.y1, p[3]);   // cara 1 (arriba)
    ok[4] = intermediate(start, end, 2, b.z0, p[4]);   // cara 2 (-Z)
    ok[5] = intermediate(start, end, 2, b.z1, p[5]);   // cara 3 (+Z)
    const int faceOf[6] = { 4, 5, 0, 1, 2, 3 };

    // isVecInside{YZ,XZ,XY}Bounds: los límites son inclusivos
    auto inYZ = [&](const float* v) { return v[1] >= b.y0 && v[1] <= b.y1 && v[2] >= b.z0 && v[2] <= b.z1; };
    auto inXZ = [&](const float* v) { return v[0] >= b.x0 && v[0] <= b.x1 && v[2] >= b.z0 && v[2] <= b.z1; };
    auto inXY = [&](const float* v) { return v[0] >= b.x0 && v[0] <= b.x1 && v[1] >= b.y0 && v[1] <= b.y1; };
    if (ok[0] && !inYZ(p[0])) ok[0] = false;
    if (ok[1] && !inYZ(p[1])) ok[1] = false;
    if (ok[2] && !inXZ(p[2])) ok[2] = false;
    if (ok[3] && !inXZ(p[3])) ok[3] = false;
    if (ok[4] && !inXY(p[4])) ok[4] = false;
    if (ok[5] && !inXY(p[5])) ok[5] = false;

    int best = -1; float bestD = 0.0f;
    for (int i = 0; i < 6; i++) {
        if (!ok[i]) continue;
        float dx = p[i][0] - start[0], dy = p[i][1] - start[1], dz = p[i][2] - start[2];
        float d = dx * dx + dy * dy + dz * dz;
        if (best < 0 || d < bestD) { best = i; bestD = d; }   // empate: gana la primera, como el original (compara con <)
    }
    RayHit r; r.hit = best >= 0; r.face = best >= 0 ? faceOf[best] : -1;
    r.x = r.y = r.z = 0.0f;
    if (best >= 0) { r.x = p[best][0]; r.y = p[best][1]; r.z = p[best][2]; }
    return r;
}

// ---------------------------------------------------------------------------------------------
// Modelo: RenderBlocks.renderBlockTorch + renderTorchAtAngle. 5 quads = 20 vértices.
// Coordenadas RELATIVAS AL CENTRO de la celda (la celda va de -0.5 a +0.5). UV en texels dentro de la celda
// de textura de 16x16 (u hacia la derecha, v hacia abajo).
// ---------------------------------------------------------------------------------------------
struct Vtx { float x, y, z, u, v; };
const int MODEL_VERTS = 20;

inline void buildModel(int meta, Vtx out[MODEL_VERTS]) {
    // renderBlockTorch: desplazamiento de la base y inclinación según la orientación
    const float lean = 0.4f;
    float dx = 0.0f, dz = 0.0f, tiltX = 0.0f, tiltZ = 0.0f, baseY = 0.0f;
    const float shift = 0.5f - lean; // 0.1
    if (meta == 1)      { dx = -shift; tiltX = -lean; baseY = 0.2f; }
    else if (meta == 2) { dx =  shift; tiltX =  lean; baseY = 0.2f; }
    else if (meta == 3) { dz = -shift; tiltZ = -lean; baseY = 0.2f; }
    else if (meta == 4) { dz =  shift; tiltZ =  lean; baseY = 0.2f; }

    // renderTorchAtAngle (pasado a coordenadas relativas al centro: el original suma 0.5 a x y z, y la base y
    // está medida desde el borde inferior de la celda)
    const float cx = dx, cz = dz;
    const float yb = baseY - 0.5f;
    const float s = 1.0f / 16.0f;       // var36
    const float k = 0.625f;             // var38: altura de la tapa
    const float cap0x = cx + tiltX * (1.0f - k), cap0z = cz + tiltZ * (1.0f - k);

    int n = 0;
    auto V = [&](float x, float y, float z, float u, float v) { out[n].x = x; out[n].y = y; out[n].z = z; out[n].u = u; out[n].v = v; n++; };

    // tapa 2x2 (la llama): texels u 7..9, v 6..8
    V(cap0x - s, yb + k, cap0z - s, 7.0f, 6.0f);
    V(cap0x - s, yb + k, cap0z + s, 7.0f, 8.0f);
    V(cap0x + s, yb + k, cap0z + s, 9.0f, 8.0f);
    V(cap0x + s, yb + k, cap0z - s, 9.0f, 6.0f);

    // cuatro planos verticales de la altura de la celda, la base corrida por la inclinación
    const float U0 = 0.0f, U1 = 15.99f, V0 = 0.0f, V1 = 15.99f; // el original usa (texel + 15.99) / 256
    // plano en x = cx - s
    V(cx - s,         yb + 1.0f, cz - 0.5f,         U0, V0);
    V(cx - s + tiltX, yb,        cz - 0.5f + tiltZ, U0, V1);
    V(cx - s + tiltX, yb,        cz + 0.5f + tiltZ, U1, V1);
    V(cx - s,         yb + 1.0f, cz + 0.5f,         U1, V0);
    // plano en x = cx + s
    V(cx + s,         yb + 1.0f, cz + 0.5f,         U0, V0);
    V(cx + tiltX + s, yb,        cz + 0.5f + tiltZ, U0, V1);
    V(cx + tiltX + s, yb,        cz - 0.5f + tiltZ, U1, V1);
    V(cx + s,         yb + 1.0f, cz - 0.5f,         U1, V0);
    // plano en z = cz + s
    V(cx - 0.5f,         yb + 1.0f, cz + s,         U0, V0);
    V(cx - 0.5f + tiltX, yb,        cz + s + tiltZ, U0, V1);
    V(cx + 0.5f + tiltX, yb,        cz + s + tiltZ, U1, V1);
    V(cx + 0.5f,         yb + 1.0f, cz + s,         U1, V0);
    // plano en z = cz - s
    V(cx + 0.5f,         yb + 1.0f, cz - s,         U0, V0);
    V(cx + 0.5f + tiltX, yb,        cz - s + tiltZ, U0, V1);
    V(cx - 0.5f + tiltX, yb,        cz - s + tiltZ, U1, V1);
    V(cx - 0.5f,         yb + 1.0f, cz - s,         U1, V0);
}

}  // namespace Torch

#endif
