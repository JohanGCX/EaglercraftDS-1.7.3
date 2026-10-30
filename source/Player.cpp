#include "Player.h"
#include "ChunkManager.h"
#include <math.h>

Player::Player() {
    x = 0.0f;
    y = 12.0f;
    z = 3.0f;
    yaw = 0.0f;
    pitch = 0.0f;

    prevTickY = y;
    currentTickY = y;
    prevTickX = x;
    currentTickX = x;
    prevTickZ = z;
    currentTickZ = z;
}

void Player::applyCamera(float partialTick, float bobRoll) {
    float renderX = prevTickX + (currentTickX - prevTickX) * partialTick;
    float renderY = prevTickY + (currentTickY - prevTickY) * partialTick;
    float renderZ = prevTickZ + (currentTickZ - prevTickZ) * partialTick;

    lastRenderY = renderY;

    float yawRad = yaw * (M_PI / 180.0f);
    float pitchRad = pitch * (M_PI / 180.0f);

    float lookX = renderX - sinf(yawRad) * cosf(pitchRad);
    float lookY = renderY + sinf(pitchRad);
    float lookZ = renderZ - cosf(yawRad) * cosf(pitchRad);

    // Inclinamos el vector "up" según bobRoll, en vez de rotar la
    // matriz completa después — así el roll gira alrededor del eje
    // de VISTA de la cámara, no del eje Z fijo del mundo
    float rollRad = bobRoll * (M_PI / 180.0f);
    float upX = sinf(rollRad);
    float upY = cosf(rollRad);
    float upZ = 0.0f;

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    gluLookAt(
        renderX, renderY, renderZ,
        lookX, lookY, lookZ,
        upX, upY, upZ
    );
}

void Player::getLookDirection(float& outX, float& outY, float& outZ) const {
    float yawRad = yaw * (M_PI / 180.0f);
    float pitchRad = pitch * (M_PI / 180.0f);

    outX = -sinf(yawRad) * cosf(pitchRad);
    outY = sinf(pitchRad);
    outZ = -cosf(yawRad) * cosf(pitchRad);
}

float Player::thirdPersonDistance(const ChunkManager& chunkManager, float eyeX, float eyeY, float eyeZ) const {
    const float MAX_DIST = 4.0f, STEP = 0.1f;
    float lx, ly, lz;
    getLookDirection(lx, ly, lz);
    for (float d = STEP; d <= MAX_DIST; d += STEP) {
        if (chunkManager.isSolidAt(eyeX - lx * d, eyeY - ly * d, eyeZ - lz * d)) {
            const float r = d - STEP - 0.2f;   // un margen para no ver por dentro del bloque
            return r < 0.1f ? 0.1f : r;
        }
    }
    return MAX_DIST;
}

bool Player::tryStepUp(const ChunkManager& chunkManager, float dx, float dz) {
    if (!onGround) return false; // igual que Minecraft real: solo si está parado en el piso

    const float STEP_INCREMENT = 0.05f;

    // Se cuenta por indice (acumular 0,05 en float pasa de 0,5 y dejaba fuera el escalon de 0,5 de las losas y escaleras); el ultimo paso
    // llega un poco mas alla de STEP_HEIGHT para que los pies queden sobre el borde y no justo en el
    const int STEPS = (int)(STEP_HEIGHT / STEP_INCREMENT + 0.5f);
    for (int i = 1; i <= STEPS + 1; i++) {
        const float stepY = (i <= STEPS) ? i * STEP_INCREMENT : STEP_HEIGHT + 0.02f;
        if (!collidesAt(chunkManager, x + dx, y + stepY, z + dz)) {
            y += stepY;
            return true;
        }
    }
    return false;
}

bool Player::collidesAt(const ChunkManager& chunkManager, float testX, float testY, float testZ) const {
    // Muestreamos 4 alturas repartidas entre los pies y la cabeza,
    // separadas por menos de 1 unidad — así ningún bloque de tamaño
    // normal (1 unidad) puede "colarse" sin tocar ningún punto
    const int SAMPLES = 4;

    for (int cx = -1; cx <= 1; cx += 2) {
        for (int cz = -1; cz <= 1; cz += 2) {
            float px = testX + cx * halfWidth;
            float pz = testZ + cz * halfWidth;

            for (int s = 0; s < SAMPLES; s++) {
                float t = (float)s / (float)(SAMPLES - 1); // 0.0 a 1.0
                float sampleY = (testY - eyeToFeet) + t * (eyeToFeet + eyeToHead);

                if (chunkManager.isSolidAt(px, sampleY, pz)) return true;
            }
        }
    }
    return false;
}

