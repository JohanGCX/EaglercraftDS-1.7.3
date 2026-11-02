#ifndef MOBCULL_H
#define MOBCULL_H

#include <math.h>

// Mob performance helpers shared by SheepManager, AnimalManager and HostileManager (header only, no new globals in other headers).
//  - Draw culling: main.cpp calls MobCull::set() once per rendered frame with the eye position and the look direction; the managers skip mobs that are
//    beyond the render distance or outside the camera cone, before computing their light or touching the GPU.
//  - Far AI throttling: mobs farther than FAR_DIST from the player only run their AI every other 20 Hz tick (alternating by slot index), so the cost of
//    distant mobs is halved. On a skipped tick the interpolation state is frozen so they do not jitter.
namespace MobCull {
    struct State { float x, y, z, lx, ly, lz, maxDist2; bool valid; };
    inline State& state() { static State s = { 0, 0, 0, 0, 0, -1, 0, false }; return s; }

    // (x, y, z): the eye; (lx, ly, lz): look direction (unit vector); maxDist: render distance in blocks
    inline void set(float x, float y, float z, float lx, float ly, float lz, float maxDist) {
        State& s = state();
        s.x = x; s.y = y; s.z = z; s.lx = lx; s.ly = ly; s.lz = lz; s.maxDist2 = maxDist * maxDist; s.valid = true;
    }

    // Is a sphere of radius r centred at (x, y, z) possibly on screen? Conservative: a cone of about 60 degrees around the view axis
    // (the DS field of view is 70 vertical, ~87 horizontal, ~100 on the diagonal) plus the radius as margin.
    inline bool visible(float x, float y, float z, float r) {
        const State& s = state();
        if (!s.valid) return true;
        const float dx = x - s.x, dy = y - s.y, dz = z - s.z;
        const float d2 = dx * dx + dy * dy + dz * dz;
        if (d2 > s.maxDist2 + 2.0f * r * sqrtf(s.maxDist2) + r * r) return false;   // beyond the render distance (plus the radius)
        if (d2 <= r * r * 4.0f) return true;                                            // touching the camera
        const float dot = dx * s.lx + dy * s.ly + dz * s.lz;
        if (dot < -r) return false;                                                     // behind the camera
        const float d = sqrtf(d2);
        return dot >= d * 0.5f - r * 1.5f;                                              // inside the cone (cos 60 = 0.5)
    }

    const double FAR_DIST2 = 16.0 * 16.0;   // beyond this (horizontal) distance the AI runs every other tick

    inline unsigned& tickCounter() { static unsigned t = 0; return t; }
    inline void nextTick() { ++tickCounter(); }

    // true if this mob should skip its AI on this tick
    inline bool skipFar(double mx, double mz, double px, double pz, int slot) {
        const double dx = mx - px, dz = mz - pz;
        if (dx * dx + dz * dz <= FAR_DIST2) return false;
        return ((tickCounter() + (unsigned)slot) & 1u) != 0;
    }

    // A skipped tick: the drawing interpolates from prev to current, so prev is set to current (no jitter, no repeated step)
    template <class T>
    inline void freeze(T& a) {
        a.prevX = a.x; a.prevY = a.y; a.prevZ = a.z;
        a.prevYaw = a.yaw; a.prevPitch = a.pitch; a.prevBodyYaw = a.bodyYaw;
        a.prevLimbSwingAmount = a.limbSwingAmount;
    }
}

#endif
