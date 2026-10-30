#ifndef NETHERGEN_H
#define NETHERGEN_H

#include "BlockType.h"
#include "PerlinNoise.h"
#include "Noise.h"
#include "Lakes.h"   // JRandom (java.util.Random)

// =====================================================================
// Generador del Nether REDUCIDO. El original (ChunkProviderHell) saca el terreno de 40 octavas de ruido 3D por cada punto de una
// rejilla; aquí se usa el Perlin 2D del overworld (PerlinNoise) con una altura de suelo y otra de techo por columna, más pilares:
// misma forma general (techo de bedrock, masas colgando, suelo con islas, mar de lava en y=32, arena de almas y grava) sin el costo.
// Todo es función pura de la columna, así cada chunk puede mirar a sus vecinos (piedra luminosa y fuego) sin generarlos.
// =====================================================================
namespace NetherGen {

const int SIZE_Y = 128;
const int LAVA_LEVEL = 32;     // ChunkProviderHell: la lava llena el aire por debajo de y=32

// Perillas de forma
const float FLOOR_SCALE = 0.20f, FLOOR_NORM = 22.0f;   // el ruido de suelo
const int   FLOOR_MID = 40, FLOOR_AMP = 34;            // altura del suelo: FLOOR_MID +- FLOOR_AMP
const float CEIL_SCALE = 0.17f, CEIL_NORM = 24.0f;     // el ruido del techo
const int   CEIL_TOP = 118, CEIL_DROP = 66;            // el techo baja hasta CEIL_DROP bloques desde CEIL_TOP
const float PILLAR_SCALE = 0.30f, PILLAR_NORM = 22.0f; // ruido de los pilares
const float PILLAR_LIMIT = 0.62f;                      // sobre este valor la columna es un pilar macizo

struct Col { s16 floorTop; s16 ceilBottom; u8 pillar; u8 surface; };   // surface: 0 nada, 1 arena de almas, 2 grava

inline float fieldNoise(int gx, int gz, float scale, float ox, float oz, float norm) {
    float v = PerlinNoise::octaveNoise2D((float)gx * scale + ox, (float)gz * scale + oz) / norm;
    return v > 1.0f ? 1.0f : (v < -1.0f ? -1.0f : v);
}

// Los campos de ruido (suelo, techo, pilares y los dos de los parches) cambian despacio (la octava más rápida mide unos 5 bloques y casi no
// aporta amplitud), así que se miden una vez cada 4 bloques y se interpolan. Esto es lo que hace barato cada paso de la generación: antes,
// cada columna nueva costaba 3 a 5 ruidos de 6 octavas y un grupo de piedra luminosa toca más de 200 columnas.
struct Lat { float n1, n2, n3, a, b; };

struct LatCache {
    int tx[256], tz[256];
    Lat v[256];
    bool ready;
};
inline LatCache& latCache() { static LatCache c = { {0}, {0}, {}, false }; return c; }
inline void resetLattice() { latCache().ready = false; }   // al cambiar de semilla

inline const Lat& latticePoint(int ix, int iz) {
    LatCache& c = latCache();
    if (!c.ready) { for (int i = 0; i < 256; i++) { c.tx[i] = 0x7FFFFFFF; c.tz[i] = 0; } c.ready = true; }
    const int i = ((ix & 15) << 4) | (iz & 15);
    if (c.tx[i] != ix || c.tz[i] != iz) {
        const int gx = ix * 4, gz = iz * 4;
        Lat& l = c.v[i];
        l.n1 = fieldNoise(gx, gz, FLOOR_SCALE, 311.7f, -127.3f, FLOOR_NORM);
        l.n2 = fieldNoise(gx, gz, CEIL_SCALE, -541.9f, 233.1f, CEIL_NORM);
        l.n3 = fieldNoise(gx, gz, PILLAR_SCALE, 87.3f, 419.6f, PILLAR_NORM);
        l.a = fieldNoise(gx, gz, 0.07f, 702.2f, -88.8f, 16.0f);
        l.b = fieldNoise(gx, gz, 0.07f, -310.4f, 150.5f, 16.0f);
        c.tx[i] = ix;
        c.tz[i] = iz;
    }
    return c.v[i];
}

inline Col computeColumn(int gx, int gz) {
    Col c;
    const int ix = gx >> 2, iz = gz >> 2;                         // (desplazar con signo redondea hacia abajo, también en negativos)
    const float fx = (float)(gx - ix * 4) * 0.25f, fz = (float)(gz - iz * 4) * 0.25f;
    const Lat& p00 = latticePoint(ix, iz);
    const Lat& p10 = latticePoint(ix + 1, iz);
    const Lat& p01 = latticePoint(ix, iz + 1);
    const Lat& p11 = latticePoint(ix + 1, iz + 1);
    auto mix = [&](float a00, float a10, float a01, float a11) {
        float t0 = a00 + (a10 - a00) * fx, t1 = a01 + (a11 - a01) * fx;
        return t0 + (t1 - t0) * fz;
    };

    float n1 = mix(p00.n1, p10.n1, p01.n1, p11.n1);
    int floorTop = FLOOR_MID + (int)(n1 * (float)FLOOR_AMP);
    if (floorTop < 8) floorTop = 8;
    if (floorTop > 100) floorTop = 100;
    c.floorTop = (s16)floorTop;

    float n2 = mix(p00.n2, p10.n2, p01.n2, p11.n2);
    float mass = (n2 + 1.0f) * 0.5f;
    int ceilBottom = CEIL_TOP - (int)(mass * mass * (float)CEIL_DROP);
    if (ceilBottom < floorTop + 8) ceilBottom = floorTop + 8;
    if (ceilBottom > 118) ceilBottom = 118;
    c.ceilBottom = (s16)ceilBottom;

    float n3 = mix(p00.n3, p10.n3, p01.n3, p11.n3);
    c.pillar = n3 > PILLAR_LIMIT ? 1 : 0;

    // parches de arena de almas y grava en los suelos de y = 60 a 65 (ChunkProviderHell.func_4058_b: var4 - 4 .. var4 + 1)
    c.surface = 0;
    if (floorTop >= 60 && floorTop <= 65) {
        float a = mix(p00.a, p10.a, p01.a, p11.a);
        float b = mix(p00.b, p10.b, p01.b, p11.b);
        if (a > 0.0f) c.surface = 1;
        else if (b > 0.0f) c.surface = 2;
    }
    return c;
}

// Bloque del terreno base (sin cuevas, piedra luminosa, fuego ni manantiales) en una celda
inline BlockType baseType(const Col& c, int gx, int gy, int gz) {
    if (gy < 0 || gy >= SIZE_Y) return BlockType::AIR;

    // bedrock: abajo y arriba, con grosor al azar de 1 a 5 (ChunkProviderHell: gy <= nextInt(5) y gy >= 127 - nextInt(5))
    if (gy <= 4 || gy >= 123) {
        unsigned int h = Noise::hashInt(gx + gy * 7919, gz, 901);
        if (gy <= 4 && (int)(h % 5u) >= gy) return BlockType::BEDROCK;
        if (gy >= 123 && (int)((h >> 8) % 5u) >= 127 - gy) return BlockType::BEDROCK;
    }

    bool solid = gy <= c.floorTop || gy >= c.ceilBottom || (c.pillar && gy > c.floorTop && gy < c.ceilBottom);
    if (!solid) return gy < LAVA_LEVEL ? BlockType::LAVA : BlockType::AIR;

    if (c.surface && gy <= c.floorTop && gy > c.floorTop - 3) return c.surface == 1 ? BlockType::SOUL_SAND : BlockType::GRAVEL;
    return BlockType::NETHERRACK;
}

// ---------------------------------------------------------------------------------------------------------------------------
// Piedra luminosa: WorldGenGlowStone1 / 2 (el mismo código). Entre 1500 intentos agrega celdas de aire junto a exactamente
// una de piedra luminosa, a partir de una celda de aire colgada del techo de netherrack. Se calcula sobre un volumen local de
// 15 x 12 x 15 (x +- 7, de y - 11 a y, z +- 7); lo que sale del volumen no puede tener piedra luminosa.
// T debe ofrecer: bool isAir(x, y, z)  y  BlockType type(x, y, z)
// ---------------------------------------------------------------------------------------------------------------------------
struct Glow {
    int ox, oy, oz;
    bool cell[15][12][15];
    bool has(int x, int y, int z) const {
        int i = x - (ox - 7), j = y - (oy - 11), k = z - (oz - 7);
        if (i < 0 || i > 14 || j < 0 || j > 11 || k < 0 || k > 14) return false;
        return cell[i][j][k];
    }
    void set(int x, int y, int z) { cell[x - (ox - 7)][y - (oy - 11)][z - (oz - 7)] = true; }
};

template <class T>
bool glowCluster(T& t, Lakes::JRandom& rng, int ox, int oy, int oz, Glow& g) {
    g.ox = ox; g.oy = oy; g.oz = oz;
    for (int i = 0; i < 15; i++) for (int j = 0; j < 12; j++) for (int k = 0; k < 15; k++) g.cell[i][j][k] = false;

    if (!t.isAir(ox, oy, oz)) return false;
    if (t.type(ox, oy + 1, oz) != BlockType::NETHERRACK) return false;
    g.set(ox, oy, oz);

    // Todos los intentos caen dentro del volumen de 15 x 12 x 15: el terreno se consulta una vez por celda y se recuerda
    signed char airCache[15][12][15];
    for (int i = 0; i < 15; i++) for (int j = 0; j < 12; j++) for (int k = 0; k < 15; k++) airCache[i][j][k] = -1;

    for (int n = 0; n < 1500; n++) {
        int a = rng.nextInt(8), b = rng.nextInt(8);
        int x = ox + a - b;
        int y = oy - rng.nextInt(12);
        int c = rng.nextInt(8), d = rng.nextInt(8);
        int z = oz + c - d;
        signed char& known = airCache[x - (ox - 7)][y - (oy - 11)][z - (oz - 7)];
        if (known < 0) known = t.isAir(x, y, z) ? 1 : 0;
        if (known == 1 && !g.has(x, y, z)) {
            int adj = (g.has(x - 1, y, z) ? 1 : 0) + (g.has(x + 1, y, z) ? 1 : 0) + (g.has(x, y - 1, z) ? 1 : 0) +
                      (g.has(x, y + 1, z) ? 1 : 0) + (g.has(x, y, z - 1) ? 1 : 0) + (g.has(x, y, z + 1) ? 1 : 0);
            if (adj == 1) g.set(x, y, z);
        }
    }
    return true;
}

// WorldGenFire: 64 intentos al azar alrededor del origen (x +- 7, y +- 3, z +- 7); fuego donde hay aire sobre netherrack.
// 'place(x, y, z)' recibe cada celda donde va fuego.
template <class T, class Place>
void firePatch(T& t, Lakes::JRandom& rng, int ox, int oy, int oz, Place place) {
    for (int n = 0; n < 64; n++) {
        int a = rng.nextInt(8), b = rng.nextInt(8);
        int x = ox + a - b;
        int c = rng.nextInt(4), d = rng.nextInt(4);
        int y = oy + c - d;
        int e = rng.nextInt(8), f = rng.nextInt(8);
        int z = oz + e - f;
        if (t.isAir(x, y, z) && t.type(x, y - 1, z) == BlockType::NETHERRACK) place(x, y, z);
    }
}

}  // namespace NetherGen

#endif
