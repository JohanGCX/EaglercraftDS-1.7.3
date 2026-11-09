#include "PartialBlocks.h"
#include "Trees.h"
#include "Sound.h"
#include "ChunkManager.h"
#include "Chunk.h"
#include "PendingDrops.h"
#include "WorldConfig.h"
#include "SignManager.h"
#include "Weather.h"
#include "Music.h"
#include <math.h>

// Coordenadas globales de celda: gx = chunkX * SIZE_X + xi, gy = yi, gz = chunkZ * SIZE_Z + zi (las mismas de LiquidWorld).
// La celda (gx, gy, gz) ocupa en el mundo X de gx - 4 a gx - 3, Y de gy - 1,5 a gy - 0,5 y Z de gz - 4 a gz - 3.

namespace {
    const int SIZE_X = WorldConfig::SIZE_X, SIZE_Y = WorldConfig::SIZE_Y, SIZE_Z = WorldConfig::SIZE_Z;

    int floorDivi(int v, int d) { return v >= 0 ? v / d : -((-v + d - 1) / d); }

    Chunk* chunkOf(const ChunkManager& cm, int gx, int gz, int& xi, int& zi) {
        const int cx = floorDivi(gx, SIZE_X), cz = floorDivi(gz, SIZE_Z);
        Chunk* c = cm.getChunkByChunkCoord(cx, cz);
        if (!c || !c->isGenerationComplete()) return nullptr;
        xi = gx - cx * SIZE_X;
        zi = gz - cz * SIZE_Z;
        return c;
    }

    // Lo que no esta cargado cuenta como piedra: ni se pone ni se rompe nada junto a el
    BlockType typeAt(const ChunkManager& cm, int gx, int gy, int gz) {
        if (gy < 0) return BlockType::STONE;
        if (gy >= SIZE_Y) return BlockType::AIR;
        int xi, zi; Chunk* c = chunkOf(cm, gx, gz, xi, zi);
        return c ? c->getBlockType(xi, gy, zi) : BlockType::STONE;
    }

    int metaAt(const ChunkManager& cm, int gx, int gy, int gz) {
        if (gy < 0 || gy >= SIZE_Y) return 0;
        int xi, zi; Chunk* c = chunkOf(cm, gx, gz, xi, zi);
        return c ? c->getLiquidMeta(xi, gy, zi) : 0;
    }

    bool put(const ChunkManager& cm, int gx, int gy, int gz, BlockType t, int meta) {
        if (gy < 0 || gy >= SIZE_Y) return false;
        int xi, zi; Chunk* c = chunkOf(cm, gx, gz, xi, zi);
        if (!c) return false;
        c->setBlockWithMeta(xi, gy, zi, t, meta, &cm);
        c->notifyLiquidNeighbors(xi, gy, zi, &cm);
        return true;
    }

    void remove(const ChunkManager& cm, int gx, int gy, int gz) {
        if (gy < 0 || gy >= SIZE_Y) return;
        int xi, zi; Chunk* c = chunkOf(cm, gx, gz, xi, zi);
        if (c) c->breakBlockAt(xi, gy, zi, &cm);
    }

    void dropAt(int gx, int gy, int gz, BlockType item) {
        PendingDrops::push((float)gx - SIZE_X / 2.0f + 0.5f, (float)gy - 1.0f, (float)gz - SIZE_Z / 2.0f + 0.5f, item);
    }

    // Block.canPlaceBlockAt: aire o algo que se reemplaza (material "ground cover" y lo que ItemBlock desplaza)
    bool replaceable(BlockType t) {
        return t == BlockType::AIR || t == BlockType::TALLGRASS || t == BlockType::DEAD_BUSH || t == BlockType::SNOW_LAYER || t == BlockType::FIRE;
    }

    bool normalCube(const ChunkManager& cm, int gx, int gy, int gz) { return isNormalCube(typeAt(cm, gx, gy, gz)); }

    // Material.isSolid (lo que piden el cartel y el pastel para apoyarse): bloques enteros, vidrio, hojas, cactus y los parciales que no son cartel
    bool solidMaterial(const ChunkManager& cm, int gx, int gy, int gz) {
        const BlockType t = typeAt(cm, gx, gy, gz);
        return isNormalCube(t) || t == BlockType::GLASS || t == BlockType::CACTUS || isLeavesBlock(t) || (isPartialBlock(t) && !isSignBlock(t) && t != BlockType::CROPS);
    }

