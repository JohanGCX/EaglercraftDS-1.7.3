#ifndef HEALTHHUD_H
#define HEALTHHUD_H

#include "Health.h"

// Barra de vida, armadura y burbujas agrandada 2x en la esquina de abajo a la izquierda de la pantalla de arriba (ver HealthHudLayout.h)
namespace HealthHud {
    // Dibujar ANTES del terreno, como la mano: si el 3D se queda sin presupuesto de polígonos, lo que se pierde es lo último que se dibuja.
    // armorValue = puntos de armadura (0 si no hay), headInWater = la cabeza está bajo el agua (muestra las burbujas).
    void draw(const HealthState& s, int armorValue, bool headInWater);
}

#endif