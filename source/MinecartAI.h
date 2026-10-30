#ifndef MINECARTAI_H
#define MINECARTAI_H

// Fisica de la vagoneta: port de EntityMinecart.onUpdate (Beta 1.7.3) y de la posicion sobre el riel que usa RenderMinecart (func_514_g y func_515_a).
// El mundo W (en coordenadas de rejilla, ver MinecartData.h) debe dar:
//   BlockType type(int x, int y, int z);   int meta(int x, int y, int z);   bool normal(int x, int y, int z);   bool solid(int x, int y, int z);
// normal = World.isBlockNormalCube; solid = bloque que detiene a la vagoneta.

#include <math.h>
#include "MinecartData.h"
#include "RailLogic.h"

namespace MinecartAI {

    const double WIDTH = 0.98, HEIGHT = 0.7, Y_OFFSET = 0.35;
    const double PI_D = 3.14159265358979323846;

    struct Vec { double x, y, z; };
    struct Box { double minX, minY, minZ, maxX, maxY, maxZ; };

    inline int fl(double v) { return (int)floor(v); }

    inline Box bodyBox(const Minecart& c) {
        const double minY = c.y - Y_OFFSET;
        return { c.x - WIDTH / 2.0, minY, c.z - WIDTH / 2.0, c.x + WIDTH / 2.0, minY + HEIGHT, c.z + WIDTH / 2.0 };
    }

    inline void setPosition(Minecart& c, double x, double y, double z) { c.x = x; c.y = y; c.z = z; }

    // ---- Posicion sobre el riel ----
    // func_514_g: el punto del riel que corresponde a (x, y, z) (la vagoneta se pega al riel). false si ahi no hay riel
    template <class W>
    inline bool railPos(W& w, double x, double y, double z, Vec& out) {
        const int ix = fl(x);
        int iy = fl(y);
        const int iz = fl(z);
        if (Rails::isRailAt(w, ix, iy - 1, iz)) --iy;
        const BlockType t = w.type(ix, iy, iz);
        if (!isRailBlock(t)) return false;
        int m = w.meta(ix, iy, iz);
        if (Rails::hasPower(t)) m &= 7;
        const int (*e)[3] = Rails::endpoints(m);
        double k = 0.0;
        const double x0 = (double)ix + 0.5 + (double)e[0][0] * 0.5;
        const double y0 = (double)iy + 0.5 + (double)e[0][1] * 0.5;
        const double z0 = (double)iz + 0.5 + (double)e[0][2] * 0.5;
        const double x1 = (double)ix + 0.5 + (double)e[1][0] * 0.5;
        const double y1 = (double)iy + 0.5 + (double)e[1][1] * 0.5;
        const double z1 = (double)iz + 0.5 + (double)e[1][2] * 0.5;
        const double dx = x1 - x0;
        const double dy = (y1 - y0) * 2.0;
        const double dz = z1 - z0;
        if (dx == 0.0) { x = (double)ix + 0.5; k = z - (double)iz; }
        else if (dz == 0.0) { z = (double)iz + 0.5; k = x - (double)ix; }
        else { k = ((x - x0) * dx + (z - z0) * dz) * 2.0; }
        x = x0 + dx * k;
        double yy = y0 + dy * k;
        z = z0 + dz * k;
        if (dy < 0.0) yy += 1.0;
        if (dy > 0.0) yy += 0.5;
        out.x = x; out.y = yy; out.z = z;
        return true;
    }

