#ifndef NOISE_H
#define NOISE_H

class Noise {
public:
    static void setSeed(unsigned int seed);
    static float valueNoise2D(float x, float z);
    static float biomeNoise2D(float x, float z);

    // Entero pseudoaleatorio determinista basado en semilla+coordenadas+sal
    // (el "salt" permite obtener valores DISTINTOS en el mismo punto
    // para propósitos distintos, ej. "tipo de árbol" vs "altura del tronco")
    static unsigned int hashInt(int x, int z, int salt);
};

#endif