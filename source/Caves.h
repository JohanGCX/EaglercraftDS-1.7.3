#ifndef CAVES_H
#define CAVES_H

#include <stdint.h>
#include "WorldConfig.h"
#include "BlockType.h"

// Cuevas de Minecraft Beta 1.7.3 (MapGenBase + MapGenCaves), adaptadas a chunks de 8x8.
//
// El original simula los túneles de los 17x17 chunks de 16x16 que rodean al chunk que se genera, y
// descarta lo que no llega. Aquí cada "celda" de 16x16 bloques simula sus túneles UNA sola vez y guarda
// la lista de esferas que el original excavaría; cada chunk de 8x8 solo lee esas listas. Como todos
// los chunks leen la misma lista, las cuevas no tienen cortes en los bordes entre chunks.

// Tu terreno es más bajo que el de la Beta (mar en 38 contra 63). Estas escalas se calibraron para que
// la proporción de piedra hueca y de bocas en la superficie sea la del original (error medio 5%).
#ifndef CAVES_ORIGIN_Y_SCALE
#define CAVES_ORIGIN_Y_SCALE 0.9f
#endif
#ifndef CAVES_RADIUS_SCALE
#define CAVES_RADIUS_SCALE 0.9f
#endif

namespace Caves {

    // Celdas a cada lado que se consultan. El original usa 8, pero el 99,96% de las esferas queda a
    // 5 celdas o menos de su origen y ninguna pasa de 6.
    const int REACH_CELLS = 5;

    // Esfera a excavar. Posiciones RELATIVAS a la esquina de su celda de origen, en 1/8 de bloque
    // (así caben en 16 bits aunque el jugador esté a miles de bloques del inicio). Radio en 1/16.
    struct Sphere {
        int16_t x8, y8, z8;
        uint8_t r16;
        uint8_t flat;   // 1 = sala (achatada a la mitad en vertical)
    };

    void reset(unsigned int worldSeed); // mundo nuevo: vacía el caché
    void beginFrame();                  // una vez por cuadro (marca de uso para el caché)

    // Hace avanzar la simulación de las celdas que necesita el chunk (chunkX, chunkZ).
    // Devuelve true cuando están todas listas. workDone indica si se gastó CPU en este llamado.
    bool prepare(int chunkX, int chunkZ, bool& workDone);

    // Posición de una excavación en curso (para repartirla en varios cuadros)
    struct CarveCursor {
        int cell;     // celda actual: 0 .. (2*REACH_CELLS+1)^2 - 1
        int sphere;   // esfera dentro de esa celda
        CarveCursor() : cell(0), sphere(0) {}
    };

    // Excava, como mucho, 'maxCarves' esferas que toquen el chunk y devuelve true cuando terminó.
    // Requiere que prepare() haya devuelto true en este mismo cuadro. Siempre recorre las esferas en el
    // mismo orden, así que el resultado no depende de cómo se reparta en cuadros.
    bool carveStep(BlockType (*blocks)[WorldConfig::SIZE_Y][WorldConfig::SIZE_Z],
                   int chunkX, int chunkZ, CarveCursor& cur, int maxCarves);

    // Excava una esfera dentro de un chunk de 8x8x128. Reemplaza piedra, tierra y pasto (como el
    // original), no excava si hay agua en su caja dentro de este chunk, corre el pasto hacia abajo
    // y quita las plantas que quedarían flotando.
    void carveSphere(BlockType (*blocks)[WorldConfig::SIZE_Y][WorldConfig::SIZE_Z],
                     int chunkX, int chunkZ, const Sphere& s);

    // Estadísticas (para el perfilador)
    int cellsSimulated();
    int cellsTruncated();   // celdas a las que les faltaron esferas por falta de banco (debería ser 0)
    int evictions();        // celdas expulsadas del caché
    int blocksFree();
}

#endif
