#ifndef BOATAI_H
#define BOATAI_H

// Fisica del bote: port de EntityBoat.onUpdate (Beta 1.7.3, rama de un solo jugador). Flota segun cuanto de su caja esta en agua, el jinete lo empuja
// (motion += motionDelJinete * 0,2), se orienta hacia donde se mueve y se rompe si choca de lado a mas de 0,15 de velocidad.
// El mundo W (coordenadas de rejilla) debe dar:  BlockType type(int x, int y, int z);   int meta(int x, int y, int z);   bool solid(int x, int y, int z);

#include <math.h>
#include "BoatData.h"
#include "MinecartAI.h"
#include "Liquid.h"

namespace BoatAI {

    const double WIDTH = 1.5, HEIGHT = 0.6, Y_OFFSET = 0.3;
    const double MOUNTED_Y = -0.3 + 1.12;   // getMountedYOffset (height * 0 - 0,3) + EntityPlayer.getYOffset sentado (1,62 - 0,5): los ojos del jugador sobre el centro del bote

    using MinecartAI::Box;
    using MinecartAI::fl;

    inline Box bodyBox(const Boat& b) {
        const double minY = b.y - Y_OFFSET;
        return { b.x - WIDTH / 2.0, minY, b.z - WIDTH / 2.0, b.x + WIDTH / 2.0, minY + HEIGHT, b.z + WIDTH / 2.0 };
    }

    // World.isAABBInMaterial(caja, agua)
    template <class W>
    inline bool boxInWater(W& w, double minX, double minY, double minZ, double maxX, double maxY, double maxZ) {
        const int x0 = fl(minX), x1 = fl(maxX + 1.0), y0 = fl(minY), y1 = fl(maxY + 1.0), z0 = fl(minZ), z1 = fl(maxZ + 1.0);
        for (int x = x0; x < x1; x++)
            for (int y = y0; y < y1; y++)
                for (int z = z0; z < z1; z++) {
                    if (!isWaterBlock(w.type(x, y, z))) continue;
                    const double surface = (double)((float)(y + 1) - Liquid::percentAir(w.meta(x, y, z)));
                    if ((double)y1 >= surface) return true;
                }
        return false;
    }

    // Entity.moveEntity: primero Y, luego X y luego Z contra los bloques solidos. Devuelve si choco de lado
    template <class W>
    inline bool moveEntity(Boat& c, double dx, double dy, double dz, W& w) {
        const double ox = dx, oy = dy, oz = dz;
        Box e = bodyBox(c);
        const int x0 = fl(e.minX + (dx < 0.0 ? dx : 0.0)), x1 = fl(e.maxX + (dx > 0.0 ? dx : 0.0));
        const int y0 = fl(e.minY + (dy < 0.0 ? dy : 0.0)) - 1, y1 = fl(e.maxY + (dy > 0.0 ? dy : 0.0)) + 1;
        const int z0 = fl(e.minZ + (dz < 0.0 ? dz : 0.0)), z1 = fl(e.maxZ + (dz > 0.0 ? dz : 0.0));
        if ((x1 - x0 + 1) * (y1 - y0 + 1) * (z1 - z0 + 1) > 300) return false;

        for (int i = x0; i <= x1; i++) for (int k = z0; k <= z1; k++) for (int j = y0; j <= y1; j++) {
            if (!w.solid(i, j, k)) continue;
            const Box b = { (double)i, (double)j, (double)k, (double)i + 1.0, (double)j + 1.0, (double)k + 1.0 };
            dy = MinecartAI::calcY(b, e, dy);
        }
        e.minY += dy; e.maxY += dy;
        for (int i = x0; i <= x1; i++) for (int k = z0; k <= z1; k++) for (int j = y0; j <= y1; j++) {
            if (!w.solid(i, j, k)) continue;
            const Box b = { (double)i, (double)j, (double)k, (double)i + 1.0, (double)j + 1.0, (double)k + 1.0 };
            dx = MinecartAI::calcX(b, e, dx);
        }
        e.minX += dx; e.maxX += dx;
        for (int i = x0; i <= x1; i++) for (int k = z0; k <= z1; k++) for (int j = y0; j <= y1; j++) {
            if (!w.solid(i, j, k)) continue;
            const Box b = { (double)i, (double)j, (double)k, (double)i + 1.0, (double)j + 1.0, (double)k + 1.0 };
            dz = MinecartAI::calcZ(b, e, dz);
        }
        e.minZ += dz; e.maxZ += dz;

        c.x = (e.minX + e.maxX) / 2.0;
        c.y = e.minY + Y_OFFSET;
        c.z = (e.minZ + e.maxZ) / 2.0;
        c.onGround = (oy != dy && oy < 0.0);
        if (ox != dx) c.motionX = 0.0;
        if (oy != dy) c.motionY = 0.0;
        if (oz != dz) c.motionZ = 0.0;
        return ox != dx || oz != dz;
    }

