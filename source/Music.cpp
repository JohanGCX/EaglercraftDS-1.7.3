#include "Music.h"
#include "Options.h"
#include "ImaAdpcm.h"
#include <nds.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// ---- Beta 1.7.3 timing (SoundManager): first track after 0..10 min, then 10..20 min after each track ends (20 ticks per second). ----
// Options -> sound & music -> music wait (Options::musicWait): the pause before each track, both after entering a world and between tracks.
//   0 fast: 5-15 s   1 normal: 5-10 min   2 long: 10-20 min (the Beta's gap between tracks)
static const u32 WAIT_MIN_S[3]  = { 5,  300, 600 };
static const u32 WAIT_RAND_S[3] = { 10, 300, 600 };   // plus a random 0..RAND-1 seconds

namespace {
    const char* const TRACKS[] = { "calm1", "calm2", "calm3", "hal1", "hal2", "hal3", "hal4", "nuance1", "nuance2", "piano1", "piano2", "piano3" };
    const int TRACK_COUNT = (int)(sizeof(TRACKS) / sizeof(TRACKS[0]));

    const int      RATE          = 8000;                 // Hz, same as the .mus files
    const u32      RING_SAMPLES  = 4096;                 // 8 KB of main RAM; power of two. 4096 samples = 512 ms
    const u32      RING_MASK     = RING_SAMPLES - 1;
    const int      BLOCK_BYTES   = 512;
    const int      BLOCK_SAMPLES = (BLOCK_BYTES - 4) * 2 + 1;   // 1017
    const u32      MARGIN        = 640;                  // safety (samples) against timer estimate error and ARM7 start latency
    // Music volume (Options::musicVolume 0..5 = 25, 50, 75, 100, 150, 200 %): up to 100 % it is the hardware volume (0..127);
    // above that the hardware stays at 127 and the decoded samples are multiplied in software (Q8: 256 = x1, clamped).
    const int      LEVEL_PERCENT[6] = { 25, 50, 75, 100, 150, 200 };
    int  levelPercent() { const int v = Options::get().musicVolume; return LEVEL_PERCENT[v <= 5 ? v : 3]; }
    int  hwVolume()     { const int p = levelPercent(); return p >= 100 ? 127 : (127 * p) / 100; }
    int  gainQ8()       { const int p = levelPercent(); return p > 100 ? (256 * p) / 100 : 256; }
    int  s_hwVolume = -1;           // last hardware volume sent to the channel
    // libnds programs the channel timer with SOUND_FREQ(f) = -(0x1000000 / f), and the sound timer runs at bus clock / 2,
    // so one sample lasts 2 * (0x1000000 / f) bus ticks. Using the exact figure matters: 8000 Hz would drift 0.1 % (0.3 s per track).
    const u32      TICKS_PER_SAMPLE = 2u * (0x1000000u / RATE);

    s16  s_ring[RING_SAMPLES] __attribute__((aligned(32)));
    u8   s_block[BLOCK_BYTES] __attribute__((aligned(4)));

    FILE* s_file = nullptr;
    int   s_channel = -1;
    bool  s_eof = false;
    u32   s_written = 0;            // samples decoded into the ring since the channel started (monotonic)
    u64   s_startTicks = 0;         // s_now when the channel started
    u64   s_nextStart = 0;          // s_now at which the next track may start
    bool  s_waiting = false;        // s_nextStart is valid
    bool  s_noFiles = false;        // no .mus found: do not retry until the next game session
    int   s_lastTrack = -1;
    int   s_waitLevel = -1;         // musicWait the current s_nextStart was drawn with

    // Disco de la jukebox: usa el mismo canal y el mismo anillo que la musica de fondo (nunca suenan a la vez)
    bool  s_record = false;
    float s_recX = 0, s_recY = 0, s_recZ = 0;
    int   s_recGx = 0, s_recGy = 0, s_recGz = 0;
    float s_lisX = 0, s_lisY = 0, s_lisZ = 0, s_lisYaw = 0;
    int   s_pan = 64;
    const float RECORD_RANGE = 64.0f;   // SoundManager.playStreaming: atenuacion lineal hasta 64 bloques

    float recordDistance() {
        const float dx = s_recX - s_lisX, dy = s_recY - s_lisY, dz = s_recZ - s_lisZ;
        return sqrtf(dx * dx + dy * dy + dz * dz);
    }
    // Volumen del canal: el de la musica (hwVolume) por la distancia en el caso del disco
    int channelVolume() {
        if (!s_record) return hwVolume();
        float f = 1.0f - recordDistance() / RECORD_RANGE;
        if (f < 0.0f) f = 0.0f;
        return (int)((float)hwVolume() * f);
    }
    // Paneo del disco: cuanto queda a la derecha del jugador (Player: derecha = (cos yaw, -sen yaw))
    int channelPan() {
        if (!s_record) return 64;
        const float dx = s_recX - s_lisX, dz = s_recZ - s_lisZ;
        const float d = sqrtf(dx * dx + dz * dz);
        if (d < 1.0f) return 64;
        const float yr = s_lisYaw * 3.14159265f / 180.0f;
        const float side = (dx * cosf(yr) - dz * sinf(yr)) / d;   // -1 izquierda, 1 derecha
        int p = 64 + (int)(side * 48.0f);
        return p < 0 ? 0 : (p > 127 ? 127 : p);
    }

