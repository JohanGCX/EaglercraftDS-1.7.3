#ifndef PARTIALBLOCKS_H
#define PARTIALBLOCKS_H

// Bloques que no llenan la celda: puertas, trampilla y cama (BlockDoor, BlockTrapDoor y BlockBed de Beta 1.7.3).
// La forma y la orientacion salen de la metadata de 4 bits de la celda (la misma tabla que guarda el fuego y los liquidos: Chunk::getLiquidMeta).
//   Puerta:    bits 0-1 direccion, bit 2 "abierta" (el 2 de la formula de BlockDoor.getState), bit 3 mitad de arriba
//   Trampilla: bits 0-1 lado donde se apoya, bit 2 abierta
//   Cama:      bits 0-1 direccion, bit 3 segunda mitad (la que se pone lejos del jugador)

#include <nds.h>
#include <stdlib.h>
#include <math.h>
#include "BlockType.h"
#include "BlockFace.h"
#include "Redstone.h"
#include "Pistons.h"

class ChunkManager;

namespace Partial {

    // BlockDoor.getState: posicion de la hoja dentro de la celda (0 = pegada a -Z, 1 = a +X, 2 = a +Z, 3 = a -X)
    inline int doorState(int meta) { return (meta & 4) == 0 ? ((meta - 1) & 3) : (meta & 3); }

    // ---- Losas, escaleras, escalera de mano y valla ----
    // Valla: la metadata dice con que lados se une (bit 0 -Z, 1 +Z, 2 -X, 3 +X). Union de poste y travesanos: BlockFence.setBlockBoundsBasedOnState
    inline void fenceBox(int meta, float b[6]) {
        b[0] = (meta & 4) ? 0.0f : 6.0f / 16.0f;  b[3] = (meta & 8) ? 1.0f : 10.0f / 16.0f;
        b[2] = (meta & 1) ? 0.0f : 6.0f / 16.0f;  b[5] = (meta & 2) ? 1.0f : 10.0f / 16.0f;
        b[1] = 0.0f; b[4] = 1.0f;
    }

    // Caja de seleccion. La escalera (BlockStairs) se selecciona entera, como en Beta 1.7.3; la losa es media celda
    inline void structureBounds(BlockType t, int meta, float b[6]) {
        b[0] = 0.0f; b[1] = 0.0f; b[2] = 0.0f; b[3] = 1.0f; b[4] = 1.0f; b[5] = 1.0f;
        if (isSlabBlock(t)) {
            b[4] = 0.5f;
        } else if (t == BlockType::FENCE) {
            fenceBox(meta, b);
        } else if (t == BlockType::LADDER) {   // BlockLadder.setBlockBoundsBasedOnState: 2/16 pegado a la pared (meta 2 = +Z, 3 = -Z, 4 = +X, 5 = -X)
            const float w = 2.0f / 16.0f;
            switch (meta) {
                case 2: b[2] = 1.0f - w; break;
                case 3: b[5] = w; break;
                case 4: b[0] = 1.0f - w; break;
                default: b[3] = w; break;
            }
        }
    }

