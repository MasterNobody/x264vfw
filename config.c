/*****************************************************************************
 * config.c: vfw x264 encoder
 *****************************************************************************
 * Copyright (C) 2003 Laurent Aimar
 * $Id: config.c,v 1.1 2004/06/03 19:27:09 fenrir Exp $
 *
 * Authors: Justin Clay
 *          Laurent Aimar <fenrir@via.ecp.fr>
 *          Antony Boucher <proximodo@free.fr>
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

/**************************************************************************
 *
 *  History:
 *
 *  2004.05.14  CBR encode mode support
 *
 **************************************************************************/

#include "x264vfw.h"
#include <stdio.h> /* sprintf */
#include <commctrl.h>
#include <assert.h>

#ifdef _MSC_VER
#define X264_VERSION ""
#else
#include "config.h"
#endif

/* Registry */
#define X264_REG_KEY     HKEY_CURRENT_USER
#define X264_REG_PARENT  "Software\\GNU"
#define X264_REG_CHILD   "x264"            /* not "x264vfw" because of GordianKnot compatibility */
#define X264_REG_CLASS   "config"

/* Description */
#define X264_NAME        "x264vfw"
#define X264_DEF_TEXT    "Are you sure you want to load default values?"

/* Registery handling */
typedef struct
{
    char *reg_value;
    int  *config_int;
    int  default_int;
} reg_int_t;

typedef struct
{
    char *reg_value;
    char *config_str;
    char *default_str;
    int  max_len;      /* maximum string length, including the terminating NULL char */
} reg_str_t;

CONFIG reg;
HWND   hTooltip;
HWND   hTabs[8];
int    b_tabs_updated;

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

static const reg_int_t reg_int_table[] =
{
    /* Bitrate */
    { "encoding_type",   &reg.i_encoding_type,      4   }, /* take into account GordianKnot workaround */
    { "quantizer",       &reg.i_qp,                 26  },
    { "ratefactor",      &reg.i_rf_constant,        260 },
    { "passbitrate",     &reg.i_passbitrate,        800 },
    { "pass_number",     &reg.i_pass,               1   },
    { "fast1pass",       &reg.b_fast1pass,          0   },
    { "updatestats",     &reg.b_updatestats,        1   },

    /* Rate Control */
    { "key_boost",       &reg.i_key_boost,          40  },
    { "b_red",           &reg.i_b_red,              30  },
    { "curve_comp",      &reg.i_curve_comp,         60  },
    { "qp_min",          &reg.i_qp_min,             10  },
    { "qp_max",          &reg.i_qp_max,             51  },
    { "qp_step",         &reg.i_qp_step,            4   },
    { "scenecut",        &reg.i_scenecut_threshold, 40  },
    { "keyint_min",      &reg.i_keyint_min,         25  },
    { "keyint_max",      &reg.i_keyint_max,         250 },

    /* MBs & Frames */
    { "dct8x8",          &reg.b_dct8x8,             1   },
    { "i8x8",            &reg.b_i8x8,               1   },
    { "i4x4",            &reg.b_i4x4,               1   },
    { "psub16x16",       &reg.b_psub16x16,          1   },
    { "psub8x8",         &reg.b_psub8x8,            0   },
    { "bsub16x16",       &reg.b_bsub16x16,          1   },
    { "bmax",            &reg.i_bframe,             0   },
    { "vd_hack",         &reg.b_vd_hack,            0   },
    { "b_bias",          &reg.i_bframe_bias,        0   },
    { "b_adapt",         &reg.b_bframe_adaptive,    1   },
    { "b_refs",          &reg.b_b_refs,             1   },
    { "b_wpred",         &reg.b_b_wpred,            1   },
    { "b_bidir_me",      &reg.b_bidir_me,           1   },
    { "direct_pred",     &reg.i_direct_mv_pred,     1   },

    /* More... */
    { "subpel",          &reg.i_subpel_refine,      5   },
    { "brdo",            &reg.b_brdo,               1   },
    { "me_method",       &reg.i_me_method,          2   },
    { "me_range",        &reg.i_me_range,           16  },
    { "chroma_me",       &reg.b_chroma_me,          1   },
    { "refmax",          &reg.i_refmax,             1   },
    { "mixedref",        &reg.b_mixedref,           1   },
    { "log_level",       &reg.i_log_level,          2   },
    { "fourcc_num",      &reg.i_fcc_num,            0   },
    { "sar_width",       &reg.i_sar_width,          1   },
    { "sar_height",      &reg.i_sar_height,         1   },
    { "threads",         &reg.i_threads,            1   },
    { "cabac",           &reg.b_cabac,              1   },
    { "trellis",         &reg.i_trellis,            1   },
    { "noise_reduction", &reg.i_noise_reduction,    0   },
    { "loop_filter",     &reg.b_filter,             1   },
    { "inloop_a",        &reg.i_inloop_a,           0   },
    { "inloop_b",        &reg.i_inloop_b,           0   },
    { "interlaced",      &reg.b_interlaced,         0   },

    /* Command Line */
    { "use_cmdline",     &reg.b_use_cmdline,        0   }
};

static const reg_str_t reg_str_table[] =
{
    { "statsfile",     reg.stats,         ".\\x264.stats",           MAX_STATS_PATH     },
    { "extra_cmdline", reg.extra_cmdline, "",                        MAX_CMDLINE        },
    { "fourcc",        reg.fcc,           (char *)&fcc_str_table[0], sizeof(fourcc_str) },
    { "cmdline",       reg.cmdline,       "",                        MAX_CMDLINE        }
};

double GetDlgItemDouble(HWND hDlg, int nIDDlgItem)
{
    char temp[MAX_PATH];

    GetDlgItemText(hDlg, nIDDlgItem, temp, MAX_PATH);
    return atof(temp);
}

void SetDlgItemDouble(HWND hDlg, int nIDDlgItem, double dblValue)
{
    char temp[MAX_PATH];

    sprintf(temp, "%.1f", dblValue);
    SetDlgItemText(hDlg, nIDDlgItem, temp);
}

#define GordianKnotWorkaround(encoding_type)\
{\
    switch (encoding_type)\
    {\
        case 0:\
          encoding_type = 3;\
          break;\
        case 1:\
          encoding_type = 1;\
          break;\
        case 2:\
          encoding_type = 4;\
          break;\
        case 3:\
          encoding_type = 0;\
          break;\
        case 4:\
          encoding_type = 2;\
          break;\
        default:\
          assert(0);\
          break;\
    }\
}

/* Registry access */
void config_reg_load(CONFIG *config)
{
    HKEY    hKey;
    DWORD   i_size;
    int     i;

    RegOpenKeyEx(X264_REG_KEY, X264_REG_PARENT "\\" X264_REG_CHILD, 0, KEY_READ, &hKey);

    /* Read all integers */
    for (i = 0; i < sizeof(reg_int_table) / sizeof(reg_int_t); i++)
    {
        i_size = sizeof(int);
        if (RegQueryValueEx(hKey, reg_int_table[i].reg_value, 0, 0, (LPBYTE)reg_int_table[i].config_int, &i_size) != ERROR_SUCCESS)
            *reg_int_table[i].config_int = reg_int_table[i].default_int;
    }

    /* Read strings */
    for (i = 0; i < sizeof(reg_str_table) / sizeof(reg_str_t); i++)
    {
        i_size = reg_str_table[i].max_len;
        if (RegQueryValueEx(hKey, reg_str_table[i].reg_value, 0, 0, (LPBYTE)reg_str_table[i].config_str, &i_size) != ERROR_SUCCESS)
            strcpy(reg_str_table[i].config_str, reg_str_table[i].default_str);
    }

    RegCloseKey(hKey);

    GordianKnotWorkaround(reg.i_encoding_type);
    memcpy(config, &reg, sizeof(CONFIG));
}

void config_reg_save(CONFIG *config)
{
    HKEY    hKey;
    DWORD   dwDisposition;
    int     i;

    if (RegCreateKeyEx(X264_REG_KEY, X264_REG_PARENT "\\" X264_REG_CHILD, 0, X264_REG_CLASS, REG_OPTION_NON_VOLATILE, KEY_WRITE, 0, &hKey, &dwDisposition) != ERROR_SUCCESS)
        return;

    memcpy(&reg, config, sizeof(CONFIG));
    GordianKnotWorkaround(reg.i_encoding_type);

    /* Save all integers */
    for (i = 0; i < sizeof(reg_int_table) / sizeof(reg_int_t); i++)
        RegSetValueEx(hKey, reg_int_table[i].reg_value, 0, REG_DWORD, (LPBYTE)reg_int_table[i].config_int, sizeof(int));

    /* Save strings */
    for (i = 0; i < sizeof(reg_str_table) / sizeof(reg_str_t); i++)
        RegSetValueEx(hKey, reg_str_table[i].reg_value, 0, REG_SZ, (LPBYTE)reg_str_table[i].config_str, strlen(reg_str_table[i].config_str) + 1);

    RegCloseKey(hKey);
}

