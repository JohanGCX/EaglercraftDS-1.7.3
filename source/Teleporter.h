#ifndef TELEPORTER_H
#define TELEPORTER_H

#include "BlockType.h"
#include "Liquid.h"
#include "Lakes.h"   // JRandom

// =====================================================================
// Teleporter de Beta 1.7.3: al llegar por un portal busca el más cercano y, si no hay, construye uno. Port literal; trabaja con las
// celdas globales del mundo (cada celda ocupa [n, n+1) en cada eje) y con la posición del jugador en ese mismo sistema.
// S es un Liquid::Sim<W>: de él salen la lectura de bloques (sim.w) y los cambios con aviso a los vecinos.
// 'Bounds' limita la búsqueda a las columnas cargadas (las demás no tienen portal ni aire, así que el resultado es el mismo).
// =====================================================================
namespace Teleporter {

struct Bounds { int x0, x1, z0, z1; };   // rango inclusivo de columnas cargadas
struct Spot { double x, y, z; };         // dónde queda el jugador (pies), en celdas globales

// Teleporter.func_4106_b: el portal más cercano en un radio de 128
template <class S>
bool findPortal(S& sim, const Bounds& b, double posX, double posY, double posZ, Spot& out) {
    const int radius = 128;
    double best = -1.0;
    int bx = 0, by = 0, bz = 0;
    int px = (int)floor(posX), pz = (int)floor(posZ);

    int xa = px - radius, xb = px + radius, za = pz - radius, zb = pz + radius;
    if (xa < b.x0) xa = b.x0;
    if (xb > b.x1) xb = b.x1;
    if (za < b.z0) za = b.z0;
    if (zb > b.z1) zb = b.z1;

    for (int x = xa; x <= xb; ++x) {
        double dx = (double)x + 0.5 - posX;
        for (int z = za; z <= zb; ++z) {
            double dz = (double)z + 0.5 - posZ;
            for (int y = 127; y >= 0; --y) {
                if (sim.w.getType(x, y, z) == BlockType::PORTAL) {
                    while (sim.w.getType(x, y - 1, z) == BlockType::PORTAL) --y;
                    double dy = (double)y + 0.5 - posY;
                    double d = dx * dx + dy * dy + dz * dz;
                    if (best < 0.0 || d < best) { best = d; bx = x; by = y; bz = z; }
                }
            }
        }
    }

    if (best < 0.0) return false;
    double x = (double)bx + 0.5, y = (double)by + 0.5, z = (double)bz + 0.5;
    if (sim.w.getType(bx - 1, by, bz) == BlockType::PORTAL) x -= 0.5;
    if (sim.w.getType(bx + 1, by, bz) == BlockType::PORTAL) x += 0.5;
    if (sim.w.getType(bx, by, bz - 1) == BlockType::PORTAL) z -= 0.5;
    if (sim.w.getType(bx, by, bz + 1) == BlockType::PORTAL) z += 0.5;
    out.x = x; out.y = y; out.z = z;
    return true;
}

// Teleporter.func_4108_c: construye un portal en el mejor sitio libre en 16 bloques a la redonda (o uno forzado entre y=70 y 118)
template <class S>
bool createPortal(S& sim, Lakes::JRandom& rng, const Bounds& b, double posX, double posY, double posZ) {
    const int radius = 16;
    double best = -1.0;
    int px = (int)floor(posX), py = (int)floor(posY), pz = (int)floor(posZ);
    int cx = px, cy = py, cz = pz;   // mejor sitio
    int orient = 0;
    int firstOrient = rng.nextInt(4);

    int xa = px - radius, xb = px + radius, za = pz - radius, zb = pz + radius;
    if (xa < b.x0) xa = b.x0;
    if (xb > b.x1) xb = b.x1;
    if (za < b.z0) za = b.z0;
    if (zb > b.z1) zb = b.z1;

    auto isAir = [&](int x, int y, int z) { return sim.w.getType(x, y, z) == BlockType::AIR; };
    auto solidAt = [&](int x, int y, int z) { return isMaterialSolid(sim.w.getType(x, y, z)); };

    // primera pasada: un portal de 4 de ancho por 3 de profundidad (hace falta espacio para plataforma y portal)
    for (int x = xa; x <= xb; ++x) {
        double dx = (double)x + 0.5 - posX;
        for (int z = za; z <= zb; ++z) {
            double dz = (double)z + 0.5 - posZ;
            for (int y = 127; y >= 0; --y) {
                if (!isAir(x, y, z)) continue;
                while (y > 0 && isAir(x, y - 1, z)) --y;

                for (int o = firstOrient; o < firstOrient + 4; ++o) {
                    int ax = o % 2, az = 1 - ax;
                    if (o % 4 >= 2) { ax = -ax; az = -az; }

                    bool fits = true;
                    for (int a = 0; a < 3 && fits; ++a) {
                        for (int c = 0; c < 4 && fits; ++c) {
                            for (int d = -1; d < 4; ++d) {
                                int qx = x + (c - 1) * ax + a * az;
                                int qy = y + d;
                                int qz = z + (c - 1) * az - a * ax;
                                if ((d < 0 && !solidAt(qx, qy, qz)) || (d >= 0 && !isAir(qx, qy, qz))) { fits = false; break; }
                            }
                        }
                    }
                    if (!fits) break;   // continue label293: se abandona este (x, y, z) entero

                    double dy = (double)y + 0.5 - posY;
                    double dist = dx * dx + dy * dy + dz * dz;
                    if (best < 0.0 || dist < best) { best = dist; cx = x; cy = y; cz = z; orient = o % 4; }
                }
            }
        }
    }

    // segunda pasada: un portal plano de 4 de ancho (dos orientaciones)
    if (best < 0.0) {
        for (int x = xa; x <= xb; ++x) {
            double dx = (double)x + 0.5 - posX;
            for (int z = za; z <= zb; ++z) {
                double dz = (double)z + 0.5 - posZ;
                for (int y = 127; y >= 0; --y) {
                    if (!isAir(x, y, z)) continue;
                    while (isAir(x, y - 1, z)) --y;

                    for (int o = firstOrient; o < firstOrient + 2; ++o) {
                        int ax = o % 2, az = 1 - ax;

                        bool fits = true;
                        for (int c = 0; c < 4 && fits; ++c) {
                            for (int d = -1; d < 4; ++d) {
                                int qx = x + (c - 1) * ax;
                                int qy = y + d;
                                int qz = z + (c - 1) * az;
                                if ((d < 0 && !solidAt(qx, qy, qz)) || (d >= 0 && !isAir(qx, qy, qz))) { fits = false; break; }
                            }
                        }
                        if (!fits) break;

                        double dy = (double)y + 0.5 - posY;
                        double dist = dx * dx + dy * dy + dz * dz;
                        if (best < 0.0 || dist < best) { best = dist; cx = x; cy = y; cz = z; orient = o % 2; }
                    }
                }
            }
        }
    }

    int fx = cx, fy = cy, fz = cz;
    int ox = orient % 2, oz = 1 - ox;
    if (orient % 4 >= 2) { ox = -ox; oz = -oz; }

    if (best < 0.0) {   // no hay sitio: se fuerza uno entre y=70 y y=118, con una plataforma de obsidiana
        if (cy < 70) cy = 70;
        if (cy > 118) cy = 118;
        fy = cy;
        for (int a = -1; a <= 1; ++a) {
            for (int c = 1; c < 3; ++c) {
                for (int d = -1; d < 3; ++d) {
                    int qx = fx + (c - 1) * ox + a * oz;
                    int qy = fy + d;
                    int qz = fz + (c - 1) * oz - a * ox;
                    sim.setBlockWithNotify(qx, qy, qz, d < 0 ? BlockType::OBSIDIAN : BlockType::AIR);
                }
            }
        }
    }

    for (int pass = 0; pass < 4; ++pass) {
        sim.editing = true;
        for (int c = 0; c < 4; ++c) {
            for (int d = -1; d < 4; ++d) {
                int qx = fx + (c - 1) * ox;
                int qy = fy + d;
                int qz = fz + (c - 1) * oz;
                bool frame = c == 0 || c == 3 || d == -1 || d == 3;
                sim.setBlockWithNotify(qx, qy, qz, frame ? BlockType::OBSIDIAN : BlockType::PORTAL);
            }
        }
        sim.editing = false;

        for (int c = 0; c < 4; ++c) {
            for (int d = -1; d < 4; ++d) {
                sim.notifyNeighbors(fx + (c - 1) * ox, fy + d, fz + (c - 1) * oz);
            }
        }
    }
    return true;
}

// Teleporter.func_4107_a: busca; si no hay portal, lo construye y vuelve a buscar
template <class S>
bool placeInPortal(S& sim, Lakes::JRandom& rng, const Bounds& b, double posX, double posY, double posZ, Spot& out) {
    if (findPortal(sim, b, posX, posY, posZ, out)) return true;
    createPortal(sim, rng, b, posX, posY, posZ);
    return findPortal(sim, b, posX, posY, posZ, out);
}

}  // namespace Teleporter

#endif
