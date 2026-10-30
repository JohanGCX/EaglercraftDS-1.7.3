#include <stdio.h>
#include "ChestStore.h"

static ChestData s_chests[ChestStore::MAX_CHESTS];

static void clearSlot(ChestSlot& s) { s.type = 0; s.count = 0; s.durability = 0; }

static void clearEntry(ChestData& d) {
    d.used = false;
    d.gx = d.gy = d.gz = 0;
    for (int i = 0; i < ChestStore::SLOTS; i++) clearSlot(d.slot[i]);
}

void ChestStore::clearAll() {
    for (int i = 0; i < MAX_CHESTS; i++) clearEntry(s_chests[i]);
}

ChestData* ChestStore::find(int gx, int gy, int gz) {
    for (int i = 0; i < MAX_CHESTS; i++) {
        ChestData& d = s_chests[i];
        if (d.used && d.gx == gx && d.gy == gy && d.gz == gz) return &d;
    }
    return nullptr;
}

ChestData* ChestStore::findOrCreate(int gx, int gy, int gz) {
    ChestData* existing = find(gx, gy, gz);
    if (existing) return existing;
    for (int i = 0; i < MAX_CHESTS; i++) {
        ChestData& d = s_chests[i];
        if (!d.used) {
            clearEntry(d);
            d.used = true;
            d.gx = gx; d.gy = gy; d.gz = gz;
            return &d;
        }
    }
    return nullptr;
}

int ChestStore::usedCount() {
    int n = 0;
    for (int i = 0; i < MAX_CHESTS; i++) if (s_chests[i].used) n++;
    return n;
}

void ChestStore::remove(int gx, int gy, int gz) {
    ChestData* d = find(gx, gy, gz);
    if (d) clearEntry(*d);
}

bool ChestStore::transferWithInventory(Inventory& inv, int invSlot, ChestSlot& cs) {
    if (invSlot < 0 || invSlot >= Inventory::TOTAL_SIZE) return false;

    const bool csEmpty = cs.count == 0;
    const bool invEmpty = inv.isSlotEmpty(invSlot);
    if (csEmpty && invEmpty) return false;

    // Casilla del cofre vacía: pasa la pila completa del inventario
    if (csEmpty) {
        cs.type = (u8)inv.getSlotType(invSlot);
        cs.count = (u8)inv.getSlotCount(invSlot);
        cs.durability = (u16)inv.getSlotDurability(invSlot);
        inv.setSlotDirect(invSlot, BlockType::AIR, 0);
        return true;
    }

    // Casilla del inventario vacía: la pila del cofre vuelve al inventario
    if (invEmpty) {
        inv.setSlotDirect(invSlot, (BlockType)cs.type, cs.count);
        inv.setSlotDurability(invSlot, cs.durability);
        clearSlot(cs);
        return true;
    }

    const BlockType invType = inv.getSlotType(invSlot);
    const int invCount = inv.getSlotCount(invSlot);
    const int invDurability = inv.getSlotDurability(invSlot);

    // Mismo ítem que se apila (las herramientas y los baldes no): se funde hacia el cofre
    if (invType == (BlockType)cs.type && maxStackFor(invType) > 1) {
        int space = maxStackFor(invType) - cs.count;
        if (space > 0) {
            int toMove = (invCount < space) ? invCount : space;
            cs.count = (u8)(cs.count + toMove);
            inv.removeFromSlot(invSlot, toMove);
            return true;
        }
    }

    // Intercambio
    ChestSlot old = cs;
    cs.type = (u8)invType;
    cs.count = (u8)invCount;
    cs.durability = (u16)invDurability;
    inv.setSlotDirect(invSlot, (BlockType)old.type, old.count);
    inv.setSlotDurability(invSlot, old.durability);
    return true;
}

// ---------------------------------------------------------------------
// Guardado: chests.dat dentro de la carpeta de la dimensión
// ---------------------------------------------------------------------

static void putI32(FILE* f, int v) {
    fputc(v & 0xFF, f); fputc((v >> 8) & 0xFF, f); fputc((v >> 16) & 0xFF, f); fputc((v >> 24) & 0xFF, f);
}

static bool getI32(FILE* f, int& out) {
    int b0 = fgetc(f), b1 = fgetc(f), b2 = fgetc(f), b3 = fgetc(f);
    if (b0 == EOF || b1 == EOF || b2 == EOF || b3 == EOF) return false;
    out = (int)((unsigned)b0 | ((unsigned)b1 << 8) | ((unsigned)b2 << 16) | ((unsigned)b3 << 24));
    return true;
}

bool ChestStore::saveToFile(const char* path) {
    FILE* f = fopen(path, "wb");
    if (!f) return false;

    int count = 0;
    for (int i = 0; i < MAX_CHESTS; i++) if (s_chests[i].used) count++;

    fputc('C', f); fputc('H', f); fputc('S', f); fputc('T', f);
    fputc(1, f); // versión del formato
    putI32(f, count);

    for (int i = 0; i < MAX_CHESTS; i++) {
        const ChestData& d = s_chests[i];
        if (!d.used) continue;
        putI32(f, d.gx); putI32(f, d.gy); putI32(f, d.gz);
        for (int s = 0; s < SLOTS; s++) {
            fputc(d.slot[s].type, f);
            fputc(d.slot[s].count, f);
            fputc(d.slot[s].durability & 0xFF, f);
            fputc((d.slot[s].durability >> 8) & 0xFF, f);
        }
    }

    bool ok = !ferror(f);
    fclose(f);
    return ok;
}

bool ChestStore::loadFromFile(const char* path) {
    clearAll();

    FILE* f = fopen(path, "rb");
    if (!f) return false;

    bool ok = (fgetc(f) == 'C' && fgetc(f) == 'H' && fgetc(f) == 'S' && fgetc(f) == 'T' && fgetc(f) == 1);
    int count = 0;
    if (ok) ok = getI32(f, count) && count >= 0 && count <= MAX_CHESTS;

    for (int i = 0; ok && i < count; i++) {
        ChestData& d = s_chests[i];
        clearEntry(d);
        d.used = true;
        ok = getI32(f, d.gx) && getI32(f, d.gy) && getI32(f, d.gz);
        for (int s = 0; ok && s < SLOTS; s++) {
            int t = fgetc(f), c = fgetc(f), d0 = fgetc(f), d1 = fgetc(f);
            if (t == EOF || c == EOF || d0 == EOF || d1 == EOF) { ok = false; break; }
            d.slot[s].type = (u8)t;
            d.slot[s].count = (u8)c;
            d.slot[s].durability = (u16)(d0 | (d1 << 8));
        }
        if (!ok) clearEntry(d);
    }

    fclose(f);
    return ok;
}