void config_defaults(CONFIG *config)
{
    int     i;

    /* Read all integers */
    for (i = 0; i < sizeof(reg_int_table) / sizeof(reg_int_t); i++)
        *reg_int_table[i].config_int = reg_int_table[i].default_int;

    /* Read strings */
    for (i = 0; i < sizeof(reg_str_table) / sizeof(reg_str_t); i++)
        strcpy(reg_str_table[i].config_str, reg_str_table[i].default_str);

    GordianKnotWorkaround(reg.i_encoding_type);
    memcpy(config, &reg, sizeof(CONFIG));
}

/* Tabs */
void tabs_enable_items(HWND hDlg, CONFIG *config)
{
    ShowWindow(GetDlgItem(hTabs[0], IDC_BITRATELABEL), config->i_encoding_type > 0);
    ShowWindow(GetDlgItem(hTabs[0], IDC_BITRATEEDIT), config->i_encoding_type > 0);
    ShowWindow(GetDlgItem(hTabs[0], IDC_BITRATESLIDER), config->i_encoding_type > 0);
    ShowWindow(GetDlgItem(hTabs[0], IDC_BITRATELOW), config->i_encoding_type > 0);
    ShowWindow(GetDlgItem(hTabs[0], IDC_BITRATEHIGH), config->i_encoding_type > 0);

    EnableWindow(GetDlgItem(hTabs[0], IDC_UPDATESTATS), config->i_encoding_type == 4 && config->i_pass > 1);
    EnableWindow(GetDlgItem(hTabs[0], IDC_STATSFILE), config->i_encoding_type == 4);
    EnableWindow(GetDlgItem(hTabs[0], IDC_STATSFILE_BROWSE), config->i_encoding_type == 4);

//    EnableWindow(GetDlgItem(hTabs[0], IDC_EXTRA_CMDLINE), TRUE);

    EnableWindow(GetDlgItem(hTabs[1], IDC_IPRATIO), config->i_encoding_type > 0);
    EnableWindow(GetDlgItem(hTabs[1], IDC_PBRATIO), config->i_encoding_type > 0 && config->i_bframe > 0);
    EnableWindow(GetDlgItem(hTabs[1], IDC_CURVECOMP), config->i_encoding_type > 1);

    EnableWindow(GetDlgItem(hTabs[1], IDC_QPMIN), config->i_encoding_type > 1);
    EnableWindow(GetDlgItem(hTabs[1], IDC_QPMAX), config->i_encoding_type > 1);
    EnableWindow(GetDlgItem(hTabs[1], IDC_QPSTEP), config->i_encoding_type > 1);

//    EnableWindow(GetDlgItem(hTabs[1], IDC_SCENECUT), TRUE);
//    EnableWindow(GetDlgItem(hTabs[1], IDC_KEYINTMIN), TRUE);
//    EnableWindow(GetDlgItem(hTabs[1], IDC_KEYINTMAX), TRUE);

    EnableWindow(GetDlgItem(hTabs[2], IDC_DCT8X8), config->i_encoding_type > 0);
    EnableWindow(GetDlgItem(hTabs[2], IDC_I8X8), config->i_encoding_type > 0 && config->b_dct8x8);
//    EnableWindow(GetDlgItem(hTabs[2], IDC_I4X4), TRUE);
//    EnableWindow(GetDlgItem(hTabs[2], IDC_P8X8), TRUE);
    EnableWindow(GetDlgItem(hTabs[2], IDC_P4X4), config->b_psub16x16);
    EnableWindow(GetDlgItem(hTabs[2], IDC_B8X8), config->i_bframe > 0);

//    EnableWindow(GetDlgItem(hTabs[2], IDC_BFRAME), TRUE);

#ifdef VIRTUALDUB_HACK
//    EnableWindow(GetDlgItem(hTabs[2], IDC_VDHACK), TRUE);
#else
    ShowWindow(GetDlgItem(hTabs[2], IDC_VDHACK), FALSE);
#endif

    EnableWindow(GetDlgItem(hTabs[2], IDC_BBIASSLIDER), config->i_bframe > 0 && config->b_bframe_adaptive);
    EnableWindow(GetDlgItem(hTabs[2], IDC_BBIAS), config->i_bframe > 0 && config->b_bframe_adaptive);
    EnableWindow(GetDlgItem(hTabs[2], IDC_BADAPT), config->i_bframe > 0);
    EnableWindow(GetDlgItem(hTabs[2], IDC_BREFS), config->i_bframe > 1);
    EnableWindow(GetDlgItem(hTabs[2], IDC_WBPRED), config->i_bframe > 0);
    EnableWindow(GetDlgItem(hTabs[2], IDC_BIDIR_ME), config->i_bframe > 0);
    EnableWindow(GetDlgItem(hTabs[2], IDC_DIRECTPRED), config->i_bframe > 0);

//    EnableWindow(GetDlgItem(hTabs[3], IDC_SUBPEL), TRUE);
    EnableWindow(GetDlgItem(hTabs[3], IDC_BRDO), config->i_bframe > 0 && config->i_subpel_refine >= 5);
//    EnableWindow(GetDlgItem(hTabs[3], IDC_ME_METHOD), TRUE);
//    EnableWindow(GetDlgItem(hTabs[3], IDC_MERANGE), TRUE);
    EnableWindow(GetDlgItem(hTabs[3], IDC_CHROMAME), config->i_subpel_refine >= 4);
//    EnableWindow(GetDlgItem(hTabs[3], IDC_FRAMEREFS), TRUE);
    EnableWindow(GetDlgItem(hTabs[3], IDC_MIXEDREF), config->i_refmax > 1);

//    EnableWindow(GetDlgItem(hTabs[3], IDC_LOG), TRUE);
//    EnableWindow(GetDlgItem(hTabs[3], IDC_FOURCC), TRUE);

//    EnableWindow(GetDlgItem(hTabs[3], IDC_SAR_W), TRUE);
//    EnableWindow(GetDlgItem(hTabs[3], IDC_SAR_H), TRUE);

#ifdef HAVE_PTHREAD
//    EnableWindow(GetDlgItem(hTabs[3], IDC_THREADEDIT), TRUE);
#else
    ShowWindow(GetDlgItem(hTabs[3], IDC_THREADLABEL), FALSE);
    ShowWindow(GetDlgItem(hTabs[3], IDC_THREADEDIT), FALSE);
#endif

//    EnableWindow(GetDlgItem(hTabs[3], IDC_CABAC), TRUE);
    EnableWindow(GetDlgItem(hTabs[3], IDC_TRELLIS), config->i_encoding_type > 0 && config->b_cabac);
    EnableWindow(GetDlgItem(hTabs[3], IDC_NR), config->i_encoding_type > 0);
    EnableWindow(GetDlgItem(hTabs[3], IDC_LOOPFILTER), config->i_encoding_type > 0);
    EnableWindow(GetDlgItem(hTabs[3], IDC_INLOOP_A), config->i_encoding_type > 0 && config->b_filter);
    EnableWindow(GetDlgItem(hTabs[3], IDC_INLOOP_B), config->i_encoding_type > 0 && config->b_filter);
//    EnableWindow(GetDlgItem(hTabs[3], IDC_INTERLACED), TRUE);

//    EnableWindow(GetDlgItem(hDlg, IDC_USE_CMDLINE), TRUE);
    EnableWindow(GetDlgItem(hDlg, IDC_CMDLINE), config->b_use_cmdline);
}

