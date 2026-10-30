#ifndef SLEEPING_H
#define SLEEPING_H

#include "BlockType.h"
#include "PartialBlocks.h"

// Dormir (Beta 1.7.3): BlockBed.blockActivated, EntityPlayer.sleepInBedAt / wakeUpPlayer, EntityRenderer.orientCamera y GuiIngame (el velo oscuro).
// Las celdas son globales (gx, gy, gz); la cama se maneja siempre por su mitad con el bit 8 de la metadata (la que la Beta llama "foot"), que es donde se duerme.
// Las posiciones son las de los mobs: x y z del bloque (gx - 4, gz - 4) y la y de la celda gy cubre de gy - 1,5 a gy - 0,5.
namespace Sleeping {
    // Cuanto dura el velo que oscurece la pantalla: sleepTimer sube hasta 100 durmiendo y hasta 110 al despertar (GuiIngame)
    const int FULL_ASLEEP = 100;
    const int FADE_END = 110;

    // EntityPlayer.sleepInBedAt: donde quedan los ojos. El jugador se acuesta en el extremo de la almohada de esa mitad.
    inline void eyePosition(int gx, int gy, int gz, int dir, float& x, float& y, float& z) {
        float ox = 0.5f, oz = 0.5f;
        switch (dir & 3) {
            case 0: oz = 0.9f; break;
            case 1: ox = 0.1f; break;
            case 2: oz = 0.1f; break;
            default: ox = 0.9f; break;
        }
        x = (float)(gx - 4) + ox;
        z = (float)(gz - 4) + oz;
        y = (float)gy - 0.5625f;   // la Beta pone la y en el bloque + 15/16; aqui los bloques estan medio bloque mas abajo
    }

    // EntityPlayer.getBedOrientationInDegrees
    inline float bedOrientationDeg(int dir) {
        static const float D[4] = { 90.0f, 0.0f, 270.0f, 180.0f };
        return D[dir & 3];
    }

    // EntityPlayer.func_22052_e: cuanto se corre el modelo al dibujarlo (el cuerpo se tiende desde la almohada hacia los pies)
    inline void modelShift(int dir, float& sx, float& sz) {
        sx = 0.0f; sz = 0.0f;
        switch (dir & 3) {
            case 0: sz = -1.8f; break;
            case 1: sx = 1.8f; break;
            case 2: sz = 1.8f; break;
            default: sx = -1.8f; break;
        }
    }

    // BlockBed.getNearestEmptyChunkCoordinates(..., 0): la primera celda con suelo macizo y dos de aire encima, mirando los 3 x 3 alrededor de cada mitad
    // de la cama. typeAt(gx, gy, gz) -> BlockType. Devuelve false si no hay ninguna.
    template <class TypeAt>
    inline bool nearestEmpty(TypeAt typeAt, int gx, int gy, int gz, int dir, int& ox, int& oy, int& oz) {
        int mx, mz;
        Partial::bedOffset(dir, mx, mz);
        for (int h = 0; h <= 1; h++) {
            const int x0 = gx - mx * h - 1, z0 = gz - mz * h - 1;
            for (int x = x0; x <= x0 + 2; x++) {
                for (int z = z0; z <= z0 + 2; z++) {
                    if (isNormalCube(typeAt(x, gy - 1, z)) && typeAt(x, gy, z) == BlockType::AIR && typeAt(x, gy + 1, z) == BlockType::AIR) {
                        ox = x; oy = gy; oz = z;
                        return true;
                    }
                }
            }
        }
        return false;
    }

    // Donde quedan los ojos del jugador de pie en la celda (ox, oy, oz): setPosition(x + 0,5, y + 1,62 + 0,1, z + 0,5)
    inline void standingEye(int ox, int oy, int oz, float& x, float& y, float& z) {
        x = (float)(ox - 4) + 0.5f;
        z = (float)(oz - 4) + 0.5f;
        y = ((float)oy - 1.5f) + 1.62f + 0.1f;
    }

    // GuiIngame: el velo (0x101020, hasta 220 de alfa) crece hasta el 100 y baja en los 10 ticks siguientes. Devuelve 0 a 1.
    inline float overlayAmount(int sleepTimer, float partialTick) {
        if (sleepTimer <= 0) return 0.0f;
        const float t = (float)sleepTimer + partialTick;
        float f = t / (float)FULL_ASLEEP;
        if (f > 1.0f) f = 1.0f - (t - (float)FULL_ASLEEP) / 10.0f;
        if (f < 0.0f) f = 0.0f;
        return f;
    }
}

#endif
