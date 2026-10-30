#ifndef RAILLOGIC_H
#define RAILLOGIC_H

// Rieles de Beta 1.7.3: RailLogic (la forma de cada riel segun sus vecinos) y las tablas de la vagoneta (EntityMinecart.field_855_j).
// Es un port de RailLogic.java; el mundo entra como plantilla W (asi se prueba sin el motor), que debe dar:
//   BlockType type(int x, int y, int z) const;   int meta(int x, int y, int z) const;   void setMeta(int x, int y, int z, int m);
// (setMeta = setBlockMetadataWithNotify: cambia la metadata y avisa a los vecinos.)
// Metadata del riel: 0 = Z (norte-sur), 1 = X (este-oeste), 2 = sube hacia +X, 3 = sube hacia -X, 4 = sube hacia -Z, 5 = sube hacia +Z,
// 6 = curva +X/+Z, 7 = curva -X/+Z, 8 = curva -X/-Z, 9 = curva +X/-Z. Propulsor y detector: bits 0-2 = forma (0 a 5), bit 8 = encendido.

#include "BlockType.h"

namespace Rails {

    // BlockRail.isPowered: verdadero para el propulsor y para el detector (no tienen curvas)
    inline bool hasPower(BlockType t) { return t == BlockType::RAIL_POWERED || t == BlockType::RAIL_DETECTOR; }

    // EntityMinecart.field_855_j: para cada forma, los dos extremos del riel {dx, dy, dz}
    inline const int (*endpoints(int shape))[3] {
        static const int T[10][2][3] = {
            { {0, 0, -1}, {0, 0, 1} }, { {-1, 0, 0}, {1, 0, 0} }, { {-1, -1, 0}, {1, 0, 0} }, { {-1, 0, 0}, {1, -1, 0} },
            { {0, 0, -1}, {0, -1, 1} }, { {0, -1, -1}, {0, 0, 1} }, { {0, 0, 1}, {1, 0, 0} }, { {0, 0, 1}, {-1, 0, 0} },
            { {0, 0, -1}, {-1, 0, 0} }, { {0, 0, -1}, {1, 0, 0} }
        };
        return T[shape < 0 ? 0 : (shape > 9 ? 0 : shape)];
    }

    struct Pos { int x, y, z; };

    template <class W>
    inline bool isRailAt(W& w, int x, int y, int z) { return isRailBlock(w.type(x, y, z)); }

    template <class W>
    struct Logic {
        W& w;
        int tx, ty, tz;
        bool powered;
        int n;
        Pos c[4];

        Logic(W& world, int x, int y, int z) : w(world), tx(x), ty(y), tz(z), powered(false), n(0) {
            int m = w.meta(x, y, z);
            if (hasPower(w.type(x, y, z))) { powered = true; m &= ~8; }
            setConnections(m);
        }

        void add(int x, int y, int z) { if (n < 4) { c[n].x = x; c[n].y = y; c[n].z = z; n++; } }

        void setConnections(int m) {
            n = 0;
            switch (m) {
                case 0: add(tx, ty, tz - 1); add(tx, ty, tz + 1); break;
                case 1: add(tx - 1, ty, tz); add(tx + 1, ty, tz); break;
                case 2: add(tx - 1, ty, tz); add(tx + 1, ty + 1, tz); break;
                case 3: add(tx - 1, ty + 1, tz); add(tx + 1, ty, tz); break;
                case 4: add(tx, ty + 1, tz - 1); add(tx, ty, tz + 1); break;
                case 5: add(tx, ty, tz - 1); add(tx, ty + 1, tz + 1); break;
                case 6: add(tx + 1, ty, tz); add(tx, ty, tz + 1); break;
                case 7: add(tx - 1, ty, tz); add(tx, ty, tz + 1); break;
                case 8: add(tx - 1, ty, tz); add(tx, ty, tz - 1); break;
                case 9: add(tx + 1, ty, tz); add(tx, ty, tz - 1); break;
                default: break;
            }
        }

        // getMinecartTrackLogic: el riel de esa celda, o el de arriba, o el de abajo
        bool findTrack(const Pos& p, Pos& out) {
            if (isRailAt(w, p.x, p.y, p.z)) { out = p; return true; }
            if (isRailAt(w, p.x, p.y + 1, p.z)) { out.x = p.x; out.y = p.y + 1; out.z = p.z; return true; }
            if (isRailAt(w, p.x, p.y - 1, p.z)) { out.x = p.x; out.y = p.y - 1; out.z = p.z; return true; }
            return false;
        }

