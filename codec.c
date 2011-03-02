/*****************************************************************************
 * codec.c: main encoding/decoding functions
 *****************************************************************************
 * Copyright (C) 2003-2011 x264vfw project
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

const named_str_t preset_table[COUNT_PRESET] =
{
    { "Ultrafast", "ultrafast" },
    { "Superfast", "superfast" },
    { "Veryfast",  "veryfast"  },
    { "Faster",    "faster"    },
    { "Fast",      "fast"      },
    { "Medium",    "medium"    },
    { "Slow",      "slow"      },
    { "Slower",    "slower"    },
    { "Veryslow",  "veryslow"  },
    { "Placebo",   "placebo"   }
};

const named_str_t tune_table[COUNT_TUNE] =
{
    { "None",        ""           },
    { "Film",        "film"       },
    { "Animation",   "animation"  },
    { "Grain",       "grain"      },
    { "Still image", "stillimage" },
    { "PSNR",        "psnr"       },
    { "SSIM",        "ssim"       }
};

const named_str_t profile_table[COUNT_PROFILE] =
{
    { "Auto",     ""         },
    { "Baseline", "baseline" },
    { "Main",     "main"     },
    { "High",     "high"     }
};

const named_int_t level_table[COUNT_LEVEL] =
{
    { "Auto", -1 },
    { "1.0",  10 },
    { "1b",    9 },
    { "1.1",  11 },
    { "1.2",  12 },
    { "1.3",  13 },
    { "2.0",  20 },
    { "2.1",  21 },
    { "2.2",  22 },
    { "3.0",  30 },
    { "3.1",  31 },
    { "3.2",  32 },
    { "4.0",  40 },
    { "4.1",  41 },
    { "4.2",  42 },
    { "5.0",  50 },
    { "5.1",  51 }
};

const named_fourcc_t fourcc_table[COUNT_FOURCC] =
{
    { "H264", mmioFOURCC('H','2','6','4') },
    { "h264", mmioFOURCC('h','2','6','4') },
    { "X264", mmioFOURCC('X','2','6','4') },
    { "x264", mmioFOURCC('x','2','6','4') },
    { "AVC1", mmioFOURCC('A','V','C','1') },
    { "avc1", mmioFOURCC('a','v','c','1') },
    { "VSSH", mmioFOURCC('V','S','S','H') }
};

const char * const muxer_names[] =
{
    "auto",
    "raw",
    "mkv",
    "flv",
    "mp4",
    "avi",
    NULL
};

const char * const log_level_names[] = { "none", "error", "warning", "info", "debug", NULL };

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
        case PIX_FMT_UYVY422:
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
    for (i = 0; i < COUNT_FOURCC; i++)
        if (fourcc == fourcc_table[i].value)
            return TRUE;
    return FALSE;
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
    outhdr->biCompression = config->i_fourcc >= 0 && config->i_fourcc < COUNT_FOURCC
                            ? fourcc_table[config->i_fourcc].value
                            : fourcc_table[0].value;
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

/* Log functions */
void x264vfw_log_create(CODEC *codec)
{
    x264vfw_log_destroy(codec);
    if (codec->config.i_log_level > 0)
        codec->hCons = CreateDialog(g_hInst, MAKEINTRESOURCE(IDD_LOG), GetDesktopWindow(), callback_log);
}

void x264vfw_log_destroy(CODEC *codec)
{
    if (codec->hCons)
    {
        DestroyWindow(codec->hCons);
        codec->hCons = NULL;
    }
    codec->b_visible = FALSE;
}

static void x264vfw_log_internal(CODEC *codec, const char *name, int i_level, const char *psz_fmt, va_list arg)
{
    char msg[1024];
    char *s_level;

    switch (i_level)
    {
        case X264_LOG_ERROR:
            s_level = "error";
            break;

        case X264_LOG_WARNING:
            s_level = "warning";
            break;

        case X264_LOG_INFO:
            s_level = "info";
            break;

        case X264_LOG_DEBUG:
            s_level = "debug";
            break;

        default:
            s_level = "unknown";
            break;
    }
    sprintf(msg, "%s [%s]: ", name, s_level);
    vsprintf(msg + strlen(msg), psz_fmt, arg);

    DPRINTF(msg);

    if (codec && codec->hCons)
    {
        int  idx;
        HWND h_console;
        HDC  hdc;

        /* Strip final linefeeds (required) */
        idx = strlen(msg) - 1;
        while (idx >= 0 && (msg[idx] == '\n' || msg[idx] == '\r'))
            msg[idx--] = 0;

        if (!codec->b_visible)
        {
            ShowWindow(codec->hCons, SW_SHOWNORMAL);
            codec->b_visible = TRUE;
        }
        h_console = GetDlgItem(codec->hCons, LOG_IDC_CONSOLE);
        idx = SendMessage(h_console, LB_ADDSTRING, 0, (LPARAM)msg);

        /* Make sure that the last added item is visible (autoscroll) */
        if (idx >= 0)
            SendMessage(h_console, LB_SETTOPINDEX, (WPARAM)idx, 0);

        hdc = GetDC(h_console);
        if (hdc)
        {
            HFONT hfntDefault;
            SIZE  sz;

            strcat(msg, "X"); /* Otherwise item may be clipped */
            hfntDefault = SelectObject(hdc, (HFONT)SendMessage(h_console, WM_GETFONT, 0, 0));
            GetTextExtentPoint32(hdc, msg, strlen(msg), &sz);
            if (sz.cx > SendMessage(h_console, LB_GETHORIZONTALEXTENT, 0, 0))
                SendMessage(h_console, LB_SETHORIZONTALEXTENT, (WPARAM)sz.cx, 0);
            SelectObject(hdc, hfntDefault);
            ReleaseDC(h_console, hdc);
        }
    }
}

