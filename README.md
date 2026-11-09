<div align="center">

# Minecraft: Nintendo DS Edition
### Eaglercraft Beta 1.7.3 — Nintendo DS Port

A from-scratch port of Minecraft Beta 1.7.3 to the Nintendo DS, written in C++ with devkitPro / libnds.

![Platform](https://img.shields.io/badge/platform-Nintendo%20DS%20%2F%20DSi-red)
![Toolchain](https://img.shields.io/badge/toolchain-devkitARM%20%2B%20libnds-blue)
![License](https://img.shields.io/badge/license-MIT-green)
![Status](https://img.shields.io/badge/version-1.0-brightgreen)

</div>

---

## About

**Minecraft: Nintendo DS Edition** is an adaptation (a semi-port) of everything that
[Eaglercraft Beta 1.7.3](https://github.com/PeytonPlayz595/Beta-1.7.3) brings, rebuilt for the
Nintendo DS family. It is not a wrapper or an emulator layer: the game is a native ARM9 program with its own
world generator, lighting engine, fluid simulation, mob AI, redstone, save system and menus,
all written to fit a console with **4 MB of RAM, no FPU, and a geometry engine capped at roughly 2,000
polygons per frame**.

The goal is to follow Beta 1.7.3 behaviour as closely as the hardware allows. Wherever the original
Java code could be consulted (via the decompiled Eaglercraft sources), the logic was ported from it rather than guessed,
and wherever the DS could not keep up, the deviation is deliberate and documented in
[Known differences](#known-differences-from-beta-173).

> **Status:** 1.0. The game is feature-complete for what the hardware can carry and has been tested on
> emulators and real hardware. Bug reports are very welcome.

## Features

**World**
- Deterministic, chunked terrain (8 × 128 × 8 chunks) with normalized Perlin (fBm) relief, biomes (ocean, desert, forest, taiga, plains), trees (including
  the big oak and spruce shapes), cacti, dead bushes, flowers and tall grass
- Caves, bedrock floor, ore veins (coal, iron, gold, diamond, redstone, lapis), lakes, springs and dungeons with mob spawners
- The **Nether**: obsidian portals, a separate dimension with its own terrain, lava seas and a portal-linked 8:1 coordinate scale
- Day/night cycle, weather (rain, snow, thunderstorms and lightning), clouds, and a sun and moon in the sky (DS and DSi chunk modes)
- Beta-style fluids: flowing water and lava, obsidian and cobblestone generation, falling sand and gravel

**Gameplay**
- 36-slot inventory with persistent tool durability, 2×2 and 3×3 crafting, furnaces, chests, dispensers, armor and health
- Mining times, tool tiers, and the full tool and weapon set (wood, stone, iron, gold, diamond)
- Farming (hoe, seeds, wheat, bread), fishing, bows and arrows, buckets, flint and steel and fire
- Redstone (wire, torches, repeaters, levers, buttons, pressure plates), pistons, TNT, note blocks, minecarts with rails, boats, signs, paintings
- Beds and sleeping, maps, compass and clock
- Four difficulty levels (peaceful, easy, normal, hard) with Beta's damage and regeneration rules

**Mobs**
- Pigs, cows, sheep (with dyeable wool), chickens, squid, wolves (tameable)
- Zombies, skeletons, spiders, creepers, zombie pigmen, ghasts and slimes, with the Beta pathfinding approach (A\*, capped for the DS)

**Console-specific**
- Dual-screen UI: the game on the top screen, hotbar, inventory, crafting and menus on the touch screen
- Two control profiles (**normal**, with touch-screen camera, and **xyba**, with button camera) and fully rebindable buttons
- Three chunk modes — **fast**, **ds** and **dsi** — to trade view distance for speed (see [Performance](#performance))
- World selection, multiple save slots, custom seeds, and settings that persist on the SD card
- Beta texture packs: drop the pack's `.zip` in `/mcdsefiles/texturepacks/` and pick it from the main menu
- Built-in profiler and debug screen for developers (off by default)

## Requirements

### To play

- A Nintendo DS, DS Lite or DSi with a **flashcart**, **or** an emulator
  such as [melonDS](https://melonds.kuribo64.net/) or [no$gba](https://problemkaputt.de/gba.htm)
- An **SD card** with a working DLDI driver. The game stores worlds and settings in `/mcdsefiles/` on the card
- Real hardware is the target. The original DS (Phat) is the baseline for performance work, and running on a DSi unlocks the `dsi` chunk mode

### To build

- [devkitPro](https://devkitpro.org/wiki/Getting_Started) with **devkitARM** and **libnds**, including libfat/filesystem and `grit`.
  On Windows this is the standard devkitPro installer's *NDS Development* option; with `dkp-pacman` you can install it with:

  ```sh
  dkp-pacman -S nds-dev
  ```

## Building

The project uses the standard devkitPro NDS Makefile layout.

```
EaglercraftDS-1.7.3/
├── source/       C++ sources and headers
├── graphics/     PNG textures and grit conversion settings
├── tools/        Build helpers (alphakey, mkmusic.py, mkicon.py)
├── icon.bmp      Game icon shown in Twilight Menu++ / flashcart menus
├── Makefile
├── LICENSE
└── README.md
```

1. Install devkitPro and make sure `DEVKITPRO` and `DEVKITARM` are set (the installer does this for you).
2. Open the devkitPro **MSYS2** terminal (or any shell where those variables are set).
3. Clone the repository and build:

   ```sh
   git clone https://github.com/JohanGCX/EaglercraftDS-1.7.3.git
   cd EaglercraftDS-1.7.3
   make
   ```

4. The result is `EaglercraftDS-1.7.3.nds` in the project folder (the name follows the folder name).
5. To rebuild from scratch: `make clean && make`.

Notes:

- Optional build defines: `-DMC_NO_ITCM` keeps the hot functions out of ITCM (use it if the link fails with an ITCM overflow) and `-DMC_NO_DSI_BOOST`
  does not request the 133 MHz DSi clock.
- The build converts the PNGs in `graphics/` with `grit`. For the terrain and item atlases it first runs `tools/alphakey`, which turns
  PNG transparency into the magenta key colour (`FF00FF`) that the DS needs. This keeps standard RGBA texture packs usable.
- Main RAM is almost fully used. Rarely used code (menus, saving, redstone, …) is compiled with `-Os` on purpose. Please keep that in mind when
  adding code. A build that overflows main RAM fails at link time.

### Windows, Linux and macOS

devkitPro officially supports Windows, Linux and macOS, and the game source is plain C++ with libnds, so the project is not tied to one platform.
The only helper tool is `tools/alphakey`, whose source (`tools/alphakey.c`, using the public-domain `stb_image` headers) is portable C. The Makefile picks the right one automatically:

- **Windows (devkitPro MSYS2):** uses the prebuilt `tools/alphakey.exe`
- **Linux and macOS:** compiles `tools/alphakey.c` once with the system C compiler (`cc`, `gcc` or `clang`), the first time you run `make`. If you have no compiler, install one
  (for example `sudo apt install build-essential` on Debian/Ubuntu, or the Xcode command line tools on macOS)

> Windows is the platform the project is developed on. The Linux and macOS path follows the same devkitPro toolchain, but it has had less testing.
> If you hit a problem there, please open an issue.

## Installing and running

1. Copy `EaglercraftDS-1.7.3.nds` to your flashcart's SD card (or open it in an emulator).
2. Launch it from your flashcart menu. On first run the game creates `/mcdsefiles/` on the SD card:

   ```
   /mcdsefiles/
   ├── options.dat           Settings
   ├── controlsprofile.dat   Button bindings
   ├── skins/                Player skins (.png)
   ├── skinscache/           Converted skins
   ├── texturepacks/         Texture packs (.zip or folder)
   ├── cache/                Converted texture packs
   └── worlds/world1, …      Saved worlds
   ```

3. Choose **New World** or select an existing one, and play.

Worlds are saved when you leave through the pause menu. Use the in-game menu to exit rather than turning the console off.

## Controls

Default bindings of the **normal** profile (the camera is moved by dragging on the touch screen):

| Action | Button |
|---|---|
| Move | D-pad |
| Jump | A |
| Sneak | B |
| Break block / attack | R |
| Place block / use item | L |
| Toggle third person | Y |

The hotbar, inventory and crafting are used with the stylus or your finger on the bottom screen.
The **xyba** profile moves the camera with the face buttons instead of touch. Every action can be rebound in
**Options → Controls**.

## Options

Open the pause menu and choose **Options** (also available from the title screen).

| Option | Description |
|---|---|
| difficulty | Peaceful, easy, normal or hard |
| controls | Control profile and button bindings |
| auto jump | Jump automatically over one-block steps |
| chunk mode | `fast`, `ds` or `dsi` (see below). Changing it in game reloads the world around you |
| render | Geometry detail: `high`, `balanced`, `fast` |
| default seed / custom default seed | Random or fixed seed for new worlds |
| clockcompassinvupdate | Keep the compass and clock sprites updating in the inventory |
| particles / sound | Turn effects on or off |
| inventoryMap | New worlds start with a map in the last hotbar slot |
| debug options | Developer tools: debug chests, `spikes.txt` log, debug screen contents and shortcuts, `oldmobspawnlimit` (the old, higher mob limits) and `oldterraingenerator` (new worlds use the original terrain generator) |

## Texture packs

Beta 1.7.3 texture packs work as they did on PC: copy the pack's `.zip` (no need to extract it) to `/mcdsefiles/texturepacks/` and choose it with the **Texture Packs** button of the main menu. An extracted folder works too.

- A pack is listed only if it has `pack.txt`, `terrain.png` and `gui/items.png`. The name shown is the first line of `pack.txt`
- Used from the pack: blocks and items (`terrain.png`, `gui/items.png`), grass and leaf colours (`misc/`), the health and armor icons (`gui/icons.png`), the mobs and animals (`mob/`), and the minecart, boat and sign (`item/`). Anything the pack does not include keeps the default texture; the menus, skins, paintings, sun and moon are not replaced
- The first time a pack is chosen the console converts it to 256 colours, which takes a few seconds; the result is kept in `/mcdsefiles/cache/`, so later loads are instant
- 16x packs are used as they are; 32x and 64x packs are reduced to 16x
- If the chosen pack is removed or cannot be loaded, the game goes back to the default textures by itself

## Skins

Copy a skin `.png` to `/mcdsefiles/skins/` and choose it with the **Skins** button of the main menu. Both the classic Beta skin (64 × 32) and the modern one (64 × 64) work, as well as HD versions (they are reduced). The player model uses the classic layout, so of a modern skin only the top half is used (no hat layer or separate left arm and leg).
As with texture packs, the first use converts the skin (kept in `/mcdsefiles/skinscache/`), and if the chosen skin is removed the game goes back to Steve.

## Music

The Beta music (calm, hal, nuance, piano) plays in game, with Beta's timing (a pause before each track, chosen in *Options → sound & music → music wait*: `fast` 5-15 seconds, `normal` 5-10 minutes (default), `long` 10-20 minutes like the Beta).
The tracks are built into the `.nds` (NitroFS, from `nitrofiles/music/` in the repository), so nothing has to be copied to the SD card. A `.mus` file with the same name in `/mcdsefiles/music/` on the SD card replaces the built-in track.
Convert `.ogg` files with `python3 tools/mkmusic.py <folder with oggs> -o <output folder>` (needs ffmpeg). All sound settings are in *Options → sound & music*: sound effects on/off, music on/off, music volume (25% to 200%) and music wait.
For testing, set *music wait* to `fast`: the first track starts 5 to 15 seconds after entering a world.

## Performance

The DS has very little memory and processing power, so the engine is built around a few ideas:

- **Chunk modes.** `fast` is the lightest (shortest view distance), `ds` is balanced, and `dsi` uses the extra speed of DSi hardware for
  longer view distance. On a regular DS, `dsi` is not available and is switched to `ds` automatically
- **Time-sliced work.** World generation, lighting, mesh building and saving are split across frames with a shared time budget so the frame
  rate stays steady
- **Display lists** for terrain, level of detail for distant chunks, view-based culling, hardware fog and a fixed-point-heavy design (no FPU)
- **Fast lookups for the hot paths.** Chunk and block queries use a direct-mapped slot table, inline block reads and a few tiny functions placed in the
  ARM9's zero-wait ITCM memory. On DSi the game also requests the 133 MHz CPU clock and uses larger terrain caches sized for the `dsi` window
- **A built-in profiler.** *Options → debug options* lets you show it on the debug screen (open it with the touch
  screen mini-button) and optionally log slow frames to `/mcdsefiles/spikes.txt`

## Known differences from Beta 1.7.3

This is a port to very limited hardware, so some things are intentionally different:

- Chunks are 8 × 8 blocks (instead of 16 × 16) and view distance is short. Terrain, caves, lakes and structures are scaled to match
- The Nether terrain is a simplified version of the original generator (2D noise) for performance
- Overworld terrain is a 2D height field (normalized fBm noise) instead of Beta's 3D density field. Worlds created before this generator keep the original one
  (version 0), and *Options → debug options → oldterraingenerator* creates new worlds with it too
- Animals and hostile mobs are not saved with the world, to save space on the SD card. Tamed wolves are saved
- Background music is streamed at 8 kHz mono (see [Music](#music)). Sound effects are a reduced set
- No stars or sunrise/sunset glow in the sky. The sun and moon are shown only in `ds` and `dsi` chunk modes, at a shorter distance
- Leaves do not block light; only water and ice do
- The map is carried in the hotbar (as on the Legacy console editions) instead of being shown on the lower screen
- Some textures were reduced to fit in video memory

## Roadmap

- More testing of the Linux and macOS build
- More mob and block polish, and further optimization of mob rendering
- Documentation for contributors

## Contributing

Bug reports, performance traces and testing on real hardware are the most useful contributions. When reporting a problem, please include:

- Your console model (DS, DS Lite, DSi, 3DS) and whether you use a flashcart or an emulator
- The chunk mode and render setting in use
- Steps to reproduce, and the contents of `/mcdsefiles/spikes.txt` if the problem is about performance (enable it in *Options → debug options*)

## Credits

This project would not exist without:

- **Claude** (by [Anthropic](https://www.anthropic.com)): wrote the code of the implementation, from the engine and world generation to the mobs, redstone and
  save system, working from the decompiled Beta 1.7.3 sources
- **[JohanGCX](https://github.com/JohanGCX)**: project lead; guided the vision and direction of the development and designed and implemented the menus
  exclusive to this port
- **[figuidos](https://www.youtube.com/@figuidos)**: community contributor; chunk lookup and ITCM optimizations, DSi terrain caches and clock boost, the
  normalized terrain generator, the background music streaming, the sound & music menu, the game icon and the sleeping suffocation fix,
  plus testing and bug reports
- **[PeytonPlayz595](https://github.com/PeytonPlayz595)**: the decompilation of Minecraft Beta 1.7.3
  ([Eaglercraft Beta 1.7.3](https://github.com/PeytonPlayz595/Beta-1.7.3)), the reference for the game logic
- **The community and fans**, who proposed ideas, reported bugs and gave feedback during development through itch.io and YouTube
- **[Mojang Studios](https://www.minecraft.net)**, creators and owners of Minecraft, including its game design, textures, sounds
  and music. Everything this port reproduces comes from their work
- The **[devkitPro](https://devkitpro.org)** team, **libnds**, **libfat** and **grit**, which make homebrew development on the DS possible

## License

The source code of this port is released under the [MIT License](LICENSE).

## Legal notice

This is an unofficial, non-commercial fan project. It is **not** affiliated with, endorsed by or sponsored by Mojang Studios,
Microsoft or Nintendo. *Minecraft* is a trademark of Mojang Studios. *Nintendo DS* and *DSi* are trademarks of Nintendo.

The MIT license applies only to the code written for this port. It does **not** grant any rights over Minecraft's game design, names,
textures, sounds, music or other assets, which belong to Mojang Studios and Microsoft, and are used here with the credit they deserve. If you
are a rights holder and have a concern about this project, please open an issue and it will be addressed promptly.