    // func_515_a: el punto del riel a 'off' bloques por delante (o por detras, con off negativo)
    template <class W>
    inline bool railPosAhead(W& w, double x, double y, double z, double off, Vec& out) {
        const int ix = fl(x);
        int iy = fl(y);
        const int iz = fl(z);
        if (Rails::isRailAt(w, ix, iy - 1, iz)) --iy;
        const BlockType t = w.type(ix, iy, iz);
        if (!isRailBlock(t)) return false;
        int m = w.meta(ix, iy, iz);
        if (Rails::hasPower(t)) m &= 7;
        y = (double)iy;
        if (m >= 2 && m <= 5) y = (double)(iy + 1);
        const int (*e)[3] = Rails::endpoints(m);
        double dx = (double)(e[1][0] - e[0][0]);
        double dz = (double)(e[1][2] - e[0][2]);
        const double len = sqrt(dx * dx + dz * dz);
        dx /= len; dz /= len;
        x += dx * off;
        z += dz * off;
        if (e[0][1] != 0 && fl(x) - ix == e[0][0] && fl(z) - iz == e[0][2]) y += (double)e[0][1];
        else if (e[1][1] != 0 && fl(x) - ix == e[1][0] && fl(z) - iz == e[1][2]) y += (double)e[1][1];
        return railPos(w, x, y, z, out);
    }

    // ---- Colision con bloques (Entity.moveEntity: primero Y, luego X y luego Z) ----
    inline double calcX(const Box& b, const Box& e, double d) {
        if (e.maxY <= b.minY || e.minY >= b.maxY || e.maxZ <= b.minZ || e.minZ >= b.maxZ) return d;
        if (d > 0.0 && e.maxX <= b.minX) { const double m = b.minX - e.maxX; if (m < d) d = m; }
        if (d < 0.0 && e.minX >= b.maxX) { const double m = b.maxX - e.minX; if (m > d) d = m; }
        return d;
    }
    inline double calcY(const Box& b, const Box& e, double d) {
        if (e.maxX <= b.minX || e.minX >= b.maxX || e.maxZ <= b.minZ || e.minZ >= b.maxZ) return d;
        if (d > 0.0 && e.maxY <= b.minY) { const double m = b.minY - e.maxY; if (m < d) d = m; }
        if (d < 0.0 && e.minY >= b.maxY) { const double m = b.maxY - e.minY; if (m > d) d = m; }
        return d;
    }
    inline double calcZ(const Box& b, const Box& e, double d) {
        if (e.maxX <= b.minX || e.minX >= b.maxX || e.maxY <= b.minY || e.minY >= b.maxY) return d;
        if (d > 0.0 && e.maxZ <= b.minZ) { const double m = b.minZ - e.maxZ; if (m < d) d = m; }
        if (d < 0.0 && e.minZ >= b.maxZ) { const double m = b.maxZ - e.minZ; if (m > d) d = m; }
        return d;
    }

    template <class W>
    inline void moveEntity(Minecart& c, double dx, double dy, double dz, W& w) {
        const double ox = dx, oy = dy, oz = dz;
        Box e = bodyBox(c);
        const int x0 = fl(e.minX + (dx < 0.0 ? dx : 0.0)) , x1 = fl(e.maxX + (dx > 0.0 ? dx : 0.0));
        const int y0 = fl(e.minY + (dy < 0.0 ? dy : 0.0)) - 1, y1 = fl(e.maxY + (dy > 0.0 ? dy : 0.0)) + 1;
        const int z0 = fl(e.minZ + (dz < 0.0 ? dz : 0.0)) , z1 = fl(e.maxZ + (dz > 0.0 ? dz : 0.0));
        if ((x1 - x0 + 1) * (y1 - y0 + 1) * (z1 - z0 + 1) > 200) return;   // un salto absurdo: se ignora

        for (int i = x0; i <= x1; i++) for (int k = z0; k <= z1; k++) for (int j = y0; j <= y1; j++) {
            if (!w.solid(i, j, k)) continue;
            const Box b = { (double)i, (double)j, (double)k, (double)i + 1.0, (double)j + 1.0, (double)k + 1.0 };
            dy = calcY(b, e, dy);
        }
        e.minY += dy; e.maxY += dy;
        for (int i = x0; i <= x1; i++) for (int k = z0; k <= z1; k++) for (int j = y0; j <= y1; j++) {
            if (!w.solid(i, j, k)) continue;
            const Box b = { (double)i, (double)j, (double)k, (double)i + 1.0, (double)j + 1.0, (double)k + 1.0 };
            dx = calcX(b, e, dx);
        }
        e.minX += dx; e.maxX += dx;
        for (int i = x0; i <= x1; i++) for (int k = z0; k <= z1; k++) for (int j = y0; j <= y1; j++) {
            if (!w.solid(i, j, k)) continue;
            const Box b = { (double)i, (double)j, (double)k, (double)i + 1.0, (double)j + 1.0, (double)k + 1.0 };
            dz = calcZ(b, e, dz);
        }
        e.minZ += dz; e.maxZ += dz;

        c.x = (e.minX + e.maxX) / 2.0;
        c.y = e.minY + Y_OFFSET;
        c.z = (e.minZ + e.maxZ) / 2.0;
        c.onGround = (oy != dy && oy < 0.0);
        if (ox != dx) c.motionX = 0.0;
        if (oy != dy) c.motionY = 0.0;
        if (oz != dz) c.motionZ = 0.0;
    }

