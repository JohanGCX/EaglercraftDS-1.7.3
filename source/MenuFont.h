#ifndef MENUFONT_H
#define MENUFONT_H

#include <nds.h>

// Fuente de bits de 4 x 6 (la misma de siempre de la seleccion de mundo, ahora con todas las letras) para escribir sobre las
// imagenes de los menus. Escala 2 = 8 x 12 pixeles por letra; hasta 12 letras caben en un recuadro de 124 pixeles.
namespace MenuFont {
    int advance(int scale);                       // lo que avanza cada letra
    int width(const char* text, int scale);       // ancho total del texto
    const char* const* glyphRows(char c);         // las 6 filas del glifo ("#" = pixel encendido) o nullptr si no hay
    int scaleFor(int len);                        // 2 hasta 12 letras; mas largo se achica a 1 para que quepa

    // Fondo de 16 bits (BgType_Bmp16): color con el bit 15 puesto
    void draw16(u16* fb, const char* text, int x, int y, int scale, u16 color);
    void fill16(u16* fb, int x0, int y0, int x1, int y1, u16 color);

    // Fondo de 8 bits con paleta (BgType_Bmp8): la VRAM solo admite escrituras de 16 bits, aqui se arma cada byte
    void draw8(u16* vram, const char* text, int x, int y, int scale, u8 colorIndex);
    void fill8(u16* vram, int x0, int y0, int x1, int y1, u8 colorIndex);
}

#endif
