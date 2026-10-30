#ifndef PISTONS_H
#define PISTONS_H

// Pistones de Beta 1.7.3: BlockPistonBase, BlockPistonExtension, BlockPistonMoving y TileEntityPiston.
// Coordenadas globales de celda (gx, gy, gz), las mismas de Redstone.h.
//   Base:   metadata = direccion (0 abajo, 1 arriba, 2 -Z, 3 +Z, 4 -X, 5 +X) | 8 si esta extendido
//   Cabeza: metadata = direccion | 8 si es de un piston pegajoso
// El bloque que se esta moviendo (PISTON_MOVING) guarda lo que lleva en una lista de esta unidad (el TileEntityPiston).
// Sin sonidos ni empuje de entidades (todavia).

#include <nds.h>
#include "BlockType.h"
#include "BlockFace.h"

class ChunkManager;

namespace Pistons {

    void reset();                              // al entrar a otro mundo: sin bloques en movimiento
    void tick(const ChunkManager& cm);         // un tick del juego (20 por segundo): TileEntityPiston.updateEntity

    // Caja (de 0 a 1) para seleccionar y chocar. Los de Partial::bounds llaman aqui
    void bounds(BlockType t, int meta, float b[6]);

    // Avisos que manda Redstone.cpp
    void baseNeighborChanged(const ChunkManager& cm, int x, int y, int z);                 // BlockPistonBase.onNeighborBlockChange
    void baseAdded(const ChunkManager& cm, int x, int y, int z);                           // BlockPistonBase.onBlockAdded
    void headNeighborChanged(const ChunkManager& cm, int x, int y, int z);                 // BlockPistonExtension.onNeighborBlockChange
    void headRemoved(const ChunkManager& cm, int x, int y, int z, int meta);               // BlockPistonExtension.onBlockRemoval
    void cleanupOrphan(const ChunkManager& cm, int x, int y, int z);                       // PISTON_MOVING sin datos (mundo guardado a medias): se quita

    // ItemPiston / ItemBlock: poner el piston. (gx, gy, gz) = celda apuntada; yaw del jugador; posicion de sus ojos en el mundo
    bool useItem(const ChunkManager& cm, BlockType item, int gx, int gy, int gz, BlockFace face, float yawDeg,
                 float eyeX, float eyeY, float eyeZ);

    // ---- Dibujo ----
    // Un cuadrilatero: p = 4 vertices (x, y, z) dentro de la celda (0 a 1, mas el desplazamiento pedido), uv = 4 pares en texeles de terrain.png
    typedef void (*QuadFn)(void* ctx, const float p[12], const float uv[8]);
    void emitBase(QuadFn fn, void* ctx, BlockType t, int meta, bool forceExtended, float ox, float oy, float oz);
    void emitHead(QuadFn fn, void* ctx, int meta, bool longArm, int frontTexOverride, float ox, float oy, float oz);
    void emitBox(QuadFn fn, void* ctx, BlockType t, int meta, float ox, float oy, float oz);   // cualquier otro bloque, como caja con sus texturas

    // Los bloques en movimiento (se dibujan cada cuadro, como las entidades)
    void draw(float playerX, float playerY, float playerZ, float partialTick);
}

#endif
