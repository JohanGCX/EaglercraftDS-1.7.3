#ifndef LAKES_H
#define LAKES_H

#include <string.h>
#include <stdint.h>

// =====================================================================
// Lagos de Minecraft Beta 1.7.3: WorldGenLakes (forma, validez, hierba en las orillas y cáscara de piedra de la lava).
//
// La lógica es pura: trabaja contra un terreno T y un escritor W, y se prueba en el host contra el Java real. En el
// juego, T es el modelo de terreno por altura (cada chunk evalúa los lagos que empiezan en sus vecinos y escribe solo
// lo que cae dentro de él, igual que las menas, así que no hay costuras) y W escribe en los bloques del chunk.
//
// Un lago ocupa una región de 16 x 16 x 8 celdas que arranca en (x0, y0, z0). Índice de celda: (x * 16 + z) * 8 + y.
// =====================================================================

namespace Lakes {

// java.util.Random, para que la forma sea la misma que en el original con la misma semilla
struct JRandom {
    uint64_t seed;
    explicit JRandom(long long s) { setSeed(s); }
    void setSeed(long long s) { seed = ((uint64_t)s ^ 0x5DEECE66DULL) & ((1ULL << 48) - 1); }
    int next(int bits) {
        seed = (seed * 0x5DEECE66DULL + 0xBULL) & ((1ULL << 48) - 1);
        return (int)(int32_t)(uint32_t)(seed >> (48 - bits));
    }
    int nextInt(int n) {
        if ((n & -n) == n) return (int)(((int64_t)n * (int64_t)next(31)) >> 31);
        int bits, val;
        do {
            bits = next(31);
            val = bits % n;
        } while ((int32_t)((uint32_t)bits - (uint32_t)val + (uint32_t)(n - 1)) < 0);
        return val;
    }
    // Avanza como si se hubieran hecho n llamadas a next(), sin hacerlas: composición de n pasos de la congruencia lineal
    void skip(unsigned int n) {
        const uint64_t MASK = (1ULL << 48) - 1;
        uint64_t a = 0x5DEECE66DULL, c = 0xBULL, accA = 1, accC = 0;
        while (n) {
            if (n & 1) { accA = (accA * a) & MASK; accC = (accC * a + c) & MASK; }
            c = ((a + 1) * c) & MASK;
            a = (a * a) & MASK;
            n >>= 1;
        }
        seed = (seed * accA + accC) & MASK;
    }
    double nextDouble() {
        return (double)(((int64_t)next(26) << 27) + next(27)) * (1.0 / (double)(1ULL << 53));
    }
};

struct Result {
    bool valid;
    int x0, y0, z0;
    bool lake[2048];   // celdas del lago: abajo (y < 4) líquido, arriba (y >= 4) aire
    bool stone[2048];  // solo lava: celdas de la orilla que pasan a piedra
};

inline int cellIndex(int x, int z, int y) { return (x * 16 + z) * 8 + y; }

// ¿la celda (x,z,y) no es del lago pero toca una que sí? (las 6 caras, dentro de la región)
inline bool isShell(const bool* g, int x, int z, int y) {
    if (g[cellIndex(x, z, y)]) return false;
    return (x < 15 && g[cellIndex(x + 1, z, y)]) || (x > 0 && g[cellIndex(x - 1, z, y)]) ||
           (z < 15 && g[cellIndex(x, z + 1, y)]) || (z > 0 && g[cellIndex(x, z - 1, y)]) ||
           (y < 7 && g[cellIndex(x, z, y + 1)]) || (y > 0 && g[cellIndex(x, z, y - 1)]);
}

// WorldGenLakes.generate sin escribir: calcula la forma y decide si el lago es válido. 'ox, oy, oz' es el origen que se
// le pasa al generador del original (la región arranca 8 celdas antes en x y en z).
//
// T debe ofrecer, con coordenadas globales:
//   bool isAir(x, y, z)          el bloque es aire
//   bool isSolid(x, y, z)        el material es sólido
//   bool isLiquid(x, y, z)       el material es líquido
//   bool isSameLiquid(x, y, z)   el bloque es justo el líquido de este lago (quieto)
template <class T>
bool build(T& t, JRandom& rng, bool lava, int ox, int oy, int oz, Result& out) {
    int x0 = ox - 8, z0 = oz - 8, y = oy;
    for (; y > 0 && t.isAir(x0, y, z0); --y) {
    }
    int y0 = y - 4;
    out.valid = false;
    out.x0 = x0; out.y0 = y0; out.z0 = z0;
    memset(out.lake, 0, sizeof(out.lake));
    memset(out.stone, 0, sizeof(out.stone));
    bool* g = out.lake;

    int blobs = rng.nextInt(4) + 4;
    for (int i = 0; i < blobs; i++) {
        double sx = rng.nextDouble() * 6.0 + 3.0;
        double sy = rng.nextDouble() * 4.0 + 2.0;
        double sz = rng.nextDouble() * 6.0 + 3.0;
        double cx = rng.nextDouble() * (16.0 - sx - 2.0) + 1.0 + sx / 2.0;
        double cy = rng.nextDouble() * (8.0 - sy - 4.0) + 2.0 + sy / 2.0;
        double cz = rng.nextDouble() * (16.0 - sz - 2.0) + 1.0 + sz / 2.0;

        // el término de cada eje se calcula una sola vez; la suma solo puede dar menos de 1 si cada término es menor que 1
        double dx2[16], dy2[8], dz2[16];
        int xa = 15, xb = 0, za = 15, zb = 0, ya = 7, yb = 0;
        for (int xx = 1; xx < 15; xx++) {
            double d = ((double)xx - cx) / (sx / 2.0);
            dx2[xx] = d * d;
            if (dx2[xx] < 1.0) { if (xx < xa) xa = xx; if (xx > xb) xb = xx; }
        }
        for (int zz = 1; zz < 15; zz++) {
            double d = ((double)zz - cz) / (sz / 2.0);
            dz2[zz] = d * d;
            if (dz2[zz] < 1.0) { if (zz < za) za = zz; if (zz > zb) zb = zz; }
        }
        for (int yy = 1; yy < 7; yy++) {
            double d = ((double)yy - cy) / (sy / 2.0);
            dy2[yy] = d * d;
            if (dy2[yy] < 1.0) { if (yy < ya) ya = yy; if (yy > yb) yb = yy; }
        }
        for (int xx = xa; xx <= xb; xx++)
            for (int zz = za; zz <= zb; zz++)
                for (int yy = ya; yy <= yb; yy++)
                    if (dx2[xx] + dy2[yy] + dz2[zz] < 1.0) g[cellIndex(xx, zz, yy)] = true;
    }

    // validez: el borde de arriba no puede tocar líquido; el de abajo tiene que ser sólido o el mismo líquido
    for (int x = 0; x < 16; x++)
        for (int z = 0; z < 16; z++)
            for (int yy = 0; yy < 8; yy++) {
                if (!isShell(g, x, z, yy)) continue;
                int gx = x0 + x, gy = y0 + yy, gz = z0 + z;
                if (yy >= 4 && t.isLiquid(gx, gy, gz)) return false;
                if (yy < 4 && !t.isSolid(gx, gy, gz) && !t.isSameLiquid(gx, gy, gz)) return false;
            }

    // lava: la orilla se vuelve piedra (siempre abajo, la mitad de las veces arriba). El azar se gasta en el mismo orden.
    if (lava) {
        for (int x = 0; x < 16; x++)
            for (int z = 0; z < 16; z++)
                for (int yy = 0; yy < 8; yy++) {
                    if (!isShell(g, x, z, yy)) continue;
                    bool take = (yy < 4) || rng.nextInt(2) != 0;
                    if (take && t.isSolid(x0 + x, y0 + yy, z0 + z)) out.stone[cellIndex(x, z, yy)] = true;
                }
    }

    out.valid = true;
    return true;
}

// Lo que se escribe. W debe ofrecer, con coordenadas globales (puede ignorar las que no le corresponden):
//   int  top(x, z)                  altura del bloque sólido más alto de la columna del terreno (-1 si no hay)
//   bool isDirt(x, y, z)            el bloque actual es tierra
//   void put(x, y, z, kind)         kind: 0 aire, 1 líquido, 2 hierba, 3 piedra
template <class W>
void apply(W& w, const Result& r, bool lava) {
    for (int x = 0; x < 16; x++)
        for (int z = 0; z < 16; z++)
            for (int yy = 0; yy < 8; yy++)
                if (r.lake[cellIndex(x, z, yy)]) w.put(r.x0 + x, r.y0 + yy, r.z0 + z, yy >= 4 ? 0 : 1);

    // hierba en la tierra que quedó a la vista del cielo (en el original: luz del cielo > 0 en esa celda)
    for (int x = 0; x < 16; x++)
        for (int z = 0; z < 16; z++) {
            int top = w.top(r.x0 + x, r.z0 + z);
            for (int yy = 4; yy < 8; yy++) {
                if (!r.lake[cellIndex(x, z, yy)]) continue;
                int gy = r.y0 + yy;
                if (!w.isDirt(r.x0 + x, gy - 1, r.z0 + z)) continue;
                // abierta al cielo: el terreno no sobrepasa el techo del lago y toda celda por encima es del lago o aire
                bool open = top <= r.y0 + 7;
                for (int k = yy + 1; open && k < 8; k++) {
                    if (gy + (k - yy) <= top && !r.lake[cellIndex(x, z, k)]) open = false;
                }
                if (open) w.put(r.x0 + x, gy - 1, r.z0 + z, 2);
            }
        }

    if (lava) {
        for (int x = 0; x < 16; x++)
            for (int z = 0; z < 16; z++)
                for (int yy = 0; yy < 8; yy++)
                    if (r.stone[cellIndex(x, z, yy)]) w.put(r.x0 + x, r.y0 + yy, r.z0 + z, 3);
    }
}

}  // namespace Lakes

#endif
