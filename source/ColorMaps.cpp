#include "ColorMaps.h"
#include "ColorMapData.h"
#include <stdlib.h>
#include <string.h>

static const unsigned char* s_grass = GRASS_TABLE;
static const unsigned char* s_foliage = FOLIAGE_TABLE;
static unsigned char* s_custom = nullptr;   // las dos tablas de un texture pack (solo existe con un pack que las trae)

// ColorizerGrass.getGrassColor: humedad *= temperatura; x = (1 - temperatura) * 255; y = (1 - humedad) * 255
static void lookup(const unsigned char* table, float temperature, float humidity, u8& r, u8& g, u8& b) {
    humidity *= temperature;
    float fx = (1.0f - temperature) * 255.0f * (1.0f / 8.0f);
    float fy = (1.0f - humidity) * 255.0f * (1.0f / 8.0f);
    if (fx < 0.0f) fx = 0.0f;
    if (fy < 0.0f) fy = 0.0f;
    if (fx > 31.999f) fx = 31.999f;
    if (fy > 31.999f) fy = 31.999f;
    int i = (int)fx, j = (int)fy;
    float u = fx - (float)i, v = fy - (float)j;
    const unsigned char* p00 = table + (j * 33 + i) * 3;
    const unsigned char* p10 = p00 + 3;
    const unsigned char* p01 = p00 + 33 * 3;
    const unsigned char* p11 = p01 + 3;
    float out[3];
    for (int c = 0; c < 3; c++) {
        float a = (float)p00[c] + ((float)p10[c] - (float)p00[c]) * u;
        float d = (float)p01[c] + ((float)p11[c] - (float)p01[c]) * u;
        out[c] = a + (d - a) * v;
    }
    r = (u8)(out[0] + 0.5f);
    g = (u8)(out[1] + 0.5f);
    b = (u8)(out[2] + 0.5f);
}

void ColorMaps::grass(float temperature, float humidity, u8& r, u8& g, u8& b) {
    lookup(s_grass, temperature, humidity, r, g, b);
}

void ColorMaps::foliage(float temperature, float humidity, u8& r, u8& g, u8& b) {
    lookup(s_foliage, temperature, humidity, r, g, b);
}

void ColorMaps::useDefault() {
    s_grass = GRASS_TABLE;
    s_foliage = FOLIAGE_TABLE;
    free(s_custom);
    s_custom = nullptr;
}

bool ColorMaps::setCustom(const u8* grassTable, const u8* foliageTable) {
    if (!grassTable && !foliageTable) { useDefault(); return true; }
    if (!s_custom) s_custom = (unsigned char*)malloc(2 * TABLE_BYTES);
    if (!s_custom) { useDefault(); return false; }
    memcpy(s_custom, grassTable ? grassTable : GRASS_TABLE, TABLE_BYTES);
    memcpy(s_custom + TABLE_BYTES, foliageTable ? foliageTable : FOLIAGE_TABLE, TABLE_BYTES);
    s_grass = s_custom;
    s_foliage = s_custom + TABLE_BYTES;
    return true;
}

const u8* ColorMaps::grassTable() { return s_grass; }
const u8* ColorMaps::foliageTable() { return s_foliage; }