void Player::handleInput(u32 keysHeld, const ChunkManager& chunkManager) {
    (void)keysHeld;
    (void)chunkManager;
}

void Player::physicsTick(u32 keysHeld, const ChunkManager& chunkManager, bool autoJumpEnabled) {
    prevTickY = y;
    prevTickX = x;
    prevTickZ = z;
    prevEyeDrop = eyeDrop;
    eyeDrop += ((sneaking ? SNEAK_EYE_DROP : 0.0f) - eyeDrop) * 0.5f;

    // Entity.handleWaterMovement / handleLavaMovement: la caja del jugador, achicada, contra las celdas de líquido
    const float feetY = y - eyeToFeet, headY = y + eyeToHead;
    float push[3] = { 0.0f, 0.0f, 0.0f };
    bool inWater = chunkManager.liquidAcceleration(x - halfWidth + 0.001f, feetY + 0.4f + 0.001f, z - halfWidth + 0.001f,
                                                   x + halfWidth - 0.001f, headY - 0.4f - 0.001f, z + halfWidth - 0.001f, 1, push);
    bool inLava = chunkManager.lavaInBox(x - halfWidth + 0.1f, feetY + 0.4f, z - halfWidth + 0.1f,
                                         x + halfWidth - 0.1f, headY - 0.4f, z + halfWidth - 0.1f);

    lastInWater = inWater;
    lastInLava = inLava;
    if (inWater) fallDistance = 0.0f;   // Entity.handleWaterMovement
    if (inLava) fallDistance *= 0.5f;   // Entity.setOnFireFromLava

    if (inWater || inLava) {
        if (keysHeld & KEY_A) {
            velocityY += 0.04f; // acumulativo, no fijo — confirmado real
        }
        onGround = false;
    } else if ((keysHeld & KEY_A) && onGround) {
        velocityY = JUMP_VELOCITY;
        onGround = false;
    }

    // Block.slipperiness: 0,6 normal y 0,98 el hielo; el suelo frena a slipperiness x 0,91 (0,546 normal) y la aceleracion en tierra
    // se ajusta por 0,16277136 / f^3 (igual a 1 con 0,546). Se mira el bloque bajo los pies.
    float groundFriction = GROUND_FRICTION;
    if (onGround && chunkManager.getBlockTypeAt(x, feetY - 0.5f, z) == BlockType::ICE) groundFriction = 0.98f * 0.91f;
    float friction = onGround ? groundFriction : AIR_FRICTION;
    if (inWater)     friction = WATER_DRAG;
    else if (inLava) friction = LAVA_DRAG;
    velocityX *= friction;
    velocityZ *= friction;

    if (inWater) { // las corrientes de agua arrastran (0.014 por tick en la dirección de la corriente)
        velocityX += push[0];
        velocityY += push[1];
        velocityZ += push[2];
    }

    float yawRad = yaw * (M_PI / 180.0f);
    float forwardX = -sinf(yawRad);
    float forwardZ = -cosf(yawRad);
    float rightX = cosf(yawRad);
    float rightZ = -sinf(yawRad);

    // EntityLiving.onLivingUpdate escala la entrada por 0.98 y Entity.moveFlying la normaliza solo si pasa de 1
    float strafeIn = 0.0f, forwardIn = 0.0f;
    if (keysHeld & KEY_UP)    forwardIn += 1.0f;
    if (keysHeld & KEY_DOWN)  forwardIn -= 1.0f;
    if (keysHeld & KEY_LEFT)  strafeIn  -= 1.0f;
    if (keysHeld & KEY_RIGHT) strafeIn  += 1.0f;
    strafeIn *= INPUT_SCALE;
    forwardIn *= INPUT_SCALE;
    if (sneaking) { strafeIn *= SNEAK_SPEED; forwardIn *= SNEAK_SPEED; }

    float accel = onGround ? GROUND_ACCELERATION * (0.16277136f / (groundFriction * groundFriction * groundFriction)) : AIR_ACCELERATION;
    if (inWater || inLava) accel = LIQUID_ACCELERATION;

    float inputMag = sqrtf(strafeIn * strafeIn + forwardIn * forwardIn);
    if (inputMag >= 0.01f) {
        if (inputMag < 1.0f) inputMag = 1.0f;
        float k = accel / inputMag;
        strafeIn *= k;
        forwardIn *= k;
        velocityX += forwardX * forwardIn + rightX * strafeIn;
        velocityZ += forwardZ * forwardIn + rightZ * strafeIn;
    }

    float speed = sqrtf(velocityX * velocityX + velocityZ * velocityZ);
    if (speed > MAX_SPEED) {
        float scale = MAX_SPEED / speed;
        velocityX *= scale;
        velocityZ *= scale;
    }

    // EntityLiving.isOnLadder / moveEntityWithHeading: en la escalera de mano la velocidad lateral se limita a 0,15, no se acumula caida
    // y no se baja mas rapido de 0,15; agachado se queda quieto. Contra la pared (mas abajo) se sube a 0,2 por tick.
    const bool onLadder = !inWater && !inLava && chunkManager.getBlockTypeAt(x, y - eyeToFeet + 0.001f, z) == BlockType::LADDER;
    if (onLadder) {
        const float LIM = 0.15f;
        if (velocityX < -LIM) velocityX = -LIM; else if (velocityX > LIM) velocityX = LIM;
        if (velocityZ < -LIM) velocityZ = -LIM; else if (velocityZ > LIM) velocityZ = LIM;
        fallDistance = 0.0f;
        if (velocityY < -0.15f) velocityY = -0.15f;
        if (sneaking && velocityY < 0.0f) velocityY = 0.0f;
    }

    // --- Auto-salto (movido aquí: antes de que la colisión horizontal
    // pueda poner velocityX/Z en 0, y antes de resolver el movimiento
    // vertical, para que el salto sí se aplique este mismo tick) ---
    if (autoJumpEnabled && onGround && !sneaking) {
        bool movementKeyHeld = (keysHeld & (KEY_UP | KEY_DOWN | KEY_LEFT | KEY_RIGHT)) != 0;
        float moveSpeed = sqrtf(velocityX * velocityX + velocityZ * velocityZ);
        if (movementKeyHeld && moveSpeed > 0.001f) {
            float dirX = velocityX / moveSpeed;
            float dirZ = velocityZ / moveSpeed;

            const float AHEAD_DIST = 0.35f;
            float feetY = y - eyeToFeet;

            bool shouldJump = false;

            for (int cx = -1; cx <= 1 && !shouldJump; cx += 2) {
                for (int cz = -1; cz <= 1 && !shouldJump; cz += 2) {
                    float cornerX = x + cx * halfWidth;
                    float cornerZ = z + cz * halfWidth;

                    float aheadX = cornerX + dirX * AHEAD_DIST;
                    float aheadZ = cornerZ + dirZ * AHEAD_DIST;

                    bool blockedAtFeet = chunkManager.isSolidAt(aheadX, feetY + 0.5f, aheadZ);
                    bool clearOneUp = !chunkManager.isSolidAt(aheadX, feetY + 1.0f, aheadZ);
                    bool clearAtHead = !chunkManager.isSolidAt(aheadX, y + eyeToHead, aheadZ);

                    if (blockedAtFeet && clearOneUp && clearAtHead) {
                        shouldJump = true;
                    }
                }
            }

            if (shouldJump) {
                velocityY = JUMP_VELOCITY;
                onGround = false;
            }
        }
    }

    // Entity.moveEntity: agachado y en el suelo no se avanza hacia un lado si debajo (a un bloque) no hay suelo; se frena de a 0,05
    if (sneaking && onGround) {
        float dx = velocityX, dz = velocityZ;
        while (dx != 0.0f && !collidesAt(chunkManager, x + dx, y - 1.0f, z)) {
            if (dx < 0.05f && dx >= -0.05f) dx = 0.0f; else if (dx > 0.0f) dx -= 0.05f; else dx += 0.05f;
        }
        while (dz != 0.0f && !collidesAt(chunkManager, x, y - 1.0f, z + dz)) {
            if (dz < 0.05f && dz >= -0.05f) dz = 0.0f; else if (dz > 0.0f) dz -= 0.05f; else dz += 0.05f;
        }
        while (dx != 0.0f && dz != 0.0f && !collidesAt(chunkManager, x + dx, y - 1.0f, z + dz)) {
            if (dx < 0.05f && dx >= -0.05f) dx = 0.0f; else if (dx > 0.0f) dx -= 0.05f; else dx += 0.05f;
            if (dz < 0.05f && dz >= -0.05f) dz = 0.0f; else if (dz > 0.0f) dz -= 0.05f; else dz += 0.05f;
        }
        velocityX = dx;
        velocityZ = dz;
    }

    // BlockWeb.onEntityCollidedWithBlock + Entity.moveEntity: dentro de una telaraña el desplazamiento se multiplica (x 0,25 en horizontal,
    // x 0,05 en vertical) y la velocidad se anula despues. Se mira la caja del jugador achicada 0,001.
    bool inWeb = false;
    {
        const float wy0 = y - eyeToFeet + 0.001f, wy1 = y + eyeToHead - 0.001f;
        for (int cx = -1; cx <= 1 && !inWeb; cx += 2)
            for (int cz = -1; cz <= 1 && !inWeb; cz += 2)
                for (int s = 0; s < 3 && !inWeb; s++) {
                    const float wy = wy0 + (wy1 - wy0) * (float)s * 0.5f;
                    if (chunkManager.getBlockTypeAt(x + cx * (halfWidth - 0.001f), wy, z + cz * (halfWidth - 0.001f)) == BlockType::WEB) inWeb = true;
                }
    }
    if (inWeb) { velocityX *= 0.25f; velocityY *= 0.05f; velocityZ *= 0.25f; }

    const float yBefore = y;        // posY al empezar el tick (var3 en moveEntityWithHeading)
    bool collidedH = false;         // isCollidedHorizontally
    if (!collidesAt(chunkManager, x + velocityX, y, z)) {
        x += velocityX;
    } else if (tryStepUp(chunkManager, velocityX, 0.0f)) {
        x += velocityX;
    } else {
        velocityX = 0.0f;
        collidedH = true;
    }
    if (!collidesAt(chunkManager, x, y, z + velocityZ)) {
        z += velocityZ;
    } else if (tryStepUp(chunkManager, 0.0f, velocityZ)) {
        z += velocityZ;
    } else {
        velocityZ = 0.0f;
        collidedH = true;
    }

    if (onLadder && collidedH) velocityY = 0.2f;   // isCollidedHorizontally && isOnLadder: trepa

    // BlockSoulSand.onEntityCollidedWithBlock: sobre la arena de almas la velocidad horizontal se multiplica por 0.4 cada tick
    // (el original lo hace porque los pies quedan 2/16 dentro del bloque; aquí se mira el bloque que pisás)
    if (onGround && chunkManager.getBlockTypeAt(x, y - eyeToFeet - 0.05f, z) == BlockType::SOUL_SAND) {
        velocityX *= 0.4f;
        velocityZ *= 0.4f;
    }

    const float STEP = 0.02f;
    float direction = (velocityY > 0) ? 1.0f : -1.0f;
    float testY = y;
    float remaining = fabsf(velocityY);
    bool collided = false;

    while (remaining > 0.0f) {
        float stepAmount = (remaining < STEP) ? remaining : STEP;
        float nextY = testY + direction * stepAmount;

        if (collidesAt(chunkManager, x, nextY, z)) {
            collided = true;
            break;
        }

        testY = nextY;
        remaining -= stepAmount;
    }

    y = testY;

    if (collided) {
        if (velocityY < 0) {
            onGround = true;
        }
        velocityY = 0.0f;
    } else {
        onGround = false;
    }

    if (inWeb) { velocityX = 0.0f; velocityY = 0.0f; velocityZ = 0.0f; }   // Entity.moveEntity: motionX/Y/Z = 0 tras moverse en la telaraña

    // Entity.updateFallState: al tocar suelo la distancia acumulada queda pendiente (main la convierte en daño); en el aire suma lo que bajó
    if (onGround) {
        if (fallDistance > 0.0f) { pendingFall = fallDistance; fallDistance = 0.0f; }
    } else if (y - yBefore < 0.0f) {
        fallDistance -= (y - yBefore);
    }

    currentTickY = y;
    currentTickX = x;
    currentTickZ = z;

    float speed2 = sqrtf(velocityX * velocityX + velocityZ * velocityZ);
    bobPhase += speed2 * 2.0f;
    bobPhase = fmodf(bobPhase, 2.0f * (float)M_PI);

    if (onGround) {
        bobStrength = speed2;
    } else {
        const float AIR_BOB_DECAY = 0.85f;
        bobStrength *= AIR_BOB_DECAY;
    }

    if (inWater) {
        velocityY = velocityY * WATER_DRAG - 0.02f; // moveEntityWithHeading: primero * 0.8, después - 0.02
    } else if (inLava) {
        velocityY = velocityY * LAVA_DRAG - 0.02f;   // moveEntityWithHeading en lava
    } else {
        velocityY = (velocityY - GRAVITY) * DRAG_Y;
    }

    // EntityLiving.moveEntityWithHeading: nadando contra el borde de un bloque, si la caja subida unos 0,6 queda sin
    // choques y sin líquido, el jugador sale impulsado hacia arriba (motionY = 0.3). Así se sube a un escalón desde el agua.
    if ((inWater || inLava) && collidedH) {
        const float drag = inWater ? WATER_DRAG : LAVA_DRAG;
        float dy = velocityY + 0.6f - (y - yBefore);
        float cx = x + velocityX * drag, cy = y + dy, cz = z + velocityZ * drag;
        if (!collidesAt(chunkManager, cx, cy, cz) &&
            !chunkManager.anyLiquidInBox(cx - halfWidth, cy - eyeToFeet, cz - halfWidth,
                                         cx + halfWidth, cy + eyeToHead, cz + halfWidth)) {
            velocityY = 0.3f;
        }
    }
}


