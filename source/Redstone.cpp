#include "Redstone.h"
#include "Sound.h"
#include "ChunkManager.h"
#include "Chunk.h"
#include "PendingDrops.h"
#include "WorldConfig.h"
#include "Pistons.h"
#include "RailLogic.h"
#include "TntManager.h"
#include "Dispenser.h"
#include "NoteBlocks.h"
#include <math.h>
#include <stdlib.h>
#include <vector>

// Coordenadas globales de celda: gx = chunkX * SIZE_X + xi, gy = yi, gz = chunkZ * SIZE_Z + zi (las mismas de PartialBlocks y LiquidWorld).
// La celda (gx, gy, gz) ocupa en el mundo X de gx - 4 a gx - 3, Y de gy - 1,5 a gy - 0,5 y Z de gz - 4 a gz - 3.
//
// Lados (el argumento "side" de Block.isPoweringTo): 0 abajo, 1 arriba, 2 -Z, 3 +Z, 4 -X, 5 +X; siempre "de quien pregunta hacia el bloque preguntado".

namespace {
    const int SIZE_X = WorldConfig::SIZE_X, SIZE_Y = WorldConfig::SIZE_Y, SIZE_Z = WorldConfig::SIZE_Z;

    const ChunkManager* W = nullptr;
    bool g_wiresProvide = true;            // BlockRedstoneWire.wiresProvidePower
    unsigned long g_time = 0;              // World.getWorldTime (ticks del juego)
    unsigned long g_seq = 0;
    Redstone::EntityEnumerator g_enum = nullptr;
    void* g_enumUser = nullptr;

    const int DX[6] = { 0, 0, 0, 0, -1, 1 };
    const int DY[6] = { -1, 1, 0, 0, 0, 0 };
    const int DZ[6] = { 0, 0, -1, 1, 0, 0 };

    struct P { int x, y, z; };

    // ---- Acceso al mundo ----
    int floorDivi(int v, int d) { return v >= 0 ? v / d : -((-v + d - 1) / d); }

    Chunk* chunkOf(int gx, int gz, int& xi, int& zi) {
        const int cx = floorDivi(gx, SIZE_X), cz = floorDivi(gz, SIZE_Z);
        Chunk* c = W->getChunkByChunkCoord(cx, cz);
        if (!c || !c->isGenerationComplete()) return nullptr;
        xi = gx - cx * SIZE_X;
        zi = gz - cz * SIZE_Z;
        return c;
    }

    // Lo que no esta cargado cuenta como piedra: no tiene energia y no se toca
    BlockType T(int x, int y, int z) {
        if (y < 0) return BlockType::STONE;
        if (y >= SIZE_Y) return BlockType::AIR;
        int xi, zi; Chunk* c = chunkOf(x, z, xi, zi);
        return c ? c->getBlockType(xi, y, zi) : BlockType::STONE;
    }

    int M(int x, int y, int z) {
        if (y < 0 || y >= SIZE_Y) return 0;
        int xi, zi; Chunk* c = chunkOf(x, z, xi, zi);
        return c ? c->getLiquidMeta(xi, y, zi) : 0;
    }

    bool normal(int x, int y, int z) { return isNormalCube(T(x, y, z)); }

    // setBlockMetadata sin avisar (los bloques de redstone llevan disableNeighborNotifyOnMetadataChange): solo vuelve a dibujar
    void setM(int x, int y, int z, int m) {
        if (y < 0 || y >= SIZE_Y) return;
        int xi, zi; Chunk* c = chunkOf(x, z, xi, zi);
        if (c) c->setBlockMeta(xi, y, zi, m, W);
    }

    // setBlockAndMetadataWithNotify: cambia el bloque y avisa (Chunk::notifyLiquidNeighbors llega a Redstone::blockChanged)
    bool setTM(int x, int y, int z, BlockType t, int m) {
        if (y < 0 || y >= SIZE_Y) return false;
        int xi, zi; Chunk* c = chunkOf(x, z, xi, zi);
        if (!c) return false;
        c->setBlockWithMeta(xi, y, zi, t, m, W);
        c->notifyLiquidNeighbors(xi, y, zi, W);
        return true;
    }

    void removeBlock(int x, int y, int z) {
        if (y < 0 || y >= SIZE_Y) return;
        int xi, zi; Chunk* c = chunkOf(x, z, xi, zi);
        if (c) c->breakBlockAt(xi, y, zi, W);
    }

    void dropItem(int x, int y, int z, BlockType item) {
        PendingDrops::push((float)x - SIZE_X / 2.0f + 0.5f, (float)y - 1.0f, (float)z - SIZE_Z / 2.0f + 0.5f, item);
    }

    // ---- Tipos ----
    // Block.canProvidePower
    bool canProvidePower(BlockType t) {
        switch (t) {
            case BlockType::REDSTONE_WIRE: return g_wiresProvide;
            case BlockType::LEVER: case BlockType::BUTTON: case BlockType::PLATE_STONE: case BlockType::PLATE_WOOD:
            case BlockType::TORCH_REDSTONE_OFF: case BlockType::TORCH_REDSTONE_ON:
            case BlockType::RAIL_DETECTOR:   // BlockDetectorRail.canProvidePower
                return true;
            default: return false;
        }
    }

    bool reactsToNeighbors(BlockType t) {
        return isRedstoneComponent(t) || isDoorBlock(t) || t == BlockType::TRAPDOOR || isPistonBase(t) || t == BlockType::PISTON_HEAD ||
               t == BlockType::TNT || isDispenserBlock(t) || t == BlockType::NOTE_BLOCK;
    }

    // ---- Energia (World.isBlockProvidingPowerTo y compania) ----
    bool indirectlyGettingPowered(int x, int y, int z);
    bool gettingPowered(int x, int y, int z);

    // BlockRedstoneWire.isPowerProviderOrWire. d = direccion (0 al sur +Z, 1 al oeste -X, 2 al norte -Z, 3 al este +X) o -1
    bool isPowerProviderOrWire(int x, int y, int z, int d) {
        const BlockType t = T(x, y, z);
        if (t == BlockType::REDSTONE_WIRE) return true;
        if (t == BlockType::AIR) return false;
        if (canProvidePower(t)) return true;
        if (!isRepeaterBlock(t)) return false;
        static const int OPP[4] = { 2, 3, 0, 1 };   // ModelBed.field_22279_b: la direccion contraria
        return d == OPP[M(x, y, z) & 3];
    }

