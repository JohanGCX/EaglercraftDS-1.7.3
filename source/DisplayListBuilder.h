#ifndef DISPLAY_LIST_BUILDER_H
#define DISPLAY_LIST_BUILDER_H

#include <nds.h>

// Arma un display list de la GPU del DS en dos pasadas:
//  1) modo "contar" (out == nullptr): recorre la geometría sin escribir
//     nada, solo cuenta cuántas words va a ocupar el resultado.
//  2) modo "escribir" (out != nullptr): mismo recorrido exacto, esta vez
//     volcando los comandos empaquetados al buffer ya alocado del
//     tamaño exacto que devolvió la pasada de conteo.
// out[0] queda reservado para el tamaño total (lo escribe finish());
// el cuerpo del display list se escribe a partir de out[1].
class DisplayListBuilder {
public:
    // capacity: cuántas words de cuerpo caben en 'out' (sin contar la de tamaño). Si el display list no cabe, se sigue contando y overflowed() da true
    // (sirve para armar la banda en una sola pasada sobre un búfer de trabajo y copiarla al tamaño justo; solo si no cabe se arma de nuevo).
    explicit DisplayListBuilder(u32* out, u32 capacity = 0xFFFFFFFFu);

    void begin(int primitive);          // GL_QUADS, etc.
    void color3b(u8 r, u8 g, u8 b);      // mismo cálculo que glColor3b
    void texCoordPacked(u32 packedUV);   // ya armado con TEXTURE_PACK(...)
    void vertex(v16 x, v16 y, v16 z);
    void end();
    // comandos de matriz dentro de la lista (MTX_PUSH 0x11, MTX_POP 0x12, MTX_TRANS 0x1C): x, y, z en punto fijo 20.12
    void mtxPush();
    void mtxPop(int n = 1);
    void mtxTrans(int x, int y, int z);

    // Cierra el último grupo de comandos (rellena con NOP si hace
    // falta) y, en modo escribir, deja el total de words en out[0].
    // Devuelve ese total (words de cuerpo, sin contar la word de
    // tamaño) — es lo que hay que alocar como out[1 + total].
    u32 finish();
    u32 getVertexCount() const { return vertexCount; }
    bool isCounting() const { return out == nullptr; }
    bool overflowed() const { return overflow; }
    void polyFormat(u32 attr);

private:
    u32* out;
    u32 wordIndex;
    u32 capacity;
    bool overflow;

    u8 cmdQueue[4];
    int cmdCount;
    u32 paramQueue[12];
    int paramCount;
    u32 vertexCount;

    void writeWord(u32 word);
    void pushParam(u32 word);
    void pushCommand(u8 cmd);
    void flushGroup();
    void maybeFlush();
};

#endif