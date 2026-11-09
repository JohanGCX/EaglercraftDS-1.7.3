#ifndef WEATHER_H
#define WEATHER_H

class ChunkManager;

// El clima de la Beta 1.7.3 (World.updateWeather, EntityLightningBolt, EntityRenderer.renderRainSnow). Sin texturas: la lluvia, la nieve y el rayo son geometria de color plano.
// Lluvia y tormenta alternan con temporizadores al azar (la fuerza sube o baja 0,01 por tick, asi que el cambio tarda 5 s); en los biomas frios (taiga, tundra) cae nieve en
// vez de lluvia y los desiertos no tienen. Solo en la superficie: en el Nether no hay clima. Las celdas (gx, gy, gz) son las del chunk (gx = chunkX * 8 + xi, gy = yi).
namespace Weather {
    void reset();                    // al entrar a un mundo (sin lluvia, temporizadores a 0: el primero se sortea solo)
    void load();                     // weather.dat de la carpeta del mundo (si no existe se queda el clima despejado)
    void save();

    // 20 Hz. (px, py, pz): los ojos del jugador. overworld: false en el Nether (no pasa nada y todo vale 0)
    void tick(const ChunkManager& cm, float px, float py, float pz, bool overworld);

    float rain(float partialTick);       // World.func_27162_g: 0 a 1
    float thunder(float partialTick);    // World.func_27166_f: 0 a 1 (ya multiplicado por la lluvia)
    void debugToggle();              // depuracion: tormenta con rayos al instante, o despejado si ya llovia
    void clearAfterSleep();
    bool isRaining();                    // func_27161_C: lluvia > 0,2
    bool isThundering();                 // func_27160_B: tormenta > 0,9
    float flash(float partialTick);      // 0 a 1: destello del cielo cuando cae un rayo

    // World.canBlockBeRainedOn: llueve y la celda ve el cielo en un bioma con lluvia (no nieve, no desierto)
    bool canRainAtGrid(int gx, int gy, int gz);
    bool canRainAtWorld(float x, float y, float z);   // coordenadas de los mobs (el bloque j cubre y de j - 0,5 a j + 0,5)

    // Un rayo acaba de caer o sigue activo este tick (EntityLightningBolt.onUpdate): daña y prende lo que esta a 3 bloques. Una vez por tick mientras dura.
    bool popStrike(float& x, float& y, float& z);

    // Con la matriz de la camara puesta, al final de las cosas del mundo. (ex, ey, ez): ojos del jugador; yaw: el del jugador; dayBright: brillo del cielo 0..1
    void draw(float partialTick, float ex, float ey, float ez, float yawDeg, float dayBright);
}

#endif
