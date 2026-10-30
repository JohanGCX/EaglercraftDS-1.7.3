#ifndef SHEEPMODEL_H
#define SHEEPMODEL_H

#include "SheepData.h"

// Modelo de la oveja (ModelSheep2 = piel con sheep.png, ModelSheep1 = lana con sheep_fur.png) y su dibujo (RenderSheep / RenderLiving).
namespace SheepModel {
    void init();   // arma la geometría, una vez, después de cargar la textura

    // Dibuja una oveja. lightLevel: luz (0 a 15) a la altura de su cuerpo. Se llama con la matriz de la cámara ya puesta.
    void draw(const Sheep& s, float partialTick, unsigned char lightLevel);
}

#endif