void x264vfw_cli_log(void *p_private, const char *name, int i_level, const char *psz_fmt, ...)
{
    CODEC *codec = p_private;
    va_list arg;
    va_start(arg, psz_fmt);
    if (codec && (i_level <= codec->config.i_log_level - 1))
    {
        if (!codec->hCons)
            x264vfw_log_create(codec);
        x264vfw_log_internal(codec, name, i_level, psz_fmt, arg);
    }
    else
        x264vfw_log_internal(NULL, name, i_level, psz_fmt, arg);
    va_end(arg);
}

void x264vfw_log(CODEC *codec, int i_level, const char *psz_fmt, ...)
{
    va_list arg;
    va_start(arg, psz_fmt);
    if (codec && (i_level <= codec->config.i_log_level - 1))
    {
        if (!codec->hCons)
            x264vfw_log_create(codec);
        x264vfw_log_internal(codec, "x264vfw", i_level, psz_fmt, arg);
    }
    else
        x264vfw_log_internal(NULL, "x264vfw", i_level, psz_fmt, arg);
    va_end(arg);
}

static void x264vfw_log_callback(void *p_private, int i_level, const char *psz_fmt, va_list arg)
{
    x264vfw_log_internal(p_private, "x264vfw", i_level, psz_fmt, arg);
}

enum
{
    OPT_FRAMES = 256,
    OPT_SEEK,
    OPT_QPFILE,
    OPT_THREAD_INPUT,
    OPT_QUIET,
    OPT_NOPROGRESS,
    OPT_VISUALIZE,
    OPT_LONGHELP,
    OPT_PROFILE,
    OPT_PRESET,
    OPT_TUNE,
    OPT_FASTFIRSTPASS,
    OPT_SLOWFIRSTPASS,
    OPT_FULLHELP,
    OPT_FPS,
    OPT_MUXER,
    OPT_DEMUXER,
    OPT_INDEX,
    OPT_INTERLACED,
    OPT_TCFILE_IN,
    OPT_TCFILE_OUT,
    OPT_TIMEBASE,
    OPT_PULLDOWN,
    OPT_LOG_LEVEL,
    OPT_DTS_COMPRESSION,
#if X264VFW_USE_VIRTUALDUB_HACK
    OPT_VD_HACK,
#endif
    OPT_NO_OUTPUT
} OptionsOPT;

