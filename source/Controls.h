#ifndef CONTROLS_H
#define CONTROLS_H

#include <nds.h>

// Controles personalizables. Cada modo de controles (0 normal, 1 xyba) tiene su propio perfil: un boton por accion, guardado en
// /mcdsefiles/controlsprofile.dat. Una accion sin boton (0) queda desactivada. Los botones de la DS que se pueden asignar:
// A B X Y L R + cruceta + SELECT (START es la pausa y no se puede cambiar).
namespace Controls {
    enum Action {
        FORWARD, BACK, STRAFE_LEFT, STRAFE_RIGHT, JUMP, BREAK, PLACE, SNEAK, THIRD_PERSON,
        LOOK_LEFT, LOOK_RIGHT, LOOK_UP, LOOK_DOWN,
        COUNT
    };

    void load();                                   // al arrancar (despues de Options::load)
    bool save();
    void resetProfile(int mode);                   // solo el perfil indicado
    u16  key(Action a);                            // boton de la accion en el perfil activo (Options::controlMode); 0 = ninguno
    u16  getKey(int mode, Action a);
    void bind(int mode, Action a, u16 keyMask);    // si otra accion del perfil usaba ese boton, intercambian (queda con el que tenia esta)
    void cycle(int mode, Action a, int dir);       // siguiente / anterior boton de la lista (incluye "none")
    bool isBindable(u32 keysDown, u16& outKey);    // primer boton asignable presionado en keysDown
    const char* keyName(u16 keyMask);
    const char* actionName(Action a);

    // Teclas "de juego" para Player::handleInput: devuelve keysHeld con cruceta y A reemplazadas por lo que el perfil asigna
    // a adelante / atras / izquierda / derecha / saltar (Player sigue leyendo KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT y KEY_A)
    u32 gameHeld(u32 keysHeld);
}

#endif
