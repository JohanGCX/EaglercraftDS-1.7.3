#ifndef DROPPEDITEM_H
#define DROPPEDITEM_H

#include "BlockType.h"

class ChunkManager;

class DroppedItem {
public:
    void spawn(float x, float y, float z, BlockType type, bool isThrown = false,
               float throwVelX = 0.0f, float throwVelY = 0.0f, float throwVelZ = 0.0f,
               int durability = -1, int stackCount = 1);

    int getDurability() const { return durability; }
    int getCount() const { return count; }
    void update(const ChunkManager& chunkManager);
    void draw(float playerX, float playerY, float playerZ, float playerYaw, float partialTick) const;

    bool isActive() const { return active; }
    float getX() const { return x; }
    float getY() const { return y; }
    float getZ() const { return z; }
    BlockType getType() const { return type; }
    bool canBePickedUp(float playerX, float playerFeetY, float playerZ) const;

    void startPickupAnimation(float targetX, float targetY, float targetZ);
    bool consumePickupComplete();

   // Fusión de pilas cercanas (como en versiones posteriores del original; ahorra entidades en la DS)
    bool isResting() const { return active && !pickingUp && !settling; }
    bool canMergeWith(const DroppedItem& o) const {
        if (!isResting() || !o.isResting()) return false;
        if (type != o.type) return false;
        if (count + o.count > maxStackFor(type)) return false;
        float dx = x - o.x, dy = y - o.y, dz = z - o.z;
        return dx * dx + dz * dz <= 1.0f && dy * dy <= 0.25f;
    }
    void absorb(DroppedItem& o) { count += o.count; o.active = false; }

private:
    bool active = false;
    float x, y, z;
    BlockType type;
    int durability = -1;
    int count = 1;
    int health = 5;   // EntityItem.health

    float rotationPhase = 0.0f;
    float bobPhase = 0.0f;
    int ageTicks = 0;

    bool settling = false;
    float velocityX = 0.0f;
    float velocityY = 0.0f;
    float velocityZ = 0.0f;

    static constexpr float ITEM_GRAVITY = 0.04f;
    static constexpr float ITEM_DRAG = 0.98f;

    static constexpr float ROTATION_SPEED = 2.87675f;
    static constexpr float BOB_SPEED = (3.14159265f / 20.0f);
    static constexpr float BOB_MIN = 0.0625f;
    static constexpr float BOB_MAX = 0.2625f;
    static constexpr float ITEM_SCALE = 0.25f;
    static constexpr float FLAT_ITEM_SCALE = 0.5f;
    static constexpr int MAX_STACK_COUNT = 64;

    static constexpr int PICKUP_DELAY_TICKS = 10;        // 0.5s — ítems normales (romper bloques)
    static constexpr int THROWN_PICKUP_DELAY_TICKS = 40; // 2s — ítems tirados manualmente
    static constexpr int DESPAWN_TICKS = 6000;
    static constexpr float PICKUP_RADIUS_H = 1.6f + 0.125f;
    static constexpr float PLAYER_HEIGHT = 1.8f;

    bool pickingUp = false;
    int pickupAnimTicks = 0;
    float pickupTargetX, pickupTargetY, pickupTargetZ;
    bool pickupCompleteFlag = false;

    float prevX = 0.0f, prevY = 0.0f, prevZ = 0.0f;
    float prevRotationPhase = 0.0f;

    static constexpr int PICKUP_ANIM_DURATION = 4;

    int pickupDelayTicks = PICKUP_DELAY_TICKS;
};

#endif