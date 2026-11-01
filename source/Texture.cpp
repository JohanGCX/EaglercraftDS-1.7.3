#include "Texture.h"
#include <nds.h>
#include "terrain.h"
#include "char.h"   // char.png (la skin de Steve de la Beta), antes skin.png
#include "items.h"
#include "icons.h"
#include "sheep.h"
#include "cart.h"
#include "boat.h"
#include "cow.h"
#include "pig.h"
#include "chicken.h"
#include "zombie.h"
#include <string.h>
#include "sign.h"
#include "MenuFont.h"
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
#include "PaintingTex.h"
#include <stdlib.h>
#include "SkyTex.h"

int Texture::textureID = 0;
int Texture::paintingTextureID = 0;
int Texture::skySunTextureID = 0;
int Texture::skyMoonTextureID = 0;
int Texture::skinTextureID = 0;
int Texture::itemsTextureID = 0;
int Texture::iconsTextureID = 0;
int Texture::sheepTextureID = 0;
int Texture::cartTextureID = 0;
int Texture::boatTextureID = 0;
int Texture::cowTextureID = 0;
int Texture::pigTextureID = 0;
int Texture::chickenTextureID = 0;
int Texture::zombieTextureID = 0;
int Texture::signTextureID = 0;
int Texture::signFontTextureID = 0;
int Texture::skeletonTextureID = 0;
int Texture::spiderTextureID = 0;
int Texture::spidereyesTextureID = 0;
int Texture::creeperTextureID = 0;
int Texture::pigzombieTextureID = 0;
int Texture::ghastTextureID = 0;
int Texture::ghastfireTextureID = 0;
int Texture::slimeTextureID = 0;
int Texture::squidTextureID = 0;
int Texture::wolfTextureID = 0;
int Texture::wolfangryTextureID = 0;
int Texture::wolftameTextureID = 0;

bool Texture::loadTerrainAtlas() {
    glGenTextures(1, &textureID);
    glBindTexture(0, textureID);

    int result = glTexImage2D(0, 0, GL_RGB256,
                               TEXTURE_SIZE_256, TEXTURE_SIZE_256, 0,
                               TEXGEN_TEXCOORD | GL_TEXTURE_COLOR0_TRANSPARENT, (u8*)terrainBitmap);
    if (result == 0) return false;

    glColorTableEXT(0, 0, 256, 0, 0, (u16*)terrainPal);

    return true;
}

// char.png: 64 x 64 o 64 x 32 (el original de la Beta mide 64 x 32). El alto sale del tamaño del bitmap de 8 bits.
bool Texture::loadSkinAtlas() {
    glGenTextures(1, &skinTextureID);
    glBindTexture(0, skinTextureID);

    const GL_TEXTURE_SIZE_ENUM height = (charBitmapLen <= 64 * 32) ? TEXTURE_SIZE_32 : TEXTURE_SIZE_64;
    int result = glTexImage2D(0, 0, GL_RGB256,
                               TEXTURE_SIZE_64, height, 0,
                               TEXGEN_TEXCOORD, (u8*)charBitmap);
    if (result == 0) return false;

    glColorTableEXT(0, 0, 256, 0, 0, (u16*)charPal);

    return true;
}

bool Texture::loadItemsAtlas() {
    glGenTextures(1, &itemsTextureID);
    glBindTexture(0, itemsTextureID);

    int result = glTexImage2D(0, 0, GL_RGB256,
                               TEXTURE_SIZE_256, TEXTURE_SIZE_256, 0,
                               TEXGEN_TEXCOORD | GL_TEXTURE_COLOR0_TRANSPARENT, (u8*)itemsBitmap);
    if (result == 0) return false;

    glColorTableEXT(0, 0, 256, 0, 0, (u16*)itemsPal);

    return true;
}

// icons.png recortado a 128 x 32 (la esquina superior izquierda del original de la Beta, 4 KB en vez de 64 KB): la mira, los corazones, la armadura y las
// burbujas, en sus coordenadas de siempre (las UV no cambian porque el origen es el mismo)
bool Texture::loadIconsAtlas() {
    glGenTextures(1, &iconsTextureID);
    glBindTexture(0, iconsTextureID);

    int result = glTexImage2D(0, 0, GL_RGB256,
                               TEXTURE_SIZE_128, TEXTURE_SIZE_32, 0,
                               TEXGEN_TEXCOORD | GL_TEXTURE_COLOR0_TRANSPARENT, (u8*)iconsBitmap);
    if (result == 0) return false;

    glColorTableEXT(0, 0, 256, 0, 0, (u16*)iconsPal);

    return true;
}

