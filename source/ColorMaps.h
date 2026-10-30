#ifndef COLORMAPS_H
#define COLORMAPS_H

#include <nds.h>

// ColorizerGrass / ColorizerFoliage de la Beta 1.7.3: el color sale de un mapa indexado por temperatura y humedad (grasscolor.png y
// foliagecolor.png). Aquí el mapa de 256 x 256 se reemplazó por una tabla de 33 x 33 con interpolación (ColorMapData.h): casi igual
// de preciso para un degradado tan suave y ocupa unos 6 KB en vez de 256 KB.
namespace ColorMaps {
    void grass(float temperature, float humidity, u8& r, u8& g, u8& b);
    void foliage(float temperature, float humidity, u8& r, u8& g, u8& b);
}

#endif