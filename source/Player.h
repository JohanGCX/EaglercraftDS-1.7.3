#ifndef PLAYER_H
#define PLAYER_H

#include <nds.h>

class ChunkManager;

class Player {
public:
    float x, y, z;
    float yaw;
    float pitch;

    Player();

    void applyCamera(float partialTick, float bobRoll);
    void handleInput(u32 keysHeld, const ChunkManager& chunkManager);
    void handleTouch(bool touching, int touchX, int touchY);
    void getLookDirection(float& outX, float& outY, float& outZ) const;
    // Tercera persona (EntityRenderer.orientCamera): hasta donde puede alejarse la camara detras de los ojos sin entrar en un bloque (maximo 4)
    float thirdPersonDistance(const ChunkManager& chunkManager, float eyeX, float eyeY, float eyeZ) const;
    float getBobStrengthDebug() const { return bobStrength; }

    void physicsTick(u32 keysHeld, const ChunkManager& chunkManager, bool autoJumpEnabled);

    void getViewBob(float partialTick, float& bobX, float& bobY, float& bobRoll) const;
    float getDistanceWalked() const { return currentTickDistance; }
    float getLastRenderY() const { return lastRenderY; }
    void handleButtonLook(bool lookLeft, bool lookRight, bool lookUp, bool lookDown);
    bool wouldOverlapBlock(float blockWorldX, float blockWorldY, float blockWorldZ) const;
    void teleportTo(float nx, float ny, float nz);
    void rideTo(float ex, float ey, float ez);   // va sentado en una vagoneta: sin fisica propia, main lo coloca cada tick (ojos en ex, ey, ez)
    void setDead();   // EntityPlayer.onDeath: queda caído en el suelo, sin controles
    void knockBack(float attackerX, float attackerZ);   // EntityLiving.knockBack: el empuje al recibir un golpe de alguien en (attackerX, attackerZ)
    void addVelocity(float vx, float vy, float vz) { velocityX += vx; velocityY += vy; velocityZ += vz; }   // Entity.motion += (explosiones)

    float getVelX() const { return velocityX; }
    float getVelY() const { return velocityY; }
    float getVelZ() const { return velocityZ; }
    bool isOnGround() const { return onGround; }
    // Agacharse: mas lento, no se cae de los bordes y los ojos bajan
    void setSneak(bool s) { sneaking = s; }
    bool isSneaking() const { return sneaking; }
    float getEyeDrop(float pt) const { return prevEyeDrop + (eyeDrop - prevEyeDrop) * pt; }   // cuanto bajan los ojos (bloques)
    bool wasInWater() const { return lastInWater; }   // Entity.handleWaterMovement del último tick
    bool wasInLava() const { return lastInLava; }     // Entity.handleLavaMovement del último tick
    // Entity.updateFallState: distancia de caída que se acaba de aplicar al tocar suelo (0 si no aterrizó); se lee y se borra
    float takeFallDistance() { float f = pendingFall; pendingFall = 0.0f; return f; }
    float getTickMoveX() const { return currentTickX - prevTickX; }
    float getTickMoveZ() const { return currentTickZ - prevTickZ; }
    void getRenderPosition(float pt, float& rx, float& ry, float& rz) const {
        rx = prevTickX + (currentTickX - prevTickX) * pt;
        ry = prevTickY + (currentTickY - prevTickY) * pt;
        rz = prevTickZ + (currentTickZ - prevTickZ) * pt;
    }

private:
    mutable float lastRenderY = 0.0f;
    static constexpr float MAX_PITCH = 88.0f;
    static constexpr float TOUCH_SENSITIVITY = 1.0f;

    // Medidas del cuerpo; variables porque al morir pasan a una caja de 0,2 con los ojos a 0,1 del suelo (EntityPlayer.onDeath)
    float halfWidth = 0.3f;
    float eyeToFeet = 1.62f;
    float eyeToHead = 0.18f;

    static constexpr float GRAVITY = 0.08f;
    static constexpr float DRAG_Y = 0.98f;
    static constexpr float JUMP_VELOCITY = 0.42f;
    static constexpr float WATER_GRAVITY = 0.01f; // mucho más suave que GRAVITY normal
    static constexpr float WATER_DRAG_Y = 0.85f;  

    // EntityLiving.moveEntityWithHeading: moveFlying(..., 0.1 en tierra [* 0.16277136 / 0.546^3 = 1.0], 0.02 en aire,
    // agua y lava) con la entrada escalada por 0.98 (EntityLiving.onLivingUpdate)
    static constexpr float GROUND_ACCELERATION = 0.1f;  
    static constexpr float AIR_ACCELERATION = 0.02f;   
    static constexpr float LIQUID_ACCELERATION = 0.02f;
    static constexpr float INPUT_SCALE = 0.98f;
    static constexpr float WATER_DRAG = 0.8f;   // horizontal y vertical, cada tick
    static constexpr float LAVA_DRAG = 0.5f;
    static constexpr float GROUND_FRICTION = 0.546f;
    static constexpr float AIR_FRICTION = 0.91f;
    static constexpr float MAX_SPEED = 0.5f;

    static constexpr float STEP_HEIGHT = 0.5f;

    float velocityX = 0.0f;
    float velocityY = 0.0f;
    float velocityZ = 0.0f;
    bool onGround = false;
    bool sneaking = false;
    float eyeDrop = 0.0f, prevEyeDrop = 0.0f;
    static constexpr float SNEAK_EYE_DROP = 0.2f;
    static constexpr float SNEAK_SPEED = 0.3f;   // MovementInput: la entrada se multiplica por 0,3 al agacharse
    float fallDistance = 0.0f;   // Entity.fallDistance: cuánto lleva cayendo
    float pendingFall = 0.0f;    // la caída que acaba de terminar, para que main la convierta en daño
    bool lastInWater = false;
    bool lastInLava = false;

    float prevTickX = 0.0f;
    float currentTickX = 0.0f;
    float prevTickY = 0.0f;
    float currentTickY = 0.0f;
    float prevTickZ = 0.0f;
    float currentTickZ = 0.0f;

    float bobPhase = 0.0f; 
    float prevTickDistance = 0.0f;
    float currentTickDistance = 0.0f;
    float distanceAccum = 0.0f;
    float bobStrength = 0.0f;

    bool wasTouching = false;
    int prevTouchX = 0;
    int prevTouchY = 0;

    float buttonLookSpeedH = 0.0f;
    float buttonLookSpeedV = 0.0f;

    bool collidesAt(const ChunkManager& chunkManager, float testX, float testY, float testZ) const;
    bool tryStepUp(const ChunkManager& chunkManager, float dx, float dz);
};

#endif