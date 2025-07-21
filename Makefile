ifeq ($(MAKECMDGOALS),release)

.PHONY: release
release:
	$(MAKE) PLATFORM=musl zip
	$(MAKE) PLATFORM=musl appimage-zip
	$(MAKE) PLATFORM=musl appimage-release
	$(MAKE) PLATFORM=win32 zip
	$(MAKE) PLATFORM=win64 zip
	mkdir -p release
	cp -t release _musl/*.zip _win32/*.zip _win64/*.zip _musl/*.AppImage

else ifeq ($(MAKECMDGOALS),clean-all)

.PHONY: clean-all
clean-all:
	rm -rf release
	$(MAKE) clean
	$(MAKE) PLATFORM=musl clean
	$(MAKE) PLATFORM=win32 clean
	$(MAKE) PLATFORM=win64 clean

else ifeq (,$(filter _%,$(notdir $(CURDIR))))

include target.mk

else

VPATH = $(SRCDIR)

VERSION = 0.3.1

ifeq ($(PLATFORM),win32)
TARGET = doom-ascii.exe
CC = i686-w64-mingw32-gcc-win32
else ifeq ($(PLATFORM),win64)
TARGET = doom-ascii.exe
CC = x86_64-w64-mingw32-gcc-win32
else ifeq ($(PLATFORM),musl)
TARGET = doom-ascii
CC = musl-gcc
CFLAGS += -DNORMALUNIX -DLINUX -static
else
TARGET = doom-ascii
CFLAGS += -DNORMALUNIX -DLINUX
endif

TARGET_TRIPLE = $(subst -, ,$(shell $(CC) -dumpmachine))
ARCH = $(firstword $(TARGET_TRIPLE))
OS = $(word 2,$(TARGET_TRIPLE))

TARGETAPP = doom-ascii.AppImage
TARGETAPPREL = doom-ascii-$(VERSION)-$(ARCH).AppImage
TARGETZIP = doom-ascii-$(VERSION)-$(ARCH)-$(OS).zip
TARGETAPPZIP = $(TARGETAPPREL).zip

OBJDIR = obj
APPDIR = $(OBJDIR)/io.github.wojciech_graj.doom_ascii.AppDir
OUTDIR = game
APPOUTDIR = appimage

CFLAGS += -O3 -flto -Wall -D_DEFAULT_SOURCE -DVERSION=$(VERSION) -std=c99 #-DSNDSERV -DUSEASM
LDFLAGS += -flto
LIBS += -lm

ifndef NSIGN
APPFLAGS += --sign
endif

SRC = i_main.c dummy.c am_map.c doomdef.c doomstat.c dstrings.c d_event.c d_items.c d_iwad.c \
	d_loop.c d_main.c d_mode.c d_net.c f_finale.c f_wipe.c g_game.c hu_lib.c hu_stuff.c info.c \
	i_cdmus.c i_endoom.c i_joystick.c i_scale.c i_sound.c i_system.c i_timer.c memio.c m_argv.c \
	m_bbox.c m_cheat.c m_config.c m_controls.c m_fixed.c m_menu.c m_misc.c m_random.c \
	p_ceilng.c p_doors.c p_enemy.c p_floor.c p_inter.c p_lights.c p_map.c p_maputl.c p_mobj.c \
	p_plats.c p_pspr.c p_saveg.c p_setup.c p_sight.c p_spec.c p_switch.c p_telept.c p_tick.c \
	p_user.c r_bsp.c r_data.c r_draw.c r_main.c r_plane.c r_segs.c r_sky.c r_things.c sha1.c \
	sounds.c statdump.c st_lib.c st_stuff.c s_sound.c tables.c v_video.c wi_stuff.c \
	w_checksum.c w_file.c w_main.c w_wad.c z_zone.c w_file_stdc.c i_input.c i_video.c \
	doomgeneric.c doomgeneric_ascii.c
OBJS = $(SRC:%.c=$(OBJDIR)/%.o)

OBJSAPP = $(APPDIR)/usr/bin/$(TARGET) $(APPDIR)/AppRun $(APPDIR)/io.github.wojciech_graj.doom_ascii.desktop $(APPDIR)/io.github.wojciech_graj.doom_ascii.png $(APPDIR)/usr/share/metainfo/io.github.wojciech_graj.doom_ascii.appdata.xml

.PHONY: all
all: $(OUTDIR)/$(TARGET) $(OUTDIR)/.default.cfg

.PHONY: appimage
appimage: $(APPOUTDIR)/$(TARGETAPP) $(APPOUTDIR)/.default.cfg

.PHONY: appimage-release
appimage-release: $(TARGETAPPREL)

.PHONY: zip
zip: $(TARGETZIP)

.PHONY: appimage-zip
appimage-zip: $(TARGETAPPZIP)

$(OBJDIR)/$(TARGETAPP): $(OBJSAPP)
	@mkdir -p $(@D)
	appimagetool $(APPDIR) $@ $(APPFLAGS)

$(APPDIR)/AppRun:
	@mkdir -p $(@D)
	ln -s usr/bin/doom-ascii $@

$(OBJDIR)/$(TARGET): $(OBJS)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@ $(LIBS)

$(OBJDIR)/%.o: $(SRCDIR)/%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGETAPPREL): $(OBJDIR)/$(TARGETAPP)
	cp $< $@

define copy_rule
$1/%: $2/%
	@mkdir -p $$(@D)
	cp $$< $$@
endef

$(eval $(call copy_rule,$(OUTDIR),$(OBJDIR)))
$(eval $(call copy_rule,$(OUTDIR),$(SRCDIR)))
$(eval $(call copy_rule,$(APPDIR),$(SRCDIR)/AppDir))
$(eval $(call copy_rule,$(APPDIR)/usr/share/metainfo,$(SRCDIR)/AppDir))
$(eval $(call copy_rule,$(APPDIR)/usr/bin,$(OBJDIR)))
$(eval $(call copy_rule,$(APPOUTDIR),$(SRCDIR)))
$(eval $(call copy_rule,$(APPOUTDIR),$(OBJDIR)))

define zip_rule
$1: $(OBJDIR)/$2 ../README.md ../LICENSE $(SRCDIR)/.default.cfg
	$$(eval $$@_TMP := $$(shell mktemp -d))
	cp -t $$($$@_TMP) $$^
	zip -r -j $$@ $$($$@_TMP)
	rm -rf $$($$@_TMP)
endef

$(eval $(call zip_rule,$(TARGETZIP),$(TARGET)))
$(eval $(call zip_rule,$(TARGETAPPZIP),$(TARGETAPP)))

endif
