#ifndef LIQUID_H
#define LIQUID_H

#include "BlockType.h"
#include "TntManager.h"
#include "Weather.h"
#include <math.h>

// =====================================================================
// Líquidos de Minecraft Beta 1.7.3: BlockFluid + BlockFlowing + BlockStationary.
// Agua y lava comparten TODO el código; solo cambian la velocidad (tick de 5 contra 30, y la lava avanza de a
// 2 niveles en el mundo normal), la regla de fuente infinita (solo el agua) y la mezcla agua + lava.
//
// Nivel de cada celda (metadata): 0 = fuente, 1..7 = flujo (más lejos de la fuente), +8 = cayendo desde arriba.
// WATER y LAVA son los bloques "quietos" (ids 9 y 11 del original); WATER_FLOW y LAVA_FLOW los "corrientes" (8 y 10).
//
// La lógica es pura: trabaja contra un mundo W que se prueba en el host contra el Java real y que en el juego
// es el ChunkManager (celdas globales: gx = chunkX*8 + xi, gy = yi, gz = chunkZ*8 + zi).
//
// W debe ofrecer:
//   BlockType getType(int x, int y, int z);                       // celda sin cargar = bloque sólido
//   int       getMeta(int x, int y, int z);
//   bool      inRange(int x, int y, int z);                       // celda editable (cargada, 0 <= y < 128)
//   void      putCell(int x, int y, int z, BlockType t, int meta);// escribe sin eventos
//   void      pushTick(int x, int y, int z, BlockType t, int delay);
//   int       randInt(int n);
//   void      dropItem(int x, int y, int z, BlockType old);       // lo que suelta un bloque desplazado por el agua
//   void      otherNeighbor(int x, int y, int z);                 // aviso a bloques que no son líquidos (antorchas)
//   void      mixEffect();                                        // sonido y humo al mezclarse lava y agua
// =====================================================================

namespace Liquid {

// ---- ticks programados (World.scheduleBlockUpdate / World.TickUpdates), en Liquid.cpp ----
void resetTicks();
void advanceTime();                    // una vez por tick del juego (20 Hz)
bool pushTick(int gx, int gy, int gz, BlockType t, int delay);  // sin duplicados; false si ya estaba o no hay lugar
bool popDue(int& gx, int& gy, int& gz, BlockType& t);           // la próxima vencida: orden (vencimiento, llegada)
int  pendingCount();

// WorldProviderHell.isHellWorld: en el Nether la lava avanza de a 1 nivel (como el agua) y el agua del balde se evapora
void setHell(bool hell);
bool isHell();

inline int tickRateOf(BlockType t) { return t == BlockType::FIRE ? 40 : ((t == BlockType::SAND || t == BlockType::GRAVEL) ? 3 : (isWaterBlock(t) ? 5 : 30)); }

// BlockFluid.getPercentAir
inline float percentAir(int meta) {
    if (meta >= 8) meta = 0;
    return (float)(meta + 1) / 9.0f;
}

template <class W>
struct Sim {
    W& w;
    bool editing = false;               // World.editingBlocks: mientras vale, no se avisa a los vecinos
    int numAdjacentSources = 0;
    bool optimal[4];
    int flowCost[4];

    explicit Sim(W& world) : w(world) {}

    // ------------------------------------------------------------------ materiales
    int matAt(int x, int y, int z) { return liquidMaterial(w.getType(x, y, z)); }

    // BlockFluid.getFlowDecay: -1 si no es del mismo material, si no su metadata
    int getFlowDecay(BlockType self, int x, int y, int z) {
        return matAt(x, y, z) != liquidMaterial(self) ? -1 : w.getMeta(x, y, z);
    }
    int getEffectiveFlowDecay(int mat, int x, int y, int z) {
        if (matAt(x, y, z) != mat) return -1;
        int m = w.getMeta(x, y, z);
        return m >= 8 ? 0 : m;
    }

    // BlockFlowing.blockBlocksFlow (puertas, carteles, escaleras y caña aún no existen)
    bool blockBlocksFlow(int x, int y, int z) {
        BlockType t = w.getType(x, y, z);
        if (t == BlockType::AIR) return false;
        return isMaterialSolid(t);
    }

