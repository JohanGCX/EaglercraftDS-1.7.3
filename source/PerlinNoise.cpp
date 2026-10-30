#include "PerlinNoise.h"
#include "Noise.h"
#include <math.h>

int PerlinNoise::permutations[512];
bool PerlinNoise::initialized = false;

void PerlinNoise::init(unsigned int seed) {
    int base[256];
    for (int i = 0; i < 256; i++) {
        base[i] = i;
    }

    for (int i = 0; i < 256; i++) {
        unsigned int roll = Noise::hashInt((int)seed, i, 500) % (256 - i);
        int j = i + (int)roll;
        int temp = base[i];
        base[i] = base[j];
        base[j] = temp;
    }

    for (int i = 0; i < 256; i++) {
        permutations[i] = base[i];
        permutations[i + 256] = base[i];
    }

    initialized = true;
}

float PerlinNoise::fade(float t) {
    return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
}

float PerlinNoise::lerp(float t, float a, float b) {
    return a + t * (b - a);
}

float PerlinNoise::grad(int hash, float x, float y, float z) {
    int h = hash & 15;
    float u = h < 8 ? x : y;
    float v = h < 4 ? y : (h != 12 && h != 14 ? z : x);
    return ((h & 1) == 0 ? u : -u) + ((h & 2) == 0 ? v : -v);
}

float PerlinNoise::generateNoise3D(float x, float y, float z) {
    int X = (int)floorf(x) & 255;
    int Y = (int)floorf(y) & 255;
    int Z = (int)floorf(z) & 255;

    x -= floorf(x);
    y -= floorf(y);
    z -= floorf(z);

    float u = fade(x);
    float v = fade(y);
    float w = fade(z);

    int A  = permutations[X] + Y;
    int AA = permutations[A] + Z;
    int AB = permutations[A + 1] + Z;
    int B  = permutations[X + 1] + Y;
    int BA = permutations[B] + Z;
    int BB = permutations[B + 1] + Z;

    return lerp(w,
        lerp(v,
            lerp(u, grad(permutations[AA],     x,      y,      z),
                    grad(permutations[BA],     x-1.0f, y,      z)),
            lerp(u, grad(permutations[AB],     x,      y-1.0f, z),
                    grad(permutations[BB],     x-1.0f, y-1.0f, z))),
        lerp(v,
            lerp(u, grad(permutations[AA+1],   x,      y,      z-1.0f),
                    grad(permutations[BA+1],   x-1.0f, y,      z-1.0f)),
            lerp(u, grad(permutations[AB+1],   x,      y-1.0f, z-1.0f),
                    grad(permutations[BB+1],   x-1.0f, y-1.0f, z-1.0f))));
}

float PerlinNoise::octaveNoise2D(float x, float z) {
    float total = 0.0f;
    float frequency = 1.0f;

    for (int i = 0; i < OCTAVE_COUNT; i++) {
        total += generateNoise3D(x * frequency, z * frequency, 0.0f) / frequency;
        frequency /= 2.0f;
    }

    return total;
}