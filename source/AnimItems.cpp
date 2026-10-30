#include "AnimItems.h"
#include "AnimItemsData.h"
#include "TextureAnimCore.h"
#include "Texture.h"
#include "items.h"
#include <nds.h>
#include <math.h>

namespace {
    const float TWO_PI = 6.28318530718f;
    const int COMPASS_COL = 6, COMPASS_ROW = 3, CLOCK_COL = 6, CLOCK_ROW = 4;

    unsigned char s_mapCompass[16], s_mapClock[16];
    unsigned char s_icon[2][256];       // el cuadro actual ya en indices de la paleta del atlas de items (0 = brujula, 1 = reloj)
    int s_frame[2] = { 0, 0 };
    bool s_dirtyVram[2] = { false, false };
    bool s_changed = false;
    unsigned char* s_tex = nullptr;

    float s_needle = 0.0f, s_needleVel = 0.0f;   // field_4230_i y field_4229_j de TextureCompassFX
    bool s_needleReady = false;

    void setFrame(int which, int frame) {
        if (frame == s_frame[which] && s_icon[which][0] != 255) return;
        s_frame[which] = frame;
        const unsigned char* src = which == 0 ? COMPASS_FRAMES[frame] : CLOCK_FRAMES[frame];
        const unsigned char* map = which == 0 ? s_mapCompass : s_mapClock;
        for (int i = 0; i < 256; i++) s_icon[which][i] = map[src[i]];
        s_dirtyVram[which] = true;
        s_changed = true;
    }
}

void AnimItems::init() {
    s_tex = nullptr;
    const unsigned short* pal = (const unsigned short*)itemsPal;
    TextureAnimCore::buildMap(COMPASS_COLORS, COMPASS_COLOR_COUNT, pal, s_mapCompass);
    TextureAnimCore::buildMap(CLOCK_COLORS, CLOCK_COLOR_COUNT, pal, s_mapClock);
    s_icon[0][0] = s_icon[1][0] = 255;   // fuerza el primer cuadro
    s_frame[0] = s_frame[1] = -1;
    setFrame(0, 0);
    setFrame(1, 0);
    s_changed = false;
    s_dirtyVram[0] = s_dirtyVram[1] = false;

    // el puntero tiene que caer en el banco de texturas A..D mapeado como LCD (0x06800000..0x06880000); si no, solo se animan los iconos 2D
    if (Texture::itemsTextureID == 0) return;
    const unsigned int addr = (unsigned int)glGetTexturePointer(Texture::itemsTextureID);
    if (addr < 0x06800000u || addr >= 0x06880000u) return;
    s_tex = (unsigned char*)addr;
    s_dirtyVram[0] = s_dirtyVram[1] = true;
}

unsigned char* AnimItems::itemsTexturePointer() { return s_tex; }

void AnimItems::reset() {
    s_needle = 0.0f; s_needleVel = 0.0f; s_needleReady = false;
}

void AnimItems::tick(float px, float pz, float yaw, float spawnX, float spawnZ, float celestialAngle) {
    // TextureCompassFX.onTick: el angulo hacia el punto de aparicion, medido en el sentido de las agujas del reloj desde donde mira el jugador
    const float yr = yaw * 3.14159265f / 180.0f;
    const float fx = -sinf(yr), fz = -cosf(yr);       // hacia donde mira
    const float rx = cosf(yr), rz = -sinf(yr);        // a su derecha
    const float dx = spawnX - px, dz = spawnZ - pz;
    const float target = atan2f(dx * rx + dz * rz, dx * fx + dz * fz);
    if (!s_needleReady) { s_needle = target; s_needleVel = 0.0f; s_needleReady = true; }
    float diff = target - s_needle;
    while (diff < -3.14159265f) diff += TWO_PI;
    while (diff >= 3.14159265f) diff -= TWO_PI;
    if (diff < -1.0f) diff = -1.0f;
    if (diff > 1.0f) diff = 1.0f;
    s_needleVel += diff * 0.1f;
    s_needleVel *= 0.8f;
    s_needle += s_needleVel;
    float a = s_needle;
    while (a < 0.0f) a += TWO_PI;
    while (a >= TWO_PI) a -= TWO_PI;
    setFrame(0, ((int)(a / TWO_PI * (float)COMPASS_FRAME_COUNT + 0.5f)) % COMPASS_FRAME_COUNT);

    // TextureWatchFX: la hora del dia; el cuadro 0 es el mediodia (angulo celeste 0)
    float c = celestialAngle;
    while (c < 0.0f) c += 1.0f;
    while (c >= 1.0f) c -= 1.0f;
    setFrame(1, ((int)(c * (float)CLOCK_FRAME_COUNT + 0.5f)) % CLOCK_FRAME_COUNT);
}

bool AnimItems::takeChanged() {
    const bool c = s_changed;
    s_changed = false;
    return c;
}

void AnimItems::onVBlank(unsigned char* itemsTexture) {
    if (!itemsTexture) return;
    if (s_dirtyVram[0]) {
        TextureAnimCore::blitCell(itemsTexture, COMPASS_COL, COMPASS_ROW, COMPASS_FRAMES[s_frame[0]], s_mapCompass);
        s_dirtyVram[0] = false;
    }
    if (s_dirtyVram[1]) {
        TextureAnimCore::blitCell(itemsTexture, CLOCK_COL, CLOCK_ROW, CLOCK_FRAMES[s_frame[1]], s_mapClock);
        s_dirtyVram[1] = false;
    }
}

static bool s_invAnimated = true;
void AnimItems::setInventoryAnimated(bool on) { s_invAnimated = on; }

int AnimItems::pixelOverride(int px, int py) {
    if (!s_invAnimated) return -1;
    if (px < COMPASS_COL * 16 || px >= COMPASS_COL * 16 + 16) return -1;
    if (py >= COMPASS_ROW * 16 && py < COMPASS_ROW * 16 + 16) return s_icon[0][(py - COMPASS_ROW * 16) * 16 + (px - COMPASS_COL * 16)];
    if (py >= CLOCK_ROW * 16 && py < CLOCK_ROW * 16 + 16) return s_icon[1][(py - CLOCK_ROW * 16) * 16 + (px - CLOCK_COL * 16)];
    return -1;
}
