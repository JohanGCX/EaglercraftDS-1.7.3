#ifndef SHEEPAI_H
#define SHEEPAI_H

#include <math.h>
#include "BlockType.h"
#include "SheepData.h"
#include <stdlib.h>
#include <string.h>
#include "Sound.h"

// =====================================================================
// Física, IA y combate de la oveja: port de EntityLiving / EntityCreature / EntityAnimal / EntitySheep (Beta 1.7.3). Código puro, sin libnds:
// el mundo entra como parámetro de plantilla W con
//     BlockType type(int i, int j, int k)       celda: i = floor(x), j = floor(y + 0.5), k = floor(z)   (el bloque j cubre y de j-0,5 a j+0,5)
//     float light(int i, int j, int k)          brillo 0..1 (para elegir destinos)
//     bool chunkReady(int cx, int cz)           el chunk existe y terminó de generarse
// Los caminos se calculan con el Pathfinder (A*) de la Beta, con un tope de puntos y de pasos (ver PATH_MAX_*).
// =====================================================================
namespace SheepAI {
    const double WIDTH = 0.9, HEIGHT = 1.3, HALF = 0.45;
    const double EYE = HEIGHT * 0.85;          // EntityLiving.getEyeHeight
    const double PI_D = 3.14159265358979323846;
    const float MOVE_SPEED = 0.7f;             // EntityLiving.moveSpeed

    struct Rng {
        unsigned int s;
        explicit Rng(unsigned int seed = 0x9E3779B9u) : s(seed) {}
        unsigned int next() { s = s * 1664525u + 1013904223u; return s >> 8; }
        float nextFloat() { return (float)(next() & 0xFFFFFFu) / 16777216.0f; }
        int nextInt(int n) { return (int)(nextFloat() * (float)n); }
    };

    struct PlayerPos { double x, y, z; bool valid; };   // los ojos del jugador

    inline double wrap180(double a) { while (a < -180.0) a += 360.0; while (a >= 180.0) a -= 360.0; return a; }
    inline int cellY(double y) { return (int)floor(y + 0.5); }

    // ---- EntitySheep.getRandomFleeceColor ----
    inline int randomFleeceColor(Rng& r) {
        const int n = r.nextInt(100);
        return n < 5 ? 15 : (n < 10 ? 7 : (n < 15 ? 8 : (n < 18 ? 12 : (r.nextInt(500) == 0 ? 6 : 0))));
    }

    template <class W> inline bool blocking(W& w, int i, int j, int k) { return isMaterialSolid(w.type(i, j, k)); }

    // ---- AxisAlignedBB.calculateXOffset/Y/Z: la caja de la celda contra la caja de la oveja ----
    struct Box { double x0, y0, z0, x1, y1, z1; };
    inline Box cellBox(int i, int j, int k) { return { (double)i, (double)j - 0.5, (double)k, (double)i + 1.0, (double)j + 0.5, (double)k + 1.0 }; }
    // La caja se recalcula desde el centro en cada tick (el original la guarda): el redondeo la puede dejar a ~1e-16 DENTRO del bloque con el que
    // chocó, y entonces el chequeo exacto ya no la frena. Por eso se acepta un margen de EPS y la distancia nunca baja de 0.
    const double EPS = 1.0e-7;
    inline double calcX(const Box& b, const Box& e, double dx) {
        if (e.y1 > b.y0 + EPS && e.y0 < b.y1 - EPS && e.z1 > b.z0 + EPS && e.z0 < b.z1 - EPS) {
            if (dx > 0.0 && e.x1 <= b.x0 + EPS) { double d = b.x0 - e.x1; if (d < 0.0) d = 0.0; if (d < dx) dx = d; }
            if (dx < 0.0 && e.x0 >= b.x1 - EPS) { double d = b.x1 - e.x0; if (d > 0.0) d = 0.0; if (d > dx) dx = d; }
        }
        return dx;
    }
    inline double calcY(const Box& b, const Box& e, double dy) {
        if (e.x1 > b.x0 + EPS && e.x0 < b.x1 - EPS && e.z1 > b.z0 + EPS && e.z0 < b.z1 - EPS) {
            if (dy > 0.0 && e.y1 <= b.y0 + EPS) { double d = b.y0 - e.y1; if (d < 0.0) d = 0.0; if (d < dy) dy = d; }
            if (dy < 0.0 && e.y0 >= b.y1 - EPS) { double d = b.y1 - e.y0; if (d > 0.0) d = 0.0; if (d > dy) dy = d; }
        }
        return dy;
    }
    inline double calcZ(const Box& b, const Box& e, double dz) {
        if (e.x1 > b.x0 + EPS && e.x0 < b.x1 - EPS && e.y1 > b.y0 + EPS && e.y0 < b.y1 - EPS) {
            if (dz > 0.0 && e.z1 <= b.z0 + EPS) { double d = b.z0 - e.z1; if (d < 0.0) d = 0.0; if (d < dz) dz = d; }
            if (dz < 0.0 && e.z0 >= b.z1 - EPS) { double d = b.z1 - e.z0; if (d > 0.0) d = 0.0; if (d > dz) dz = d; }
        }
        return dz;
    }