    bool wirePoweringTo(int x, int y, int z, int s) {
        if (!g_wiresProvide) return false;
        if (M(x, y, z) == 0) return false;
        if (s == 1) return true;
        bool w = isPowerProviderOrWire(x - 1, y, z, 1) || (!normal(x - 1, y, z) && isPowerProviderOrWire(x - 1, y - 1, z, -1));
        bool e = isPowerProviderOrWire(x + 1, y, z, 3) || (!normal(x + 1, y, z) && isPowerProviderOrWire(x + 1, y - 1, z, -1));
        bool n = isPowerProviderOrWire(x, y, z - 1, 2) || (!normal(x, y, z - 1) && isPowerProviderOrWire(x, y - 1, z - 1, -1));
        bool so = isPowerProviderOrWire(x, y, z + 1, 0) || (!normal(x, y, z + 1) && isPowerProviderOrWire(x, y - 1, z + 1, -1));
        if (!normal(x, y + 1, z)) {
            if (normal(x - 1, y, z) && isPowerProviderOrWire(x - 1, y + 1, z, -1)) w = true;
            if (normal(x + 1, y, z) && isPowerProviderOrWire(x + 1, y + 1, z, -1)) e = true;
            if (normal(x, y, z - 1) && isPowerProviderOrWire(x, y + 1, z - 1, -1)) n = true;
            if (normal(x, y, z + 1) && isPowerProviderOrWire(x, y + 1, z + 1, -1)) so = true;
        }
        if (!n && !e && !w && !so && s >= 2 && s <= 5) return true;
        if (s == 2 && n && !w && !e) return true;
        if (s == 3 && so && !w && !e) return true;
        if (s == 4 && w && !n && !so) return true;
        return s == 5 && e && !n && !so;
    }

    // Block.isPoweringTo
    bool isPoweringTo(BlockType t, int m, int x, int y, int z, int s) {
        switch (t) {
            case BlockType::REDSTONE_WIRE: return wirePoweringTo(x, y, z, s);
            case BlockType::LEVER: case BlockType::BUTTON: return (m & 8) > 0;
            case BlockType::PLATE_STONE: case BlockType::PLATE_WOOD: return m > 0;
            case BlockType::RAIL_DETECTOR: return (m & 8) != 0;
            case BlockType::TORCH_REDSTONE_ON:
                if (m == 5 && s == 1) return false;
                if (m == 3 && s == 3) return false;
                if (m == 4 && s == 2) return false;
                if (m == 1 && s == 5) return false;
                return !(m == 2 && s == 4);
            case BlockType::REPEATER_ON: {
                const int d = m & 3;
                return (d == 0 && s == 3) || (d == 1 && s == 4) || (d == 2 && s == 2) || (d == 3 && s == 5);
            }
            default: return false;
        }
    }

    // Block.isIndirectlyPoweringTo
    bool isIndirectlyPoweringTo(BlockType t, int m, int x, int y, int z, int s) {
        switch (t) {
            case BlockType::REDSTONE_WIRE: return g_wiresProvide ? wirePoweringTo(x, y, z, s) : false;
            case BlockType::LEVER: case BlockType::BUTTON: {
                if ((m & 8) == 0) return false;
                const int v = m & 7;
                return ((v == 6 || v == 5) && s == 1) || (v == 4 && s == 2) || (v == 3 && s == 3) || (v == 2 && s == 4) || (v == 1 && s == 5);
            }
            case BlockType::PLATE_STONE: case BlockType::PLATE_WOOD: return m != 0 && s == 1;
            case BlockType::RAIL_DETECTOR: return (m & 8) != 0 && s == 1;   // BlockDetectorRail.isIndirectlyPoweringTo: solo hacia arriba
            case BlockType::TORCH_REDSTONE_ON: case BlockType::TORCH_REDSTONE_OFF:
                return s == 0 ? isPoweringTo(t, m, x, y, z, s) : false;
            case BlockType::REPEATER_ON: return isPoweringTo(t, m, x, y, z, s);
            default: return false;
        }
    }

    bool providingPowerTo(int x, int y, int z, int s) {   // World.isBlockProvidingPowerTo
        const BlockType t = T(x, y, z);
        if (t == BlockType::AIR) return false;
        if (!isRedstoneComponent(t)) return false;
        return isIndirectlyPoweringTo(t, M(x, y, z), x, y, z, s);
    }

    bool gettingPowered(int x, int y, int z) {   // World.isBlockGettingPowered
        return providingPowerTo(x, y - 1, z, 0) || providingPowerTo(x, y + 1, z, 1) || providingPowerTo(x, y, z - 1, 2) ||
               providingPowerTo(x, y, z + 1, 3) || providingPowerTo(x - 1, y, z, 4) || providingPowerTo(x + 1, y, z, 5);
    }

    bool indirectProvidingPowerTo(int x, int y, int z, int s) {   // World.isBlockIndirectlyProvidingPowerTo
        const BlockType t = T(x, y, z);
        if (isNormalCube(t)) return gettingPowered(x, y, z);
        if (t == BlockType::AIR || !isRedstoneComponent(t)) return false;
        return isPoweringTo(t, M(x, y, z), x, y, z, s);
    }

    bool indirectlyGettingPowered(int x, int y, int z) {   // World.isBlockIndirectlyGettingPowered
        return indirectProvidingPowerTo(x, y - 1, z, 0) || indirectProvidingPowerTo(x, y + 1, z, 1) || indirectProvidingPowerTo(x, y, z - 1, 2) ||
               indirectProvidingPowerTo(x, y, z + 1, 3) || indirectProvidingPowerTo(x - 1, y, z, 4) || indirectProvidingPowerTo(x + 1, y, z, 5);
    }

    // ---- Avisos y recursion ----
    // La recursion del original (cables que se avisan unos a otros) se corta a cierta profundidad y el resto se hace despues, para no agotar la pila
    const int MAX_DEPTH = 22;
    int g_depth = 0;
    int g_top = 0;

    struct Def { int kind; int x, y, z, a, b, c, id; };   // kind 0 = neighborChanged(x,y,z,id), 1 = wireCompute(x,y,z, a,b,c, id = metaOverride)
    std::vector<Def> g_def;

    struct Rem { int x, y, z; BlockType t; int m; };
    std::vector<Rem> g_rem;

    struct Tick { int x, y, z; BlockType t; unsigned long due, seq; };
    std::vector<Tick> g_ticks;

    struct Burn { int x, y, z; unsigned long time; };
    std::vector<Burn> g_burn;

    std::vector<P> g_wireSet;   // BlockRedstoneWire.field_21031_b

    void neighborChanged(int x, int y, int z, BlockType changed);
    void wireCompute(int x, int y, int z, int sx, int sy, int sz, int metaOverride);

    void notify(int x, int y, int z, BlockType id) {   // World.notifyBlocksOfNeighborChange
        for (int d = 0; d < 6; d++) neighborChanged(x + DX[d], y + DY[d], z + DZ[d], id);
    }
    void notifyAround(int x, int y, int z, BlockType id) {   // las 6 celdas de alrededor avisan a sus vecinos (lo que hacen onBlockAdded/onBlockRemoval de las antorchas)
        for (int d = 0; d < 6; d++) notify(x + DX[d], y + DY[d], z + DZ[d], id);
    }
    // el lado donde se apoya la palanca o el boton (metadata 1 a 4; el resto = abajo)
    void notifyAttached(int x, int y, int z, int v, BlockType id) {
        if (v == 1) notify(x - 1, y, z, id);
        else if (v == 2) notify(x + 1, y, z, id);
        else if (v == 3) notify(x, y, z - 1, id);
        else if (v == 4) notify(x, y, z + 1, id);
        else notify(x, y - 1, z, id);
    }

    void schedule(int x, int y, int z, BlockType t, int delay) {   // World.scheduleBlockUpdate (sin repetir la misma celda y bloque)
        for (size_t i = 0; i < g_ticks.size(); i++)
            if (g_ticks[i].x == x && g_ticks[i].y == y && g_ticks[i].z == z && g_ticks[i].t == t) return;
        if (g_ticks.size() >= 1024) return;
        Tick k; k.x = x; k.y = y; k.z = z; k.t = t; k.due = g_time + (unsigned long)(delay > 0 ? delay : 1); k.seq = g_seq++;
        g_ticks.push_back(k);
    }