    // Cajas con las que se DIBUJA y se choca (hasta 9). Devuelve cuantas. Escalera: la mitad de abajo y el escalon (BlockStairs.getCollidingBoundingBoxes)
    inline int shapeBoxes(BlockType t, int meta, float out[][6]) {
        if (isStairsBlock(t)) {
            out[0][0] = 0.0f; out[0][1] = 0.0f; out[0][2] = 0.0f; out[0][3] = 1.0f; out[0][4] = 0.5f; out[0][5] = 1.0f;
            out[1][0] = 0.0f; out[1][1] = 0.5f; out[1][2] = 0.0f; out[1][3] = 1.0f; out[1][4] = 1.0f; out[1][5] = 1.0f;
            switch (meta & 3) {
                case 0: out[1][0] = 0.5f; break;   // sube hacia +X
                case 1: out[1][3] = 0.5f; break;   // -X
                case 2: out[1][2] = 0.5f; break;   // +Z
                default: out[1][5] = 0.5f; break;  // -Z
            }
            return 2;
        }
        if (t == BlockType::FENCE) {   // RenderBlocks.renderBlockFence: poste de 4/16 y dos travesanos de 2/16 por lado unido
            const float p0 = 6.0f / 16.0f, p1 = 10.0f / 16.0f, a0 = 7.0f / 16.0f, a1 = 9.0f / 16.0f;
            const float yLow0 = 6.0f / 16.0f, yLow1 = 9.0f / 16.0f, yHi0 = 12.0f / 16.0f, yHi1 = 15.0f / 16.0f;
            int n = 0;
            out[n][0] = p0; out[n][1] = 0.0f; out[n][2] = p0; out[n][3] = p1; out[n][4] = 1.0f; out[n][5] = p1; n++;
            for (int d = 0; d < 4; d++) {
                if (!(meta & (1 << d))) continue;
                float x0, x1, z0, z1;
                switch (d) {
                    case 0: x0 = a0; x1 = a1; z0 = 0.0f; z1 = p0; break;     // -Z
                    case 1: x0 = a0; x1 = a1; z0 = p1; z1 = 1.0f; break;     // +Z
                    case 2: x0 = 0.0f; x1 = p0; z0 = a0; z1 = a1; break;     // -X
                    default: x0 = p1; x1 = 1.0f; z0 = a0; z1 = a1; break;    // +X
                }
                for (int k = 0; k < 2; k++) {
                    out[n][0] = x0; out[n][1] = k ? yHi0 : yLow0; out[n][2] = z0;
                    out[n][3] = x1; out[n][4] = k ? yHi1 : yLow1; out[n][5] = z1; n++;
                }
            }
            return n;
        }
        structureBounds(t, meta, out[0]);
        return 1;
    }

    // Caja del bloque dentro de la celda, de 0 a 1: {minX, minY, minZ, maxX, maxY, maxZ}
    inline void bounds(BlockType t, int meta, float b[6]) {
        const float th = 3.0f / 16.0f;
        b[0] = 0.0f; b[1] = 0.0f; b[2] = 0.0f; b[3] = 1.0f; b[4] = 1.0f; b[5] = 1.0f;
        if (t == BlockType::SNOW_LAYER) { b[4] = 2.0f / 16.0f; return; }
        if (isStructureBlock(t)) { structureBounds(t, meta, b); return; }   // capa de nieve: 2/16 de alto (solo para seleccion; no es un bloque parcial)
        if (isRedstoneComponent(t)) { Redstone::bounds(t, meta, b); return; }
        if (isPistonBlock(t)) { Pistons::bounds(t, meta, b); return; }
        if (isDoorBlock(t)) {   // BlockDoor.setDoorRotation
            switch (doorState(meta)) {
                case 0: b[5] = th; break;
                case 1: b[0] = 1.0f - th; break;
                case 2: b[2] = 1.0f - th; break;
                default: b[3] = th; break;
            }
        } else if (t == BlockType::TRAPDOOR) {   // BlockTrapDoor.setBlockBoundsForBlockRender
            b[4] = th;
            if (meta & 4) {
                b[4] = 1.0f;
                switch (meta & 3) {
                    case 0: b[2] = 1.0f - th; break;
                    case 1: b[5] = th; break;
                    case 2: b[0] = 1.0f - th; break;
                    default: b[3] = th; break;
                }
            }
        } else if (t == BlockType::BED_BLOCK) {
            b[4] = 9.0f / 16.0f;
        } else if (t == BlockType::FARMLAND) {   // BlockFarmland: setBlockBounds(0, 0, 0, 1, 15/16, 1)
            b[4] = 15.0f / 16.0f;
        } else if (t == BlockType::CROPS) {   // BlockCrops: setBlockBounds(0, 0, 0, 1, 0,25, 1)
            b[4] = 0.25f;
        } else if (t == BlockType::CAKE_BLOCK) {   // BlockCake.getSelectedBoundingBoxFromPool: cada porcion comida recorta 2/16 del lado -X
            b[0] = (float)(1 + (meta > 5 ? 5 : meta) * 2) / 16.0f; b[2] = 1.0f / 16.0f; b[3] = 1.0f - 1.0f / 16.0f; b[5] = 1.0f - 1.0f / 16.0f;
            b[4] = 0.5f;
        } else if (t == BlockType::SIGN_POST) {   // BlockSign: un poste de 0,5 x 0,5 (la seleccion; el dibujo es el modelo de SignModel)
            b[0] = 0.25f; b[2] = 0.25f; b[3] = 0.75f; b[5] = 0.75f;
        } else if (t == BlockType::SIGN_WALL) {   // BlockSign.setBlockBoundsBasedOnState: 2/16 de grueso, de 9/32 a 25/32 de alto
            b[1] = 9.0f / 32.0f; b[4] = 25.0f / 32.0f;
            switch (meta) {
                case 2: b[2] = 1.0f - 2.0f / 16.0f; break;
                case 3: b[5] = 2.0f / 16.0f; break;
                case 4: b[0] = 1.0f - 2.0f / 16.0f; break;
                default: b[3] = 2.0f / 16.0f; break;
            }
        }
    }