    inline Box bodyBox(const Sheep& s) { return { s.x - HALF, s.y, s.z - HALF, s.x + HALF, s.y + HEIGHT, s.z + HALF }; }

    // ¿hay algún bloque sólido o algún líquido (según 'what') en la caja?  what: 0 = sólido, 1 = agua, 2 = lava, 3 = cualquier líquido
    template <class W>
    inline bool anyIn(W& w, const Box& b, int what) {
        for (int i = (int)floor(b.x0); i <= (int)floor(b.x1); i++)
            for (int j = cellY(b.y0); j <= cellY(b.y1); j++)
                for (int k = (int)floor(b.z0); k <= (int)floor(b.z1); k++) {
                    const BlockType t = w.type(i, j, k);
                    if (what == 0 ? isMaterialSolid(t) : (what == 1 ? isWaterBlock(t) : (what == 2 ? isLavaBlock(t) : isLiquidBlock(t)))) return true;
                }
        return false;
    }

    // Entity.handleWaterMovement: la caja sin 0,4 arriba ni abajo y sin 0,001 a los lados
    template <class W> inline bool inWater(const Sheep& s, W& w) {
        return anyIn(w, { s.x - HALF + 0.001, s.y + 0.4 + 0.001, s.z - HALF + 0.001, s.x + HALF - 0.001, s.y + HEIGHT - 0.4 - 0.001, s.z + HALF - 0.001 }, 1);
    }
    // Entity.handleLavaMovement: la caja sin 0,1 a los lados y sin 0,4 arriba ni abajo
    template <class W> inline bool inLava(const Sheep& s, W& w) {
        return anyIn(w, { s.x - HALF + 0.1, s.y + 0.4, s.z - HALF + 0.1, s.x + HALF - 0.1, s.y + HEIGHT - 0.4, s.z + HALF - 0.1 }, 2);
    }
    template <class W> inline bool headInWater(const Sheep& s, W& w) {
        return isWaterBlock(w.type((int)floor(s.x), cellY(s.y + EYE), (int)floor(s.z)));
    }
    // Entity.isBoundingBoxBurning: hay un bloque de fuego dentro de la caja del cuerpo
    template <class W> inline bool inFire(const Sheep& s, W& w) {
        for (int i = (int)floor(s.x - HALF + 0.001); i <= (int)floor(s.x + HALF - 0.001); i++)
            for (int j = cellY(s.y + 0.001); j <= cellY(s.y + HEIGHT - 0.001); j++)
                for (int k = (int)floor(s.z - HALF + 0.001); k <= (int)floor(s.z + HALF - 0.001); k++)
                    if (w.type(i, j, k) == BlockType::FIRE) return true;
        return false;
    }
    // Entity.isEntityInsideOpaqueBlock: 8 puntos alrededor de la cabeza
    template <class W> inline bool insideOpaque(const Sheep& s, W& w) {
        for (int n = 0; n < 8; n++) {
            const double dx = (((n >> 0) % 2) - 0.5) * WIDTH * 0.9, dy = (((n >> 1) % 2) - 0.5) * 0.1, dz = (((n >> 2) % 2) - 0.5) * WIDTH * 0.9;
            if (isNormalCube(w.type((int)floor(s.x + dx), cellY(s.y + EYE + dy), (int)floor(s.z + dz)))) return true;
        }
        return false;
    }

    // ---- Entity.moveEntity (sin escalón: en este mundo de cubos enteros no cambia nada). Devuelve la caída que se acaba de terminar (0 si no) ----
    template <class W>
    float moveEntity(Sheep& s, double dx, double dy, double dz, W& w) {
        if (s.isInWeb) { s.isInWeb = false; dx *= 0.25; dy *= 0.05; dz *= 0.25; s.motionX = s.motionY = s.motionZ = 0.0; }   // Entity.moveEntity con isInWeb
        Box e = bodyBox(s);
        const double odx = dx, ody = dy, odz = dz;
        const int i0 = (int)floor(fmin(e.x0, e.x0 + dx)), i1 = (int)floor(fmax(e.x1, e.x1 + dx));
        const int j0 = cellY(fmin(e.y0, e.y0 + dy)), j1 = cellY(fmax(e.y1, e.y1 + dy));
        const int k0 = (int)floor(fmin(e.z0, e.z0 + dz)), k1 = (int)floor(fmax(e.z1, e.z1 + dz));

        for (int i = i0; i <= i1; i++) for (int j = j0; j <= j1; j++) for (int k = k0; k <= k1; k++)
            if (blocking(w, i, j, k)) dy = calcY(cellBox(i, j, k), e, dy);
        e.y0 += dy; e.y1 += dy;
        for (int i = i0; i <= i1; i++) for (int j = j0; j <= j1; j++) for (int k = k0; k <= k1; k++)
            if (blocking(w, i, j, k)) dx = calcX(cellBox(i, j, k), e, dx);
        e.x0 += dx; e.x1 += dx;
        for (int i = i0; i <= i1; i++) for (int j = j0; j <= j1; j++) for (int k = k0; k <= k1; k++)
            if (blocking(w, i, j, k)) dz = calcZ(cellBox(i, j, k), e, dz);
        e.z0 += dz; e.z1 += dz;

        s.x = (e.x0 + e.x1) * 0.5; s.y = e.y0; s.z = (e.z0 + e.z1) * 0.5;
        s.onGround = (ody != dy) && ody < 0.0;
        s.collidedH = (odx != dx) || (odz != dz);
        {   // Entity.doBlockCollisions -> BlockWeb.onEntityCollidedWithBlock: tocar telaraña marca isInWeb
            const int a0 = (int)floor(e.x0 + 0.001), a1 = (int)floor(e.x1 - 0.001), b0 = cellY(e.y0 + 0.001), b1 = cellY(e.y1 - 0.001), c0 = (int)floor(e.z0 + 0.001), c1 = (int)floor(e.z1 - 0.001);
            for (int i = a0; i <= a1; i++) for (int j = b0; j <= b1; j++) for (int k = c0; k <= c1; k++)
                if (w.type(i, j, k) == BlockType::WEB) s.isInWeb = true;
        }
        if (ody != dy) s.motionY = 0.0;
        if (odx != dx) s.motionX = 0.0;
        if (odz != dz) s.motionZ = 0.0;

        // Entity.updateFallState
        float landed = 0.0f;
        if (s.onGround) { if (s.fallDistance > 0.0f) { landed = s.fallDistance; s.fallDistance = 0.0f; } }
        else if (dy < 0.0) s.fallDistance -= (float)dy;
        return landed;
    }

