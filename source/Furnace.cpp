#include <stdio.h>
#include "Furnace.h"
#include "ChunkManager.h"
#include "WorldConfig.h"

static FurnaceData s_furnaces[Furnace::MAX_FURNACES];

static void clearSlot(FurnaceSlot& s) {
    s.type = BlockType::AIR;
    s.count = 0;
    s.durability = 0;
    s.empty = true;
}

static void clearEntry(FurnaceData& d) {
    d.used = false;
    d.gx = d.gy = d.gz = 0;
    for (int i = 0; i < 3; i++) clearSlot(d.slot[i]);
    d.burnTime = 0;
    d.burnTimeMax = 0;
    d.cookTime = 0;
    d.dirty = false;
}

void Furnace::clearAll() {
    for (int i = 0; i < MAX_FURNACES; i++) clearEntry(s_furnaces[i]);
}

FurnaceData* Furnace::find(int gx, int gy, int gz) {
    for (int i = 0; i < MAX_FURNACES; i++) {
        FurnaceData& d = s_furnaces[i];
        if (d.used && d.gx == gx && d.gy == gy && d.gz == gz) return &d;
    }
    return nullptr;
}

FurnaceData* Furnace::findOrCreate(int gx, int gy, int gz) {
    FurnaceData* existing = find(gx, gy, gz);
    if (existing) return existing;

    for (int i = 0; i < MAX_FURNACES; i++) {
        FurnaceData& d = s_furnaces[i];
        if (!d.used) {
            clearEntry(d);
            d.used = true;
            d.gx = gx; d.gy = gy; d.gz = gz;
            return &d;
        }
    }
    return nullptr;
}

void Furnace::remove(int gx, int gy, int gz) {
    FurnaceData* d = find(gx, gy, gz);
    if (d) clearEntry(*d);
}

bool Furnace::transferWithInventory(Inventory& inv, int invSlot, FurnaceData& f, int furnaceSlot) {
    if (invSlot < 0 || invSlot >= Inventory::TOTAL_SIZE) return false;
    if (furnaceSlot < 0 || furnaceSlot > 2) return false;

    FurnaceSlot& fs = f.slot[furnaceSlot];
    bool invEmpty = inv.isSlotEmpty(invSlot);

    // Resultado: solo sale. Hacia un slot vacío del inventario, o sumándose
    // a una pila del mismo tipo con espacio. Nunca entra nada desde el inventario.
    if (furnaceSlot == SLOT_RESULT) {
        if (fs.empty) return false;

        if (invEmpty) {
            inv.setSlotDirect(invSlot, fs.type, fs.count);
            inv.setSlotDurability(invSlot, fs.durability);
            clearSlot(fs);
            return true;
        }

        BlockType invType = inv.getSlotType(invSlot);
        if (invType == fs.type && !isTool(invType)) {
            int invCount = inv.getSlotCount(invSlot);
            int space = Inventory::MAX_STACK - invCount;
            if (space > 0) {
                int toMove = (fs.count < space) ? fs.count : space;
                inv.setSlotDirect(invSlot, invType, invCount + toMove);
                fs.count -= toMove;
                if (fs.count <= 0) clearSlot(fs);
                return true;
            }
        }

        return false; // otro tipo de ítem, o pila llena: no pasa nada
    }

    if (fs.empty && invEmpty) return false;

    // Slot del horno vacío: pasa la pila completa del inventario al horno
    if (fs.empty) {
        fs.type = inv.getSlotType(invSlot);
        fs.count = inv.getSlotCount(invSlot);
        fs.durability = inv.getSlotDurability(invSlot);
        fs.empty = false;
        inv.setSlotDirect(invSlot, BlockType::AIR, 0);
        return true;
    }

    // Slot del inventario vacío: la pila del horno vuelve al inventario
    if (invEmpty) {
        inv.setSlotDirect(invSlot, fs.type, fs.count);
        inv.setSlotDurability(invSlot, fs.durability);
        clearSlot(fs);
        return true;
    }

    // Los dos tienen algo
    BlockType invType = inv.getSlotType(invSlot);
    int invCount = inv.getSlotCount(invSlot);
    int invDurability = inv.getSlotDurability(invSlot);

    // Mismo tipo (las herramientas nunca se apilan): fusiona hacia el horno
    if (invType == fs.type && !isTool(invType)) {
        int space = Inventory::MAX_STACK - fs.count;
        if (space > 0) {
            int toMove = (invCount < space) ? invCount : space;
            fs.count += toMove;
            inv.removeFromSlot(invSlot, toMove);
            return true;
        }
    }

    // Intercambio
    FurnaceSlot old = fs;
    fs.type = invType;
    fs.count = invCount;
    fs.durability = invDurability;
    fs.empty = false;
    inv.setSlotDirect(invSlot, old.type, old.count);
    inv.setSlotDurability(invSlot, old.durability);
    return true;
}

