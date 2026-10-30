#ifndef RECIPES_H
#define RECIPES_H

#include "BlockType.h"
#include "Inventory.h"

enum class RecipeCell : unsigned char { EMPTY, INGREDIENT_A, INGREDIENT_B, INGREDIENT_C, INGREDIENT_D };

struct Recipe {
    BlockType altA[3];
    BlockType altB[3]; // {AIR, AIR, AIR} si no se usa
    RecipeCell pattern[9];
    BlockType result;
    int resultCount;
    bool requiresTable;
    BlockType altC[3]; // tercer ingrediente (repetidor y piston); sin usar = {AIR, AIR, AIR}
    BlockType altD[3]; // cuarto ingrediente (solo el piston)
};

// Celdas del grid 2x2: 0=arriba-izq, 1=arriba-der, 2=abajo-izq, 3=abajo-der
static const Recipe RECIPES[] = {
    // 1 tronco (oak o birch) -> 4 tablones
    {
        {BlockType::OAK_LOG, BlockType::BIRCH_LOG, BlockType::SPRUCE_LOG}, {BlockType::AIR, BlockType::AIR},
        {RecipeCell::INGREDIENT_A, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY},
        BlockType::WOOD_PLANKS, 4, false
    },
    // 2 tablones (apilados verticalmente) -> 4 palos
    {
        {BlockType::WOOD_PLANKS, BlockType::AIR}, {BlockType::AIR, BlockType::AIR},
        {RecipeCell::INGREDIENT_A, RecipeCell::EMPTY, RecipeCell::INGREDIENT_A, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY},
        BlockType::STICK, 4, false
    },
    // Pico de madera (requiere mesa)
    {
        {BlockType::WOOD_PLANKS, BlockType::AIR}, {BlockType::STICK, BlockType::AIR},
        {RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A,
         RecipeCell::EMPTY, RecipeCell::INGREDIENT_B, RecipeCell::EMPTY,
         RecipeCell::EMPTY, RecipeCell::INGREDIENT_B, RecipeCell::EMPTY},
        BlockType::WOOD_PICKAXE, 1, true
    },
    // Pico de piedra (requiere mesa)
    {
        {BlockType::COBBLESTONE, BlockType::AIR}, {BlockType::STICK, BlockType::AIR},
        {RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A,
         RecipeCell::EMPTY, RecipeCell::INGREDIENT_B, RecipeCell::EMPTY,
         RecipeCell::EMPTY, RecipeCell::INGREDIENT_B, RecipeCell::EMPTY},
        BlockType::STONE_PICKAXE, 1, true
    },
    // 4 tablones -> mesa de crafteo
    {
        {BlockType::WOOD_PLANKS, BlockType::AIR}, {BlockType::AIR, BlockType::AIR},
        {RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A,
         RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY},
        BlockType::CRAFTING_TABLE, 1, false
    },
        // Espada de madera: tablón arriba-medio, tablón medio-medio, palo abajo-medio
    {
        {BlockType::WOOD_PLANKS, BlockType::AIR}, {BlockType::STICK, BlockType::AIR},
        {RecipeCell::EMPTY, RecipeCell::INGREDIENT_A, RecipeCell::EMPTY,
         RecipeCell::EMPTY, RecipeCell::INGREDIENT_A, RecipeCell::EMPTY,
         RecipeCell::EMPTY, RecipeCell::INGREDIENT_B, RecipeCell::EMPTY},
        BlockType::WOOD_SWORD, 1, true
    },
    // Espada de piedra
    {
        {BlockType::COBBLESTONE, BlockType::AIR}, {BlockType::STICK, BlockType::AIR},
        {RecipeCell::EMPTY, RecipeCell::INGREDIENT_A, RecipeCell::EMPTY,
         RecipeCell::EMPTY, RecipeCell::INGREDIENT_A, RecipeCell::EMPTY,
         RecipeCell::EMPTY, RecipeCell::INGREDIENT_B, RecipeCell::EMPTY},
        BlockType::STONE_SWORD, 1, true
    },
    // Pala de madera: tablón arriba-medio, palo medio-medio, palo abajo-medio
    {
        {BlockType::WOOD_PLANKS, BlockType::AIR}, {BlockType::STICK, BlockType::AIR},
        {RecipeCell::EMPTY, RecipeCell::INGREDIENT_A, RecipeCell::EMPTY,
         RecipeCell::EMPTY, RecipeCell::INGREDIENT_B, RecipeCell::EMPTY,
         RecipeCell::EMPTY, RecipeCell::INGREDIENT_B, RecipeCell::EMPTY},
        BlockType::WOOD_SHOVEL, 1, true
    },
    // Pala de piedra
    {
        {BlockType::COBBLESTONE, BlockType::AIR}, {BlockType::STICK, BlockType::AIR},
        {RecipeCell::EMPTY, RecipeCell::INGREDIENT_A, RecipeCell::EMPTY,
         RecipeCell::EMPTY, RecipeCell::INGREDIENT_B, RecipeCell::EMPTY,
         RecipeCell::EMPTY, RecipeCell::INGREDIENT_B, RecipeCell::EMPTY},
        BlockType::STONE_SHOVEL, 1, true
    },
    // Hacha de madera: forma de L (arriba-izq, arriba-medio, medio-izq), palos medio-medio + abajo-medio
    {
        {BlockType::WOOD_PLANKS, BlockType::AIR}, {BlockType::STICK, BlockType::AIR},
        {RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::EMPTY,
         RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_B, RecipeCell::EMPTY,
         RecipeCell::EMPTY, RecipeCell::INGREDIENT_B, RecipeCell::EMPTY},
        BlockType::WOOD_AXE, 1, true
    },
    // Hacha de piedra
    {
        {BlockType::COBBLESTONE, BlockType::AIR}, {BlockType::STICK, BlockType::AIR},
        {RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::EMPTY,
         RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_B, RecipeCell::EMPTY,
         RecipeCell::EMPTY, RecipeCell::INGREDIENT_B, RecipeCell::EMPTY},
        BlockType::STONE_AXE, 1, true
    },
    {
        {BlockType::COAL, BlockType::CHARCOAL}, {BlockType::STICK, BlockType::AIR},
        {RecipeCell::INGREDIENT_A, RecipeCell::EMPTY, RecipeCell::INGREDIENT_B, RecipeCell::EMPTY,
         RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY},
        BlockType::TORCH, 4, false
    },
    // 4 arena -> 1 arenisca
    {
        {BlockType::SAND, BlockType::AIR}, {BlockType::AIR, BlockType::AIR},
        {RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A,
         RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY},
        BlockType::SANDSTONE, 1, false
    },
    // 1 rosa -> 1 tinte rojo
    {
        {BlockType::ROSE, BlockType::AIR}, {BlockType::AIR, BlockType::AIR},
        {RecipeCell::INGREDIENT_A, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY},
        BlockType::DYE_RED, 2, false
    },
    // 1 diente de león -> 1 tinte amarillo
    {
        {BlockType::DANDELION, BlockType::AIR}, {BlockType::AIR, BlockType::AIR},
        {RecipeCell::INGREDIENT_A, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY},
        BlockType::DYE_YELLOW, 2, false
    },
    // Horno: 8 adoquines en anillo, centro vacío
    {
        {BlockType::COBBLESTONE, BlockType::AIR}, {BlockType::AIR, BlockType::AIR},
        {RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A,
         RecipeCell::INGREDIENT_A, RecipeCell::EMPTY, RecipeCell::INGREDIENT_A,
         RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A},
        BlockType::FURNACE, 1, true
    },
    // Pico de hierro (requiere mesa)
    {
        {BlockType::IRON_INGOT, BlockType::AIR}, {BlockType::STICK, BlockType::AIR},
        {RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A,
         RecipeCell::EMPTY, RecipeCell::INGREDIENT_B, RecipeCell::EMPTY,
         RecipeCell::EMPTY, RecipeCell::INGREDIENT_B, RecipeCell::EMPTY},
        BlockType::IRON_PICKAXE, 1, true
    },
    // Espada de hierro
    {
        {BlockType::IRON_INGOT, BlockType::AIR}, {BlockType::STICK, BlockType::AIR},
        {RecipeCell::EMPTY, RecipeCell::INGREDIENT_A, RecipeCell::EMPTY,
         RecipeCell::EMPTY, RecipeCell::INGREDIENT_A, RecipeCell::EMPTY,
         RecipeCell::EMPTY, RecipeCell::INGREDIENT_B, RecipeCell::EMPTY},
        BlockType::IRON_SWORD, 1, true
    },
    // Pala de hierro
    {
        {BlockType::IRON_INGOT, BlockType::AIR}, {BlockType::STICK, BlockType::AIR},
        {RecipeCell::EMPTY, RecipeCell::INGREDIENT_A, RecipeCell::EMPTY,
         RecipeCell::EMPTY, RecipeCell::INGREDIENT_B, RecipeCell::EMPTY,
         RecipeCell::EMPTY, RecipeCell::INGREDIENT_B, RecipeCell::EMPTY},
        BlockType::IRON_SHOVEL, 1, true
    },
    // Hacha de hierro
    {
        {BlockType::IRON_INGOT, BlockType::AIR}, {BlockType::STICK, BlockType::AIR},
        {RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::EMPTY,
         RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_B, RecipeCell::EMPTY,
         RecipeCell::EMPTY, RecipeCell::INGREDIENT_B, RecipeCell::EMPTY},
        BlockType::IRON_AXE, 1, true
    },
    // Balde: 3 lingotes de hierro en V (requiere mesa)
    {
        {BlockType::IRON_INGOT, BlockType::AIR}, {BlockType::AIR, BlockType::AIR},
        {RecipeCell::INGREDIENT_A, RecipeCell::EMPTY, RecipeCell::INGREDIENT_A,
         RecipeCell::EMPTY, RecipeCell::INGREDIENT_A, RecipeCell::EMPTY,
         RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY},
        BlockType::BUCKET, 1, true
    },
    // Pico de diamante
    {
        {BlockType::DIAMOND, BlockType::AIR}, {BlockType::STICK, BlockType::AIR},
        {RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A,
         RecipeCell::EMPTY, RecipeCell::INGREDIENT_B, RecipeCell::EMPTY,
         RecipeCell::EMPTY, RecipeCell::INGREDIENT_B, RecipeCell::EMPTY},
        BlockType::DIAMOND_PICKAXE, 1, true
    },
    // Espada de diamante
    {
        {BlockType::DIAMOND, BlockType::AIR}, {BlockType::STICK, BlockType::AIR},
        {RecipeCell::EMPTY, RecipeCell::INGREDIENT_A, RecipeCell::EMPTY,
         RecipeCell::EMPTY, RecipeCell::INGREDIENT_A, RecipeCell::EMPTY,
         RecipeCell::EMPTY, RecipeCell::INGREDIENT_B, RecipeCell::EMPTY},
        BlockType::DIAMOND_SWORD, 1, true
    },
    // Pala de diamante
    {
        {BlockType::DIAMOND, BlockType::AIR}, {BlockType::STICK, BlockType::AIR},
        {RecipeCell::EMPTY, RecipeCell::INGREDIENT_A, RecipeCell::EMPTY,
         RecipeCell::EMPTY, RecipeCell::INGREDIENT_B, RecipeCell::EMPTY,
         RecipeCell::EMPTY, RecipeCell::INGREDIENT_B, RecipeCell::EMPTY},
        BlockType::DIAMOND_SHOVEL, 1, true
    },
    // Hacha de diamante
    {
        {BlockType::DIAMOND, BlockType::AIR}, {BlockType::STICK, BlockType::AIR},
        {RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::EMPTY,
         RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_B, RecipeCell::EMPTY,
         RecipeCell::EMPTY, RecipeCell::INGREDIENT_B, RecipeCell::EMPTY},
        BlockType::DIAMOND_AXE, 1, true
    },
    // Tazon: 3 tablones en V da 4 (Beta)
    {
        {BlockType::WOOD_PLANKS, BlockType::AIR}, {BlockType::AIR, BlockType::AIR},
        {RecipeCell::INGREDIENT_A, RecipeCell::EMPTY, RecipeCell::INGREDIENT_A,
         RecipeCell::EMPTY, RecipeCell::INGREDIENT_A, RecipeCell::EMPTY,
         RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY},
        BlockType::BOWL, 4, true
    },
    // Sopa de hongos: hongo rojo, hongo marron y tazon en columna (RecipesFood)
    {
        {BlockType::MUSHROOM_RED, BlockType::AIR}, {BlockType::MUSHROOM_BROWN, BlockType::AIR},
        {RecipeCell::EMPTY, RecipeCell::INGREDIENT_A, RecipeCell::EMPTY,
         RecipeCell::EMPTY, RecipeCell::INGREDIENT_B, RecipeCell::EMPTY,
         RecipeCell::EMPTY, RecipeCell::INGREDIENT_C, RecipeCell::EMPTY},
        BlockType::MUSHROOM_STEW, 1, true,
        {BlockType::BOWL, BlockType::AIR}
    },
    // Pan: tres trigos en fila (RecipesCrafting)
    {
        {BlockType::WHEAT, BlockType::AIR}, {BlockType::AIR, BlockType::AIR},
        {RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY,
         RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A,
         RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY},
        BlockType::BREAD, 1, true
    },
    // Gold tools (RecipesTools / RecipesWeapons con el lingote de oro)
    {
        {BlockType::GOLD_INGOT, BlockType::AIR}, {BlockType::STICK, BlockType::AIR},
        {RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A,
         RecipeCell::EMPTY, RecipeCell::INGREDIENT_B, RecipeCell::EMPTY,
         RecipeCell::EMPTY, RecipeCell::INGREDIENT_B, RecipeCell::EMPTY},
        BlockType::GOLD_PICKAXE, 1, true
    },
    {
        {BlockType::GOLD_INGOT, BlockType::AIR}, {BlockType::STICK, BlockType::AIR},
        {RecipeCell::EMPTY, RecipeCell::INGREDIENT_A, RecipeCell::EMPTY,
         RecipeCell::EMPTY, RecipeCell::INGREDIENT_A, RecipeCell::EMPTY,
         RecipeCell::EMPTY, RecipeCell::INGREDIENT_B, RecipeCell::EMPTY},
        BlockType::GOLD_SWORD, 1, true
    },
    {
        {BlockType::GOLD_INGOT, BlockType::AIR}, {BlockType::STICK, BlockType::AIR},
        {RecipeCell::EMPTY, RecipeCell::INGREDIENT_A, RecipeCell::EMPTY,
         RecipeCell::EMPTY, RecipeCell::INGREDIENT_B, RecipeCell::EMPTY,
         RecipeCell::EMPTY, RecipeCell::INGREDIENT_B, RecipeCell::EMPTY},
        BlockType::GOLD_SHOVEL, 1, true
    },
    {
        {BlockType::GOLD_INGOT, BlockType::AIR}, {BlockType::STICK, BlockType::AIR},
        {RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::EMPTY,
         RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_B, RecipeCell::EMPTY,
         RecipeCell::EMPTY, RecipeCell::INGREDIENT_B, RecipeCell::EMPTY},
        BlockType::GOLD_AXE, 1, true
    },
    {
        {BlockType::GOLD_INGOT, BlockType::AIR}, {BlockType::STICK, BlockType::AIR},
        {RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::EMPTY,
         RecipeCell::EMPTY, RecipeCell::INGREDIENT_B, RecipeCell::EMPTY,
         RecipeCell::EMPTY, RecipeCell::INGREDIENT_B, RecipeCell::EMPTY},
        BlockType::HOE_GOLD, 1, true
    },
    // BLOCK_IRON (RecipesIngots)
    {
        {BlockType::IRON_INGOT, BlockType::AIR}, {BlockType::AIR, BlockType::AIR},
        {RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A,
         RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A,
         RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A},
        BlockType::BLOCK_IRON, 1, true
    },
    {
        {BlockType::BLOCK_IRON, BlockType::AIR}, {BlockType::AIR, BlockType::AIR},
        {RecipeCell::INGREDIENT_A, RecipeCell::EMPTY, RecipeCell::EMPTY,
         RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY,
         RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY},
        BlockType::IRON_INGOT, 9, false
    },
    // BLOCK_GOLD (RecipesIngots)
    {
        {BlockType::GOLD_INGOT, BlockType::AIR}, {BlockType::AIR, BlockType::AIR},
        {RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A,
         RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A,
         RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A},
        BlockType::BLOCK_GOLD, 1, true
    },
    {
        {BlockType::BLOCK_GOLD, BlockType::AIR}, {BlockType::AIR, BlockType::AIR},
        {RecipeCell::INGREDIENT_A, RecipeCell::EMPTY, RecipeCell::EMPTY,
         RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY,
         RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY},
        BlockType::GOLD_INGOT, 9, false
    },
    // BLOCK_DIAMOND (RecipesIngots)
    {
        {BlockType::DIAMOND, BlockType::AIR}, {BlockType::AIR, BlockType::AIR},
        {RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A,
         RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A,
         RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A},
        BlockType::BLOCK_DIAMOND, 1, true
    },
    {
        {BlockType::BLOCK_DIAMOND, BlockType::AIR}, {BlockType::AIR, BlockType::AIR},
        {RecipeCell::INGREDIENT_A, RecipeCell::EMPTY, RecipeCell::EMPTY,
         RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY,
         RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY},
        BlockType::DIAMOND, 9, false
    },
    // BLOCK_LAPIS (RecipesIngots)
    {
        {BlockType::DYE_LAPIS, BlockType::AIR}, {BlockType::AIR, BlockType::AIR},
        {RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A,
         RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A,
         RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A},
        BlockType::BLOCK_LAPIS, 1, true
    },
    {
        {BlockType::BLOCK_LAPIS, BlockType::AIR}, {BlockType::AIR, BlockType::AIR},
        {RecipeCell::INGREDIENT_A, RecipeCell::EMPTY, RecipeCell::EMPTY,
         RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY,
         RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY},
        BlockType::DYE_LAPIS, 9, false
    },
    // Azucar, papel, libro y estanteria (CraftingManager)
    {
        {BlockType::SUGAR_CANE_ITEM, BlockType::AIR}, {BlockType::AIR, BlockType::AIR},
        {RecipeCell::INGREDIENT_A, RecipeCell::EMPTY, RecipeCell::EMPTY,
         RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY,
         RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY},
        BlockType::SUGAR, 1, false
    },
    {
        {BlockType::SUGAR_CANE_ITEM, BlockType::AIR}, {BlockType::AIR, BlockType::AIR},
        {RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A,
         RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY,
         RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY},
        BlockType::PAPER, 3, true
    },
    {
        {BlockType::PAPER, BlockType::AIR}, {BlockType::AIR, BlockType::AIR},
        {RecipeCell::EMPTY, RecipeCell::INGREDIENT_A, RecipeCell::EMPTY,
         RecipeCell::EMPTY, RecipeCell::INGREDIENT_A, RecipeCell::EMPTY,
         RecipeCell::EMPTY, RecipeCell::INGREDIENT_A, RecipeCell::EMPTY},
        BlockType::BOOK, 1, true
    },
    {
        {BlockType::WOOD_PLANKS, BlockType::AIR}, {BlockType::BOOK, BlockType::AIR},
        {RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A,
         RecipeCell::INGREDIENT_B, RecipeCell::INGREDIENT_B, RecipeCell::INGREDIENT_B,
         RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A},
        BlockType::BOOKSHELF, 1, true
    },
    // Galleta (RecipesFood): trigo, cacao, trigo; da 8
    {
        {BlockType::WHEAT, BlockType::AIR}, {BlockType::DYE_COCOA, BlockType::AIR},
        {RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY,
         RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_B, RecipeCell::INGREDIENT_A,
         RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY},
        BlockType::COOKIE, 8, true
    },
    // Bloque de ladrillos, de nieve y de arcilla: 4 en cuadrado (CraftingManager)
    {
        {BlockType::BRICK, BlockType::AIR}, {BlockType::AIR, BlockType::AIR},
        {RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A,
         RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY},
        BlockType::BRICK_BLOCK, 1, false
    },
    {
        {BlockType::SNOWBALL, BlockType::AIR}, {BlockType::AIR, BlockType::AIR},
        {RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A,
         RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY},
        BlockType::SNOW_BLOCK, 1, false
    },
    {
        {BlockType::CLAY_BALL, BlockType::AIR}, {BlockType::AIR, BlockType::AIR},
        {RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A,
         RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY},
        BlockType::CLAY, 1, false
    },
    // Azada de madera: dos materiales arriba y dos palos en columna a la derecha (ItemHoe)
    {
        {BlockType::WOOD_PLANKS, BlockType::AIR}, {BlockType::STICK, BlockType::AIR},
        {RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::EMPTY,
         RecipeCell::EMPTY, RecipeCell::INGREDIENT_B, RecipeCell::EMPTY,
         RecipeCell::EMPTY, RecipeCell::INGREDIENT_B, RecipeCell::EMPTY},
        BlockType::HOE_WOOD, 1, true
    },
    // Azada de piedra: dos materiales arriba y dos palos en columna a la derecha (ItemHoe)
    {
        {BlockType::COBBLESTONE, BlockType::AIR}, {BlockType::STICK, BlockType::AIR},
        {RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::EMPTY,
         RecipeCell::EMPTY, RecipeCell::INGREDIENT_B, RecipeCell::EMPTY,
         RecipeCell::EMPTY, RecipeCell::INGREDIENT_B, RecipeCell::EMPTY},
        BlockType::HOE_STONE, 1, true
    },
    // Azada de hierro: dos materiales arriba y dos palos en columna a la derecha (ItemHoe)
    {
        {BlockType::IRON_INGOT, BlockType::AIR}, {BlockType::STICK, BlockType::AIR},
        {RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::EMPTY,
         RecipeCell::EMPTY, RecipeCell::INGREDIENT_B, RecipeCell::EMPTY,
         RecipeCell::EMPTY, RecipeCell::INGREDIENT_B, RecipeCell::EMPTY},
        BlockType::HOE_IRON, 1, true
    },
    // Azada de diamante: dos materiales arriba y dos palos en columna a la derecha (ItemHoe)
    {
        {BlockType::DIAMOND, BlockType::AIR}, {BlockType::STICK, BlockType::AIR},
        {RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::EMPTY,
         RecipeCell::EMPTY, RecipeCell::INGREDIENT_B, RecipeCell::EMPTY,
         RecipeCell::EMPTY, RecipeCell::INGREDIENT_B, RecipeCell::EMPTY},
        BlockType::HOE_DIAMOND, 1, true
    },
    // Mechero: 1 lingote de hierro + 1 pedernal (sin forma; entra en la cuadrícula de 2x2)
    {
        {BlockType::IRON_INGOT, BlockType::AIR}, {BlockType::FLINT, BlockType::AIR},
        {RecipeCell::EMPTY, RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_B,   // 2x2: lingote arriba-derecha, pedernal abajo-izquierda
         RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY,
         RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY},
        BlockType::FLINT_AND_STEEL, 1, false
    },
    // Piedra luminosa: 4 polvos en cuadrícula de 2x2
    {
        {BlockType::GLOWSTONE_DUST, BlockType::AIR}, {BlockType::AIR, BlockType::AIR},
        {RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A,
         RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY},
        BlockType::GLOWSTONE, 1, false
    },
    // Cofre: 8 tablones en anillo (mesa de crafteo)
    {
        {BlockType::WOOD_PLANKS, BlockType::AIR}, {BlockType::AIR, BlockType::AIR},
        {RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A,
         RecipeCell::INGREDIENT_A, RecipeCell::EMPTY,        RecipeCell::INGREDIENT_A,
         RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A},
        BlockType::CHEST, 1, true
    },
    // Tijeras: 2 lingotes de hierro en diagonal (arriba-der y abajo-izq), sin mesa: entran en la cuadrícula de 2x2
    {
        {BlockType::IRON_INGOT, BlockType::AIR}, {BlockType::AIR, BlockType::AIR},
        {RecipeCell::EMPTY, RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::EMPTY,
         RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY},
        BlockType::SHEARS, 1, false
    },
    // Puerta de madera: 6 tablones en 2 x 3 (mesa de crafteo)
    {
        {BlockType::WOOD_PLANKS, BlockType::AIR}, {BlockType::AIR, BlockType::AIR},
        {RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::EMPTY,
         RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::EMPTY,
         RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::EMPTY},
        BlockType::DOOR_WOOD_ITEM, 1, true
    },
    // Puerta de hierro: 6 lingotes de hierro en 2 x 3
    {
        {BlockType::IRON_INGOT, BlockType::AIR}, {BlockType::AIR, BlockType::AIR},
        {RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::EMPTY,
         RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::EMPTY,
         RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::EMPTY},
        BlockType::DOOR_IRON_ITEM, 1, true
    },
    // Trampilla: 6 tablones en 3 x 2 dan 2 trampillas
    {
        {BlockType::WOOD_PLANKS, BlockType::AIR}, {BlockType::AIR, BlockType::AIR},
        {RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A,
         RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A,
         RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY},
        BlockType::TRAPDOOR, 2, true
    },
    // Cartel: 6 tablones y 1 palo (mesa de crafteo): 1 cartel
    {
        {BlockType::WOOD_PLANKS, BlockType::AIR}, {BlockType::STICK, BlockType::AIR},
        {RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A,
         RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A,
         RecipeCell::EMPTY, RecipeCell::INGREDIENT_B, RecipeCell::EMPTY},
        BlockType::SIGN_ITEM, 1, true
    },
    // Cama: 3 lanas (de cualquier color) arriba y 3 tablones abajo
    {
        {BlockType::WOOL_WHITE, BlockType::WOOL_BLACK, BlockType::AIR}, {BlockType::WOOD_PLANKS, BlockType::AIR},
        {RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A,
         RecipeCell::INGREDIENT_B, RecipeCell::INGREDIENT_B, RecipeCell::INGREDIENT_B,
         RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY},
        BlockType::BED_ITEM, 1, true
    },
    // Palanca: un palo sobre un cobblestone
    {
        {BlockType::STICK, BlockType::AIR}, {BlockType::COBBLESTONE, BlockType::AIR},
        {RecipeCell::INGREDIENT_A, RecipeCell::EMPTY, RecipeCell::INGREDIENT_B, RecipeCell::EMPTY,
         RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY},
        BlockType::LEVER, 1, false
    },
    // Antorcha de redstone: polvo de redstone sobre un palo
    {
        {BlockType::REDSTONE, BlockType::AIR}, {BlockType::STICK, BlockType::AIR},
        {RecipeCell::INGREDIENT_A, RecipeCell::EMPTY, RecipeCell::INGREDIENT_B, RecipeCell::EMPTY,
         RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY},
        BlockType::TORCH_REDSTONE_ON, 1, false
    },
    // Boton: dos piedras una sobre la otra
    {
        {BlockType::STONE, BlockType::AIR}, {BlockType::AIR, BlockType::AIR},
        {RecipeCell::INGREDIENT_A, RecipeCell::EMPTY, RecipeCell::INGREDIENT_A, RecipeCell::EMPTY,
         RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY},
        BlockType::BUTTON, 1, false
    },
    // Placa de presion de piedra: dos piedras lado a lado
    {
        {BlockType::STONE, BlockType::AIR}, {BlockType::AIR, BlockType::AIR},
        {RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::EMPTY, RecipeCell::EMPTY,
         RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY},
        BlockType::PLATE_STONE, 1, false
    },
    // Placa de presion de madera: dos tablones lado a lado
    {
        {BlockType::WOOD_PLANKS, BlockType::AIR}, {BlockType::AIR, BlockType::AIR},
        {RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::EMPTY, RecipeCell::EMPTY,
         RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY},
        BlockType::PLATE_WOOD, 1, false
    },
    // Repetidor (mesa): dos antorchas de redstone con un polvo en medio, y tres piedras debajo
    {
        {BlockType::TORCH_REDSTONE_ON, BlockType::AIR}, {BlockType::REDSTONE, BlockType::AIR},
        {RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_B, RecipeCell::INGREDIENT_A,
         RecipeCell::INGREDIENT_C, RecipeCell::INGREDIENT_C, RecipeCell::INGREDIENT_C,
         RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY},
        BlockType::REPEATER_ITEM, 1, true,
        {BlockType::STONE, BlockType::AIR, BlockType::AIR}
    },
    // Piston (mesa): tres tablones arriba; cobblestone, hierro, cobblestone; cobblestone, polvo de redstone, cobblestone
    // (el piston pegajoso lleva una bola de slime, que todavia no existe en este juego)
    {
        {BlockType::WOOD_PLANKS, BlockType::AIR}, {BlockType::COBBLESTONE, BlockType::AIR},
        {RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A,
         RecipeCell::INGREDIENT_B, RecipeCell::INGREDIENT_C, RecipeCell::INGREDIENT_B,
         RecipeCell::INGREDIENT_B, RecipeCell::INGREDIENT_D, RecipeCell::INGREDIENT_B},
        BlockType::PISTON_BASE, 1, true,
        {BlockType::IRON_INGOT, BlockType::AIR, BlockType::AIR},
        {BlockType::REDSTONE, BlockType::AIR, BlockType::AIR}
    },
    // Losa de piedra: 3 piedras en fila dan 6 losas
    {
        {BlockType::STONE, BlockType::AIR}, {BlockType::AIR, BlockType::AIR},
        {RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY},
        BlockType::SLAB_STONE, 3, true
    },
    // Losa de arenisca
    {
        {BlockType::SANDSTONE, BlockType::AIR}, {BlockType::AIR, BlockType::AIR},
        {RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY},
        BlockType::SLAB_SAND, 3, true
    },
    // Losa de madera
    {
        {BlockType::WOOD_PLANKS, BlockType::AIR}, {BlockType::AIR, BlockType::AIR},
        {RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY},
        BlockType::SLAB_WOOD, 3, true
    },
    // Losa de roca
    {
        {BlockType::COBBLESTONE, BlockType::AIR}, {BlockType::AIR, BlockType::AIR},
        {RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY},
        BlockType::SLAB_COBBLE, 3, true
    },
    // Escalera de madera: 6 tablones en escalon dan 4
    {
        {BlockType::WOOD_PLANKS, BlockType::AIR}, {BlockType::AIR, BlockType::AIR},
        {RecipeCell::INGREDIENT_A, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::EMPTY, RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A},
        BlockType::STAIRS_WOOD, 4, true
    },
    // Escalera de roca
    {
        {BlockType::COBBLESTONE, BlockType::AIR}, {BlockType::AIR, BlockType::AIR},
        {RecipeCell::INGREDIENT_A, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::EMPTY, RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A},
        BlockType::STAIRS_COBBLE, 4, true
    },
    // Escalera de mano: 7 palos en H dan 2
    {
        {BlockType::STICK, BlockType::AIR}, {BlockType::AIR, BlockType::AIR},
        {RecipeCell::INGREDIENT_A, RecipeCell::EMPTY, RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::EMPTY, RecipeCell::INGREDIENT_A},
        BlockType::LADDER, 2, true
    },
    // Valla: 6 palos en 3 x 2 dan 2
    {
        {BlockType::STICK, BlockType::AIR}, {BlockType::AIR, BlockType::AIR},
        {RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY},
        BlockType::FENCE, 2, true
    },
    // Rieles (mesa): 6 hierros en dos columnas y un palo en medio dan 16
    {
        {BlockType::IRON_INGOT, BlockType::AIR}, {BlockType::STICK, BlockType::AIR},
        {RecipeCell::INGREDIENT_A, RecipeCell::EMPTY, RecipeCell::INGREDIENT_A,
         RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_B, RecipeCell::INGREDIENT_A,
         RecipeCell::INGREDIENT_A, RecipeCell::EMPTY, RecipeCell::INGREDIENT_A},
        BlockType::RAIL, 16, true
    },
    // Riel propulsor: 6 de oro, un palo y polvo de redstone dan 6
    {
        {BlockType::GOLD_INGOT, BlockType::AIR}, {BlockType::STICK, BlockType::AIR},
        {RecipeCell::INGREDIENT_A, RecipeCell::EMPTY, RecipeCell::INGREDIENT_A,
         RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_B, RecipeCell::INGREDIENT_A,
         RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_C, RecipeCell::INGREDIENT_A},
        BlockType::RAIL_POWERED, 6, true,
        {BlockType::REDSTONE, BlockType::AIR, BlockType::AIR}
    },
    // Riel detector: 6 de hierro, una placa de presion de piedra y polvo de redstone dan 6
    {
        {BlockType::IRON_INGOT, BlockType::AIR}, {BlockType::PLATE_STONE, BlockType::AIR},
        {RecipeCell::INGREDIENT_A, RecipeCell::EMPTY, RecipeCell::INGREDIENT_A,
         RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_B, RecipeCell::INGREDIENT_A,
         RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_C, RecipeCell::INGREDIENT_A},
        BlockType::RAIL_DETECTOR, 6, true,
        {BlockType::REDSTONE, BlockType::AIR, BlockType::AIR}
    },
    // Vagoneta: 5 hierros en U
    {
        {BlockType::IRON_INGOT, BlockType::AIR}, {BlockType::AIR, BlockType::AIR},
        {RecipeCell::INGREDIENT_A, RecipeCell::EMPTY, RecipeCell::INGREDIENT_A,
         RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A,
         RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY},
        BlockType::MINECART, 1, true
    },
    // Vagoneta con cofre: un cofre sobre una vagoneta
    {
        {BlockType::CHEST, BlockType::AIR}, {BlockType::MINECART, BlockType::AIR},
        {RecipeCell::INGREDIENT_A, RecipeCell::EMPTY, RecipeCell::EMPTY,
         RecipeCell::INGREDIENT_B, RecipeCell::EMPTY, RecipeCell::EMPTY,
         RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY},
        BlockType::MINECART_CHEST, 1, true
    },
    // Vagoneta con horno: un horno sobre una vagoneta
    {
        {BlockType::FURNACE, BlockType::AIR}, {BlockType::MINECART, BlockType::AIR},
        {RecipeCell::INGREDIENT_A, RecipeCell::EMPTY, RecipeCell::EMPTY,
         RecipeCell::INGREDIENT_B, RecipeCell::EMPTY, RecipeCell::EMPTY,
         RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY},
        BlockType::MINECART_FURNACE, 1, true
    },
    // Bote: 5 tablas en U
    {
        {BlockType::WOOD_PLANKS, BlockType::AIR}, {BlockType::AIR, BlockType::AIR},
        {RecipeCell::INGREDIENT_A, RecipeCell::EMPTY, RecipeCell::INGREDIENT_A,
         RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A,
         RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY},
        BlockType::BOAT, 1, true
    },
    // TNT: 5 polvoras y 4 arenas alternadas (como en Beta, mesa de crafteo)
    {
        {BlockType::GUNPOWDER, BlockType::AIR}, {BlockType::SAND, BlockType::AIR},
        {RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_B, RecipeCell::INGREDIENT_A,
         RecipeCell::INGREDIENT_B, RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_B,
         RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_B, RecipeCell::INGREDIENT_A},
        BlockType::TNT, 1, true
    },
    // Bloque de notas: 8 tablones en anillo con 1 polvo de redstone en el centro
    {
        {BlockType::WOOD_PLANKS, BlockType::AIR}, {BlockType::REDSTONE, BlockType::AIR},
        {RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A,
         RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_B, RecipeCell::INGREDIENT_A,
         RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A},
        BlockType::NOTE_BLOCK, 1, true
    },
    // Arco: 3 palos y 3 hilos en dos columnas (mesa de crafteo)
    {
        {BlockType::STICK, BlockType::AIR}, {BlockType::STRING, BlockType::AIR},
        {RecipeCell::EMPTY, RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_B,
         RecipeCell::INGREDIENT_A, RecipeCell::EMPTY, RecipeCell::INGREDIENT_B,
         RecipeCell::EMPTY, RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_B},
        BlockType::BOW, 1, true
    },
    // Flechas: pedernal arriba, palo en medio y pluma abajo dan 4 (mesa de crafteo)
    {
        {BlockType::FLINT, BlockType::AIR}, {BlockType::STICK, BlockType::AIR},
        {RecipeCell::EMPTY, RecipeCell::INGREDIENT_A, RecipeCell::EMPTY,
         RecipeCell::EMPTY, RecipeCell::INGREDIENT_B, RecipeCell::EMPTY,
         RecipeCell::EMPTY, RecipeCell::INGREDIENT_C, RecipeCell::EMPTY},
        BlockType::ARROW, 4, true, {BlockType::FEATHER, BlockType::AIR}
    },
    // Dispensador: 7 cobblestone en U, el arco arriba al centro y 1 polvo de redstone abajo (mesa de crafteo)
    {
        {BlockType::COBBLESTONE, BlockType::AIR}, {BlockType::BOW, BlockType::AIR},
        {RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A,
         RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_B, RecipeCell::INGREDIENT_A,
         RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_C, RecipeCell::INGREDIENT_A},
        BlockType::DISPENSER, 1, true, {BlockType::REDSTONE, BlockType::AIR}
    },
    // ---- RecipesDyes / CraftingManager (parte7) ----
    {
        {BlockType::INK_SAC, BlockType::AIR, BlockType::AIR}, {BlockType::WOOL_WHITE, BlockType::AIR, BlockType::AIR},
        {RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_B, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY},
        BlockType::WOOL_BLACK, 1, false
    },
    {
        {BlockType::DYE_RED, BlockType::AIR, BlockType::AIR}, {BlockType::WOOL_WHITE, BlockType::AIR, BlockType::AIR},
        {RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_B, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY},
        BlockType::WOOL_RED, 1, false
    },
    {
        {BlockType::DYE_GREEN, BlockType::AIR, BlockType::AIR}, {BlockType::WOOL_WHITE, BlockType::AIR, BlockType::AIR},
        {RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_B, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY},
        BlockType::WOOL_GREEN, 1, false
    },
    {
        {BlockType::DYE_COCOA, BlockType::AIR, BlockType::AIR}, {BlockType::WOOL_WHITE, BlockType::AIR, BlockType::AIR},
        {RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_B, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY},
        BlockType::WOOL_BROWN, 1, false
    },
    {
        {BlockType::DYE_LAPIS, BlockType::AIR, BlockType::AIR}, {BlockType::WOOL_WHITE, BlockType::AIR, BlockType::AIR},
        {RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_B, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY},
        BlockType::WOOL_BLUE, 1, false
    },
    {
        {BlockType::DYE_PURPLE, BlockType::AIR, BlockType::AIR}, {BlockType::WOOL_WHITE, BlockType::AIR, BlockType::AIR},
        {RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_B, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY},
        BlockType::WOOL_PURPLE, 1, false
    },
    {
        {BlockType::DYE_CYAN, BlockType::AIR, BlockType::AIR}, {BlockType::WOOL_WHITE, BlockType::AIR, BlockType::AIR},
        {RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_B, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY},
        BlockType::WOOL_CYAN, 1, false
    },
    {
        {BlockType::DYE_LIGHT_GRAY, BlockType::AIR, BlockType::AIR}, {BlockType::WOOL_WHITE, BlockType::AIR, BlockType::AIR},
        {RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_B, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY},
        BlockType::WOOL_LIGHT_GRAY, 1, false
    },
    {
        {BlockType::DYE_GRAY, BlockType::AIR, BlockType::AIR}, {BlockType::WOOL_WHITE, BlockType::AIR, BlockType::AIR},
        {RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_B, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY},
        BlockType::WOOL_GRAY, 1, false
    },
    {
        {BlockType::DYE_PINK, BlockType::AIR, BlockType::AIR}, {BlockType::WOOL_WHITE, BlockType::AIR, BlockType::AIR},
        {RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_B, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY},
        BlockType::WOOL_PINK, 1, false
    },
    {
        {BlockType::DYE_LIME, BlockType::AIR, BlockType::AIR}, {BlockType::WOOL_WHITE, BlockType::AIR, BlockType::AIR},
        {RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_B, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY},
        BlockType::WOOL_LIME, 1, false
    },
    {
        {BlockType::DYE_YELLOW, BlockType::AIR, BlockType::AIR}, {BlockType::WOOL_WHITE, BlockType::AIR, BlockType::AIR},
        {RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_B, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY},
        BlockType::WOOL_YELLOW, 1, false
    },
    {
        {BlockType::DYE_LIGHT_BLUE, BlockType::AIR, BlockType::AIR}, {BlockType::WOOL_WHITE, BlockType::AIR, BlockType::AIR},
        {RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_B, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY},
        BlockType::WOOL_LIGHT_BLUE, 1, false
    },
    {
        {BlockType::DYE_MAGENTA, BlockType::AIR, BlockType::AIR}, {BlockType::WOOL_WHITE, BlockType::AIR, BlockType::AIR},
        {RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_B, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY},
        BlockType::WOOL_MAGENTA, 1, false
    },
    {
        {BlockType::DYE_ORANGE, BlockType::AIR, BlockType::AIR}, {BlockType::WOOL_WHITE, BlockType::AIR, BlockType::AIR},
        {RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_B, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY},
        BlockType::WOOL_ORANGE, 1, false
    },
    {
        {BlockType::BONE, BlockType::AIR, BlockType::AIR}, {BlockType::AIR, BlockType::AIR, BlockType::AIR},
        {RecipeCell::INGREDIENT_A, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY},
        BlockType::BONE_MEAL, 3, false
    },
    {
        {BlockType::DYE_RED, BlockType::AIR, BlockType::AIR}, {BlockType::BONE_MEAL, BlockType::AIR, BlockType::AIR},
        {RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_B, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY},
        BlockType::DYE_PINK, 2, false
    },
    {
        {BlockType::DYE_RED, BlockType::AIR, BlockType::AIR}, {BlockType::DYE_YELLOW, BlockType::AIR, BlockType::AIR},
        {RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_B, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY},
        BlockType::DYE_ORANGE, 2, false
    },
    {
        {BlockType::DYE_GREEN, BlockType::AIR, BlockType::AIR}, {BlockType::BONE_MEAL, BlockType::AIR, BlockType::AIR},
        {RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_B, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY},
        BlockType::DYE_LIME, 2, false
    },
    {
        {BlockType::INK_SAC, BlockType::AIR, BlockType::AIR}, {BlockType::BONE_MEAL, BlockType::AIR, BlockType::AIR},
        {RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_B, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY},
        BlockType::DYE_GRAY, 2, false
    },
    {
        {BlockType::DYE_GRAY, BlockType::AIR, BlockType::AIR}, {BlockType::BONE_MEAL, BlockType::AIR, BlockType::AIR},
        {RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_B, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY},
        BlockType::DYE_LIGHT_GRAY, 2, false
    },
    {
        {BlockType::INK_SAC, BlockType::AIR, BlockType::AIR}, {BlockType::BONE_MEAL, BlockType::AIR, BlockType::AIR},
        {RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_B, RecipeCell::INGREDIENT_B, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY},
        BlockType::DYE_LIGHT_GRAY, 3, false
    },
    {
        {BlockType::DYE_LAPIS, BlockType::AIR, BlockType::AIR}, {BlockType::BONE_MEAL, BlockType::AIR, BlockType::AIR},
        {RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_B, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY},
        BlockType::DYE_LIGHT_BLUE, 2, false
    },
    {
        {BlockType::DYE_LAPIS, BlockType::AIR, BlockType::AIR}, {BlockType::DYE_GREEN, BlockType::AIR, BlockType::AIR},
        {RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_B, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY},
        BlockType::DYE_CYAN, 2, false
    },
    {
        {BlockType::DYE_LAPIS, BlockType::AIR, BlockType::AIR}, {BlockType::DYE_RED, BlockType::AIR, BlockType::AIR},
        {RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_B, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY},
        BlockType::DYE_PURPLE, 2, false
    },
    {
        {BlockType::DYE_PURPLE, BlockType::AIR, BlockType::AIR}, {BlockType::DYE_PINK, BlockType::AIR, BlockType::AIR},
        {RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_B, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY},
        BlockType::DYE_MAGENTA, 2, false
    },
    {
        {BlockType::DYE_LAPIS, BlockType::AIR, BlockType::AIR}, {BlockType::DYE_RED, BlockType::AIR, BlockType::AIR},
        {RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_B, RecipeCell::INGREDIENT_C, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY},
        BlockType::DYE_MAGENTA, 3, false,
        {BlockType::DYE_PINK, BlockType::AIR, BlockType::AIR},
        {BlockType::AIR, BlockType::AIR, BlockType::AIR}
    },
    {
        {BlockType::DYE_LAPIS, BlockType::AIR, BlockType::AIR}, {BlockType::DYE_RED, BlockType::AIR, BlockType::AIR},
        {RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_B, RecipeCell::INGREDIENT_B, RecipeCell::INGREDIENT_C, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY},
        BlockType::DYE_MAGENTA, 4, false,
        {BlockType::BONE_MEAL, BlockType::AIR, BlockType::AIR},
        {BlockType::AIR, BlockType::AIR, BlockType::AIR}
    },
    {
        {BlockType::PUMPKIN, BlockType::AIR, BlockType::AIR}, {BlockType::TORCH, BlockType::AIR, BlockType::AIR},
        {RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_B, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY},
        BlockType::JACK_O_LANTERN, 1, false
    },
    {
        {BlockType::SLIMEBALL, BlockType::AIR, BlockType::AIR}, {BlockType::PISTON_BASE, BlockType::AIR, BlockType::AIR},
        {RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_B, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY},
        BlockType::PISTON_STICKY, 1, false
    },
    {
        {BlockType::STRING, BlockType::AIR, BlockType::AIR}, {BlockType::AIR, BlockType::AIR, BlockType::AIR},
        {RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY},
        BlockType::WOOL_WHITE, 1, false
    },
    {
        {BlockType::BLOCK_GOLD, BlockType::AIR, BlockType::AIR}, {BlockType::APPLE, BlockType::AIR, BlockType::AIR},
        {RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_B, RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A},
        BlockType::GOLDEN_APPLE, 1, true
    },
    {
        {BlockType::MILK_BUCKET, BlockType::AIR, BlockType::AIR}, {BlockType::SUGAR, BlockType::AIR, BlockType::AIR},
        {RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_B, RecipeCell::INGREDIENT_C, RecipeCell::INGREDIENT_B, RecipeCell::INGREDIENT_D, RecipeCell::INGREDIENT_D, RecipeCell::INGREDIENT_D},
        BlockType::CAKE_ITEM, 1, true,
        {BlockType::EGG, BlockType::AIR, BlockType::AIR},
        {BlockType::WHEAT, BlockType::AIR, BlockType::AIR}
    },
    // Tocadiscos: 8 tablones y un diamante en el centro
    {
        {BlockType::WOOD_PLANKS, BlockType::AIR}, {BlockType::DIAMOND, BlockType::AIR},
        {RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_B, RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A},
        BlockType::JUKEBOX, 1, true
    },
    // Cuadro: 8 palos y una lana (de cualquier color) en el centro
    {
        {BlockType::STICK, BlockType::AIR}, {BlockType::WOOL_WHITE, BlockType::WOOL_BLACK, BlockType::AIR},
        {RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_B, RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A},
        BlockType::PAINTING, 1, true
    },
    // ---- RecipesArmor: cada pieza con 5, 8, 7 o 4 unidades del material (la malla se hace con fuego en la Beta y no se puede fabricar) ----
    {
        {BlockType::LEATHER, BlockType::AIR}, {BlockType::AIR, BlockType::AIR},
        {RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::EMPTY, RecipeCell::INGREDIENT_A, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY},
        BlockType::HELMET_LEATHER, 1, true
    },
    {
        {BlockType::LEATHER, BlockType::AIR}, {BlockType::AIR, BlockType::AIR},
        {RecipeCell::INGREDIENT_A, RecipeCell::EMPTY, RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A},
        BlockType::CHESTPLATE_LEATHER, 1, true
    },
    {
        {BlockType::LEATHER, BlockType::AIR}, {BlockType::AIR, BlockType::AIR},
        {RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::EMPTY, RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::EMPTY, RecipeCell::INGREDIENT_A},
        BlockType::LEGGINGS_LEATHER, 1, true
    },
    {
        {BlockType::LEATHER, BlockType::AIR}, {BlockType::AIR, BlockType::AIR},
        {RecipeCell::INGREDIENT_A, RecipeCell::EMPTY, RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::EMPTY, RecipeCell::INGREDIENT_A, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY},
        BlockType::BOOTS_LEATHER, 1, true
    },
    {
        {BlockType::IRON_INGOT, BlockType::AIR}, {BlockType::AIR, BlockType::AIR},
        {RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::EMPTY, RecipeCell::INGREDIENT_A, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY},
        BlockType::HELMET_IRON, 1, true
    },
    {
        {BlockType::IRON_INGOT, BlockType::AIR}, {BlockType::AIR, BlockType::AIR},
        {RecipeCell::INGREDIENT_A, RecipeCell::EMPTY, RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A},
        BlockType::CHESTPLATE_IRON, 1, true
    },
    {
        {BlockType::IRON_INGOT, BlockType::AIR}, {BlockType::AIR, BlockType::AIR},
        {RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::EMPTY, RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::EMPTY, RecipeCell::INGREDIENT_A},
        BlockType::LEGGINGS_IRON, 1, true
    },
    {
        {BlockType::IRON_INGOT, BlockType::AIR}, {BlockType::AIR, BlockType::AIR},
        {RecipeCell::INGREDIENT_A, RecipeCell::EMPTY, RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::EMPTY, RecipeCell::INGREDIENT_A, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY},
        BlockType::BOOTS_IRON, 1, true
    },
    {
        {BlockType::DIAMOND, BlockType::AIR}, {BlockType::AIR, BlockType::AIR},
        {RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::EMPTY, RecipeCell::INGREDIENT_A, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY},
        BlockType::HELMET_DIAMOND, 1, true
    },
    {
        {BlockType::DIAMOND, BlockType::AIR}, {BlockType::AIR, BlockType::AIR},
        {RecipeCell::INGREDIENT_A, RecipeCell::EMPTY, RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A},
        BlockType::CHESTPLATE_DIAMOND, 1, true
    },
    {
        {BlockType::DIAMOND, BlockType::AIR}, {BlockType::AIR, BlockType::AIR},
        {RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::EMPTY, RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::EMPTY, RecipeCell::INGREDIENT_A},
        BlockType::LEGGINGS_DIAMOND, 1, true
    },
    {
        {BlockType::DIAMOND, BlockType::AIR}, {BlockType::AIR, BlockType::AIR},
        {RecipeCell::INGREDIENT_A, RecipeCell::EMPTY, RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::EMPTY, RecipeCell::INGREDIENT_A, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY},
        BlockType::BOOTS_DIAMOND, 1, true
    },
    {
        {BlockType::GOLD_INGOT, BlockType::AIR}, {BlockType::AIR, BlockType::AIR},
        {RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::EMPTY, RecipeCell::INGREDIENT_A, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY},
        BlockType::HELMET_GOLD, 1, true
    },
    {
        {BlockType::GOLD_INGOT, BlockType::AIR}, {BlockType::AIR, BlockType::AIR},
        {RecipeCell::INGREDIENT_A, RecipeCell::EMPTY, RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A},
        BlockType::CHESTPLATE_GOLD, 1, true
    },
    {
        {BlockType::GOLD_INGOT, BlockType::AIR}, {BlockType::AIR, BlockType::AIR},
        {RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::EMPTY, RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::EMPTY, RecipeCell::INGREDIENT_A},
        BlockType::LEGGINGS_GOLD, 1, true
    },
    {
        {BlockType::GOLD_INGOT, BlockType::AIR}, {BlockType::AIR, BlockType::AIR},
        {RecipeCell::INGREDIENT_A, RecipeCell::EMPTY, RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::EMPTY, RecipeCell::INGREDIENT_A, RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::EMPTY},
        BlockType::BOOTS_GOLD, 1, true
    },
    // ---- brujula, reloj, mapa y cana de pescar (RecipesTools / CraftingManager) ----
    {
        {BlockType::IRON_INGOT, BlockType::AIR}, {BlockType::REDSTONE, BlockType::AIR},
        {RecipeCell::EMPTY, RecipeCell::INGREDIENT_A, RecipeCell::EMPTY, RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_B, RecipeCell::INGREDIENT_A, RecipeCell::EMPTY, RecipeCell::INGREDIENT_A, RecipeCell::EMPTY},
        BlockType::COMPASS, 1, true
    },
    {
        {BlockType::GOLD_INGOT, BlockType::AIR}, {BlockType::REDSTONE, BlockType::AIR},
        {RecipeCell::EMPTY, RecipeCell::INGREDIENT_A, RecipeCell::EMPTY, RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_B, RecipeCell::INGREDIENT_A, RecipeCell::EMPTY, RecipeCell::INGREDIENT_A, RecipeCell::EMPTY},
        BlockType::CLOCK, 1, true
    },
    {
        {BlockType::PAPER, BlockType::AIR}, {BlockType::COMPASS, BlockType::AIR},
        {RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_B, RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_A},
        BlockType::MAP, 1, true
    },
    {
        {BlockType::STICK, BlockType::AIR}, {BlockType::STRING, BlockType::AIR},
        {RecipeCell::EMPTY, RecipeCell::EMPTY, RecipeCell::INGREDIENT_A, RecipeCell::EMPTY, RecipeCell::INGREDIENT_A, RecipeCell::INGREDIENT_B, RecipeCell::INGREDIENT_A, RecipeCell::EMPTY, RecipeCell::INGREDIENT_B},
        BlockType::FISHING_ROD, 1, true
    },

};
static const int RECIPE_COUNT = sizeof(RECIPES) / sizeof(RECIPES[0]);