    // BlockFlowing.liquidCanDisplaceBlock: el agua no desplaza a la lava, y nada desplaza a su propio material
    bool liquidCanDisplace(BlockType self, int x, int y, int z) {
        int m = matAt(x, y, z);
        if (m == liquidMaterial(self)) return false;
        if (m == 2) return false;
        return !blockBlocksFlow(x, y, z);
    }

    // ------------------------------------------------------------------ cambios de bloque (Chunk + World)
    void scheduleBlockUpdate(int x, int y, int z, BlockType t, int delay) { w.pushTick(x, y, z, t, delay); }

    // Chunk.setBlockIDWithMetadata: no hace nada si no cambia ni el bloque ni la metadata; si no, guarda y
    // llama a onBlockAdded del bloque nuevo
    bool setBlockAndMetadata(int x, int y, int z, BlockType t, int meta) {
        if (!w.inRange(x, y, z)) return false;
        if (w.getType(x, y, z) == t && w.getMeta(x, y, z) == meta) return false;
        w.putCell(x, y, z, t, meta);
        if (t != BlockType::AIR) onBlockAdded(t, x, y, z);
        return true;
    }
    // Chunk.setBlockID: si el id no cambia no hace nada; si no, la metadata queda en 0
    bool setBlock(int x, int y, int z, BlockType t) {
        if (!w.inRange(x, y, z)) return false;
        if (w.getType(x, y, z) == t) return false;
        w.putCell(x, y, z, t, 0);
        if (t != BlockType::AIR) onBlockAdded(t, x, y, z);
        return true;
    }
    bool setBlockWithNotify(int x, int y, int z, BlockType t) {
        if (!setBlock(x, y, z, t)) return false;
        notifyNeighbors(x, y, z);
        return true;
    }
    bool setBlockAndMetadataWithNotify(int x, int y, int z, BlockType t, int meta) {
        if (!setBlockAndMetadata(x, y, z, t, meta)) return false;
        notifyNeighbors(x, y, z);
        return true;
    }
    // World.setBlockMetadataWithNotify: siempre avisa a los vecinos (aunque la metadata no cambie)
    void setMetaWithNotify(int x, int y, int z, int meta) {
        if (!w.inRange(x, y, z)) return;
        w.putCell(x, y, z, w.getType(x, y, z), meta);
        notifyNeighbors(x, y, z);
    }

    // World.notifyBlocksOfNeighborChange: -X, +X, -Y, +Y, -Z, +Z
    void notifyNeighbors(int x, int y, int z) {
        if (editing) return;
        notifyOne(x - 1, y, z); notifyOne(x + 1, y, z);
        notifyOne(x, y - 1, z); notifyOne(x, y + 1, z);
        notifyOne(x, y, z - 1); notifyOne(x, y, z + 1);
    }
    void notifyOne(int x, int y, int z) {
        if (!w.inRange(x, y, z)) return;
        BlockType t = w.getType(x, y, z);
        if (t == BlockType::FIRE) {
            fireNeighborChange(x, y, z);                                  // BlockFire.onNeighborBlockChange
        } else if (t == BlockType::PORTAL) {
            portalNeighborChange(x, y, z);                                // BlockPortal.onNeighborBlockChange
        } else if (t == BlockType::CACTUS) {
            cactusNeighborChange(x, y, z);                                // BlockCactus.onNeighborBlockChange
        } else if (isFlowingLiquid(t)) {
            checkForHarden(t, x, y, z);                                   // BlockFluid.onNeighborBlockChange
        } else if (isLiquidBlock(t)) {                                    // BlockStationary.onNeighborBlockChange
            checkForHarden(t, x, y, z);
            if (w.getType(x, y, z) == t) stationaryToFlowing(t, x, y, z);
        } else if (t == BlockType::SAND || t == BlockType::GRAVEL) {
            scheduleBlockUpdate(x, y, z, t, tickRateOf(t));               // BlockSand.onNeighborBlockChange
        } else {
            w.otherNeighbor(x, y, z);
        }
    }

    // BlockSand.canFallBelow: aire, fuego o liquido
    bool sandCanFallBelow(int x, int y, int z) {
        BlockType b = w.getType(x, y, z);
        return b == BlockType::AIR || b == BlockType::FIRE || isLiquidBlock(b);
    }

