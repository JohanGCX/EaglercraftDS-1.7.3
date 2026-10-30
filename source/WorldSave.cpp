#include "WorldSave.h"
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <stddef.h>

static const unsigned int CURRENT_FORMAT_VERSION = 5;

static const char* MCDS_ROOT      = "/mcdsefiles";
static const char* MCDS_SKINS     = "/mcdsefiles/skins";
static const char* MCDS_WORLDS    = "/mcdsefiles/worlds";
static const char* TEMPWORLD_PATH = "/mcdsefiles/worlds/.tempworld";

char WorldSave::s_activeBasePath[64] = "";
int WorldSave::s_activeWorldNum = 1;
bool WorldSave::s_promoted = false;
bool WorldSave::s_startedFromExisting = false;
int WorldSave::s_dimension = 0;

static bool folderExists(const char* path) {
    DIR* d = opendir(path);
    if (!d) return false;
    closedir(d);
    return true;
}

// Vacía una carpeta, con sus subcarpetas (la del Nether, DIM-1). remove() en FAT no borra carpetas con contenido, así que
// se borra cada archivo y cada subcarpeta ya vacía; la carpeta en sí la borra quien llama.
static void clearFolderContents(const char* folderPath) {
    char fullPath[192];

    for (int guard = 0; guard < 5000; guard++) { // tope de seguridad, no debería acercarse nunca
        DIR* dir = opendir(folderPath);
        if (!dir) return;

        struct dirent* entry = nullptr;
        bool foundOne = false;
        while ((entry = readdir(dir)) != nullptr) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
            snprintf(fullPath, sizeof(fullPath), "%s/%s", folderPath, entry->d_name);
            foundOne = true;
            break; // un solo archivo por vuelta
        }
        closedir(dir);

        if (!foundOne) return; // ya no queda nada: listo

        struct stat st;
        if (stat(fullPath, &st) == 0 && S_ISDIR(st.st_mode)) {
            clearFolderContents(fullPath);
            rmdir(fullPath);
        } else {
            remove(fullPath);
        }
    }
}

void WorldSave::ensureDirectoryStructure() {
    mkdir(MCDS_ROOT, 0777);
    mkdir(MCDS_SKINS, 0777);
    mkdir(MCDS_WORLDS, 0777);
    mkdir(TEMPWORLD_PATH, 0777);
}

void WorldSave::getWorldFolderPath(int worldNum, char* outPath) {
    sprintf(outPath, "%s/world%d", MCDS_WORLDS, worldNum);
}

bool WorldSave::worldSlotExists(int worldNum) {
    char path[64];
    getWorldFolderPath(worldNum, path);
    return folderExists(path);
}

bool WorldSave::readWorldName(int worldNum, char* outName, int capacity) {
    char folder[64], path[96];
    getWorldFolderPath(worldNum, folder);
    snprintf(path, sizeof(path), "%s/name.dat", folder);
    outName[0] = 0;
    FILE* f = fopen(path, "rb");
    if (!f) return false;
    char raw[40];
    size_t n = fread(raw, 1, sizeof(raw) - 1, f);
    fclose(f);
    int len = 0;
    for (size_t i = 0; i < n && len < capacity - 1; i++) {
        const char c = raw[i];
        if (c >= 32 && c < 127) outName[len++] = c;   // solo caracteres imprimibles
    }
    outName[len] = 0;
    return len > 0;
}

void WorldSave::writeActiveWorldName(const char* name) {
    if (!name || !name[0]) return;
    char path[96];
    getBaseFilePath("name.dat", path);
    FILE* f = fopen(path, "wb");
    if (!f) return;
    fwrite(name, 1, strlen(name), f);
    fclose(f);
}

int WorldSave::findFreeWorldSlot() {
    for (int n = 1; n <= 100; n++) {
        if (!worldSlotExists(n)) return n;
    }
    return 1; // no debería pasar nunca
}

void WorldSave::recoverInterruptedSession() {
    if (!folderExists(TEMPWORLD_PATH)) return;

    char metaPath[96];
    snprintf(metaPath, sizeof(metaPath), "%s/meta.dat", TEMPWORLD_PATH);
    if (!worldExists(metaPath)) return;

    char slotPath[64];
    getWorldFolderPath(findFreeWorldSlot(), slotPath);
    rename(TEMPWORLD_PATH, slotPath); // si falla, queda en .tempworld como antes
}

void WorldSave::beginSessionNew() {
    clearFolderContents(TEMPWORLD_PATH);
    rmdir(TEMPWORLD_PATH);
    mkdir(TEMPWORLD_PATH, 0777);

    s_activeWorldNum = findFreeWorldSlot();
    strcpy(s_activeBasePath, TEMPWORLD_PATH);
    s_promoted = false;
    s_startedFromExisting = false;
    s_dimension = 0;
}

