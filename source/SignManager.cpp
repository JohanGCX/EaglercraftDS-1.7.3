#include "SignManager.h"
#include "ModelBox.h"
#include "Texture.h"
#include "TextKeyboard.h"
#include "MenuFont.h"
#include "BlockType.h"
#include "Chunk.h"
#include "ChunkManager.h"
#include "WorldConfig.h"
#include "HandAnimations.h"
#include <nds.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

namespace {
    struct Sign {
        bool active;
        int gx, gy, gz;
        char text[SignManager::LINES][SignManager::LINE_LEN + 1];
    };
    Sign s_signs[SignManager::MAX_SIGNS];
    int s_pending = -1;

    // ---- modelo (SignModel): tablero y poste, en v16 (unidad de modelo = 1/16 de bloque = 256) ----
    struct Quad { s16 px[4], py[4], pz[4]; s16 tu[4], tv[4]; };
    struct Box { Quad q[6]; };
    Box s_board, s_stick;
    bool s_ready = false;

    void buildBox(Box& out, int tx, int ty, float x, float y, float z, int w, int h, int d) {
        ModelBox::Face faces[6];
        ModelBox::build(faces, tx, ty, x, y, z, w, h, d, 0.0f, 0, 0.125f);
        for (int f = 0; f < 6; f++)
            for (int k = 0; k < 4; k++) {
                out.q[f].px[k] = (s16)lroundf(faces[f].v[k].x * 256.0f);
                out.q[f].py[k] = (s16)lroundf(faces[f].v[k].y * 256.0f);
                out.q[f].pz[k] = (s16)lroundf(faces[f].v[k].z * 256.0f);
                out.q[f].tu[k] = (s16)lroundf(faces[f].v[k].u * 16.0f);
                out.q[f].tv[k] = (s16)lroundf(faces[f].v[k].v * 16.0f);
            }
    }

    void init() {
        buildBox(s_board, 0, 0, -12.0f, -14.0f, -1.0f, 24, 12, 2);
        buildBox(s_stick, 0, 14, -1.0f, -2.0f, -1.0f, 2, 14, 2);
        s_ready = true;
    }

    void drawBox(const Box& b) {
        glBegin(GL_QUADS);
        for (int f = 0; f < 6; f++)
            for (int k = 0; k < 4; k++) {
                GFX_TEX_COORD = TEXTURE_PACK(b.q[f].tu[k], b.q[f].tv[k]);
                glVertex3v16(b.q[f].px[k], b.q[f].py[k], b.q[f].pz[k]);
            }
        glEnd();
    }

    float brightnessOf(unsigned char level) {
        float f = 1.0f - (float)level / 15.0f;
        return (1.0f - f) / (f * 3.0f + 1.0f) * 0.95f + 0.05f;
    }

    int find(int gx, int gy, int gz) {
        for (int i = 0; i < SignManager::MAX_SIGNS; i++)
            if (s_signs[i].active && s_signs[i].gx == gx && s_signs[i].gy == gy && s_signs[i].gz == gz) return i;
        return -1;
    }

    // Texto: celdas de 4 x 6 texeles en el atlas de 128 x 32 (indice = caracter - 32). Un pixel de la fuente mide 1,2 pixeles de Beta (1/90 de bloque)
    const float PIXEL = 1.2f / 90.0f;
    const float ADVANCE = 6.0f / 90.0f;
    const float LINE_STEP = 10.0f / 90.0f;

