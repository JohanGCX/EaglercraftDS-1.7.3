#include "Liquid.h"

// Cola de ticks programados (World.scheduleBlockUpdate / World.TickUpdates de Beta 1.7.3).
// Sin duplicados (mismo bloque y celda), y se atienden por orden de vencimiento y, a igual vencimiento, de llegada.
namespace {
    struct Tick { int gx, gy, gz; u8 type; u8 used; u32 due; u32 seq; };
    const int CAP = 640;
    Tick s_ticks[CAP];
    int s_count = 0;
    u32 s_time = 0;
    u32 s_seq = 0;
    u32 s_nextDue = 0xFFFFFFFFu;   // cota inferior del vencimiento mas proximo: si el tiempo no llego, popDue sale sin recorrer los 640
    bool s_hell = false;
}

namespace Liquid {

void resetTicks() {
    for (int i = 0; i < CAP; i++) s_ticks[i].used = 0;
    s_count = 0; s_time = 0; s_seq = 0; s_nextDue = 0xFFFFFFFFu;
}

void advanceTime() { s_time++; }
void setHell(bool hell) { s_hell = hell; }
bool isHell() { return s_hell; }
int pendingCount() { return s_count; }

bool pushTick(int gx, int gy, int gz, BlockType t, int delay) {
    int freeSlot = -1;
    for (int i = 0; i < CAP; i++) {
        if (!s_ticks[i].used) { if (freeSlot < 0) freeSlot = i; continue; }
        if (s_ticks[i].gx == gx && s_ticks[i].gy == gy && s_ticks[i].gz == gz && s_ticks[i].type == (u8)t) return false;
    }
    if (freeSlot < 0) return false;
    Tick& e = s_ticks[freeSlot];
    e.gx = gx; e.gy = gy; e.gz = gz; e.type = (u8)t; e.used = 1;
    e.due = s_time + (u32)(delay > 0 ? delay : 0);
    e.seq = s_seq++;
    if (e.due < s_nextDue) s_nextDue = e.due;
    s_count++;
    return true;
}

bool popDue(int& gx, int& gy, int& gz, BlockType& t) {
    if (s_count <= 0 || s_nextDue > s_time) return false;
    int best = -1;
    u32 minLeft = 0xFFFFFFFFu;
    for (int i = 0; i < CAP; i++) {
        const Tick& e = s_ticks[i];
        if (!e.used) continue;
        if (e.due > s_time) { if (e.due < minLeft) minLeft = e.due; continue; }
        if (best < 0 || e.due < s_ticks[best].due || (e.due == s_ticks[best].due && e.seq < s_ticks[best].seq)) best = i;
    }
    if (best < 0) { s_nextDue = minLeft; return false; }
    gx = s_ticks[best].gx; gy = s_ticks[best].gy; gz = s_ticks[best].gz; t = (BlockType)s_ticks[best].type;
    s_ticks[best].used = 0;
    s_count--;
    s_nextDue = 0;   // puede haber mas vencidos: el proximo popDue recorre otra vez (y si no queda ninguno, fija la cota)
    return true;
}

}  // namespace Liquid
