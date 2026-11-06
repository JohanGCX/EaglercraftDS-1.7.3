#include "Options.h"
#include <nds.h>
#include <stdio.h>
#include <string.h>

static const char* OPTIONS_PATH = "/mcdsefiles/options.dat";
static Options::Data s_data = { false, 0, 0, false, 0, 0, true, true, true, true, 2, true };  // cofre de depuracion apagado, modo fast y seed random (load() ajusta el modo segun el aparato)

Options::Data& Options::get() { return s_data; }

// Formato: "MCO1", banderas (bit 0 = debugLargeChest), modo de chunks, modo de seed, seed custom puesta (0/1), seed custom (4 bytes).
// Los archivos viejos son mas cortos: lo que falta queda en su valor por defecto.
void Options::load() {
    s_data.debugLargeChest = false;
    s_data.chunkMode = isDSiMode() ? 2 : 0;   // sin opcion guardada: DS normal arranca en fast, DSi en dsi
    s_data.seedMode = 0;
    s_data.customSeedSet = false;
    s_data.customSeed = 0;
    s_data.controlMode = 0;
    s_data.autoJump = true;
    s_data.clockCompassInvUpdate = true;
    s_data.particles = true;
    s_data.sound = true;
    s_data.difficulty = 2;
    s_data.inventoryMap = true;
    s_data.debugScreenOptions = false;
    s_data.spikeLog = false;
    s_data.debugCoordsOnly = false;
    s_data.renderQuality = 0;
    s_data.oldMobSpawnLimit = false;
    s_data.oldTerrainGenerator = false;
    FILE* f = fopen(OPTIONS_PATH, "rb");
    if (!f) return;
    unsigned char buf[25];
    size_t n = fread(buf, 1, sizeof(buf), f);
    if (n >= 5 && memcmp(buf, "MCO1", 4) == 0) {
        s_data.debugLargeChest = (buf[4] & 1) != 0;
        if (n >= 6 && buf[5] <= 2) s_data.chunkMode = buf[5];
        if (s_data.chunkMode == 2 && !isDSiMode()) s_data.chunkMode = 1;   // el archivo vino de una DSi: en una DS normal el modo dsi no existe, pasa a ds
        if (n >= 8) {
            s_data.seedMode = (buf[6] == 1) ? 1 : 0;
            s_data.customSeedSet = (buf[7] == 1);
        }
        if (n >= 12) s_data.customSeed = (unsigned int)buf[8] | ((unsigned int)buf[9] << 8) | ((unsigned int)buf[10] << 16) | ((unsigned int)buf[11] << 24);
        if (n >= 14) {
            s_data.controlMode = (buf[12] == 1) ? 1 : 0;
            s_data.autoJump = (buf[13] != 0);
        }
        if (n >= 15) s_data.clockCompassInvUpdate = (buf[14] != 0);
        if (n >= 16) s_data.particles = (buf[15] != 0);
        if (n >= 17) s_data.sound = (buf[16] != 0);
        if (n >= 18) s_data.inventoryMap = (buf[17] == 0);   // se guarda invertido (1 = desactivado): los archivos viejos con 0 quedan en true
        if (n >= 19) s_data.debugScreenOptions = (buf[18] == 1);
        if (n >= 20) s_data.difficulty = (buf[19] <= 3) ? buf[19] : 2;
        if (n >= 21) s_data.renderQuality = (buf[20] <= 2) ? buf[20] : 0;
        if (n >= 22) s_data.spikeLog = (buf[21] == 1);
        if (n >= 23) s_data.debugCoordsOnly = (buf[22] == 1);
        if (n >= 24) s_data.oldMobSpawnLimit = (buf[23] == 1);
        if (n >= 25) s_data.oldTerrainGenerator = (buf[24] == 1);
    }
    fclose(f);
}

bool Options::save() {
    FILE* f = fopen(OPTIONS_PATH, "wb");
    if (!f) return false;
    unsigned char buf[25] = { 'M', 'C', 'O', '1', (unsigned char)(s_data.debugLargeChest ? 1 : 0), s_data.chunkMode,
                              s_data.seedMode, (unsigned char)(s_data.customSeedSet ? 1 : 0),
                              (unsigned char)(s_data.customSeed & 0xFF), (unsigned char)((s_data.customSeed >> 8) & 0xFF),
                              (unsigned char)((s_data.customSeed >> 16) & 0xFF), (unsigned char)((s_data.customSeed >> 24) & 0xFF),
                              s_data.controlMode, (unsigned char)(s_data.autoJump ? 1 : 0), (unsigned char)(s_data.clockCompassInvUpdate ? 1 : 0), (unsigned char)(s_data.particles ? 1 : 0), (unsigned char)(s_data.sound ? 1 : 0), (unsigned char)(s_data.inventoryMap ? 0 : 1), (unsigned char)(s_data.debugScreenOptions ? 1 : 0), s_data.difficulty, s_data.renderQuality, (unsigned char)(s_data.spikeLog ? 1 : 0), (unsigned char)(s_data.debugCoordsOnly ? 1 : 0), (unsigned char)(s_data.oldMobSpawnLimit ? 1 : 0), (unsigned char)(s_data.oldTerrainGenerator ? 1 : 0) };
    const bool ok = fwrite(buf, 1, sizeof(buf), f) == sizeof(buf);
    fclose(f);
    return ok;
}
