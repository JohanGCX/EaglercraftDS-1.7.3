#ifndef WORLDSELECTMENU_H
#define WORLDSELECTMENU_H

class WorldSelectMenu {
public:
    // Arma la lista de mundos existentes (huecos incluidos) y dibuja
    // el fondo correspondiente (con o sin mundos) + el nombre inicial.
    static void init();

    // Llamar después de Create/Delete — rearma la lista y el fondo
    // (puede cambiar entre "hay mundos"/"no hay mundos").
    static void refresh();

    static void selectPrevious();
    static void selectNext();

    // Número real de carpeta (worldN) actualmente seleccionado, o -1
    // si no hay ningún mundo.
    static int getSelectedWorldNum();
    static int getWorldCount();

    static bool isCreateTouched(int touchX, int touchY);
    static bool isDeleteTouched(int touchX, int touchY);
    static bool isPlayTouched(int touchX, int touchY);
    static bool isBackTouched(int touchX, int touchY);
    static bool isLeftArrowTouched(int touchX, int touchY);
    static bool isRightArrowTouched(int touchX, int touchY);
};

#endif