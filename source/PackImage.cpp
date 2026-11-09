#include "PackImage.h"
#include <stdlib.h>
#include <string.h>

// ======================================================================================================================
// Inflate (RFC 1951). Decodificador canonico simple (como puff de zlib) con ventana circular de 32 KB: la entrada se pide byte a byte y la
// salida se entrega en pedazos, asi nunca hace falta tener el archivo descomprimido entero en memoria.
// ======================================================================================================================
namespace {
    const uint32_t WSIZE = 32768, WMASK = WSIZE - 1;

    struct Huff { short count[16]; short symbol[288]; };

    struct Inf {
        int (*rd)(void*); void* rctx;
        bool (*wr)(void*, const uint8_t*, int); void* wctx;
        uint32_t bitbuf; int bitcnt;
        uint32_t pos, flushed;
        bool err, stopped;
        Huff lit, dist, clen;
        uint8_t win[WSIZE];
    };

    int bits(Inf& s, int n) {
        uint32_t v = s.bitbuf;
        while (s.bitcnt < n) {
            const int b = s.rd(s.rctx);
            if (b < 0) { s.err = true; return 0; }
            v |= (uint32_t)b << s.bitcnt;
            s.bitcnt += 8;
        }
        s.bitbuf = v >> n;
        s.bitcnt -= n;
        return (int)(v & ((1u << n) - 1));
    }

    bool flush(Inf& s) {
        while (s.flushed != s.pos) {
            const uint32_t start = s.flushed & WMASK;
            uint32_t n = s.pos - s.flushed;
            if (start + n > WSIZE) n = WSIZE - start;
            if (!s.wr(s.wctx, s.win + start, (int)n)) { s.stopped = true; return false; }
            s.flushed += n;
        }
        return true;
    }

    inline bool put(Inf& s, uint8_t b) {
        s.win[s.pos & WMASK] = b;
        s.pos++;
        if (s.pos - s.flushed >= 16384) return flush(s);   // nunca queda mas de media ventana sin entregar: las copias hacia atras siguen validas
        return true;
    }

    void build(Huff& h, const short* len, int n) {
        for (int i = 0; i < 16; i++) h.count[i] = 0;
        for (int i = 0; i < n; i++) h.count[len[i]]++;
        h.count[0] = 0;
        short offs[16];
        offs[1] = 0;
        for (int i = 1; i < 15; i++) offs[i + 1] = offs[i] + h.count[i];
        for (int i = 0; i < n; i++) if (len[i]) h.symbol[offs[len[i]]++] = (short)i;
    }

    int decode(Inf& s, const Huff& h) {
        int code = 0, first = 0, index = 0;
        for (int len = 1; len <= 15; len++) {
            code |= bits(s, 1);
            if (s.err) return -1;
            const int count = h.count[len];
            if (code - count < first) return h.symbol[index + (code - first)];
            index += count;
            first += count;
            first <<= 1;
            code <<= 1;
        }
        return -1;
    }

    const short LBASE[29] = { 3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258 };
    const short LEXT[29]  = { 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0 };
    const short DBASE[30] = { 1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145,
                              8193, 12289, 16385, 24577 };
    const short DEXT[30]  = { 0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13 };

    bool codes(Inf& s) {
        for (;;) {
            int sym = decode(s, s.lit);
            if (sym < 0) return false;
            if (sym < 256) { if (!put(s, (uint8_t)sym)) return false; }
            else if (sym == 256) return true;
            else {
                sym -= 257;
                if (sym >= 29) return false;
                int len = LBASE[sym] + bits(s, LEXT[sym]);
                const int ds = decode(s, s.dist);
                if (ds < 0 || ds >= 30) return false;
                const uint32_t d = (uint32_t)(DBASE[ds] + bits(s, DEXT[ds]));
                if (s.err || d > s.pos || d > WSIZE) return false;
                while (len--) if (!put(s, s.win[(s.pos - d) & WMASK])) return false;
            }
        }
    }

