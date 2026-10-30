#ifndef PERLINNOISE_H
#define PERLINNOISE_H

class PerlinNoise {
public:
    static void init(unsigned int seed);
    static float generateNoise3D(float x, float y, float z);
    static float octaveNoise2D(float x, float z);

private:
    static int permutations[512];
    static bool initialized;

    static float fade(float t);
    static float lerp(float t, float a, float b);
    static float grad(int hash, float x, float y, float z);

    static const int OCTAVE_COUNT = 6;
};

#endif