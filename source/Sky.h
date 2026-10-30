#ifndef SKY_H
#define SKY_H

// Sol, luna y resplandor del amanecer y el atardecer (RenderGlobal.renderSky de la Beta 1.7.3). Solo en modos ds y dsi.
namespace Sky {
    // angle = calculateCelestialAngle; rain 0..1; sky* = el color de fondo actual (0..31) hacia el que se desvanece el resplandor
    void draw(float ex, float ey, float ez, float angle, float rain, int skyR, int skyG, int skyB);
}

#endif
