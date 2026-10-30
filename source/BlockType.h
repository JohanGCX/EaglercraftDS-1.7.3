#ifndef BLOCKTYPE_H
#define BLOCKTYPE_H

#include <nds.h>
#include "Cube.h"

enum class BlockType : unsigned char {
    AIR = 0,
    GRASS,
    DIRT,
    STONE,
    OAK_LOG,
    BIRCH_LOG,
    OAK_LEAVES,
    BIRCH_LEAVES,
    TALLGRASS,
    WOOD_PLANKS,
    STICK,
    COBBLESTONE,
    WOOD_PICKAXE,
    STONE_PICKAXE,
    WOOD_SWORD,
    WOOD_SHOVEL,
    WOOD_AXE,
    STONE_SWORD,
    STONE_SHOVEL,
    STONE_AXE,
    CRAFTING_TABLE,
    COAL_ORE,
    COAL,
    SAND,
    GRAVEL,
    WATER,
    CACTUS,
    TORCH,
    SNOW_LAYER,
    SPRUCE_LOG,
    SPRUCE_LEAVES,
    DEAD_BUSH,
    SANDSTONE,
    ROSE,
    DANDELION,
    DYE_RED,
    DYE_YELLOW,
    IRON_ORE,
    FURNACE,
    FURNACE_W,
    FURNACE_N,
    FURNACE_E,
    FURNACE_LIT_S,
    FURNACE_LIT_W,
    FURNACE_LIT_N,
    FURNACE_LIT_E,
    IRON_INGOT,
    CHARCOAL,
    DYE_GREEN,
    GLASS,
    IRON_PICKAXE,
    IRON_SWORD,
    IRON_SHOVEL,
    IRON_AXE,
    BEDROCK,
    TORCH_W,   // antorcha apoyada en el bloque de -X (metadata 1 del original)
    TORCH_E,   // apoyada en el bloque de +X (metadata 2)
    TORCH_N,   // apoyada en el bloque de -Z (metadata 3)
    TORCH_S,   // apoyada en el bloque de +Z (metadata 4)
    TORCH_F,   // antorcha de piso, apoyada en el bloque de abajo (metadata 5)
    WATER_FLOW, // agua corriente (WATER es el agua quieta, id 9 del original; WATER_FLOW el 8)
    LAVA,       // lava quieta (id 11)
    LAVA_FLOW,  // lava corriente (id 10)
    OBSIDIAN,
    BUCKET,        // balde vacío (items.png)
    WATER_BUCKET,  // balde con agua
    LAVA_BUCKET,   // balde con lava
    DIAMOND_ORE,   // mena de diamante (terrain.png)
    DIAMOND,       // diamante (items.png)
    DIAMOND_PICKAXE,
    DIAMOND_SWORD,
    DIAMOND_SHOVEL,
    DIAMOND_AXE,
    FIRE,             // fuego (BlockFire): terrain.png (15,1) y (15,2)
    FLINT,            // pedernal (items.png)
    FLINT_AND_STEEL,  // mechero (items.png)
    PORTAL,           // portal del Nether (BlockPortal): terrain.png (14,0)
    NETHERRACK,       // netherrack (terrain.png (7,6))
    SOUL_SAND,        // arena de almas (terrain.png (8,6))
    GLOWSTONE,        // piedra luminosa (terrain.png (9,6))
    GLOWSTONE_DUST,   // polvo de piedra luminosa (items.png (9,4))
    CHEST,            // cofre (terrain.png: 25 arriba y abajo, 26 lado, 27 frente, 41/42 y 57/58 del doble)
    WOOL_WHITE,       // lana (BlockCloth): metadata 0 = blanca ... 15 = negra, el mismo número que el color de la oveja
    WOOL_ORANGE,
    WOOL_MAGENTA,
    WOOL_LIGHT_BLUE,
    WOOL_YELLOW,
    WOOL_LIME,
    WOOL_PINK,
    WOOL_GRAY,
    WOOL_LIGHT_GRAY,
    WOOL_CYAN,
    WOOL_PURPLE,
    WOOL_BLUE,
    WOOL_BROWN,
    WOOL_GREEN,
    WOOL_RED,
    WOOL_BLACK,
    SHEARS,           // tijeras (ItemShears, items.png (13,5)): 238 usos
    GOLD_ORE,         // mena de oro (terrain.png (0,2))
    REDSTONE_ORE,     // mena de redstone (terrain.png (3,3))
    LAPIS_ORE,        // mena de lapislazuli (terrain.png (0,10))
    GOLD_INGOT,       // lingote de oro (items.png (7,2))
    REDSTONE,         // polvo de redstone (items.png (8,3))
    DYE_LAPIS,        // lapislazuli: tinte con damage 4 (items.png (14,8))
    CLAY,             // bloque de arcilla (terrain.png (8,4))
    CLAY_BALL,        // bola de arcilla (items.png (9,3))
    BRICK,            // ladrillo (items.png (6,1)): bola de arcilla cocida
    MUSHROOM_BROWN,   // hongo marron (terrain.png (13,1))
    MUSHROOM_RED,     // hongo rojo (terrain.png (12,1))
    SUGAR_CANE,       // BLOQUE de cana de azucar (terrain.png (9,4)): se dibuja en cruz y se apila. No se obtiene: suelta SUGAR_CANE_ITEM
    SUGAR_CANE_ITEM,  // objeto cana de azucar (items.png (11,1)): al ponerlo sale el bloque SUGAR_CANE
    PUMPKIN,          // calabaza (BlockPumpkin): la cara tallada mira al sur (+Z); es el objeto del inventario
    COMPASS,          // brujula (items.png (6,3)): antes aqui iba PUMPKIN_W; la direccion de la calabaza ahora va en la metadata
    CLOCK,            // reloj (items.png (6,4)): antes PUMPKIN_N
    MAP,              // mapa (items.png (12,3)): antes PUMPKIN_E
    JACK_O_LANTERN,   // linterna de calabaza (emite luz 15): cara al sur; es el objeto del inventario
    SADDLE,           // silla de montar (items.png (8,6)): antes JACK_W
    FISHING_ROD,      // cana de pescar (items.png (5,4)): antes JACK_N
    SPARE_ITEM,       // libre: antes JACK_E
    MOSSY_COBBLESTONE, // cobblestone musgoso (terrain.png (4,2)): solo sale en mazmorras
    MOB_SPAWNER,       // generador de monstruos (terrain.png (1,4)): jaula; no suelta nada y no se obtiene
    DOOR_WOOD,         // puerta de madera (BlockDoor): una por celda, mitad de abajo y de arriba; la orientacion va en la metadata (liquidMeta)
    DOOR_IRON,         // puerta de hierro
    TRAPDOOR,          // trampilla (BlockTrapDoor, terrain.png (4,5)); tambien es el objeto del inventario
    BED_BLOCK,         // mitad de una cama (BlockBed): la metadata dice cual mitad y hacia donde mira
    DOOR_WOOD_ITEM,    // objeto puerta de madera (items.png (11,2))
    DOOR_IRON_ITEM,    // objeto puerta de hierro (items.png (12,2))
    BED_ITEM,          // objeto cama (items.png (13,2))
    SIGN_POST,         // cartel de pie (BlockSign): la metadata (0 a 15) es el giro en pasos de 22,5 grados
    SIGN_WALL,         // cartel de pared: la metadata es la cara donde esta pegado (2 a 5, como Beta)
    CAKE_BLOCK,        // pastel (BlockCake, terrain.png (9,7)): la metadata (0 a 5) son las porciones comidas
    SIGN_ITEM,         // objeto cartel (items.png (10,2))
    CAKE_ITEM,         // objeto pastel (items.png (13,1))
    REDSTONE_WIRE,     // cable de redstone (BlockRedstoneWire, terrain.png (4,10)): la metadata (0 a 15) es la fuerza de la corriente
    LEVER,             // palanca (BlockLever, terrain.png (0,6)): metadata 1-4 en pared, 5-6 en el suelo, bit 8 = encendida; tambien es el objeto
    BUTTON,            // boton de piedra (BlockButton): metadata 1-4 en pared, bit 8 = pulsado; tambien es el objeto
    PLATE_STONE,       // placa de presion de piedra (BlockPressurePlate): metadata 1 = pisada; tambien es el objeto
    PLATE_WOOD,        // placa de presion de madera
    TORCH_REDSTONE_OFF,  // antorcha de redstone apagada (terrain.png (3,7)): la metadata es la de la antorcha (1-5)
    TORCH_REDSTONE_ON,   // antorcha de redstone encendida (terrain.png (3,6)); tambien es el objeto
    REPEATER_OFF,      // repetidor apagado (BlockRedstoneRepeater): metadata = direccion | retardo << 2
    REPEATER_ON,       // repetidor encendido
    REDSTONE_ORE_LIT,  // mena de redstone encendida (la misma textura; emite luz 9)
    REPEATER_ITEM,     // objeto repetidor (items.png (6,5))
    PISTON_BASE,       // piston (BlockPistonBase): metadata = direccion (0 abajo, 1 arriba, 2 -Z, 3 +Z, 4 -X, 5 +X) | 8 si esta extendido; tambien es el objeto
    PISTON_STICKY,     // piston pegajoso
    PISTON_HEAD,       // cabeza del piston (BlockPistonExtension): direccion | 8 si es del pegajoso
    PISTON_MOVING,     // bloque en movimiento (BlockPistonMoving): los datos del bloque que se mueve estan en Pistons.cpp
    SLAB_STONE,        // losa de piedra (BlockStep, meta 0): media celda; tambien es el objeto
    SLAB_SAND,         // losa de arenisca (meta 1)
    SLAB_WOOD,         // losa de madera (meta 2)
    SLAB_COBBLE,       // losa de roca (meta 3)
    DOUBLE_SLAB_STONE, // losa doble (celda entera): suelta dos losas
    DOUBLE_SLAB_SAND,
    DOUBLE_SLAB_WOOD,
    DOUBLE_SLAB_COBBLE,
    STAIRS_WOOD,       // escalera de madera (BlockStairs): la metadata 0 a 3 es hacia donde sube; tambien es el objeto
    STAIRS_COBBLE,     // escalera de roca
    LADDER,            // escalera de mano (BlockLadder, terrain.png (3,5)): la metadata 2 a 5 es la cara donde esta pegada
    FENCE,             // valla (BlockFence): la metadata son los lados con union (bit 0 -Z, 1 +Z, 2 -X, 3 +X)
    RAIL,              // riel (BlockRail, terrain.png (0,8) recto y (0,7) curva): metadata 0 a 9 = forma (ver RailLogic.h); tambien es el objeto
    RAIL_POWERED,      // riel propulsor (terrain.png (3,10) apagado, (3,11) encendido): metadata 0 a 5 = forma, bit 8 = encendido
    RAIL_DETECTOR,     // riel detector (terrain.png (3,12)): metadata 0 a 5 = forma, bit 8 = hay una vagoneta encima
    MINECART,          // vagoneta (items.png (7,8)): objeto
    MINECART_CHEST,    // vagoneta con cofre (items.png (7,9))
    MINECART_FURNACE,  // vagoneta con horno (items.png (7,10))
    BOAT,              // bote (items.png (8,8)): objeto
    LEATHER,           // cuero (items.png (7,6)): lo suelta la vaca
    PORK_RAW,          // carne de cerdo cruda (items.png (7,5)): cura 3
    PORK_COOKED,       // carne de cerdo cocida (items.png (8,5)): cura 8
    FEATHER,           // pluma (items.png (8,1)): la suelta la gallina
    EGG,               // huevo (items.png (12,0)): lo pone la gallina
    MILK_BUCKET,       // balde de leche (items.png (13,4)): se saca de una vaca con un balde vacio
    ARROW,             // flecha (items.png (5,2)): la sueltan los esqueletos
    BONE,              // hueso (items.png (12,1)): lo sueltan los esqueletos
    STRING,            // hilo (items.png (8,0)): lo suelta la arana
    GUNPOWDER,         // polvora (items.png (8,2)): la suelta el creeper
    INK_SAC,           // bolsa de tinta (items.png (14,4), dyePowder damage 0): la suelta el calamar
    SLIMEBALL,         // bola de slime (items.png (14,1)): la suelta el slime chico
    TNT,               // dinamita (BlockTNT, terrain.png (8,0) lado, (9,0) arriba, (10,0) abajo)
    NOTE_BLOCK,        // bloque de notas (BlockNote, terrain.png (10,4)): la nota (0 a 24) se guarda en NoteBlocks
    DISPENSER,         // dispensador (BlockDispenser): el frente mira al sur (+Z); es el objeto del inventario. Su contenido (9 casillas) esta en ChestStore
    DISPENSER_W,       // frente al oeste (-X)
    DISPENSER_N,       // frente al norte (-Z)
    DISPENSER_E,       // frente al este (+X)
    BOW,               // arco (ItemBow, items.png (5,1)): gasta una flecha por disparo; no tiene durabilidad
    SNOWBALL,          // bola de nieve (items.png (14,0)): la suelta la nieve al picarla con pala; se lanza (de a 16)
    FARMLAND,          // tierra de cultivo (BlockFarmland, terrain.png (7,5) seca y (6,5) mojada): la metadata (0 a 7) es la humedad. Bloque parcial de 15/16; suelta tierra
    HOE_WOOD,          // azadas (ItemHoe, items.png fila 8): crean tierra de cultivo en tierra o cesped
    HOE_STONE,
    HOE_IRON,
    HOE_DIAMOND,
    CROPS,             // trigo plantado (BlockCrops, terrain.png (8..15,5) segun la etapa): la metadata (0 a 7) es el crecimiento. Bloque parcial, sin colision
    SEEDS,             // semillas de trigo (items.png (9,0)): se plantan sobre tierra de cultivo
    WHEAT,             // trigo (items.png (9,1)): lo suelta el cultivo maduro
    BREAD,             // pan (items.png (9,2)): cura 5
    BOWL,              // tazon vacio (items.png (7,4))
    MUSHROOM_STEW,     // sopa de hongos (items.png (8,4)): cura 10, devuelve el tazon
    COOKIE,            // galleta (items.png (12,5)): cura 1, se apila de a 8
    BLOCK_IRON,        // bloque de hierro (terrain.png (6,1))
    BLOCK_GOLD,        // bloque de oro (terrain.png (7,1))
    BLOCK_DIAMOND,     // bloque de diamante (terrain.png (8,1))
    BLOCK_LAPIS,       // bloque de lapislazuli (terrain.png (0,9))
    BOOKSHELF,         // estanteria (terrain.png (3,2), arriba y abajo tablones): no suelta nada
    SPONGE,            // esponja (terrain.png (0,3)): en la Beta 1.7.3 no absorbe agua
    GOLD_SWORD,        // herramientas de oro (items.png (4,4) a (4,8)): EnumToolMaterial.GOLD
    GOLD_SHOVEL,
    GOLD_PICKAXE,
    GOLD_AXE,
    HOE_GOLD,
    SUGAR,             // azucar (items.png (13,0))
    PAPER,             // papel (items.png (10,3))
    BOOK,              // libro (items.png (11,3))
    APPLE,             // manzana (items.png (10,0)): cura 4
    GOLDEN_APPLE,      // manzana dorada (items.png (11,0)): cura 42
    FISH_RAW,          // pescado crudo (items.png (9,5)): cura 2
    FISH_COOKED,       // pescado cocido (items.png (10,5)): cura 5
    DYE_COCOA,         // granos de cacao (items.png (14,7))
    BRICK_BLOCK,       // bloque de ladrillos (terrain.png (7,0))
    SNOW_BLOCK,        // bloque de nieve (terrain.png (2,4)): suelta 4 bolas de nieve (solo con pala)
    ICE,               // hielo (terrain.png (3,4)): resbala, se derrite con luz y al romperlo deja agua
    WEB,               // telaraña (terrain.png (11,0)): frena a quien la toca, suelta hilo
    DYE_PURPLE,        // tintes (ItemDye, items.png (14 + d/8, 4 + d%8)): morado 5
    DYE_CYAN,          // cian 6
    DYE_LIGHT_GRAY,    // gris claro 7
    DYE_GRAY,          // gris 8
    DYE_PINK,          // rosa 9
    DYE_LIME,          // lima 10
    DYE_LIGHT_BLUE,    // celeste 12
    DYE_MAGENTA,       // magenta 13
    DYE_ORANGE,        // naranja 14
    BONE_MEAL,         // polvo de hueso 15: hace crecer el cultivo y esparce pasto alto y flores
    JUKEBOX,           // tocadiscos (BlockJukeBox, terrain.png (10,4) y arriba (11,4)): metadata 0 vacio, 1 disco 13, 2 disco cat
    PAINTING,          // cuadro (ItemPainting, items.png (10,1)): se cuelga de una pared
    RECORD_13,         // disco "13" (ItemRecord, items.png (0,15))
    RECORD_CAT,        // disco "cat" (items.png (1,15))
    // Armaduras (ItemArmor, items.png columna = material, fila = pieza). Orden: cuero, hierro, diamante, oro; casco, peto, pantalones, botas. No hay malla: en la Beta solo se hacia con fuego y el enum de bloques (1 byte) no tiene mas sitio
    HELMET_LEATHER,
    CHESTPLATE_LEATHER,
    LEGGINGS_LEATHER,
    BOOTS_LEATHER,
    HELMET_IRON,
    CHESTPLATE_IRON,
    LEGGINGS_IRON,
    BOOTS_IRON,
    HELMET_DIAMOND,
    CHESTPLATE_DIAMOND,
    LEGGINGS_DIAMOND,
    BOOTS_DIAMOND,
    HELMET_GOLD,
    CHESTPLATE_GOLD,
    LEGGINGS_GOLD,
    BOOTS_GOLD,
    SAPLING_OAK,       // retoño de roble (terrain.png (15,0)): se dibuja en cruz; meta 8 = ya paso la mitad del crecimiento
    SAPLING_SPRUCE,    // retoño de abeto (terrain.png (15,3))
    SAPLING_BIRCH      // retoño de abedul (terrain.png (15,4))
};

