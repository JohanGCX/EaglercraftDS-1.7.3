#ifndef CLIMATE_H
#define CLIMATE_H

#include <math.h>
#include <stdint.h>
#include "Lakes.h"   // JRandom (java.util.Random)
#include "Biome.h"

// =====================================================================
// Clima de la Beta 1.7.3 (WorldChunkManager): temperatura y humedad por ruido simplex, y de ahí el bioma (BiomeGenBase.getBiome).
// NoiseGeneratorOctaves2 viene de la descompilación; NoiseGenerator2 (el simplex 2D) no estaba entre los archivos y está escrito
// con el simplex 2D estándar de Gustavson, que es el que usa la Beta.
// =====================================================================
namespace Climate {

struct Simplex {
    uint8_t perm[512];
    double xo, yo, zo;

    void init(Lakes::JRandom& r) {
        xo = r.nextDouble() * 256.0;
        yo = r.nextDouble() * 256.0;
        zo = r.nextDouble() * 256.0;
        for (int i = 0; i < 256; i++) perm[i] = (uint8_t)i;
        for (int i = 0; i < 256; i++) {
            int j = r.nextInt(256 - i) + i;
            uint8_t tmp = perm[i];
            perm[i] = perm[j];
            perm[j] = tmp;
            perm[i + 256] = perm[i];
        }
    }

    static int wrap(float d) { return d > 0.0f ? (int)d : (int)d - 1; }

    // NoiseGenerator2.func_4157_a para un punto: (x, z) ya con la escala; devuelve 70 * (n0 + n1 + n2)
    float eval(float x, float z) const {
        static const int G[12][2] = { {1,1},{-1,1},{1,-1},{-1,-1},{1,0},{-1,0},{1,0},{-1,0},{0,1},{0,-1},{0,1},{0,-1} };
        const float F2 = 0.3660254f, G2 = 0.21132487f;
        float s = (x + z) * F2;
        int i = wrap(x + s), j = wrap(z + s);
        float t = (float)(i + j) * G2;
        float x0 = x - ((float)i - t), y0 = z - ((float)j - t);
        int i1 = 0, j1 = 1;
        if (x0 > y0) { i1 = 1; j1 = 0; }
        float x1 = x0 - (float)i1 + G2, y1 = y0 - (float)j1 + G2;
        float x2 = x0 - 1.0f + 2.0f * G2, y2 = y0 - 1.0f + 2.0f * G2;
        int ii = i & 255, jj = j & 255;
        int g0 = perm[ii + perm[jj]] % 12;
        int g1 = perm[ii + i1 + perm[jj + j1]] % 12;
        int g2 = perm[ii + 1 + perm[jj + 1]] % 12;
        float n0 = 0.0f, n1 = 0.0f, n2 = 0.0f;
        float t0 = 0.5f - x0 * x0 - y0 * y0;
        if (t0 >= 0.0f) { t0 *= t0; n0 = t0 * t0 * ((float)G[g0][0] * x0 + (float)G[g0][1] * y0); }
        float t1 = 0.5f - x1 * x1 - y1 * y1;
        if (t1 >= 0.0f) { t1 *= t1; n1 = t1 * t1 * ((float)G[g1][0] * x1 + (float)G[g1][1] * y1); }
        float t2 = 0.5f - x2 * x2 - y2 * y2;
        if (t2 >= 0.0f) { t2 *= t2; n2 = t2 * t2 * ((float)G[g2][0] * x2 + (float)G[g2][1] * y2); }
        return 70.0f * (n0 + n1 + n2);
    }
};

// NoiseGeneratorOctaves2.func_4111_a para un punto: la frecuencia se multiplica por 'lac' en cada octava y la amplitud, por 1/pers
template <int N>
struct Octaves {
    Simplex gen[N];
    void init(Lakes::JRandom& r) { for (int k = 0; k < N; k++) gen[k].init(r); }

