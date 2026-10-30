#ifndef FURNACE_H
#define FURNACE_H

#include "BlockType.h"
#include "Inventory.h"

class ChunkManager;

struct FurnaceSlot {
    BlockType type;
    int count;
    int durability;
    bool empty;
};

struct FurnaceData {
    bool used;
    int gx, gy, gz;      // coordenadas globales del bloque: chunkX*8+xi, yi, chunkZ*8+zi
    FurnaceSlot slot[3]; // 0 = ingrediente, 1 = combustible, 2 = resultado
    int burnTime;        // ticks de combustión que le quedan
    int burnTimeMax;     // duración total del combustible actual
    int cookTime;        // progreso de cocción (0..COOK_TIME)
    bool dirty;          // true si el último tick cambió algo visible
};

namespace Furnace {
    const int SLOT_INGREDIENT = 0;
    const int SLOT_FUEL = 1;
    const int SLOT_RESULT = 2;
    const int MAX_FURNACES = 32;
    const int COOK_TIME = 200;
    bool saveToFile(const char* path);
    bool loadFromFile(const char* path);

    FurnaceData* find(int gx, int gy, int gz);
    FurnaceData* findOrCreate(int gx, int gy, int gz); // nullptr si la tabla está llena
    void remove(int gx, int gy, int gz);
    void clearAll();

    // Un toque en el slot 'invSlot' del inventario con 'furnaceSlot' seleccionado.
    bool transferWithInventory(Inventory& inv, int invSlot, FurnaceData& f, int furnaceSlot);

    int fuelBurnTime(BlockType t); // 0 si no es combustible
    bool smeltResult(BlockType in, BlockType& out, int& outCount);

    // Un tick (1/20 s) de todos los hornos cuyo chunk está cargado
    void tickAll(const ChunkManager& cm);
}

#endif