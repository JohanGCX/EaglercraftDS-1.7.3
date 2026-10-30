#ifndef OPTIONS_H
#define OPTIONS_H

// Opciones del juego, las que se cambian desde el botón Options del menú. Se guardan en /mcdsefiles/options.dat.
namespace Options {
    struct Data {
        bool debugLargeChest;   // al crear un mundo nuevo, generar dos cofres grandes con todos los bloques y objetos
        unsigned char chunkMode;   // 0 = fast (3x3), 1 = ds (5x5), 2 = dsi (7x7, solo en DSi)
        unsigned char seedMode;    // seed de los mundos nuevos con el recuadro vacio: 0 = random, 1 = custom (customSeed)
        bool customSeedSet;        // false = "custom default seed" vacia
        unsigned int customSeed;
        unsigned char controlMode; // 0 = normal (la touch zone mueve la camara), 1 = botones (XYBA mueven la camara, la touch zone salta)
        bool autoJump;             // salto automatico al chocar con un bloque
        bool clockCompassInvUpdate;   // la brujula y el reloj se animan tambien en la barra, el inventario y el crafteo (false: solo en la mano)
        bool particles;            // particulas (humo, llamas, bloques al romper, lluvia...): on / off
        bool sound;                // sonidos: on / off
        unsigned char difficulty;  // 0 = peaceful (sin monstruos ni daño, regenera vida), 1 = easy, 2 = normal, 3 = hard
        bool spikeLog;             // anotar los cuadros lentos en /mcdsefiles/spikes.txt (false por defecto)
        bool debugCoordsOnly;      // la debug screen muestra solo las coordenadas (false: rendimiento y coordenadas)
        bool debugScreenOptions;   // en la debug screen: B = dia/noche, Y = lluvia, X = daño on/off (false por defecto)
        unsigned char renderQuality;   // 0 = high, 1 = balanced, 2 = fast (menos detalle cercano y menos vertices por cuadro)
        bool inventoryMap;         // el jugador siempre lleva un mapa en la casilla 9 de la barra (como la Legacy Edition)
    };

    Data& get();
    void load();   // lee el archivo; si no existe o no es válido, quedan los valores por defecto
    bool save();
}

#endif