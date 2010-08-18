/*****************************************************************************
 * x264cli.h: x264cli common
 *****************************************************************************
 * Copyright (C) 2003-2010 x264 project
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

static inline int64_t gcd( int64_t a, int64_t b )
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

static inline int64_t lcm( int64_t a, int64_t b )
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

void x264_cli_log( const char *name, int i_level, const char *fmt, ... );
void x264_cli_printf( int i_level, const char *fmt, ... );

#define RETURN_IF_ERR( cond, name, ret, ... )\
if( cond )\
{\
    x264_cli_log( name, X264_LOG_ERROR, __VA_ARGS__ );\
    return ret;\
}

#define FAIL_IF_ERR( cond, name, ... ) RETURN_IF_ERR( cond, name, -1, __VA_ARGS__ )

#endif
