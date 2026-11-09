#include "Weather.h"
#include "ChunkManager.h"
#include "Chunk.h"
#include "BlockType.h"
#include "WorldSave.h"
#include "Particles.h"
#include "Sound.h"
#include <nds.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

namespace {
    bool s_active = false;
    bool s_raining = false, s_thundering = false;
    int s_rainTime = 0, s_thunderTime = 0;
    float s_rain = 0.0f, s_prevRain = 0.0f, s_thunder = 0.0f, s_prevThunder = 0.0f;
    int s_flash = 0;
    int s_ticks = 0;
    const ChunkManager* s_cm = nullptr;

    unsigned s_rng = 0x2F6E2B1u;
    unsigned rnd() { s_rng ^= s_rng << 13; s_rng ^= s_rng >> 17; s_rng ^= s_rng << 5; return s_rng; }
    int nextInt(int n) { return (int)(rnd() % (unsigned)n); }

    unsigned hash2(int a, int b) {
        unsigned h = (unsigned)a * 73856093u ^ (unsigned)b * 19349663u;
        h ^= h >> 13; h *= 0x5bd1e995u; h ^= h >> 15;
        return h;
    }

    // ---- columnas: la celda mas alta con algo solido o liquido (findTopSolidBlock) y que cae en ella (0 nada, 1 lluvia, 2 nieve) ----
    const int R = 4, N = 2 * R + 1;
    struct Col { int gx, gz; short top; signed char kind; bool valid; };
    Col s_cols[N][N];

    Chunk* chunkAtGrid(int gx, int gz) {
        if (!s_cm) return nullptr;
        Chunk* c = s_cm->getChunkByChunkCoord(gx >> 3, gz >> 3);
        return (c && c->isGenerationComplete()) ? c : nullptr;
    }

    int topOf(const Chunk* c, int xi, int zi) {
        for (int y = 127; y >= 1; y--) {
            const BlockType t = c->getBlockType(xi, y, zi);
            if (isMaterialSolid(t) || isLiquidBlock(t)) return y + 1;
        }
        return 0;
    }

    int kindOf(const Chunk* c, int xi, int zi) {
        const BiomeType b = c->getBiomeAt(xi, zi);
        if (b == BiomeType::TAIGA || b == BiomeType::TUNDRA) return 2;
        if (b == BiomeType::DESERT) return 0;
        return 1;
    }

    void refresh(int gx, int gz) {
        Col& col = s_cols[((gx % N) + N) % N][((gz % N) + N) % N];
        Chunk* c = chunkAtGrid(gx, gz);
        col.gx = gx; col.gz = gz;
        if (!c) { col.valid = false; return; }
        col.top = (short)topOf(c, gx & 7, gz & 7);
        col.kind = (signed char)kindOf(c, gx & 7, gz & 7);
        col.valid = true;
    }

    // ---- el rayo ----
    struct Bolt {
        bool active = false;
        int gx = 0, gy = 0, gz = 0;
        float x = 0, y = 0, z = 0;
        int life = 0, flashes = 0;
        unsigned seed = 0;
        bool strikePending = false;
    };
    Bolt s_bolt;

    void fireAt(int gx, int gy, int gz) { if (s_cm) s_cm->ignite(gx, gy, gz); }

    void spawnBolt(int gx, int top, int gz) {
        Bolt& b = s_bolt;
        b.active = true; b.gx = gx; b.gy = top; b.gz = gz;
        b.x = (float)(gx - 4) + 0.5f; b.y = (float)top - 1.5f; b.z = (float)(gz - 4) + 0.5f;
        b.life = 2; b.flashes = nextInt(3) + 1; b.seed = rnd(); b.strikePending = false;
        fireAt(gx, top, gz);
        for (int i = 0; i < 4; i++) fireAt(gx + nextInt(3) - 1, top + nextInt(3) - 1, gz + nextInt(3) - 1);
    }

    void tickBolt() {
        Bolt& b = s_bolt;
        if (!b.active) return;
        --b.life;
        if (b.life < 0) {
            if (b.flashes == 0) { b.active = false; return; }
            if (b.life < -nextInt(10)) {
                --b.flashes; b.life = 1; b.seed = rnd();
                fireAt(b.gx, b.gy, b.gz);
            }
        }
        if (b.life >= 0) { b.strikePending = true; s_flash = 2; }
    }

