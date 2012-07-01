/*****************************************************************************
 * x264vfw.h: x264vfw main header
 *****************************************************************************
 * Copyright (C) 2003-2012 x264vfw project
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
#include "x264cli.h"
#include "output/output.h"
#include "resource.h"

/* Name */
#define X264VFW_NAME_L L"x264vfw"
#define X264VFW_DESC_L L"x264vfw - H.264/MPEG-4 AVC codec"

/* Codec FourCC */
#define FOURCC_X264 mmioFOURCC('X','2','6','4')

/* YUV 4:2:0 planar */
#define FOURCC_I420 mmioFOURCC('I','4','2','0')
#define FOURCC_IYUV mmioFOURCC('I','Y','U','V')
#define FOURCC_YV12 mmioFOURCC('Y','V','1','2')
/* YUV 4:2:2 planar */
#define FOURCC_YV16 mmioFOURCC('Y','V','1','6')
/* YUV 4:4:4 planar */
#define FOURCC_YV24 mmioFOURCC('Y','V','2','4')
/* YUV 4:2:0, with one Y plane and one packed U+V */
#define FOURCC_NV12 mmioFOURCC('N','V','1','2')
/* YUV 4:2:2 packed */
#define FOURCC_YUYV mmioFOURCC('Y','U','Y','V')
#define FOURCC_YUY2 mmioFOURCC('Y','U','Y','2')
#define FOURCC_UYVY mmioFOURCC('U','Y','V','Y')
#define FOURCC_HDYC mmioFOURCC('H','D','Y','C')

#define X264VFW_WEBSITE "http://sourceforge.net/projects/x264vfw/"

#define X264VFW_FORMAT_VERSION 2

/* Limits */
#define MAX_QUANT   51
#define MAX_BITRATE 999999

#define MAX_STATS_PATH   (MAX_PATH - 5) /* -5 because x264 add ".temp" for temp file */
#define MAX_STATS_SIZE   X264_MAX(MAX_STATS_PATH, MAX_PATH)
#define MAX_OUTPUT_PATH  MAX_PATH
#define MAX_OUTPUT_SIZE  X264_MAX(MAX_OUTPUT_PATH, MAX_PATH)
#define MAX_CMDLINE      4096

#define COUNT_PRESET  10
#define COUNT_TUNE    7
#define COUNT_PROFILE 7
#define COUNT_LEVEL   18
#define COUNT_FOURCC  7

/* Types */
typedef struct
{
    const char * const name;
    const char * const value;
} named_str_t;

typedef struct
{
    const char * const name;
    const DWORD value;
} named_fourcc_t;

typedef struct
{
    const char * const name;
    const int value;
} named_int_t;

/* CONFIG: VFW config */
typedef struct
{
    int i_format_version;
    /* Basic */
    int i_preset;
    int i_tuning;
    int i_profile;
    int i_level;
    int b_fastdecode;
    int b_zerolatency;
    int b_keep_input_csp;
    /* Rate control */
    int i_encoding_type;
    int i_qp;
    int i_rf_constant;  /* 1pass VBR, nominal QP */
    int i_passbitrate;
    int i_pass;
    int b_fast1pass;    /* Turns off some flags during 1st pass */
    int b_createstats;  /* Creates the statsfile in single pass mode */
    int b_updatestats;  /* Updates the statsfile during 2nd pass */
    char stats[MAX_STATS_SIZE];
    /* Output */
    int i_output_mode;
    int i_fourcc;
#if X264VFW_USE_VIRTUALDUB_HACK
    int b_vd_hack;
#else
    int reserved_b_vd_hack;
#endif
    char output_file[MAX_OUTPUT_SIZE];
    /* Sample Aspect Ratio */
    int i_sar_width;
    int i_sar_height;
    /* Debug */
    int i_log_level;
    int b_psnr;
    int b_ssim;
    int b_no_asm;
    /* Decoder && AVI Muxer */
#if defined(HAVE_FFMPEG) && X264VFW_USE_DECODER
    int b_disable_decoder;
#else
    int reserved_b_disable_decoder;
#endif
    /* Extra command line */
    char extra_cmdline[MAX_CMDLINE];
} CONFIG;

