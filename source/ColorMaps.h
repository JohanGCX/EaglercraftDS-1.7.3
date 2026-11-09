#ifndef COLORMAPS_H
#define COLORMAPS_H

#include <nds.h>

// ColorizerGrass / ColorizerFoliage de la Beta 1.7.3: el color sale de un mapa indexado por temperatura y humedad (grasscolor.png y
// foliagecolor.png). Aquí el mapa de 256 x 256 se reemplazó por una tabla de 33 x 33 con interpolación (ColorMapData.h): casi igual
// de preciso para un degradado tan suave y ocupa unos 6 KB en vez de 256 KB.
// Un texture pack puede traer sus propios mapas (misc/grasscolor.png y misc/foliagecolor.png): TexturePack los pasa con setCustom.
namespace ColorMaps {
    const int TABLE_BYTES = 33 * 33 * 3;   // RGB del pixel (min(i * 8, 255), min(j * 8, 255)) del mapa, i y j de 0 a 32

    void grass(float temperature, float humidity, u8& r, u8& g, u8& b);
    void foliage(float temperature, float humidity, u8& r, u8& g, u8& b);

    // Tablas del pack (se copian a un bloque de memoria propio de 6,5 KB); nullptr = la de siempre para ese mapa
    bool setCustom(const u8* grassTable, const u8* foliageTable);
    void useDefault();                     // vuelve a las tablas del juego y suelta la memoria
    const u8* grassTable();                // la tabla en uso
    const u8* foliageTable();
}

#endif
