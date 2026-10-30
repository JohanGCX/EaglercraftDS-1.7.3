#ifndef HEALTHHUDLAYOUT_H
#define HEALTHHUDLAYOUT_H

#include "Health.h"
#include "Lakes.h"

// =====================================================================
// Barra de vida, armadura y burbujas (GuiIngame.renderGameOverlay) agrandada 2x, abajo a la izquierda de la pantalla de arriba (256 x 192):
// corazones abajo, armadura encima y burbujas arriba. Coordenadas de pantalla en píxeles; celdas de icons.png en texeles.
// =====================================================================
struct HudQuad {
    short x, y;       // esquina superior izquierda en pantalla
    short u, v;       // esquina superior izquierda del ícono de 9 x 9 en icons.png
    unsigned char layer;   // 0 fondo, 1 destello, 2 relleno (capas que se apilan en el mismo lugar)
};

namespace HealthHudLayout {
    const int SCALE = 2;                 // 9 x 9 -> 18 x 18
    const int ICON = 9 * SCALE;          // tamaño de cada ícono en pantalla
    const int STRIDE = 8 * SCALE;        // separación entre íconos (los de la Beta se pisan 1 px)
    const int LEFT = 4;
    const int HEARTS_Y = 192 - 4 - ICON; // fila de los corazones
    const int ROW_STEP = ICON + 2;       // cada fila sube un ícono y 2 px
    const int MAX_QUADS = 64;

    // GuiIngame: 'headInWater' = isInsideOfMaterial(water); 'armor' = getPlayerArmorValue (0 si no hay armadura); 'updateCounter' = contador de ticks
    inline int build(const HealthState& s, int armor, bool headInWater, int updateCounter, HudQuad* out) {
        int n = 0;
        bool flash = (s.heartsLife / 3) % 2 == 1;
        if (s.heartsLife < 10) flash = false;

        Lakes::JRandom rnd((long long)(int)((unsigned int)updateCounter * 312871u));   // this.rand.setSeed(updateCounter * 312871)

        const int armorY = HEARTS_Y - ROW_STEP;
        for (int i = 0; i < 10; i++) {
            if (armor > 0) {
                int u = 16;                                   // vacío
                if (i * 2 + 1 < armor) u = 34;                // lleno
                else if (i * 2 + 1 == armor) u = 25;          // medio
                out[n++] = { (short)(LEFT + i * STRIDE), (short)armorY, (short)u, 9, 0 };
            }

            int y = HEARTS_Y;
            if (s.health <= 4) y += rnd.nextInt(2) * SCALE;   // con 4 o menos de vida los corazones tiemblan
            const int x = LEFT + i * STRIDE;
            out[n++] = { (short)x, (short)y, (short)(16 + (flash ? 9 : 0)), 0, 0 };
            if (flash) {
                if (i * 2 + 1 < s.prevHealth) out[n++] = { (short)x, (short)y, 70, 0, 1 };
                if (i * 2 + 1 == s.prevHealth) out[n++] = { (short)x, (short)y, 79, 0, 1 };
            }
            if (i * 2 + 1 < s.health) out[n++] = { (short)x, (short)y, 52, 0, 2 };
            if (i * 2 + 1 == s.health) out[n++] = { (short)x, (short)y, 61, 0, 2 };
        }

        if (headInWater) {
            const int bubbleY = (armor > 0 ? armorY : HEARTS_Y) - ROW_STEP;
            int full = (int)ceil((double)(s.air - 2) * 10.0 / 300.0);
            int popped = (int)ceil((double)s.air * 10.0 / 300.0) - full;
            for (int i = 0; i < full + popped && i < 20; i++) {
                out[n++] = { (short)(LEFT + i * STRIDE), (short)bubbleY, (short)(i < full ? 16 : 25), 18, 0 };
            }
        }
        return n;
    }
}

#endif