    inline double hyp(double a, double b) { return sqrt(a * a + b * b); }

    // EntityMinecart.onUpdate (un tick). rnd4 = un entero al azar de 0 a 3 (el humo de la vagoneta con horno gasta combustible 1 de cada 4 ticks)
    template <class W>
    inline void tick(Minecart& c, W& w, int rnd4) {
        if (c.timeSinceHit > 0) --c.timeSinceHit;
        if (c.damage > 0) --c.damage;
        c.prevX = c.x; c.prevY = c.y; c.prevZ = c.z;
        c.prevYaw = c.yaw; c.prevPitch = c.pitch;

        c.motionY -= 0.04;
        const int ix = fl(c.x);
        int iy = fl(c.y);
        const int iz = fl(c.z);
        if (Rails::isRailAt(w, ix, iy - 1, iz)) --iy;

        const double maxSpeed = 0.4;
        bool smoke = false;
        const double slope = 1.0 / 128.0;
        const BlockType bt = w.type(ix, iy, iz);
        if (isRailBlock(bt)) {
            Vec before;
            const bool haveBefore = railPos(w, c.x, c.y, c.z, before);
            int meta = w.meta(ix, iy, iz);
            c.y = (double)iy;
            bool poweredOn = false, poweredOff = false;
            if (bt == BlockType::RAIL_POWERED) {
                poweredOn = (meta & 8) != 0;
                poweredOff = !poweredOn;
            }
            if (Rails::hasPower(bt)) meta &= 7;
            if (meta >= 2 && meta <= 5) c.y = (double)(iy + 1);
            if (meta == 2) c.motionX -= slope;
            if (meta == 3) c.motionX += slope;
            if (meta == 4) c.motionZ += slope;
            if (meta == 5) c.motionZ -= slope;

            const int (*e)[3] = Rails::endpoints(meta);
            double dirX = (double)(e[1][0] - e[0][0]);
            double dirZ = (double)(e[1][2] - e[0][2]);
            const double dirLen = sqrt(dirX * dirX + dirZ * dirZ);
            if (c.motionX * dirX + c.motionZ * dirZ < 0.0) { dirX = -dirX; dirZ = -dirZ; }
            double speed = hyp(c.motionX, c.motionZ);
            c.motionX = speed * dirX / dirLen;
            c.motionZ = speed * dirZ / dirLen;
            if (poweredOff) {   // propulsor apagado: frena
                speed = hyp(c.motionX, c.motionZ);
                if (speed < 0.03) { c.motionX = 0.0; c.motionY = 0.0; c.motionZ = 0.0; }
                else { c.motionX *= 0.5; c.motionY = 0.0; c.motionZ *= 0.5; }
            }

            double along = 0.0;
            const double sx = (double)ix + 0.5 + (double)e[0][0] * 0.5;
            const double sz = (double)iz + 0.5 + (double)e[0][2] * 0.5;
            const double ex = (double)ix + 0.5 + (double)e[1][0] * 0.5;
            const double ez = (double)iz + 0.5 + (double)e[1][2] * 0.5;
            dirX = ex - sx;
            dirZ = ez - sz;
            if (dirX == 0.0) { c.x = (double)ix + 0.5; along = c.z - (double)iz; }
            else if (dirZ == 0.0) { c.z = (double)iz + 0.5; along = c.x - (double)ix; }
            else { along = ((c.x - sx) * dirX + (c.z - sz) * dirZ) * 2.0; }
            c.x = sx + dirX * along;
            c.z = sz + dirZ * along;
            setPosition(c, c.x, c.y + Y_OFFSET, c.z);

            double mx = c.motionX, mz = c.motionZ;
            if (c.ridden) { mx *= 0.75; mz *= 0.75; }
            if (mx < -maxSpeed) mx = -maxSpeed;
            if (mx > maxSpeed) mx = maxSpeed;
            if (mz < -maxSpeed) mz = -maxSpeed;
            if (mz > maxSpeed) mz = maxSpeed;
            moveEntity(c, mx, 0.0, mz, w);

            if (e[0][1] != 0 && fl(c.x) - ix == e[0][0] && fl(c.z) - iz == e[0][2]) setPosition(c, c.x, c.y + (double)e[0][1], c.z);
            else if (e[1][1] != 0 && fl(c.x) - ix == e[1][0] && fl(c.z) - iz == e[1][2]) setPosition(c, c.x, c.y + (double)e[1][1], c.z);

            if (c.ridden) {
                c.motionX *= 0.997; c.motionY = 0.0; c.motionZ *= 0.997;
            } else {
                if (c.type == 2) {
                    const double pl = hyp(c.pushX, c.pushZ);
                    if (pl > 0.01) {
                        smoke = true;
                        c.pushX /= pl; c.pushZ /= pl;
                        c.motionX *= 0.8; c.motionY = 0.0; c.motionZ *= 0.8;
                        c.motionX += c.pushX * 0.04; c.motionZ += c.pushZ * 0.04;
                    } else {
                        c.motionX *= 0.9; c.motionY = 0.0; c.motionZ *= 0.9;
                    }
                }
                c.motionX *= 0.96; c.motionY = 0.0; c.motionZ *= 0.96;
            }

            Vec after;
            if (railPos(w, c.x, c.y, c.z, after) && haveBefore) {
                const double dh = (before.y - after.y) * 0.05;
                speed = hyp(c.motionX, c.motionZ);
                if (speed > 0.0) {
                    c.motionX = c.motionX / speed * (speed + dh);
                    c.motionZ = c.motionZ / speed * (speed + dh);
                }
                setPosition(c, c.x, after.y, c.z);
            }

            const int nx = fl(c.x), nz = fl(c.z);
            if (nx != ix || nz != iz) {
                speed = hyp(c.motionX, c.motionZ);
                c.motionX = speed * (double)(nx - ix);
                c.motionZ = speed * (double)(nz - iz);
            }

            if (c.type == 2) {
                const double pl = hyp(c.pushX, c.pushZ);
                if (pl > 0.01 && c.motionX * c.motionX + c.motionZ * c.motionZ > 0.001) {
                    c.pushX /= pl; c.pushZ /= pl;
                    if (c.pushX * c.motionX + c.pushZ * c.motionZ < 0.0) { c.pushX = 0.0; c.pushZ = 0.0; }
                    else { c.pushX = c.motionX; c.pushZ = c.motionZ; }
                }
            }

            if (poweredOn) {   // propulsor encendido: acelera, o la arranca si esta parada contra un bloque
                speed = hyp(c.motionX, c.motionZ);
                if (speed > 0.01) {
                    c.motionX += c.motionX / speed * 0.06;
                    c.motionZ += c.motionZ / speed * 0.06;
                } else if (meta == 1) {
                    if (w.normal(ix - 1, iy, iz)) c.motionX = 0.02;
                    else if (w.normal(ix + 1, iy, iz)) c.motionX = -0.02;
                } else if (meta == 0) {
                    if (w.normal(ix, iy, iz - 1)) c.motionZ = 0.02;
                    else if (w.normal(ix, iy, iz + 1)) c.motionZ = -0.02;
                }
            }
        } else {
            if (c.motionX < -maxSpeed) c.motionX = -maxSpeed;
            if (c.motionX > maxSpeed) c.motionX = maxSpeed;
            if (c.motionZ < -maxSpeed) c.motionZ = -maxSpeed;
            if (c.motionZ > maxSpeed) c.motionZ = maxSpeed;
            if (c.onGround) { c.motionX *= 0.5; c.motionY *= 0.5; c.motionZ *= 0.5; }
            moveEntity(c, c.motionX, c.motionY, c.motionZ, w);
            if (!c.onGround) { c.motionX *= 0.95; c.motionY *= 0.95; c.motionZ *= 0.95; }
        }

        c.pitch = 0.0f;
        const double ddx = c.prevX - c.x, ddz = c.prevZ - c.z;
        if (ddx * ddx + ddz * ddz > 0.001) {
            c.yaw = (float)(atan2(ddz, ddx) * 180.0 / PI_D);
            if (c.flipped) c.yaw += 180.0f;
        }
        double dyaw = (double)(c.yaw - c.prevYaw);
        while (dyaw >= 180.0) dyaw -= 360.0;
        while (dyaw < -180.0) dyaw += 360.0;
        if (dyaw < -170.0 || dyaw >= 170.0) { c.yaw += 180.0f; c.flipped = !c.flipped; }

        if (smoke && rnd4 == 0) {
            --c.fuel;
            if (c.fuel < 0) { c.pushX = 0.0; c.pushZ = 0.0; }
        }
    }

