#ifndef FISHINGHOOK_H
#define FISHINGHOOK_H

class ChunkManager;

// La cana de pescar (ItemFishingRod + EntityFish + RenderFish de la Beta 1.7.3). Hay un solo anzuelo (el del jugador). Se lanza con clic derecho; vuela con la fisica de la
// Beta (gravedad 0,04, freno 0,92), se clava en un bloque o flota en el agua; cuando pica (1 de 500 por tick) se hunde 10 a 40 ticks y un clic recoge un pescado crudo.
// Coordenadas de los mobs (el bloque j cubre y de j-0,5 a j+0,5); (eyeX, eyeY, eyeZ) son los ojos del jugador.
namespace FishingHook {
    void clear();
    bool active();

    // ItemFishingRod.onItemRightClick, sin anzuelo: lo lanza hacia donde mira el jugador (yaw / pitch como Player)
    bool cast(double eyeX, double eyeY, double eyeZ, float yaw, float pitch);

    // EntityFish.catchFish: recoge el anzuelo. damage = lo que gasta la cana (0 nada, 1 pescado, 2 clavado en tierra, 3 enganchado a un mob). Si pescó, fish = true y salen
    // el pescado en (x, y, z) con la velocidad (vx, vy, vz) hacia el jugador.
    struct Catch { int damage; bool fish; float x, y, z, vx, vy, vz; };
    Catch reel(double eyeX, double eyeY, double eyeZ);

    // 20 Hz. holdingRod: lleva la cana en la mano; alive: el jugador esta vivo. El anzuelo desaparece si no, o a mas de 32 bloques.
    void tick(const ChunkManager& cm, double eyeX, double eyeY, double eyeZ, bool holdingRod, bool alive);

    // Con la matriz de la camara puesta. swing: progreso del golpe (0 a 1); thirdPerson: la linea sale del costado del cuerpo en vez de la mano.
    void draw(const ChunkManager& cm, float partialTick, float camYaw, float camPitch, double eyeX, double eyeY, double eyeZ, float playerYaw, float playerPitch, float swing, bool thirdPerson);
}

#endif
