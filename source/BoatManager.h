#ifndef BOATMANAGER_H
#define BOATMANAGER_H

#include "BlockType.h"

class ChunkManager;

// Los botes del juego (EntityBoat): como mucho 8. La fisica esta en BoatAI.h y el modelo en BoatModel.cpp; aqui se conectan con el mundo, el jugador
// (subirse, golpearlos) y lo que sueltan. Las posiciones que entran y salen van en coordenadas del MUNDO (las de la camara).
namespace BoatManager {
    const int MAX_BOATS = 8;

    struct Drop { BlockType type; int count; int durability; float x, y, z; };

    void clear();            // al entrar a un mundo o cambiar de dimension
    int count();

    // ItemBoat.onItemRightClick: un rayo de 5 bloques desde los ojos (se detiene en el agua) y el bote aparece sobre la celda que toca. false si no toca nada o no caben mas
    bool spawnFromRay(const ChunkManager& cm, float ox, float oy, float oz, float dx, float dy, float dz);

    // 20 Hz. (inX, inZ): hacia donde empuja el jinete con las teclas (modulo hasta 1; 0 si no va nadie dentro)
    void tick(const ChunkManager& cm, float inX, float inZ);
    bool popDrop(Drop& out);

    void draw(const ChunkManager& cm, float partialTick);

    bool isRiding();
    bool riderEye(float& x, float& y, float& z);                 // los ojos del jugador mientras va dentro; false si no va en ninguno
    bool dismount(float& eyeX, float& eyeY, float& eyeZ);        // se baja: donde quedan sus ojos (encima del bote)
    bool takeForcedDismount(float& eyeX, float& eyeY, float& eyeZ);   // el bote se rompio o se perdio con el jugador dentro

    int pick(float ox, float oy, float oz, float dx, float dy, float dz, float maxDist, float& hitDist);   // indice del bote apuntado o -1
    void attack(int index, int damage);
    bool interact(int index);                                    // EntityBoat.interact: el jugador se sube. true si se subio

    bool saveToFile(const char* path);
    bool loadFromFile(const char* path);
}

#endif