    // ---- daño: EntityLiving.attackEntityFrom + knockBack ----
    inline void knockBack(Sheep& s, double dx, double dz) {
        const double d = sqrt(dx * dx + dz * dz);
        s.motionX /= 2.0; s.motionY /= 2.0; s.motionZ /= 2.0;
        s.motionX -= dx / d * 0.4; s.motionY += 0.4; s.motionZ -= dz / d * 0.4;
        if (s.motionY > 0.4) s.motionY = 0.4;
    }
    // attackerX/Z: posición de quien golpea (hasAttacker = false para daño del entorno). Devuelve true si el daño se aplicó.
    inline bool damage(Sheep& s, int amount, bool hasAttacker, double ax, double az) {
        if (s.hs.health <= 0) return false;
        const bool newHit = !((float)s.hs.heartsLife > (float)Health::HEARTS_HALVES_LIFE / 2.0f);
        if (!Health::attack(s.hs, amount)) return false;
        Sound::mobHurt(200, (float)s.x, (float)s.y, (float)s.z);
        if (newHit && hasAttacker) {
            double dx = ax - s.x, dz = az - s.z;
            if (dx * dx + dz * dz < 1.0e-4) { dx = 0.01; dz = 0.0; }
            s.hs.attackedAtYaw = (float)(atan2(dz, dx) * 180.0 / PI_D) - s.yaw;
            knockBack(s, dx, dz);
        }
        if (s.hs.health <= 0) s.diedPending = true;
        return true;
    }

    // ---- EntityLiving.faceEntity / updateRotation ----
    inline float updateRotation(float cur, float target, float maxStep) {
        float d = (float)wrap180((double)target - (double)cur);
        if (d > maxStep) d = maxStep;
        if (d < -maxStep) d = -maxStep;
        return cur + d;
    }
    inline void faceEntity(Sheep& s, const PlayerPos& p, float yawStep, float pitchStep) {
        const double dx = p.x - s.x, dz = p.z - s.z, dy = p.y - (s.y + EYE);
        const double h = sqrt(dx * dx + dz * dz);
        const float targetYaw = (float)(atan2(dz, dx) * 180.0 / PI_D) - 90.0f;
        const float targetPitch = (float)(-(atan2(dy, h) * 180.0 / PI_D));
        s.pitch = updateRotation(s.pitch, targetPitch, pitchStep);
        s.yaw = updateRotation(s.yaw, targetYaw, yawStep);
    }

    // ---- EntityAnimal.getBlockPathWeight: el pasto vale 10; si no, el brillo del lugar menos 0,5 ----
    template <class W>
    inline float pathWeight(W& w, int i, int j, int k) { return w.type(i, j - 1, k) == BlockType::GRASS ? 10.0f : w.light(i, j, k) - 0.5f; }

    // =====================================================================
    // Pathfinder (A*), con PathPoint, Path (montículo) y PathEntity de la Beta. En lugar de objetos, los puntos viven en un arreglo fijo
    // de PATH_MAX_NODES; si se llena o se pasa de PATH_MAX_EXPANSIONS la búsqueda termina con el mejor camino parcial (el original no tiene tope).
    // =====================================================================
    const int PATH_MAX_NODES = 200;
    const int PATH_TABLE = 1024;               // tabla de dispersión de los puntos abiertos (MCHash)
    const int PATH_MAX_EXPANSIONS = 80;

    struct PNode {
        int x, y, z;
        float total;        // totalPathDistance
        float toNext;       // distanceToNext
        float toTarget;     // distanceToTarget: la prioridad en el montículo
        int prev;           // previous (índice o -1)
        int heapIndex;      // index (-1: no está en el montículo)
        bool closed;        // isFirst
    };
    struct PathWork {
        PNode nodes[PATH_MAX_NODES];
        int nodeCount;
        int heap[PATH_MAX_NODES];
        int heapCount;
        short table[PATH_TABLE];
    };
    inline PathWork& pathWork() { static PathWork w; return w; }

