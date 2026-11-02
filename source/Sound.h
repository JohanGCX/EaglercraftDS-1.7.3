#ifndef SOUND_H
#define SOUND_H
#include "BlockType.h"

// Sonidos esenciales (sin musica). Muestras PCM 8-bit en SoundData.h, reproducidas con soundPlaySample.
namespace Sound {
    enum Group { G_GRASS = 0, G_WOOD = 1, G_STONE = 2, G_SAND = 3, G_GRAVEL = 4, G_SNOW = 5 };

    void init();                                           // soundEnable (idempotente)
    void setListener(float x, float y, float z, float yawDeg);   // se llama una vez por cuadro
    Group groupOf(BlockType t);

    void blockBreak(BlockType t, float x, float y, float z);
    void blockPlace(BlockType t, float x, float y, float z);
    void footstep(BlockType under);                        // pasos del jugador (no posicional)
    void playerHurt();
    void explosion(float x, float y, float z);
    void fuse(float x, float y, float z);
    void mobHurt(int kind, float x, float y, float z);     // kind: 0 vaca, 1 cerdo, 2 pollo, 100+h hostil, 200 oveja
    void mobSay(int kind, float x, float y, float z);
    void doorCell(bool open, int gx, int gy, int gz);      // puerta / trampilla (celda de la cuadricula)
    void uiClick();                                         // click de los botones y menus (no posicional, suena aunque no haya partida)
    void note(int instrument, int note, float x, float y, float z);   // bloque de notas (instrumento 0 a 4, nota 0 a 24), posicion en coordenadas de mundo
    void clickCell(int gx, int gy, int gz);                // palanca, boton, repetidor
    void splash(float x, float y, float z, float vol);
    void rain(float x, float y, float z, bool covered);    // una gota que suena (covered: el jugador esta bajo techo)
    void thunder(float x, float y, float z);               // un rayo cae en (x, y, z): se oye de lejos
}

#endif
