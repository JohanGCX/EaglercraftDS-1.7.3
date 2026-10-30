/*
 * alphakey: convierte el canal alfa de un PNG en un color "llave" opaco
 * (magenta FF00FF por defecto), para que grit (-gT FF00FF) lo mande al
 * indice 0 de la paleta y el DS lo dibuje transparente.
 *
 * Uso:   alphakey entrada.png salida.png [umbral_alfa]
 *
 *  - pixel con alfa <  umbral (128 por defecto) -> FF00FF opaco  (transparente)
 *  - pixel con alfa >= umbral                   -> mismo color, opaco
 *  - un pixel OPACO que ya fuera FF00FF exacto se corre a FE00FF, para que
 *    nunca se confunda con la llave.
 *
 * Los PNG sin canal alfa pasan sin cambios (salvo el chequeo del FF00FF).
 * El PNG original no se toca.
 *
 * Compilar: gcc -O2 -o alphakey alphakey.c
 */
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image.h"
#include "stb_image_write.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char** argv) {
    if (argc < 3) {
        fprintf(stderr, "uso: alphakey entrada.png salida.png [umbral_alfa]\n");
        return 2;
    }
    int threshold = (argc > 3) ? atoi(argv[3]) : 128;

    int w, h, channels;
    unsigned char* px = stbi_load(argv[1], &w, &h, &channels, 4); // siempre RGBA
    if (!px) {
        fprintf(stderr, "alphakey: no se pudo leer '%s': %s\n", argv[1], stbi_failure_reason());
        return 1;
    }

    long transparent = 0, collisions = 0;
    for (long i = 0; i < (long)w * h; i++) {
        unsigned char* p = px + i * 4;
        if (p[3] < threshold) {
            p[0] = 255; p[1] = 0; p[2] = 255;
            transparent++;
        } else if (p[0] == 255 && p[1] == 0 && p[2] == 255) {
            p[0] = 254; // un FF00FF opaco de verdad no puede ser la llave
            collisions++;
        }
        p[3] = 255;
    }

    if (!stbi_write_png(argv[2], w, h, 4, px, w * 4)) {
        fprintf(stderr, "alphakey: no se pudo escribir '%s'\n", argv[2]);
        stbi_image_free(px);
        return 1;
    }
    stbi_image_free(px);

    fprintf(stderr, "alphakey: %s (%dx%d): %ld pixeles transparentes -> FF00FF", argv[1], w, h, transparent);
    if (collisions) fprintf(stderr, ", %ld FF00FF opacos corridos a FE00FF", collisions);
    fprintf(stderr, "\n");
    return 0;
}