    inline unsigned pathHash(int x, int y, int z) {
        unsigned h = ((unsigned)x * 73856093u) ^ ((unsigned)y * 19349663u) ^ ((unsigned)z * 83492791u);
        return (h ^ (h >> 11)) & (unsigned)(PATH_TABLE - 1);
    }
    // Pathfinder.openPoint
    inline int openPoint(PathWork& pw, int x, int y, int z) {
        const unsigned h = pathHash(x, y, z);
        for (int probe = 0; probe < PATH_TABLE; probe++) {
            const unsigned slot = (h + (unsigned)probe) & (unsigned)(PATH_TABLE - 1);
            const short e = pw.table[slot];
            if (e == 0) {
                if (pw.nodeCount >= PATH_MAX_NODES) return -1;
                const int idx = pw.nodeCount++;
                PNode& n = pw.nodes[idx];
                n.x = x; n.y = y; n.z = z; n.total = 0.0f; n.toNext = 0.0f; n.toTarget = 0.0f; n.prev = -1; n.heapIndex = -1; n.closed = false;
                pw.table[slot] = (short)(idx + 1);
                return idx;
            }
            const PNode& n = pw.nodes[e - 1];
            if (n.x == x && n.y == y && n.z == z) return e - 1;
        }
        return -1;
    }
    inline float pointDist(const PNode& a, const PNode& b) {   // PathPoint.distanceTo
        const float dx = (float)(b.x - a.x), dy = (float)(b.y - a.y), dz = (float)(b.z - a.z);
        return sqrtf(dx * dx + dy * dy + dz * dz);
    }

    // Path: montículo binario por distanceToTarget
    inline void heapSortBack(PathWork& pw, int i) {
        const int node = pw.heap[i];
        const float key = pw.nodes[node].toTarget;
        while (i > 0) {
            const int parent = (i - 1) >> 1;
            if (key >= pw.nodes[pw.heap[parent]].toTarget) break;
            pw.heap[i] = pw.heap[parent];
            pw.nodes[pw.heap[i]].heapIndex = i;
            i = parent;
        }
        pw.heap[i] = node;
        pw.nodes[node].heapIndex = i;
    }
    inline void heapSortForward(PathWork& pw, int i) {
        const int node = pw.heap[i];
        const float key = pw.nodes[node].toTarget;
        for (;;) {
            const int l = 1 + (i << 1), r = l + 1;
            if (l >= pw.heapCount) break;
            const int ln = pw.heap[l];
            const float lk = pw.nodes[ln].toTarget;
            int rn = -1; float rk = 3.4e38f;
            if (r < pw.heapCount) { rn = pw.heap[r]; rk = pw.nodes[rn].toTarget; }
            if (lk < rk) {
                if (lk >= key) break;
                pw.heap[i] = ln; pw.nodes[ln].heapIndex = i; i = l;
            } else {
                if (rk >= key) break;
                pw.heap[i] = rn; pw.nodes[rn].heapIndex = i; i = r;
            }
        }
        pw.heap[i] = node;
        pw.nodes[node].heapIndex = i;
    }
    inline void heapAdd(PathWork& pw, int node) {
        pw.heap[pw.heapCount] = node;
        pw.nodes[node].heapIndex = pw.heapCount;
        heapSortBack(pw, pw.heapCount++);
    }
    inline int heapDequeue(PathWork& pw) {
        const int top = pw.heap[0];
        pw.heap[0] = pw.heap[--pw.heapCount];
        if (pw.heapCount > 0) heapSortForward(pw, 0);
        pw.nodes[top].heapIndex = -1;
        return top;
    }
    inline void heapChange(PathWork& pw, int node, float value) {
        const float old = pw.nodes[node].toTarget;
        pw.nodes[node].toTarget = value;
        if (value < old) heapSortBack(pw, pw.nodes[node].heapIndex); else heapSortForward(pw, pw.nodes[node].heapIndex);
    }

    // Pathfinder.getVerticalOffset: 1 libre, 0 sólido, -1 agua, -2 lava, mirando la caja de sx x sy x sz celdas que parte de (x, y, z)
    template <class W>
    inline int verticalOffset(W& w, int x, int y, int z, int sx, int sy, int sz) {
        for (int i = x; i < x + sx; i++)
            for (int j = y; j < y + sy; j++)
                for (int k = z; k < z + sz; k++) {
                    const BlockType t = w.type(i, j, k);
                    if (t != BlockType::AIR) {
                        if (isMaterialSolid(t)) return 0;
                        if (isWaterBlock(t)) return -1;
                        if (isLavaBlock(t)) return -2;
                    }
                }
        return 1;
    }

    // Pathfinder.getSafePoint: el punto, o uno más arriba si 'step' es 1, y después se baja hasta el suelo (máximo 3 de caída; la lava descarta)
    template <class W>
    inline int safePoint(PathWork& pw, W& w, int x, int y, int z, int sx, int sy, int sz, int step) {
        int p = -1;
        if (verticalOffset(w, x, y, z, sx, sy, sz) == 1) p = openPoint(pw, x, y, z);
        if (p == -1 && step > 0 && verticalOffset(w, x, y + step, z, sx, sy, sz) == 1) { p = openPoint(pw, x, y + step, z); y += step; }
        if (p != -1) {
            int fall = 0, below = 0;
            while (y > -1) {
                below = verticalOffset(w, x, y - 1, z, sx, sy, sz);
                if (below != 1) break;
                ++fall;
                if (fall >= 4) return -1;
                --y;
                if (y > -1) p = openPoint(pw, x, y, z);
                if (p == -1) return -1;
            }
            if (below == -2) return -1;
        }
        return p;
    }