// Bloques parciales (puertas, trampilla, cama): no llenan la celda, la forma sale de la metadata (PartialBlocks.h)
inline bool isDoorBlock(BlockType t) { return t == BlockType::DOOR_WOOD || t == BlockType::DOOR_IRON; }
inline bool isSignBlock(BlockType t) { return t == BlockType::SIGN_POST || t == BlockType::SIGN_WALL; }
inline bool isRedstoneTorch(BlockType t) { return t == BlockType::TORCH_REDSTONE_OFF || t == BlockType::TORCH_REDSTONE_ON; }
inline bool isRepeaterBlock(BlockType t) { return t == BlockType::REPEATER_OFF || t == BlockType::REPEATER_ON; }
inline bool isRailBlock(BlockType t) { return t == BlockType::RAIL || t == BlockType::RAIL_POWERED || t == BlockType::RAIL_DETECTOR; }
inline bool isMinecartItem(BlockType t) { return t == BlockType::MINECART || t == BlockType::MINECART_CHEST || t == BlockType::MINECART_FURNACE; }
// Objetos de los animales (cuero, cerdo, pluma, huevo, leche): solo se llevan, no se colocan
inline bool isAnimalItem(BlockType t) { return t == BlockType::LEATHER || t == BlockType::PORK_RAW || t == BlockType::PORK_COOKED || t == BlockType::FEATHER || t == BlockType::EGG || t == BlockType::MILK_BUCKET ||
    t == BlockType::ARROW || t == BlockType::BONE || t == BlockType::STRING || t == BlockType::GUNPOWDER || t == BlockType::INK_SAC || t == BlockType::SLIMEBALL ||
    t == BlockType::BOW || t == BlockType::SNOWBALL || t == BlockType::BREAD ||
    t == BlockType::BOWL || t == BlockType::MUSHROOM_STEW || t == BlockType::COOKIE; }
// ItemArmor: pieza 0 casco, 1 peto, 2 pantalones, 3 botas (armorType); material 0 cuero, 1 hierro, 2 diamante, 3 oro (en la Beta renderIndex 0, 2, 3, 4)
inline bool isArmor(BlockType t) { return t >= BlockType::HELMET_LEATHER && t <= BlockType::BOOTS_GOLD; }
inline int armorPiece(BlockType t) { return ((int)t - (int)BlockType::HELMET_LEATHER) & 3; }
inline int armorMaterial(BlockType t) { return ((int)t - (int)BlockType::HELMET_LEATHER) >> 2; }
inline int armorRenderIndex(BlockType t) { static const int R[4] = { 0, 2, 3, 4 }; return R[armorMaterial(t)]; }   // columna en items.png y textura armor/
inline int armorReduceAmount(BlockType t) { static const int R[4] = { 3, 8, 6, 3 }; return R[armorPiece(t)]; }          // ItemArmor.damageReduceAmountArray
inline int armorMaxDurability(BlockType t) {                                                                              // maxDamageArray[pieza] * 3 << armorLevel (el oro usa nivel 1)
    static const int D[4] = { 11, 16, 15, 13 }; static const int LEVEL[4] = { 0, 2, 3, 1 };
    return (D[armorPiece(t)] * 3) << LEVEL[armorMaterial(t)];
}
// Objetos sueltos nuevos (azucar, papel, libro, manzanas, pescado, cacao): solo se llevan
inline bool isMiscItem(BlockType t) { return t == BlockType::SUGAR || t == BlockType::PAPER || t == BlockType::BOOK || t == BlockType::APPLE || t == BlockType::GOLDEN_APPLE ||
    t == BlockType::FISH_RAW || t == BlockType::FISH_COOKED || t == BlockType::DYE_COCOA ||
    t == BlockType::DYE_PURPLE || t == BlockType::DYE_CYAN || t == BlockType::DYE_LIGHT_GRAY || t == BlockType::DYE_GRAY || t == BlockType::DYE_PINK ||
    t == BlockType::DYE_LIME || t == BlockType::DYE_LIGHT_BLUE || t == BlockType::DYE_MAGENTA || t == BlockType::DYE_ORANGE || t == BlockType::BONE_MEAL ||
    t == BlockType::PAINTING || t == BlockType::RECORD_13 || t == BlockType::RECORD_CAT || isArmor(t) ||
    t == BlockType::COMPASS || t == BlockType::CLOCK || t == BlockType::MAP || t == BlockType::SADDLE || t == BlockType::FISHING_ROD; }
inline bool isRecord(BlockType t) { return t == BlockType::RECORD_13 || t == BlockType::RECORD_CAT; }
// Herramientas de oro (menos la azada, que cuenta en isHoeTool)
inline bool isGoldTool(BlockType t) { return t == BlockType::GOLD_SWORD || t == BlockType::GOLD_SHOVEL || t == BlockType::GOLD_PICKAXE || t == BlockType::GOLD_AXE; }
inline bool isPlateBlock(BlockType t) { return t == BlockType::PLATE_STONE || t == BlockType::PLATE_WOOD; }
// Bloques de redstone que no llenan la celda (se dibujan y se seleccionan por su caja: PartialBlocks.h / Redstone.cpp)
inline bool isRedstoneComponent(BlockType t) {
    return t == BlockType::REDSTONE_WIRE || t == BlockType::LEVER || t == BlockType::BUTTON || isPlateBlock(t) || isRedstoneTorch(t) || isRepeaterBlock(t) || isRailBlock(t);
}
inline bool isPistonBase(BlockType t) { return t == BlockType::PISTON_BASE || t == BlockType::PISTON_STICKY; }
inline bool isPistonBlock(BlockType t) { return isPistonBase(t) || t == BlockType::PISTON_HEAD || t == BlockType::PISTON_MOVING; }
// Losas, escaleras, escalera de mano y valla (PartialBlocks.h): bloques que no llenan la celda
inline bool isSlabBlock(BlockType t) { return t >= BlockType::SLAB_STONE && t <= BlockType::SLAB_COBBLE; }
inline bool isDoubleSlabBlock(BlockType t) { return t >= BlockType::DOUBLE_SLAB_STONE && t <= BlockType::DOUBLE_SLAB_COBBLE; }
inline BlockType slabOfDouble(BlockType t) { return (BlockType)((int)BlockType::SLAB_STONE + ((int)t - (int)BlockType::DOUBLE_SLAB_STONE)); }
inline BlockType doubleOfSlab(BlockType t) { return (BlockType)((int)BlockType::DOUBLE_SLAB_STONE + ((int)t - (int)BlockType::SLAB_STONE)); }
inline bool isStairsBlock(BlockType t) { return t == BlockType::STAIRS_WOOD || t == BlockType::STAIRS_COBBLE; }
inline bool isStructureBlock(BlockType t) { return isSlabBlock(t) || isStairsBlock(t) || t == BlockType::LADDER || t == BlockType::FENCE; }
inline bool isPartialBlock(BlockType t) { return isStructureBlock(t) || isPistonBlock(t) || isDoorBlock(t) || t == BlockType::TRAPDOOR || t == BlockType::BED_BLOCK || isSignBlock(t) || t == BlockType::CAKE_BLOCK || t == BlockType::FARMLAND || t == BlockType::CROPS || isRedstoneComponent(t); }

// TORCH, la de siempre, es la antorcha SIN orientación (metadata 0): es el ítem del inventario y lo que traen los
// mundos guardados antes de este cambio. Al cargar el chunk se orienta sola, como el updateTick del original.
inline bool isTorchBlock(BlockType t) {
    return t == BlockType::TORCH || t == BlockType::TORCH_W || t == BlockType::TORCH_E ||
           t == BlockType::TORCH_N || t == BlockType::TORCH_S || t == BlockType::TORCH_F;
}

// Metadata de BlockTorch: 0 = sin orientar, 1 = apoyo en -X, 2 = +X, 3 = -Z, 4 = +Z, 5 = apoyo abajo
inline int torchMeta(BlockType t) {
    switch (t) {
        case BlockType::TORCH_W: return 1;
        case BlockType::TORCH_E: return 2;
        case BlockType::TORCH_N: return 3;
        case BlockType::TORCH_S: return 4;
        case BlockType::TORCH_F: return 5;
        default: return 0;
    }
}

