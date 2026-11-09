#---------------------------------------------------------------------------------
# Verifica que devkitARM esté definido
#---------------------------------------------------------------------------------
ifeq ($(strip $(DEVKITARM)),)
$(error "Please set DEVKITARM in your environment. export DEVKITARM=<path to>devkitARM")
endif

include $(DEVKITARM)/ds_rules

#---------------------------------------------------------------------------------
# TARGET es el nombre del .nds que se genera
# BUILD es el directorio donde se guardan los objetos intermedios
# SOURCES es la lista de directorios donde buscar código fuente
# INCLUDES es la lista de directorios donde buscar headers
# DATA es el directorio con archivos de datos binarios
#---------------------------------------------------------------------------------
TARGET		:=	$(shell basename $(CURDIR))
BUILD		:=	build
SOURCES		:=	source
GRAPHICS	:=	graphics
INCLUDES	:=	include
DATA		:=	data
# NITRO: carpeta que ndstool mete dentro del .nds como sistema de archivos (NitroFS, se lee como nitro:/...). Ahi va la musica: nitrofiles/music/*.mus
NITRO		:=	nitrofiles

#---------------------------------------------------------------------------------
# opciones para el código
#---------------------------------------------------------------------------------
ARCH	:=	-mthumb -mthumb-interwork

CFLAGS	:=	-g -Wall -O2\
 		-march=armv5te -mtune=arm946e-s -fomit-frame-pointer\
		-ffast-math -ffunction-sections -fdata-sections \
		$(ARCH)

CFLAGS	+=	$(INCLUDE) -DARM9
CXXFLAGS	:=	$(CFLAGS) -fno-rtti -fno-exceptions -fno-threadsafe-statics -std=gnu++11

ASFLAGS	:=	-g $(ARCH)
LDFLAGS	 =	-specs=ds_arm9.specs -g $(ARCH) -Wl,-Map,$(notdir $*.map) -Wl,--gc-sections

#---------------------------------------------------------------------------------
# lista de librerías a usar
#---------------------------------------------------------------------------------
LIBS	:= -lfilesystem -lfat -lnds9

#---------------------------------------------------------------------------------
# rutas donde buscar las librerías
#---------------------------------------------------------------------------------
LIBDIRS	:= $(LIBNDS)


#---------------------------------------------------------------------------------
# no toques nada de aquí en adelante a menos que sepas lo que haces
#---------------------------------------------------------------------------------
ifneq ($(BUILD),$(notdir $(CURDIR)))

# alphakey (tools/alphakey.c) pasa la transparencia de los PNG a magenta FF00FF antes de grit.
# En Windows (MSYS2 de devkitPro, que no trae compilador del sistema) se usa tools/alphakey.exe ya compilado;
# en Linux y macOS se compila solo la primera vez con el compilador del sistema (cc, gcc o clang).
HOST_OS := $(shell uname -s)
ifneq (,$(findstring MINGW,$(HOST_OS))$(findstring MSYS,$(HOST_OS))$(findstring CYGWIN,$(HOST_OS)))
export ALPHAKEY := $(CURDIR)/tools/alphakey.exe
else
export ALPHAKEY := $(CURDIR)/tools/alphakey
endif
HOSTCC := $(shell command -v cc 2>/dev/null || command -v gcc 2>/dev/null || command -v clang 2>/dev/null)

# solo se construye si falta (dependencia de orden): no depende de la fecha de los archivos
$(ALPHAKEY): | tools/alphakey.c
	@echo building alphakey ...
	@[ -n "$(HOSTCC)" ] || { echo "No host C compiler found: install gcc/clang, or build tools/alphakey.c by hand"; exit 1; }
	$(HOSTCC) -O2 -o $@ tools/alphakey.c -lm

# sin esto, la regla de alphakey (la primera del archivo) se volvia el objetivo por defecto y make no compilaba nada
.DEFAULT_GOAL := $(BUILD)

export OUTPUT	:=	$(CURDIR)/$(TARGET)

# ds_rules le pasa NITRO_FILES a ndstool (-d): todo lo que hay en nitrofiles/ queda dentro del .nds
ifneq ($(strip $(NITRO)),)
export NITRO_FILES	:=	$(CURDIR)/$(NITRO)
endif

# Banner del .nds (lo que muestran Twilight Menu++ y el menu del flashcart): icono 32x32 de 16 colores (se genera con tools/mkicon.py) y los tres textos.
# ndstool los recibe de ds_rules como GAME_ICON y GAME_TITLE / GAME_SUBTITLE1 / GAME_SUBTITLE2.
export GAME_ICON		:=	$(CURDIR)/icon.bmp
export GAME_TITLE		:=	Minecraft DS
export GAME_SUBTITLE1	:=	Eaglercraft Beta 1.7.3
export GAME_SUBTITLE2	:=	Nintendo DS Edition

export VPATH	:=	$(foreach dir,$(SOURCES),$(CURDIR)/$(dir)) \
			$(foreach dir,$(DATA),$(CURDIR)/$(dir)) \
			$(foreach dir,$(GRAPHICS),$(CURDIR)/$(dir))

