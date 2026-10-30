#ifndef STEVEMODEL_H
#define STEVEMODEL_H

#include <nds.h>
#include "BlockType.h"

// Modelo del jugador en tercera persona (ModelBiped + RenderPlayer de Beta 1.7.3) con la textura char.png.
// Solo hay vista trasera, asi que no se envia la cara frontal de cada pieza (nunca se ve): 30 poligonos en vez de 36.
namespace SteveModel {
    void init();                      // arma la geometria, una vez
    void reset(float yaw);            // al entrar a un mundo o reaparecer

    // Un tick de 20 Hz (EntityLiving.onUpdate): giro del cuerpo, balanceo de piernas y brazos
    void tick(float yaw, float movedX, float movedZ, float swingProgressNow, bool sneak);

    // Dibuja al jugador con la matriz de la camara ya puesta. feetX/feetY/feetZ: los pies; yaw y pitch del jugador (grados, como Player).
    void draw(float feetX, float feetY, float feetZ, float yaw, float pitch, float partialTick, float swingProgress,
              unsigned char lightLevel, bool hurt, int deathTime, bool sneak, BlockType held, bool riding = false,   // riding: sentado en una vagoneta o un bote
              int sleepDir = -1,   // sleepDir: 0 a 3 = acostado en una cama con esa direccion (feetY = los ojos - 0,2); -1 = no duerme
              const signed char* armor = nullptr);   // armadura puesta: material de casco, peto, pantalones y botas (0 cuero, 1 hierro, 2 diamante, 3 oro; -1 = nada)
}

#endif