static char short_options[] = "8A:B:b:f:hI:i:m:o:p:q:r:t:Vvw";
static struct option long_options[] =
{
    { "help",              no_argument,       NULL, 'h'                 },
    { "longhelp",          no_argument,       NULL, OPT_LONGHELP        },
    { "fullhelp",          no_argument,       NULL, OPT_FULLHELP        },
    { "version",           no_argument,       NULL, 'V'                 },
    { "profile",           required_argument, NULL, OPT_PROFILE         },
    { "preset",            required_argument, NULL, OPT_PRESET          },
    { "tune",              required_argument, NULL, OPT_TUNE            },
    { "fast-firstpass",    no_argument,       NULL, OPT_FASTFIRSTPASS   },
    { "slow-firstpass",    no_argument,       NULL, OPT_SLOWFIRSTPASS   },
    { "bitrate",           required_argument, NULL, 'B'                 },
    { "bframes",           required_argument, NULL, 'b'                 },
    { "b-adapt",           required_argument, NULL, 0                   },
    { "no-b-adapt",        no_argument,       NULL, 0                   },
    { "b-bias",            required_argument, NULL, 0                   },
    { "b-pyramid",         required_argument, NULL, 0                   },
    { "open-gop",          required_argument, NULL, 0                   },
    { "min-keyint",        required_argument, NULL, 'i'                 },
    { "keyint",            required_argument, NULL, 'I'                 },
    { "intra-refresh",     no_argument,       NULL, 0                   },
    { "scenecut",          required_argument, NULL, 0                   },
    { "no-scenecut",       no_argument,       NULL, 0                   },
    { "nf",                no_argument,       NULL, 0                   },
    { "no-deblock",        no_argument,       NULL, 0                   },
    { "filter",            required_argument, NULL, 0                   },
    { "deblock",           required_argument, NULL, 'f'                 },
    { "interlaced",        no_argument,       NULL, 0                   },
    { "no-interlaced",     no_argument,       NULL, 0                   },
    { "tff",               no_argument,       NULL, 0                   },
    { "bff",               no_argument,       NULL, 0                   },
    { "constrained-intra", no_argument,       NULL, 0                   },
    { "cabac",             no_argument,       NULL, 0                   },
    { "no-cabac",          no_argument,       NULL, 0                   },
    { "qp",                required_argument, NULL, 'q'                 },
    { "qpmin",             required_argument, NULL, 0                   },
    { "qpmax",             required_argument, NULL, 0                   },
    { "qpstep",            required_argument, NULL, 0                   },
    { "crf",               required_argument, NULL, 0                   },
    { "rc-lookahead",      required_argument, NULL, 0                   },
    { "ref",               required_argument, NULL, 'r'                 },
    { "asm",               required_argument, NULL, 0                   },
    { "no-asm",            no_argument,       NULL, 0                   },
    { "sar",               required_argument, NULL, 0                   },
    { "fps",               required_argument, NULL, 0                   },
    { "frames",            required_argument, NULL, OPT_FRAMES          },
    { "seek",              required_argument, NULL, OPT_SEEK            },
    { "output",            required_argument, NULL, 'o'                 },
    { "muxer",             required_argument, NULL, OPT_MUXER           },
    { "demuxer",           required_argument, NULL, OPT_DEMUXER         },
    { "stdout",            required_argument, NULL, OPT_MUXER           },
    { "stdin",             required_argument, NULL, OPT_DEMUXER         },
    { "index",             required_argument, NULL, OPT_INDEX           },
    { "analyse",           required_argument, NULL, 0                   },
    { "partitions",        required_argument, NULL, 'A'                 },
    { "direct",            required_argument, NULL, 0                   },
    { "weightb",           no_argument,       NULL, 'w'                 },
    { "no-weightb",        no_argument,       NULL, 0                   },
    { "weightp",           required_argument, NULL, 0                   },
    { "me",                required_argument, NULL, 0                   },
    { "merange",           required_argument, NULL, 0                   },
    { "mvrange",           required_argument, NULL, 0                   },
    { "mvrange-thread",    required_argument, NULL, 0                   },
    { "subme",             required_argument, NULL, 'm'                 },
    { "psy-rd",            required_argument, NULL, 0                   },
    { "psy",               no_argument,       NULL, 0                   },
    { "no-psy",            no_argument,       NULL, 0                   },
    { "mixed-refs",        no_argument,       NULL, 0                   },
    { "no-mixed-refs",     no_argument,       NULL, 0                   },
    { "chroma-me",         no_argument,       NULL, 0                   },
    { "no-chroma-me",      no_argument,       NULL, 0                   },
    { "8x8dct",            no_argument,       NULL, '8'                 },
    { "no-8x8dct",         no_argument,       NULL, 0                   },
    { "trellis",           required_argument, NULL, 't'                 },
    { "fast-pskip",        no_argument,       NULL, 0                   },
    { "no-fast-pskip",     no_argument,       NULL, 0                   },
    { "dct-decimate",      no_argument,       NULL, 0                   },
    { "no-dct-decimate",   no_argument,       NULL, 0                   },
    { "aq-strength",       required_argument, NULL, 0                   },
    { "aq-mode",           required_argument, NULL, 0                   },
    { "deadzone-inter",    required_argument, NULL, 0                   },
    { "deadzone-intra",    required_argument, NULL, 0                   },
    { "level",             required_argument, NULL, 0                   },
    { "ratetol",           required_argument, NULL, 0                   },
    { "vbv-maxrate",       required_argument, NULL, 0                   },
    { "vbv-bufsize",       required_argument, NULL, 0                   },
    { "vbv-init",          required_argument, NULL, 0                   },
    { "crf-max",           required_argument, NULL, 0                   },
    { "ipratio",           required_argument, NULL, 0                   },
    { "pbratio",           required_argument, NULL, 0                   },
    { "chroma-qp-offset",  required_argument, NULL, 0                   },
    { "pass",              required_argument, NULL, 'p'                 },
    { "stats",             required_argument, NULL, 0                   },
    { "qcomp",             required_argument, NULL, 0                   },
    { "mbtree",            no_argument,       NULL, 0                   },
    { "no-mbtree",         no_argument,       NULL, 0                   },
    { "qblur",             required_argument, NULL, 0                   },
    { "cplxblur",          required_argument, NULL, 0                   },
    { "zones",             required_argument, NULL, 0                   },
    { "qpfile",            required_argument, NULL, OPT_QPFILE          },
    { "threads",           required_argument, NULL, 0                   },
    { "sliced-threads",    no_argument,       NULL, 0                   },
    { "no-sliced-threads", no_argument,       NULL, 0                   },
    { "slice-max-size",    required_argument, NULL, 0                   },
    { "slice-max-mbs",     required_argument, NULL, 0                   },
    { "slices",            required_argument, NULL, 0                   },
    { "thread-input",      no_argument,       NULL, OPT_THREAD_INPUT    },
    { "sync-lookahead",    required_argument, NULL, 0                   },
    { "deterministic",     no_argument,       NULL, 0                   },
    { "non-deterministic", no_argument,       NULL, 0                   },
    { "psnr",              no_argument,       NULL, 0                   },
    { "no-psnr",           no_argument,       NULL, 0                   },
    { "ssim",              no_argument,       NULL, 0                   },
    { "no-ssim",           no_argument,       NULL, 0                   },
    { "quiet",             no_argument,       NULL, OPT_QUIET           },
    { "verbose",           no_argument,       NULL, 'v'                 },
    { "log-level",         required_argument, NULL, OPT_LOG_LEVEL       },
    { "progress",          no_argument,       NULL, OPT_NOPROGRESS      },
    { "no-progress",       no_argument,       NULL, OPT_NOPROGRESS      },
    { "visualize",         no_argument,       NULL, OPT_VISUALIZE       },
    { "dump-yuv",          required_argument, NULL, 0                   },
    { "sps-id",            required_argument, NULL, 0                   },
    { "aud",               no_argument,       NULL, 0                   },
    { "no-aud",            no_argument,       NULL, 0                   },
    { "nr",                required_argument, NULL, 0                   },
    { "cqm",               required_argument, NULL, 0                   },
    { "cqmfile",           required_argument, NULL, 0                   },
    { "cqm4",              required_argument, NULL, 0                   },
    { "cqm4i",             required_argument, NULL, 0                   },
    { "cqm4iy",            required_argument, NULL, 0                   },
    { "cqm4ic",            required_argument, NULL, 0                   },
    { "cqm4p",             required_argument, NULL, 0                   },
    { "cqm4py",            required_argument, NULL, 0                   },
    { "cqm4pc",            required_argument, NULL, 0                   },
    { "cqm8",              required_argument, NULL, 0                   },
    { "cqm8i",             required_argument, NULL, 0                   },
    { "cqm8p",             required_argument, NULL, 0                   },
    { "overscan",          required_argument, NULL, 0                   },
    { "videoformat",       required_argument, NULL, 0                   },
    { "fullrange",         required_argument, NULL, 0                   },
    { "colorprim",         required_argument, NULL, 0                   },
    { "transfer",          required_argument, NULL, 0                   },
    { "colormatrix",       required_argument, NULL, 0                   },
    { "chromaloc",         required_argument, NULL, 0                   },
    { "force-cfr",         no_argument,       NULL, 0                   },
    { "tcfile-in",         required_argument, NULL, OPT_TCFILE_IN       },
    { "tcfile-out",        required_argument, NULL, OPT_TCFILE_OUT      },
    { "timebase",          required_argument, NULL, OPT_TIMEBASE        },
    { "pic-struct",        no_argument,       NULL, 0                   },
    { "crop-rect",         required_argument, NULL, 0                   },
    { "nal-hrd",           required_argument, NULL, 0                   },
    { "pulldown",          required_argument, NULL, OPT_PULLDOWN        },
    { "fake-interlaced",   no_argument,       NULL, 0                   },
    { "frame-packing",     required_argument, NULL, 0                   },
    { "dts-compress",      no_argument,       NULL, OPT_DTS_COMPRESSION },
#if X264VFW_USE_VIRTUALDUB_HACK
    { "vd-hack",           no_argument,       NULL, OPT_VD_HACK         },
#endif
    { "no-output",         no_argument,       NULL, OPT_NO_OUTPUT       },
    { NULL,                0,                 NULL, 0                   }
};

