/*****************************************************************************
 * csp.c: colorspace conversion functions
 *****************************************************************************
 * Copyright (C) 2004-2011 x264vfw project
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

#define BITS         22
#define INT_FIX      (1 << BITS)
#define INT_ROUND    (INT_FIX >> 1)
#define FIX(f)       ((uint32_t)((f) * INT_FIX + 0.5))

#define Kb_601       0.114
#define Kr_601       0.299
#define Kb_709       0.0722
#define Kr_709       0.2126

#define Kg( rec )    (1.0 - Kb_##rec - Kr_##rec)
#define Sb( rec )    (1.0 - Kb_##rec)
#define Sr( rec )    (1.0 - Kr_##rec)

#define Ky_tv        (1.0             * 219.0 / 255.0)
#define Ku_tv( rec ) ((0.5 / Sb(rec)) * 224.0 / 255.0)
#define Kv_tv( rec ) ((0.5 / Sr(rec)) * 224.0 / 255.0)
#define Ay_tv        16.0
#define Au_tv        128.0
#define Av_tv        128.0

#define Ky_pc        1.0
#define Ku_pc( rec ) (0.5 / Sb(rec))
#define Kv_pc( rec ) (0.5 / Sr(rec))
#define Ay_pc        0.0
#define Au_pc        127.5
#define Av_pc        127.5

#define Y_R( rec, scale )   FIX(Kr_##rec * Ky_##scale)
#define Y_G( rec, scale )   FIX(Kg(rec)  * Ky_##scale)
#define Y_B( rec, scale )   FIX(Kb_##rec * Ky_##scale)
#define Y_ADD( scale )      ((uint32_t)(Ay_##scale * INT_FIX + INT_ROUND + 0.5))

#define U_R( rec, scale )   FIX(Kr_##rec * Ku_##scale(rec))
#define U_G( rec, scale )   FIX(Kg(rec)  * Ku_##scale(rec))
#define U_B( rec, scale )   FIX(Sb(rec)  * Ku_##scale(rec))
#define U_ADD( scale )      ((uint32_t)((Au_##scale * INT_FIX + INT_ROUND) * 4 + 0.5))

#define V_R( rec, scale )   FIX(Sr(rec)  * Kv_##scale(rec))
#define V_G( rec, scale )   FIX(Kg(rec)  * Kv_##scale(rec))
#define V_B( rec, scale )   FIX(Kb_##rec * Kv_##scale(rec))
#define V_ADD( scale )      ((uint32_t)((Av_##scale * INT_FIX + INT_ROUND) * 4 + 0.5))

#define RGB_TO_I420( name, POS_R, POS_G, POS_B, S_RGB, rec, scale )               \
static void name##_##rec##_##scale( x264_image_t *img_dst, x264_image_t *img_src, \
                                    int i_width, int i_height )                   \
{                                                                                 \
    uint8_t *src = img_src->plane[0];                                             \
    int     i_src= img_src->i_stride[0];                                          \
    int     i_y  = img_dst->i_stride[0];                                          \
    uint8_t *y   = img_dst->plane[0];                                             \
    uint8_t *u   = img_dst->plane[1];                                             \
    uint8_t *v   = img_dst->plane[2];                                             \
                                                                                  \
    if( img_src->i_csp & X264VFW_CSP_VFLIP )                                      \
    {                                                                             \
        src += ( i_height - 1 ) * i_src;                                          \
        i_src = -i_src;                                                           \
    }                                                                             \
                                                                                  \
    for(  ; i_height > 0; i_height -= 2 )                                         \
    {                                                                             \
        uint8_t *ss = src;                                                        \
        uint8_t *yy = y;                                                          \
        uint8_t *uu = u;                                                          \
        uint8_t *vv = v;                                                          \
        int w;                                                                    \
                                                                                  \
        for( w = i_width; w > 0; w -= 2 )                                         \
        {                                                                         \
            uint32_t cr = 0,cg = 0,cb = 0;                                        \
            uint32_t r, g, b;                                                     \
                                                                                  \
            /* Luma */                                                            \
            cr = r = ss[POS_R];                                                   \
            cg = g = ss[POS_G];                                                   \
            cb = b = ss[POS_B];                                                   \
                                                                                  \
            yy[0] = (Y_ADD(scale) +                                               \
                     Y_R(rec, scale) * r +                                        \
                     Y_G(rec, scale) * g +                                        \
                     Y_B(rec, scale) * b) >> BITS;                                \
                                                                                  \
            cr+= r = ss[POS_R+i_src];                                             \
            cg+= g = ss[POS_G+i_src];                                             \
            cb+= b = ss[POS_B+i_src];                                             \
                                                                                  \
            yy[i_y] = (Y_ADD(scale) +                                             \
                       Y_R(rec, scale) * r +                                      \
                       Y_G(rec, scale) * g +                                      \
                       Y_B(rec, scale) * b) >> BITS;                              \
            yy++;                                                                 \
            ss += S_RGB;                                                          \
                                                                                  \
            cr+= r = ss[POS_R];                                                   \
            cg+= g = ss[POS_G];                                                   \
            cb+= b = ss[POS_B];                                                   \
                                                                                  \
            yy[0] = (Y_ADD(scale) +                                               \
                     Y_R(rec, scale) * r +                                        \
                     Y_G(rec, scale) * g +                                        \
                     Y_B(rec, scale) * b) >> BITS;                                \
                                                                                  \
            cr+= r = ss[POS_R+i_src];                                             \
            cg+= g = ss[POS_G+i_src];                                             \
            cb+= b = ss[POS_B+i_src];                                             \
                                                                                  \
            yy[i_y] = (Y_ADD(scale) +                                             \
                       Y_R(rec, scale) * r +                                      \
                       Y_G(rec, scale) * g +                                      \
                       Y_B(rec, scale) * b) >> BITS;                              \
            yy++;                                                                 \
            ss += S_RGB;                                                          \
                                                                                  \
            /* Chroma */                                                          \
            *uu++ = (uint8_t)((U_ADD(scale) +                                     \
                               U_B(rec, scale) * cb -                             \
                               U_R(rec, scale) * cr -                             \
                               U_G(rec, scale) * cg) >> (BITS+2));                \
                                                                                  \
            *vv++ = (uint8_t)((V_ADD(scale) +                                     \
                               V_R(rec, scale) * cr -                             \
                               V_G(rec, scale) * cg -                             \
                               V_B(rec, scale) * cb) >> (BITS+2));                \
        }                                                                         \
                                                                                  \
        src += 2*i_src;                                                           \
        y += 2*img_dst->i_stride[0];                                              \
        u += img_dst->i_stride[1];                                                \
        v += img_dst->i_stride[2];                                                \
    }                                                                             \
}