    bool stored(Inf& s) {
        s.bitbuf = 0; s.bitcnt = 0;   // al borde del byte
        int h[4];
        for (int i = 0; i < 4; i++) { h[i] = s.rd(s.rctx); if (h[i] < 0) return false; }
        const int len = h[0] | (h[1] << 8), nlen = h[2] | (h[3] << 8);
        if (len != (~nlen & 0xFFFF)) return false;
        for (int i = 0; i < len; i++) {
            const int b = s.rd(s.rctx);
            if (b < 0) return false;
            if (!put(s, (uint8_t)b)) return false;
        }
        return true;
    }

    bool fixedBlock(Inf& s) {
        short l[288];
        int i = 0;
        for (; i < 144; i++) l[i] = 8;
        for (; i < 256; i++) l[i] = 9;
        for (; i < 280; i++) l[i] = 7;
        for (; i < 288; i++) l[i] = 8;
        build(s.lit, l, 288);
        for (i = 0; i < 30; i++) l[i] = 5;
        build(s.dist, l, 30);
        return codes(s);
    }

    bool dynamicBlock(Inf& s) {
        static const uint8_t ORDER[19] = { 16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15 };
        const int nlen = bits(s, 5) + 257, ndist = bits(s, 5) + 1, ncode = bits(s, 4) + 4;
        if (s.err || nlen > 286 || ndist > 30) return false;
        short l[320];
        int i;
        for (i = 0; i < ncode; i++) l[ORDER[i]] = (short)bits(s, 3);
        for (; i < 19; i++) l[ORDER[i]] = 0;
        if (s.err) return false;
        build(s.clen, l, 19);
        i = 0;
        while (i < nlen + ndist) {
            int sym = decode(s, s.clen);
            if (sym < 0) return false;
            if (sym < 16) { l[i++] = (short)sym; continue; }
            short v = 0;
            int rep;
            if (sym == 16) { if (i == 0) return false; v = l[i - 1]; rep = 3 + bits(s, 2); }
            else if (sym == 17) rep = 3 + bits(s, 3);
            else rep = 11 + bits(s, 7);
            if (s.err || i + rep > nlen + ndist) return false;
            while (rep--) l[i++] = v;
        }
        if (l[256] == 0) return false;
        build(s.lit, l, nlen);
        build(s.dist, l + nlen, ndist);
        return codes(s);
    }
}

bool PackImage::inflate(int (*rd)(void*), void* rctx, bool (*wr)(void*, const uint8_t*, int), void* wctx) {
    Inf* s = (Inf*)malloc(sizeof(Inf));
    if (!s) return false;
    s->rd = rd; s->rctx = rctx; s->wr = wr; s->wctx = wctx;
    s->bitbuf = 0; s->bitcnt = 0; s->pos = 0; s->flushed = 0; s->err = false; s->stopped = false;
    bool ok = true;
    int last;
    do {
        last = bits(*s, 1);
        const int type = bits(*s, 2);
        if (s->err) { ok = false; break; }
        if (type == 0) ok = stored(*s);
        else if (type == 1) ok = fixedBlock(*s);
        else if (type == 2) ok = dynamicBlock(*s);
        else ok = false;
    } while (ok && !last);
    if (ok) ok = flush(*s);
    if (!ok && s->stopped && s->pos > 0) ok = true;   // quien recibe pidio cortar (ya tiene lo que queria)
    free(s);
    return ok;
}