inline BlockType torchFromMeta(int meta) {
    switch (meta) {
        case 1: return BlockType::TORCH_W;
        case 2: return BlockType::TORCH_E;
        case 3: return BlockType::TORCH_N;
        case 4: return BlockType::TORCH_S;
        case 5: return BlockType::TORCH_F;
        default: return BlockType::TORCH;
    }
}

// ---- Líquidos (BlockFluid / BlockFlowing / BlockStationary) ----
inline bool isWaterBlock(BlockType t) { return t == BlockType::WATER || t == BlockType::WATER_FLOW; }
inline bool isLavaBlock(BlockType t)  { return t == BlockType::LAVA  || t == BlockType::LAVA_FLOW; }
inline bool isLiquidBlock(BlockType t) { return isWaterBlock(t) || isLavaBlock(t); }
// Plantas que se dibujan en cruz (dos planos): flores, hongos y caña de azúcar
inline bool isSapling(BlockType t) { return t == BlockType::SAPLING_OAK || t == BlockType::SAPLING_SPRUCE || t == BlockType::SAPLING_BIRCH; }
inline bool isCrossPlant(BlockType t) {
    return isSapling(t) || t == BlockType::ROSE || t == BlockType::DANDELION || t == BlockType::MUSHROOM_RED || t == BlockType::MUSHROOM_BROWN ||
           t == BlockType::SUGAR_CANE || t == BlockType::WEB;
}

inline bool isFlowingLiquid(BlockType t) { return t == BlockType::WATER_FLOW || t == BlockType::LAVA_FLOW; }
// 0 = no es líquido, 1 = agua, 2 = lava (equivale a comparar el Material del original)
inline int liquidMaterial(BlockType t) { return isWaterBlock(t) ? 1 : (isLavaBlock(t) ? 2 : 0); }
inline BlockType liquidFlowingOf(BlockType t) { return isWaterBlock(t) ? BlockType::WATER_FLOW : BlockType::LAVA_FLOW; }
inline BlockType liquidStillOf(BlockType t)   { return isWaterBlock(t) ? BlockType::WATER : BlockType::LAVA; }

// Material.getIsSolid(): lo que bloquea el flujo. No son sólidos: aire, líquidos, plantas, antorchas y capa de nieve.
inline bool isMaterialSolid(BlockType t) {
    switch (t) {
        case BlockType::AIR:
        case BlockType::FIRE:
        case BlockType::PORTAL:
        case BlockType::TALLGRASS:
        case BlockType::DEAD_BUSH:
        case BlockType::ROSE:
        case BlockType::DANDELION:
        case BlockType::SAPLING_OAK: case BlockType::SAPLING_SPRUCE: case BlockType::SAPLING_BIRCH:
        case BlockType::MUSHROOM_RED:
        case BlockType::MUSHROOM_BROWN:
        case BlockType::SUGAR_CANE:
        case BlockType::WEB:     // BlockWeb.getCollisionBoundingBoxFromPool = null: se atraviesa (frena en Player)
        case BlockType::CROPS:   // Material.plants: no es solido (sin colision; el agua la arrastra)
        case BlockType::SNOW_LAYER:
        case BlockType::RAIL: case BlockType::RAIL_POWERED: case BlockType::RAIL_DETECTOR:   // Material.circuits: no es solido
            return false;
        default:
            return !isLiquidBlock(t) && !isTorchBlock(t);
    }
}

// Texturas del horno (terrain.png)
const TextureCell FURNACE_TOP_CELL  = {14, 3};
const TextureCell FURNACE_SIDE_CELL = {13, 2};
const TextureCell FURNACE_FRONT_OFF = {12, 2};
const TextureCell FURNACE_FRONT_LIT = {13, 3};

inline bool isFurnaceBlock(BlockType t) {
    return t == BlockType::FURNACE || t == BlockType::FURNACE_W || t == BlockType::FURNACE_N ||
           t == BlockType::FURNACE_E || t == BlockType::FURNACE_LIT_S || t == BlockType::FURNACE_LIT_W ||
           t == BlockType::FURNACE_LIT_N || t == BlockType::FURNACE_LIT_E;
}

inline bool furnaceIsLit(BlockType t) {
    return t == BlockType::FURNACE_LIT_S || t == BlockType::FURNACE_LIT_W ||
           t == BlockType::FURNACE_LIT_N || t == BlockType::FURNACE_LIT_E;
}

// Hacia dónde mira la cara frontal (mismo orden que emitCube): 0=sur(+Z) 1=norte(-Z) 2=oeste(-X) 3=este(+X)
inline int furnaceFrontDir(BlockType t) {
    switch (t) {
        case BlockType::FURNACE_W: case BlockType::FURNACE_LIT_W: return 2;
        case BlockType::FURNACE_N: case BlockType::FURNACE_LIT_N: return 1;
        case BlockType::FURNACE_E: case BlockType::FURNACE_LIT_E: return 3;
        default: return 0;
    }
}

inline BlockType makeFurnaceBlock(int frontDir, bool lit) {
    static const BlockType OFF[4] = { BlockType::FURNACE, BlockType::FURNACE_N, BlockType::FURNACE_W, BlockType::FURNACE_E };
    static const BlockType ON[4]  = { BlockType::FURNACE_LIT_S, BlockType::FURNACE_LIT_N, BlockType::FURNACE_LIT_W, BlockType::FURNACE_LIT_E };
    if (frontDir < 0 || frontDir > 3) frontDir = 0;
    return lit ? ON[frontDir] : OFF[frontDir];
}

// Dispensador (terrain.png): lado (13,2), frente (14,2), arriba y abajo (14,3). Misma convencion de frente que el horno (0 sur, 1 norte, 2 oeste, 3 este)
const TextureCell DISPENSER_FRONT_CELL = {14, 2};
inline bool isDispenserBlock(BlockType t) {
    return t == BlockType::DISPENSER || t == BlockType::DISPENSER_W || t == BlockType::DISPENSER_N || t == BlockType::DISPENSER_E;
}
inline int dispenserFrontDir(BlockType t) {
    switch (t) {
        case BlockType::DISPENSER_W: return 2;
        case BlockType::DISPENSER_N: return 1;
        case BlockType::DISPENSER_E: return 3;
        default: return 0;
    }
}
inline BlockType makeDispenserBlock(int frontDir) {
    static const BlockType D[4] = { BlockType::DISPENSER, BlockType::DISPENSER_N, BlockType::DISPENSER_W, BlockType::DISPENSER_E };
    if (frontDir < 0 || frontDir > 3) frontDir = 0;
    return D[frontDir];
}

// Calabaza y linterna de calabaza (terrain.png): arriba y abajo (6,6), lado (6,7), cara tallada (7,7) y la de la linterna (8,7)
const TextureCell PUMPKIN_TOP_CELL   = {6, 6};
const TextureCell PUMPKIN_SIDE_CELL  = {6, 7};
const TextureCell PUMPKIN_FRONT_CELL = {7, 7};
const TextureCell JACK_FRONT_CELL    = {8, 7};

inline bool isJackBlock(BlockType t) {
    return t == BlockType::JACK_O_LANTERN;
}
inline bool isPumpkinBlock(BlockType t) {
    return t == BlockType::PUMPKIN || t == BlockType::JACK_O_LANTERN;
}
// La cara tallada va en la metadata del bloque (misma convención que el horno): 0 = sur (+Z), 1 = norte (-Z), 2 = oeste (-X), 3 = este (+X)

// Nivel de luz que emite un bloque (= (int)(15 * lightValue) de Block.java, Beta 1.7.3)
inline int blockLightEmission(BlockType t) {
    if (isTorchBlock(t)) return 14;
    if (t == BlockType::TORCH_REDSTONE_ON) return 7;    // Block.torchRedstoneActive: setLightValue(0.5)
    if (t == BlockType::REPEATER_ON || t == BlockType::REDSTONE_ORE_LIT) return 9;   // setLightValue(10 / 16)
    if (isLavaBlock(t)) return 15; // Block.lava: setLightValue(1.0)
    if (t == BlockType::FIRE) return 15; // Block.fire: setLightValue(1.0)
    if (t == BlockType::PORTAL) return 11; // Block.portal: setLightValue(12/16) -> (int)(0.75 * 15)
    if (t == BlockType::GLOWSTONE) return 15; // Block.glowStone: setLightValue(1.0)
    if (furnaceIsLit(t)) return 13;
    if (isJackBlock(t)) return 15; // Block.pumpkinLantern: setLightValue(1.0)
    return 0;
}

// ---- Lana (BlockCloth): 16 colores, el mismo orden que la metadata del original y que fleeceColorTable de la oveja ----
inline bool isWool(BlockType t) { return t >= BlockType::WOOL_WHITE && t <= BlockType::WOOL_BLACK; }
inline int woolColor(BlockType t) { return (int)t - (int)BlockType::WOOL_WHITE; }                  // 0 = blanca ... 15 = negra
// ItemDye: el "damage" del tinte (0 negro ... 15 polvo de hueso), o -1 si no es un tinte. El color de lana que da es 15 - damage (BlockCloth.func_21034_c)
inline int dyeDamage(BlockType t) {
    switch (t) {
        case BlockType::INK_SAC: return 0;       case BlockType::DYE_RED: return 1;        case BlockType::DYE_GREEN: return 2;
        case BlockType::DYE_COCOA: return 3;     case BlockType::DYE_LAPIS: return 4;      case BlockType::DYE_PURPLE: return 5;
        case BlockType::DYE_CYAN: return 6;      case BlockType::DYE_LIGHT_GRAY: return 7; case BlockType::DYE_GRAY: return 8;
        case BlockType::DYE_PINK: return 9;      case BlockType::DYE_LIME: return 10;      case BlockType::DYE_YELLOW: return 11;
        case BlockType::DYE_LIGHT_BLUE: return 12; case BlockType::DYE_MAGENTA: return 13; case BlockType::DYE_ORANGE: return 14;
        case BlockType::BONE_MEAL: return 15;
        default: return -1;
    }
}
inline BlockType woolType(int color) { return (BlockType)((int)BlockType::WOOL_WHITE + (color & 15)); }

// BlockCloth.getBlockTextureFromSideAndMetadata: la blanca es la celda 64 (col 0, fila 4); las demás, 113 + ((~m & 8) >> 3) + (~m & 7) * 16
inline TextureCell woolCell(int color) {
    TextureCell c;
    if (color == 0) { c.col = 0; c.row = 4; return c; }
    const int index = 113 + (((~color) & 8) >> 3) + ((~color) & 7) * 16;
    c.col = index % 16;
    c.row = index / 16;
    return c;
}