    // Caja de colision. Los carteles no estorban (getCollisionBoundingBoxFromPool = null); el pastel llega 1/16 mas bajo que su seleccion
    inline bool hasCollision(BlockType t) { return !isSignBlock(t) && t != BlockType::CROPS && t != BlockType::LADDER && t != BlockType::PISTON_MOVING && (!isRedstoneComponent(t) || isRepeaterBlock(t)); }   // de la redstone solo estorba el repetidor
    inline void collisionBounds(BlockType t, int meta, float b[6]) {
        bounds(t, meta, b);
        if (t == BlockType::CAKE_BLOCK) b[4] = 0.5f - 1.0f / 16.0f;
        if (t == BlockType::FARMLAND) b[4] = 1.0f;   // BlockFarmland.getCollisionBoundingBoxFromPool: la celda entera (se ve y se apunta a 15/16, pero se pisa a 1)
        if (isPistonBase(t)) { b[0] = b[1] = b[2] = 0.0f; b[3] = b[4] = b[5] = 1.0f; }   // BlockPistonBase.getCollidingBoundingBoxes: siempre la celda entera
    }

    // Cajas de choque de un bloque parcial (la escalera tiene dos; la valla, una de 1,5 de alto)
    inline int collisionBoxes(BlockType t, int meta, float out[][6]) {
        if (isStairsBlock(t)) return shapeBoxes(t, meta, out);
        collisionBounds(t, meta, out[0]);
        if (t == BlockType::FENCE) out[0][4] = 1.5f;
        return 1;
    }

    // Desplazamiento (x, z) de la cabecera hacia la otra mitad: BlockBed.headBlockToFootBlockMap
    inline void bedOffset(int dir, int& dx, int& dz) {
        static const int OFF[4][2] = { {0, 1}, {-1, 0}, {0, -1}, {1, 0} };
        dx = OFF[dir & 3][0]; dz = OFF[dir & 3][1];
    }

    // ---- Texturas (terrain.png) ----
    // Puerta, BlockDoor.getBlockTextureFromSideAndMetadata. face: 0 abajo, 1 arriba, 2 norte (-Z), 3 sur (+Z), 4 oeste (-X), 5 este (+X)
    inline TextureCell doorCell(BlockType t, int meta, int face, bool& flip) {
        flip = false;
        const int base = (t == BlockType::DOOR_IRON) ? 98 : 97;   // mitad de abajo; la de arriba esta una fila mas arriba
        TextureCell c;
        int idx = base;
        if (face >= 2) {
            const int state = doorState(meta);
            const bool wide = ((state == 0 || state == 2) ? 1 : 0) ^ ((face <= 3) ? 1 : 0);
            if (!wide) {   // cara ancha (la que lleva la imagen de la puerta)
                int v = state / 2 + (((face & 1) ^ state));
                v += (meta & 4) / 4;
                idx = base - (meta & 8) * 2;
                flip = (v & 1) != 0;
            }
        }
        c.col = idx % 16; c.row = idx / 16;
        return c;
    }