// ======================================================================================================================
// PNG
// ======================================================================================================================
namespace {
    uint32_t be32(const uint8_t* p) { return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3]; }

    // Los datos de todos los IDAT seguidos, como un solo flujo
    struct IdatReader { const uint8_t* p; const uint8_t* end; uint32_t left; };
    int idatRead(void* c) {
        IdatReader* r = (IdatReader*)c;
        while (r->left == 0) {
            r->p += 4;   // CRC del bloque anterior
            if (r->p + 8 > r->end) return -1;
            const uint32_t len = be32(r->p);
            if (memcmp(r->p + 4, "IDAT", 4) != 0) return -1;
            r->p += 8;
            if (len > (uint32_t)(r->end - r->p)) return -1;
            r->left = len;
        }
        r->left--;
        return *r->p++;
    }

    struct Rows {
        int w, depth, ctype, bpp, rowBytes, scale;
        uint8_t* cur; uint8_t* prev;   // rowBytes + 1 (byte de filtro + datos)
        int fill, row, rowsTotal;
        uint8_t plte[256 * 3]; uint8_t trns[256];
        bool keyOn; int keyR, keyG, keyB;
        uint32_t* sums;   // outW * 4: R*A, G*A, B*A, A
        int outW;
        uint16_t* out;
    };

    void unfilter(Rows& r) {
        uint8_t* x = r.cur + 1;
        const uint8_t* p = r.prev + 1;
        const int n = r.rowBytes, b = r.bpp;
        switch (r.cur[0]) {
            case 1: for (int i = b; i < n; i++) x[i] = (uint8_t)(x[i] + x[i - b]); break;
            case 2: for (int i = 0; i < n; i++) x[i] = (uint8_t)(x[i] + p[i]); break;
            case 3: for (int i = 0; i < n; i++) x[i] = (uint8_t)(x[i] + (((i >= b ? x[i - b] : 0) + p[i]) >> 1)); break;
            case 4:
                for (int i = 0; i < n; i++) {
                    const int a = i >= b ? x[i - b] : 0, c = i >= b ? p[i - b] : 0, up = p[i];
                    const int pp = a + up - c;
                    const int pa = abs(pp - a), pb = abs(pp - up), pc = abs(pp - c);
                    x[i] = (uint8_t)(x[i] + ((pa <= pb && pa <= pc) ? a : (pb <= pc ? up : c)));
                }
                break;
            default: break;
        }
    }

    void emitRow(Rows& r) {
        const uint8_t* x = r.cur + 1;
        const int s = r.scale;
        for (int i = 0; i < r.w; i++) {
            int R, G, B, A = 255;
            switch (r.ctype) {
                case 6: R = x[i * 4]; G = x[i * 4 + 1]; B = x[i * 4 + 2]; A = x[i * 4 + 3]; break;
                case 2: R = x[i * 3]; G = x[i * 3 + 1]; B = x[i * 3 + 2]; if (r.keyOn && R == r.keyR && G == r.keyG && B == r.keyB) A = 0; break;
                case 4: R = G = B = x[i * 2]; A = x[i * 2 + 1]; break;
                case 0: R = G = B = x[i]; if (r.keyOn && R == r.keyR) A = 0; break;
                default: {   // 3: paleta
                    int idx;
                    if (r.depth == 8) idx = x[i];
                    else {
                        const int bit = i * r.depth;
                        idx = (x[bit >> 3] >> (8 - r.depth - (bit & 7))) & ((1 << r.depth) - 1);
                    }
                    R = r.plte[idx * 3]; G = r.plte[idx * 3 + 1]; B = r.plte[idx * 3 + 2]; A = r.trns[idx];
                    break;
                }
            }
            uint32_t* sm = r.sums + (i / s) * 4;
            sm[0] += (uint32_t)(R * A); sm[1] += (uint32_t)(G * A); sm[2] += (uint32_t)(B * A); sm[3] += (uint32_t)A;
        }
        if (r.row % s == s - 1) {
            const int oy = r.row / s, area = s * s;
            for (int ox = 0; ox < r.outW; ox++) {
                uint32_t* sm = r.sums + ox * 4;
                uint16_t v = 0;
                if (sm[3] / (uint32_t)area >= 128) {
                    const int R = (int)(sm[0] / sm[3]), G = (int)(sm[1] / sm[3]), B = (int)(sm[2] / sm[3]);
                    v = (uint16_t)(0x8000 | ((B >> 3) << 10) | ((G >> 3) << 5) | (R >> 3));
                }
                r.out[oy * r.outW + ox] = v;
                sm[0] = sm[1] = sm[2] = sm[3] = 0;
            }
        }
    }

    bool rowsWrite(void* c, const uint8_t* data, int n) {
        Rows& r = *(Rows*)c;
        while (n > 0) {
            if (r.row >= r.rowsTotal) return true;   // lo que sobre despues de la ultima fila se ignora
            int take = r.rowBytes + 1 - r.fill;
            if (take > n) take = n;
            memcpy(r.cur + r.fill, data, (size_t)take);
            r.fill += take; data += take; n -= take;
            if (r.fill == r.rowBytes + 1) {
                if (r.cur[0] > 4) return false;
                unfilter(r);
                emitRow(r);
                uint8_t* t = r.prev; r.prev = r.cur; r.cur = t;
                r.fill = 0;
                r.row++;
            }
        }
        return true;
    }
}