    // ---- Cable ----
    int maxStrength(int x, int y, int z, int prev) {
        if (T(x, y, z) != BlockType::REDSTONE_WIRE) return prev;
        const int m = M(x, y, z);
        return m > prev ? m : prev;
    }

    void addWirePos(int x, int y, int z) {
        for (size_t i = 0; i < g_wireSet.size(); i++)
            if (g_wireSet[i].x == x && g_wireSet[i].y == y && g_wireSet[i].z == z) return;
        P p; p.x = x; p.y = y; p.z = z;
        g_wireSet.push_back(p);
    }

    void flushWireSet() {
        std::vector<P> list = g_wireSet;
        g_wireSet.clear();
        for (size_t i = 0; i < list.size(); i++) notify(list[i].x, list[i].y, list[i].z, BlockType::REDSTONE_WIRE);
    }

    // BlockRedstoneWire.func_21030_a: recalcula la fuerza de este cable y avisa a los que cambian. (sx, sy, sz) = de donde vino el aviso
    void wireCompute(int x, int y, int z, int sx, int sy, int sz, int metaOverride) {
        if (g_depth >= MAX_DEPTH) {
            if (g_def.size() < 4096) { Def d; d.kind = 1; d.x = x; d.y = y; d.z = z; d.a = sx; d.b = sy; d.c = sz; d.id = metaOverride; g_def.push_back(d); }
            return;
        }
        ++g_depth;
        const int old = metaOverride >= 0 ? metaOverride : M(x, y, z);
        int strength = 0;
        g_wiresProvide = false;
        const bool powered = indirectlyGettingPowered(x, y, z);
        g_wiresProvide = true;
        if (powered) {
            strength = 15;
        } else {
            for (int i = 0; i < 4; i++) {
                int nx = x, nz = z;
                if (i == 0) nx = x - 1;
                if (i == 1) nx = x + 1;
                if (i == 2) nz = z - 1;
                if (i == 3) nz = z + 1;
                if (nx != sx || y != sy || nz != sz) strength = maxStrength(nx, y, nz, strength);
                if (normal(nx, y, nz) && !normal(x, y + 1, z)) {
                    if (nx != sx || y + 1 != sy || nz != sz) strength = maxStrength(nx, y + 1, nz, strength);
                } else if (!normal(nx, y, nz) && (nx != sx || y - 1 != sy || nz != sz)) {
                    strength = maxStrength(nx, y - 1, nz, strength);
                }
            }
            strength = strength > 0 ? strength - 1 : 0;
        }
        if (old != strength) {
            if (T(x, y, z) == BlockType::REDSTONE_WIRE) setM(x, y, z, strength);
            int cur = strength;
            for (int i = 0; i < 4; i++) {
                int nx = x, nz = z;
                int ny = y - 1;
                if (i == 0) nx = x - 1;
                if (i == 1) nx = x + 1;
                if (i == 2) nz = z - 1;
                if (i == 3) nz = z + 1;
                if (normal(nx, y, nz)) ny += 2;
                int v = maxStrength(nx, y, nz, -1);
                cur = (T(x, y, z) == BlockType::REDSTONE_WIRE) ? M(x, y, z) : strength;
                if (cur > 0) --cur;
                if (v >= 0 && v != cur) wireCompute(nx, y, nz, x, y, z, -1);
                v = maxStrength(nx, ny, nz, -1);
                cur = (T(x, y, z) == BlockType::REDSTONE_WIRE) ? M(x, y, z) : strength;
                if (cur > 0) --cur;
                if (v >= 0 && v != cur) wireCompute(nx, ny, nz, x, y, z, -1);
            }
            if (old == 0 || cur == 0) {
                addWirePos(x, y, z);
                addWirePos(x - 1, y, z); addWirePos(x + 1, y, z);
                addWirePos(x, y - 1, z); addWirePos(x, y + 1, z);
                addWirePos(x, y, z - 1); addWirePos(x, y, z + 1);
            }
        }
        --g_depth;
    }

    void wireUpdate(int x, int y, int z) {   // updateAndPropagateCurrentStrength
        wireCompute(x, y, z, x, y, z, -1);
        flushWireSet();
    }

    void wireNeighborNotify(int x, int y, int z) {   // notifyWireNeighborsOfNeighborChange
        if (T(x, y, z) != BlockType::REDSTONE_WIRE) return;
        const BlockType w = BlockType::REDSTONE_WIRE;
        notify(x, y, z, w);
        notify(x - 1, y, z, w); notify(x + 1, y, z, w);
        notify(x, y, z - 1, w); notify(x, y, z + 1, w);
        notify(x, y - 1, z, w); notify(x, y + 1, z, w);
    }

    void wireSideNotifications(int x, int y, int z) {
        wireNeighborNotify(x - 1, y, z); wireNeighborNotify(x + 1, y, z);
        wireNeighborNotify(x, y, z - 1); wireNeighborNotify(x, y, z + 1);
        wireNeighborNotify(x - 1, y + (normal(x - 1, y, z) ? 1 : -1), z);
        wireNeighborNotify(x + 1, y + (normal(x + 1, y, z) ? 1 : -1), z);
        wireNeighborNotify(x, y + (normal(x, y, z - 1) ? 1 : -1), z - 1);
        wireNeighborNotify(x, y + (normal(x, y, z + 1) ? 1 : -1), z + 1);
    }

    // ---- Antorcha ----
    bool torchPowered(int x, int y, int z) {   // BlockRedstoneTorch.func_30002_h: el bloque de apoyo recibe energia
        const int m = M(x, y, z);
        return (m == 5 && indirectProvidingPowerTo(x, y - 1, z, 0)) || (m == 3 && indirectProvidingPowerTo(x, y, z - 1, 2)) ||
               (m == 4 && indirectProvidingPowerTo(x, y, z + 1, 3)) || (m == 1 && indirectProvidingPowerTo(x - 1, y, z, 4)) ||
               (m == 2 && indirectProvidingPowerTo(x + 1, y, z, 5));
    }

    bool burnedOut(int x, int y, int z, bool add) {   // checkForBurnout: 8 cambios en 100 ticks
        if (add) { Burn b; b.x = x; b.y = y; b.z = z; b.time = g_time; g_burn.push_back(b); }
        int n = 0;
        for (size_t i = 0; i < g_burn.size(); i++)
            if (g_burn[i].x == x && g_burn[i].y == y && g_burn[i].z == z && ++n >= 8) return true;
        return false;
    }

    void torchTick(int x, int y, int z, BlockType t) {
        const bool powered = torchPowered(x, y, z);
        while (!g_burn.empty() && g_time - g_burn[0].time > 100) g_burn.erase(g_burn.begin());
        const int m = M(x, y, z);
        if (t == BlockType::TORCH_REDSTONE_ON) {
            if (powered) {
                setTM(x, y, z, BlockType::TORCH_REDSTONE_OFF, m);
                burnedOut(x, y, z, true);   // el original ademas suena y echa humo
            }
        } else if (!powered && !burnedOut(x, y, z, false)) {
            setTM(x, y, z, BlockType::TORCH_REDSTONE_ON, m);
        }
    }