inline void getBlockTextures(BlockType type, TextureCell& top, TextureCell& side, TextureCell& bottom) {
    if (isWool(type)) { top = side = bottom = woolCell(woolColor(type)); return; }
    switch (type) {
        case BlockType::GRASS:
            top = {0, 0}; side = {3, 0}; bottom = {2, 0};
            break;
        case BlockType::DIRT:
            top = {2, 0}; side = {2, 0}; bottom = {2, 0};
            break;
        case BlockType::STONE:
            top = {1, 0}; side = {1, 0}; bottom = {1, 0};
            break;
        case BlockType::OAK_LOG:
            top = {5, 1}; side = {4, 1}; bottom = {5, 1};
            break;
        case BlockType::BIRCH_LOG:
            top = {5, 1}; side = {5, 7}; bottom = {5, 1};
            break;
        case BlockType::OAK_LEAVES:
            top = {5, 3}; side = {5, 3}; bottom = {5, 3};   // antes {4, 3}
            break;
        case BlockType::BIRCH_LEAVES:
            top = {5, 3}; side = {5, 3}; bottom = {5, 3};
            break;
	case BlockType::TALLGRASS:
            top = {7, 2}; side = {7, 2}; bottom = {7, 2};
            break;
	case BlockType::WOOD_PLANKS:
            top = {4, 0}; side = {4, 0}; bottom = {4, 0};
            break;
        case BlockType::STICK:
            top = {5, 3}; side = {5, 3}; bottom = {5, 3}; // celda dentro de items.png, no terrain.png
            break;
	case BlockType::COBBLESTONE:
            top = {0, 1}; side = {0, 1}; bottom = {0, 1};
            break;
        case BlockType::MOSSY_COBBLESTONE:
            top = {4, 2}; side = {4, 2}; bottom = {4, 2}; // terrain.png
            break;
        case BlockType::MOB_SPAWNER:
            top = {1, 4}; side = {1, 4}; bottom = {1, 4}; // terrain.png
            break;
        case BlockType::SLAB_STONE: case BlockType::DOUBLE_SLAB_STONE:   // BlockStep: arriba (6,0), costado (5,0)
            top = {6, 0}; side = {5, 0}; bottom = {6, 0};
            break;
        case BlockType::SLAB_SAND: case BlockType::DOUBLE_SLAB_SAND:
            top = {0, 11}; side = {0, 12}; bottom = {0, 13};
            break;
        case BlockType::SLAB_WOOD: case BlockType::DOUBLE_SLAB_WOOD: case BlockType::STAIRS_WOOD: case BlockType::FENCE:
            top = {4, 0}; side = {4, 0}; bottom = {4, 0};
            break;
        case BlockType::SLAB_COBBLE: case BlockType::DOUBLE_SLAB_COBBLE: case BlockType::STAIRS_COBBLE:
            top = {0, 1}; side = {0, 1}; bottom = {0, 1};
            break;
        case BlockType::LADDER:
            top = {3, 5}; side = {3, 5}; bottom = {3, 5};   // terrain.png (83)
            break;
        case BlockType::TRAPDOOR:
            top = {4, 5}; side = {4, 5}; bottom = {4, 5}; // terrain.png (indice 84)
            break;
        case BlockType::DOOR_WOOD:
            top = {1, 6}; side = {1, 6}; bottom = {1, 6}; // terrain.png (97)
            break;
        case BlockType::DOOR_IRON:
            top = {2, 6}; side = {2, 6}; bottom = {2, 6}; // terrain.png (98)
            break;
        case BlockType::BED_BLOCK:
            top = {6, 8}; side = {6, 9}; bottom = {4, 0}; // terrain.png (134, 150, tablones)
            break;
        case BlockType::DOOR_WOOD_ITEM:
            top = {11, 2}; side = {11, 2}; bottom = {11, 2}; // items.png
            break;
        case BlockType::DOOR_IRON_ITEM:
            top = {12, 2}; side = {12, 2}; bottom = {12, 2}; // items.png
            break;
        case BlockType::BED_ITEM:
            top = {13, 2}; side = {13, 2}; bottom = {13, 2}; // items.png
            break;
        case BlockType::SIGN_POST: case BlockType::SIGN_WALL:
            top = {4, 0}; side = {4, 0}; bottom = {4, 0}; // terrain.png (tablones: el cartel usa /item/sign.png, aqui no hay ese archivo)
            break;
        case BlockType::CAKE_BLOCK:
            top = {9, 7}; side = {10, 7}; bottom = {12, 7}; // terrain.png (121, 122, 124); la cara del corte es (11,7)
            break;
        case BlockType::SIGN_ITEM:
            top = {10, 2}; side = {10, 2}; bottom = {10, 2}; // items.png
            break;
        case BlockType::CAKE_ITEM:
            top = {13, 1}; side = {13, 1}; bottom = {13, 1}; // items.png
            break;
        case BlockType::REDSTONE_WIRE:
            top = {4, 10}; side = {4, 10}; bottom = {4, 10}; // terrain.png (164)
            break;
        case BlockType::RAIL:
            top = {0, 8}; side = {0, 8}; bottom = {0, 8}; // terrain.png (128); la curva es (0,7)
            break;
        case BlockType::RAIL_POWERED:
            top = {3, 11}; side = {3, 11}; bottom = {3, 11}; // terrain.png (179 encendido); apagado (3,10)
            break;
        case BlockType::RAIL_DETECTOR:
            top = {3, 12}; side = {3, 12}; bottom = {3, 12}; // terrain.png (195)
            break;
        case BlockType::MINECART:
            top = {7, 8}; side = {7, 8}; bottom = {7, 8}; // items.png
            break;
        case BlockType::MINECART_CHEST:
            top = {7, 9}; side = {7, 9}; bottom = {7, 9}; // items.png
            break;
        case BlockType::MINECART_FURNACE:
            top = {7, 10}; side = {7, 10}; bottom = {7, 10}; // items.png
            break;
        case BlockType::BOAT:
            top = {8, 8}; side = {8, 8}; bottom = {8, 8}; // items.png
            break;
        case BlockType::LEATHER:
            top = {7, 6}; side = {7, 6}; bottom = {7, 6}; // items.png
            break;
        case BlockType::PORK_RAW:
            top = {7, 5}; side = {7, 5}; bottom = {7, 5}; // items.png
            break;
        case BlockType::PORK_COOKED:
            top = {8, 5}; side = {8, 5}; bottom = {8, 5}; // items.png
            break;
        case BlockType::FEATHER:
            top = {8, 1}; side = {8, 1}; bottom = {8, 1}; // items.png
            break;
        case BlockType::EGG:
            top = {12, 0}; side = {12, 0}; bottom = {12, 0}; // items.png
            break;
        case BlockType::MILK_BUCKET:
            top = {13, 4}; side = {13, 4}; bottom = {13, 4}; // items.png
            break;
        case BlockType::ARROW:
            top = {5, 2}; side = {5, 2}; bottom = {5, 2}; // items.png
            break;
        case BlockType::BOW:
            top = {5, 1}; side = {5, 1}; bottom = {5, 1}; // items.png
            break;
        case BlockType::SNOWBALL:
            top = {14, 0}; side = {14, 0}; bottom = {14, 0}; // items.png
            break;
        case BlockType::BONE:
            top = {12, 1}; side = {12, 1}; bottom = {12, 1}; // items.png
            break;
        case BlockType::STRING:
            top = {8, 0}; side = {8, 0}; bottom = {8, 0}; // items.png
            break;
        case BlockType::GUNPOWDER:
            top = {8, 2}; side = {8, 2}; bottom = {8, 2}; // items.png
            break;
        case BlockType::INK_SAC:
            top = {14, 4}; side = {14, 4}; bottom = {14, 4}; // items.png
            break;
        case BlockType::SLIMEBALL:
            top = {14, 1}; side = {14, 1}; bottom = {14, 1}; // items.png
            break;
        case BlockType::LEVER:
            top = {0, 6}; side = {0, 6}; bottom = {0, 6}; // terrain.png (96): la palanca (el soporte de cobblestone lo pone el dibujo)
            break;
        case BlockType::BUTTON: case BlockType::PLATE_STONE:
            top = {1, 0}; side = {1, 0}; bottom = {1, 0}; // piedra
            break;
        case BlockType::PLATE_WOOD:
            top = {4, 0}; side = {4, 0}; bottom = {4, 0}; // tablones
            break;
        case BlockType::TORCH_REDSTONE_OFF:
            top = {3, 7}; side = {3, 7}; bottom = {3, 7}; // terrain.png (115)
            break;
        case BlockType::TORCH_REDSTONE_ON:
            top = {3, 6}; side = {3, 6}; bottom = {3, 6}; // terrain.png (99)
            break;
        case BlockType::REPEATER_OFF:
            top = {3, 8}; side = {5, 0}; bottom = {3, 7}; // terrain.png (131, 5, 115)
            break;
        case BlockType::REPEATER_ON:
            top = {3, 9}; side = {5, 0}; bottom = {3, 6}; // terrain.png (147, 5, 99)
            break;
        case BlockType::REDSTONE_ORE_LIT:
            top = {3, 3}; side = {3, 3}; bottom = {3, 3}; // terrain.png
            break;
        case BlockType::PISTON_BASE:
            top = {11, 6}; side = {12, 6}; bottom = {13, 6}; // terrain.png (107 frente, 108 lado, 109 atras)
            break;
        case BlockType::PISTON_STICKY:
            top = {10, 6}; side = {12, 6}; bottom = {13, 6}; // terrain.png (106 frente pegajoso)
            break;
        case BlockType::PISTON_HEAD: case BlockType::PISTON_MOVING:
            top = {11, 6}; side = {12, 6}; bottom = {11, 6};
            break;
        case BlockType::REPEATER_ITEM:
            top = {6, 5}; side = {6, 5}; bottom = {6, 5}; // items.png
            break;
        case BlockType::WOOD_PICKAXE:
            top = {0, 6}; side = {0, 6}; bottom = {0, 6}; // items.png
            break;
        case BlockType::STONE_PICKAXE:
            top = {1, 6}; side = {1, 6}; bottom = {1, 6}; // items.png
            break;
	case BlockType::CRAFTING_TABLE:
            top = {11, 2}; side = {12, 3}; bottom = {4, 0}; // bottom = misma que tablones
            break;
	case BlockType::WOOD_SWORD:
            top = {0, 4}; side = {0, 4}; bottom = {0, 4};
            break;
        case BlockType::WOOD_SHOVEL:
            top = {0, 5}; side = {0, 5}; bottom = {0, 5};
            break;
        case BlockType::CROPS:
            top = {8, 5}; side = {8, 5}; bottom = {8, 5}; // terrain.png (88 + etapa): la etapa se elige en Chunk.cpp
            break;
        case BlockType::SEEDS:
            top = {9, 0}; side = {9, 0}; bottom = {9, 0}; // items.png (9)
            break;
        case BlockType::BLOCK_IRON:    top = {6, 1}; side = {6, 1}; bottom = {6, 1}; break;
        case BlockType::BLOCK_GOLD:    top = {7, 1}; side = {7, 1}; bottom = {7, 1}; break;
        case BlockType::BLOCK_DIAMOND: top = {8, 1}; side = {8, 1}; bottom = {8, 1}; break;
        case BlockType::BLOCK_LAPIS:   top = {0, 9}; side = {0, 9}; bottom = {0, 9}; break;
        case BlockType::BOOKSHELF:     top = {4, 0}; side = {3, 2}; bottom = {4, 0}; break;   // BlockBookshelf: caras de arriba y abajo (lados <= 1) usan los tablones
        case BlockType::SPONGE:        top = {0, 3}; side = {0, 3}; bottom = {0, 3}; break;
        case BlockType::GOLD_SWORD:    top = {4, 4}; side = {4, 4}; bottom = {4, 4}; break;
        case BlockType::GOLD_SHOVEL:   top = {4, 5}; side = {4, 5}; bottom = {4, 5}; break;
        case BlockType::GOLD_PICKAXE:  top = {4, 6}; side = {4, 6}; bottom = {4, 6}; break;
        case BlockType::GOLD_AXE:      top = {4, 7}; side = {4, 7}; bottom = {4, 7}; break;
        case BlockType::HOE_GOLD:      top = {4, 8}; side = {4, 8}; bottom = {4, 8}; break;
        case BlockType::SUGAR:         top = {13, 0}; side = {13, 0}; bottom = {13, 0}; break;
        case BlockType::PAPER:         top = {10, 3}; side = {10, 3}; bottom = {10, 3}; break;
        case BlockType::BOOK:          top = {11, 3}; side = {11, 3}; bottom = {11, 3}; break;
        case BlockType::APPLE:         top = {10, 0}; side = {10, 0}; bottom = {10, 0}; break;
        case BlockType::GOLDEN_APPLE:  top = {11, 0}; side = {11, 0}; bottom = {11, 0}; break;
        case BlockType::FISH_RAW:      top = {9, 5}; side = {9, 5}; bottom = {9, 5}; break;
        case BlockType::FISH_COOKED:   top = {10, 5}; side = {10, 5}; bottom = {10, 5}; break;
        case BlockType::DYE_COCOA:     top = {14, 7}; side = {14, 7}; bottom = {14, 7}; break;
        case BlockType::BRICK_BLOCK: top = {7, 0}; side = {7, 0}; bottom = {7, 0}; break;
        case BlockType::SNOW_BLOCK:  top = {2, 4}; side = {2, 4}; bottom = {2, 4}; break;
        case BlockType::ICE:         top = {3, 4}; side = {3, 4}; bottom = {3, 4}; break;
        case BlockType::WEB:         top = {11, 0}; side = {11, 0}; bottom = {11, 0}; break;
        case BlockType::DYE_PURPLE:     top = {14, 9};  side = {14, 9};  bottom = {14, 9};  break;
        case BlockType::DYE_CYAN:       top = {14, 10}; side = {14, 10}; bottom = {14, 10}; break;
        case BlockType::DYE_LIGHT_GRAY: top = {14, 11}; side = {14, 11}; bottom = {14, 11}; break;
        case BlockType::DYE_GRAY:       top = {15, 4};  side = {15, 4};  bottom = {15, 4};  break;
        case BlockType::DYE_PINK:       top = {15, 5};  side = {15, 5};  bottom = {15, 5};  break;
        case BlockType::DYE_LIME:       top = {15, 6};  side = {15, 6};  bottom = {15, 6};  break;
        case BlockType::DYE_LIGHT_BLUE: top = {15, 8};  side = {15, 8};  bottom = {15, 8};  break;
        case BlockType::DYE_MAGENTA:    top = {15, 9};  side = {15, 9};  bottom = {15, 9};  break;
        case BlockType::DYE_ORANGE:     top = {15, 10}; side = {15, 10}; bottom = {15, 10}; break;
        case BlockType::BONE_MEAL:      top = {15, 11}; side = {15, 11}; bottom = {15, 11}; break;
        case BlockType::JUKEBOX:        top = {11, 4};  side = {10, 4};  bottom = {10, 4};  break;   // BlockJukeBox: blockIndexInTexture 74, arriba +1
        case BlockType::PAINTING:       top = {10, 1};  side = {10, 1};  bottom = {10, 1};  break;
        case BlockType::RECORD_13:      top = {0, 15};  side = {0, 15};  bottom = {0, 15};  break;
        case BlockType::RECORD_CAT:     top = {1, 15};  side = {1, 15};  bottom = {1, 15};  break;
        case BlockType::COMPASS:        top = {6, 3};   side = {6, 3};   bottom = {6, 3};   break;
        case BlockType::CLOCK:          top = {6, 4};   side = {6, 4};   bottom = {6, 4};   break;
        case BlockType::MAP:            top = {12, 3};  side = {12, 3};  bottom = {12, 3};  break;
        case BlockType::SADDLE:         top = {8, 6};   side = {8, 6};   bottom = {8, 6};   break;
        case BlockType::FISHING_ROD:    top = {5, 4};   side = {5, 4};   bottom = {5, 4};   break;
        case BlockType::HELMET_LEATHER: case BlockType::CHESTPLATE_LEATHER: case BlockType::LEGGINGS_LEATHER: case BlockType::BOOTS_LEATHER:
        case BlockType::HELMET_IRON: case BlockType::CHESTPLATE_IRON: case BlockType::LEGGINGS_IRON: case BlockType::BOOTS_IRON:
        case BlockType::HELMET_DIAMOND: case BlockType::CHESTPLATE_DIAMOND: case BlockType::LEGGINGS_DIAMOND: case BlockType::BOOTS_DIAMOND:
        case BlockType::HELMET_GOLD: case BlockType::CHESTPLATE_GOLD: case BlockType::LEGGINGS_GOLD: case BlockType::BOOTS_GOLD:
            { const TextureCell ac = { armorRenderIndex(type), armorPiece(type) }; top = ac; side = ac; bottom = ac; }   // items.png: columna = material, fila = pieza
            break;
        case BlockType::BOWL:
            top = {7, 4}; side = {7, 4}; bottom = {7, 4}; // items.png
            break;
        case BlockType::MUSHROOM_STEW:
            top = {8, 4}; side = {8, 4}; bottom = {8, 4}; // items.png
            break;
        case BlockType::COOKIE:
            top = {12, 5}; side = {12, 5}; bottom = {12, 5}; // items.png
            break;
        case BlockType::BREAD:
            top = {9, 2}; side = {9, 2}; bottom = {9, 2}; // items.png (41)
            break;
        case BlockType::WHEAT:
            top = {9, 1}; side = {9, 1}; bottom = {9, 1}; // items.png (25)
            break;
        case BlockType::FARMLAND:
            top = {7, 5}; side = {2, 0}; bottom = {2, 0}; // terrain.png: seca (87); la mojada es (6,5) (Chunk.cpp); los lados y el fondo son tierra
            break;
        case BlockType::HOE_WOOD:
            top = {0, 8}; side = {0, 8}; bottom = {0, 8}; // items.png: la fila de abajo del hacha
            break;
        case BlockType::HOE_STONE:
            top = {1, 8}; side = {1, 8}; bottom = {1, 8};
            break;
        case BlockType::HOE_IRON:
            top = {2, 8}; side = {2, 8}; bottom = {2, 8};
            break;
        case BlockType::HOE_DIAMOND:
            top = {3, 8}; side = {3, 8}; bottom = {3, 8};
            break;
        case BlockType::WOOD_AXE:
            top = {0, 7}; side = {0, 7}; bottom = {0, 7}; // corregido: fila 7
            break;
        case BlockType::STONE_SWORD:
            top = {1, 4}; side = {1, 4}; bottom = {1, 4};
            break;
        case BlockType::STONE_SHOVEL:
            top = {1, 5}; side = {1, 5}; bottom = {1, 5};
            break;
        case BlockType::STONE_AXE:
            top = {1, 7}; side = {1, 7}; bottom = {1, 7}; // corregido: fila 7
            break;
	case BlockType::COAL_ORE:
            top = {2, 2}; side = {2, 2}; bottom = {2, 2};
            break;
        case BlockType::COAL:
            top = {7, 0}; side = {7, 0}; bottom = {7, 0}; // items.png
            break;
	case BlockType::SAND:
            top = {2, 1}; side = {2, 1}; bottom = {2, 1};
            break;
        case BlockType::GRAVEL:
            top = {3, 1}; side = {3, 1}; bottom = {3, 1};
            break;
        case BlockType::WATER:
        case BlockType::WATER_FLOW: // la Beta no tiene animación aquí: la misma celda para quieta y corriente
            top = {13, 12}; side = {13, 12}; bottom = {13, 12};
            break;
        case BlockType::LAVA:
        case BlockType::LAVA_FLOW:
            top = {13, 14}; side = {13, 14}; bottom = {13, 14};
            break;
        case BlockType::OBSIDIAN:
            top = {5, 2}; side = {5, 2}; bottom = {5, 2};
            break;
        case BlockType::CACTUS:
            top = {5, 4}; side = {6, 4}; bottom = {7, 4};
            break;
	case BlockType::TORCH:
        case BlockType::TORCH_W:
        case BlockType::TORCH_E:
        case BlockType::TORCH_N:
        case BlockType::TORCH_S:
        case BlockType::TORCH_F:
            top = {0, 5}; side = {0, 5}; bottom = {0, 5};
            break;
	case BlockType::SNOW_LAYER:
            top = {2, 4}; side = {2, 4}; bottom = {2, 4};
            break;
	case BlockType::SPRUCE_LOG:
            top = {5, 1}; side = {4, 7}; bottom = {5, 1};
            break;
        case BlockType::SPRUCE_LEAVES:
            top = {5, 8}; side = {5, 8}; bottom = {5, 8};   // antes {4, 8}
            break;
	case BlockType::DEAD_BUSH:
            top = {7, 3}; side = {7, 3}; bottom = {7, 3};
            break;
	case BlockType::SANDSTONE:
            top = {0, 11}; side = {0, 12}; bottom = {0, 13};
            break;
	case BlockType::ROSE:
            top = {12, 0}; side = {12, 0}; bottom = {12, 0};
            break;
        case BlockType::DANDELION:
            top = {13, 0}; side = {13, 0}; bottom = {13, 0};
            break;
        case BlockType::SAPLING_OAK:
            top = {15, 0}; side = {15, 0}; bottom = {15, 0};
            break;
        case BlockType::SAPLING_SPRUCE:
            top = {15, 3}; side = {15, 3}; bottom = {15, 3};
            break;
        case BlockType::SAPLING_BIRCH:
            top = {15, 4}; side = {15, 4}; bottom = {15, 4};
            break;
        case BlockType::DYE_RED:
            top = {14, 5}; side = {14, 5}; bottom = {14, 5}; // items.png
            break;
        case BlockType::DYE_YELLOW:
            top = {15, 7}; side = {15, 7}; bottom = {15, 7}; // items.png
            break;
	case BlockType::IRON_ORE:
            top = {1, 2}; side = {1, 2}; bottom = {1, 2};
            break;
	case BlockType::FURNACE:
        case BlockType::FURNACE_W:
        case BlockType::FURNACE_N:
        case BlockType::FURNACE_E:
            top = FURNACE_TOP_CELL; side = FURNACE_FRONT_OFF; bottom = FURNACE_TOP_CELL;
            break;
        case BlockType::FURNACE_LIT_S:
        case BlockType::FURNACE_LIT_W:
        case BlockType::FURNACE_LIT_N:
        case BlockType::FURNACE_LIT_E:
            top = FURNACE_TOP_CELL; side = FURNACE_FRONT_LIT; bottom = FURNACE_TOP_CELL;
            break;
        case BlockType::BUCKET:
            top = {10, 4}; side = {10, 4}; bottom = {10, 4}; // items.png
            break;
        case BlockType::WATER_BUCKET:
            top = {11, 4}; side = {11, 4}; bottom = {11, 4}; // items.png
            break;
        case BlockType::LAVA_BUCKET:
            top = {12, 4}; side = {12, 4}; bottom = {12, 4}; // items.png
            break;
        case BlockType::DIAMOND_ORE:
            top = {2, 3}; side = {2, 3}; bottom = {2, 3}; // terrain.png
            break;
        case BlockType::GOLD_ORE:
            top = {0, 2}; side = {0, 2}; bottom = {0, 2}; // terrain.png
            break;
        case BlockType::REDSTONE_ORE:
            top = {3, 3}; side = {3, 3}; bottom = {3, 3}; // terrain.png
            break;
        case BlockType::LAPIS_ORE:
            top = {0, 10}; side = {0, 10}; bottom = {0, 10}; // terrain.png
            break;
        case BlockType::CLAY:
            top = {8, 4}; side = {8, 4}; bottom = {8, 4}; // terrain.png
            break;
        case BlockType::MUSHROOM_BROWN:
            top = {13, 1}; side = {13, 1}; bottom = {13, 1}; // terrain.png
            break;
        case BlockType::MUSHROOM_RED:
            top = {12, 1}; side = {12, 1}; bottom = {12, 1}; // terrain.png
            break;
        case BlockType::SUGAR_CANE:
            top = {9, 4}; side = {9, 4}; bottom = {9, 4}; // terrain.png
            break;
        case BlockType::PUMPKIN:
            top = PUMPKIN_TOP_CELL; side = PUMPKIN_FRONT_CELL; bottom = PUMPKIN_TOP_CELL;   // el icono muestra la cara tallada
            break;
        case BlockType::JACK_O_LANTERN:
            top = PUMPKIN_TOP_CELL; side = JACK_FRONT_CELL; bottom = PUMPKIN_TOP_CELL;
            break;
        case BlockType::SUGAR_CANE_ITEM:
            top = {11, 1}; side = {11, 1}; bottom = {11, 1}; // items.png
            break;
        case BlockType::CLAY_BALL:
            top = {9, 3}; side = {9, 3}; bottom = {9, 3}; // items.png
            break;
        case BlockType::BRICK:
            top = {6, 1}; side = {6, 1}; bottom = {6, 1}; // items.png
            break;
        case BlockType::GOLD_INGOT:
            top = {7, 2}; side = {7, 2}; bottom = {7, 2}; // items.png
            break;
        case BlockType::REDSTONE:
            top = {8, 3}; side = {8, 3}; bottom = {8, 3}; // items.png
            break;
        case BlockType::DYE_LAPIS:
            top = {14, 8}; side = {14, 8}; bottom = {14, 8}; // items.png (tinte, damage 4)
            break;
        case BlockType::DIAMOND:
            top = {7, 3}; side = {7, 3}; bottom = {7, 3}; // items.png
            break;
        case BlockType::DIAMOND_SWORD:
            top = {3, 4}; side = {3, 4}; bottom = {3, 4}; // items.png
            break;
        case BlockType::DIAMOND_SHOVEL:
            top = {3, 5}; side = {3, 5}; bottom = {3, 5};
            break;
        case BlockType::DIAMOND_PICKAXE:
            top = {3, 6}; side = {3, 6}; bottom = {3, 6};
            break;
        case BlockType::DIAMOND_AXE:
            top = {3, 7}; side = {3, 7}; bottom = {3, 7};
            break;
        case BlockType::FIRE:
            top = {15, 1}; side = {15, 1}; bottom = {15, 1}; // terrain.png: primera capa; la segunda es (15,2)
            break;
        case BlockType::FLINT:
            top = {6, 0}; side = {6, 0}; bottom = {6, 0}; // items.png
            break;
        case BlockType::FLINT_AND_STEEL:
            top = {5, 0}; side = {5, 0}; bottom = {5, 0}; // items.png
            break;
        case BlockType::SHEARS:
            top = {13, 5}; side = {13, 5}; bottom = {13, 5}; // items.png (Item.shears.setIconCoord(13, 5))
            break;
        case BlockType::PORTAL:
            top = {14, 0}; side = {14, 0}; bottom = {14, 0}; // terrain.png (un solo cuadro: sin animación)
            break;
        case BlockType::NETHERRACK:
            top = {7, 6}; side = {7, 6}; bottom = {7, 6}; // terrain.png
            break;
        case BlockType::TNT:
            top = {9, 0}; side = {8, 0}; bottom = {10, 0}; // terrain.png
            break;
        case BlockType::NOTE_BLOCK:
            top = {10, 4}; side = {10, 4}; bottom = {10, 4}; // terrain.png
            break;
        case BlockType::DISPENSER: case BlockType::DISPENSER_W: case BlockType::DISPENSER_N: case BlockType::DISPENSER_E:
            top = {14, 3}; side = DISPENSER_FRONT_CELL; bottom = {14, 3}; // terrain.png; el icono muestra el frente
            break;
        case BlockType::SOUL_SAND:
            top = {8, 6}; side = {8, 6}; bottom = {8, 6}; // terrain.png
            break;
        case BlockType::GLOWSTONE:
            top = {9, 6}; side = {9, 6}; bottom = {9, 6}; // terrain.png
            break;
        case BlockType::GLOWSTONE_DUST:
            top = {9, 4}; side = {9, 4}; bottom = {9, 4}; // items.png
            break;
        case BlockType::CHEST:
            top = {9, 1}; side = {11, 1}; bottom = {9, 1}; // el lado es el frente (índice 27): así se reconoce en el inventario
            break;
	case BlockType::IRON_INGOT:
            top = {7, 1}; side = {7, 1}; bottom = {7, 1}; // items.png
            break;
        case BlockType::CHARCOAL:
            top = {7, 0}; side = {7, 0}; bottom = {7, 0}; // items.png (igual que el carbón)
            break;
        case BlockType::DYE_GREEN:
            top = {14, 6}; side = {14, 6}; bottom = {14, 6}; // items.png
            break;
	case BlockType::GLASS:
            top = {1, 3}; side = {1, 3}; bottom = {1, 3};
            break;
	case BlockType::IRON_PICKAXE:
            top = {2, 6}; side = {2, 6}; bottom = {2, 6}; // items.png
            break;
        case BlockType::IRON_SWORD:
            top = {2, 4}; side = {2, 4}; bottom = {2, 4}; // items.png
            break;
        case BlockType::IRON_SHOVEL:
            top = {2, 5}; side = {2, 5}; bottom = {2, 5}; // items.png
            break;
        case BlockType::IRON_AXE:
            top = {2, 7}; side = {2, 7}; bottom = {2, 7}; // items.png
            break;
	case BlockType::BEDROCK:
            top = {1, 1}; side = {1, 1}; bottom = {1, 1};
            break;
        default:
            top = {0, 0}; side = {0, 0}; bottom = {0, 0};
            break;
    }
}