    // BlockSand.updateTick: sin apoyo se quita el bloque y aparece la entidad que cae; sin lugar para la entidad, reintenta
    void sandTick(BlockType t, int x, int y, int z) {
        if (!sandCanFallBelow(x, y - 1, z)) return;
        if (!w.spawnFalling(x, y, z, t)) { scheduleBlockUpdate(x, y, z, t, tickRateOf(t)); return; }
        setBlockWithNotify(x, y, z, BlockType::AIR);
    }

    // BlockCactus.canBlockStay: ningún costado sólido (otro cactus también cuenta) y arena o cactus debajo
    bool cactusCanStay(int x, int y, int z) {
        if (isMaterialSolid(w.getType(x - 1, y, z))) return false;
        if (isMaterialSolid(w.getType(x + 1, y, z))) return false;
        if (isMaterialSolid(w.getType(x, y, z - 1))) return false;
        if (isMaterialSolid(w.getType(x, y, z + 1))) return false;
        BlockType below = w.getType(x, y - 1, z);
        return below == BlockType::CACTUS || below == BlockType::SAND;
    }

    // BlockCactus.onNeighborBlockChange: si ya no puede quedarse (le pusieron un bloque al lado o le quitaron la base), suelta el ítem y desaparece
    void cactusNeighborChange(int x, int y, int z) {
        if (!cactusCanStay(x, y, z)) {
            w.dropItem(x, y, z, BlockType::CACTUS);
            setBlockWithNotify(x, y, z, BlockType::AIR);
        }
    }

    // BlockFluid.onBlockAdded / BlockFlowing.onBlockAdded
    void onBlockAdded(BlockType t, int x, int y, int z) {
        if (t == BlockType::FIRE) { fireOnBlockAdded(x, y, z); return; }
        if (t == BlockType::SAND || t == BlockType::GRAVEL) { scheduleBlockUpdate(x, y, z, t, tickRateOf(t)); return; }   // BlockSand.onBlockAdded
        if (!isLiquidBlock(t)) return;
        checkForHarden(t, x, y, z);
        if (isFlowingLiquid(t) && w.getType(x, y, z) == t) scheduleBlockUpdate(x, y, z, t, tickRateOf(t));
    }

    // BlockFluid.checkForHarden: la lava tocada por agua (de lado o desde arriba) se endurece:
    // fuente -> obsidiana, nivel 1 a 4 -> adoquín, nivel 5 a 7 (y cayendo) no cambia
    void checkForHarden(BlockType t, int x, int y, int z) {
        if (w.getType(x, y, z) != t) return;
        if (liquidMaterial(t) != 2) return;
        bool wet = matAt(x, y, z - 1) == 1 || matAt(x, y, z + 1) == 1 || matAt(x - 1, y, z) == 1 ||
                   matAt(x + 1, y, z) == 1 || matAt(x, y + 1, z) == 1;
        if (!wet) return;
        int m = w.getMeta(x, y, z);
        if (m == 0) setBlockWithNotify(x, y, z, BlockType::OBSIDIAN);
        else if (m <= 4) setBlockWithNotify(x, y, z, BlockType::COBBLESTONE);
        w.mixEffect();   // triggerLavaMixEffects: sonido y humo
    }

    // BlockStationary.func_30004_j: la masa quieta se vuelve corriente para volver a calcularse
    void stationaryToFlowing(BlockType t, int x, int y, int z) {
        int meta = w.getMeta(x, y, z);
        BlockType f = liquidFlowingOf(t);
        editing = true;
        setBlockAndMetadata(x, y, z, f, meta);
        scheduleBlockUpdate(x, y, z, f, tickRateOf(f));
        editing = false;
    }

    // BlockFlowing.func_30003_j: la corriente que ya no cambia pasa a quieta
    void flowingToStationary(BlockType self, int x, int y, int z) {
        int meta = w.getMeta(x, y, z);
        setBlockAndMetadata(x, y, z, liquidStillOf(self), meta);
    }

    // ------------------------------------------------------------------ BlockFlowing
    int getSmallestFlowDecay(BlockType self, int x, int y, int z, int cur) {
        int d = getFlowDecay(self, x, y, z);
        if (d < 0) return cur;
        if (d == 0) numAdjacentSources++;
        if (d >= 8) d = 0;
        return (cur >= 0 && d >= cur) ? cur : d;
    }

