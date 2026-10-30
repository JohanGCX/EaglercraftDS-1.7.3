#include "Profiler.h"
#include <malloc.h>
#include <stdio.h>
#include "Options.h"

#ifndef PROF_SPIKE_PATH
#define PROF_SPIKE_PATH "/mcdsefiles/spikes.txt"
#endif

static const u32 TICKS_PER_MS = 33514;
static const u32 SPIKE_THRESHOLD = TICKS_PER_MS * 30;   // desde 30 ms el cuadro se anota
static const int MAX_SPIKES = 48;
static const int PARTS = Prof::SLOT_COUNT - 1;          // las partes del cuadro (todo menos FRAME)

static u32 frameAcc[Prof::SLOT_COUNT] = {};
static u32 windowPeak[Prof::SLOT_COUNT] = {};
static u32 shownPeak[Prof::SLOT_COUNT] = {};
static int windowFrames = 0;
static u32 frameNumber = 0;

static unsigned long minFreeKB = 0xFFFFFFFF;

// el paso más pesado del cuadro en curso, y el peor cuadro de la ventana de 2 s (el que se muestra)
static const char* frameStepLabel = nullptr;
static u32 frameStepTicks = 0;
static u32 winWorstFrame = 0, winWorstStep = 0;
static const char* winWorstLabel = nullptr;
static u32 shownWorstStep = 0;
static const char* shownWorstLabel = nullptr;

struct Spike {
    u32 frame;
    u16 total;              // décimas de ms
    u16 part[PARTS];
    u16 stepMs10;
    const char* label;
    int x, y, z, dim;
};
static Spike spikes[MAX_SPIKES];
static int spikeN = 0;
static int ctxX = 0, ctxY = 0, ctxZ = 0, ctxDim = 0;

void Prof::resetMinRam() { minFreeKB = 0xFFFFFFFF; }

static const char* NAMES[Prof::SLOT_COUNT] = {
    "update", "column", "struct", "faces", "sky", "bands", "draw", "lod", "cull", "calls", "logic", "ents", "FRAME"
};
static const char* SHORT[PARTS] = { "upd", "col", "est", "car", "sky", "ban", "drw", "lod", "cul", "cal", "log", "ent" };

void Prof::init() { cpuStartTiming(2); } // temporizadores 2 y 3 en cascada
u32 Prof::now() { return cpuGetTiming(); }
void Prof::add(int slot, u32 ticks) { frameAcc[slot] += ticks; }

void Prof::step(const char* label, u32 ticks) {
    if (ticks > frameStepTicks) { frameStepTicks = ticks; frameStepLabel = label; }
}

void Prof::setContext(int x, int y, int z, int dimension) { ctxX = x; ctxY = y; ctxZ = z; ctxDim = dimension; }

static u16 ms10(u32 ticks) {
    const u32 v = (u32)(((unsigned long long)ticks * 10ULL) / TICKS_PER_MS);
    return v > 65535 ? 65535 : (u16)v;
}

static void recordSpike(u32 frameTicks) {
    int slot = spikeN;
    if (spikeN >= MAX_SPIKES) {   // lleno: se reemplaza el más leve, solo si este es peor
        slot = 0;
        for (int i = 1; i < MAX_SPIKES; i++) if (spikes[i].total < spikes[slot].total) slot = i;
        if (spikes[slot].total >= ms10(frameTicks)) return;
    } else {
        spikeN++;
    }
    Spike& s = spikes[slot];
    s.frame = frameNumber;
    s.total = ms10(frameTicks);
    for (int i = 0; i < PARTS; i++) s.part[i] = ms10(frameAcc[i]);
    s.stepMs10 = ms10(frameStepTicks);
    s.label = frameStepLabel ? frameStepLabel : "-";
    s.x = ctxX; s.y = ctxY; s.z = ctxZ; s.dim = ctxDim;
}

