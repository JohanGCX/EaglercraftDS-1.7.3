# CLAUDE.md — Technical guide to *Minecraft: Nintendo DS Edition* (Eaglercraft Beta 1.7.3 — Nintendo DS Port)

> **Purpose of this file.** It is written so that an AI assistant (Claude, DeepSeek, or any other model) that receives it as the *first message* of a chat
> can understand how the whole project works at the same technical level as the people who built it, and can then help someone modify the game safely.
> If you are an AI reading this: read the whole file before proposing any change, and follow the rules in sections 2 and 3.
>
> **If you are a human**: paste this file as your first message (or attach it), then attach the source files you want to change and describe the change.

---

## 1. What this project is

A from-scratch, native **C++ port of Minecraft Beta 1.7.3 to the Nintendo DS** (ARM9, devkitPro + libnds). It is *not* an emulator layer or a wrapper:
world generation, lighting, fluids, redstone, mobs, saving and the menus are all implemented in this repository.

- Reference for game logic: the decompiled Beta 1.7.3 Java sources (the Eaglercraft Beta 1.7.3 project by PeytonPlayz595). Wherever those sources were
  available, behaviour was **ported from them**, and comments name the original classes (`WorldGenLakes`, `BlockFluid`, `EntityMinecart.onUpdate`, …).
  The owner of the project has the full decompilation and can paste any `.java` file when exact original behaviour is needed — **ask for it instead of guessing.**
- Target: real DS hardware (DS Fat/Lite is the performance baseline), also DSi and emulators (melonDS reproduces the DS limits; no$gba hides them).
- Code size: about 47,000 lines, 70 `.cpp` and 103 `.h` files, all in `source/` (flat directory, no subfolders).
- Language of the codebase: **English is the target** for new code, identifiers and anything user visible. Most *existing comments* are still in Spanish
  (the project was developed in Spanish). Do not be confused by that; new comments should be in English.

### Hard hardware constraints (these shape every design decision)

| Constraint | Consequence |
|---|---|
| ARM946E-S @ 67 MHz, **no FPU** | `float`/`double` are emulated in software and slow. Use fixed-point / integers in hot loops. Float is fine in rarely run code. |
| **4 MB main RAM**, almost fully used | Every new array, static table or code size matters. A build that overflows RAM fails at link time (region `lma9`). Rarely used code is compiled with `-Os` (see Makefile `SMALL_OFILES`). |
| 3D engine: ~2048 polygons / ~6144 vertices per frame, 15-bit depth, hardware fog, no per-vertex alpha, no additive blending | Geometry is budgeted, culled and LOD'd. Polygons beyond the limit are silently dropped. |
| VRAM: banks A and D = textures, F and G = texture palettes, B = top BG, C = sub BG | Textures are paletted (256 or 16 colours). A new texture may *not fit*. |
| 8 KB data cache, 16-bit main-RAM bus | Smaller (Thumb, `-Os`) code often wins. Cache flushes by `glCallList` are costly. |
| Two screens: top = main engine (3D game), bottom = sub engine (touch UI / text console) | The bottom screen is a 2D/console UI, not a second view. |
| SD card through DLDI/libfat | File I/O is slow. It must be done in small pieces, never in bursts. |

---

## 2. Rules for an AI assistant modifying this project (read carefully)

1. **You cannot compile or run the game.** Nothing you write is tested unless the user tests it. Say so, and tell the user exactly what to check.
2. **Never edit from memory.** Ask the user for the *current* version of every file you will touch (`Chunk.cpp`, `main.cpp`, …) and read it completely
   before giving changes. Patches pasted into stale copies were a frequent source of duplicated or misplaced code. Give complete replaced blocks or whole files,
   with enough context to locate them, and say which files changed.
3. **Match the original game.** When the behaviour of Beta 1.7.3 matters, ask for the relevant decompiled `.java` file and port it; do not rely on general
   Minecraft knowledge of later versions (Beta 1.7.3 differs: no hunger, no sprinting, different wolves, etc.).
4. **Respect the budgets.** Anything that runs every frame or every tick must have a bounded cost. Heavy work must be split across frames (see §7 and §11).
   *A per-object cap does not bound the frame cost when many objects each spend their own cap; use a shared per-frame budget.*
5. **No new float in hot paths. No `rand()` in world generation** (generation must be deterministic from the seed, use `Noise::hashInt` or a local RNG).
6. **Save compatibility.** Never reorder `BlockType` values or save-file layouts; append at the end (see §8 and §14).
7. **RAM is the scarcest resource.** Before adding a big static array, check its size, and say what you expect it to cost. Prefer sharing buffers.
8. **Keep changes small and local.** Do not refactor unrelated code; do not rename files (the Makefile compiles every `.cpp` in `source/`).
9. **Write new comments and identifiers in English.**
10. **If something is unclear, ask for the file.** It is always better to request a file than to invent an API that may not exist.

Typical workflow with the project owner: they describe a change → you ask for the files → they paste them → you answer with changed files/blocks and
a "what to test" list → they build and report back (with the profiler output if it is about performance).

---

## 3. Build system

- Toolchain: devkitPro **devkitARM** + **libnds** + libfat/filesystem + **grit**. Install on any platform with `dkp-pacman -S nds-dev`.
- Layout: `source/` (code), `graphics/` (PNG + `.grit` conversion settings), `tools/` (`alphakey`), `Makefile`, `LICENSE`, `README.md`, `CLAUDE.md`.
- Build with `make` in a shell where `DEVKITPRO`/`DEVKITARM` are set. Output: `<folder name>.nds` (the Makefile uses `basename $(CURDIR)` as the target name).
- Compiler flags (Makefile): `-g -Wall -O2 -march=armv5te -mtune=arm946e-s -fomit-frame-pointer -ffast-math -ffunction-sections -fdata-sections -mthumb -mthumb-interwork -DARM9`, C++: `-fno-rtti -fno-exceptions -std=gnu++11`;
  link with `--gc-sections`, libs `-lfilesystem -lfat -lnds9`. No exceptions, no RTTI, C++11 only.
  Files listed in `SMALL_OFILES` are compiled with `-Os`. Current list: `CraftingScreen InventoryScreen MenuFont TextKeyboard NewWorldMenu Options PauseScreen SettingsMenu SubScreenMenu TopScreenMenu WorldSelectMenu GameOverScreen MenuPanorama SignManager WorldSave Profiler Governor Furnace ChestStore Redstone Pistons NoteBlocks TntManager Dispenser Projectiles MinecartManager BoatManager PaintingManager PartialBlocks` (all `.o`).
  Add a file to that list if it is cold code and RAM is tight. An experiment compiling hot math in ARM mode (`-marm`) was slower (a column went from 2.5 to 23 ms: 16-bit RAM bus + 8 KB cache favour compact Thumb) and is commented out — do not retry it.
- Output name: `TARGET := basename $(CURDIR)`; build dir `build/`; `DATA := data`, `INCLUDES := include` (these two directories may not exist; fine).
- PNG → data: `graphics/*.png` are converted by `grit` with the settings in the matching `.grit` file. Typical `.grit`: `-gb -gB8 -p` (8-bit bitmap + palette),
  optionally `-gzl` (LZ77, decompressed straight into VRAM with `decompress(..., LZ77Vram)`). Each image `foo.png` produces `foo.h` with `fooBitmap`, `fooPal`, `fooPalLen`.
- **Transparency:** the DS has no alpha in paletted textures. `terrain.png`, `items.png`, `arrows.png`, `icons.png` go through `tools/alphakey` first (alpha < 128 →
  magenta `FF00FF`, a genuinely opaque `FF00FF` becomes `FE00FF`), then grit with `-gT FF00FF` so that colour becomes palette index 0, and the texture is loaded with
  `GL_TEXTURE_COLOR0_TRANSPARENT`. This keeps standard RGBA Beta texture packs usable. `tools/alphakey.c` is portable C (uses `stb_image`); the Makefile uses `alphakey.exe`
  on Windows and compiles `alphakey.c` once with the host `cc` on Linux/macOS. Custom rules for these atlases are placed *after* the main rule (placing them first would make
  them the default make target).
- Some data headers are generated from PNG/OGG by scripts and marked "Generated … do not edit by hand": `SoundData.h`, `SoundAmbient.h`, `SkyTex.h`, `CloudsData.h`,
  `ColorMapData.h`, `AnimData.h`, `AnimItemsData.h`, `PaintingTex.h`, `MapBg.h`, `ArmorShape.h`. If you must change one, regenerate it instead of editing numbers.

---

## 4. Runtime architecture

### 4.1 Entry point and states (`main.cpp`, ~3300 lines)

`main()` initialises the renderer, shows the startup splash (`StartupSplash`: Eagtek logo, Mojang logo, 1 s each, bottom screen white), initialises libfat, `Prof`, `WorldSave`,
`Options`, `Controls`, `Sound`, loads all textures, then enters the main loop. Game state is the enum `GameState`:

`STATE_MENU` → `STATE_WORLD_SELECT` → `STATE_CREATE_WORLD` → `STATE_LOADING` → `STATE_PLAYING`, plus `STATE_OPTIONS`.

Most game logic is still inside `main()` as local variables and lambdas (`startWorld`, `changeDimension(bool reload)`, `respawnPlayer`, `closePause`, …). When changing it, search by
name; the file is long and has many `static` helpers at the top (`computeCelestialAngle`, `computeSkyBrightness`, `ensureInventoryMap`, …).

### 4.2 Timing

- The loop runs at **60 Hz** (`swiWaitForVBlank`). **3D is drawn every 3rd iteration** (`renderFrameCounter % 3`); **game ticks run at 20 Hz** (every 3 iterations), as in the original.
  Order inside one iteration of the `STATE_PLAYING` loop in `main()`: `Governor::setEnabled(true)` + `Chunk::setBudgetPercent(Governor::budgetPercent())` → `chunkManager->update()` (**every** iteration: generation, lighting, load/save queues — this is the background work)
  → input and touch UI → `frameCounter++`, `renderFrameCounter++`, world time advance (`gameTimeTicks`, skylight subtract) → if `renderFrameCounter % 3 == 0`: 3D drawing (`chunkManager->drawAll`, then clouds, sky, weather, entities, HUD)
  → profiler end-of-frame (`Prof::endFrame`, debug screen print every 30 frames) → pacing and `swiWaitForVBlank` → if `frameCounter >= 3`: the **20 Hz logic tick** (`PROF(Prof::LOGIC)`): player physics, then `tickLiquids`, `Redstone::tick`, `Pistons::tick`,
  weather, maps, mobs (`SheepManager::tick`, step label `t mobs`), etc. Constants: `FRAMES_PER_WORLD_TICK = 3`, `WORLD_DAY_TICKS = 24000`.
  Per-frame budgets (`Chunk.cpp`): heavy generation `HEAVY_BUDGET_TICKS` ≈ 6 ms × chunk-mode multiplier, display-list rebuild `REBUILD_BUDGET_TICKS` ≈ 4 ms, both scaled by `Governor::budgetPercent()`; at least one unit of work is always allowed per frame.
  A static 24 KB scratch buffer (`DL_SCRATCH_WORDS = 6144` words) is used to build each band before copying it to its exact size.
