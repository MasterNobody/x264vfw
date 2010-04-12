HAVE_FFMPEG=yes
ifeq ($(ARCH),X86_64)
ifeq ($(WINDRES)x,x)
WINDRES = x86_64-pc-mingw32-windres
endif
endif