    // ---- Repetidor ----
    const int REPEATER_DELAY[4] = { 1, 2, 3, 4 };

    bool repeaterInput(int x, int y, int z, int meta) {   // func_22022_g
        switch (meta & 3) {
            case 0: return indirectProvidingPowerTo(x, y, z + 1, 3) || (T(x, y, z + 1) == BlockType::REDSTONE_WIRE && M(x, y, z + 1) > 0);
            case 1: return indirectProvidingPowerTo(x - 1, y, z, 4) || (T(x - 1, y, z) == BlockType::REDSTONE_WIRE && M(x - 1, y, z) > 0);
            case 2: return indirectProvidingPowerTo(x, y, z - 1, 2) || (T(x, y, z - 1) == BlockType::REDSTONE_WIRE && M(x, y, z - 1) > 0);
            default: return indirectProvidingPowerTo(x + 1, y, z, 5) || (T(x + 1, y, z) == BlockType::REDSTONE_WIRE && M(x + 1, y, z) > 0);
        }
    }

    void repeaterCheck(int x, int y, int z, BlockType t) {   // lo de onNeighborBlockChange: programa el cambio si el estado no coincide con la entrada
        const int m = M(x, y, z);
        const bool powered = repeaterInput(x, y, z, m);
        const int delay = REPEATER_DELAY[(m & 12) >> 2] * 2;
        if (t == BlockType::REPEATER_ON && !powered) schedule(x, y, z, t, delay);
        else if (t == BlockType::REPEATER_OFF && powered) schedule(x, y, z, t, delay);
    }

    void repeaterTick(int x, int y, int z, BlockType t) {
        const int m = M(x, y, z);
        const bool powered = repeaterInput(x, y, z, m);
        if (t == BlockType::REPEATER_ON && !powered) {
            setTM(x, y, z, BlockType::REPEATER_OFF, m);
        } else if (t == BlockType::REPEATER_OFF) {
            setTM(x, y, z, BlockType::REPEATER_ON, m);
            if (!powered) schedule(x, y, z, BlockType::REPEATER_ON, REPEATER_DELAY[(m & 12) >> 2] * 2);
        }
    }

    // ---- Placa de presion ----
    struct BoxQuery { float minX, minY, minZ, maxX, maxY, maxZ; bool everything; bool found; };
    void querySink(void* ctx, float a, float b, float c, float d, float e, float f, bool isItem) {
        BoxQuery* q = (BoxQuery*)ctx;
        if (q->found) return;
        if (isItem && !q->everything) return;
        if (d > q->minX && a < q->maxX && e > q->minY && b < q->maxY && f > q->minZ && c < q->maxZ) q->found = true;
    }

    void plateBox(int x, int y, int z, float out[6]) {   // la caja que detecta: 2/16 adentro de los lados y 0,25 de alto
        const float cx = (float)x - 4.0f, cy = (float)y - 1.5f, cz = (float)z - 4.0f;
        out[0] = cx + 2.0f / 16.0f; out[1] = cy; out[2] = cz + 2.0f / 16.0f;
        out[3] = cx + 1.0f - 2.0f / 16.0f; out[4] = cy + 0.25f; out[5] = cz + 1.0f - 2.0f / 16.0f;
    }

    void plateCheck(int x, int y, int z) {   // setStateIfMobInteractsWithPlate
        const BlockType t = T(x, y, z);
        if (!isPlateBlock(t)) return;
        BoxQuery q; plateBox(x, y, z, &q.minX);
        q.everything = (t == BlockType::PLATE_WOOD); q.found = false;
        if (g_enum) g_enum(g_enumUser, querySink, &q);
        const bool was = M(x, y, z) == 1;
        if (q.found && !was) {
            setM(x, y, z, 1);
            notify(x, y, z, t); notify(x, y - 1, z, t);
        }
        if (!q.found && was) {
            setM(x, y, z, 0);
            notify(x, y, z, t); notify(x, y - 1, z, t);
        }
        if (q.found) schedule(x, y, z, t, 20);
    }

    std::vector<P> g_touch;
    void touchSink(void* ctx, float a, float b, float c, float d, float e, float f, bool isItem) {
        (void)ctx;
        const int x0 = (int)floorf(a + 4.0f), x1 = (int)floorf(d + 4.0f);
        const int y0 = (int)floorf(b + 1.5f + 0.001f), y1 = (int)floorf(e + 1.5f);
        const int z0 = (int)floorf(c + 4.0f), z1 = (int)floorf(f + 4.0f);
        if ((x1 - x0 + 1) * (y1 - y0 + 1) * (z1 - z0 + 1) > 48) return;
        for (int x = x0; x <= x1; x++)
            for (int y = y0; y <= y1; y++)
                for (int z = z0; z <= z1; z++) {
                    const BlockType t = T(x, y, z);
                    if (!isPlateBlock(t)) continue;
                    if (isItem && t != BlockType::PLATE_WOOD) continue;
                    if (M(x, y, z) == 1) continue;   // ya pisada: la mantiene su tick
                    float pb[6]; plateBox(x, y, z, pb);
                    if (d > pb[0] && a < pb[3] && e > pb[1] && b < pb[4] && f > pb[2] && c < pb[5]) {
                        bool dup = false;
                        for (size_t i = 0; i < g_touch.size(); i++) if (g_touch[i].x == x && g_touch[i].y == y && g_touch[i].z == z) dup = true;
                        if (!dup) { P p; p.x = x; p.y = y; p.z = z; g_touch.push_back(p); }
                    }
                }
    }

    // ---- Riel detector (BlockDetectorRail) ----
    Redstone::EntityEnumerator g_cartEnum = nullptr;
    void* g_cartUser = nullptr;

    void detectorCheck(int x, int y, int z) {   // setStateIfMinecartInteractsWithRail
        const BlockType t = T(x, y, z);
        if (t != BlockType::RAIL_DETECTOR) return;
        const int m = M(x, y, z);
        const bool was = (m & 8) != 0;
        BoxQuery q; plateBox(x, y, z, &q.minX);
        q.everything = true; q.found = false;
        if (g_cartEnum) g_cartEnum(g_cartUser, querySink, &q);
        if (q.found && !was) {
            setM(x, y, z, m | 8);
            notify(x, y, z, t); notify(x, y - 1, z, t);
        }
        if (!q.found && was) {
            setM(x, y, z, m & 7);
            notify(x, y, z, t); notify(x, y - 1, z, t);
        }
        if (q.found) schedule(x, y, z, t, 20);
    }

