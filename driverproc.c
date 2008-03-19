/*****************************************************************************
 * drvproc.c: vfw x264 wrapper
 *****************************************************************************
 * Copyright (C) 2003 Laurent Aimar
 * $Id: driverproc.c,v 1.1 2004/06/03 19:27:09 fenrir Exp $
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

#ifdef PTW32_STATIC_LIB
#include <pthread.h>
#endif

#include "x264vfw.h"

#ifndef attribute_align_arg
#if defined(__GNUC__) && (__GNUC__ > 4 || __GNUC__ == 4 && __GNUC_MINOR__>1)
#    define attribute_align_arg __attribute__((force_align_arg_pointer))
#else
#    define attribute_align_arg
#endif
#endif

/* Global DLL instance */
HINSTANCE g_hInst;

/* Calling back point for our DLL so we can keep track of the window in g_hInst */
BOOL WINAPI DllMain(HANDLE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    g_hInst = (HINSTANCE) hModule;
    return TRUE;
}

static void log_callback(void* ptr, int level, const char* fmt, va_list vl)
{
    if (level <= av_log_get_level())
        DVPRINTF(fmt, vl);
}

/* This little puppy handles the calls which vfw programs send out to the codec */
LRESULT WINAPI attribute_align_arg DriverProc(DWORD dwDriverId, HDRVR hDriver, UINT uMsg, LPARAM lParam1, LPARAM lParam2)
{
    CODEC *codec = (CODEC *)dwDriverId;

    switch (uMsg)
    {
        case DRV_LOAD:
#ifdef PTW32_STATIC_LIB
            pthread_win32_process_attach_np();
            pthread_win32_thread_attach_np();
#endif
            avcodec_init();
            avcodec_register_all();
            av_log_set_callback(log_callback);
            return DRV_OK;

        case DRV_FREE:
#ifdef PTW32_STATIC_LIB
            pthread_win32_thread_detach_np();
            pthread_win32_process_detach_np();
#endif
            return DRV_OK;

        case DRV_OPEN:
        {
            ICOPEN *icopen = (ICOPEN *)lParam2;

            if (icopen && icopen->fccType != ICTYPE_VIDEO)
                return 0;

            if (!(codec = malloc(sizeof(CODEC))))
            {
                if (icopen)
                    icopen->dwError = ICERR_MEMORY;
                return 0;
            }

            memset(codec, 0, sizeof(CODEC));
            config_reg_load(&codec->config);
            default_compress_frames_info(codec);

            if (icopen)
                icopen->dwError = ICERR_OK;
            return (LRESULT)codec;
        }

        case DRV_CLOSE:
            /* From xvid: compress_end/decompress_end don't always get called */
            compress_end(codec);
            decompress_end(codec);
            x264_log_vfw_destroy(codec);
            free(codec);
            return DRV_OK;

        case DRV_QUERYCONFIGURE:
            return 0;

        case DRV_CONFIGURE:
            return DRV_CANCEL;

/*
        case DRV_DISABLE:
        case DRV_ENABLE:
        case DRV_INSTALL:
        case DRV_REMOVE:
        case DRV_EXITSESSION:
        case DRV_POWER:
            return DRV_OK;
*/

        /* ICM */
        case ICM_GETSTATE:
            if (!(void *)lParam1)
                return sizeof(CONFIG);
            if (lParam2 < sizeof(CONFIG))
                return ICERR_BADSIZE;
            memcpy((void *)lParam1, &codec->config, sizeof(CONFIG));
            /* Reset params that don't need saving */
            ((CONFIG *)lParam1)->b_save = 0;
            return ICERR_OK;

        case ICM_SETSTATE:
            if (!(void *)lParam1)
            {
                config_reg_load(&codec->config);
                return 0;
            }
            if (lParam2 < sizeof(CONFIG))
                return 0;
            memcpy(&codec->config, (void *)lParam1, sizeof(CONFIG));
            return sizeof(CONFIG);

        case ICM_GETINFO:
        {
            ICINFO *icinfo = (ICINFO *)lParam1;

            if (lParam2 < sizeof(ICINFO))
                return 0;

            /* Fill all members of the ICINFO structure except szDriver */
            icinfo->dwSize       = sizeof(ICINFO);
            icinfo->fccType      = ICTYPE_VIDEO;
            icinfo->fccHandler   = FOURCC_X264;
            icinfo->dwFlags      = VIDCF_COMPRESSFRAMES | VIDCF_FASTTEMPORALC | VIDCF_FASTTEMPORALD;
            icinfo->dwVersion    = 0;
            icinfo->dwVersionICM = ICVERSION;
            wcscpy(icinfo->szName, X264_NAME_L);
            wcscpy(icinfo->szDescription, X264_DESC_L);

            return sizeof(ICINFO);
        }

        case ICM_CONFIGURE:
            if (lParam1 != -1)
            {
                CONFIG temp;

                codec->config.b_save = FALSE;
                memcpy(&temp, &codec->config, sizeof(CONFIG));

                DialogBoxParam(g_hInst, MAKEINTRESOURCE(IDD_MAINCONFIG), (HWND)lParam1, callback_main, (LPARAM)&temp);

                if (temp.b_save)
                {
                    memcpy(&codec->config, &temp, sizeof(CONFIG));
                    config_reg_save(&codec->config);
                }
            }
            return ICERR_OK;

        case ICM_ABOUT:
            if (lParam1 != -1)
                DialogBoxParam(g_hInst, MAKEINTRESOURCE(IDD_ABOUT), (HWND)lParam1, callback_about, 0);
            return ICERR_OK;

        case ICM_GET:
            if (!(void *)lParam1)
                return 0;
            return ICERR_OK;

        case ICM_SET:
            return 0;

        /* Compressor */
        case ICM_COMPRESS_GET_FORMAT:
            return compress_get_format(codec, (BITMAPINFO *)lParam1, (BITMAPINFO *)lParam2);

        case ICM_COMPRESS_GET_SIZE:
            return compress_get_size(codec, (BITMAPINFO *)lParam1, (BITMAPINFO *)lParam2);

        case ICM_COMPRESS_QUERY:
            return compress_query(codec, (BITMAPINFO *)lParam1, (BITMAPINFO *)lParam2);

        case ICM_COMPRESS_BEGIN:
            return compress_begin(codec, (BITMAPINFO *)lParam1, (BITMAPINFO *)lParam2);

        case ICM_COMPRESS:
            return compress(codec, (ICCOMPRESS *)lParam1);

        case ICM_COMPRESS_END:
            default_compress_frames_info(codec);
            return compress_end(codec);

        case ICM_COMPRESS_FRAMES_INFO:
            return compress_frames_info(codec, (ICCOMPRESSFRAMES *)lParam1);

        /* Decompressor */
        case ICM_DECOMPRESS_GET_FORMAT:
            return decompress_get_format(codec, (BITMAPINFO *)lParam1, (BITMAPINFO *)lParam2);

        case ICM_DECOMPRESS_QUERY:
            return decompress_query(codec, (BITMAPINFO *)lParam1, (BITMAPINFO *)lParam2);

        case ICM_DECOMPRESS_BEGIN:
            return decompress_begin(codec, (BITMAPINFO *)lParam1, (BITMAPINFO *)lParam2);

        case ICM_DECOMPRESS:
            return decompress(codec, (ICDECOMPRESS *)lParam1);

        case ICM_DECOMPRESS_END:
            return decompress_end(codec);

        default:
            if (uMsg < DRV_USER)
                return DefDriverProc(dwDriverId, hDriver, uMsg, lParam1, lParam2);
            else
                return ICERR_UNSUPPORTED;
    }
}

void WINAPI Configure(HWND hwnd, HINSTANCE hinst, LPTSTR lpCmdLine, int nCmdShow)
{
    if (DriverProc(0, 0, DRV_LOAD, 0, 0))
    {
        DWORD dwDriverId;

        dwDriverId = DriverProc(0, 0, DRV_OPEN, 0, 0);
        if (dwDriverId != (DWORD)NULL)
        {
            DriverProc(dwDriverId, 0, ICM_CONFIGURE, (LPARAM)GetDesktopWindow(), 0);
            DriverProc(dwDriverId, 0, DRV_CLOSE, 0, 0);
        }
       DriverProc(0, 0, DRV_FREE, 0, 0);
    }
}
