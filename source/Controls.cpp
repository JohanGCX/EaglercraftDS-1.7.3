#include "Controls.h"
#include "Options.h"
#include <stdio.h>
#include <string.h>

static const char* PATH = "/mcdsefiles/controlsprofile.dat";

// Botones asignables en el orden en que los recorre izquierda / derecha (el 0 = ninguno va al final)
static const u16 KEYS[] = { KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT, KEY_A, KEY_B, KEY_X, KEY_Y, KEY_L, KEY_R, KEY_SELECT, 0 };
static const int KEY_N = (int)(sizeof(KEYS) / sizeof(KEYS[0]));

// Perfiles por defecto = lo que hacian los modos antes: normal (camara por touch) y xyba (camara por botones)
static const u16 DEFAULTS[2][Controls::COUNT] = {
    // FORWARD  BACK      LEFT      RIGHT      JUMP   BREAK  PLACE  SNEAK  THIRD  LOOK_L  LOOK_R  LOOK_U  LOOK_D
    { KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT, KEY_A, KEY_R, KEY_L, KEY_B, KEY_Y, 0, 0, 0, 0 },
    { KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT, 0,     KEY_R, KEY_L, 0,     0,     KEY_Y, KEY_A, KEY_X, KEY_B },
};

static u16 s_keys[2][Controls::COUNT];

static bool valid(u16 k) { for (int i = 0; i < KEY_N; i++) if (KEYS[i] == k) return true; return false; }

void Controls::resetProfile(int mode) {
    if (mode < 0 || mode > 1) return;
    for (int a = 0; a < COUNT; a++) s_keys[mode][a] = DEFAULTS[mode][a];
}

// Formato: "MCK1" + 2 perfiles x 13 acciones x 2 bytes (little endian)
void Controls::load() {
    resetProfile(0);
    resetProfile(1);
    FILE* f = fopen(PATH, "rb");
    if (!f) return;
    unsigned char buf[4 + 2 * COUNT * 2];
    const size_t n = fread(buf, 1, sizeof(buf), f);
    fclose(f);
    if (n != sizeof(buf) || memcmp(buf, "MCK1", 4) != 0) return;
    int p = 4;
    for (int m = 0; m < 2; m++)
        for (int a = 0; a < COUNT; a++, p += 2) {
            const u16 k = (u16)(buf[p] | (buf[p + 1] << 8));
            if (valid(k)) s_keys[m][a] = k;
        }
}

bool Controls::save() {
    FILE* f = fopen(PATH, "wb");
    if (!f) return false;
    unsigned char buf[4 + 2 * COUNT * 2] = { 'M', 'C', 'K', '1' };
    int p = 4;
    for (int m = 0; m < 2; m++)
        for (int a = 0; a < COUNT; a++, p += 2) { buf[p] = (unsigned char)(s_keys[m][a] & 0xFF); buf[p + 1] = (unsigned char)(s_keys[m][a] >> 8); }
    const bool ok = fwrite(buf, 1, sizeof(buf), f) == sizeof(buf);
    fclose(f);
    return ok;
}

u16 Controls::getKey(int mode, Action a) { return s_keys[mode == 1 ? 1 : 0][a]; }
u16 Controls::key(Action a) { return getKey(Options::get().controlMode, a); }

void Controls::bind(int mode, Action a, u16 k) {
    mode = mode == 1 ? 1 : 0;
    if (!valid(k)) return;
    const u16 old = s_keys[mode][a];
    if (k != 0) {
        for (int b = 0; b < COUNT; b++)
            if (b != (int)a && s_keys[mode][b] == k) s_keys[mode][b] = old;   // intercambio: ninguna accion queda con el mismo boton
    }
    s_keys[mode][a] = k;
}

void Controls::cycle(int mode, Action a, int dir) {
    int i = 0;
    for (; i < KEY_N; i++) if (KEYS[i] == getKey(mode, a)) break;
    i = (i + (dir < 0 ? KEY_N - 1 : 1)) % KEY_N;
    bind(mode, a, KEYS[i]);
}

bool Controls::isBindable(u32 keysDown, u16& out) {
    for (int i = 0; i < KEY_N; i++)
        if (KEYS[i] && (keysDown & KEYS[i])) { out = KEYS[i]; return true; }
    return false;
}

const char* Controls::keyName(u16 k) {
    switch (k) {
        case KEY_A: return "A"; case KEY_B: return "B"; case KEY_X: return "X"; case KEY_Y: return "Y";
        case KEY_L: return "L"; case KEY_R: return "R"; case KEY_UP: return "UP"; case KEY_DOWN: return "DOWN";
        case KEY_LEFT: return "LEFT"; case KEY_RIGHT: return "RIGHT"; case KEY_SELECT: return "SELECT";
        default: return "none";
    }
}

const char* Controls::actionName(Action a) {
    static const char* const N[COUNT] = { "forward", "back", "strafe left", "strafe right", "jump", "break / attack", "place / use",
                                          "sneak", "third person", "look left", "look right", "look up", "look down" };
    return N[a];
}

u32 Controls::gameHeld(u32 held) {
    u32 out = held & ~(u32)(KEY_UP | KEY_DOWN | KEY_LEFT | KEY_RIGHT | KEY_A);
    if (held & key(FORWARD))      out |= KEY_UP;
    if (held & key(BACK))         out |= KEY_DOWN;
    if (held & key(STRAFE_LEFT))  out |= KEY_LEFT;
    if (held & key(STRAFE_RIGHT)) out |= KEY_RIGHT;
    if (held & key(JUMP))         out |= KEY_A;
    return out;
}
