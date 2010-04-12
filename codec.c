/*****************************************************************************
 * codec.c: vfw x264 encoder
 *****************************************************************************
 * Copyright (C) 2003 Laurent Aimar
 * $Id: codec.c,v 1.1 2004/06/03 19:27:09 fenrir Exp $
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

#include "x264vfw.h"

#include <assert.h>
#include <wchar.h>

#define _GNU_SOURCE
#include <getopt.h>

/* Return a valid x264 colorspace or X264VFW_CSP_NONE if it is not supported */
static int get_csp(BITMAPINFOHEADER *hdr)
{
    /* For YUV the bitmap is always top-down regardless of the biHeight sign */
    int i_vflip = 0;

    switch (hdr->biCompression)
    {
        case FOURCC_I420:
        case FOURCC_IYUV:
            return X264VFW_CSP_I420 | i_vflip;

        case FOURCC_YV12:
            return X264VFW_CSP_YV12 | i_vflip;

        case FOURCC_YUYV:
        case FOURCC_YUY2:
            return X264VFW_CSP_YUYV | i_vflip;

        case FOURCC_UYVY:
        case FOURCC_HDYC:
            return X264VFW_CSP_UYVY | i_vflip;

        case BI_RGB:
        {
            i_vflip = hdr->biHeight < 0 ? 0 : X264VFW_CSP_VFLIP;
            if (hdr->biBitCount == 24)
                return X264VFW_CSP_BGR | i_vflip;
            if (hdr->biBitCount == 32)
                return X264VFW_CSP_BGRA | i_vflip;
            return X264VFW_CSP_NONE;
        }

        default:
            return X264VFW_CSP_NONE;
    }
}

#if defined(HAVE_FFMPEG) && X264VFW_USE_DECODER
static enum PixelFormat csp_to_pix_fmt(int i_csp)
{
    switch (i_csp)
    {
        case X264VFW_CSP_I420:
        case X264VFW_CSP_YV12:
            return PIX_FMT_YUV420P;

        case X264VFW_CSP_YUYV:
            return PIX_FMT_YUYV422;

        case X264VFW_CSP_UYVY:
            return PIX_FMT_UYVY422;

        case X264VFW_CSP_BGR:
            return PIX_FMT_BGR24;

        case X264VFW_CSP_BGRA:
            return PIX_FMT_BGRA;

        default:
            return PIX_FMT_NONE;
    }
}

static int x264vfw_picture_fill(AVPicture *picture, uint8_t *ptr, enum PixelFormat pix_fmt, int width, int height)
{
    memset(picture, 0, sizeof(AVPicture));

    switch (pix_fmt)
    {
        case PIX_FMT_YUV420P:
        {
            int size, size2;
            picture->linesize[0] = width;
            picture->linesize[1] =
            picture->linesize[2] = picture->linesize[0] / 2;
            size  = picture->linesize[0] * height;
            size2 = picture->linesize[1] * height / 2;
            picture->data[0] = ptr;
            picture->data[1] = picture->data[0] + size;
            picture->data[2] = picture->data[1] + size2;
            return size + 2 * size2;
        }
        case PIX_FMT_YUYV422:
            picture->linesize[0] = width * 2;
            picture->data[0] = ptr;
            return picture->linesize[0] * height;
        case PIX_FMT_BGR24:
            picture->linesize[0] = (width * 3 + 3) & ~3;
            picture->data[0] = ptr;
            return picture->linesize[0] * height;
        case PIX_FMT_BGRA:
            picture->linesize[0] = width * 4;
            picture->data[0] = ptr;
            return picture->linesize[0] * height;
        default:
            return -1;
    }
    return 0;
}

static int x264vfw_picture_get_size(enum PixelFormat pix_fmt, int width, int height)
{
    AVPicture dummy_pict;
    return x264vfw_picture_fill(&dummy_pict, NULL, pix_fmt, width, height);
}
#endif

static int supported_fourcc(DWORD fourcc)
{
    int i;
    for (i = 0; i < sizeof(fcc_str_table) / sizeof(fourcc_str); i++)
        if (fourcc == mmioFOURCC(fcc_str_table[i][0], fcc_str_table[i][1], fcc_str_table[i][2], fcc_str_table[i][3]))
            return 1;
    return 0;
}

/* Return the output format of the compressed data */
LRESULT compress_get_format(CODEC *codec, BITMAPINFO *lpbiInput, BITMAPINFO *lpbiOutput)
{
    BITMAPINFOHEADER *inhdr = &lpbiInput->bmiHeader;
    BITMAPINFOHEADER *outhdr = &lpbiOutput->bmiHeader;
    CONFIG           *config = &codec->config;
    int              iWidth;
    int              iHeight;

    if (!lpbiOutput)
        return sizeof(BITMAPINFOHEADER);

    if (get_csp(inhdr) == X264VFW_CSP_NONE)
        return ICERR_BADFORMAT;

    iWidth  = inhdr->biWidth;
    iHeight = abs(inhdr->biHeight);
    if (iWidth <= 0 || iHeight <= 0)
        return ICERR_BADFORMAT;
    /* We need x2 width/height */
    if (iWidth % 2 || iHeight % 2)
        return ICERR_BADFORMAT;

    memset(outhdr, 0, sizeof(BITMAPINFOHEADER));
    outhdr->biSize        = sizeof(BITMAPINFOHEADER);
    outhdr->biWidth       = iWidth;
    outhdr->biHeight      = iHeight;
    outhdr->biPlanes      = 1;
    outhdr->biBitCount    = 24;
    outhdr->biCompression = mmioFOURCC(config->fcc[0], config->fcc[1], config->fcc[2], config->fcc[3]);
    outhdr->biSizeImage   = compress_get_size(codec, lpbiInput, lpbiOutput);

    return ICERR_OK;
}

/* Return the maximum number of bytes a single compressed frame can occupy */
LRESULT compress_get_size(CODEC *codec, BITMAPINFO *lpbiInput, BITMAPINFO *lpbiOutput)
{
    return ((lpbiOutput->bmiHeader.biWidth + 15) & ~15) * ((lpbiOutput->bmiHeader.biHeight + 31) & ~31) * 3 + 4096;
}

/* Test if the driver can compress a specific input format to a specific output format */
LRESULT compress_query(CODEC *codec, BITMAPINFO *lpbiInput, BITMAPINFO *lpbiOutput)
{
    BITMAPINFOHEADER *inhdr = &lpbiInput->bmiHeader;
    BITMAPINFOHEADER *outhdr = &lpbiOutput->bmiHeader;
    int              iWidth;
    int              iHeight;

    if (get_csp(inhdr) == X264VFW_CSP_NONE)
        return ICERR_BADFORMAT;

    iWidth  = inhdr->biWidth;
    iHeight = abs(inhdr->biHeight);
    if (iWidth <= 0 || iHeight <= 0)
        return ICERR_BADFORMAT;
    /* We need x2 width/height */
    if (iWidth % 2 || iHeight % 2)
        return ICERR_BADFORMAT;

    if (!lpbiOutput)
        return ICERR_OK;

    if (iWidth != outhdr->biWidth || iHeight != outhdr->biHeight)
        return ICERR_BADFORMAT;

    if (!supported_fourcc(outhdr->biCompression))
        return ICERR_BADFORMAT;

    /* MSDN says that biSizeImage may be set to zero for BI_RGB bitmaps
       But some buggy applications don't set it also for other bitmap types */
    if (outhdr->biSizeImage != 0 && outhdr->biSizeImage < compress_get_size(codec, lpbiInput, lpbiOutput))
        return ICERR_BADFORMAT;

    return ICERR_OK;
}

void x264_log_vfw_create(CODEC *codec)
{
    x264_log_vfw_destroy(codec);
    if (codec->config.i_log_level > 0)
        codec->hCons = CreateDialog(g_hInst, MAKEINTRESOURCE(IDD_ERRCONSOLE), GetDesktopWindow(), callback_err_console);
}

void x264_log_vfw_destroy(CODEC *codec)
{
    if (codec->hCons)
    {
        DestroyWindow(codec->hCons);
        codec->hCons = NULL;
    }
    codec->b_visible = FALSE;
}

