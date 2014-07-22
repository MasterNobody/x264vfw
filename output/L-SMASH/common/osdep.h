/*****************************************************************************
 * osdep.h:
 *****************************************************************************
 * Copyright (C) 2010-2014 L-SMASH project
 *
 * Authors: Yusuke Nakamura <muken.the.vfrmaniac@gmail.com>
 *          Takashi Hirata <silverfilain@gmail.com>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *****************************************************************************/

/* This file is available under an ISC license. */

#ifndef OSDEP_H
#define OSDEP_H

#define _FILE_OFFSET_BITS 64

#ifdef __MINGW32__
#define lsmash_fseek fseeko64
#define lsmash_ftell ftello64
#endif

#ifdef _WIN32
#  include <stdio.h>
   FILE *lsmash_win32_fopen( const char *name, const char *mode );
#  define lsmash_fopen lsmash_win32_fopen
#else
#  define lsmash_fopen fopen
#endif

#ifdef _WIN32
#  include <wchar.h>
   int lsmash_string_to_wchar( int cp, const char *from, wchar_t **to );
   int lsmash_string_from_wchar( int cp, const wchar_t *from, char **to );
#endif

#endif
