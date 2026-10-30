#include "HealthHud.h"
#include "HealthHudLayout.h"
#include "Texture.h"
#include <nds.h>

// El plano de la barra queda a Z0/4096 de la cámara. Con Z0 = 686 (0,1675) y el campo de visión de 70 grados de Renderer::setupProjection,
// cada píxel de pantalla son exactamente 5 unidades v16: las esquinas de los íconos caen en coordenadas enteras, sin errores de redondeo.
static const int Z0 = 686;
static const int UNITS_PER_PIXEL = 5;
// La DS rellena también el último píxel del borde derecho e inferior de un polígono: un ícono de 18 px sale de 19. Se achica ese borde en
// EDGE_INSET píxeles (0 = sin corrección). Si algún día se ve cortado un píxel, bajar a 0.
static const int EDGE_INSET = 1;
static const int LAYER_STEP = 2;   // cada capa (fondo, destello, relleno) queda 2 unidades v16 más cerca: el mismo píxel se puede dibujar encima

void HealthHud::draw(const HealthState& s, int armorValue, bool headInWater) {
    HudQuad quads[HealthHudLayout::MAX_QUADS];
    const int count = HealthHudLayout::build(s, armorValue, headInWater, s.hudTicks, quads);
    if (count <= 0) return;

    glPolyFmt(POLY_ALPHA(31) | POLY_CULL_NONE | POLY_ID(9)); // sin niebla
    glBindTexture(0, Texture::iconsTextureID);
    glColor3b(255, 255, 255);

    // La mano comprime su profundidad contra el plano cercano (HandAnimations::kDepthSquashMatrix: NDC z entre -1 y -0.75) para quedar delante
    // del mundo. La barra, a 0,1675 de la camara, caia detras de ella y el brazo la tapaba. Aqui se hace lo mismo con la barra, un poco mas cerca
    // (z' = 0.5 z - 0.825: cae en torno a -0.92). La proyeccion es la de Renderer::setupProjection; las capas siguen ordenadas entre si.
    static const m4x4 kHudDepth = {{
        inttof32(1), 0, 0, 0,
        0, inttof32(1), 0, 0,
        0, 0, floattof32(0.5f), 0,
        0, 0, floattof32(-0.825f), inttof32(1)
    }};
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glMultMatrix4x4(&kHudDepth);
    gluPerspective(70, 256.0 / 192.0, 0.1f, 40.0f);   // los mismos NEAR_PLANE y FAR_PLANE de Renderer.cpp

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
        glLoadIdentity();
        glTranslatef(0.0f, 0.0f, -(float)Z0 / 4096.0f);
        glBegin(GL_QUADS);
        for (int i = 0; i < count; i++) {
            const HudQuad& q = quads[i];
            const int dist = Z0 - LAYER_STEP * q.layer;  // la capa más cercana se achica para seguir cayendo en el mismo píxel
            const int z = LAYER_STEP * q.layer;

            // píxel de pantalla -> unidades de cámara (x a la derecha, y hacia arriba, centro de la pantalla en (128, 96))
            int px[2] = { q.x, q.x + HealthHudLayout::ICON - EDGE_INSET };
            int py[2] = { q.y, q.y + HealthHudLayout::ICON - EDGE_INSET };
            short cx[2], cy[2];
            for (int k = 0; k < 2; k++) {
                int a = (px[k] - 128) * UNITS_PER_PIXEL * dist;
                int b = (96 - py[k]) * UNITS_PER_PIXEL * dist;
                cx[k] = (short)((a + (a >= 0 ? Z0 / 2 : -Z0 / 2)) / Z0);
                cy[k] = (short)((b + (b >= 0 ? Z0 / 2 : -Z0 / 2)) / Z0);
            }

            // coordenadas de textura en 12.4: el borde lejano queda 1/16 de texel antes del final del ícono, para no tomar el texel de al lado
            const int u0 = q.u << 4, v0 = q.v << 4, u1 = ((q.u + 9) << 4) - 1, v1 = ((q.v + 9) << 4) - 1;
            GFX_TEX_COORD = TEXTURE_PACK(u0, v1); glVertex3v16(cx[0], cy[1], z);
            GFX_TEX_COORD = TEXTURE_PACK(u1, v1); glVertex3v16(cx[1], cy[1], z);
            GFX_TEX_COORD = TEXTURE_PACK(u1, v0); glVertex3v16(cx[1], cy[0], z);
            GFX_TEX_COORD = TEXTURE_PACK(u0, v0); glVertex3v16(cx[0], cy[0], z);
        }
        glEnd();
    glPopMatrix(1);
    glMatrixMode(GL_PROJECTION);
    glPopMatrix(1);
    glMatrixMode(GL_MODELVIEW);

    glBindTexture(0, Texture::textureID);                        // lo siguiente (terreno) vuelve a su atlas
    glPolyFmt(POLY_ALPHA(31) | POLY_CULL_NONE | POLY_FOG);       // el formato base de Renderer::setupProjection
}