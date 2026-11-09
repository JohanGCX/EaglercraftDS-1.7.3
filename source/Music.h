#ifndef MUSIC_H
#define MUSIC_H

// Background music of Beta 1.7.3 (calm, hal, nuance, piano), streamed from the SD card.
// Files: <name>.mus, made with tools/mkmusic.py (8 kHz mono IMA-ADPCM). Looked up first on the SD (/mcdsefiles/music/, to replace a track)
// and then inside the .nds (nitro:/music/, from nitrofiles/music/ in the repo). If neither has it nothing plays.
// The ARM9 reads one 512-byte block at a time, decodes it to 16-bit PCM into a small ring buffer and a looping hardware channel plays the ring.
namespace Music {
    // Call once per main-loop iteration (any state). inGame = a world is loaded and being played.
    // When inGame is false (or sound/music are off) the music stops and the timer for the next track restarts.
    void update(bool inGame);

    // Discos de la jukebox (World.playRecord de la Beta): "13" o "cat" (13.mus / cat.mus, en la SD o dentro del .nds como la musica).
    // Mientras suena un disco la musica de fondo se calla y su espera queda parada; al terminar o al sacar el disco la espera se sortea de nuevo.
    // Suena desde la jukebox: el volumen baja con la distancia (0 a 64 bloques) y se reparte entre izquierda y derecha.
    void playRecord(const char* name, float x, float y, float z, int gx, int gy, int gz);   // (x, y, z) centro del bloque; (gx, gy, gz) su celda
    void stopRecordAt(int gx, int gy, int gz);                     // se saco el disco o se rompio esa jukebox (si es la que suena)
    bool recordCell(int& gx, int& gy, int& gz);                    // la jukebox que suena (false = ninguna)
    void setListener(float x, float y, float z, float yawDeg);     // el jugador, cada cuadro (el yaw es el de Player)
}

#endif
