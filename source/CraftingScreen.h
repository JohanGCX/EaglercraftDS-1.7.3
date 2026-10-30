#ifndef CRAFTINGSCREEN_H
#define CRAFTINGSCREEN_H

#include "Inventory.h"

struct FurnaceData;
struct ChestView;

class CraftingScreen {
public:
    static void show(const Inventory& inventory, int selectedRecipe);
    static void showTop(int selectedRecipe);

    static bool isPreviousTouched(int touchX, int touchY);
    static bool isNextTouched(int touchX, int touchY);
    static bool isCraftTouched(int touchX, int touchY);
    static void resetPage();            // al abrir el crafteo: vuelve a la pagina 1
    static bool changePage(int delta);  // next / previous: true si cambio de pagina (hay que volver a dibujar)

    // Retorna el índice REAL de receta (no la posición visible), o -1
    static int getTouchedRecipeSlot(int touchX, int touchY);

    static int getBgId() { return bgId; }
    static void show(const Inventory& inventory, int selectedRecipe, bool usingTable);
    static void showTop(int selectedRecipe, bool usingTable);
    static void showFurnaceTop(bool lit, const FurnaceData* data, int selectedSlot);
    static void updateFurnaceAnim(const FurnaceData* data);   // repinta solo la llama y la flecha
    static bool furnaceAnimStale(const FurnaceData* data);   // la llama o la flecha ya no coinciden con lo dibujado
    static void showChestTop(const ChestView& view, int selectedSlot);
    static void invalidate() { bgId = -1; }

private:
    static int bgId;
    static int visibleRecipeIndices[36];
    static int visibleCount;
};

#endif