static void x264_log_vfw(void *p_private, int i_level, const char *psz_fmt, va_list arg)
{
    char  error_msg[1024];
    int   idx;
    CODEC *codec = p_private;
    char  *psz_prefix;
    HWND  h_console;
    HDC   hdc;

    DVPRINTF(psz_fmt, arg);

    switch (i_level)
    {
        case X264_LOG_ERROR:
            psz_prefix = "error";
            break;

        case X264_LOG_WARNING:
            psz_prefix = "warning";
            break;

        case X264_LOG_INFO:
            psz_prefix = "info";
            break;

        case X264_LOG_DEBUG:
            psz_prefix = "debug";
            break;

        default:
            psz_prefix = "unknown";
            break;
    }
    sprintf(error_msg, "x264vfw [%s]: ", psz_prefix);
    vsprintf(error_msg+strlen(error_msg), psz_fmt, arg);

    /* Strip final linefeeds (required) */
    idx = strlen(error_msg) - 1;
    while (idx >= 0 && error_msg[idx] == '\n')
        error_msg[idx--] = 0;

    if (codec->hCons)
    {
        if (!codec->b_visible)
        {
            ShowWindow(codec->hCons, SW_SHOWNORMAL);
            codec->b_visible = TRUE;
        }
        h_console = GetDlgItem(codec->hCons, IDC_ERRCONSOLE_CONSOLE);
        idx = SendMessage(h_console, LB_ADDSTRING, 0, (LPARAM)&error_msg);

        /* Make sure that the last added item is visible (autoscroll) */
        if (idx >= 0)
            SendMessage(h_console, LB_SETTOPINDEX, (WPARAM)idx, 0);

        hdc = GetDC(h_console);
        if (hdc)
        {
            HFONT hfntDefault;
            SIZE  sz;

            strcat(error_msg, "X"); /* Otherwise item may be clipped */
            hfntDefault = SelectObject(hdc, (HFONT)SendMessage(h_console, WM_GETFONT, 0, 0));
            GetTextExtentPoint32(hdc, error_msg, strlen(error_msg), &sz);
            if (sz.cx > SendMessage(h_console, LB_GETHORIZONTALEXTENT, 0, 0))
                SendMessage(h_console, LB_SETHORIZONTALEXTENT, (WPARAM)sz.cx, 0);
            SelectObject(hdc, hfntDefault);
            ReleaseDC(h_console, hdc);
        }
    }
}

void x264vfw_log(CODEC *codec, int i_level, const char *psz_fmt, ...)
{
    va_list arg;
    va_start(arg, psz_fmt);
    if (i_level <= codec->config.i_log_level - 1)
    {
        if (!codec->hCons)
            x264_log_vfw_create(codec);
        x264_log_vfw(codec, i_level, psz_fmt, arg);
    }
    else
        DVPRINTF(psz_fmt, arg);
    va_end(arg);
}

#define OPT_FRAMES 256
#define OPT_SEEK 257
#define OPT_QPFILE 258
#define OPT_THREAD_INPUT 259
#define OPT_QUIET 260
#define OPT_NOPROGRESS 261
#define OPT_VISUALIZE 262
#define OPT_LONGHELP 263
#define OPT_PROFILE 264
#define OPT_PRESET 265
#define OPT_TUNE 266
#define OPT_SLOWFIRSTPASS 267
#define OPT_FULLHELP 268
#define OPT_FPS 269
#define OPT_MUXER 270
#define OPT_DEMUXER 271
#define OPT_INDEX 272
#define OPT_INTERLACED 273
#define OPT_TCFILE_IN 274
#define OPT_TCFILE_OUT 275
#define OPT_TIMEBASE 276
#define OPT_PULLDOWN 277
#if X264VFW_USE_VIRTUALDUB_HACK
#define OPT_VD_HACK 512
#endif
#define OPT_NO_OUTPUT 513

