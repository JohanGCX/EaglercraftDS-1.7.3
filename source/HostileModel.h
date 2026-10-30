#ifndef HOSTILEMODEL_H
#define HOSTILEMODEL_H

#include "HostileData.h"

// Modelos de los hostiles (ModelZombie / ModelSkeleton sobre ModelBiped, ModelCreeper, ModelSpider) y de la flecha, y su dibujo (RenderLiving). Todas las texturas
// son de 64 x 32. El sombrero de las cabezas (ModelBiped.bipedHeadwear) no se dibuja: en el zombie y el esqueleto es una zona vacia.
namespace HostileModel {
    void init();   // arma la geometria, una vez

    // Dibuja un mob. lightLevel: luz (0 a 15) a la altura de su cuerpo. Se llama con la matriz de la camara ya puesta.
    void draw(const Mob& m, float partialTick, unsigned char lightLevel);
    void drawArrow(const Arrow& a, float partialTick, unsigned char lightLevel);
    void drawFireball(const Fireball& f, float partialTick);
    void drawFire(float x, float y, float z, float width, float height);   // las llamas animadas (Render.renderEntityOnFire) para animales y ovejas; (x, y, z) = pies
    void setCamera(float yaw, float pitch);   // hacia donde mira el jugador (la bola de fuego es un sprite que lo mira)
}

#endif
