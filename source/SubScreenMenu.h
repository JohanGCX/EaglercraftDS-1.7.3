#ifndef SUBSCREENMENU_H
#define SUBSCREENMENU_H

class SubScreenMenu {
public:
    static void init(bool hasSave); // carga la imagen del menú (hasSave ya no se usa: hay una sola)

    static bool isNewLoadTouched(int touchX, int touchY);   // Play game
    static bool isOptionsTouched(int touchX, int touchY);   // Options
};

#endif