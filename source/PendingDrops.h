#ifndef PENDINGDROPS_H
#define PENDINGDROPS_H

#include "BlockType.h"

struct PendingDrop {
    float x, y, z;
    BlockType type;
};

class PendingDrops {
public:
    static void push(float x, float y, float z, BlockType type);
    static bool pop(PendingDrop& out);

private:
    static PendingDrop queue[16];
    static int count;
};

#endif