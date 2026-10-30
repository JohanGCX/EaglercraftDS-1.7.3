#include "Sound.h"
#include "Options.h"
#include "SoundData.h"
#include "SoundAmbient.h"
#include <nds.h>
#include <math.h>
#include <stdlib.h>

namespace {
    bool  s_init = false;
    float s_lx = 0, s_ly = 0, s_lz = 0, s_cosY = 1, s_sinY = 0;
    unsigned s_frame = 0;
    unsigned s_last[S_COUNT + 4];       // ultimo cuadro en que sono cada clip (anti-saturacion)
    unsigned s_lastAny = 0;
    int s_alt = 0;                  // alterna entre las 2 variantes

    const float RANGE = 16.0f;

    void playClip(const SoundClip& c, int slot, float vol, float pitch, float pan) {
        if (!Options::get().sound || !s_init) return;
        if (s_frame - s_last[slot] < 3) return;
        s_last[slot] = s_frame;
        if (vol <= 0.02f) return;
        if (vol > 1.0f) vol = 1.0f;
        int v = (int)(vol * 127.0f); if (v < 1) v = 1;
        int p = (int)(pan * 64.0f + 64.0f); if (p < 0) p = 0; if (p > 127) p = 127;
        int f = (int)((float)c.rate * pitch); if (f < 4000) f = 4000; if (f > 32000) f = 32000;
        soundPlaySample(c.data, SoundFormat_8Bit, c.size, (u16)f, (u8)v, (u8)p, false, 0);
    }

    void raw(int id, float vol, float pitch, float pan) {
        if (id < 0 || id >= S_COUNT) return;
        playClip(SOUND_CLIPS[id], id, vol, pitch, pan);
    }

    float rnd() { return (float)(rand() & 255) / 255.0f; }

    void at(int id, float x, float y, float z, float vol, float pitch) {
        const float dx = x - s_lx, dy = y - s_ly, dz = z - s_lz;
        const float d2 = dx * dx + dy * dy + dz * dz;
        if (d2 > RANGE * RANGE) return;
        const float d = sqrtf(d2);
        float a = vol * (1.0f - d / RANGE);
        float pan = 0.0f;
        if (d > 0.5f) {
            const float rx = s_cosY, rz = -s_sinY;   // derecha de la camara
            pan = (dx * rx + dz * rz) / d;
            pan *= 0.8f;
        }
        raw(id, a, pitch, pan);
    }

    int stepBase(int g) { return g < 3 ? S_STEP_GRASS1 + 2 * g : S_STEP_SAND1 + 2 * (g - 3); }
    int digBase(int g)  { return g < 3 ? S_DIG_GRASS1 + 2 * g  : S_STEP_SAND1 + 2 * (g - 3); }   // arena, grava y nieve: sin clips de romper, se usan los de pasos

    int pick(int base) {   // base = primer clip de un par
        s_alt ^= 1;
        return base + ((rand() & 1) ^ s_alt);
    }
}

void Sound::init() {
    if (s_init) return;
    soundEnable();
    for (int i = 0; i < S_COUNT; i++) s_last[i] = 0;
    s_init = true;
}

void Sound::setListener(float x, float y, float z, float yawDeg) {
    s_lx = x; s_ly = y; s_lz = z;
    const float r = yawDeg * 3.14159265f / 180.0f;
    s_cosY = cosf(r); s_sinY = sinf(r);
    s_frame++;
}

