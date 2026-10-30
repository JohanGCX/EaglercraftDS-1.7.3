#ifndef HAND_ANIMATIONS_H
#define HAND_ANIMATIONS_H

#include <nds.h>
#include "BlockType.h"

class ChunkManager;

// Port de la mano en primera persona de Minecraft Beta 1.7.3:
// brazo, bloque en la mano, ítem 2D extruido, swing y balanceo al caminar.
namespace HandAnimations {

    // Datos de un tick de 20 Hz (lo que en el original sale de EntityPlayer)
    struct TickInput {
        BlockType heldType;      // lo que hay en el slot seleccionado (AIR = mano vacía)
        int heldDurability;      // durabilidad si es herramienta (0 si no)
        int heldSlot;            // índice del slot seleccionado
        bool attackHeld;         // botón de romper mantenido (y sin menú abierto)
        bool targetingBlock;     // la mira apunta a un bloque
        float velX, velY, velZ;  // velocidad en bloques por tick
        float movedX, movedZ;    // desplazamiento real de este tick
        bool onGround;
        bool sneaking;           // agachado: en el suelo no hay balanceo de la mano
    };

    // Datos de cada frame de dibujo
    struct RenderInput {
        float partialTicks;      // 0..1 entre el último tick y el siguiente
        float pitch, yaw;        // de Player, en grados
        u8 eyeLight;             // nivel de luz (0..15) en el bloque de los ojos
        u8 bodyLight;            // nivel de luz (0..15) a 0,66 de la altura del cuerpo
        float posX, posZ;        // posicion del jugador (para la marca del mapa)
        int dimension;           // 0 o -1
    };

    void reset();                              // al entrar a un mundo

    // Eventos de entrada: se llaman en el frame en que ocurren y se aplican en el próximo tick
    void onAttackPressed(bool targetingBlock); // se apretó el botón de romper
    void onBlockPlaced(bool stackEmptied);     // placeBlockAt devolvió true

    void tick(const TickInput& in);            // 20 Hz: swing, equippedProgress, balanceo
    void render(const RenderInput& in);        // proyección propia + brazo, bloque o ítem 2D

    // El mismo balanceo que la mano, para la cámara del mundo
    // (en el original se aplica a los dos, antes de orientar la cámara)
    void applyWorldViewBobbing(float partialTicks);

    // Progreso del golpe (0 = quieto, hasta 1), para animar el brazo del modelo en tercera persona
    float getSwingProgress(float partialTicks);

    // Objeto en la mano del modelo en tercera persona (RenderBiped.renderEquippedItems). Se llama con la matriz del brazo derecho ya puesta.
    void setupWorldItemLights();                       // llamar con solo la matriz de la camara
    void renderItemThirdPerson(BlockType t, u8 light); // transformacion segun el tipo y dibujo

    // Nivel de luz (0..15) en un punto del mundo, como World.getBlockLightValue
    u8 lightLevelAt(const ChunkManager& cm, float x, float y, float z);
}

#endif