    // BlockFence.canConnectFenceTo: otra valla o un bloque entero y opaco (menos la calabaza)
    bool fenceConnects(const ChunkManager& cm, int gx, int gy, int gz) {
        const BlockType t = typeAt(cm, gx, gy, gz);
        return t == BlockType::FENCE || (isNormalCube(t) && !isPumpkinBlock(t));
    }

    // Metadata de la valla: bit 0 -Z, 1 +Z, 2 -X, 3 +X
    int fenceMeta(const ChunkManager& cm, int gx, int gy, int gz) {
        return (fenceConnects(cm, gx, gy, gz - 1) ? 1 : 0) | (fenceConnects(cm, gx, gy, gz + 1) ? 2 : 0) |
               (fenceConnects(cm, gx - 1, gy, gz) ? 4 : 0) | (fenceConnects(cm, gx + 1, gy, gz) ? 8 : 0);
    }

    // World.getBlockLightValue: la luz del cielo menos la hora (noche) o la de bloque, la mayor
    int fullLight(const ChunkManager& cm, int gx, int gy, int gz, int& rawSky) {
        rawSky = 15;
        if (gy < 0 || gy >= SIZE_Y) return 15;
        int xi, zi; Chunk* c = chunkOf(cm, gx, gz, xi, zi);
        if (!c) return 15;
        rawSky = (int)c->getSkyLight(xi, gy, zi);
        int sky = rawSky - cm.getSkylightSubtract();
        if (sky < 0) sky = 0;
        const int bl = (int)c->getBlockLight(xi, gy, zi);
        return sky > bl ? sky : bl;
    }

    // BlockFlower.canBlockStay con la regla de BlockCrops: luz 8 o cielo abierto, y tierra de cultivo debajo
    bool cropCanStay(const ChunkManager& cm, int gx, int gy, int gz) {
        int rawSky;
        const int light = fullLight(cm, gx, gy, gz, rawSky);
        return (light >= 8 || rawSky >= 15) && typeAt(cm, gx, gy - 1, gz) == BlockType::FARMLAND;
    }

    void dropCrop(const ChunkManager& cm, int gx, int gy, int gz) {
        int wheat, seeds;
        Partial::cropDrops(metaAt(cm, gx, gy, gz), wheat, seeds);
        for (int k = 0; k < wheat; k++) dropAt(gx, gy, gz, BlockType::WHEAT);
        for (int k = 0; k < seeds; k++) dropAt(gx, gy, gz, BlockType::SEEDS);
    }

