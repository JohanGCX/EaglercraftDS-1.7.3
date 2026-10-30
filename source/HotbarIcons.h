#ifndef HOTBARICONS_H
#define HOTBARICONS_H

#include "Inventory.h"

class HotbarIcons {
public:
    // Redibuja el fondo limpio de la hotbar y todos los íconos de
    // bloque según el inventario actual
    static void render(const Inventory& inventory, int selectedSlot);
};

#endif