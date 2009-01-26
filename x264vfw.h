/*****************************************************************************
 * x264vfw.h: vfw x264 encoder
 *****************************************************************************
 * Copyright (C) 2003 Laurent Aimar
 * $Id: x264vfw.h,v 1.1 2004/06/03 19:27:09 fenrir Exp $
 *
 * Authors: Justin Clay
 *          Laurent Aimar <fenrir@via.ecp.fr>
 *          Anton Mitrofanov (a.k.a. BugMaster)
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

#ifndef X264VFW_X264VFW_H
#define X264VFW_X264VFW_H

#include "x264vfw_config.h"

#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <windows.h>
#include <vfw.h>

#include <x264.h>
#if X264VFW_USE_DECODER
#include <avcodec.h>
#include <avutil.h>

#if !FFMPEG_HAVE_IMG_CONVERT
#include <swscale.h>
#else
int img_convert(AVPicture *dst, int dst_pix_fmt,
                const AVPicture *src, int src_pix_fmt,
                int src_width, int src_height);
#endif
#endif

#include "csp.h"
#include "resource.h"

#ifdef _MSC_VER
#define inline __inline
#endif

#if defined(__GNUC__) && (__GNUC__ > 3 || __GNUC__ == 3 && __GNUC_MINOR__ > 0)
#define UNUSED __attribute__((unused))
#define ALWAYS_INLINE __attribute__((always_inline)) inline
#define NOINLINE __attribute__((noinline))
#else
#define UNUSED
#define ALWAYS_INLINE inline
#define NOINLINE
#endif

#if defined(__GNUC__) && (__GNUC__ > 4 || __GNUC__ == 4 && __GNUC_MINOR__>1)
#define attribute_align_arg __attribute__((force_align_arg_pointer))
#else
#define attribute_align_arg
#endif

#define X264_MAX(a, b) (((a)>(b)) ? (a) : (b))
#define X264_MIN(a, b) (((a)<(b)) ? (a) : (b))
#define X264_CLIP(v, min, max) (((v)<(min)) ? (min) : ((v)>(max)) ? (max) : (v))

/* Name */
#define X264_NAME_L     L"x264vfw"
#define X264_DESC_L     L"x264vfw - H.264/MPEG-4 AVC codec"

/* Codec fcc */
#define FOURCC_X264 mmioFOURCC('X','2','6','4')

/* YUV 4:2:0 planar */
#define FOURCC_I420 mmioFOURCC('I','4','2','0')
#define FOURCC_IYUV mmioFOURCC('I','Y','U','V')
#define FOURCC_YV12 mmioFOURCC('Y','V','1','2')

/* YUV 4:2:2 packed */
#define FOURCC_YUY2 mmioFOURCC('Y','U','Y','2')
#define FOURCC_YUYV mmioFOURCC('Y','U','Y','V')

#define X264VFW_WEBSITE "http://sourceforge.net/projects/x264vfw/"

/* Limits */
#define X264_BITRATE_MAX 20000
#define X264_QUANT_MAX   51
#define X264_BFRAME_MAX  16
#define X264_THREAD_MAX  128

#define MAX_STATS_PATH   (MAX_PATH - 5) // -5 because x264 add ".temp" for temp file
#define MAX_STATS_SIZE   X264_MAX(MAX_STATS_PATH, MAX_PATH)
#define MAX_CMDLINE      4096
#define MAX_ZONES_SIZE   4096

typedef char fourcc_str[4 + 1];

/* CONFIG: vfw config */
typedef struct
{
    int b_save;

    /* Main */
    int i_encoding_type;
    int i_qp;
    int i_rf_constant;  /* 1pass VBR, nominal QP */
    int i_passbitrate;
    int i_pass;
    int b_fast1pass;    /* turns off some flags during 1st pass */
    int b_updatestats;  /* updates the statsfile during 2nd pass */
    char stats[MAX_STATS_SIZE];
    char extra_cmdline[MAX_CMDLINE];

    /* Misc */
    int i_avc_level;
    int i_sar_width;
    int i_sar_height;

    /* Debug */
    int i_log_level;
    int b_psnr;
    int b_ssim;
    int b_no_asm;

    /* VFW */
    int i_fcc_num;
    fourcc_str fcc;     /* fourcc used */
#if X264VFW_USE_VIRTUALDUB_HACK
    int b_vd_hack;
#endif
#if X264VFW_USE_DECODER
    int b_disable_decoder;
#endif

    /* Analysis */
    int b_dct8x8;
    int b_intra_i8x8;
    int b_intra_i4x4;
    int b_i8x8;
    int b_i4x4;
    int b_psub16x16;
    int b_psub8x8;
    int b_bsub16x16;
    int b_fast_pskip;
    int i_refmax;
    int b_mixedref;
    int i_me_method;
    int i_me_range;
    int i_subpel_refine;
    int b_chroma_me;
    float f_psy_rdo;
    int i_keyint_min;
    int i_keyint_max;
    int i_scenecut_threshold;
    int b_pre_scenecut;
    int i_bframe;
    int i_bframe_adaptive;
    int i_bframe_bias;
    int i_direct_mv_pred;
    int b_b_refs;
    int b_b_wpred;

    /* Encoding */
    int b_filter;
    int i_inloop_a;
    int i_inloop_b;
    int i_interlaced;
    int b_cabac;
    int b_dct_decimate;
    int i_noise_reduction;
    int i_trellis;
    int i_intra_deadzone;
    int i_inter_deadzone;
    int i_cqm;
    char cqmfile[MAX_PATH];
    float f_psy_trellis;

    /* Rate control */
    int i_vbv_maxrate;
    int i_vbv_bufsize;
    int i_vbv_occupancy;
    int i_qp_min;
    int i_qp_max;
    int i_qp_step;
    float f_ipratio;
    float f_pbratio;
    int i_chroma_qp_offset;
    float f_qcomp;
    float f_cplxblur;
    float f_qblur;
    float f_ratetol;

    /* AQ */
    int i_aq_mode;
    float f_aq_strength;
#if X264_PATCH_VAQ_MOD
    int i_aq_metric;
    float f_aq_sensitivity;
#endif

    /* Multithreading */
#if X264VFW_USE_THREADS
    int i_threads;
    int b_mt_deterministic;
#if X264_PATCH_THREAD_POOL
    int i_thread_queue;
#endif
#endif

    /* VUI */
    int i_overscan;
    int i_vidformat;
    int i_fullrange;
    int i_colorprim;
    int i_transfer;
    int i_colmatrix;
    int i_chromaloc;

    /* Config */
    int b_use_cmdline;
    char cmdline[MAX_CMDLINE];
} CONFIG;

