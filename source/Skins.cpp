#include "Skins.h"
#include "PackImage.h"
#include "Texture.h"
#include "char.h"
#include <nds.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <dirent.h>

// Ver Skins.h. Mismo esquema que TexturePack.cpp, con una sola textura (char.png: 64 x 64, 8 bits).
namespace {
    const char* const SKINS_DIR = "/mcdsefiles/skins";
    const char* const CACHE_DIR = "/mcdsefiles/skinscache";
    const char* const SEL_PATH  = "/mcdsefiles/skin.txt";                  // skin elegida (sin archivo = Default)
    const char* const MARK_PATH = "/mcdsefiles/skinscache/converting";     // existe mientras se convierte: si al arrancar sigue ahi, se corto
    const char* const DEFAULT_PATH = "/mcdsefiles/skinscache/default.bin";

    const u32 CACHE_VERSION = 1;
    const u32 MAX_PNG = 1024 * 1024;
    const int SKIN_W = 64;
    const int SKIN_H = (charBitmapLen >= 64 * 64) ? 64 : 32;   // el bitmap del juego (char.png moderno: 64 x 64)
    const int PAL_ENTRIES = (charPalLen / 2) < 256 ? (charPalLen / 2) : 256;

    struct Header { char magic[4]; u32 version, sigA, sigB, bmpLen, palLen; };

    u32  s_builtinSum = 0;
    bool s_overwritten = false;
    char s_active[48] = "";
    const char* s_err = "";

    u8*  bmp() { return (u8*)(const void*)charBitmap; }
    u16* pal() { return (u16*)(const void*)charPal; }

    u32 builtinSum() {
        u32 h = 2166136261u;
        const u32* w = (const u32*)charBitmap;
        for (u32 i = 0; i < charBitmapLen / 4; i++) { h ^= w[i]; h *= 16777619u; }
        w = (const u32*)charPal;
        for (u32 i = 0; i < charPalLen / 4; i++) { h ^= w[i]; h *= 16777619u; }
        return h;
    }

    bool readHeader(const char* path, Header& h) {
        FILE* f = fopen(path, "rb");
        if (!f) return false;
        const bool ok = fread(&h, 1, sizeof(h), f) == sizeof(h);
        fclose(f);
        return ok && memcmp(h.magic, "MSK1", 4) == 0 && h.version == CACHE_VERSION && h.bmpLen == charBitmapLen && h.palLen == charPalLen;
    }

    bool loadBin(const char* path) {
        Header h;
        if (!readHeader(path, h)) return false;
        FILE* f = fopen(path, "rb");
        if (!f) return false;
        fseek(f, sizeof(Header), SEEK_SET);
        const bool ok = fread(bmp(), 1, charBitmapLen, f) == charBitmapLen && fread(pal(), 1, charPalLen, f) == charPalLen;
        fclose(f);
        return ok;
    }