    void flowIntoBlock(BlockType self, int x, int y, int z, int level) {
        if (!liquidCanDisplace(self, x, y, z)) return;
        BlockType old = w.getType(x, y, z);
        if (old != BlockType::AIR) {
            if (isLavaBlock(self)) w.mixEffect();
            else w.dropItem(x, y, z, old);
        }
        setBlockAndMetadataWithNotify(x, y, z, self, level);
    }

    int calculateFlowCost(BlockType self, int x, int y, int z, int cost, int fromDir) {
        int best = 1000;
        for (int d = 0; d < 4; d++) {
            if ((d == 0 && fromDir == 1) || (d == 1 && fromDir == 0) || (d == 2 && fromDir == 3) || (d == 3 && fromDir == 2)) continue;
            int nx = x, nz = z;
            if (d == 0) nx = x - 1;
            if (d == 1) nx = x + 1;
            if (d == 2) nz = z - 1;
            if (d == 3) nz = z + 1;
            if (!blockBlocksFlow(nx, y, nz) && (matAt(nx, y, nz) != liquidMaterial(self) || w.getMeta(nx, y, nz) != 0)) {
                if (!blockBlocksFlow(nx, y - 1, nz)) return cost;
                if (cost < 4) {
                    int c = calculateFlowCost(self, nx, y, nz, cost + 1, d);
                    if (c < best) best = c;
                }
            }
        }
        return best;
    }

    void getOptimalFlowDirections(BlockType self, int x, int y, int z) {
        for (int d = 0; d < 4; d++) {
            flowCost[d] = 1000;
            int nx = x, nz = z;
            if (d == 0) nx = x - 1;
            if (d == 1) nx = x + 1;
            if (d == 2) nz = z - 1;
            if (d == 3) nz = z + 1;
            if (!blockBlocksFlow(nx, y, nz) && (matAt(nx, y, nz) != liquidMaterial(self) || w.getMeta(nx, y, nz) != 0)) {
                if (!blockBlocksFlow(nx, y - 1, nz)) flowCost[d] = 0;
                else flowCost[d] = calculateFlowCost(self, nx, y, nz, 1, d);
            }
        }
        int m = flowCost[0];
        for (int d = 1; d < 4; d++) if (flowCost[d] < m) m = flowCost[d];
        for (int d = 0; d < 4; d++) optimal[d] = (flowCost[d] == m);
    }

    // BlockFlowing.updateTick. 'self' es el tipo CORRIENTE (WATER_FLOW o LAVA_FLOW).
    void updateTick(BlockType self, int x, int y, int z) {
        const bool lava = isLavaBlock(self);
        const int rate = lava ? (isHell() ? 1 : 2) : 1;   // el Nether avanza de a 1; el mundo normal, la lava de a 2
        int decay = getFlowDecay(self, x, y, z);
        bool canSettle = true;

        if (decay > 0) {
            numAdjacentSources = 0;
            int s = getSmallestFlowDecay(self, x - 1, y, z, -100);
            s = getSmallestFlowDecay(self, x + 1, y, z, s);
            s = getSmallestFlowDecay(self, x, y, z - 1, s);
            s = getSmallestFlowDecay(self, x, y, z + 1, s);
            int nd = s + rate;
            if (nd >= 8 || s < 0) nd = -1;

            if (getFlowDecay(self, x, y + 1, z) >= 0) {
                int above = getFlowDecay(self, x, y + 1, z);
                nd = above >= 8 ? above : above + 8;
            }

            if (numAdjacentSources >= 2 && !lava) {
                BlockType below = w.getType(x, y - 1, z);
                if (isMaterialSolid(below)) nd = 0;
                else if (liquidMaterial(below) == liquidMaterial(self) && w.getMeta(x, y, z) == 0) nd = 0;
            }

            if (lava && decay < 8 && nd < 8 && nd > decay && w.randInt(4) != 0) {
                nd = decay;
                canSettle = false;
            }

            if (nd != decay) {
                decay = nd;
                if (nd < 0) {
                    setBlockWithNotify(x, y, z, BlockType::AIR);
                } else {
                    setMetaWithNotify(x, y, z, nd);
                    scheduleBlockUpdate(x, y, z, self, tickRateOf(self));
                    notifyNeighbors(x, y, z);
                }
            } else if (canSettle) {
                flowingToStationary(self, x, y, z);
            }
        } else {
            flowingToStationary(self, x, y, z);
        }

        if (liquidCanDisplace(self, x, y - 1, z)) {
            if (decay >= 8) setBlockAndMetadataWithNotify(x, y - 1, z, self, decay);
            else            setBlockAndMetadataWithNotify(x, y - 1, z, self, decay + 8);
        } else if (decay >= 0 && (decay == 0 || blockBlocksFlow(x, y - 1, z))) {
            getOptimalFlowDirections(self, x, y, z);
            int nl = decay + rate;
            if (decay >= 8) nl = 1;
            if (nl >= 8) return;
            if (optimal[0]) flowIntoBlock(self, x - 1, y, z, nl);
            if (optimal[1]) flowIntoBlock(self, x + 1, y, z, nl);
            if (optimal[2]) flowIntoBlock(self, x, y, z - 1, nl);
            if (optimal[3]) flowIntoBlock(self, x, y, z + 1, nl);
        }
    }

