#include "Maps.h"
#include "ChunkManager.h"
#include "Chunk.h"
#include "WorldSave.h"
#include "WorldConfig.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

namespace {
    const int MAP_SCALE = 0;   // 0 = un pixel por bloque (cada chunk de 8 x 8 se ve como 8 x 8 puntos); 3 = la escala de la Beta recien crafteado
    // MapColor.mapColorArray (colorValue)
    const unsigned MAP_RGB[14] = { 0x000000, 0x7FB238, 0xF7E9A3, 0xA7A7A7, 0xFF0000, 0xA0A0FF, 0xA7A7A7, 0x007C00,
                                   0xFFFFFF, 0xA4A8B8, 0xB76A2F, 0x707070, 0x4040FF, 0x685332 };

    struct MapSlot {
        int id = 0;               // 0 = libre
        int cx = 0, cz = 0;
        int dim = 0;
        int scale = 3;
        bool dirty = false;
        int counter = 0;          // MapData.field_28175_g
        unsigned stamp = 0;
        u8 colors[Maps::SIZE * Maps::SIZE];
    };
    MapSlot s_slots[Maps::CACHE];
    unsigned s_stamp = 0;

    const int HEADER = 16;
    const int RECORD = HEADER + Maps::SIZE * Maps::SIZE;

    FILE* openFile() {
        char path[96];
        WorldSave::getBaseFilePath("maps.dat", path);
        FILE* f = fopen(path, "rb+");
        if (!f) f = fopen(path, "wb+");
        return f;
    }

    void writeSlot(FILE* f, const MapSlot& s) {
        u8 head[HEADER];
        memset(head, 0, sizeof(head));
        head[0] = 1;
        head[1] = (u8)(s.dim < 0 ? 255 : 0);
        head[2] = (u8)s.scale;
        memcpy(head + 4, &s.cx, 4);
        memcpy(head + 8, &s.cz, 4);
        fseek(f, (long)(s.id - 1) * RECORD, SEEK_SET);
        fwrite(head, 1, HEADER, f);
        fwrite(s.colors, 1, sizeof(s.colors), f);
    }

    MapSlot* find(int id) {
        for (int i = 0; i < Maps::CACHE; i++) if (s_slots[i].id == id) { s_slots[i].stamp = ++s_stamp; return &s_slots[i]; }
        return nullptr;
    }

    // Un hueco: uno libre o el menos usado (se guarda antes si cambio)
    MapSlot* freeSlot(FILE* f) {
        MapSlot* best = &s_slots[0];
        for (int i = 0; i < Maps::CACHE; i++) {
            if (s_slots[i].id == 0) { best = &s_slots[i]; break; }
            if (s_slots[i].stamp < best->stamp) best = &s_slots[i];
        }
        if (best->id != 0 && best->dirty && f) writeSlot(f, *best);
        best->id = 0; best->dirty = false;
        return best;
    }

    MapSlot* load(int id) {
        MapSlot* s = find(id);
        if (s) return s;
        if (id <= 0) return nullptr;
        FILE* f = openFile();
        if (!f) return nullptr;
        u8 head[HEADER];
        fseek(f, (long)(id - 1) * RECORD, SEEK_SET);
        if (fread(head, 1, HEADER, f) != (size_t)HEADER || head[0] != 1) { fclose(f); return nullptr; }
        s = freeSlot(f);
        s->id = id;
        s->dim = head[1] == 255 ? -1 : 0;
        s->scale = head[2] > 4 ? 3 : head[2];
        memcpy(&s->cx, head + 4, 4);
        memcpy(&s->cz, head + 8, 4);
        s->counter = 0;
        s->dirty = false;
        s->stamp = ++s_stamp;
        if (fread(s->colors, 1, sizeof(s->colors), f) != sizeof(s->colors)) memset(s->colors, 0, sizeof(s->colors));
        fclose(f);
        return s;
    }