    // Cama: cara de la celda y si va espejada. skip = la cara que da a la otra mitad no se dibuja
    inline TextureCell bedCell(int meta, int face, bool& flip, bool& skip) {
        flip = false; skip = false;
        const bool second = (meta & 8) != 0;
        const int dir = meta & 3;
        TextureCell c;
        if (face == 0) { c.col = 4; c.row = 0; return c; }                       // abajo: tablones
        if (face == 1) { c.col = second ? 7 : 6; c.row = 8; return c; }          // arriba (134 / 135)
        int px, pz; bedOffset(dir, px, pz);
        if (second) { px = -px; pz = -pz; }                                       // hacia donde queda la otra mitad
        int fx = 0, fz = 0;
        switch (face) { case 2: fz = -1; break; case 3: fz = 1; break; case 4: fx = -1; break; default: fx = 1; break; }
        if (fx == px && fz == pz) { skip = true; c.col = 6; c.row = 9; return c; }       // la que toca a la otra mitad
        if (fx == -px && fz == -pz) { c.col = second ? 8 : 5; c.row = 9; return c; }      // el extremo de la cama
        c.col = second ? 7 : 6; c.row = 9;                                                 // los costados
        static const int FLIP_FACE[4] = { 5, 3, 4, 2 };                                    // RenderBlocks.renderBlockBed: var65
        flip = (face == FLIP_FACE[dir]);
        return c;
    }

    // UV de la cara de arriba de la cama segun la direccion (RenderBlocks.renderBlockBed): esquinas (maxX,maxZ), (maxX,minZ), (minX,minZ), (minX,maxZ)
    inline const signed char* bedTopUV(int dir) {
        static const signed char T[4][8] = {
            { 1, 1,  0, 1,  0, 0,  1, 0 },
            { 0, 1,  0, 0,  1, 0,  1, 1 },
            { 0, 0,  1, 0,  1, 1,  0, 1 },
            { 1, 0,  1, 1,  0, 1,  0, 0 },
        };
        return T[dir & 3];
    }

    // Lo que suelta al picarlo: la puerta, solo la mitad de abajo; la cama, solo la cabecera (la mitad sin el bit 8)
    inline bool dropsOnMining(BlockType t, int meta) {
        if (isDoorBlock(t) || t == BlockType::BED_BLOCK) return (meta & 8) == 0;
        if (t == BlockType::CAKE_BLOCK) return false;   // BlockCake.idDropped = 0
        if (t == BlockType::CROPS) return false;        // BlockCrops.dropBlockAsItemWithChance: trigo y semillas segun la etapa (main.cpp / cropDrops)
        if (t == BlockType::PISTON_HEAD || t == BlockType::PISTON_MOVING) return false;   // BlockPistonExtension.quantityDropped = 0
        return true;
    }

