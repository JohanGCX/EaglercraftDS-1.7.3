#ifndef TEXTURE_H
#define TEXTURE_H

// Los cuadros: la textura va reducida a 128 x 128 (16 KB). Si la VRAM no alcanza, activar la linea de abajo: 64 x 64 (4 KB, mas borrosa).
// #define PAINTING_TEX_64
#ifdef PAINTING_TEX_64
#define PAINTING_UV 4   // texeles x 16 por pixel de kz.png
#else
#define PAINTING_UV 8
#endif

class Texture {
public:
    static bool loadTerrainAtlas();
    static bool loadSkinAtlas();
    static bool loadItemsAtlas();
    static bool loadIconsAtlas();
    static bool loadCartAtlas();    // cart.png: la vagoneta, 64 x 32
    static bool loadBoatAtlas();    // boat.png: el bote, 64 x 32
    static void unloadEntityTextures();   // libera la VRAM de las entidades mientras el menu muestra el panorama
    static void reloadEntityTextures();   // las vuelve a cargar (no hace nada si ya estan)
    static void refreshEntityTextures();  // un texture pack cambio sus bitmaps: si estan en la VRAM, se suben otra vez (si no, ya se cargan de los nuevos)
    static bool loadAnimalAtlases(); // cow.png, pig.png y chicken.png: 64 x 32 cada una
    static bool loadHostileAtlases(); // zombie, skeleton, spider, spidereyes, creeper, pigzombie, ghast, ghastfire, slime, squid, wolf, wolfangry y wolftame: 64 x 32 cada una, con transparencia
    static bool loadPaintingAtlas();   // kz.png reducido (PaintingTex.h), 256 colores
    static bool loadSheepAtlas();   // sheep.png: piel arriba y lana abajo, 64 x 64   // icons.png (mira, corazones, armadura, burbujas), recortado a 128 x 32

    static int textureID;
    static int skinTextureID;
    static int itemsTextureID;
    static int iconsTextureID;
    static int sheepTextureID;
    static int cartTextureID;
    static int boatTextureID;
    static int cowTextureID;
    static int pigTextureID;
    static int chickenTextureID;
    static int zombieTextureID;
    static int skeletonTextureID;
    static int spiderTextureID;
    static int spidereyesTextureID;
    static int creeperTextureID;
    static int pigzombieTextureID;
    static int ghastTextureID;
    static int ghastfireTextureID;
    static int slimeTextureID;
    static int squidTextureID;
    static int wolfTextureID;
    static int wolfangryTextureID;
    static int wolftameTextureID;
    static int skySunTextureID;    // sol y luna de la Beta (32 x 32, 16 colores)
    static int skyMoonTextureID;
    static bool loadSkyTextures();
    static int paintingTextureID;
    static int signTextureID;      // sign.png: tablero y poste del cartel
    static int signFontTextureID;  // atlas de letras (4 x 6) armado al arrancar desde MenuFont
};

#endif