static char short_options[] = "8A:B:b:f:hI:i:m:o:p:q:r:t:Vvw";
static struct option long_options[] =
{
    { "help",              no_argument,       NULL, 'h'               },
    { "longhelp",          no_argument,       NULL, OPT_LONGHELP      },
    { "fullhelp",          no_argument,       NULL, OPT_FULLHELP      },
    { "version",           no_argument,       NULL, 'V'               },
    { "profile",           required_argument, NULL, OPT_PROFILE       },
    { "preset",            required_argument, NULL, OPT_PRESET        },
    { "tune",              required_argument, NULL, OPT_TUNE          },
    { "slow-firstpass",    no_argument,       NULL, OPT_SLOWFIRSTPASS },
    { "bitrate",           required_argument, NULL, 'B'               },
    { "bframes",           required_argument, NULL, 'b'               },
    { "b-adapt",           required_argument, NULL, 0                 },
    { "no-b-adapt",        no_argument,       NULL, 0                 },
    { "b-bias",            required_argument, NULL, 0                 },
    { "b-pyramid",         required_argument, NULL, 0                 },
    { "min-keyint",        required_argument, NULL, 'i'               },
    { "keyint",            required_argument, NULL, 'I'               },
    { "intra-refresh",     no_argument,       NULL, 0                 },
    { "scenecut",          required_argument, NULL, 0                 },
    { "no-scenecut",       no_argument,       NULL, 0                 },
    { "nf",                no_argument,       NULL, 0                 },
    { "no-deblock",        no_argument,       NULL, 0                 },
    { "filter",            required_argument, NULL, 0                 },
    { "deblock",           required_argument, NULL, 'f'               },
    { "interlaced",        no_argument,       NULL, 0                 },
    { "no-interlaced",     no_argument,       NULL, 0                 },
    { "tff",               no_argument,       NULL, 0                 },
    { "bff",               no_argument,       NULL, 0                 },
    { "constrained-intra", no_argument,       NULL, 0                 },
    { "cabac",             no_argument,       NULL, 0                 },
    { "no-cabac",          no_argument,       NULL, 0                 },
    { "qp",                required_argument, NULL, 'q'               },
    { "qpmin",             required_argument, NULL, 0                 },
    { "qpmax",             required_argument, NULL, 0                 },
    { "qpstep",            required_argument, NULL, 0                 },
    { "crf",               required_argument, NULL, 0                 },
    { "rc-lookahead",      required_argument, NULL, 0                 },
    { "ref",               required_argument, NULL, 'r'               },
    { "asm",               required_argument, NULL, 0                 },
    { "no-asm",            no_argument,       NULL, 0                 },
    { "sar",               required_argument, NULL, 0                 },
    { "fps",               required_argument, NULL, 0                 },
    { "frames",            required_argument, NULL, OPT_FRAMES        },
    { "seek",              required_argument, NULL, OPT_SEEK          },
    { "output",            required_argument, NULL, 'o'               },
    { "muxer",             required_argument, NULL, OPT_MUXER         },
    { "demuxer",           required_argument, NULL, OPT_DEMUXER       },
    { "stdout",            required_argument, NULL, OPT_MUXER         },
    { "stdin",             required_argument, NULL, OPT_DEMUXER       },
    { "index",             required_argument, NULL, OPT_INDEX         },
    { "analyse",           required_argument, NULL, 0                 },
    { "partitions",        required_argument, NULL, 'A'               },
    { "direct",            required_argument, NULL, 0                 },
    { "weightb",           no_argument,       NULL, 'w'               },
    { "no-weightb",        no_argument,       NULL, 0                 },
    { "weightp",           required_argument, NULL, 0                 },
    { "me",                required_argument, NULL, 0                 },
    { "merange",           required_argument, NULL, 0                 },
    { "mvrange",           required_argument, NULL, 0                 },
    { "mvrange-thread",    required_argument, NULL, 0                 },
    { "subme",             required_argument, NULL, 'm'               },
    { "psy-rd",            required_argument, NULL, 0                 },
    { "psy",               no_argument,       NULL, 0                 },
    { "no-psy",            no_argument,       NULL, 0                 },
    { "mixed-refs",        no_argument,       NULL, 0                 },
    { "no-mixed-refs",     no_argument,       NULL, 0                 },
    { "chroma-me",         no_argument,       NULL, 0                 },
    { "no-chroma-me",      no_argument,       NULL, 0                 },
    { "8x8dct",            no_argument,       NULL, 0                 },
    { "no-8x8dct",         no_argument,       NULL, 0                 },
    { "trellis",           required_argument, NULL, 't'               },
    { "fast-pskip",        no_argument,       NULL, 0                 },
    { "no-fast-pskip",     no_argument,       NULL, 0                 },
    { "dct-decimate",      no_argument,       NULL, 0                 },
    { "no-dct-decimate",   no_argument,       NULL, 0                 },
    { "aq-strength",       required_argument, NULL, 0                 },
    { "aq-mode",           required_argument, NULL, 0                 },
    { "deadzone-inter",    required_argument, NULL, 0                 },
    { "deadzone-intra",    required_argument, NULL, 0                 },
    { "level",             required_argument, NULL, 0                 },
    { "ratetol",           required_argument, NULL, 0                 },
    { "vbv-maxrate",       required_argument, NULL, 0                 },
    { "vbv-bufsize",       required_argument, NULL, 0                 },
    { "vbv-init",          required_argument, NULL, 0                 },
    { "crf-max",           required_argument, NULL, 0                 },
    { "ipratio",           required_argument, NULL, 0                 },
    { "pbratio",           required_argument, NULL, 0                 },
    { "chroma-qp-offset",  required_argument, NULL, 0                 },
    { "pass",              required_argument, NULL, 'p'               },
    { "stats",             required_argument, NULL, 0                 },
    { "qcomp",             required_argument, NULL, 0                 },
    { "mbtree",            no_argument,       NULL, 0                 },
    { "no-mbtree",         no_argument,       NULL, 0                 },
    { "qblur",             required_argument, NULL, 0                 },
    { "cplxblur",          required_argument, NULL, 0                 },
    { "zones",             required_argument, NULL, 0                 },
    { "qpfile",            required_argument, NULL, OPT_QPFILE        },
    { "threads",           required_argument, NULL, 0                 },
    { "sliced-threads",    no_argument,       NULL, 0                 },
    { "no-sliced-threads", no_argument,       NULL, 0                 },
    { "slice-max-size",    required_argument, NULL, 0                 },
    { "slice-max-mbs",     required_argument, NULL, 0                 },
    { "slices",            required_argument, NULL, 0                 },
    { "thread-input",      no_argument,       NULL, OPT_THREAD_INPUT  },
    { "sync-lookahead",    required_argument, NULL, 0                 },
    { "deterministic",     no_argument,       NULL, 0                 },
    { "non-deterministic", no_argument,       NULL, 0                 },
    { "psnr",              no_argument,       NULL, 0                 },
    { "no-psnr",           no_argument,       NULL, 0                 },
    { "ssim",              no_argument,       NULL, 0                 },
    { "no-ssim",           no_argument,       NULL, 0                 },
    { "quiet",             no_argument,       NULL, OPT_QUIET         },
    { "verbose",           no_argument,       NULL, 'v'               },
    { "progress",          no_argument,       NULL, OPT_NOPROGRESS    },
    { "no-progress",       no_argument,       NULL, OPT_NOPROGRESS    },
    { "visualize",         no_argument,       NULL, OPT_VISUALIZE     },
    { "dump-yuv",          required_argument, NULL, 0                 },
    { "sps-id",            required_argument, NULL, 0                 },
    { "aud",               no_argument,       NULL, 0                 },
    { "no-aud",            no_argument,       NULL, 0                 },
    { "nr",                required_argument, NULL, 0                 },
    { "cqm",               required_argument, NULL, 0                 },
    { "cqmfile",           required_argument, NULL, 0                 },
    { "cqm4",              required_argument, NULL, 0                 },
    { "cqm4i",             required_argument, NULL, 0                 },
    { "cqm4iy",            required_argument, NULL, 0                 },
    { "cqm4ic",            required_argument, NULL, 0                 },
    { "cqm4p",             required_argument, NULL, 0                 },
    { "cqm4py",            required_argument, NULL, 0                 },
    { "cqm4pc",            required_argument, NULL, 0                 },
    { "cqm8",              required_argument, NULL, 0                 },
    { "cqm8i",             required_argument, NULL, 0                 },
    { "cqm8p",             required_argument, NULL, 0                 },
    { "overscan",          required_argument, NULL, 0                 },
    { "videoformat",       required_argument, NULL, 0                 },
    { "fullrange",         required_argument, NULL, 0                 },
    { "colorprim",         required_argument, NULL, 0                 },
    { "transfer",          required_argument, NULL, 0                 },
    { "colormatrix",       required_argument, NULL, 0                 },
    { "chromaloc",         required_argument, NULL, 0                 },
    { "force-cfr",         no_argument,       NULL, 0                 },
    { "tcfile-in",         required_argument, NULL, OPT_TCFILE_IN     },
    { "tcfile-out",        required_argument, NULL, OPT_TCFILE_OUT    },
    { "timebase",          required_argument, NULL, OPT_TIMEBASE      },
    { "pic-struct",        no_argument,       NULL, 0                 },
    { "nal-hrd",           required_argument, NULL, 0                 },
    { "pulldown",          required_argument, NULL, OPT_PULLDOWN      },
#if X264VFW_USE_VIRTUALDUB_HACK
    { "vd-hack",           no_argument,       NULL, OPT_VD_HACK       },
#endif
    { "no-output",         no_argument,       NULL, OPT_NO_OUTPUT     },
    { 0,                   0,                 0,    0                 }
};

#define MAX_ARG_NUM (MAX_CMDLINE / 2 + 1)

/* Split command line (for quote in parameter it must be tripled) */
static int split_cmd(const char *cmdline, char **argv, char *arg_mem)
{
    int  argc = 1;
    char *p, *q;
    int  s = 0;

    memset(argv, 0, sizeof(char *) * MAX_ARG_NUM);
    argv[0] = "x264vfw";
    p = arg_mem;
    q = (char *)cmdline;
    while (*q != 0)
    {
        switch (s)
        {
            case 0:
                switch (*q)
                {
                    case '\t':
                    case '\r':
                    case '\n':
                    case ' ':
                        break;

                    case '"':
                        s = 2;
                        argv[argc] = p;
                        argc++;
                        break;

                    default:
                        s = 1;
                        argv[argc] = p;
                        argc++;
                        *p = *q;
                        p++;
                        break;
                }
                break;

            case 1:
                switch (*q)
                {
                    case '\t':
                    case '\r':
                    case '\n':
                    case ' ':
                        s = 0;
                        *p = 0;
                        p++;
                        break;

                    case '"':
                        s = 2;
                        break;

                    default:
                        *p = *q;
                        p++;
                        break;
                }
                break;

            case 2:
                switch (*q)
                {
                    case '"':
                        s = 3;
                        break;

                    default:
                        *p = *q;
                        p++;
                        break;
                }
                break;

            case 3:
                switch (*q)
                {
                    case '\t':
                    case '\r':
                    case '\n':
                    case ' ':
                        s = 0;
                        *p = 0;
                        p++;
                        break;

                    default:
                        s = 1;
                        *p = *q;
                        p++;
                        break;
                }
                break;

            default:
                assert(0);
                break;
        }
        q++;
    }
    *p = 0;
    return argc;
}

static const char * const muxer_names[] =
{
    "auto",
    "raw",
    "mkv",
    "flv",
    "mp4",
    "avi",
    0
};

