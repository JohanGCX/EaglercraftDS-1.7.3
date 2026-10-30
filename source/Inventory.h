#ifndef INVENTORY_H
#define INVENTORY_H

#include "BlockType.h"

class Inventory {
public:
    static const int HOTBAR_SIZE = 9;
    static const int TOTAL_SIZE = 36; // 9 hotbar + 27 inventario extendido
    static const int MAX_STACK = 64;
    // Armadura (InventoryPlayer.armorInventory): ranuras 36 a 39 = casco, peto, pantalones, botas. Los bucles de TOTAL_SIZE no las incluyen a proposito
    static const int ARMOR_BASE = 36;
    static const int ARMOR_SLOTS = 4;
    static const int SLOT_COUNT = ARMOR_BASE + ARMOR_SLOTS;

    Inventory();

    // Agrega 'quantity' unidades de 'type'. Rellena primero los slots
    // existentes del mismo tipo (el MÁS lleno primero, para minimizar
    // slots parcialmente usados), y solo usa slots vacíos cuando ya no
    // hay espacio en los existentes. Retorna cuántas unidades NO
    // pudieron agregarse (0 si todo cupo, inventario lleno si no).
    int addItem(BlockType type, int quantity, bool& usedNewSlot);
    bool canAccept(BlockType type) const;

    void removeOneFromSlot(int slotIndex);
    void setSlotDirect(int slotIndex, BlockType type, int count);

    bool isSlotEmpty(int slotIndex) const;
    BlockType getSlotType(int slotIndex) const;
    int getSlotCount(int slotIndex) const;

    // Mecánica tipo Pocket Edition: mismo tipo con espacio -> transfiere
    // de A a B hasta llenar B; si no aplica -> intercambia posiciones
    void swapOrMerge(int slotA, int slotB);

    void removeFromSlot(int slotIndex, int amount);

    // Agrega una herramienta nueva SIEMPRE en un slot vacío (nunca
    // apila), con durabilidad completa. Retorna false si no hay espacio.
    bool addTool(BlockType toolType);

    int getSlotDurability(int slotIndex) const;
    void setSlotDurability(int slotIndex, int durability);

    // Reduce 1 punto de durabilidad; si llega a 0, destruye la
    // herramienta (vacía el slot). Retorna false si el slot se rompió.
    bool reduceDurability(int slotIndex, int amount = 1);

    // InventoryPlayer.getTotalArmorValue: (suma de proteccion - 1) * durabilidad que queda / durabilidad maxima + 1 (0 sin armadura)
    int getTotalArmorValue() const;
    // InventoryPlayer.damageArmor: cada pieza puesta pierde damage / 4 (minimo 1); la que se gasta se rompe
    void damageArmor(int damage);
    bool addToolWithDurability(BlockType toolType, int durability);

private:
    BlockType slotType[SLOT_COUNT];
    int slotCount[SLOT_COUNT];
    bool slotEmpty[SLOT_COUNT];
    int slotDurability[SLOT_COUNT];
};

#endif