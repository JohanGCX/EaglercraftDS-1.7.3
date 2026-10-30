#ifndef TEXTKEYBOARD_H
#define TEXTKEYBOARD_H

// Teclado virtual en la consola de la pantalla de abajo (se toca la pantalla). Bloquea hasta que se pulsa OK o Cancel.
//   numeric = false: qwerty con numeros, mayusculas con la tecla ^ y los simbolos - _ . y espacio
//   numeric = true : solo digitos (para la seed)
// buf trae el texto inicial y al aceptar queda con el texto nuevo (maxLen caracteres como maximo, buf con maxLen + 1 de espacio).
// Devuelve true si se acepto (OK o START) y false si se cancelo (Cancel o B). Al volver, la consola de abajo queda en uso:
// quien llama tiene que redibujar su pantalla.
namespace TextKeyboard {
    bool edit(const char* title, char* buf, int maxLen, bool numeric, bool menuTick = true);   // menuTick = false dentro de la partida (arriba no hay panorama que girar)
}

#endif
