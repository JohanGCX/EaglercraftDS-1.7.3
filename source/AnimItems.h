#ifndef ANIMITEMS_H
#define ANIMITEMS_H

// La brujula y el reloj animados (TextureCompassFX y TextureWatchFX de la Beta): la aguja apunta al punto de aparicion del mundo y el reloj marca la hora del
// ciclo de dia y noche. Cada uno es una tira de cuadros de 16 x 16 (compass.png 32, watch.png 64) que se copia a su celda del atlas de items (brujula
// (6, 3), reloj (6, 4)) y tambien se usa para los iconos 2D (barra, inventario y crafteo).
namespace AnimItems {
    void init();                  // con el atlas de items ya cargado (despues de TextureAnim::init)
    void reset();                 // al entrar a un mundo: la aguja arranca quieta

    // 20 Hz. (px, pz) y yaw del jugador (Player), (spawnX, spawnZ) el punto de aparicion y celestialAngle el de WorldProvider.calculateCelestialAngle (0 = mediodia).
    void tick(float px, float pz, float yaw, float spawnX, float spawnZ, float celestialAngle);

    bool takeChanged();           // true si algun cuadro cambio desde la ultima vez (para volver a dibujar la barra)

    void onVBlank(unsigned char* itemsTexture);   // con los bancos de texturas en LCD: copia el cuadro nuevo a la VRAM (lo llama TextureAnim)
    unsigned char* itemsTexturePointer();         // donde esta el atlas de items en la VRAM (nullptr si no se puede animar)

    // Iconos 2D: el indice de paleta (del atlas de items) del pixel (px, py) del atlas si cae en la celda de la brujula o el reloj; -1 si no
    int pixelOverride(int px, int py);
    void setInventoryAnimated(bool on);   // false: los iconos 2D se quedan con el dibujo fijo del atlas (solo la mano y el objeto tirado se animan)
}

#endif