    // World.updateWeather
    void updateWeather() {
        if (s_thunderTime <= 0) {
            s_thunderTime = s_thundering ? nextInt(12000) + 3600 : nextInt(168000) + 12000;
        } else if (--s_thunderTime <= 0) {
            s_thundering = !s_thundering;
        }
        if (s_rainTime <= 0) {
            s_rainTime = s_raining ? nextInt(12000) + 12000 : nextInt(168000) + 12000;
        } else if (--s_rainTime <= 0) {
            s_raining = !s_raining;
        }
        s_prevRain = s_rain;
        s_rain += s_raining ? 0.01f : -0.01f;
        if (s_rain < 0.0f) s_rain = 0.0f;
        if (s_rain > 1.0f) s_rain = 1.0f;
        s_prevThunder = s_thunder;
        s_thunder += s_thundering ? 0.01f : -0.01f;
        if (s_thunder < 0.0f) s_thunder = 0.0f;
        if (s_thunder > 1.0f) s_thunder = 1.0f;
    }

    const int LIGHTNING_PER_100K = 250;   // el original: 1 de cada 100000 por chunk de 16 x 16 en 19 x 19 chunks (~361); aqui se mira solo lo cargado, asi que se sube el azar
}

void Weather::reset() {
    s_raining = s_thundering = false;
    s_rainTime = s_thunderTime = 0;
    s_rain = s_prevRain = s_thunder = s_prevThunder = 0.0f;
    s_flash = 0; s_ticks = 0;
    s_bolt = Bolt();
    for (int i = 0; i < N; i++) for (int j = 0; j < N; j++) s_cols[i][j].valid = false;
    s_cm = nullptr;
}

void Weather::load() {
    char path[96];
    WorldSave::getBaseFilePath("weather.dat", path);
    FILE* f = fopen(path, "rb");
    if (!f) return;
    int v[6];
    float fl[2];
    if (fread(v, sizeof(int), 6, f) == 6 && fread(fl, sizeof(float), 2, f) == 2 && v[0] == 0x57544831) {
        s_raining = v[1] != 0; s_thundering = v[2] != 0; s_rainTime = v[3]; s_thunderTime = v[4];
        s_rain = s_prevRain = fl[0]; s_thunder = s_prevThunder = fl[1];
    }
    fclose(f);
}

void Weather::save() {
    char path[96];
    WorldSave::getBaseFilePath("weather.dat", path);
    FILE* f = fopen(path, "wb");
    if (!f) return;
    const int v[6] = { 0x57544831, s_raining ? 1 : 0, s_thundering ? 1 : 0, s_rainTime, s_thunderTime, 0 };
    const float fl[2] = { s_rain, s_thunder };
    fwrite(v, sizeof(int), 6, f);
    fwrite(fl, sizeof(float), 2, f);
    fclose(f);
}

// World.wakeUpAllPlayers -> func_27163_E (clearWeather): al amanecer tras dormir se acaba la lluvia y la tormenta. La intensidad baja sola
// (0,01 por tick, como siempre) y los contadores en 0 hacen que el proximo tick sortee cuanto dura el buen tiempo.
void Weather::clearAfterSleep() {
    s_raining = s_thundering = false;
    s_rainTime = s_thunderTime = 0;
}

void Weather::debugToggle() {
    const bool on = !(s_raining || s_rain > 0.05f);
    s_raining = s_thundering = on;
    s_rainTime = s_thunderTime = 12000;
    s_rain = s_prevRain = s_thunder = s_prevThunder = on ? 1.0f : 0.0f;
}

float Weather::rain(float pt) { return s_active ? s_prevRain + (s_rain - s_prevRain) * pt : 0.0f; }
float Weather::thunder(float pt) { return s_active ? (s_prevThunder + (s_thunder - s_prevThunder) * pt) * rain(pt) : 0.0f; }
bool Weather::isRaining() { return rain(1.0f) > 0.2f; }
bool Weather::isThundering() { return thunder(1.0f) > 0.9f; }

float Weather::flash(float pt) {
    if (!s_active || s_flash <= 0) return 0.0f;
    float f = (float)s_flash - pt;
    if (f > 1.0f) f = 1.0f;
    if (f < 0.0f) f = 0.0f;
    return f;
}

