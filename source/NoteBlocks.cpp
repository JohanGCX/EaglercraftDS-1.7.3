#include "NoteBlocks.h"
#include "BlockType.h"
#include "Chunk.h"
#include "ChunkManager.h"
#include "WorldConfig.h"
#include "Sound.h"
#include <nds.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

namespace {
    struct Entry { bool used; int gx, gy, gz; int note; bool prev; };
    Entry s_notes[NoteBlocks::MAX_NOTES];

    Entry* find(int gx, int gy, int gz) {
        for (int i = 0; i < NoteBlocks::MAX_NOTES; i++)
            if (s_notes[i].used && s_notes[i].gx == gx && s_notes[i].gy == gy && s_notes[i].gz == gz) return &s_notes[i];
        return nullptr;
    }
    Entry* findOrCreate(int gx, int gy, int gz) {
        Entry* e = find(gx, gy, gz);
        if (e) return e;
        for (int i = 0; i < NoteBlocks::MAX_NOTES; i++) {
            if (s_notes[i].used) continue;
            s_notes[i].used = true; s_notes[i].gx = gx; s_notes[i].gy = gy; s_notes[i].gz = gz; s_notes[i].note = 0; s_notes[i].prev = false;
            return &s_notes[i];
        }
        return nullptr;
    }

    BlockType typeAtGrid(const ChunkManager& cm, int gx, int gy, int gz) {
        if (gy < 0 || gy >= WorldConfig::SIZE_Y) return BlockType::AIR;
        Chunk* c = cm.getChunkByChunkCoord(gx >> 3, gz >> 3);
        if (!c || !c->isGenerationComplete()) return BlockType::STONE;
        return c->getBlockType(gx & 7, gy, gz & 7);
    }

    // Material del bloque de abajo -> instrumento (TileEntityNote.triggerNote): 1 roca (bombo), 2 arena (caja), 3 vidrio (platillo), 4 madera (bajo), 0 arpa
    int instrumentFor(BlockType t) {
        if (t == BlockType::STONE || t == BlockType::COBBLESTONE || t == BlockType::MOSSY_COBBLESTONE || t == BlockType::BEDROCK || t == BlockType::OBSIDIAN ||
            t == BlockType::NETHERRACK || t == BlockType::SANDSTONE || t == BlockType::COAL_ORE || t == BlockType::IRON_ORE || t == BlockType::GOLD_ORE ||
            t == BlockType::DIAMOND_ORE || t == BlockType::REDSTONE_ORE || t == BlockType::REDSTONE_ORE_LIT || t == BlockType::LAPIS_ORE ||
            t == BlockType::MOB_SPAWNER || isFurnaceBlock(t) || isDispenserBlock(t) || isSlabBlock(t) || isDoubleSlabBlock(t) || t == BlockType::STAIRS_COBBLE) {
            if (t == BlockType::SLAB_WOOD || t == BlockType::DOUBLE_SLAB_WOOD) return 4;   // la losa de madera es de material madera
            return 1;
        }
        if (t == BlockType::BOOKSHELF) return 4;   // material madera
        if (t == BlockType::SAND || t == BlockType::GRAVEL || t == BlockType::SOUL_SAND) return 2;
        if (t == BlockType::GLASS || t == BlockType::GLOWSTONE) return 3;
        if (t == BlockType::WOOD_PLANKS || t == BlockType::OAK_LOG || t == BlockType::BIRCH_LOG || t == BlockType::SPRUCE_LOG || t == BlockType::CRAFTING_TABLE ||
            t == BlockType::CHEST || t == BlockType::NOTE_BLOCK || t == BlockType::JUKEBOX || t == BlockType::FENCE || t == BlockType::STAIRS_WOOD || t == BlockType::TRAPDOOR ||
            t == BlockType::DOOR_WOOD || t == BlockType::SIGN_POST || t == BlockType::SIGN_WALL) return 4;
        return 0;
    }
}

int NoteBlocks::noteAt(int gx, int gy, int gz) { const Entry* e = find(gx, gy, gz); return e ? e->note : 0; }

void NoteBlocks::changePitch(int gx, int gy, int gz) {
    Entry* e = findOrCreate(gx, gy, gz);
    if (e) e->note = (e->note + 1) % 25;
}

void NoteBlocks::trigger(const ChunkManager& cm, int gx, int gy, int gz) {
    if (typeAtGrid(cm, gx, gy + 1, gz) != BlockType::AIR) return;
    const int instrument = instrumentFor(typeAtGrid(cm, gx, gy - 1, gz));
    const Entry* e = find(gx, gy, gz);
    const int note = e ? e->note : 0;
    Sound::note(instrument, note, (float)(gx - 4) + 0.5f, (float)(gy - 1), (float)(gz - 4) + 0.5f);   // Sound atenua por distancia (32 bloques) y panea
}

bool NoteBlocks::powerChanged(int gx, int gy, int gz, bool powered) {
    Entry* e = findOrCreate(gx, gy, gz);
    if (!e) return powered;   // tabla llena: sin estado, suena cada vez que se enciende
    const bool changed = e->prev != powered;
    e->prev = powered;
    return changed && powered;
}

void NoteBlocks::remove(int gx, int gy, int gz) {
    Entry* e = find(gx, gy, gz);
    if (e) e->used = false;
}

void NoteBlocks::clearAll() { memset(s_notes, 0, sizeof(s_notes)); }

void NoteBlocks::setListener(float, float, float) {}   // sin uso: Sound ya conoce al jugador (se conserva por compatibilidad)

void NoteBlocks::tickSound() {}   // las notas son muestras cortas: no hay que cortarlas

// notes.dat: 'MNOT', version 1, cantidad, y por bloque: gx, gy, gz (int32) y la nota (1 byte)
bool NoteBlocks::saveToFile(const char* path) {
    FILE* f = fopen(path, "wb");
    if (!f) return false;
    int n = 0;
    for (int i = 0; i < MAX_NOTES; i++) if (s_notes[i].used && s_notes[i].note != 0) n++;
    fputc('M', f); fputc('N', f); fputc('O', f); fputc('T', f);
    fputc(1, f);
    fputc(n, f);
    for (int i = 0; i < MAX_NOTES; i++) {
        const Entry& e = s_notes[i];
        if (!e.used || e.note == 0) continue;
        const int p[3] = { e.gx, e.gy, e.gz };
        fwrite(p, 4, 3, f);
        fputc(e.note, f);
    }
    fclose(f);
    return true;
}

bool NoteBlocks::loadFromFile(const char* path) {
    clearAll();
    FILE* f = fopen(path, "rb");
    if (!f) return false;
    char magic[4];
    if (fread(magic, 1, 4, f) != 4 || magic[0] != 'M' || magic[1] != 'N' || magic[2] != 'O' || magic[3] != 'T' || fgetc(f) != 1) { fclose(f); return false; }
    const int n = fgetc(f);
    for (int i = 0; i < n && i < MAX_NOTES; i++) {
        int p[3];
        if (fread(p, 4, 3, f) != 3) break;
        const int note = fgetc(f);
        Entry* e = findOrCreate(p[0], p[1], p[2]);
        if (e && note >= 0 && note <= 24) e->note = note;
    }
    fclose(f);
    return true;
}
