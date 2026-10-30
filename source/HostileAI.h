#ifndef HOSTILEAI_H
#define HOSTILEAI_H

#include <math.h>
#include "AnimalAI.h"
#include "HostileData.h"

// =====================================================================
// IA y combate de los mobs con IA propia: zombie, esqueleto, araña, creeper, zombi cerdo, slime, ghast, lobo y calamar. Port de EntityMob / EntityCreature /
// EntityZombie / EntitySkeleton / EntitySpider / EntityCreeper / EntityPigZombie / EntitySlime / EntityGhast (EntityFlying) / EntityWolf / EntitySquid
// y de EntityArrow y EntityFireball (Beta 1.7.3). Codigo puro, sin libnds. La fisica, el camino (A*) y el daño son los de AnimalAI; el mundo entra como
// parametro de plantilla W, con lo de AnimalAI mas:
//     int rawSky(i, j, k)         luz del cielo guardada (0..15), sin restar la noche (World.getSavedLightValue)
//     int totalLight(i, j, k)     World.getBlockLightValue: max(luz de bloque, cielo - skylightSubtracted)
//     int blockLight(i, j, k)     solo la luz de bloque (el Nether no tiene cielo)
//     bool lineClear(x0,y0,z0, x1,y1,z1)   sin bloques en el medio (rayTraceBlocks == null)
//     bool canSeeSky(i, j, k)     World.canBlockSeeTheSky
//     bool isDay()                World.isDaytime (skylightSubtracted < 4)
// y un Sink S con los efectos:
//     void playerHit(int amount, double srcX, double srcZ)
//     void addArrow(const Arrow& a)
//     void addFireball(const Fireball& f)
//     void explode(Mob& m, float size)                           el creeper
//     void explodeAt(double x, double y, double z, float size, bool flaming)
//     void splitSlime(const Mob& m)                              al morir un slime grande: 4 mas chicos
//     bool mobHit(x0,y0,z0, x1,y1,z1)                           ¿el segmento toca a algun otro mob? (la bola de fuego)
//     int heldKind()                                             lo que lleva el jugador en la mano: 0 nada, 1 hueso, 2 carne (el lobo inclina la cabeza)
// =====================================================================
namespace HostileAI {
    using AnimalAI::PlayerPos;
    using AnimalAI::Rng;

    const double PI_D = AnimalAI::PI_D;

    const float WOLF_BITE_RANGE = 2.0f;

    inline bool isMobKind(int k) { return k <= H_PIGZOMBIE; }   // EntityMob: zombie, esqueleto, araña, creeper y zombi cerdo

    inline void setup(Mob& m, int kind, int slimeSize = 1) {
        m = Mob();
        m.hkind = kind; m.kind = 100 + kind;
        m.width = 0.6; m.height = 1.8;
        m.moveSpeed = 0.7f; m.attackStrength = 2;
        if (kind == H_ZOMBIE) { m.moveSpeed = 0.5f; m.attackStrength = 5; }
        else if (kind == H_PIGZOMBIE) { m.moveSpeed = 0.5f; m.attackStrength = 5; }
        else if (kind == H_SPIDER) { m.width = 1.4; m.height = 0.9; m.moveSpeed = 0.8f; }
        else if (kind == H_SLIME) {
            m.slimeSize = slimeSize;
            m.width = m.height = 0.6 * (double)slimeSize;
            m.hs.health = m.hs.prevHealth = slimeSize * slimeSize;
        }
        else if (kind == H_GHAST) { m.width = m.height = 4.0; m.hs.health = m.hs.prevHealth = 10; }
        else if (kind == H_WOLF) { m.width = m.height = 0.8; m.moveSpeed = 1.1f; m.hs.health = m.hs.prevHealth = 8; }
        else if (kind == H_SQUID) { m.width = m.height = 0.95; m.hs.health = m.hs.prevHealth = 10; }
    }

    template <class R> inline double gaussian(R& rng) {   // Random.nextGaussian (Box-Muller)
        double u1 = (double)rng.nextFloat(); if (u1 < 1.0e-6) u1 = 1.0e-6;
        const double u2 = (double)rng.nextFloat();
        return sqrt(-2.0 * log(u1)) * cos(2.0 * PI_D * u2);
    }

    // java.util.Random (para el slime: la semilla de cada chunk de 16 x 16 y la longitud de los tentaculos del ghast)
    struct JavaRandom {
        unsigned long long seed;
        explicit JavaRandom(long long s) { seed = ((unsigned long long)s ^ 0x5DEECE66DULL) & ((1ULL << 48) - 1ULL); }
        int next(int bits) { seed = (seed * 0x5DEECE66DULL + 0xBULL) & ((1ULL << 48) - 1ULL); return (int)(seed >> (48 - bits)); }
        int nextInt(int n) {
            if ((n & -n) == n) return (int)(((long long)n * (long long)next(31)) >> 31);
            int bits, val;
            do { bits = next(31); val = bits % n; } while (bits - val + (n - 1) < 0);
            return val;
        }
    };

    // Chunk.func_997_a(987234911L).nextInt(10) == 0: los "chunks de slime" (16 x 16). cx, cz: coordenadas de chunk de la Beta (floor(x / 16))
    inline bool isSlimeChunk(long long worldSeed, int cx, int cz) {
        const long long a = (long long)(int)((unsigned)cx * (unsigned)cx * 4987142u);
        const long long b = (long long)(int)((unsigned)cx * 5947611u);
        const long long c = (long long)(cz * cz) * 4392871LL;
        const long long d = (long long)(int)((unsigned)cz * 389711u);
        JavaRandom r((worldSeed + a + b + c + d) ^ 987234911LL);
        return r.nextInt(10) == 0;
    }

    inline double dist3(const Mob& s, const PlayerPos& pl) {
        const double dx = pl.x - s.x, dy = pl.y - s.y, dz = pl.z - s.z;
        return sqrt(dx * dx + dy * dy + dz * dz);
    }

    // EntityLiving.getEntityBrightness: el brillo (0..1) a 0,66 de la altura del cuerpo
    template <class W> inline double brightnessAt(const Mob& s, W& w) {
        return (double)w.light((int)floor(s.x), AnimalAI::cellY(s.y + s.height * 0.66), (int)floor(s.z));
    }

    // EntityLiving.canEntityBeSeen: sin bloques entre los pies del mob (getEyeHeight = 0 en la 1.7.3) y los ojos del jugador (+ 0,12)
    template <class W> inline bool canSee(const Mob& s, W& w, const PlayerPos& pl) {
        return w.lineClear(s.x, s.y, s.z, pl.x, pl.y + 0.12, pl.z);
    }