    // Pathfinder.findPathOptions: hasta 4 vecinos (y los de un escalón más arriba si hay hueco sobre la cabeza), dentro de maxDist del destino
    template <class W>
    inline int pathOptions(PathWork& pw, W& w, int cur, int sx, int sy, int sz, int target, float maxDist, int out[4]) {
        int n = 0;
        int step = 0;
        const PNode c = pw.nodes[cur];
        if (verticalOffset(w, c.x, c.y + 1, c.z, sx, sy, sz) == 1) step = 1;
        const int nx[4] = { c.x, c.x - 1, c.x + 1, c.x }, nz[4] = { c.z + 1, c.z, c.z, c.z - 1 };
        for (int d = 0; d < 4; d++) {
            const int p = safePoint(pw, w, nx[d], c.y, nz[d], sx, sy, sz, step);
            if (p != -1 && !pw.nodes[p].closed && pointDist(pw.nodes[p], pw.nodes[target]) < maxDist) out[n++] = p;
        }
        return n;
    }

    // Pathfinder.createEntityPathTo + addToPath: A* desde la celda de la oveja hasta (tx, ty, tz). Guarda el camino en la oveja (PathEntity).
    // Si el destino no se alcanza, el camino llega al punto explorado más cercano; devuelve false si ni eso (el original devuelve null).
    template <class W>
    bool findPath(Sheep& s, W& w, int tx, int ty, int tz, float maxDist) {
        PathWork& pw = pathWork();
        pw.nodeCount = 0; pw.heapCount = 0;
        memset(pw.table, 0, sizeof(pw.table));

        const int start = openPoint(pw, (int)floor(s.x - HALF), cellY(s.y + 1.0e-6), (int)floor(s.z - HALF));
        const int target = openPoint(pw, tx, ty, tz);
        if (start < 0 || target < 0) return false;
        const int sx = (int)floorf((float)WIDTH + 1.0f), sy = (int)floorf((float)HEIGHT + 1.0f), sz = sx;   // tamaño de la oveja en celdas: 1 x 2 x 1

        pw.nodes[start].total = 0.0f;
        pw.nodes[start].toNext = pointDist(pw.nodes[start], pw.nodes[target]);
        pw.nodes[start].toTarget = pw.nodes[start].toNext;
        heapAdd(pw, start);
        int best = start;
        int found = -1;

        for (int expansions = 0; pw.heapCount > 0 && expansions < PATH_MAX_EXPANSIONS; expansions++) {
            const int cur = heapDequeue(pw);
            if (cur == target) { found = target; break; }
            if (pointDist(pw.nodes[cur], pw.nodes[target]) < pointDist(pw.nodes[best], pw.nodes[target])) best = cur;
            pw.nodes[cur].closed = true;

            int opts[4];
            const int n = pathOptions(pw, w, cur, sx, sy, sz, target, maxDist, opts);
            for (int i = 0; i < n; i++) {
                const int o = opts[i];
                const float cost = pw.nodes[cur].total + pointDist(pw.nodes[cur], pw.nodes[o]);
                if (pw.nodes[o].heapIndex < 0 || cost < pw.nodes[o].total) {
                    pw.nodes[o].prev = cur;
                    pw.nodes[o].total = cost;
                    pw.nodes[o].toNext = pointDist(pw.nodes[o], pw.nodes[target]);
                    if (pw.nodes[o].heapIndex >= 0) heapChange(pw, o, pw.nodes[o].total + pw.nodes[o].toNext);
                    else { pw.nodes[o].toTarget = pw.nodes[o].total + pw.nodes[o].toNext; heapAdd(pw, o); }
                }
            }
        }

        const int end = found >= 0 ? found : best;
        if (found < 0 && end == start) return false;                       // sin camino
        int chain[PATH_MAX_NODES]; int len = 0;                            // createEntityPath: de atrás hacia adelante
        for (int n = end; n != -1 && len < PATH_MAX_NODES; n = pw.nodes[n].prev) chain[len++] = n;
        int keep = len < Sheep::MAX_PATH ? len : Sheep::MAX_PATH;
        for (int i = 0; i < keep; i++) { const PNode& p = pw.nodes[chain[len - 1 - i]]; s.pathX[i] = p.x; s.pathY[i] = p.y; s.pathZ[i] = p.z; }
        s.pathLen = keep; s.pathIndex = 0;
        return true;
    }