    // Un bloque parcial revisa si le queda su apoyo (BlockDoor, BlockBed y BlockTrapDoor .onNeighborBlockChange)
    void validate(const ChunkManager& cm, int gx, int gy, int gz) {
        const BlockType t = typeAt(cm, gx, gy, gz);
        const int meta = metaAt(cm, gx, gy, gz);
        if (isDoorBlock(t)) {
            if (meta & 8) {   // mitad de arriba: sin la de abajo no queda nada
                if (typeAt(cm, gx, gy - 1, gz) != t) remove(cm, gx, gy, gz);
            } else {
                const bool noUpper = typeAt(cm, gx, gy + 1, gz) != t;
                const bool noFloor = !normalCube(cm, gx, gy - 1, gz);
                if (noUpper || noFloor) {
                    remove(cm, gx, gy, gz);   // al faltar la de abajo, la de arriba se quita sola (sin soltar nada)
                    dropAt(gx, gy, gz, getDropType(t));
                }
            }
        } else if (t == BlockType::BED_BLOCK) {
            int dx, dz; Partial::bedOffset(meta & 3, dx, dz);
            if (meta & 8) { dx = -dx; dz = -dz; }
            if (typeAt(cm, gx + dx, gy, gz + dz) != t) {
                remove(cm, gx, gy, gz);
                if (!(meta & 8)) dropAt(gx, gy, gz, BlockType::BED_ITEM);   // BlockBed.idDropped: solo la cabecera
            }
        } else if (t == BlockType::SIGN_POST || t == BlockType::SIGN_WALL) {   // BlockSign.onNeighborBlockChange
            bool gone;
            if (t == BlockType::SIGN_POST) {
                gone = !solidMaterial(cm, gx, gy - 1, gz);
            } else {
                int ax = gx, az = gz;
                switch (meta) { case 2: az++; break; case 3: az--; break; case 4: ax++; break; default: ax--; break; }
                gone = !solidMaterial(cm, ax, gy, az);
            }
            if (gone) {
                remove(cm, gx, gy, gz);
                dropAt(gx, gy, gz, BlockType::SIGN_ITEM);
            }
        } else if (t == BlockType::CAKE_BLOCK) {   // BlockCake.canBlockStay: sin suelo solido se cae sin soltar nada
            if (!solidMaterial(cm, gx, gy - 1, gz)) remove(cm, gx, gy, gz);
        } else if (t == BlockType::CROPS) {   // BlockFlower.onNeighborBlockChange: sin su apoyo se cae y suelta lo suyo
            if (!cropCanStay(cm, gx, gy, gz)) {
                dropCrop(cm, gx, gy, gz);
                remove(cm, gx, gy, gz);
            }
        } else if (t == BlockType::FARMLAND) {   // BlockFarmland.onNeighborBlockChange: con un bloque solido encima vuelve a tierra
            const BlockType up = typeAt(cm, gx, gy + 1, gz);
            const bool upSolid = solidMaterial(cm, gx, gy + 1, gz) && up != BlockType::LADDER && (!isRedstoneComponent(up) || isPlateBlock(up));
            if (upSolid) put(cm, gx, gy, gz, BlockType::DIRT, 0);
        } else if (t == BlockType::LADDER) {   // BlockLadder.onNeighborBlockChange: sin la pared se cae y suelta el objeto
            int ax = gx, az = gz;
            switch (meta) { case 2: az++; break; case 3: az--; break; case 4: ax++; break; default: ax--; break; }
            if (!normalCube(cm, ax, gy, az)) {
                remove(cm, gx, gy, gz);
                dropAt(gx, gy, gz, BlockType::LADDER);
            }
        } else if (t == BlockType::FENCE) {   // une o suelta la union con los vecinos (BlockFence.canConnectFenceTo)
            const int want = fenceMeta(cm, gx, gy, gz);
            if (want != meta) {
                int xi, zi; Chunk* c = chunkOf(cm, gx, gz, xi, zi);
                if (c) c->setBlockMeta(xi, gy, zi, want, &cm);
            }
        } else if (t == BlockType::TRAPDOOR) {
            int ax = gx, az = gz;
            switch (meta & 3) { case 0: az++; break; case 1: az--; break; case 2: ax++; break; default: ax--; break; }
            if (!normalCube(cm, ax, gy, az)) {
                remove(cm, gx, gy, gz);
                dropAt(gx, gy, gz, BlockType::TRAPDOOR);
            }
        }
    }
}