bool PackImage::decodePng(const uint8_t* png, uint32_t len, uint16_t* out, int outW, int outH) {
    static const uint8_t SIG[8] = { 0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A };
    if (len < 8 || memcmp(png, SIG, 8) != 0) return false;
    const uint8_t* end = png + len;
    const uint8_t* p = png + 8;
    Rows r;
    memset(&r, 0, sizeof(r));
    memset(r.trns, 255, sizeof(r.trns));
    int h = 0;
    bool haveHeader = false;
    IdatReader rd = { nullptr, end, 0 };
    for (;;) {
        if (p + 12 > end) return false;
        const uint32_t clen = be32(p);
        const uint8_t* type = p + 4;
        const uint8_t* data = p + 8;
        if (clen > (uint32_t)(end - data)) return false;
        if (!memcmp(type, "IHDR", 4) && clen >= 13) {
            r.w = (int)be32(data); h = (int)be32(data + 4);
            r.depth = data[8]; r.ctype = data[9];
            if (data[12] != 0) return false;   // entrelazado: no
            haveHeader = true;
        } else if (!memcmp(type, "PLTE", 4)) {
            memcpy(r.plte, data, clen > 768 ? 768 : clen);
        } else if (!memcmp(type, "tRNS", 4)) {
            if (r.ctype == 3) memcpy(r.trns, data, clen > 256 ? 256 : clen);
            else if (r.ctype == 0 && clen >= 2) { r.keyOn = true; r.keyR = data[1]; }
            else if (r.ctype == 2 && clen >= 6) { r.keyOn = true; r.keyR = data[1]; r.keyG = data[3]; r.keyB = data[5]; }
        } else if (!memcmp(type, "IDAT", 4)) {
            rd.p = data; rd.left = clen;
            break;
        } else if (!memcmp(type, "IEND", 4)) return false;
        p = data + clen + 4;
    }
    if (!haveHeader || outW <= 0 || outH <= 0) return false;
    r.scale = r.w / outW;   // 16x = 1; 32x = 2; 64x = 4; 128x = 8 (se promedia)
    if (r.scale < 1 || r.scale > 8 || (r.scale & (r.scale - 1)) || r.w != outW * r.scale || h != outH * r.scale) return false;
    r.outW = outW;
    int channels;
    switch (r.ctype) {
        case 0: channels = 1; if (r.depth != 8) return false; break;
        case 2: channels = 3; if (r.depth != 8) return false; break;
        case 3: channels = 1; if (r.depth != 1 && r.depth != 2 && r.depth != 4 && r.depth != 8) return false; break;
        case 4: channels = 2; if (r.depth != 8) return false; break;
        case 6: channels = 4; if (r.depth != 8) return false; break;
        default: return false;
    }
    r.bpp = (channels * r.depth + 7) / 8;
    r.rowBytes = (r.w * channels * r.depth + 7) / 8;
    r.rowsTotal = h;
    r.out = out;
    uint8_t* mem = (uint8_t*)malloc((size_t)(r.rowBytes + 1) * 2);
    if (!mem) return false;
    r.cur = mem;
    r.prev = mem + r.rowBytes + 1;
    r.sums = (uint32_t*)calloc((size_t)outW * 4, sizeof(uint32_t));
    if (!r.sums) { free(mem); return false; }
    memset(r.prev, 0, (size_t)r.rowBytes + 1);

    // Cabecera zlib (2 bytes): deflate, sin diccionario
    const int cmf = idatRead(&rd), flg = idatRead(&rd);
    bool ok = cmf >= 0 && flg >= 0 && (cmf & 15) == 8 && !(flg & 0x20) && ((cmf << 8) | flg) % 31 == 0;
    if (ok) ok = PackImage::inflate(idatRead, &rd, rowsWrite, &r);
    ok = ok && r.row >= h;
    free(r.sums);
    free(mem);
    return ok;
}

