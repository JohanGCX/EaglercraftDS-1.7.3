#ifndef MINECARTMODEL_H
#define MINECARTMODEL_H

#include "MinecartData.h"
#include "MinecartAI.h"

// Modelo de la vagoneta (ModelMinecart, textura cart.png de 64 x 32) y su dibujo (RenderMinecart). La vagoneta con cofre o con horno lleva ademas
// el bloque dentro, a 12/16 de su tamano.
namespace MinecartModel {
    void init();   // arma la geometria, una vez, despues de cargar las texturas

    // pose: lo que da MinecartAI::renderPose (coordenadas de rejilla). lightLevel: luz de 0 a 15. Se llama con la matriz de la camara ya puesta.
    void draw(const Minecart& c, const MinecartAI::Pose& pose, float partialTick, unsigned char lightLevel);
}

#endif
