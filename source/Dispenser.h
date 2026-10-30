#ifndef DISPENSER_H
#define DISPENSER_H

#include "BlockType.h"

class ChunkManager;

// Dispensador (BlockDispenser + TileEntityDispenser, Beta 1.7.3). Su contenido (9 casillas) vive en ChestStore con la celda global del bloque.
// Cuando le llega redstone (tick programado a los 4 ticks) saca 1 objeto de una casilla al azar por el frente. Los objetos salen como objetos
// sueltos; las flechas, los huevos y las bolas de nieve salen como proyectiles (Projectiles).
namespace Dispenser {
    const int SLOTS = 9;

    struct Drop {
        float x, y, z;          // coordenadas de mundo
        float vx, vy, vz;       // velocidad por tick
        BlockType type;
        int durability;         // -1 si no es herramienta
    };

    void dispense(const ChunkManager& cm, int gx, int gy, int gz);   // BlockDispenser.dispenseItem
    bool popDrop(Drop& out);
    void clearQueue();
}

#endif