// Cuenta cuántas celdas del patrón corresponden a un ingrediente —
// esto reemplaza el countA/countB de antes, calculado dinámicamente
inline int countPatternCells(const Recipe& r, RecipeCell which) {
    int count = 0;
    for (int i = 0; i < 9; i++) {
        if (r.pattern[i] == which) count++;
    }
    return count;
}

inline bool ingredientMatches(BlockType t, const BlockType alt[3]) {
    if (alt[0] == BlockType::WOOL_WHITE && alt[1] == BlockType::WOOL_BLACK && isWool(t)) return true;   // marcador {blanca, negra}: lana de cualquier color (cama); {blanca} sola = solo blanca
    for (int a = 0; a < 3; a++) {
        if (a > 0 && alt[a] == BlockType::AIR) continue; // alternativa sin usar
        if (t == alt[a]) return true;
    }
    return false;
}

inline int countMatchingIngredient(const Inventory& inv, const BlockType alt[3]) {
    int total = 0;
    for (int i = 0; i < Inventory::TOTAL_SIZE; i++) {
        if (inv.isSlotEmpty(i)) continue;
        if (ingredientMatches(inv.getSlotType(i), alt)) {
            total += inv.getSlotCount(i);
        }
    }
    return total;
}

inline void consumeMatchingIngredient(Inventory& inv, const BlockType alt[3], int amount) {
    int remaining = amount;
    for (int i = 0; i < Inventory::TOTAL_SIZE && remaining > 0; i++) {
        if (inv.isSlotEmpty(i)) continue;
        if (ingredientMatches(inv.getSlotType(i), alt)) {
            if (inv.getSlotType(i) == BlockType::MILK_BUCKET) {   // pastel: los baldes de leche encontrados se devuelven vacios
                inv.setSlotDirect(i, BlockType::BUCKET, 1);
                remaining -= 1;
                continue;
            }
            int available = inv.getSlotCount(i);
            int toTake = (remaining < available) ? remaining : available;
            inv.removeFromSlot(i, toTake);
            remaining -= toTake;
        }
    }
}

