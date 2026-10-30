#ifndef MAPS_H
#define MAPS_H

#include <nds.h>
#include "BlockType.h"

class ChunkManager;

// El mapa (ItemMap + MapData + MapColor + MapCoord de la Beta 1.7.3). Cada mapa es de 128 x 128 pixeles de 1 byte (color * 4 + sombra), a escala 3
// (un pixel = 8 x 8 bloques). Su numero va en la durabilidad del objeto (0 = aun sin asignar). Los datos viven en maps.dat de la carpeta del mundo
// (un registro por mapa, valido en las dos dimensiones); en memoria solo caben unos pocos a la vez.
namespace Maps {
    const int SIZE = 128;
    const int CACHE = 3;

    void reset();                                  // al entrar a un mundo / cambiar de dimension: olvida lo que hay en memoria (flush antes si hay que guardar)
    void flush();                                  // escribe los mapas modificados en maps.dat

    // ItemMap.onCreated / func_28012_a: un mapa nuevo centrado en (x, z) (celda del jugador); devuelve su numero (> 0) o 0 si fallo
    int create(int centerX, int centerZ, int dimension);

    // 20 Hz, con el mapa en la mano (ItemMap.onUpdate con isSelected): explora el terreno cercano, de a una columna por tick
    void tick(const ChunkManager& cm, int id, float px, float pz, int dimension);

    // Para dibujar el mapa de la mano: los 16384 bytes o nullptr; y la marca del jugador (posicion en pixeles desde el centro, true si cae dentro)
    const u8* pixels(int id);
    bool playerMarker(int id, float px, float pz, int dimension, float& mx, float& my);   // mx, my en pixeles (-64..64); my crece hacia el sur

    // Color RGB (0..255) de un byte de mapa; false si el pixel esta sin explorar
    bool colorOf(u8 value, int& r, int& g, int& b);
}

#endif
