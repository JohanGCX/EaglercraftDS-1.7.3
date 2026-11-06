#ifndef WORLDCONFIG_H
#define WORLDCONFIG_H

#include <nds/ndstypes.h>

// MC_HOT: marks tiny functions called thousands of times per frame (chunk lookup, world->grid conversion) so they live in ITCM
// (32 KB of zero-wait-state memory inside the ARM9) instead of main RAM behind the 16-bit bus and the 8 KB cache.
// Only a few hundred bytes in total. To switch it off, define MC_NO_ITCM (for example CFLAGS += -DMC_NO_ITCM in the Makefile).
#if defined(ITCM_CODE) && !defined(MC_NO_ITCM)
#define MC_HOT ITCM_CODE
#else
#define MC_HOT
#endif

namespace WorldConfig {
    constexpr int SIZE_X = 8;
    constexpr int SIZE_Y = 128;
    constexpr int SIZE_Z = 8;

    // Modo de chunks (Options): el ChunkManager lo fija al crearse con setChunkMode().
    //   0 = fast: ventana de 3 x 3 (como antes), 1 columna por cuadro
    //   1 = ds:   ventana de 5 x 5, hasta 3 columnas por cuadro
    //   2 = dsi:  ventana de 7 x 7, hasta 4 columnas por cuadro con el mismo presupuesto de tiempo que ds (solo si el aparato es una DSi)
    // Cada chunk cuesta unos 31 KB de RAM fijos mas sus listas de dibujo.
    constexpr int MAX_VIEW_RADIUS = 3;

    inline int& viewRadiusRef() { static int r = 2; return r; }
    inline int& genColsRef()    { static int c = 3; return c; }
    inline int& budgetMulRef()  { static int m = 1; return m; }

    inline int viewRadius()     { return viewRadiusRef(); }
    inline int genColsPerFrame(){ return genColsRef(); }
    inline int budgetMul()      { return budgetMulRef(); }

    // Distancia maxima (en unidades de mundo) a la que se renderizan bloques desde el jugador: 9 con radio 1, y cada radio mas suma 8.
    inline float renderDistance() { return 8.0f * (float)viewRadiusRef() + 1.0f; }

    // Terrain generator version of the world being played. It is stored in meta.dat (WorldMetadata::terrainGen), so a world keeps the
    // terrain it was created with: 0 = original generator (every world made before this field existed), 1 = normalized relief (no plateaus).
    // Changing what a generator version produces would leave seams between saved and newly generated chunks: add a new version instead.
    constexpr int CURRENT_TERRAIN_GEN = 1;
    inline int& terrainGenRef() { static int g = 0; return g; }
    inline int terrainGen()     { return terrainGenRef(); }
    inline void setTerrainGen(int g) { terrainGenRef() = g; }

    inline void setChunkMode(int radius, int cols, int mul) {
        viewRadiusRef() = radius; genColsRef() = cols; budgetMulRef() = mul;
    }
}

#endif