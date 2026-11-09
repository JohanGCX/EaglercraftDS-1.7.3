#include "TexturePack.h"
#include "PackImage.h"
#include "Texture.h"
#include "TextureAnim.h"
#include "AnimItems.h"
#include "ColorMaps.h"
#include "terrain.h"
#include "items.h"
#include "icons.h"
#include "sign.h"
#include "cart.h"
#include "boat.h"
#include "pig.h"
#include "cow.h"
#include "chicken.h"
#include "sheep.h"
#include "zombie.h"
#include "skeleton.h"
#include "spider.h"
#include "spidereyes.h"
#include "creeper.h"
#include "pigzombie.h"
#include "ghast.h"
#include "ghastfire.h"
#include "slime.h"
#include "squid.h"
#include "wolf.h"
#include "wolfangry.h"
#include "wolftame.h"
#include <nds.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <dirent.h>

// Ver TexturePack.h. Las texturas que trae el juego (los bitmaps y paletas de grit) estan en la RAM principal: el pack se escribe encima de
// ellas, asi todo lo que las lee (la barra, el inventario, el crafteo, las animaciones, los modelos al cargarse) usa el pack sin cambiar nada.
namespace {
    const char* const PACKS_DIR = "/mcdsefiles/texturepacks";
    const char* const CACHE_DIR = "/mcdsefiles/cache";
    const char* const SEL_PATH  = "/mcdsefiles/texturepack.txt";            // nombre del pack elegido (vacio o sin archivo = Default)
    const char* const MARK_PATH = "/mcdsefiles/cache/converting";           // existe mientras se convierte: si al arrancar sigue ahi, se corto
    const char* const DEFAULT_PATH = "/mcdsefiles/cache/default.bin";

    const u32 CACHE_VERSION = 3;   // subirlo si cambia la conversion: los caches viejos se rehacen solos (2: mapas de color; 3: mobs, vehiculos, cartel, iconos)
    const u32 MAX_PNG = 2u * 1024 * 1024;

    // Archivos de un pack de la Beta 1.7.3 que se usan (rutas exactas dentro del zip o de la carpeta)
    enum {
        F_PACKTXT, F_GRASS, F_FOLIAGE, F_TERRAIN, F_ITEMS, F_ICONS, F_SIGN, F_CART, F_BOAT, F_PIG, F_COW, F_CHICKEN, F_SHEEP, F_SHEEPFUR,
        F_ZOMBIE, F_SKELETON, F_SPIDER, F_SPIDEREYES, F_CREEPER, F_PIGZOMBIE, F_GHAST, F_GHASTFIRE, F_SLIME, F_SQUID, F_WOLF, F_WOLFANGRY,
        F_WOLFTAME, F_COUNT
    };
    const char* const FILES[F_COUNT] = {
        "pack.txt", "misc/grasscolor.png", "misc/foliagecolor.png", "terrain.png", "gui/items.png", "gui/icons.png", "item/sign.png",
        "item/cart.png", "item/boat.png", "mob/pig.png", "mob/cow.png", "mob/chicken.png", "mob/sheep.png", "mob/sheep_fur.png",
        "mob/zombie.png", "mob/skeleton.png", "mob/spider.png", "mob/spider_eyes.png", "mob/creeper.png", "mob/pigzombie.png",
        "mob/ghast.png", "mob/ghast_fire.png", "mob/slime.png", "mob/squid.png", "mob/wolf.png", "mob/wolf_angry.png", "mob/wolf_tame.png"
    };