static int select_output(const char *muxer, char *filename, x264_param_t *param, CODEC *codec)
{
    const char *ext = get_filename_extension(filename);

    if (!strcmp(filename, "-"))
        return 0;

    if (strcasecmp(muxer, "auto"))
        ext = muxer;

    if (!strcasecmp(ext, "mp4"))
    {
#ifdef MP4_OUTPUT
        codec->cli_output = mp4_output;
        param->b_annexb = 0;
        param->b_dts_compress = 0;
        param->b_repeat_headers = 0;
        if (param->i_nal_hrd == X264_NAL_HRD_CBR)
        {
            x264vfw_log(codec, X264_LOG_WARNING, "cbr nal-hrd is not compatible with mp4\n");
            param->i_nal_hrd = X264_NAL_HRD_VBR;
        }
#else
        x264vfw_log(codec, X264_LOG_ERROR, "not compiled with MP4 output support\n");
        return -1;
#endif
    }
    else if (!strcasecmp(ext, "mkv"))
    {
        codec->cli_output = mkv_output;
        param->b_annexb = 0;
        param->b_dts_compress = 0;
        param->b_repeat_headers = 0;
    }
    else if (!strcasecmp(ext, "flv"))
    {
        codec->cli_output = flv_output;
        param->b_annexb = 0;
        param->b_dts_compress = 1;
        param->b_repeat_headers = 0;
    }
    else if (!strcasecmp(ext, "avi"))
    {
#if defined(HAVE_FFMPEG)
        codec->cli_output = avi_output;
        param->b_annexb = 1;
        param->b_dts_compress = 0;
        param->b_repeat_headers = 1;
        if (param->b_vfr_input)
        {
            x264vfw_log(codec, X264_LOG_WARNING, "VFR is not compatible with AVI output\n");
            param->b_vfr_input = 0;
        }
#else
        x264vfw_log(codec, X264_LOG_ERROR, "not compiled with AVI output support\n");
        return -1;
#endif
    }
    else
        codec->cli_output = raw_output;
    codec->b_cli_output = TRUE;
    return 0;
}

/*****************************************************************************
 * Parse:
 *****************************************************************************/
static int Parse(int argc, char **argv, x264_param_t *param, CODEC *codec)
{
    char *output_filename = "-";
    const char *muxer = muxer_names[0];

    opterr = 0; /* Suppress error messages printing in getopt */
    optind = 0; /* Initialize getopt */

    /* Parse command line options */
    for (;;)
    {
        int b_error = 0;
        int long_options_index = -1;
        int checked_optind = optind > 0 ? optind : 1;
        int c = getopt_long(argc, argv, short_options, long_options, &long_options_index);

        if (c == -1)
            break;

        switch (c)
        {
            case 'h':
            case OPT_LONGHELP:
            case OPT_FULLHELP:
            case 'V':
            case OPT_FRAMES:
            case OPT_SEEK:
            case OPT_DEMUXER:
            case OPT_INDEX:
            case OPT_QPFILE:
            case OPT_THREAD_INPUT:
            case OPT_NOPROGRESS:
            case OPT_VISUALIZE:
            case OPT_TUNE:
            case OPT_PRESET:
            case OPT_PROFILE:
            case OPT_SLOWFIRSTPASS:
            case OPT_TCFILE_IN:
            case OPT_TCFILE_OUT:
            case OPT_TIMEBASE:
            case OPT_PULLDOWN:
                x264vfw_log(codec, X264_LOG_WARNING, "not supported option: '%s'\n", argv[checked_optind]);
                break;

            case 'o':
                output_filename = optarg;
                break;

            case OPT_MUXER:
            {
                int i;

                for (i = 0; muxer_names[i] && strcasecmp(muxer_names[i], optarg);)
                    i++;
                if (!muxer_names[i])
                {
                    x264vfw_log(codec, X264_LOG_ERROR, "invalid muxer '%s'\n", optarg);
                    return -1;
                }
                muxer = optarg;
                break;
            }

            case OPT_QUIET:
                param->i_log_level = X264_LOG_NONE;
                break;

            case 'v':
                param->i_log_level = X264_LOG_DEBUG;
                break;

#if X264VFW_USE_VIRTUALDUB_HACK
            case OPT_VD_HACK:
                codec->b_use_vd_hack = TRUE;
                break;
#endif

            case OPT_NO_OUTPUT:
                codec->b_no_output = TRUE;
                break;

            default:
                if (long_options_index < 0)
                {
                    int i;

                    for (i = 0; long_options[i].name; i++)
                        if (long_options[i].val == c)
                        {
                            long_options_index = i;
                            break;
                        }
                    if (long_options_index < 0)
                    {
                        x264vfw_log(codec, X264_LOG_ERROR, "unknown option or absent argument: '%s'\n", argv[checked_optind]);
                        return -1;
                    }
                }
                b_error = x264_param_parse(param, long_options[long_options_index].name, optarg);
                break;
        }

        if (b_error)
        {
            switch (b_error)
            {
                case X264_PARAM_BAD_NAME:
                    x264vfw_log(codec, X264_LOG_ERROR, "unknown option: '%s'\n", argv[checked_optind]);
                    break;

                case X264_PARAM_BAD_VALUE:
                    x264vfw_log(codec, X264_LOG_ERROR, "invalid argument: '%s' = '%s'\n", argv[checked_optind], optarg);
                    break;

                default:
                    x264vfw_log(codec, X264_LOG_ERROR, "unknown error with option: '%s'\n", argv[checked_optind]);
                    break;
            }
            return -1;
        }
    }

    param->b_vfr_input = 0; /* VFW supports only CFR */
    param->i_timebase_num = param->i_fps_den;
    param->i_timebase_den = param->i_fps_num;

    if (select_output(muxer, output_filename, param, codec) < 0)
        return -1;
    if (!codec->b_cli_output)
    {
        param->b_annexb = 1;
        param->b_dts_compress = 0;
        param->b_repeat_headers = 1; /* VFW needs SPS/PPS before each keyframe */
    }
    if (codec->b_cli_output && codec->cli_output.open_file(output_filename, &codec->cli_hout) < 0)
    {
        x264vfw_log(codec, X264_LOG_ERROR, "could not open output file: '%s'\n", output_filename);
        return -1;
    }

    return 0;
}

