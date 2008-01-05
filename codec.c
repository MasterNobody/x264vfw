/*****************************************************************************
 * codec.c: vfw x264 encoder
 *****************************************************************************
 * Copyright (C) 2003 Laurent Aimar
 * $Id: codec.c,v 1.1 2004/06/03 19:27:09 fenrir Exp $
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

#include "x264vfw.h"

#include <stdio.h>
#include <io.h>

#define X264_MAX(a, b) ((a)>(b) ? (a) : (b))
#define X264_MIN(a, b) ((a)<(b) ? (a) : (b))

/* Return a valid x264 colorspace or X264_CSP_NONE if unsuported */
static int get_csp(BITMAPINFOHEADER *hdr)
{
    int i_vflip = hdr->biHeight < 0 ? X264_CSP_VFLIP : 0;

    switch (hdr->biCompression)
    {
        case FOURCC_I420:
        case FOURCC_IYUV:
            return X264_CSP_I420 | i_vflip;

        case FOURCC_YV12:
            return X264_CSP_YV12 | i_vflip;

        case FOURCC_YUYV:
        case FOURCC_YUY2:
            return X264_CSP_YUYV | i_vflip;

        case BI_RGB:
        {
            i_vflip = hdr->biHeight < 0 ? 0 : X264_CSP_VFLIP;
            if (hdr->biBitCount == 24)
                return X264_CSP_BGR | i_vflip;
            if (hdr->biBitCount == 32)
                return X264_CSP_BGRA | i_vflip;
            return X264_CSP_NONE;
        }

        default:
            return X264_CSP_NONE;
    }
}

/* Return the output format of the compressed data */
LRESULT compress_get_format(CODEC *codec, BITMAPINFO *lpbiInput, BITMAPINFO *lpbiOutput)
{
    BITMAPINFOHEADER *inhdr = &lpbiInput->bmiHeader;
    BITMAPINFOHEADER *outhdr = &lpbiOutput->bmiHeader;
    CONFIG           *config = &codec->config;
    int              iHeight;

    if (lpbiOutput == NULL)
        return sizeof(BITMAPINFOHEADER);

    if (get_csp(inhdr) == X264_CSP_NONE)
        return ICERR_BADFORMAT;

    iHeight = inhdr->biHeight < 0 ? -inhdr->biHeight : inhdr->biHeight;
    if (inhdr->biWidth <= 0 || iHeight <= 0)
        return ICERR_BADFORMAT;
    /* We need x2 width/height */
    if (inhdr->biWidth % 2 || iHeight % 2)
        return ICERR_BADFORMAT;

    memcpy(outhdr, inhdr, sizeof(BITMAPINFOHEADER));
    outhdr->biSize = sizeof(BITMAPINFOHEADER);
    outhdr->biSizeImage = compress_get_size(codec, lpbiInput, lpbiOutput);
    outhdr->biXPelsPerMeter = 0;
    outhdr->biYPelsPerMeter = 0;
    outhdr->biClrUsed = 0;
    outhdr->biClrImportant = 0;
    outhdr->biCompression = mmioFOURCC(config->fcc[0], config->fcc[1], config->fcc[2], config->fcc[3]);

    return ICERR_OK;
}

/* Return the maximum number of bytes a single compressed frame can occupy */
LRESULT compress_get_size(CODEC *codec, BITMAPINFO *lpbiInput, BITMAPINFO *lpbiOutput)
{
    return ((lpbiOutput->bmiHeader.biWidth + 15) & ~15) * ((lpbiOutput->bmiHeader.biHeight + 31) & ~31) * 3 + 4096;
}