    // ---- findPlayerToAttack de cada tipo ----
    template <class W>
    inline bool findTarget(const Mob& s, W& w, const PlayerPos& pl) {
        if (!pl.valid) return false;
        const double dx = pl.x - s.x, dy = pl.y - s.y, dz = pl.z - s.z;
        if (dx * dx + dy * dy + dz * dz > 256.0) return false;            // getClosestPlayerToEntity(this, 16.0)
        if (s.hkind == H_WOLF) return s.angry;                            // EntityWolf.findPlayerToAttack: solo si esta enojado
        if (s.hkind == H_SPIDER) return brightnessAt(s, w) < 0.5;         // la araña solo ataca con poca luz y no necesita verlo
        if (s.hkind == H_PIGZOMBIE && s.anger == 0) return false;
        return canSee(s, w, pl);
    }

    // ---- EntityArrow.setArrowHeading ----
    template <class R>
    inline void setHeading(Arrow& a, double dx, double dy, double dz, float speed, float spread, R& rng) {
        double len = sqrt(dx * dx + dy * dy + dz * dz);
        if (len < 1.0e-9) len = 1.0;
        dx /= len; dy /= len; dz /= len;
        dx += gaussian(rng) * 0.0075 * (double)spread;
        dy += gaussian(rng) * 0.0075 * (double)spread;
        dz += gaussian(rng) * 0.0075 * (double)spread;
        dx *= (double)speed; dy *= (double)speed; dz *= (double)speed;
        a.mx = dx; a.my = dy; a.mz = dz;
        const double h = sqrt(dx * dx + dz * dz);
        a.yaw = a.prevYaw = (float)(atan2(dx, dz) * 180.0 / PI_D);
        a.pitch = a.prevPitch = (float)(atan2(dy, h) * 180.0 / PI_D);
        a.ticksInGround = 0;
    }

    // ---- EntityMob.attackEntity: un golpe cada 20 ticks si esta a menos de 2 y se tocan en altura ----
    template <class S>
    inline void meleeAttack(Mob& s, const PlayerPos& pl, float dist, S& sink) {
        const double pMinY = pl.y - 1.62, pMaxY = pl.y + 0.18;   // caja del jugador (los ojos estan a 1,62 de los pies; mide 1,8)
        if (s.attackTime <= 0 && dist < 2.0f && pMaxY > s.y && pMinY < s.y + s.height) {
            s.attackTime = 20;
            sink.playerHit(s.attackStrength, s.x, s.z);
        }
    }

    // ---- attackEntity de cada tipo (el objetivo esta a la vista) ----
    template <class W, class R, class S>
    inline void attackEntity(Mob& s, W& w, R& rng, const PlayerPos& pl, float dist, S& sink) {
        switch (s.hkind) {
        case H_SKELETON: {
            if (dist < 10.0f) {
                const double dx = pl.x - s.x, dz = pl.z - s.z;
                if (s.attackTime == 0) {
                    Arrow a;
                    a.active = true;
                    const double yr = (double)s.yaw * PI_D / 180.0;
                    a.x = s.x - cos(yr) * 0.16;
                    a.y = s.y - 0.1 + 1.4;                                  // EntityArrow(world, owner) y luego posY += 1,4
                    a.z = s.z - sin(yr) * 0.16;
                    a.prevX = a.x; a.prevY = a.y; a.prevZ = a.z;
                    a.ownerX = s.x; a.ownerZ = s.z;
                    const double dy = pl.y + 0.12 - 0.2 - a.y;              // los ojos del jugador menos 0,2
                    const double flat = sqrt(dx * dx + dz * dz) * 0.2;      // sube un poco segun la distancia
                    setHeading(a, dx, dy + flat, dz, 0.6f, 12.0f, rng);
                    sink.addArrow(a);
                    s.attackTime = 30;
                }
                s.yaw = (float)(atan2(dz, dx) * 180.0 / PI_D) - 90.0f;
                s.hasAttacked = true;
            }
            break;
        }
        case H_SPIDER: {
            const double b = brightnessAt(s, w);
            if (b > 0.5 && rng.nextInt(100) == 0) {
                s.hasTarget = false; s.pathLen = 0;                         // con luz se olvida del jugador
            } else if (dist > 2.0f && dist < 6.0f && rng.nextInt(10) == 0) {
                if (s.onGround) {                                           // salto sobre el jugador
                    const double dx = pl.x - s.x, dz = pl.z - s.z;
                    const double h = sqrt(dx * dx + dz * dz);
                    if (h > 1.0e-6) {
                        s.motionX = dx / h * 0.5 * 0.8 + s.motionX * 0.2;
                        s.motionZ = dz / h * 0.5 * 0.8 + s.motionZ * 0.2;
                    }
                    s.motionY = 0.4;
                }
            } else {
                meleeAttack(s, pl, dist, sink);
            }
            break;
        }
        case H_CREEPER: {
            const int st = s.creeperState;
            if ((st <= 0 && dist < 3.0f) || (st > 0 && dist < 7.0f)) {
                s.creeperState = 1;
                if (s.ignite == 0) Sound::fuse((float)s.x, (float)s.y, (float)s.z);   // siseo al encenderse
                ++s.ignite;
                if (s.ignite >= 30) {
                    sink.explode(s, 3.0f);
                    s.active = false;                                       // setEntityDead
                    return;
                }
                s.hasAttacked = true;
            } else {
                s.creeperState = -1;
                --s.ignite;
                if (s.ignite < 0) s.ignite = 0;
            }
            break;
        }
        case H_WOLF: {   // EntityWolf.attackEntity: salta sobre el objetivo a 2 a 6 de distancia o muerde a menos de 1,5 (sin pausa propia: manda la invulnerabilidad)
            if (dist > 2.0f && dist < 6.0f && rng.nextInt(10) == 0) {
                if (s.onGround) {
                    const double dx = pl.x - s.x, dz = pl.z - s.z;
                    const double h = sqrt(dx * dx + dz * dz);
                    if (h > 1.0e-6) {
                        s.motionX = dx / h * 0.5 * 0.8 + s.motionX * 0.2;
                        s.motionZ = dz / h * 0.5 * 0.8 + s.motionZ * 0.2;
                    }
                    s.motionY = 0.4;
                }
            } else if (s.attackTime <= 0 && dist < WOLF_BITE_RANGE && (pl.y + 0.18) > s.y && (pl.y - 1.62) < s.y + s.height) {   // la Beta usa 1,5 contra la distancia a los OJOS del jugador (>= 1,62): nunca muerde; de la 1.2.5 en adelante es 2,0
                s.attackTime = 10;   // (la Beta muerde en cada tick y manda la invulnerabilidad del jugador: 10 ticks)
                sink.playerHit(s.tamed ? 4 : 2, s.x, s.z);
            }
            break;
        }
        default:
            meleeAttack(s, pl, dist, sink);
            break;
        }
    }