inline bool canCraft(const Inventory& inv, const Recipe& r) {
    int countA = countPatternCells(r, RecipeCell::INGREDIENT_A);
    if (countMatchingIngredient(inv, r.altA) < countA) return false;

    int countB = countPatternCells(r, RecipeCell::INGREDIENT_B);
    if (countB > 0) {
        if (countMatchingIngredient(inv, r.altB) < countB) return false;
    }
    int countC = countPatternCells(r, RecipeCell::INGREDIENT_C);
    if (countC > 0) {
        if (countMatchingIngredient(inv, r.altC) < countC) return false;
    }
    int countD = countPatternCells(r, RecipeCell::INGREDIENT_D);
    if (countD > 0) {
        if (countMatchingIngredient(inv, r.altD) < countD) return false;
    }
    return true;
}

inline bool doCraft(Inventory& inv, const Recipe& r) {
    if (!canCraft(inv, r)) return false;

    int countA = countPatternCells(r, RecipeCell::INGREDIENT_A);
    consumeMatchingIngredient(inv, r.altA, countA);

    int countB = countPatternCells(r, RecipeCell::INGREDIENT_B);
    if (countB > 0) {
        consumeMatchingIngredient(inv, r.altB, countB);
    }
    int countC = countPatternCells(r, RecipeCell::INGREDIENT_C);
    if (countC > 0) {
        consumeMatchingIngredient(inv, r.altC, countC);
    }
    int countD = countPatternCells(r, RecipeCell::INGREDIENT_D);
    if (countD > 0) {
        consumeMatchingIngredient(inv, r.altD, countD);
    }

    if (hasDurability(r.result)) {
        inv.addTool(r.result);
    } else {
        bool usedNewSlot;
        inv.addItem(r.result, r.resultCount, usedNewSlot);
    }
    return true;
}

#endif