    // ---- EntityCreature.func_31026_E: 10 destinos al azar (x +-6, y +-3, z +-6), gana el de mayor peso, y se calcula el camino hasta él ----
    template <class W, class R>
    inline void pickPath(Sheep& s, W& w, R& rng) {
        bool found = false; float best = -99999.0f; int bi = 0, bj = 0, bk = 0;
        const int feet = cellY(s.y + 1.0e-6);
        for (int n = 0; n < 10; n++) {
            const int i = (int)floor(s.x + (double)rng.nextInt(13) - 6.0), j = feet + rng.nextInt(7) - 3, k = (int)floor(s.z + (double)rng.nextInt(13) - 6.0);
            const float weight = pathWeight(w, i, j, k);
            if (weight > best) { best = weight; bi = i; bj = j; bk = k; found = true; }
        }
        if (found) { s.pathLen = 0; findPath(s, w, bi, bj, bk, 10.0f); }
    }

    // ---- EntityCreature.updatePlayerActionState (sin objetivo, como EntityAnimal) + EntityLiving.updatePlayerActionState ----
    template <class W, class R>
    inline void updateActionState(Sheep& s, W& w, R& rng, const PlayerPos& pl) {
        if ((!s.hasPath() && rng.nextInt(80) == 0) || rng.nextInt(80) == 0) pickPath(s, w, rng);

        const bool water = inWater(s, w), lava = inLava(s, w);
        s.pitch = 0.0f;
        if (s.hasPath() && rng.nextInt(100) != 0) {
            // PathEntity.getPosition + el avance al siguiente punto al estar a menos de 2 x ancho (en horizontal)
            const double radius = WIDTH * 2.0;
            bool have = true;
            double nx = 0.0, nz = 0.0; int ny = 0;
            for (;;) {
                nx = (double)s.pathX[s.pathIndex] + 0.5; nz = (double)s.pathZ[s.pathIndex] + 0.5; ny = s.pathY[s.pathIndex];
                if ((nx - s.x) * (nx - s.x) + (nz - s.z) * (nz - s.z) < radius * radius) {
                    ++s.pathIndex;
                    if (s.pathIndex >= s.pathLen) { s.pathLen = 0; have = false; break; }   // isFinished: sin camino
                } else break;
            }
            s.isJumping = false;
            if (have) {
                const double dx = nx - s.x, dz = nz - s.z;
                const double dy = (double)(ny - ((int)floor(s.y) + 1));                    // celda de los pies: floor(minY + 0,5) del original
                float diff = (float)wrap180(atan2(dz, dx) * 180.0 / PI_D - 90.0 - (double)s.yaw);
                if (diff > 30.0f) diff = 30.0f;
                if (diff < -30.0f) diff = -30.0f;
                s.moveForward = MOVE_SPEED;
                s.yaw += diff;
                if (dy > 0.0) s.isJumping = true;
            }
            if (s.collidedH && !s.hasPath()) s.isJumping = true;
            if (rng.nextFloat() < 0.8f && (water || lava)) s.isJumping = true;
        } else {
            // EntityLiving.updatePlayerActionState: pasear al azar y, a veces, mirar al jugador que está cerca
            s.pathLen = 0;
            s.moveStrafing = 0.0f; s.moveForward = 0.0f;
            if (rng.nextFloat() < 0.02f) {
                const double dx = pl.x - s.x, dz = pl.z - s.z;
                if (pl.valid && dx * dx + dz * dz <= 64.0) { s.lookingAtPlayer = true; s.lookTicks = 10 + rng.nextInt(20); }
                else s.randomYawVelocity = (rng.nextFloat() - 0.5f) * 20.0f;
            }
            if (s.lookingAtPlayer) {
                faceEntity(s, pl, 10.0f, 40.0f);
                const double dx = pl.x - s.x, dz = pl.z - s.z;
                if (s.lookTicks-- <= 0 || !pl.valid || dx * dx + dz * dz > 64.0) s.lookingAtPlayer = false;
            } else {
                if (rng.nextFloat() < 0.05f) s.randomYawVelocity = (rng.nextFloat() - 0.5f) * 20.0f;
                s.yaw += s.randomYawVelocity;
                s.pitch = 0.0f;
            }
            if (water || lava) s.isJumping = rng.nextFloat() < 0.8f;
        }
    }

    // ---- EntityLiving.moveFlying ----
    inline void moveFlying(Sheep& s, float strafe, float forward, float friction) {
        float d = sqrtf(strafe * strafe + forward * forward);
        if (d >= 0.01f) {
            if (d < 1.0f) d = 1.0f;
            d = friction / d;
            strafe *= d; forward *= d;
            const float sn = (float)sin((double)s.yaw * PI_D / 180.0), cs = (float)cos((double)s.yaw * PI_D / 180.0);
            s.motionX += (double)(strafe * cs - forward * sn);
            s.motionZ += (double)(forward * cs + strafe * sn);
        }
    }

