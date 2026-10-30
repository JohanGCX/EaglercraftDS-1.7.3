#ifndef SHEEPDATA_H
#define SHEEPDATA_H

#include "Health.h"

// Una oveja (EntitySheep sobre EntityAnimal/EntityCreature/EntityLiving). Posición en coordenadas del mundo, y = pies. Los ángulos son los de
// EntityLiving, en grados, con yaw 0 mirando a +Z. La posición y el movimiento van en double como en el original.
struct Sheep {
    bool active = false;
    bool isInWeb = false;   // Entity.isInWeb: estuvo dentro de una telaraña (frena el siguiente movimiento)

    double x = 0.0, y = 0.0, z = 0.0;
    double prevX = 0.0, prevY = 0.0, prevZ = 0.0;
    double motionX = 0.0, motionY = 0.0, motionZ = 0.0;
    bool onGround = false;
    float walkedDist = 0.0f; int nextStep = 1;       // Entity.distanceWalkedModified / nextStepDistance: cada paso puede pisotear la tierra de cultivo
    bool collidedH = false;                          // isCollidedHorizontally
    float fallDistance = 0.0f;

    float yaw = 0.0f, prevYaw = 0.0f;                // rotationYaw: hacia dónde mira la cabeza
    float pitch = 0.0f, prevPitch = 0.0f;            // rotationPitch
    float bodyYaw = 0.0f, prevBodyYaw = 0.0f;        // renderYawOffset: hacia dónde mira el cuerpo

    float limbSwing = 0.0f;                          // fase de las patas (field_703_S)
    float limbSwingAmount = 0.0f;                    // cuánto se mueven, 0 a 1 (field_704_R)
    float prevLimbSwingAmount = 0.0f;                // field_705_Q

    int fleeceColor = 0;                             // 0 = blanca ... 15 = negra (igual que la lana)
    bool sheared = false;                            // esquilada: solo se ve la piel

    // IA (EntityLiving / EntityCreature)
    float moveStrafing = 0.0f, moveForward = 0.0f;
    float randomYawVelocity = 0.0f;
    bool isJumping = false;
    static const int MAX_PATH = 64;
    int pathLen = 0, pathIndex = 0;                  // PathEntity: puntos del camino y por cuál va (pathLen = 0: sin camino)
    int pathX[MAX_PATH], pathY[MAX_PATH], pathZ[MAX_PATH];   // celdas del camino (la oveja se para en el centro de cada una)
    bool hasPath() const { return pathLen > 0; }
    bool lookingAtPlayer = false;                    // currentTarget
    int lookTicks = 0;                               // numTicksToChaseTarget

    // vida (EntityLiving.health = 10): se reutiliza la lógica de daño del jugador (invulnerabilidad de 10 ticks, hurtTime, ...)
    HealthState hs;
    int air = 300;
    int fire = 0;               // Entity.fire: ticks que le quedan ardiendo
    bool diedPending = false;                        // acaba de morir: el administrador suelta su lana

    int ticksExisted = 0;

    Sheep() { hs.health = 10; hs.prevHealth = 10; }
};

#endif