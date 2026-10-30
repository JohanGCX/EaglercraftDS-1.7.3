#ifndef RENDERER_H
#define RENDERER_H

#include <nds.h>

class Renderer {
public:
    void init();
    void setupProjection();  // renombrado: ya no configura la cámara, solo proyección
};

#endif