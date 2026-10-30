#ifndef WORLDCONFIG_H
#define WORLDCONFIG_H

namespace WorldConfig {
    constexpr int SIZE_X = 8;
    constexpr int SIZE_Y = 128;
    constexpr int SIZE_Z = 8;

    // Modo de chunks (Options): el ChunkManager lo fija al crearse con setChunkMode().
    //   0 = fast: ventana de 3 x 3 (como antes), 1 columna por cuadro
    //   1 = ds:   ventana de 5 x 5, hasta 3 columnas por cuadro
    //   2 = dsi:  ventana de 7 x 7, hasta 6 columnas por cuadro y el doble de presupuesto de tiempo (solo si el aparato es una DSi)
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

    inline void setChunkMode(int radius, int cols, int mul) {
        viewRadiusRef() = radius; genColsRef() = cols; budgetMulRef() = mul;
    }
}

#endif