void Prof::endFrame() {
    const u32 frameTicks = frameAcc[FRAME];
    if (frameTicks > winWorstFrame) { winWorstFrame = frameTicks; winWorstStep = frameStepTicks; winWorstLabel = frameStepLabel; }
    if (frameTicks >= SPIKE_THRESHOLD && Options::get().spikeLog) recordSpike(frameTicks);
    frameStepTicks = 0; frameStepLabel = nullptr;
    ++frameNumber;

    for (int i = 0; i < SLOT_COUNT; i++) {
        if (frameAcc[i] > windowPeak[i]) windowPeak[i] = frameAcc[i];
        frameAcc[i] = 0;
    }
    if (++windowFrames >= 120) {
        for (int i = 0; i < SLOT_COUNT; i++) {
            shownPeak[i] = windowPeak[i];
            windowPeak[i] = 0;
        }
        shownWorstStep = winWorstStep; shownWorstLabel = winWorstLabel;
        winWorstFrame = 0; winWorstStep = 0; winWorstLabel = nullptr;
        windowFrames = 0;
    }
}

int Prof::spikeCount() { return spikeN; }

// One line per spike, oldest first: game second, frame ms and each part, the heaviest step and where the player was
bool Prof::flushSpikes() {
    if (spikeN == 0) return true;
    if (!Options::get().spikeLog) { spikeN = 0; return true; }   // spike log desactivado: se descartan
    FILE* f = fopen(PROF_SPIKE_PATH, "a");
    if (!f) return false;
    static bool header = false;
    if (!header) { fprintf(f, "---- session ----\n"); header = true; }
    for (int pass = 0; pass < spikeN; pass++) {   // orden por número de cuadro
        int k = -1;
        for (int i = 0; i < spikeN; i++) {
            if (spikes[i].frame == 0xFFFFFFFFu) continue;
            if (k < 0 || spikes[i].frame < spikes[k].frame) k = i;
        }
        if (k < 0) break;
        const Spike& s = spikes[k];
        fprintf(f, "t=%us f=%u.%u", (unsigned)(s.frame / 60), s.total / 10, s.total % 10);
        for (int i = 0; i < PARTS; i++) fprintf(f, " %s=%u.%u", SHORT[i], s.part[i] / 10, s.part[i] % 10);
        fprintf(f, " | %s %u.%u | x=%d y=%d z=%d d=%d\n", s.label, s.stepMs10 / 10, s.stepMs10 % 10, s.x, s.y, s.z, s.dim);
        spikes[k].frame = 0xFFFFFFFFu;
    }
    fclose(f);
    spikeN = 0;
    return true;
}

void Prof::print() {
    iprintf("\x1b[2J"); // limpia la consola
    const u32 sm = ms10(shownWorstStep);
    iprintf("PEAK ms (last 2 s)\nworst: %s %u.%u s%d\n\n", shownWorstLabel ? shownWorstLabel : "-", (unsigned)(sm / 10), (unsigned)(sm % 10), spikeN);
    for (int i = 0; i < SLOT_COUNT; i += 2) {   // dos columnas por linea (la consola solo tiene 24 filas)
        unsigned long h = ((unsigned long)shownPeak[i] * 100UL) / 33514UL;
        iprintf("%-7s%3lu.%02lu", NAMES[i], h / 100, h % 100);
        if (i + 1 < SLOT_COUNT) {
            unsigned long h2 = ((unsigned long)shownPeak[i + 1] * 100UL) / 33514UL;
            iprintf("  %-7s%3lu.%02lu", NAMES[i + 1], h2 / 100, h2 % 100);
        }
        iprintf("\n");
    }
    struct mallinfo mi = mallinfo();
    unsigned long freeKB = ((unsigned long)(getHeapLimit() - getHeapEnd()) + mi.fordblks) / 1024;
    if (freeKB < minFreeKB) minFreeKB = freeKB;
    iprintf("\nFree RAM %lu KB\nMin RAM  %lu KB\n", freeKB, minFreeKB);
}