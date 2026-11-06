#ifndef PERLINNOISE_H
#define PERLINNOISE_H

class PerlinNoise {
public:
    static void init(unsigned int seed);
    static float generateNoise3D(float x, float y, float z);
    static float octaveNoise2D(float x, float z);
    // Terrain generator 1: classic fBm. Same 6 octaves (so the same cost as octaveNoise2D), but each octave has HALF the amplitude of the previous
    // one while its frequency doubles, and the sum is divided by the total amplitude, so the result stays around -1..1 (rarely beyond +-0.9)
    // instead of reaching +-35 like octaveNoise2D. Used by terrain generator 1 (WorldConfig::terrainGen()).
    static float fbmNoise2D(float x, float z);

private:
    static int permutations[512];
    static bool initialized;

    static float fade(float t);
    static float lerp(float t, float a, float b);
    static float grad(int hash, float x, float y, float z);

    static const int OCTAVE_COUNT = 6;
};

#endif