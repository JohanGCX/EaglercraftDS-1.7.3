#ifndef PROJECTILES_H
#define PROJECTILES_H

class ChunkManager;

// Proyectiles del jugador y del dispensador (Beta 1.7.3): EntityArrow, EntitySnowball y EntityEgg. Vuelan con la misma fisica que la flecha del esqueleto (velocidad
// 1,5 al disparar, gravedad 0,03 por tick, freno 0,99 en el aire y 0,8 en el agua), se clavan en los bloques (la flecha) o se rompen (bola de nieve y huevo) y le
// pegan a cualquier mob (SheepManager::pick / attack): la flecha hace 4 de dano, la bola de nieve y el huevo 0 (solo empujan). El huevo da 1 de 8 de soltar una
// gallina (1 de 32 de que sean 4). Una flecha del jugador clavada se vuelve a recoger al pisarla.
// Las coordenadas son las de los mobs (el bloque j cubre y de j-0,5 a j+0,5; x e z enteros); (px, py, pz) del tick son los ojos del jugador.
namespace Projectiles {
    enum Kind { ARROW = 0, SNOWBALL = 1, EGG = 2 };
    const int MAX = 24;

    void clear();

    // Dispara desde (x, y, z) hacia (dx, dy, dz) (no hace falta que este normalizado). speed y spread como EntityArrow.setArrowHeading. ownerX / ownerZ: de donde viene
    // el golpe (el empuje). pickable: la flecha se puede volver a recoger (las del jugador y las del dispensador).
    bool spawn(int kind, double x, double y, double z, double dx, double dy, double dz, float speed, float spread, bool pickable, double ownerX, double ownerZ);

    // EntityArrow(world, player) / EntitySnowball / EntityEgg: sale de los ojos, un poco a la derecha y 0,1 mas abajo, hacia donde mira.
    // yaw y pitch: los del jugador (Player::yaw / pitch); (eyeX, eyeY, eyeZ): sus ojos.
    bool shoot(int kind, double eyeX, double eyeY, double eyeZ, float yaw, float pitch);

    // El jugador como blanco: EntityArrow.onUpdate lo cuenta entre las entidades que puede golpear (una flecha tuya solo despues de 5 ticks en el aire; la del dispensador, siempre).
    // box: la caja del jugador {minX, minY, minZ, maxX, maxY, maxZ}; la Beta la agranda 0,3. hurt aplica el golpe y dice si hizo dano (false = sigue invulnerable y la
    // flecha rebota). hasSource = false: golpe sin atacante (el dispensador), sin empujon.
    typedef bool (*PlayerHurtFn)(void* ctx, int amount, float srcX, float srcZ, bool hasSource);
    struct PlayerTarget { bool valid; float box[6]; PlayerHurtFn hurt; void* ctx; };

    // 20 Hz. Devuelve cuantas flechas recogio el jugador en este tick (main las suma al inventario). target (opcional): el jugador, para que lo golpeen.
    int tick(const ChunkManager& cm, float px, float py, float pz, bool canPickUp, const PlayerTarget* target = nullptr);

    // Se llama con la matriz de la camara ya puesta. camYaw / camPitch: los del jugador (la bola de nieve y el huevo son sprites que lo miran).
    void draw(const ChunkManager& cm, float partialTick, float camYaw, float camPitch);

    // RenderArrow: el modelo 3D de la flecha (arrows.png). (x, y, z): el centro; yaw / pitch en grados como los de la flecha; shake: los ticks de temblor que quedan
    // (ya con el partialTick restado); level: luz 0 a 15. Lo usan tambien las flechas de los esqueletos (HostileModel).
    void drawArrowModel(float x, float y, float z, float yaw, float pitch, float shake, unsigned char level);
}

#endif
