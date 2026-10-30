#include "Inventory.h"

Inventory::Inventory() {
    for (int i = 0; i < SLOT_COUNT; i++) {
        slotEmpty[i] = true;
        slotCount[i] = 0;
        slotDurability[i] = 0;
    }
}

bool Inventory::addTool(BlockType toolType) {
    for (int i = 0; i < TOTAL_SIZE; i++) {
        if (slotEmpty[i]) {
            slotType[i] = toolType;
            slotCount[i] = 1;
            slotEmpty[i] = false;
            slotDurability[i] = getToolMaxDurability(toolType);
            return true;
        }
    }
    return false; // inventario lleno
}

int Inventory::getSlotDurability(int slotIndex) const {
    if (slotIndex < 0 || slotIndex >= SLOT_COUNT) return 0;
    return slotDurability[slotIndex];
}

void Inventory::setSlotDurability(int slotIndex, int durability) {
    if (slotIndex < 0 || slotIndex >= SLOT_COUNT) return;
    slotDurability[slotIndex] = durability;
}

bool Inventory::canAccept(BlockType type) const {
    for (int i = 0; i < TOTAL_SIZE; i++) {
        if (slotEmpty[i]) return true;
        if (slotType[i] == type && slotCount[i] < maxStackFor(type)) return true;
    }
    return false;
}

bool Inventory::reduceDurability(int slotIndex, int amount) {
    if (slotIndex < 0 || slotIndex >= SLOT_COUNT) return true;
    if (slotEmpty[slotIndex]) return true;

    slotDurability[slotIndex] -= amount;
    if (slotDurability[slotIndex] < 0) {
        slotEmpty[slotIndex] = true;
        slotCount[slotIndex] = 0;
        slotDurability[slotIndex] = 0;
        return false; // se rompió
    }
    return true;
}

int Inventory::addItem(BlockType type, int quantity, bool& usedNewSlot) {
    usedNewSlot = false;
    int remaining = quantity;

    // Fase 1: rellenar slots EXISTENTES del mismo tipo, el más lleno primero
    while (remaining > 0) {
        int bestSlot = -1;
        int bestCount = -1;

        for (int i = 0; i < TOTAL_SIZE; i++) {
            if (!slotEmpty[i] && slotType[i] == type && slotCount[i] < maxStackFor(type)) {
                if (slotCount[i] > bestCount) {
                    bestCount = slotCount[i];
                    bestSlot = i;
                }
            }
        }

        if (bestSlot == -1) break;

        int space = maxStackFor(type) - slotCount[bestSlot];
        int toAdd = (remaining < space) ? remaining : space;
        slotCount[bestSlot] += toAdd;
        remaining -= toAdd;
    }

    // Fase 2: slots vacíos para lo que sobre, en stacks de hasta 64
    while (remaining > 0) {
        int emptySlot = -1;
        for (int i = 0; i < TOTAL_SIZE; i++) {
            if (slotEmpty[i]) { emptySlot = i; break; }
        }
        if (emptySlot == -1) break; // inventario lleno

        int limit = maxStackFor(type);
        int toAdd = (remaining < limit) ? remaining : limit;
        slotType[emptySlot] = type;
        slotCount[emptySlot] = toAdd;
        slotEmpty[emptySlot] = false;
        remaining -= toAdd;
        usedNewSlot = true;
    }

    return remaining;
}

void Inventory::removeOneFromSlot(int slotIndex) {
    if (slotIndex < 0 || slotIndex >= SLOT_COUNT) return;
    if (slotEmpty[slotIndex]) return;

    slotCount[slotIndex]--;
    if (slotCount[slotIndex] <= 0) {
        slotEmpty[slotIndex] = true;
        slotCount[slotIndex] = 0;
    }
}

void Inventory::setSlotDirect(int slotIndex, BlockType type, int count) {
    if (slotIndex < 0 || slotIndex >= SLOT_COUNT) return;

    if (count <= 0) {
        slotEmpty[slotIndex] = true;
        slotCount[slotIndex] = 0;
    } else {
        slotType[slotIndex] = type;
        slotCount[slotIndex] = count;
        slotEmpty[slotIndex] = false;
    }
}

bool Inventory::isSlotEmpty(int slotIndex) const {
    if (slotIndex < 0 || slotIndex >= SLOT_COUNT) return true;
    return slotEmpty[slotIndex];
}