    // Cada textura del juego que un pack puede reemplazar. w x h = el bitmap de grit; el PNG del pack se lee a decW x decH (un pack de mas
    // resolucion se reduce) y si es mas grande que el bitmap se usa su esquina de arriba a la izquierda (icons.png va recortado a 128 x 32).
    // La oveja junta dos archivos: sheep.png arriba (filas 0..31) y sheep_fur.png abajo (32..63).
    struct Slot { int file, file2; const void* bmp; u32 bmpLen; const void* pal; u32 palLen; short w, h, decW, decH; };
    #define SLOT(f, n, w, h)  { f, -1, n##Bitmap, n##BitmapLen, n##Pal, n##PalLen, w, h, w, h }
    const Slot SLOTS[] = {
        SLOT(F_TERRAIN, terrain, 256, 256),
        SLOT(F_ITEMS, items, 256, 256),
        { F_ICONS, -1, iconsBitmap, iconsBitmapLen, iconsPal, iconsPalLen, 128, 32, 256, 256 },
        SLOT(F_SIGN, sign, 64, 32), SLOT(F_CART, cart, 64, 32), SLOT(F_BOAT, boat, 64, 32),
        SLOT(F_PIG, pig, 64, 32), SLOT(F_COW, cow, 64, 32), SLOT(F_CHICKEN, chicken, 64, 32),
        { F_SHEEP, F_SHEEPFUR, sheepBitmap, sheepBitmapLen, sheepPal, sheepPalLen, 64, 64, 64, 32 },
        SLOT(F_ZOMBIE, zombie, 64, 32), SLOT(F_SKELETON, skeleton, 64, 32), SLOT(F_SPIDER, spider, 64, 32),
        SLOT(F_SPIDEREYES, spidereyes, 64, 32), SLOT(F_CREEPER, creeper, 64, 32), SLOT(F_PIGZOMBIE, pigzombie, 64, 32),
        SLOT(F_GHAST, ghast, 64, 32), SLOT(F_GHASTFIRE, ghastfire, 64, 32), SLOT(F_SLIME, slime, 64, 32), SLOT(F_SQUID, squid, 64, 32),
        SLOT(F_WOLF, wolf, 64, 32), SLOT(F_WOLFANGRY, wolfangry, 64, 32), SLOT(F_WOLFTAME, wolftame, 64, 32),
    };
    #undef SLOT
    const int SLOT_COUNT = (int)(sizeof(SLOTS) / sizeof(SLOTS[0]));
    const int REQUIRED_SLOTS = 2;   // terrain e items: sin ellos el pack no vale; los demas, si faltan, quedan los del juego

    u8*  bmpOf(const Slot& s) { return (u8*)s.bmp; }
    u16* palOf(const Slot& s) { return (u16*)s.pal; }
    int  palEntries(const Slot& s) { const int n = (int)(s.palLen / 2); return n < 256 ? n : 256; }

    struct Header { char magic[4]; u32 version, sigA, sigB, layoutBytes, layoutCount; };

    u32  s_builtinSum = 0;      // firma de las texturas originales (si cambian en una version nueva, default.bin se rehace)
    bool s_overwritten = false; // las texturas en RAM ya no son las originales
    u8   s_mapFlags = 0;        // los mapas de color que trae el pack en uso (bit 0 pasto, bit 1 hojas)
    char s_active[48] = "";
    const char* s_err = "";

    u32 layoutBytes() { u32 n = 0; for (int i = 0; i < SLOT_COUNT; i++) n += SLOTS[i].bmpLen + SLOTS[i].palLen; return n; }

    u32 fnv(u32 h, const void* p, u32 len) {
        const u32* w = (const u32*)p;
        for (u32 i = 0; i < len / 4; i++) { h ^= w[i]; h *= 16777619u; }
        return h;
    }
    u32 builtinSum() {
        u32 h = 2166136261u;
        for (int i = 0; i < SLOT_COUNT; i++) { h = fnv(h, SLOTS[i].bmp, SLOTS[i].bmpLen); h = fnv(h, SLOTS[i].pal, SLOTS[i].palLen); }
        return h;
    }

    bool readHeader(const char* path, Header& h) {
        FILE* f = fopen(path, "rb");
        if (!f) return false;
        const bool ok = fread(&h, 1, sizeof(h), f) == sizeof(h);
        fclose(f);
        return ok && memcmp(h.magic, "MTP1", 4) == 0 && h.version == CACHE_VERSION && h.layoutBytes == layoutBytes() && h.layoutCount == (u32)SLOT_COUNT;
    }