    // Block.blockMaterial.materialMapColor, por bloque (0 = airColor: se mira a traves)
    int mapColorOf(BlockType t) {
        if (t == BlockType::AIR) return 0;
        if (isTorchBlock(t)) return 0;
        if (isLeavesBlock(t)) return 7;
        switch (t) {
            case BlockType::GLASS: case BlockType::FIRE: case BlockType::PORTAL: case BlockType::GLOWSTONE:
            case BlockType::REDSTONE_WIRE: case BlockType::LEVER: case BlockType::BUTTON: case BlockType::REPEATER_OFF: case BlockType::REPEATER_ON:
            case BlockType::TORCH_REDSTONE_OFF: case BlockType::TORCH_REDSTONE_ON: case BlockType::RAIL: case BlockType::RAIL_POWERED: case BlockType::RAIL_DETECTOR:
            case BlockType::LADDER: case BlockType::PISTON_HEAD: case BlockType::PISTON_MOVING:
                return 0;
            case BlockType::GRASS: return 1;
            case BlockType::DIRT: case BlockType::FARMLAND: return 10;
            case BlockType::SAND: case BlockType::GRAVEL: case BlockType::SOUL_SAND: return 2;
            case BlockType::TALLGRASS: case BlockType::DEAD_BUSH: case BlockType::ROSE: case BlockType::DANDELION: case BlockType::MUSHROOM_BROWN:
            case BlockType::MUSHROOM_RED: case BlockType::SUGAR_CANE: case BlockType::CROPS: case BlockType::CACTUS: case BlockType::PUMPKIN: case BlockType::JACK_O_LANTERN:
                return 7;
            case BlockType::WATER: case BlockType::WATER_FLOW: return 12;
            case BlockType::LAVA: case BlockType::LAVA_FLOW: case BlockType::TNT: return 4;
            case BlockType::ICE: return 5;
            case BlockType::SNOW_LAYER: case BlockType::SNOW_BLOCK: return 8;
            case BlockType::CLAY: return 9;
            case BlockType::WEB: case BlockType::SPONGE: return 3;
            case BlockType::BLOCK_IRON: case BlockType::BLOCK_GOLD: case BlockType::BLOCK_DIAMOND: case BlockType::BLOCK_LAPIS: case BlockType::DOOR_IRON: return 6;
            case BlockType::OAK_LOG: case BlockType::BIRCH_LOG: case BlockType::SPRUCE_LOG: case BlockType::WOOD_PLANKS: case BlockType::CRAFTING_TABLE:
            case BlockType::CHEST: case BlockType::BOOKSHELF: case BlockType::JUKEBOX: case BlockType::NOTE_BLOCK: case BlockType::FENCE: case BlockType::DOOR_WOOD:
            case BlockType::TRAPDOOR: case BlockType::SIGN_POST: case BlockType::SIGN_WALL: case BlockType::SLAB_WOOD: case BlockType::DOUBLE_SLAB_WOOD:
            case BlockType::STAIRS_WOOD: case BlockType::BED_BLOCK: case BlockType::CAKE_BLOCK:
                return 13;
            default: break;
        }
        if (isWool(t)) return 3;
        return 11;   // piedra, menas, ladrillo, horno...: Material.rock
    }
}

void Maps::reset() {
    for (int i = 0; i < CACHE; i++) { s_slots[i].id = 0; s_slots[i].dirty = false; }
}

void Maps::flush() {
    bool any = false;
    for (int i = 0; i < CACHE; i++) if (s_slots[i].id && s_slots[i].dirty) any = true;
    if (!any) return;
    FILE* f = openFile();
    if (!f) return;
    for (int i = 0; i < CACHE; i++) if (s_slots[i].id && s_slots[i].dirty) { writeSlot(f, s_slots[i]); s_slots[i].dirty = false; }
    fclose(f);
}

int Maps::create(int centerX, int centerZ, int dimension) {
    FILE* f = openFile();
    if (!f) return 0;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    int id = (int)(size / RECORD) + 1;
    MapSlot* s = freeSlot(f);
    s->id = id; s->cx = centerX; s->cz = centerZ; s->dim = dimension < 0 ? -1 : 0; s->scale = MAP_SCALE; s->counter = 0;
    memset(s->colors, 0, sizeof(s->colors));
    s->stamp = ++s_stamp;
    writeSlot(f, *s);
    s->dirty = false;
    fclose(f);
    return id;
}

const u8* Maps::pixels(int id) {
    MapSlot* s = load(id);
    return s ? s->colors : nullptr;
}

bool Maps::playerMarker(int id, float px, float pz, int dimension, float& mx, float& my) {
    MapSlot* s = load(id);
    if (!s || s->dim != (dimension < 0 ? -1 : 0)) return false;
    const float k = (float)(1 << s->scale);
    mx = (px - (float)s->cx) / k;
    my = (pz - (float)s->cz) / k;
    return mx >= -64.0f && mx <= 64.0f && my >= -64.0f && my <= 64.0f;
}

bool Maps::colorOf(u8 value, int& r, int& g, int& b) {
    const int idx = value / 4;
    if (idx <= 0 || idx >= 14) return false;
    const int sh = value & 3;
    const int m = sh == 2 ? 255 : (sh == 0 ? 180 : 220);
    r = ((MAP_RGB[idx] >> 16) & 255) * m / 255;
    g = ((MAP_RGB[idx] >> 8) & 255) * m / 255;
    b = (MAP_RGB[idx] & 255) * m / 255;
    return true;
}

