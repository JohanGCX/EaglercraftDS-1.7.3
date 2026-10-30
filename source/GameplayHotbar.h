#ifndef GAMEPLAYHOTBAR_H
#define GAMEPLAYHOTBAR_H

// Zonas tactiles de la pantalla de abajo en juego (ingamesubscreen.png)
class GameplayHotbar {
public:
    static void show(); // dibuja el bitmap de la hotbar en la pantalla inferior

    static bool isButton1Touched(int touchX, int touchY); // crafteo
    static bool isButton2Touched(int touchX, int touchY); // inventario
    static bool isButton3Touched(int touchX, int touchY); // agacharse (sin implementar)
    static bool isButton4Touched(int touchX, int touchY); // tirar
    static bool isThrowTouched(int touchX, int touchY);   // = boton 4
    static bool isInventoryThrowTouched(int touchX, int touchY); // boton de tirar de la pantalla de inventario (posicion de siempre)
    static bool isTouchZone(int touchX, int touchY);      // unica zona que mueve la camara (modo normal) o salta (modo xyba)
    static bool isMiniButton1Touched(int touchX, int touchY); // pausa
    static bool isMiniButton2Touched(int touchX, int touchY); // tercera persona (pendiente)
    static bool isMiniButton3Touched(int touchX, int touchY); // debug screen

    static int getBgId() { return bgId; }
    static void invalidate() { bgId = -1; }
private:
    static int bgId;
};

#endif