RGB_TO_I420(  rgb_to_i420, 0, 1, 2, 3, 601, tv )
RGB_TO_I420(  bgr_to_i420, 2, 1, 0, 3, 601, tv )
RGB_TO_I420( bgra_to_i420, 2, 1, 0, 4, 601, tv )
RGB_TO_I420(  rgb_to_i420, 0, 1, 2, 3, 601, pc )
RGB_TO_I420(  bgr_to_i420, 2, 1, 0, 3, 601, pc )
RGB_TO_I420( bgra_to_i420, 2, 1, 0, 4, 601, pc )
RGB_TO_I420(  rgb_to_i420, 0, 1, 2, 3, 709, tv )
RGB_TO_I420(  bgr_to_i420, 2, 1, 0, 3, 709, tv )
RGB_TO_I420( bgra_to_i420, 2, 1, 0, 4, 709, tv )
RGB_TO_I420(  rgb_to_i420, 0, 1, 2, 3, 709, pc )
RGB_TO_I420(  bgr_to_i420, 2, 1, 0, 3, 709, pc )
RGB_TO_I420( bgra_to_i420, 2, 1, 0, 4, 709, pc )

void x264vfw_csp_init( x264vfw_csp_function_t *pf, int i_x264_csp, int i_colmatrix, int b_fullrange )
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
            if( i_colmatrix == 1 )
            {
                // BT.709
                if( b_fullrange )
                {
                    // PC Scale
                    pf->convert[X264VFW_CSP_RGB ] =  rgb_to_i420_709_pc;
                    pf->convert[X264VFW_CSP_BGR ] =  bgr_to_i420_709_pc;
                    pf->convert[X264VFW_CSP_BGRA] = bgra_to_i420_709_pc;
                }
                else
                {
                    // TV Scale
                    pf->convert[X264VFW_CSP_RGB ] =  rgb_to_i420_709_tv;
                    pf->convert[X264VFW_CSP_BGR ] =  bgr_to_i420_709_tv;
                    pf->convert[X264VFW_CSP_BGRA] = bgra_to_i420_709_tv;
                }
            }
            else
            {
                // BT.601
                if( b_fullrange )
                {
                    // PC Scale
                    pf->convert[X264VFW_CSP_RGB ] =  rgb_to_i420_601_pc;
                    pf->convert[X264VFW_CSP_BGR ] =  bgr_to_i420_601_pc;
                    pf->convert[X264VFW_CSP_BGRA] = bgra_to_i420_601_pc;
                }
                else
                {
                    // TV Scale
                    pf->convert[X264VFW_CSP_RGB ] =  rgb_to_i420_601_tv;
                    pf->convert[X264VFW_CSP_BGR ] =  bgr_to_i420_601_tv;
                    pf->convert[X264VFW_CSP_BGRA] = bgra_to_i420_601_tv;
                }
            }
            break;

        default:
            /* For now, can't happen */
            assert(0);
            break;
    }
}
