#ifndef _X264_VFW_H
#define _X264_VFW_H

#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <windows.h>
#include <vfw.h>

#include <x264.h>

#include "resource.h"

/* Name */
#define X264_NAME_L     L"x264vfw"
#define X264_DESC_L     L"x264vfw - H.264/MPEG-4 AVC encoder"

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
#define X264_BITRATE_MAX 9999
#define X264_QUANT_MAX   51
#define X264_BFRAME_MAX  16
#define X264_THREAD_MAX  128

typedef char fourcc_str[4 + 1];

/* CONFIG: vfw config */
typedef struct
{
    int b_save;

    /* Bitrate */
    int i_encoding_type;
    int i_qp;
    int i_rf_constant;  /* 1pass VBR, nominal QP */
    int i_passbitrate;
    int i_pass;
    int b_fast1pass;    /* turns off some flags during 1st pass */
    int b_updatestats;  /* updates the statsfile during 2nd pass */
    char stats[MAX_PATH];

    /* Rate Control */
    int i_key_boost;
    int i_b_red;
    int i_curve_comp;
    int i_qp_min;
    int i_qp_max;
    int i_qp_step;
    int i_scenecut_threshold;
    int i_keyint_min;
    int i_keyint_max;

    /* MBs & Frames */
    int b_dct8x8;
    int b_i8x8;
    int b_i4x4;
    int b_psub16x16;
    int b_psub8x8;
    int b_bsub16x16;
    int i_bframe;
    int b_vd_hack;
    int i_bframe_bias;
    int b_bframe_adaptive;
    int b_b_refs;
    int b_b_wpred;
    int b_bidir_me;
    int i_direct_mv_pred;

    /* More... */
    int i_subpel_refine;
    int b_brdo;
    int i_me_method;
    int i_me_range;
    int b_chroma_me;
    int i_refmax;
    int b_mixedref;
    int i_log_level;
    int i_fcc_num;
    fourcc_str fcc;     /* fourcc used */
    int i_sar_width;
    int i_sar_height;
    int i_threads;
    int b_cabac;
    int i_trellis;
    int i_noise_reduction;
    int b_filter;
    int i_inloop_a;
    int i_inloop_b;
    int b_interlaced;
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

#ifdef BUGGY_APPS_HACK
    BITMAPINFOHEADER *prev_lpbiOutput;
    DWORD prev_output_biSizeImage;
#endif
#ifdef VIRTUALDUB_HACK
    int i_frame_remain;
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

/* Log functions */
void x264_log_vfw_create(CODEC *codec);
void x264_log_vfw_destroy(CODEC *codec);

/* Config functions */
void config_reg_load(CONFIG *config);
void config_reg_save(CONFIG *config);

/* Dialog callbacks */
BOOL CALLBACK callback_main (HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
BOOL CALLBACK callback_tabs(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
BOOL CALLBACK callback_about(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
BOOL CALLBACK callback_err_console(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);

/* DLL instance */
extern HINSTANCE g_hInst;

#ifdef _DEBUG
#include <stdio.h> /* vsprintf */
#define DPRINTF_BUF_SZ 1024
static __inline void DPRINTF(char *fmt, ...)
{
    va_list args;
    char buf[DPRINTF_BUF_SZ];

    va_start(args, fmt);
    vsprintf(buf, fmt, args);
    va_end(args);
    OutputDebugString(buf);
}
#else
static __inline void DPRINTF(char *fmt, ...) { }
#endif

#endif
