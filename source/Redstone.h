#ifndef REDSTONE_H
#define REDSTONE_H

// Redstone de Beta 1.7.3: cable, antorcha, palanca, boton, placas de presion, repetidor y mena encendida.
// Es un port de BlockRedstoneWire, BlockRedstoneTorch, BlockLever, BlockButton, BlockPressurePlate, BlockRedstoneRepeater y BlockRedstoneOre,
// con las funciones de energia de World (isBlockProvidingPowerTo, isBlockGettingPowered, ...) y los ticks programados (scheduleBlockUpdate).
//
// Coordenadas globales de celda (gx, gy, gz), las mismas de PartialBlocks y LiquidWorld. La metadata va en la tabla de 4 bits de la celda (Chunk::getLiquidMeta):
//   Cable:     0 a 15, la fuerza de la corriente
//   Palanca:   1-4 en pared, 5-6 en el suelo; bit 8 = encendida
//   Boton:     1-4 en pared; bit 8 = pulsado
//   Placa:     1 = pisada
//   Antorcha:  1-5 como la antorcha normal (1 apoyo en -X, 2 +X, 3 -Z, 4 +Z, 5 abajo)
//   Repetidor: bits 0-1 direccion, bits 2-3 retardo
// Sin sonidos ni particulas (todavia).

#include <nds.h>
#include "BlockType.h"
#include "BlockFace.h"

class ChunkManager;

namespace Redstone {

    // Lo que la redstone necesita saber de las entidades (para las placas): main.cpp enumera las cajas en coordenadas del mundo
    typedef void (*EntitySink)(void* ctx, float minX, float minY, float minZ, float maxX, float maxY, float maxZ, bool isItem);
    typedef void (*EntityEnumerator)(void* user, EntitySink sink, void* ctx);   // llama a sink con la caja de cada entidad
    void setEntityEnumerator(EntityEnumerator e, void* user);
    void setMinecartEnumerator(EntityEnumerator e, void* user);   // las cajas de las vagonetas (para el riel detector); en coordenadas del mundo

    // Un tick del juego (20 por segundo): ticks programados + placas pisadas
    void tick(const ChunkManager& cm);

    // Tick aleatorio de una celda (ChunkManager::tickLiquids lo llama para los tipos que dice isRandomTicking)
    inline bool isRandomTicking(BlockType t) {
        return t == BlockType::BUTTON || isPlateBlock(t) || isRedstoneTorch(t) || t == BlockType::REDSTONE_ORE_LIT || t == BlockType::PISTON_MOVING || t == BlockType::RAIL_DETECTOR;
    }
    void randomTick(const ChunkManager& cm, int gx, int gy, int gz, BlockType t);

    // Un bloque cambio en (gx, gy, gz) (ya puesto o roto): se avisa a los vecinos y el bloque nuevo hace su onBlockAdded.
    // Lo llama ChunkManager::liquidBlockChanged, que Chunk::notifyLiquidNeighbors dispara tras cada cambio hecho por el jugador o por el juego.
    void blockChanged(const ChunkManager& cm, int gx, int gy, int gz);

    // Se quito o se cambio un bloque de redstone (Block.onBlockRemoval): Chunk lo llama desde setBlockDirect y breakBlockAt con lo que habia.
    // Se encola y se atiende en el siguiente blockChanged o tick.
    void blockRemoved(const ChunkManager& cm, int gx, int gy, int gz, BlockType oldType, int oldMeta);

    // Clic derecho o izquierdo sobre un bloque de redstone: palanca, boton, repetidor. true = el clic se gasto
    bool activate(const ChunkManager& cm, int gx, int gy, int gz);

    // La mena se enciende al golpearla, usarla o pisarla (BlockRedstoneOre.func_320_h)
    void lightOre(const ChunkManager& cm, int gx, int gy, int gz);

    // Poner el objeto: polvo de redstone, palanca, boton, placas, antorcha de redstone y repetidor. (gx, gy, gz) = celda apuntada
    bool useItem(const ChunkManager& cm, BlockType item, int gx, int gy, int gz, BlockFace face, float yawDeg);

    // Cajas (de 0 a 1) para dibujar y seleccionar. Los de Partial::bounds llaman aqui
    void bounds(BlockType t, int meta, float b[6]);

    // Para los pistones (Pistons.cpp): World.isBlockIndirectlyProvidingPowerTo y World.notifyBlocksOfNeighborChange
    bool indirectPowerAt(const ChunkManager& cm, int x, int y, int z, int side);
    void notifyNeighbors(const ChunkManager& cm, int x, int y, int z, BlockType id);

    void reset();   // al entrar a otro mundo o dimension: borra los ticks pendientes
}

#endif
