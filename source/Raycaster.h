#ifndef RAYCASTER_H
#define RAYCASTER_H

#include "BlockFace.h"

class ChunkManager;
class Chunk;

struct RaycastHit {
    bool hit;
    Chunk* chunk;
    int blockXi, blockYi, blockZi;
    float hitX, hitY, hitZ;
    BlockFace face;
};

class Raycaster {
public:
    static RaycastHit cast(const ChunkManager& chunkManager,
                            float originX, float originY, float originZ,
                            float dirX, float dirY, float dirZ,
                            float maxDistance, bool hitLiquidSources = false);
};

#endif