    // ---- attackBlockedEntity: solo el creeper (apaga la mecha) ----
    inline void attackBlocked(Mob& s) {
        if (s.hkind != H_CREEPER) return;
        if (s.ignite > 0) {
            s.creeperState = -1;
            --s.ignite;
            if (s.ignite < 0) s.ignite = 0;
        }
    }

    // ---- EntityCreature.func_31026_E: 10 destinos al azar; los hostiles buscan lugares oscuros (EntityMob.getBlockPathWeight = 0,5 - brillo) y el lobo
    // el pasto (EntityAnimal.getBlockPathWeight) ----
    template <class W, class R>
    inline void pickPath(Mob& s, W& w, R& rng) {
        bool found = false; float best = -99999.0f; int bi = 0, bj = 0, bk = 0;
        const int feet = AnimalAI::cellY(s.y + 1.0e-6);
        for (int n = 0; n < 10; n++) {
            const int i = (int)floor(s.x + (double)rng.nextInt(13) - 6.0), j = feet + rng.nextInt(7) - 3, k = (int)floor(s.z + (double)rng.nextInt(13) - 6.0);
            const float weight = s.hkind == H_WOLF ? AnimalAI::pathWeight(w, i, j, k) : 0.5f - w.light(i, j, k);
            if (weight > best) { best = weight; bi = i; bj = j; bk = k; found = true; }
        }
        if (found) AnimalAI::findPath(s, w, bi, bj, bk, 10.0f);
    }

    // getPathToEntity(this, jugador, 16): camino hasta la celda de los pies del jugador
    template <class W>
    inline void pathToPlayer(Mob& s, W& w, const PlayerPos& pl) {
        AnimalAI::findPath(s, w, (int)floor(pl.x), AnimalAI::cellY(pl.y - 1.62 + 1.0e-6), (int)floor(pl.z), 16.0f);
    }

    // EntityWolf.getPathOrWalkableBlock: camino hasta el dueño; si no hay y esta a mas de 12, aparece en una celda libre del anillo de 5 x 5 que lo rodea
    template <class W>
    inline void wolfToOwner(Mob& s, W& w, const PlayerPos& pl, float dist) {
        const int ti = (int)floor(pl.x), tk = (int)floor(pl.z), tj = AnimalAI::cellY(pl.y - 1.62 + 1.0e-6);
        s.pathLen = 0;
        if (AnimalAI::findPath(s, w, ti, tj, tk, 16.0f)) return;
        if (dist > 12.0f) {
            for (int a = 0; a <= 4; a++) for (int b = 0; b <= 4; b++) {
                if (a >= 1 && b >= 1 && a <= 3 && b <= 3) continue;
                const int i = ti - 2 + a, k = tk - 2 + b;
                if (isNormalCube(w.type(i, tj - 1, k)) && !isNormalCube(w.type(i, tj, k)) && !isNormalCube(w.type(i, tj + 1, k))) {
                    s.x = (double)i + 0.5; s.y = (double)tj - 0.5; s.z = (double)k + 0.5;
                    s.motionX = s.motionY = s.motionZ = 0.0; s.pathLen = 0;
                    return;
                }
            }
        }
    }

    // ---- EntityCreature.updatePlayerActionState (con objetivo) + EntityLiving.updatePlayerActionState ----
    template <class W, class R, class S>
    inline void updateActionState(Mob& s, W& w, R& rng, const PlayerPos& pl, S& sink) {
        s.hasAttacked = (s.hkind == H_WOLF && s.sitting);                   // isMovementCeased(): el lobo sentado no se mueve
        if (!s.hasTarget) {
            if (findTarget(s, w, pl)) { s.hasTarget = true; s.pathLen = 0; pathToPlayer(s, w, pl); }
        } else if (!pl.valid) {
            s.hasTarget = false;                                            // el jugador ya no esta vivo
        } else {
            const float dist = (float)dist3(s, pl);
            if (canSee(s, w, pl)) attackEntity(s, w, rng, pl, dist, sink);
            else attackBlocked(s);
            if (!s.active) return;
        }

        if (s.hasAttacked || !s.hasTarget || (s.hasPath() && rng.nextInt(20) != 0)) {
            if (!s.hasAttacked && ((!s.hasPath() && rng.nextInt(80) == 0) || rng.nextInt(80) == 0)) pickPath(s, w, rng);
        } else {
            s.pathLen = 0;
            pathToPlayer(s, w, pl);
        }

        const bool water = AnimalAI::inWater(s, w), lava = AnimalAI::inLava(s, w);
        s.pitch = 0.0f;
        if (s.hasPath() && rng.nextInt(100) != 0) {
            const double radius = s.width * 2.0;
            bool have = true;
            double nx = 0.0, nz = 0.0; int ny = 0;
            for (;;) {
                nx = (double)s.pathX[s.pathIndex] + 0.5; nz = (double)s.pathZ[s.pathIndex] + 0.5; ny = s.pathY[s.pathIndex];
                if ((nx - s.x) * (nx - s.x) + (nz - s.z) * (nz - s.z) < radius * radius) {
                    ++s.pathIndex;
                    if (s.pathIndex >= s.pathLen) { s.pathLen = 0; have = false; break; }
                } else break;
            }
            s.isJumping = false;
            if (have) {
                const double dx = nx - s.x, dz = nz - s.z;
                const double dy = (double)(ny - ((int)floor(s.y) + 1));
                float diff = (float)AnimalAI::wrap180(atan2(dz, dx) * 180.0 / PI_D - 90.0 - (double)s.yaw);
                if (diff > 30.0f) diff = 30.0f;
                if (diff < -30.0f) diff = -30.0f;
                s.moveForward = s.moveSpeed;
                s.yaw += diff;
                if (s.hasAttacked && s.hasTarget) {                         // se mueve de lado mirando al objetivo
                    const double tx = pl.x - s.x, tz = pl.z - s.z;
                    const float old = s.yaw;
                    s.yaw = (float)(atan2(tz, tx) * 180.0 / PI_D) - 90.0f;
                    const float a = (old - s.yaw + 90.0f) * 3.14159265f / 180.0f;
                    s.moveStrafing = -sinf(a) * s.moveForward;
                    s.moveForward = cosf(a) * s.moveForward;
                }
                if (dy > 0.0) s.isJumping = true;
            }
            if (s.hasTarget) AnimalAI::faceEntity(s, pl, 30.0f, 30.0f);
            if (s.collidedH && !s.hasPath()) s.isJumping = true;
            if (rng.nextFloat() < 0.8f && (water || lava)) s.isJumping = true;
        } else {
            // EntityLiving.updatePlayerActionState: pasear al azar y, a veces, mirar al jugador que esta cerca
            s.pathLen = 0;
            s.moveStrafing = 0.0f; s.moveForward = 0.0f;
            if (rng.nextFloat() < 0.02f) {
                const double dx = pl.x - s.x, dz = pl.z - s.z;
                if (pl.valid && dx * dx + dz * dz <= 64.0) { s.lookingAtPlayer = true; s.lookTicks = 10 + rng.nextInt(20); }
                else s.randomYawVelocity = (rng.nextFloat() - 0.5f) * 20.0f;
            }
            if (s.lookingAtPlayer) {
                AnimalAI::faceEntity(s, pl, 10.0f, s.hkind == H_WOLF && s.sitting ? 20.0f : 40.0f);
                const double dx = pl.x - s.x, dz = pl.z - s.z;
                if (s.lookTicks-- <= 0 || !pl.valid || dx * dx + dz * dz > 64.0) s.lookingAtPlayer = false;
            } else {
                if (rng.nextFloat() < 0.05f) s.randomYawVelocity = (rng.nextFloat() - 0.5f) * 20.0f;
                s.yaw += s.randomYawVelocity;
                s.pitch = 0.0f;
            }
            if (water || lava) s.isJumping = rng.nextFloat() < 0.8f;
        }

        if (s.hkind == H_WOLF) {                                            // EntityWolf.updatePlayerActionState
            if (!s.hasAttacked && !s.hasPath() && s.tamed) {                // el domado sigue a su dueño a mas de 5 bloques
                if (pl.valid) {
                    const float d = (float)dist3(s, pl);
                    if (d > 5.0f) wolfToOwner(s, w, pl, d);
                } else if (!water) s.sitting = true;
            }
            if (water) s.sitting = false;
        }
    }