    // BlockDetectorRail.onEntityCollidedWithBlock: una vagoneta que toca un detector apagado lo enciende
    std::vector<P> g_cartTouch;
    void cartTouchSink(void* ctx, float a, float b, float c, float d, float e, float f, bool isItem) {
        (void)ctx; (void)isItem;
        const int x0 = (int)floorf(a + 4.0f), x1 = (int)floorf(d + 4.0f);
        const int y0 = (int)floorf(b + 1.5f - 0.001f), y1 = (int)floorf(e + 1.5f);
        const int z0 = (int)floorf(c + 4.0f), z1 = (int)floorf(f + 4.0f);
        if ((x1 - x0 + 1) * (y1 - y0 + 1) * (z1 - z0 + 1) > 48) return;
        for (int x = x0; x <= x1; x++)
            for (int y = y0; y <= y1; y++)
                for (int z = z0; z <= z1; z++) {
                    if (T(x, y, z) != BlockType::RAIL_DETECTOR) continue;
                    if (M(x, y, z) & 8) continue;   // ya encendido: lo mantiene su tick
                    bool dup = false;
                    for (size_t i = 0; i < g_cartTouch.size(); i++) if (g_cartTouch[i].x == x && g_cartTouch[i].y == y && g_cartTouch[i].z == z) dup = true;
                    if (!dup) { P p; p.x = x; p.y = y; p.z = z; g_cartTouch.push_back(p); }
                }
    }

    // ---- Rieles: el mundo tal como lo ve RailLogic (cambiar la metadata avisa a los vecinos: World.setBlockMetadataWithNotify) ----
    struct RailWorld {
        BlockType type(int x, int y, int z) const { return T(x, y, z); }
        int meta(int x, int y, int z) const { return M(x, y, z); }
        void setMeta(int x, int y, int z, int m) { setM(x, y, z, m); notify(x, y, z, T(x, y, z)); }
    };

    // ---- Ticks ----
    void buttonRelease(int x, int y, int z) {
        const int m = M(x, y, z);
        if ((m & 8) == 0) return;
        setM(x, y, z, m & 7);
        notify(x, y, z, BlockType::BUTTON);
        notifyAttached(x, y, z, m & 7, BlockType::BUTTON);
    }

    void updateTick(BlockType t, int x, int y, int z) {
        switch (t) {
            case BlockType::BUTTON: buttonRelease(x, y, z); break;
            case BlockType::PLATE_STONE: case BlockType::PLATE_WOOD:
                if (M(x, y, z) != 0) plateCheck(x, y, z);
                break;
            case BlockType::TORCH_REDSTONE_ON: case BlockType::TORCH_REDSTONE_OFF: torchTick(x, y, z, t); break;
            case BlockType::REPEATER_ON: case BlockType::REPEATER_OFF: repeaterTick(x, y, z, t); break;
            case BlockType::REDSTONE_ORE_LIT: setTM(x, y, z, BlockType::REDSTONE_ORE, 0); break;
            case BlockType::DISPENSER: case BlockType::DISPENSER_W: case BlockType::DISPENSER_N: case BlockType::DISPENSER_E:   // BlockDispenser.updateTick
                if (indirectlyGettingPowered(x, y, z) || indirectlyGettingPowered(x, y + 1, z)) Dispenser::dispense(*W, x, y, z);
                break;
            case BlockType::RAIL_DETECTOR:
                if (M(x, y, z) & 8) detectorCheck(x, y, z);   // BlockDetectorRail.updateTick
                break;
            default: break;
        }
    }

    // ---- Lo que hace cada bloque al cambiar un vecino (onNeighborBlockChange) ----
    bool anySideNormal(int x, int y, int z) {
        return normal(x - 1, y, z) || normal(x + 1, y, z) || normal(x, y, z - 1) || normal(x, y, z + 1);
    }

    void doorPower(int x, int y, int z, bool powered) {   // BlockDoor.onPoweredBlockChange (x, y, z = la mitad de abajo)
        const BlockType t = T(x, y, z);
        const int m = M(x, y, z);
        if (((m & 4) > 0) == powered) return;
        if (T(x, y + 1, z) == t) setM(x, y + 1, z, (m ^ 4) + 8);
        setM(x, y, z, m ^ 4);
    }

    void neighborDo(BlockType t, int x, int y, int z, BlockType changed) {
        const int m = M(x, y, z);
        switch (t) {
            case BlockType::REDSTONE_WIRE:
                if (!normal(x, y - 1, z)) { dropItem(x, y, z, BlockType::REDSTONE); removeBlock(x, y, z); }
                else wireUpdate(x, y, z);
                break;
            case BlockType::LEVER: {
                const int v = m & 7;
                bool gone = !(anySideNormal(x, y, z) || normal(x, y - 1, z));
                if (!gone) {
                    gone = (v == 1 && !normal(x - 1, y, z)) || (v == 2 && !normal(x + 1, y, z)) || (v == 3 && !normal(x, y, z - 1)) ||
                           (v == 4 && !normal(x, y, z + 1)) || ((v == 5 || v == 6) && !normal(x, y - 1, z));
                }
                if (gone) { dropItem(x, y, z, BlockType::LEVER); removeBlock(x, y, z); }
                break;
            }
            case BlockType::BUTTON: {
                const int v = m & 7;
                bool gone = !anySideNormal(x, y, z);
                if (!gone) gone = (v == 1 && !normal(x - 1, y, z)) || (v == 2 && !normal(x + 1, y, z)) || (v == 3 && !normal(x, y, z - 1)) || (v == 4 && !normal(x, y, z + 1));
                if (gone) { dropItem(x, y, z, BlockType::BUTTON); removeBlock(x, y, z); }
                break;
            }
            case BlockType::PLATE_STONE: case BlockType::PLATE_WOOD:
                if (!normal(x, y - 1, z)) { dropItem(x, y, z, t); removeBlock(x, y, z); }
                break;
            case BlockType::TORCH_REDSTONE_ON: case BlockType::TORCH_REDSTONE_OFF: {
                const bool gone = (m == 1 && !normal(x - 1, y, z)) || (m == 2 && !normal(x + 1, y, z)) || (m == 3 && !normal(x, y, z - 1)) ||
                                  (m == 4 && !normal(x, y, z + 1)) || (m == 5 && !normal(x, y - 1, z));
                if (gone) { dropItem(x, y, z, BlockType::TORCH_REDSTONE_ON); removeBlock(x, y, z); }
                else schedule(x, y, z, t, 2);
                break;
            }
            case BlockType::REPEATER_ON: case BlockType::REPEATER_OFF:
                if (!normal(x, y - 1, z)) { dropItem(x, y, z, BlockType::REPEATER_ITEM); removeBlock(x, y, z); }
                else repeaterCheck(x, y, z, t);
                break;
            case BlockType::DOOR_WOOD: case BlockType::DOOR_IRON: {
                if (!canProvidePower(changed)) break;
                int by = y;
                if (m & 8) { if (T(x, y - 1, z) != t) break; by = y - 1; }
                doorPower(x, by, z, indirectlyGettingPowered(x, by, z) || indirectlyGettingPowered(x, by + 1, z));
                break;
            }
            case BlockType::RAIL: case BlockType::RAIL_POWERED: case BlockType::RAIL_DETECTOR: {   // BlockRail.onNeighborBlockChange
                const bool pw = Rails::hasPower(t);
                const int v = pw ? (m & 7) : m;
                const bool gone = !normal(x, y - 1, z) || (v == 2 && !normal(x + 1, y, z)) || (v == 3 && !normal(x - 1, y, z)) ||
                                  (v == 4 && !normal(x, y, z - 1)) || (v == 5 && !normal(x, y, z + 1));
                if (gone) { dropItem(x, y, z, t); removeBlock(x, y, z); break; }
                RailWorld rw;
                if (t == BlockType::RAIL_POWERED) {
                    bool on = indirectlyGettingPowered(x, y, z) || indirectlyGettingPowered(x, y + 1, z);
                    auto poweredAt = [](int a, int b, int c) { return indirectlyGettingPowered(a, b, c); };
                    on = on || Rails::chainPowered(rw, poweredAt, x, y, z, m, true, 0) || Rails::chainPowered(rw, poweredAt, x, y, z, m, false, 0);
                    bool changed = false;
                    if (on && (m & 8) == 0) { rw.setMeta(x, y, z, v | 8); changed = true; }
                    else if (!on && (m & 8) != 0) { rw.setMeta(x, y, z, v); changed = true; }
                    if (changed) {
                        notify(x, y - 1, z, t);
                        if (v >= 2 && v <= 5) notify(x, y + 1, z, t);
                    }
                } else if (t == BlockType::RAIL && canProvidePower(changed) && Rails::adjacentCount(rw, x, y, z) == 3) {
                    Rails::reshape(rw, x, y, z, indirectlyGettingPowered(x, y, z), false);
                }
                break;
            }
            case BlockType::PISTON_BASE: case BlockType::PISTON_STICKY:
                Pistons::baseNeighborChanged(*W, x, y, z);
                break;
            case BlockType::PISTON_HEAD:
                Pistons::headNeighborChanged(*W, x, y, z);
                break;
            case BlockType::TNT:   // BlockTNT.onNeighborBlockChange: con redstone se enciende
                if (canProvidePower(changed) && indirectlyGettingPowered(x, y, z)) { removeBlock(x, y, z); TntManager::primeCell(x, y, z); }
                break;
            case BlockType::DISPENSER: case BlockType::DISPENSER_W: case BlockType::DISPENSER_N: case BlockType::DISPENSER_E:   // BlockDispenser.onNeighborBlockChange
                if (canProvidePower(changed) && (indirectlyGettingPowered(x, y, z) || indirectlyGettingPowered(x, y + 1, z))) schedule(x, y, z, t, 4);
                break;
            case BlockType::NOTE_BLOCK:   // BlockNote.onNeighborBlockChange: suena al pasar de apagado a encendido
                if (canProvidePower(changed) && NoteBlocks::powerChanged(x, y, z, gettingPowered(x, y, z))) NoteBlocks::trigger(*W, x, y, z);
                break;
            case BlockType::TRAPDOOR:
                if (canProvidePower(changed)) {
                    const bool powered = indirectlyGettingPowered(x, y, z);
                    if (((m & 4) > 0) != powered) setM(x, y, z, m ^ 4);
                }
                break;
            default: break;
        }
    }

