#ifndef GAMEOVERSCREEN_H
#define GAMEOVERSCREEN_H

// Pantalla de abajo al morir (GuiGameOver): la imagen gameover.png tal cual, con los botones Respawn y Title menu
class GameOverScreen {
public:
    static void show();

    static bool isRespawnTouched(int touchX, int touchY);
    static bool isTitleTouched(int touchX, int touchY);
};

#endif