    // Archivo: Header, cada textura (bitmap y paleta) en el orden de SLOTS, 1 byte con los mapas de color del pack y las dos tablas de ColorMaps
    bool loadBin(const char* path) {
        Header h;
        if (!readHeader(path, h)) return false;
        FILE* f = fopen(path, "rb");
        if (!f) return false;
        fseek(f, sizeof(Header), SEEK_SET);
        bool ok = true;
        for (int i = 0; ok && i < SLOT_COUNT; i++)
            ok = fread(bmpOf(SLOTS[i]), 1, SLOTS[i].bmpLen, f) == SLOTS[i].bmpLen && fread(palOf(SLOTS[i]), 1, SLOTS[i].palLen, f) == SLOTS[i].palLen;
        u8 flags = 0;
        if (ok) ok = fread(&flags, 1, 1, f) == 1;
        if (ok && (flags & 3)) {
            u8* maps = (u8*)malloc(2 * ColorMaps::TABLE_BYTES);
            ok = maps && fread(maps, 1, 2 * ColorMaps::TABLE_BYTES, f) == (size_t)(2 * ColorMaps::TABLE_BYTES);
            if (ok) ColorMaps::setCustom((flags & 1) ? maps : nullptr, (flags & 2) ? maps + ColorMaps::TABLE_BYTES : nullptr);
            free(maps);
        } else ColorMaps::useDefault();
        s_mapFlags = ok ? (flags & 3) : 0;
        fclose(f);
        return ok;
    }

    bool writeBin(const char* path, u32 sigA, u32 sigB) {
        char tmp[96];
        snprintf(tmp, sizeof(tmp), "%s.tmp", path);
        FILE* f = fopen(tmp, "wb");
        if (!f) return false;
        Header h = { { 'M', 'T', 'P', '1' }, CACHE_VERSION, sigA, sigB, layoutBytes(), (u32)SLOT_COUNT };
        bool ok = fwrite(&h, 1, sizeof(h), f) == sizeof(h);
        for (int i = 0; ok && i < SLOT_COUNT; i++)
            ok = fwrite(SLOTS[i].bmp, 1, SLOTS[i].bmpLen, f) == SLOTS[i].bmpLen && fwrite(SLOTS[i].pal, 1, SLOTS[i].palLen, f) == SLOTS[i].palLen;
        ok = ok && fwrite(&s_mapFlags, 1, 1, f) == 1
                && fwrite(ColorMaps::grassTable(), 1, ColorMaps::TABLE_BYTES, f) == (size_t)ColorMaps::TABLE_BYTES
                && fwrite(ColorMaps::foliageTable(), 1, ColorMaps::TABLE_BYTES, f) == (size_t)ColorMaps::TABLE_BYTES;
        fclose(f);
        if (!ok) { remove(tmp); return false; }
        remove(path);
        return rename(tmp, path) == 0;
    }

    void writeSelection(const char* name) {
        if (!name[0]) { remove(SEL_PATH); return; }
        FILE* f = fopen(SEL_PATH, "wb");
        if (!f) return;
        fwrite(name, 1, strlen(name), f);
        fclose(f);
    }

    void readSelection(char* out, int cap) {
        out[0] = 0;
        FILE* f = fopen(SEL_PATH, "rb");
        if (!f) return;
        u8 buf[64];
        const int n = (int)fread(buf, 1, sizeof(buf), f);
        fclose(f);
        PackImage::firstLine(buf, (u32)(n > 0 ? n : 0), out, cap);
    }

    void upload(int texId, const void* bmp, u32 bytes, const void* pal, int entries) { TexturePack::uploadTexture(texId, bmp, bytes, pal, entries); }

    void afterChange() {
        DC_FlushAll();
        upload(Texture::textureID, terrainBitmap, terrainBitmapLen, terrainPal, palEntries(SLOTS[0]));
        upload(Texture::itemsTextureID, itemsBitmap, itemsBitmapLen, itemsPal, palEntries(SLOTS[1]));
        upload(Texture::iconsTextureID, iconsBitmap, iconsBitmapLen, iconsPal, palEntries(SLOTS[2]));
        Texture::refreshEntityTextures();   // mobs, vehiculos y cartel (en el menu estan sueltos: se cargan de los nuevos al salir)
        TextureAnim::init();   // fuego y portal: sus colores se buscan otra vez en la paleta nueva
        AnimItems::init();     // brujula y reloj, igual
    }