    bool writeBin(const char* path, u32 sigA, u32 sigB) {
        char tmp[96];
        snprintf(tmp, sizeof(tmp), "%s.tmp", path);
        FILE* f = fopen(tmp, "wb");
        if (!f) return false;
        Header h = { { 'M', 'S', 'K', '1' }, CACHE_VERSION, sigA, sigB, charBitmapLen, charPalLen };
        const bool ok = fwrite(&h, 1, sizeof(h), f) == sizeof(h) && fwrite(charBitmap, 1, charBitmapLen, f) == charBitmapLen
                        && fwrite(charPal, 1, charPalLen, f) == charPalLen;
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

    void afterChange() {
        DC_FlushAll();
        TexturePack::uploadTexture(Texture::skinTextureID, charBitmap, charBitmapLen, charPal, PAL_ENTRIES);   // al arrancar todavia no esta cargada (id 0)
    }

    bool ensureDefault() {
        Header h;
        if (readHeader(DEFAULT_PATH, h) && h.sigA == s_builtinSum) return true;
        if (s_overwritten) return false;
        return writeBin(DEFAULT_PATH, s_builtinSum, 0);
    }

    bool restoreDefault() {
        if (!s_overwritten) return true;
        if (!loadBin(DEFAULT_PATH)) return false;
        s_overwritten = false;
        return true;
    }

    u32 be32(const u8* p) { return ((u32)p[0] << 24) | ((u32)p[1] << 16) | ((u32)p[2] << 8) | p[3]; }

    // Tamano del PNG (cabecera IHDR). Vale 64 x 32 (clasica) o 64 x 64 (moderna), y lo mismo multiplicado por 2, 4 u 8 (HD).
    bool skinSize(const char* path, int& w, int& h) {
        FILE* f = fopen(path, "rb");
        if (!f) return false;
        u8 b[24];
        const bool ok = fread(b, 1, sizeof(b), f) == sizeof(b);
        fclose(f);
        if (!ok || b[0] != 0x89 || b[1] != 'P' || memcmp(b + 12, "IHDR", 4) != 0) return false;
        w = (int)be32(b + 16); h = (int)be32(b + 20);
        const int s = w / 64;
        return w == 64 * s && (s == 1 || s == 2 || s == 4 || s == 8) && (h == w || h * 2 == w);
    }

    bool endsWithPng(const char* n) {
        const int l = (int)strlen(n);
        return l > 4 && strcasecmp(n + l - 4, ".png") == 0;
    }

    bool validate(const char* name, int* w = nullptr, int* h = nullptr) {
        if (!endsWithPng(name)) return false;
        char p[128];
        snprintf(p, sizeof(p), "%s/%s", SKINS_DIR, name);
        int ww, hh;
        if (!skinSize(p, ww, hh)) return false;
        if (w) *w = ww;
        if (h) *h = hh;
        return true;
    }

    bool convert(const char* name) {
        int w, h;
        if (!validate(name, &w, &h)) { s_err = "not a 64x32 or 64x64 skin"; return false; }
        char p[128];
        snprintf(p, sizeof(p), "%s/%s", SKINS_DIR, name);
        FILE* f = fopen(p, "rb");
        if (!f) { s_err = "cannot read the skin"; return false; }
        fseek(f, 0, SEEK_END);
        const long sz = ftell(f);
        fseek(f, 0, SEEK_SET);
        u8* png = (sz > 0 && (u32)sz <= MAX_PNG) ? (u8*)malloc((size_t)sz) : nullptr;
        if (png && fread(png, 1, (size_t)sz, f) != (size_t)sz) { free(png); png = nullptr; }
        fclose(f);
        if (!png) { s_err = "cannot read the skin"; return false; }
        u16* rgb = (u16*)calloc((size_t)SKIN_W * 64, sizeof(u16));
        bool ok = rgb != nullptr;
        if (!ok) s_err = "not enough memory";
        // Clasica (2:1): llena la mitad de arriba y abajo queda transparente. Moderna (1:1): entera (el modelo usa solo la mitad de arriba).
        const int outH = (h == w) ? 64 : 32;
        if (ok && !PackImage::decodePng(png, (u32)sz, rgb, SKIN_W, outH)) { ok = false; s_err = "unsupported PNG format"; }
        free(png);
        if (ok && !PackImage::quantize(rgb, SKIN_W * SKIN_H, bmp(), pal(), PAL_ENTRIES)) { ok = false; s_err = "not enough memory"; }
        free(rgb);
        return ok;
    }
}

const char* Skins::activeName() { return s_active; }
const char* Skins::lastError() { return s_err; }

bool Skins::apply(const char* name) {
    s_err = "";
    if (!name || !name[0]) {
        if (!restoreDefault()) { s_err = "default.bin is missing: restart the game"; return false; }
        afterChange();
        s_active[0] = 0;
        writeSelection("");
        return true;
    }
    if (strlen(name) >= sizeof(s_active) || !validate(name)) { s_err = "skin not found or wrong size"; return false; }
    if (!ensureDefault()) { s_err = "cannot save default.bin (SD card)"; return false; }

    char p[128];
    snprintf(p, sizeof(p), "%s/%s", SKINS_DIR, name);
    struct stat st;
    u32 sigA = 0, sigB = 0;
    if (stat(p, &st) == 0) { sigA = (u32)st.st_size; sigB = (u32)st.st_mtime; }
    char cache[96];
    snprintf(cache, sizeof(cache), "%s/%s.bin", CACHE_DIR, name);
    Header h;
    bool ok;
    if (readHeader(cache, h) && h.sigA == sigA && h.sigB == sigB) {
        s_overwritten = true;
        ok = loadBin(cache);
        if (!ok) s_err = "cannot read the cache";
    } else {
        FILE* m = fopen(MARK_PATH, "wb");
        if (m) { fwrite(name, 1, strlen(name), m); fclose(m); }
        s_overwritten = true;
        ok = convert(name);
        if (ok) writeBin(cache, sigA, sigB);
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

int Skins::scan(TexturePack::Entry* out, int max) {
    DIR* d = opendir(SKINS_DIR);
    if (!d) return 0;
    int n = 0;
    struct dirent* e;
    while (n < max && (e = readdir(d)) != nullptr) {
        if (e->d_name[0] == '.' || strlen(e->d_name) >= sizeof(out[n].name)) continue;
        if (!validate(e->d_name)) continue;   // solo PNG de 64 x 32 o 64 x 64 (o HD)
        strcpy(out[n].name, e->d_name);
        int l = (int)strlen(e->d_name) - 4;   // sin ".png"
        if (l > (int)sizeof(out[n].title) - 1) l = (int)sizeof(out[n].title) - 1;
        memcpy(out[n].title, e->d_name, (size_t)l);
        out[n].title[l] = 0;
        out[n].zip = false;
        n++;
    }
    closedir(d);
    for (int i = 1; i < n; i++)
        for (int j = i; j > 0 && strcasecmp(out[j - 1].name, out[j].name) > 0; j--) { TexturePack::Entry t = out[j]; out[j] = out[j - 1]; out[j - 1] = t; }
    return n;
}

void Skins::boot() {
    mkdir(SKINS_DIR, 0777);
    mkdir(CACHE_DIR, 0777);
    s_builtinSum = builtinSum();
    s_overwritten = false;
    ensureDefault();

    FILE* m = fopen(MARK_PATH, "rb");   // killswitch: una conversion que no termino -> Default
    if (m) {
        fclose(m);
        remove(MARK_PATH);
        writeSelection("");
        iprintf("Skin reset to Default\n");
        return;
    }
    char name[48];
    readSelection(name, (int)sizeof(name));
    if (!name[0]) return;
    if (!apply(name)) {
        writeSelection("");
        iprintf("Skin unavailable: Default\n");
    }
}
