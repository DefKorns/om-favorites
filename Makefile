MOD_ID       := om-favorites
MOD_NAME     := Options Menu - Options Menu Favorites
MOD_CATEGORY := Options Menu - Addons
MOD_DEPS     := mod/uninstall mod/etc/options_menu/om-favorites/favorites

FRAMEWORK_DIR = vendor/OptionsMenu/src/framework
VENDOR_SRC_DIR = vendor/OptionsMenu/src
CXX = g++
STRIP = strip
ifdef CROSS_PREFIX
PKG_CONFIG_LIBDIR = /usr/lib/arm-linux-gnueabihf/pkgconfig
SDL_CFLAGS = -I/usr/include/arm-linux-gnueabihf $(shell PKG_CONFIG_LIBDIR=$(PKG_CONFIG_LIBDIR) pkg-config --cflags sdl2)
SDL_LIBS = $(shell PKG_CONFIG_LIBDIR=$(PKG_CONFIG_LIBDIR) pkg-config --libs sdl2)
LDFLAGS = -Wl,--allow-shlib-undefined
else
SDL_CFLAGS = $(shell sdl2-config --cflags)
SDL_LIBS = $(shell sdl2-config --libs)
LDFLAGS =
endif
CXXFLAGS = -std=c++11 -Os -Wall -I$(VENDOR_SRC_DIR) $(SDL_CFLAGS)
LDLIBS = $(SDL_LIBS)
SOURCES = src/main.cpp $(VENDOR_SRC_DIR)/localization.cpp $(FRAMEWORK_DIR)/controller.cpp
OBJECTS = $(SOURCES:.cpp=.o)
DEPDIR = .deps

all: hmod

compile: $(MOD_DEPS)

mod/etc/options_menu/om-favorites/favorites: $(OBJECTS)
	mkdir -p $(@D)
	$(CROSS_PREFIX)$(CXX) $(OBJECTS) $(LDLIBS) $(LDFLAGS) -Wl,-rpath,/etc/options_menu/lib -o $@
	$(CROSS_PREFIX)$(STRIP) $@

%.o: %.cpp
	@mkdir -p $(DEPDIR)
	$(CROSS_PREFIX)$(CXX) $(CXXFLAGS) -MMD -MP -MF $(DEPDIR)/$(subst /,_,$*).d -c $< -o $@

-include $(wildcard $(DEPDIR)/*.d)

clean:
	find . -name "*.o" -type f -not -path "./vendor/*" -delete
	rm -rf $(DEPDIR)
	rm -f mod/etc/options_menu/om-favorites/favorites
	rm -rf out/

include vendor/hmod-build/hmod.mk

.PHONY: all compile clean
