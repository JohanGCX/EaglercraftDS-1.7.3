#ifndef PAUSESCREEN_H
#define PAUSESCREEN_H

// Menu de pausa (pantalla de abajo): imagen pauseMenu.png con 4 botones en las mismas posiciones del menu principal.
// El juego no se congela mientras esta abierto; solo se bloquean los controles.
namespace PauseScreen {
    enum Button { BTN_NONE = 0, BTN_BACK, BTN_OPTIONS, BTN_SAVE, BTN_QUIT };

    void show();                                // dibuja la imagen
    Button touched(int touchX, int touchY);     // que boton esta bajo el lapiz
}

#endif