/* Prepare to compress data */
LRESULT compress_begin(CODEC *codec, BITMAPINFO *lpbiInput, BITMAPINFO *lpbiOutput)
{
    CONFIG *config = &codec->config;
    x264_param_t param;
    char cqm_file[MAX_PATH];
    char stat_out[MAX_STATS_SIZE];
    char stat_in[MAX_STATS_SIZE];

    /* Destroy previous handle */
    compress_end(codec);

    if (compress_query(codec, lpbiInput, lpbiOutput) != ICERR_OK)
    {
        x264vfw_log(codec, X264_LOG_ERROR, "incompatible input/output frame format (encode)\n");
        codec->b_encoder_error = TRUE;
        return ICERR_BADFORMAT;
    }

#if X264VFW_USE_BUGGY_APPS_HACK
    codec->prev_lpbiOutput = NULL;
    codec->prev_output_biSizeImage = 0;
    codec->b_check_size = lpbiOutput->bmiHeader.biSizeImage != 0;
#endif

    /* Get default param */
    x264_param_default(&param);
#if X264VFW_USE_VIRTUALDUB_HACK
    codec->b_use_vd_hack = FALSE; /* Don't use "VD hack" by default */
    codec->save_fcc = lpbiOutput->bmiHeader.biCompression;
#endif
    codec->b_no_output   = FALSE;
    codec->i_frame_remain = codec->i_frame_total ? codec->i_frame_total : -1;

    /* Video Properties */
    param.i_width  = lpbiInput->bmiHeader.biWidth;
    param.i_height = abs(lpbiInput->bmiHeader.biHeight);
    param.i_csp    = X264_CSP_I420;

    x264vfw_csp_init(&codec->csp, param.i_csp);
    x264_picture_alloc(&codec->conv_pic, param.i_csp, param.i_width, param.i_height);

    param.i_frame_total = codec->i_frame_total;
    if (codec->i_fps_num > 0 && codec->i_fps_den > 0)
    {
        param.i_fps_num = codec->i_fps_num;
        param.i_fps_den = codec->i_fps_den;
    }

    /* Log */
    x264_log_vfw_create(codec);
    param.pf_log = x264_log_vfw;
    param.p_log_private = codec;

    if (config->b_use_cmdline)
    {
        char *argv[MAX_ARG_NUM];
        char arg_mem[MAX_CMDLINE];
        int argc = split_cmd(config->cmdline, argv, (char *)&arg_mem);

        /* Parse command line */
        if (Parse(argc, argv, &param, codec) < 0)
        {
            codec->b_encoder_error = TRUE;
            compress_end(codec);
            return ICERR_ERROR;
        }
    }
    else
    {
#if X264VFW_USE_VIRTUALDUB_HACK
        codec->b_use_vd_hack = config->b_vd_hack;
#endif

        /* CPU flags */
        param.cpu = config->b_no_asm ? 0 : param.cpu;
#if X264VFW_USE_THREADS
        param.i_threads = config->i_threads;
        param.b_deterministic = config->i_threads != 1 ? config->b_mt_deterministic : TRUE;
        param.b_sliced_threads = config->i_threads != 1 ? config->b_mt_sliced : FALSE;
#else
        param.i_threads = 1;
        param.b_deterministic = TRUE;
        param.b_sliced_threads = FALSE;
#endif

        /* Video Properties */
        switch (config->i_avc_level)
        {
            case  1: param.i_level_idc = 10; break;
            case  2: param.i_level_idc = 11; break;
            case  3: param.i_level_idc = 12; break;
            case  4: param.i_level_idc = 13; break;
            case  5: param.i_level_idc = 20; break;
            case  6: param.i_level_idc = 21; break;
            case  7: param.i_level_idc = 22; break;
            case  8: param.i_level_idc = 30; break;
            case  9: param.i_level_idc = 31; break;
            case 10: param.i_level_idc = 32; break;
            case 11: param.i_level_idc = 40; break;
            case 12: param.i_level_idc = 41; break;
            case 13: param.i_level_idc = 42; break;
            case 14: param.i_level_idc = 50; break;
            case 15: param.i_level_idc = 51; break;
            default: param.i_level_idc = -1; break;
        }
        param.vui.i_sar_width = config->i_sar_width;
        param.vui.i_sar_height = config->i_sar_height;
        param.vui.i_overscan = config->i_overscan;
        param.vui.i_vidformat = config->i_vidformat;
        param.vui.b_fullrange = config->i_fullrange > 0;
        param.vui.i_colorprim = config->i_colorprim;
        param.vui.i_transfer = config->i_transfer;
        param.vui.i_colmatrix = config->i_colmatrix;
        param.vui.i_chroma_loc = config->i_chromaloc;

        /* Bitstream parameters */
        param.i_frame_reference = config->i_refmax;
        param.i_keyint_max = config->i_keyint_max;
        param.i_keyint_min = config->i_keyint_min;
        param.i_scenecut_threshold = config->i_scenecut_threshold;
        param.i_bframe = config->i_encoding_type > 0 ? config->i_bframe : 0;
        param.i_bframe_adaptive = config->i_encoding_type > 0 && config->i_bframe > 0 ? config->i_bframe_adaptive : 0;
        param.i_bframe_bias = config->i_encoding_type > 0 && config->i_bframe > 0 && config->i_bframe_adaptive > 0 ? config->i_bframe_bias : 0;
        param.i_bframe_pyramid = config->i_encoding_type > 0 && config->i_bframe > 1 ? config->i_b_refs : 0;

        param.b_deblocking_filter = config->i_encoding_type > 0 && config->b_filter;
        param.i_deblocking_filter_alphac0 = config->i_encoding_type > 0 && config->b_filter ? config->i_inloop_a : 0;
        param.i_deblocking_filter_beta = config->i_encoding_type > 0 && config->b_filter ? config->i_inloop_b : 0;

        param.b_cabac = config->b_cabac;

        param.b_interlaced = config->i_interlaced > 0;
        param.b_tff = config->i_interlaced != 2;

        strcpy(cqm_file, config->cqmfile);
        param.i_cqm_preset = config->i_encoding_type > 0 ? config->i_cqm : 0;
        param.psz_cqm_file = config->i_encoding_type > 0 && config->i_cqm == 2 ? (char *)&cqm_file : NULL;

        /* Log */
        param.i_log_level = config->i_log_level - 1;

        /* Encoder analyser parameters */
        param.analyse.intra = 0;
        param.analyse.inter = 0;
        if ((config->i_encoding_type > 0 || config->b_cabac) && config->b_dct8x8)
        {
            if (config->b_intra_i8x8)
                param.analyse.intra |= X264_ANALYSE_I8x8;
            if (config->b_i8x8)
                param.analyse.inter |= X264_ANALYSE_I8x8;
        }
        if (config->b_intra_i4x4)
            param.analyse.intra |= X264_ANALYSE_I4x4;
        if (config->b_i4x4)
            param.analyse.inter |= X264_ANALYSE_I4x4;
        if (config->b_psub16x16)
        {
            param.analyse.inter |= X264_ANALYSE_PSUB16x16;
            if (config->b_psub8x8)
                param.analyse.inter |= X264_ANALYSE_PSUB8x8;
        }
        if (config->i_encoding_type > 0 && config->i_bframe > 0 && config->b_bsub16x16)
            param.analyse.inter |= X264_ANALYSE_BSUB16x16;

        param.analyse.b_transform_8x8 = config->i_encoding_type > 0 && config->b_dct8x8;
        param.analyse.i_weighted_pred = config->i_p_wpred;
        param.analyse.b_weighted_bipred = config->i_encoding_type > 0 && config->i_bframe > 0 && config->b_b_wpred;
        param.analyse.i_direct_mv_pred = config->i_encoding_type > 0 && config->i_bframe > 0 ? config->i_direct_mv_pred : 0;
        param.analyse.i_chroma_qp_offset = config->i_encoding_type > 0 ? config->i_chroma_qp_offset : 0;

        param.analyse.i_me_method = config->i_me_method;
        param.analyse.i_me_range = config->i_me_range;
        param.analyse.i_subpel_refine = config->i_subpel_refine;
        param.analyse.b_chroma_me = config->i_subpel_refine >= 5 && config->b_chroma_me;
        param.analyse.b_mixed_references = config->i_refmax > 1 && config->b_mixedref;
        param.analyse.i_trellis = config->i_encoding_type > 0 && config->b_cabac ? config->i_trellis : 0;
        param.analyse.b_fast_pskip = config->i_encoding_type > 0 && config->b_fast_pskip;
        param.analyse.b_dct_decimate = config->i_encoding_type > 0 && config->b_dct_decimate;
        param.analyse.i_noise_reduction = config->i_encoding_type > 0 ? config->i_noise_reduction : 0;
        param.analyse.f_psy_rd = config->i_encoding_type > 0 && config->i_subpel_refine >= 6 ? config->f_psy_rdo : 0.0;
        param.analyse.f_psy_trellis = config->i_encoding_type > 0 && config->b_cabac && config->i_trellis > 0 ? config->f_psy_trellis : 0.0;

        param.analyse.i_luma_deadzone[0] = config->i_encoding_type > 0 ? config->i_inter_deadzone : 0;
        param.analyse.i_luma_deadzone[1] = config->i_encoding_type > 0 ? config->i_intra_deadzone : 0;

        param.analyse.b_psnr = config->i_encoding_type > 0 && config->i_log_level >= 3 && config->b_psnr;
        param.analyse.b_ssim = config->i_encoding_type > 0 && config->i_log_level >= 3 && config->b_ssim;

        /* Rate control parameters */
        param.rc.i_qp_min = config->i_encoding_type > 1 ? config->i_qp_min : param.rc.i_qp_min;
        param.rc.i_qp_max = config->i_encoding_type > 1 ? config->i_qp_max : param.rc.i_qp_max;
        param.rc.i_qp_step = config->i_encoding_type > 1 ? config->i_qp_step : param.rc.i_qp_step;

        param.rc.f_rate_tolerance = config->i_encoding_type > 2 ? config->f_ratetol : param.rc.f_rate_tolerance;
        param.rc.i_vbv_max_bitrate = config->i_encoding_type > 1 ? config->i_vbv_maxrate : 0;
        param.rc.i_vbv_buffer_size = config->i_encoding_type > 1 ? config->i_vbv_bufsize : 0;
        param.rc.f_vbv_buffer_init = config->i_encoding_type > 1 ? (float)config->i_vbv_occupancy / 100.0 : param.rc.f_vbv_buffer_init;
        param.rc.f_ip_factor = config->i_encoding_type > 0 ? config->f_ipratio : 1.0;
        param.rc.f_pb_factor = config->i_encoding_type > 0 && config->i_bframe > 0 ? config->f_pbratio : 1.0;

        param.rc.i_aq_mode = config->i_encoding_type > 1 ? config->i_aq_mode : 0;
        param.rc.f_aq_strength = config->i_encoding_type > 1 && config->i_aq_mode > 0 ? config->f_aq_strength : 0.0;

        strcpy(stat_out, config->stats);
        strcpy(stat_in, config->stats);
        param.rc.b_stat_write = FALSE;
        param.rc.psz_stat_out = (char *)&stat_out;
        param.rc.b_stat_read = FALSE;
        param.rc.psz_stat_in = (char *)&stat_in;

        param.rc.f_qcompress = config->i_encoding_type > 1 ? config->f_qcomp : 1.0;
        param.rc.f_qblur = config->i_encoding_type == 4 && config->i_pass > 1 ? config->f_qblur : param.rc.f_qblur;
        param.rc.f_complexity_blur = config->i_encoding_type == 4 && config->i_pass > 1 ? config->f_cplxblur : param.rc.f_complexity_blur;

        switch (config->i_encoding_type)
        {
            case 0: /* 1 PASS LOSSLESS */
                param.rc.i_rc_method = X264_RC_CQP;
                param.rc.i_qp_constant = 0;
                param.rc.b_stat_write = config->b_createstats;
                break;

            case 1: /* 1 PASS CQP */
                param.rc.i_rc_method = X264_RC_CQP;
                param.rc.i_qp_constant = config->i_qp;
                param.rc.b_stat_write = config->b_createstats;
                break;

            case 2: /* 1 PASS VBR */
                param.rc.i_rc_method = X264_RC_CRF;
                param.rc.f_rf_constant = (float)config->i_rf_constant * 0.1;
                param.rc.b_stat_write = config->b_createstats;
                break;

            case 3: /* 1 PASS ABR */
                param.rc.i_rc_method = X264_RC_ABR;
                param.rc.i_bitrate = config->i_passbitrate;
                param.rc.b_stat_write = config->b_createstats;
                break;

            case 4: /* 2 PASS */
                param.rc.i_rc_method = X264_RC_ABR;
                param.rc.i_bitrate = config->i_passbitrate;
                if (config->i_pass <= 1)
                {
                    codec->b_no_output = TRUE;
                    param.rc.b_stat_write = TRUE;
                    if (config->b_fast1pass)
                    {
                        /* Adjust or turn off some flags to gain speed, if needed */
                        param.i_frame_reference = 1;
                        param.analyse.intra = 0;
                        param.analyse.inter = 0;
                        param.analyse.b_transform_8x8 = FALSE;
                        param.analyse.i_me_method = X264_ME_DIA;
                        param.analyse.i_subpel_refine = X264_MIN(2, param.analyse.i_subpel_refine); /* subme=1 may lead for significant quality decrease */
                        param.analyse.b_chroma_me = FALSE;
                        param.analyse.b_mixed_references = FALSE;
                        param.analyse.f_psy_rd = 0.0;
                    }
                }
                else
                {
                    param.rc.b_stat_write = config->b_updatestats;
                    param.rc.b_stat_read = TRUE;
                }
                break;

            default:
                assert(0);
                break;
        }

        /* For default zero latency */
        param.i_sync_lookahead = 0;
        param.rc.i_lookahead = 0;

        {
            char *argv[MAX_ARG_NUM];
            char arg_mem[MAX_CMDLINE];
            int argc = split_cmd(config->extra_cmdline, argv, (char *)&arg_mem);

            /* Parse extra command line options */
            if (Parse(argc, argv, &param, codec) < 0)
            {
                codec->b_encoder_error = TRUE;
                compress_end(codec);
                return ICERR_ERROR;
            }
        }
    }

    /* Open the encoder */
    codec->h = x264_encoder_open(&param);
    if (!codec->h)
    {
        x264vfw_log(codec, X264_LOG_ERROR, "x264_encoder_open failed\n");
        codec->b_encoder_error = TRUE;
        compress_end(codec);
        return ICERR_ERROR;
    }

    x264_encoder_parameters(codec->h, &param);

    if (codec->b_cli_output)
    {
#if X264VFW_USE_VIRTUALDUB_HACK
        codec->b_use_vd_hack = FALSE;
#endif
        codec->b_no_output = TRUE;
        if (codec->cli_output.set_param(codec->cli_hout, &param) < 0)
        {
            x264vfw_log(codec, X264_LOG_ERROR, "can't set outfile param\n");
            codec->b_encoder_error = TRUE;
            compress_end(codec);
            return ICERR_ERROR;
        }
        if (!param.b_repeat_headers)
        {
            /* Write SPS/PPS/SEI */
            x264_nal_t *headers;
            int i_nal;

            if (x264_encoder_headers(codec->h, &headers, &i_nal) < 0)
            {
                x264vfw_log(codec, X264_LOG_ERROR, "x264_encoder_headers failed\n");
                codec->b_encoder_error = TRUE;
                compress_end(codec);
                return ICERR_ERROR;
            }
            if (codec->cli_output.write_headers(codec->cli_hout, headers) < 0)
            {
                x264vfw_log(codec, X264_LOG_ERROR, "can't write SPS/PPS/SEI to outfile\n");
                codec->b_encoder_error = TRUE;
                compress_end(codec);
                return ICERR_ERROR;
            }
        }
    }

    return ICERR_OK;
}