Sound::Group Sound::groupOf(BlockType t) {
    switch (t) {
        case BlockType::OAK_LOG: case BlockType::BIRCH_LOG: case BlockType::SPRUCE_LOG:
        case BlockType::WOOD_PLANKS: case BlockType::CRAFTING_TABLE: case BlockType::CHEST:
        case BlockType::DOOR_WOOD: case BlockType::TRAPDOOR: case BlockType::SIGN_POST: case BlockType::SIGN_WALL:
        case BlockType::PLATE_WOOD: case BlockType::SLAB_WOOD: case BlockType::DOUBLE_SLAB_WOOD:
        case BlockType::STAIRS_WOOD: case BlockType::LADDER: case BlockType::FENCE:
        case BlockType::NOTE_BLOCK: case BlockType::BOOKSHELF: case BlockType::JUKEBOX:
        case BlockType::BED_BLOCK: case BlockType::PUMPKIN: case BlockType::JACK_O_LANTERN:
        case BlockType::CAKE_BLOCK: case BlockType::LEVER: case BlockType::BUTTON:
            return G_WOOD;
        case BlockType::STONE: case BlockType::COBBLESTONE: case BlockType::MOSSY_COBBLESTONE:
        case BlockType::COAL_ORE: case BlockType::IRON_ORE: case BlockType::GOLD_ORE: case BlockType::REDSTONE_ORE:
        case BlockType::REDSTONE_ORE_LIT: case BlockType::LAPIS_ORE: case BlockType::DIAMOND_ORE:
        case BlockType::FURNACE: case BlockType::FURNACE_W: case BlockType::FURNACE_N: case BlockType::FURNACE_E:
        case BlockType::FURNACE_LIT_S: case BlockType::FURNACE_LIT_W: case BlockType::FURNACE_LIT_N: case BlockType::FURNACE_LIT_E:
        case BlockType::DISPENSER: case BlockType::DISPENSER_W: case BlockType::DISPENSER_N: case BlockType::DISPENSER_E:
        case BlockType::BEDROCK: case BlockType::OBSIDIAN: case BlockType::NETHERRACK: case BlockType::GLASS:
        case BlockType::GLOWSTONE: case BlockType::SANDSTONE: case BlockType::BRICK_BLOCK: case BlockType::MOB_SPAWNER:
        case BlockType::BLOCK_IRON: case BlockType::BLOCK_GOLD: case BlockType::BLOCK_DIAMOND: case BlockType::BLOCK_LAPIS:
        case BlockType::DOOR_IRON: case BlockType::PLATE_STONE: case BlockType::ICE:
        case BlockType::SLAB_STONE: case BlockType::SLAB_SAND: case BlockType::SLAB_COBBLE:
        case BlockType::DOUBLE_SLAB_STONE: case BlockType::DOUBLE_SLAB_SAND: case BlockType::DOUBLE_SLAB_COBBLE:
        case BlockType::STAIRS_COBBLE: case BlockType::RAIL: case BlockType::RAIL_POWERED: case BlockType::RAIL_DETECTOR:
        case BlockType::PISTON_BASE: case BlockType::PISTON_STICKY: case BlockType::PISTON_HEAD: case BlockType::PISTON_MOVING:
        case BlockType::REPEATER_OFF: case BlockType::REPEATER_ON:
            return G_STONE;
        case BlockType::SAND: case BlockType::SOUL_SAND:
            return G_SAND;
        case BlockType::GRAVEL: case BlockType::DIRT: case BlockType::CLAY: case BlockType::FARMLAND:   // Beta: la tierra suena a grava
            return G_GRAVEL;
        case BlockType::SNOW_BLOCK: case BlockType::SNOW_LAYER:
            return G_SNOW;
        default:
            return G_GRASS;
    }
}

void Sound::blockBreak(BlockType t, float x, float y, float z) {
    if (t == BlockType::AIR || isLiquidBlock(t)) return;
    const int g = (int)groupOf(t);   // orden en SoundData: GRASS, WOOD, STONE
    at(pick(digBase(g)), x, y, z, 1.0f, 0.8f + rnd() * 0.15f);
}

void Sound::blockPlace(BlockType t, float x, float y, float z) {
    if (t == BlockType::AIR || isLiquidBlock(t)) return;
    const int g = (int)groupOf(t);
    at(pick(digBase(g)), x, y, z, 0.7f, 0.75f + rnd() * 0.1f);
}

void Sound::footstep(BlockType under) {
    if (under == BlockType::AIR || isLiquidBlock(under)) return;
    const int g = (int)groupOf(under);
    raw(pick(stepBase(g)), 0.3f, 0.95f + rnd() * 0.1f, 0.0f);
}

void Sound::playerHurt() {
    raw(pick(S_HURT_PLAYER1), 1.0f, 0.95f + rnd() * 0.1f, 0.0f);
}

void Sound::explosion(float x, float y, float z) {
    at(S_EXPLODE, x, y, z, 1.0f, 0.9f + rnd() * 0.2f);
}

void Sound::fuse(float x, float y, float z) {
    at(S_FUSE, x, y, z, 0.9f, 1.0f);
}