bool Weather::canRainAtGrid(int gx, int gy, int gz) {
    if (!isRaining()) return false;
    Chunk* c = chunkAtGrid(gx, gz);
    if (!c) return false;
    if (topOf(c, gx & 7, gz & 7) > gy) return false;
    return kindOf(c, gx & 7, gz & 7) == 1;
}

bool Weather::canRainAtWorld(float x, float y, float z) {
    return canRainAtGrid((int)floorf(x) + 4, (int)floorf(y + 0.5f) + 1, (int)floorf(z) + 4);
}

bool Weather::popStrike(float& x, float& y, float& z) {
    if (!s_bolt.active || !s_bolt.strikePending) return false;
    s_bolt.strikePending = false;
    x = s_bolt.x; y = s_bolt.y; z = s_bolt.z;
    return true;
}

void Weather::tick(const ChunkManager& cm, float px, float py, float pz, bool overworld) {
    s_cm = &cm;
    s_active = overworld;
    if (!overworld) return;
    ++s_ticks;
    if (s_flash > 0) --s_flash;
    updateWeather();

    const int pgx = (int)floorf(px) + 4, pgz = (int)floorf(pz) + 4;

    // las columnas de alrededor (para dibujar la lluvia y la nieve): un puñado por tick, en ronda
    {
        static int s_idx = 0;
        for (int n = 0; n < 24; n++) {
            s_idx = (s_idx + 1) % (N * N);
            refresh(pgx - R + s_idx % N, pgz - R + s_idx / N);
        }
    }

    // gotas que rebotan en el suelo: unas pocas por tick, sobre las columnas de lluvia de alrededor
    if (s_rain > 0.3f) {
        for (int n = 0; n < 2; n++) {
            const Col& col = s_cols[nextInt(N)][nextInt(N)];
            if (!col.valid || col.kind != 1 || col.top <= 0 || nextInt(100) >= (int)(s_rain * 60.0f)) continue;
            Particles::spawn(Particles::SPLASH, (float)(col.gx - 4) + (float)nextInt(1000) / 1000.0f, (float)col.top - 1.5f + 0.05f, (float)(col.gz - 4) + (float)nextInt(1000) / 1000.0f, 0.0f, 0.0f, 0.0f);
        }
    }

    // EntityRenderer.addRainParticles: el sonido de la lluvia. Entre las 100 * lluvia^2 columnas probadas (aqui las de alrededor) se elige una al azar de las que reciben lluvia;
    // suena si hay alguna y el contador supera un azar de 0 a 2. Bajo techo suena mas lejano y grave.
    if (s_rain > 0.0f) {
        static int s_soundCounter = 0;
        int hits = 0;
        float sx = 0, sy = 0, sz = 0;
        const int tries = (int)(30.0f * s_rain * s_rain);
        for (int n = 0; n < tries; n++) {
            const Col& col = s_cols[nextInt(N)][nextInt(N)];
            if (!col.valid || col.kind != 1 || col.top <= 0) continue;
            ++hits;
            if (nextInt(hits) == 0) { sx = (float)(col.gx - 4) + 0.5f; sy = (float)col.top - 1.0f; sz = (float)(col.gz - 4) + 0.5f; }
        }
        if (hits > 0 && nextInt(3) < s_soundCounter++) {
            s_soundCounter = 0;
            Sound::rain(sx, sy, sz, sy > py + 1.0f);
        }
    }

    // rayos: con tormenta fuerte, un azar por tick; cae en una columna cargada a menos de 40 bloques que vea el cielo, en un bioma con lluvia
    tickBolt();
    if (!s_bolt.active && isRaining() && isThundering() && nextInt(100000) < LIGHTNING_PER_100K) {
        const int gx = pgx + nextInt(81) - 40, gz = pgz + nextInt(81) - 40;
        Chunk* c = chunkAtGrid(gx, gz);
        if (c) {
            const int top = topOf(c, gx & 7, gz & 7);
            if (kindOf(c, gx & 7, gz & 7) == 1 && top > 0 && top < 126) spawnBolt(gx, top, gz);
        }
    }

    // World.updateBlocksAndPlayCaveSounds: en biomas con nieve, la nieve cubre el suelo bajo la lluvia y el agua quieta se congela (con luz de bloque menor a 10)
    for (int n = 0; n < 6; n++) {
        const int gx = pgx + nextInt(81) - 40, gz = pgz + nextInt(81) - 40;
        Chunk* c = chunkAtGrid(gx, gz);
        if (!c) continue;
        const int xi = gx & 7, zi = gz & 7;
        if (kindOf(c, xi, zi) != 2) continue;
        const int top = topOf(c, xi, zi);
        if (top < 2 || top >= 127) continue;
        if (c->getBlockLight(xi, top, zi) >= 10) continue;
        const BlockType below = c->getBlockType(xi, top - 1, zi);
        const BlockType here = c->getBlockType(xi, top, zi);
        if (isRaining() && here == BlockType::AIR && below != BlockType::ICE && below != BlockType::SNOW_LAYER && !isLiquidBlock(below) &&
            (isNormalCube(below) || isLeavesBlock(below))) {
            cm.setBlockGlobal(gx, top, gz, BlockType::SNOW_LAYER);
        }
        if (below == BlockType::WATER && c->getLiquidMeta(xi, top - 1, zi) == 0) cm.setBlockGlobal(gx, top - 1, gz, BlockType::ICE);
    }
}

