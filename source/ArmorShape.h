#ifndef ARMORSHAPE_H
#define ARMORSHAPE_H

// Forma de la armadura (las zonas opacas de armor_*_1.png y armor_*_2.png, iguales en todos los materiales), generada de las texturas originales:
// rectangulos de texeles por cara de cada caja. Asi la armadura se dibuja en color plano sin cargar ninguna textura.
// Cada fila: cara (0 a 5), origen y tamano de la cara en la textura (fu, fv, fw, fh) y el rectangulo opaco (u, v, w, h).
struct ArmorRect { unsigned char face, fu, fv, fw, fh, u, v, w, h; };

static const int ARMOR_1_HEAD_N = 11;
static const ArmorRect ARMOR_1_HEAD[] = {{0,16,8,8,8,16,8,8,4}, {0,16,8,8,8,20,12,4,1}, {1,0,8,8,8,0,8,8,4}, {1,0,8,8,8,0,12,4,1}, {2,8,0,8,8,8,0,8,8}, {4,8,8,8,8,8,8,8,3}, {4,8,8,8,8,8,11,1,1}, {4,8,8,8,8,15,11,1,1}, {4,8,8,8,8,11,11,2,2}, {5,24,8,8,8,24,8,8,6}, {5,24,8,8,8,26,14,4,1}};
static const int ARMOR_1_BODY_N = 13;
static const ArmorRect ARMOR_1_BODY[] = {{0,28,20,4,12,28,20,4,9}, {1,16,20,4,12,16,20,4,9}, {4,20,20,8,12,20,20,2,1}, {4,20,20,8,12,26,20,2,1}, {4,20,20,8,12,20,21,3,1}, {4,20,20,8,12,25,21,3,1}, {4,20,20,8,12,20,22,8,7}, {4,20,20,8,12,21,29,6,1}, {4,20,20,8,12,22,30,4,1}, {5,32,20,8,12,32,20,2,1}, {5,32,20,8,12,38,20,2,1}, {5,32,20,8,12,32,21,8,8}, {5,32,20,8,12,33,29,6,1}};
static const int ARMOR_1_ARM_N = 5;
static const ArmorRect ARMOR_1_ARM[] = {{0,48,20,4,12,48,20,4,5}, {1,40,20,4,12,40,20,4,5}, {2,44,16,4,4,44,16,4,4}, {4,44,20,4,12,44,20,4,5}, {5,52,20,4,12,52,20,4,5}};
static const int ARMOR_1_LEG_N = 5;
static const ArmorRect ARMOR_1_LEG[] = {{0,8,20,4,12,8,26,4,6}, {1,0,20,4,12,0,26,4,6}, {3,8,16,4,4,8,16,4,4}, {4,4,20,4,12,4,26,4,6}, {5,12,20,4,12,12,26,4,6}};
static const int ARMOR_2_HEAD_N = 0;
static const ArmorRect* const ARMOR_2_HEAD = 0;
static const int ARMOR_2_BODY_N = 4;
static const ArmorRect ARMOR_2_BODY[] = {{0,28,20,4,12,28,27,4,5}, {1,16,20,4,12,16,27,4,5}, {4,20,20,8,12,20,27,8,5}, {5,32,20,8,12,32,27,8,5}};
static const int ARMOR_2_ARM_N = 0;
static const ArmorRect* const ARMOR_2_ARM = 0;
static const int ARMOR_2_LEG_N = 5;
static const ArmorRect ARMOR_2_LEG[] = {{0,8,20,4,12,8,20,4,9}, {1,0,20,4,12,0,20,4,9}, {2,4,16,4,4,4,16,4,4}, {4,4,20,4,12,4,20,4,9}, {5,12,20,4,12,12,20,4,9}};

#endif
