/*****************************************************************************
 * csp.h: colorspace conversion functions
 *****************************************************************************
 * Copyright (C) 2004-2012 x264vfw project
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

#ifndef X264VFW_CSP_H
#define X264VFW_CSP_H

#include "common.h"

/* Colorspace type */
#define X264VFW_CSP_MASK           0x00ff  /* */
#define X264VFW_CSP_NONE           0x0000  /* Invalid mode     */
#define X264VFW_CSP_I420           0x0001  /* yuv 4:2:0 planar */
#define X264VFW_CSP_I422           0x0002  /* yuv 4:2:2 planar */
#define X264VFW_CSP_I444           0x0003  /* yuv 4:4:4 planar */
#define X264VFW_CSP_YV12           0x0004  /* yuv 4:2:0 planar */
#define X264VFW_CSP_YUYV           0x0005  /* yuv 4:2:2 packed */
#define X264VFW_CSP_UYVY           0x0006  /* yuv 4:2:2 packed */
#define X264VFW_CSP_RGB            0x0007  /* rgb 24bits       */
#define X264VFW_CSP_BGR            0x0008  /* bgr 24bits       */
#define X264VFW_CSP_BGRA           0x0009  /* bgr 32bits       */
#define X264VFW_CSP_MAX            0x0010  /* end of list */
#define X264VFW_CSP_VFLIP          0x1000  /* */

typedef void (*x264vfw_csp_t) ( x264_image_t *, x264_image_t *,
                                int i_width, int i_height );

typedef struct
{
    x264vfw_csp_t convert[X264VFW_CSP_MAX];
} x264vfw_csp_function_t;

void x264vfw_csp_init( x264vfw_csp_function_t *pf, int i_x264_csp, int i_colmatrix, int b_fullrange );

#endif