// ------------------------------------------------------------------------------------------------ dibujo
namespace {
    inline void quad4(float ax, float ay, float az, float bx, float by, float bz, float cx, float cy, float cz, float dx, float dy, float dz) {
        glVertex3v16(floattov16(ax), floattov16(ay), floattov16(az));
        glVertex3v16(floattov16(bx), floattov16(by), floattov16(bz));
        glVertex3v16(floattov16(cx), floattov16(cy), floattov16(cz));
        glVertex3v16(floattov16(dx), floattov16(dy), floattov16(dz));
    }

    void drawPrecip(float pt, float ex, float ey, float ez, float yawDeg, float dayBright) {
        const float rs = Weather::rain(pt);
        const float yr = yawDeg * (3.14159265f / 180.0f);
        const float rx = cosf(yr), rz = -sinf(yr);   // la derecha de la camara
        const float t = ((float)s_ticks + pt) / 20.0f;
        const int baseX = (int)floorf(ex), baseY = (int)floorf(ey), baseZ = (int)floorf(ez);
        const float yLo = ey - 7.0f - (float)baseY, yHi = ey + 7.0f - (float)baseY;   // relativo a baseY (< 8: el rango de v16)
        const float f = 0.4f + 0.6f * dayBright;
        const int pgx = baseX + 4, pgz = baseZ + 4;

        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        glTranslatef((float)baseX, (float)baseY, (float)baseZ);
        glBindTexture(0, 0);
        for (int pass = 0; pass < 2; pass++) {   // 0: lluvia, 1: nieve
            if (pass == 0) { glPolyFmt(POLY_ALPHA(15) | POLY_CULL_NONE | POLY_ID(40)); glColor3b((int)(165 * f), (int)(185 * f), (int)(255 * f)); }
            else           { glPolyFmt(POLY_ALPHA(24) | POLY_CULL_NONE | POLY_ID(41)); glColor3b((int)(255 * f), (int)(255 * f), (int)(255 * f)); }
            glBegin(GL_QUADS);
            for (int gz = pgz - R; gz <= pgz + R; gz++) {
                for (int gx = pgx - R; gx <= pgx + R; gx++) {
                    const Col& col = s_cols[((gx % N) + N) % N][((gz % N) + N) % N];
                    if (!col.valid || col.gx != gx || col.gz != gz || col.kind != (pass == 0 ? 1 : 2)) continue;
                    const unsigned h = hash2(gx, gz);
                    if ((float)(h & 255) >= rs * 255.0f) continue;
                    float lo = (float)col.top - 1.5f - (float)baseY;   // la superficie
                    if (lo < yLo) lo = yLo;
                    if (lo >= yHi) continue;
                    const float span = yHi - lo + 1.0f;
                    for (int s = 0; s < (pass == 0 ? 1 : 2); s++) {
                        const unsigned hh = hash2((int)h, s + 7);
                        const float x = (float)(gx - 4 - baseX) + (float)(hh & 1023) / 1024.0f;
                        const float z = (float)(gz - 4 - baseZ) + (float)((hh >> 10) & 1023) / 1024.0f;
                        const float off = (float)((hh >> 20) & 255) / 256.0f * span;
                        if (pass == 0) {
                            float top = yHi - fmodf(off + t * 9.0f, span), bot = top - 0.9f;
                            if (bot < lo) bot = lo;
                            if (top > yHi) top = yHi;
                            if (top <= bot) continue;
                            const float hw = 0.012f;
                            quad4(x - hw * rx, bot, z - hw * rz, x + hw * rx, bot, z + hw * rz, x + hw * rx, top, z + hw * rz, x - hw * rx, top, z - hw * rz);
                        } else {
                            const float y = yHi - fmodf(off + t * 1.6f, span);
                            if (y <= lo || y > yHi) continue;
                            const float sx = x + sinf(t * 1.5f + (float)(hh & 63)) * 0.25f;
                            const float hw = 0.035f;
                            quad4(sx - hw * rx, y - hw, z - hw * rz, sx + hw * rx, y - hw, z + hw * rz, sx + hw * rx, y + hw, z + hw * rz, sx - hw * rx, y + hw, z - hw * rz);
                        }
                    }
                }
            }
            glEnd();
        }
        glPopMatrix(1);
    }