void Player::getViewBob(float partialTick, float& bobX, float& bobY, float& bobRoll) const {
    (void)partialTick;

    // EntityPlayer.onLivingUpdate: la fuerza del balanceo es la velocidad horizontal ya con la fricción, con tope de 0.1
    float f2 = bobStrength * GROUND_FRICTION;
    if (f2 > 0.1f) f2 = 0.1f;

    bobX = sinf(bobPhase) * f2 * 0.5f;
    bobY = -fabsf(cosf(bobPhase)) * f2;
    bobRoll = sinf(bobPhase) * f2 * 3.0f;

    const float MAX_BOB_Y = 0.5f;
    if (bobY < -MAX_BOB_Y) bobY = -MAX_BOB_Y;

    const float MAX_BOB_ROLL = 2.0f;
    if (bobRoll > MAX_BOB_ROLL) bobRoll = MAX_BOB_ROLL;
    if (bobRoll < -MAX_BOB_ROLL) bobRoll = -MAX_BOB_ROLL;
}

void Player::handleTouch(bool touching, int touchX, int touchY) {
    if (touching) {
        if (wasTouching) {
            int dx = touchX - prevTouchX;
            int dy = touchY - prevTouchY;

            yaw -= dx * TOUCH_SENSITIVITY;
            pitch -= dy * TOUCH_SENSITIVITY;

            if (yaw >= 360.0f) yaw -= 360.0f;
            if (yaw < 0.0f) yaw += 360.0f;
            if (pitch > MAX_PITCH) pitch = MAX_PITCH;
            if (pitch < -MAX_PITCH) pitch = -MAX_PITCH;
        }
        prevTouchX = touchX;
        prevTouchY = touchY;
        wasTouching = true;
    } else {
        wasTouching = false;
    }
}