    void neighborChanged(int x, int y, int z, BlockType changed) {
        if (y < 0 || y >= SIZE_Y) return;
        const BlockType t = T(x, y, z);
        if (!reactsToNeighbors(t)) return;
        if (g_depth >= MAX_DEPTH) {
            if (g_def.size() < 4096) { Def d; d.kind = 0; d.x = x; d.y = y; d.z = z; d.a = d.b = d.c = 0; d.id = (int)changed; g_def.push_back(d); }
            return;
        }
        ++g_depth;
        neighborDo(t, x, y, z, changed);
        --g_depth;
    }

    // ---- Cola de trabajo ----
    void drainRemoved();

    void drainDeferred() {
        ++g_top;
        size_t head = 0;
        int guard = 0;
        while (head < g_def.size() && guard++ < 8000) {
            const Def d = g_def[head++];
            if (d.kind == 0) neighborChanged(d.x, d.y, d.z, (BlockType)d.id);
            else { wireCompute(d.x, d.y, d.z, d.a, d.b, d.c, d.id); flushWireSet(); }
            if (head > 1024 && head * 2 > g_def.size()) { g_def.erase(g_def.begin(), g_def.begin() + head); head = 0; }
        }
        g_def.clear();
        --g_top;
    }

    struct Top {
        explicit Top(const ChunkManager& cm) { W = &cm; if (g_top == 0) g_depth = 0; ++g_top; }
        ~Top() { if (--g_top == 0) { drainRemoved(); drainDeferred(); } }
    };

    // Block.onBlockRemoval de lo que se quito
    void handleRemoved(BlockType t, int m, int x, int y, int z) {
        switch (t) {
            case BlockType::REDSTONE_WIRE:
                notify(x, y + 1, z, t); notify(x, y - 1, z, t);
                wireCompute(x, y, z, x, y, z, m);
                flushWireSet();
                wireSideNotifications(x, y, z);
                break;
            case BlockType::LEVER: case BlockType::BUTTON:
                if (m & 8) { notify(x, y, z, t); notifyAttached(x, y, z, m & 7, t); }
                break;
            case BlockType::PLATE_STONE: case BlockType::PLATE_WOOD:
                if (m > 0) { notify(x, y, z, t); notify(x, y - 1, z, t); }
                break;
            case BlockType::TORCH_REDSTONE_ON: case BlockType::REPEATER_ON:
                notifyAround(x, y, z, t);
                break;
            case BlockType::PISTON_HEAD:
                Pistons::headRemoved(*W, x, y, z, m);
                break;
            default: break;
        }
    }

    void drainRemoved() {
        size_t head = 0;
        while (head < g_rem.size()) {
            const Rem r = g_rem[head++];
            handleRemoved(r.t, r.m, r.x, r.y, r.z);
        }
        g_rem.clear();
    }

    int faceCode(BlockFace f) {
        switch (f) {
            case BlockFace::BOTTOM: return 0;
            case BlockFace::TOP: return 1;
            case BlockFace::NORTH: return 2;
            case BlockFace::SOUTH: return 3;
            case BlockFace::WEST: return 4;
            case BlockFace::EAST: return 5;
            default: return -1;
        }
    }

    // ItemBlock: el objeto se pone en la celda de la cara apuntada. true si ahi cabe (aire, liquido, nieve, fuego, pasto alto)
    bool freeCell(BlockType t) {
        return t == BlockType::AIR || t == BlockType::TALLGRASS || t == BlockType::DEAD_BUSH || t == BlockType::SNOW_LAYER || t == BlockType::FIRE || isLiquidBlock(t);
    }

    bool put(int x, int y, int z, BlockType t, int meta) { return setTM(x, y, z, t, meta); }
}