static int encode_frame(CODEC *codec, x264_picture_t *pic, x264_picture_t *pic_out, uint8_t *buf, DWORD buf_size, int *got_picture)
{
    x264_nal_t *nal;
    int        i_nal;
    int        i_frame_size;

    *got_picture = 0;
    i_frame_size = x264_encoder_encode(codec->h, &nal, &i_nal, pic, pic_out);
    if (i_frame_size < 0)
    {
        x264vfw_log(codec, X264_LOG_ERROR, "x264_encoder_encode failed\n");
        return -1;
    }
    if (i_frame_size)
    {
        *got_picture = 1;
        if (codec->b_cli_output)
            if (codec->cli_output.write_frame(codec->cli_hout, nal[0].p_payload, i_frame_size, pic_out) < 0)
            {
                x264vfw_log(codec, X264_LOG_ERROR, "can't write frame to outfile\n");
                return -1;
            }
        if (!codec->b_no_output && buf)
        {
#if X264VFW_USE_BUGGY_APPS_HACK
            if (i_frame_size > buf_size && codec->b_check_size)
#else
            if (i_frame_size > buf_size)
#endif
            {
                x264vfw_log(codec, X264_LOG_ERROR, "output frame buffer too small (size %d / needed %d)\n", (int)buf_size, i_frame_size);
                return -1;
            }
            memcpy(buf, nal[0].p_payload, i_frame_size);
        }
        else
            i_frame_size = 0;
    }
    return i_frame_size;
}