// Luz del cielo que se pierde al atravesar el bloque (Block.lightOpacity): el agua y el hielo restan 3 por bloque
inline int lightDim(BlockType type) {
    return (type == BlockType::WATER || type == BlockType::WATER_FLOW || type == BlockType::ICE) ? 3 : 0;
}

inline bool isLightOpaque(BlockType type) {
    return type != BlockType::AIR && type != BlockType::TALLGRASS &&
           type != BlockType::WATER && type != BlockType::WATER_FLOW && !isTorchBlock(type) && type != BlockType::FIRE && type != BlockType::PORTAL &&
           type != BlockType::SNOW_LAYER && type != BlockType::DEAD_BUSH &&
           !isCrossPlant(type) && !isPartialBlock(type) &&
           type != BlockType::GLASS && type != BlockType::ICE;
}

// Color de los ÍTEMS y de las caras fuera del mundo (Block.getRenderColor): pasto con temperatura 0,5 y humedad 1,0 (0x7CBD6B), hojas de roble
// 0x48B518, de abedul 0x80A755 y de abeto 0x619961. Dentro del mundo el color sale del clima de cada columna (Chunk::columnTint);
// 'isForest' ya no se usa y queda por compatibilidad con quien lo llama.
inline void getGrassTint(BlockType type, bool isForest, u8& r, u8& g, u8& b) {
    (void)isForest;
    if (type == BlockType::GRASS || type == BlockType::TALLGRASS) {
        r = 124; g = 189; b = 107;
    } else if (type == BlockType::OAK_LEAVES) {
        r = 72; g = 181; b = 24;
    } else if (type == BlockType::BIRCH_LEAVES) {
        r = 128; g = 167; b = 85;
    } else if (type == BlockType::SPRUCE_LEAVES) {
        r = 97; g = 153; b = 97;
    } else {
        r = 255; g = 255; b = 255;
    }
}