    // World.TickUpdates sobre una entrada vencida: solo actúa si el bloque sigue siendo el mismo
    void runScheduled(int x, int y, int z, BlockType t) {
        if (!w.inRange(x, y, z)) return;
        if (w.getType(x, y, z) != t || t == BlockType::AIR) return;
        if (t == BlockType::FIRE) { fireUpdateTick(x, y, z); return; }
        if (t == BlockType::SAND || t == BlockType::GRAVEL) { sandTick(t, x, y, z); return; }
        if (isFlowingLiquid(t)) updateTick(t, x, y, z);   // la lava quieta solo tiene tick aleatorio (lavaRandomTick)
    }

    // ------------------------------------------------------------------ Fuego (BlockFire) y lava que lo prende
    // Sin lluvia ni clima en este juego: las condiciones de lluvia del original son siempre falsas y se omiten.
    // El netherrack (que mantiene el fuego encendido) llega con el Nether.
    // BlockPortal.tryToCreatePortal, literal: el fuego se encendió sobre obsidiana; si a su alrededor hay un marco de obsidiana
    // de 4 de ancho por 5 de alto (sin las esquinas) y el interior es aire o fuego, el interior de 2 x 3 pasa a portal.
    // Devuelve si se creó.
    bool tryToCreatePortal(int x, int y, int z) {
        int dx = 0, dz = 0;
        if (w.getType(x - 1, y, z) == BlockType::OBSIDIAN || w.getType(x + 1, y, z) == BlockType::OBSIDIAN) dx = 1;
        if (w.getType(x, y, z - 1) == BlockType::OBSIDIAN || w.getType(x, y, z + 1) == BlockType::OBSIDIAN) dz = 1;
        if (dx == dz) return false;

        if (w.getType(x - dx, y, z - dz) == BlockType::AIR) { x -= dx; z -= dz; }

        for (int a = -1; a <= 2; a++) {
            for (int b = -1; b <= 3; b++) {
                bool frame = a == -1 || a == 2 || b == -1 || b == 3;
                if ((a != -1 && a != 2) || (b != -1 && b != 3)) {   // las esquinas no cuentan
                    BlockType t = w.getType(x + dx * a, y + b, z + dz * a);
                    if (frame) {
                        if (t != BlockType::OBSIDIAN) return false;
                    } else if (t != BlockType::AIR && t != BlockType::FIRE) {
                        return false;
                    }
                }
            }
        }

        editing = true;
        for (int a = 0; a < 2; a++)
            for (int b = 0; b < 3; b++)
                setBlockWithNotify(x + dx * a, y + b, z + dz * a, BlockType::PORTAL);
        editing = false;
        return true;
    }