- Pacing: if a drawn frame exceeds ~16.7 ms the vblank wait rounds up and the whole tick takes 4 instead of 3 vblanks. That delay is recorded as a **debt** (`s_lagTicks`) and later
  iterations (which do not draw) skip their wait until it is repaid, so the tick rate stays at 20 Hz while the average load is sustainable.
- `Governor` (`Governor.cpp`): looks at the previous frame time. If it exceeded ~20 ms it halves the budgets (`budgetPercent()`) and may skip the heavy work of up to two consecutive frames
  (`skipHeavy()`). It is disabled on the loading screen.
- Day length: world ticks at 20/s; `computeCelestialAngle` is an exact port of `WorldProvider.calculateCelestialAngle`.

### 4.3 Profiler and debug screen (`Profiler.h/.cpp`)

- Hardware timer based (`Prof::now()`, 33514 ticks per ms). `PROF(Prof::SLOT)` accumulates per-frame time by subsystem (`UPDATE, GEN_COL, GEN_STRUCT, FACES, SKY, DL_REBUILD, DRAW_ALL, DL_LOD, DL_CULL, DL_CALL, LOGIC, ENTS, FRAME`).
  `PROFSTEP("label")` records a *named step* (a lake, a tree, a band rebuild, `t mobs`…), and each frame keeps the heaviest step.
- **Debug screen** (bottom screen console): opened with the touch mini-button during play. Shows peak ms per subsystem over 2 s, the worst step, free/min RAM, GPU overflow flags,
  display-list KB, chunk pool, health and coordinates. `Options → debug options → debug screen info` can reduce it to coordinates only.
- **Spike log:** with `Options → debug options → spikes.txt log` on, every frame ≥ 30 ms is appended to `/mcdsefiles/spikes.txt` (48 worst kept, written when the console is
  opened or when quitting). Format: `t=<s> f=<total ms> upd=… col=… est=… car=… sky=… ban=… drw=… lod=… cul=… cal=… log=… ent=… | <heaviest step> <ms> | x y z d`.
  **This is the main tool for performance work: ask the user for this file when something is slow.**
- Optional debug keys (when `debugScreenOptions` is on and the debug screen is open): `B` day/night, `Y` rain toggle, `X` damage on/off (peaceful only).

---

## 5. Source file map

All files are in `source/`. "Port of …" means it follows the named Beta 1.7.3 Java class.

### 5.1 Core, world and rendering

| File(s) | Role |
|---|---|
| `main.cpp` | Game loop, states, input, world start/save/dimension change, HUD flow, debug screen, almost all glue. |
| `WorldConfig.h` | Chunk size `SIZE_X=8, SIZE_Y=128, SIZE_Z=8`; chunk-mode parameters (view radius, columns per frame, time-budget multiplier). |
| `Chunk.h/.cpp` (~4900 lines) | **The heart of the engine.** One 8×128×8 chunk: blocks, light, face cache, occupancy bits, generation (terrain, caves hook, lakes, trees, ores, cactus, springs, plants, dungeons, Nether features), editing (`setBlockDirect`, `placeBlockAt`, `breakBlockAt`), meshing into display lists, LOD, load/save. |
| `ChunkManager.h/.cpp` | Window of loaded chunks around the player, a preallocated **pool** of `Chunk` objects (placement new), neighbour lookup, load/save queues, cross-chunk light propagation, per-frame shared budgets, `drawAll`. Also fixes the chunk mode at construction (falls back from `dsi` to `ds` on plain DS or low RAM). |
| `DisplayListBuilder.h/.cpp` | Builds a GPU display list in two passes (count, then write). |
| `Cube.h/.cpp`, `BlockFace.h` | Cube/face primitives. |
| `Noise.*`, `PerlinNoise.*` | Deterministic noise (`Noise::hashInt`), Perlin terrain (6 octaves). *(Used by terrain and lakes; read before changing generation.)* |
| `Biome.h`, `Climate.h`, `ColorMaps.*`, `ColorMapData.h` | Biomes (ocean, desert, generic/plains, forest, taiga…), temperature/humidity, grass/foliage tinting from Beta colour maps. |
| `Caves.h/.cpp`, `CaveSinTable.h` | Port of `MapGenCaves`: tunnels simulated per 16×16 cell and cached as spheres; bedrock floor. |
| `Lakes.h`, `NetherGen.h`, `Trees.*` | Lake generation (`WorldGenLakes`), Nether terrain/features (reduced 2D-noise version), saplings and tree growth (oak, birch, spruce/taiga, big oak). |
| `Liquid.h/.cpp` | Beta water/lava simulation (`BlockFluid`/`BlockFlowing`) with a scheduled-tick queue; lava rules, obsidian/cobblestone generation. |
| `Weather.*`, `Clouds.*`, `Sky.*`, `SkyTex.h` | Rain/snow/thunder/lightning, clouds from `clouds.png` bits, sun and moon (only chunk modes ds/dsi; the sunrise glow code exists but is disabled by `SKY_GLOW 0`). |
| `Renderer.*` | VRAM bank setup, projection (FOV 70°, near 0.1, far 40) and hardware fog derived from the chunk-mode render distance. |
| `Texture.*`, `TextureAnim*`, `AnimData.h`, `AnimItems*` | Texture loading (every `Texture::*TextureID`), animated fire/portal textures, animated compass and clock. `unloadEntityTextures`/`reloadEntityTextures` free VRAM while the menu panorama is shown. |
| `Raycaster.*` | Exact DDA block raycast ported from `World.rayTraceBlocks`. |
| `Player.*` | Camera, movement physics (ported accelerations, step height 0.5, optional auto-jump), sneaking, third person. |
| `Profiler.*`, `Governor.*` | See §4. |

### 5.2 Blocks, items, inventory, crafting

