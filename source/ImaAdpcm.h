#ifndef IMA_ADPCM_H
#define IMA_ADPCM_H
// IMA ADPCM decoder for mono WAV-style blocks (the format written by tools/mkmusic.py).
// Plain C++ with no libnds dependency, so it can be tested on a PC.
#include <stdint.h>

namespace ImaAdpcm {
    static const int16_t STEP[89] = {
        7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45, 50, 55, 60, 66, 73, 80, 88, 97, 107, 118, 130, 143,
        157, 173, 190, 209, 230, 253, 279, 307, 337, 371, 408, 449, 494, 544, 598, 658, 724, 796, 876, 963, 1060, 1166, 1282, 1411, 1552,
        1707, 1878, 2066, 2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358, 5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487,
        12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767 };
    static const int8_t INDEX_ADJ[16] = { -1, -1, -1, -1, 2, 4, 6, 8, -1, -1, -1, -1, 2, 4, 6, 8 };

    // Decodes one block (4-byte header + nibbles) straight into a power-of-two ring buffer.
    // out[(pos + i) & mask] receives sample i. Returns the number of samples written ((bytes-4)*2+1), 0 if the block is too short.
    inline int decodeBlockToRing(const uint8_t* in, int bytes, int16_t* ring, uint32_t mask, uint32_t pos) {
        if (bytes < 5) return 0;
        int pred = (int16_t)(in[0] | (in[1] << 8));
        int idx = in[2]; if (idx > 88) idx = 88;
        ring[pos++ & mask] = (int16_t)pred;
        int n = 1;
        for (int i = 4; i < bytes; i++) {
            const int b = in[i];
            for (int h = 0; h < 2; h++) {
                const int code = h ? (b >> 4) : (b & 15);
                const int step = STEP[idx];
                int diff = ((2 * (code & 7) + 1) * step) >> 3;   // same rounding as ffmpeg's adpcm_ima_wav (the encoder tools/mkmusic.py uses)
                if (code & 8) diff = -diff;
                pred += diff;
                if (pred > 32767) pred = 32767; else if (pred < -32768) pred = -32768;
                idx += INDEX_ADJ[code];
                if (idx < 0) idx = 0; else if (idx > 88) idx = 88;
                ring[pos++ & mask] = (int16_t)pred;
                n++;
            }
        }
        return n;
    }
}

#endif