bool WorldSave::beginSessionLoad(int worldNum) {
    char worldPath[64];
    getWorldFolderPath(worldNum, worldPath);
    if (!folderExists(worldPath)) return false;

    // Un mundo que ya existe se usa DONDE ESTÁ: nunca se renombra. Así un apagado
    // en cualquier momento (sin START ni SELECT) no puede dejarlo fuera del
    // selector ni a merced de la limpieza de .tempworld.
    s_activeWorldNum = worldNum;
    strcpy(s_activeBasePath, worldPath);
    s_promoted = true;          // ya está publicado en worldN
    s_startedFromExisting = true;
    s_dimension = 0;
    return true;
}

void WorldSave::endSession() {
    if (s_promoted) return; // ya está publicado en worldN, nada que hacer

    if (s_startedFromExisting) {
        // Era un mundo que ya existía — aunque no se haya guardado
        // esta sesión, no debe desaparecer del selector. Lo devolvemos
        // a su nombre visible con lo que sea que ya se haya escrito
        // (chunks expulsados durante la sesión, si hubo alguno).
        char worldPath[64];
        getWorldFolderPath(s_activeWorldNum, worldPath);
        rename(TEMPWORLD_PATH, worldPath);
    }
    // Si era un mundo NUEVO y nunca se guardó, .tempworld queda
    // huérfano a propósito — la limpieza defensiva de la próxima
    // sesión se encarga, tal como se diseñó.
}

void WorldSave::commitSession() {
    if (s_promoted) return; // ya está publicado en worldN, nada que mover

    char worldPath[64];
    getWorldFolderPath(s_activeWorldNum, worldPath);

    if (folderExists(worldPath)) {
        clearFolderContents(worldPath);
        rmdir(worldPath);
    }

    if (rename(TEMPWORLD_PATH, worldPath) == 0) {
        strcpy(s_activeBasePath, worldPath);
        s_promoted = true;
    }
}

void WorldSave::getActiveWorldPath(char* outPath) {
    if (s_dimension == -1) sprintf(outPath, "%s/DIM-1", s_activeBasePath);
    else strcpy(outPath, s_activeBasePath);
}

void WorldSave::getActiveFilePath(const char* filename, char* outPath) {
    char dimPath[96];
    getActiveWorldPath(dimPath);
    sprintf(outPath, "%s/%s", dimPath, filename);
}

void WorldSave::getBaseFilePath(const char* filename, char* outPath) {
    sprintf(outPath, "%s/%s", s_activeBasePath, filename);
}

void WorldSave::setActiveDimension(int dim) {
    s_dimension = (dim == -1) ? -1 : 0;
}

int WorldSave::getActiveDimension() {
    return s_dimension;
}

// La carpeta del Nether (worldN/DIM-1, o .tempworld/DIM-1 si el mundo todavía no se guardó) se crea al entrar por primera vez;
// al guardar, el cambio de nombre de .tempworld a worldN se lleva la subcarpeta entera.
void WorldSave::ensureDimensionFolder() {
    if (s_dimension != -1) return;
    char dimPath[96];
    getActiveWorldPath(dimPath);
    mkdir(dimPath, 0777);
}

bool WorldSave::saveMetadata(const WorldMetadata& meta, const char* path) {
    FILE* file = fopen(path, "wb");
    if (!file) return false;

    unsigned int savedFormatVersion = CURRENT_FORMAT_VERSION;
    size_t written = fwrite(&meta, sizeof(WorldMetadata), 1, file);

    if (written == 1) {
        fseek(file, 0, SEEK_SET);
        fwrite(&savedFormatVersion, sizeof(unsigned int), 1, file);
    }

    fclose(file);
    return written == 1;
}

bool WorldSave::loadMetadata(WorldMetadata& outMeta, const char* path) {
    FILE* file = fopen(path, "rb");
    if (!file) return false;

    memset(&outMeta, 0, sizeof(WorldMetadata));
    const size_t OLD_SIZE = offsetof(WorldMetadata, armorType);   // antes de la armadura
    size_t read = fread(&outMeta, 1, sizeof(WorldMetadata), file);
    fclose(file);

    if (read < OLD_SIZE) return false;
    if (outMeta.formatVersion != CURRENT_FORMAT_VERSION) return false;

    return true;
}

bool WorldSave::worldExists(const char* path) {
    FILE* file = fopen(path, "rb");
    if (!file) return false;
    fclose(file);
    return true;
}

void WorldSave::deleteWorld(const char* path) {
    clearFolderContents(path);
    rmdir(path);
}