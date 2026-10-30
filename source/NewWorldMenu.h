#ifndef NEWWORLDMENU_H
#define NEWWORLDMENU_H

// Menu "Create world" (pantalla de abajo): nombre y seed con teclado virtual, Create world a la izquierda y Cancel a la derecha.
class NewWorldMenu {
public:
    enum Result { RES_NONE = 0, RES_CREATE = 1, RES_CANCEL = 2 };

    static const int MAX_NAME_LEN = 20;   // hasta 12 letras se ven al tamano normal; de 13 a 20 se achican para caber en el recuadro
    static const int MAX_SEED_DIGITS = 9; // 999.999.999 cabe en un unsigned int sin desbordar

    static void open();                   // campos vacios y dibuja el menu
    static Result update(unsigned int keysDown);   // una vez por cuadro; abre el teclado al tocar un recuadro

    // Nombre escrito ("" = vacio: el mundo se llama World N)
    static const char* getName();
    // true y la seed si el recuadro de la seed no esta vacio
    static bool getSeed(unsigned int& outSeed);
};

#endif
