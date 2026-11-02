#ifndef NOTEBLOCKS_H
#define NOTEBLOCKS_H

class ChunkManager;

// Bloque de notas (BlockNote + TileEntityNote, Beta 1.7.3). La nota (0 a 24) de cada bloque se guarda aqui por celda global (gx, gy, gz), por dimension (notes.dat).
// El sonido son las muestras de la Beta (SoundNote.h: arpa, bombo, caja, platillo y bajo) reproducidas con Sound::note, que cambia la velocidad para dar el tono. Beta: el instrumento depende del material del bloque de abajo y solo suena con aire encima.
namespace NoteBlocks {
    const int MAX_NOTES = 128;

    int  noteAt(int gx, int gy, int gz);                          // 0 si el bloque aun no tiene entrada
    void changePitch(int gx, int gy, int gz);                     // TileEntityNote.changePitch: (nota + 1) % 25
    void trigger(const ChunkManager& cm, int gx, int gy, int gz); // TileEntityNote.triggerNote: suena si encima hay aire
    // BlockNote.onNeighborBlockChange: devuelve true si hay que sonar (paso de apagado a encendido); recuerda el estado
    bool powerChanged(int gx, int gy, int gz, bool powered);
    void remove(int gx, int gy, int gz);
    void clearAll();

    void setListener(float x, float y, float z);                  // donde esta el jugador, en coordenadas de mundo (para el volumen)
    void tickSound();                                             // un tick del juego: corta las notas que ya duraron lo suyo

    bool saveToFile(const char* path);
    bool loadFromFile(const char* path);
}

#endif