void tabs_update_items(HWND hDlg, CONFIG *config)
{
    char szTmp[1024];

    sprintf(szTmp, "Build date: %s %s\nlibx264 core %d%s", __DATE__, __TIME__, X264_BUILD, X264_VERSION);
    SetDlgItemText(hTabs[0], IDC_BUILDREV, szTmp);

    if (SendMessage(GetDlgItem(hTabs[0], IDC_BITRATEMODE), CB_GETCOUNT, 0, 0) == 0)
    {
        SendDlgItemMessage(hTabs[0], IDC_BITRATEMODE, CB_ADDSTRING, 0, (LPARAM)"Single pass - lossless");
        SendDlgItemMessage(hTabs[0], IDC_BITRATEMODE, CB_ADDSTRING, 0, (LPARAM)"Single pass - quantizer-based (CQP)");
        SendDlgItemMessage(hTabs[0], IDC_BITRATEMODE, CB_ADDSTRING, 0, (LPARAM)"Single pass - ratefactor-based (CRF)");
        SendDlgItemMessage(hTabs[0], IDC_BITRATEMODE, CB_ADDSTRING, 0, (LPARAM)"Single pass - bitrate-based (ABR)");
        SendDlgItemMessage(hTabs[0], IDC_BITRATEMODE, CB_ADDSTRING, 0, (LPARAM)"Multipass - 1st pass");
        SendDlgItemMessage(hTabs[0], IDC_BITRATEMODE, CB_ADDSTRING, 0, (LPARAM)"Multipass - 1st pass (fast)");
        SendDlgItemMessage(hTabs[0], IDC_BITRATEMODE, CB_ADDSTRING, 0, (LPARAM)"Multipass - Nth pass");
    }
    SendMessage(GetDlgItem(hTabs[0], IDC_BITRATEEDIT), EM_LIMITTEXT, 4, 0);
    switch (config->i_encoding_type)
    {
        case 0: /* 1 pass, lossless */
            SendDlgItemMessage(hTabs[0], IDC_BITRATEMODE, CB_SETCURSEL, 0, 0);
            SetDlgItemText(hTabs[0], IDC_BITRATELABEL, "");
            SetDlgItemText(hTabs[0], IDC_BITRATELOW, "");
            SetDlgItemText(hTabs[0], IDC_BITRATEHIGH, "");
            SetDlgItemInt(hTabs[0], IDC_BITRATEEDIT, 0, FALSE);
            SendDlgItemMessage(hTabs[0], IDC_BITRATESLIDER, TBM_SETRANGE, TRUE, (LPARAM)MAKELONG(0, 0));
            SendDlgItemMessage(hTabs[0], IDC_BITRATESLIDER, TBM_SETPOS, TRUE, 0);
            break;

        case 1: /* 1 pass, quantizer-based (CQP) */
            SendDlgItemMessage(hTabs[0], IDC_BITRATEMODE, CB_SETCURSEL, 1, 0);
            SetDlgItemText(hTabs[0], IDC_BITRATELABEL, "Quantizer");
            SetDlgItemText(hTabs[0], IDC_BITRATELOW, "1 (High quality)");
            sprintf(szTmp, "(Low quality) %d", X264_QUANT_MAX);
            SetDlgItemText(hTabs[0], IDC_BITRATEHIGH, szTmp);
            SetDlgItemInt(hTabs[0], IDC_BITRATEEDIT, config->i_qp, FALSE);
            SendDlgItemMessage(hTabs[0], IDC_BITRATESLIDER, TBM_SETRANGE, TRUE, (LPARAM)MAKELONG(1, X264_QUANT_MAX));
            SendDlgItemMessage(hTabs[0], IDC_BITRATESLIDER, TBM_SETPOS, TRUE, config->i_qp);
            break;

        case 2: /* 1 pass, ratefactor-based (CRF) */
            SendDlgItemMessage(hTabs[0], IDC_BITRATEMODE, CB_SETCURSEL, 2, 0);
            SetDlgItemText(hTabs[0], IDC_BITRATELABEL, "Ratefactor");
            SetDlgItemText(hTabs[0], IDC_BITRATELOW, "1.0 (High quality)");
            sprintf(szTmp, "(Low quality) %d.0", X264_QUANT_MAX);
            SetDlgItemText(hTabs[0], IDC_BITRATEHIGH, szTmp);
            SetDlgItemDouble(hTabs[0], IDC_BITRATEEDIT, config->i_rf_constant * 0.1);
            SendDlgItemMessage(hTabs[0], IDC_BITRATESLIDER, TBM_SETRANGE, TRUE, (LPARAM)MAKELONG(10, X264_QUANT_MAX * 10));
            SendDlgItemMessage(hTabs[0], IDC_BITRATESLIDER, TBM_SETPOS, TRUE, config->i_rf_constant);
            break;

        case 3: /* 1 pass, bitrate-based (ABR) */
            SendDlgItemMessage(hTabs[0], IDC_BITRATEMODE, CB_SETCURSEL, 3, 0);
            SetDlgItemText(hTabs[0], IDC_BITRATELABEL, "Average bitrate (kbit/s)");
            SetDlgItemText(hTabs[0], IDC_BITRATELOW, "1");
            sprintf(szTmp, "%d", X264_BITRATE_MAX);
            SetDlgItemText(hTabs[0], IDC_BITRATEHIGH, szTmp);
            SetDlgItemInt(hTabs[0], IDC_BITRATEEDIT, config->i_passbitrate, FALSE);
            SendDlgItemMessage(hTabs[0], IDC_BITRATESLIDER, TBM_SETRANGE, TRUE, (LPARAM)MAKELONG(1, X264_BITRATE_MAX));
            SendDlgItemMessage(hTabs[0], IDC_BITRATESLIDER, TBM_SETPOS, TRUE, config->i_passbitrate);
            break;

        case 4: /* 2 pass */
            if (config->i_pass > 1)
                SendDlgItemMessage(hTabs[0], IDC_BITRATEMODE, CB_SETCURSEL, 6, 0);
            else if (config->b_fast1pass)
                SendDlgItemMessage(hTabs[0], IDC_BITRATEMODE, CB_SETCURSEL, 5, 0);
            else
                SendDlgItemMessage(hTabs[0], IDC_BITRATEMODE, CB_SETCURSEL, 4, 0);
            SetDlgItemText(hTabs[0], IDC_BITRATELABEL, "Target bitrate (kbit/s)");
            SetDlgItemText(hTabs[0], IDC_BITRATELOW, "1");
            sprintf(szTmp, "%d", X264_BITRATE_MAX);
            SetDlgItemText(hTabs[0], IDC_BITRATEHIGH, szTmp);
            SetDlgItemInt(hTabs[0], IDC_BITRATEEDIT, config->i_passbitrate, FALSE);
            SendDlgItemMessage(hTabs[0], IDC_BITRATESLIDER, TBM_SETRANGE, TRUE, (LPARAM)MAKELONG(1, X264_BITRATE_MAX));
            SendDlgItemMessage(hTabs[0], IDC_BITRATESLIDER, TBM_SETPOS, TRUE, config->i_passbitrate);
            break;

        default:
            assert(0);
            break;
    }
    CheckDlgButton(hTabs[0], IDC_UPDATESTATS, config->b_updatestats);
    SendMessage(GetDlgItem(hTabs[0], IDC_STATSFILE), EM_LIMITTEXT, MAX_STATS_PATH - 1, 0);
    SetDlgItemText(hTabs[0], IDC_STATSFILE, config->stats);

    SendMessage(GetDlgItem(hTabs[0], IDC_EXTRA_CMDLINE), EM_LIMITTEXT, MAX_CMDLINE - 1, 0);
    SetDlgItemText(hTabs[0], IDC_EXTRA_CMDLINE, config->extra_cmdline);

    SendMessage(GetDlgItem(hTabs[1], IDC_IPRATIO), EM_LIMITTEXT, 4, 0);
    SetDlgItemInt(hTabs[1], IDC_IPRATIO, config->i_key_boost, FALSE);
    SendMessage(GetDlgItem(hTabs[1], IDC_PBRATIO), EM_LIMITTEXT, 4, 0);
    SetDlgItemInt(hTabs[1], IDC_PBRATIO, config->i_b_red, FALSE);
    SendMessage(GetDlgItem(hTabs[1], IDC_CURVECOMP), EM_LIMITTEXT, 4, 0);
    SetDlgItemInt(hTabs[1], IDC_CURVECOMP, config->i_curve_comp, FALSE);

    SendMessage(GetDlgItem(hTabs[1], IDC_QPMIN), EM_LIMITTEXT, 4, 0);
    SetDlgItemInt(hTabs[1], IDC_QPMIN, config->i_qp_min, FALSE);
    SendMessage(GetDlgItem(hTabs[1], IDC_QPMAX), EM_LIMITTEXT, 4, 0);
    SetDlgItemInt(hTabs[1], IDC_QPMAX, config->i_qp_max, FALSE);
    SendMessage(GetDlgItem(hTabs[1], IDC_QPSTEP), EM_LIMITTEXT, 4, 0);
    SetDlgItemInt(hTabs[1], IDC_QPSTEP, config->i_qp_step, FALSE);

    SendMessage(GetDlgItem(hTabs[1], IDC_SCENECUT), EM_LIMITTEXT, 4, 0);
    SetDlgItemInt(hTabs[1], IDC_SCENECUT, config->i_scenecut_threshold, TRUE);
    SendMessage(GetDlgItem(hTabs[1], IDC_KEYINTMIN), EM_LIMITTEXT, 4, 0);
    SetDlgItemInt(hTabs[1], IDC_KEYINTMIN, config->i_keyint_min, FALSE);
    SendMessage(GetDlgItem(hTabs[1], IDC_KEYINTMAX), EM_LIMITTEXT, 4, 0);
    SetDlgItemInt(hTabs[1], IDC_KEYINTMAX, config->i_keyint_max, FALSE);

    CheckDlgButton(hTabs[2], IDC_DCT8X8, config->b_dct8x8);
    CheckDlgButton(hTabs[2], IDC_I8X8, config->b_i8x8);
    CheckDlgButton(hTabs[2], IDC_I4X4, config->b_i4x4);
    CheckDlgButton(hTabs[2], IDC_P8X8, config->b_psub16x16);
    CheckDlgButton(hTabs[2], IDC_P4X4, config->b_psub8x8);
    CheckDlgButton(hTabs[2], IDC_B8X8, config->b_bsub16x16);

    SendMessage(GetDlgItem(hTabs[2], IDC_BFRAME), EM_LIMITTEXT, 4, 0);
    SetDlgItemInt(hTabs[2], IDC_BFRAME, config->i_bframe, FALSE);
    CheckDlgButton(hTabs[2], IDC_VDHACK, config->b_vd_hack);
    SendDlgItemMessage(hTabs[2], IDC_BBIASSLIDER, TBM_SETRANGE, TRUE, (LPARAM)MAKELONG(-90, 100));
    SendDlgItemMessage(hTabs[2], IDC_BBIASSLIDER, TBM_SETPOS, TRUE, config->i_bframe_bias);
    SendMessage(GetDlgItem(hTabs[2], IDC_BBIAS), EM_LIMITTEXT, 4, 0);
    SetDlgItemInt(hTabs[2], IDC_BBIAS, config->i_bframe_bias, TRUE);
    CheckDlgButton(hTabs[2], IDC_BADAPT, config->b_bframe_adaptive);
    CheckDlgButton(hTabs[2], IDC_BREFS, config->b_b_refs);
    CheckDlgButton(hTabs[2], IDC_WBPRED, config->b_b_wpred);
    CheckDlgButton(hTabs[2], IDC_BIDIR_ME, config->b_bidir_me);
    if (SendMessage(GetDlgItem(hTabs[2], IDC_DIRECTPRED), CB_GETCOUNT, 0, 0) == 0)
    {
        SendDlgItemMessage(hTabs[2], IDC_DIRECTPRED, CB_ADDSTRING, 0, (LPARAM)"None");
        SendDlgItemMessage(hTabs[2], IDC_DIRECTPRED, CB_ADDSTRING, 0, (LPARAM)"Spatial");
        SendDlgItemMessage(hTabs[2], IDC_DIRECTPRED, CB_ADDSTRING, 0, (LPARAM)"Temporal");
        SendDlgItemMessage(hTabs[2], IDC_DIRECTPRED, CB_ADDSTRING, 0, (LPARAM)"Auto");
    }
    SendDlgItemMessage(hTabs[2], IDC_DIRECTPRED, CB_SETCURSEL, (config->i_direct_mv_pred), 0);

    if (SendMessage(GetDlgItem(hTabs[3], IDC_SUBPEL), CB_GETCOUNT, 0, 0) == 0)
    {
        SendDlgItemMessage(hTabs[3], IDC_SUBPEL, CB_ADDSTRING, 0, (LPARAM)"1 (Fast)");
        SendDlgItemMessage(hTabs[3], IDC_SUBPEL, CB_ADDSTRING, 0, (LPARAM)"2");
        SendDlgItemMessage(hTabs[3], IDC_SUBPEL, CB_ADDSTRING, 0, (LPARAM)"3");
        SendDlgItemMessage(hTabs[3], IDC_SUBPEL, CB_ADDSTRING, 0, (LPARAM)"4");
        SendDlgItemMessage(hTabs[3], IDC_SUBPEL, CB_ADDSTRING, 0, (LPARAM)"5 (Normal)");
        SendDlgItemMessage(hTabs[3], IDC_SUBPEL, CB_ADDSTRING, 0, (LPARAM)"6 (RDO)");
        SendDlgItemMessage(hTabs[3], IDC_SUBPEL, CB_ADDSTRING, 0, (LPARAM)"7 (Best)");
    }
    SendDlgItemMessage(hTabs[3], IDC_SUBPEL, CB_SETCURSEL, (config->i_subpel_refine), 0);
    CheckDlgButton(hTabs[3], IDC_BRDO, config->b_brdo);
    if (SendMessage(GetDlgItem(hTabs[3], IDC_ME_METHOD), CB_GETCOUNT, 0, 0) == 0)
    {
        SendDlgItemMessage(hTabs[3], IDC_ME_METHOD, CB_ADDSTRING, 0, (LPARAM)"dia");
        SendDlgItemMessage(hTabs[3], IDC_ME_METHOD, CB_ADDSTRING, 0, (LPARAM)"hex");
        SendDlgItemMessage(hTabs[3], IDC_ME_METHOD, CB_ADDSTRING, 0, (LPARAM)"umh");
        SendDlgItemMessage(hTabs[3], IDC_ME_METHOD, CB_ADDSTRING, 0, (LPARAM)"esa");
        SendDlgItemMessage(hTabs[3], IDC_ME_METHOD, CB_ADDSTRING, 0, (LPARAM)"tesa");
    }
    SendDlgItemMessage(hTabs[3], IDC_ME_METHOD, CB_SETCURSEL, (config->i_me_method), 0);
    SendMessage(GetDlgItem(hTabs[3], IDC_MERANGE), EM_LIMITTEXT, 4, 0);
    SetDlgItemInt(hTabs[3], IDC_MERANGE, config->i_me_range, FALSE);
    CheckDlgButton(hTabs[3], IDC_CHROMAME, config->b_chroma_me);
    SendMessage(GetDlgItem(hTabs[3], IDC_FRAMEREFS), EM_LIMITTEXT, 4, 0);
    SetDlgItemInt(hTabs[3], IDC_FRAMEREFS, config->i_refmax, FALSE);
    CheckDlgButton(hTabs[3], IDC_MIXEDREF, config->b_mixedref);

    if (SendMessage(GetDlgItem(hTabs[3], IDC_LOG), CB_GETCOUNT, 0, 0) == 0)
    {
        SendDlgItemMessage(hTabs[3], IDC_LOG, CB_ADDSTRING, 0, (LPARAM)"None");
        SendDlgItemMessage(hTabs[3], IDC_LOG, CB_ADDSTRING, 0, (LPARAM)"Error");
        SendDlgItemMessage(hTabs[3], IDC_LOG, CB_ADDSTRING, 0, (LPARAM)"Warning");
        SendDlgItemMessage(hTabs[3], IDC_LOG, CB_ADDSTRING, 0, (LPARAM)"Info");
        SendDlgItemMessage(hTabs[3], IDC_LOG, CB_ADDSTRING, 0, (LPARAM)"Debug");
    }
    SendDlgItemMessage(hTabs[3], IDC_LOG, CB_SETCURSEL, (config->i_log_level), 0);
    if (SendMessage(GetDlgItem(hTabs[3], IDC_FOURCC), CB_GETCOUNT, 0, 0) == 0)
    {
        int i;
        for (i = 0; i < sizeof(fcc_str_table) / sizeof(fourcc_str); i++)
            SendDlgItemMessage(hTabs[3], IDC_FOURCC, CB_ADDSTRING, 0, (LPARAM)&fcc_str_table[i]);
    }
    SendDlgItemMessage(hTabs[3], IDC_FOURCC, CB_SETCURSEL, (config->i_fcc_num), 0);

    SendMessage(GetDlgItem(hTabs[3], IDC_SAR_W), EM_LIMITTEXT, 4, 0);
    SetDlgItemInt(hTabs[3], IDC_SAR_W, config->i_sar_width, FALSE);
    SendMessage(GetDlgItem(hTabs[3], IDC_SAR_H), EM_LIMITTEXT, 4, 0);
    SetDlgItemInt(hTabs[3], IDC_SAR_H, config->i_sar_height, FALSE);
    SendMessage(GetDlgItem(hTabs[3], IDC_THREADEDIT), EM_LIMITTEXT, 4, 0);
    SetDlgItemInt(hTabs[3], IDC_THREADEDIT, config->i_threads, FALSE);

    CheckDlgButton(hTabs[3], IDC_CABAC, config->b_cabac);
    CheckDlgButton(hTabs[3], IDC_TRELLIS, config->i_trellis);
    SendMessage(GetDlgItem(hTabs[3], IDC_NR), EM_LIMITTEXT, 4, 0);
    SetDlgItemInt(hTabs[3], IDC_NR, config->i_noise_reduction, FALSE);
    CheckDlgButton(hTabs[3], IDC_LOOPFILTER, config->b_filter);
    SendDlgItemMessage(hTabs[3], IDC_INLOOP_A, TBM_SETRANGE, TRUE, (LPARAM)MAKELONG(-6, 6));
    SendDlgItemMessage(hTabs[3], IDC_INLOOP_A, TBM_SETPOS, TRUE, config->i_inloop_a);
    SetDlgItemInt(hTabs[3], IDC_LOOPA_TXT, config->i_inloop_a, TRUE);
    SendDlgItemMessage(hTabs[3], IDC_INLOOP_B, TBM_SETRANGE, TRUE, (LPARAM)MAKELONG(-6, 6));
    SendDlgItemMessage(hTabs[3], IDC_INLOOP_B, TBM_SETPOS, TRUE, config->i_inloop_b);
    SetDlgItemInt(hTabs[3], IDC_LOOPB_TXT, config->i_inloop_b, TRUE);
    CheckDlgButton(hTabs[3], IDC_INTERLACED, config->b_interlaced);

    CheckDlgButton(hDlg, IDC_USE_CMDLINE, config->b_use_cmdline);
    SendMessage(GetDlgItem(hDlg, IDC_CMDLINE), EM_LIMITTEXT, MAX_CMDLINE - 1, 0);
    SetDlgItemText(hDlg, IDC_CMDLINE, config->cmdline);
}