    // ---- Dibujo: la posicion y los giros que calcula RenderMinecart.func_152_a ----
    struct Pose { double x, y, z; float yawDeg, pitchDeg; };

    template <class W>
    inline Pose renderPose(const Minecart& c, W& w, float pt) {
        const double px = c.prevX + (c.x - c.prevX) * (double)pt;
        const double py = c.prevY + (c.y - c.prevY) * (double)pt;
        const double pz = c.prevZ + (c.z - c.prevZ) * (double)pt;
        Pose p;
        p.x = px; p.y = py; p.z = pz;
        p.yawDeg = c.prevYaw + (c.yaw - c.prevYaw) * pt;
        p.pitchDeg = c.prevPitch + (c.pitch - c.prevPitch) * pt;
        Vec cur;
        if (railPos(w, px, py, pz, cur)) {
            Vec a, b;
            if (!railPosAhead(w, px, py, pz, 0.3, a)) a = cur;
            if (!railPosAhead(w, px, py, pz, -0.3, b)) b = cur;
            p.x = cur.x;
            p.y = (a.y + b.y) / 2.0;
            p.z = cur.z;
            double dx = b.x - a.x, dy = b.y - a.y, dz = b.z - a.z;
            const double len = sqrt(dx * dx + dy * dy + dz * dz);
            if (len != 0.0) {
                dx /= len; dy /= len; dz /= len;
                p.yawDeg = (float)(atan2(dz, dx) * 180.0 / PI_D);
                p.pitchDeg = (float)(atan(dy) * 73.0);
            }
        }
        return p;
    }

    // Rayo contra la caja de la vagoneta (agrandada 0,1 como la de cualquier entidad). Devuelve la distancia o -1
    inline double rayHit(const Minecart& c, double ox, double oy, double oz, double dx, double dy, double dz, double maxDist) {
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