export DEPSDIR	:=	$(CURDIR)/$(BUILD)

CFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
CPPFILES	:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
SFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))
BINFILES	:=	$(foreach dir,$(DATA),$(notdir $(wildcard $(dir)/*.*)))
PNGFILES	:=	$(foreach dir,$(GRAPHICS),$(notdir $(wildcard $(dir)/*.png)))

#---------------------------------------------------------------------------------
# usa CXX para enlazar si hay archivos .cpp, si no usa CC
#---------------------------------------------------------------------------------
ifeq ($(strip $(CPPFILES)),)
	export LD	:=	$(CC)
else
	export LD	:=	$(CXX)
endif

export OFILES	:=	$(PNGFILES:.png=.o) \
			$(BINFILES:.%=%.o) \
			$(CPPFILES:.cpp=.o) \
			$(CFILES:.c=.o) \
			$(SFILES:.s=.o)

export HFILES	:=	$(PNGFILES:.png=.h)

export INCLUDE	:=	$(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
			$(foreach dir,$(LIBDIRS),-I$(dir)/include) \
			-I$(CURDIR)/$(BUILD)

export LIBPATHS	:=	$(foreach dir,$(LIBDIRS),-L$(dir)/lib)

.PHONY: $(BUILD) clean

#---------------------------------------------------------------------------------
$(BUILD): $(ALPHAKEY)
	@[ -d $@ ] || mkdir -p $@
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

#---------------------------------------------------------------------------------
clean:
	@echo clean ...
	@rm -fr $(BUILD) $(TARGET).elf $(TARGET).nds

#---------------------------------------------------------------------------------
else

DEPENDS	:=	$(OFILES:.o=.d)

#---------------------------------------------------------------------------------
# regla principal de compilación
#---------------------------------------------------------------------------------
$(OUTPUT).nds	:	$(OUTPUT).elf
$(OUTPUT).elf	:	$(OFILES)
# Los archivos fuente dependen de los headers generados por grit
$(OFILES) : $(HFILES)

# Experimento APAGADO: compilar en modo ARM (en vez de Thumb) el código con mucha matemática. En las pruebas empeoró (columna de 2,5 a 23 ms):
# la RAM de la DS tiene bus de 16 bits y el Thumb, más compacto, suele ganar cuando el código no entra en la caché de 8 KB.
# Para volver a probarlo, sacá el # de las dos líneas siguientes.
# La memoria principal de la DS (4 MB) esta al limite: el codigo que no corre en el bucle caliente (menus, pantallas, redstone, guardado) va en -Os (mas chico)
SMALL_OFILES := CraftingScreen.o InventoryScreen.o MenuFont.o TextKeyboard.o NewWorldMenu.o Options.o PauseScreen.o SettingsMenu.o SubScreenMenu.o TopScreenMenu.o \
                WorldSelectMenu.o GameOverScreen.o MenuPanorama.o SignManager.o WorldSave.o Profiler.o Governor.o Furnace.o ChestStore.o Redstone.o Pistons.o \
                NoteBlocks.o TntManager.o Dispenser.o Projectiles.o MinecartManager.o BoatManager.o PaintingManager.o PartialBlocks.o Music.o TexturePack.o TexturePackMenu.o Skins.o
$(SMALL_OFILES) : CXXFLAGS += -Os

#ARM_OFILES := PerlinNoise.o Noise.o Caves.o ColorMaps.o
#$(ARM_OFILES) : CXXFLAGS += -marm

# Atlas con transparencia: alphakey (alfa -> FF00FF) y después grit con -gT FF00FF
terrain.s terrain.h : terrain.png terrain.grit
	$(ALPHAKEY) $(filter %.png,$^) terrain_keyed.png
	cp $(filter %.grit,$^) terrain_keyed.grit
	grit terrain_keyed.png -fts -oterrain

items.s items.h : items.png items.grit
	$(ALPHAKEY) $(filter %.png,$^) items_keyed.png
	cp $(filter %.grit,$^) items_keyed.grit
	grit items_keyed.png -fts -oitems

# arrows.png (la flecha 3D, 32 x 32): alphakey y grit con -gT FF00FF (el color 0 de la paleta es transparente)
arrows.s arrows.h : arrows.png arrows.grit
	$(ALPHAKEY) $(filter %.png,$^) arrows_keyed.png
	cp $(filter %.grit,$^) arrows_keyed.grit
	grit arrows_keyed.png -fts -oarrows

icons.s icons.h : icons.png icons.grit
	$(ALPHAKEY) $(filter %.png,$^) icons_keyed.png
	cp $(filter %.grit,$^) icons_keyed.grit
	grit icons_keyed.png -fts -oicons

#---------------------------------------------------------------------------------
# Regla que convierte PNG a datos de textura usando grit
#---------------------------------------------------------------------------------
%.s %.h	:	%.png %.grit
	grit $< -fts -o$*

-include $(DEPENDS)

#---------------------------------------------------------------------------------------
endif