// ======================================================================================================================
// Paleta: median cut sobre los colores de 15 bits (la DS no tiene mas que eso)
// ======================================================================================================================
namespace {
    struct Col { uint16_t c; uint16_t n; };
    int s_axis = 0;
    inline int chan(uint16_t c, int a) { return (c >> (a * 5)) & 31; }
    int cmpAxis(const void* a, const void* b) { return chan(((const Col*)a)->c, s_axis) - chan(((const Col*)b)->c, s_axis); }
    struct Box { int start, end; };
}

bool PackImage::quantize(const uint16_t* px, int count, uint8_t* outIdx, uint16_t* outPal, int palEntries) {
    const int maxColors = palEntries - 1;   // el 0 es lo transparente
    if (maxColors < 1) return false;
    uint16_t* hist = (uint16_t*)calloc(32768, sizeof(uint16_t));
    if (!hist) return false;
    int uniq = 0;
    for (int i = 0; i < count; i++) {
        if (!(px[i] & 0x8000)) continue;
        uint16_t& h = hist[px[i] & 0x7FFF];
        if (h == 0) uniq++;
        if (h != 0xFFFF) h++;
    }
    Col* cols = (Col*)malloc(sizeof(Col) * (uniq > 0 ? uniq : 1));
    Box* boxes = (Box*)malloc(sizeof(Box) * (size_t)maxColors);
    if (!cols || !boxes) { free(hist); free(cols); free(boxes); return false; }
    int k = 0;
    for (int c = 0; c < 32768; c++) if (hist[c]) { cols[k].c = (uint16_t)c; cols[k].n = hist[c]; k++; }

    int nb = 0;
    if (uniq > 0) { boxes[0].start = 0; boxes[0].end = uniq; nb = 1; }
    while (nb < maxColors) {
        // la caja con mas rango en algun canal (a igual rango, la de mas pixeles)
        int best = -1, bestSpan = 0, bestAxis = 0; uint32_t bestN = 0;
        for (int b = 0; b < nb; b++) {
            if (boxes[b].end - boxes[b].start < 2) continue;
            int lo[3] = { 31, 31, 31 }, hi[3] = { 0, 0, 0 };
            uint32_t n = 0;
            for (int i = boxes[b].start; i < boxes[b].end; i++) {
                for (int a = 0; a < 3; a++) { const int v = chan(cols[i].c, a); if (v < lo[a]) lo[a] = v; if (v > hi[a]) hi[a] = v; }
                n += cols[i].n;
            }
            for (int a = 0; a < 3; a++) {
                const int span = hi[a] - lo[a];
                if (span > bestSpan || (span == bestSpan && span > 0 && n > bestN)) { best = b; bestSpan = span; bestAxis = a; bestN = n; }
            }
        }
        if (best < 0) break;
        Box& bx = boxes[best];
        s_axis = bestAxis;
        qsort(cols + bx.start, (size_t)(bx.end - bx.start), sizeof(Col), cmpAxis);
        uint32_t total = 0, acc = 0;
        for (int i = bx.start; i < bx.end; i++) total += cols[i].n;
        int cut = bx.start + 1;
        for (int i = bx.start; i < bx.end - 1; i++) { acc += cols[i].n; cut = i + 1; if (acc * 2 >= total) break; }
        boxes[nb].start = cut; boxes[nb].end = bx.end;
        bx.end = cut;
        nb++;
    }

    outPal[0] = 0x7C1F;   // magenta: el color 0 de los atlas (transparente)
    for (int i = 1; i < palEntries; i++) outPal[i] = 0;
    for (int b = 0; b < nb; b++) {
        uint32_t sr = 0, sg = 0, sb = 0, n = 0;
        for (int i = boxes[b].start; i < boxes[b].end; i++) {
            const uint32_t w = cols[i].n;
            sr += chan(cols[i].c, 0) * w; sg += chan(cols[i].c, 1) * w; sb += chan(cols[i].c, 2) * w; n += w;
            hist[cols[i].c] = (uint16_t)(b + 1);   // el histograma pasa a ser la tabla color -> indice
        }
        if (n == 0) n = 1;
        outPal[b + 1] = (uint16_t)(((sb + n / 2) / n) << 10 | ((sg + n / 2) / n) << 5 | ((sr + n / 2) / n));
    }
    for (int i = 0; i < count; i++) outIdx[i] = (px[i] & 0x8000) ? (uint8_t)hist[px[i] & 0x7FFF] : 0;
    free(boxes); free(cols); free(hist);
    return true;
}

