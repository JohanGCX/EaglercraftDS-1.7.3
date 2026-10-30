#ifndef CLOUDS_H
#define CLOUDS_H

// Nubes (RenderGlobal.renderCloudsFancy de la Beta 1.7.3): la forma sale de clouds.png (CloudsData.h), cada pixel es un cuadro de 12 x 12 bloques de 4 de grosor a y = 108,
// que se mueven 0,6 bloques por segundo. Color plano (sin textura), con el 80 % de opacidad, y sin niebla. Para gastar poco: solo se arman las celdas a 5 cuadros a la redonda,
// fusionadas en rectangulos por fila, y las paredes solo cerca. Se ven desde el suelo solo en el modo dsi; en los demas, solo al subir cerca de su altura.
namespace Clouds {
    void reset();
    void tick();   // 20 Hz
    // Con la matriz de la camara puesta. (ex, ey, ez): ojos del jugador; sky: brillo del cielo 0..1; rain / thunder: 0..1; dsiMode: modo de chunks dsi
    void draw(float partialTick, float ex, float ey, float ez, float sky, float rain, float thunder, bool dsiMode);
}

#endif