    // 32-bit hardware timer (timers 0 and 1 in cascade; the profiler uses 2 and 3), accumulated to 64 bits. Wraps every 128 s, update() runs every frame.
    u32 s_lastRaw = 0;
    u64 s_now = 0;
    bool s_timerOn = false;

    u32 rawTicks() {
        u16 hi1, lo, hi2;
        do { hi1 = TIMER_DATA(1); lo = TIMER_DATA(0); hi2 = TIMER_DATA(1); } while (hi1 != hi2);
        return ((u32)hi1 << 16) | lo;
    }

    void tickClock() {
        if (!s_timerOn) {
            TIMER_DATA(0) = 0; TIMER_DATA(1) = 0;
            TIMER_CR(0) = TIMER_ENABLE | TIMER_DIV_1;
            TIMER_CR(1) = TIMER_ENABLE | TIMER_CASCADE;
            s_lastRaw = rawTicks();
            s_timerOn = true;
        }
        const u32 raw = rawTicks();
        s_now += (u32)(raw - s_lastRaw);
        s_lastRaw = raw;
    }

    u64 secondsToTicks(u32 s) { return (u64)s * BUS_CLOCK; }

    void scheduleNext() {
        int lvl = Options::get().musicWait; if (lvl < 0 || lvl > 2) lvl = 1;
        const u32 s = WAIT_MIN_S[lvl] + (u32)rand() % WAIT_RAND_S[lvl];
        s_nextStart = s_now + secondsToTicks(s);
        s_waitLevel = lvl;
        s_waiting = true;
    }

    void closeFile() { if (s_file) { fclose(s_file); s_file = nullptr; } }

    void stopChannel() {
        if (s_channel >= 0) { soundKill(s_channel); s_channel = -1; }
    }

    // Decodes the next block of the open file into the ring. Returns false at end of file.
    bool decodeNext() {
        if (!s_file || s_eof) return false;
        const int n = (int)fread(s_block, 1, BLOCK_BYTES, s_file);
        const u32 pos = s_written & RING_MASK;
        const int samples = ImaAdpcm::decodeBlockToRing(s_block, n, s_ring, RING_MASK, pos);
        if (samples <= 0) { s_eof = true; return false; }
        if (n < BLOCK_BYTES) s_eof = true;
        const int gain = gainQ8();
        if (gain != 256) {   // software boost above 100 %
            for (int i = 0; i < samples; i++) {
                int v = (s_ring[(pos + (u32)i) & RING_MASK] * gain) >> 8;
                if (v > 32767) v = 32767; else if (v < -32768) v = -32768;
                s_ring[(pos + (u32)i) & RING_MASK] = (s16)v;
            }
        }
        // The ARM7 reads the ring from main RAM: write the data cache back (two pieces when the block wraps)
        const u32 first = (pos + samples <= RING_SAMPLES) ? (u32)samples : RING_SAMPLES - pos;
        DC_FlushRange(&s_ring[pos], first * 2);
        if (first < (u32)samples) DC_FlushRange(&s_ring[0], ((u32)samples - first) * 2);
        s_written += (u32)samples;
        return true;
    }

    // (Re)starts the hardware channel on a freshly filled ring. Continues from the current file position.
    bool startChannel() {
        stopChannel();
        s_written = 0;
        while (s_written < RING_SAMPLES - BLOCK_SAMPLES && decodeNext()) {}
        if (s_written == 0) return false;
        s_hwVolume = channelVolume();
        s_pan = channelPan();
        s_channel = soundPlaySample(s_ring, SoundFormat_16Bit, RING_SAMPLES * 2, RATE, (u8)s_hwVolume, (u8)s_pan, true, 0);
        s_startTicks = s_now;
        return s_channel >= 0;
    }

    void finishTrack() {
        stopChannel();
        closeFile();
        s_eof = false;
        s_record = false;
        scheduleNext();   // tambien despues de un disco: la espera de la musica de fondo empieza de cero
    }