inline bool requiresPickaxe(BlockType type) {
    return type == BlockType::STONE || type == BlockType::COBBLESTONE || type == BlockType::MOSSY_COBBLESTONE || type == BlockType::MOB_SPAWNER || type == BlockType::COAL_ORE ||
           type == BlockType::SANDSTONE || type == BlockType::IRON_ORE || type == BlockType::DIAMOND_ORE ||
           type == BlockType::GOLD_ORE || type == BlockType::REDSTONE_ORE || type == BlockType::REDSTONE_ORE_LIT || type == BlockType::LAPIS_ORE ||
           type == BlockType::OBSIDIAN || type == BlockType::NETHERRACK || type == BlockType::GLOWSTONE || isFurnaceBlock(type) || isDispenserBlock(type) ||
           isSlabBlock(type) || isDoubleSlabBlock(type) || type == BlockType::STAIRS_COBBLE ||   // BlockStep es de material roca (tambien la de madera)
           type == BlockType::BRICK_BLOCK ||   // Material.rock
           type == BlockType::BLOCK_IRON || type == BlockType::BLOCK_GOLD || type == BlockType::BLOCK_DIAMOND || type == BlockType::BLOCK_LAPIS ||
           type == BlockType::DOOR_IRON;   // ItemPickaxe.canHarvestBlock: el material hierro (la puerta) tambien exige pico
}

inline int pickaxeTier(BlockType tool) {
    if (tool == BlockType::STONE_PICKAXE) return 2;
    if (tool == BlockType::WOOD_PICKAXE || tool == BlockType::GOLD_PICKAXE) return 1;   // oro: nivel de cosecha 0, igual que la madera
    if (tool == BlockType::IRON_PICKAXE) return 3;
    if (tool == BlockType::DIAMOND_PICKAXE) return 4; // nivel de cosecha 3 (EnumToolMaterial.EMERALD) + 1
    if (tool == BlockType::STONE_PICKAXE) return 2;
    return 0;
}

// Nivel mínimo de pico para que el bloque suelte algo
inline int minPickaxeTier(BlockType type) {
    if (type == BlockType::OBSIDIAN) return 4;     // ItemPickaxe.canHarvestBlock: solo con nivel de cosecha 3 (diamante)
    if (type == BlockType::DIAMOND_ORE) return 3;  // nivel de cosecha 2 o más (hierro y diamante)
    if (type == BlockType::IRON_ORE || type == BlockType::BLOCK_IRON || type == BlockType::BLOCK_LAPIS) return 2;
    if (type == BlockType::BLOCK_GOLD || type == BlockType::BLOCK_DIAMOND) return 3;
    if (type == BlockType::GOLD_ORE || type == BlockType::REDSTONE_ORE || type == BlockType::REDSTONE_ORE_LIT) return 3;   // nivel de cosecha 2: hierro o diamante
    if (type == BlockType::LAPIS_ORE) return 2;                                      // nivel de cosecha 1: piedra o mejor
    if (requiresPickaxe(type)) return 1;
    return 0;
}

// =====================================================================
// Velocidad de picado: port de Block.blockStrength (Beta 1.7.3). Cada tick de 20 Hz se suma la "fuerza" y el bloque se rompe al llegar a 1:
//   si el jugador puede cosechar el bloque:  eficacia de la herramienta / dureza / 30   (eficacia / 5 en el agua y / 5 en el aire)
//   si no puede (roca, hierro y nieve exigen la herramienta correcta):  1 / dureza / 100
// =====================================================================

// Block.setHardness; negativa = irrompible. Valores de Block.txt.
inline float blockHardness(BlockType t) {
    if (isWool(t)) return 0.8f;   // Block.cloth
    switch (t) {
        case BlockType::BEDROCK:
        case BlockType::PORTAL:
        case BlockType::PISTON_MOVING:
            return -1.0f;
        case BlockType::PISTON_BASE: case BlockType::PISTON_STICKY: case BlockType::PISTON_HEAD:
            return 0.5f;   // Block.pistonBase / pistonStickyBase / pistonExtension: setHardness(0.5)
        case BlockType::OBSIDIAN:
            return 10.0f;
        case BlockType::BLOCK_IRON: case BlockType::BLOCK_DIAMOND: return 5.0f;
        case BlockType::BLOCK_GOLD: case BlockType::BLOCK_LAPIS: return 3.0f;
        case BlockType::BOOKSHELF: return 1.5f;
        case BlockType::BRICK_BLOCK: return 2.0f;
        case BlockType::SNOW_BLOCK: return 0.2f;
        case BlockType::ICE: return 0.5f;
        case BlockType::WEB: return 4.0f;
        case BlockType::SPONGE: return 0.6f;
        case BlockType::PUMPKIN:
        case BlockType::JACK_O_LANTERN:
            return 1.0f;   // Block.pumpkin y Block.pumpkinLantern
        case BlockType::FURNACE: case BlockType::FURNACE_W: case BlockType::FURNACE_N: case BlockType::FURNACE_E:
        case BlockType::FURNACE_LIT_S: case BlockType::FURNACE_LIT_W: case BlockType::FURNACE_LIT_N: case BlockType::FURNACE_LIT_E:
        case BlockType::DISPENSER: case BlockType::DISPENSER_W: case BlockType::DISPENSER_N: case BlockType::DISPENSER_E:
            return 3.5f;   // Block.dispenser: setHardness(3.5)
        case BlockType::TNT:
            return 0.0f;   // Block.tnt: setHardness(0)
        case BlockType::NOTE_BLOCK:
            return 0.8f;   // Block.music: setHardness(0.8)
        case BlockType::JUKEBOX:
            return 2.0f;   // Block.jukebox: setHardness(2.0)
        case BlockType::COAL_ORE: case BlockType::IRON_ORE: case BlockType::DIAMOND_ORE:
        case BlockType::GOLD_ORE: case BlockType::REDSTONE_ORE: case BlockType::REDSTONE_ORE_LIT: case BlockType::LAPIS_ORE:
            return 3.0f;
        case BlockType::LEVER: case BlockType::BUTTON: case BlockType::PLATE_STONE: case BlockType::PLATE_WOOD:
            return 0.5f;   // Block.lever, Block.button y las placas: setHardness(0.5)
        case BlockType::CRAFTING_TABLE: case BlockType::CHEST:
            return 2.5f;
        case BlockType::MOB_SPAWNER: case BlockType::DOOR_IRON:
            return 5.0f;
        case BlockType::DOOR_WOOD: case BlockType::TRAPDOOR:
            return 3.0f;
        case BlockType::BED_BLOCK:
            return 0.2f;
        case BlockType::SIGN_POST: case BlockType::SIGN_WALL:
            return 1.0f;
        case BlockType::SLAB_STONE: case BlockType::SLAB_SAND: case BlockType::SLAB_WOOD: case BlockType::SLAB_COBBLE:
        case BlockType::DOUBLE_SLAB_STONE: case BlockType::DOUBLE_SLAB_SAND: case BlockType::DOUBLE_SLAB_WOOD: case BlockType::DOUBLE_SLAB_COBBLE:
        case BlockType::STAIRS_WOOD: case BlockType::STAIRS_COBBLE: case BlockType::FENCE:
            return 2.0f;   // Block.stairSingle / stairDouble: 2; las escaleras y la valla copian la dureza de su material
        case BlockType::LADDER:
            return 0.4f;
        case BlockType::RAIL: case BlockType::RAIL_POWERED: case BlockType::RAIL_DETECTOR:
            return 0.7f;   // Block.rail / railPowered / railDetector: setHardness(0.7)
        case BlockType::CAKE_BLOCK:
            return 0.5f;
        case BlockType::FARMLAND:
            return 0.6f;   // Block.tilledField: setHardness(0.6)
        case BlockType::COBBLESTONE: case BlockType::MOSSY_COBBLESTONE: case BlockType::WOOD_PLANKS:
        case BlockType::OAK_LOG: case BlockType::BIRCH_LOG: case BlockType::SPRUCE_LOG:
            return 2.0f;
        case BlockType::STONE:
            return 1.5f;
        case BlockType::SANDSTONE:
            return 0.8f;
        case BlockType::GRASS: case BlockType::GRAVEL: case BlockType::CLAY:
            return 0.6f;
        case BlockType::DIRT: case BlockType::SAND: case BlockType::SOUL_SAND:
            return 0.5f;
        case BlockType::CACTUS: case BlockType::NETHERRACK:
            return 0.4f;
        case BlockType::GLASS: case BlockType::GLOWSTONE:
            return 0.3f;
        case BlockType::OAK_LEAVES: case BlockType::BIRCH_LEAVES: case BlockType::SPRUCE_LEAVES:
            return 0.2f;
        case BlockType::SNOW_LAYER:
            return 0.1f;
        case BlockType::WATER: case BlockType::WATER_FLOW: case BlockType::LAVA: case BlockType::LAVA_FLOW:
            return 100.0f;
        default:   // plantas, flores, pasto alto, arbusto seco, antorchas y fuego: dureza 0 (se rompen al instante)
            return 0.0f;
    }
}

inline bool isLeavesBlock(BlockType t) { return t == BlockType::OAK_LEAVES || t == BlockType::BIRCH_LEAVES || t == BlockType::SPRUCE_LEAVES; }

inline bool isAxeTool(BlockType t) {
    return t == BlockType::WOOD_AXE || t == BlockType::STONE_AXE || t == BlockType::IRON_AXE || t == BlockType::DIAMOND_AXE || t == BlockType::GOLD_AXE;
}
// Objetos de la granja (semillas y trigo): solo se llevan; las semillas se plantan con Partial::useSeeds
inline bool isFarmItem(BlockType t) { return t == BlockType::SEEDS || t == BlockType::WHEAT; }
inline bool isHoeTool(BlockType t) {
    return t == BlockType::HOE_WOOD || t == BlockType::HOE_STONE || t == BlockType::HOE_IRON || t == BlockType::HOE_DIAMOND || t == BlockType::HOE_GOLD;
}
inline bool isShovelTool(BlockType t) {
    return t == BlockType::WOOD_SHOVEL || t == BlockType::STONE_SHOVEL || t == BlockType::IRON_SHOVEL || t == BlockType::DIAMOND_SHOVEL || t == BlockType::GOLD_SHOVEL;
}

