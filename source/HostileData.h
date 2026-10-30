#ifndef HOSTILEDATA_H
#define HOSTILEDATA_H

#include "AnimalData.h"

// Los mobs con IA propia (HostileManager): hostiles (EntityMob y los que son IMob), el lobo (neutral) y el calamar (de agua).
enum { H_ZOMBIE = 0, H_SKELETON = 1, H_SPIDER = 2, H_CREEPER = 3, H_PIGZOMBIE = 4, H_SLIME = 5, H_GHAST = 6, H_WOLF = 7, H_SQUID = 8, H_KIND_COUNT = 9 };

// Un mob (EntityZombie / EntitySkeleton / EntitySpider / EntityCreeper / EntityPigZombie sobre EntityMob / EntityCreature / EntityLiving, mas EntitySlime,
// EntityGhast (EntityFlying), EntityWolf (EntityAnimal) y EntitySquid (EntityWaterMob)).
// Usa la misma física y el mismo camino (A*) que los animales: Mob hereda de Animal, asi que las funciones de AnimalAI valen tal cual.
struct Mob : public Animal {
    int hkind = H_ZOMBIE;
    bool hasTarget = false;     // EntityCreature.playerToAttack != null
    bool hasAttacked = false;   // EntityCreature.hasAttacked: se queda quieto mirando al objetivo (esqueleto que dispara, creeper que se enciende)
    int attackTime = 0;         // EntityLiving.attackTime
    float moveSpeed = 0.7f;     // EntityLiving.moveSpeed
    int attackStrength = 2;     // EntityMob.attackStrength
    int entityAge = 0;          // EntityLiving.entityAge (despawnEntity)

    // EntityCreeper
    int ignite = 0, lastIgnite = 0;   // timeSinceIgnited, lastActiveTime (llega a 30 y explota)
    int creeperState = -1;            // dataWatcher 16: 1 = encendido, -1 = apagado

    // EntityPigZombie
    int anger = 0;                    // angerLevel

    // EntitySlime: tamano 1, 2 o 4; vida = tamano al cuadrado; field_768_a / field_767_b = el aplastamiento al saltar y caer
    int slimeSize = 1;
    int slimeJump = 0;                // slimeJumpDelay
    float squish = 0.0f, prevSquish = 0.0f;

    // EntityGhast
    int courseCooldown = 0;           // courseChangeCooldown
    int aggro = 0;                    // aggroCooldown
    int attackCounter = 0, prevAttackCounter = 0;
    double wayX = 0.0, wayY = 0.0, wayZ = 0.0;   // el punto al que flota

    // EntityWolf (los bits de dataWatcher 16: sentado, enojado, domado)
    bool sitting = false, angry = false, tamed = false;
    float interest = 0.0f, prevInterest = 0.0f;  // field_25048_b / field_25054_c: inclina la cabeza si el jugador lleva un hueso o carne

    // EntitySquid (field_21089_a ... field_21079_m)
    float sqPitch = 0.0f, sqPrevPitch = 0.0f;    // field_21089_a / 21088_b
    float sqRoll = 0.0f, sqPrevRoll = 0.0f;      // field_21087_c / 21086_f
    float sqPhase = 0.0f;                        // field_21085_g
    float sqTent = 0.0f, sqPrevTent = 0.0f;      // field_21083_i / 21082_j: el angulo de los tentaculos
    float sqSpeed = 0.0f;                        // randomMotionSpeed
    float sqStep = 0.2f;                         // field_21080_l
    float sqBoost = 0.0f;                        // field_21079_m
    float sqVx = 0.0f, sqVy = 0.0f, sqVz = 0.0f; // randomMotionVecX/Y/Z

    Mob() { hs.health = 20; hs.prevHealth = 20; kind = 100; }
};

// EntityArrow (la flecha del esqueleto). Posicion en las mismas coordenadas del mundo que los mobs (la celda j cubre y de j-0,5 a j+0,5).
struct Arrow {
    bool active = false;
    double x = 0.0, y = 0.0, z = 0.0;
    double prevX = 0.0, prevY = 0.0, prevZ = 0.0;
    double mx = 0.0, my = 0.0, mz = 0.0;
    float yaw = 0.0f, pitch = 0.0f, prevYaw = 0.0f, prevPitch = 0.0f;
    bool inGround = false;
    int shake = 0;
    int ticksInGround = 0, ticksInAir = 0;
    int tileI = 0, tileJ = 0, tileK = 0;     // la celda donde quedo clavada
    double ownerX = 0.0, ownerZ = 0.0;       // de donde vino (el empuje al golpear al jugador)
};

// EntityFireball (la bola de fuego del ghast): acelera 0,1 por tick hacia donde apunta y explota (tamano 1, con fuego) al chocar.
struct Fireball {
    bool active = false;
    double x = 0.0, y = 0.0, z = 0.0;
    double prevX = 0.0, prevY = 0.0, prevZ = 0.0;
    double mx = 0.0, my = 0.0, mz = 0.0;
    double ax = 0.0, ay = 0.0, az = 0.0;     // field_9405_b / c / d: la aceleracion
    int ticksInAir = 0;                      // field_9395_l: el ghast que la lanza no la choca hasta los 25 ticks
    int life = 0;                            // seguro: se borra a los 600 ticks
};

#endif