    // ---- Con el mundo ----
    // El clic derecho sobre el bloque lo consume la puerta de hierro y la cama aunque no hagan nada; la puerta de madera y la trampilla se abren
    // (el cartel no tiene blockActivated: el clic derecho sigue con la logica normal de poner bloques)
    inline bool isInteractive(BlockType t) { return isPartialBlock(t) && t != BlockType::FARMLAND && t != BlockType::CROPS && !isSignBlock(t) && !isPistonBlock(t) && !isStructureBlock(t) && (!isRedstoneComponent(t) || t == BlockType::LEVER || t == BlockType::BUTTON || isRepeaterBlock(t)); }
    // BlockCrops.dropBlockAsItemWithChance: el trigo solo si esta maduro (etapa 7); 3 intentos de semilla, cada uno con probabilidad (etapa + 1) / 15
    inline void cropDrops(int meta, int& wheat, int& seeds) {
        wheat = (meta == 7) ? 1 : 0;
        seeds = 0;
        for (int k = 0; k < 3; k++) if (rand() % 15 <= meta) ++seeds;
    }
    // ItemSeeds.onItemUse: solo por la cara de arriba de tierra de cultivo con aire encima. true = se planto (gasta una semilla)
    bool useSeeds(const ChunkManager& cm, int gx, int gy, int gz, BlockFace face);
    // ItemDye con polvo de hueso sobre un cultivo (BlockCrops.fertilize: +2 a 5 etapas) o sobre pasto (esparce pasto alto y flores). true = se gasta una unidad.
    bool useBoneMeal(const ChunkManager& cm, int gx, int gy, int gz);
    // Tocadiscos (BlockJukeBox): el disco se guarda en la metadata de la celda (0 vacio, 1 disco "13", 2 disco "cat")
    BlockType jukeboxRecord(const ChunkManager& cm, int gx, int gy, int gz);                        // el disco que tiene dentro o AIR
    bool jukeboxInsert(const ChunkManager& cm, int gx, int gy, int gz, BlockType record);           // ItemRecord.onItemUse: true si estaba vacio y lo guardo
    BlockType jukeboxEject(const ChunkManager& cm, int gx, int gy, int gz);                         // BlockJukeBox.func_28038_b_: lo saca y lo devuelve (AIR si no habia)
    // BlockCrops.updateTick (tick aleatorio): se cae sin su apoyo y, con luz, crece una etapa con probabilidad segun el terreno de alrededor
    void cropsTick(const ChunkManager& cm, int gx, int gy, int gz);
    // ItemHoe.onItemUse: tierra, o cesped sin nada encima (y no por la cara de abajo), pasa a tierra de cultivo seca. true = se uso (gasta 1 de durabilidad)
    bool useHoe(const ChunkManager& cm, int gx, int gy, int gz, BlockFace face);
    // BlockFarmland.updateTick (tick aleatorio, 1 de cada 5): con agua a 4 bloques se moja del todo; si no, se seca de a poco y al final vuelve a tierra
    void farmlandTick(const ChunkManager& cm, int gx, int gy, int gz, int roll);
    // BlockFarmland.onEntityWalking: 1 de cada 4 pasos la tierra de cultivo vuelve a tierra
    void trample(const ChunkManager& cm, int gx, int gy, int gz);
    // Entity.moveEntity -> Block.onEntityWalking para un mob (Animal, Mob u Sheep: con x, y (pies), z, prevX, prevZ, walkedDist y nextStep): se llama una vez por tick
    // despues de moverlo. La distancia caminada (x 0,6) cuenta pasos de 1 en 1 y el bloque bajo los pies (0,2 mas abajo) lo pisa
    void mobStep(const ChunkManager& cm, int gx, int gy, int gz);   // 1 de cada 4 pasos sobre tierra de cultivo la deshace
    template <class E>
    inline void mobWalked(const ChunkManager& cm, E& e) {
        const double dx = e.x - e.prevX, dz = e.z - e.prevZ;
        const double d2 = dx * dx + dz * dz;
        if (d2 > 4.0) return;   // un salto de posicion (teletransporte), no un paso
        e.walkedDist += (float)(sqrt(d2) * 0.6);
        if (e.walkedDist > (float)e.nextStep) {
            ++e.nextStep;
            mobStep(cm, (int)floor(e.x + 4.0), (int)floor(e.y + 0.3) + 1, (int)floor(e.z + 4.0));   // celda de mob j = floor(y + 0,3) (los pies - 0,2); rejilla = j + 1
        }
    }
    bool eatCake(const ChunkManager& cm, int gx, int gy, int gz);   // BlockCake.eatCakeSlice (sin la parte de curar): una porcion menos
    bool activate(const ChunkManager& cm, int gx, int gy, int gz);   // abre o cierra; true = el clic se gasto

    // ItemDoor / ItemBed / ItemBlock(trampilla). (gx, gy, gz) = celda apuntada; yawDeg = giro del jugador (el de Player)
    bool useItem(const ChunkManager& cm, BlockType item, int gx, int gy, int gz, BlockFace face, float yawDeg);

    // Se quito un bloque en (gx, gy, gz): los vecinos parciales revisan si les quedo su apoyo (onNeighborBlockChange)
    void neighborsChanged(const ChunkManager& cm, int gx, int gy, int gz);

    // Colision: ¿la caja del mundo toca alguno de estos bloques? (la usa Player::collidesAt)
    // cur (opcional): caja actual del jugador {minX,minY,minZ,maxX,maxY,maxZ}; los bloques que ya la cruzan no cuentan como obstaculo
    bool boxHit(const ChunkManager& cm, float minX, float minY, float minZ, float maxX, float maxY, float maxZ, const float* cur = nullptr);

    // Rayo contra la caja del bloque. s y e: inicio y fin del rayo en coordenadas de la celda (0 a 1). Devuelve true si la corta.
    bool rayVsBox(const float bd[6], const float s[3], const float e[3], BlockFace& face, float hit[3]);
}

#endif
