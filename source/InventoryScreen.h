#ifndef INVENTORYSCREEN_H
#define INVENTORYSCREEN_H

#include "Inventory.h"

class InventoryScreen {
public:
    static void show(const Inventory& inventory, int heldSlot, int swapSelectedSlot);

    // Detección de touch: retorna el índice de slot (0-35) tocado,
    // o -1 si el toque no cayó sobre ningún slot
    static int getTouchedSlot(int touchX, int touchY);

    static int getBgId() { return bgId; }
    static void invalidate() { bgId = -1; }

private:
    static int bgId;
};

#endif