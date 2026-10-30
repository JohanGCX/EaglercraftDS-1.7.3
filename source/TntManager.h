#ifndef TNTMANAGER_H
#define TNTMANAGER_H

class ChunkManager;

// TNT encendido (EntityTNTPrimed + RenderTNTPrimed, Beta 1.7.3): cae como un bloque, parpadea en blanco y a los 80 ticks explota (fuerza 4).
// Se enciende con redstone, fuego, el mechero o la explosion de otro TNT (mecha corta). Las celdas son globales (gx, gy, gz).
namespace TntManager {
    const int MAX_TNT = 24;

    void clear();
    // BlockTNT.onBlockDestroyedByPlayer con metadata 1: el bloque ya se quito; sale el TNT encendido en su centro (mecha 80)
    void primeCell(int gx, int gy, int gz);
    // BlockTNT.onBlockDestroyedByExplosion: mecha corta al azar (10 a 29 ticks) y sin el salto inicial al azar
    void primeFromExplosion(int gx, int gy, int gz);

    void tick(const ChunkManager& cm);                                              // un tick del juego (20 Hz)
    void draw(const ChunkManager& cm, float playerX, float playerY, float playerZ, float partialTick);
}

#endif
