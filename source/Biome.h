#ifndef BIOME_H
#define BIOME_H

// Los primeros cinco valores son los de siempre; el resto son los biomas de la Beta 1.7.3 (BiomeGenBase) que salen del clima.
// OCEAN es la marca de las columnas bajo el agua (el spawn evita esas columnas); GENERIC queda para el Nether y valores por defecto.
enum class BiomeType : unsigned char {
    OCEAN, DESERT, GENERIC, FOREST, TAIGA,
    TUNDRA, RAINFOREST, SEASONAL_FOREST, SAVANNA, SHRUBLAND, SWAMPLAND, PLAINS
};

namespace BiomeConfig {
    constexpr float BIOME_NOISE_SCALE = 0.015f;   // ruido de las cuencas oceánicas del terreno

    inline BiomeType getBiomeFromNoise(float noiseValue) {
        if (noiseValue < 0.25f) return BiomeType::OCEAN;
        if (noiseValue < 0.5f) return BiomeType::DESERT;
        if (noiseValue < 0.75f) return BiomeType::GENERIC;
        return BiomeType::FOREST;
    }

    constexpr float PATCH_NOISE_SCALE = 0.15f;
    constexpr float PATCH_THRESHOLD = 0.82f;

    // ChunkProviderGenerate.populate: bonus de árboles por bioma (bosque, selva y taiga +5; bosque estacional +2; desierto, tundra y
    // llanura -20; el resto solo el 1 de cada 10 de base). Devuelve cada cuántas columnas se intenta un árbol (0 = nunca)
    inline int treeAttemptDenominator(BiomeType b) {
        switch (b) {
            case BiomeType::FOREST:
            case BiomeType::RAINFOREST:
            case BiomeType::TAIGA:           return 40;
            case BiomeType::SEASONAL_FOREST: return 72;
            case BiomeType::SAVANNA:
            case BiomeType::SHRUBLAND:
            case BiomeType::SWAMPLAND:
            case BiomeType::GENERIC:         return 2048;
            default:                         return 0;   // desierto, tundra, llanura, océano
        }
    }
}

#endif