    // ---- EntitySlime.updatePlayerActionState ----
    template <class W, class R>
    inline void updateSlime(Mob& s, W& w, R& rng, const PlayerPos& pl) {
        const bool near = pl.valid && dist3(s, pl) < 16.0;                  // getClosestPlayerToEntity(this, 16.0)
        if (near) AnimalAI::faceEntity(s, pl, 10.0f, 20.0f);
        if (s.onGround && s.slimeJump-- <= 0) {
            s.slimeJump = rng.nextInt(20) + 10;
            if (near) s.slimeJump /= 3;
            s.isJumping = true;
            s.squish = 1.0f;
            s.moveStrafing = 1.0f - rng.nextFloat() * 2.0f;
            s.moveForward = (float)s.slimeSize;
        } else {
            s.isJumping = false;
            if (s.onGround) { s.moveStrafing = 0.0f; s.moveForward = 0.0f; }
        }
        (void)w;
    }

    // ---- EntityGhast.isCourseTraversable: la caja (de 4 x 4 x 4) avanza hacia el punto y no puede chocar con bloques ----
    template <class W>
    inline bool ghastCourseClear(const Mob& s, W& w, double dist) {
        const double ux = (s.wayX - s.x) / dist, uy = (s.wayY - s.y) / dist, uz = (s.wayZ - s.z) / dist;
        AnimalAI::Box b = AnimalAI::bodyBox(s);
        const int step = dist > 24.0 ? 2 : 1;                               // (el original avanza de a 1; de a 2 en los trayectos largos para ahorrar)
        for (int n = 1; (double)n < dist; n += step) {
            const double k = (double)step;
            b.x0 += ux * k; b.x1 += ux * k; b.y0 += uy * k; b.y1 += uy * k; b.z0 += uz * k; b.z1 += uz * k;
            if (AnimalAI::anyIn(w, b, 0)) return false;
        }
        return true;
    }

    // ---- EntityGhast.updatePlayerActionState ----
    template <class W, class R, class S>
    inline void updateGhast(Mob& s, W& w, R& rng, const PlayerPos& pl, S& sink) {
        s.prevAttackCounter = s.attackCounter;
        double dx = s.wayX - s.x, dy = s.wayY - s.y, dz = s.wayZ - s.z;
        double dist = sqrt(dx * dx + dy * dy + dz * dz);
        if (dist < 1.0 || dist > 60.0) {
            s.wayX = s.x + (double)((rng.nextFloat() * 2.0f - 1.0f) * 16.0f);
            s.wayY = s.y + (double)((rng.nextFloat() * 2.0f - 1.0f) * 16.0f);
            s.wayZ = s.z + (double)((rng.nextFloat() * 2.0f - 1.0f) * 16.0f);
        }
        if (s.courseCooldown-- <= 0) {
            s.courseCooldown += rng.nextInt(5) + 2;
            if (dist > 1.0e-6 && ghastCourseClear(s, w, dist)) {
                s.motionX += dx / dist * 0.1; s.motionY += dy / dist * 0.1; s.motionZ += dz / dist * 0.1;
            } else {
                s.wayX = s.x; s.wayY = s.y; s.wayZ = s.z;
            }
        }
        if (!pl.valid) s.hasTarget = false;
        if (!s.hasTarget || s.aggro-- <= 0) {
            s.hasTarget = pl.valid && dist3(s, pl) < 100.0;                 // getClosestPlayerToEntity(this, 100.0)
            if (s.hasTarget) s.aggro = 20;
        }
        const double range = 64.0;
        if (s.hasTarget && dist3(s, pl) < range) {
            const double tx = pl.x - s.x;
            const double ty = (pl.y - 1.62 + 0.9) - (s.y + s.height * 0.5);
            const double tz = pl.z - s.z;
            s.yaw = s.bodyYaw = -(float)(atan2(tx, tz) * 180.0 / PI_D);
            if (w.lineClear(s.x, s.y + s.height * 0.85, s.z, pl.x, pl.y + 0.12, pl.z)) {
                ++s.attackCounter;
                if (s.attackCounter == 20) {
                    Fireball f;
                    f.active = true;
                    const double yr = (double)s.yaw * PI_D / 180.0;
                    f.x = s.x - sin(yr) * 4.0;                               // getLook(1.0): (-sin(yaw), 0, cos(yaw))
                    f.y = s.y + s.height * 0.5 + 0.5;
                    f.z = s.z + cos(yr) * 4.0;
                    f.prevX = f.x; f.prevY = f.y; f.prevZ = f.z;
                    double fx = tx + gaussian(rng) * 0.4, fy = ty + gaussian(rng) * 0.4, fz = tz + gaussian(rng) * 0.4;
                    double fl = sqrt(fx * fx + fy * fy + fz * fz);
                    if (fl < 1.0e-9) fl = 1.0;
                    f.ax = fx / fl * 0.1; f.ay = fy / fl * 0.1; f.az = fz / fl * 0.1;
                    sink.addFireball(f);
                    s.attackCounter = -40;
                }
            } else if (s.attackCounter > 0) {
                --s.attackCounter;
            }
        } else {
            s.yaw = s.bodyYaw = -(float)(atan2(s.motionX, s.motionZ) * 180.0 / PI_D);
            if (s.attackCounter > 0) --s.attackCounter;
        }
    }

