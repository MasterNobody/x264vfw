/*****************************************************************************
 * common.h: misc common functions
 *****************************************************************************
 * Copyright (C) 2010-2012 x264vfw project
 *
 * Authors: Anton Mitrofanov <BugMaster@narod.ru>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

#ifndef X264VFW_COMMON_H
#define X264VFW_COMMON_H

#define _LARGEFILE_SOURCE 1
#define _FILE_OFFSET_BITS 64

#include "x264vfw_config.h"

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <windows.h>

#include <x264.h>
#include "config.h"

#ifdef HAVE_STDINT_H
#include <stdint.h>
#else
#include <inttypes.h>
#endif

#if defined(__GNUC__) && (__GNUC__ > 3 || __GNUC__ == 3 && __GNUC_MINOR__ > 0)
#define UNUSED __attribute__((unused))
#define ALWAYS_INLINE __attribute__((always_inline)) inline
#define NOINLINE __attribute__((noinline))
#define MAY_ALIAS __attribute__((may_alias))
#define x264_constant_p(x) __builtin_constant_p(x)
#define x264_nonconstant_p(x) (!__builtin_constant_p(x))
#else
#define UNUSED
#define ALWAYS_INLINE inline
#define NOINLINE
#define MAY_ALIAS
#define x264_constant_p(x) 0
#define x264_nonconstant_p(x) 0
#endif

#if defined(__GNUC__) && (__GNUC__ > 4 || __GNUC__ == 4 && __GNUC_MINOR__>1)
#define attribute_align_arg __attribute__((force_align_arg_pointer))
#else
#define attribute_align_arg
#endif

#define X264_MIN(a, b) (((a)<(b)) ? (a) : (b))
#define X264_MAX(a, b) (((a)>(b)) ? (a) : (b))
#define X264_CLIP(v, min, max) (((v)<(min)) ? (min) : ((v)>(max)) ? (max) : (v))

#define WORD_SIZE sizeof(void*)

#define asm __asm__

#if WORDS_BIGENDIAN
#define endian_fix(x) (x)
#define endian_fix64(x) (x)
#define endian_fix32(x) (x)
#define endian_fix16(x) (x)
#else
#if defined(__GNUC__) && HAVE_MMX
static ALWAYS_INLINE uint32_t endian_fix32( uint32_t x )
{
    asm("bswap %0":"+r"(x));
    return x;
}
#elif defined(__GNUC__) && HAVE_ARMV6
static ALWAYS_INLINE uint32_t endian_fix32( uint32_t x )
{
    asm("rev %0, %0":"+r"(x));
    return x;
}
#else
static ALWAYS_INLINE uint32_t endian_fix32( uint32_t x )
{
    return (x<<24) + ((x<<8)&0xff0000) + ((x>>8)&0xff00) + (x>>24);
}
#endif
#if defined(__GNUC__) && ARCH_X86_64
static ALWAYS_INLINE uint64_t endian_fix64( uint64_t x )
{
    asm("bswap %0":"+r"(x));
    return x;
}
#else
static ALWAYS_INLINE uint64_t endian_fix64( uint64_t x )
{
    return endian_fix32(x>>32) + ((uint64_t)endian_fix32(x)<<32);
}
#endif
static ALWAYS_INLINE intptr_t endian_fix( intptr_t x )
{
    return WORD_SIZE == 8 ? endian_fix64(x) : endian_fix32(x);
}
static ALWAYS_INLINE uint16_t endian_fix16( uint16_t x )
{
    return (x<<8)|(x>>8);
}
#endif

static inline uint8_t x264_is_regular_file( FILE *filehandle )
{
    struct stat file_stat;
    if( fstat( fileno( filehandle ), &file_stat ) )
        return -1;
    return S_ISREG( file_stat.st_mode );
}

static inline uint8_t x264_is_regular_file_path( const char *filename )
{
    struct stat file_stat;
    if( stat( filename, &file_stat ) )
        return -1;
    return S_ISREG( file_stat.st_mode );
}

#if X264VFW_DEBUG_OUTPUT
#define DPRINTF_BUF_SZ 1024
static inline void DPRINTF(const char *fmt, ...)
{
    va_list arg;
    char buf[DPRINTF_BUF_SZ];

    va_start(arg, fmt);
    memset(buf, 0, sizeof(buf));
    vsnprintf(buf, sizeof(buf) - 1, fmt, arg);
    va_end(arg);
    OutputDebugString(buf);
}
static inline void DVPRINTF(const char *fmt, va_list arg)
{
    char buf[DPRINTF_BUF_SZ];

    memset(buf, 0, sizeof(buf));
    vsnprintf(buf, sizeof(buf) - 1, fmt, arg);
    OutputDebugString(buf);
}
#else
static inline void DPRINTF(const char *fmt, ...) {}
static inline void DVPRINTF(const char *fmt, va_list arg) {}
#endif

#endif