    bool ensureDefault() {
        Header h;
        if (readHeader(DEFAULT_PATH, h) && h.sigA == s_builtinSum) return true;
        if (s_overwritten) return false;   // ya no hay originales en RAM para copiar
        s_mapFlags = 0;                    // default.bin: los mapas de color del juego
        return writeBin(DEFAULT_PATH, s_builtinSum, 0);
    }

    bool restoreDefault() {
        if (!s_overwritten) { ColorMaps::useDefault(); s_mapFlags = 0; return true; }
        if (!loadBin(DEFAULT_PATH)) return false;
        s_overwritten = false;
        return true;
    }

    bool endsWithZip(const char* n) {
        const int l = (int)strlen(n);
        return l > 4 && strcasecmp(n + l - 4, ".zip") == 0;
    }

    bool isFile(const char* path) { struct stat st; return stat(path, &st) == 0 && S_ISREG(st.st_mode); }

    // Un pack abierto: el zip con su indice, o la carpeta
    struct Source {
        bool zip;
        FILE* f;
        char base[96];
        PackImage::ZipItem items[F_COUNT];
    };

    bool openSource(const char* name, Source& s) {
        snprintf(s.base, sizeof(s.base), "%s/%s", PACKS_DIR, name);
        s.f = nullptr;
        struct stat st;
        if (stat(s.base, &st) != 0) return false;
        if (S_ISDIR(st.st_mode)) { s.zip = false; return true; }
        if (!endsWithZip(name)) return false;
        s.zip = true;
        s.f = fopen(s.base, "rb");
        if (!s.f) return false;
        if (!PackImage::zipIndex(s.f, FILES, F_COUNT, s.items)) { fclose(s.f); s.f = nullptr; return false; }
        return true;
    }

    void closeSource(Source& s) { if (s.f) fclose(s.f); s.f = nullptr; }

    bool has(Source& s, int file) {
        if (s.zip) return s.items[file].found;
        char p[160];
        snprintf(p, sizeof(p), "%s/%s", s.base, FILES[file]);
        return isFile(p);
    }

    u8* readFile(Source& s, int file, u32 maxBytes, u32& len) {
        len = 0;
        if (s.zip) return PackImage::zipExtract(s.f, s.items[file], maxBytes, len);
        char p[160];
        snprintf(p, sizeof(p), "%s/%s", s.base, FILES[file]);
        FILE* f = fopen(p, "rb");
        if (!f) return nullptr;
        fseek(f, 0, SEEK_END);
        const long sz = ftell(f);
        fseek(f, 0, SEEK_SET);
        u8* out = (sz > 0 && (u32)sz <= maxBytes) ? (u8*)malloc((size_t)sz) : nullptr;
        if (out && fread(out, 1, (size_t)sz, f) != (size_t)sz) { free(out); out = nullptr; }
        fclose(f);
        if (out) len = (u32)sz;
        return out;
    }

    // PNG del pack a pixeles RGB15 (decW x decH)
    bool decodeFile(Source& s, int file, u16* out, int w, int h) {
        u32 len;
        u8* png = readFile(s, file, MAX_PNG, len);
        if (!png) return false;
        const bool ok = PackImage::decodePng(png, len, out, w, h);
        free(png);
        return ok;
    }

    // Valida el pack (pack.txt, terrain.png y gui/items.png) y saca el titulo (primera linea de pack.txt)
    bool validate(const char* name, char* title, int cap) {
        Source s;
        if (!openSource(name, s)) return false;
        bool ok = has(s, F_PACKTXT) && has(s, F_TERRAIN) && has(s, F_ITEMS);
        if (ok && title) {
            title[0] = 0;
            u32 n;
            u8* txt = readFile(s, F_PACKTXT, 16384, n);
            if (txt) { PackImage::firstLine(txt, n, title, cap); free(txt); }
            if (!title[0]) { strncpy(title, name, (size_t)cap - 1); title[cap - 1] = 0; }
        }
        closeSource(s);
        return ok;
    }