    // BlockPortal.onNeighborBlockChange, literal: el portal se deshace si falta la obsidiana de abajo o de arriba, si la
    // columna no mide justo 3, o si no tiene portal o marco a los lados
    void portalNeighborChange(int x, int y, int z) {
        int ax = 0, az = 1;
        if (w.getType(x - 1, y, z) == BlockType::PORTAL || w.getType(x + 1, y, z) == BlockType::PORTAL) { ax = 1; az = 0; }

        int base = y;
        while (w.getType(x, base - 1, z) == BlockType::PORTAL) base--;

        if (w.getType(x, base - 1, z) != BlockType::OBSIDIAN) {
            setBlockWithNotify(x, y, z, BlockType::AIR);
        } else {
            int n = 1;
            while (n < 4 && w.getType(x, base + n, z) == BlockType::PORTAL) n++;
            if (n == 3 && w.getType(x, base + n, z) == BlockType::OBSIDIAN) {
                bool alongX = w.getType(x - 1, y, z) == BlockType::PORTAL || w.getType(x + 1, y, z) == BlockType::PORTAL;
                bool alongZ = w.getType(x, y, z - 1) == BlockType::PORTAL || w.getType(x, y, z + 1) == BlockType::PORTAL;
                if (alongX && alongZ) {
                    setBlockWithNotify(x, y, z, BlockType::AIR);
                } else if ((w.getType(x + ax, y, z + az) != BlockType::OBSIDIAN || w.getType(x - ax, y, z - az) != BlockType::PORTAL) &&
                           (w.getType(x - ax, y, z - az) != BlockType::OBSIDIAN || w.getType(x + ax, y, z + az) != BlockType::PORTAL)) {
                    setBlockWithNotify(x, y, z, BlockType::AIR);
                }
            } else {
                setBlockWithNotify(x, y, z, BlockType::AIR);
            }
        }
    }

    bool canBlockCatchFire(int x, int y, int z) { return fireEncourage(w.getType(x, y, z)) > 0; }
    bool canNeighborBurn(int x, int y, int z) {   // BlockFire.func_263_h
        return canBlockCatchFire(x + 1, y, z) || canBlockCatchFire(x - 1, y, z) || canBlockCatchFire(x, y - 1, z) ||
               canBlockCatchFire(x, y + 1, z) || canBlockCatchFire(x, y, z - 1) || canBlockCatchFire(x, y, z + 1);
    }
    bool fireCanPlaceAt(int x, int y, int z) { return isNormalCube(w.getType(x, y - 1, z)) || canNeighborBurn(x, y, z); }

    // BlockFire.getChanceOfNeighborsEncouragingFire: la mayor de las 6 caras, solo si la celda es aire
    int chanceOfNeighborsEncouragingFire(int x, int y, int z) {
        if (w.getType(x, y, z) != BlockType::AIR) return 0;
        int c = 0, e;
        e = fireEncourage(w.getType(x + 1, y, z)); if (e > c) c = e;
        e = fireEncourage(w.getType(x - 1, y, z)); if (e > c) c = e;
        e = fireEncourage(w.getType(x, y - 1, z)); if (e > c) c = e;
        e = fireEncourage(w.getType(x, y + 1, z)); if (e > c) c = e;
        e = fireEncourage(w.getType(x, y, z - 1)); if (e > c) c = e;
        e = fireEncourage(w.getType(x, y, z + 1)); if (e > c) c = e;
        return c;
    }

    // BlockFire.tryToCatchBlockOnFire: el bloque se quema (pasa a fuego o desaparece)
    void tryToCatchBlockOnFire(int x, int y, int z, int chance, int age) {
        const BlockType burning = w.getType(x, y, z);
        int ability = fireCatch(burning);
        if (w.randInt(chance) < ability) {
            if (w.randInt(age + 10) < 5) {
                int na = age + w.randInt(5) / 4;
                if (na > 15) na = 15;
                setBlockAndMetadataWithNotify(x, y, z, BlockType::FIRE, na);
            } else {
                setBlockWithNotify(x, y, z, BlockType::AIR);
            }
            if (burning == BlockType::TNT) TntManager::primeCell(x, y, z);   // BlockFire.tryToCatchBlockOnFire: el TNT se enciende en vez de quemarse
        }
    }

    // BlockFire.onBlockAdded
    void fireOnBlockAdded(int x, int y, int z) {
        if (w.getType(x, y - 1, z) != BlockType::OBSIDIAN || !tryToCreatePortal(x, y, z)) {
            if (!isNormalCube(w.getType(x, y - 1, z)) && !canNeighborBurn(x, y, z)) setBlockWithNotify(x, y, z, BlockType::AIR);
            else scheduleBlockUpdate(x, y, z, BlockType::FIRE, tickRateOf(BlockType::FIRE));
        }
    }

