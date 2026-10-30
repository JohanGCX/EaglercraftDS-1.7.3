#ifndef BOATDATA_H
#define BOATDATA_H

// Un bote (EntityBoat). Las posiciones van en coordenadas de rejilla de Beta, igual que las vagonetas: la celda (gx, gy, gz) del mundo ocupa de gx a gx + 1,
// de gy a gy + 1 y de gz a gz + 1 (el mundo del dibujo es X - 4, Y - 1,5, Z - 4). y es el centro de la caja (yOffset = 0,3).
struct Boat {
    bool active = false;
    int uid = 0;

    double x = 0.0, y = 0.0, z = 0.0;
    double prevX = 0.0, prevY = 0.0, prevZ = 0.0;
    double motionX = 0.0, motionY = 0.0, motionZ = 0.0;
    bool onGround = false;

    float yaw = 0.0f, prevYaw = 0.0f;

    int damage = 0;              // boatCurrentDamage: se rompe al pasar de 40
    int timeSinceHit = 0;        // boatTimeSinceHit: el temblor del golpe
    int rockDir = 1;             // boatRockDirection

    bool ridden = false;         // el jugador va dentro
};

#endif