    // ---- EntityLiving.moveEntityWithHeading (agua, lava y suelo/aire) ----
    template <class W>
    inline float moveWithHeading(Sheep& s, float strafe, float forward, W& w) {
        float landed = 0.0f;
        const bool water = inWater(s, w), lava = !water && inLava(s, w);
        if (water || lava) {
            const double y0 = s.y;
            moveFlying(s, strafe, forward, 0.02f);
            landed = moveEntity(s, s.motionX, s.motionY, s.motionZ, w);
            const double drag = water ? 0.8 : 0.5;
            s.motionX *= drag; s.motionY *= drag; s.motionZ *= drag;
            s.motionY -= 0.02;
            if (s.collidedH) {   // isOffsetPositionInLiquid: si subida ~0,6 la caja queda libre y sin líquido, se sale del agua de un salto
                Box b = bodyBox(s);
                const double oy = s.motionY + 0.6 - s.y + y0;
                b.x0 += s.motionX; b.x1 += s.motionX; b.y0 += oy; b.y1 += oy; b.z0 += s.motionZ; b.z1 += s.motionZ;
                if (!anyIn(w, b, 0) && !anyIn(w, b, 3)) s.motionY = 0.3;
            }
        } else {
            float friction = 0.546f;                                        // suelo (resbalosidad 0,6 x 0,91)
            if (s.onGround && w.type((int)floor(s.x), cellY(s.y - 0.01), (int)floor(s.z)) == BlockType::ICE) friction = 0.98f * 0.91f;   // hielo: resbalosidad 0,98
            const float k = 0.16277136f / (friction * friction * friction);
            moveFlying(s, strafe, forward, s.onGround ? 0.1f * k : 0.02f);
            landed = moveEntity(s, s.motionX, s.motionY, s.motionZ, w);
            s.motionY -= 0.08;
            s.motionY *= 0.98;
            const double f = s.onGround ? (double)friction : 0.91;
            s.motionX *= f; s.motionZ *= f;
        }
        // field_705_Q / 704_R / 703_S: cuánto y a qué fase se mueven las patas
        s.prevLimbSwingAmount = s.limbSwingAmount;
        const double vx = s.x - s.prevX, vz = s.z - s.prevZ;
        float v = (float)(sqrt(vx * vx + vz * vz) * 4.0);
        if (v > 1.0f) v = 1.0f;
        s.limbSwingAmount += (v - s.limbSwingAmount) * 0.4f;
        s.limbSwing += s.limbSwingAmount;
        return landed;
    }

    // ---- Un tick (20 Hz): Entity.onEntityUpdate + EntityLiving.onEntityUpdate + onLivingUpdate + el cálculo de giros de onUpdate ----
    template <class W, class R>
    void tick(Sheep& s, W& w, R& rng, const PlayerPos& pl) {
        s.prevX = s.x; s.prevY = s.y; s.prevZ = s.z;
        s.prevYaw = s.yaw; s.prevPitch = s.pitch; s.prevBodyYaw = s.bodyYaw;
        ++s.ticksExisted;
        if (s.hs.health > 0 && (rand() % 160) == 0) Sound::mobSay(200, (float)s.x, (float)s.y, (float)s.z);   // sonido generico

        const bool alive = s.hs.health > 0;
        if (inWater(s, w)) { s.fallDistance = 0.0f; s.fire = 0; }           // handleWaterMovement
        if (s.fire > 0) { if (s.fire % 20 == 0) damage(s, 1, false, 0.0, 0.0); --s.fire; }   // Entity.onEntityUpdate: el fuego quita 1 cada segundo
        if (alive && insideOpaque(s, w)) damage(s, 1, false, 0.0, 0.0);
        if (s.hs.health > 0 && headInWater(s, w)) {
            --s.air;
            if (s.air == -20) { s.air = 0; damage(s, 2, false, 0.0, 0.0); }
        } else s.air = 300;
        if (s.hs.hurtTime > 0) --s.hs.hurtTime;
        if (s.hs.heartsLife > 0) --s.hs.heartsLife;
        if (s.hs.health <= 0) {
            ++s.hs.deathTime;
            if (s.hs.deathTime > 20) { s.active = false; return; }          // onEntityDeath + setEntityDead
        }

        // onLivingUpdate
        if (s.hs.health <= 0) {                                             // isMovementBlocked
            s.isJumping = false; s.moveStrafing = 0.0f; s.moveForward = 0.0f; s.randomYawVelocity = 0.0f;
        } else {
            updateActionState(s, w, rng, pl);
        }
        const bool water = inWater(s, w), lava = inLava(s, w);
        if (lava) { damage(s, 4, false, 0.0, 0.0); s.fire = 600; s.fallDistance *= 0.5f; }   // setOnFireFromLava
        if (s.hs.health > 0 && !water && inFire(s, w)) { damage(s, 1, false, 0.0, 0.0); if (s.fire <= 0) s.fire = 300; }   // moveEntity: dentro del fuego se prende
        if (s.isJumping) {
            if (water) s.motionY += 0.04;
            else if (lava) s.motionY += 0.04;
            else if (s.onGround) s.motionY = 0.42;                          // jump()
        }
        s.moveStrafing *= 0.98f; s.moveForward *= 0.98f; s.randomYawVelocity *= 0.9f;
        const float landed = moveWithHeading(s, s.moveStrafing, s.moveForward, w);
        if (landed > 0.0f) { const int d = (int)ceil((double)(landed - 3.0f)); if (d > 0) damage(s, d, false, 0.0, 0.0); }   // EntityLiving.fall

        // EntityLiving.onUpdate: hacia dónde mira el cuerpo
        const double vx = s.x - s.prevX, vz = s.z - s.prevZ;
        const float moved = (float)sqrt(vx * vx + vz * vz);
        float target = s.bodyYaw;
        if (moved > 0.05f) target = (float)(atan2(vz, vx) * 180.0 / PI_D) - 90.0f;
        float d1 = (float)wrap180((double)target - (double)s.bodyYaw);
        s.bodyYaw += d1 * 0.3f;
        float d2 = (float)wrap180((double)s.yaw - (double)s.bodyYaw);
        if (d2 < -75.0f) d2 = -75.0f;
        if (d2 >= 75.0f) d2 = 75.0f;
        s.bodyYaw = s.yaw - d2;
        if (d2 * d2 > 2500.0f) s.bodyYaw += d2 * 0.2f;
        while (s.yaw - s.prevYaw < -180.0f) s.prevYaw -= 360.0f;
        while (s.yaw - s.prevYaw >= 180.0f) s.prevYaw += 360.0f;
        while (s.bodyYaw - s.prevBodyYaw < -180.0f) s.prevBodyYaw -= 360.0f;
        while (s.bodyYaw - s.prevBodyYaw >= 180.0f) s.prevBodyYaw += 360.0f;
        while (s.pitch - s.prevPitch < -180.0f) s.prevPitch -= 360.0f;
        while (s.pitch - s.prevPitch >= 180.0f) s.prevPitch += 360.0f;
    }

