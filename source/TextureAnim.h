#ifndef TEXTUREANIM_H
#define TEXTUREANIM_H

// Texturas animadas del atlas del terreno: el fuego (celdas (15,1) y (15,2)) y el portal (celda (14,0)). Cada 3 VBlank (20 por segundo,
// como los ticks del juego) se copia el cuadro siguiente de una tira de 32 sobre la celda, directamente en la VRAM de texturas. Las listas
// de dibujo de los chunks no se tocan: ya apuntan a esas celdas.
namespace TextureAnim {
    // Llamar una vez, con el atlas del terreno ya cargado (Texture::loadTerrainAtlas)
    void init();

    // Llamar en cada cuadro justo DESPUÉS de swiWaitForVBlank(): el 3D no está dibujando y se puede escribir en la VRAM de texturas
    void onVBlank();
}

#endif