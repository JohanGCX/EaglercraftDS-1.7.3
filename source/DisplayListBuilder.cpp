#include "DisplayListBuilder.h"

DisplayListBuilder::DisplayListBuilder(u32* outBuf, u32 cap)
    : out(outBuf), wordIndex(0), capacity(cap), overflow(false), cmdCount(0), paramCount(0), vertexCount(0) {}

void DisplayListBuilder::writeWord(u32 word) {
    if (out) {
        if (wordIndex < capacity) out[1 + wordIndex] = word;
        else overflow = true;
    }
    wordIndex++;
}

void DisplayListBuilder::pushParam(u32 word) {
    paramQueue[paramCount++] = word;
}

void DisplayListBuilder::polyFormat(u32 attr) {
    pushCommand(FIFO_POLY_FORMAT);
    pushParam(attr);
    maybeFlush();
}

void DisplayListBuilder::flushGroup() {
    if (cmdCount == 0) return;

    u8 c0 = cmdQueue[0];
    u8 c1 = (cmdCount > 1) ? cmdQueue[1] : (u8)FIFO_NOP;
    u8 c2 = (cmdCount > 2) ? cmdQueue[2] : (u8)FIFO_NOP;
    u8 c3 = (cmdCount > 3) ? cmdQueue[3] : (u8)FIFO_NOP;

    writeWord(FIFO_COMMAND_PACK(c0, c1, c2, c3));

    for (int i = 0; i < paramCount; i++) {
        writeWord(paramQueue[i]);
    }

    cmdCount = 0;
    paramCount = 0;
}

void DisplayListBuilder::pushCommand(u8 cmd) {
    cmdQueue[cmdCount++] = cmd;
    // el flush ya NO pasa acá — se dispara en maybeFlush(), después de
    // que el llamador terminó de encolar los parámetros de este comando
}

void DisplayListBuilder::maybeFlush() {
    if (cmdCount == 4) flushGroup();
}

void DisplayListBuilder::begin(int primitive) {
    pushCommand(FIFO_BEGIN);
    pushParam((u32)primitive);
    maybeFlush();
}

void DisplayListBuilder::color3b(u8 r, u8 g, u8 b) {
    pushCommand(FIFO_COLOR);
    pushParam(RGB15(r >> 3, g >> 3, b >> 3));
    maybeFlush();
}

void DisplayListBuilder::texCoordPacked(u32 packedUV) {
    pushCommand(FIFO_TEX_COORD);
    pushParam(packedUV);
    maybeFlush();
}

void DisplayListBuilder::vertex(v16 x, v16 y, v16 z) {
    vertexCount++;
    // Formato compacto VERTEX10 (punto fijo 4.6, rango -8..+7.98): una sola
    // palabra por vértice en vez de dos. Dentro de una banda todas las
    // coordenadas están entre -0.5 y 7.5 y son múltiplos de 1/16, así que
    // la conversión desde v16 (4.12) es exacta: basta con correr 6 bits.
    u32 px = (u32)((x >> 6) & 0x3FF);
    u32 py = (u32)((y >> 6) & 0x3FF);
    u32 pz = (u32)((z >> 6) & 0x3FF);
    pushCommand(FIFO_VERTEX10);
    pushParam(px | (py << 10) | (pz << 20));
    maybeFlush();
}

void DisplayListBuilder::mtxPush() {
    pushCommand(0x11);
    maybeFlush();
}

void DisplayListBuilder::mtxPop(int n) {
    pushCommand(0x12);
    pushParam((u32)n);
    maybeFlush();
}

void DisplayListBuilder::mtxTrans(int x, int y, int z) {
    pushCommand(0x1C);
    pushParam((u32)x);
    pushParam((u32)y);
    pushParam((u32)z);
    maybeFlush();
}

void DisplayListBuilder::end() {
    pushCommand(FIFO_END);
    maybeFlush();
}

u32 DisplayListBuilder::finish() {
    flushGroup();
    if (out && !overflow) out[0] = wordIndex;
    return wordIndex;
}