    // ---- aparición: una columna con pasto, dos celdas libres encima, en uno de los 9 chunks alrededor del jugador y a 6 bloques o más de él ----
    // pcx, pcz: chunk del jugador. Devuelve false si no encontró sitio en 12 intentos.
    template <class W, class R>
    bool findSpawn(W& w, R& rng, int pcx, int pcz, double px, double pz, double& ox, double& oy, double& oz) {
        for (int attempt = 0; attempt < 30; attempt++) {
            const int cx = pcx + rng.nextInt(5) - 2, cz = pcz + rng.nextInt(5) - 2;
            if (!w.chunkReady(cx, cz)) continue;
            const int i = cx * 8 + rng.nextInt(8) - 4, k = cz * 8 + rng.nextInt(8) - 4;   // celda del mundo: x = (celda de rejilla) - 4
            int ground = -9999;
            for (int j = 125; j >= 0; j--) {
                const BlockType t = w.type(i, j, k);
                if (isMaterialSolid(t) || isLiquidBlock(t)) { ground = j; break; }
            }
            if (ground == -9999 || w.type(i, ground, k) != BlockType::GRASS) continue;
            if (isMaterialSolid(w.type(i, ground + 1, k)) || isMaterialSolid(w.type(i, ground + 2, k))) continue;
            if (isLiquidBlock(w.type(i, ground + 1, k))) continue;
            if (w.light(i, ground + 1, k) < 0.30f) continue;     // Beta: las ovejas necesitan luz 9 o mas (de noche no aparecen)
            const double x = (double)i + 0.5, z = (double)k + 0.5;
            if ((x - px) * (x - px) + (z - pz) * (z - pz) < 196.0) continue;
            ox = x; oy = (double)ground + 0.5; oz = z;
            return true;
        }
        return false;
    }

    // ---- desaparición: fuera de los 9 chunks del jugador, o en un chunk que ya no está listo, se borra al instante ----
    inline int chunkOf(double v) { return (int)floor((v + 4.0) / 8.0); }
    template <class W>
    inline bool shouldDespawn(const Sheep& s, W& w, int pcx, int pcz) {
        const int cx = chunkOf(s.x), cz = chunkOf(s.z);
        if (abs(cx - pcx) > 3 || abs(cz - pcz) > 3) return true;
        return !w.chunkReady(cx, cz);
    }

    // ---- Minecraft.getMouseOver: el rayo contra la caja de la oveja agrandada 0,1 (Entity.getCollisionBorderSize). Devuelve la distancia o -1 ----
    inline double rayHit(const Sheep& s, double ox, double oy, double oz, double dx, double dy, double dz, double maxDist) {
        const double b = 0.1;
        const double lo[3] = { s.x - HALF - b, s.y - b, s.z - HALF - b }, hi[3] = { s.x + HALF + b, s.y + HEIGHT + b, s.z + HALF + b };
        const double o[3] = { ox, oy, oz }, d[3] = { dx, dy, dz };
        double tmin = 0.0, tmax = maxDist;
        for (int a = 0; a < 3; a++) {
            if (fabs(d[a]) < 1.0e-12) { if (o[a] < lo[a] || o[a] > hi[a]) return -1.0; }
            else {
                double t1 = (lo[a] - o[a]) / d[a], t2 = (hi[a] - o[a]) / d[a];
                if (t1 > t2) { const double t = t1; t1 = t2; t2 = t; }
                if (t1 > tmin) tmin = t1;
                if (t2 < tmax) tmax = t2;
                if (tmin > tmax) return -1.0;
            }
        }
        return tmin;
    }

    // bloque de la celda (centro bx, by, bz del mundo) contra la oveja: para no poder colocar un bloque dentro de ella
    inline bool overlapsBlock(const Sheep& s, double bx, double by, double bz) {
        return s.x - HALF < bx + 0.5 && s.x + HALF > bx - 0.5 && s.y < by + 0.5 && s.y + HEIGHT > by - 0.5 && s.z - HALF < bz + 0.5 && s.z + HALF > bz - 0.5;
    }
}

#endif