#define MAX_ARG_NUM (MAX_CMDLINE / 2 + 1)

/* Split command line (for quote in parameter it must be tripled) */
static int split_cmdline(const char *cmdline, char **argv, char *arg_mem)
{
    int  argc = 1;
    int  s = 0;
    char *p;
    const char *q;

    memset(argv, 0, sizeof(char *) * MAX_ARG_NUM);
    argv[0] = "x264vfw";
    p = arg_mem;
    q = cmdline;
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

static int select_output(const char *muxer, char *filename, x264_param_t *param, CODEC *codec)
{
    const char *ext = get_filename_extension(filename);

    if (!strcmp(filename, "-"))
        return 0;

    if (strcasecmp(muxer, "auto"))
        ext = muxer;

    if (!strcasecmp(ext, "mp4"))
    {
        codec->cli_output = mp4_output;
        param->b_annexb = 0;
        param->b_repeat_headers = 0;
        if (param->i_nal_hrd == X264_NAL_HRD_CBR)
        {
            x264vfw_log(codec, X264_LOG_WARNING, "cbr nal-hrd is not compatible with mp4\n");
            param->i_nal_hrd = X264_NAL_HRD_VBR;
        }
    }
    else if (!strcasecmp(ext, "mkv"))
    {
        codec->cli_output = mkv_output;
        param->b_annexb = 0;
        param->b_repeat_headers = 0;
    }
    else if (!strcasecmp(ext, "flv"))
    {
        codec->cli_output = flv_output;
        param->b_annexb = 0;
        param->b_repeat_headers = 0;
    }
    else if (!strcasecmp(ext, "avi"))
    {
#if defined(HAVE_FFMPEG)
        codec->cli_output = avi_output;
        param->b_annexb = 1;
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

/* Parse command line for preset/tune options */
static int parse_preset_tune(int argc, char **argv, x264_param_t *param, CODEC *codec)
{
    opterr = 0; /* Suppress error messages printing in getopt */
    optind = 0; /* Initialize getopt */

    for (;;)
    {
        int checked_optind = optind > 0 ? optind : 1;
        int c = getopt_long(argc, argv, short_options, long_options, NULL);
        if (c == -1)
            break;
        if (c == OPT_PRESET)
            codec->preset = optarg;
        else if (c == OPT_TUNE)
            codec->tune = optarg;
        else if (c == '?')
        {
            x264vfw_log(codec, X264_LOG_ERROR, "unknown option or absent argument: '%s'\n", argv[checked_optind]);
            return -1;
        }
    }
    return 0;
}

static int parse_enum_name(const char *arg, const char * const *names, const char **dst)
{
    int i;
    for (i = 0; names[i]; i++)
        if (!strcasecmp(arg, names[i]))
        {
            *dst = names[i];
            return 0;
        }
    return -1;
}

static int parse_enum_value(const char *arg, const char * const *names, int *dst)
{
    int i;
    for(i = 0; names[i]; i++)
        if (!strcasecmp(arg, names[i]))
        {
            *dst = i;
            return 0;
        }
    return -1;
}

/* Parse command line for all other options */
static int parse_cmdline(int argc, char **argv, x264_param_t *param, CODEC *codec)
{
    opterr = 0; /* Suppress error messages printing in getopt */
    optind = 0; /* Initialize getopt */

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
            case OPT_TCFILE_IN:
            case OPT_TCFILE_OUT:
            case OPT_TIMEBASE:
            case OPT_PULLDOWN:
                x264vfw_log(codec, X264_LOG_WARNING, "not supported option: '%s'\n", argv[checked_optind]);
                break;

            case 'o':
                codec->cli_output_file = optarg;
                break;

            case OPT_MUXER:
                if (parse_enum_name(optarg, muxer_names, &codec->cli_output_muxer) < 0)
                {
                    x264vfw_log(codec, X264_LOG_ERROR, "unknown muxer '%s'\n", optarg);
                    return -1;
                }
                break;

            case OPT_QUIET:
                param->i_log_level = X264_LOG_NONE;
                break;

            case 'v':
                param->i_log_level = X264_LOG_DEBUG;
                break;

            case OPT_LOG_LEVEL:
                if (!parse_enum_value(optarg, log_level_names, &param->i_log_level))
                    param->i_log_level += X264_LOG_NONE;
                else
                    param->i_log_level = atoi(optarg);
                break;

            case OPT_TUNE:
            case OPT_PRESET:
                break;

            case OPT_PROFILE:
                codec->profile = optarg;
                break;

            case OPT_FASTFIRSTPASS:
                codec->b_fast1pass = TRUE;
                break;

            case OPT_SLOWFIRSTPASS:
                codec->b_fast1pass = FALSE;
                break;

            case 'r':
                codec->b_user_ref = TRUE;
                goto generic_option;

            case OPT_DTS_COMPRESSION:
                codec->cli_output_opt.use_dts_compress = 1;
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
generic_option:
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
    return 0;
}

/* Prepare to compress data */
LRESULT compress_begin(CODEC *codec, BITMAPINFO *lpbiInput, BITMAPINFO *lpbiOutput)
{
    CONFIG *config = &codec->config;
    char tune_buf[64];
    x264_param_t param;
    char *argv[MAX_ARG_NUM];
    char arg_mem[MAX_CMDLINE];
    int argc;
    char stat_out[MAX_STATS_SIZE];
    char stat_in[MAX_STATS_SIZE];

    /* Destroy previous handle */
    compress_end(codec);
    /* Create log window (or clear it) */
    x264vfw_log_create(codec);

    if (compress_query(codec, lpbiInput, lpbiOutput) != ICERR_OK)
    {
        x264vfw_log(codec, X264_LOG_ERROR, "incompatible input/output frame format (encode)\n");
        codec->b_encoder_error = TRUE;
        return ICERR_BADFORMAT;
    }

    /* Default internal codec params */
#if X264VFW_USE_BUGGY_APPS_HACK
    codec->prev_lpbiOutput = NULL;
    codec->prev_output_biSizeImage = 0;
    codec->b_check_size = lpbiOutput->bmiHeader.biSizeImage != 0;
#endif
#if X264VFW_USE_VIRTUALDUB_HACK
    codec->b_use_vd_hack = config->b_vd_hack;
    codec->save_fourcc = lpbiOutput->bmiHeader.biCompression;
#endif
    codec->b_no_output = FALSE;
    codec->b_fast1pass = FALSE;
    codec->b_user_ref = FALSE;
    codec->i_frame_remain = codec->i_frame_total ? codec->i_frame_total : -1;

    /* Preset/Tuning/Profile */
    codec->preset = config->i_preset >= 0 && config->i_preset < COUNT_PRESET
                    ? preset_table[config->i_preset].value
                    : NULL;
    if (codec->preset && !codec->preset[0])
        codec->preset = NULL;
    codec->profile = config->i_profile >= 0 && config->i_profile < COUNT_PROFILE
                     ? profile_table[config->i_profile].value
                     : NULL;
    if (codec->profile && !codec->profile[0])
        codec->profile = NULL;
    if (config->i_tuning >= 0 && config->i_tuning < COUNT_TUNE)
        strcpy(tune_buf, tune_table[config->i_tuning].value);
    else
        strcpy(tune_buf, "");
    if (config->b_fastdecode)
    {
        if (tune_buf[0])
            strcat(tune_buf, ",");
        strcat(tune_buf, "fastdecode");
    }
    if (config->b_zerolatency)
    {
        if (tune_buf[0])
            strcat(tune_buf, ",");
        strcat(tune_buf, "zerolatency");
    }
    codec->tune = tune_buf[0] ? tune_buf : NULL;

    /* Split extra command line on separate args for getopt processing */
    argc = split_cmdline(config->extra_cmdline, argv, arg_mem);

    /* Presets are applied before all other options. */
    if (parse_preset_tune(argc, argv, &param, codec) < 0)
        goto fail;

    /* Get default x264 params */
    if (x264_param_default_preset(&param, codec->preset, codec->tune) < 0)
    {
        x264vfw_log(codec, X264_LOG_ERROR, "x264_param_default_preset failed\n");
        goto fail;
    }

    /* Video Properties */
    param.i_width  = lpbiInput->bmiHeader.biWidth;
    param.i_height = abs(lpbiInput->bmiHeader.biHeight);
    param.i_csp    = X264_CSP_I420;

    /* ICM_COMPRESS_FRAMES_INFO params */
    param.i_frame_total = codec->i_frame_total;
    if (codec->i_fps_num > 0 && codec->i_fps_den > 0)
    {
        param.i_fps_num = codec->i_fps_num;
        param.i_fps_den = codec->i_fps_den;
    }

    /* Basic */
    param.i_level_idc = config->i_level >= 0 && config->i_level < COUNT_LEVEL
                        ? level_table[config->i_level].value
                        : -1;

    /* Rate control */
    param.rc.b_stat_write = 0;
    param.rc.b_stat_read = 0;
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
                codec->b_fast1pass = config->b_fast1pass;
                param.rc.b_stat_write = 1;
            }
            else
            {
                param.rc.b_stat_write = config->b_updatestats;
                param.rc.b_stat_read = 1;
            }
            break;

        default:
            assert(0);
            break;
    }

    if (param.rc.b_stat_write || param.rc.b_stat_read)
    {
        strcpy(stat_out, config->stats);
        strcpy(stat_in, config->stats);
        param.rc.psz_stat_out = stat_out;
        param.rc.psz_stat_in = stat_in;
    }

    /* Output */
    codec->b_cli_output = FALSE;
    codec->cli_output_file = config->i_output_mode == 1 ? config->output_file : "-";
    codec->cli_output_muxer = muxer_names[0];
    memset(&codec->cli_output_opt, 0, sizeof(cli_output_opt_t));
    codec->cli_output_opt.p_private = codec;

    /* Sample Aspect Ratio */
    param.vui.i_sar_width = config->i_sar_width;
    param.vui.i_sar_height = config->i_sar_height;

    /* Debug */
    param.pf_log = x264vfw_log_callback;
    param.p_log_private = codec;
    param.i_log_level = config->i_log_level - 1;
    param.analyse.b_psnr = config->i_encoding_type > 0 && config->i_log_level >= 3 && config->b_psnr;
    param.analyse.b_ssim = config->i_encoding_type > 0 && config->i_log_level >= 3 && config->b_ssim;
    param.cpu = config->b_no_asm ? 0 : param.cpu;

    /* Parse extra command line options */
    if (parse_cmdline(argc, argv, &param, codec) < 0)
        goto fail;

    /* VFW supports only CFR */
    param.b_vfr_input = 0;
    param.i_timebase_num = param.i_fps_den;
    param.i_timebase_den = param.i_fps_num;

    /* If "1st pass (fast)" mode or --fast-firstpass is used, apply faster settings. */
    if (codec->b_fast1pass)
        x264_param_apply_fastfirstpass(&param);

    /* Apply profile restrictions. */
    if (x264_param_apply_profile(&param, codec->profile) < 0)
    {
        x264vfw_log(codec, X264_LOG_ERROR, "x264_param_apply_profile failed\n");
        goto fail;
    }

    /* Automatically reduce reference frame count to match the user's target level
     * if the user didn't explicitly set a reference frame count. */
    if (!codec->b_user_ref)
    {
        int i;
        int mbs = (((param.i_width)+15)>>4) * (((param.i_height)+15)>>4);
        for (i = 0; x264_levels[i].level_idc != 0; i++)
            if (param.i_level_idc == x264_levels[i].level_idc)
            {
                while (mbs * 384 * param.i_frame_reference > x264_levels[i].dpb &&
                       param.i_frame_reference > 1)
                {
                    param.i_frame_reference--;
                }
                break;
            }
    }

    /* Configure CLI output */
    if (select_output(codec->cli_output_muxer, codec->cli_output_file, &param, codec) < 0)
        goto fail;
    if (!codec->b_cli_output)
    {
        param.b_annexb = 1;
        param.b_repeat_headers = 1; /* VFW needs SPS/PPS before each keyframe */
    }
    if (!codec->b_no_output && codec->b_cli_output && codec->cli_output.open_file(codec->cli_output_file, &codec->cli_hout, &codec->cli_output_opt) < 0)
    {
        x264vfw_log(codec, X264_LOG_ERROR, "could not open output file: '%s'\n", codec->cli_output_file);
        goto fail;
    }

    /* Open the encoder */
    codec->h = x264_encoder_open(&param);
    if (!codec->h)
    {
        x264vfw_log(codec, X264_LOG_ERROR, "x264_encoder_open failed\n");
        goto fail;
    }

    x264_encoder_parameters(codec->h, &param);

    if (codec->b_cli_output)
    {
#if X264VFW_USE_VIRTUALDUB_HACK
        codec->b_use_vd_hack = FALSE;
#endif
        if (!codec->b_no_output)
        {
            if (codec->cli_output.set_param(codec->cli_hout, &param) < 0)
            {
                x264vfw_log(codec, X264_LOG_ERROR, "can't set outfile param\n");
                goto fail;
            }
            if (!param.b_repeat_headers)
            {
                /* Write SPS/PPS/SEI */
                x264_nal_t *headers;
                int i_nal;

                if (x264_encoder_headers(codec->h, &headers, &i_nal) < 0)
                {
                    x264vfw_log(codec, X264_LOG_ERROR, "x264_encoder_headers failed\n");
                    goto fail;
                }
                if (codec->cli_output.write_headers(codec->cli_hout, headers) < 0)
                {
                    x264vfw_log(codec, X264_LOG_ERROR, "can't write SPS/PPS/SEI to outfile\n");
                    goto fail;
                }
            }
        }
    }

#if X264VFW_USE_VIRTUALDUB_HACK
    codec->b_warn_frame_loss = !(codec->b_use_vd_hack || codec->b_cli_output);
#else
    codec->b_warn_frame_loss = !codec->b_cli_output;
#endif

    /* Colorspace conversion */
    x264vfw_csp_init(&codec->csp, param.i_csp, param.vui.i_colmatrix, param.vui.b_fullrange);
    if (x264_picture_alloc(&codec->conv_pic, param.i_csp, param.i_width, param.i_height) < 0)
    {
        x264vfw_log(codec, X264_LOG_ERROR, "x264_picture_alloc failed\n");
        goto fail;
    }

    return ICERR_OK;
fail:
    codec->b_encoder_error = TRUE;
    compress_end(codec);
    return ICERR_ERROR;
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
        if (!codec->b_no_output && codec->b_cli_output)
            if (codec->cli_output.write_frame(codec->cli_hout, nal[0].p_payload, i_frame_size, pic_out) < 0)
            {
                x264vfw_log(codec, X264_LOG_ERROR, "can't write frame to outfile\n");
                return -1;
            }
        if (!(codec->b_no_output || codec->b_cli_output) && buf)
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

    if (!got_picture && codec->b_warn_frame_loss)
    {
        codec->b_warn_frame_loss = FALSE;
        x264vfw_log(codec, X264_LOG_WARNING, "Few frames probably would be lost. Ways to fix this:\n");
#if X264VFW_USE_VIRTUALDUB_HACK
        x264vfw_log(codec, X264_LOG_WARNING, " - if you use VirtualDub or its fork than you can enable 'VirtualDub Hack' option\n");
#endif
        x264vfw_log(codec, X264_LOG_WARNING, " - you can enable 'File' output mode\n");
        x264vfw_log(codec, X264_LOG_WARNING, " - you can enable 'Zero Latency' option\n");
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
        outhdr->biCompression = codec->save_fourcc;
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
            int64_t largest_pts = codec->conv_pic.i_pts - 1;
            int64_t second_largest_pts = largest_pts - 1;
            codec->cli_output.close_file(codec->cli_hout, largest_pts, second_largest_pts);
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
    int              picture_size;

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

    picture_size = x264vfw_picture_get_size(PIX_FMT_BGRA, iWidth, iHeight);
    if (picture_size < 0)
        return ICERR_BADFORMAT;

    memset(outhdr, 0, sizeof(BITMAPINFOHEADER));
    outhdr->biSize        = sizeof(BITMAPINFOHEADER);
    outhdr->biWidth       = iWidth;
    outhdr->biHeight      = iHeight;
    outhdr->biPlanes      = 1;
    outhdr->biBitCount    = 32;
    outhdr->biCompression = BI_RGB;
    outhdr->biSizeImage   = picture_size;

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
    int              picture_size;

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

    picture_size = x264vfw_picture_get_size(pix_fmt, iWidth, iHeight);
    if (picture_size < 0)
        return ICERR_BADFORMAT;

    /* MSDN says that biSizeImage may be set to zero for BI_RGB bitmaps
       But some buggy applications don't set it also for other bitmap types */
    if (outhdr->biSizeImage != 0 && outhdr->biSizeImage < picture_size)
        return ICERR_BADFORMAT;

    return ICERR_OK;
}

LRESULT decompress_begin(CODEC *codec, BITMAPINFO *lpbiInput, BITMAPINFO *lpbiOutput)
{
    int i_csp;

    decompress_end(codec);

    if (decompress_query(codec, lpbiInput, lpbiOutput) != ICERR_OK)
    {
        x264vfw_log(codec, X264_LOG_DEBUG, "incompatible input/output frame format (decode)\n");
        return ICERR_BADFORMAT;
    }

    i_csp = get_csp(&lpbiOutput->bmiHeader);
    codec->decoder_pix_fmt = csp_to_pix_fmt(i_csp & X264VFW_CSP_MASK);
    codec->decoder_vflip = (i_csp & X264VFW_CSP_VFLIP) != 0;
    codec->decoder_swap_UV = (i_csp & X264VFW_CSP_MASK) == X264VFW_CSP_YV12;

    codec->decoder = avcodec_find_decoder(CODEC_ID_H264);
    if (!codec->decoder)
    {
        x264vfw_log(codec, X264_LOG_DEBUG, "avcodec_find_decoder failed\n");
        return ICERR_ERROR;
    }

    codec->decoder_context = avcodec_alloc_context();
    if (!codec->decoder_context)
    {
        x264vfw_log(codec, X264_LOG_DEBUG, "avcodec_alloc_context failed\n");
        return ICERR_ERROR;
    }

    codec->decoder_frame = avcodec_alloc_frame();
    if (!codec->decoder_frame)
    {
        x264vfw_log(codec, X264_LOG_DEBUG, "avcodec_alloc_frame failed\n");
        av_freep(&codec->decoder_context);
        return ICERR_ERROR;
    }

    codec->decoder_context->coded_width  = lpbiInput->bmiHeader.biWidth;
    codec->decoder_context->coded_height = lpbiInput->bmiHeader.biHeight;
    codec->decoder_context->codec_tag = lpbiInput->bmiHeader.biCompression;

    if (avcodec_open(codec->decoder_context, codec->decoder) < 0)
    {
        x264vfw_log(codec, X264_LOG_DEBUG, "avcodec_open failed\n");
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
    DWORD neededsize = inhdr->biSizeImage + FF_INPUT_BUFFER_PADDING_SIZE;
    int len, got_picture;
    AVPicture picture;
    int picture_size;

    got_picture = 0;
#if X264VFW_USE_VIRTUALDUB_HACK
    if (!(inhdr->biSizeImage == 1 && ((uint8_t *)icd->lpInput)[0] == 0x7f))
    {
#endif
        /* Check overflow */
        if (neededsize < FF_INPUT_BUFFER_PADDING_SIZE)
        {
            x264vfw_log(codec, X264_LOG_DEBUG, "buffer overflow check failed\n");
            return ICERR_ERROR;
        }
        if (codec->decoder_buf_size < neededsize)
        {
            av_free(codec->decoder_buf);
            codec->decoder_buf_size = 0;
            codec->decoder_buf = av_malloc(neededsize);
            if (!codec->decoder_buf)
            {
                x264vfw_log(codec, X264_LOG_DEBUG, "failed to realloc decoder buffer\n");
                return ICERR_ERROR;
            }
            codec->decoder_buf_size = neededsize;
        }
        memcpy(codec->decoder_buf, icd->lpInput, inhdr->biSizeImage);
        memset(codec->decoder_buf + inhdr->biSizeImage, 0, FF_INPUT_BUFFER_PADDING_SIZE);
        codec->decoder_pkt.data = codec->decoder_buf;
        codec->decoder_pkt.size = inhdr->biSizeImage;

        if (inhdr->biSizeImage >= 4)
        {
            uint8_t *buf = codec->decoder_buf;
            uint32_t buf_size = inhdr->biSizeImage;
            uint32_t nal_size = endian_fix32(*(uint32_t *)buf);
            /* Check startcode */
            if (nal_size != 0x00000001)
            {
                /* Check that this is correct size prefixed format */
                while ((uint64_t)buf_size >= (uint64_t)nal_size + 8)
                {
                    buf += nal_size + 4;
                    buf_size -= nal_size + 4;
                    nal_size = endian_fix32(*(uint32_t *)buf);
                }
                if ((uint64_t)buf_size == (uint64_t)nal_size + 4)
                {
                    /* Convert to Annex B */
                    buf = codec->decoder_buf;
                    buf_size = inhdr->biSizeImage;
                    nal_size = endian_fix32(*(uint32_t *)buf);
                    *(uint32_t *)buf = endian_fix32(0x00000001);
                    while ((uint64_t)buf_size >= (uint64_t)nal_size + 8)
                    {
                        buf += nal_size + 4;
                        buf_size -= nal_size + 4;
                        nal_size = endian_fix32(*(uint32_t *)buf);
                        *(uint32_t *)buf = endian_fix32(0x00000001);
                    }
                }
            }
        }

        len = avcodec_decode_video2(codec->decoder_context, codec->decoder_frame, &got_picture, &codec->decoder_pkt);
        if (len < 0)
        {
            x264vfw_log(codec, X264_LOG_DEBUG, "avcodec_decode_video2 failed\n");
            return ICERR_ERROR;
        }
#if X264VFW_USE_VIRTUALDUB_HACK
    }
#endif

    picture_size = x264vfw_picture_get_size(codec->decoder_pix_fmt, inhdr->biWidth, inhdr->biHeight);
    if (picture_size < 0)
    {
        x264vfw_log(codec, X264_LOG_DEBUG, "x264vfw_picture_get_size failed\n");
        return ICERR_ERROR;
    }

    if (!got_picture)
    {
        /* Frame was decoded but delayed so we would show the BLACK-frame instead */
        if (codec->decoder_pix_fmt == PIX_FMT_YUV420P)
        {
            int luma_size = picture_size * 2 / 3;
            memset(icd->lpOutput, 0x10, luma_size); /* TV Scale */
            memset(icd->lpOutput + luma_size, 0x80, picture_size - luma_size);
        }
        else if (codec->decoder_pix_fmt == PIX_FMT_YUYV422)
            wmemset(icd->lpOutput, 0x8010, picture_size / sizeof(wchar_t)); /* TV Scale */
        else if (codec->decoder_pix_fmt == PIX_FMT_UYVY422)
            wmemset(icd->lpOutput, 0x1080, picture_size / sizeof(wchar_t)); /* TV Scale */
        else
            memset(icd->lpOutput, 0x00, picture_size);
        //icd->lpbiOutput->biSizeImage = picture_size;
        return ICERR_OK;
    }

    if (x264vfw_picture_fill(&picture, icd->lpOutput, codec->decoder_pix_fmt, inhdr->biWidth, inhdr->biHeight) < 0)
    {
        x264vfw_log(codec, X264_LOG_DEBUG, "x264vfw_picture_fill failed\n");
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
            x264vfw_log(codec, X264_LOG_DEBUG, "sws_getContext failed\n");
            return ICERR_ERROR;
        }
    }

    sws_scale(codec->sws, (const uint8_t * const *)codec->decoder_frame->data, codec->decoder_frame->linesize, 0, inhdr->biHeight, picture.data, picture.linesize);
    //icd->lpbiOutput->biSizeImage = picture_size;

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