    // BlockFire.onNeighborBlockChange
    void fireNeighborChange(int x, int y, int z) {
        if (!isNormalCube(w.getType(x, y - 1, z)) && !canNeighborBurn(x, y, z)) setBlockWithNotify(x, y, z, BlockType::AIR);
    }

    // BlockFire.updateTick, literal (también lo que sigue ejecutándose si el fuego se quita al principio)
    void fireUpdateTick(int x, int y, int z) {
        const bool onNetherrack = (w.getType(x, y - 1, z) == BlockType::NETHERRACK); // el netherrack mantiene el fuego encendido
        if (!fireCanPlaceAt(x, y, z)) setBlockWithNotify(x, y, z, BlockType::AIR);
        // BlockFire.updateTick: la lluvia apaga el fuego (el suyo o el de una celda vecina que ve el cielo)
        if (Weather::isRaining() && (Weather::canRainAtGrid(x, y, z) || Weather::canRainAtGrid(x - 1, y, z) || Weather::canRainAtGrid(x + 1, y, z) ||
                                     Weather::canRainAtGrid(x, y, z - 1) || Weather::canRainAtGrid(x, y, z + 1))) {
            setBlockWithNotify(x, y, z, BlockType::AIR);
            return;
        }

        int age = w.getMeta(x, y, z);
        if (age < 15 && w.inRange(x, y, z)) w.putCell(x, y, z, w.getType(x, y, z), age + w.randInt(3) / 2);  // setBlockMetadata, sin avisar
        else if (age < 15) w.randInt(3);

        scheduleBlockUpdate(x, y, z, BlockType::FIRE, tickRateOf(BlockType::FIRE));
        if (!onNetherrack && !canNeighborBurn(x, y, z)) {
            if (!isNormalCube(w.getType(x, y - 1, z)) || age > 3) setBlockWithNotify(x, y, z, BlockType::AIR);
        } else if (!onNetherrack && !canBlockCatchFire(x, y - 1, z) && age == 15 && w.randInt(4) == 0) {
            setBlockWithNotify(x, y, z, BlockType::AIR);
        } else {
            tryToCatchBlockOnFire(x + 1, y, z, 300, age);
            tryToCatchBlockOnFire(x - 1, y, z, 300, age);
            tryToCatchBlockOnFire(x, y - 1, z, 250, age);
            tryToCatchBlockOnFire(x, y + 1, z, 250, age);
            tryToCatchBlockOnFire(x, y, z - 1, 300, age);
            tryToCatchBlockOnFire(x, y, z + 1, 300, age);

            for (int px = x - 1; px <= x + 1; px++) {
                for (int pz = z - 1; pz <= z + 1; pz++) {
                    for (int py = y - 1; py <= y + 4; py++) {
                        if (px != x || py != y || pz != z) {
                            int chance = 100;
                            if (py > y + 1) chance += (py - (y + 1)) * 100;
                            int enc = chanceOfNeighborsEncouragingFire(px, py, pz);
                            if (enc > 0) {
                                int t = (enc + 40) / (age + 30);
                                if (t > 0 && w.randInt(chance) <= t) {
                                    int na = age + w.randInt(5) / 4;
                                    if (na > 15) na = 15;
                                    setBlockAndMetadataWithNotify(px, py, pz, BlockType::FIRE, na);
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // BlockStationary.updateTick (lava quieta, tick aleatorio): sube al azar hasta 2 veces buscando aire junto a algo
    // que arde, y ahí enciende fuego; si se topa con algo sólido, se detiene
    void lavaRandomTick(int x, int y, int z) {
        int n = w.randInt(3);
        for (int i = 0; i < n; i++) {
            x += w.randInt(3) - 1;
            y += 1;
            z += w.randInt(3) - 1;
            BlockType t = w.getType(x, y, z);
            if (t == BlockType::AIR) {
                if (materialBurns(w.getType(x - 1, y, z)) || materialBurns(w.getType(x + 1, y, z)) ||
                    materialBurns(w.getType(x, y, z - 1)) || materialBurns(w.getType(x, y, z + 1)) ||
                    materialBurns(w.getType(x, y - 1, z)) || materialBurns(w.getType(x, y + 1, z))) {
                    setBlockWithNotify(x, y, z, BlockType::FIRE);
                    return;
                }
            } else if (isMaterialSolid(t)) {
                return;
            }
        }
    }

    // ------------------------------------------------------------------ BlockFluid.getFlowVector
    // Dirección hacia donde empuja una celda de líquido (para arrastrar al jugador)
    void flowVector(int mat, int x, int y, int z, double out[3]) {
        double vx = 0, vy = 0, vz = 0;
        int own = getEffectiveFlowDecay(mat, x, y, z);

        for (int d = 0; d < 4; d++) {
            int nx = x, nz = z;
            if (d == 0) nx = x - 1;
            if (d == 1) nz = z - 1;
            if (d == 2) nx = x + 1;
            if (d == 3) nz = z + 1;
            int nd = getEffectiveFlowDecay(mat, nx, y, nz);
            if (nd < 0) {
                if (!isMaterialSolid(w.getType(nx, y, nz))) {
                    nd = getEffectiveFlowDecay(mat, nx, y - 1, nz);
                    if (nd >= 0) {
                        int k = nd - (own - 8);
                        vx += (double)((nx - x) * k);
                        vz += (double)((nz - z) * k);
                    }
                }
            } else {
                int k = nd - own;
                vx += (double)((nx - x) * k);
                vz += (double)((nz - z) * k);
            }
        }

        if (w.getMeta(x, y, z) >= 8) {
            bool solid = solidFor(mat, x, y, z - 1) || solidFor(mat, x, y, z + 1) || solidFor(mat, x - 1, y, z) ||
                         solidFor(mat, x + 1, y, z) || solidFor(mat, x, y + 1, z - 1) || solidFor(mat, x, y + 1, z + 1) ||
                         solidFor(mat, x - 1, y + 1, z) || solidFor(mat, x + 1, y + 1, z);
            if (solid) {
                norm(vx, vy, vz);
                vy += -6.0;
            }
        }
        norm(vx, vy, vz);
        out[0] = vx; out[1] = vy; out[2] = vz;
    }
    // BlockFluid.getIsBlockSolid (caras laterales): el mismo líquido no cuenta; el resto, si su material es sólido
    bool solidFor(int mat, int x, int y, int z) {
        BlockType t = w.getType(x, y, z);
        if (liquidMaterial(t) == mat) return false;
        return isMaterialSolid(t);
    }
    static void norm(double& x, double& y, double& z) {
        double l = sqrt(x * x + y * y + z * z);
        if (l < 1.0E-4) { x = y = z = 0.0; return; }
        x /= l; y /= l; z /= l;
    }

    // World.handleMaterialAcceleration: empuje de las corrientes sobre una caja (en coordenadas de celda del
    // original: cada celda ocupa [n, n+1)). Devuelve si la caja toca ese líquido.
    bool materialAcceleration(double minX, double minY, double minZ, double maxX, double maxY, double maxZ,
                              int mat, double out[3]) {
        int x0 = (int)floor(minX), x1 = (int)floor(maxX + 1.0);
        int y0 = (int)floor(minY), y1 = (int)floor(maxY + 1.0);
        int z0 = (int)floor(minZ), z1 = (int)floor(maxZ + 1.0);
        bool any = false;
        double vx = 0, vy = 0, vz = 0;
        for (int x = x0; x < x1; x++)
            for (int y = y0; y < y1; y++)
                for (int z = z0; z < z1; z++) {
                    if (matAt(x, y, z) != mat) continue;
                    double surface = (double)((float)(y + 1) - percentAir(w.getMeta(x, y, z)));
                    if ((double)y1 >= surface) {
                        any = true;
                        double v[3]; flowVector(mat, x, y, z, v);
                        vx += v[0]; vy += v[1]; vz += v[2];
                    }
                }
        double l = sqrt(vx * vx + vy * vy + vz * vz);
        out[0] = out[1] = out[2] = 0.0;
        if (l > 0.0) {
            out[0] = vx / l * 0.014; out[1] = vy / l * 0.014; out[2] = vz / l * 0.014;
        }
        return any;
    }
};

}  // namespace Liquid

#endif
