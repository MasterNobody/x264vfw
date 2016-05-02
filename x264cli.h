/*****************************************************************************
 * x264cli.h: x264cli common
 *****************************************************************************
 * Copyright (C) 2003-2016 x264vfw project
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *          Loren Merritt <lorenm@u.washington.edu>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111, USA.
 *****************************************************************************/

#ifndef X264_CLI_H
#define X264_CLI_H

#include "common.h"

typedef void *hnd_t;

static inline uint64_t gcd( uint64_t a, uint64_t b )
{
    while( 1 )
    {
        int64_t c = a % b;
        if( !c )
            return b;
        a = b;
        b = c;
    }
}

static inline uint64_t lcm( uint64_t a, uint64_t b )
{
    return ( a / gcd( a, b ) ) * b;
}

static inline char *get_filename_extension( char *filename )
{
    char *ext = filename + strlen( filename );
    while( *ext != '.' && ext > filename )
        ext--;
    ext += *ext == '.';
    return ext;
}

void x264vfw_cli_log( void *p_private, const char *name, int i_level, const char *psz_fmt, ... );

#ifdef _WIN32
#define utf8_to_utf16( utf8, utf16 )\
    MultiByteToWideChar( CP_UTF8, MB_ERR_INVALID_CHARS, utf8, -1, utf16, sizeof(utf16)/sizeof(wchar_t) )
FILE *x264vfw_fopen( const char *filename, const char *mode );
int x264vfw_rename( const char *oldname, const char *newname );
#define x264vfw_struct_stat struct _stati64
#define x264vfw_fstat _fstati64
int x264vfw_stat( const char *path, x264vfw_struct_stat *buf );
int x264vfw_is_pipe( const char *path );
#else
#define x264vfw_fopen       fopen
#define x264vfw_rename      rename
#define x264vfw_struct_stat struct stat
#define x264vfw_fstat       fstat
#define x264vfw_stat        stat
#define x264vfw_is_pipe(x)  0
#endif

static inline int x264vfw_is_regular_file( FILE *filehandle )
{
    x264vfw_struct_stat file_stat;
    if( x264vfw_fstat( fileno( filehandle ), &file_stat ) )
        return 1;
    return S_ISREG( file_stat.st_mode );
}

static inline int x264vfw_is_regular_file_path( const char *filename )
{
    x264vfw_struct_stat file_stat;
    if( x264vfw_stat( filename, &file_stat ) )
        return !x264vfw_is_pipe( filename );
    return S_ISREG( file_stat.st_mode );
}

#endif