    // ---- EntitySquid.isInWater: agua en la celda que cubre el centro del calamar ----
    template <class W>
    inline bool squidInWater(const Mob& s, W& w) {
        const int j = (int)floor(s.y + 1.1);
        const int i0 = (int)floor(s.x - s.width * 0.5), i1 = (int)floor(s.x + s.width * 0.5);
        const int k0 = (int)floor(s.z - s.width * 0.5), k1 = (int)floor(s.z + s.width * 0.5);
        for (int i = i0; i <= i1; i++) for (int k = k0; k <= k1; k++) if (isWaterBlock(w.type(i, j, k))) return true;
        return false;
    }

    // ---- EntitySquid.updatePlayerActionState + onLivingUpdate (despues de mover) ----
    template <class R>
    inline void squidPickVector(Mob& s, R& rng, bool inWater) {
        if (rng.nextInt(50) == 0 || !inWater || (s.sqVx == 0.0f && s.sqVy == 0.0f && s.sqVz == 0.0f)) {
            const float a = rng.nextFloat() * 3.14159265f * 2.0f;
            s.sqVx = cosf(a) * 0.2f;
            s.sqVy = -0.1f + rng.nextFloat() * 0.2f;
            s.sqVz = sinf(a) * 0.2f;
        }
    }

    template <class W, class R>
    inline void squidAnimate(Mob& s, W& w, R& rng) {
        const float TWO_PI = 3.14159265f * 2.0f, PIF = 3.14159265f;
        s.sqPrevPitch = s.sqPitch; s.sqPrevRoll = s.sqRoll; s.sqPrevTent = s.sqTent;
        s.sqPhase += s.sqStep;
        if (s.sqPhase > TWO_PI) {
            s.sqPhase -= TWO_PI;
            if (rng.nextInt(10) == 0) s.sqStep = 1.0f / (rng.nextFloat() + 1.0f) * 0.2f;
        }
        if (squidInWater(s, w)) {
            if (s.sqPhase < PIF) {
                const float f = s.sqPhase / PIF;
                s.sqTent = sinf(f * f * PIF) * PIF * 0.25f;
                if ((double)f > 0.75) { s.sqSpeed = 1.0f; s.sqBoost = 1.0f; }
                else s.sqBoost *= 0.8f;
            } else {
                s.sqTent = 0.0f;
                s.sqSpeed *= 0.9f;
                s.sqBoost *= 0.99f;
            }
            s.motionX = (double)(s.sqVx * s.sqSpeed);
            s.motionY = (double)(s.sqVy * s.sqSpeed);
            s.motionZ = (double)(s.sqVz * s.sqSpeed);
            const float h = (float)sqrt(s.motionX * s.motionX + s.motionZ * s.motionZ);
            s.bodyYaw += (-(float)(atan2(s.motionX, s.motionZ)) * 180.0f / PIF - s.bodyYaw) * 0.1f;
            s.yaw = s.bodyYaw;
            s.sqRoll += PIF * s.sqBoost * 1.5f;
            s.sqPitch += (-(float)(atan2((double)h, s.motionY)) * 180.0f / PIF - s.sqPitch) * 0.1f;
        } else {
            s.sqTent = fabsf(sinf(s.sqPhase)) * PIF * 0.25f;
            s.motionX = 0.0;
            s.motionY -= 0.08;
            s.motionY *= 0.98;
            s.motionZ = 0.0;
            s.sqPitch = (float)((double)s.sqPitch + (double)(-90.0f - s.sqPitch) * 0.02);
        }
    }

