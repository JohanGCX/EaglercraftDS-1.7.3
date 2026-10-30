#include "Noise.h"
#include <math.h>

static unsigned int currentSeed = 0;

void Noise::setSeed(unsigned int seed) {
    currentSeed = seed;
}

static int hash2D(int x, int z, int salt) {
    int n = x * 374761393 + z * 668265263 + (int)currentSeed * 982451653 + salt * 1620364959;
    n = (n ^ (n >> 13)) * 1274126177;
    return n ^ (n >> 16);
}

unsigned int Noise::hashInt(int x, int z, int salt) {
    return (unsigned int)hash2D(x, z, salt);
}

static float randomGridValue(int x, int z) {
    int h = hash2D(x, z, 0) & 0x7fffffff;
    return (float)h / (float)0x7fffffff;
}


static float smoothstep(float t) {
    return t * t * (3.0f - 2.0f * t);
}

float Noise::valueNoise2D(float x, float z) {
    int x0 = (int)floorf(x);
    int z0 = (int)floorf(z);
    int x1 = x0 + 1;
    int z1 = z0 + 1;

    float tx = x - x0;
    float tz = z - z0;

    float v00 = randomGridValue(x0, z0);
    float v10 = randomGridValue(x1, z0);
    float v01 = randomGridValue(x0, z1);
    float v11 = randomGridValue(x1, z1);

    float sx = smoothstep(tx);
    float sz = smoothstep(tz);

    float ix0 = v00 + (v10 - v00) * sx;
    float ix1 = v01 + (v11 - v01) * sx;

    return ix0 + (ix1 - ix0) * sz;
}

float Noise::biomeNoise2D(float x, float z) {
    // Offset grande para desacoplar este ruido del de altura del
    // terreno, aunque usen la misma función/semilla internamente
    return valueNoise2D(x + 10000.0f, z + 10000.0f);
}