// ======================================================================================================================
// Zip
// ======================================================================================================================

namespace {
    uint32_t le16(const uint8_t* p) { return (uint32_t)p[0] | ((uint32_t)p[1] << 8); }
    uint32_t le32(const uint8_t* p) { return le16(p) | (le16(p + 2) << 16); }

    struct FileReader { FILE* f; uint32_t left; int pos, len; uint8_t buf[512]; };
    int fileRead(void* c) {
        FileReader* r = (FileReader*)c;
        if (r->pos >= r->len) {
            if (r->left == 0) return -1;
            const uint32_t n = r->left < sizeof(r->buf) ? r->left : (uint32_t)sizeof(r->buf);
            r->len = (int)fread(r->buf, 1, n, r->f);
            if (r->len <= 0) return -1;
            r->left -= (uint32_t)r->len;
            r->pos = 0;
        }
        return r->buf[r->pos++];
    }

    struct MemWriter { uint8_t* p; uint32_t len, cap; };
    bool memWrite(void* c, const uint8_t* d, int n) {
        MemWriter* w = (MemWriter*)c;
        if (w->len + (uint32_t)n > w->cap) return false;
        memcpy(w->p + w->len, d, (size_t)n);
        w->len += (uint32_t)n;
        return true;
    }
}

bool PackImage::zipIndex(FILE* f, const char* const* names, int count, ZipItem* items) {
    for (int i = 0; i < count; i++) items[i].found = false;
    if (fseek(f, 0, SEEK_END) != 0) return false;
    const long size = ftell(f);
    if (size < 22) return false;
    // Fin del directorio central: en los ultimos 22 bytes + el comentario del zip (se busca en los ultimos 4 KB)
    const long tail = size < 4096 ? size : 4096;
    uint8_t* t = (uint8_t*)malloc((size_t)tail);
    if (!t) return false;
    fseek(f, size - tail, SEEK_SET);
    if (fread(t, 1, (size_t)tail, f) != (size_t)tail) { free(t); return false; }
    long eocd = -1;
    for (long i = tail - 22; i >= 0; i--) if (t[i] == 'P' && t[i + 1] == 'K' && t[i + 2] == 5 && t[i + 3] == 6) { eocd = i; break; }
    if (eocd < 0) { free(t); return false; }
    const uint32_t entries = le16(t + eocd + 10), cdSize = le32(t + eocd + 12), cdOff = le32(t + eocd + 16);
    free(t);
    if (cdSize > 1024 * 1024 || (long)cdOff + (long)cdSize > size) return false;
    uint8_t* cd = (uint8_t*)malloc(cdSize ? cdSize : 1);
    if (!cd) return false;
    fseek(f, (long)cdOff, SEEK_SET);
    if (fread(cd, 1, cdSize, f) != cdSize) { free(cd); return false; }
    uint32_t p = 0;
    for (uint32_t e = 0; e < entries && p + 46 <= cdSize; e++) {
        if (le32(cd + p) != 0x02014b50u) break;
        const uint32_t nameLen = le16(cd + p + 28), extraLen = le16(cd + p + 30), commentLen = le16(cd + p + 32);
        if (p + 46 + nameLen > cdSize) break;
        for (int i = 0; i < count; i++) {
            if (nameLen == strlen(names[i]) && memcmp(cd + p + 46, names[i], nameLen) == 0) {
                ZipItem& it = items[i];
                it.flags = le16(cd + p + 8); it.method = le16(cd + p + 10);
                it.csize = le32(cd + p + 20); it.usize = le32(cd + p + 24); it.localOff = le32(cd + p + 42);
                it.found = !(it.flags & 1) && (it.method == 0 || it.method == 8);   // cifrados u otros metodos: como si no estuviera
            }
        }
        p += 46 + nameLen + extraLen + commentLen;
    }
    free(cd);
    return true;
}