void Player::handleButtonLook(bool lookLeft, bool lookRight, bool lookUp, bool lookDown) {
    const float MAX_SPEED_DEG = 3.0f; // velocidad máxima, grados por frame
    const float ACCEL = 0.15f;        // qué tan rápido acelera hasta la velocidad máxima
    const float DECEL = 0.5f;         // qué tan rápido frena al soltar (más rápido que acelerar)

    bool horizontalHeld = lookLeft || lookRight;
    bool verticalHeld = lookUp || lookDown;

    if (horizontalHeld) {
        buttonLookSpeedH += ACCEL;
        if (buttonLookSpeedH > MAX_SPEED_DEG) buttonLookSpeedH = MAX_SPEED_DEG;
    } else {
        buttonLookSpeedH -= DECEL;
        if (buttonLookSpeedH < 0.0f) buttonLookSpeedH = 0.0f;
    }

    if (verticalHeld) {
        buttonLookSpeedV += ACCEL;
        if (buttonLookSpeedV > MAX_SPEED_DEG) buttonLookSpeedV = MAX_SPEED_DEG;
    } else {
        buttonLookSpeedV -= DECEL;
        if (buttonLookSpeedV < 0.0f) buttonLookSpeedV = 0.0f;
    }

    if (lookLeft)  yaw += buttonLookSpeedH;
    if (lookRight) yaw -= buttonLookSpeedH;
    if (lookUp)    pitch += buttonLookSpeedV;
    if (lookDown)  pitch -= buttonLookSpeedV;

    if (yaw >= 360.0f) yaw -= 360.0f;
    if (yaw < 0.0f) yaw += 360.0f;
    if (pitch > MAX_PITCH) pitch = MAX_PITCH;
    if (pitch < -MAX_PITCH) pitch = -MAX_PITCH;
}