/* Compress a frame of data */
LRESULT compress(CODEC *codec, ICCOMPRESS *icc)
{
    BITMAPINFOHEADER *inhdr = icc->lpbiInput;
    BITMAPINFOHEADER *outhdr = icc->lpbiOutput;

    x264_picture_t pic;
    x264_picture_t pic_out;

    int        i_out;
    int        got_picture;

    int        i_csp;
    int        iWidth;
    int        iHeight;

#if X264VFW_USE_BUGGY_APPS_HACK
    /* Workaround for the bug in some weird applications
       Because Microsoft's documentation is incomplete many applications are buggy in my opinion
       lpbiOutput->biSizeImage is used to return value so the applications should update lpbiOutput->biSizeImage on every call
       But some applications don't update it, thus lpbiOutput->biSizeImage become smaller and smaller
       The size of the buffer isn't likely to change during encoding */
    if (codec->prev_lpbiOutput == outhdr)
        outhdr->biSizeImage = X264_MAX(outhdr->biSizeImage, codec->prev_output_biSizeImage);
    codec->prev_lpbiOutput = outhdr;
    codec->prev_output_biSizeImage = outhdr->biSizeImage;
#endif

    if (codec->i_frame_remain)
    {
        if (codec->i_frame_remain != -1)
            codec->i_frame_remain--;

        /* Init the picture */
        memset(&pic, 0, sizeof(x264_picture_t));
        pic.img.i_csp = get_csp(inhdr);
        i_csp   = pic.img.i_csp & X264VFW_CSP_MASK;
        iWidth  = inhdr->biWidth;
        iHeight = abs(inhdr->biHeight);

        switch (i_csp)
        {
            case X264VFW_CSP_I420:
            case X264VFW_CSP_YV12:
                pic.img.i_plane     = 3;
                pic.img.i_stride[0] = iWidth;
                pic.img.i_stride[1] =
                pic.img.i_stride[2] = pic.img.i_stride[0] / 2;

                pic.img.plane[0]    = (uint8_t *)icc->lpInput;
                pic.img.plane[1]    = pic.img.plane[0] + pic.img.i_stride[0] * iHeight;
                pic.img.plane[2]    = pic.img.plane[1] + pic.img.i_stride[1] * iHeight / 2;
                break;

            case X264VFW_CSP_YUYV:
            case X264VFW_CSP_UYVY:
                pic.img.i_plane     = 1;
                pic.img.i_stride[0] = 2 * iWidth;
                pic.img.plane[0]    = (uint8_t *)icc->lpInput;
                break;

            case X264VFW_CSP_BGR:
                pic.img.i_plane     = 1;
                pic.img.i_stride[0] = (3 * iWidth + 3) & ~3;
                pic.img.plane[0]    = (uint8_t *)icc->lpInput;
                break;

            case X264VFW_CSP_BGRA:
                pic.img.i_plane     = 1;
                pic.img.i_stride[0] = 4 * iWidth;
                pic.img.plane[0]    = (uint8_t *)icc->lpInput;
                break;

            default:
                x264vfw_log(codec, X264_LOG_ERROR, "unknown input frame colorspace\n");
                codec->b_encoder_error = TRUE;
                return ICERR_BADFORMAT;
        }

        codec->csp.convert[i_csp](&codec->conv_pic.img, &pic.img, iWidth, iHeight);

        /* Support keyframe forcing */
        /* Disabled because VirtualDub incorrectly force them with "VirtualDub Hack" option */
        //codec->conv_pic.i_type = icc->dwFlags & ICCOMPRESS_KEYFRAME ? X264_TYPE_IDR : X264_TYPE_AUTO;

        /* Encode it */
        i_out = encode_frame(codec, &codec->conv_pic, &pic_out, icc->lpOutput, outhdr->biSizeImage, &got_picture);
        codec->conv_pic.i_pts++;
    }
    else
        i_out = encode_frame(codec, NULL, &pic_out, icc->lpOutput, outhdr->biSizeImage, &got_picture);

    if (i_out < 0)
    {
        codec->b_encoder_error = TRUE;
        return ICERR_ERROR;
    }

#if X264VFW_USE_VIRTUALDUB_HACK
    if (codec->b_use_vd_hack && !got_picture)
    {
        *icc->lpdwFlags = 0;
        ((uint8_t *)icc->lpOutput)[0] = 0x7f;
        outhdr->biSizeImage = 1;
        outhdr->biCompression = mmioFOURCC('X','V','I','D');
    }
    else
    {
#endif
        if (pic_out.b_keyframe)
            *icc->lpdwFlags = AVIIF_KEYFRAME;
        else
            *icc->lpdwFlags = 0;
        outhdr->biSizeImage = i_out;
#if X264VFW_USE_VIRTUALDUB_HACK
        outhdr->biCompression = codec->save_fcc;
    }
#endif

    return ICERR_OK;
}

/* End compression and free resources allocated for compression */
LRESULT compress_end(CODEC *codec)
{
    if (codec->h)
    {
        if (!codec->b_encoder_error && codec->b_cli_output)
        {
            x264_picture_t pic_out;
            int got_picture;

            /* Flush delayed frames */
            while (x264_encoder_delayed_frames(codec->h))
                if (encode_frame(codec, NULL, &pic_out, NULL, 0, &got_picture) < 0)
                    break;
        }
        x264_encoder_close(codec->h);
        codec->h = NULL;
    }
    if (codec->b_cli_output)
    {
        if (codec->cli_hout)
        {
            codec->cli_output.close_file(codec->cli_hout, codec->conv_pic.i_pts - 1, codec->conv_pic.i_pts - 2);
            codec->cli_hout = NULL;
        }
        memset(&codec->cli_output, 0, sizeof(cli_output_t));
        codec->b_cli_output = FALSE;
    }
    x264_picture_clean(&codec->conv_pic);
    memset(&codec->conv_pic, 0, sizeof(x264_picture_t));
    codec->b_encoder_error = FALSE;
    return ICERR_OK;
}

/* Set the parameters for the pending compression */
LRESULT compress_frames_info(CODEC *codec, ICCOMPRESSFRAMES *icf)
{
    codec->i_frame_total = icf->lFrameCount;
    codec->i_fps_num = icf->dwRate;
    codec->i_fps_den = icf->dwScale;
    return ICERR_OK;
}

/* Set the parameters for the pending compression (default values for buggy applications which don't send ICM_COMPRESS_FRAMES_INFO) */
void default_compress_frames_info(CODEC *codec)
{
    /* Zero values are correct (they will be checked in compress_begin) */
    codec->i_frame_total = 0;
    codec->i_fps_num = 0;
    codec->i_fps_den = 0;
}

#if defined(HAVE_FFMPEG) && X264VFW_USE_DECODER
LRESULT decompress_get_format(CODEC *codec, BITMAPINFO *lpbiInput, BITMAPINFO *lpbiOutput)
{
    BITMAPINFOHEADER *inhdr = &lpbiInput->bmiHeader;
    BITMAPINFOHEADER *outhdr = &lpbiOutput->bmiHeader;
    int              iWidth;
    int              iHeight;

    if (!lpbiOutput)
        return sizeof(BITMAPINFOHEADER);

    if (!supported_fourcc(inhdr->biCompression))
        return ICERR_BADFORMAT;

    iWidth  = inhdr->biWidth;
    iHeight = inhdr->biHeight;
    if (iWidth <= 0 || iHeight <= 0)
        return ICERR_BADFORMAT;
    /* We need x2 width/height */
    if (iWidth % 2 || iHeight % 2)
        return ICERR_BADFORMAT;

    memset(outhdr, 0, sizeof(BITMAPINFOHEADER));
    outhdr->biSize        = sizeof(BITMAPINFOHEADER);
    outhdr->biWidth       = iWidth;
    outhdr->biHeight      = iHeight;
    outhdr->biPlanes      = 1;
    outhdr->biBitCount    = 32;
    outhdr->biCompression = BI_RGB;
    outhdr->biSizeImage   = x264vfw_picture_get_size(PIX_FMT_BGRA, iWidth, iHeight);

    return ICERR_OK;
}

LRESULT decompress_query(CODEC *codec, BITMAPINFO *lpbiInput, BITMAPINFO *lpbiOutput)
{
    BITMAPINFOHEADER *inhdr = &lpbiInput->bmiHeader;
    BITMAPINFOHEADER *outhdr = &lpbiOutput->bmiHeader;
    int              iWidth;
    int              iHeight;
    int              i_csp;
    enum PixelFormat pix_fmt;

    if (!supported_fourcc(inhdr->biCompression))
        return ICERR_BADFORMAT;

    iWidth  = inhdr->biWidth;
    iHeight = inhdr->biHeight;
    if (iWidth <= 0 || iHeight <= 0)
        return ICERR_BADFORMAT;
    /* We need x2 width/height */
    if (iWidth % 2 || iHeight % 2)
        return ICERR_BADFORMAT;

    if (!lpbiOutput)
        return ICERR_OK;

    if (iWidth != outhdr->biWidth || iHeight != abs(outhdr->biHeight))
        return ICERR_BADFORMAT;

    i_csp = get_csp(outhdr);
    if (i_csp == X264VFW_CSP_NONE)
        return ICERR_BADFORMAT;

    pix_fmt = csp_to_pix_fmt(i_csp & X264VFW_CSP_MASK);
    if (pix_fmt == PIX_FMT_NONE)
        return ICERR_BADFORMAT;

    /* MSDN says that biSizeImage may be set to zero for BI_RGB bitmaps
       But some buggy applications don't set it also for other bitmap types */
    if (outhdr->biSizeImage != 0 && outhdr->biSizeImage < x264vfw_picture_get_size(pix_fmt, iWidth, iHeight))
        return ICERR_BADFORMAT;

    return ICERR_OK;
}