        bool isMinecartTrack(int x, int y, int z) {
            return isRailAt(w, x, y, z) || isRailAt(w, x, y + 1, z) || isRailAt(w, x, y - 1, z);
        }

        bool isConnectedTo(const Logic& o) const {
            for (int i = 0; i < n; i++) if (c[i].x == o.tx && c[i].z == o.tz) return true;
            return false;
        }

        bool isInTrack(int x, int z) const {
            for (int i = 0; i < n; i++) if (c[i].x == x && c[i].z == z) return true;
            return false;
        }

        // func_785_b: deja solo las conexiones que el otro riel tambien tiene
        void prune() {
            for (int i = 0; i < n; i++) {
                Pos p;
                bool keep = false;
                if (findTrack(c[i], p)) {
                    Logic o(w, p.x, p.y, p.z);
                    if (o.isConnectedTo(*this)) { c[i] = p; keep = true; }
                }
                if (!keep) {
                    for (int k = i; k < n - 1; k++) c[k] = c[k + 1];
                    n--; i--;
                }
            }
        }

        int adjacentTracks() {
            int k = 0;
            if (isMinecartTrack(tx, ty, tz - 1)) ++k;
            if (isMinecartTrack(tx, ty, tz + 1)) ++k;
            if (isMinecartTrack(tx - 1, ty, tz)) ++k;
            if (isMinecartTrack(tx + 1, ty, tz)) ++k;
            return k;
        }

        // handleKeyPress
        bool canConnect(const Logic& o) const {
            if (isConnectedTo(o)) return true;
            if (n == 2) return false;
            return true;   // con 0 o 1 conexiones siempre (la comparacion de alturas del original da lo mismo en las dos ramas)
        }

        // func_788_d: se une al riel o y recalcula su forma
        void connectTo(const Logic& o) {
            add(o.tx, o.ty, o.tz);
            const bool n_ = isInTrack(tx, tz - 1), s_ = isInTrack(tx, tz + 1), w_ = isInTrack(tx - 1, tz), e_ = isInTrack(tx + 1, tz);
            int m = -1;
            if (n_ || s_) m = 0;
            if (w_ || e_) m = 1;
            if (!powered) {
                if (s_ && e_ && !n_ && !w_) m = 6;
                if (s_ && w_ && !n_ && !e_) m = 7;
                if (n_ && w_ && !s_ && !e_) m = 8;
                if (n_ && e_ && !s_ && !w_) m = 9;
            }
            if (m == 0) {
                if (isRailAt(w, tx, ty + 1, tz - 1)) m = 4;
                if (isRailAt(w, tx, ty + 1, tz + 1)) m = 5;
            }
            if (m == 1) {
                if (isRailAt(w, tx + 1, ty + 1, tz)) m = 2;
                if (isRailAt(w, tx - 1, ty + 1, tz)) m = 3;
            }
            if (m < 0) m = 0;
            int out = m;
            if (powered) out = (w.meta(tx, ty, tz) & 8) | m;
            w.setMeta(tx, ty, tz, out);
        }

        // func_786_c: ¿hay riel en esa celda al que este se pueda unir?
        bool neighborOpen(int x, int y, int z) {
            Pos p, q; p.x = x; p.y = y; p.z = z;
            if (!findTrack(p, q)) return false;
            Logic o(w, q.x, q.y, q.z);
            o.prune();
            return o.canConnect(*this);
        }

