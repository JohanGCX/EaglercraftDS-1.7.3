#ifndef PAINTINGMANAGER_H
#define PAINTINGMANAGER_H

#include "BlockType.h"

class ChunkManager;

// Cuadros (EntityPainting + EnumArt + RenderPainting + ItemPainting). Cuelgan de una pared; el motivo se elige al azar entre los que caben.
// La textura es kz.png reducida a la mitad (128 x 128, 256 colores) para que entre en la VRAM. Hasta MAX_PAINTINGS a la vez.
namespace PaintingManager {
    const int MAX_PAINTINGS = 32;

    struct Drop { BlockType type; int count; int durability; float x, y, z; };

    void clear();

    // ItemPainting.onItemUse: (gx, gy, gz) es el bloque clicado y dir la direccion del cuadro (0 = pared con la cara al norte, 1 = oeste, 2 = sur, 3 = este).
    // true si colgo uno (hay que gastar el objeto)
    bool place(const ChunkManager& cm, int gx, int gy, int gz, int dir);

    void tick(const ChunkManager& cm);          // 20 Hz: cada 100 ticks revisa que siga la pared (EntityPainting.onUpdate)
    bool popDrop(Drop& out);

    void draw(const ChunkManager& cm, float px, float py, float pz);

    int pick(float ox, float oy, float oz, float dx, float dy, float dz, float maxDist, float& hitDist);   // cuadro apuntado o -1
    void attack(int index);                      // EntityPainting.attackEntityFrom: cualquier golpe lo suelta como objeto

    bool saveToFile(const char* path);
    bool loadFromFile(const char* path);
}

#endif