/* Test if driver can compress a specific input format to a specific output format */
LRESULT compress_query(CODEC *codec, BITMAPINFO *lpbiInput, BITMAPINFO *lpbiOutput)
{
    BITMAPINFOHEADER *inhdr = &lpbiInput->bmiHeader;
    BITMAPINFOHEADER *outhdr = &lpbiOutput->bmiHeader;
    CONFIG           *config = &codec->config;
    int              iHeight;

    if (get_csp(inhdr) == X264_CSP_NONE)
        return ICERR_BADFORMAT;

    iHeight = inhdr->biHeight < 0 ? -inhdr->biHeight : inhdr->biHeight;
    if (inhdr->biWidth <= 0 || iHeight <= 0)
        return ICERR_BADFORMAT;
    /* We need x2 width/height */
    if (inhdr->biWidth % 2 || iHeight % 2)
        return ICERR_BADFORMAT;

    if (lpbiOutput == NULL)
        return ICERR_OK;

    if (inhdr->biWidth != outhdr->biWidth || iHeight != outhdr->biHeight)
        return ICERR_BADFORMAT;

    if (outhdr->biCompression != mmioFOURCC(config->fcc[0], config->fcc[1], config->fcc[2], config->fcc[3]))
        return ICERR_BADFORMAT;

    return ICERR_OK;
}

void x264_log_vfw_create(CODEC *codec)
{
    x264_log_vfw_destroy(codec);
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

    vsprintf(error_msg, psz_fmt, arg);

    /* Strip final linefeeds (required) */
    idx = strlen(error_msg) - 1;
    while (idx >= 0 && error_msg[idx] == '\n')
        error_msg[idx--] = 0;

    if (!codec->b_visible)
    {
        ShowWindow(codec->hCons, SW_SHOW);
        codec->b_visible = TRUE;
    }
    idx = SendDlgItemMessage(codec->hCons, IDC_CONSOLE, LB_ADDSTRING, 0, (LPARAM)error_msg);

    /* Make sure that the last item added is visible (autoscroll) */
    if (idx >= 0)
        SendDlgItemMessage(codec->hCons, IDC_CONSOLE, LB_SETTOPINDEX, (WPARAM)idx, 0);
}