// EnumToolMaterial.getEfficiencyOnProperMaterial: madera 2, piedra 4, hierro 6, diamante 8, oro 12
inline float toolEfficiency(BlockType tool) {
    switch (tool) {
        case BlockType::WOOD_PICKAXE: case BlockType::WOOD_AXE: case BlockType::WOOD_SHOVEL:         return 2.0f;
        case BlockType::STONE_PICKAXE: case BlockType::STONE_AXE: case BlockType::STONE_SHOVEL:      return 4.0f;
        case BlockType::IRON_PICKAXE: case BlockType::IRON_AXE: case BlockType::IRON_SHOVEL:         return 6.0f;
        case BlockType::DIAMOND_PICKAXE: case BlockType::DIAMOND_AXE: case BlockType::DIAMOND_SHOVEL: return 8.0f;
        case BlockType::GOLD_PICKAXE: case BlockType::GOLD_AXE: case BlockType::GOLD_SHOVEL:         return 12.0f;
        default: return 1.0f;
    }
}

inline bool isSword(BlockType type);    // se definen más abajo
inline bool isPickaxe(BlockType type);

// ItemStack.getStrVsBlock. Cada herramienta solo es rápida en su lista (ItemPickaxe, ItemAxe, ItemSpade); en lo demás vale 1, como la mano.
// La espada (ItemSword) vale 1,5 con todo. Ojo: la obsidiana, el horno y la piedra luminosa no están en la lista del pico, ni la mesa de crafteo
// en la del hacha, ni la arena de almas en la de la pala.
inline float toolStrengthVsBlock(BlockType tool, BlockType block) {
    if (isSword(tool)) return block == BlockType::WEB ? 15.0f : 1.5f;   // ItemSword.getStrVsBlock: la telaraña 15
    if (tool == BlockType::SHEARS) return (isLeavesBlock(block) || block == BlockType::WEB) ? 15.0f : (isWool(block) ? 5.0f : 1.0f);   // ItemShears.getStrVsBlock: telaraña y hojas 15, lana 5
    if (isPickaxe(tool)) {
        if (block == BlockType::STONE || block == BlockType::COBBLESTONE || block == BlockType::MOSSY_COBBLESTONE || block == BlockType::MOB_SPAWNER || block == BlockType::SANDSTONE || block == BlockType::IRON_ORE ||
            block == BlockType::COAL_ORE || block == BlockType::DIAMOND_ORE || block == BlockType::NETHERRACK ||
            block == BlockType::GOLD_ORE || block == BlockType::REDSTONE_ORE || block == BlockType::REDSTONE_ORE_LIT || block == BlockType::LAPIS_ORE ||
            block == BlockType::BLOCK_IRON || block == BlockType::BLOCK_GOLD || block == BlockType::BLOCK_DIAMOND || block == BlockType::BLOCK_LAPIS || block == BlockType::ICE ||
            isDispenserBlock(block) || isSlabBlock(block) || isDoubleSlabBlock(block)) return toolEfficiency(tool);   // ItemPickaxe: stairSingle y stairDouble estan en su lista
    } else if (isAxeTool(tool)) {
        if (block == BlockType::WOOD_PLANKS || block == BlockType::OAK_LOG || block == BlockType::BIRCH_LOG || block == BlockType::SPRUCE_LOG ||
            block == BlockType::CHEST || block == BlockType::NOTE_BLOCK || block == BlockType::BOOKSHELF || block == BlockType::JUKEBOX) return toolEfficiency(tool);
    } else if (isShovelTool(tool)) {
        if (block == BlockType::GRASS || block == BlockType::DIRT || block == BlockType::SAND || block == BlockType::GRAVEL || block == BlockType::CLAY ||
            block == BlockType::SNOW_LAYER || block == BlockType::SNOW_BLOCK || block == BlockType::FARMLAND) return toolEfficiency(tool);
    }
    return 1.0f;
}

// InventoryPlayer.canHarvestBlock: la roca exige pico (y con nivel para algunos minerales), la nieve exige pala
inline bool canHarvestBlock(BlockType block, BlockType tool) {
    if (requiresPickaxe(block)) return pickaxeTier(tool) >= minPickaxeTier(block);
    if (block == BlockType::SNOW_LAYER || block == BlockType::SNOW_BLOCK) return isShovelTool(tool);   // ItemSpade.canHarvestBlock
    return true;
}

// Block.blockStrength: lo que se suma al daño del bloque en cada tick (se rompe al llegar a 1). 0 = irrompible.
inline float getBlockStrength(BlockType block, BlockType tool, bool onGround, bool headInWater) {
    const float h = blockHardness(block);
    if (h < 0.0f) return 0.0f;
    if (!canHarvestBlock(block, tool)) return h == 0.0f ? 1.0e9f : 1.0f / h / 100.0f;
    float strength = toolStrengthVsBlock(tool, block);
    if (headInWater) strength /= 5.0f;   // EntityPlayer.getCurrentPlayerStrVsBlock
    if (!onGround) strength /= 5.0f;
    return h == 0.0f ? 1.0e9f : strength / h / 30.0f;
}

// Daño de un golpe a un ser vivo (EntityPlayer.attackTargetEntityWithCurrentItem): ItemSword = 4 + 2 x material; ItemTool = base (pico 2, hacha 3,
// pala 1) + material; la mano y cualquier otro ítem, 1. Material: madera 0, piedra 1, hierro 2, diamante 3.
inline int getDamageVsEntity(BlockType tool) {
    switch (tool) {
        case BlockType::WOOD_SWORD: return 4;   case BlockType::STONE_SWORD: return 6;   case BlockType::IRON_SWORD: return 8;   case BlockType::DIAMOND_SWORD: return 10; case BlockType::GOLD_SWORD: return 4;
        case BlockType::WOOD_PICKAXE: return 2; case BlockType::STONE_PICKAXE: return 3; case BlockType::IRON_PICKAXE: return 4; case BlockType::DIAMOND_PICKAXE: return 5; case BlockType::GOLD_PICKAXE: return 2;
        case BlockType::WOOD_AXE: return 3;     case BlockType::STONE_AXE: return 4;     case BlockType::IRON_AXE: return 5;     case BlockType::DIAMOND_AXE: return 6; case BlockType::GOLD_AXE: return 3;
        case BlockType::WOOD_SHOVEL: return 1;  case BlockType::STONE_SHOVEL: return 2;  case BlockType::IRON_SHOVEL: return 3;  case BlockType::DIAMOND_SHOVEL: return 4; case BlockType::GOLD_SHOVEL: return 1;
        default: return 1;
    }
}

inline bool usesItemAtlas(BlockType type) {
    return type == BlockType::STICK || type == BlockType::COAL ||
           type == BlockType::WOOD_PICKAXE || type == BlockType::STONE_PICKAXE ||
           type == BlockType::WOOD_SWORD   || type == BlockType::STONE_SWORD ||
           type == BlockType::WOOD_SHOVEL  || type == BlockType::STONE_SHOVEL ||
           type == BlockType::WOOD_AXE     || type == BlockType::STONE_AXE ||
           type == BlockType::DYE_RED      || type == BlockType::DYE_YELLOW ||
           type == BlockType::IRON_INGOT   || type == BlockType::CHARCOAL ||
           type == BlockType::DYE_GREEN    ||
           type == BlockType::IRON_PICKAXE || type == BlockType::IRON_SWORD ||
           type == BlockType::IRON_SHOVEL  || type == BlockType::IRON_AXE ||
           type == BlockType::BUCKET       || type == BlockType::WATER_BUCKET || type == BlockType::LAVA_BUCKET ||
           type == BlockType::DIAMOND      || type == BlockType::DIAMOND_PICKAXE || type == BlockType::DIAMOND_SWORD ||
           type == BlockType::DIAMOND_SHOVEL || type == BlockType::DIAMOND_AXE || isHoeTool(type) || isFarmItem(type) ||
           type == BlockType::FLINT        || type == BlockType::FLINT_AND_STEEL ||
           type == BlockType::GLOWSTONE_DUST || type == BlockType::SHEARS ||
           type == BlockType::GOLD_INGOT || type == BlockType::REDSTONE || type == BlockType::DYE_LAPIS ||
           type == BlockType::CLAY_BALL || type == BlockType::BRICK || type == BlockType::SUGAR_CANE_ITEM ||
           type == BlockType::DOOR_WOOD_ITEM || type == BlockType::DOOR_IRON_ITEM || type == BlockType::BED_ITEM ||
           type == BlockType::SIGN_ITEM || type == BlockType::CAKE_ITEM || type == BlockType::REPEATER_ITEM || isMinecartItem(type) || type == BlockType::BOAT || isAnimalItem(type) || isMiscItem(type) || isGoldTool(type);
}

// Balde vacío, de agua y de lava (Item.bucketEmpty / bucketWater / bucketLava)
inline bool isBucket(BlockType type) {
    return type == BlockType::BUCKET || type == BlockType::WATER_BUCKET || type == BlockType::LAVA_BUCKET;
}

// Herramientas de diamante (EnumToolMaterial.EMERALD)
inline bool isDiamondTool(BlockType type) {
    return type == BlockType::DIAMOND_PICKAXE || type == BlockType::DIAMOND_SWORD ||
           type == BlockType::DIAMOND_SHOVEL  || type == BlockType::DIAMOND_AXE;
}

inline bool isPlaceable(BlockType type) {
    return type != BlockType::STICK && type != BlockType::COAL && !isLiquidBlock(type) &&
           type != BlockType::WOOD_PICKAXE && type != BlockType::STONE_PICKAXE &&
           type != BlockType::WOOD_SWORD   && type != BlockType::STONE_SWORD &&
           type != BlockType::WOOD_SHOVEL  && type != BlockType::STONE_SHOVEL &&
           type != BlockType::WOOD_AXE     && type != BlockType::STONE_AXE &&
           type != BlockType::DYE_RED      && type != BlockType::DYE_YELLOW &&
           type != BlockType::IRON_INGOT   && type != BlockType::CHARCOAL &&
           type != BlockType::DYE_GREEN    &&
           type != BlockType::IRON_PICKAXE && type != BlockType::IRON_SWORD &&
           type != BlockType::IRON_SHOVEL  && type != BlockType::IRON_AXE &&
           !isBucket(type) && type != BlockType::DIAMOND && !isDiamondTool(type) &&
           type != BlockType::FIRE && type != BlockType::FLINT && type != BlockType::FLINT_AND_STEEL &&
           type != BlockType::PORTAL && type != BlockType::GLOWSTONE_DUST && type != BlockType::SHEARS && !isHoeTool(type) && !isFarmItem(type) &&
           type != BlockType::GOLD_INGOT && type != BlockType::REDSTONE && type != BlockType::DYE_LAPIS &&
           type != BlockType::CLAY_BALL && type != BlockType::BRICK &&
           type != BlockType::DOOR_WOOD_ITEM && type != BlockType::DOOR_IRON_ITEM && type != BlockType::BED_ITEM &&
           type != BlockType::SIGN_ITEM && type != BlockType::CAKE_ITEM &&
           type != BlockType::REPEATER_ITEM && type != BlockType::LEVER && type != BlockType::BUTTON && !isPlateBlock(type) &&
           !isRedstoneTorch(type) && type != BlockType::REDSTONE_WIRE && !isPistonBlock(type) && !isRailBlock(type) && !isMinecartItem(type) && type != BlockType::BOAT && !isAnimalItem(type) && !isMiscItem(type) && !isGoldTool(type);   // puertas, cama, cartel y pastel: Partial::useItem   // el polvo de redstone se podra poner cuando haya cables
}

// Con durabilidad guardada (herramientas y armaduras). isTool se queda solo para lo que se usa como herramienta (golpear, picar)
inline bool isTool(BlockType type);
inline bool hasDurability(BlockType type) { return isTool(type) || isArmor(type) || type == BlockType::FISHING_ROD; }
inline bool isTool(BlockType type) {
    return type == BlockType::WOOD_PICKAXE || type == BlockType::STONE_PICKAXE ||
           type == BlockType::WOOD_SWORD   || type == BlockType::STONE_SWORD ||
           type == BlockType::WOOD_SHOVEL  || type == BlockType::STONE_SHOVEL ||
           type == BlockType::WOOD_AXE     || type == BlockType::STONE_AXE ||
           type == BlockType::IRON_PICKAXE || type == BlockType::IRON_SWORD ||
           type == BlockType::IRON_SHOVEL  || type == BlockType::IRON_AXE ||
           isDiamondTool(type) || isGoldTool(type) || isHoeTool(type) ||   // ItemHoe: maxStackSize 1 y con durabilidad
           type == BlockType::FLINT_AND_STEEL || // con durabilidad y de a uno; no gasta al picar (main.cpp lo excluye)
           type == BlockType::SHEARS;            // con durabilidad y de a uno; solo gasta al romper hojas (main.cpp)
}