    // Firma de los archivos de origen (tamano y fecha): si cambian, el cache se vuelve a hacer
    void sourceSig(const char* name, u32& a, u32& b) {
        char p[160];
        struct stat st;
        a = 0; b = 0;
        snprintf(p, sizeof(p), "%s/%s", PACKS_DIR, name);
        if (stat(p, &st) != 0) return;
        if (!S_ISDIR(st.st_mode)) { a = (u32)st.st_size; b = (u32)st.st_mtime; return; }
        for (int i = 0; i < F_COUNT; i++) {
            snprintf(p, sizeof(p), "%s/%s/%s", PACKS_DIR, name, FILES[i]);
            if (stat(p, &st) == 0) { a = a * 31u + (u32)st.st_size; b = b * 2654435761u + (u32)st.st_mtime; }
        }
    }

    // Una textura del pack -> 256 colores, escrita encima de la del juego. false si el pack no la trae o no se pudo leer (queda la del juego).
    bool convertSlot(Source& s, const Slot& sl, u16* rgb) {
        if (!has(s, sl.file) || (sl.file2 >= 0 && !has(s, sl.file2))) return false;
        if (!decodeFile(s, sl.file, rgb, sl.decW, sl.decH)) return false;
        if (sl.file2 >= 0 && !decodeFile(s, sl.file2, rgb + sl.decW * sl.decH, sl.decW, sl.decH)) return false;
        if (sl.decW != sl.w) {   // recorte: la esquina de arriba a la izquierda (las filas se corren hacia adelante, sin pisarse)
            for (int y = 0; y < sl.h; y++)
                for (int x = 0; x < sl.w; x++) rgb[y * sl.w + x] = rgb[y * sl.decW + x];
        }
        return PackImage::quantize(rgb, sl.w * sl.h, bmpOf(sl), palOf(sl), palEntries(sl));
    }

    // misc/grasscolor.png y misc/foliagecolor.png: opcionales. Los que falten (o no se puedan leer) quedan con los del juego.
    void convertColorMaps(Source& s, u16* rgb) {
        u8* tables = (u8*)malloc(2 * ColorMaps::TABLE_BYTES);
        s_mapFlags = 0;
        if (tables) {
            for (int k = 0; k < 2; k++) {
                const int file = k == 0 ? F_GRASS : F_FOLIAGE;
                if (has(s, file) && decodeFile(s, file, rgb, 256, 256)) {
                    PackImage::colorTable(rgb, tables + k * ColorMaps::TABLE_BYTES);
                    s_mapFlags |= (u8)(1 << k);
                }
            }
        }
        if (!ColorMaps::setCustom((s_mapFlags & 1) ? tables : nullptr, (s_mapFlags & 2) ? tables + ColorMaps::TABLE_BYTES : nullptr)) s_mapFlags = 0;
        free(tables);
    }

    bool convertPack(const char* name) {
        Source s;
        if (!openSource(name, s)) { s_err = "cannot open the pack"; return false; }
        u16* rgb = (u16*)malloc(65536 * sizeof(u16));
        if (!rgb) { closeSource(s); s_err = "not enough memory"; return false; }
        bool ok = true;
        for (int i = 0; i < SLOT_COUNT; i++) {
            const bool done = convertSlot(s, SLOTS[i], rgb);
            if (!done && i < REQUIRED_SLOTS) { ok = false; s_err = "terrain.png or items.png: unsupported (size or format)"; break; }
        }
        if (ok) convertColorMaps(s, rgb);
        free(rgb);
        closeSource(s);
        return ok;
    }
}

// Una textura de la RAM a la VRAM: los texeles directo en el banco (como TextureAnim) y la paleta con glColorTableEXT (libnds suelta la vieja
// y pone la nueva, del mismo tamano)
void TexturePack::uploadTexture(int texId, const void* bmp, unsigned int bytes, const void* pal, int entries) {
    if (texId == 0) return;
    glBindTexture(0, texId);
    const u32 addr = (u32)glGetTexturePointer(texId);
    if (addr >= 0x06800000u && addr + bytes <= 0x06880000u) {
        vramSetBankA(VRAM_A_LCD);
        vramSetBankD(VRAM_D_LCD);
        volatile u32* d = (volatile u32*)addr;
        const u32* s = (const u32*)bmp;
        for (u32 i = 0; i < bytes / 4; i++) d[i] = s[i];
        vramSetBankA(VRAM_A_TEXTURE);
        vramSetBankD(VRAM_D_TEXTURE);
    }
    glColorTableEXT(0, 0, (u16)entries, 0, 0, (u16*)pal);
}

