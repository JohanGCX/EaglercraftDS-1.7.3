#ifndef PROFILER_H
#define PROFILER_H

#include <nds.h>

namespace Prof {
    enum Slot { UPDATE = 0, GEN_COL, GEN_STRUCT, FACES, SKY, DL_REBUILD, DRAW_ALL, DL_LOD, DL_CULL, DL_CALL, LOGIC, ENTS, FRAME, SLOT_COUNT };

    void init();
    u32 now();
    void add(int slot, u32 ticks);
    void endFrame();
    void print();
    void setVerts(int v);
    void resetMinRam();

    // Registro de picos: cada paso concreto (un árbol, un lago, una banda de luz...) avisa cuánto tardó; de cada cuadro se guarda el más pesado.
    // Los cuadros de 30 ms o más se anotan (los 48 peores) con el tiempo de cada parte, ese paso y dónde estaba el jugador.
    void step(const char* label, u32 ticks);
    void setContext(int x, int y, int z, int dimension);   // posición del jugador, para el registro
    int spikeCount();                                       // picos anotados que todavía no se guardaron
    bool flushSpikes();                                     // los agrega a /mcdsefiles/spikes.txt y los borra de la memoria
}

struct ProfScope {
    int slot;
    u32 start;
    ProfScope(int s) : slot(s), start(Prof::now()) {}
    ~ProfScope() { Prof::add(slot, Prof::now() - start); }
};

struct ProfStep {
    const char* label;
    u32 start;
    ProfStep(const char* l) : label(l), start(Prof::now()) {}
    ~ProfStep() { Prof::step(label, Prof::now() - start); }
};

#define PROF(slot) ProfScope profScopeGuard(slot)
#define PROF_CAT2(a, b) a##b
#define PROF_CAT(a, b) PROF_CAT2(a, b)
#define PROFSTEP(label) ProfStep PROF_CAT(profStepGuard, __LINE__)(label)

#endif