/* Assigns tooltips */
BOOL CALLBACK enum_tooltips(HWND hWnd, LPARAM lParam)
{
    char help[500];

    /* The tooltip for a control is named the same as the control itself */
    if (LoadString(g_hInst, GetDlgCtrlID(hWnd), help, 500))
    {
        TOOLINFO ti;

        ti.cbSize = sizeof(TOOLINFO);
        ti.uFlags = TTF_SUBCLASS | TTF_IDISHWND;
        ti.hwnd = GetParent(hWnd);
        ti.uId = (LPARAM)hWnd;
        ti.lpszText = help;

        SendMessage(hTooltip, TTM_ADDTOOL, 0, (LPARAM)&ti);
    }

    return TRUE;
}

/* Main window */
BOOL CALLBACK callback_main(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CONFIG *config = (CONFIG *)GetWindowLong(hDlg, GWL_USERDATA);

    switch (uMsg)
    {
        case WM_INITDIALOG:
        {
            TCITEM tie;
            RECT rect;
            HWND hTabCtrl = GetDlgItem(hDlg, IDC_TABCONTROL);

            SetWindowLong(hDlg, GWL_USERDATA, lParam);
            config = (CONFIG *)lParam;

            b_tabs_updated = FALSE;

            // insert tabs in tab control
            tie.mask = TCIF_TEXT;
            tie.iImage = -1;
            tie.pszText = "Bitrate";         TabCtrl_InsertItem(hTabCtrl, 0, &tie);
            tie.pszText = "Rate Control";    TabCtrl_InsertItem(hTabCtrl, 1, &tie);
            tie.pszText = "MBs && Frames";   TabCtrl_InsertItem(hTabCtrl, 2, &tie);
            tie.pszText = "More...";         TabCtrl_InsertItem(hTabCtrl, 3, &tie);
            hTabs[0] = CreateDialogParam(g_hInst, MAKEINTRESOURCE(IDD_TAB_BITRATE),     hDlg, (DLGPROC)callback_tabs, lParam);
            hTabs[1] = CreateDialogParam(g_hInst, MAKEINTRESOURCE(IDD_TAB_RATECONTROL), hDlg, (DLGPROC)callback_tabs, lParam);
            hTabs[2] = CreateDialogParam(g_hInst, MAKEINTRESOURCE(IDD_TAB_IPFRAMES),    hDlg, (DLGPROC)callback_tabs, lParam);
            hTabs[3] = CreateDialogParam(g_hInst, MAKEINTRESOURCE(IDD_TAB_MISC),        hDlg, (DLGPROC)callback_tabs, lParam);
            GetClientRect(hTabCtrl, &rect);
            TabCtrl_AdjustRect(hTabCtrl, FALSE, &rect);
            MoveWindow(hTabs[0], rect.left, rect.top, rect.right - rect.left + 1, rect.bottom - rect.top + 1, TRUE);
            MoveWindow(hTabs[1], rect.left, rect.top, rect.right - rect.left + 1, rect.bottom - rect.top + 1, TRUE);
            MoveWindow(hTabs[2], rect.left, rect.top, rect.right - rect.left + 1, rect.bottom - rect.top + 1, TRUE);
            MoveWindow(hTabs[3], rect.left, rect.top, rect.right - rect.left + 1, rect.bottom - rect.top + 1, TRUE);

            if ((hTooltip = CreateWindow(TOOLTIPS_CLASS, NULL, TTS_ALWAYSTIP, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, NULL, NULL, g_hInst, NULL)))
            {
                SetWindowPos(hTooltip, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
                SendMessage(hTooltip, TTM_SETMAXTIPWIDTH, 0, 400);
                EnumChildWindows(hDlg, enum_tooltips, 0);
            }

            tabs_enable_items(hDlg, config);
            tabs_update_items(hDlg, config);
            b_tabs_updated = TRUE;
            ShowWindow(hTabs[0], SW_SHOW);
            BringWindowToTop(hTabs[0]);
            UpdateWindow(hDlg);
            break;
        }

        case WM_NOTIFY:
        {
            NMHDR FAR *tem = (NMHDR FAR *)lParam;

            if (tem->code == TCN_SELCHANGING)
            {
                int num;
                HWND hTabCtrl = GetDlgItem(hDlg, IDC_TABCONTROL);

                SetFocus(hTabCtrl);
                num = TabCtrl_GetCurSel(hTabCtrl);
                ShowWindow(hTabs[num], SW_HIDE);
                UpdateWindow(hDlg);
            }
            else if (tem->code == TCN_SELCHANGE)
            {
                int num;
                HWND hTabCtrl = GetDlgItem(hDlg, IDC_TABCONTROL);

                SetFocus(hTabCtrl);
                num = TabCtrl_GetCurSel(hTabCtrl);
                ShowWindow(hTabs[num], SW_SHOW);
                BringWindowToTop(hTabs[num]);
                UpdateWindow(hDlg);
            }
            else
                return FALSE;
            break;
        }

        case WM_COMMAND:
            switch (HIWORD(wParam))
            {
                case BN_CLICKED:
                    switch (LOWORD(wParam))
                    {
                        case IDOK:
                            config->b_save = TRUE;
                            EndDialog(hDlg, LOWORD(wParam));
                            break;

                        case IDCANCEL:
                            config->b_save = FALSE;
                            EndDialog(hDlg, LOWORD(wParam));
                            break;

                        case IDC_DEFAULTS:
                            if (MessageBox(hDlg, X264_DEF_TEXT, X264_NAME, MB_YESNO) == IDYES)
                            {
                                config_defaults(config);
                                b_tabs_updated = FALSE;
                                tabs_enable_items(hDlg, config);
                                tabs_update_items(hDlg, config);
                                b_tabs_updated = TRUE;
                            }
                            break;

                        case IDC_USE_CMDLINE:
                            config->b_use_cmdline = IsDlgButtonChecked(hDlg, IDC_USE_CMDLINE);
                            EnableWindow(GetDlgItem(hDlg, IDC_CMDLINE), config->b_use_cmdline);
                            break;

                        default:
                            return FALSE;
                    }
                    break;

                case EN_CHANGE:
                    switch (LOWORD(wParam))
                    {
                        case IDC_CMDLINE:
                            if (GetDlgItemText(hDlg, IDC_CMDLINE, config->cmdline, MAX_CMDLINE) == 0)
                                strcpy(config->cmdline, "");
                            break;

                        default:
                            return FALSE;
                    }
                    break;

                case EN_KILLFOCUS:
                    switch (LOWORD(wParam))
                    {
                        case IDC_CMDLINE:
                            if (GetDlgItemText(hDlg, IDC_CMDLINE, config->cmdline, MAX_CMDLINE) == 0)
                                strcpy(config->cmdline, "");
                            SetDlgItemText(hDlg, IDC_CMDLINE, config->cmdline);
                            break;

                        default:
                            return FALSE;
                    }
                    break;

                default:
                    return FALSE;
            }
            break;

        default:
            return FALSE;
    }

    return TRUE;
}

void CheckControlTextIsNumber(HWND hDlgItem, BOOL bSigned, int iDecimalPlacesNum)
{
    char text_old[MAX_PATH];
    char text_new[MAX_PATH];
    char *src, *dest;
    DWORD start, end, num, pos;
    BOOL bChanged = FALSE;
    BOOL bCopy = FALSE;
    int q = !bSigned;

    SendMessage(hDlgItem, EM_GETSEL, (WPARAM)&start, (LPARAM)&end);
    num = SendMessage(hDlgItem, WM_GETTEXT, MAX_PATH, (LPARAM)&text_old);
    src = &text_old[0];
    dest = &text_new[0];
    pos = 0;
    while (num > 0)
    {
        bCopy = TRUE;
        if (q == 0 && *src == '-')
        {
            q = 1;
        }
        else if ((q == 0 || q == 1) && *src >= '0' && *src <= '9')
        {
            q = 2;
        }
        else if (q == 2 && *src >= '0' && *src <= '9')
        {
        }
        else if (q == 2 && iDecimalPlacesNum > 0 && *src == '.')
        {
            q = 3;
        }
        else if (q == 3 && iDecimalPlacesNum > 0 && *src >= '0' && *src <= '9')
        {
            iDecimalPlacesNum--;
        }
        else
            bCopy = FALSE;
        if (bCopy)
        {
            *dest = *src;
            dest++;
            pos++;
        }
        else
        {
            bChanged = TRUE;
            if (pos < start)
                start--;
            if (pos < end)
                end--;
        }
        src++;
        num--;
    }
    *dest = 0;
    if (bChanged)
    {
        SendMessage(hDlgItem, WM_SETTEXT, 0, (LPARAM)&text_new);
        SendMessage(hDlgItem, EM_SETSEL, start, end);
    }
}

#define CHECKED_SET_INT(var, hDlg, nIDDlgItem, bSigned, min, max)\
{\
    var = GetDlgItemInt(hDlg, nIDDlgItem, NULL, bSigned);\
    if (var < min)\
    {\
        var = min;\
        SetDlgItemInt(hDlg, nIDDlgItem, var, bSigned);\
        SendMessage(GetDlgItem(hDlg, nIDDlgItem), EM_SETSEL, -2, -2);\
    }\
    else if (var > max)\
    {\
        var = max;\
        SetDlgItemInt(hDlg, nIDDlgItem, var, bSigned);\
        SendMessage(GetDlgItem(hDlg, nIDDlgItem), EM_SETSEL, -2, -2);\
    }\
}

#define CHECKED_SET_MIN_INT(var, hDlg, nIDDlgItem, bSigned, min, max)\
{\
    var = GetDlgItemInt(hDlg, nIDDlgItem, NULL, bSigned);\
    if (var < min)\
    {\
        var = min;\
        SetDlgItemInt(hDlg, nIDDlgItem, var, bSigned);\
        SendMessage(GetDlgItem(hDlg, nIDDlgItem), EM_SETSEL, -2, -2);\
    }\
    else if (var > max)\
        var = max;\
}

#define CHECKED_SET_MAX_INT(var, hDlg, nIDDlgItem, bSigned, min, max)\
{\
    var = GetDlgItemInt(hDlg, nIDDlgItem, NULL, bSigned);\
    if (var < min)\
        var = min;\
    else if (var > max)\
    {\
        var = max;\
        SetDlgItemInt(hDlg, nIDDlgItem, var, bSigned);\
        SendMessage(GetDlgItem(hDlg, nIDDlgItem), EM_SETSEL, -2, -2);\
    }\
}

#define CHECKED_SET_SHOW_INT(var, hDlg, nIDDlgItem, bSigned, min, max)\
{\
    var = GetDlgItemInt(hDlg, nIDDlgItem, NULL, bSigned);\
    if (var < min)\
        var = min;\
    else if (var > max)\
        var = max;\
    SetDlgItemInt(hDlg, nIDDlgItem, var, bSigned);\
}

BOOL CALLBACK callback_tabs(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CONFIG *config = (CONFIG *)GetWindowLong(hDlg, GWL_USERDATA);

    if (uMsg == WM_INITDIALOG)
    {
        SetWindowLong(hDlg, GWL_USERDATA, lParam);
        config = (CONFIG *)lParam;
        return TRUE;
    }
    if (!b_tabs_updated)
        return FALSE;
    switch (uMsg)
    {
        case WM_COMMAND:
            switch (HIWORD(wParam))
            {
                case LBN_SELCHANGE:
                    switch (LOWORD(wParam))
                    {
                        case IDC_BITRATEMODE:
                            config->i_pass = 1;
                            config->b_fast1pass = FALSE;
                            switch (SendDlgItemMessage(hTabs[0], IDC_BITRATEMODE, CB_GETCURSEL, 0, 0))
                            {
                                case 0:
                                    config->i_encoding_type = 0;
                                    break;

                                case 1:
                                    config->i_encoding_type = 1;
                                    break;

                                case 2:
                                    config->i_encoding_type = 2;
                                    break;

                                case 3:
                                    config->i_encoding_type = 3;
                                    break;

                                case 4:
                                    config->i_encoding_type = 4;
                                    break;

                                case 5:
                                    config->i_encoding_type = 4;
                                    config->b_fast1pass = TRUE;
                                    break;

                                case 6:
                                    config->i_encoding_type = 4;
                                    config->i_pass = 2;
                                    break;

                                default:
                                    assert(0);
                                    break;
                            }
                            b_tabs_updated = FALSE;
                            tabs_update_items(hDlg, config);
                            b_tabs_updated = TRUE;
                            break;

                        case IDC_DIRECTPRED:
                            config->i_direct_mv_pred = SendDlgItemMessage(hTabs[2], IDC_DIRECTPRED, CB_GETCURSEL, 0, 0);
                            break;

                        case IDC_SUBPEL:
                            config->i_subpel_refine = SendDlgItemMessage(hTabs[3], IDC_SUBPEL, CB_GETCURSEL, 0, 0);
                            break;

                        case IDC_ME_METHOD:
                            config->i_me_method = SendDlgItemMessage(hTabs[3], IDC_ME_METHOD, CB_GETCURSEL, 0, 0);
                            break;

                        case IDC_LOG:
                            config->i_log_level = SendDlgItemMessage(hTabs[3], IDC_LOG, CB_GETCURSEL, 0, 0);
                            break;

                        case IDC_FOURCC:
                            config->i_fcc_num = SendDlgItemMessage(hTabs[3], IDC_FOURCC, CB_GETCURSEL, 0, 0);
                            if (config->i_fcc_num < 0)
                            {
                                config->i_fcc_num = 0;
                            }
                            else if (config->i_fcc_num > sizeof(fcc_str_table) / sizeof(fourcc_str) - 1)
                            {
                                config->i_fcc_num = sizeof(fcc_str_table) / sizeof(fourcc_str) - 1;
                            }
                            strcpy(config->fcc, fcc_str_table[config->i_fcc_num]);
                            break;

                        default:
                            return FALSE;
                    }
                    break;

                case BN_CLICKED:
                    switch (LOWORD(wParam))
                    {
                        case IDC_UPDATESTATS:
                            config->b_updatestats = IsDlgButtonChecked(hTabs[0], IDC_UPDATESTATS);
                            break;

                        case IDC_STATSFILE_BROWSE:
                            {
                                OPENFILENAME ofn;
                                char tmp[MAX_STATS_SIZE];

                                GetDlgItemText(hTabs[0], IDC_STATSFILE, tmp, MAX_STATS_SIZE);

                                memset(&ofn, 0, sizeof(OPENFILENAME));
                                ofn.lStructSize = sizeof(OPENFILENAME);

                                ofn.hwndOwner = hDlg;
                                ofn.lpstrFilter = "Statsfile (*.stats)\0*.stats\0All files (*.*)\0*.*\0\0";
                                ofn.lpstrFile = tmp;
                                ofn.nMaxFile = MAX_STATS_SIZE;
                                ofn.Flags = OFN_PATHMUSTEXIST;

                                if (config->i_pass <= 1)
                                    ofn.Flags |= OFN_OVERWRITEPROMPT;
                                else
                                    ofn.Flags |= OFN_FILEMUSTEXIST;

                                if ((config->i_pass <= 1 && GetSaveFileName(&ofn)) ||
                                    (config->i_pass > 1 && GetOpenFileName(&ofn)))
                                    SetDlgItemText(hTabs[0], IDC_STATSFILE, tmp);
                            }
                            break;

                        case IDC_DCT8X8:
                            config->b_dct8x8 = IsDlgButtonChecked(hTabs[2], IDC_DCT8X8);
                            break;

                        case IDC_I8X8:
                            config->b_i8x8 = IsDlgButtonChecked(hTabs[2], IDC_I8X8);
                            break;

                        case IDC_I4X4:
                            config->b_i4x4 = IsDlgButtonChecked(hTabs[2], IDC_I4X4);
                            break;

                        case IDC_P8X8:
                            config->b_psub16x16 = IsDlgButtonChecked(hTabs[2], IDC_P8X8);
                            break;

                        case IDC_P4X4:
                            config->b_psub8x8 = IsDlgButtonChecked(hTabs[2], IDC_P4X4);
                            break;

                        case IDC_B8X8:
                            config->b_bsub16x16 = IsDlgButtonChecked(hTabs[2], IDC_B8X8);
                            break;

                        case IDC_VDHACK:
                            config->b_vd_hack = IsDlgButtonChecked(hTabs[2], IDC_VDHACK);
                            break;

                        case IDC_BADAPT:
                            config->b_bframe_adaptive = IsDlgButtonChecked(hTabs[2], IDC_BADAPT);
                            break;

                        case IDC_BREFS:
                            config->b_b_refs = IsDlgButtonChecked(hTabs[2], IDC_BREFS);
                            break;

                        case IDC_WBPRED:
                            config->b_b_wpred = IsDlgButtonChecked(hTabs[2], IDC_WBPRED);
                            break;

                        case IDC_BIDIR_ME:
                            config->b_bidir_me = IsDlgButtonChecked(hTabs[2], IDC_BIDIR_ME);
                            break;

                        case IDC_BRDO:
                            config->b_brdo = IsDlgButtonChecked(hTabs[3], IDC_BRDO);
                            break;

                        case IDC_CHROMAME:
                            config->b_chroma_me = IsDlgButtonChecked(hTabs[3], IDC_CHROMAME);
                            break;

                        case IDC_MIXEDREF:
                            config->b_mixedref = IsDlgButtonChecked(hTabs[3], IDC_MIXEDREF);
                            break;

                        case IDC_CABAC:
                            config->b_cabac = IsDlgButtonChecked(hTabs[3], IDC_CABAC);
                            break;

                        case IDC_TRELLIS:
                            config->i_trellis = IsDlgButtonChecked(hTabs[3], IDC_TRELLIS);
                            break;

                        case IDC_LOOPFILTER:
                            config->b_filter = IsDlgButtonChecked(hTabs[3], IDC_LOOPFILTER);
                            break;

                        case IDC_INTERLACED:
                            config->b_interlaced = IsDlgButtonChecked(hTabs[3], IDC_INTERLACED);
                            break;

                        default:
                            return FALSE;
                    }
                    break;

                case EN_CHANGE:
                    switch (LOWORD(wParam))
                    {
                        case IDC_BITRATEEDIT:
                            switch (config->i_encoding_type)
                            {
                                case 0:
                                    break;

                                case 1:
                                    CheckControlTextIsNumber(GetDlgItem(hTabs[0], IDC_BITRATEEDIT), FALSE, 0);
                                    CHECKED_SET_MAX_INT(config->i_qp, hTabs[0], IDC_BITRATEEDIT, FALSE, 1, X264_QUANT_MAX);
                                    SendDlgItemMessage(hTabs[0], IDC_BITRATESLIDER, TBM_SETPOS, TRUE, config->i_qp);
                                    break;

                                case 2:
                                    CheckControlTextIsNumber(GetDlgItem(hTabs[0], IDC_BITRATEEDIT), FALSE, 1);
                                    config->i_rf_constant = (int)(GetDlgItemDouble(hTabs[0], IDC_BITRATEEDIT) * 10);
                                    if (config->i_rf_constant < 10)
                                        config->i_rf_constant = 10;
                                    else if (config->i_rf_constant > X264_QUANT_MAX * 10)
                                    {
                                        config->i_rf_constant = X264_QUANT_MAX * 10;
                                        SetDlgItemDouble(hTabs[0], IDC_BITRATEEDIT, config->i_rf_constant * 0.1);
                                        SendMessage(GetDlgItem(hTabs[0], IDC_BITRATEEDIT), EM_SETSEL, -2, -2);
                                    }
                                    SendDlgItemMessage(hTabs[0], IDC_BITRATESLIDER, TBM_SETPOS, TRUE, config->i_rf_constant);
                                    break;

                                case 3:
                                case 4:
                                    CheckControlTextIsNumber(GetDlgItem(hTabs[0], IDC_BITRATEEDIT), FALSE, 0);
                                    CHECKED_SET_MAX_INT(config->i_passbitrate, hTabs[0], IDC_BITRATEEDIT, FALSE, 1, X264_BITRATE_MAX);
                                    SendDlgItemMessage(hTabs[0], IDC_BITRATESLIDER, TBM_SETPOS, TRUE, config->i_passbitrate);
                                    break;

                                default:
                                    assert(0);
                                    break;
                            }
                            break;

                        case IDC_STATSFILE:
                            if (GetDlgItemText(hTabs[0], IDC_STATSFILE, config->stats, MAX_STATS_PATH) == 0)
                                strcpy(config->stats, ".\\x264.stats");
                            break;

                        case IDC_EXTRA_CMDLINE:
                            if (GetDlgItemText(hTabs[0], IDC_EXTRA_CMDLINE, config->extra_cmdline, MAX_CMDLINE) == 0)
                                strcpy(config->extra_cmdline, "");
                            break;

                        case IDC_IPRATIO:
                            CHECKED_SET_MAX_INT(config->i_key_boost, hTabs[1], IDC_IPRATIO, FALSE, 0, 100);
                            break;

                        case IDC_PBRATIO:
                            CHECKED_SET_MAX_INT(config->i_b_red, hTabs[1], IDC_PBRATIO, FALSE, 0, 100);
                            break;

                        case IDC_CURVECOMP:
                            CHECKED_SET_MAX_INT(config->i_curve_comp, hTabs[1], IDC_CURVECOMP, FALSE, 0, 100);
                            break;

                        case IDC_QPMIN:
                            CHECKED_SET_MAX_INT(config->i_qp_min, hTabs[1], IDC_QPMIN, FALSE, 0, config->i_qp_max);
                            break;

                        case IDC_QPMAX:
                            CHECKED_SET_MAX_INT(config->i_qp_max, hTabs[1], IDC_QPMAX, FALSE, config->i_qp_min, X264_QUANT_MAX);
                            break;

                        case IDC_QPSTEP:
                            CHECKED_SET_MAX_INT(config->i_qp_step, hTabs[1], IDC_QPSTEP, FALSE, 1, X264_QUANT_MAX);
                            break;

                        case IDC_SCENECUT:
                            CheckControlTextIsNumber(GetDlgItem(hTabs[1], IDC_SCENECUT), TRUE, 0);
                            CHECKED_SET_INT(config->i_scenecut_threshold, hTabs[1], IDC_SCENECUT, TRUE, -1, 100);
                            break;

                        case IDC_KEYINTMIN:
                            CHECKED_SET_MAX_INT(config->i_keyint_min, hTabs[1], IDC_KEYINTMIN, FALSE, 1, config->i_keyint_max);
                            break;

                        case IDC_KEYINTMAX:
                            CHECKED_SET_MAX_INT(config->i_keyint_max, hTabs[1], IDC_KEYINTMAX, FALSE, config->i_keyint_min, 9999);
                            break;

                        case IDC_BFRAME:
                            CHECKED_SET_MAX_INT(config->i_bframe, hTabs[2], IDC_BFRAME, FALSE, 0, X264_BFRAME_MAX);
                            break;

                        case IDC_BBIAS:
                            CheckControlTextIsNumber(GetDlgItem(hTabs[2], IDC_BBIAS), TRUE, 0);
                            CHECKED_SET_INT(config->i_bframe_bias, hTabs[2], IDC_BBIAS, TRUE, -90, 100);
                            SendDlgItemMessage(hTabs[2], IDC_BBIASSLIDER, TBM_SETPOS, TRUE, config->i_bframe_bias);
                            break;

                        case IDC_MERANGE:
                            CHECKED_SET_MAX_INT(config->i_me_range, hTabs[3], IDC_MERANGE, FALSE, 4, 64);
                            break;

                        case IDC_FRAMEREFS:
                            CHECKED_SET_MAX_INT(config->i_refmax, hTabs[3], IDC_FRAMEREFS, FALSE, 1, 16);
                            break;

                        case IDC_SAR_W:
                            CHECKED_SET_MAX_INT(config->i_sar_width, hTabs[3], IDC_SAR_W, FALSE, 1, 9999);
                            break;

                        case IDC_SAR_H:
                            CHECKED_SET_MAX_INT(config->i_sar_height, hTabs[3], IDC_SAR_H, FALSE, 1, 9999);
                            break;

                        case IDC_THREADEDIT:
                            CHECKED_SET_MAX_INT(config->i_threads, hTabs[3], IDC_THREADEDIT, FALSE, 0, X264_THREAD_MAX);
                            break;

                        case IDC_NR:
                            CHECKED_SET_MAX_INT(config->i_noise_reduction, hTabs[3], IDC_NR, FALSE, 0, 9999);
                            break;

                        default:
                            return FALSE;
                    }
                    break;

                case EN_KILLFOCUS:
                    switch (LOWORD(wParam))
                    {
                        case IDC_BITRATEEDIT:
                            switch (config->i_encoding_type)
                            {
                                case 0:
                                    break;

                                case 1:
                                    CHECKED_SET_SHOW_INT(config->i_qp, hTabs[0], IDC_BITRATEEDIT, FALSE, 1, X264_QUANT_MAX);
                                    SendDlgItemMessage(hTabs[0], IDC_BITRATESLIDER, TBM_SETPOS, TRUE, config->i_qp);
                                    break;

                                case 2:
                                    config->i_rf_constant = (int)(GetDlgItemDouble(hTabs[0], IDC_BITRATEEDIT) * 10);
                                    if (config->i_rf_constant < 10)
                                        config->i_rf_constant = 10;
                                    else if (config->i_rf_constant > X264_QUANT_MAX * 10)
                                        config->i_rf_constant = X264_QUANT_MAX * 10;
                                    SetDlgItemDouble(hTabs[0], IDC_BITRATEEDIT, config->i_rf_constant * 0.1);
                                    SendDlgItemMessage(hTabs[0], IDC_BITRATESLIDER, TBM_SETPOS, TRUE, config->i_rf_constant);
                                    break;

                                case 3:
                                case 4:
                                    CHECKED_SET_SHOW_INT(config->i_passbitrate, hTabs[0], IDC_BITRATEEDIT, FALSE, 1, X264_BITRATE_MAX);
                                    SendDlgItemMessage(hTabs[0], IDC_BITRATESLIDER, TBM_SETPOS, TRUE, config->i_passbitrate);
                                    break;

                                default:
                                    assert(0);
                                    break;
                            }
                            break;

                        case IDC_STATSFILE:
                            if (GetDlgItemText(hTabs[0], IDC_STATSFILE, config->stats, MAX_STATS_PATH) == 0)
                                strcpy(config->stats, ".\\x264.stats");
                            SetDlgItemText(hTabs[0], IDC_STATSFILE, config->stats);
                            break;

                        case IDC_EXTRA_CMDLINE:
                            if (GetDlgItemText(hTabs[0], IDC_EXTRA_CMDLINE, config->extra_cmdline, MAX_CMDLINE) == 0)
                                strcpy(config->extra_cmdline, "");
                            SetDlgItemText(hTabs[0], IDC_EXTRA_CMDLINE, config->extra_cmdline);
                            break;

                        case IDC_IPRATIO:
                            CHECKED_SET_SHOW_INT(config->i_key_boost, hTabs[1], IDC_IPRATIO, FALSE, 0, 100);
                            break;

                        case IDC_PBRATIO:
                            CHECKED_SET_SHOW_INT(config->i_b_red, hTabs[1], IDC_PBRATIO, FALSE, 0, 100);
                            break;

                        case IDC_CURVECOMP:
                            CHECKED_SET_SHOW_INT(config->i_curve_comp, hTabs[1], IDC_CURVECOMP, FALSE, 0, 100);
                            break;

                        case IDC_QPMIN:
                            CHECKED_SET_SHOW_INT(config->i_qp_min, hTabs[1], IDC_QPMIN, FALSE, 0, config->i_qp_max);
                            break;

                        case IDC_QPMAX:
                            CHECKED_SET_SHOW_INT(config->i_qp_max, hTabs[1], IDC_QPMAX, FALSE, config->i_qp_min, X264_QUANT_MAX);
                            break;

                        case IDC_QPSTEP:
                            CHECKED_SET_SHOW_INT(config->i_qp_step, hTabs[1], IDC_QPSTEP, FALSE, 1, X264_QUANT_MAX);
                            break;

                        case IDC_SCENECUT:
                            CHECKED_SET_SHOW_INT(config->i_scenecut_threshold, hTabs[1], IDC_SCENECUT, TRUE, -1, 100);
                            break;

                        case IDC_KEYINTMIN:
                            CHECKED_SET_SHOW_INT(config->i_keyint_min, hTabs[1], IDC_KEYINTMIN, FALSE, 1, config->i_keyint_max);
                            break;

                        case IDC_KEYINTMAX:
                            CHECKED_SET_SHOW_INT(config->i_keyint_max, hTabs[1], IDC_KEYINTMAX, FALSE, config->i_keyint_min, 9999);
                            break;

                        case IDC_BFRAME:
                            CHECKED_SET_SHOW_INT(config->i_bframe, hTabs[2], IDC_BFRAME, FALSE, 0, X264_BFRAME_MAX);
                            break;

                        case IDC_BBIAS:
                            CHECKED_SET_SHOW_INT(config->i_bframe_bias, hTabs[2], IDC_BBIAS, TRUE, -90, 100);
                            SendDlgItemMessage(hTabs[2], IDC_BBIASSLIDER, TBM_SETPOS, TRUE, config->i_bframe_bias);
                            break;

                        case IDC_MERANGE:
                            CHECKED_SET_SHOW_INT(config->i_me_range, hTabs[3], IDC_MERANGE, FALSE, 4, 64);
                            break;

                        case IDC_FRAMEREFS:
                            CHECKED_SET_SHOW_INT(config->i_refmax, hTabs[3], IDC_FRAMEREFS, FALSE, 1, 16);
                            break;

                        case IDC_SAR_W:
                            CHECKED_SET_SHOW_INT(config->i_sar_width, hTabs[3], IDC_SAR_W, FALSE, 1, 9999);
                            break;

                        case IDC_SAR_H:
                            CHECKED_SET_SHOW_INT(config->i_sar_height, hTabs[3], IDC_SAR_H, FALSE, 1, 9999);
                            break;

                        case IDC_THREADEDIT:
                            CHECKED_SET_SHOW_INT(config->i_threads, hTabs[3], IDC_THREADEDIT, FALSE, 0, X264_THREAD_MAX);
                            break;

                        case IDC_NR:
                            CHECKED_SET_SHOW_INT(config->i_noise_reduction, hTabs[3], IDC_NR, FALSE, 0, 9999);
                            break;

                        default:
                            return FALSE;
                    }
                    break;

                default:
                    return FALSE;
            }
            break;

        case WM_HSCROLL:
            if ((HWND)lParam == GetDlgItem(hTabs[0], IDC_BITRATESLIDER))
            {
                switch (config->i_encoding_type)
                {
                    case 0:
                        break;

                    case 1:
                        config->i_qp = SendDlgItemMessage(hTabs[0], IDC_BITRATESLIDER, TBM_GETPOS, 0, 0);
                        SetDlgItemInt(hTabs[0], IDC_BITRATEEDIT, config->i_qp, FALSE);
                        break;

                    case 2:
                        config->i_rf_constant = SendDlgItemMessage(hTabs[0], IDC_BITRATESLIDER, TBM_GETPOS, 0, 0);
                        SetDlgItemDouble(hTabs[0], IDC_BITRATEEDIT, config->i_rf_constant * 0.1);
                        break;

                    case 3:
                    case 4:
                        config->i_passbitrate = SendDlgItemMessage(hTabs[0], IDC_BITRATESLIDER, TBM_GETPOS, 0, 0);
                        SetDlgItemInt(hTabs[0], IDC_BITRATEEDIT, config->i_passbitrate, FALSE);
                        break;

                    default:
                        assert(0);
                        break;
                }
            }
            else if ((HWND)lParam == GetDlgItem(hTabs[2], IDC_BBIASSLIDER))
            {
                config->i_bframe_bias = SendDlgItemMessage(hTabs[2], IDC_BBIASSLIDER, TBM_GETPOS, 0, 0);
                SetDlgItemInt(hTabs[2], IDC_BBIAS, config->i_bframe_bias, TRUE);
            }
            else if ((HWND)lParam == GetDlgItem(hTabs[3], IDC_INLOOP_A))
            {
                config->i_inloop_a = SendDlgItemMessage(hTabs[3], IDC_INLOOP_A, TBM_GETPOS, 0, 0);
                SetDlgItemInt(hTabs[3], IDC_LOOPA_TXT, config->i_inloop_a, TRUE);
            }
            else if ((HWND)lParam == GetDlgItem(hTabs[3], IDC_INLOOP_B))
            {
                config->i_inloop_b = SendDlgItemMessage(hTabs[3], IDC_INLOOP_B, TBM_GETPOS, 0, 0);
                SetDlgItemInt(hTabs[3], IDC_LOOPB_TXT, config->i_inloop_b, TRUE);
            }
            else
                return FALSE;
            break;

        default:
            return FALSE;
    }

    tabs_enable_items(hDlg, config);
    return TRUE;
}

/* About box */
BOOL CALLBACK callback_about(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
        case WM_INITDIALOG:
        {
            char temp[1024];

            sprintf(temp, "Build date: %s %s\nlibx264 core %d%s", __DATE__, __TIME__, X264_BUILD, X264_VERSION);
            SetDlgItemText(hDlg, IDC_BUILD, temp);
            break;
        }

        case WM_COMMAND:
            switch (HIWORD(wParam))
            {
                case BN_CLICKED:
                    if (LOWORD(wParam) == IDC_HOMEPAGE)
                        ShellExecute(hDlg, "open", X264VFW_WEBSITE, NULL, NULL, SW_SHOWNORMAL);
                    else if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
                        EndDialog(hDlg, LOWORD(wParam));
                    else
                        return FALSE;
                    break;

                default:
                    return FALSE;
            }
            break;

        default:
            return FALSE;
    }

    return TRUE;
}

