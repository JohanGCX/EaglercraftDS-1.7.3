#ifndef DEBUGCHESTS_H
#define DEBUGCHESTS_H

#include "BlockType.h"
#include "ItemOrder.h"
#include <math.h>

// =====================================================================
// Cofres de depuración (opción "debug large chest"): al crear un mundo nuevo aparecen dos cofres grandes con todos los bloques y objetos
// del juego, cerca del jugador y en su mismo chunk. Aquí solo hay lógica pura (sin libnds): qué ítems llevan y dónde se pueden poner.
// =====================================================================
namespace DebugChests {
    const int MAX_ITEMS = 216;            // cuatro cofres grandes de 54 casillas
    const BlockType LAST_TYPE = BlockType::SAPLING_BIRCH;   // último valor del enum: si agregás tipos nuevos al final, actualizá esto

    // Lo que NO se puede tener en la mano: aire, líquidos, fuego, portal, bedrock y las variantes técnicas (hornos orientados o
    // encendidos, antorchas según el bloque de apoyo): de cada una queda solo el ítem normal (FURNACE, TORCH)
    inline bool isGivenItem(BlockType t) {
        switch (t) {
            case BlockType::AIR:
            case BlockType::WATER: case BlockType::WATER_FLOW: case BlockType::LAVA: case BlockType::LAVA_FLOW:
            case BlockType::FIRE: case BlockType::PORTAL: case BlockType::BEDROCK:
            case BlockType::MOB_SPAWNER: case BlockType::SPARE_ITEM:  // no se obtienen en el juego
            case BlockType::SIGN_POST: case BlockType::SIGN_WALL: case BlockType::CAKE_BLOCK: case BlockType::FARMLAND: case BlockType::CROPS:   // bloques del mundo: se tienen como objeto
            case BlockType::REDSTONE_WIRE: case BlockType::TORCH_REDSTONE_OFF: case BlockType::REPEATER_OFF: case BlockType::REPEATER_ON:   // bloques del mundo: se tienen como REDSTONE, TORCH_REDSTONE_ON y REPEATER_ITEM
            case BlockType::PISTON_HEAD: case BlockType::PISTON_MOVING:   // partes del piston: solo existen en el mundo
            case BlockType::DOUBLE_SLAB_STONE: case BlockType::DOUBLE_SLAB_SAND: case BlockType::DOUBLE_SLAB_WOOD: case BlockType::DOUBLE_SLAB_COBBLE:   // se obtienen juntando dos losas
            case BlockType::REDSTONE_ORE_LIT:   // la mena encendida se obtiene apagada
            case BlockType::DOOR_WOOD: case BlockType::DOOR_IRON: case BlockType::BED_BLOCK:   // son bloques del mundo: se tienen como objeto
            case BlockType::SUGAR_CANE:   // el bloque no se tiene en la mano: se pone con el objeto SUGAR_CANE_ITEM
            case BlockType::DISPENSER_W: case BlockType::DISPENSER_N: case BlockType::DISPENSER_E:   // el dispensador se pone orientado hacia el jugador
            case BlockType::FURNACE_W: case BlockType::FURNACE_N: case BlockType::FURNACE_E:
            case BlockType::FURNACE_LIT_S: case BlockType::FURNACE_LIT_W: case BlockType::FURNACE_LIT_N: case BlockType::FURNACE_LIT_E:
            case BlockType::TORCH_W: case BlockType::TORCH_E: case BlockType::TORCH_N: case BlockType::TORCH_S: case BlockType::TORCH_F:
                return false;
            default:
                return true;
        }
    }

    // Llena 'out' (MAX_ITEMS como mínimo) con los ítems, en el orden del enum; devuelve cuántos son (nunca más de MAX_ITEMS)
    inline int buildItemList(BlockType* out) {
        int n = 0;
        for (int v = 1; v <= (int)LAST_TYPE && n < MAX_ITEMS; v++) {
            const BlockType t = (BlockType)v;
            if (isGivenItem(t)) out[n++] = t;
        }
        // en el orden del original (ids de bloque y objeto de la Beta 1.7.3), no en el del enum
        for (int a = 1; a < n; a++) {
            const BlockType v = out[a];
            const int kv = ItemOrder::key(v);
            int b = a - 1;
            while (b >= 0 && ItemOrder::key(out[b]) > kv) { out[b + 1] = out[b]; b--; }
            out[b + 1] = v;
        }
        return n;
    }

    // ---- dónde van ----
    struct Site {
        int gx, gy, gz;   // celda global del primer cofre (el de menor x o z); gy = altura de los dos cofres
        bool alongX;      // el segundo cofre está en gx + 1 (si no, en gz + 1)
        int groundA, groundB;   // altura del suelo bajo cada mitad; si difieren en 1, la mitad baja se rellena con tierra
    };

    // Lo que se atraviesa al buscar el suelo desde arriba (plantas, nieve, copas y troncos) y lo que un cofre puede reemplazar
    inline bool isSkippable(BlockType t) {
        return t == BlockType::AIR || t == BlockType::TALLGRASS || t == BlockType::SNOW_LAYER || t == BlockType::DEAD_BUSH ||
               isCrossPlant(t) || t == BlockType::OAK_LEAVES || t == BlockType::BIRCH_LEAVES ||
               t == BlockType::SPRUCE_LEAVES || t == BlockType::OAK_LOG || t == BlockType::BIRCH_LOG || t == BlockType::SPRUCE_LOG ||
               isTorchBlock(t) || t == BlockType::FIRE;
    }
    inline bool isReplaceable(BlockType t) {
        return t == BlockType::AIR || t == BlockType::TALLGRASS || t == BlockType::SNOW_LAYER || t == BlockType::DEAD_BUSH ||
               isCrossPlant(t);
    }

