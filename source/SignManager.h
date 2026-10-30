#ifndef SIGNMANAGER_H
#define SIGNMANAGER_H

class ChunkManager;

// Carteles con textura propia (sign.png) y texto. Un cartel registrado aqui se dibuja cada cuadro (tablero + poste + 4 lineas de hasta 15 letras)
// en vez de ir en la malla del chunk; los carteles viejos sin registrar siguen saliendo con la textura de tablones.
// El texto se pide con el teclado al colocar el cartel (como en Beta, donde solo se edita al ponerlo).
namespace SignManager {
    const int MAX_SIGNS = 48;
    const int LINES = 4;
    const int LINE_LEN = 15;

    bool isRegistered(int gx, int gy, int gz);          // lo usa Chunk para no meterlo en la malla
    bool place(int gx, int gy, int gz);                 // registra un cartel nuevo (vacio) y lo deja pendiente de edicion; false si no cabe
    bool runPendingEdit();                              // abre el teclado linea por linea para el cartel recien puesto (bloquea)
    bool hasPending();

    void draw(const ChunkManager& cm, float px, float py, float pz);   // px, py, pz: posicion del jugador (para no dibujar texto lejano o por detras)

    void clear();
    bool saveToFile(const char* path);
    bool loadFromFile(const char* path);
}

#endif