    float sample(float x, float z, float scale, float lac) const {
        const float sx = scale / 1.5f;
        float total = 0.0f, freq = 1.0f, amp = 1.0f;
        for (int k = 0; k < N; k++) {
            float px = x * sx * freq + (float)gen[k].xo;
            float pz = z * sx * freq + (float)gen[k].yo;
            total += gen[k].eval(px, pz) * (0.55f / amp);
            freq *= lac;
            amp *= 0.5f;
        }
        return total;
    }
};

struct State {
    Octaves<4> temperature;
    Octaves<4> humidity;
    Octaves<2> detail;
    uint32_t seed;
    bool ready;
};

inline State& state() { static State s = { {}, {}, {}, 0u, false }; return s; }

// WorldChunkManager(World): tres generadores con la semilla multiplicada por 9871, 39811 y 543321
inline void initIfSeedChanged(uint32_t seed) {
    State& s = state();
    if (s.ready && s.seed == seed) return;
    Lakes::JRandom rt((long long)seed * 9871LL), rh((long long)seed * 39811LL), rd((long long)seed * 543321LL);
    s.temperature.init(rt);
    s.humidity.init(rh);
    s.detail.init(rd);
    s.seed = seed;
    s.ready = true;
}

// WorldChunkManager.loadBlockGeneratorData, para una columna: temperatura y humedad en 0..1
inline void raw(int gx, int gz, float& outT, float& outH) {
    const State& s = state();
    float temp = s.temperature.sample((float)gx, (float)gz, 0.025f, 0.25f);
    float hum  = s.humidity.sample((float)gx, (float)gz, 0.05f, 1.0f / 3.0f);
    float det  = s.detail.sample((float)gx, (float)gz, 0.25f, 0.5882352941176471f);
    float d = det * 1.1f + 0.5f;
    float t = (temp * 0.15f + 0.7f) * 0.99f + d * 0.01f;
    float h = (hum * 0.15f + 0.5f) * 0.998f + d * 0.002f;
    t = 1.0f - (1.0f - t) * (1.0f - t);
    if (t < 0.0f) t = 0.0f;
    if (h < 0.0f) h = 0.0f;
    if (t > 1.0f) t = 1.0f;
    if (h > 1.0f) h = 1.0f;
    outT = t;
    outH = h;
}

// BiomeGenBase.getBiomeFromLookup (tabla de 64 x 64) + getBiome
inline BiomeType biomeFor(float t, float h) {
    int ti = (int)(t * 63.0f), hi = (int)(h * 63.0f);
    float a = (float)ti / 63.0f;
    float b = (float)hi / 63.0f;
    b *= a;
    if (a < 0.1f) return BiomeType::TUNDRA;
    if (b < 0.2f) return a < 0.5f ? BiomeType::TUNDRA : (a < 0.95f ? BiomeType::SAVANNA : BiomeType::DESERT);
    if (b > 0.5f && a < 0.7f) return BiomeType::SWAMPLAND;
    if (a < 0.5f) return BiomeType::TAIGA;
    if (a < 0.97f) return b < 0.35f ? BiomeType::SHRUBLAND : BiomeType::FOREST;
    return b < 0.45f ? BiomeType::PLAINS : (b < 0.9f ? BiomeType::SEASONAL_FOREST : BiomeType::RAINFOREST);
}

// El clima cambia muy despacio (la octava más rápida tiene unos 60 bloques de largo): en vez de evaluarlo en cada columna se mide en
// una malla de 4 x 4 puntos separados por 4 bloques, que cubre el chunk y un borde de 1 columna, y se interpola.
struct Lattice {
    int x0, z0;           // esquina (mundo) del primer punto de la malla
    float t[4][4], h[4][4];

    void fill(int chunkOriginX, int chunkOriginZ) {
        x0 = chunkOriginX - 4;
        z0 = chunkOriginZ - 4;
        for (int i = 0; i < 4; i++)
            for (int j = 0; j < 4; j++)
                raw(x0 + i * 4, z0 + j * 4, t[i][j], h[i][j]);
    }

    // La misma malla, de a una fila (4 puntos, unos 2,5 ms) por llamada: así la primera columna de cada chunk no carga con los 10 ms de las 16 mediciones
    void fillRow(int chunkOriginX, int chunkOriginZ, int i) {
        x0 = chunkOriginX - 4;
        z0 = chunkOriginZ - 4;
        for (int j = 0; j < 4; j++) raw(x0 + i * 4, z0 + j * 4, t[i][j], h[i][j]);
    }

    // válido para columnas de [chunkOriginX - 1, chunkOriginX + 8]
    void at(int gx, int gz, float& outT, float& outH) const {
        float u = (float)(gx - x0) * 0.25f, v = (float)(gz - z0) * 0.25f;
        int i = (int)u, j = (int)v;
        if (i < 0) i = 0;
        if (i > 2) i = 2;
        if (j < 0) j = 0;
        if (j > 2) j = 2;
        float fu = u - (float)i, fv = v - (float)j;
        float t0 = t[i][j] + (t[i + 1][j] - t[i][j]) * fu;
        float t1 = t[i][j + 1] + (t[i + 1][j + 1] - t[i][j + 1]) * fu;
        float h0 = h[i][j] + (h[i + 1][j] - h[i][j]) * fu;
        float h1 = h[i][j + 1] + (h[i + 1][j + 1] - h[i][j + 1]) * fu;
        outT = t0 + (t1 - t0) * fv;
        outH = h0 + (h1 - h0) * fv;
    }
};

}  // namespace Climate

#endif