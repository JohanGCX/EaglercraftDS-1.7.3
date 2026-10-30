#ifndef SHEEPMANAGER_H
#define SHEEPMANAGER_H

#include "BlockType.h"

class ChunkManager;

// Las ovejas del juego: como mucho 2, aparecen en el chunk del jugador o en uno de los 8 de alrededor, y se borran al instante si su chunk queda
// fuera de esos 9. La lógica está en SheepAI.h; aquí se conecta con el mundo, el dibujo y el combate.
namespace SheepManager {
    const int MAX_SHEEP = 2;
    const int ANIMAL_BASE = MAX_SHEEP;   // los indices desde aqui son vacas, cerdos y gallinas (AnimalManager)
    const int HOSTILE_BASE = ANIMAL_BASE + 6;   // y desde aqui los hostiles (HostileManager); 6 = AnimalManager::MAX_ANIMALS

    struct Drop { BlockType type; int count; float x, y, z; bool shear; };   // lo que suelta una oveja: lana al morir, o al esquilarla (shear)

    void clear(unsigned int seed);      // borra todas (al entrar a un mundo o cambiar de dimensión)
    void explosionDamage(const ChunkManager& cm, double ex, double ey, double ez, float size);   // animales y ovejas (HostileManager lo llama en cada explosion)
    void setActive(bool active);        // false en el Nether: no aparecen ovejas
    int count();

    // 20 Hz. (px, py, pz): los ojos del jugador.
    void tick(const ChunkManager& cm, float px, float py, float pz, bool playerAlive = true);
    bool popDrop(Drop& out);

    void draw(const ChunkManager& cm, float partialTick);
    void setCamera(float yaw, float pitch);   // la mirada del jugador, para la bola de fuego del ghast

    // Combate (Minecraft.getMouseOver + EntityPlayer.attackTargetEntityWithCurrentItem)
    int pick(float ox, float oy, float oz, float dx, float dy, float dz, float maxDist, float& hitDist);   // índice de la oveja apuntada o -1
    bool attack(int index, int damage, float attackerX, float attackerZ);
    void lightning(double x, double y, double z);   // un rayo en (x, y, z): ovejas, animales y hostiles cercanos
    bool anchor(int index, float& x, float& y, float& z);   // la caña: donde se pega el anzuelo en el mob (false si ya no existe o murio)
    void pull(int index, float vx, float vy, float vz);     // la caña: suma velocidad al mob
    // EntityWolf.interact: hueso (domar, 1 de 3) o carne de cerdo (curar) en la mano, o un clic con las manos libres sobre un lobo domado (sentarlo). 0: nada, 1: se gasta el
    // objeto de la mano, 2: el lobo se sento o se paro
    int interact(int index, BlockType held);
    void setHeld(BlockType held);   // el objeto en la mano (cada tick): el lobo inclina la cabeza si es un hueso (lobo salvaje) o carne (lobo domado)
    // Empuje mutuo jugador <-> mobs; vx, vz: lo que se le suma a la velocidad del jugador. Una vez por tick de fisica.
    void pushWithPlayer(float px, float py, float pz, float& vx, float& vz);
    bool saddle(int index);   // ItemSaddle.saddleEntity: ensilla un cerdo (true si lo hizo)
    bool mount(int index);    // EntityPig.interact: monta un cerdo ensillado
    bool milk(int index);    // EntityCow.interact con un balde vacio: true si el objetivo es una vaca viva (main pone la leche en la mano)
    bool dye(int index, int fleeceColor);   // ItemDye.saddleEntity: tiñe una oveja viva con lana de otro color (true si lo hizo)
    bool shear(int index);   // EntitySheep.interact con tijeras: si tiene lana, la esquila y suelta de 2 a 4 (devuelve true si lo hizo)
    bool overlapsBlock(float blockCenterX, float blockCenterY, float blockCenterZ);   // para no colocar un bloque dentro de una oveja
    int collectBoxes(float out[][6], int maxN);   // cajas {minX,minY,minZ,maxX,maxY,maxZ} de las ovejas vivas (las placas de presion)
}

#endif