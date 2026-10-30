#ifndef MINECARTDATA_H
#define MINECARTDATA_H

// Una vagoneta (EntityMinecart). Todas las posiciones van en coordenadas de rejilla de Beta: la celda (gx, gy, gz) del mundo ocupa de gx a gx + 1,
// de gy a gy + 1 y de gz a gz + 1 (el mundo del dibujo es X - 4, Y - 1,5, Z - 4). posY es el centro de la caja (yOffset = 0,35).
struct Minecart {
    bool active = false;
    int uid = 0;                 // identificador unico (el cofre de la vagoneta con cofre se guarda con el)
    int type = 0;                // 0 = vagoneta, 1 = con cofre, 2 = con horno (EntityMinecart.minecartType)

    double x = 0.0, y = 0.0, z = 0.0;
    double prevX = 0.0, prevY = 0.0, prevZ = 0.0;
    double motionX = 0.0, motionY = 0.0, motionZ = 0.0;
    bool onGround = false;

    float yaw = 0.0f, prevYaw = 0.0f;
    float pitch = 0.0f, prevPitch = 0.0f;
    bool flipped = false;        // field_856_i: la vagoneta va de espaldas

    int damage = 0;              // minecartCurrentDamage: se rompe al pasar de 40
    int timeSinceHit = 0;        // minecartTimeSinceHit: el temblor del golpe
    int rockDir = 1;             // minecartRockDirection

    int fuel = 0;                // vagoneta con horno: ticks de combustible
    double pushX = 0.0, pushZ = 0.0;

    bool ridden = false;         // el jugador va dentro
};

#endif
