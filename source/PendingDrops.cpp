#include "PendingDrops.h"

PendingDrop PendingDrops::queue[16];
int PendingDrops::count = 0;

void PendingDrops::push(float x, float y, float z, BlockType type) {
    if (count >= 16) return;
    queue[count] = {x, y, z, type};
    count++;
}

bool PendingDrops::pop(PendingDrop& out) {
    if (count <= 0) return false;
    count--;
    out = queue[count];
    return true;
}