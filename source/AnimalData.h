#ifndef ANIMALDATA_H
#define ANIMALDATA_H

#include "Health.h"

enum { KIND_COW = 0, KIND_PIG = 1, KIND_CHICKEN = 2, KIND_COUNT = 3 };

// Un animal (vaca, cerdo o gallina: EntityCow / EntityPig / EntityChicken sobre EntityAnimal/EntityCreature/EntityLiving). Posición en coordenadas del mundo, y = pies. Los ángulos son los de
// EntityLiving, en grados, con yaw 0 mirando a +Z. La posición y el movimiento van en double como en el original.
struct Animal {
    bool active = false;
    bool isInWeb = false;   // Entity.isInWeb: estuvo dentro de una telaraña (frena el siguiente movimiento)

    double x = 0.0, y = 0.0, z = 0.0;
    double prevX = 0.0, prevY = 0.0, prevZ = 0.0;
    double motionX = 0.0, motionY = 0.0, motionZ = 0.0;
    bool onGround = false;
    bool collidedH = false;                          // isCollidedHorizontally
    float fallDistance = 0.0f;
    float walkedDist = 0.0f; int nextStep = 1;       // Entity.distanceWalkedModified / nextStepDistance: cada paso puede pisotear la tierra de cultivo

    float yaw = 0.0f, prevYaw = 0.0f;                // rotationYaw: hacia dónde mira la cabeza
    float pitch = 0.0f, prevPitch = 0.0f;            // rotationPitch
    float bodyYaw = 0.0f, prevBodyYaw = 0.0f;        // renderYawOffset: hacia dónde mira el cuerpo

    float limbSwing = 0.0f;                          // fase de las patas (field_703_S)
    float limbSwingAmount = 0.0f;                    // cuánto se mueven, 0 a 1 (field_704_R)
    float prevLimbSwingAmount = 0.0f;                // field_705_Q

    // Que animal es y su tamano (setSize): vaca 0,9 x 1,3, cerdo 0,9 x 0,9, gallina 0,3 x 0,4
    int kind = 0;
    double width = 0.9, height = 1.3;
    bool saddled = false;                            // EntityPig.getSaddled (dataWatcher 16)
    bool noFall = false;                             // la gallina no se hace dano al caer

    // EntityChicken: aleteo (field_752_b, destPos, field_757_d, field_756_e, field_755_h) y huevo
    float wingPhase = 0.0f, wingPrevPhase = 0.0f;
    float wingDest = 0.0f, wingPrevDest = 0.0f;
    float wingSpeed = 1.0f;
    int eggTimer = 6000;
    bool eggPending = false;                         // puso un huevo: el administrador lo suelta

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
    bool diedPending = false;                        // acaba de morir: el administrador suelta lo suyo

    int ticksExisted = 0;

    Animal() { hs.health = 10; hs.prevHealth = 10; }
};

#endif