uint8_t* PackImage::zipExtract(FILE* f, const ZipItem& it, uint32_t maxBytes, uint32_t& outLen) {
    outLen = 0;
    if (!it.found || it.usize > maxBytes) return nullptr;
    uint8_t lh[30];
    if (fseek(f, (long)it.localOff, SEEK_SET) != 0 || fread(lh, 1, 30, f) != 30 || le32(lh) != 0x04034b50u) return nullptr;
    if (fseek(f, (long)(it.localOff + 30 + le16(lh + 26) + le16(lh + 28)), SEEK_SET) != 0) return nullptr;
    uint8_t* out = (uint8_t*)malloc(it.usize ? it.usize : 1);
    if (!out) return nullptr;
    bool ok;
    if (it.method == 0) ok = fread(out, 1, it.usize, f) == it.usize;
    else {
        FileReader* r = (FileReader*)malloc(sizeof(FileReader));
        if (!r) { free(out); return nullptr; }
        r->f = f; r->left = it.csize; r->pos = 0; r->len = 0;
        MemWriter w = { out, 0, it.usize };
        ok = inflate(fileRead, r, memWrite, &w) && w.len == it.usize;
        free(r);
    }
    if (!ok) { free(out); return nullptr; }
    outLen = it.usize;
    return out;
}

void PackImage::colorTable(const uint16_t* px, uint8_t* t) {
    for (int j = 0; j < 33; j++)
        for (int i = 0; i < 33; i++) {
            const int x = i * 8 > 255 ? 255 : i * 8, y = j * 8 > 255 ? 255 : j * 8;
            const uint16_t c = px[y * 256 + x];
            uint8_t* o = t + (j * 33 + i) * 3;
            for (int k = 0; k < 3; k++) { const int v = (c >> (k * 5)) & 31; o[k] = (uint8_t)((v << 3) | (v >> 2)); }
        }
}

void PackImage::firstLine(const uint8_t* text, uint32_t len, char* out, int cap) {
    int n = 0;
    uint32_t i = 0;
    if (len >= 3 && text[0] == 0xEF && text[1] == 0xBB && text[2] == 0xBF) i = 3;   // BOM de UTF-8
    while (i < len && (text[i] == ' ' || text[i] == '\t' || text[i] == '\r' || text[i] == '\n')) i++;
    for (; i < len && text[i] != '\n' && text[i] != '\r' && n < cap - 1; i++)
        if (text[i] >= 32 && text[i] < 127) out[n++] = (char)text[i];
    while (n > 0 && out[n - 1] == ' ') n--;
    out[n] = 0;
}