        // func_792_a: pone la forma del riel segun los vecinos. hasPowerIn = si recibe corriente (solo cuenta para las curvas); force = escribir siempre
        void update(bool hasPowerIn, bool force) {
            const bool nn = neighborOpen(tx, ty, tz - 1);
            const bool ss = neighborOpen(tx, ty, tz + 1);
            const bool ww = neighborOpen(tx - 1, ty, tz);
            const bool ee = neighborOpen(tx + 1, ty, tz);
            int m = -1;
            if ((nn || ss) && !ww && !ee) m = 0;
            if ((ww || ee) && !nn && !ss) m = 1;
            if (!powered) {
                if (ss && ee && !nn && !ww) m = 6;
                if (ss && ww && !nn && !ee) m = 7;
                if (nn && ww && !ss && !ee) m = 8;
                if (nn && ee && !ss && !ww) m = 9;
            }
            if (m == -1) {
                if (nn || ss) m = 0;
                if (ww || ee) m = 1;
                if (!powered) {
                    if (hasPowerIn) {
                        if (ss && ee) m = 6;
                        if (ww && ss) m = 7;
                        if (ee && nn) m = 9;
                        if (nn && ww) m = 8;
                    } else {
                        if (nn && ww) m = 8;
                        if (ee && nn) m = 9;
                        if (ww && ss) m = 7;
                        if (ss && ee) m = 6;
                    }
                }
            }
            if (m == 0) {
                if (isRailAt(w, tx, ty + 1, tz - 1)) m = 4;
                if (isRailAt(w, tx, ty + 1, tz + 1)) m = 5;
            }
            if (m == 1) {
                if (isRailAt(w, tx + 1, ty + 1, tz)) m = 2;
                if (isRailAt(w, tx - 1, ty + 1, tz)) m = 3;
            }
            if (m < 0) m = 0;

            setConnections(m);
            int out = m;
            if (powered) out = (w.meta(tx, ty, tz) & 8) | m;
            if (force || w.meta(tx, ty, tz) != out) {
                w.setMeta(tx, ty, tz, out);
                for (int i = 0; i < n; i++) {
                    Pos q;
                    if (!findTrack(c[i], q)) continue;
                    Logic o(w, q.x, q.y, q.z);
                    o.prune();
                    if (o.canConnect(*this)) o.connectTo(*this);
                }
            }
        }
    };

    // BlockRail.onBlockAdded / onNeighborBlockChange (el riel normal con 3 vecinos): recalcula la forma
    template <class W>
    inline void reshape(W& w, int x, int y, int z, bool hasPowerIn, bool force) {
        Logic<W> l(w, x, y, z);
        l.update(hasPowerIn, force);
    }

    template <class W>
    inline int adjacentCount(W& w, int x, int y, int z) {
        Logic<W> l(w, x, y, z);
        return l.adjacentTracks();
    }

    // BlockRail.func_27043_a / func_27044_a: ¿hay un propulsor encendido, hasta 8 rieles mas alla, en esa direccion? (energia que se pasa de riel en riel)
    // poweredAt(x, y, z) = World.isBlockIndirectlyGettingPowered en esa celda y en la de arriba.
    template <class W, class P>
    inline bool chainPowered(W& w, P& poweredAt, int x, int y, int z, int meta, bool forward, int depth);

    template <class W, class P>
    inline bool chainEnd(W& w, P& poweredAt, int x, int y, int z, bool forward, int depth, int axis) {
        if (w.type(x, y, z) == BlockType::RAIL_POWERED) {
            const int m = w.meta(x, y, z);
            const int s = m & 7;
            if (axis == 1 && (s == 0 || s == 4 || s == 5)) return false;
            if (axis == 0 && (s == 1 || s == 2 || s == 3)) return false;
            if ((m & 8) != 0) {
                if (!poweredAt(x, y, z) && !poweredAt(x, y + 1, z)) return chainPowered(w, poweredAt, x, y, z, m, forward, depth + 1);
                return true;
            }
        }
        return false;
    }

    template <class W, class P>
    inline bool chainPowered(W& w, P& poweredAt, int x, int y, int z, int meta, bool forward, int depth) {
        if (depth >= 8) return false;
        int s = meta & 7;
        bool flat = true;
        switch (s) {
            case 0: if (forward) ++z; else --z; break;
            case 1: if (forward) --x; else ++x; break;
            case 2: if (forward) --x; else { ++x; ++y; flat = false; } s = 1; break;
            case 3: if (forward) { --x; ++y; flat = false; } else ++x; s = 1; break;
            case 4: if (forward) ++z; else { --z; ++y; flat = false; } s = 0; break;
            case 5: if (forward) { ++z; ++y; flat = false; } else --z; s = 0; break;
            default: break;
        }
        if (chainEnd(w, poweredAt, x, y, z, forward, depth, s)) return true;
        return flat && chainEnd(w, poweredAt, x, y - 1, z, forward, depth, s);
    }
}

#endif
