#ifndef PARTICLES_H
#define PARTICLES_H

#include "BlockType.h"

class ChunkManager;

// Particulas (EffectRenderer, EntityFX y las de RenderGlobal.spawnParticle de la Beta 1.7.3). Un pozo chico de MAX_PARTICLES (cada una es un solo quad que mira a la camara):
// las de bloque (romper y golpear) usan 4 x 4 pixeles al azar de la celda de terrain.png; las demas (humo, llama, salpicadura, nieve, lava, explosion, polvo de redstone)
// son de color plano, sin texturas, para no gastar VRAM. Coordenadas del mundo (la celda j cubre de j - 0,5 a j + 0,5).
namespace Particles {
    const int MAX_PARTICLES = 64;

    enum Kind { SMOKE, FLAME, SPLASH, SNOW, LAVA, BUBBLE, EXPLODE, REDDUST, DIG, HEART };

    void clear();
    void tick(const ChunkManager& cm, float px, float py, float pz);   // 20 Hz: fisica y las particulas ambientales (antorchas, fuego, lava, hornos) alrededor del jugador
    void draw(const ChunkManager& cm, float ex, float ey, float ez, float yawDeg, float pitchDeg);   // con la matriz de la camara puesta

    // vx, vy, vz: la velocidad que se le suma (como en spawnParticle); scale: 1 normal
    void spawn(Kind kind, float x, float y, float z, float vx, float vy, float vz, float scale = 1.0f);
    void blockBreak(BlockType type, float cx, float cy, float cz);                          // addBlockDestroyEffects (con menos particulas que las 64 de la Beta)
    void blockHit(BlockType type, float cx, float cy, float cz, int dx, int dy, int dz);    // addBlockHitEffects: sobre la cara apuntada (dx, dy, dz = su normal)
    void explosion(float x, float y, float z, float size);                                  // humo y polvo de una explosion
    void spawnerBurst(float cx, float cy, float cz);                                        // al aparecer un mob de un generador
}

#endif