namespace Redstone {

void setEntityEnumerator(EntityEnumerator e, void* user) { g_enum = e; g_enumUser = user; }
void setMinecartEnumerator(EntityEnumerator e, void* user) { g_cartEnum = e; g_cartUser = user; }

void reset() {
    g_ticks.clear(); g_burn.clear(); g_rem.clear(); g_def.clear(); g_wireSet.clear(); g_touch.clear(); g_cartTouch.clear();
    g_time = 0; g_seq = 0; g_depth = 0; g_wiresProvide = true;
}

void bounds(BlockType t, int meta, float b[6]) {
    b[0] = 0.0f; b[1] = 0.0f; b[2] = 0.0f; b[3] = 1.0f; b[4] = 1.0f; b[5] = 1.0f;
    switch (t) {
        case BlockType::REDSTONE_WIRE: b[4] = 1.0f / 16.0f; break;
        case BlockType::REPEATER_ON: case BlockType::REPEATER_OFF: b[4] = 2.0f / 16.0f; break;
        case BlockType::RAIL: case BlockType::RAIL_POWERED: case BlockType::RAIL_DETECTOR: {   // BlockRail.setBlockBoundsBasedOnState: 2/16, o 10/16 en las pendientes
            const int s = Rails::hasPower(t) ? (meta & 7) : meta;
            b[4] = (s >= 2 && s <= 5) ? 10.0f / 16.0f : 2.0f / 16.0f;
            break;
        }
        case BlockType::PLATE_STONE: case BlockType::PLATE_WOOD:
            b[0] = 1.0f / 16.0f; b[2] = 1.0f / 16.0f; b[3] = 15.0f / 16.0f; b[5] = 15.0f / 16.0f;
            b[4] = (meta == 1) ? 1.0f / 32.0f : 1.0f / 16.0f;
            break;
        case BlockType::LEVER: {   // BlockLever.setBlockBoundsBasedOnState
            const float s = 3.0f / 16.0f;
            switch (meta & 7) {
                case 1: b[0] = 0.0f; b[1] = 0.2f; b[2] = 0.5f - s; b[3] = s * 2.0f; b[4] = 0.8f; b[5] = 0.5f + s; break;
                case 2: b[0] = 1.0f - s * 2.0f; b[1] = 0.2f; b[2] = 0.5f - s; b[3] = 1.0f; b[4] = 0.8f; b[5] = 0.5f + s; break;
                case 3: b[0] = 0.5f - s; b[1] = 0.2f; b[2] = 0.0f; b[3] = 0.5f + s; b[4] = 0.8f; b[5] = s * 2.0f; break;
                case 4: b[0] = 0.5f - s; b[1] = 0.2f; b[2] = 1.0f - s * 2.0f; b[3] = 0.5f + s; b[4] = 0.8f; b[5] = 1.0f; break;
                default: b[0] = 0.25f; b[1] = 0.0f; b[2] = 0.25f; b[3] = 0.75f; b[4] = 0.6f; b[5] = 0.75f; break;
            }
            break;
        }
        case BlockType::BUTTON: {   // BlockButton.setBlockBoundsBasedOnState
            const float d = (meta & 8) ? 1.0f / 16.0f : 2.0f / 16.0f;
            const float y0 = 6.0f / 16.0f, y1 = 10.0f / 16.0f, w = 3.0f / 16.0f;
            switch (meta & 7) {
                case 1: b[0] = 0.0f; b[1] = y0; b[2] = 0.5f - w; b[3] = d; b[4] = y1; b[5] = 0.5f + w; break;
                case 2: b[0] = 1.0f - d; b[1] = y0; b[2] = 0.5f - w; b[3] = 1.0f; b[4] = y1; b[5] = 0.5f + w; break;
                case 3: b[0] = 0.5f - w; b[1] = y0; b[2] = 0.0f; b[3] = 0.5f + w; b[4] = y1; b[5] = d; break;
                default: b[0] = 0.5f - w; b[1] = y0; b[2] = 1.0f - d; b[3] = 0.5f + w; b[4] = y1; b[5] = 1.0f; break;
            }
            break;
        }
        case BlockType::TORCH_REDSTONE_ON: case BlockType::TORCH_REDSTONE_OFF:   // BlockTorch.getSelectedBoundingBoxFromPool
            switch (meta) {
                case 1: b[0] = 0.0f; b[1] = 0.2f; b[2] = 0.35f; b[3] = 0.3f; b[4] = 0.8f; b[5] = 0.65f; break;
                case 2: b[0] = 0.7f; b[1] = 0.2f; b[2] = 0.35f; b[3] = 1.0f; b[4] = 0.8f; b[5] = 0.65f; break;
                case 3: b[0] = 0.35f; b[1] = 0.2f; b[2] = 0.0f; b[3] = 0.65f; b[4] = 0.8f; b[5] = 0.3f; break;
                case 4: b[0] = 0.35f; b[1] = 0.2f; b[2] = 0.7f; b[3] = 0.65f; b[4] = 0.8f; b[5] = 1.0f; break;
                default: b[0] = 0.4f; b[1] = 0.0f; b[2] = 0.4f; b[3] = 0.6f; b[4] = 0.6f; b[5] = 0.6f; break;
            }
            break;
        default: break;
    }
}

void blockRemoved(const ChunkManager& cm, int gx, int gy, int gz, BlockType oldType, int oldMeta) {
    (void)cm;
    if (!isRedstoneComponent(oldType) && oldType != BlockType::PISTON_HEAD) return;
    if (g_rem.size() >= 256) return;
    Rem r; r.x = gx; r.y = gy; r.z = gz; r.t = oldType; r.m = oldMeta;
    g_rem.push_back(r);
}

void blockChanged(const ChunkManager& cm, int gx, int gy, int gz) {
    Top top(cm);
    drainRemoved();
    const BlockType t = T(gx, gy, gz);
    switch (t) {   // onBlockAdded del bloque nuevo
        case BlockType::REDSTONE_WIRE:
            wireUpdate(gx, gy, gz);
            notify(gx, gy + 1, gz, t); notify(gx, gy - 1, gz, t);
            wireSideNotifications(gx, gy, gz);
            break;
        case BlockType::TORCH_REDSTONE_ON:
            notifyAround(gx, gy, gz, t);
            schedule(gx, gy, gz, t, 2);
            break;
        case BlockType::TORCH_REDSTONE_OFF:
            schedule(gx, gy, gz, t, 2);
            break;
        case BlockType::REPEATER_ON: case BlockType::REPEATER_OFF:
            notifyAround(gx, gy, gz, t);
            break;
        case BlockType::PISTON_BASE: case BlockType::PISTON_STICKY:
            Pistons::baseAdded(cm, gx, gy, gz);
            break;
        case BlockType::TNT:   // BlockTNT.onBlockAdded: si ya hay corriente, se enciende
            if (indirectlyGettingPowered(gx, gy, gz)) { removeBlock(gx, gy, gz); TntManager::primeCell(gx, gy, gz); }
            break;
        case BlockType::RAIL: case BlockType::RAIL_POWERED: case BlockType::RAIL_DETECTOR: {   // BlockRail.onBlockAdded: toma su forma de los rieles vecinos
            RailWorld rw;
            Rails::reshape(rw, gx, gy, gz, indirectlyGettingPowered(gx, gy, gz), true);
            break;
        }
        default: break;
    }
    notify(gx, gy, gz, t);   // World.notifyBlockChange
}

void tick(const ChunkManager& cm) {
    Top top(cm);
    ++g_time;
    drainRemoved();
    for (int guard = 0; guard < 256; guard++) {
        int best = -1;
        for (size_t i = 0; i < g_ticks.size(); i++) {
            if (g_ticks[i].due > g_time) continue;
            if (best < 0 || g_ticks[i].due < g_ticks[best].due || (g_ticks[i].due == g_ticks[best].due && g_ticks[i].seq < g_ticks[best].seq)) best = (int)i;
        }
        if (best < 0) break;
        const Tick k = g_ticks[best];
        g_ticks.erase(g_ticks.begin() + best);
        if (T(k.x, k.y, k.z) == k.t) updateTick(k.t, k.x, k.y, k.z);
    }
    if (g_cartEnum) {   // BlockDetectorRail.onEntityCollidedWithBlock
        g_cartTouch.clear();
        g_cartEnum(g_cartUser, cartTouchSink, nullptr);
        std::vector<P> touched = g_cartTouch;
        g_cartTouch.clear();
        for (size_t i = 0; i < touched.size(); i++) detectorCheck(touched[i].x, touched[i].y, touched[i].z);
    }
    if (g_enum) {   // BlockPressurePlate.onEntityCollidedWithBlock
        g_touch.clear();
        g_enum(g_enumUser, touchSink, nullptr);
        std::vector<P> touched = g_touch;
        g_touch.clear();
        for (size_t i = 0; i < touched.size(); i++) { plateCheck(touched[i].x, touched[i].y, touched[i].z); }
    }
}

void randomTick(const ChunkManager& cm, int gx, int gy, int gz, BlockType t) {
    Top top(cm);
    if (t == BlockType::PISTON_MOVING) { Pistons::cleanupOrphan(cm, gx, gy, gz); return; }   // un bloque en movimiento que se guardo a medias
    if (isRepeaterBlock(t)) { repeaterCheck(gx, gy, gz, t); return; }   // por si un cambio quedo a medias al guardar
    updateTick(t, gx, gy, gz);
}

bool indirectPowerAt(const ChunkManager& cm, int x, int y, int z, int side) {
    W = &cm;
    return indirectProvidingPowerTo(x, y, z, side);
}

void notifyNeighbors(const ChunkManager& cm, int x, int y, int z, BlockType id) {
    Top top(cm);
    notify(x, y, z, id);
}

void lightOre(const ChunkManager& cm, int gx, int gy, int gz) {
    Top top(cm);
    if (T(gx, gy, gz) == BlockType::REDSTONE_ORE) setTM(gx, gy, gz, BlockType::REDSTONE_ORE_LIT, 0);
}

bool activate(const ChunkManager& cm, int gx, int gy, int gz) {
    Top top(cm);
    const BlockType t = T(gx, gy, gz);
    const int m = M(gx, gy, gz);
    if (t == BlockType::LEVER) {   // BlockLever.blockActivated
        const int v = m & 7;
        setM(gx, gy, gz, v + (8 - (m & 8)));
        Sound::clickCell(gx, gy, gz);
        notify(gx, gy, gz, t);
        notifyAttached(gx, gy, gz, v, t);
        return true;
    }
    if (t == BlockType::BUTTON) {   // BlockButton.blockActivated
        if (m & 8) return true;
        setM(gx, gy, gz, (m & 7) + 8);
        Sound::clickCell(gx, gy, gz);
        notify(gx, gy, gz, t);
        notifyAttached(gx, gy, gz, m & 7, t);
        schedule(gx, gy, gz, t, 20);
        return true;
    }
    if (isRepeaterBlock(t)) {   // BlockRedstoneRepeater.blockActivated: el retardo da la vuelta
        int d = (m & 12) >> 2;
        d = (d + 1) << 2 & 12;
        setM(gx, gy, gz, d | (m & 3));
        return true;
    }
    return false;
}

bool useItem(const ChunkManager& cm, BlockType item, int gx, int gy, int gz, BlockFace face, float yawDeg) {
    Top top(cm);
    int f = faceCode(face);
    if (f < 0) return false;
    int tx = gx, ty = gy, tz = gz;
    if (item != BlockType::REDSTONE && T(gx, gy, gz) == BlockType::SNOW_LAYER) {
        f = 1;   // ItemBlock: sobre una capa de nieve se pone en la misma celda
    } else {
        tx += DX[f]; ty += DY[f]; tz += DZ[f];
    }
    if (ty < 0 || ty >= SIZE_Y) return false;
    const BlockType here = T(tx, ty, tz);

    if (item == BlockType::REDSTONE) {   // ItemRedstone.onItemUse: solo en aire, sobre un bloque entero
        if (here != BlockType::AIR || !normal(tx, ty - 1, tz)) return false;
        return put(tx, ty, tz, BlockType::REDSTONE_WIRE, 0);
    }
    if (!freeCell(here)) return false;

    if (item == BlockType::LEVER) {   // BlockLever.canPlaceBlockOnSide / onBlockPlaced
        int meta = -1;
        if (f == 1 && normal(tx, ty - 1, tz)) meta = 5 + (rand() & 1);
        else if (f == 2 && normal(tx, ty, tz + 1)) meta = 4;
        else if (f == 3 && normal(tx, ty, tz - 1)) meta = 3;
        else if (f == 4 && normal(tx + 1, ty, tz)) meta = 2;
        else if (f == 5 && normal(tx - 1, ty, tz)) meta = 1;
        if (meta < 0) return false;
        return put(tx, ty, tz, BlockType::LEVER, meta);
    }
    if (item == BlockType::BUTTON) {   // BlockButton.canPlaceBlockOnSide / onBlockPlaced: solo en paredes
        int meta = -1;
        if (f == 2 && normal(tx, ty, tz + 1)) meta = 4;
        else if (f == 3 && normal(tx, ty, tz - 1)) meta = 3;
        else if (f == 4 && normal(tx + 1, ty, tz)) meta = 2;
        else if (f == 5 && normal(tx - 1, ty, tz)) meta = 1;
        if (meta < 0) return false;
        return put(tx, ty, tz, BlockType::BUTTON, meta);
    }
    if (isRailBlock(item)) {   // ItemBlock + BlockRail.canPlaceBlockAt: sobre un bloque entero (la forma la pone onBlockAdded)
        if (!normal(tx, ty - 1, tz)) return false;
        return put(tx, ty, tz, item, 0);
    }
    if (isPlateBlock(item)) {   // BlockPressurePlate.canPlaceBlockAt
        if (!normal(tx, ty - 1, tz)) return false;
        return put(tx, ty, tz, item, 0);
    }
    if (item == BlockType::TORCH_REDSTONE_ON) {   // BlockTorch.canPlaceBlockOnSide / onBlockPlaced
        int meta = -1;
        if (f == 1 && normal(tx, ty - 1, tz)) meta = 5;
        else if (f == 2 && normal(tx, ty, tz + 1)) meta = 4;
        else if (f == 3 && normal(tx, ty, tz - 1)) meta = 3;
        else if (f == 4 && normal(tx + 1, ty, tz)) meta = 2;
        else if (f == 5 && normal(tx - 1, ty, tz)) meta = 1;
        if (meta < 0) return false;
        return put(tx, ty, tz, BlockType::TORCH_REDSTONE_ON, meta);
    }
    if (item == BlockType::REPEATER_ITEM) {   // ItemReed + BlockRedstoneRepeater.onBlockPlacedBy
        if (!normal(tx, ty - 1, tz)) return false;
        const float yawB = 180.0f - yawDeg;   // el giro de Beta
        const int meta = (((int)floorf(yawB * 4.0f / 360.0f + 0.5f) & 3) + 2) % 4;
        if (!put(tx, ty, tz, BlockType::REPEATER_OFF, meta)) return false;
        if (repeaterInput(tx, ty, tz, meta)) schedule(tx, ty, tz, BlockType::REPEATER_OFF, 1);
        return true;
    }
    return false;
}

}
