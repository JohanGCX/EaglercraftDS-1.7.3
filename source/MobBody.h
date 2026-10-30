#ifndef MOBBODY_H
#define MOBBODY_H

#include <math.h>

// Colision entre mobs (Entity.applyEntityCollision): cada manager (ovejas, animales, hostiles) entrega los cuerpos de sus mobs vivos y SheepManager
// los empuja a todos entre si, sin importar de que manager sean.
struct MobBody {
    double* x; double* y; double* z;     // posicion (y = pies)
    double* mx; double* mz;              // velocidad
    double w, h;                         // ancho y alto
};

namespace MobBodies {
    // other se aleja de self (+) y self se aleja de other (-): 0,05 de velocidad, mas fuerte cuanto mas cerca
    inline void pushApart(double sx, double sz, double ox, double oz, double& oMx, double& oMz, double& sMx, double& sMz) {
        double dx = ox - sx, dz = oz - sz;
        double m = fabs(dx) > fabs(dz) ? fabs(dx) : fabs(dz);
        if (m < 0.01) return;
        m = sqrt(m);
        dx /= m; dz /= m;
        double inv = 1.0 / m; if (inv > 1.0) inv = 1.0;
        dx *= inv * 0.05; dz *= inv * 0.05;
        sMx -= dx; sMz -= dz;
        oMx += dx; oMz += dz;
    }

    // EntityLiving.onLivingUpdate: cada mob empuja a los que tocan su caja (+0,2 en horizontal). Se aplica por pares ordenados (cada uno empuja al otro en su
    // propio tick), como en el original.
    inline void collide(MobBody* b, int n) {
        for (int i = 0; i < n; i++) {
            for (int j = 0; j < n; j++) {
                if (i == j) continue;
                const MobBody& a = b[i];
                const MobBody& o = b[j];
                const double hx = a.w * 0.5 + 0.2 + o.w * 0.5;
                if (fabs(*o.x - *a.x) >= hx || fabs(*o.z - *a.z) >= hx) continue;
                if (*o.y >= *a.y + a.h || *o.y + o.h <= *a.y) continue;
                pushApart(*a.x, *a.z, *o.x, *o.z, *o.mx, *o.mz, *a.mx, *a.mz);
            }
        }
    }
}

#endif