    // RenderLightningBolt: 8 tramos de 16 bloques que se desvian al azar (semilla del rayo); tres capas, mas gruesas y mas tenues hacia afuera
    void drawBolt(float ex, float ey, float ez, float yawDeg) {
        const Bolt& b = s_bolt;
        const float dx = b.x - ex, dz = b.z - ez;
        if (dx * dx + dz * dz > 120.0f * 120.0f) return;
        const float yr = yawDeg * (3.14159265f / 180.0f);
        const float rx = cosf(yr), rz = -sinf(yr);
        unsigned seed = b.seed | 1u;
        auto nxt = [&seed](int n) { seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5; return (int)(seed % (unsigned)n); };
        float ox[9], oz[9];
        ox[0] = oz[0] = 0.0f;
        for (int k = 1; k < 9; k++) { ox[k] = ox[k - 1] + (float)(nxt(11) - 5); oz[k] = oz[k - 1] + (float)(nxt(11) - 5); }

        static const float W[3] = { 0.36f, 0.18f, 0.07f };
        static const int A[3] = { 6, 11, 31 };
        glMatrixMode(GL_MODELVIEW);
        glBindTexture(0, 0);
        for (int layer = 0; layer < 3; layer++) {
            glPolyFmt(POLY_ALPHA(A[layer]) | POLY_CULL_NONE | POLY_ID(42 + layer));
            glColor3b(layer == 2 ? 255 : 225, layer == 2 ? 255 : 225, 255);
            for (int k = 0; k < 8; k++) {
                for (int p = 0; p < 4; p++) {   // 4 pedazos de 4 bloques: v16 no pasa de 8
                    const float f0 = (float)p / 4.0f, f1 = (float)(p + 1) / 4.0f;
                    const float x0 = ox[k] + (ox[k + 1] - ox[k]) * f0, x1 = ox[k] + (ox[k + 1] - ox[k]) * f1;
                    const float z0 = oz[k] + (oz[k + 1] - oz[k]) * f0, z1 = oz[k] + (oz[k + 1] - oz[k]) * f1;
                    const float y0 = (float)(k * 16) + (float)p * 4.0f;
                    const float w = W[layer] * ((float)k * 0.1f + 1.0f) * 0.5f;
                    glPushMatrix();
                    glTranslatef(b.x + x0, b.y + y0, b.z + z0);
                    glBegin(GL_QUADS);
                    quad4(-w * rx, 0.0f, -w * rz, w * rx, 0.0f, w * rz, (x1 - x0) + w * rx, 4.0f, (z1 - z0) + w * rz, (x1 - x0) - w * rx, 4.0f, (z1 - z0) - w * rz);
                    glEnd();
                    glPopMatrix(1);
                }
            }
        }
    }
}

void Weather::draw(float pt, float ex, float ey, float ez, float yawDeg, float dayBright) {
    if (!s_active) return;
    if (rain(pt) > 0.01f) drawPrecip(pt, ex, ey, ez, yawDeg, dayBright);
    if (s_bolt.active && s_bolt.life >= 0) drawBolt(ex, ey, ez, yawDeg);
}
