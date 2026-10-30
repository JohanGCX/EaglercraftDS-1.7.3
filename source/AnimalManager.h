#ifndef ANIMALMANAGER_H
#define ANIMALMANAGER_H

#include "BlockType.h"
#include "MobBody.h"

class ChunkManager;

// Las vacas, cerdos y gallinas del juego: como mucho 2 de cada una (6 en total), aparecen en el chunk del jugador o en uno de los 8 de alrededor y se
// borran al instante si su chunk queda fuera de esos 9. La logica esta en AnimalAI.h; aqui se conecta con el mundo, el dibujo y el combate.
// SheepManager los reenvia (sus indices van despues de los de las ovejas), por eso main.cpp casi no cambia.
namespace AnimalManager {
    const int MAX_PER_KIND = 4;
    const int MAX_ANIMALS = 6;
    const int ANIMAL_SPAWN_CAP = 2;   // tope blando de vacas/cerdos/gallinas (con las 2 ovejas son ~4 en total, cerca de la densidad de la Beta)

    struct Drop { BlockType type; int count; float x, y, z; };

    void clear(unsigned int seed);      // borra todos (al entrar a un mundo o cambiar de dimension)
    void lightning(double x, double y, double z);   // EntityLightningBolt: el cerdo se vuelve zombi cerdo; los demas reciben 5 de daño y fuego
    bool anchor(int index, float& x, float& y, float& z);   // la caña: donde se pega el anzuelo (x, 80% de la altura, z) de un animal vivo
    void pull(int index, float vx, float vy, float vz);     // la caña: suma velocidad
    void explosionDamage(const ChunkManager& cm, double ex, double ey, double ez, float size);   // la onda de una explosion en (ex, ey, ez) del mundo
    void setActive(bool active);        // false en el Nether: no aparecen
    int count();

    // 20 Hz. (px, py, pz): los ojos del jugador.
    void tick(const ChunkManager& cm, float px, float py, float pz);
    bool popDrop(Drop& out);
    bool spawnChicken(double x, double y, double z);   // EntityEgg: una gallina nueva en (x, y, z) del mundo (false si no hay lugar)

    void draw(const ChunkManager& cm, float partialTick);

    int pick(float ox, float oy, float oz, float dx, float dy, float dz, float maxDist, float& hitDist);   // indice del animal apuntado o -1
    bool attack(int index, int damage, float attackerX, float attackerZ);
    bool saddle(int index);                                       // ItemSaddle.saddleEntity: ensilla un cerdo vivo sin silla (true si lo hizo: se gasta la silla)
    bool mount(int index);                                        // EntityPig.interact: sube al jugador a un cerdo con silla y sin jinete
    bool isRiding();
    bool riderEye(float& x, float& y, float& z);                  // donde van los ojos del jugador montado
    bool dismount(float& x, float& y, float& z);                  // el jugador se baja (agacharse): devuelve donde quedan sus ojos
    bool takeForcedDismount(float& x, float& y, float& z);        // el cerdo murio o desaparecio: true una vez
    bool milk(int index);                                         // EntityCow.interact con un balde vacio: true si es una vaca viva
    bool overlapsBlock(float blockCenterX, float blockCenterY, float blockCenterZ);
    int collectBoxes(float out[][6], int maxN);
    int collectBodies(MobBody* out, int maxN);   // para la colision entre todos los mobs (SheepManager)
}

#endif