    // EntityBoat.onUpdate (un tick). (riderMX, riderMZ): el movimiento del jinete (solo si hasRider). Devuelve true si el bote se destruyo al chocar
    template <class W>
    inline bool tick(Boat& b, W& w, bool hasRider, double riderMX, double riderMZ) {
        b.prevYaw = b.yaw;                         // Entity.onUpdate
        if (b.timeSinceHit > 0) --b.timeSinceHit;
        if (b.damage > 0) --b.damage;
        b.prevX = b.x; b.prevY = b.y; b.prevZ = b.z;

        // cuanto de la caja esta en agua: 5 rebanadas, cada una 1/8 de bloque mas abajo
        const Box e = bodyBox(b);
        double water = 0.0;
        for (int i = 0; i < 5; i++) {
            const double y0 = e.minY + (e.maxY - e.minY) * (double)(i + 0) / 5.0 - 0.125;
            const double y1 = e.minY + (e.maxY - e.minY) * (double)(i + 1) / 5.0 - 0.125;
            if (boxInWater(w, e.minX, y0, e.minZ, e.maxX, y1, e.maxZ)) water += 1.0 / 5.0;
        }

        if (water < 1.0) {
            b.motionY += (double)0.04f * (water * 2.0 - 1.0);
        } else {
            if (b.motionY < 0.0) b.motionY /= 2.0;
            b.motionY += (double)0.007f;
        }

        if (hasRider) {
            b.motionX += riderMX * 0.2;
            b.motionZ += riderMZ * 0.2;
        }

        const double lim = 0.4;
        if (b.motionX < -lim) b.motionX = -lim;
        if (b.motionX > lim) b.motionX = lim;
        if (b.motionZ < -lim) b.motionZ = -lim;
        if (b.motionZ > lim) b.motionZ = lim;

        if (b.onGround) { b.motionX *= 0.5; b.motionY *= 0.5; b.motionZ *= 0.5; }

        const bool collided = moveEntity(b, b.motionX, b.motionY, b.motionZ, w);
        const double speed = sqrt(b.motionX * b.motionX + b.motionZ * b.motionZ);

        if (collided && speed > 0.15) return true;   // se rompe contra la orilla: suelta 3 tablas y 2 palos
        b.motionX *= (double)0.99f; b.motionY *= (double)0.95f; b.motionZ *= (double)0.99f;

        // se orienta hacia donde va (cada tick gira como mucho 20 grados)
        double target = (double)b.yaw;
        const double dx = b.prevX - b.x, dz = b.prevZ - b.z;
        if (dx * dx + dz * dz > 0.001) target = (double)((float)(atan2(dz, dx) * 180.0 / MinecartAI::PI_D));
        double diff = target - (double)b.yaw;
        while (diff >= 180.0) diff -= 360.0;
        while (diff < -180.0) diff += 360.0;
        if (diff > 20.0) diff = 20.0;
        if (diff < -20.0) diff = -20.0;
        b.yaw = (float)((double)b.yaw + diff);
        return false;
    }

    // Rayo contra la caja del bote (agrandada 0,1 como la de cualquier entidad). Devuelve la distancia o -1
    inline double rayHit(const Boat& c, double ox, double oy, double oz, double dx, double dy, double dz, double maxDist) {
        Box b = bodyBox(c);
        const double g = 0.1;
        b.minX -= g; b.minY -= g; b.minZ -= g; b.maxX += g; b.maxY += g; b.maxZ += g;
        double t0 = 0.0, t1 = maxDist;
        const double o[3] = { ox, oy, oz }, d[3] = { dx, dy, dz };
        const double lo[3] = { b.minX, b.minY, b.minZ }, hi[3] = { b.maxX, b.maxY, b.maxZ };
        for (int a = 0; a < 3; a++) {
            if (fabs(d[a]) < 1e-9) { if (o[a] < lo[a] || o[a] > hi[a]) return -1.0; continue; }
            double ta = (lo[a] - o[a]) / d[a], tb = (hi[a] - o[a]) / d[a];
            if (ta > tb) { const double t = ta; ta = tb; tb = t; }
            if (ta > t0) t0 = ta;
            if (tb < t1) t1 = tb;
            if (t0 > t1) return -1.0;
        }
        return t0;
    }
}

#endif