/* Prepare to compress data */
LRESULT compress_begin(CODEC *codec, BITMAPINFO *lpbiInput, BITMAPINFO *lpbiOutput)
{
    CONFIG *config = &codec->config;
    x264_param_t param;

    /* Destroy previous handle */
    compress_end(codec);

    if (compress_query(codec, lpbiInput, lpbiOutput) != ICERR_OK)
        return ICERR_BADFORMAT;

#ifdef BUGGY_APPS_HACK
    codec->prev_lpbiOutput = NULL;
    codec->prev_output_biSizeImage = 0;
#endif
#ifdef VIRTUALDUB_HACK
    codec->i_frame_remain = codec->i_frame_total;
#endif

    /* Get default param */
    x264_param_default(&param);

    /* CPU flags */
#ifdef HAVE_PTHREAD
    param.i_threads = config->i_threads;
#else
    param.i_threads = 1;
#endif

    /* Video Properties */
    param.i_width  = lpbiInput->bmiHeader.biWidth;
    param.i_height = lpbiInput->bmiHeader.biHeight < 0 ? -lpbiInput->bmiHeader.biHeight : lpbiInput->bmiHeader.biHeight;
    param.i_frame_total = codec->i_frame_total;
    param.vui.i_sar_width = config->i_sar_width;
    param.vui.i_sar_height = config->i_sar_height;
    param.i_fps_num = codec->i_fps_num;
    param.i_fps_den = codec->i_fps_den;

    /* Bitstream parameters */
    param.i_frame_reference = config->i_refmax;
    param.i_keyint_min = config->i_keyint_min;
    param.i_keyint_max = config->i_keyint_max;
    param.i_scenecut_threshold = config->i_scenecut_threshold;
    param.i_bframe = config->i_bframe;
    param.b_bframe_adaptive = config->i_bframe > 0 && config->b_bframe_adaptive;
    param.i_bframe_bias = config->i_bframe > 0 && config->b_bframe_adaptive ? config->i_bframe_bias : 0;
    param.b_bframe_pyramid = config->i_bframe > 1 && config->b_b_refs;

    param.b_deblocking_filter = config->i_encoding_type > 0 && config->b_filter;
    param.i_deblocking_filter_alphac0 = config->i_encoding_type > 0 && config->b_filter ? config->i_inloop_a : 0;
    param.i_deblocking_filter_beta = config->i_encoding_type > 0 && config->b_filter ? config->i_inloop_b : 0;

    param.b_cabac = config->b_cabac;

    param.b_interlaced = config->b_interlaced;

    /* Log */
    x264_log_vfw_create(codec);
    param.pf_log = x264_log_vfw;
    param.p_log_private = codec;
    param.i_log_level = config->i_log_level - 1;

    /* Encoder analyser parameters */
    param.analyse.intra = 0;
    param.analyse.inter = 0;
    if (config->i_encoding_type > 0 && config->b_dct8x8 && config->b_i8x8)
    {
        param.analyse.intra |= X264_ANALYSE_I8x8;
        param.analyse.inter |= X264_ANALYSE_I8x8;
    }
    if (config->b_i4x4)
    {
        param.analyse.intra |= X264_ANALYSE_I4x4;
        param.analyse.inter |= X264_ANALYSE_I4x4;
    }
    if (config->b_psub16x16)
    {
        param.analyse.inter |= X264_ANALYSE_PSUB16x16;
        if (config->b_psub8x8)
            param.analyse.inter |= X264_ANALYSE_PSUB8x8;
    }
    if (config->i_bframe > 0 && config->b_bsub16x16)
        param.analyse.inter |= X264_ANALYSE_BSUB16x16;

    param.analyse.b_transform_8x8 = config->i_encoding_type > 0 && config->b_dct8x8;
    param.analyse.b_weighted_bipred = config->i_bframe > 0 && config->b_b_wpred;
    param.analyse.i_direct_mv_pred = config->i_bframe > 0 ? config->i_direct_mv_pred : 0;

    param.analyse.i_me_method = config->i_me_method;
    param.analyse.i_me_range = config->i_me_range;
    param.analyse.i_subpel_refine = config->i_subpel_refine + 1; /* 0..6 -> 1..7 */
    param.analyse.b_bidir_me = config->i_bframe > 0 && config->b_bidir_me;
    param.analyse.b_chroma_me = config->i_subpel_refine >= 4 && config->b_chroma_me;
    param.analyse.b_bframe_rdo = config->i_bframe > 0 && config->i_subpel_refine >= 5 && config->b_brdo;
    param.analyse.b_mixed_references = config->i_refmax > 1 && config->b_mixedref;
    param.analyse.i_trellis = config->i_encoding_type > 0 && config->b_cabac ? config->i_trellis : 0;
    param.analyse.i_noise_reduction = config->i_encoding_type > 0 ? config->i_noise_reduction : 0;

    param.analyse.b_psnr = config->i_log_level >= 3;
    param.analyse.b_ssim = config->i_log_level >= 3;

    /* Rate control parameters */
    param.rc.i_qp_min = config->i_encoding_type > 1 ? config->i_qp_min : 0;
    param.rc.i_qp_max = config->i_encoding_type > 1 ? config->i_qp_max : X264_QUANT_MAX;
    param.rc.i_qp_step = config->i_encoding_type > 1 ? config->i_qp_step : X264_QUANT_MAX;

    param.rc.f_ip_factor = config->i_encoding_type > 0 ? 1.0 + (float)config->i_key_boost / 100.0 : 1.0;
    param.rc.f_pb_factor = config->i_encoding_type > 0 && config->i_bframe > 0 ? 1.0 + (float)config->i_b_red / 100.0 : 1.0;
    param.rc.f_qcompress = config->i_encoding_type > 1 ? (float)config->i_curve_comp / 100.0 : 1.0;

    param.rc.psz_stat_out = malloc(MAX_PATH);
    param.rc.psz_stat_in = malloc(MAX_PATH);

    switch (config->i_encoding_type)
    {
        case 0: /* 1 PASS LOSSLESS */
            param.rc.i_rc_method = X264_RC_CQP;
            param.rc.i_qp_constant = 0;
            param.rc.b_stat_read = FALSE;
            param.rc.b_stat_write = FALSE;
            break;

        case 1: /* 1 PASS CQP */
            param.rc.i_rc_method = X264_RC_CQP;
            param.rc.i_qp_constant = config->i_qp;
            param.rc.b_stat_read = FALSE;
            param.rc.b_stat_write = FALSE;
            break;

        case 2: /* 1 PASS VBR */
            param.rc.i_rc_method = X264_RC_CRF;
            param.rc.f_rf_constant = (float)config->i_rf_constant * 0.1;
            param.rc.b_stat_read = FALSE;
            param.rc.b_stat_write = FALSE;
            break;

        case 3: /* 1 PASS ABR */
            param.rc.i_rc_method = X264_RC_ABR;
            param.rc.i_bitrate = config->i_passbitrate;
            param.rc.b_stat_read = FALSE;
            param.rc.b_stat_write = FALSE;
            break;

        case 4: /* 2 PASS */
        {
            param.rc.i_rc_method = X264_RC_ABR;
            param.rc.i_bitrate = config->i_passbitrate;
            strcpy(param.rc.psz_stat_in, config->stats);
            strcpy(param.rc.psz_stat_out, config->stats);
            if (config->i_pass <= 1)
            {
                param.rc.b_stat_read = FALSE;
                param.rc.b_stat_write = TRUE;
                param.rc.f_rate_tolerance = 4.0;
                if (config->b_fast1pass)
                {
                    /* Adjust or turn off some flags to gain speed, if needed */
                    param.i_frame_reference = 1;
                    param.analyse.intra = 0;
                    param.analyse.inter = 0;
                    param.analyse.b_transform_8x8 = FALSE;
                    param.analyse.i_me_method = X264_ME_DIA;
                    param.analyse.i_subpel_refine = X264_MIN(2, param.analyse.i_subpel_refine); //subme=1 may lead for significant quality decrease
                    param.analyse.b_chroma_me = FALSE;
                    param.analyse.b_bframe_rdo = FALSE;
                    param.analyse.b_mixed_references = FALSE;
                }
            }
            else
            {
                param.rc.b_stat_read = TRUE;
                param.rc.b_stat_write = config->b_updatestats;
            }
            break;
        }
    }

    /* Open the encoder */
    codec->h = x264_encoder_open(&param);

    free(param.rc.psz_stat_out);
    free(param.rc.psz_stat_in);

    if (codec->h == NULL)
        return ICERR_ERROR;

    return ICERR_OK;
}