// ItemMap.func_28011_a: una columna de pixeles por tick (la columna x con x & 15 == contador & 15), alrededor del jugador. En vez de los 64 bloques de cada
// pixel se miran 4 (cuadricula 2 x 2 dentro del cuadro de 8 x 8) para que el tick de la DS aguante; solo se pinta lo que ya tiene chunk generado
void Maps::tick(const ChunkManager& cm, int id, float pxf, float pzf, int dimension) {
    MapSlot* m = load(id);
    if (!m || m->dim != (dimension < 0 ? -1 : 0)) return;
    const int S = 1 << m->scale;
    const int px = (int)floorf(pxf), pz = (int)floorf(pzf);
    const int pcx = (int)floorf((float)(px - m->cx) / (float)S) + 64;
    const int pcy = (int)floorf((float)(pz - m->cz) / (float)S) + 64;
    int R = 128 / S;
    if (dimension < 0) R /= 2;
    if (R > 48) R = 48;   // la DS solo tiene cargados unos 6 chunks alrededor: no se recorre mas (a escala 0 serian 128 pixeles de radio)
    ++m->counter;
    const bool nether = dimension < 0;

    for (int x = pcx - R + 1; x < pcx + R; x++) {
        if ((x & 15) != (m->counter & 15)) continue;
        if (x < 0 || x >= SIZE) continue;
        double prevH = 0.0;
        bool havePrev = false;
        for (int y = pcy - R - 1; y < pcy + R; y++) {
            if (y < -1 || y >= SIZE) continue;
            const int dx = x - pcx, dy = y - pcy;
            const bool edge = dx * dx + dy * dy > (R - 2) * (R - 2);
            const int wx = (m->cx / S + x - SIZE / 2) * S;
            const int wz = (m->cz / S + y - SIZE / 2) * S;
            int counts[14] = {0};
            int samples = 0, waterDepth = 0;
            double h = 0.0;
            if (nether) {
                int hsh = wx + wz * 231871;
                hsh = hsh * hsh * 31287121 + hsh * 11;
                counts[((hsh >> 20) & 1) == 0 ? 10 : 11] = 4;
                samples = 4; h = 100.0;
            } else {
                const int ns = S == 1 ? 1 : 2, off = S == 1 ? 0 : S / 4, stp = S / 2;
                for (int sz = 0; sz < ns; sz++) for (int sx = 0; sx < ns; sx++) {
                    const int bx = wx + off + sx * stp, bz = wz + off + sz * stp;
                    const int gx = bx + 4, gz = bz + 4;
                    Chunk* c = cm.getChunkByChunkCoord(gx >> 3, gz >> 3);
                    if (!c || !c->isGenerationComplete()) continue;
                    const int xi = gx & 7, zi = gz & 7;
                    int yi = WorldConfig::SIZE_Y - 1;
                    int col = 0;
                    BlockType t = BlockType::AIR;
                    for (; yi >= 1; yi--) {
                        t = c->getBlockType(xi, yi, zi);
                        col = mapColorOf(t);
                        if (col != 0) break;
                    }
                    if (col == 0) continue;
                    if (isLiquidBlock(t)) {   // la profundidad del agua oscurece el pixel
                        for (int d = yi - 1; d > 0 && d > yi - 24; d--) { if (!isLiquidBlock(c->getBlockType(xi, d, zi))) break; ++waterDepth; }
                    }
                    counts[col]++; samples++;
                    h += (double)(yi + 1);
                }
                if (samples == 0) continue;
                h /= samples;
                waterDepth = waterDepth / samples;
            }
            int best = 0, bestCount = 0;
            for (int i = 1; i < 14; i++) if (counts[i] > bestCount) { bestCount = counts[i]; best = i; }
            if (!havePrev) { prevH = h; havePrev = true; }
            double v = (h - prevH) * 4.0 / (double)(S + 4) + ((double)((x + y) & 1) - 0.5) * 0.4;
            int shade = 1;
            if (v > 0.6) shade = 2;
            if (v < -0.6) shade = 0;
            if (best == 12) {
                v = (double)waterDepth * 0.1 + (double)((x + y) & 1) * 0.2;
                shade = 1;
                if (v < 0.5) shade = 2;
                if (v > 0.9) shade = 0;
            }
            prevH = h;
            if (y >= 0 && dx * dx + dy * dy < R * R && (!edge || ((x + y) & 1) != 0)) {
                const u8 nv = (u8)(best * 4 + shade);
                if (m->colors[x + y * SIZE] != nv) { m->colors[x + y * SIZE] = nv; m->dirty = true; }
            }
        }
    }
}