    // ---- Un tick (20 Hz) de un mob ----
    template <class W, class R, class S>
    void tick(Mob& s, W& w, R& rng, const PlayerPos& pl, S& sink) {
        s.prevX = s.x; s.prevY = s.y; s.prevZ = s.z;
        s.prevYaw = s.yaw; s.prevPitch = s.pitch; s.prevBodyYaw = s.bodyYaw;
        ++s.ticksExisted;
        if (s.hs.health > 0 && (rand() % 160) == 0) Sound::mobSay(s.kind, (float)s.x, (float)s.y, (float)s.z);   // sonido generico
        s.lastIgnite = s.ignite;                                            // EntityCreeper.onUpdate: lastActiveTime
        if (s.hkind == H_PIGZOMBIE) s.moveSpeed = s.hasTarget ? 0.95f : 0.5f;
        const bool wasGround = s.onGround;                                  // EntitySlime.onUpdate: para saber cuando cae
        s.prevSquish = s.squish;
        s.prevInterest = s.interest;

        const bool alive = s.hs.health > 0;
        const bool swimmer = s.hkind == H_SQUID;                            // canBreatheUnderwater
        if (AnimalAI::inWater(s, w)) { s.fallDistance = 0.0f; s.fire = 0; } // handleWaterMovement
        if (s.hkind == H_GHAST) s.fire = 0;                                 // isImmuneToFire
        if (s.fire > 0) {                                                   // Entity.onEntityUpdate: el fuego quita 1 cada segundo
            if (s.hkind == H_PIGZOMBIE) s.fire -= 4;
            else { if (s.fire % 20 == 0) AnimalAI::damage(s, 1, false, 0.0, 0.0); --s.fire; }
        }
        if (alive && AnimalAI::insideOpaque(s, w)) AnimalAI::damage(s, 1, false, 0.0, 0.0);
        if (!swimmer && s.hs.health > 0 && AnimalAI::headInWater(s, w)) {
            --s.air;
            if (s.air == -20) { s.air = 0; AnimalAI::damage(s, 2, false, 0.0, 0.0); }
        } else if (swimmer && s.hs.health > 0 && !squidInWater(s, w)) {
            --s.air;                                                        // EntityWaterMob.onEntityUpdate: fuera del agua se ahoga
            if (s.air == -20) { s.air = 0; AnimalAI::damage(s, 2, false, 0.0, 0.0); }
        } else s.air = 300;
        if (s.hs.hurtTime > 0) --s.hs.hurtTime;
        if (s.hs.heartsLife > 0) --s.hs.heartsLife;
        if (s.hs.health <= 0) {
            ++s.hs.deathTime;
            if (s.hs.deathTime > 20) {
                if (s.hkind == H_SLIME && s.slimeSize > 1) sink.splitSlime(s);   // EntitySlime.setEntityDead: 4 mas chicos
                s.active = false; return;
            }
        }
        if (s.attackTime > 0) --s.attackTime;

        // EntityLiving.despawnEntity (los chunks lejanos los borra el administrador); el lobo domado no desaparece
        ++s.entityAge;
        if (pl.valid && !(s.hkind == H_WOLF && s.tamed)) {
            const double dx = pl.x - s.x, dy = pl.y - s.y, dz = pl.z - s.z;
            const double d2 = dx * dx + dy * dy + dz * dz;
            if (d2 > 16384.0) { s.active = false; return; }
            if (s.entityAge > 600 && rng.nextInt(800) == 0) {
                if (d2 < 1024.0) s.entityAge = 0; else { s.active = false; return; }
            }
        }

        // EntityMob.onLivingUpdate: con luz envejece el doble de rapido; zombie y esqueleto se queman de dia al aire libre
        if (isMobKind(s.hkind)) {
            const double bright = brightnessAt(s, w);
            if (bright > 0.5) s.entityAge += 2;
            if ((s.hkind == H_ZOMBIE || s.hkind == H_SKELETON) && w.isDay()) {
                if (bright > 0.5 && w.canSeeSky((int)floor(s.x), AnimalAI::cellY(s.y + s.height * 0.66), (int)floor(s.z)) && rng.nextFloat() * 30.0f < ((float)bright - 0.4f) * 2.0f)
                    s.fire = 300;
            }
        }

        // el lobo: inclina la cabeza si el jugador (cerca y mirandolo) lleva un hueso o carne
        if (s.hkind == H_WOLF) {
            bool interested = false;
            if (s.lookingAtPlayer && !s.hasPath() && !s.angry) {
                const int held = sink.heldKind();
                if (!s.tamed && held == 1) interested = true;
                else if (s.tamed && held == 2) interested = true;
            }
            s.interest += ((interested ? 1.0f : 0.0f) - s.interest) * 0.4f;
            if (interested) s.lookTicks = 10;
        }

        // ---- IA y movimiento segun el tipo ----
        if (s.hkind == H_SQUID) {
            const bool inW = AnimalAI::inWater(s, w);
            if (s.hs.health <= 0) { s.isJumping = false; s.moveStrafing = 0.0f; s.moveForward = 0.0f; s.randomYawVelocity = 0.0f; }
            else squidPickVector(s, rng, inW);
            const float landed = AnimalAI::moveEntity(s, s.motionX, s.motionY, s.motionZ, w);   // moveEntityWithHeading: solo mueve
            if (landed > 0.0f) { const int d = (int)ceil((double)(landed - 3.0f)); if (d > 0) AnimalAI::damage(s, d, false, 0.0, 0.0); }
            s.prevLimbSwingAmount = s.limbSwingAmount;
            { const double vx = s.x - s.prevX, vz = s.z - s.prevZ; float v = (float)(sqrt(vx * vx + vz * vz) * 4.0); if (v > 1.0f) v = 1.0f; s.limbSwingAmount += (v - s.limbSwingAmount) * 0.4f; s.limbSwing += s.limbSwingAmount; }
            squidAnimate(s, w, rng);
            while (s.yaw - s.prevYaw < -180.0f) s.prevYaw -= 360.0f;
            while (s.yaw - s.prevYaw >= 180.0f) s.prevYaw += 360.0f;
            while (s.bodyYaw - s.prevBodyYaw < -180.0f) s.prevBodyYaw -= 360.0f;
            while (s.bodyYaw - s.prevBodyYaw >= 180.0f) s.prevBodyYaw += 360.0f;
            return;
        }

        if (s.hkind == H_GHAST) {
            if (s.hs.health > 0) updateGhast(s, w, rng, pl, sink);
            // EntityFlying.moveEntityWithHeading: sin gravedad; 0,91 de friccion en el aire (0,546 sobre el suelo, 0,8 en el agua)
            const bool inW = AnimalAI::inWater(s, w);
            const float landed = AnimalAI::moveEntity(s, s.motionX, s.motionY, s.motionZ, w);
            (void)landed;                                                   // EntityFlying.fall: vacio
            const double f = inW ? 0.8 : (s.onGround ? 0.546 : 0.91);
            s.motionX *= f; s.motionY *= f; s.motionZ *= f;
            s.prevLimbSwingAmount = s.limbSwingAmount;
            { const double vx = s.x - s.prevX, vz = s.z - s.prevZ; float v = (float)(sqrt(vx * vx + vz * vz) * 4.0); if (v > 1.0f) v = 1.0f; s.limbSwingAmount += (v - s.limbSwingAmount) * 0.4f; s.limbSwing += s.limbSwingAmount; }
        } else {
            if (s.hs.health <= 0) {
                s.isJumping = false; s.moveStrafing = 0.0f; s.moveForward = 0.0f; s.randomYawVelocity = 0.0f;
            } else if (s.hkind == H_SLIME) {
                updateSlime(s, w, rng, pl);
            } else {
                updateActionState(s, w, rng, pl, sink);
                if (!s.active) return;                                      // el creeper exploto
            }
            const bool water = AnimalAI::inWater(s, w), lava = AnimalAI::inLava(s, w);
            if (lava) {
                if (s.hkind != H_PIGZOMBIE) { AnimalAI::damage(s, 4, false, 0.0, 0.0); s.fire = 600; }   // setOnFireFromLava
                s.fallDistance *= 0.5f;
            }
            if (s.hs.health > 0 && !water && s.hkind != H_GHAST && s.hkind != H_PIGZOMBIE && AnimalAI::inFire(s, w)) { AnimalAI::damage(s, 1, false, 0.0, 0.0); if (s.fire <= 0) s.fire = 300; }   // moveEntity: dentro del fuego se prende
            if (s.isJumping) {
                if (water) s.motionY += 0.04;
                else if (lava) s.motionY += 0.04;
                else if (s.onGround) s.motionY = 0.42;
            }
            s.moveStrafing *= 0.98f; s.moveForward *= 0.98f; s.randomYawVelocity *= 0.9f;
            const float landed = AnimalAI::moveWithHeading(s, s.moveStrafing, s.moveForward, w);
            if (landed > 0.0f) { const int d = (int)ceil((double)(landed - 3.0f)); if (d > 0) AnimalAI::damage(s, d, false, 0.0, 0.0); }
        }

        // EntitySlime.onUpdate: al caer se aplasta; el aplastamiento se apaga de a poco. Y el golpe al jugador (onCollideWithPlayer, solo los grandes)
        if (s.hkind == H_SLIME) {
            if (s.onGround && !wasGround) s.squish = -0.5f;
            s.squish *= 0.6f;
            if (s.slimeSize > 1 && s.hs.health > 0 && pl.valid && canSee(s, w, pl) && dist3(s, pl) < 0.6 * (double)s.slimeSize)
                sink.playerHit(s.slimeSize, s.x, s.z);
        }

        // EntityCreeper.onUpdate: sin objetivo, la mecha se apaga de a poco
        if (s.hkind == H_CREEPER && !s.hasTarget && s.ignite > 0) {
            s.creeperState = -1;
            --s.ignite;
            if (s.ignite < 0) s.ignite = 0;
        }

        // EntityLiving.onUpdate: hacia donde mira el cuerpo
        const double vx = s.x - s.prevX, vz = s.z - s.prevZ;
        const float moved = (float)sqrt(vx * vx + vz * vz);
        float target = s.bodyYaw;
        if (moved > 0.05f) target = (float)(atan2(vz, vx) * 180.0 / PI_D) - 90.0f;
        float d1 = (float)AnimalAI::wrap180((double)target - (double)s.bodyYaw);
        s.bodyYaw += d1 * 0.3f;
        float d2 = (float)AnimalAI::wrap180((double)s.yaw - (double)s.bodyYaw);
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

    // ---- EntityArrow.onUpdate (solo contra bloques y contra el jugador) ----
    // Devuelve en 'hit' si le pego al jugador. La caja del jugador se agranda 0,3 como en el original.
    inline bool segmentHitsBox(double x0, double y0, double z0, double x1, double y1, double z1,
                               double bx0, double by0, double bz0, double bx1, double by1, double bz1) {
        double tmin = 0.0, tmax = 1.0;
        const double o[3] = { x0, y0, z0 }, d[3] = { x1 - x0, y1 - y0, z1 - z0 };
        const double lo[3] = { bx0, by0, bz0 }, hi[3] = { bx1, by1, bz1 };
        for (int a = 0; a < 3; a++) {
            if (fabs(d[a]) < 1.0e-12) { if (o[a] < lo[a] || o[a] > hi[a]) return false; }
            else {
                double t1 = (lo[a] - o[a]) / d[a], t2 = (hi[a] - o[a]) / d[a];
                if (t1 > t2) { const double t = t1; t1 = t2; t2 = t; }
                if (t1 > tmin) tmin = t1;
                if (t2 < tmax) tmax = t2;
                if (tmin > tmax) return false;
            }
        }
        return true;
    }

    template <class W, class R, class S>
    void arrowTick(Arrow& a, W& w, R& rng, const PlayerPos& pl, S& sink) {
        a.prevX = a.x; a.prevY = a.y; a.prevZ = a.z;
        a.prevYaw = a.yaw; a.prevPitch = a.pitch;
        if (a.shake > 0) --a.shake;

        if (a.inGround) {
            if (AnimalAI::blocking(w, a.tileI, a.tileJ, a.tileK)) {
                if (++a.ticksInGround >= 1200) a.active = false;            // desaparece a los 60 s
            } else {                                                        // se rompio el bloque: vuelve a caer
                a.inGround = false;
                a.mx *= (double)(rng.nextFloat() * 0.2f);
                a.my *= (double)(rng.nextFloat() * 0.2f);
                a.mz *= (double)(rng.nextFloat() * 0.2f);
                a.ticksInGround = 0; a.ticksInAir = 0;
            }
            return;
        }

        ++a.ticksInAir;
        const double tx = a.x + a.mx, ty = a.y + a.my, tz = a.z + a.mz;
        // bloques: se recorre el segmento de a 0,1 y se frena en el primero que bloquea
        const double len = sqrt(a.mx * a.mx + a.my * a.my + a.mz * a.mz);
        int steps = (int)ceil(len / 0.1);
        if (steps < 1) steps = 1;
        bool hitBlock = false;
        double hx = a.x, hy = a.y, hz = a.z;
        int bi = 0, bj = 0, bk = 0;
        for (int n = 1; n <= steps; n++) {
            const double t = (double)n / (double)steps;
            const double px = a.x + a.mx * t, py = a.y + a.my * t, pz = a.z + a.mz * t;
            const int i = (int)floor(px), j = AnimalAI::cellY(py), k = (int)floor(pz);
            if (AnimalAI::blocking(w, i, j, k)) { hitBlock = true; bi = i; bj = j; bk = k; break; }
            hx = px; hy = py; hz = pz;
        }
        const double ex = hitBlock ? hx : tx, ey = hitBlock ? hy : ty, ez = hitBlock ? hz : tz;

        if (pl.valid) {                                                     // el jugador: caja de 0,6 x 1,8 agrandada 0,3
            const double minY = pl.y - 1.62;
            if (segmentHitsBox(a.x, a.y, a.z, ex, ey, ez, pl.x - 0.6, minY - 0.3, pl.z - 0.6, pl.x + 0.6, minY + 1.8 + 0.3, pl.z + 0.6)) {
                sink.playerHit(4, a.ownerX, a.ownerZ);
                a.active = false;
                return;
            }
        }

        if (hitBlock) {
            double dx = hx - a.x, dy = hy - a.y, dz = hz - a.z;
            const double l = sqrt(dx * dx + dy * dy + dz * dz);
            a.tileI = bi; a.tileJ = bj; a.tileK = bk;
            a.mx = dx; a.my = dy; a.mz = dz;
            if (l > 1.0e-6) { a.x -= dx / l * 0.05; a.y -= dy / l * 0.05; a.z -= dz / l * 0.05; }
            a.x += a.mx; a.y += a.my; a.z += a.mz;
            a.inGround = true;
            a.shake = 7;
            return;
        }

        a.x = tx; a.y = ty; a.z = tz;
        const double h = sqrt(a.mx * a.mx + a.mz * a.mz);
        float newYaw = (float)(atan2(a.mx, a.mz) * 180.0 / PI_D);
        float newPitch = (float)(atan2(a.my, h) * 180.0 / PI_D);
        while (newPitch - a.prevPitch < -180.0f) a.prevPitch -= 360.0f;
        while (newPitch - a.prevPitch >= 180.0f) a.prevPitch += 360.0f;
        while (newYaw - a.prevYaw < -180.0f) a.prevYaw -= 360.0f;
        while (newYaw - a.prevYaw >= 180.0f) a.prevYaw += 360.0f;
        a.pitch = a.prevPitch + (newPitch - a.prevPitch) * 0.2f;
        a.yaw = a.prevYaw + (newYaw - a.prevYaw) * 0.2f;
        const bool wet = isWaterBlock(w.type((int)floor(a.x), AnimalAI::cellY(a.y), (int)floor(a.z)));
        const double drag = wet ? 0.8 : 0.99;
        a.mx *= drag; a.my *= drag; a.mz *= drag;
        a.my -= 0.03;
    }

    // ---- EntityFireball.onUpdate: choca con bloques, con el jugador y con otros mobs (caja agrandada 0,3) y explota (tamano 1, con fuego) ----
    template <class W, class S>
    void fireballTick(Fireball& f, W& w, const PlayerPos& pl, S& sink) {
        f.prevX = f.x; f.prevY = f.y; f.prevZ = f.z;
        ++f.ticksInAir;
        if (++f.life > 600) { f.active = false; return; }
        const double tx = f.x + f.mx, ty = f.y + f.my, tz = f.z + f.mz;
        const double len = sqrt(f.mx * f.mx + f.my * f.my + f.mz * f.mz);
        int steps = (int)ceil(len / 0.1);
        if (steps < 1) steps = 1;
        bool hit = false;
        double hx = f.x, hy = f.y, hz = f.z;
        for (int n = 1; n <= steps; n++) {
            const double t = (double)n / (double)steps;
            const double px = f.x + f.mx * t, py = f.y + f.my * t, pz = f.z + f.mz * t;
            if (AnimalAI::blocking(w, (int)floor(px), AnimalAI::cellY(py), (int)floor(pz))) { hit = true; break; }
            hx = px; hy = py; hz = pz;
        }
        const double ex = hit ? hx : tx, ey = hit ? hy : ty, ez = hit ? hz : tz;
        if (!hit && pl.valid) {                                              // la caja del jugador (agrandada 0,3 en cada lado)
            const double minY = pl.y - 1.62;
            if (segmentHitsBox(f.x, f.y, f.z, ex, ey, ez, pl.x - 0.6, minY - 0.3, pl.z - 0.6, pl.x + 0.6, minY + 1.8 + 0.3, pl.z + 0.6)) hit = true;
        }
        if (!hit && sink.mobHit(f.x, f.y, f.z, ex, ey, ez)) hit = true;
        if (hit) {
            sink.explodeAt(hit ? ex : f.x, hit ? ey : f.y, hit ? ez : f.z, 1.0f, true);
            f.active = false;
            return;
        }
        f.x = tx; f.y = ty; f.z = tz;
        const bool wet = isWaterBlock(w.type((int)floor(f.x), AnimalAI::cellY(f.y), (int)floor(f.z)));
        const double drag = wet ? 0.8 : 0.95;
        f.mx += f.ax; f.my += f.ay; f.mz += f.az;
        f.mx *= drag; f.my *= drag; f.mz *= drag;
    }

    // ---- aparicion (SpawnerAnimals) ----
    template <class W>
    inline bool boxClear(W& w, double x, double y, double z, double width, double height) {
        const int i0 = (int)floor(x - width * 0.5), i1 = (int)floor(x + width * 0.5);
        const int k0 = (int)floor(z - width * 0.5), k1 = (int)floor(z + width * 0.5);
        const int j0 = AnimalAI::cellY(y + 1.0e-6), j1 = AnimalAI::cellY(y + height - 1.0e-6);
        for (int i = i0; i <= i1; i++) for (int j = j0; j <= j1; j++) for (int k = k0; k <= k1; k++) {
            const BlockType t = w.type(i, j, k);
            if (isMaterialSolid(t) || isLiquidBlock(t)) return false;
        }
        return true;
    }

    // SpawnerAnimals.canCreatureTypeSpawnAtLocation: en el aire, suelo normal debajo y nada que llene la celda ni la de arriba; en el agua, agua y nada sobre ella
    template <class W>
    inline bool airSpawnSpot(W& w, int i, int j, int k) {
        return isNormalCube(w.type(i, j - 1, k)) && !isNormalCube(w.type(i, j, k)) && !isLiquidBlock(w.type(i, j, k)) && !isNormalCube(w.type(i, j + 1, k));
    }
    template <class W>
    inline bool waterSpawnSpot(W& w, int i, int j, int k) {
        return isWaterBlock(w.type(i, j, k)) && !isNormalCube(w.type(i, j + 1, k));
    }

    // EntityMob.isValidLightLevel: luz del cielo > nextInt(32) no, y luz total <= nextInt(8) si (en el Nether no hay cielo: solo la luz de bloque)
    template <class W, class R>
    inline bool mobLightOk(W& w, R& rng, bool nether, int i, int j, int k) {
        if (!nether && w.rawSky(i, j, k) > rng.nextInt(32)) return false;
        const int light = nether ? w.blockLight(i, j, k) : w.totalLight(i, j, k);
        return light <= rng.nextInt(8);
    }

    // Lo que decide cada getCanSpawnHere (ademas del sitio: airSpawnSpot / waterSpawnSpot). (i, j, k): celda de los pies; (x, y, z): posicion
    template <class W, class R>
    bool canSpawnHere(int kind, W& w, R& rng, bool nether, long long worldSeed, int i, int j, int k, double x, double y, double z) {
        double width = 0.6, height = 1.8;
        if (kind == H_SPIDER) { width = 1.4; height = 0.9; }
        else if (kind == H_GHAST) { width = height = 4.0; }
        else if (kind == H_WOLF) { width = height = 0.8; }
        else if (kind == H_SLIME) { width = height = 0.6; }
        else if (kind == H_SQUID) { return true; }                          // EntityWaterMob.getCanSpawnHere: solo que no haya otro mob encima
        if (!boxClear(w, x, y, z, width, height)) return false;             // EntityLiving.getCanSpawnHere
        switch (kind) {
        case H_ZOMBIE: case H_SKELETON: case H_SPIDER: case H_CREEPER: case H_PIGZOMBIE: {
            if (!mobLightOk(w, rng, nether, i, j, k)) return false;
            return w.light(i, j, k) <= 0.5f;                                // EntityCreature: getBlockPathWeight >= 0 (0,5 - brillo)
        }
        case H_SLIME:
            return rng.nextInt(10) == 0 && isSlimeChunk(worldSeed, (int)floor((double)i / 16.0), (int)floor((double)k / 16.0)) && (j + 1) < 16;   // posY < 16
        case H_GHAST:
            return rng.nextInt(20) == 0;
        case H_WOLF:
            return w.type(i, j - 1, k) == BlockType::GRASS && w.totalLight(i, j, k) > 8;   // EntityAnimal.getCanSpawnHere
        default:
            return true;
        }
    }
}

#endif