    // Altura del suelo de la columna, o -1 si no sirve (agua, lava, cactus, sin suelo)
    template <class TypeAt>
    inline int groundHeight(TypeAt typeAt, int gx, int gz) {
        for (int y = 127; y >= 1; y--) {
            const BlockType t = typeAt(gx, y, gz);
            if (isSkippable(t)) continue;
            if (isLiquidBlock(t) || t == BlockType::CACTUS || !isNormalCube(t)) return -1;
            return y;
        }
        return -1;
    }

    // Busca el mejor lugar para un cofre grande dentro del chunk cuya esquina mínima es (x0, z0) y de tamaño sizeX x sizeZ: dos columnas
    // contiguas con suelo, a 'wanted' bloques del jugador (celda pgx, pgz), sin tocar al jugador ni quedar pegado a 'avoid'.
    // Se prefiere el suelo parejo; con 1 de diferencia se nivela la mitad baja con tierra. Devuelve false si no hay sitio (océano, pared).
    template <class TypeAt>
    inline bool findSite(TypeAt typeAt, int x0, int z0, int sizeX, int sizeZ, int pgx, int pgz, float wanted, const Site* avoid, Site& out, const Site* avoid2 = nullptr, const Site* avoid3 = nullptr) {
        float bestScore = 1e30f;
        bool found = false;
        for (int gx = x0; gx < x0 + sizeX; gx++) {
            for (int gz = z0; gz < z0 + sizeZ; gz++) {
                for (int dir = 0; dir < 2; dir++) {
                    const bool alongX = (dir == 0);
                    const int hx = gx + (alongX ? 1 : 0), hz = gz + (alongX ? 0 : 1);   // la segunda mitad
                    if (hx >= x0 + sizeX || hz >= z0 + sizeZ) continue;
                    if ((gx == pgx && gz == pgz) || (hx == pgx && hz == pgz)) continue;
                    if (avoid) {
                        const int ax2 = avoid->gx + (avoid->alongX ? 1 : 0), az2 = avoid->gz + (avoid->alongX ? 0 : 1);
                        auto near = [&](int cx, int cz) {
                            return (abs(cx - avoid->gx) <= 1 && abs(cz - avoid->gz) <= 1) || (abs(cx - ax2) <= 1 && abs(cz - az2) <= 1);
                        };
                        if (near(gx, gz) || near(hx, hz)) continue;
                    }
                    if (avoid2) {
                        const int bx2 = avoid2->gx + (avoid2->alongX ? 1 : 0), bz2 = avoid2->gz + (avoid2->alongX ? 0 : 1);
                        auto near2 = [&](int cx, int cz) {
                            return (abs(cx - avoid2->gx) <= 1 && abs(cz - avoid2->gz) <= 1) || (abs(cx - bx2) <= 1 && abs(cz - bz2) <= 1);
                        };
                        if (near2(gx, gz) || near2(hx, hz)) continue;
                    }
                    if (avoid3) {
                        const int cx2 = avoid3->gx + (avoid3->alongX ? 1 : 0), cz2 = avoid3->gz + (avoid3->alongX ? 0 : 1);
                        auto near3 = [&](int cx, int cz) {
                            return (abs(cx - avoid3->gx) <= 1 && abs(cz - avoid3->gz) <= 1) || (abs(cx - cx2) <= 1 && abs(cz - cz2) <= 1);
                        };
                        if (near3(gx, gz) || near3(hx, hz)) continue;
                    }
                    const int gA = groundHeight(typeAt, gx, gz), gB = groundHeight(typeAt, hx, hz);
                    if (gA < 0 || gB < 0) continue;
                    const int diff = abs(gA - gB);
                    if (diff > 1) continue;
                    const int y = (gA > gB ? gA : gB) + 1;
                    bool clear = true;
                    for (int yy = gA + 1; yy <= y; yy++) if (!isReplaceable(typeAt(gx, yy, gz))) clear = false;
                    for (int yy = gB + 1; yy <= y; yy++) if (!isReplaceable(typeAt(hx, yy, hz))) clear = false;
                    if (!clear) continue;

                    const float cx = (float)gx + 0.5f + (alongX ? 0.5f : 0.0f), cz = (float)gz + 0.5f + (alongX ? 0.0f : 0.5f);
                    const float dx = cx - ((float)pgx + 0.5f), dz = cz - ((float)pgz + 0.5f);
                    const float dist = sqrtf(dx * dx + dz * dz);
                    const float score = (dist - wanted) * (dist - wanted) + (float)diff * 4.0f;
                    if (score < bestScore) {
                        bestScore = score;
                        out.gx = gx; out.gy = y; out.gz = gz; out.alongX = alongX; out.groundA = gA; out.groundB = gB;
                        found = true;
                    }
                }
            }
        }
        return found;
    }
}

#endif