/* Compress a frame of data */
LRESULT compress(CODEC *codec, ICCOMPRESS *icc)
{
    BITMAPINFOHEADER *inhdr = icc->lpbiInput;
    BITMAPINFOHEADER *outhdr = icc->lpbiOutput;

    x264_picture_t pic;
    x264_picture_t pic_out;

    x264_nal_t *nal;
    int        i_nal;
    int        i_out;
    int        iHeight;

#ifdef BUGGY_APPS_HACK
    // Work around applications' bugs
    // Because Microsoft's document is incomplete, many applications are buggy in my opinion.
    //  lpbiOutput->biSizeImage is used to return value, the applications should update lpbiOutput->biSizeImage on every call.
    //  But some applications doesn't do this, thus lpbiOutput->biSizeImage smaller and smaller.
    //  The size of the buffer isn't likely to change during encoding.
    if (codec->prev_lpbiOutput == outhdr)
        outhdr->biSizeImage = X264_MAX(outhdr->biSizeImage, codec->prev_output_biSizeImage); // looks like very bad code, but I have no choice. Not one application need this.
    codec->prev_lpbiOutput = outhdr;
    codec->prev_output_biSizeImage = outhdr->biSizeImage;
    // End of work around applications' bugs.
#endif
#ifdef VIRTUALDUB_HACK
    if (codec->i_frame_remain)
    {
        codec->i_frame_remain--;
#endif
        /* Init the picture */
        memset(&pic, 0, sizeof(x264_picture_t));
        pic.img.i_csp = get_csp(inhdr);

        switch (pic.img.i_csp & X264_CSP_MASK)
        {
            case X264_CSP_I420:
            case X264_CSP_YV12:
                iHeight = inhdr->biHeight < 0 ? -inhdr->biHeight : inhdr->biHeight;

                pic.img.i_plane     = 3;
                pic.img.i_stride[0] = inhdr->biWidth;
                pic.img.i_stride[1] =
                pic.img.i_stride[2] = pic.img.i_stride[0] / 2;

                pic.img.plane[0]    = (uint8_t *)icc->lpInput;
                pic.img.plane[1]    = pic.img.plane[0] + pic.img.i_stride[0] * iHeight;
                pic.img.plane[2]    = pic.img.plane[1] + pic.img.i_stride[1] * iHeight / 2;
                break;

            case X264_CSP_YUYV:
                pic.img.i_plane     = 1;
                pic.img.i_stride[0] = 2 * inhdr->biWidth;
                pic.img.plane[0]    = (uint8_t *)icc->lpInput;
                break;

            case X264_CSP_BGR:
                pic.img.i_plane     = 1;
                pic.img.i_stride[0] = (3 * inhdr->biWidth + 3) & ~3;
                pic.img.plane[0]    = (uint8_t *)icc->lpInput;
                break;

            case X264_CSP_BGRA:
                pic.img.i_plane     = 1;
                pic.img.i_stride[0] = 4 * inhdr->biWidth;
                pic.img.plane[0]    = (uint8_t *)icc->lpInput;
                break;

            default:
                return ICERR_BADFORMAT;
        }

        /* Encode it */
        x264_encoder_encode(codec->h, &nal, &i_nal, &pic, &pic_out);
#ifdef VIRTUALDUB_HACK
    }
    else
        x264_encoder_encode(codec->h, &nal, &i_nal, NULL, &pic_out);
#endif

    /* Create bitstream, unless we're dropping it in 1st pass */
    i_out = 0;

    if (codec->config.i_encoding_type != 4 || codec->config.i_pass > 1)
    {
        int i;
        for (i = 0; i < i_nal; i++)
        {
            int i_size = outhdr->biSizeImage - i_out;
            x264_nal_encode((uint8_t *)icc->lpOutput + i_out, &i_size, 1, &nal[i]);

            i_out += i_size;
        }
    }

#ifdef VIRTUALDUB_HACK
    if (codec->config.b_vd_hack && i_nal == 0)
    {
        *icc->lpdwFlags = 0;
        ((char *)icc->lpOutput)[0] = 0x7f;
        outhdr->biSizeImage = 1;
        outhdr->biCompression = mmioFOURCC('X','V','I','D');
    }
    else
    {
#endif
        /* Set key frame only for IDR, as they are real synch point, I frame
           aren't always synch point (ex: with multi refs, ref marking) */
        if (pic_out.i_type == X264_TYPE_IDR)
            *icc->lpdwFlags = AVIIF_KEYFRAME;
        else
            *icc->lpdwFlags = 0;
        outhdr->biSizeImage = i_out;
#ifdef VIRTUALDUB_HACK
        outhdr->biCompression = mmioFOURCC(codec->config.fcc[0], codec->config.fcc[1], codec->config.fcc[2], codec->config.fcc[3]);
    }
#endif

    return ICERR_OK;
}

/* End compression and free resources allocated for compression */
LRESULT compress_end(CODEC *codec)
{
    if (codec->h != NULL)
    {
        x264_encoder_close(codec->h);
        codec->h = NULL;
    }
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