// sheep.png: 64 x 64, la piel en las filas 0 a 31 y la lana en las 32 a 63. Sin transparencia: el modelo solo usa zonas opacas.
bool Texture::loadSheepAtlas() {
    glGenTextures(1, &sheepTextureID);
    glBindTexture(0, sheepTextureID);

    int result = glTexImage2D(0, 0, GL_RGB256,
                               TEXTURE_SIZE_64, TEXTURE_SIZE_64, 0,
                               TEXGEN_TEXCOORD, (u8*)sheepBitmap);
    if (result == 0) return false;

    glColorTableEXT(0, 0, 256, 0, 0, (u16*)sheepPal);

    return true;
}

// cart.png: 64 x 32, la vagoneta (ModelMinecart). Sin transparencia.
bool Texture::loadCartAtlas() {
    glGenTextures(1, &cartTextureID);
    glBindTexture(0, cartTextureID);

    int result = glTexImage2D(0, 0, GL_RGB256,
                               TEXTURE_SIZE_64, TEXTURE_SIZE_32, 0,
                               TEXGEN_TEXCOORD, (u8*)cartBitmap);
    if (result == 0) return false;

    glColorTableEXT(0, 0, 256, 0, 0, (u16*)cartPal);

    return true;
}

// boat.png: 64 x 32, el bote (ModelBoat). Sin transparencia.
bool Texture::loadBoatAtlas() {
    glGenTextures(1, &boatTextureID);
    glBindTexture(0, boatTextureID);

    int result = glTexImage2D(0, 0, GL_RGB256,
                               TEXTURE_SIZE_64, TEXTURE_SIZE_32, 0,
                               TEXGEN_TEXCOORD, (u8*)boatBitmap);
    if (result == 0) return false;

    glColorTableEXT(0, 0, 256, 0, 0, (u16*)boatPal);

    return true;
}

// cow.png, pig.png y chicken.png: 64 x 32, sin transparencia (el modelo solo usa zonas opacas).
bool Texture::loadAnimalAtlases() {
    bool ok = true;
    glGenTextures(1, &cowTextureID);
    glBindTexture(0, cowTextureID);
    if (glTexImage2D(0, 0, GL_RGB256, TEXTURE_SIZE_64, TEXTURE_SIZE_32, 0, TEXGEN_TEXCOORD, (u8*)cowBitmap) == 0) ok = false;
    else glColorTableEXT(0, 0, 256, 0, 0, (u16*)cowPal);
    glGenTextures(1, &pigTextureID);
    glBindTexture(0, pigTextureID);
    if (glTexImage2D(0, 0, GL_RGB256, TEXTURE_SIZE_64, TEXTURE_SIZE_32, 0, TEXGEN_TEXCOORD, (u8*)pigBitmap) == 0) ok = false;
    else glColorTableEXT(0, 0, 256, 0, 0, (u16*)pigPal);
    glGenTextures(1, &chickenTextureID);
    glBindTexture(0, chickenTextureID);
    if (glTexImage2D(0, 0, GL_RGB256, TEXTURE_SIZE_64, TEXTURE_SIZE_32, 0, TEXGEN_TEXCOORD | GL_TEXTURE_COLOR0_TRANSPARENT, (u8*)chickenBitmap) == 0) ok = false;   // la gallina si tiene transparencia (las patas)
    else glColorTableEXT(0, 0, 256, 0, 0, (u16*)chickenPal);
    return ok;
}

// Texturas de los hostiles: todas con transparencia (el color 0 de la paleta es el magenta FF00FF)
static bool loadHostile(int& id, const void* bitmap, const void* pal) {
    glGenTextures(1, &id);
    glBindTexture(0, id);
    if (glTexImage2D(0, 0, GL_RGB256, TEXTURE_SIZE_64, TEXTURE_SIZE_32, 0, TEXGEN_TEXCOORD | GL_TEXTURE_COLOR0_TRANSPARENT, (u8*)bitmap) == 0) return false;
    glColorTableEXT(0, 0, 256, 0, 0, (u16*)pal);
    return true;
}