| File(s) | Role |
|---|---|
| `BlockType.h` (~1500 lines) | `enum class BlockType : unsigned char`: **every block and item id**, plus per-type tables (durability, tool tier, hardness, light emission, solidity, textures, drops…). Items share the enum with blocks. **Append new entries at the end only.** |
| `ItemOrder.h` | Orders blocks/items like Beta 1.7.3's numeric ids (used by the creative/debug lists and crafting list). |
| `Recipes.h` | Crafting recipes (2×2 and 3×3, alternates via `altA/altB`), `RECIPE_COUNT` computed with `sizeof`. |
| `Inventory.*` | 36 slots + armor slots 36–39, durability per slot, stacking logic. |
| `InventoryScreen.*`, `CraftingScreen.*`, `HotbarIcons.*`, `GameplayHotbar.*` | Bottom-screen inventory, crafting, hotbar rendering and touch zones. |
| `DroppedItem.*`, `PendingDrops.*` | Dropped item entities (stacked, merge when near each other), queue of drops created during block updates. |
| `HeldBlock`/`HandAnimations.*` | First-person hand/item rendering and swing animation (port of the Beta hand). |
| `Furnace.*`, `ChestStore.*`, `Chest.h`, `Dispenser.*` | Furnace logic (fuel, smelting, light), chest/dispenser contents keyed by global cell. |
| `PartialBlocks.*` | Doors, trapdoor, bed: blocks that use metadata and don't fill the cell. |
| `Redstone.*` | Port of `BlockRedstoneWire/Torch/Lever/Button/PressurePlate/Repeater/Ore` plus the `World` power functions. API (namespace `Redstone`): `tick(cm)` (20 Hz scheduled ticks + plates), `randomTick`, `blockChanged`, `blockRemoved` (queued), `activate` (lever/button/repeater click), `lightOre`, `useItem`, `bounds`, `indirectPowerAt`/`notifyNeighbors` (used by pistons), `reset()`. Entity boxes for plates and detector rails come from enumerators registered by `main.cpp` (`setEntityEnumerator`, `setMinecartEnumerator`). Metadata: wire 0–15 strength; lever 1–4 wall / 5–6 floor, bit 8 = on; button 1–4, bit 8 = pressed; plate 1 = pressed; torch 1–5; repeater bits 0–1 direction, 2–3 delay. No sounds/particles yet. |
| `Pistons.*` | Port of `BlockPistonBase/Extension/Moving` + `TileEntityPiston`. Base meta = direction (0 down, 1 up, 2 −Z, 3 +Z, 4 −X, 5 +X) \| 8 if extended; head meta = direction \| 8 if sticky. Moving blocks live in a list inside `Pistons.cpp`. API: `tick`, `baseNeighborChanged`, `baseAdded`, `headNeighborChanged`, `headRemoved`, `cleanupOrphan`, `useItem`, `bounds`, `emitBase/emitHead/emitBox` (quad callbacks used by `Chunk`'s meshing), `draw` (moving blocks, per frame). No sound and no entity pushing. |
| `Sleeping.h` | Header-only port of bed sleeping: `eyePosition`, `bedOrientationDeg`, `modelShift`, `nearestEmpty` (wake-up cell search, template over a `typeAt` functor), `standingEye`, `overlayAmount` (dark veil: grows to tick 100, fades by 110). Beds are handled by their "foot" half (metadata bit 8). |
| `Maps.*` | Port of `ItemMap/MapData`: 128×128 one-byte pixels (color*4+shade), scale 3 (1 pixel = 8×8 blocks); map number stored in the item's durability (0 = unassigned); data in `maps.dat` (valid in both dimensions); only `CACHE = 3` maps in RAM. API: `create`, `tick` (explores one column per tick while held), `pixels`, `playerMarker`, `colorOf`, `flush`, `reset`. |
| `Dispenser.*` | Port of `BlockDispenser`: 9 slots stored in `ChestStore` by global cell; redstone schedules a 4-tick dispense that emits a `Drop` (position, velocity, type, durability) which `main.cpp` pops with `popDrop` and spawns as a dropped item or projectile (arrows/eggs/snowballs). |
| `RailLogic.h`, `NoteBlocks.*`, `Spawners.*`, `TntManager.*`, `FallingBlocks.*`, `SignManager.*`, `PaintingManager.*`, `Teleporter.h`, `Health.h` | Rail shapes (metadata 0–9), note blocks, mob spawners, primed TNT, falling sand/gravel, signs, paintings, portal search/build, health/damage rules. |
| `DebugChests.h` | Debug option: spawns two large chests with all blocks/items in new worlds. |

### 5.3 Mobs and entities

| File(s) | Role |
|---|---|
| `SheepManager.*` | Entry point of the mob managers: forwards to `AnimalManager` and `HostileManager` (cap, spawn timers, drawing). |
| `AnimalAI.h/AnimalManager.*/AnimalModel.*/AnimalData.h`, `Sheep*` | Cows, pigs, chickens, sheep. AI headers (`*AI.h`) contain physics + A\* pathfinding; `*Manager.cpp` connects them to the world; `*Model.cpp` draws them. |
| `HostileAI.h/HostileManager.*/HostileModel.*/HostileData.h` | Zombie, skeleton, spider, creeper, slime, zombie pigman, ghast, wolf (tameable), squid. |
| `MinecartAI.h/MinecartManager.*/MinecartModel.*`, `BoatAI.h/BoatManager.*/BoatModel.*` | Minecarts (rails, chest/furnace carts) and boats. |
| `MobBody.h`, `ExplosionDamage.h`, `ModelBox.h`, `SteveModel.*`, `FishingHook.*`, `Projectiles.*`, `Particles.*` | Mob-vs-mob collision, explosion damage, model box helper (port of `ModelRenderer.addBox`), player model, fishing rod, arrows/snowballs/eggs, particles. |

Mob AI notes: A\* pathfinding is capped (`PATH_MAX_NODES = 200`, `PATH_MAX_EXPANSIONS = 80`) for speed. Mobs use *aligned world cells* (integer x/z; block `j` covers `y ∈ [j-0.5, j+0.5]`), minecarts and boats use *Beta grid coordinates* — see §9.

### 5.4 UI, menus, persistence, audio

| File(s) | Role |
|---|---|
| `TopScreenMenu.*`, `MenuPanorama.*`, `SubScreenMenu.*`, `WorldSelectMenu.*`, `NewWorldMenu.*`, `PauseScreen.*`, `GameOverScreen.*`, `LoadingScreen.*`, `StartupSplash.*` | Menus and full-screen images. Menu images are 8-bit paletted bitmaps (often LZ77) drawn on BG layers. The title panorama is a rotating 3D cube. |
| `SettingsMenu.*`, `Options.*`, `Controls.*`, `TextKeyboard.*`, `MenuFont.*` | Options console UI (list with scrolling and submenus), settings storage, rebindable controls (two profiles), on-screen keyboard, 4×6 bit font. |
| `WorldSave.*` | Folder structure on the SD card, world slots, session handling (§14). |
| `Sound.*`, `SoundData.h`, `SoundAmbient.h` | Audio: 8-bit PCM clips played with `soundPlaySample`. `Sound::uiClick()` plays the button click (no per-frame filter, works in menus). |
| `HealthHud.*`, `HealthHudLayout.h`, `ArmorShape.h` | Health/armor/bubble HUD on the top screen. |

---

## 6. Coordinates and units (a frequent source of bugs)

- **Chunk grid:** `(xi, yi, zi)` with `xi,zi ∈ [0,8)`, `yi ∈ [0,128)`. Chunk coordinates `(chunkX, chunkZ)`; global cell `gx = chunkX*8 + xi`, `gy = yi`, `gz = chunkZ*8 + zi`.
- **World (float) coordinates:** a block is centred on a half-integer; `gridToWorldX/Z` already include the offset. `gridToWorldY(yi)` has no offset (`yi - 1`) and a grid Y index is never interchangeable with a world Y without it.
  When converting back with negative numbers use `(int)floorf(...)`, never truncation, and don't add `+0.5` again.
- **Mobs:** aligned cells where block `j` covers `[j-0.5, j+0.5]` in Y. **Minecarts/boats/rails:** Beta grid coordinates where cell `(gx,gy,gz)` spans `gx..gx+1`. Check the header of the manager you edit.
- **Nether:** 1 block in the Nether = 8 in the overworld (`changeDimension` scales the player position).
- **Fixed-point:** libnds `v16` (4.12 fixed, range ±8) is used for vertices. Large coordinates (e.g. in `Sky.cpp`) are divided by 16 and multiplied back by the matrix scale.
  Hardware depth is 15-bit and non-linear; fog uses `fixed12d3` depth, not `v16`.

---

## 7. World engine in detail

### 7.1 Chunk modes (`Options::chunkMode`, `WorldConfig`)

Set through `WorldConfig::setChunkMode(radius, cols, mul)` by `ChunkManager` when it is constructed (static defaults before that: radius 2, cols 3, mul 1). The real values are set in the `ChunkManager` constructor: `setChunkMode(radius, mode==0 ? 1 : mode==1 ? 3 : 4, 1)`. `WorldConfig::MAX_VIEW_RADIUS = 3`.

| Mode | View radius (window) | Max columns generated per frame | Time-budget multiplier | Notes |
|---|---|---|---|---|
| 0 `fast` | 1 (3×3 chunks) | 1 | 1× | Lightest. No sun/moon. |
| 1 `ds` | 2 (5×5) | 3 | 1× | Balanced. (With no saved options, a plain DS starts in `fast` and a DSi in `dsi`.) |
| 2 `dsi` | 3 (7×7) | 4 | 1× (a 2× budget stretched frames, so dsi uses 4 columns at the same time budget as ds) | Only on DSi hardware; `Options::load()` and the menu map `dsi` to `ds` on a plain DS. |

Pool sizing (constructor): `windowCount = (2r+1)²`, `maxChunks = windowCount + (r>1 ? 3 : 7)` → 16 / 28 / 52 chunks, `poolSize = maxChunks + POOL_SPARE`. The constructor allocates the pool with `new (std::nothrow)`; if allocation fails (or dsi leaves < 700 KB free for display lists) it drops one mode and retries. The initial 3×3 is created first; the rest of a larger window is requested after it completes (`fillPending`).

Render distance in world units = `8 * radius + 1` (`WorldConfig::renderDistance()`): 9, 17, 25. `ChunkManager` reads the mode in its constructor and falls back if RAM is short. Changing the mode from the pause menu → Options reloads the world
(`changeDimension(true)`): saves everything, recreates the manager and keeps the player position, mobs and dropped items.

### 7.1b `ChunkManager::update` order (every main-loop iteration)
1. Mark chunks `inWindow` (|dx|,|dz| ≤ radius from the centre chunk).
2. Pop **one** queued file load (`pendingLoadSlots`; loading is queued, never done inside `findOrCreateSlot`); a loaded chunk finishes biome/colours over several frames (`loadingBiome`, `continueLoad()`, nearest first).
3. Flush **one** deferred save (`pendingSaveChunks`; modified chunks evicted from the window are saved one per frame).
4. Heavy work: chunks needing generation/cache/skylight/emitter work are sorted by grid distance to the centre (nearest first) and updated, at most `MAX_CONCURRENT_HEAVY_CHUNKS = 3` per frame and only while `Chunk::heavyBudgetLeft()`; skipped entirely when `Governor::skipHeavy()`. When a chunk becomes complete its 4 neighbours get `borderRefreshPending`.
5. `continueSkyFlood()`, then **one** neighbour border refresh per frame (7 columns).
6. Already-complete chunks get their cheap `update()`.
7. Recentre with hysteresis (player must be > half a chunk + 1 block from the current centre): all target positions are computed first, then `findOrCreateSlot` reuses free slots or evicts the farthest non-protected chunk.

`drawAll`: chunks within `renderDistance()+6`, sorted by **grid** distance to the centre chunk (stable order), nearest first, sharing a vertex budget `vertBudget` = 7000 / 11000 / 15000 for `renderQuality` fast / balanced / high, and a rebuild budget (0 when the Governor says skip).

### 7.1c `tickLiquids` (20 Hz) and random ticks
`tickLiquids()` runs up to `LIQUID_TICKS_PER_STEP = 24` scheduled liquid ticks (`Liquid::popDue` → `Liquid::Sim<LiquidWorld>::runScheduled`), then random ticks: **20 random cells per chunk per tick** (Beta: 80 per 16×16 chunk). Dispatch by block: LAVA → `lavaRandomTick`; CROPS → `Partial::cropsTick`; saplings → `Trees::saplingTick`; leaves with meta bit 8 → `Trees::leafTick`; ICE (melts if block light > 8) / SNOW_LAYER / SNOW_BLOCK (> 11, drops 4 snowballs); FARMLAND → `Partial::farmlandTick`; redstone random-ticking blocks and repeaters → `Redstone::randomTick`. `LiquidWorld` is the adapter given to the liquid simulator (`getType/getMeta/putCell/pushTick/randInt/dropItem/spawnFalling…`); water drops torches, seeds, cross plants, cactus.

### 7.2 Memory model

- A `Chunk` costs about **31 KB of fixed RAM** plus its display lists. Main members (all sized by `WorldConfig`): `blocks[8][128][8]` (`BlockType`, 8 KB), `faceCache[8][128][8]` (`FaceMask`),
  `lightData[8][128][8]` (u8: low nibble block light, high nibble sky light, 8 KB), `liquidMeta` (4096 B, 4 bits per cell), `occupancy[8][8][4]` (128 bits per column), `columnMinY/MaxY`, `biomeColumn`, `grassTint`/`foliageTint`, `skyBottom`,
  the climate lattice, the `dlBands`/`dlBands2` pointer arrays (64 each) and the LOD list. Memory **canaries** (`0xDEAD0001`…`0xDEAD0006`) surround `blocks`, `faceCache` and `occupancy` to detect overruns (`checkMemoryCanaries()`).
- `ChunkManager` has `CAP_CHUNKS = (2*MAX_RADIUS+1)^2 + 3 = 52` slots and a **fixed pool** of `CAP_CHUNKS + POOL_SPARE(2)` objects allocated once; objects are reused (destructor + placement new) to avoid heap fragmentation
  (the original cause of crashes was fragmentation: no 67 KB contiguous block left). `poolKB()` reports the pool size on the debug screen. In a smaller mode only `windowCount` (9 / 25 / 49) slots are active (`maxChunks`).
  Extra chunks beyond the first 3×3 are created after the loading screen (`fillPending`).
- Chunks outside the 3×3 window free their display lists (`Chunk::setInWindow`/`freeDisplayLists`).
- File I/O for chunks goes through **queues** (`pendingLoadSlots`, `pendingSaveChunks`), one operation per frame each. Never do synchronous save+load in bursts.

### 7.3 Generation pipeline (`Chunk::update`, spread across many frames)

`Chunk::update` is called once per chunk per frame by `ChunkManager::update`. A finished chunk only runs `continueFaceCacheRecalc`, `continueInitialSkyLight` and `continueEmitterReseed`.
`structureGenPhase` values: **0 lakes → 1 trees → 2 ores → 3 cactus → 4 springs → 6 plants → 7 dungeons → 5 closing** (overworld); **10 nether features → 5 closing** (Nether). One phase step per frame; `closing` sets `generationComplete`, rebuilds occupancy and starts the full face-cache and initial sky-light passes.
Each step calls `PROFSTEP("<name>")`, so the spike log names the phase. Order summary:
1. Climate sampling (4 frames) → 2. terrain columns (`generateColumn`, up to `genColsPerFrame` per call but only while the time budget allows; stops if the next column would not fit the remaining budget) with cave simulation prepared alongside →
3. `caves` carve → 4. structure phases in order: **lakes → trees → ores → cactus → springs → plants → dungeons** (overworld) or **Nether features** → `closing`. Each phase is itself incremental.
Lakes are split in two calls (compute, then write) and share one static result buffer guarded by an owner pointer.
- During generation `setBlockDirect` recalculates nothing; writes into already-complete neighbours only schedule a refresh.
- `generationComplete` chunks are not saved until modified. Generated structures are reproducible from the seed.

### 7.4 Lighting

Sky light + block light, 4 bits each, flood-fill BFS across chunks via `ChunkManager`: queues of `LIGHT_QUEUE_CAPACITY = 4096` nodes, `MAX_FLOOD_STEPS_PER_CALL = 400` for block light and
`SKY_FLOOD_STEPS_PER_CALL = 80` for sky light (leftover sky work stays in `skyPending` and continues next frame via `continueSkyFlood()`). Public API: `propagateBlockLight`, `removeBlockLight`, `propagateSkyColumn`, `relightNewAirCell`, `removeSkyLightAt`, `seedSkyRange`. Removal uses two-phase propagation. The brightness curve is the exact
`WorldProvider.generateLightBrightnessTable`. Lighting is **baked into vertex colours**. Day/night uses `skylightSubtract` (0–11) computed like `World.calculateSkylightSubtracted`.
Initial sky light is done in two phases (A: straight columns, B: seeding at height differences), a few columns per frame. Emitters (torches, furnaces…) are re-seeded when a chunk loads (`reseedEmitters`).

### 7.5 Meshing and drawing

- **Occupancy bits** (`occupancy[x][z][4]`) give fast per-column visibility; `faceCache` stores which faces of a block are exposed. Both must be kept current on every edit (missing faces were caused by stale occupancy).
- **Display lists:** each chunk is split in vertical **bands of `DL_BAND_HEIGHT = 2`** (64 bands) and each band is built in two x-halves (`dlBands`, `dlBands2`). Building is incremental:
  time budget (~4 ms per frame) shared across all chunks; passing a pointer budget through `ChunkManager::drawAll → Chunk::draw → continueDisplayListRebuild`.
  If the chunk is edited mid-rebuild, `displayListDirtyDuringRebuild` restarts from band 0; use `markDisplayListDirty()` instead of setting flags directly.
- **LOD:** distant chunks use simplified top faces (`drawLodTopQuad`); some blocks are excluded (tall grass, torches, cactus, leaves, snow layer, dead bush).
- **Culling/budgets:** bands outside the field of view are skipped (`BAND_R` is computed for 8×2×8 bands; recompute it if band size changes); `vertBudget` limits vertices sent per frame; back-face culling only inside bands.
  `Options::renderQuality` (high/balanced/fast) lowers near detail and the vertex budget.
- Polygon formats: terrain culled/both-sided/water (alpha 22 with `POLY_ID`) variants; glass and leaves draw both sides.

### 7.6 Saving

Per-chunk files `<world folder>/mc_chunk_<chunkX>_<chunkZ>.dat` (a legacy seed-named variant `mc_chunk_<seed>_<x>_<z>.dat` exists in code), written only for chunks that were modified **and** fully generated (`isModified()`; an incomplete chunk is never saved).
File format: raw `blocks[8][128][8]` (8192 bytes, one byte per `BlockType`) followed by `liquidMeta` (4096 bytes). Older files without the metadata tail load as zeros. Nothing else is stored: light, face cache, occupancy, biome and tints are **recomputed on load**
(`loadFromFile` → `continueLoad`, spread over several frames, because biome+colours cost 19–46 ms at once). A file whose layer y=0 is entirely air is treated as corrupt, deleted, and the chunk regenerates from the seed.
Unmodified chunks are never saved: they regenerate deterministically. Extra per-world files are listed in §14.

---

## 8. Blocks and items

- Blocks and items share `BlockType` (`unsigned char`, ids 0–255). **The enum is completely full: it has exactly 256 entries (the last is `SAPLING_BIRCH`).** The only reusable slot is `SPARE_ITEM` (a leftover from the old `JACK_E`).
  Orientation variants were already folded into metadata (pumpkin/jack-o-lantern direction, compass/clock/map reused old slots), and the comment at the armor section notes there was no room for chainmail.
  **Any new block or item requires freeing slots first**: e.g. use `SPARE_ITEM`, merge variants into metadata (the way `WOOL_*` or `DYE_*` could be), or change the underlying type to `unsigned short` — which affects the `blocks[][][]` array (8 KB → 16 KB per chunk), save files, inventory and many tables, so it is a major, RAM-costly change. Always tell the user this before proposing a new block/item.
- Adding a block/item (checklist): (1) append to the enum, (2) add entries in each per-type table in `BlockType.h` (name, hardness, tool, solidity/transparency, light emission, drop, stack size, durability…),
  (3) texture cell in `terrain.png` or `items.png` (and the texture-cell lookup function), (4) rendering path (cube, cross quad, partial block, flat item…), (5) recipe in `Recipes.h`,
  (6) sound group in `Sound::groupOf` if it should not be grass, (7) behaviour (placement rules, block updates, drops).
- **Metadata:** each chunk has one shared 4-bit-per-cell table, `liquidMeta[LIQUID_META_BYTES]` (`8*128*8/2` = 4096 bytes), read with `Chunk::getLiquidMeta` and written through `setBlockWithMeta`/`setBlockMeta`/`setMetaQuiet`.
  Despite the name it is the general metadata of the port: liquid level (0–7, +8 falling), crop growth, farmland moisture, door/bed/trapdoor/sign/ladder orientation, redstone power/state, piston direction, rail shape, slab/stairs variant, cake slices, sapling stage, leaf marks.
  Values above 15 do not fit; "bit 8" flags (lever on, button pressed, piston extended) are within the 4 bits (0–15). Other per-cell data lives outside the chunk: note-block pitch (`NoteBlocks`), chest/furnace/dispenser contents (keyed by global cell), sign text (`SignManager`), moving piston blocks (`Pistons.cpp` list).
  Furnace, dispenser and torch orientation are encoded in block *types* (e.g. `FURNACE_W/N/E`, `FURNACE_LIT_*`, `DISPENSER_W/N/E`, `TORCH_W/E/N/S/F`). Follow whichever pattern the neighbouring block uses.
- Tool tiers, durability and break times follow Beta (`pickaxeTier`, hardness in ticks, `reduceDurability` semantics from `ItemStack.damageItem`).
- `isFlatInHand(type)` / `usesItemAtlas(type)` select flat item sprites vs 3D blocks in hand.

---

## 9. Entities and physics notes

- Mob managers cap their population (e.g. few animals per chunk neighbourhood, squids, hostiles by light level and difficulty) and delete mobs whose chunk unloads. **Animals and hostiles are not saved** with the world (tamed wolves are, in `wolves.dat`).
- AI headers are templates over a small "world" interface so the physics can be reused by `*Manager.cpp`. They use `double` for physics math; they are one of the remaining costs (`t mobs` in the profiler).
- Drawing mobs: each part is `glPushMatrix`/transform/immediate-mode quads (`ModelBox`). A known future optimisation is display lists per model and distance culling.
- Explosions, projectiles, TNT, falling blocks, fishing, boats and minecarts each have their own manager with its own `tick` called from the 20 Hz logic section and `draw` called from the render section of `main.cpp`.
- Health rules (`Health.h`): armor, invulnerability frames, knock-back, drowning, fall damage. Difficulty: peaceful (no hostile spawns, the player still takes damage and regenerates 1 heart/s), easy, normal, hard (no natural regeneration).

### 9.1 AI headers (`SheepAI.h`, `AnimalAI.h`, `HostileAI.h`)
- Header-only templates over a **world adapter** (`CMWorld` for animals, `HWorld` for hostiles). Adapter interface: `type(i,j,k)`, `light`, `chunkReady`; hostiles add `rawSky/totalLight/blockLight/lineClear/canSeeSky/isDay`. A **Sink** receives side effects: `playerHit`, `addArrow`, `addFireball`, `explode/explodeAt`, `splitSlime`, `mobHit`, `heldKind`.
- Physics: per-axis move in order **Y, X, Z** against cell boxes (`EPS 1e-7`); gravity 0.08, drag 0.98, ground friction 0.546 (ice 0.98*0.91), water drag 0.8, lava 0.5, jump 0.42. Mobs use aligned cells (cell `j` covers `[j-0.5, j+0.5]`, see §6).
- Pathfinding: A* with `PATH_MAX_NODES 200`, `PATH_TABLE 1024`, `PATH_MAX_EXPANSIONS 80` per call, `Animal::MAX_PATH 64`. `pickPath` tries 10 random destinations (±6/±3/±6) weighted: animals prefer grass (10) else light-0.5; hostiles prefer dark (0.5-light).
- Randomness: tiny LCG `Rng`; slimes use `JavaRandom` + `isSlimeChunk` to match Beta chunk selection.
- Spawn helpers: `findSpawn` (30 attempts in a 5×5 chunk area, grass, light ≥ 0.30, ≥ 14 blocks from player), `packSpot`, `shouldDespawn` (more than 3 chunks away), `canSpawnHere`.
- Per-kind setup: zombie/pigman speed 0.5, attack 5; spider 1.4×0.9, speed 0.8; slime HP = size²; ghast 4×4, HP 10; wolf 0.8, HP 8, speed 1.1; squid 0.95, HP 10. Skeleton arrow: attackTime 30, speed 0.6, spread 12. Creeper: fuse 30 ticks, explosion size 3. Wolf bite: range 2.0, attackTime 10. Ghast fireball at attackCounter 20. `arrowTick`/`fireballTick` move projectiles.

### 9.2 Managers: spawning, caps, explosions
- Animal spawn timer 300 + rand(200) ticks (first at 200); weights cow 8 / pig 10 / chicken 10. Hostile spawn attempt every 10–19 ticks (first at 40). `KIND_CAP = {4,4,4,4,6,4,3,4,3}`; category caps `MONSTER_CAP`, `WOLF_CAP`, `SQUID_CAP`. Minimum spawn distance 12. Wolves spawn only in FOREST/TAIGA. Nether has its own lists. `setPeaceful` disables hostiles.
- Interaction: pig riding API; chickens lay eggs; lightning turns pig → zombie pigman; pigmen anger radius 32; wolf: bone tames (1/3 chance), pork heals 3, use toggles sit. `sleepSpawn` and `spawnerSpawn` are entry points for beds and monster spawners.
- Tamed wolves persist in `wolves.dat` (magic `'MWLF'`, v1, max 8 entries: x,y,z doubles, yaw float, health int, sitting byte).
- Explosions (`HostileManager`): rays over a 16³ surface, block `resistanceOf`, unbreakable list, 30% drops, TNT chain reaction, optional fire placement (ghast fireballs); `MAX_EXPLOSIONS 24` queued. Hit queue `HIT_QUEUE 8`; drop queues animals 16 / hostiles 24 (shared budgets, not per-mob).

### 9.3 Liquids, particles, partial blocks
- `Liquid.cpp`: scheduled-tick queue (`CAP 640`, entries `{gx,gy,gz,type,used,due,seq}`), deduplicated by (cell,type), `popDue` ordered by `due` then `seq`, bounded by `s_nextDue`. API: `resetTicks/advanceTime/setHell/isHell/pendingCount/pushTick/popDue`. Flow rules live in the block code that pushes ticks.
- `Particles.cpp`: kinds SMOKE, EXPLODE, SNOW, FLAME, SPLASH, LAVA, BUBBLE, HEART, REDDUST, DIG. Fixed pool replacing the oldest; 14 dig particles per break (Beta uses 64). Ambient emitters sample 48 random points within ±8 (torches, fire, lava, lit furnaces). Two draw passes (flat colour; `terrain.png` 4×4 fragments), `POLY_ID(46+pass)`, cull distance 40, light sampled every 5 ticks. Everything is gated by `Options::particles`.
- `PartialBlocks.cpp` (global-cell coordinates) holds `validate()` support rules (doors, bed, sign, cake, crops, farmland, ladder, fence, trapdoor) plus the activation/use/tick logic. Metadata in `liquidMeta`: door bit 8 = upper half, bit 4 = open; trapdoor bit 4 open + `meta&3` direction; bed `dir&3` + bit 8 = foot; fence bits 1/2/4/8 = connections; sign post 0–15, wall 2–5; crops 0–7 (growth formula in `cropsTick`); farmland 0–7 (wetness); cake 0–5; jukebox 0 empty / 1 / 2 disc. Also: hoe, seeds, bone meal, cake eating, trampling (`mobStep`), `boxHit`/`rayVsBox` for non-full hitboxes.
- `PauseScreen.cpp`: bitmap on the sub-screen BG3 (8-bit paletted if `pauseMenuPalLen` exists, else 16-bit, maybe LZ77). Touch rects x 51..204; y 38–63 BACK, 68–93 OPTIONS, 98–123 SAVE, 128–153 QUIT (same layout as the main menu image).

### 9.4 Minecarts, boats, TNT
- **Coordinates:** `MinecartManager`/`BoatManager` work in Beta **grid** coordinates; world = grid − (4, 1.5, 4) (`OX=4, OY=1.5, OZ=4`). `CartWorld` adapter gives `type/meta/normal/solid/ready`; a cart whose chunk is not ready stays frozen. Unloaded cells read as STONE.
- `MinecartAI.h`: port of `EntityMinecart.onUpdate` (width 0.98, height 0.7, `Y_OFFSET 0.35`, max speed 0.4, slope accel 1/128). Rail shape comes from `RailLogic.h` (`Rails::endpoints(meta)`, `hasPower`, `isRailAt`). Powered rail: bit 8 = on (accelerates +0.06, or kick-starts at 0.02 against a block); off = brakes. Furnace cart (type 2): `fuel` (+1200 per coal, spent 1 in 4 ticks while pushing), `pushX/pushZ` direction. `renderPose` reproduces `RenderMinecart` (rail-following yaw/pitch). Types: 0 empty (rideable), 1 chest (inventory in `ChestStore` with key `CHEST_KEY_Y`), 2 furnace.
- `MinecartManager`: `MAX_CARTS` slots, `s_riding` index, drop queue 48, damage > 40 breaks the cart (drops cart + chest/furnace + contents), cart-vs-cart push (`cartCollision`), player pushes carts, `pick`/`attack`/`interact` (returns `INTERACT_MOUNTED/CHEST/COAL_USED`), `takeForcedDismount`, **`carts.dat`** (`'MCRT'`, v1, `s_nextUid`, count, then per cart: uid, type, x/y/z doubles, yaw float, flipped, fuel, pushX/pushZ doubles).
- `BoatAI.h`: port of `EntityBoat.onUpdate` (1.5×0.6, `Y_OFFSET 0.3`): buoyancy from 5 water slices (`boxInWater`, uses `Liquid::percentAir`), rider pushes `motion += riderMotion*0.2`, clamp 0.4, drag 0.99/0.95/0.99, turns ≤ 20°/tick toward motion, **breaks (3 planks + 2 sticks) if it hits something sideways above speed 0.15**. Reuses `MinecartAI` collision helpers (`calcX/Y/Z`).
- `TntManager` (`MAX_TNT` slots): primed from a click (`primeCell`, fuse 80) or from a chain explosion (`primeFromExplosion`, fuse 10–29); gravity 0.04, drag 0.98, ground friction 0.7; explodes with strength 4 through `HostileManager::explodeAt`; if no slot is free it explodes immediately. Draw: textured cube (`getBlockTextures(TNT)`), grows ×1.3 in the last 10 ticks and flashes a translucent white cube every 5 ticks (`POLY_ID(41)`).

### 9.5 World-generation pieces
- **Caves (`Caves.cpp`)**: port of `MapGenBase`+`MapGenCaves` validated against the decompiled Java (108 chunks, 3 seeds, 0 differing blocks). The world is split in 16×16 **cells**; each cell's tunnels are simulated incrementally (`STEPS_PER_CALL = 40`, one job at a time) into compact `Sphere{x8,y8,z8,r16,flat}` records stored in a shared pool (`POOL_BLOCKS` 80 × 256 spheres = 160 KB; `WINDOW` 32×32 toroidal cell cache; LRU eviction; max 24 blocks/cell). `Caves::prepare(chunkX,chunkZ,workDone)` returns true when every cell within `REACH_CELLS` is ready; `carveStep` then carves spheres into the chunk with a cursor (resumable, `maxCarves` per call). Caves below y=10 fill with **lava**; carving is skipped if water is in the box. `CAVES_RADIUS_SCALE`, `CAVES_ORIGIN_Y_SCALE` (in `Caves.h`) tune size/height. `beginFrame()` advances the LRU tick; stats: `cellsSimulated/cellsTruncated/evictions/blocksFree`.
- **Lakes (`Lakes.h`)**: pure logic (`Lakes::build` computes a 16×16×8 blob shape with `java.util.Random`, validates the shell; `Lakes::apply` writes liquid below y+4, air above, grass on exposed dirt, stone shell for lava). Templates `T` (terrain query) and `W` (writer); each chunk evaluates lakes starting in neighbours and writes only its own part, so there are no seams. `JRandom::skip(n)` fast-forwards the LCG.
- **Trees (`Trees.cpp`)**: `genRound` (oak base height 4, birch 5), `genSpruce` (WorldGenTaiga2), `BigTree` (WorldGenBigTree, 1 in 10 oaks). Uses a local xorshift `Rng`, not Java's. Saplings: `saplingTick` (needs light ≥ 9, 1/30 chance; first stage sets meta bit 8, second stage grows) and `grow`. Leaves: any log/leaf removal marks nearby leaves with **meta bit 8** (`onBlockRemoved`, radius 4 for logs, 1 for leaves); `leafTick` runs a 9×9×9 distance flood (≤ 4 steps to a log) and decays leaves without support (1/20 drop a sapling). It refuses to run unless the four corner chunks ±5 are loaded.
- **Weather (`Weather.cpp`)**: port of `World.updateWeather` (rain/thunder timers in ticks, `rain`/`thunder` levels move 0.01 per tick). Only the Overworld is active. Precipitation columns: a 9×9 ring (`R = 4`) refreshed 24 columns per tick; biome kind: TAIGA/TUNDRA snow (2), DESERT none (0), others rain (1). Draws rain/snow as thin quads (`POLY_ID(40/41)`), lightning bolt as 3 translucent layers (`POLY_ID(42+layer)`), rain splashes via `Particles::SPLASH`, rain sound via `Sound::rain`. Lightning: `LIGHTNING_PER_100K = 250` per tick while thundering, column within ±40 that sees the sky; sets fire via `cm.ignite`. Snow layers form and still water freezes in snowy biomes (block light < 10). Persisted in **`weather.dat`** (magic `0x57544831` "WTH1", 6 ints + 2 floats). `debugToggle()` is the debug-screen Y key.

### 9.6 Audio internals (`Sound.cpp`)
- `soundPlaySample` with 8-bit clips from `SOUND_CLIPS` (`SoundData.h`) and ambient clips `AMB_CLIPS` (`SoundAmbient.h`). Every sound goes through `playClip`, which checks `Options::sound`, applies a **per-clip 3-frame anti-spam filter** (`s_frame` advances in `setListener`, called once per game tick), clamps volume (≥ 0.02), pan (0–127) and rate (4000–32000 Hz).
- Positional sounds (`at`) fade linearly to 0 at `RANGE = 16` blocks and pan by projecting onto the camera's right vector (×0.8). `groupOf(BlockType)` maps blocks to step/dig groups (grass, wood, stone, sand, gravel, snow; dirt/farmland use the gravel group as in Beta; sand/gravel/snow have no dig clips, so step clips are reused). Mobs: `mobClip(kind)` maps kind ids (0 cow, 1 pig, 2 chicken, 200 sheep, 100–107 hostiles) to SAY clips; HURT is always SAY+1 in `SoundData`. Squids make no sound.
- Special cases: `uiClick()` bypasses the frame filter (the frame counter does not advance in menus); `thunder` resets the filter and plays thunder + a low-pitched explosion; `rain` alternates two clips and is quieter/lower-pitched when covered; `doorCell`/`clickCell` convert grid → world (`gx-4+0.5, gy-1, gz-4+0.5`).

### 9.7 Inventory and crafting UI internals
- `Inventory`: `TOTAL_SIZE` normal slots (hotbar 0–8, bag 9–35), armor slots at `ARMOR_BASE` (`ARMOR_SLOTS` = 4: helmet, chest, legs, boots), `SLOT_COUNT` total. `addItem` first tops up the **fullest** existing stack of the same type, then fills empty slots (stack size `maxStackFor`). Items with durability never stack or merge (`hasDurability`); `swapOrMerge` also enforces that armor slots accept only the right piece. Armor: `getTotalArmorValue() = (sum(reduce)-1)*durabilityLeft/maxDurability + 1` over worn pieces; `damageArmor(d)` wears each piece by `max(1, d/4)`. `reduceDurability` returns false when the item breaks.
- `InventoryScreen`: bitmap background `inventoryMenu` (LZ77) on sub-screen BG3 (16-bit, bit 15 forced to 1), icons are drawn **by CPU into the framebuffer** from `terrainBitmap`/`itemsBitmap` palette indices (24×24 px, nearest-neighbour stretch; leaves tinted; button/plates squashed; `iconShapeVisible` gives slab/stairs/fence silhouettes; compass/clock use `AnimItems::pixelOverride`). Slot grid: x = 4 + col·28; hotbar y = 4, bag y = 41 + row·28, armor row y = 133. 4×6 digit font for counts, durability bar (green→red). `getTouchedSlot(x,y)` is the hit test.
- `CraftingScreen`: lists **only craftable recipes** (`canCraft`) in original item-id order (`ItemOrder`), 36 per page (4 rows of 9, `PAGE_SIZE`), prev/next/craft buttons at y 2–28 (x 2–61, 66–125, 194–253), page indicator at (138,10). `requiresTable` recipes are hidden unless a table is used. The top screen shows the selected recipe's pattern (2×2 `topcrafting1`, 3×3 `topcrafting2`; cells 28 px apart). `showFurnaceTop`: `topfurnace2` idle / `topfurnace1` lit; flame (rows 82–107) and arrow (cols 127–158) are magenta placeholders repainted every frame by `updateFurnaceAnim` (only those two rectangles are redrawn to avoid flicker). `showChestTop` draws chests (9×3 from y=56, 9×6 from y=14, `normalchest/largechest`) and the dispenser (3×3 at x=88, y=56, `dispensergui`).
- `NewWorldMenu`: bitmap `createWorldMenu` (8-bit paletted if `createWorldMenuPalLen` exists, else 16-bit; chosen by the preprocessor). Text fields are redrawn into the bitmap with `MenuFont` (name box x 66–189, y 48–73; seed box y 105–130); buttons at y 162–190 (left half = create, right half = cancel); START creates, B cancels; fields open `TextKeyboard::edit`. `getSeed` returns false when the seed field is empty (random seed).

### 9.8 More internals (Nether, sky, saves, menus, chests, notes)
- **Nether generator (`NetherGen.h`)** is a *reduced* version of `ChunkProviderHell`: pure function of the column (`computeColumn(gx,gz)` → `Col{floorTop, ceilBottom, pillar, surface}`), using the overworld 2D Perlin instead of 40 octaves of 3D noise. Noise fields (floor, ceiling, pillar, two surface patches) are sampled on a 4-block lattice cached in a 16×16 `LatCache` and bilinearly interpolated; call `resetLattice()` when the seed changes. Floor = `FLOOR_MID 40 ± FLOOR_AMP 34` clamped 8–100; ceiling hangs from `CEIL_TOP 118` by up to `CEIL_DROP 66` (never closer than 8 above the floor, max 118); pillar columns are solid (`n3 > 0.62`); soul sand (surface 1) / gravel (2) patches only where the floor is at y 60–65; lava fills air below `LAVA_LEVEL 32`; bedrock 1–5 thick at the bottom (y ≤ 4) and top (y ≥ 123) via `Noise::hashInt`. Glowstone: `glowCluster` (1500 attempts in a 15×12×15 local volume, port of WorldGenGlowStone1/2) and fire: `firePatch` (64 attempts). Both take a `Lakes::JRandom`.
- **`Noise.cpp`**: only hash-based value noise (`hashInt`, `valueNoise2D`, `biomeNoise2D` with a +10000 offset); `Noise::setSeed` sets the global seed used by every hash. Real terrain noise is `PerlinNoise`.
- **Sky (`Sky.cpp`)**: sun and moon are textured quads at distance 100 scaled by `K = 0.3*16` (→ 30 units, because the DS far plane is 40; vertices are divided by 16 to fit v16 ≤ 8). Alpha = `30*(1-rain)`; sun hidden when `c < -0.1`, moon when `c > 0.1`; sun `POLY_ID(41)`, moon `POLY_ID(42)`; the sunrise glow fan (`POLY_ID(40)`) only compiles with `SKY_GLOW 1`. `SkyTex.h` holds 32×32 16-colour bitmaps (colour 0 = transparent; the originals use additive black).
- **Clouds (`Clouds.cpp`)**: bits from `CloudsData.h` (`CLOUD_BITS`, 256×256 cells, 8 words per row); cell size 12 blocks, thickness 4, base y ≈ 107.83, drift 0.03 blocks/tick (`Clouds::tick`), draws a (2R+1)=11 cell square around the player, merging runs along X into one box; side walls only within 3 cells; colour depends on sky brightness, rain and thunder; `POLY_ALPHA(25)`, `POLY_ID(44)`, no fog. **Only drawn from the ground in dsi mode**; otherwise only when the eye is above y = 80.
- **Saves (`WorldSave.cpp`)**: `CURRENT_FORMAT_VERSION = 5`; `loadMetadata` rejects other versions (`WorldMetadata` is written as a raw struct, version field overwritten at offset 0; fields after `armorType` were added later). Slots `world1…world100`; `.tempworld` holds a **new, never-saved** world; `beginSessionNew` wipes it and picks the first free slot; `beginSessionLoad` plays an existing world in place (never renamed); `commitSession` replaces `worldN` with `.tempworld` (rename); `endSession` returns an existing world's folder if it was parked in `.tempworld`; `recoverInterruptedSession` promotes a leftover `.tempworld` containing `meta.dat` to a free slot. Nether data lives in `DIM-1/` of the world folder (`setActiveDimension(-1)`, `ensureDimensionFolder`); `getBaseFilePath` ignores the dimension (used for world-wide files such as `weather.dat`, `name.dat`). `name.dat` is plain ASCII (printable only, ≤ ~39 chars read).
- **Menus**: `SubScreenMenu` (main menu bottom screen): `menu_nosave` 8-bit paletted LZ77 bitmap; Play y 38–63, Options y 128–153, x 51–204 (Skins/Texture Packs buttons exist in the image but are inert). `WorldSelectMenu`: 16-bit bitmap (`selectWorld` or `selectWorldNoWorld`), name box x 65–191, y 108–127… 145; buttons y 162–190: Create x 0–62, Delete 66–125, Play 130–189, Back 194–253; arrows x 0–64 / 192–255, y 51–113; list = existing `worldN` slots (max 100). `TextKeyboard::edit(title, buf, maxLen≤30, numeric, menuTick)`: a console-based on-screen keyboard (8×8 cell grid, keys as `[ x ]` text) with Shift/Back/Space/OK/Cancel (numeric layout: keypad with Back and Clear); START confirms, B cancels, trailing spaces trimmed, every key plays `Sound::uiClick`; blocking loop that keeps the menu panorama ticking when `menuTick`.
- **Controls (`Controls.cpp`)**: assignable keys `UP DOWN LEFT RIGHT A B X Y L R SELECT none`; 13 actions in order FORWARD, BACK, STRAFE_LEFT, STRAFE_RIGHT, JUMP, BREAK, PLACE, SNEAK, THIRD, LOOK_L/R/U/D. Defaults: *normal* = D-pad move, A jump, R break, L place, B sneak, Y third person; *xyba* = D-pad move, R break, L place, camera on Y/A/X/B. `bind` **swaps** with whichever action already used the key. `gameHeld(held)` remaps the held mask so the rest of the game reads a canonical UP/DOWN/LEFT/RIGHT/A.
- **Chests**: `Chest.h` has no stored orientation; front face and left/right half of a double chest are derived from neighbours at draw time (`sideIndex`, terrain.png cells 25–27, 41/42 front of double, 57/58 back); `canPlaceAt` forbids triple chests. `ChestStore` keeps up to `MAX_CHESTS` entries keyed by grid cell `(gx,gy,gz)` with `SLOTS` slots of `{type u8, count u8, durability u16}`; minecart chests reuse it with the cart's uid as `gx` and `MinecartManager::CHEST_KEY_Y` as `gy`. `transferWithInventory`: move whole stack into an empty slot, merge stackables, otherwise swap. File `chests.dat`: `'CHST'`, v1, count, then per chest gx,gy,gz (int32 LE) + slots × 4 bytes.
- **Note blocks (`NoteBlocks.cpp`)**: state table `MAX_NOTES` (gx,gy,gz,note 0–24,prev power); instrument chosen from the block below (`instrumentFor`: stone-like → bass drum, sand/gravel/soul sand → snare, glass/glowstone → clicks, wood-like → bass guitar, else harp); pitch = 2^((note−12)/12). Sound uses the DS **PSG/noise channels** (not samples), up to 8 voices stopped by `tickSound()` after 2–6 ticks; volume fades to 0 at 32 blocks. Needs air above. `notes.dat`: `'MNOT'`, v1, count byte, then gx,gy,gz int32 + note byte (only non-zero notes are saved).
- **Mob–mob collision (`MobBody.h`)**: each manager exports `MobBody` pointers (x,y,z,velocity,w,h); `MobBodies::collide` pushes every overlapping pair apart (0.05 velocity, stronger when closer, box inflated by 0.2).
- **Sleeping (`Sleeping.h`)**: eye position/orientation/model shift per bed direction, `nearestEmpty` (first cell with solid floor and 2 air above in the 3×3 around each bed half), overlay `0x101020` up to alpha 220 (`FULL_ASLEEP 100`, fades until 110).
- **Mob spawners (`Spawners.cpp`)**: up to `MAX_ENTRIES = 8` active spawner blocks tracked at once. Every 20 ticks it scans the chunks within 16 blocks of the player (±17 in Y) for `MOB_SPAWNER` and registers those within 16 blocks; entries drop out when the block disappears or the player leaves range. The mob kind is the block's `liquidMeta` (0 zombie, 1 skeleton, 2 spider). Per entry: smoke + flame particles every 2 ticks, spin `1000/(delay+200)` degrees per tick, and when `delay` reaches 0 it tries 4 spawns through `HostileManager::spawnerSpawn(cm, kind, x, y, z)` (negative result = blocked → retry in 200–799 ticks; positive = spawned → `Particles::spawnerBurst` and new delay 200–799). `draw` renders a miniature rotating mob (`HostileModel::draw` with a temporary `Mob`, scale 7/16, tilted −30°) inside the cage. Initial delay is 20 ticks.
- **Fixed screens**: `StartupSplash` (Eagtek logo then Mojang logo, 60 frames each, 8-bit LZ77 bitmaps decompressed straight into VRAM; the bottom screen stays white until `finish()` restores the console). `GameOverScreen`: `gameover` 8-bit LZ77 bitmap on the sub screen; Respawn button x 51–202, y 83–104; Title menu y 129–150. `LoadingScreen`: `genericbg` (sub, 16-bit) and `topgenericbg` (top, LZ77 16-bit) with bit 15 forced to 1; `showTopDirtOnly()` redraws only the top screen.

---

## 10. UI and screen programming

- **Top screen** (main engine): 3D during play; in the menu either the 3D panorama (`MenuPanorama`) with the logo as an 8-bit BG layer in front, or a static 16-bit background. Switching to/from the panorama frees the
  entity textures from VRAM (`Texture::unloadEntityTextures`) because the panorama needs 32 KB contiguous.
- **Bottom screen** (sub engine): the touch UI (hotbar, inventory, crafting, furnace/chest views), pause/game-over images, and the **text console** used by Options and the debug screen (`consoleDemoInit()` re-initialises it;
  call it after anything that changed the sub video mode). Touch zones are measured on the source PNGs (`GameplayHotbar.cpp` etc.).
- Menu images: 8-bit paletted (`-gb -gB8 -p`, optionally `-gzl`), copied/decompressed straight into VRAM and palette RAM; they must not stay in RAM.
- Controls: `Controls` keeps two profiles (**normal** = touch camera, **xyba** = button camera), each mapping 13 actions to buttons; saved in `controlsprofile.dat`.
- UI sounds: `Sound::uiClick()` is called by every button (menus, pause, game over, options, keyboard, crafting buttons, mini-buttons); not for hotbar/inventory slots.
- Startup: `StartupSplash::show()` (logos, bottom white) → loading continues with the screens frozen → `StartupSplash::finish()` right before `SubScreenMenu::init()`/`TopScreenMenu::init()`.

---

## 11. Performance playbook (lessons learned — follow them)

1. **Measure first.** Use the profiler/`spikes.txt`; do not guess. A spike line tells you the subsystem and the heaviest named step.
2. **Budgets must be shared.** Per-chunk caps multiply; use a per-frame budget passed by pointer (as `vertBudget` and the band rebuild budget are).
3. **Split big steps by position or phase** (halves of a band, columns, lake build vs. write, border refresh with a cursor) and keep **consistency**: if an edit happens between slices, mark the work dirty and restart, otherwise the result is stale.
4. **Don't start work that doesn't fit:** estimate the cost from the previous item (as in the column loop) before beginning another.
5. **Avoid floats and `double` in per-block loops;** avoid per-quad heavy matrix pushes; prefer display lists for static geometry.
6. **I/O in small pieces** (one chunk file per frame per queue).
7. **Mind RAM:** pool objects, free display lists outside the window, share static buffers, use `-Os` for cold code, compress menu images.
8. After any optimisation, ask for a new `spikes.txt` and compare. Typical remaining hot spots: band rebuild (`list`), neighbour border refresh (`neighbors`), ore/lake generation, mob logic (`t mobs`), mob drawing (`ent`), chunk save (`chunk save`, FAT I/O).
9. Real hardware matters: the GPU silently drops polygons/vertices over the limit ("slivers"/missing faces); emulators such as no$gba hide limits that melonDS and real DS Fat show.

---

## 12. Rendering, textures and models

### 12.1 Projection, fog, VRAM banks (`Renderer.cpp`)

- `Renderer::init()`: bank **A and D = textures** (`VRAM_A_TEXTURE`, `VRAM_D_TEXTURE`, 2 × 128 KB), **F and G = texture palettes** (slot 0 and 1), main engine `MODE_0_3D`, antialiasing off, fog on. (Bank B = top-screen menu BG, bank C = sub BG.)
- `setupProjection()`: `gluPerspective(70, 256/192, near 0.1, far 40)`, default polygon format `POLY_ALPHA(31) | POLY_CULL_NONE | POLY_FOG`. Fog is derived from `WorldConfig::renderDistance()`: starts at 60 % and is full at 85 % of it.
  The hardware depth is 15-bit and non-linear, so offsets are computed with the same formula as the projection; `glFogShift` is picked as the finest step that still covers the range (32 fog steps). `glFogColor` is overwritten every frame in `main.cpp` to match the sky.
- The hand uses its own projection (near 0.05, far 80) with the depth range squashed to 1/8 (`kDepthSquashMatrix`) so it always draws in front of the world — this emulates the depth-buffer clear the original does before drawing the hand.

### 12.2 Texture loading (`Texture.h/.cpp`)

All textures are **paletted** (`GL_RGB256`, or `GL_RGB16` for the 16-colour ones) and uploaded with `glTexImage2D` + `glColorTableEXT`. The generated grit headers provide `<name>Bitmap`, `<name>Pal`. `glTexImage2D` returns 0 when VRAM does not fit: every loader checks this.
`GL_TEXTURE_COLOR0_TRANSPARENT` makes palette index 0 (magenta key) transparent.

| `Texture::` ID | Source | Size / format | Transparent |
|---|---|---|---|
| `textureID` (terrain) | `terrain.png` | 256×256, 256 colours | yes (alphakey) |
| `itemsTextureID` | `items.png` | 256×256, 256 | yes |
| `iconsTextureID` | `icons.png` (crosshair, hearts, armor, bubbles) | 256×256, 256 | yes |
| `skinTextureID` | `char.png` (player skin; height 32 or 64 chosen from `charBitmapLen`) | 64×32 or 64×64, 256 | no |
| `sheepTextureID` | `sheep.png` (skin rows 0–31, wool rows 32–63) | 64×64, 256 | no |
| `cartTextureID`, `boatTextureID` | `cart.png`, `boat.png` | 64×32, 256 | no |
| `cowTextureID`, `pigTextureID` | `cow.png`, `pig.png` | 64×32, 256 | no |
| `chickenTextureID` | `chicken.png` | 64×32, 256 | yes (legs) |
| zombie, skeleton, spider, spidereyes, creeper, pigzombie, ghast, ghastfire, slime, squid, wolf, wolfangry, wolftame, sign | `*.png` | 64×32, 256 | yes |
| `signFontTextureID` | **generated at boot** from `MenuFont` glyphs (4×6 per char, index = c − 32) plus the two arrow-model pieces (shaft 16×5 at (0,20), cap 5×5 at (0,26)) | 128×32, **16 colours** | yes |
| `paintingTextureID` | `PaintingTex.h` (reduced `kz.png`) | 128×128 (or 64×64 with `PAINTING_TEX_64`) | no |
| `skySunTextureID`, `skyMoonTextureID` | `SkyTex.h` | 32×32, 16 colours (512 B each) | yes |

- **VRAM is full.** That is why: armor on the player is drawn as flat-coloured polygons (`SteveModel`, shapes from `ArmorShape.h`), the held map and its parchment are flat colours (`HandAnimations`, `MapBg.h`), and the sign font and arrow share one atlas. Paintings and the sun/moon are *optional*: if they do not fit they degrade silently
  (white paintings / no sun). Never assume a new texture fits: check the return value and give the feature a fallback. Add one only after freeing another.
- Loaded once at startup: terrain, skin, items, icons (never unloaded). **Entity textures** (sun, moon, painting, sheep, cart, boat, cow, pig, chicken, all hostiles, sign, sign font — 24 IDs) are freed by `unloadEntityTextures()` while the 3D menu panorama needs 32 KB of contiguous VRAM,
  and reloaded by `reloadEntityTextures()` (sheep → cart → boat → animals → hostiles → sign font → sky → painting). **If you add an entity texture, add its ID to the `ids[]` array in `unloadEntityTextures` (and bump its size) and to the reload chain**, otherwise it leaks VRAM or stays missing after returning from the menu.

### 12.3 Texture coordinates

- Terrain/items atlas cells are 16×16 texels, addressed `TextureCell {col, row}`; texel UV = `col*16 … col*16+15.99`. Vertex texcoords are written as `GFX_TEX_COORD = TEXTURE_PACK(u, v)` with **12.4 fixed point texels** (`u*16`).
  Beta's `terrain.png` coordinate (x, y) in code comments means (column, row) in the 16×16 grid (e.g. fire = (15,1) and (15,2), portal = (14,0)).
- Models use `ModelBox` (below); `uvInset` (1/8 texel) shrinks each face to avoid bleeding, like `TexturedQuad`'s 0.1 texel.

### 12.4 Texture animation (`TextureAnim.*`, `AnimItems.*`)

Fire (cells (15,1), (15,2)) and the Nether portal (cell (14,0)) are animated by **writing into VRAM**: `TextureAnim::onVBlank()` is called right after `swiWaitForVBlank()`; every 3rd vblank (20 Hz) it switches banks A and D to `VRAM_*_LCD`, copies the next frame of a 32-frame strip onto the cell
(`TextureAnimCore::blitCell`, colours matched to the atlas palette by `buildMap`), updates the compass and clock cells in the items atlas (`AnimItems`), and switches the banks back to texture mode. Display lists are untouched because they already point at those cells.
`init()` needs `glGetTexturePointer(Texture::textureID)` to land inside `0x06800000–0x06880000`; otherwise terrain animation is disabled (compass/clock still work if the items texture is writable).
Consequence: the animated cells must exist in `terrain.png` at those coordinates and the animation data (`AnimData.h`, `AnimItemsData.h`) is generated, not hand-edited.

### 12.5 Graphics conversion (`graphics/*.grit`, Makefile)

- Atlases with transparency use `-gb -gB8 -p -gTFF00FF` (`terrain.grit`): 8-bit bitmap, palette, magenta as transparent colour (index 0). The Makefile adds `-fts` and runs `alphakey` first for terrain/items/arrows/icons.
- Full-screen menu images (e.g. `subcrafting.grit`) use `-gB16 -gb -gzl`: a 16-bit bitmap compressed with LZ77 for decompression straight into VRAM. Other menu images use 8-bit with palette (`-gB8 -p`, optionally `-gzl`).
- To add an image: PNG + `.grit` with the same base name in `graphics/`; include the generated `<name>.h`.

### 12.6 Mob models (`ModelBox.h`, `*Model.cpp`)

- `ModelBox::build()` is a port of `ModelRenderer.addBox` + `TexturedQuad`: given texture origin (tx, ty), corner (x,y,z), size (w,h,d) and inflate, it returns 6 faces × 4 vertices (float position in 1/16-block units, UV in texels). Face order: x2, x1, y1 (bottom), y2 (top), z1 (front), z2 (back).
  `vOffset` is used for the wool layer of the sheep (32 rows lower).
- Each `*Model.cpp` converts those boxes once at `init()` into a `Part { Quad quads[6]; pointX/Y/Z }` of **`s16` v16 vertices** (`position * 256`, so 1/16 block = 256) and **`s16` 12.4 texcoords** (`uv * 16`), so nothing is computed with floats per vertex at draw time.
  `mirror` reflects the vertices (left arm/leg reuse the right textures).
- Drawing a part (`drawPart`): `glPushMatrix` → `glTranslatef(point/16)` → rotations **Z, Y, X** (same order as `ModelRenderer.render`; `drawPartRot` uses Y, X, Z for the wolf's head parts) → 6 quads with `glVertex3v16` → `glPopMatrix`. This costs matrix operations per part, which is why mob drawing (`ent` in the profiler) is a known hotspot.
- `draw()` pattern (all mobs): interpolate position/yaw with `partialTick` → `glTranslatef(x, y, z)`, `glRotatef(180 − bodyYaw, Y)`, death tip-over, `glScalef(−1, −1, 1)` (the model's Y points down), `glTranslatef(0, −24/16 − 0.0078125, 0)` → bind texture → colour from `brightnessOf(lightLevel)` (`glColor3b`, red tint when hurt/dying, orange when burning) → body parts.
  Extra passes use `glPolyFmt(POLY_ALPHA(a) … POLY_ID(n))` for translucency: slime outer layer (alpha 20), spider glowing eyes (alpha from darkness), creeper white flash. Distinct `POLY_ID`s are needed so translucent layers do not cull each other.
  Burning entities call `drawFireOverlay` (stacked billboards using the animated fire cells (15,1)/(15,2) of `terrain.png`).
- Animation formulas are the original `setRotationAngles` (limb swing `cos(swing*0.6662)*1.4*amount`, etc.).
- Player model (`SteveModel`): 6 parts (head, body, 2 arms, 2 legs, all 6 faces drawn), scaled 0.9375, angles from `setAngles` (walking, riding, holding item, swing, sneaking); `tick()` runs at 20 Hz (body-yaw follow, limb amount); armor = flat-coloured `FlatQuad`s per material built from `ArmorShape.h` rectangles (two sets: inflate 1.0 for helmet/chest/boots, 0.5 for leggings);
  held items use `HandAnimations::renderItemThirdPerson`; sleeping and death use extra rotations.
- First-person hand (`HandAnimations`): port of `ItemRenderer`/`EntityRenderer` hand code. Per-tick state (`tick(TickInput)`): equip animation, swing (8 ticks), click timing, view-bobbing. `render(RenderInput)` draws either the arm, a 3D block (`drawBlockInHand`), an extruded 2D item (`drawItem2D`, 16 strips per side) or the map with two arms (`drawMapHeld`, ≤ 900 flat quads, falls back to a 32×32 grid when needed).
  Uses the DS hardware lights (`POLY_FORMAT_LIGHT0 | LIGHT1`) with `setupItemLights` for item shading (the only place where hardware lighting is used). `lightLevelAt(cm, x, y, z)` is the shared helper to sample light at a world position.
- Sky (`Sky.cpp`): sun/moon quads at 30 / 20 units (far plane is 40), alpha-blended with transparent colour 0 (`SKY_GLOW 0`: the sunrise glow code exists but is disabled; the DS has no additive blending nor per-vertex alpha).
- Translucent polygons (alpha < 31) do not write depth; water uses alpha 22 with `POLY_ID(22)`.

### 12.7 Mob managers (how data, AI and model connect)

- Data structs: `Animal` (`AnimalData.h`: position/motion as `double`, previous values for interpolation, yaw/bodyYaw/pitch, limb swing, size, health `HealthState hs`, path arrays up to `MAX_PATH = 64`, chicken wing/egg fields, `kind` = `KIND_COW/PIG/CHICKEN`),
  `Mob : Animal` (`HostileData.h`: `hkind` = `H_ZOMBIE … H_SQUID` (9 kinds), plus creeper, pigman, slime, ghast, wolf and squid state). `Sheep` is its own struct (`SheepAI.h`). `Arrow` and `Fireball` are separate small structs.
- AI headers (`SheepAI.h`, `AnimalAI.h`, `HostileAI.h`) are header-only code templated on a "world" adapter (`type(i,j,k)`, `light(i,j,k)`, `chunkReady(cx,cz)`); each manager defines `CMWorld`, an adapter over `ChunkManager` with a one-chunk cache (world cell i ↔ grid cell i+4; chunk = cell>>3).
  Because the physics and A\* are shared, `Mob` reuses `AnimalAI` functions.
- **`SheepManager` is the facade used by `main.cpp`** and fans out to `AnimalManager` and `HostileManager`. Mob indices are partitioned: `0 … MAX_SHEEP-1` = sheep, `ANIMAL_BASE …` = animals, `HOSTILE_BASE …` = hostiles (`pick`, `attack`, `anchor`, `pull`, `interact`, `shear`, `milk`, `saddle`, `mount`, `dye` dispatch by range).
  API: `clear(seed)`, `setActive(bool)` (animals only in the overworld; hostiles run in both dimensions, with `setNether`), `tick(cm, px, py, pz, playerAlive)`, `draw(cm, partialTick)`, `setCamera`, `popDrop`, `explosionDamage`, `lightning`, `collectBoxes`, `overlapsBlock`, `pushWithPlayer`, `count`.
- Sheep spawning: a 15–25 s timer (`300 + nextInt(200)` ticks) tries `findSpawn`, then adds a herd of up to 4 (`packSpot`); mobs farther than the 9-chunk neighbourhood despawn instantly; mobs with broken coordinates are removed; if no mob exists for 60 s the RNG and timer are reset (self-repair).
  All bodies push each other through `MobBodies::collide` after every tick. `Partial::mobWalked` lets mobs trample farmland.
- Drops go through small ring queues (`DROP_QUEUE = 8`) that `main.cpp` drains with `popDrop`.

---

## 13. Audio

- Clips are 8-bit signed mono PCM arrays in `SoundData.h` (indexed by the `SoundId` enum and the `SOUND_CLIPS` table, with sample rate per clip). Extra ambience (rain, thunder) in `SoundAmbient.h`.
- Groups: grass, wood, stone, sand, gravel, snow (step sounds; break/place use the same set for sand/gravel/snow because the dig clips are missing). `Sound::groupOf(BlockType)` maps blocks to groups
  (note: dirt, clay and farmland use gravel, as in Beta).
- No music (disabled for performance). `Options::sound` disables everything.
- To add a sound: convert the OGG to 8-bit mono (11025 or 8000 Hz), append the array to `SoundData.h`, add an entry to `SOUND_CLIPS`, add an enum value **before** `S_COUNT`, and add a function in `Sound.cpp`.
  Beware: `playClip` has a per-clip "not twice within 3 frames" filter keyed on the frame counter that only advances while playing; UI sounds bypass it.

---

## 14. Persistence

### SD card layout

```
/mcdsefiles/
├── options.dat            "MCO1" + 23 bytes of options (see below)
├── controlsprofile.dat    "MCK1" + 2 profiles × 13 actions × u16 (little endian)
├── spikes.txt             optional profiler log
├── skins/                 player skins
└── worlds/
    ├── world1 … worldN/   saved worlds (or .tempworld while a new world has never been saved)
    │   ├── meta.dat        player position/inventory/spawn/time/armor …
    │   ├── name.dat        world name
    │   ├── dim.dat bed.dat health.dat
    │   ├── furnaces.dat chests.dat carts.dat boats.dat paintings.dat signs.dat wolves.dat notes.dat maps.dat
    │   ├── mc_chunk_<x>_<z>.dat   modified chunks only
    │   └── DIM-1/          Nether (same set of files)
```

`WorldSave` tracks the active folder. New worlds start in `.tempworld` and are renamed to `worldN` at first save; an existing world is played in place. On boot `recoverInterruptedSession()`
restores or discards a leftover `.tempworld`. Deleting a world removes subfolders too (FAT `remove()` doesn't delete non-empty folders).

### `options.dat` layout (23 bytes; older files are shorter and missing fields keep defaults)

| Byte | Field | Notes |
|---|---|---|
| 0–3 | `"MCO1"` | magic |
| 4 | debugLargeChest | |
| 5 | chunkMode | 0 fast, 1 ds, 2 dsi (forced to 1 on plain DS when loading) |
| 6 | seedMode | 0 random, 1 custom |
| 7 | customSeedSet | |
| 8–11 | customSeed | u32 little endian |
| 12 | controlMode | 0 normal, 1 xyba |
| 13 | autoJump | |
| 14 | clockCompassInvUpdate | |
| 15 | particles | |
| 16 | sound | |
| 17 | inventoryMap | **stored inverted** (1 = disabled) so old files default to enabled |
| 18 | debugScreenOptions | |
| 19 | difficulty | 0–3 |
| 20 | renderQuality | 0 high, 1 balanced, 2 fast |
| 21 | spikeLog | |
| 22 | debugCoordsOnly | |
| 23 | oldMobSpawnLimit | 1 = original (pre-RC1) mob limits; 0 (default) = half. See `MobCull.h` for draw culling and far-AI throttling |

**To add an option:** add the field to `Options::Data` (+ comment), set the default in `Options::load()`, read it with an `n >= <size>` guard, write it in `Options::save()` and bump the buffer size in both places,
then add a row in `SettingsMenu.cpp` (main list or a submenu) and handle it in `SettingsMenu::update`. Always append the byte at the end.

### Settings UI map (`SettingsMenu.cpp`)

Main list: `back, difficulty, controls (submenu), auto jump, chunk mode, render, default seed, custom default seed, clockcompassinvupdate, debug options (submenu), particles, sound, inventoryMap`.
Controls submenu: `back, controls mode, reset this profile, one row per action`. Debug options submenu: `back, debug large chest, spikes.txt log, debug screen info (full/coords), debug screen options`.
Every A/B/LEFT/RIGHT press plays `Sound::uiClick()`.

---

## 15. Recipes for common tasks

- **New mob:** (1) add a kind constant (`H_*` in `HostileData.h` or `KIND_*` in `AnimalData.h`; keep `*_COUNT` updated) and any state fields; (2) AI in the matching `*AI.h` (copy the closest mob; use the `CMWorld` adapter interface); (3) register spawn/despawn/drops/attack in `HostileManager`/`AnimalManager` (the `SheepManager` facade forwards by index range); (4) model: `Part`s built with `ModelBox::build` in `init()`, drawn in `HostileModel::draw`/`AnimalModel::draw` following §12.6; (5) texture 64×32: add the `graphics/<name>.png`+`.grit`, an ID in `Texture.h/.cpp`, a loader call in `loadHostileAtlases`/`loadAnimalAtlases`, **and** the ID in `unloadEntityTextures` (§12.2) — check it fits in VRAM; (6) sounds, spawn rules and cap, health/hurt/death, drops, a `PROFSTEP`. Mobs are not saved.
- **New block with behaviour:** first check the enum is full (§8: only `SPARE_ITEM` is free), then enum + tables, then hook into `Chunk::placeBlockAt`/`breakBlockAt`, block-update dispatch, and the relevant manager (`Liquid`, `Redstone`, `PartialBlocks`, …).
- **New texture:** put a PNG + `.grit` in `graphics/`; use `-gB8 -p` (or 16 colours) and check VRAM; add a loader in `Texture.cpp` and an ID in `Texture.h`; add to `unloadEntityTextures` if it can be freed for the menu.
- **New screen/menu:** follow `PauseScreen`/`GameOverScreen` (image + hit rectangles measured on the PNG) and add click sounds.
- **Change generation:** must stay deterministic and incremental; add a named `PROFSTEP`; test that a chunk generated at different times is identical.
- **Performance regression:** ask for `spikes.txt`, identify the step, apply §11.

---

## 16. Known differences from Beta 1.7.3 (deliberate)

8×8 chunks and short view distance (everything scaled); simplified 2D-noise Nether terrain; animals/hostiles not saved; no music and a reduced sound set; no stars or sunrise glow
(sun/moon only in ds/dsi); leaves don't attenuate light (water/ice do); the map is carried in the hotbar (Legacy-style) instead of on the bottom screen; capped A\* pathfinding; reduced-size painting texture; simplified water-surface height.
See `README.md` for the user-facing version of this list.

---

## 17. Debugging checklist

- Crash on boot / link error `lma9`: RAM overflow. Look at new static arrays; move code to `-Os`; shrink tables.
- Textures black/missing: VRAM didn't fit (`glTexImage2D` returned 0); check load order and the unload list.
- Missing block faces: stale `occupancy`/`faceCache`; check that every edit path updates them and marks the display list dirty.
- Slivers/flicker only on real hardware: too many polygons/vertices; look at `vertBudget`, band culling, the debug screen's `underflow/overflow`.
- Stutter: read `spikes.txt`; find the step; split or budget it.
- Mobs vanish/teleport after a reload: managers delete mobs whose chunk unloads; the chunk-mode reload path deliberately keeps them.
- Save problems: the chunk file name must be consistent between save and load; per-world files are written by the save routine in `main.cpp` (`SELECT`/quit paths) and read in `startWorld`.

---

## 18. Credits and license

Code written by Claude (Anthropic) under the direction of JohanGCX, who also designed and implemented the menus exclusive to this port. Game logic reference: the decompiled Beta 1.7.3 by PeytonPlayz595 (Eaglercraft).
*Minecraft* and its assets belong to Mojang Studios. Source code is MIT licensed (see `LICENSE`); game assets are not covered by that license.
