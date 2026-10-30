#include "TextureAnim.h"
#include "TextureAnimCore.h"
#include "AnimData.h"
#include "Texture.h"
#include "terrain.h"
#include "AnimItems.h"
#include <nds.h>

namespace {
    unsigned char s_mapFire0[ANIM_COLORS];
    unsigned char s_mapFire1[ANIM_COLORS];
    unsigned char s_mapPortal[ANIM_COLORS];
    unsigned char* s_texture = nullptr;
    int s_vblanks = 0;
    int s_frame = 0;
}

void TextureAnim::init() {
    s_texture = nullptr;

    // El puntero a la textura tiene que caer en el banco de texturas A..D mapeado como LCD (0x06800000..0x06880000); si no, sin animación
    unsigned int addr = (unsigned int)glGetTexturePointer(Texture::textureID);
    if (addr < 0x06800000u || addr >= 0x06880000u) return;

    const unsigned short* pal = (const unsigned short*)terrainPal;
    TextureAnimCore::buildMap(FIRE0_COLORS, ANIM_COLORS, pal, s_mapFire0);
    TextureAnimCore::buildMap(FIRE1_COLORS, ANIM_COLORS, pal, s_mapFire1);
    TextureAnimCore::buildMap(PORTAL_COLORS, ANIM_COLORS, pal, s_mapPortal);

    s_texture = (unsigned char*)addr;
    s_vblanks = 0;
    s_frame = 0;
}

void TextureAnim::onVBlank() {
    if (!s_texture && !AnimItems::itemsTexturePointer()) return;
    if (++s_vblanks < 3) return;
    s_vblanks = 0;
    s_frame = (s_frame + 1) & (ANIM_FRAMES - 1);
    if (!s_texture) {   // sin atlas del terreno animable: solo la brujula y el reloj
        vramSetBankA(VRAM_A_LCD);
        vramSetBankD(VRAM_D_LCD);
        AnimItems::onVBlank(AnimItems::itemsTexturePointer());
        vramSetBankA(VRAM_A_TEXTURE);
        vramSetBankD(VRAM_D_TEXTURE);
        return;
    }

    // Los bancos de texturas (A y D, ver Renderer::init) pasan a LCD un instante para poder escribirlos, y vuelven a ser texturas
    vramSetBankA(VRAM_A_LCD);
    vramSetBankD(VRAM_D_LCD);
    TextureAnimCore::blitCell(s_texture, 15, 1, FIRE0_FRAMES[s_frame], s_mapFire0);
    TextureAnimCore::blitCell(s_texture, 15, 2, FIRE1_FRAMES[s_frame], s_mapFire1);
    TextureAnimCore::blitCell(s_texture, 14, 0, PORTAL_FRAMES[s_frame], s_mapPortal);
    AnimItems::onVBlank(AnimItems::itemsTexturePointer());
    vramSetBankA(VRAM_A_TEXTURE);
    vramSetBankD(VRAM_D_TEXTURE);
}