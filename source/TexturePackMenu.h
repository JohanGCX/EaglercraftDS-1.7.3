#ifndef TEXTUREPACKMENU_H
#define TEXTUREPACKMENU_H

#include <nds.h>

// Botones "Skins" y "Texture Packs" del menu principal: lista Default y las skins de /mcdsefiles/skins o los packs validos de
// /mcdsefiles/texturepacks (consola de la pantalla de abajo, como Options). A elige (se aplica al momento), B vuelve.
namespace TexturePackMenu {
    void open(bool skins = false);
    bool update(u32 keysDown);   // true = volver al menu principal
}

#endif