    void drawText(const Sign& s) {
        glBindTexture(0, Texture::signFontTextureID);
        glColor3b(0, 0, 0);
        const int fixed = 4096;   // v16: 1 bloque
        glBegin(GL_QUADS);
        for (int line = 0; line < SignManager::LINES; line++) {
            const int n = (int)strlen(s.text[line]);
            if (n == 0) continue;
            const float width = (float)n * ADVANCE - (ADVANCE - 4.0f * PIXEL);
            float x = -width * 0.5f;
            const float yTop = ((float)line * 10.0f - 20.0f) / 90.0f;   // Beta: y = linea * 10 - 4 * 5, hacia abajo
            for (int c = 0; c < n; c++, x += ADVANCE) {
                const int idx = (unsigned char)s.text[line][c] - 32;
                if (idx <= 0 || idx >= 96) continue;   // espacio o fuera de rango
                const int u0 = (idx & 31) * 4, v0 = (idx >> 5) * 6;
                const s16 x0 = (s16)lroundf(x * fixed), x1 = (s16)lroundf((x + 4.0f * PIXEL) * fixed);
                const s16 y0 = (s16)lroundf(-yTop * fixed), y1 = (s16)lroundf(-(yTop + 6.0f * PIXEL) * fixed);   // y arriba positivo
                GFX_TEX_COORD = TEXTURE_PACK(u0 * 16, v0 * 16);              glVertex3v16(x0, y0, 0);
                GFX_TEX_COORD = TEXTURE_PACK(u0 * 16, (v0 + 6) * 16);        glVertex3v16(x0, y1, 0);
                GFX_TEX_COORD = TEXTURE_PACK((u0 + 4) * 16, (v0 + 6) * 16);  glVertex3v16(x1, y1, 0);
                GFX_TEX_COORD = TEXTURE_PACK((u0 + 4) * 16, v0 * 16);        glVertex3v16(x1, y0, 0);
            }
        }
        glEnd();
    }
}

bool SignManager::isRegistered(int gx, int gy, int gz) { return find(gx, gy, gz) >= 0; }

bool SignManager::place(int gx, int gy, int gz) {
    int slot = find(gx, gy, gz);
    if (slot < 0) for (int i = 0; i < MAX_SIGNS; i++) if (!s_signs[i].active) { slot = i; break; }
    if (slot < 0) return false;   // sin sitio: el cartel queda con la textura de tablones y sin texto
    Sign& s = s_signs[slot];
    memset(&s, 0, sizeof(s));
    s.active = true; s.gx = gx; s.gy = gy; s.gz = gz;
    s_pending = slot;
    return true;
}

bool SignManager::hasPending() { return s_pending >= 0; }

bool SignManager::runPendingEdit() {
    if (s_pending < 0) return false;
    Sign& s = s_signs[s_pending];
    s_pending = -1;
    if (!s.active) return false;
    for (int i = 0; i < LINES; i++) {
        char title[24];
        snprintf(title, sizeof(title), "Sign - line %d of %d", i + 1, LINES);
        char buf[LINE_LEN + 1];
        strcpy(buf, s.text[i]);
        if (!TextKeyboard::edit(title, buf, LINE_LEN, false, false)) break;   // Cancel: se queda con lo escrito hasta ahora
        strncpy(s.text[i], buf, LINE_LEN);
        s.text[i][LINE_LEN] = 0;
    }
    return true;
}