const char* TexturePack::activeName() { return s_active; }
const char* TexturePack::lastError() { return s_err; }

bool TexturePack::apply(const char* name) {
    s_err = "";
    if (!name || !name[0]) {
        if (!restoreDefault()) { s_err = "default.bin is missing: restart the game"; return false; }
        afterChange();
        s_active[0] = 0;
        writeSelection("");
        return true;
    }
    if (strlen(name) >= sizeof(s_active) || !validate(name, nullptr, 0)) { s_err = "pack not found or incomplete"; return false; }
    if (!ensureDefault()) { s_err = "cannot save default.bin (SD card)"; return false; }   // sin camino de vuelta no se toca nada

    u32 sigA, sigB;
    sourceSig(name, sigA, sigB);
    char cache[96];
    snprintf(cache, sizeof(cache), "%s/%s.bin", CACHE_DIR, name);
    Header h;
    bool ok;
    if (readHeader(cache, h) && h.sigA == sigA && h.sigB == sigB) {
        s_overwritten = true;
        ok = loadBin(cache);
        if (!ok) s_err = "cannot read the cache";
    } else {
        // Primero todo a Default: lo que el pack no traiga tiene que quedar como el original (no como el pack anterior)
        if (!restoreDefault()) { s_err = "default.bin is missing: restart the game"; return false; }
        FILE* m = fopen(MARK_PATH, "wb");
        if (m) { fwrite(name, 1, strlen(name), m); fclose(m); }
        s_overwritten = true;
        ok = convertPack(name);
        if (ok) writeBin(cache, sigA, sigB);   // si no se puede guardar, se vuelve a convertir la proxima vez
        remove(MARK_PATH);
    }
    if (!ok) {
        restoreDefault();
        afterChange();
        s_active[0] = 0;
        writeSelection("");
        return false;
    }
    afterChange();
    strcpy(s_active, name);
    writeSelection(name);
    return true;
}

int TexturePack::scan(Entry* out, int max) {
    DIR* d = opendir(PACKS_DIR);
    if (!d) return 0;
    int n = 0;
    struct dirent* e;
    while (n < max && (e = readdir(d)) != nullptr) {
        if (e->d_name[0] == '.' || strlen(e->d_name) >= sizeof(out[n].name)) continue;
        char title[sizeof(out[n].title)];
        if (!validate(e->d_name, title, (int)sizeof(title))) continue;   // sin pack.txt, terrain.png o gui/items.png: no aparece
        strcpy(out[n].name, e->d_name);
        strcpy(out[n].title, title);
        out[n].zip = endsWithZip(e->d_name);
        n++;
    }
    closedir(d);
    for (int i = 1; i < n; i++)   // por nombre
        for (int j = i; j > 0 && strcasecmp(out[j - 1].name, out[j].name) > 0; j--) { Entry t = out[j]; out[j] = out[j - 1]; out[j - 1] = t; }
    return n;
}

void TexturePack::boot() {
    mkdir(PACKS_DIR, 0777);
    mkdir(CACHE_DIR, 0777);
    s_builtinSum = builtinSum();
    s_overwritten = false;
    ensureDefault();

    // Killswitch: una conversion que no termino (la consola se apago o se colgo) -> Default
    FILE* m = fopen(MARK_PATH, "rb");
    if (m) {
        fclose(m);
        remove(MARK_PATH);
        writeSelection("");
        iprintf("Texture pack reset to Default\n");
        return;
    }
    char name[48];
    readSelection(name, (int)sizeof(name));
    if (!name[0]) return;
    iprintf("Loading texture pack...\n");
    if (!apply(name)) {   // el pack se borro, ya no esta completo o no se pudo leer: Default (apply ya lo dejo anotado)
        writeSelection("");
        iprintf("Texture pack unavailable: Default\n");
    }
}