// Item.getItemStackLimit: las herramientas y los baldes van de a uno; el resto, hasta 64
inline int maxStackFor(BlockType type) {
    if (type == BlockType::DOOR_WOOD_ITEM || type == BlockType::DOOR_IRON_ITEM) return 1;   // ItemDoor: maxStackSize = 1
    if (type == BlockType::SIGN_ITEM || type == BlockType::CAKE_ITEM) return 1;             // ItemSign y el pastel: de a uno
    if (isMinecartItem(type) || type == BlockType::BOAT || type == BlockType::MILK_BUCKET) return 1;                                                      // ItemMinecart: maxStackSize = 1
    if (type == BlockType::APPLE || type == BlockType::GOLDEN_APPLE || type == BlockType::FISH_RAW || type == BlockType::FISH_COOKED) return 1;   // ItemFood: de a uno
    if (type == BlockType::COOKIE) return 8;                                                // ItemCookie: se apila de a 8
    if (type == BlockType::MUSHROOM_STEW) return 1;                                         // ItemSoup: de a uno
    if (type == BlockType::PORK_RAW || type == BlockType::PORK_COOKED || type == BlockType::BREAD) return 1;            // ItemFood: maxStackSize = 1
    if (isRecord(type)) return 1;                                                           // ItemRecord: maxStackSize = 1
    if (type == BlockType::BED_ITEM) return 1;                                              // ItemBed: maxStackSize = 1
    if (type == BlockType::BOW) return 1;                                                   // ItemBow: maxStackSize = 1
    if (type == BlockType::MAP || type == BlockType::SADDLE) return 1;                      // ItemMap y ItemSaddle: maxStackSize = 1
    if (type == BlockType::SNOWBALL || type == BlockType::EGG) return 16;                   // ItemSnowball / ItemEgg: maxStackSize = 16
    return (hasDurability(type) || isBucket(type)) ? 1 : 64;
}

// EnumToolMaterial.getMaxUses: madera 59, piedra 131, hierro 250, diamante 1561. La espada gasta 2 por bloque
// (ItemSword.onBlockDestroyed), así que dura la mitad de bloques con el mismo número.
inline int getToolMaxDurability(BlockType type) {
    if (isArmor(type)) return armorMaxDurability(type);   // ItemArmor: setMaxDamage(maxDamageArray * 3 << nivel)
    if (type == BlockType::FISHING_ROD) return 64;        // ItemFishingRod: setMaxDamage(64)
    switch (type) {
        case BlockType::WOOD_PICKAXE:
        case BlockType::WOOD_SHOVEL:
        case BlockType::WOOD_AXE:
        case BlockType::WOOD_SWORD:
        case BlockType::HOE_WOOD:
            return 59;
        case BlockType::STONE_PICKAXE:
        case BlockType::STONE_SHOVEL:
        case BlockType::STONE_AXE:
        case BlockType::STONE_SWORD:
        case BlockType::HOE_STONE:
            return 131;
        case BlockType::IRON_PICKAXE:
        case BlockType::IRON_SHOVEL:
        case BlockType::IRON_AXE:
        case BlockType::IRON_SWORD:
        case BlockType::HOE_IRON:
            return 250;
        case BlockType::DIAMOND_PICKAXE:
        case BlockType::DIAMOND_SHOVEL:
        case BlockType::DIAMOND_AXE:
        case BlockType::DIAMOND_SWORD:
        case BlockType::HOE_DIAMOND:
            return 1561;
        case BlockType::GOLD_PICKAXE: case BlockType::GOLD_SHOVEL: case BlockType::GOLD_AXE: case BlockType::GOLD_SWORD: case BlockType::HOE_GOLD:
            return 32;   // EnumToolMaterial.GOLD
        case BlockType::FLINT_AND_STEEL:
            return 64;   // ItemFlintAndSteel: setMaxDamage(64)
        case BlockType::SHEARS:
            return 238;   // ItemShears.setMaxDamage
        default:
            return 0;
    }
}

inline bool isSword(BlockType type) {
    return type == BlockType::WOOD_SWORD || type == BlockType::STONE_SWORD || type == BlockType::IRON_SWORD ||
           type == BlockType::DIAMOND_SWORD || type == BlockType::GOLD_SWORD;
}

inline bool isPickaxe(BlockType type) {
    return type == BlockType::WOOD_PICKAXE || type == BlockType::STONE_PICKAXE || type == BlockType::IRON_PICKAXE ||
           type == BlockType::DIAMOND_PICKAXE || type == BlockType::GOLD_PICKAXE;
}

inline BlockType getDropType(BlockType minedType) {
    switch (minedType) {
        case BlockType::GRASS: case BlockType::FARMLAND: return BlockType::DIRT;   // BlockFarmland.idDropped: tierra
        case BlockType::DIAMOND_ORE: return BlockType::DIAMOND; // BlockOre.idDropped: la mena suelta el diamante
        case BlockType::SUGAR_CANE: return BlockType::SUGAR_CANE_ITEM;   // el bloque no se obtiene: suelta el objeto
        case BlockType::SNOW_BLOCK: return BlockType::SNOWBALL;   // BlockSnowBlock.idDropped: 4 bolas (main.cpp)
        case BlockType::WEB: return BlockType::STRING;            // BlockWeb.idDropped: hilo
        case BlockType::CLAY: return BlockType::CLAY_BALL;        // BlockClay.idDropped: 4 bolas (main.cpp)
        case BlockType::REDSTONE_ORE: return BlockType::REDSTONE; // 4 o 5 polvos (main.cpp)
        case BlockType::LAPIS_ORE: return BlockType::DYE_LAPIS;   // 4 a 8 lapislazuli (main.cpp)
        case BlockType::DOOR_WOOD: return BlockType::DOOR_WOOD_ITEM;   // solo la mitad de abajo (main.cpp)
        case BlockType::DOOR_IRON: return BlockType::DOOR_IRON_ITEM;
        case BlockType::BED_BLOCK: return BlockType::BED_ITEM;         // solo la cabecera (main.cpp)
        case BlockType::SIGN_POST: case BlockType::SIGN_WALL: return BlockType::SIGN_ITEM;   // BlockSign.idDropped
        case BlockType::REDSTONE_WIRE: case BlockType::REDSTONE_ORE_LIT: return BlockType::REDSTONE;   // BlockRedstoneWire / BlockRedstoneOre.idDropped
        case BlockType::TORCH_REDSTONE_OFF: case BlockType::TORCH_REDSTONE_ON: return BlockType::TORCH_REDSTONE_ON;   // BlockRedstoneTorch.idDropped: la encendida
        case BlockType::REPEATER_OFF: case BlockType::REPEATER_ON: return BlockType::REPEATER_ITEM;
        // el pastel no suelta nada (BlockCake.idDropped = 0): Partial::dropsOnMining
        case BlockType::GLOWSTONE: return BlockType::GLOWSTONE_DUST; // BlockGlowStone.idDropped (2 a 4 unidades: main.cpp)
        case BlockType::DOUBLE_SLAB_STONE: case BlockType::DOUBLE_SLAB_SAND: case BlockType::DOUBLE_SLAB_WOOD: case BlockType::DOUBLE_SLAB_COBBLE:
            return slabOfDouble(minedType);   // BlockStep.quantityDropped: la doble suelta 2 losas (main.cpp)
        default:
            if (isTorchBlock(minedType)) return BlockType::TORCH; // cualquier orientación suelta la misma antorcha
            if (isDispenserBlock(minedType)) return BlockType::DISPENSER;
            if (isPumpkinBlock(minedType)) return isJackBlock(minedType) ? BlockType::JACK_O_LANTERN : BlockType::PUMPKIN;
            return isFurnaceBlock(minedType) ? BlockType::FURNACE : minedType;
    }
}

inline bool isFlatInHand(BlockType type) {
    return usesItemAtlas(type) || isTorchBlock(type) || type == BlockType::LADDER ||
           type == BlockType::LEVER || type == BlockType::TORCH_REDSTONE_ON || type == BlockType::TORCH_REDSTONE_OFF || type == BlockType::REDSTONE_WIRE ||
           isRailBlock(type) || isCrossPlant(type);
}

// Iconos 2D (hotbar, inventario, crafteo): losas, escaleras y valla recortan la textura con su silueta. (dx, dy) = pixel del icono; size = lado
inline bool iconShapeVisible(BlockType t, int dx, int dy, int size) {
    const int x = dx * 24 / size, y = dy * 24 / size;   // siempre sobre una grilla de 24
    if (isSlabBlock(t)) return y >= 12;                  // media celda, apoyada abajo
    if (isStairsBlock(t)) return y >= 12 || x >= 12;     // escalon: la mitad de abajo y el cuarto de arriba a la derecha
    if (t == BlockType::FENCE) return (x >= 9 && x < 15) || (y >= 6 && y < 10) || (y >= 15 && y < 19);   // poste y dos travesanos
    return true;
}

inline bool isFlower(BlockType type) {
    return type == BlockType::ROSE || type == BlockType::DANDELION;
}

// World.isBlockNormalCube de Beta 1.7.3: material sólido y opaco (no marcado como translúcido) que además se
// dibuja como cubo entero. Quedan afuera: aire, agua, plantas, antorchas, capa de nieve, cactus, vidrio y hojas.
// (Verificado contra Material.java y la fórmula de World.java.)
inline bool isNormalCube(BlockType t) {
    switch (t) {
        case BlockType::AIR:
        case BlockType::FIRE:
        case BlockType::PORTAL:
        case BlockType::GLOWSTONE: // material de vidrio: no es un cubo normal (no sostiene antorchas ni fuego)
        case BlockType::WATER:
        case BlockType::WATER_FLOW:
        case BlockType::LAVA:
        case BlockType::LAVA_FLOW:
        case BlockType::TALLGRASS:
        case BlockType::DEAD_BUSH:
        case BlockType::ROSE:
        case BlockType::DANDELION:
        case BlockType::SAPLING_OAK: case BlockType::SAPLING_SPRUCE: case BlockType::SAPLING_BIRCH:
        case BlockType::MUSHROOM_RED:
        case BlockType::MUSHROOM_BROWN:
        case BlockType::SUGAR_CANE:
        case BlockType::SNOW_LAYER:
        case BlockType::CACTUS:
        case BlockType::GLASS:
        case BlockType::ICE:      // BlockBreakable: no es opaco
        case BlockType::WEB:
        case BlockType::OAK_LEAVES:
        case BlockType::BIRCH_LEAVES:
        case BlockType::SPRUCE_LEAVES:
        case BlockType::DOOR_WOOD:
        case BlockType::DOOR_IRON:
        case BlockType::TRAPDOOR:
        case BlockType::BED_BLOCK:
        case BlockType::SIGN_POST:
        case BlockType::SIGN_WALL:
        case BlockType::CAKE_BLOCK:
        case BlockType::FARMLAND:
        case BlockType::CROPS:
            return false;
        default:
            return !isTorchBlock(t) && !usesItemAtlas(t) && !isRedstoneComponent(t) && !isPistonBlock(t) && !isStructureBlock(t); // los ítems nunca están en el mundo
    }
}

// ---- Fuego (BlockFire.initializeBlock: setBurnRate(id, chanceToEncourageFire, abilityToCatchFire)) ----
// Solo los bloques que existen en este juego: tablones 5/20, troncos 5/5, hojas 30/60, lana 30/60, pasto alto 60/100.
inline int fireEncourage(BlockType t) {
    if (isWool(t)) return 30;   // lana 30/60
    switch (t) {
        case BlockType::WOOD_PLANKS: case BlockType::FENCE: case BlockType::STAIRS_WOOD: return 5;
        case BlockType::OAK_LOG: case BlockType::BIRCH_LOG: case BlockType::SPRUCE_LOG: return 5;
        case BlockType::OAK_LEAVES: case BlockType::BIRCH_LEAVES: case BlockType::SPRUCE_LEAVES: return 30;
        case BlockType::TALLGRASS: return 60;
        case BlockType::TNT: return 15;
        default: return 0;
    }
}

inline int fireCatch(BlockType t) {
    if (isWool(t)) return 60;
    switch (t) {
        case BlockType::WOOD_PLANKS: case BlockType::FENCE: case BlockType::STAIRS_WOOD: return 20;
        case BlockType::OAK_LOG: case BlockType::BIRCH_LOG: case BlockType::SPRUCE_LOG: return 5;
        case BlockType::OAK_LEAVES: case BlockType::BIRCH_LEAVES: case BlockType::SPRUCE_LEAVES: return 60;
        case BlockType::TALLGRASS: return 100;
        case BlockType::TNT: return 100;
        default: return 0;
    }
}

// Material.getBurning (madera y hojas): la lava quieta les prende fuego (BlockStationary.func_301_k)
inline bool materialBurns(BlockType t) {
    return isWool(t) || t == BlockType::WOOD_PLANKS || t == BlockType::FENCE || t == BlockType::STAIRS_WOOD || t == BlockType::CRAFTING_TABLE || t == BlockType::CHEST || t == BlockType::OAK_LOG ||
           t == BlockType::BIRCH_LOG || t == BlockType::SPRUCE_LOG || t == BlockType::OAK_LEAVES ||
           t == BlockType::BIRCH_LEAVES || t == BlockType::SPRUCE_LEAVES;
}

#endif