static int mobClip(int kind, bool hurt) {
    int say = -1;
    switch (kind) {
        case 0: say = S_COW_SAY; break;
        case 1: say = S_PIG_SAY; break;
        case 2: say = S_CHICKEN_SAY; break;
        case 200: say = S_SHEEP_SAY; break;
        case 100: say = S_ZOMBIE_SAY; break;
        case 101: say = S_SKELETON_SAY; break;
        case 102: say = S_SPIDER_SAY; break;
        case 103: say = S_CREEPER_SAY; break;
        case 104: say = S_PIGZOMBIE_SAY; break;
        case 105: say = S_SLIME_SAY; break;
        case 106: say = S_GHAST_SAY; break;
        case 107: say = S_WOLF_SAY; break;
        default: return -1;   // calamar y otros: sin sonido
    }
    return hurt ? say + 1 : say;   // en SoundData cada _HURT va justo despues de su _SAY
}

void Sound::mobHurt(int kind, float x, float y, float z) {
    const int c = mobClip(kind, true);
    if (c >= 0) at(c, x, y, z, 1.0f, 0.9f + rnd() * 0.2f);
}

void Sound::mobSay(int kind, float x, float y, float z) {
    const int c = mobClip(kind, false);
    if (c >= 0) at(c, x, y, z, 0.8f, 0.9f + rnd() * 0.2f);
}

static void cellToWorld(int gx, int gy, int gz, float& x, float& y, float& z) {
    x = (float)(gx - 4) + 0.5f; y = (float)(gy - 1); z = (float)(gz - 4) + 0.5f;   // la cuadricula se desplaza 4 celdas respecto al mundo
}

void Sound::doorCell(bool open, int gx, int gy, int gz) {
    float x, y, z; cellToWorld(gx, gy, gz, x, y, z);
    at(open ? S_DOOR_OPEN : S_DOOR_CLOSE, x, y, z, 1.0f, 0.9f + rnd() * 0.1f);
}

void Sound::uiClick() {
    if (!Options::get().sound || !s_init) return;
    const SoundClip& c = SOUND_CLIPS[S_UI_CLICK];
    soundPlaySample(c.data, SoundFormat_8Bit, c.size, c.rate, 100, 64, false, 0);   // sin el filtro por cuadro: en los menus el contador de cuadros no avanza
}

void Sound::clickCell(int gx, int gy, int gz) {
    float x, y, z; cellToWorld(gx, gy, gz, x, y, z);
    at(S_CLICK, x, y, z, 0.8f, 0.6f);
}

void Sound::splash(float x, float y, float z, float vol) {
    at(S_SPLASH, x, y, z, vol, 0.9f + rnd() * 0.2f);
}

// EntityRenderer.addRainParticles: "ambient.weather.rain" a 0,1 (pitch 0,5, bajo techo) o 0,2 (pitch 1,0)
void Sound::rain(float x, float y, float z, bool covered) {
    const float dx = x - s_lx, dz = z - s_lz;
    float pan = 0.0f;
    const float d = sqrtf(dx * dx + dz * dz);
    if (d > 0.5f) pan = (dx * s_cosY - dz * s_sinY) / d * 0.8f;
    s_alt ^= 1;
    playClip(AMB_CLIPS[A_RAIN1 + s_alt], S_COUNT + s_alt, covered ? 0.45f : 0.7f, covered ? 0.5f : 1.0f, pan);
}

// EntityLightningBolt: "ambient.weather.thunder" a volumen 10000 (se oye desde lejos) y "random.explode" a 2
void Sound::thunder(float x, float y, float z) {
    const float dx = x - s_lx, dy = y - s_ly, dz = z - s_lz;
    const float d = sqrtf(dx * dx + dy * dy + dz * dz);
    float vol = 1.0f - d / 160.0f;
    if (vol < 0.35f) vol = 0.35f;
    float pan = 0.0f;
    if (d > 0.5f) pan = (dx * s_cosY - dz * s_sinY) / d * 0.6f;
    s_alt ^= 1;
    s_last[S_COUNT + 2 + s_alt] = 0;
    playClip(AMB_CLIPS[A_THUNDER1 + s_alt], S_COUNT + 2 + s_alt, vol, 0.8f + rnd() * 0.2f, pan);
    s_last[S_EXPLODE] = 0;
    raw(S_EXPLODE, vol * 0.7f, 0.5f + rnd() * 0.2f, pan);
}
