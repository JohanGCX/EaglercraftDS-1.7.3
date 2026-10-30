#ifndef EXPLOSIONDAMAGE_H
#define EXPLOSIONDAMAGE_H

#include <math.h>
#include "BlockType.h"

// Explosion.doExplosionA, la parte de las entidades (Beta 1.7.3), para cualquier criatura con una caja: dano segun la distancia y lo que se ve
// del centro de la explosion (World.func_675_a), y el empuje. W debe ofrecer BlockType type(i, j, k) en celdas de mundo (la celda j cubre de j - 0,5 a j + 0,5).
namespace ExplosionDamage {
    template <class W>
    inline bool lineClear(W& w, double x0, double y0, double z0, double x1, double y1, double z1) {
        const double dx = x1 - x0, dy = y1 - y0, dz = z1 - z0;
        const double len = sqrt(dx * dx + dy * dy + dz * dz);
        const int steps = (int)ceil(len / 0.25);
        if (steps < 2) return true;
        for (int n = 1; n < steps; n++) {
            const double t = (double)n / (double)steps;
            if (isMaterialSolid(w.type((int)floor(x0 + dx * t), (int)floor(y0 + dy * t + 0.5), (int)floor(z0 + dz * t)))) return false;
        }
        return true;
    }

    template <class W>
    inline float exposure(W& w, double ex, double ey, double ez, double x0, double y0, double z0, double x1, double y1, double z1) {
        const double sx = 1.0 / ((x1 - x0) * 2.0 + 1.0), sy = 1.0 / ((y1 - y0) * 2.0 + 1.0), sz = 1.0 / ((z1 - z0) * 2.0 + 1.0);
        int hit = 0, total = 0;
        for (double f = 0.0; f <= 1.0; f += sx) for (double g = 0.0; g <= 1.0; g += sy) for (double h = 0.0; h <= 1.0; h += sz) {
            if (lineClear(w, x0 + (x1 - x0) * f, y0 + (y1 - y0) * g, z0 + (z1 - z0) * h, ex, ey, ez)) ++hit;
            ++total;
        }
        return total > 0 ? (float)hit / (float)total : 0.0f;
    }

    // (x, y, z) = pies de la criatura. Devuelve true si la alcanza: dmg = dano y (kx, ky, kz) = lo que se suma a su velocidad
    template <class W>
    inline bool compute(W& w, double ex, double ey, double ez, float size, double x, double y, double z, double width, double height,
                        int& dmg, double& kx, double& ky, double& kz) {
        const float D = size * 2.0f;
        const double dx = x - ex, dy = y - ey, dz = z - ez;
        const double d = sqrt(dx * dx + dy * dy + dz * dz);
        if (d / (double)D > 1.0 || d < 1.0e-6) return false;
        const double e = (double)exposure(w, ex, ey, ez, x - width * 0.5, y, z - width * 0.5, x + width * 0.5, y + height, z + width * 0.5);
        const double v = (1.0 - d / (double)D) * e;
        dmg = (int)((v * v + v) / 2.0 * 8.0 * (double)D + 1.0);
        kx = dx / d * v; ky = dy / d * v; kz = dz / d * v;
        return true;
    }
}

#endif
