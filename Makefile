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

# Current dir
DIR_CUR = $(shell pwd)

# Path to include filen library and src
DIR_SRC = $(DIR_CUR)
X264_DIR = $(DIR_CUR)/../x264
FFMPEG_DIR = $(DIR_CUR)/../ffmpeg

# Sources
SRC_C = codec.c config.c csp.c driverproc.c
SRC_RES = resource.rc

# Alias
RM = rm -rf
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
CFLAGS += -I$(DIR_SRC)/w32api -I$(X264_DIR)

##############################################################################
# Compiler flags for linking stage
##############################################################################

VFW_LDFLAGS = -L$(X264_DIR) -lx264

##############################################################################
# Conditional options
##############################################################################

ifeq ($(HAVE_FFMPEG),yes)
CFLAGS += -I$(FFMPEG_DIR) -I$(FFMPEG_DIR)/libavcodec -I$(FFMPEG_DIR)/libavutil -I$(FFMPEG_DIR)/libswscale
VFW_LDFLAGS += -L$(FFMPEG_DIR)/libavcodec -lavcodec -L$(FFMPEG_DIR)/libavutil -lavutil -L$(FFMPEG_DIR)/libswscale -lswscale
endif

##############################################################################
# Rules
##############################################################################

OBJECTS  = $(SRC_C:.c=.obj)
OBJECTS += $(SRC_RES:.rc=.obj)

.SUFFIXES: .obj .rc .c

DIR_BUILD = $(DIR_CUR)/bin
VPATH = $(DIR_SRC):$(DIR_BUILD)

all: $(DLL)

config.mak:
	@echo " Copy config.mak from $(X264_DIR)"
	@cp $(X264_DIR)/config.mak $(DIR_SRC)/config.mak

$(DIR_BUILD):
	@echo " D: $(DIR_BUILD)"
	@mkdir -p $(DIR_BUILD)

.rc.obj:
	@echo " W: $(@D)/$(<F)"
	@mkdir -p $(DIR_BUILD)/$(@D)
	@$(WINDRES) \
	--include-dir=$(DIR_SRC) \
	--input-format=rc \
	--output-format=coff \
	-o $(DIR_BUILD)/$@ $<

.c.obj:
	@echo " C: $(@D)/$(<F)"
	@mkdir -p $(DIR_BUILD)/$(@D)
	@$(CC) $(CFLAGS) -c -o $(DIR_BUILD)/$@ $<

$(DLL): $(DIR_BUILD) $(OBJECTS)
	@echo " L: $(@F)"
	@cp $(DIR_SRC)/driverproc.def $(DIR_BUILD)/driverproc.def
	@cd $(DIR_BUILD) && \
	$(CC) \
	-mno-cygwin -shared -Wl,-dll,--out-implib,$@.a,--enable-stdcall-fixup \
	-o $@ \
	$(OBJECTS) driverproc.def \
	$(VFW_LDFLAGS) $(LDFLAGS) -lgdi32 -lwinmm -lcomdlg32 -lcomctl32

clean:
	@echo " Cl: Object files and target lib"
	@$(RM) $(DIR_BUILD)

##############################################################################
# Builds the NSIS installer script for Windows.
# NSIS 2.x is required and makensis.exe should be in the path
##############################################################################

build-installer: $(DLL)
	@cp $(DIR_BUILD)/$(DLL) $(DIR_SRC)/installer
	@makensis $(DIR_SRC)/installer/$(INST_NSI)
	@mv $(DIR_SRC)/installer/$(INST_EXE) $(DIR_BUILD)
	@rm $(DIR_SRC)/installer/$(DLL)
