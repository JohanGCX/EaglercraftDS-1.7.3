#ifndef HEALTH_H
#define HEALTH_H

#include <math.h>

// =====================================================================
// Vida del jugador de la Beta 1.7.3 (EntityLiving / Entity / EntityPlayer), sin armas ni mobs: daño por caída, ahogo, lava, fuego,
// cactus y asfixia, con la invulnerabilidad de 10 ticks, y la regeneración del modo pacífico. Un tick = 1/20 s.
// Al llegar a 0 corazones el jugador muere (dead = true): ya no regenera ni recibe daño; main suelta el inventario y muestra Game Over.
// =====================================================================
struct HealthState {
    int health = 20;            // puntos de vida (medios corazones), 0..20
    int prevHealth = 20;        // la vida antes del último golpe (para el destello de los corazones)
    int heartsLife = 0;         // ticks de invulnerabilidad que quedan (arranca en 20 con cada golpe)
    int lastDamage = 0;         // el daño del último golpe (field_9346_af)
    int hurtTime = 0;           // ticks que quedan del temblor de cámara
    int maxHurtTime = 10;
    float attackedAtYaw = 0.0f; // sin atacante: 0 o 180 al azar (decide hacia qué lado se inclina la cámara)
    int air = 300;              // aire bajo el agua (EntityLiving.air)
    int fire = 0;               // ticks que le quedan ardiendo
    int ticksExisted = 0;
    int hudTicks = 0;           // GuiIngame.updateCounter: sigue contando tambien despues de morir (los corazones siguen temblando)
    unsigned int rng = 12345u;
    bool enabled = true;        // false: el daño se ignora (la barra y la regeneración siguen), o sea, no hay muerte
    bool dead = false;          // murió: EntityLiving.health <= 0
    int armorPoints = 0;        // InventoryPlayer.getTotalArmorValue (main lo actualiza cada tick)
    int damageRemainder = 0;    // EntityPlayer.damageRemainder: lo que sobra de dividir el daño entre 25
    int armorWear[8];           // golpes que desgastan la armadura y main todavia no aplico (cantidad de daño de cada uno)
    int armorWearCount = 0;
    bool isPlayer = false;      // el jugador: el daño depende de la dificultad (EntityPlayer.attackEntityFrom)
    int difficulty = 2;         // 0 pacifico (sin daño), 1 facil (daño / 3 + 1), 2 normal, 3 dificil (daño x 3 / 2)
    int deathTime = 0;          // ticks desde la muerte; llega a 21 y se queda ahí (la entidad muerta deja de actualizarse)
};

namespace Health {
    const int MAX_HEALTH = 20;
    const int HEARTS_HALVES_LIFE = 20;   // EntityLiving.heartsHalvesLife
    const int MAX_AIR = 300;

    inline int randomBit(HealthState& s) { s.rng = s.rng * 1664525u + 1013904223u; return (int)((s.rng >> 24) & 1u); }

    // EntityPlayer.damageEntity: la armadura reduce el daño (25 - armadura) / 25, con el resto acumulado, y se desgasta
    inline int applyArmor(HealthState& s, int amount) {
        const int v = amount * (25 - s.armorPoints) + s.damageRemainder;
        if (s.armorPoints > 0 && s.armorWearCount < 8) s.armorWear[s.armorWearCount++] = amount;
        s.damageRemainder = v % 25;
        return v / 25;
    }

    // EntityLiving.attackEntityFrom sin atacante. Devuelve true si el daño se aplicó.
    inline bool attack(HealthState& s, int amount) {
        if (!s.enabled) return false;
        if (s.health <= 0) return false;                 // en 0 corazones no se acumula más daño
        if (s.isPlayer) {                                // EntityPlayer.attackEntityFrom: la dificultad escala todo el daño
            if (s.difficulty == 1) amount = amount / 3 + 1;
            else if (s.difficulty >= 3) amount = amount * 3 / 2;
        }
        bool newHit = true;
        if ((float)s.heartsLife > (float)HEARTS_HALVES_LIFE / 2.0f) {
            if (amount <= s.lastDamage) return false;    // dentro de la invulnerabilidad solo cuenta lo que supere al golpe anterior
            s.health -= applyArmor(s, amount - s.lastDamage);
            s.lastDamage = amount;
            newHit = false;
        } else {
            s.lastDamage = amount;
            s.prevHealth = s.health;
            s.heartsLife = HEARTS_HALVES_LIFE;
            s.health -= applyArmor(s, amount);
            s.hurtTime = s.maxHurtTime = 10;
        }
        s.attackedAtYaw = 0.0f;
        if (newHit) s.attackedAtYaw = (float)(randomBit(s) * 180);   // sin atacante: (int)(Math.random() * 2) * 180
        if (s.health <= 0) { s.health = 0; s.dead = true; }
        return true;
    }

