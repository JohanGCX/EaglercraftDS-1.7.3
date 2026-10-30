#ifndef STARTUPSPLASH_H
#define STARTUPSPLASH_H

// Pantalla de arranque: logo de Eagtek y luego el de Mojang en la pantalla de arriba, 1 segundo cada uno, con la de abajo en blanco.
namespace StartupSplash {
    void show();     // los dos logos (unos 2 segundos); al volver, el de Mojang y la pantalla blanca se quedan fijos mientras todo carga
    void finish();   // justo antes de dibujar el menu: devuelve la pantalla de abajo a su estado normal
}

#endif