    // Abre <name>.mus: primero en la SD, despues dentro del .nds. Deja el archivo listo en el primer bloque (o nullptr)
    FILE* openTrack(const char* name) {
        char path[48];
        snprintf(path, sizeof(path), "/mcdsefiles/music/%s.mus", name);
        FILE* f = fopen(path, "rb");
        if (!f) {
            snprintf(path, sizeof(path), "nitro:/music/%s.mus", name);
            f = fopen(path, "rb");
        }
        if (!f) return nullptr;
        u8 head[16];
        if (fread(head, 1, sizeof(head), f) != sizeof(head) || memcmp(head, "MUS1", 4) != 0
            || (head[4] | (head[5] << 8)) != RATE || (head[6] | (head[7] << 8)) != BLOCK_BYTES) { fclose(f); return nullptr; }
        fseek(f, 512, SEEK_SET);   // blocks start after the 512-byte header (sector aligned)
        return f;
    }

    void startRandomTrack() {
        int start = (int)((u32)rand() % (u32)TRACK_COUNT);
        if (start == s_lastTrack) start = (start + 1) % TRACK_COUNT;
        for (int k = 0; k < TRACK_COUNT; k++) {
            const int t = (start + k) % TRACK_COUNT;
            // SD first (lets the player replace a track), then the copy built into the .nds (NitroFS: nitrofiles/music/ in the repo)
            FILE* f = openTrack(TRACKS[t]);
            if (!f) continue;
            s_file = f; s_eof = false; s_lastTrack = t;
            if (startChannel()) return;
            closeFile(); stopChannel();
        }
        s_noFiles = true;   // nothing playable: stay silent for this session
    }
}

void Music::setListener(float x, float y, float z, float yawDeg) { s_lisX = x; s_lisY = y; s_lisZ = z; s_lisYaw = yawDeg; }

bool Music::recordCell(int& gx, int& gy, int& gz) {
    if (!s_record) return false;
    gx = s_recGx; gy = s_recGy; gz = s_recGz;
    return true;
}

void Music::playRecord(const char* name, float x, float y, float z, int gx, int gy, int gz) {
    stopChannel();          // lo que sonaba (musica de fondo u otro disco) se corta
    closeFile();
    s_eof = false;
    s_waiting = false;      // la espera de la musica de fondo queda parada mientras suene el disco
    s_record = false;
    if (!Options::get().sound) return;
    FILE* f = openTrack(name);
    if (!f) return;         // sin 13.mus / cat.mus: el disco no suena
    s_file = f;
    s_record = true;
    s_recX = x; s_recY = y; s_recZ = z;
    s_recGx = gx; s_recGy = gy; s_recGz = gz;
    if (!startChannel()) finishTrack();
}

void Music::stopRecordAt(int gx, int gy, int gz) {
    if (s_record && gx == s_recGx && gy == s_recGy && gz == s_recGz) finishTrack();
}

void Music::update(bool inGame) {
    tickClock();
    const bool sound = inGame && Options::get().sound;
    const bool want = sound && (Options::get().music || s_record);   // un disco suena aunque la musica de fondo este apagada (no si el sonido lo esta)
    if (!want) {
        if (s_channel >= 0 || s_file) { stopChannel(); closeFile(); s_eof = false; }
        s_record = false;
        s_waiting = false;
        s_noFiles = false;
        return;
    }
    if (s_noFiles && !s_record) return;

    if (s_channel < 0) {   // between tracks
        if (!s_waiting || (int)Options::get().musicWait != s_waitLevel) scheduleNext();   // first time, or the wait was changed in the menu: draw it again
        else if (s_now >= s_nextStart) startRandomTrack();
        return;
    }

    // Volumen del menu o, con un disco, la distancia a la jukebox (se manda solo si cambio, el paneo cada tanto)
    const int vol = channelVolume();
    if (vol != s_hwVolume) { s_hwVolume = vol; soundSetVolume(s_channel, (u8)vol); }
    if (s_record) {
        const int pan = channelPan();
        if (pan - s_pan > 2 || s_pan - pan > 2) { s_pan = pan; soundSetPan(s_channel, (u8)pan); }
    }

    // Playing: estimate how many samples the ARM7 has consumed from the elapsed bus ticks
    const u32 played = (u32)((s_now - s_startTicks) / TICKS_PER_SAMPLE);
    const u32 ahead = s_written - played;   // samples decoded but not yet played (wraps negative on underrun)

    if (s_eof) {   // everything is decoded: wait for the ring to drain, then schedule the next track
        if ((int)ahead <= 0) finishTrack();
        return;
    }
    if ((int)ahead <= 0 || ahead > RING_SAMPLES) {
        // Underrun (a long frame, the world loading...): the ring was replayed. Refill it and restart the channel from the current file position.
        if (!startChannel()) finishTrack();
        return;
    }
    // At most one SD sector per frame, only when there is room (plus the safety margin)
    if (RING_SAMPLES - ahead >= (u32)BLOCK_SAMPLES + MARGIN) decodeNext();
}