/* CODEC: vfw codec instance */
typedef struct
{
    CONFIG config;

    /* ICM_COMPRESS_FRAMES_INFO params */
    int i_frame_total;
    int i_fps_num;
    int i_fps_den;

    /* x264 handle */
    x264_t *h;

    /* log console handle */
    HWND hCons;
    BOOL b_visible;

#if X264VFW_USE_BUGGY_APPS_HACK
    BITMAPINFOHEADER *prev_lpbiOutput;
    DWORD prev_output_biSizeImage;
#endif
#if X264VFW_USE_VIRTUALDUB_HACK
    int b_use_vd_hack;
    DWORD save_fcc;
#endif
    int b_no_output;
    int i_frame_remain;

    x264_csp_function_t csp;
    x264_picture_t conv_pic;

#if X264VFW_USE_DECODER
    int               decoder_enabled;
    AVCodec           *decoder;
    AVCodecContext    *decoder_context;
    AVFrame           *decoder_frame;
    void              *decoder_buf;
    unsigned int      decoder_buf_size;
    enum PixelFormat  decoder_pix_fmt;
    int               decoder_vflip;
    int               decoder_swap_UV;
#if !FFMPEG_HAVE_IMG_CONVERT
    struct SwsContext *sws;
#endif
#endif
} CODEC;

/* Compress functions */
LRESULT compress_get_format(CODEC *, BITMAPINFO *, BITMAPINFO *);
LRESULT compress_get_size(CODEC *, BITMAPINFO *, BITMAPINFO *);
LRESULT compress_query(CODEC *, BITMAPINFO *, BITMAPINFO *);
LRESULT compress_begin(CODEC *, BITMAPINFO *, BITMAPINFO *);
LRESULT compress(CODEC *, ICCOMPRESS *);
LRESULT compress_end(CODEC *);
LRESULT compress_frames_info(CODEC *, ICCOMPRESSFRAMES *);
void default_compress_frames_info(CODEC *);

#if X264VFW_USE_DECODER
/* Decompress functions */
LRESULT decompress_get_format(CODEC *, BITMAPINFO *, BITMAPINFO *);
LRESULT decompress_query(CODEC *, BITMAPINFO *, BITMAPINFO *);
LRESULT decompress_begin(CODEC *, BITMAPINFO *, BITMAPINFO *);
LRESULT decompress(CODEC *, ICDECOMPRESS *);
LRESULT decompress_end(CODEC *);
#endif

/* Log functions */
void x264_log_vfw_create(CODEC *codec);
void x264_log_vfw_destroy(CODEC *codec);

/* Config functions */
void config_reg_load(CONFIG *config);
void config_reg_save(CONFIG *config);

/* Dialog callbacks */
INT_PTR CALLBACK callback_main (HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK callback_tabs(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK callback_about(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK callback_err_console(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);

/* DLL instance */
extern HINSTANCE g_hInst;

/* Supported FourCC codes */
/* FIXME: static is not good, but sizeof operator doesn't work with extern */
static const fourcc_str fcc_str_table[] =
{
    "H264\0",
    "h264\0",
    "X264\0",
    "x264\0",
    "AVC1\0",
    "avc1\0",
    "VSSH\0"
};

#ifdef _DEBUG
#include <stdio.h> /* vsprintf */
#define DPRINTF_BUF_SZ 1024
static inline void DPRINTF(const char *fmt, ...)
{
    va_list args;
    char buf[DPRINTF_BUF_SZ];

    va_start(args, fmt);
    vsprintf(buf, fmt, args);
    va_end(args);
    OutputDebugString(buf);
}
static inline void DVPRINTF(const char *fmt, va_list args)
{
    char buf[DPRINTF_BUF_SZ];

    vsprintf(buf, fmt, args);
    OutputDebugString(buf);
}
#else
static inline void DPRINTF(const char *fmt, ...) { }
static inline void DVPRINTF(const char *fmt, va_list args) {}
#endif

#endif