// Atlas de letras para los carteles: 128 x 32, 4 bits por texel (indice 0 transparente, 1 blanco), celda de 4 x 6 por caracter (indice = c - 32).
// Debajo de las letras (desde y = 20) van los dos pedazos del modelo de la flecha (RenderArrow): el tubo de 16 x 5 en (0, 20) y la tapa de la cola de 5 x 5 en (0, 26).
// Se pintan aca, con la paleta de 16 colores de este atlas, porque una textura mas no entra en la VRAM. Un caracter por texel: 0 transparente, 2 a 8 colores.
static const char* const ARROW_SHAFT[5] = { "2340000000000000", "3230000000005550", "2366676767678888", "3234000000005550", "2340000000000000" };
static const char* const ARROW_CAP[5]   = { "43434", "34743", "47674", "34743", "43434" };
static bool loadSignFont() {
    static u8 bmp[128 * 32 / 2];
    static const u16 pal[16] = { 0, 0x7FFF, RGB15(30, 30, 30), RGB15(25, 25, 25), RGB15(29, 29, 29), RGB15(13, 13, 14), RGB15(17, 13, 7), RGB15(12, 9, 4), RGB15(8, 8, 9), 0, 0, 0, 0, 0, 0, 0 };
    memset(bmp, 0, sizeof(bmp));
    for (int c = 32; c < 128; c++) {
        const char* const* rows = MenuFont::glyphRows((char)c);
        if (!rows) continue;
        const int cx = ((c - 32) & 31) * 4, cy = ((c - 32) >> 5) * 6;
        for (int r = 0; r < 6; r++)
            for (int k = 0; k < 4; k++) {
                if (rows[r][k] != '#') continue;
                const int x = cx + k, y = cy + r, i = y * 128 + x;
                bmp[i >> 1] |= (u8)((i & 1) ? (1 << 4) : 1);
            }
    }
    for (int part = 0; part < 2; part++) {
        const char* const* rows = part == 0 ? ARROW_SHAFT : ARROW_CAP;
        const int w = part == 0 ? 16 : 5, oy = part == 0 ? 20 : 26;
        for (int r = 0; r < 5; r++)
            for (int k = 0; k < w; k++) {
                const int v = rows[r][k] - '0';
                if (v == 0) continue;
                const int i = (oy + r) * 128 + k;
                bmp[i >> 1] |= (u8)((i & 1) ? (v << 4) : v);
            }
    }
    glGenTextures(1, &Texture::signFontTextureID);
    glBindTexture(0, Texture::signFontTextureID);
    if (glTexImage2D(0, 0, GL_RGB16, TEXTURE_SIZE_128, TEXTURE_SIZE_32, 0, TEXGEN_TEXCOORD | GL_TEXTURE_COLOR0_TRANSPARENT, bmp) == 0) return false;
    glColorTableEXT(0, 0, 16, 0, 0, pal);
    return true;
}

bool Texture::loadPaintingAtlas() {
    if (paintingTextureID != 0) return true;
    glGenTextures(1, &paintingTextureID);
    glBindTexture(0, paintingTextureID);
#ifdef PAINTING_TEX_64
    // paintingBitmap es de 128 x 128: se reduce aqui a 64 x 64 (un texel de cada cuatro) en un buffer temporal de 4 KB que se libera al subirla a la VRAM
    const GL_TEXTURE_SIZE_ENUM sz = TEXTURE_SIZE_64;
    u8* small = (u8*)malloc(64 * 64);
    if (!small) { glDeleteTextures(1, &paintingTextureID); paintingTextureID = 0; return false; }
    for (int y = 0; y < 64; y++)
        for (int x = 0; x < 64; x++) small[y * 64 + x] = ((const u8*)paintingBitmap)[(y * 2) * 128 + x * 2];
    const u8* src = small;
#else
    const GL_TEXTURE_SIZE_ENUM sz = TEXTURE_SIZE_128;
    const u8* src = (const u8*)paintingBitmap;
#endif
    const int r = glTexImage2D(0, 0, GL_RGB256, sz, sz, 0, TEXGEN_TEXCOORD, (u8*)src);
#ifdef PAINTING_TEX_64
    free(small);
#endif
    if (r == 0) {
        glDeleteTextures(1, &paintingTextureID);
        paintingTextureID = 0;   // no cabe: los cuadros se dibujan en blanco plano, como antes
        return false;
    }
    glColorTableEXT(0, 0, 256, 0, 0, (u16*)paintingPal);
    return true;
}