// ---------------------------------------------------------------------
// Cocción (portada de TileEntityFurnace / FurnaceRecipes de Beta 1.7.3)
// ---------------------------------------------------------------------

int Furnace::fuelBurnTime(BlockType t) {
    switch (t) {
        // bloques de material madera
        case BlockType::OAK_LOG:
        case BlockType::BIRCH_LOG:
        case BlockType::SPRUCE_LOG:
        case BlockType::WOOD_PLANKS:
        case BlockType::CRAFTING_TABLE:
        case BlockType::CHEST:
            return 300;
        case BlockType::STICK:
            return 100;
        case BlockType::COAL:
        case BlockType::CHARCOAL:
            return 1600;
        default:
            return 0;
    }
}

bool Furnace::smeltResult(BlockType in, BlockType& out, int& outCount) {
    outCount = 1;
    switch (in) {
        case BlockType::IRON_ORE:    out = BlockType::IRON_INGOT; return true;
        case BlockType::GOLD_ORE:    out = BlockType::GOLD_INGOT; return true;
        case BlockType::CLAY_BALL:   out = BlockType::BRICK;      return true;
        case BlockType::PORK_RAW:    out = BlockType::PORK_COOKED; return true;
        case BlockType::FISH_RAW:    out = BlockType::FISH_COOKED; return true;
        case BlockType::COBBLESTONE: out = BlockType::STONE;      return true;
        case BlockType::CACTUS:      out = BlockType::DYE_GREEN;  return true;
        case BlockType::OAK_LOG:
        case BlockType::BIRCH_LOG:
        case BlockType::SPRUCE_LOG:  out = BlockType::CHARCOAL;   return true;
	case BlockType::SAND:        out = BlockType::GLASS;      return true;
        default: return false;
    }
}

static bool canSmelt(const FurnaceData& f) {
    const FurnaceSlot& in = f.slot[Furnace::SLOT_INGREDIENT];
    if (in.empty || in.count <= 0) return false;

    BlockType outType;
    int outCount;
    if (!Furnace::smeltResult(in.type, outType, outCount)) return false;

    const FurnaceSlot& out = f.slot[Furnace::SLOT_RESULT];
    if (out.empty) return true;
    if (out.type != outType) return false;
    return out.count + outCount <= Inventory::MAX_STACK;
}

static void smeltItem(FurnaceData& f) {
    FurnaceSlot& in = f.slot[Furnace::SLOT_INGREDIENT];
    FurnaceSlot& out = f.slot[Furnace::SLOT_RESULT];

    BlockType outType;
    int outCount;
    if (!Furnace::smeltResult(in.type, outType, outCount)) return;

    if (out.empty) {
        out.type = outType;
        out.count = outCount;
        out.durability = 0;
        out.empty = false;
    } else if (out.type == outType) {
        out.count += outCount;
    }

    in.count--;
    if (in.count <= 0) clearSlot(in);
}

// Devuelve true si algo visible cambió en este tick
static bool tickOne(FurnaceData& f) {
    bool changed = false;
    bool wasBurning = f.burnTime > 0;

    if (f.burnTime > 0) f.burnTime--;

    if (f.burnTime == 0 && canSmelt(f)) {
        FurnaceSlot& fuel = f.slot[Furnace::SLOT_FUEL];
        int t = (!fuel.empty && fuel.count > 0) ? Furnace::fuelBurnTime(fuel.type) : 0;
        f.burnTime = f.burnTimeMax = t;
        if (t > 0) {
            changed = true;
            fuel.count--;
            if (fuel.count <= 0) clearSlot(fuel);
        }
    }

    if (f.burnTime > 0 && canSmelt(f)) {
        f.cookTime++;
        if (f.cookTime == Furnace::COOK_TIME) {
            f.cookTime = 0;
            smeltItem(f);
            changed = true;
        }
    } else {
        f.cookTime = 0;
    }

    if (wasBurning != (f.burnTime > 0)) changed = true;
    return changed;
}

static int floorDivInt(int a, int b) {
    int d = a / b;
    if ((a % b != 0) && ((a < 0) != (b < 0))) d--;
    return d;
}

