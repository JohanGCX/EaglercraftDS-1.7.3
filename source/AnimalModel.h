#ifndef ANIMALMODEL_H
#define ANIMALMODEL_H

#include "AnimalData.h"

// Modelos de la vaca (ModelCow, cow.png), el cerdo (ModelPig, pig.png) y la gallina (ModelChicken, chicken.png), todas de 64 x 32, y su dibujo (RenderLiving).
namespace AnimalModel {
    void init();   // arma la geometria, una vez, despues de cargar las texturas

    // Dibuja un animal. lightLevel: luz (0 a 15) a la altura de su cuerpo. Se llama con la matriz de la camara ya puesta.
    void draw(const Animal& a, float partialTick, unsigned char lightLevel);
}

#endif
