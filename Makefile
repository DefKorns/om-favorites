MOD_ID   := om-favorites
MOD_NAME := Options Menu Favorites
MOD_CATEGORY := Games & Tools
MOD_CREATOR  := DefKorns

BINARIES := mod/etc/options_menu/favorites_manager/favorites_manager

CXX ?= g++
STRIP ?= strip

ifdef CROSS_PREFIX
PKG_CONFIG_LIBDIR = /usr/lib/arm-linux-gnueabihf/pkgconfig
SDL_CFLAGS = -I/usr/include/arm-linux-gnueabihf $(shell PKG_CONFIG_LIBDIR=$(PKG_CONFIG_LIBDIR) pkg-config --cflags sdl2 SDL2_ttf libpng)
SDL_LIBS = $(shell PKG_CONFIG_LIBDIR=$(PKG_CONFIG_LIBDIR) pkg-config --libs sdl2 SDL2_ttf libpng)
LDFLAGS = -Wl,--allow-shlib-undefined
else
SDL_CFLAGS = $(shell sdl2-config --cflags 2>/dev/null || pkg-config --cflags sdl2 SDL2_ttf libpng 2>/dev/null)
SDL_LIBS = $(shell sdl2-config --libs 2>/dev/null || pkg-config --libs sdl2 SDL2_ttf libpng 2>/dev/null)
LDFLAGS =
endif

CXXFLAGS = -std=c++14 -Os -Isrc -Ivendor/OptionsMenu/src $(SDL_CFLAGS) -DMOD_VERSION=\"v$(MOD_VER)\"
LDLIBS = $(SDL_LIBS)

# Framework objects from vendor/OptionsMenu
OM_SOURCES = vendor/OptionsMenu/src/localization.cpp \
             vendor/OptionsMenu/src/framework/badge.cpp \
             vendor/OptionsMenu/src/framework/dialog.cpp \
             vendor/OptionsMenu/src/framework/sdl_context.cpp \
             vendor/OptionsMenu/src/framework/texture.cpp \
             vendor/OptionsMenu/src/framework/controller.cpp \
             vendor/OptionsMenu/src/framework/powerwatch.cpp \
             vendor/OptionsMenu/src/framework/draw_helpers.cpp \
             vendor/OptionsMenu/src/framework/utf8.cpp \
             vendor/OptionsMenu/src/framework/font8x8_lookup.cpp \
             vendor/OptionsMenu/src/framework/uitheme.cpp

APP_SOURCES = src/favorites_manager.cpp src/favorites_core.cpp

OM_OBJECTS = $(OM_SOURCES:.cpp=.o)
APP_OBJECTS = src/favorites_manager.o src/favorites_core.o

ALL_OBJECTS = $(OM_OBJECTS) $(APP_OBJECTS)

MOD_DEPS := $(BINARIES)

all: hmod

compile: $(BINARIES)

%.o: %.cpp
	$(CROSS_PREFIX)$(CXX) $(CXXFLAGS) -c $< -o $@

mod/etc/options_menu/favorites_manager/favorites_manager: $(APP_OBJECTS) $(OM_OBJECTS)
	@mkdir -p mod/etc/options_menu/favorites_manager
	$(CROSS_PREFIX)$(CXX) $^ $(LDLIBS) $(LDFLAGS) -Wl,-rpath,/etc/options_menu/lib -o $@
ifdef CROSS_PREFIX
	$(CROSS_PREFIX)$(STRIP) $@
endif
	chmod 755 $@
	chmod 755 mod/etc/options_menu/favorites_manager/launch_favorites_manager.sh

clean:
	rm -f $(ALL_OBJECTS) $(BINARIES)
	rm -rf out

-include vendor/hmod-build/hmod.mk
