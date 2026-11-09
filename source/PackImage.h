#ifndef PACKIMAGE_H
#define PACKIMAGE_H

// Lectura de los texture packs de la Beta (zip o carpeta) sin libnds: inflate (deflate de zip y de PNG), PNG a 256 x 256 RGB15 y reduccion a una
// paleta de 256 colores (median cut). Todo en C++ plano: se puede probar en una PC. Lo usa TexturePack.cpp.
#include <stdint.h>
#include <stdio.h>

namespace PackImage {
    // Inflate (RFC 1951, sin cabecera zlib). rd devuelve el siguiente byte o -1 al terminarse; wr recibe la salida en pedazos (false = cortar).
    // Usa unos 34 KB de memoria dinamica mientras dura. true si el flujo termino bien (o wr pidio cortar despues de recibir datos).
    bool inflate(int (*rd)(void*), void* rctx, bool (*wr)(void*, const uint8_t*, int), void* wctx);

    // PNG (en memoria) a outW x outH pixeles RGB15: bit 15 = opaco, 0 = transparente (alfa < 128). El PNG puede medir 1, 2, 4 u 8 veces eso
    // (packs 16x, 32x, 64x, 128x: los grandes se reducen promediando). Tipos de color 0, 2, 3, 4 y 6 de 8 bits (paleta tambien de 1, 2 y 4), sin entrelazado.
    bool decodePng(const uint8_t* png, uint32_t len, uint16_t* out, int outW, int outH);

    // Reduce los pixeles (formato de decodePng) a palEntries colores: el indice 0 queda para lo transparente (magenta, como grit -gT FF00FF).
    bool quantize(const uint16_t* px, int count, uint8_t* outIdx, uint16_t* outPal, int palEntries);

    // Zip: busca los archivos del pack en el directorio central.
    struct ZipItem { uint32_t method, flags, csize, usize, localOff; bool found; };
    // items[i] = names[i] (ruta exacta dentro del zip, p. ej. "gui/items.png"); found = false si no esta, esta cifrado o no es deflate / stored
    bool zipIndex(FILE* f, const char* const* names, int count, ZipItem* items);
    // Saca un archivo del zip a memoria nueva (malloc; lo libera quien llama). nullptr si no se puede o pasa de maxBytes.
    uint8_t* zipExtract(FILE* f, const ZipItem& it, uint32_t maxBytes, uint32_t& outLen);

    // grasscolor.png / foliagecolor.png (ya pasado por decodePng) a la tabla de 33 x 33 de ColorMaps: RGB de 8 bits del pixel (min(i*8,255), min(j*8,255))
    void colorTable(const uint16_t* px256, uint8_t* table33x33x3);

    // Primera linea de pack.txt: solo caracteres imprimibles, sin espacios a los costados, como mucho cap - 1 letras
    void firstLine(const uint8_t* text, uint32_t len, char* out, int cap);
}

#endif