namespace Partial {

bool activate(const ChunkManager& cm, int gx, int gy, int gz) {
    const BlockType t = typeAt(cm, gx, gy, gz);
    if (!isPartialBlock(t)) return false;
    if (isRedstoneComponent(t)) return Redstone::activate(cm, gx, gy, gz);   // palanca, boton y repetidor
    if (t == BlockType::DOOR_IRON || t == BlockType::BED_BLOCK || t == BlockType::CAKE_BLOCK) return true;   // la de hierro solo con redstone; la cama (dormir) la maneja main.cpp

    int xi, zi; Chunk* c = chunkOf(cm, gx, gz, xi, zi);
    if (!c) return true;
    const int meta = c->getLiquidMeta(xi, gy, zi);
    if (t == BlockType::TRAPDOOR) {
        c->setBlockMeta(xi, gy, zi, meta ^ 4, &cm);
        Sound::doorCell((meta & 4) == 0, gx, gy, gz);
        return true;
    }
    // BlockDoor.blockActivated: desde la mitad de arriba se actua sobre la de abajo
    if (meta & 8) {
        if (typeAt(cm, gx, gy - 1, gz) == t) return activate(cm, gx, gy - 1, gz);
        return true;
    }
    if (typeAt(cm, gx, gy + 1, gz) == t) {
        int uxi, uzi; Chunk* uc = chunkOf(cm, gx, gz, uxi, uzi);
        if (uc) uc->setBlockMeta(uxi, gy + 1, uzi, (meta ^ 4) + 8, &cm);
    }
    c->setBlockMeta(xi, gy, zi, meta ^ 4, &cm);
    Sound::doorCell((meta & 4) == 0, gx, gy, gz);
    return true;
}

bool useItem(const ChunkManager& cm, BlockType item, int gx, int gy, int gz, BlockFace face, float yawDeg) {
    // Player: adelante = (-sen yaw, -cos yaw); Beta: (-sen yaw, +cos yaw). Las formulas de abajo son las de Beta
    const float yawB = 180.0f - yawDeg;

    if (item == BlockType::DOOR_WOOD_ITEM || item == BlockType::DOOR_IRON_ITEM) {   // ItemDoor.onItemUse
        if (face != BlockFace::TOP) return false;
        const BlockType door = (item == BlockType::DOOR_WOOD_ITEM) ? BlockType::DOOR_WOOD : BlockType::DOOR_IRON;
        const int y = gy + 1;
        if (y >= SIZE_Y - 1) return false;
        if (!normalCube(cm, gx, gy, gz)) return false;
        if (!replaceable(typeAt(cm, gx, y, gz)) || !replaceable(typeAt(cm, gx, y + 1, gz))) return false;

        int dir = (int)floorf((yawB + 180.0f) * 4.0f / 360.0f - 0.5f) & 3;
        int dx, dz; bedOffset(dir, dx, dz);   // misma tabla (0,1) (-1,0) (0,-1) (1,0)
        const int left  = (normalCube(cm, gx - dx, y, gz - dz) ? 1 : 0) + (normalCube(cm, gx - dx, y + 1, gz - dz) ? 1 : 0);
        const int right = (normalCube(cm, gx + dx, y, gz + dz) ? 1 : 0) + (normalCube(cm, gx + dx, y + 1, gz + dz) ? 1 : 0);
        const bool doorLeft  = typeAt(cm, gx - dx, y, gz - dz) == door || typeAt(cm, gx - dx, y + 1, gz - dz) == door;
        const bool doorRight = typeAt(cm, gx + dx, y, gz + dz) == door || typeAt(cm, gx + dx, y + 1, gz + dz) == door;
        bool mirror = false;
        if (doorLeft && !doorRight) mirror = true;
        else if (right > left) mirror = true;
        if (mirror) dir = ((dir - 1) & 3) + 4;

        put(cm, gx, y, gz, door, dir);
        put(cm, gx, y + 1, gz, door, dir + 8);
        return true;
    }

    if (item == BlockType::BED_ITEM) {   // ItemBed.onItemUse
        if (face != BlockFace::TOP) return false;
        const int y = gy + 1;
        if (y >= SIZE_Y) return false;
        const int dir = (int)floorf(yawB * 4.0f / 360.0f + 0.5f) & 3;
        int dx, dz; bedOffset(dir, dx, dz);
        if (typeAt(cm, gx, y, gz) != BlockType::AIR || typeAt(cm, gx + dx, y, gz + dz) != BlockType::AIR) return false;
        if (!normalCube(cm, gx, gy, gz) || !normalCube(cm, gx + dx, gy, gz + dz)) return false;
        put(cm, gx, y, gz, BlockType::BED_BLOCK, dir);
        put(cm, gx + dx, y, gz + dz, BlockType::BED_BLOCK, dir + 8);
        return true;
    }

    if (item == BlockType::SIGN_ITEM) {   // ItemSign.onItemUse
        if (face == BlockFace::BOTTOM) return false;
        if (!solidMaterial(cm, gx, gy, gz)) return false;
        int tx = gx, ty = gy, tz = gz;
        switch (face) {
            case BlockFace::TOP:   ty++; break;
            case BlockFace::NORTH: tz--; break;
            case BlockFace::SOUTH: tz++; break;
            case BlockFace::WEST:  tx--; break;
            default:               tx++; break;
        }
        if (ty >= SIZE_Y || !replaceable(typeAt(cm, tx, ty, tz))) return false;
        if (face == BlockFace::TOP) {
            const int meta = (int)floorf((yawB + 180.0f) * 16.0f / 360.0f + 0.5f) & 15;
            SignManager::place(tx, ty, tz);   // antes de poner el bloque, para que la malla no lo dibuje con tablones
            return put(cm, tx, ty, tz, BlockType::SIGN_POST, meta);
        }
        int meta = 2;   // la cara de Beta: 2 = -Z, 3 = +Z, 4 = -X, 5 = +X
        switch (face) { case BlockFace::NORTH: meta = 2; break; case BlockFace::SOUTH: meta = 3; break; case BlockFace::WEST: meta = 4; break; default: meta = 5; break; }
        SignManager::place(tx, ty, tz);
        return put(cm, tx, ty, tz, BlockType::SIGN_WALL, meta);
    }

    if (item == BlockType::CAKE_ITEM) {   // ItemReed.onItemUse con BlockCake: en la celda de la cara apuntada (la misma si es nieve)
        int tx = gx, ty = gy, tz = gz;
        if (typeAt(cm, gx, gy, gz) != BlockType::SNOW_LAYER) {
            switch (face) {
                case BlockFace::BOTTOM: ty--; break;
                case BlockFace::TOP:    ty++; break;
                case BlockFace::NORTH:  tz--; break;
                case BlockFace::SOUTH:  tz++; break;
                case BlockFace::WEST:   tx--; break;
                default:                tx++; break;
            }
        }
        if (ty < 0 || ty >= SIZE_Y || !replaceable(typeAt(cm, tx, ty, tz))) return false;
        if (!solidMaterial(cm, tx, ty - 1, tz)) return false;   // canBlockStay
        return put(cm, tx, ty, tz, BlockType::CAKE_BLOCK, 0);
    }

    if (isStructureBlock(item)) {   // losas, escaleras, escalera de mano y valla
        // ItemSlab.onItemUse: una losa sobre la cara de arriba de otra igual la vuelve doble
        if (isSlabBlock(item) && face == BlockFace::TOP && typeAt(cm, gx, gy, gz) == item) {
            return put(cm, gx, gy, gz, doubleOfSlab(item), 0);
        }
        int tx = gx, ty = gy, tz = gz;
        if (typeAt(cm, gx, gy, gz) != BlockType::SNOW_LAYER) {   // la nieve se reemplaza en el lugar
            switch (face) {
                case BlockFace::BOTTOM: ty--; break;
                case BlockFace::TOP:    ty++; break;
                case BlockFace::NORTH:  tz--; break;
                case BlockFace::SOUTH:  tz++; break;
                case BlockFace::WEST:   tx--; break;
                default:                tx++; break;
            }
        }
        if (ty < 0 || ty >= SIZE_Y) return false;
        const BlockType here = typeAt(cm, tx, ty, tz);
        if (!(replaceable(here) || isLiquidBlock(here))) return false;

        int meta = 0;
        if (isStairsBlock(item)) {   // BlockStairs.onBlockPlacedBy: sube hacia donde mira el jugador
            const int v = (int)floorf(yawB * 4.0f / 360.0f + 0.5f) & 3;
            meta = (v == 0) ? 2 : (v == 1) ? 1 : (v == 2) ? 3 : 0;
        } else if (item == BlockType::LADDER) {   // BlockLadder.canPlaceBlockOnSide: solo en la cara lateral de un bloque entero
            switch (face) {
                case BlockFace::NORTH: meta = 2; break;
                case BlockFace::SOUTH: meta = 3; break;
                case BlockFace::WEST:  meta = 4; break;
                case BlockFace::EAST:  meta = 5; break;
                default: return false;
            }
            int ax = tx, az = tz;
            switch (meta) { case 2: az++; break; case 3: az--; break; case 4: ax++; break; default: ax--; break; }
            if (!normalCube(cm, ax, ty, az)) return false;
        } else if (item == BlockType::FENCE) {   // BlockFence.canPlaceBlockAt: sobre otra valla o sobre un material solido
            const BlockType below = typeAt(cm, tx, ty - 1, tz);
            if (below != BlockType::FENCE && !solidMaterial(cm, tx, ty - 1, tz)) return false;
            meta = fenceMeta(cm, tx, ty, tz);
        }
        if (!put(cm, tx, ty, tz, item, meta)) return false;
        // BlockStep.onBlockAdded: una losa puesta justo encima de otra igual se funde con ella en una doble
        if (isSlabBlock(item) && typeAt(cm, tx, ty - 1, tz) == item) {
            remove(cm, tx, ty, tz);
            put(cm, tx, ty - 1, tz, doubleOfSlab(item), 0);
        }
        neighborsChanged(cm, tx, ty, tz);   // las vallas de alrededor se unen a la nueva
        return true;
    }

    if (item == BlockType::TRAPDOOR) {   // ItemBlock + BlockTrapDoor.canPlaceBlockOnSide / onBlockPlaced: solo en las caras laterales
        int tx = gx, tz = gz, meta = 0;
        switch (face) {
            case BlockFace::NORTH: tz--; meta = 0; break;
            case BlockFace::SOUTH: tz++; meta = 1; break;
            case BlockFace::WEST:  tx--; meta = 2; break;
            case BlockFace::EAST:  tx++; meta = 3; break;
            default: return false;
        }
        if (!normalCube(cm, gx, gy, gz)) return false;   // el bloque de apoyo es el apuntado
        const BlockType here = typeAt(cm, tx, gy, tz);
        if (!(replaceable(here) || isLiquidBlock(here))) return false;
        return put(cm, tx, gy, tz, BlockType::TRAPDOOR, meta);
    }
    return false;
}

bool useHoe(const ChunkManager& cm, int gx, int gy, int gz, BlockFace face) {
    const BlockType t = typeAt(cm, gx, gy, gz);
    const BlockType up = typeAt(cm, gx, gy + 1, gz);
    if ((face == BlockFace::BOTTOM || up != BlockType::AIR || t != BlockType::GRASS) && t != BlockType::DIRT) return false;
    return put(cm, gx, gy, gz, BlockType::FARMLAND, 0);
}

void farmlandTick(const ChunkManager& cm, int gx, int gy, int gz, int roll) {
    if (roll != 0) return;   // rand.nextInt(5) == 0 (el que llama pasa el resultado)
    int xi, zi; Chunk* c = chunkOf(cm, gx, gz, xi, zi);
    if (!c || c->getBlockType(xi, gy, zi) != BlockType::FARMLAND) return;
    bool water = Weather::canRainAtGrid(gx, gy + 1, gz);   // BlockFarmland: la lluvia moja la tierra; si no, isWaterNearby: agua en la capa de la tierra y la de encima, a 4 de distancia
    for (int x = gx - 4; x <= gx + 4 && !water; x++)
        for (int y = gy; y <= gy + 1 && !water; y++)
            for (int z = gz - 4; z <= gz + 4; z++)
                if (isWaterBlock(typeAt(cm, x, y, z))) { water = true; break; }
    const int meta = c->getLiquidMeta(xi, gy, zi);
    if (!water) {
        if (meta > 0) c->setBlockMeta(xi, gy, zi, meta - 1, &cm);
        else if (typeAt(cm, gx, gy + 1, gz) != BlockType::CROPS) put(cm, gx, gy, gz, BlockType::DIRT, 0);   // isCropsNearby: con un cultivo encima sigue seca
    } else if (meta != 7) {
        c->setBlockMeta(xi, gy, zi, 7, &cm);
    }
}

bool useSeeds(const ChunkManager& cm, int gx, int gy, int gz, BlockFace face) {
    if (face != BlockFace::TOP) return false;
    if (typeAt(cm, gx, gy, gz) != BlockType::FARMLAND || typeAt(cm, gx, gy + 1, gz) != BlockType::AIR) return false;
    return put(cm, gx, gy + 1, gz, BlockType::CROPS, 0);
}

BlockType jukeboxRecord(const ChunkManager& cm, int gx, int gy, int gz) {
    if (typeAt(cm, gx, gy, gz) != BlockType::JUKEBOX) return BlockType::AIR;
    const int m = metaAt(cm, gx, gy, gz);
    return m == 1 ? BlockType::RECORD_13 : (m == 2 ? BlockType::RECORD_CAT : BlockType::AIR);
}

bool jukeboxInsert(const ChunkManager& cm, int gx, int gy, int gz, BlockType record) {
    if (typeAt(cm, gx, gy, gz) != BlockType::JUKEBOX || metaAt(cm, gx, gy, gz) != 0 || !isRecord(record)) return false;
    put(cm, gx, gy, gz, BlockType::JUKEBOX, record == BlockType::RECORD_13 ? 1 : 2);
    // World.playRecord: suena desde el centro del bloque
    Music::playRecord(record == BlockType::RECORD_13 ? "13" : "cat", (float)gx - SIZE_X / 2.0f + 0.5f, (float)gy - 1.0f, (float)gz - SIZE_Z / 2.0f + 0.5f, gx, gy, gz);
    return true;
}

BlockType jukeboxEject(const ChunkManager& cm, int gx, int gy, int gz) {
    const BlockType r = jukeboxRecord(cm, gx, gy, gz);
    if (r != BlockType::AIR) {
        put(cm, gx, gy, gz, BlockType::JUKEBOX, 0);
        Music::stopRecordAt(gx, gy, gz);   // BlockJukeBox.ejectRecord: playRecord(null) calla el disco
    }
    return r;
}

bool useBoneMeal(const ChunkManager& cm, int gx, int gy, int gz) {
    const BlockType t = typeAt(cm, gx, gy, gz);
    if (t == BlockType::CROPS) {
        put(cm, gx, gy, gz, BlockType::CROPS, 7);   // BlockCrops.fertilize (Beta 1.7.3): la etapa pasa directo a 7 (maduro)
        return true;
    }
    if (isSapling(t)) {   // BlockSapling.fertilize: crece al instante
        Trees::grow(cm, gx, gy, gz);
        return true;
    }
    if (t == BlockType::GRASS) {
        for (int n = 0; n < 128; n++) {
            int x = gx, y = gy + 1, z = gz;
            bool ok = true;
            for (int q = 0; q < n / 16; q++) {
                x += rand() % 3 - 1;
                y += (rand() % 3 - 1) * (rand() % 3) / 2;
                z += rand() % 3 - 1;
                if (typeAt(cm, x, y - 1, z) != BlockType::GRASS || isNormalCube(typeAt(cm, x, y, z))) { ok = false; break; }
            }
            if (!ok || typeAt(cm, x, y, z) != BlockType::AIR) continue;
            if (rand() % 10 != 0) put(cm, x, y, z, BlockType::TALLGRASS, 0);
            else if (rand() % 3 != 0) put(cm, x, y, z, BlockType::DANDELION, 0);
            else put(cm, x, y, z, BlockType::ROSE, 0);
        }
        return true;
    }
    return false;
}

void cropsTick(const ChunkManager& cm, int gx, int gy, int gz) {
    int xi, zi; Chunk* c = chunkOf(cm, gx, gz, xi, zi);
    if (!c || c->getBlockType(xi, gy, zi) != BlockType::CROPS) return;
    if (!cropCanStay(cm, gx, gy, gz)) { dropCrop(cm, gx, gy, gz); remove(cm, gx, gy, gz); return; }
    int rawSky;
    if (fullLight(cm, gx, gy + 1, gz, rawSky) < 9) return;
    int meta = c->getLiquidMeta(xi, gy, zi);
    if (meta >= 7) return;
    // BlockCrops.getGrowthRate: 1 + lo que aportan las 9 celdas de abajo (tierra de cultivo 1, mojada 3; las de alrededor, la cuarta parte);
    // se divide entre 2 si hay cultivos en diagonal o en fila en los dos ejes
    float rate = 1.0f;
    const bool west = typeAt(cm, gx - 1, gy, gz) == BlockType::CROPS, east = typeAt(cm, gx + 1, gy, gz) == BlockType::CROPS;
    const bool north = typeAt(cm, gx, gy, gz - 1) == BlockType::CROPS, south = typeAt(cm, gx, gy, gz + 1) == BlockType::CROPS;
    const bool diag = typeAt(cm, gx - 1, gy, gz - 1) == BlockType::CROPS || typeAt(cm, gx + 1, gy, gz - 1) == BlockType::CROPS ||
                      typeAt(cm, gx + 1, gy, gz + 1) == BlockType::CROPS || typeAt(cm, gx - 1, gy, gz + 1) == BlockType::CROPS;
    for (int x = gx - 1; x <= gx + 1; x++)
        for (int z = gz - 1; z <= gz + 1; z++) {
            float v = 0.0f;
            if (typeAt(cm, x, gy - 1, z) == BlockType::FARMLAND) { v = 1.0f; if (metaAt(cm, x, gy - 1, z) > 0) v = 3.0f; }
            if (x != gx || z != gz) v /= 4.0f;
            rate += v;
        }
    if (diag || ((west || east) && (north || south))) rate /= 2.0f;
    int n = (int)(100.0f / rate);
    if (n < 1) n = 1;
    if (rand() % n == 0) c->setBlockMeta(xi, gy, zi, meta + 1, &cm);
}

void mobStep(const ChunkManager& cm, int gx, int gy, int gz) {
    if (typeAt(cm, gx, gy, gz) == BlockType::FARMLAND && (rand() & 3) == 0) put(cm, gx, gy, gz, BlockType::DIRT, 0);
}

void trample(const ChunkManager& cm, int gx, int gy, int gz) {
    if (typeAt(cm, gx, gy, gz) == BlockType::FARMLAND) put(cm, gx, gy, gz, BlockType::DIRT, 0);
}

bool eatCake(const ChunkManager& cm, int gx, int gy, int gz) {
    int xi, zi; Chunk* c = chunkOf(cm, gx, gz, xi, zi);
    if (!c || c->getBlockType(xi, gy, zi) != BlockType::CAKE_BLOCK) return false;
    const int m = c->getLiquidMeta(xi, gy, zi) + 1;
    if (m >= 6) remove(cm, gx, gy, gz);
    else c->setBlockMeta(xi, gy, zi, m, &cm);
    return true;
}

void neighborsChanged(const ChunkManager& cm, int gx, int gy, int gz) {
    static const int D[6][3] = { {-1, 0, 0}, {1, 0, 0}, {0, -1, 0}, {0, 1, 0}, {0, 0, -1}, {0, 0, 1} };
    for (int i = 0; i < 6; i++) {
        const int nx = gx + D[i][0], ny = gy + D[i][1], nz = gz + D[i][2];
        if (ny < 0 || ny >= SIZE_Y) continue;
        if (isPartialBlock(typeAt(cm, nx, ny, nz))) validate(cm, nx, ny, nz);
    }
}

bool boxHit(const ChunkManager& cm, float minX, float minY, float minZ, float maxX, float maxY, float maxZ, const float* cur) {
    const int x0 = (int)floorf(minX + 4.0f), x1 = (int)floorf(maxX + 4.0f);
    const int y0 = (int)floorf(minY + 1.5f) - 1, y1 = (int)floorf(maxY + 1.5f);   // una fila mas abajo: la valla llega a 1,5
    const int z0 = (int)floorf(minZ + 4.0f), z1 = (int)floorf(maxZ + 4.0f);
    for (int x = x0; x <= x1; x++)
        for (int y = y0; y <= y1; y++)
            for (int z = z0; z <= z1; z++) {
                if (y < 0 || y >= SIZE_Y) continue;
                int xi, zi; Chunk* c = chunkOf(cm, x, z, xi, zi);
                if (!c) continue;
                const BlockType t = c->getBlockType(xi, y, zi);
                if (!isPartialBlock(t)) continue;
                if (!hasCollision(t)) continue;
                float boxes[9][6]; const int nb = collisionBoxes(t, c->getLiquidMeta(xi, y, zi), boxes);
                const float cx = (float)x - 4.0f, cy = (float)y - 1.5f, cz = (float)z - 4.0f;
                for (int k = 0; k < nb; k++) {
                    const float* bd = boxes[k];
                    if (maxX > cx + bd[0] && minX < cx + bd[3] &&
                        maxY > cy + bd[1] && minY < cy + bd[4] &&
                        maxZ > cz + bd[2] && minZ < cz + bd[5]) {
                        // Si el jugador ya esta metido en este bloque (cerro la puerta encima), se puede mover dentro de el: no lo frena
                        if (cur && cur[3] > cx + bd[0] && cur[0] < cx + bd[3] &&
                            cur[4] > cy + bd[1] && cur[1] < cy + bd[4] &&
                            cur[5] > cz + bd[2] && cur[2] < cz + bd[5]) continue;
                        return true;
                    }
                }
            }
    return false;
}

bool rayVsBox(const float bd[6], const float s[3], const float e[3], BlockFace& face, float hit[3]) {
    float tMin = 0.0f, tMax = 1.0f;
    int axisIn = -1; bool positiveIn = true;
    for (int i = 0; i < 3; i++) {
        const float d = e[i] - s[i];
        if (fabsf(d) < 1.0e-7f) {
            if (s[i] < bd[i] || s[i] > bd[i + 3]) return false;
            continue;
        }
        float t1 = (bd[i] - s[i]) / d, t2 = (bd[i + 3] - s[i]) / d;
        const bool pos = d > 0.0f;
        if (t1 > t2) { const float tmp = t1; t1 = t2; t2 = tmp; }
        if (t1 > tMin) { tMin = t1; axisIn = i; positiveIn = pos; }
        if (t2 < tMax) tMax = t2;
        if (tMin > tMax) return false;
    }
    for (int i = 0; i < 3; i++) hit[i] = s[i] + (e[i] - s[i]) * tMin;
    switch (axisIn) {
        case 0:  face = positiveIn ? BlockFace::WEST : BlockFace::EAST; break;
        case 1:  face = positiveIn ? BlockFace::BOTTOM : BlockFace::TOP; break;
        case 2:  face = positiveIn ? BlockFace::NORTH : BlockFace::SOUTH; break;
        default: face = BlockFace::TOP; break;   // el rayo ya empieza dentro de la caja
    }
    return true;
}

}