BlockType Inventory::getSlotType(int slotIndex) const {
    return slotType[slotIndex];
}

int Inventory::getSlotCount(int slotIndex) const {
    return slotCount[slotIndex];
}

void Inventory::swapOrMerge(int slotA, int slotB) {
    if (slotA == slotB) return;
    if (slotA < 0 || slotA >= SLOT_COUNT || slotB < 0 || slotB >= SLOT_COUNT) return;

    bool aEmpty = slotEmpty[slotA];
    bool bEmpty = slotEmpty[slotB];

    // Ranuras de armadura: solo aceptan la pieza que les toca (ContainerPlayer: SlotArmor)
    if (slotA >= ARMOR_BASE && !bEmpty && !(isArmor(slotType[slotB]) && armorPiece(slotType[slotB]) == slotA - ARMOR_BASE)) return;
    if (slotB >= ARMOR_BASE && !aEmpty && !(isArmor(slotType[slotA]) && armorPiece(slotType[slotA]) == slotB - ARMOR_BASE)) return;

    // Las herramientas NUNCA se apilan/fusionan (isTool), incluso si
    // por casualidad son del mismo tipo — siempre deben intercambiarse
    bool eligibleForMerge = !aEmpty && !bEmpty &&
                             slotType[slotA] == slotType[slotB] &&
                             !hasDurability(slotType[slotA]);

    if (eligibleForMerge) {
        int space = maxStackFor(slotType[slotB]) - slotCount[slotB];
        if (space > 0) {
            int toMove = (slotCount[slotA] < space) ? slotCount[slotA] : space;
            slotCount[slotB] += toMove;
            slotCount[slotA] -= toMove;
            if (slotCount[slotA] <= 0) {
                slotEmpty[slotA] = true;
                slotCount[slotA] = 0;
            }
            return;
        }
    }

    BlockType tempType = slotType[slotA];
    int tempCount = slotCount[slotA];
    bool tempEmpty = slotEmpty[slotA];
    int tempDurability = slotDurability[slotA]; // nuevo

    slotType[slotA] = slotType[slotB];
    slotCount[slotA] = slotCount[slotB];
    slotEmpty[slotA] = slotEmpty[slotB];
    slotDurability[slotA] = slotDurability[slotB]; // nuevo

    slotType[slotB] = tempType;
    slotCount[slotB] = tempCount;
    slotEmpty[slotB] = tempEmpty;
    slotDurability[slotB] = tempDurability; // nuevo
}

void Inventory::removeFromSlot(int slotIndex, int amount) {
    if (slotIndex < 0 || slotIndex >= SLOT_COUNT) return;
    if (slotEmpty[slotIndex]) return;

    slotCount[slotIndex] -= amount;
    if (slotCount[slotIndex] <= 0) {
        slotEmpty[slotIndex] = true;
        slotCount[slotIndex] = 0;
    }
}

bool Inventory::addToolWithDurability(BlockType toolType, int durability) {
    for (int i = 0; i < TOTAL_SIZE; i++) {
        if (slotEmpty[i]) {
            slotType[i] = toolType;
            slotCount[i] = 1;
            slotEmpty[i] = false;
            slotDurability[i] = durability;
            return true;
        }
    }
    return false;
}

int Inventory::getTotalArmorValue() const {
    int reduce = 0, left = 0, maxDur = 0;
    for (int i = ARMOR_BASE; i < SLOT_COUNT; i++) {
        if (slotEmpty[i] || !isArmor(slotType[i])) continue;
        const int m = armorMaxDurability(slotType[i]);
        maxDur += m;
        left += slotDurability[i];      // lo que le queda (maximo - desgaste)
        reduce += armorReduceAmount(slotType[i]);
    }
    if (maxDur == 0) return 0;
    return (reduce - 1) * left / maxDur + 1;
}

void Inventory::damageArmor(int damage) {
    damage /= 4;
    if (damage < 1) damage = 1;
    for (int i = ARMOR_BASE; i < SLOT_COUNT; i++) {
        if (slotEmpty[i] || !isArmor(slotType[i])) continue;
        reduceDurability(i, damage);   // ItemStack.damageItem: al pasarse del maximo se rompe
    }
}