typedef struct
{
    CONFIG config;
    HWND hMainDlg;
    HWND hHelpDlg;
    int b_dlg_updated;
    int b_save;
} CONFIG_DATA;

/* CODEC: VFW codec instance */
typedef struct
{
    /* x264 handle */
    x264_t *h;

    /* Configuration GUI params */
    CONFIG config;

    /* Internal codec params */
    int b_encoder_error;
#if X264VFW_USE_BUGGY_APPS_HACK
    BITMAPINFOHEADER *prev_lpbiOutput;
    DWORD prev_output_biSizeImage;
    int b_check_size;
#endif
#if X264VFW_USE_VIRTUALDUB_HACK
    int b_use_vd_hack;
    DWORD save_fourcc;
#endif
    int b_no_output;
    int b_fast1pass;
    int b_user_ref;
    int i_frame_remain;
    int b_warn_frame_loss;
    int b_flush_delayed;

    /* Preset/Tuning/Profile */
    const char *preset;
    const char *tune;
    const char *profile;

    /* ICM_COMPRESS_FRAMES_INFO params */
    int i_frame_total;
    uint32_t i_fps_num;
    uint32_t i_fps_den;

    /* Colorspace conversion */
    x264vfw_csp_function_t csp;
    x264_picture_t conv_pic;

    /* Log console */
    HWND hCons;
    int b_visible;

    /* CLI output */
    int b_cli_output;
    char *cli_output_file;
    const char *cli_output_muxer;
    cli_output_t cli_output;
    cli_output_opt_t cli_output_opt;
    hnd_t cli_hout;

    /* Decoder */
#if defined(HAVE_FFMPEG) && X264VFW_USE_DECODER
    int               decoder_enabled;
    int               decoder_is_avc;
    AVCodec           *decoder;
    AVCodecContext    *decoder_context;
    AVFrame           *decoder_frame;
    void              *decoder_extradata;
    void              *decoder_buf;
    DWORD             decoder_buf_size;
    AVPacket          decoder_pkt;
    enum PixelFormat  decoder_pix_fmt;
    int               decoder_vflip;
    int               decoder_swap_UV;
    struct SwsContext *sws;
#endif
} CODEC;

/* Compress functions */
LRESULT x264vfw_compress_get_format(CODEC *, BITMAPINFO *, BITMAPINFO *);
LRESULT x264vfw_compress_get_size(CODEC *, BITMAPINFO *, BITMAPINFO *);
LRESULT x264vfw_compress_query(CODEC *, BITMAPINFO *, BITMAPINFO *);
LRESULT x264vfw_compress_begin(CODEC *, BITMAPINFO *, BITMAPINFO *);
LRESULT x264vfw_compress(CODEC *, ICCOMPRESS *);
LRESULT x264vfw_compress_end(CODEC *);
LRESULT x264vfw_compress_frames_info(CODEC *, ICCOMPRESSFRAMES *);
void x264vfw_default_compress_frames_info(CODEC *);

#if defined(HAVE_FFMPEG) && X264VFW_USE_DECODER
/* Decompress functions */
LRESULT x264vfw_decompress_get_format(CODEC *, BITMAPINFO *, BITMAPINFO *);
LRESULT x264vfw_decompress_query(CODEC *, BITMAPINFO *, BITMAPINFO *);
LRESULT x264vfw_decompress_begin(CODEC *, BITMAPINFO *, BITMAPINFO *);
LRESULT x264vfw_decompress(CODEC *, ICDECOMPRESS *);
LRESULT x264vfw_decompress_end(CODEC *);
#endif

/* Log functions */
void x264vfw_log_create(CODEC *codec);
void x264vfw_log_destroy(CODEC *codec);
void x264vfw_log(CODEC *codec, int i_level, const char *psz_fmt, ...);

/* Config functions */
void x264vfw_config_defaults(CONFIG *config);
void x264vfw_config_reg_load(CONFIG *config);
void x264vfw_config_reg_save(CONFIG *config);

/* Dialog callbacks */
INT_PTR CALLBACK x264vfw_callback_main(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK x264vfw_callback_about(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK x264vfw_callback_log(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK x264vfw_callback_help(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);

/* DLL instance */
extern HINSTANCE x264vfw_hInst;
/* DLL critical section */
extern CRITICAL_SECTION x264vfw_CS;

#endif