static bool loadSky16(int& id, const unsigned char* bmp, const unsigned short* pal) {
    glGenTextures(1, &id);
    glBindTexture(0, id);
    if (glTexImage2D(0, 0, GL_RGB16, TEXTURE_SIZE_32, TEXTURE_SIZE_32, 0, TEXGEN_TEXCOORD | GL_TEXTURE_COLOR0_TRANSPARENT, (u8*)bmp) == 0) { glDeleteTextures(1, &id); id = 0; return false; }
    glColorTableEXT(0, 0, 16, 0, 0, (u16*)pal);
    return true;
}

// sol y luna: 512 bytes cada uno; si no caben no se dibujan (no cuenta como fallo)
bool Texture::loadSkyTextures() {
    bool ok = true;
    if (skySunTextureID == 0 && !loadSky16(skySunTextureID, skySunBitmap, skySunPal)) ok = false;
    if (skyMoonTextureID == 0 && !loadSky16(skyMoonTextureID, skyMoonBitmap, skyMoonPal)) ok = false;
    return ok;
}

bool Texture::loadHostileAtlases() {
    bool ok = true;
    if (!loadHostile(zombieTextureID, zombieBitmap, zombiePal)) ok = false;
    if (!loadHostile(skeletonTextureID, skeletonBitmap, skeletonPal)) ok = false;
    if (!loadHostile(spiderTextureID, spiderBitmap, spiderPal)) ok = false;
    if (!loadHostile(spidereyesTextureID, spidereyesBitmap, spidereyesPal)) ok = false;
    if (!loadHostile(creeperTextureID, creeperBitmap, creeperPal)) ok = false;
    if (!loadHostile(pigzombieTextureID, pigzombieBitmap, pigzombiePal)) ok = false;
    if (!loadHostile(ghastTextureID, ghastBitmap, ghastPal)) ok = false;
    if (!loadHostile(ghastfireTextureID, ghastfireBitmap, ghastfirePal)) ok = false;
    if (!loadHostile(slimeTextureID, slimeBitmap, slimePal)) ok = false;
    if (!loadHostile(squidTextureID, squidBitmap, squidPal)) ok = false;
    if (!loadHostile(wolfTextureID, wolfBitmap, wolfPal)) ok = false;
    if (!loadHostile(wolfangryTextureID, wolfangryBitmap, wolfangryPal)) ok = false;
    if (!loadHostile(wolftameTextureID, wolftameBitmap, wolftamePal)) ok = false;
    if (!loadHostile(signTextureID, signBitmap, signPal)) ok = false;
    if (!loadSignFont()) ok = false;
    loadSkyTextures();
    loadPaintingAtlas();   // si no cabe, los cuadros siguen en blanco plano (no cuenta como fallo)
    return ok;
}

// Las texturas de las entidades (oveja, vagoneta, bote, vaca, cerdo y gallina) se sueltan mientras el panorama del menu ocupa la VRAM (32 KB seguidos)
// y se vuelven a cargar al salir del menu.
static bool s_entityUnloaded = false;

void Texture::unloadEntityTextures() {
    if (s_entityUnloaded) return;
    int* ids[24] = { &skySunTextureID, &skyMoonTextureID, &paintingTextureID, &sheepTextureID, &cartTextureID, &boatTextureID, &cowTextureID, &pigTextureID, &chickenTextureID,
                     &zombieTextureID, &skeletonTextureID, &spiderTextureID, &spidereyesTextureID, &creeperTextureID, &pigzombieTextureID,
                     &ghastTextureID, &ghastfireTextureID, &slimeTextureID, &squidTextureID, &wolfTextureID, &wolfangryTextureID, &wolftameTextureID, &signTextureID, &signFontTextureID };
    for (int i = 0; i < 24; i++) {
        if (*ids[i] != 0) glDeleteTextures(1, ids[i]);
        *ids[i] = 0;
    }
    s_entityUnloaded = true;
}

void Texture::reloadEntityTextures() {
    if (!s_entityUnloaded) return;
    s_entityUnloaded = false;
    loadSheepAtlas();
    loadCartAtlas();
    loadBoatAtlas();
    loadAnimalAtlases();
    loadHostileAtlases();
}
