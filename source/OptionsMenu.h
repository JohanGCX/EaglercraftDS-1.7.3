#ifndef OPTIONSMENU_H
#define OPTIONSMENU_H

#include <nds.h>

// Consola de opciones (pantalla de abajo): cruceta arriba/abajo mueve la flecha, A o izquierda/derecha cambia el valor, B o "back" vuelve
namespace OptionsMenu {
    void open();                  // dibuja la consola
    bool update(u32 keysDown);    // true cuando el jugador sale
}

#endif