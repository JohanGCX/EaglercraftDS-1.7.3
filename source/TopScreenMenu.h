#ifndef TOPSCREENMENU_H
#define TOPSCREENMENU_H

class TopScreenMenu {
public:
    static void init();    // muestra el bitmap de menú en la pantalla superior (el logo estático de siempre)
    static void tick();    // un cuadro del menú principal: el panorama giratorio con el logo encima (arranca solo la primera vez)
    static void suspend(); // al salir del menú principal: libera el panorama y vuelve al logo estático. No hace nada si no estaba activo
};

#endif