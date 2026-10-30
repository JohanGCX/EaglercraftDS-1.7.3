#ifndef MINECARTMANAGER_H
#define MINECARTMANAGER_H

#include "BlockType.h"

class ChunkManager;

// Las vagonetas del juego (EntityMinecart): como mucho 8. La fisica esta en MinecartAI.h y el modelo en MinecartModel.cpp; aqui se conectan con el mundo,
// el jugador (subirse, golpearlas), el riel detector y lo que sueltan. Las posiciones que entran y salen van en coordenadas del MUNDO (las de la camara).
namespace MinecartManager {
    const int MAX_CARTS = 8;

    // Lo que suelta una vagoneta al romperse: ella misma, el cofre o el horno, y lo que llevaba dentro
    struct Drop { BlockType type; int count; int durability; float x, y, z; };

    void clear();            // al entrar a un mundo o cambiar de dimension
    int count();

    // ItemMinecart.onItemUse: pone una vagoneta sobre el riel de la celda (gx, gy, gz). type: 0 normal, 1 cofre, 2 horno. false si no hay riel o no caben mas
    bool spawn(const ChunkManager& cm, int gx, int gy, int gz, int type);

    // 20 Hz. (px, py, pz): los ojos del jugador. Si va dentro de una vagoneta, la mueve y deja al jugador donde le toca (riderEye)
    void tick(const ChunkManager& cm, float px, float py, float pz);
    bool popDrop(Drop& out);

    void draw(const ChunkManager& cm, float partialTick);

    // Subirse: el jugador va en la vagoneta 'riding' (-1 = a pie). Los ojos del jugador mientras va dentro: world.
    bool isRiding();
    bool riderEye(float& x, float& y, float& z);                 // false si no va en ninguna
    bool dismount(float& eyeX, float& eyeY, float& eyeZ);        // se baja: donde quedan sus ojos (encima de la vagoneta). false si no iba en ninguna
    bool takeForcedDismount(float& eyeX, float& eyeY, float& eyeZ);   // la vagoneta se rompio con el jugador dentro

    // Combate y uso (Minecraft.getMouseOver, Entity.attackEntityFrom, EntityMinecart.interact)
    int pick(float ox, float oy, float oz, float dx, float dy, float dz, float maxDist, float& hitDist);   // indice de la vagoneta apuntada o -1
    void attack(int index, int damage);
    enum { INTERACT_MOUNTED = 1, INTERACT_CHEST = 2, INTERACT_COAL_USED = 4 };
    int interact(int index, bool holdingCoal, float playerX, float playerZ);
    int chestUid(int index);                                     // clave del cofre de la vagoneta con cofre: ChestStore (uid, CHEST_KEY_Y, 0)
    const int CHEST_KEY_Y = -7;

    int collectBoxes(float out[][6], int maxN);                  // cajas del mundo de las vagonetas (el riel detector y las placas de madera)

    bool saveToFile(const char* path);
    bool loadFromFile(const char* path);
}

#endif