void SignManager::draw(const ChunkManager& cm, float px, float py, float pz) {
    (void)py;
    if (!s_ready) init();
    if (Texture::signTextureID == 0) return;
    bool boundBoard = false;
    for (int i = 0; i < MAX_SIGNS; i++) {
        Sign& s = s_signs[i];
        if (!s.active) continue;
        if (i == s_pending) continue;
        const int cx = s.gx >> 3, cz = s.gz >> 3;
        Chunk* c = cm.getChunkByChunkCoord(cx, cz);
        if (!c || !c->isGenerationComplete()) continue;
        const int xi = s.gx & 7, zi = s.gz & 7;
        const BlockType t = (s.gy >= 0 && s.gy < WorldConfig::SIZE_Y) ? c->getBlockType(xi, s.gy, zi) : BlockType::AIR;
        if (!isSignBlock(t)) { s.active = false; continue; }   // lo rompieron

        const float rx = (float)(s.gx - 4) + 0.5f, ry = (float)(s.gy - 1), rz = (float)(s.gz - 4) + 0.5f;
        const float dx = rx - px, dz = rz - pz;
        if (dx * dx + dz * dz > 40.0f * 40.0f) continue;

        const int meta = c->getLiquidMeta(xi, s.gy, zi);
        const bool post = t == BlockType::SIGN_POST;
        float rotY;   // glRotatef(-giro)
        if (post) rotY = -(float)meta * 22.5f;
        else {
            float fy = 0.0f;
            if (meta == 2) fy = 180.0f; else if (meta == 4) fy = 90.0f; else if (meta == 5) fy = -90.0f;
            rotY = -fy;
        }
        const int light = HandAnimations::lightLevelAt(cm, rx, ry, rz);
        const int v = (int)(brightnessOf((unsigned char)light) * 255.0f + 0.5f);
        const float S = 2.0f / 3.0f;

        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
            glTranslatef(rx, ry, rz);
            glRotatef(rotY, 0.0f, 1.0f, 0.0f);
            if (!post) glTranslatef(0.0f, -5.0f / 16.0f, -7.0f / 16.0f);

            glPolyFmt(POLY_ALPHA(31) | POLY_CULL_NONE | POLY_FOG | POLY_ID(20));
            glColor3b(v, v, v);
            glBindTexture(0, Texture::signTextureID);
            boundBoard = true;
            glPushMatrix();
                glScalef(S, -S, -S);
                drawBox(s_board);
                if (post) drawBox(s_stick);
            glPopMatrix(1);

            // texto: solo cerca y solo desde el lado de la cara (normal = +z girado)
            const float rad = rotY * 3.14159265f / 180.0f;
            const float nx = sinf(rad), nz = cosf(rad);
            if (dx * dx + dz * dz < 12.0f * 12.0f && (px - rx) * nx + (pz - rz) * nz > 0.0f) {
                glTranslatef(0.0f, 0.5f * S, 0.07f * S + 0.008f);
                drawText(s);
            }
        glPopMatrix(1);
    }
    (void)boundBoard;
}

void SignManager::clear() {
    memset(s_signs, 0, sizeof(s_signs));
    s_pending = -1;
}

// signs.dat: 'MSGN', version 1, cantidad, y por cartel: gx, gy, gz (int32) y 4 lineas de 16 bytes
bool SignManager::saveToFile(const char* path) {
    FILE* f = fopen(path, "wb");
    if (!f) return false;
    int n = 0;
    for (int i = 0; i < MAX_SIGNS; i++) if (s_signs[i].active) n++;
    fputc('M', f); fputc('S', f); fputc('G', f); fputc('N', f);
    fputc(1, f);
    fputc(n, f);
    for (int i = 0; i < MAX_SIGNS; i++) {
        const Sign& s = s_signs[i];
        if (!s.active) continue;
        const int p[3] = { s.gx, s.gy, s.gz };
        fwrite(p, 4, 3, f);
        fwrite(s.text, LINE_LEN + 1, LINES, f);
    }
    fclose(f);
    return true;
}

bool SignManager::loadFromFile(const char* path) {
    clear();
    FILE* f = fopen(path, "rb");
    if (!f) return false;
    char magic[4];
    if (fread(magic, 1, 4, f) != 4 || magic[0] != 'M' || magic[1] != 'S' || magic[2] != 'G' || magic[3] != 'N' || fgetc(f) != 1) { fclose(f); return false; }
    const int n = fgetc(f);
    int slot = 0;
    for (int i = 0; i < n && slot < MAX_SIGNS; i++) {
        int p[3];
        Sign& s = s_signs[slot];
        if (fread(p, 4, 3, f) != 3 || fread(s.text, LINE_LEN + 1, LINES, f) != (size_t)LINES) break;
        for (int l = 0; l < LINES; l++) s.text[l][LINE_LEN] = 0;
        s.active = true; s.gx = p[0]; s.gy = p[1]; s.gz = p[2];
        slot++;
    }
    fclose(f);
    return true;
}
