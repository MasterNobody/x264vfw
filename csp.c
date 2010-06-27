/*****************************************************************************
 * csp.c: h264 encoder library
 *****************************************************************************
 * Copyright (C) 2004 Laurent Aimar
 * $Id: csp.c,v 1.1 2004/06/03 19:27:06 fenrir Exp $
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *          Anton Mitrofanov <BugMaster@narod.ru>
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

#include "x264vfw.h"

#include <assert.h>

static inline void plane_copy( uint8_t *dst, int i_dst,
                               uint8_t *src, int i_src, int w, int h )
{
    while( h-- )
    {
        memcpy( dst, src, w );
        dst += i_dst;
        src += i_src;
    }
}

static inline void plane_copy_vflip( uint8_t *dst, int i_dst,
                                     uint8_t *src, int i_src, int w, int h )
{
    plane_copy( dst, i_dst, src + (h - 1) * i_src, -i_src, w, h );
}

static inline void plane_subsamplev2( uint8_t *dst, int i_dst,
                                      uint8_t *src, int i_src, int w, int h )
{
    for( ; h > 0; h-- )
    {
        uint8_t *d = dst;
        uint8_t *s = src;
        int     i;
        for( i = w; i > 0; i-- )
        {
            *d++ = ( s[0] + s[i_src] + 1 ) >> 1;
            s++;
        }
        dst += i_dst;
        src += 2 * i_src;
    }
}

static inline void plane_subsamplev2_vflip( uint8_t *dst, int i_dst,
                                            uint8_t *src, int i_src, int w, int h )
{
    plane_subsamplev2( dst, i_dst, src + (2 * h - 1) * i_src, -i_src, w, h );
}

static inline void plane_subsamplehv2( uint8_t *dst, int i_dst,
                                       uint8_t *src, int i_src, int w, int h )
{
    for( ; h > 0; h-- )
    {
        uint8_t *d = dst;
        uint8_t *s = src;
        int     i;
        for( i = w; i > 0; i-- )
        {
            *d++ = ( s[0] + s[1] + s[i_src] + s[i_src+1] + 2 ) >> 2;
            s += 2;
        }
        dst += i_dst;
        src += 2 * i_src;
    }
}

static inline void plane_subsamplehv2_vflip( uint8_t *dst, int i_dst,
                                             uint8_t *src, int i_src, int w, int h )
{
    plane_subsamplehv2( dst, i_dst, src + (2 * h - 1) * i_src, -i_src, w, h );
}

static void i420_to_i420( x264_image_t *img_dst, x264_image_t *img_src,
                          int i_width, int i_height )
{
    if( img_src->i_csp & X264VFW_CSP_VFLIP )
    {
        plane_copy_vflip( img_dst->plane[0], img_dst->i_stride[0],
                          img_src->plane[0], img_src->i_stride[0],
                          i_width, i_height );
        plane_copy_vflip( img_dst->plane[1], img_dst->i_stride[1],
                          img_src->plane[1], img_src->i_stride[1],
                          i_width / 2, i_height / 2 );
        plane_copy_vflip( img_dst->plane[2], img_dst->i_stride[2],
                          img_src->plane[2], img_src->i_stride[2],
                          i_width / 2, i_height / 2 );
    }
    else
    {
        plane_copy( img_dst->plane[0], img_dst->i_stride[0],
                    img_src->plane[0], img_src->i_stride[0],
                    i_width, i_height );
        plane_copy( img_dst->plane[1], img_dst->i_stride[1],
                    img_src->plane[1], img_src->i_stride[1],
                    i_width / 2, i_height / 2 );
        plane_copy( img_dst->plane[2], img_dst->i_stride[2],
                    img_src->plane[2], img_src->i_stride[2],
                    i_width / 2, i_height / 2 );
    }
}

static void yv12_to_i420( x264_image_t *img_dst, x264_image_t *img_src,
                          int i_width, int i_height )
{
    if( img_src->i_csp & X264VFW_CSP_VFLIP )
    {
        plane_copy_vflip( img_dst->plane[0], img_dst->i_stride[0],
                          img_src->plane[0], img_src->i_stride[0],
                          i_width, i_height );
        plane_copy_vflip( img_dst->plane[2], img_dst->i_stride[2],
                          img_src->plane[1], img_src->i_stride[1],
                          i_width / 2, i_height / 2 );
        plane_copy_vflip( img_dst->plane[1], img_dst->i_stride[1],
                          img_src->plane[2], img_src->i_stride[2],
                          i_width / 2, i_height / 2 );
    }
    else
    {
        plane_copy( img_dst->plane[0], img_dst->i_stride[0],
                    img_src->plane[0], img_src->i_stride[0],
                    i_width, i_height );
        plane_copy( img_dst->plane[2], img_dst->i_stride[2],
                    img_src->plane[1], img_src->i_stride[1],
                    i_width / 2, i_height / 2 );
        plane_copy( img_dst->plane[1], img_dst->i_stride[1],
                    img_src->plane[2], img_src->i_stride[2],
                    i_width / 2, i_height / 2 );
    }
}

static void i422_to_i420( x264_image_t *img_dst, x264_image_t *img_src,
                          int i_width, int i_height )
{
    if( img_src->i_csp & X264VFW_CSP_VFLIP )
    {
        plane_copy_vflip( img_dst->plane[0], img_dst->i_stride[0],
                          img_src->plane[0], img_src->i_stride[0],
                          i_width, i_height );

        plane_subsamplev2_vflip( img_dst->plane[1], img_dst->i_stride[1],
                                 img_src->plane[1], img_src->i_stride[1],
                                 i_width / 2, i_height / 2 );
        plane_subsamplev2_vflip( img_dst->plane[2], img_dst->i_stride[2],
                                 img_src->plane[2], img_src->i_stride[2],
                                 i_width / 2, i_height / 2 );
    }
    else
    {
        plane_copy( img_dst->plane[0], img_dst->i_stride[0],
                    img_src->plane[0], img_src->i_stride[0],
                    i_width, i_height );

        plane_subsamplev2( img_dst->plane[1], img_dst->i_stride[1],
                           img_src->plane[1], img_src->i_stride[1],
                           i_width / 2, i_height / 2 );
        plane_subsamplev2( img_dst->plane[2], img_dst->i_stride[2],
                           img_src->plane[2], img_src->i_stride[2],
                           i_width / 2, i_height / 2 );
    }
}

static void i444_to_i420( x264_image_t *img_dst, x264_image_t *img_src,
                          int i_width, int i_height )
{
    if( img_src->i_csp & X264VFW_CSP_VFLIP )
    {
        plane_copy_vflip( img_dst->plane[0], img_dst->i_stride[0],
                          img_src->plane[0], img_src->i_stride[0],
                          i_width, i_height );

        plane_subsamplehv2_vflip( img_dst->plane[1], img_dst->i_stride[1],
                                  img_src->plane[1], img_src->i_stride[1],
                                  i_width / 2, i_height / 2 );
        plane_subsamplehv2_vflip( img_dst->plane[2], img_dst->i_stride[2],
                                  img_src->plane[2], img_src->i_stride[2],
                                  i_width / 2, i_height / 2 );
    }
    else
    {
        plane_copy( img_dst->plane[0], img_dst->i_stride[0],
                    img_src->plane[0], img_src->i_stride[0],
                    i_width, i_height );

        plane_subsamplehv2( img_dst->plane[1], img_dst->i_stride[1],
                            img_src->plane[1], img_src->i_stride[1],
                            i_width / 2, i_height / 2 );
        plane_subsamplehv2( img_dst->plane[2], img_dst->i_stride[2],
                            img_src->plane[2], img_src->i_stride[2],
                            i_width / 2, i_height / 2 );
    }
}

static void yuyv_to_i420( x264_image_t *img_dst, x264_image_t *img_src,
                          int i_width, int i_height )
{
    uint8_t *src = img_src->plane[0];
    int     i_src= img_src->i_stride[0];

    uint8_t *y   = img_dst->plane[0];
    uint8_t *u   = img_dst->plane[1];
    uint8_t *v   = img_dst->plane[2];

    if( img_src->i_csp & X264VFW_CSP_VFLIP )
    {
        src += ( i_height - 1 ) * i_src;
        i_src = -i_src;
    }

    for( ; i_height > 0; i_height -= 2 )
    {
        uint8_t *ss = src;
        uint8_t *yy = y;
        uint8_t *uu = u;
        uint8_t *vv = v;
        int w;

        for( w = i_width; w > 0; w -= 2 )
        {
            *yy++ = ss[0];
            *yy++ = ss[2];

            *uu++ = ( ss[1] + ss[1+i_src] + 1 ) >> 1;
            *vv++ = ( ss[3] + ss[3+i_src] + 1 ) >> 1;

            ss += 4;
        }
        src += i_src;
        y += img_dst->i_stride[0];
        u += img_dst->i_stride[1];
        v += img_dst->i_stride[2];

        ss = src;
        yy = y;
        for( w = i_width; w > 0; w -= 2 )
        {
            *yy++ = ss[0];
            *yy++ = ss[2];
            ss += 4;
        }
        src += i_src;
        y += img_dst->i_stride[0];
    }
}

static void uyvy_to_i420( x264_image_t *img_dst, x264_image_t *img_src,
                          int i_width, int i_height )
{
    uint8_t *src = img_src->plane[0];
    int     i_src= img_src->i_stride[0];

    uint8_t *y   = img_dst->plane[0];
    uint8_t *u   = img_dst->plane[1];
    uint8_t *v   = img_dst->plane[2];

    if( img_src->i_csp & X264VFW_CSP_VFLIP )
    {
        src += ( i_height - 1 ) * i_src;
        i_src = -i_src;
    }

    for( ; i_height > 0; i_height -= 2 )
    {
        uint8_t *ss = src;
        uint8_t *yy = y;
        uint8_t *uu = u;
        uint8_t *vv = v;
        int w;

        for( w = i_width; w > 0; w -= 2 )
        {
            *yy++ = ss[1];
            *yy++ = ss[3];

            *uu++ = ( ss[0] + ss[0+i_src] + 1 ) >> 1;
            *vv++ = ( ss[2] + ss[2+i_src] + 1 ) >> 1;

            ss += 4;
        }
        src += i_src;
        y += img_dst->i_stride[0];
        u += img_dst->i_stride[1];
        v += img_dst->i_stride[2];

        ss = src;
        yy = y;
        for( w = i_width; w > 0; w -= 2 )
        {
            *yy++ = ss[1];
            *yy++ = ss[3];
            ss += 4;
        }
        src += i_src;
        y += img_dst->i_stride[0];
    }
}

#if X264VFW_CSP_BT_709
//BT.709
#define Kb 0.0722
#define Kr 0.2126
#else
//BT.601
#define Kb 0.114
#define Kr 0.299
#endif

#define Kg (1.0 - Kb - Kr)
#define Sb (1.0 - Kb)
#define Sr (1.0 - Kr)

#if X264VFW_CSP_PC_SCALE
//PC Scale
//0 <= Y <= 255
//0 <= U <= 255
//0 <= V <= 255
#define Ky 1.0
#define Ku (0.5 / Sb)
#define Kv (0.5 / Sr)
#define Ay 0.0
#define Au 127.5
#define Av 127.5
#else
//TV Scale
//16 <= Y <= 235
//16 <= U <= 240
//16 <= V <= 240
#define Ky (1.0        * 219.0 / 255.0)
#define Ku ((0.5 / Sb) * 224.0 / 255.0)
#define Kv ((0.5 / Sr) * 224.0 / 255.0)
#define Ay 16.0
#define Au 128.0
#define Av 128.0
#endif

#define BITS      22
#define INT_FIX   (1 << BITS)
#define INT_ROUND (INT_FIX >> 1)
#define FIX(f)    ((uint32_t)((f) * INT_FIX + 0.5))

#define Y_R   FIX(Kr*Ky)
#define Y_G   FIX(Kg*Ky)
#define Y_B   FIX(Kb*Ky)
#define Y_ADD ((uint32_t)(Ay * INT_FIX + INT_ROUND + 0.5))

#define U_R   FIX(Kr*Ku)
#define U_G   FIX(Kg*Ku)
#define U_B   FIX(Sb*Ku)
#define U_ADD ((uint32_t)((Au * INT_FIX + INT_ROUND) * 4 + 0.5))

#define V_R   FIX(Sr*Kv)
#define V_G   FIX(Kg*Kv)
#define V_B   FIX(Kb*Kv)
#define V_ADD ((uint32_t)((Av * INT_FIX + INT_ROUND) * 4 + 0.5))

#define RGB_TO_I420( name, POS_R, POS_G, POS_B, S_RGB )         \
static void name( x264_image_t *img_dst, x264_image_t *img_src, \
                  int i_width, int i_height )                   \
{                                                               \
    uint8_t *src = img_src->plane[0];                           \
    int     i_src= img_src->i_stride[0];                        \
    int     i_y  = img_dst->i_stride[0];                        \
    uint8_t *y   = img_dst->plane[0];                           \
    uint8_t *u   = img_dst->plane[1];                           \
    uint8_t *v   = img_dst->plane[2];                           \
                                                                \
    if( img_src->i_csp & X264VFW_CSP_VFLIP )                    \
    {                                                           \
        src += ( i_height - 1 ) * i_src;                        \
        i_src = -i_src;                                         \
    }                                                           \
                                                                \
    for(  ; i_height > 0; i_height -= 2 )                       \
    {                                                           \
        uint8_t *ss = src;                                      \
        uint8_t *yy = y;                                        \
        uint8_t *uu = u;                                        \
        uint8_t *vv = v;                                        \
        int w;                                                  \
                                                                \
        for( w = i_width; w > 0; w -= 2 )                       \
        {                                                       \
            uint32_t cr = 0,cg = 0,cb = 0;                      \
            uint32_t r, g, b;                                   \
                                                                \
            /* Luma */                                          \
            cr = r = ss[POS_R];                                 \
            cg = g = ss[POS_G];                                 \
            cb = b = ss[POS_B];                                 \
                                                                \
            yy[0] = (Y_ADD + Y_R * r + Y_G * g + Y_B * b) >> BITS; \
                                                                \
            cr+= r = ss[POS_R+i_src];                           \
            cg+= g = ss[POS_G+i_src];                           \
            cb+= b = ss[POS_B+i_src];                           \
            yy[i_y] = (Y_ADD + Y_R * r + Y_G * g + Y_B * b) >> BITS; \
            yy++;                                               \
            ss += S_RGB;                                        \
                                                                \
            cr+= r = ss[POS_R];                                 \
            cg+= g = ss[POS_G];                                 \
            cb+= b = ss[POS_B];                                 \
                                                                \
            yy[0] = (Y_ADD + Y_R * r + Y_G * g + Y_B * b) >> BITS; \
                                                                \
            cr+= r = ss[POS_R+i_src];                           \
            cg+= g = ss[POS_G+i_src];                           \
            cb+= b = ss[POS_B+i_src];                           \
            yy[i_y] = (Y_ADD + Y_R * r + Y_G * g + Y_B * b) >> BITS; \
            yy++;                                               \
            ss += S_RGB;                                        \
                                                                \
            /* Chroma */                                        \
            *uu++ = (uint8_t)((U_ADD + U_B * cb - U_R * cr - U_G * cg) >> (BITS+2)); \
            *vv++ = (uint8_t)((V_ADD + V_R * cr - V_G * cg - V_B * cb) >> (BITS+2)); \
        }                                                       \
                                                                \
        src += 2*i_src;                                         \
        y += 2*img_dst->i_stride[0];                            \
        u += img_dst->i_stride[1];                              \
        v += img_dst->i_stride[2];                              \
    }                                                           \
}

RGB_TO_I420( rgb_to_i420,  0, 1, 2, 3 )
RGB_TO_I420( bgr_to_i420,  2, 1, 0, 3 )
RGB_TO_I420( bgra_to_i420, 2, 1, 0, 4 )

void x264vfw_csp_init( x264vfw_csp_function_t *pf, int i_x264_csp )
{
    switch( i_x264_csp )
    {
        case X264_CSP_I420:
            pf->convert[X264VFW_CSP_I420] = i420_to_i420;
            pf->convert[X264VFW_CSP_I422] = i422_to_i420;
            pf->convert[X264VFW_CSP_I444] = i444_to_i420;
            pf->convert[X264VFW_CSP_YV12] = yv12_to_i420;
            pf->convert[X264VFW_CSP_YUYV] = yuyv_to_i420;
            pf->convert[X264VFW_CSP_UYVY] = uyvy_to_i420;
            pf->convert[X264VFW_CSP_RGB ] =  rgb_to_i420;
            pf->convert[X264VFW_CSP_BGR ] =  bgr_to_i420;
            pf->convert[X264VFW_CSP_BGRA] = bgra_to_i420;
            break;

        default:
            /* For now, can't happen */
            assert(0);
            break;
    }
}