bool Player::wouldOverlapBlock(float blockWorldX, float blockWorldY, float blockWorldZ) const {
    float pxmin = x - halfWidth, pxmax = x + halfWidth;
    float pymin = y - eyeToFeet, pymax = y + eyeToHead;
    float pzmin = z - halfWidth, pzmax = z + halfWidth;

    float bxmin = blockWorldX - 0.5f, bxmax = blockWorldX + 0.5f;
    float bymin = blockWorldY - 0.5f, bymax = blockWorldY + 0.5f;
    float bzmin = blockWorldZ - 0.5f, bzmax = blockWorldZ + 0.5f;

    bool overlapX = pxmin < bxmax && pxmax > bxmin;
    bool overlapY = pymin < bymax && pymax > bymin;
    bool overlapZ = pzmin < bzmax && pzmax > bzmin;

    return overlapX && overlapY && overlapZ;
}

// Teleporter: el jugador queda en la posición dada, quieto, y la cámara no interpola desde el sitio anterior
void Player::teleportTo(float nx, float ny, float nz) {
    x = nx; y = ny; z = nz;
    prevTickX = currentTickX = nx;
    prevTickY = currentTickY = ny;
    prevTickZ = currentTickZ = nz;
    velocityX = velocityY = velocityZ = 0.0f;
    onGround = false;
    fallDistance = 0.0f;
    pendingFall = 0.0f;
}

