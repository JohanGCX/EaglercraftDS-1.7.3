#ifndef SETTINGSMENU_H
#define SETTINGSMENU_H

#include <nds.h>

// Consola de opciones (pantalla de abajo): cruceta arriba/abajo mueve la flecha, A o izquierda/derecha cambia el valor, B o "back" vuelve.
// Se usa desde el menu principal y desde el menu de pausa.
namespace SettingsMenu {
    void open();                  // dibuja la consola
    bool update(u32 keysDown);    // true cuando el jugador sale
}

#endif
