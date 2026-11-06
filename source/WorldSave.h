#ifndef WORLDSAVE_H
#define WORLDSAVE_H

#include <stdio.h>

struct WorldMetadata {
    unsigned int formatVersion;
    unsigned int seed;
    float playerX, playerY, playerZ;
    float playerYaw, playerPitch;
    bool isDay;
    int hotbarType[36];
    int hotbarCount[36];
    int hotbarDurability[36];
    unsigned int gameTimeTicks;
    int armorType[4];         // casco, peto, pantalones, botas (0 = vacio); agregado al final: los mundos anteriores se siguen leyendo
    int armorDurability[4];
    float spawnX, spawnZ;     // punto de aparicion del mundo (a lo que apunta la brujula); agregado al final
    int spawnSet;
    int terrainGen;           // generador de terreno del mundo (0 = original, 1 = relieve normalizado); agregado al final: los mundos anteriores traen 0
};

class WorldSave {
public:
    static bool saveMetadata(const WorldMetadata& meta, const char* path);
    static bool loadMetadata(WorldMetadata& outMeta, const char* path);
    static bool worldExists(const char* path); // chequeo de ARCHIVO (no carpeta)
    static void deleteWorld(const char* path); // ahora borra una CARPETA completa

    // Estructura de carpetas en la SD
    static void ensureDirectoryStructure();
    static void recoverInterruptedSession();
    static void getWorldFolderPath(int worldNum, char* outPath);
    static bool worldSlotExists(int worldNum);
    static int findFreeWorldSlot();

    // Nombre del mundo (archivo name.dat dentro de su carpeta; sin el, el mundo se llama "World N")
    static bool readWorldName(int worldNum, char* outName, int capacity);   // false si no tiene nombre propio
    static void writeActiveWorldName(const char* name);                     // en la carpeta de la sesion activa

    // Sesión activa: la carpeta real donde se lee/escribe AHORA MISMO
    // es .tempworld hasta el primer guardado de la sesión, y pasa a
    // ser worldN (sin volver a moverse) desde ahí en adelante.
    static void beginSessionNew();
    static bool beginSessionLoad(int worldNum);
    static void commitSession(); // no-op si ya se publicó antes en esta sesión
    static void getActiveWorldPath(char* outPath);   // carpeta de la DIMENSIÓN activa (worldN, o worldN/DIM-1 en el Nether)
    static void getActiveFilePath(const char* filename, char* outPath); // archivo dentro de la carpeta de la dimensión activa
    static void getBaseFilePath(const char* filename, char* outPath);   // archivo en la carpeta del mundo (meta.dat, dim.dat)

    // Dimensiones: 0 = superficie, -1 = Nether. Los chunks y los hornos de cada una viven en su propia carpeta.
    static void setActiveDimension(int dim);
    static int getActiveDimension();
    static void ensureDimensionFolder();
    static void endSession();

private:
    static char s_activeBasePath[64];
    static int s_activeWorldNum;
    static bool s_promoted;
    static bool s_startedFromExisting;
    static int s_dimension;
};

#endif