#ifndef BOATMODEL_H
#define BOATMODEL_H

#include "BoatData.h"

// Modelo del bote (ModelBoat, textura boat.png de 64 x 32) y su dibujo (RenderBoat): el fondo y cuatro costados de madera.
namespace BoatModel {
    void init();   // arma la geometria, una vez, despues de cargar las texturas

    // (x, y, z): el centro del bote ya interpolado, en coordenadas de rejilla. yawDeg: su giro. lightLevel: luz de 0 a 15.
    void draw(const Boat& b, double x, double y, double z, float yawDeg, float partialTick, unsigned char lightLevel);
}

#endif
