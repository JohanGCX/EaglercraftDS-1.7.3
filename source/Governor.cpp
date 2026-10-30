#include "Governor.h"

static const unsigned int TICKS_PER_MS = 33514;
static const unsigned int SLOW_MS = 20;        // por encima: presupuestos al 50 %
static const unsigned int SKIP_MS = 26;        // por encima: el cuadro siguiente salta el trabajo pesado
static const int MAX_SKIPS_IN_A_ROW = 2;       // nunca más de dos cuadros seguidos sin trabajo pesado

static bool s_enabled = false;
static bool s_skip = false;
static int s_skipsInARow = 0;
static int s_percent = 100;

void Governor::setEnabled(bool on) {
    s_enabled = on;
    if (!on) { s_skip = false; s_skipsInARow = 0; s_percent = 100; }
}

void Governor::onFrameEnd(unsigned int frameTicks) {
    if (!s_enabled) return;
    const unsigned int ms = frameTicks / TICKS_PER_MS;
    s_percent = (ms > SLOW_MS) ? 50 : 100;
    if (ms > SKIP_MS && s_skipsInARow < MAX_SKIPS_IN_A_ROW) { s_skip = true; ++s_skipsInARow; }
    else { s_skip = false; s_skipsInARow = 0; }
}

bool Governor::skipHeavy() { return s_enabled && s_skip; }
int Governor::budgetPercent() { return s_enabled ? s_percent : 100; }