#ifndef CHESTSTORE_H
#define CHESTSTORE_H

#include "BlockType.h"
#include "Inventory.h"

// Contenido de los cofres (TileEntityChest): 27 casillas por cofre, guardado por posición como el horno. Un cofre doble son
// dos de estos juntos (InventoryLargeChest): el menú los muestra como 54 casillas.
struct ChestSlot {
    u8 type;          // BlockType
    u8 count;         // 0 = vacía
    u16 durability;
};

struct ChestData {
    bool used;
    int gx, gy, gz;   // celda global del cofre: chunkX*8+xi, yi, chunkZ*8+zi
    ChestSlot slot[27];
};

// Lo que muestra el menú: un cofre (3 filas) o un doble (6 filas). part[0] es la mitad de arriba (el cofre de menor x o z)
struct ChestView {
    int rows;
    bool dispenser = false;   // dispensador: 9 casillas en cuadricula de 3 x 3 (rows = 1, part[0])
    ChestData* part[2];   // part[1] es nullptr si no es doble

    ChestSlot* slotAt(int i) const {
        if (i < 0 || i >= rows * 9) return nullptr;
        if (i < 27) return part[0] ? &part[0]->slot[i] : nullptr;
        return part[1] ? &part[1]->slot[i - 27] : nullptr;
    }
};

namespace ChestStore {
    const int MAX_CHESTS = 64;
    const int SLOTS = 27;

    ChestData* find(int gx, int gy, int gz);
    ChestData* findOrCreate(int gx, int gy, int gz); // nullptr si la tabla está llena
    int usedCount();
    void remove(int gx, int gy, int gz);
    void clearAll();

    // Un toque en la casilla 'invSlot' del inventario con la casilla 'cs' del cofre seleccionada (pila completa, como el horno):
    // casilla vacía recibe la pila; inventario vacío recibe la pila del cofre; mismo ítem apilable se funde hacia el cofre;
    // si no, se intercambian.
    bool transferWithInventory(Inventory& inv, int invSlot, ChestSlot& cs);

    bool saveToFile(const char* path);
    bool loadFromFile(const char* path);
}

#endif