void Furnace::tickAll(const ChunkManager& cm) {
    for (int i = 0; i < MAX_FURNACES; i++) {
        FurnaceData& f = s_furnaces[i];
        if (!f.used) continue;

        int cx = floorDivInt(f.gx, WorldConfig::SIZE_X);
        int cz = floorDivInt(f.gz, WorldConfig::SIZE_Z);
        Chunk* chunk = cm.getChunkByChunkCoord(cx, cz);

        // Chunk no cargado o todavía generándose: el horno se pausa, como en el juego real
        if (!chunk || !chunk->isGenerationComplete()) { f.dirty = false; continue; }

        int xi = f.gx - cx * WorldConfig::SIZE_X;
        int zi = f.gz - cz * WorldConfig::SIZE_Z;
        BlockType cur = chunk->getBlockType(xi, f.gy, zi);
        if (!isFurnaceBlock(cur)) { f.dirty = false; continue; }

        bool changed = tickOne(f);

        // El bloque tiene que reflejar si está quemando o no (conserva la dirección)
        bool lit = f.burnTime > 0;
        if (furnaceIsLit(cur) != lit) {
            chunk->setBlockDirect(xi, f.gy, zi, makeFurnaceBlock(furnaceFrontDir(cur), lit), &cm);
            changed = true;
        }

        f.dirty = changed;
    }
}

// ---------------------------------------------------------------------
// Guardado: furnaces.dat dentro de la carpeta del mundo
// ---------------------------------------------------------------------

static void putI32(FILE* f, int v) {
    fputc(v & 0xFF, f);
    fputc((v >> 8) & 0xFF, f);
    fputc((v >> 16) & 0xFF, f);
    fputc((v >> 24) & 0xFF, f);
}

static bool getI32(FILE* f, int& out) {
    int b0 = fgetc(f), b1 = fgetc(f), b2 = fgetc(f), b3 = fgetc(f);
    if (b0 == EOF || b1 == EOF || b2 == EOF || b3 == EOF) return false;
    out = (int)((unsigned)b0 | ((unsigned)b1 << 8) | ((unsigned)b2 << 16) | ((unsigned)b3 << 24));
    return true;
}

bool Furnace::saveToFile(const char* path) {
    FILE* f = fopen(path, "wb");
    if (!f) return false;

    int count = 0;
    for (int i = 0; i < MAX_FURNACES; i++) {
        if (s_furnaces[i].used) count++;
    }

    fputc('F', f); fputc('U', f); fputc('R', f); fputc('N', f);
    fputc(1, f); // versión del formato
    putI32(f, count);

    for (int i = 0; i < MAX_FURNACES; i++) {
        const FurnaceData& d = s_furnaces[i];
        if (!d.used) continue;

        putI32(f, d.gx); putI32(f, d.gy); putI32(f, d.gz);
        putI32(f, d.burnTime); putI32(f, d.burnTimeMax); putI32(f, d.cookTime);

        for (int s = 0; s < 3; s++) {
            const FurnaceSlot& sl = d.slot[s];
            fputc(sl.empty ? 1 : 0, f);
            fputc((int)sl.type, f);
            fputc(sl.count & 0xFF, f);
            putI32(f, sl.durability);
        }
    }

    bool ok = !ferror(f);
    fclose(f);
    return ok;
}

bool Furnace::loadFromFile(const char* path) {
    clearAll();

    FILE* f = fopen(path, "rb");
    if (!f) return false;

    bool ok = (fgetc(f) == 'F' && fgetc(f) == 'U' && fgetc(f) == 'R' && fgetc(f) == 'N' && fgetc(f) == 1);
    int count = 0;
    if (ok) ok = getI32(f, count) && count >= 0 && count <= MAX_FURNACES;

    for (int i = 0; ok && i < count; i++) {
        FurnaceData& d = s_furnaces[i];
        clearEntry(d);
        d.used = true;

        ok = getI32(f, d.gx) && getI32(f, d.gy) && getI32(f, d.gz) &&
             getI32(f, d.burnTime) && getI32(f, d.burnTimeMax) && getI32(f, d.cookTime);

        for (int s = 0; ok && s < 3; s++) {
            int emptyFlag = fgetc(f);
            int type = fgetc(f);
            int cnt = fgetc(f);
            int dur = 0;
            if (emptyFlag == EOF || type == EOF || cnt == EOF || !getI32(f, dur)) { ok = false; break; }

            FurnaceSlot& sl = d.slot[s];
            if (emptyFlag == 0 && cnt > 0) {
                sl.type = (BlockType)type;
                sl.count = cnt;
                sl.durability = dur;
                sl.empty = false;
            } else {
                clearSlot(sl);
            }
        }

        if (!ok) clearEntry(d); // entrada incompleta: se descarta
    }

    fclose(f);
    return ok;
}