/* Error console */
BOOL CALLBACK callback_err_console(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
        case WM_INITDIALOG:
            break;

        case WM_COMMAND:
            if (HIWORD(wParam) == BN_CLICKED)
            {
                switch (LOWORD(wParam))
                {
                    case IDOK:
                        DestroyWindow(hWnd);
                        break;

                    case IDC_COPYCLIP:
                        if (OpenClipboard(hWnd))
                        {
                            int i;
                            int num_lines = SendDlgItemMessage(hWnd, IDC_CONSOLE, LB_GETCOUNT, 0, 0);
                            int text_size;
                            char *buffer;
                            HGLOBAL clipbuffer;

                            if (num_lines <= 0)
                                break;

                            /* Calculate text size */
                            for (i = 0, text_size = 0; i < num_lines; i++)
                                text_size += SendDlgItemMessage(hWnd, IDC_CONSOLE, LB_GETTEXTLEN, (WPARAM)i, 0);

                            /* CR - LF for each line + terminating NULL */
                            text_size += 2 * num_lines + 1;

                            EmptyClipboard();
                            clipbuffer = GlobalAlloc(GMEM_MOVEABLE | GMEM_DDESHARE, text_size);
                            buffer = (char *)GlobalLock(clipbuffer);

                            /* Concatenate lines of text in the global buffer */
                            for (i = 0; i < num_lines; i++)
                            {
                                char msg_buf[1024];

                                SendDlgItemMessage(hWnd, IDC_CONSOLE, LB_GETTEXT, (WPARAM)i, (LPARAM)&msg_buf);
                                strcat(msg_buf, "\r\n");
                                memcpy(buffer, &msg_buf, strlen(msg_buf));
                                buffer += strlen(msg_buf);
                            }
                            *buffer = 0; /* null-terminate the buffer */

                            GlobalUnlock(clipbuffer);
                            SetClipboardData(CF_TEXT, clipbuffer);
                            CloseClipboard();
                        }
                        break;

                    default:
                        return FALSE;
                }
            }
            else
                return FALSE;
            break;

        default:
            return FALSE;
    }

    return TRUE;
}
