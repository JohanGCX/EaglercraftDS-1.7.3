#ifndef TEXTUREPACK_H
#define TEXTUREPACK_H

// Texture packs de la Beta 1.7.3: /mcdsefiles/texturepacks/<pack>.zip (como la Beta) o una carpeta descomprimida. Un pack vale solo si tiene
// pack.txt, terrain.png y gui/items.png; su titulo es la primera linea de pack.txt. Por ahora se usan terrain.png e items.png.
// La primera vez que se elige un pack, la consola convierte los PNG a 256 colores y guarda el resultado en /mcdsefiles/cache/<pack>.bin;
// despues solo se lee ese archivo. Las texturas se escriben encima de las que trae el juego (terrainBitmap, itemsBitmap y sus paletas): no
// gastan RAM. Las originales quedan copiadas en /mcdsefiles/cache/default.bin para poder volver a Default sin reiniciar.
namespace TexturePack {
    struct Entry {
        char name[48];    // nombre del zip o de la carpeta (lo que se guarda en texturepack.txt)
        char title[30];   // primera linea de pack.txt
        bool zip;
    };

    // Al arrancar, con terrain e items ya subidos a la VRAM y TextureAnim / AnimItems iniciados. Guarda default.bin si hace falta y aplica el pack
    // elegido. Si el pack ya no esta, no sirve, o la ultima conversion se corto (se apago la consola), vuelve a Default y lo deja anotado.
    void boot();

    int scan(Entry* out, int max);    // los packs validos (ordenados por nombre)
    const char* activeName();         // "" = Default
    bool apply(const char* name);     // "" = Default. Si falla queda Default y lastError() dice por que
    const char* lastError();

    // Sube una textura de 8 bits ya cargada otra vez desde la RAM (texeles en su lugar de la VRAM, paleta nueva). La usan los packs y las skins.
    void uploadTexture(int texId, const void* bmp, unsigned int bytes, const void* pal, int entries);
}

#endif
