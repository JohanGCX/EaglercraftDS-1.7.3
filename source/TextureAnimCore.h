#ifndef TEXTUREANIMCORE_H
#define TEXTUREANIMCORE_H

// Parte pura de TextureAnim (sin libnds), para poder probarla fuera de la consola.
namespace TextureAnimCore {

// Para cada color de la tira (1..count-1) busca el color más cercano de la paleta del atlas (RGB555, el índice 0 es el color
// transparente y no se usa). map[0] = 0: transparente sigue siendo transparente.
inline void buildMap(const unsigned char colors[][3], int count, const unsigned short* pal, unsigned char* map) {
    map[0] = 0;
    for (int i = 1; i < count; i++) {
        const int r = colors[i][0], g = colors[i][1], b = colors[i][2];
        int best = 1, bestDist = 1 << 30;
        for (int k = 1; k < 256; k++) {
            const int r5 = pal[k] & 31, g5 = (pal[k] >> 5) & 31, b5 = (pal[k] >> 10) & 31;
            const int pr = (r5 << 3) | (r5 >> 2), pg = (g5 << 3) | (g5 >> 2), pb = (b5 << 3) | (b5 >> 2);
            const int d = (pr - r) * (pr - r) + (pg - g) * (pg - g) + (pb - b) * (pb - b);
            if (d < bestDist) { bestDist = d; best = k; }
        }
        map[i] = (unsigned char)best;
    }
}

// Copia un cuadro de 16 x 16 (índices de la tira) a la celda (col, fila) de un atlas de 256 x 256 de 8 bits, pasando por 'map'.
// La VRAM no admite escrituras de 8 bits: se escribe de a 32 bits (cada fila de la celda son 16 bytes alineados).
inline void blitCell(unsigned char* tex, int col, int row, const unsigned char* frame, const unsigned char* map) {
    unsigned int* dst = (unsigned int*)(tex + (row * 16) * 256 + col * 16);
    for (int y = 0; y < 16; y++) {
        for (int w = 0; w < 4; w++) {
            const unsigned char* s = frame + y * 16 + w * 4;
            dst[w] = (unsigned int)map[s[0]] | ((unsigned int)map[s[1]] << 8) | ((unsigned int)map[s[2]] << 16) | ((unsigned int)map[s[3]] << 24);
        }
        dst += 256 / 4; // la fila siguiente de la textura
    }
}

}  // namespace TextureAnimCore

#endif