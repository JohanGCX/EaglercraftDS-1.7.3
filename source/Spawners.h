#ifndef SPAWNERS_H
#define SPAWNERS_H

class ChunkManager;

// Generadores de monstruos (TileEntityMobSpawner, Beta 1.7.3). El mob es la metadata del bloque (0 zombi, 1 esqueleto, 2 arana; lo elige la mazmorra al generarse).
// Con el jugador a menos de 16 bloques: cada intento espera "delay" ticks (200 + 0 a 599 despues de crear mobs, 20 la primera vez) y entonces intenta 4 veces hacer
// aparecer un mob a +-4 bloques del generador, salvo que ya haya 6 de su clase cerca. Sin particulas ni el mob girando dentro de la jaula.
namespace Spawners {
    void clear();
    void tick(const ChunkManager& cm, float px, float py, float pz);
    void draw(const ChunkManager& cm, float partialTick);   // TileEntityMobSpawnerRenderer: el mob gira dentro de la jaula (a 7/16 de su tamaño, inclinado 30 grados)   // 20 Hz. (px, py, pz): los ojos del jugador
}

#endif