LRESULT decompress_begin(CODEC *codec, BITMAPINFO *lpbiInput, BITMAPINFO *lpbiOutput)
{
    int i_csp;

    decompress_end(codec);

    if (decompress_query(codec, lpbiInput, lpbiOutput) != ICERR_OK)
    {
        x264vfw_log(codec, X264_LOG_ERROR, "incompatible input/output frame format (decode)\n");
        return ICERR_BADFORMAT;
    }

    i_csp = get_csp(&lpbiOutput->bmiHeader);
    codec->decoder_pix_fmt = csp_to_pix_fmt(i_csp & X264VFW_CSP_MASK);
    codec->decoder_vflip = (i_csp & X264VFW_CSP_VFLIP) != 0;
    codec->decoder_swap_UV = (i_csp & X264VFW_CSP_MASK) == X264VFW_CSP_YV12;

    codec->decoder = avcodec_find_decoder(CODEC_ID_H264);
    if (!codec->decoder)
    {
        x264vfw_log(codec, X264_LOG_ERROR, "avcodec_find_decoder failed\n");
        return ICERR_ERROR;
    }

    codec->decoder_context = avcodec_alloc_context();
    if (!codec->decoder_context)
    {
        x264vfw_log(codec, X264_LOG_ERROR, "avcodec_alloc_context failed\n");
        return ICERR_ERROR;
    }

    codec->decoder_frame = avcodec_alloc_frame();
    if (!codec->decoder_frame)
    {
        x264vfw_log(codec, X264_LOG_ERROR, "avcodec_alloc_frame failed\n");
        av_freep(&codec->decoder_context);
        return ICERR_ERROR;
    }

    codec->decoder_context->coded_width  = lpbiInput->bmiHeader.biWidth;
    codec->decoder_context->coded_height = lpbiInput->bmiHeader.biHeight;
    codec->decoder_context->codec_tag = lpbiInput->bmiHeader.biCompression;

    if (avcodec_open(codec->decoder_context, codec->decoder) < 0)
    {
        x264vfw_log(codec, X264_LOG_ERROR, "avcodec_open failed\n");
        av_freep(&codec->decoder_context);
        av_freep(&codec->decoder_frame);
        return ICERR_ERROR;
    }

    av_init_packet(&codec->decoder_pkt);
    codec->decoder_pkt.data = NULL;
    codec->decoder_pkt.size = 0;

    return ICERR_OK;
}

LRESULT decompress(CODEC *codec, ICDECOMPRESS *icd)
{
    BITMAPINFOHEADER *inhdr = icd->lpbiInput;
    unsigned int neededsize = inhdr->biSizeImage + FF_INPUT_BUFFER_PADDING_SIZE;
    int len, got_picture;
    AVPicture picture;

    got_picture = 0;
#if X264VFW_USE_VIRTUALDUB_HACK
    if (!(inhdr->biSizeImage == 1 && ((uint8_t *)icd->lpInput)[0] == 0x7f))
    {
#endif
        if (codec->decoder_buf_size < neededsize)
        {
            av_free(codec->decoder_buf);
            codec->decoder_buf_size = 0;
            codec->decoder_buf = av_malloc(neededsize);
            if (!codec->decoder_buf)
            {
                x264vfw_log(codec, X264_LOG_ERROR, "failed to realloc decoder buffer\n");
                return ICERR_ERROR;
            }
            codec->decoder_buf_size = neededsize;
        }
        memcpy(codec->decoder_buf, icd->lpInput, inhdr->biSizeImage);
        memset(codec->decoder_buf + inhdr->biSizeImage, 0, FF_INPUT_BUFFER_PADDING_SIZE);
        codec->decoder_pkt.data = codec->decoder_buf;
        codec->decoder_pkt.size = inhdr->biSizeImage;
        len = avcodec_decode_video2(codec->decoder_context, codec->decoder_frame, &got_picture, &codec->decoder_pkt);
        if (len < 0)
        {
            x264vfw_log(codec, X264_LOG_ERROR, "avcodec_decode_video2 failed\n");
            return ICERR_ERROR;
        }
#if X264VFW_USE_VIRTUALDUB_HACK
    }
#endif

    if (!got_picture)
    {
        /* Frame was decoded but delayed so we would show the BLACK-frame instead */
        int picture_size = x264vfw_picture_get_size(codec->decoder_pix_fmt, inhdr->biWidth, inhdr->biHeight);

        if (codec->decoder_pix_fmt == PIX_FMT_YUV420P)
        {
            int luma_size = picture_size * 2 / 3;
            memset(icd->lpOutput, 0x10, luma_size); /* TV Scale */
            memset(icd->lpOutput + luma_size, 0x80, picture_size - luma_size);
        }
        else if (codec->decoder_pix_fmt == PIX_FMT_YUYV422)
            wmemset(icd->lpOutput, 0x8010, picture_size / sizeof(wchar_t)); /* TV Scale */
        else
            memset(icd->lpOutput, 0x00, picture_size);
        //icd->lpbiOutput->biSizeImage = picture_size;
        return ICERR_OK;
    }

    if (x264vfw_picture_fill(&picture, icd->lpOutput, codec->decoder_pix_fmt, inhdr->biWidth, inhdr->biHeight) < 0)
    {
        x264vfw_log(codec, X264_LOG_ERROR, "x264vfw_picture_fill failed\n");
        return ICERR_ERROR;
    }
    if (codec->decoder_swap_UV)
    {
        uint8_t *temp_data;
        int     temp_linesize;

        temp_data = picture.data[1];
        temp_linesize = picture.linesize[1];
        picture.data[1] = picture.data[2];
        picture.linesize[1] = picture.linesize[2];
        picture.data[2] = temp_data;
        picture.linesize[2] = temp_linesize;
    }
    if (codec->decoder_vflip)
    {
        if (picture.data[0])
        {
            picture.data[0] += picture.linesize[0] * (inhdr->biHeight - 1);
            picture.linesize[0] = -picture.linesize[0];
        }
        if (picture.data[1])
        {
            picture.data[1] += picture.linesize[1] * (inhdr->biHeight - 1);
            picture.linesize[1] = -picture.linesize[1];
        }
        if (picture.data[2])
        {
            picture.data[2] += picture.linesize[2] * (inhdr->biHeight - 1);
            picture.linesize[2] = -picture.linesize[2];
        }
        if (picture.data[3])
        {
            picture.data[3] += picture.linesize[3] * (inhdr->biHeight - 1);
            picture.linesize[3] = -picture.linesize[3];
        }
    }

    if (!codec->sws)
    {
        codec->sws = sws_getContext(inhdr->biWidth, inhdr->biHeight, codec->decoder_context->pix_fmt,
                                    inhdr->biWidth, inhdr->biHeight, codec->decoder_pix_fmt,
                                    SWS_FAST_BILINEAR |
                                    SWS_FULL_CHR_H_INT | SWS_FULL_CHR_H_INP | SWS_ACCURATE_RND |
                                    SWS_CPU_CAPS_MMX | SWS_CPU_CAPS_MMX2,
                                    NULL, NULL, NULL);
        if (!codec->sws)
        {
            x264vfw_log(codec, X264_LOG_ERROR, "sws_getContext failed\n");
            return ICERR_ERROR;
        }
    }

    sws_scale(codec->sws, (const uint8_t *const *)codec->decoder_frame->data, codec->decoder_frame->linesize, 0, inhdr->biHeight, picture.data, picture.linesize);
    //icd->lpbiOutput->biSizeImage = x264vfw_picture_get_size(codec->decoder_pix_fmt, inhdr->biWidth, inhdr->biHeight);

    return ICERR_OK;
}

LRESULT decompress_end(CODEC *codec)
{
    if (codec->decoder_context)
        avcodec_close(codec->decoder_context);
    av_freep(&codec->decoder_context);
    av_freep(&codec->decoder_frame);
    av_freep(&codec->decoder_buf);
    codec->decoder_buf_size = 0;
    sws_freeContext(codec->sws);
    codec->sws = NULL;
    return ICERR_OK;
}
#endif