    // EntityLiving.heal: un jugador muerto no se cura
    inline void heal(HealthState& s, int amount) {
        if (s.health <= 0) return;
        s.health += amount;
        if (s.health > MAX_HEALTH) s.health = MAX_HEALTH;
        s.heartsLife = HEARTS_HALVES_LIFE / 2;
    }

    struct Env {
        bool inWater = false;      // handleWaterMovement (la caja achicada toca agua)
        bool inLava = false;       // handleLavaMovement
        bool headInWater = false;  // isInsideOfMaterial(Material.water)
        bool insideBlock = false;  // isEntityInsideOpaqueBlock
        bool burningBox = false;   // isBoundingBoxBurning: fuego o lava en la caja
        bool cactus = false;       // onEntityCollidedWithBlock de un cactus
        float fallLanded = 0.0f;   // distancia de caída que se aplica al tocar suelo en este tick (0 = no aterrizó)
    };

    // Un tick, en el orden del original: Entity.onEntityUpdate, EntityLiving.onEntityUpdate, Entity.moveEntity y EntityPlayer.onLivingUpdate
    inline void tick(HealthState& s, const Env& e) {
        ++s.hudTicks;
        if (s.dead) { // EntityLiving.onEntityUpdate con health <= 0: solo corre la cuenta de la muerte, hasta deathTime = 21
            if (s.hurtTime > 0) --s.hurtTime;
            if (s.heartsLife > 0) --s.heartsLife;
            if (s.deathTime <= 20) ++s.deathTime;
            return;
        }
        ++s.ticksExisted;

        // Entity.onEntityUpdate: el agua apaga el fuego; arder duele 1 cada 20 ticks; la lava duele 4 y prende por 600 ticks
        if (e.inWater) s.fire = 0;
        if (s.fire > 0) {
            if (s.fire % 20 == 0) attack(s, 1);
            --s.fire;
        }
        if (e.inLava) {
            attack(s, 4);
            s.fire = 600;
        }

        // EntityLiving.onEntityUpdate: asfixia, aire bajo el agua, y las cuentas de invulnerabilidad y temblor
        if (s.health > 0 && e.insideBlock) attack(s, 1);
        if (s.health > 0 && e.headInWater) {
            --s.air;
            if (s.air == -20) {
                s.air = 0;
                attack(s, 2);
            }
            s.fire = 0;
        } else {
            s.air = MAX_AIR;
        }
        if (s.hurtTime > 0) --s.hurtTime;
        if (s.heartsLife > 0) --s.heartsLife;

        // Entity.moveEntity: caída, cactus y fuego
        if (e.fallLanded > 0.0f) {
            int d = (int)ceilf(e.fallLanded - 3.0f);
            if (d > 0) attack(s, d);
        }
        if (e.cactus) attack(s, 1);
        if (e.burningBox) {
            attack(s, 1);
            if (!e.inWater) {
                ++s.fire;
                if (s.fire == 0) s.fire = 300;
            }
        } else if (s.fire <= 0) {
            s.fire = -1;   // fireResistance
        }

        // EntityPlayer.onLivingUpdate (modo pacífico): +1 punto cada 20 ticks (el original lo escribe como 'ticksExisted % 20 * 12 == 0')
        // (la regeneracion del modo pacifico la hace main.cpp solo con difficulty==0)
    }

    // EntityRenderer.hurtCameraEffect: ángulo de inclinación de la cámara (grados) para un cuadro intermedio 'partial' (0..1)
    inline float hurtCameraRoll(const HealthState& s, float partial) {
        float t = (float)s.hurtTime - partial;
        if (t < 0.0f) return 0.0f;
        t /= (float)s.maxHurtTime;
        float k = sinf(t * t * t * t * 3.14159265f);
        // el original gira -k*14 grados sobre el eje de la mirada, entre dos giros de +-attackedAtYaw sobre Y: con 180 se invierte el sentido
        return (s.attackedAtYaw >= 90.0f ? 1.0f : -1.0f) * k * 14.0f;
    }

    // EntityRenderer.hurtCameraEffect con health <= 0: la cámara se inclina de 0 a casi 40 grados (con deathTime = 21 son unos 3,8)
    inline float deathCameraRoll(const HealthState& s, float partial) {
        if (!s.dead) return 0.0f;
        return 40.0f - 8000.0f / ((float)s.deathTime + partial + 200.0f);
    }

    // EntityRenderer.getFOVModifier con health <= 0: el FOV se divide por este factor (1 al morir, 1,08 con deathTime = 21: el "minizoom")
    inline float deathFovFactor(const HealthState& s, float partial) {
        if (!s.dead) return 1.0f;
        const float t = (float)s.deathTime + partial;
        return (1.0f - 500.0f / (t + 500.0f)) * 2.0f + 1.0f;
    }
}

#endif