// EntityPlayer.onDeath: setSize(0.2, 0.2), motionY = 0.1, sin velocidad horizontal (no hay atacante) y yOffset = 0.1: los ojos bajan a 0,1 de los pies
void Player::setDead() {
    const float feetY = y - eyeToFeet;
    halfWidth = 0.1f;
    eyeToFeet = 0.1f;
    eyeToHead = 0.1f;
    y = feetY + eyeToFeet;
    velocityX = 0.0f;
    velocityZ = 0.0f;
    velocityY = 0.1f;
    onGround = false;
}

// Entity.updateRidden: el jugador sigue a la vagoneta. Sin velocidad propia ni caidas; la camara interpola del tick anterior al nuevo
void Player::rideTo(float ex, float ey, float ez) {
    prevTickX = currentTickX; prevTickY = currentTickY; prevTickZ = currentTickZ;
    prevEyeDrop = eyeDrop;
    x = ex; y = ey; z = ez;
    currentTickX = ex; currentTickY = ey; currentTickZ = ez;
    velocityX = velocityY = velocityZ = 0.0f;
    onGround = false;
    fallDistance = 0.0f;
    pendingFall = 0.0f;
}

// EntityLiving.knockBack: la velocidad se reduce a la mitad, sale despedido 0,4 en sentido contrario al golpe y sube hasta 0,4
void Player::knockBack(float attackerX, float attackerZ) {
    float dx = attackerX - x, dz = attackerZ - z;
    float d = sqrtf(dx * dx + dz * dz);
    if (d < 0.01f) { dx = 0.01f; dz = 0.0f; d = 0.01f; }
    velocityX /= 2.0f; velocityY /= 2.0f; velocityZ /= 2.0f;
    velocityX -= dx / d * 0.4f;
    velocityZ -= dz / d * 0.4f;
    velocityY += 0.4f;
    if (velocityY > 0.4f) velocityY = 0.4f;
}
