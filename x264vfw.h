/*****************************************************************************
 * x264vfw.h: vfw x264 encoder
 *****************************************************************************
 * Copyright (C) 2003 Laurent Aimar
 * $Id: x264vfw.h,v 1.1 2004/06/03 19:27:09 fenrir Exp $
 *
 * Authors: Justin Clay
 *          Laurent Aimar <fenrir@via.ecp.fr>
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

#ifndef X264VFW_X264VFW_H
#define X264VFW_X264VFW_H

#include "common.h"
#include <vfw.h>

#if defined(HAVE_FFMPEG)
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libswscale/swscale.h>
#endif

#include "csp.h"
#include "muxers.h"
#include "resource.h"

/* Name */
#define X264_NAME_L     L"x264vfw"
#define X264_DESC_L     L"x264vfw - H.264/MPEG-4 AVC codec"

/* Codec FourCC */
#define FOURCC_X264 mmioFOURCC('X','2','6','4')

/* YUV 4:2:0 planar */
#define FOURCC_I420 mmioFOURCC('I','4','2','0')
#define FOURCC_IYUV mmioFOURCC('I','Y','U','V')
#define FOURCC_YV12 mmioFOURCC('Y','V','1','2')

/* YUV 4:2:2 packed */
#define FOURCC_YUYV mmioFOURCC('Y','U','Y','V')
#define FOURCC_YUY2 mmioFOURCC('Y','U','Y','2')
#define FOURCC_UYVY mmioFOURCC('U','Y','V','Y')
#define FOURCC_HDYC mmioFOURCC('H','D','Y','C')

#define X264VFW_WEBSITE "http://sourceforge.net/projects/x264vfw/"

/* Limits */
#define X264_BITRATE_MAX 999999
#define X264_QUANT_MAX   51
#define X264_BFRAME_MAX  16
#define X264_THREAD_MAX  128

#define MAX_STATS_PATH   (MAX_PATH - 5) /* -5 because x264 add ".temp" for temp file */
#define MAX_STATS_SIZE   X264_MAX(MAX_STATS_PATH, MAX_PATH)
#define MAX_CMDLINE      4096
#define MAX_ZONES_SIZE   4096

typedef char fourcc_str[4 + 1];

/* CONFIG: VFW config */
typedef struct
{
    int b_save;

    /* Main */
    int i_encoding_type;
    int i_qp;
    int i_rf_constant;  /* 1pass VBR, nominal QP */
    int i_passbitrate;
    int i_pass;
    int b_fast1pass;    /* Turns off some flags during 1st pass */
    int b_createstats;  /* Creates the statsfile in single pass mode */
    int b_updatestats;  /* Updates the statsfile during 2nd pass */
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
    fourcc_str fcc;     /* FourCC used */
#if X264VFW_USE_VIRTUALDUB_HACK
    int b_vd_hack;
#endif
#if defined(HAVE_FFMPEG) && X264VFW_USE_DECODER
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
    int i_p_wpred;
    int i_bframe;
    int i_bframe_adaptive;
    int i_bframe_bias;
    int i_direct_mv_pred;
    int i_b_refs;
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

    /* Multithreading */
#if X264VFW_USE_THREADS
    int i_threads;
    int b_mt_deterministic;
    int b_mt_sliced;
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

/* CODEC: VFW codec instance */
typedef struct
{
    CONFIG config;

    /* ICM_COMPRESS_FRAMES_INFO params */
    int i_frame_total;
    int i_fps_num;
    int i_fps_den;

    /* x264 handle */
    x264_t *h;

    /* Log console handle */
    HWND hCons;
    int b_visible;

#if X264VFW_USE_BUGGY_APPS_HACK
    BITMAPINFOHEADER *prev_lpbiOutput;
    DWORD prev_output_biSizeImage;
    int b_check_size;
#endif
#if X264VFW_USE_VIRTUALDUB_HACK
    int b_use_vd_hack;
    DWORD save_fcc;
#endif
    int b_no_output;
    int i_frame_remain;
    int b_encoder_error;

    x264vfw_csp_function_t csp;
    x264_picture_t conv_pic;

    int b_cli_output;
    cli_output_t cli_output;
    hnd_t cli_hout;

#if defined(HAVE_FFMPEG) && X264VFW_USE_DECODER
    int               decoder_enabled;
    AVCodec           *decoder;
    AVCodecContext    *decoder_context;
    AVFrame           *decoder_frame;
    void              *decoder_buf;
    unsigned int      decoder_buf_size;
    AVPacket          decoder_pkt;
    enum PixelFormat  decoder_pix_fmt;
    int               decoder_vflip;
    int               decoder_swap_UV;
    struct SwsContext *sws;
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

#if defined(HAVE_FFMPEG) && X264VFW_USE_DECODER
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
void x264vfw_log(CODEC *codec, int i_level, const char *psz_fmt, ...);

/* Config functions */
void config_reg_load(CONFIG *config);
void config_reg_save(CONFIG *config);
void config_defaults(CONFIG *config);

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

#endif
