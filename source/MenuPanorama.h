#ifndef MENUPANORAMA_H
#define MENUPANORAMA_H

// El fondo giratorio del menú principal (el panorama de Minecraft desde la Beta 1.8): un cubo con las 6 imágenes visto desde adentro.
// La textura (panorama.png: 256 x 128, 6 caras de 64 x 64) se carga al entrar al menú y se libera al salir, para no gastar VRAM en el juego.
namespace MenuPanorama {
    bool load();           // false si no hay VRAM de texturas suficiente (el menú se queda con el logo estático)
    void unload();
    void draw(float ticks); // dibuja el cubo; ticks = tiempo en ticks de juego (20 por segundo). No hace glFlush
}

#endif