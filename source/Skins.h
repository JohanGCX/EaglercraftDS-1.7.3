#ifndef SKINS_H
#define SKINS_H

#include "TexturePack.h"

// Skins del jugador: /mcdsefiles/skins/<nombre>.png. Vale la skin clasica de la Beta (64 x 32) y la moderna (64 x 64), y sus versiones HD
// (128, 256... de ancho: se reducen). El modelo usa solo la mitad de arriba (el formato clasico): de una skin moderna se toma esa parte.
// Igual que los texture packs: la primera vez se convierte a 256 colores y se guarda en /mcdsefiles/skinscache/<nombre>.bin, la skin se escribe
// encima de la del juego (charBitmap / charPal, sin RAM extra) y la original queda en /mcdsefiles/skinscache/default.bin.
namespace Skins {
    void boot();   // al arrancar, ANTES de Texture::loadSkinAtlas. Si la skin elegida ya no esta o no sirve, vuelve a Default.
    int scan(TexturePack::Entry* out, int max);   // title = nombre del archivo sin .png
    const char* activeName();                     // "" = Default
    bool apply(const char* name);                 // "" = Default
    const char* lastError();
}

#endif
