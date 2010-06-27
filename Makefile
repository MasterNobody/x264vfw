##############################################################################
#
# Makefile for x264 VFW driver
#
# Author: XviD project:
#            - Milan Cutka <cutka at szm.sk>,
#            - Edouard Gomez <ed.gomez at free.fr>
#            - Peter Ross <pross@xvid.org>
# Ported to x264 by Laurent Aimar <fenrir@via.ecp.fr>
#
# $Id: Makefile,v 1.1 2004/06/03 19:29:33 fenrir Exp $
##############################################################################

# Current dir
DIR_CUR = $(shell pwd)

# Path to include files (library and src)
DIR_SRC = $(DIR_CUR)
ifeq ($(X264_DIR)x,x)
X264_DIR = $(DIR_CUR)/../x264
endif
ifeq ($(FFMPEG_DIR)x,x)
FFMPEG_DIR = $(DIR_CUR)/../ffmpeg
endif

ifeq ($(wildcard config.mak)x,x)
$(info $() Copy config.mak from $(X264_DIR))
$(shell cp -f "$(X264_DIR)/config.mak" "$(DIR_CUR)/config.mak")
endif
ifeq ($(wildcard config.h)x,x)
$(info $() Copy config.h from $(X264_DIR))
$(shell cp -f "$(X264_DIR)/config.h" "$(DIR_CUR)/config.h")
endif

include config.mak
include x264vfw_config.mak

ifeq ($(ARCH),X86_64)
# Dll to build
DLL = x264vfw64.dll

# Installer script
INST_NSI = x264vfw64.nsi

# Installer executable
INST_EXE = x264vfw64.exe
else
# Dll to build
DLL = x264vfw.dll

# Installer script
INST_NSI = x264vfw.nsi

# Installer executable
INST_EXE = x264vfw.exe
endif

# Alias
ifeq ($(WINDRES)x,x)
WINDRES = windres
endif

##############################################################################
# CFLAGS
##############################################################################

# Constants which should not be modified
# The `mingw-runtime` package is required when building with -mno-cygwin
CFLAGS += -mno-cygwin
CFLAGS += -D_WIN32_IE=0x0501
CFLAGS += "-I$(X264_DIR)"

##############################################################################
# Compiler flags for linking stage
##############################################################################

VFW_LDFLAGS = "-L$(X264_DIR)" -lx264

##############################################################################
# Conditional options
##############################################################################

RESFLAGS =
ifeq ($(HAVE_FFMPEG),yes)
RESFLAGS += "-DHAVE_FFMPEG"
CFLAGS += "-DHAVE_FFMPEG"
CFLAGS += "-I$(FFMPEG_DIR)"
VFW_LDFLAGS += "-L$(FFMPEG_DIR)/libavformat" -lavformat
VFW_LDFLAGS += "-L$(FFMPEG_DIR)/libavcodec" -lavcodec
VFW_LDFLAGS += "-L$(FFMPEG_DIR)/libavutil" -lavutil
VFW_LDFLAGS += "-L$(FFMPEG_DIR)/libswscale" -lswscale
endif

# Sources
SRC_C = codec.c config.c csp.c driverproc.c
SRC_RES = resource.rc

# Muxers
CONFIG := $(shell cat config.h)

SRC_C += output/raw.c
SRC_C += output/matroska.c output/matroska_ebml.c
SRC_C += output/flv.c output/flv_bytestream.c

ifneq ($(findstring HAVE_GPAC, $(CONFIG)),)
SRC_C += output/mp4.c
VFW_LDFLAGS += -lgpac_static
endif

ifeq ($(HAVE_FFMPEG),yes)
SRC_C += output/avi.c
endif

##############################################################################
# Rules
##############################################################################

OBJECTS  = $(SRC_C:.c=.obj)
OBJECTS += $(SRC_RES:.rc=.obj)

.SUFFIXES:
.SUFFIXES: .obj .rc .c

DIR_BUILD = $(DIR_CUR)/bin
VPATH = $(DIR_SRC):$(DIR_BUILD)

.PHONY: all clean distclean build-installer

all: $(DLL)

ifneq ($(wildcard .depend),)
include .depend
endif

# Resource dependence manually
resource.obj: resource.rc resource.h x264vfw_config.h

%.obj: %.rc
	@echo " W: $(@D)/$(<F)"
	@mkdir -p "$(DIR_BUILD)/$(@D)"
	@$(WINDRES) $(RESFLAGS) \
	--input-format=rc \
	--output-format=coff \
	-o "$(DIR_BUILD)/$@" $<

%.obj: %.c
	@echo " C: $(@D)/$(<F)"
	@mkdir -p "$(DIR_BUILD)/$(@D)"
	@$(CC) $(CFLAGS) -c -o "$(DIR_BUILD)/$@" $<

.depend: config.mak
	@rm -f .depend
	@$(foreach SRC, $(SRC_C), $(CC) $(CFLAGS) $(SRC) -MT $(SRC:%.c=%.obj) -MM -g0 1>> .depend;)

$(DLL): .depend config.mak config.h $(OBJECTS)
	@echo " L: $(@F)"
	@mkdir -p "$(DIR_BUILD)"
	@cp -f "$(DIR_SRC)/driverproc.def" "$(DIR_BUILD)/driverproc.def"
	@cd "$(DIR_BUILD)" && \
	$(CC) \
	-mno-cygwin -shared -Wl,-dll,--out-implib,$@.a,--enable-stdcall-fixup \
	-o $@ \
	$(OBJECTS) driverproc.def \
	$(VFW_LDFLAGS) $(LDFLAGS) -lgdi32 -lwinmm -lcomdlg32 -lcomctl32

clean:
	@echo " Cl: Object files and target lib"
	@rm -rf "$(DIR_BUILD)"
	@echo " Cl: .depend"
	@rm -f .depend

distclean: clean
	@echo " Cl: config.mak"
	@rm -f "$(DIR_CUR)/config.mak"
	@echo " Cl: config.h"
	@rm -f "$(DIR_CUR)/config.h"

##############################################################################
# Builds the NSIS installer script for Windows.
# NSIS 2.x is required and makensis.exe should be in the path
##############################################################################

$(INST_EXE): $(DLL)
	@cp -f "$(DIR_SRC)/installer/$(INST_NSI)" "$(DIR_BUILD)/$(INST_NSI)"
	@cp -f "$(DIR_SRC)/installer/x264vfw.ico" "$(DIR_BUILD)/x264vfw.ico"
	@makensis "$(DIR_BUILD)/$(INST_NSI)"
	@rm -f "$(DIR_BUILD)/$(INST_NSI)"
	@rm -f "$(DIR_BUILD)/x264vfw.ico"

build-installer: $(INST_EXE)
