#ifndef HOSTILEMANAGER_H
#define HOSTILEMANAGER_H

#include "BlockType.h"
#include "MobBody.h"

class ChunkManager;

// Los mobs con IA propia: zombie, esqueleto, araña, creeper, slime (overworld), zombi cerdo y ghast (Nether), lobo (bosque y taiga) y calamar (agua).
// Aparecen como en SpawnerAnimals (Beta 1.7.3): se elige un tipo de la lista del bioma segun su peso, un punto al azar y, de ahi, hasta 3 racimos de 4
// pasos de +-5 bloques; cada tipo tiene su tamano de manada (4; ghast 1; lobo 8) y sus condiciones de luz y sitio. Tope: 10 monstruos, 4 lobos y 3
// calamares a la vez (la Beta usa 70 / 15 / 5 por cada 256 chunks de 16 x 16 alrededor del jugador; aqui hay muchos menos chunks). Se borran al
// instante si su chunk queda a mas de 3 chunks. La logica esta en HostileAI.h; aqui se conecta con el mundo, el dibujo y el combate.
// SheepManager los reenvia (sus indices van despues de los de los animales), por eso main.cpp casi no cambia.
namespace HostileManager {
    const int MAX_MOBS = 16;
    const int MONSTER_CAP = 10;
    const int WOLF_CAP = 4;
    const int SQUID_CAP = 3;
    const int MAX_ARROWS = 8;
    const int MAX_FIREBALLS = 4;

    struct Drop { BlockType type; int count; float x, y, z; };
    // Un golpe al jugador: un mob, una flecha o una explosion. main aplica la vida y el empuje.
    struct PlayerHit { int amount; float srcX, srcZ; bool explosion; float kx, ky, kz; };

    void clear(unsigned int seed);      // borra todos (al entrar a un mundo o cambiar de dimension)
    void setNether(bool nether);        // Nether: solo zombis cerdo y ghasts, sin cielo que mirar
    void setActive(bool active);
    void setPeaceful(bool peaceful);    // dificultad pacifica: no aparecen monstruos y los que hay desaparecen
    int count();

    // 20 Hz. (px, py, pz): los ojos del jugador.
    void tick(const ChunkManager& cm, float px, float py, float pz, bool playerAlive);
    // Empuje mutuo jugador <-> mobs; vx, vz: lo que se le suma a la velocidad del jugador. Una vez por tick de fisica.
    void pushWithPlayer(float px, float py, float pz, float& vx, float& vz);
    // Lobos domados: se guardan con el mundo (wolves.dat de cada dimension) y no desaparecen por distancia: si se quedan lejos, aparecen junto al jugador
    // Una explosion en (x, y, z) del mundo (el TNT): rompe bloques, daña al jugador y a los mobs. Se aplica en el siguiente tick
    // Quien mas debe recibir la onda de cada explosion (animales y ovejas): lo pone SheepManager
    typedef void (*ExplosionListener)(const ChunkManager& cm, double ex, double ey, double ez, float size);
    void setExplosionListener(ExplosionListener l);
    void explodeAt(double x, double y, double z, float size, bool flaming);
    bool saveWolves(const char* path);
    bool loadWolves(const char* path);   // quedan pendientes hasta el primer tick
    void clearWolves();                   // olvida los pendientes (se llama antes de cargar un mundo)
    bool popDrop(Drop& out);
    bool popPlayerHit(PlayerHit& out);

    // SpawnerAnimals.performSleepSpawning: con el jugador ya dormido del todo, hasta 20 intentos de hacer aparecer una arana, un zombi o un esqueleto a menos de 32
    // bloques con camino hasta la cama; el mob aparece junto a la cama (la segunda celda libre de su alrededor). Devuelve true si aparecio uno: entonces el jugador se
    // despierta y la noche NO pasa. (px, pz): ojos del jugador; (bedI, bedJ, bedK): la mitad de la cama donde duerme, en celdas de mob (j = y de la Beta);
    // (offX, offZ): Partial::bedOffset de la cama.
    bool sleepSpawn(const ChunkManager& cm, float px, float pz, int bedI, int bedJ, int bedK, int offX, int offZ);

    int spawnerSpawn(const ChunkManager& cm, int mobKind, int ci, int cj, int ck);   // el generador de monstruos (Spawners.cpp): -1 tope de su clase, 1 aparecio, 0 sin sitio

    void draw(const ChunkManager& cm, float partialTick);
    void setCamera(float yaw, float pitch);

    int pick(float ox, float oy, float oz, float dx, float dy, float dz, float maxDist, float& hitDist);   // indice del mob apuntado o -1
    bool attack(int index, int damage, float attackerX, float attackerZ);
    void lightning(double x, double y, double z);   // EntityLightningBolt: 5 de daño y fuego a los mobs a menos de 3 bloques
    bool spawnPigZombie(double x, double y, double z);   // EntityPig.onStruckByLightning
    bool anchor(int index, float& x, float& y, float& z);   // la caña: donde se pega el anzuelo (x, 80% de la altura, z) de un mob vivo
    void pull(int index, float vx, float vy, float vz);     // la caña: suma velocidad
    // EntityWolf.interact. held: 0 nada, 1 hueso, 2 carne de cerdo. Devuelve 0 si no hizo nada, 1 si gasto el objeto (hueso o carne), 2 si el lobo se sento/paro
    int interact(int index, int held);
    void setHeldKind(int held);         // lo que lleva el jugador en la mano (el lobo inclina la cabeza con un hueso o carne)
    bool overlapsBlock(float blockCenterX, float blockCenterY, float blockCenterZ);
    int collectBoxes(float out[][6], int maxN);
    int collectBodies(MobBody* out, int maxN);   // para la colision entre todos los mobs (SheepManager)
}

#endif
