#ifndef FALLINGBLOCKS_H
#define FALLINGBLOCKS_H

#include "BlockType.h"

class ChunkManager;

// EntityFallingSand de Beta 1.7.3: la arena y la grava sin apoyo se vuelven un bloque que cae y se vuelve a colocar al aterrizar
namespace FallingBlocks {
    void clear();                                                // sin bloques cayendo (mundo nuevo, cambio de dimension)
    bool spawn(float cx, float cy, float cz, BlockType type);    // centro del cubo en coordenadas de mundo; false si no hay lugar
    void update(ChunkManager& cm);                               // un tick del juego (20 Hz)
    void draw(float playerX, float playerY, float playerZ, float partialTick);
}

#endif
