#ifndef TREES_H
#define TREES_H

#include "BlockType.h"

class ChunkManager;

// Retoños (BlockSapling), crecimiento de arboles (WorldGenTrees / WorldGenForest / WorldGenTaiga2 / WorldGenBigTree) y caida de hojas (BlockLeaves.updateTick).
// Coordenadas globales de celda: gx = chunkX * SIZE_X + xi, gy = yi, gz = chunkZ * SIZE_Z + zi.
namespace Trees {
    // BlockSapling.growTree: el retoño de (gx, gy, gz) se vuelve un arbol (roble, a veces grande; abeto; abedul). Si no hay sitio, queda el retoño. true si crecio.
    bool grow(const ChunkManager& cm, int gx, int gy, int gz);

    // BlockSapling.updateTick (tick aleatorio): con luz 9 o mas, 1 de cada 30 pasa a la segunda etapa y la siguiente vez crece
    void saplingTick(const ChunkManager& cm, int gx, int gy, int gz);

    // BlockLeaves.updateTick (tick aleatorio): una hoja marcada (meta 8) se queda si hay un tronco a 4 pasos o menos por otras hojas; si no, se cae
    void leafTick(const ChunkManager& cm, int gx, int gy, int gz);

    // Block.onBlockRemoval de BlockLog (marca las hojas a 4 celdas) y de BlockLeaves (las vecinas): las hojas marcadas se comprueban en su tick
    void onBlockRemoved(const ChunkManager& cm, int gx, int gy, int gz, BlockType old);

    BlockType saplingOfLeaves(BlockType leaves);   // retoño que suelta ese tipo de hoja
}

#endif
