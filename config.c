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
#ifdef _WIN64
#define X264_REG_CHILD   "x264vfw64"
#else
#define X264_REG_CHILD   "x264"            /* not "x264vfw" because of GordianKnot compatibility */
#endif
#define X264_REG_CLASS   "config"

/* Description */
#define X264_NAME        "x264vfw"
#define X264_DEF_TEXT    "Are you sure you want to load default values?"

static const char * const overscan_names[]  = { "undef", "show", "crop" };
static const char * const vidformat_names[] = { "undef", "component", "pal", "ntsc", "secam", "mac" };
static const char * const fullrange_names[] = { "off", "on" };
static const char * const colorprim_names[] = { "undef", "bt709", "bt470m", "bt470bg", "smpte170m", "smpte240m", "film" };
static const char * const transfer_names[]  = { "undef", "bt709", "bt470m", "bt470bg", "smpte170m", "smpte240m", "linear", "log100", "log316" };
static const char * const colmatrix_names[] = { "undef", "bt709", "fcc", "bt470bg", "smpte170m", "smpte240m", "YCgCo", "GBR" };

static const int overscan_loc2abs[]  = { 0, 1, 2 };
static const int vidformat_loc2abs[] = { 5, 0, 1, 2, 3, 4 };
static const int fullrange_loc2abs[] = { 0, 1 };
static const int colorprim_loc2abs[] = { 2, 1, 4, 5, 6, 7, 8 };
static const int transfer_loc2abs[]  = { 2, 1, 4, 5, 6, 7, 8, 9, 10 };
static const int colmatrix_loc2abs[] = { 2, 1, 4, 5, 6, 7, 8, 0 };

static const int overscan_abs2loc[]  = { 0, 1, 2 };
static const int vidformat_abs2loc[] = { 1, 2, 3, 4, 5, 0 };
static const int fullrange_abs2loc[] = { 0, 1 };
static const int colorprim_abs2loc[] = { 0, 1, 0, 0, 2, 3, 4, 5, 6 };
static const int transfer_abs2loc[]  = { 0, 1, 0, 0, 2, 3, 4, 5, 6, 7, 8 };
static const int colmatrix_abs2loc[] = { 7, 1, 0, 0, 2, 3, 4, 5, 6 };

/* Registery handling */
typedef struct
{
    char *reg_value;
    int  *config_int;
    int  default_int;
    int  min_int;
    int  max_int;
} reg_int_t;

typedef struct
{
    char *reg_value;
    char *config_str;
    char *default_str;
    int  max_len;      /* maximum string length, including the terminating NULL char */
} reg_str_t;

typedef struct
{
    char  *reg_value;
    float *config_float;
    float default_float;
    float min_float;
    float max_float;
} reg_float_t;

CONFIG reg;
HWND   hMainDlg;
HWND   hTabs[3];
HWND   hTooltip;
int    b_tabs_updated;

static const reg_int_t reg_int_table[] =
{
    /* Main */
    { "encoding_type",    &reg.i_encoding_type,      4,   0,   4                   }, /* take into account GordianKnot workaround */
    { "quantizer",        &reg.i_qp,                 26,  1,   X264_QUANT_MAX      },
    { "ratefactor",       &reg.i_rf_constant,        260, 10,  X264_QUANT_MAX * 10 },
    { "passbitrate",      &reg.i_passbitrate,        800, 1,   X264_BITRATE_MAX    },
    { "pass_number",      &reg.i_pass,               1,   1,   2                   },
    { "fast1pass",        &reg.b_fast1pass,          0,   0,   1                   },
    { "createstats",      &reg.b_createstats,        0,   0,   1                   },
    { "updatestats",      &reg.b_updatestats,        1,   0,   1                   },

    /* Misc */
    { "avc_level",        &reg.i_avc_level,          0,   0,   15                  },
    { "sar_width",        &reg.i_sar_width,          1,   1,   9999                },
    { "sar_height",       &reg.i_sar_height,         1,   1,   9999                },

    /* Debug */
    { "log_level",        &reg.i_log_level,          2,   0,   4                   },
    { "psnr",             &reg.b_psnr,               1,   0,   1                   },
    { "ssim",             &reg.b_ssim,               1,   0,   1                   },
    { "no_asm",           &reg.b_no_asm,             0,   0,   1                   },

    /* VFW */
    { "fourcc_num",       &reg.i_fcc_num,            0,   0,   sizeof(fcc_str_table) / sizeof(fourcc_str) - 1},
#if X264VFW_USE_VIRTUALDUB_HACK
    { "vd_hack",          &reg.b_vd_hack,            0,   0,   1                   },
#endif
#if X264VFW_USE_DECODER
    { "disable_decoder",  &reg.b_disable_decoder,    0,   0,   1                   },
#endif

    /* Analysis */
    { "dct8x8",           &reg.b_dct8x8,             1,   0,   1                   },
    { "intra_i8x8",       &reg.b_intra_i8x8,         1,   0,   1                   },
    { "intra_i4x4",       &reg.b_intra_i4x4,         1,   0,   1                   },
    { "i8x8",             &reg.b_i8x8,               1,   0,   1                   },
    { "i4x4",             &reg.b_i4x4,               1,   0,   1                   },
    { "psub16x16",        &reg.b_psub16x16,          1,   0,   1                   },
    { "psub8x8",          &reg.b_psub8x8,            0,   0,   1                   },
    { "bsub16x16",        &reg.b_bsub16x16,          1,   0,   1                   },
    { "fast_pskip",       &reg.b_fast_pskip,         1,   0,   1                   },
    { "refmax",           &reg.i_refmax,             1,   1,   16                  },
    { "mixedref",         &reg.b_mixedref,           1,   0,   1                   },
    { "me_method",        &reg.i_me_method,          2,   0,   4                   },
    { "me_range",         &reg.i_me_range,           16,  4,   64                  },
    { "subpel",           &reg.i_subpel_refine,      7,   0,   9                   },
    { "chroma_me",        &reg.b_chroma_me,          1,   0,   1                   },
    { "keyint_min",       &reg.i_keyint_min,         25,  1,   9999                },
    { "keyint_max",       &reg.i_keyint_max,         250, 1,   9999                },
    { "scenecut",         &reg.i_scenecut_threshold, 40,  0,   100                 },
    { "bmax",             &reg.i_bframe,             0,   0,   X264_BFRAME_MAX     },
    { "b_adapt",          &reg.i_bframe_adaptive,    1,   0,   2                   },
    { "b_bias",           &reg.i_bframe_bias,        0,   -90, 100                 },
    { "direct_pred",      &reg.i_direct_mv_pred,     1,   0,   3                   },
    { "b_refs",           &reg.b_b_refs,             1,   0,   1                   },
    { "b_wpred",          &reg.b_b_wpred,            1,   0,   1                   },

    /* Encoding */
    { "loop_filter",      &reg.b_filter,             1,   0,   1                   },
    { "inloop_a",         &reg.i_inloop_a,           0,   -6,  6                   },
    { "inloop_b",         &reg.i_inloop_b,           0,   -6,  6                   },
#if !X264_PATCH_HDR
    { "interlaced",       &reg.i_interlaced,         0,   0,   1                   },
#else
    { "interlaced",       &reg.i_interlaced,         0,   0,   2                   },
#endif
    { "cabac",            &reg.b_cabac,              1,   0,   1                   },
    { "dct_decimate",     &reg.b_dct_decimate,       1,   0,   1                   },
    { "noise_reduction",  &reg.i_noise_reduction,    0,   0,   9999                },
    { "trellis",          &reg.i_trellis,            1,   0,   2                   },
    { "intra_deadzone",   &reg.i_intra_deadzone,     11,  0,   32                  },
    { "inter_deadzone",   &reg.i_inter_deadzone,     21,  0,   32                  },
    { "cqm",              &reg.i_cqm,                0,   0,   2                   },

    /* Rate control */
    { "vbv_maxrate",      &reg.i_vbv_maxrate,        0,   0,   999999              },
    { "vbv_bufsize",      &reg.i_vbv_bufsize,        0,   0,   999999              },
    { "vbv_occupancy",    &reg.i_vbv_occupancy,      90,  0,   100                 },
    { "qp_min",           &reg.i_qp_min,             10,  1,   X264_QUANT_MAX      },
    { "qp_max",           &reg.i_qp_max,             51,  1,   X264_QUANT_MAX      },
    { "qp_step",          &reg.i_qp_step,            4,   1,   X264_QUANT_MAX      },
    { "chroma_qp_offset", &reg.i_chroma_qp_offset,   0,   -12, 12                  },

    /* AQ */
    { "aq_mode",          &reg.i_aq_mode,            1,   0,   1                   },
#if X264_PATCH_VAQ_MOD
    { "aq_metric",        &reg.i_aq_metric,          0,   0,   2                   },
#endif

    /* Multithreading */
#if X264VFW_USE_THREADS
    { "threads",          &reg.i_threads,            1,   0,   X264_THREAD_MAX     },
    { "mt_deterministic", &reg.b_mt_deterministic,   1,   0,   1                   },
#if X264_PATCH_THREAD_POOL
    { "thread_queue",     &reg.i_thread_queue,       0,   0,   X264_THREAD_MAX     },
#endif
#endif

    /* VUI */
    { "overscan",        &reg.i_overscan,            0,   0,   2                   },
    { "vidformat",       &reg.i_vidformat,           5,   0,   5                   },
    { "fullrange",       &reg.i_fullrange,           0,   0,   1                   },
    { "colorprim",       &reg.i_colorprim,           2,   0,   8                   },
    { "transfer",        &reg.i_transfer,            2,   0,   10                  },
    { "colmatrix",       &reg.i_colmatrix,           2,   0,   8                   },
    { "chromaloc",       &reg.i_chromaloc,           0,   0,   5                   },

    /* Config */
    { "use_cmdline",      &reg.b_use_cmdline,        0,   0,   1                   }
};

static const reg_float_t reg_float_table[] =
{
    /* Analysis */
    { "psy_rdo",        &reg.f_psy_rdo,        0.0,   0.00, 1000.00 },

    /* Encoding */
    { "psy_trellis",    &reg.f_psy_trellis,    0.0,   0.00, 1000.00 },

    /* Rate control */
    { "ipratio",        &reg.f_ipratio,        1.40,  1.00, 1000.00 },
    { "pbratio",        &reg.f_pbratio,        1.30,  1.00, 1000.00 },
    { "qcomp",          &reg.f_qcomp,          0.60,  0.00, 1000.00 },
    { "cplxblur",       &reg.f_cplxblur,       20.00, 0.00, 1000.00 },
    { "qblur",          &reg.f_qblur,          0.50,  0.00, 1000.00 },
    { "ratetol",        &reg.f_ratetol,        1.00,  0.01, 1000.00 },

    /* AQ */
    { "aq_strength",    &reg.f_aq_strength,    1.00,  0.00, 1000.00 }
#if X264_PATCH_VAQ_MOD
   ,{ "aq_sensitivity", &reg.f_aq_sensitivity, 10.00, 0.00, 1000.00 }
#endif
};

static const reg_str_t reg_str_table[] =
{
    /* Main */
    { "statsfile",     reg.stats,         ".\\x264.stats",           MAX_STATS_PATH     },
    { "extra_cmdline", reg.extra_cmdline, "",                        MAX_CMDLINE        },

    /* VFW */
    { "fourcc",        reg.fcc,           (char *)&fcc_str_table[0], sizeof(fourcc_str) },

    /* Encoding */
    { "cqmfile",       reg.cqmfile,       "",                        MAX_PATH           },

    /* Config */
    { "cmdline",       reg.cmdline,       "",                        MAX_CMDLINE        }
};

double GetDlgItemDouble(HWND hDlg, int nIDDlgItem)
{
    char temp[1024];

    GetDlgItemText(hDlg, nIDDlgItem, temp, 1024);
    return atof(temp);
}

void SetDlgItemDouble(HWND hDlg, int nIDDlgItem, double dblValue, const char *format)
{
    char temp[1024];

    sprintf(temp, format, dblValue);
    SetDlgItemText(hDlg, nIDDlgItem, temp);
}

#define VUI_abs2loc(val, name)\
    ((val) >= 0 && (val) < sizeof(name##_abs2loc) / sizeof(const int)) ? name##_abs2loc[val] : 0

#define VUI_loc2abs(val, name)\
    ((val) >= 0 && (val) < sizeof(name##_loc2abs) / sizeof(const int)) ? name##_loc2abs[val] : name##_loc2abs[0]

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
    char    temp[1024];

    RegOpenKeyEx(X264_REG_KEY, X264_REG_PARENT "\\" X264_REG_CHILD, 0, KEY_READ, &hKey);

    /* Read all integers */
    for (i = 0; i < sizeof(reg_int_table) / sizeof(reg_int_t); i++)
    {
        i_size = sizeof(int);
        if (RegQueryValueEx(hKey, reg_int_table[i].reg_value, 0, 0, (LPBYTE)reg_int_table[i].config_int, &i_size) != ERROR_SUCCESS)
            *reg_int_table[i].config_int = reg_int_table[i].default_int;
    }
    for (i = 0; i < sizeof(reg_int_table) / sizeof(reg_int_t); i++)
        *reg_int_table[i].config_int = X264_CLIP(*reg_int_table[i].config_int, reg_int_table[i].min_int, reg_int_table[i].max_int);

    /* Read all floats */
    for (i = 0; i < sizeof(reg_float_table) / sizeof(reg_float_t); i++)
    {
        i_size = 1024;
        if (RegQueryValueEx(hKey, reg_float_table[i].reg_value, 0, 0, (LPBYTE)temp, &i_size) != ERROR_SUCCESS)
            *reg_float_table[i].config_float = reg_float_table[i].default_float;
        else
            *reg_float_table[i].config_float = atof(temp);
    }
    for (i = 0; i < sizeof(reg_float_table) / sizeof(reg_float_t); i++)
        *reg_float_table[i].config_float = X264_CLIP(*reg_float_table[i].config_float, reg_float_table[i].min_float, reg_float_table[i].max_float);

    /* Read all strings */
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
    char    temp[1024];

    if (RegCreateKeyEx(X264_REG_KEY, X264_REG_PARENT "\\" X264_REG_CHILD, 0, X264_REG_CLASS, REG_OPTION_NON_VOLATILE, KEY_WRITE, 0, &hKey, &dwDisposition) != ERROR_SUCCESS)
        return;

    memcpy(&reg, config, sizeof(CONFIG));
    GordianKnotWorkaround(reg.i_encoding_type);

    /* Save all integers */
    for (i = 0; i < sizeof(reg_int_table) / sizeof(reg_int_t); i++)
        RegSetValueEx(hKey, reg_int_table[i].reg_value, 0, REG_DWORD, (LPBYTE)reg_int_table[i].config_int, sizeof(int));

    /* Save all floats */
    for (i = 0; i < sizeof(reg_float_table) / sizeof(reg_float_t); i++)
    {
        sprintf(temp, "%.2f", *reg_float_table[i].config_float);
        RegSetValueEx(hKey, reg_float_table[i].reg_value, 0, REG_SZ, (LPBYTE)temp, strlen(temp) + 1);
    }

    /* Save all strings */
    for (i = 0; i < sizeof(reg_str_table) / sizeof(reg_str_t); i++)
        RegSetValueEx(hKey, reg_str_table[i].reg_value, 0, REG_SZ, (LPBYTE)reg_str_table[i].config_str, strlen(reg_str_table[i].config_str) + 1);

    RegCloseKey(hKey);
}

void config_defaults(CONFIG *config)
{
    int     i;

    /* Set all integers */
    for (i = 0; i < sizeof(reg_int_table) / sizeof(reg_int_t); i++)
        *reg_int_table[i].config_int = reg_int_table[i].default_int;
    for (i = 0; i < sizeof(reg_int_table) / sizeof(reg_int_t); i++)
        *reg_int_table[i].config_int = X264_CLIP(*reg_int_table[i].config_int, reg_int_table[i].min_int, reg_int_table[i].max_int);

    /* Set all floats */
    for (i = 0; i < sizeof(reg_float_table) / sizeof(reg_float_t); i++)
        *reg_float_table[i].config_float = reg_float_table[i].default_float;
    for (i = 0; i < sizeof(reg_float_table) / sizeof(reg_float_t); i++)
        *reg_float_table[i].config_float = X264_CLIP(*reg_float_table[i].config_float, reg_float_table[i].min_float, reg_float_table[i].max_float);

    /* Set all strings */
    for (i = 0; i < sizeof(reg_str_table) / sizeof(reg_str_t); i++)
        strcpy(reg_str_table[i].config_str, reg_str_table[i].default_str);

    GordianKnotWorkaround(reg.i_encoding_type);
    memcpy(config, &reg, sizeof(CONFIG));
}

/* Tabs */
void tabs_enable_items(CONFIG *config)
{
    /* Main */
    ShowWindow(GetDlgItem(hTabs[0], IDC_MAIN_RC_LABEL), config->i_encoding_type > 0);
    ShowWindow(GetDlgItem(hTabs[0], IDC_MAIN_RC_VAL), config->i_encoding_type > 0);
    ShowWindow(GetDlgItem(hTabs[0], IDC_MAIN_RC_VAL_SLIDER), config->i_encoding_type > 0);
    ShowWindow(GetDlgItem(hTabs[0], IDC_MAIN_RC_LOW_LABEL), config->i_encoding_type > 0);
    ShowWindow(GetDlgItem(hTabs[0], IDC_MAIN_RC_HIGH_LABEL), config->i_encoding_type > 0);
    ShowWindow(GetDlgItem(hTabs[0], IDC_MAIN_STATS_CREATE), config->i_encoding_type != 4);
    ShowWindow(GetDlgItem(hTabs[0], IDC_MAIN_STATS_UPDATE), config->i_encoding_type == 4 && config->i_pass > 1);
    EnableWindow(GetDlgItem(hTabs[0], IDC_MAIN_STATS), config->i_encoding_type == 4 || config->b_createstats);
    EnableWindow(GetDlgItem(hTabs[0], IDC_MAIN_STATS_BROWSE), config->i_encoding_type == 4 || config->b_createstats);

    /* Debug */
    EnableWindow(GetDlgItem(hTabs[0], IDC_DEBUG_PSNR), config->i_encoding_type > 0 && config->i_log_level >= 3);
    EnableWindow(GetDlgItem(hTabs[0], IDC_DEBUG_SSIM), config->i_encoding_type > 0 && config->i_log_level >= 3);

    /* Analysis */
    EnableWindow(GetDlgItem(hTabs[1], IDC_ANALYSIS_8X8DCT), config->i_encoding_type > 0 || config->b_cabac);
    EnableWindow(GetDlgItem(hTabs[1], IDC_ANALYSIS_I_I8X8), (config->i_encoding_type > 0 || config->b_cabac) && config->b_dct8x8);
    EnableWindow(GetDlgItem(hTabs[1], IDC_ANALYSIS_PB_I8X8), (config->i_encoding_type > 0 || config->b_cabac) && config->b_dct8x8);
    EnableWindow(GetDlgItem(hTabs[1], IDC_ANALYSIS_PB_P4X4), config->b_psub16x16);
    EnableWindow(GetDlgItem(hTabs[1], IDC_ANALYSIS_PB_B8X8), config->i_bframe > 0);
    EnableWindow(GetDlgItem(hTabs[1], IDC_ANALYSIS_FAST_PSKIP), config->i_encoding_type > 0);
    EnableWindow(GetDlgItem(hTabs[1], IDC_ANALYSIS_MIXED_REFS), config->i_refmax > 1);
    EnableWindow(GetDlgItem(hTabs[1], IDC_ANALYSIS_CHROMA_ME), config->i_subpel_refine >= 5);
    EnableWindow(GetDlgItem(hTabs[1], IDC_ANALYSIS_PSY_RDO), config->i_encoding_type > 0 && config->i_subpel_refine >= 6);
    EnableWindow(GetDlgItem(hTabs[1], IDC_ANALYSIS_B_ADAPT), config->i_bframe > 0);
    EnableWindow(GetDlgItem(hTabs[1], IDC_ANALYSIS_B_BIAS), config->i_bframe > 0 && config->i_bframe_adaptive > 0);
    EnableWindow(GetDlgItem(hTabs[1], IDC_ANALYSIS_B_BIAS_SPIN), config->i_bframe > 0 && config->i_bframe_adaptive > 0);
    EnableWindow(GetDlgItem(hTabs[1], IDC_ANALYSIS_DIRECT), config->i_bframe > 0);
    EnableWindow(GetDlgItem(hTabs[1], IDC_ANALYSIS_B_PYRAMID), config->i_bframe > 1);
    EnableWindow(GetDlgItem(hTabs[1], IDC_ANALYSIS_WEIGHTB), config->i_bframe > 0);

    /* Encoding */
    EnableWindow(GetDlgItem(hTabs[1], IDC_ENC_DEBLOCK), config->i_encoding_type > 0);
    EnableWindow(GetDlgItem(hTabs[1], IDC_ENC_DEBLOCK_A), config->i_encoding_type > 0 && config->b_filter);
    EnableWindow(GetDlgItem(hTabs[1], IDC_ENC_DEBLOCK_A_SPIN), config->i_encoding_type > 0 && config->b_filter);
    EnableWindow(GetDlgItem(hTabs[1], IDC_ENC_DEBLOCK_B), config->i_encoding_type > 0 && config->b_filter);
    EnableWindow(GetDlgItem(hTabs[1], IDC_ENC_DEBLOCK_B_SPIN), config->i_encoding_type > 0 && config->b_filter);
    EnableWindow(GetDlgItem(hTabs[1], IDC_ENC_DCT_DECIMATE), config->i_encoding_type > 0);
    EnableWindow(GetDlgItem(hTabs[1], IDC_ENC_NR), config->i_encoding_type > 0);
    EnableWindow(GetDlgItem(hTabs[1], IDC_ENC_TRELLIS), config->i_encoding_type > 0 && config->b_cabac);
    EnableWindow(GetDlgItem(hTabs[1], IDC_ENC_DEADZONE_INTRA), config->i_encoding_type > 0);
    EnableWindow(GetDlgItem(hTabs[1], IDC_ENC_DEADZONE_INTRA_SPIN), config->i_encoding_type > 0);
    EnableWindow(GetDlgItem(hTabs[1], IDC_ENC_DEADZONE_INTER), config->i_encoding_type > 0);
    EnableWindow(GetDlgItem(hTabs[1], IDC_ENC_DEADZONE_INTER_SPIN), config->i_encoding_type > 0);
    EnableWindow(GetDlgItem(hTabs[1], IDC_ENC_CQM), config->i_encoding_type > 0);
    EnableWindow(GetDlgItem(hTabs[1], IDC_ENC_CQMFILE), config->i_encoding_type > 0 && config->i_cqm == 2);
    EnableWindow(GetDlgItem(hTabs[1], IDC_ENC_CQMFILE_BROWSE), config->i_encoding_type > 0 && config->i_cqm == 2);
    EnableWindow(GetDlgItem(hTabs[1], IDC_ENC_PSY_TRELLIS), config->i_encoding_type > 0 && config->b_cabac && config->i_trellis > 0);

    /* Rate control */
    EnableWindow(GetDlgItem(hTabs[2], IDC_RC_VBV_MAXRATE), config->i_encoding_type > 1);
    EnableWindow(GetDlgItem(hTabs[2], IDC_RC_VBV_BUFSIZE), config->i_encoding_type > 1);
    EnableWindow(GetDlgItem(hTabs[2], IDC_RC_VBV_INIT), config->i_encoding_type > 1);
    EnableWindow(GetDlgItem(hTabs[2], IDC_RC_VBV_INIT_SPIN), config->i_encoding_type > 1);
    EnableWindow(GetDlgItem(hTabs[2], IDC_RC_QPMIN), config->i_encoding_type > 1);
    EnableWindow(GetDlgItem(hTabs[2], IDC_RC_QPMIN_SPIN), config->i_encoding_type > 1);
    EnableWindow(GetDlgItem(hTabs[2], IDC_RC_QPMAX), config->i_encoding_type > 1);
    EnableWindow(GetDlgItem(hTabs[2], IDC_RC_QPMAX_SPIN), config->i_encoding_type > 1);
    EnableWindow(GetDlgItem(hTabs[2], IDC_RC_QPSTEP), config->i_encoding_type > 1);
    EnableWindow(GetDlgItem(hTabs[2], IDC_RC_QPSTEP_SPIN), config->i_encoding_type > 1);
    EnableWindow(GetDlgItem(hTabs[2], IDC_RC_IPRATIO), config->i_encoding_type > 0);
    EnableWindow(GetDlgItem(hTabs[2], IDC_RC_PBRATIO), config->i_encoding_type > 0 && config->i_bframe > 0);
    EnableWindow(GetDlgItem(hTabs[2], IDC_RC_CHROMA_QP_OFFSET), config->i_encoding_type > 0);
    EnableWindow(GetDlgItem(hTabs[2], IDC_RC_CHROMA_QP_OFFSET_SPIN), config->i_encoding_type > 0);
    EnableWindow(GetDlgItem(hTabs[2], IDC_RC_QCOMP), config->i_encoding_type > 1);
    EnableWindow(GetDlgItem(hTabs[2], IDC_RC_CPLXBLUR), config->i_encoding_type == 4 && config->i_pass > 1);
    EnableWindow(GetDlgItem(hTabs[2], IDC_RC_QBLUR), config->i_encoding_type == 4 && config->i_pass > 1);
    EnableWindow(GetDlgItem(hTabs[2], IDC_RC_RATETOL), config->i_encoding_type > 2);

    /* AQ */
    EnableWindow(GetDlgItem(hTabs[2], IDC_AQ_MODE), config->i_encoding_type > 1);
    EnableWindow(GetDlgItem(hTabs[2], IDC_AQ_STRENGTH), config->i_encoding_type > 1 && config->i_aq_mode > 0);
#if X264_PATCH_VAQ_MOD
    EnableWindow(GetDlgItem(hTabs[2], IDC_AQ_METRIC), config->i_encoding_type > 1 && config->i_aq_mode > 0);
    EnableWindow(GetDlgItem(hTabs[2], IDC_AQ_SENSITIVITY), config->i_encoding_type > 1 && config->i_aq_mode > 0);
#endif

    /* Multithreading */
#if X264VFW_USE_THREADS
    EnableWindow(GetDlgItem(hTabs[2], IDC_MT_DETERMINISTIC), config->i_threads != 1);
#if X264_PATCH_THREAD_POOL
    EnableWindow(GetDlgItem(hTabs[2], IDC_MT_THREAD_QUEUE), config->i_threads != 1);
    EnableWindow(GetDlgItem(hTabs[2], IDC_MT_THREAD_QUEUE_SPIN), config->i_threads != 1);
#endif
#endif

    /* Config */
    EnableWindow(GetDlgItem(hMainDlg, IDC_CONFIG_CMDLINE), config->b_use_cmdline);
}

void tabs_update_items(CONFIG *config)
{
    char szTmp[1024];

    /* Main */
    sprintf(szTmp, "Build date: %s %s\nlibx264 core %d%s", __DATE__, __TIME__, X264_BUILD, X264_VERSION);
    SetDlgItemText(hTabs[0], IDC_MAIN_BUILD_LABEL, szTmp);
    if (SendMessage(GetDlgItem(hTabs[0], IDC_MAIN_RC_MODE), CB_GETCOUNT, 0, 0) == 0)
    {
        SendDlgItemMessage(hTabs[0], IDC_MAIN_RC_MODE, CB_ADDSTRING, 0, (LPARAM)"Single pass - lossless");
        SendDlgItemMessage(hTabs[0], IDC_MAIN_RC_MODE, CB_ADDSTRING, 0, (LPARAM)"Single pass - quantizer-based (CQP)");
        SendDlgItemMessage(hTabs[0], IDC_MAIN_RC_MODE, CB_ADDSTRING, 0, (LPARAM)"Single pass - ratefactor-based (CRF)");
        SendDlgItemMessage(hTabs[0], IDC_MAIN_RC_MODE, CB_ADDSTRING, 0, (LPARAM)"Single pass - bitrate-based (ABR)");
        SendDlgItemMessage(hTabs[0], IDC_MAIN_RC_MODE, CB_ADDSTRING, 0, (LPARAM)"Multipass - 1st pass");
        SendDlgItemMessage(hTabs[0], IDC_MAIN_RC_MODE, CB_ADDSTRING, 0, (LPARAM)"Multipass - 1st pass (fast)");
        SendDlgItemMessage(hTabs[0], IDC_MAIN_RC_MODE, CB_ADDSTRING, 0, (LPARAM)"Multipass - Nth pass");
    }
    SendMessage(GetDlgItem(hTabs[0], IDC_MAIN_RC_VAL), EM_LIMITTEXT, 8, 0);
    switch (config->i_encoding_type)
    {
        case 0: /* 1 pass, lossless */
            SendDlgItemMessage(hTabs[0], IDC_MAIN_RC_MODE, CB_SETCURSEL, 0, 0);
            SetDlgItemText(hTabs[0], IDC_MAIN_RC_LABEL, "");
            SetDlgItemText(hTabs[0], IDC_MAIN_RC_LOW_LABEL, "");
            SetDlgItemText(hTabs[0], IDC_MAIN_RC_HIGH_LABEL, "");
            SetDlgItemInt(hTabs[0], IDC_MAIN_RC_VAL, 0, FALSE);
            SendDlgItemMessage(hTabs[0], IDC_MAIN_RC_VAL_SLIDER, TBM_SETRANGEMIN, TRUE, 0);
            SendDlgItemMessage(hTabs[0], IDC_MAIN_RC_VAL_SLIDER, TBM_SETRANGEMAX, TRUE, 0);
            SendDlgItemMessage(hTabs[0], IDC_MAIN_RC_VAL_SLIDER, TBM_SETPOS, TRUE, 0);
            break;

        case 1: /* 1 pass, quantizer-based (CQP) */
            SendDlgItemMessage(hTabs[0], IDC_MAIN_RC_MODE, CB_SETCURSEL, 1, 0);
            SetDlgItemText(hTabs[0], IDC_MAIN_RC_LABEL, "Quantizer");
            SetDlgItemText(hTabs[0], IDC_MAIN_RC_LOW_LABEL, "1 (High quality)");
            sprintf(szTmp, "(Low quality) %d", X264_QUANT_MAX);
            SetDlgItemText(hTabs[0], IDC_MAIN_RC_HIGH_LABEL, szTmp);
            SetDlgItemInt(hTabs[0], IDC_MAIN_RC_VAL, config->i_qp, FALSE);
            SendDlgItemMessage(hTabs[0], IDC_MAIN_RC_VAL_SLIDER, TBM_SETRANGEMIN, TRUE, 1);
            SendDlgItemMessage(hTabs[0], IDC_MAIN_RC_VAL_SLIDER, TBM_SETRANGEMAX, TRUE, X264_QUANT_MAX);
            SendDlgItemMessage(hTabs[0], IDC_MAIN_RC_VAL_SLIDER, TBM_SETPOS, TRUE, config->i_qp);
            break;

        case 2: /* 1 pass, ratefactor-based (CRF) */
            SendDlgItemMessage(hTabs[0], IDC_MAIN_RC_MODE, CB_SETCURSEL, 2, 0);
            SetDlgItemText(hTabs[0], IDC_MAIN_RC_LABEL, "Ratefactor");
            SetDlgItemText(hTabs[0], IDC_MAIN_RC_LOW_LABEL, "1.0 (High quality)");
            sprintf(szTmp, "(Low quality) %d.0", X264_QUANT_MAX);
            SetDlgItemText(hTabs[0], IDC_MAIN_RC_HIGH_LABEL, szTmp);
            SetDlgItemDouble(hTabs[0], IDC_MAIN_RC_VAL, config->i_rf_constant * 0.1, "%.1f");
            SendDlgItemMessage(hTabs[0], IDC_MAIN_RC_VAL_SLIDER, TBM_SETRANGEMIN, TRUE, 10);
            SendDlgItemMessage(hTabs[0], IDC_MAIN_RC_VAL_SLIDER, TBM_SETRANGEMAX, TRUE, X264_QUANT_MAX * 10);
            SendDlgItemMessage(hTabs[0], IDC_MAIN_RC_VAL_SLIDER, TBM_SETPOS, TRUE, config->i_rf_constant);
            break;

        case 3: /* 1 pass, bitrate-based (ABR) */
            SendDlgItemMessage(hTabs[0], IDC_MAIN_RC_MODE, CB_SETCURSEL, 3, 0);
            SetDlgItemText(hTabs[0], IDC_MAIN_RC_LABEL, "Average bitrate (kbit/s)");
            SetDlgItemText(hTabs[0], IDC_MAIN_RC_LOW_LABEL, "1");
            sprintf(szTmp, "%d", X264_BITRATE_MAX);
            SetDlgItemText(hTabs[0], IDC_MAIN_RC_HIGH_LABEL, szTmp);
            SetDlgItemInt(hTabs[0], IDC_MAIN_RC_VAL, config->i_passbitrate, FALSE);
            SendDlgItemMessage(hTabs[0], IDC_MAIN_RC_VAL_SLIDER, TBM_SETRANGEMIN, TRUE, 1);
            SendDlgItemMessage(hTabs[0], IDC_MAIN_RC_VAL_SLIDER, TBM_SETRANGEMAX, TRUE, X264_BITRATE_MAX);
            SendDlgItemMessage(hTabs[0], IDC_MAIN_RC_VAL_SLIDER, TBM_SETPOS, TRUE, config->i_passbitrate);
            break;

        case 4: /* 2 pass */
            if (config->i_pass > 1)
                SendDlgItemMessage(hTabs[0], IDC_MAIN_RC_MODE, CB_SETCURSEL, 6, 0);
            else if (config->b_fast1pass)
                SendDlgItemMessage(hTabs[0], IDC_MAIN_RC_MODE, CB_SETCURSEL, 5, 0);
            else
                SendDlgItemMessage(hTabs[0], IDC_MAIN_RC_MODE, CB_SETCURSEL, 4, 0);
            SetDlgItemText(hTabs[0], IDC_MAIN_RC_LABEL, "Target bitrate (kbit/s)");
            SetDlgItemText(hTabs[0], IDC_MAIN_RC_LOW_LABEL, "1");
            sprintf(szTmp, "%d", X264_BITRATE_MAX);
            SetDlgItemText(hTabs[0], IDC_MAIN_RC_HIGH_LABEL, szTmp);
            SetDlgItemInt(hTabs[0], IDC_MAIN_RC_VAL, config->i_passbitrate, FALSE);
            SendDlgItemMessage(hTabs[0], IDC_MAIN_RC_VAL_SLIDER, TBM_SETRANGEMIN, TRUE, 1);
            SendDlgItemMessage(hTabs[0], IDC_MAIN_RC_VAL_SLIDER, TBM_SETRANGEMAX, TRUE, X264_BITRATE_MAX);
            SendDlgItemMessage(hTabs[0], IDC_MAIN_RC_VAL_SLIDER, TBM_SETPOS, TRUE, config->i_passbitrate);
            break;

        default:
            assert(0);
            break;
    }
    CheckDlgButton(hTabs[0], IDC_MAIN_STATS_CREATE, config->b_createstats);
    CheckDlgButton(hTabs[0], IDC_MAIN_STATS_UPDATE, config->b_updatestats);
    SendMessage(GetDlgItem(hTabs[0], IDC_MAIN_STATS), EM_LIMITTEXT, MAX_STATS_PATH - 1, 0);
    SetDlgItemText(hTabs[0], IDC_MAIN_STATS, config->stats);
    SendMessage(GetDlgItem(hTabs[0], IDC_MAIN_EXTRA_CMDLINE), EM_LIMITTEXT, MAX_CMDLINE - 1, 0);
    SetDlgItemText(hTabs[0], IDC_MAIN_EXTRA_CMDLINE, config->extra_cmdline);

    /* Misc */
    if (SendMessage(GetDlgItem(hTabs[0], IDC_MISC_LEVEL), CB_GETCOUNT, 0, 0) == 0)
    {
        SendDlgItemMessage(hTabs[0], IDC_MISC_LEVEL, CB_ADDSTRING, 0, (LPARAM)"Auto");
        SendDlgItemMessage(hTabs[0], IDC_MISC_LEVEL, CB_ADDSTRING, 0, (LPARAM)"1.0");
        SendDlgItemMessage(hTabs[0], IDC_MISC_LEVEL, CB_ADDSTRING, 0, (LPARAM)"1.1");
        SendDlgItemMessage(hTabs[0], IDC_MISC_LEVEL, CB_ADDSTRING, 0, (LPARAM)"1.2");
        SendDlgItemMessage(hTabs[0], IDC_MISC_LEVEL, CB_ADDSTRING, 0, (LPARAM)"1.3");
        SendDlgItemMessage(hTabs[0], IDC_MISC_LEVEL, CB_ADDSTRING, 0, (LPARAM)"2.0");
        SendDlgItemMessage(hTabs[0], IDC_MISC_LEVEL, CB_ADDSTRING, 0, (LPARAM)"2.1");
        SendDlgItemMessage(hTabs[0], IDC_MISC_LEVEL, CB_ADDSTRING, 0, (LPARAM)"2.2");
        SendDlgItemMessage(hTabs[0], IDC_MISC_LEVEL, CB_ADDSTRING, 0, (LPARAM)"3.0");
        SendDlgItemMessage(hTabs[0], IDC_MISC_LEVEL, CB_ADDSTRING, 0, (LPARAM)"3.1");
        SendDlgItemMessage(hTabs[0], IDC_MISC_LEVEL, CB_ADDSTRING, 0, (LPARAM)"3.2");
        SendDlgItemMessage(hTabs[0], IDC_MISC_LEVEL, CB_ADDSTRING, 0, (LPARAM)"4.0");
        SendDlgItemMessage(hTabs[0], IDC_MISC_LEVEL, CB_ADDSTRING, 0, (LPARAM)"4.1");
        SendDlgItemMessage(hTabs[0], IDC_MISC_LEVEL, CB_ADDSTRING, 0, (LPARAM)"4.2");
        SendDlgItemMessage(hTabs[0], IDC_MISC_LEVEL, CB_ADDSTRING, 0, (LPARAM)"5.0");
        SendDlgItemMessage(hTabs[0], IDC_MISC_LEVEL, CB_ADDSTRING, 0, (LPARAM)"5.1");
    }
    SendDlgItemMessage(hTabs[0], IDC_MISC_LEVEL, CB_SETCURSEL, config->i_avc_level, 0);
    SendMessage(GetDlgItem(hTabs[0], IDC_MISC_SAR_W), EM_LIMITTEXT, 8, 0);
    SetDlgItemInt(hTabs[0], IDC_MISC_SAR_W, config->i_sar_width, FALSE);
    SendMessage(GetDlgItem(hTabs[0], IDC_MISC_SAR_H), EM_LIMITTEXT, 8, 0);
    SetDlgItemInt(hTabs[0], IDC_MISC_SAR_H, config->i_sar_height, FALSE);

    /* Debug */
    if (SendMessage(GetDlgItem(hTabs[0], IDC_DEBUG_LOG_LEVEL), CB_GETCOUNT, 0, 0) == 0)
    {
        SendDlgItemMessage(hTabs[0], IDC_DEBUG_LOG_LEVEL, CB_ADDSTRING, 0, (LPARAM)"None");
        SendDlgItemMessage(hTabs[0], IDC_DEBUG_LOG_LEVEL, CB_ADDSTRING, 0, (LPARAM)"Error");
        SendDlgItemMessage(hTabs[0], IDC_DEBUG_LOG_LEVEL, CB_ADDSTRING, 0, (LPARAM)"Warning");
        SendDlgItemMessage(hTabs[0], IDC_DEBUG_LOG_LEVEL, CB_ADDSTRING, 0, (LPARAM)"Info");
        SendDlgItemMessage(hTabs[0], IDC_DEBUG_LOG_LEVEL, CB_ADDSTRING, 0, (LPARAM)"Debug");
    }
    SendDlgItemMessage(hTabs[0], IDC_DEBUG_LOG_LEVEL, CB_SETCURSEL, config->i_log_level, 0);
    CheckDlgButton(hTabs[0], IDC_DEBUG_PSNR, config->b_psnr);
    CheckDlgButton(hTabs[0], IDC_DEBUG_SSIM, config->b_ssim);
    CheckDlgButton(hTabs[0], IDC_DEBUG_NO_ASM, config->b_no_asm);

    /* VFW */
    if (SendMessage(GetDlgItem(hTabs[0], IDC_VFW_FOURCC), CB_GETCOUNT, 0, 0) == 0)
    {
        int i;
        for (i = 0; i < sizeof(fcc_str_table) / sizeof(fourcc_str); i++)
            SendDlgItemMessage(hTabs[0], IDC_VFW_FOURCC, CB_ADDSTRING, 0, (LPARAM)&fcc_str_table[i]);
    }
    SendDlgItemMessage(hTabs[0], IDC_VFW_FOURCC, CB_SETCURSEL, config->i_fcc_num, 0);
#if X264VFW_USE_VIRTUALDUB_HACK
    CheckDlgButton(hTabs[0], IDC_VFW_VD_HACK, config->b_vd_hack);
#endif
#if X264VFW_USE_DECODER
    CheckDlgButton(hTabs[0], IDC_VFW_DISABLE_DECODER, config->b_disable_decoder);
#endif

    /* Analysis */
    CheckDlgButton(hTabs[1], IDC_ANALYSIS_8X8DCT, config->b_dct8x8);
    CheckDlgButton(hTabs[1], IDC_ANALYSIS_I_I8X8, config->b_intra_i8x8);
    CheckDlgButton(hTabs[1], IDC_ANALYSIS_I_I4X4, config->b_intra_i4x4);
    CheckDlgButton(hTabs[1], IDC_ANALYSIS_PB_I8X8, config->b_i8x8);
    CheckDlgButton(hTabs[1], IDC_ANALYSIS_PB_I4X4, config->b_i4x4);
    CheckDlgButton(hTabs[1], IDC_ANALYSIS_PB_P8X8, config->b_psub16x16);
    CheckDlgButton(hTabs[1], IDC_ANALYSIS_PB_P4X4, config->b_psub8x8);
    CheckDlgButton(hTabs[1], IDC_ANALYSIS_PB_B8X8, config->b_bsub16x16);
    CheckDlgButton(hTabs[1], IDC_ANALYSIS_FAST_PSKIP, config->b_fast_pskip);
    SendMessage(GetDlgItem(hTabs[1], IDC_ANALYSIS_REF), EM_LIMITTEXT, 8, 0);
    SetDlgItemInt(hTabs[1], IDC_ANALYSIS_REF, config->i_refmax, FALSE);
    CheckDlgButton(hTabs[1], IDC_ANALYSIS_MIXED_REFS, config->b_mixedref);
    if (SendMessage(GetDlgItem(hTabs[1], IDC_ANALYSIS_ME), CB_GETCOUNT, 0, 0) == 0)
    {
        SendDlgItemMessage(hTabs[1], IDC_ANALYSIS_ME, CB_ADDSTRING, 0, (LPARAM)"dia");
        SendDlgItemMessage(hTabs[1], IDC_ANALYSIS_ME, CB_ADDSTRING, 0, (LPARAM)"hex");
        SendDlgItemMessage(hTabs[1], IDC_ANALYSIS_ME, CB_ADDSTRING, 0, (LPARAM)"umh");
        SendDlgItemMessage(hTabs[1], IDC_ANALYSIS_ME, CB_ADDSTRING, 0, (LPARAM)"esa");
        SendDlgItemMessage(hTabs[1], IDC_ANALYSIS_ME, CB_ADDSTRING, 0, (LPARAM)"tesa");
    }
    SendDlgItemMessage(hTabs[1], IDC_ANALYSIS_ME, CB_SETCURSEL, config->i_me_method, 0);
    SendMessage(GetDlgItem(hTabs[1], IDC_ANALYSIS_MERANGE), EM_LIMITTEXT, 8, 0);
    SetDlgItemInt(hTabs[1], IDC_ANALYSIS_MERANGE, config->i_me_range, FALSE);
    if (SendMessage(GetDlgItem(hTabs[1], IDC_ANALYSIS_SUBME), CB_GETCOUNT, 0, 0) == 0)
    {
        SendDlgItemMessage(hTabs[1], IDC_ANALYSIS_SUBME, CB_ADDSTRING, 0, (LPARAM)"0 FullPel");
        SendDlgItemMessage(hTabs[1], IDC_ANALYSIS_SUBME, CB_ADDSTRING, 0, (LPARAM)"1 QPel SAD");
        SendDlgItemMessage(hTabs[1], IDC_ANALYSIS_SUBME, CB_ADDSTRING, 0, (LPARAM)"2 QPel SATD");
        SendDlgItemMessage(hTabs[1], IDC_ANALYSIS_SUBME, CB_ADDSTRING, 0, (LPARAM)"3 HPel+QPel");
        SendDlgItemMessage(hTabs[1], IDC_ANALYSIS_SUBME, CB_ADDSTRING, 0, (LPARAM)"4 QPel+QPel");
        SendDlgItemMessage(hTabs[1], IDC_ANALYSIS_SUBME, CB_ADDSTRING, 0, (LPARAM)"5 QPel+BiME");
        SendDlgItemMessage(hTabs[1], IDC_ANALYSIS_SUBME, CB_ADDSTRING, 0, (LPARAM)"6 RD on I/P");
        SendDlgItemMessage(hTabs[1], IDC_ANALYSIS_SUBME, CB_ADDSTRING, 0, (LPARAM)"7 RD on all");
        SendDlgItemMessage(hTabs[1], IDC_ANALYSIS_SUBME, CB_ADDSTRING, 0, (LPARAM)"8 RDr on I/P");
        SendDlgItemMessage(hTabs[1], IDC_ANALYSIS_SUBME, CB_ADDSTRING, 0, (LPARAM)"9 RDr on all");
    }
    SendDlgItemMessage(hTabs[1], IDC_ANALYSIS_SUBME, CB_SETCURSEL, config->i_subpel_refine, 0);
    CheckDlgButton(hTabs[1], IDC_ANALYSIS_CHROMA_ME, config->b_chroma_me);
    SendMessage(GetDlgItem(hTabs[1], IDC_ANALYSIS_PSY_RDO), EM_LIMITTEXT, 8, 0);
    SetDlgItemDouble(hTabs[1], IDC_ANALYSIS_PSY_RDO, config->f_psy_rdo, "%.2f");
    SendMessage(GetDlgItem(hTabs[1], IDC_ANALYSIS_MIN_KEYINT), EM_LIMITTEXT, 8, 0);
    SetDlgItemInt(hTabs[1], IDC_ANALYSIS_MIN_KEYINT, config->i_keyint_min, FALSE);
    SendMessage(GetDlgItem(hTabs[1], IDC_ANALYSIS_KEYINT), EM_LIMITTEXT, 8, 0);
    SetDlgItemInt(hTabs[1], IDC_ANALYSIS_KEYINT, config->i_keyint_max, FALSE);
    SendMessage(GetDlgItem(hTabs[1], IDC_ANALYSIS_SCENECUT), EM_LIMITTEXT, 8, 0);
    SetDlgItemInt(hTabs[1], IDC_ANALYSIS_SCENECUT, config->i_scenecut_threshold, FALSE);
    SendMessage(GetDlgItem(hTabs[1], IDC_ANALYSIS_BFRAMES), EM_LIMITTEXT, 8, 0);
    SetDlgItemInt(hTabs[1], IDC_ANALYSIS_BFRAMES, config->i_bframe, FALSE);
    if (SendMessage(GetDlgItem(hTabs[1], IDC_ANALYSIS_B_ADAPT), CB_GETCOUNT, 0, 0) == 0)
    {
        SendDlgItemMessage(hTabs[1], IDC_ANALYSIS_B_ADAPT, CB_ADDSTRING, 0, (LPARAM)"Off");
        SendDlgItemMessage(hTabs[1], IDC_ANALYSIS_B_ADAPT, CB_ADDSTRING, 0, (LPARAM)"Fast");
        SendDlgItemMessage(hTabs[1], IDC_ANALYSIS_B_ADAPT, CB_ADDSTRING, 0, (LPARAM)"Optimal");
    }
    SendDlgItemMessage(hTabs[1], IDC_ANALYSIS_B_ADAPT, CB_SETCURSEL, config->i_bframe_adaptive, 0);
    SendMessage(GetDlgItem(hTabs[1], IDC_ANALYSIS_B_BIAS), EM_LIMITTEXT, 8, 0);
    SetDlgItemInt(hTabs[1], IDC_ANALYSIS_B_BIAS, config->i_bframe_bias, TRUE);
    if (SendMessage(GetDlgItem(hTabs[1], IDC_ANALYSIS_DIRECT), CB_GETCOUNT, 0, 0) == 0)
    {
        SendDlgItemMessage(hTabs[1], IDC_ANALYSIS_DIRECT, CB_ADDSTRING, 0, (LPARAM)"None");
        SendDlgItemMessage(hTabs[1], IDC_ANALYSIS_DIRECT, CB_ADDSTRING, 0, (LPARAM)"Spatial");
        SendDlgItemMessage(hTabs[1], IDC_ANALYSIS_DIRECT, CB_ADDSTRING, 0, (LPARAM)"Temporal");
        SendDlgItemMessage(hTabs[1], IDC_ANALYSIS_DIRECT, CB_ADDSTRING, 0, (LPARAM)"Auto");
    }
    SendDlgItemMessage(hTabs[1], IDC_ANALYSIS_DIRECT, CB_SETCURSEL, config->i_direct_mv_pred, 0);
    CheckDlgButton(hTabs[1], IDC_ANALYSIS_B_PYRAMID, config->b_b_refs);
    CheckDlgButton(hTabs[1], IDC_ANALYSIS_WEIGHTB, config->b_b_wpred);

    /* Encoding */
    CheckDlgButton(hTabs[1], IDC_ENC_DEBLOCK, config->b_filter);
    SendMessage(GetDlgItem(hTabs[1], IDC_ENC_DEBLOCK_A), EM_LIMITTEXT, 8, 0);
    SetDlgItemInt(hTabs[1], IDC_ENC_DEBLOCK_A, config->i_inloop_a, TRUE);
    SendMessage(GetDlgItem(hTabs[1], IDC_ENC_DEBLOCK_B), EM_LIMITTEXT, 8, 0);
    SetDlgItemInt(hTabs[1], IDC_ENC_DEBLOCK_B, config->i_inloop_b, TRUE);
    if (SendMessage(GetDlgItem(hTabs[1], IDC_ENC_INTERLACED), CB_GETCOUNT, 0, 0) == 0)
    {
        SendDlgItemMessage(hTabs[1], IDC_ENC_INTERLACED, CB_ADDSTRING, 0, (LPARAM)"Off");
#if !X264_PATCH_HDR
        SendDlgItemMessage(hTabs[1], IDC_ENC_INTERLACED, CB_ADDSTRING, 0, (LPARAM)"On");
#else
        SendDlgItemMessage(hTabs[1], IDC_ENC_INTERLACED, CB_ADDSTRING, 0, (LPARAM)"TFF");
        SendDlgItemMessage(hTabs[1], IDC_ENC_INTERLACED, CB_ADDSTRING, 0, (LPARAM)"BFF");
#endif
    }
    SendDlgItemMessage(hTabs[1], IDC_ENC_INTERLACED, CB_SETCURSEL, config->i_interlaced, 0);
    CheckDlgButton(hTabs[1], IDC_ENC_CABAC, config->b_cabac);
    CheckDlgButton(hTabs[1], IDC_ENC_DCT_DECIMATE, config->b_dct_decimate);
    SendMessage(GetDlgItem(hTabs[1], IDC_ENC_NR), EM_LIMITTEXT, 8, 0);
    SetDlgItemInt(hTabs[1], IDC_ENC_NR, config->i_noise_reduction, FALSE);
    if (SendMessage(GetDlgItem(hTabs[1], IDC_ENC_TRELLIS), CB_GETCOUNT, 0, 0) == 0)
    {
        SendDlgItemMessage(hTabs[1], IDC_ENC_TRELLIS, CB_ADDSTRING, 0, (LPARAM)"Off");
        SendDlgItemMessage(hTabs[1], IDC_ENC_TRELLIS, CB_ADDSTRING, 0, (LPARAM)"MB encode");
        SendDlgItemMessage(hTabs[1], IDC_ENC_TRELLIS, CB_ADDSTRING, 0, (LPARAM)"Always");
    }
    SendDlgItemMessage(hTabs[1], IDC_ENC_TRELLIS, CB_SETCURSEL, config->i_trellis, 0);
    SendMessage(GetDlgItem(hTabs[1], IDC_ENC_DEADZONE_INTRA), EM_LIMITTEXT, 8, 0);
    SetDlgItemInt(hTabs[1], IDC_ENC_DEADZONE_INTRA, config->i_intra_deadzone, FALSE);
    SendMessage(GetDlgItem(hTabs[1], IDC_ENC_DEADZONE_INTER), EM_LIMITTEXT, 8, 0);
    SetDlgItemInt(hTabs[1], IDC_ENC_DEADZONE_INTER, config->i_inter_deadzone, FALSE);

    if (SendMessage(GetDlgItem(hTabs[1], IDC_ENC_CQM), CB_GETCOUNT, 0, 0) == 0)
    {
        SendDlgItemMessage(hTabs[1], IDC_ENC_CQM, CB_ADDSTRING, 0, (LPARAM)"Flat");
        SendDlgItemMessage(hTabs[1], IDC_ENC_CQM, CB_ADDSTRING, 0, (LPARAM)"JVT");
        SendDlgItemMessage(hTabs[1], IDC_ENC_CQM, CB_ADDSTRING, 0, (LPARAM)"Custom");
    }
    SendDlgItemMessage(hTabs[1], IDC_ENC_CQM, CB_SETCURSEL, config->i_cqm, 0);
    SendMessage(GetDlgItem(hTabs[1], IDC_ENC_CQMFILE), EM_LIMITTEXT, MAX_PATH - 1, 0);
    SetDlgItemText(hTabs[1], IDC_ENC_CQMFILE, config->cqmfile);
    SendMessage(GetDlgItem(hTabs[1], IDC_ENC_PSY_TRELLIS), EM_LIMITTEXT, 8, 0);
    SetDlgItemDouble(hTabs[1], IDC_ENC_PSY_TRELLIS, config->f_psy_trellis, "%.2f");

    /* Rate control */
    SendMessage(GetDlgItem(hTabs[2], IDC_RC_VBV_MAXRATE), EM_LIMITTEXT, 8, 0);
    SetDlgItemInt(hTabs[2], IDC_RC_VBV_MAXRATE, config->i_vbv_maxrate, FALSE);
    SendMessage(GetDlgItem(hTabs[2], IDC_RC_VBV_BUFSIZE), EM_LIMITTEXT, 8, 0);
    SetDlgItemInt(hTabs[2], IDC_RC_VBV_BUFSIZE, config->i_vbv_bufsize, FALSE);
    SendMessage(GetDlgItem(hTabs[2], IDC_RC_VBV_INIT), EM_LIMITTEXT, 8, 0);
    SetDlgItemInt(hTabs[2], IDC_RC_VBV_INIT, config->i_vbv_occupancy, FALSE);
    SendMessage(GetDlgItem(hTabs[2], IDC_RC_QPMIN), EM_LIMITTEXT, 8, 0);
    SetDlgItemInt(hTabs[2], IDC_RC_QPMIN, config->i_qp_min, FALSE);
    SendMessage(GetDlgItem(hTabs[2], IDC_RC_QPMAX), EM_LIMITTEXT, 8, 0);
    SetDlgItemInt(hTabs[2], IDC_RC_QPMAX, config->i_qp_max, FALSE);
    SendMessage(GetDlgItem(hTabs[2], IDC_RC_QPSTEP), EM_LIMITTEXT, 8, 0);
    SetDlgItemInt(hTabs[2], IDC_RC_QPSTEP, config->i_qp_step, FALSE);
    SendMessage(GetDlgItem(hTabs[2], IDC_RC_IPRATIO), EM_LIMITTEXT, 8, 0);
    SetDlgItemDouble(hTabs[2], IDC_RC_IPRATIO, config->f_ipratio, "%.2f");
    SendMessage(GetDlgItem(hTabs[2], IDC_RC_PBRATIO), EM_LIMITTEXT, 8, 0);
    SetDlgItemDouble(hTabs[2], IDC_RC_PBRATIO, config->f_pbratio, "%.2f");
    SendMessage(GetDlgItem(hTabs[2], IDC_RC_CHROMA_QP_OFFSET), EM_LIMITTEXT, 8, 0);
    SetDlgItemInt(hTabs[2], IDC_RC_CHROMA_QP_OFFSET, config->i_chroma_qp_offset, TRUE);
    SendMessage(GetDlgItem(hTabs[2], IDC_RC_QCOMP), EM_LIMITTEXT, 8, 0);
    SetDlgItemDouble(hTabs[2], IDC_RC_QCOMP, config->f_qcomp, "%.2f");
    SendMessage(GetDlgItem(hTabs[2], IDC_RC_CPLXBLUR), EM_LIMITTEXT, 8, 0);
    SetDlgItemDouble(hTabs[2], IDC_RC_CPLXBLUR, config->f_cplxblur, "%.2f");
    SendMessage(GetDlgItem(hTabs[2], IDC_RC_QBLUR), EM_LIMITTEXT, 8, 0);
    SetDlgItemDouble(hTabs[2], IDC_RC_QBLUR, config->f_qblur, "%.2f");
    SendMessage(GetDlgItem(hTabs[2], IDC_RC_RATETOL), EM_LIMITTEXT, 8, 0);
    SetDlgItemDouble(hTabs[2], IDC_RC_RATETOL, config->f_ratetol, "%.2f");

    /* AQ */
    if (SendMessage(GetDlgItem(hTabs[2], IDC_AQ_MODE), CB_GETCOUNT, 0, 0) == 0)
    {
        SendDlgItemMessage(hTabs[2], IDC_AQ_MODE, CB_ADDSTRING, 0, (LPARAM)"Off");
        SendDlgItemMessage(hTabs[2], IDC_AQ_MODE, CB_ADDSTRING, 0, (LPARAM)"On");
    }
    SendDlgItemMessage(hTabs[2], IDC_AQ_MODE, CB_SETCURSEL, config->i_aq_mode, 0);
    SendMessage(GetDlgItem(hTabs[2], IDC_AQ_STRENGTH), EM_LIMITTEXT, 8, 0);
    SetDlgItemDouble(hTabs[2], IDC_AQ_STRENGTH, config->f_aq_strength, "%.2f");
#if X264_PATCH_VAQ_MOD
    if (SendMessage(GetDlgItem(hTabs[2], IDC_AQ_METRIC), CB_GETCOUNT, 0, 0) == 0)
    {
        SendDlgItemMessage(hTabs[2], IDC_AQ_METRIC, CB_ADDSTRING, 0, (LPARAM)"Original");
        SendDlgItemMessage(hTabs[2], IDC_AQ_METRIC, CB_ADDSTRING, 0, (LPARAM)"BM ver. 0");
        SendDlgItemMessage(hTabs[2], IDC_AQ_METRIC, CB_ADDSTRING, 0, (LPARAM)"BM ver. 1");
    }
    SendDlgItemMessage(hTabs[2], IDC_AQ_METRIC, CB_SETCURSEL, config->i_aq_metric, 0);
    SendMessage(GetDlgItem(hTabs[2], IDC_AQ_SENSITIVITY), EM_LIMITTEXT, 8, 0);
    SetDlgItemDouble(hTabs[2], IDC_AQ_SENSITIVITY, config->f_aq_sensitivity, "%.2f");
#endif

    /* Multithreading */
#if X264VFW_USE_THREADS
    SendMessage(GetDlgItem(hTabs[2], IDC_MT_THREADS), EM_LIMITTEXT, 8, 0);
    SetDlgItemInt(hTabs[2], IDC_MT_THREADS, config->i_threads, FALSE);
    CheckDlgButton(hTabs[2], IDC_MT_DETERMINISTIC, config->b_mt_deterministic);
#if X264_PATCH_THREAD_POOL
    SendMessage(GetDlgItem(hTabs[2], IDC_MT_THREAD_QUEUE), EM_LIMITTEXT, 8, 0);
    SetDlgItemInt(hTabs[2], IDC_MT_THREAD_QUEUE, config->i_thread_queue, FALSE);
#endif
#endif

    /* VUI */
    if (SendMessage(GetDlgItem(hTabs[2], IDC_VUI_OVERSCAN), CB_GETCOUNT, 0, 0) == 0)
    {
        int i;
        for (i = 0; i < sizeof(overscan_names) / sizeof(const char *); i++)
            SendDlgItemMessage(hTabs[2], IDC_VUI_OVERSCAN, CB_ADDSTRING, 0, (LPARAM)overscan_names[i]);
    }
    SendDlgItemMessage(hTabs[2], IDC_VUI_OVERSCAN, CB_SETCURSEL, VUI_abs2loc(config->i_overscan, overscan), 0);
    if (SendMessage(GetDlgItem(hTabs[2], IDC_VUI_VIDEOFORMAT), CB_GETCOUNT, 0, 0) == 0)
    {
        int i;
        for (i = 0; i < sizeof(vidformat_names) / sizeof(const char *); i++)
            SendDlgItemMessage(hTabs[2], IDC_VUI_VIDEOFORMAT, CB_ADDSTRING, 0, (LPARAM)vidformat_names[i]);
    }
    SendDlgItemMessage(hTabs[2], IDC_VUI_VIDEOFORMAT, CB_SETCURSEL, VUI_abs2loc(config->i_vidformat, vidformat), 0);
    if (SendMessage(GetDlgItem(hTabs[2], IDC_VUI_FULLRANGE), CB_GETCOUNT, 0, 0) == 0)
    {
        int i;
        for (i = 0; i < sizeof(fullrange_names) / sizeof(const char *); i++)
            SendDlgItemMessage(hTabs[2], IDC_VUI_FULLRANGE, CB_ADDSTRING, 0, (LPARAM)fullrange_names[i]);
    }
    SendDlgItemMessage(hTabs[2], IDC_VUI_FULLRANGE, CB_SETCURSEL, VUI_abs2loc(config->i_fullrange, fullrange), 0);
    if (SendMessage(GetDlgItem(hTabs[2], IDC_VUI_COLORPRIM), CB_GETCOUNT, 0, 0) == 0)
    {
        int i;
        for (i = 0; i < sizeof(colorprim_names) / sizeof(const char *); i++)
            SendDlgItemMessage(hTabs[2], IDC_VUI_COLORPRIM, CB_ADDSTRING, 0, (LPARAM)colorprim_names[i]);
    }
    SendDlgItemMessage(hTabs[2], IDC_VUI_COLORPRIM, CB_SETCURSEL, VUI_abs2loc(config->i_colorprim, colorprim), 0);
    if (SendMessage(GetDlgItem(hTabs[2], IDC_VUI_TRANSFER), CB_GETCOUNT, 0, 0) == 0)
    {
        int i;
        for (i = 0; i < sizeof(transfer_names) / sizeof(const char *); i++)
            SendDlgItemMessage(hTabs[2], IDC_VUI_TRANSFER, CB_ADDSTRING, 0, (LPARAM)transfer_names[i]);
    }
    SendDlgItemMessage(hTabs[2], IDC_VUI_TRANSFER, CB_SETCURSEL, VUI_abs2loc(config->i_transfer, transfer), 0);
    if (SendMessage(GetDlgItem(hTabs[2], IDC_VUI_COLORMATRIX), CB_GETCOUNT, 0, 0) == 0)
    {
        int i;
        for (i = 0; i < sizeof(colmatrix_names) / sizeof(const char *); i++)
            SendDlgItemMessage(hTabs[2], IDC_VUI_COLORMATRIX, CB_ADDSTRING, 0, (LPARAM)colmatrix_names[i]);
    }
    SendDlgItemMessage(hTabs[2], IDC_VUI_COLORMATRIX, CB_SETCURSEL, VUI_abs2loc(config->i_colmatrix, colmatrix), 0);
    SendMessage(GetDlgItem(hTabs[2], IDC_VUI_CHROMALOC), EM_LIMITTEXT, 8, 0);
    SetDlgItemInt(hTabs[2], IDC_VUI_CHROMALOC, config->i_chromaloc, FALSE);

    /* Config */
    CheckDlgButton(hMainDlg, IDC_CONFIG_USE_CMDLINE, config->b_use_cmdline);
    SendMessage(GetDlgItem(hMainDlg, IDC_CONFIG_CMDLINE), EM_LIMITTEXT, MAX_CMDLINE - 1, 0);
    SetDlgItemText(hMainDlg, IDC_CONFIG_CMDLINE, config->cmdline);
}

/* Assigns tooltips */
BOOL CALLBACK enum_tooltips(HWND hWnd, LPARAM lParam)
{
    char help[1024];

    /* The tooltip for a control is named the same as the control itself */
    if (LoadString(g_hInst, GetDlgCtrlID(hWnd), help, 1024))
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
INT_PTR CALLBACK callback_main(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CONFIG *config = (CONFIG *)GetWindowLongPtr(hDlg, GWLP_USERDATA);

    switch (uMsg)
    {
        case WM_INITDIALOG:
        {
            TCITEM tie;
            RECT rect;
            HWND hTabCtrl = GetDlgItem(hDlg, IDC_CONFIG_TABCONTROL);

            SetWindowLongPtr(hDlg, GWLP_USERDATA, lParam);
            config = (CONFIG *)lParam;

            hMainDlg = hDlg;

            b_tabs_updated = FALSE;

            // insert tabs in tab control
            tie.mask = TCIF_TEXT;
            tie.iImage = -1;
            tie.pszText = "Main";                    TabCtrl_InsertItem(hTabCtrl, 0, &tie);
            tie.pszText = "Analysis && Encoding";    TabCtrl_InsertItem(hTabCtrl, 1, &tie);
            tie.pszText = "Rate control && Other";   TabCtrl_InsertItem(hTabCtrl, 2, &tie);
            hTabs[0] = CreateDialogParam(g_hInst, MAKEINTRESOURCE(IDD_TAB_MAIN),         hMainDlg, (DLGPROC)callback_tabs, lParam);
            hTabs[1] = CreateDialogParam(g_hInst, MAKEINTRESOURCE(IDD_TAB_ANALYSIS_ENC), hMainDlg, (DLGPROC)callback_tabs, lParam);
            hTabs[2] = CreateDialogParam(g_hInst, MAKEINTRESOURCE(IDD_TAB_RC_OTHER),     hMainDlg, (DLGPROC)callback_tabs, lParam);
            GetClientRect(hTabCtrl, &rect);
            TabCtrl_AdjustRect(hTabCtrl, FALSE, &rect);
            MoveWindow(hTabs[0], rect.left, rect.top, rect.right - rect.left + 1, rect.bottom - rect.top + 1, TRUE);
            MoveWindow(hTabs[1], rect.left, rect.top, rect.right - rect.left + 1, rect.bottom - rect.top + 1, TRUE);
            MoveWindow(hTabs[2], rect.left, rect.top, rect.right - rect.left + 1, rect.bottom - rect.top + 1, TRUE);

            if ((hTooltip = CreateWindow(TOOLTIPS_CLASS, NULL, TTS_ALWAYSTIP, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, NULL, NULL, g_hInst, NULL)))
            {
                SetWindowPos(hTooltip, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
                SendMessage(hTooltip, TTM_SETMAXTIPWIDTH, 0, 400);
                EnumChildWindows(hMainDlg, enum_tooltips, 0);
            }

            tabs_enable_items(config);
            tabs_update_items(config);
            b_tabs_updated = TRUE;
            SetWindowPos(hTabs[0], hTabCtrl, 0, 0, 0, 0, SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE);
            ShowWindow(hTabs[0], SW_SHOW);
            break;
        }

        case WM_NOTIFY:
        {
            NMHDR FAR *tem = (NMHDR FAR *)lParam;

            if (tem->code == TCN_SELCHANGING)
            {
                int num;
                HWND hTabCtrl = GetDlgItem(hMainDlg, IDC_CONFIG_TABCONTROL);

                SetFocus(hTabCtrl);
                num = TabCtrl_GetCurSel(hTabCtrl);
                ShowWindow(hTabs[num], SW_HIDE);
            }
            else if (tem->code == TCN_SELCHANGE)
            {
                int num;
                HWND hTabCtrl = GetDlgItem(hMainDlg, IDC_CONFIG_TABCONTROL);

                SetFocus(hTabCtrl);
                num = TabCtrl_GetCurSel(hTabCtrl);
                SetWindowPos(hTabs[num], hTabCtrl, 0, 0, 0, 0, SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE);
                ShowWindow(hTabs[num], SW_SHOW);
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
                            EndDialog(hMainDlg, LOWORD(wParam));
                            break;

                        case IDCANCEL:
                            config->b_save = FALSE;
                            EndDialog(hMainDlg, LOWORD(wParam));
                            break;

                        case IDC_CONFIG_DEFAULTS:
                            if (MessageBox(hMainDlg, X264_DEF_TEXT, X264_NAME, MB_YESNO) == IDYES)
                            {
                                config_defaults(config);
                                b_tabs_updated = FALSE;
                                tabs_enable_items(config);
                                tabs_update_items(config);
                                b_tabs_updated = TRUE;
                            }
                            break;

                        case IDC_CONFIG_USE_CMDLINE:
                            config->b_use_cmdline = IsDlgButtonChecked(hMainDlg, IDC_CONFIG_USE_CMDLINE);
                            EnableWindow(GetDlgItem(hMainDlg, IDC_CONFIG_CMDLINE), config->b_use_cmdline);
                            break;

                        default:
                            return FALSE;
                    }
                    break;

                case EN_CHANGE:
                    switch (LOWORD(wParam))
                    {
                        case IDC_CONFIG_CMDLINE:
                            if (GetDlgItemText(hMainDlg, IDC_CONFIG_CMDLINE, config->cmdline, MAX_CMDLINE) == 0)
                                strcpy(config->cmdline, "");
                            break;

                        default:
                            return FALSE;
                    }
                    break;

                case EN_KILLFOCUS:
                    switch (LOWORD(wParam))
                    {
                        case IDC_CONFIG_CMDLINE:
                            if (GetDlgItemText(hMainDlg, IDC_CONFIG_CMDLINE, config->cmdline, MAX_CMDLINE) == 0)
                                strcpy(config->cmdline, "");
                            SetDlgItemText(hMainDlg, IDC_CONFIG_CMDLINE, config->cmdline);
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
    CheckControlTextIsNumber(GetDlgItem(hDlg, nIDDlgItem), bSigned, 0);\
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
    CheckControlTextIsNumber(GetDlgItem(hDlg, nIDDlgItem), bSigned, 0);\
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
    CheckControlTextIsNumber(GetDlgItem(hDlg, nIDDlgItem), bSigned, 0);\
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
    CheckControlTextIsNumber(GetDlgItem(hDlg, nIDDlgItem), bSigned, 0);\
    var = GetDlgItemInt(hDlg, nIDDlgItem, NULL, bSigned);\
    if (var < min)\
        var = min;\
    else if (var > max)\
        var = max;\
    SetDlgItemInt(hDlg, nIDDlgItem, var, bSigned);\
}

#define CHECKED_SET_FLOAT(var, hDlg, nIDDlgItem, bSigned, min, max, iDecimalPlacesNum, format)\
{\
    CheckControlTextIsNumber(GetDlgItem(hDlg, nIDDlgItem), bSigned, iDecimalPlacesNum);\
    var = GetDlgItemDouble(hDlg, nIDDlgItem);\
    if (var < min)\
    {\
        var = min;\
        SetDlgItemDouble(hDlg, nIDDlgItem, var, format);\
        SendMessage(GetDlgItem(hDlg, nIDDlgItem), EM_SETSEL, -2, -2);\
    }\
    else if (var > max)\
    {\
        var = max;\
        SetDlgItemDouble(hDlg, nIDDlgItem, var, format);\
        SendMessage(GetDlgItem(hDlg, nIDDlgItem), EM_SETSEL, -2, -2);\
    }\
}

#define CHECKED_SET_MIN_FLOAT(var, hDlg, nIDDlgItem, bSigned, min, max, iDecimalPlacesNum, format)\
{\
    CheckControlTextIsNumber(GetDlgItem(hDlg, nIDDlgItem), bSigned, iDecimalPlacesNum);\
    var = GetDlgItemDouble(hDlg, nIDDlgItem);\
    if (var < min)\
    {\
        var = min;\
        SetDlgItemDouble(hDlg, nIDDlgItem, var, format);\
        SendMessage(GetDlgItem(hDlg, nIDDlgItem), EM_SETSEL, -2, -2);\
    }\
    else if (var > max)\
        var = max;\
}

#define CHECKED_SET_MAX_FLOAT(var, hDlg, nIDDlgItem, bSigned, min, max, iDecimalPlacesNum, format)\
{\
    CheckControlTextIsNumber(GetDlgItem(hDlg, nIDDlgItem), bSigned, iDecimalPlacesNum);\
    var = GetDlgItemDouble(hDlg, nIDDlgItem);\
    if (var < min)\
        var = min;\
    else if (var > max)\
    {\
        var = max;\
        SetDlgItemDouble(hDlg, nIDDlgItem, var, format);\
        SendMessage(GetDlgItem(hDlg, nIDDlgItem), EM_SETSEL, -2, -2);\
    }\
}

#define CHECKED_SET_SHOW_FLOAT(var, hDlg, nIDDlgItem, bSigned, min, max, iDecimalPlacesNum, format)\
{\
    CheckControlTextIsNumber(GetDlgItem(hDlg, nIDDlgItem), bSigned, iDecimalPlacesNum);\
    var = GetDlgItemDouble(hDlg, nIDDlgItem);\
    if (var < min)\
        var = min;\
    else if (var > max)\
        var = max;\
    SetDlgItemDouble(hDlg, nIDDlgItem, var, format);\
}

INT_PTR CALLBACK callback_tabs(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CONFIG *config = (CONFIG *)GetWindowLongPtr(hDlg, GWLP_USERDATA);

    if (uMsg == WM_INITDIALOG)
    {
        SetWindowLongPtr(hDlg, GWLP_USERDATA, lParam);
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
                        case IDC_MAIN_RC_MODE:
                            config->i_pass = 1;
                            config->b_fast1pass = FALSE;
                            switch (SendDlgItemMessage(hTabs[0], IDC_MAIN_RC_MODE, CB_GETCURSEL, 0, 0))
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
                            tabs_update_items(config);

                            /* Ugly hack for fixing visualization bug of IDC_MAIN_RC_VAL_SLIDER */
                            ShowWindow(GetDlgItem(hTabs[0], IDC_MAIN_RC_VAL_SLIDER), FALSE);
                            ShowWindow(GetDlgItem(hTabs[0], IDC_MAIN_RC_VAL_SLIDER), config->i_encoding_type > 0);

                            b_tabs_updated = TRUE;
                            break;

                        case IDC_MISC_LEVEL:
                            config->i_avc_level = SendDlgItemMessage(hTabs[0], IDC_MISC_LEVEL, CB_GETCURSEL, 0, 0);
                            break;

                        case IDC_DEBUG_LOG_LEVEL:
                            config->i_log_level = SendDlgItemMessage(hTabs[0], IDC_DEBUG_LOG_LEVEL, CB_GETCURSEL, 0, 0);
                            break;

                        case IDC_VFW_FOURCC:
                            config->i_fcc_num = SendDlgItemMessage(hTabs[0], IDC_VFW_FOURCC, CB_GETCURSEL, 0, 0);
                            config->i_fcc_num = X264_CLIP(config->i_fcc_num, 0, sizeof(fcc_str_table) / sizeof(fourcc_str) - 1);
                            strcpy(config->fcc, fcc_str_table[config->i_fcc_num]);
                            break;

                        case IDC_ANALYSIS_ME:
                            config->i_me_method = SendDlgItemMessage(hTabs[1], IDC_ANALYSIS_ME, CB_GETCURSEL, 0, 0);
                            break;

                        case IDC_ANALYSIS_SUBME:
                            config->i_subpel_refine = SendDlgItemMessage(hTabs[1], IDC_ANALYSIS_SUBME, CB_GETCURSEL, 0, 0);
                            break;

                        case IDC_ANALYSIS_B_ADAPT:
                            config->i_bframe_adaptive = SendDlgItemMessage(hTabs[1], IDC_ANALYSIS_B_ADAPT, CB_GETCURSEL, 0, 0);
                            break;

                        case IDC_ANALYSIS_DIRECT:
                            config->i_direct_mv_pred = SendDlgItemMessage(hTabs[1], IDC_ANALYSIS_DIRECT, CB_GETCURSEL, 0, 0);
                            break;

                        case IDC_ENC_INTERLACED:
                            config->i_interlaced = SendDlgItemMessage(hTabs[1], IDC_ENC_INTERLACED, CB_GETCURSEL, 0, 0);
                            break;

                        case IDC_ENC_TRELLIS:
                            config->i_trellis = SendDlgItemMessage(hTabs[1], IDC_ENC_TRELLIS, CB_GETCURSEL, 0, 0);
                            break;

                        case IDC_ENC_CQM:
                            config->i_cqm = SendDlgItemMessage(hTabs[1], IDC_ENC_CQM, CB_GETCURSEL, 0, 0);
                            break;

                        case IDC_AQ_MODE:
                            config->i_aq_mode = SendDlgItemMessage(hTabs[2], IDC_AQ_MODE, CB_GETCURSEL, 0, 0);
                            break;

#if X264_PATCH_VAQ_MOD
                        case IDC_AQ_METRIC:
                            config->i_aq_metric = SendDlgItemMessage(hTabs[2], IDC_AQ_METRIC, CB_GETCURSEL, 0, 0);
                            break;
#endif

                        case IDC_VUI_OVERSCAN:
                            config->i_overscan = SendDlgItemMessage(hTabs[2], IDC_VUI_OVERSCAN, CB_GETCURSEL, 0, 0);
                            config->i_overscan = VUI_loc2abs(config->i_overscan, overscan);
                            break;

                        case IDC_VUI_VIDEOFORMAT:
                            config->i_vidformat = SendDlgItemMessage(hTabs[2], IDC_VUI_VIDEOFORMAT, CB_GETCURSEL, 0, 0);
                            config->i_vidformat = VUI_loc2abs(config->i_vidformat, vidformat);
                            break;

                        case IDC_VUI_FULLRANGE:
                            config->i_fullrange = SendDlgItemMessage(hTabs[2], IDC_VUI_FULLRANGE, CB_GETCURSEL, 0, 0);
                            config->i_fullrange = VUI_loc2abs(config->i_fullrange, fullrange);
                            break;

                        case IDC_VUI_COLORPRIM:
                            config->i_colorprim = SendDlgItemMessage(hTabs[2], IDC_VUI_COLORPRIM, CB_GETCURSEL, 0, 0);
                            config->i_colorprim = VUI_loc2abs(config->i_colorprim, colorprim);
                            break;

                        case IDC_VUI_TRANSFER:
                            config->i_transfer = SendDlgItemMessage(hTabs[2], IDC_VUI_TRANSFER, CB_GETCURSEL, 0, 0);
                            config->i_transfer = VUI_loc2abs(config->i_transfer, transfer);
                            break;

                        case IDC_VUI_COLORMATRIX:
                            config->i_colmatrix = SendDlgItemMessage(hTabs[2], IDC_VUI_COLORMATRIX, CB_GETCURSEL, 0, 0);
                            config->i_colmatrix = VUI_loc2abs(config->i_colmatrix, colmatrix);
                            break;

                        default:
                            return FALSE;
                    }
                    break;

                case BN_CLICKED:
                    switch (LOWORD(wParam))
                    {
                        case IDC_MAIN_STATS_CREATE:
                            config->b_createstats = IsDlgButtonChecked(hTabs[0], IDC_MAIN_STATS_CREATE);
                            break;

                        case IDC_MAIN_STATS_UPDATE:
                            config->b_updatestats = IsDlgButtonChecked(hTabs[0], IDC_MAIN_STATS_UPDATE);
                            break;

                        case IDC_MAIN_STATS_BROWSE:
                        {
                            OPENFILENAME ofn;
                            char tmp[MAX_STATS_SIZE];

                            GetDlgItemText(hTabs[0], IDC_MAIN_STATS, tmp, MAX_STATS_SIZE);

                            memset(&ofn, 0, sizeof(OPENFILENAME));
                            ofn.lStructSize = sizeof(OPENFILENAME);

                            ofn.hwndOwner = hMainDlg;
                            ofn.lpstrFilter = "Stats files (*.stats)\0*.stats\0All files (*.*)\0*.*\0\0";
                            ofn.lpstrFile = tmp;
                            ofn.nMaxFile = MAX_STATS_SIZE;
                            ofn.Flags = OFN_PATHMUSTEXIST;

                            if (config->i_pass <= 1)
                                ofn.Flags |= OFN_OVERWRITEPROMPT;
                            else
                                ofn.Flags |= OFN_FILEMUSTEXIST;

                            if ((config->i_pass <= 1 && GetSaveFileName(&ofn)) ||
                                (config->i_pass > 1 && GetOpenFileName(&ofn)))
                                SetDlgItemText(hTabs[0], IDC_MAIN_STATS, tmp);
                            break;
                        }

                        case IDC_DEBUG_PSNR:
                            config->b_psnr = IsDlgButtonChecked(hTabs[0], IDC_DEBUG_PSNR);
                            break;

                        case IDC_DEBUG_SSIM:
                            config->b_ssim = IsDlgButtonChecked(hTabs[0], IDC_DEBUG_SSIM);
                            break;

                        case IDC_DEBUG_NO_ASM:
                            config->b_no_asm = IsDlgButtonChecked(hTabs[0], IDC_DEBUG_NO_ASM);
                            break;

#if X264VFW_USE_VIRTUALDUB_HACK
                        case IDC_VFW_VD_HACK:
                            config->b_vd_hack = IsDlgButtonChecked(hTabs[0], IDC_VFW_VD_HACK);
                            break;
#endif

#if X264VFW_USE_DECODER
                        case IDC_VFW_DISABLE_DECODER:
                            config->b_disable_decoder = IsDlgButtonChecked(hTabs[0], IDC_VFW_DISABLE_DECODER);
                            break;
#endif

                        case IDC_ANALYSIS_8X8DCT:
                            config->b_dct8x8 = IsDlgButtonChecked(hTabs[1], IDC_ANALYSIS_8X8DCT);
                            break;

                        case IDC_ANALYSIS_I_I8X8:
                            config->b_intra_i8x8 = IsDlgButtonChecked(hTabs[1], IDC_ANALYSIS_I_I8X8);
                            break;

                        case IDC_ANALYSIS_I_I4X4:
                            config->b_intra_i4x4 = IsDlgButtonChecked(hTabs[1], IDC_ANALYSIS_I_I4X4);
                            break;

                        case IDC_ANALYSIS_PB_I8X8:
                            config->b_i8x8 = IsDlgButtonChecked(hTabs[1], IDC_ANALYSIS_PB_I8X8);
                            break;

                        case IDC_ANALYSIS_PB_I4X4:
                            config->b_i4x4 = IsDlgButtonChecked(hTabs[1], IDC_ANALYSIS_PB_I4X4);
                            break;

                        case IDC_ANALYSIS_PB_P8X8:
                            config->b_psub16x16 = IsDlgButtonChecked(hTabs[1], IDC_ANALYSIS_PB_P8X8);
                            break;

                        case IDC_ANALYSIS_PB_P4X4:
                            config->b_psub8x8 = IsDlgButtonChecked(hTabs[1], IDC_ANALYSIS_PB_P4X4);
                            break;

                        case IDC_ANALYSIS_PB_B8X8:
                            config->b_bsub16x16 = IsDlgButtonChecked(hTabs[1], IDC_ANALYSIS_PB_B8X8);
                            break;

                        case IDC_ANALYSIS_FAST_PSKIP:
                            config->b_fast_pskip = IsDlgButtonChecked(hTabs[1], IDC_ANALYSIS_FAST_PSKIP);
                            break;

                        case IDC_ANALYSIS_MIXED_REFS:
                            config->b_mixedref = IsDlgButtonChecked(hTabs[1], IDC_ANALYSIS_MIXED_REFS);
                            break;

                        case IDC_ANALYSIS_CHROMA_ME:
                            config->b_chroma_me = IsDlgButtonChecked(hTabs[1], IDC_ANALYSIS_CHROMA_ME);
                            break;

                        case IDC_ANALYSIS_B_PYRAMID:
                            config->b_b_refs = IsDlgButtonChecked(hTabs[1], IDC_ANALYSIS_B_PYRAMID);
                            break;

                        case IDC_ANALYSIS_WEIGHTB:
                            config->b_b_wpred = IsDlgButtonChecked(hTabs[1], IDC_ANALYSIS_WEIGHTB);
                            break;

                        case IDC_ENC_DEBLOCK:
                            config->b_filter = IsDlgButtonChecked(hTabs[1], IDC_ENC_DEBLOCK);
                            break;

                        case IDC_ENC_CABAC:
                            config->b_cabac = IsDlgButtonChecked(hTabs[1], IDC_ENC_CABAC);
                            break;

                        case IDC_ENC_DCT_DECIMATE:
                            config->b_dct_decimate = IsDlgButtonChecked(hTabs[1], IDC_ENC_DCT_DECIMATE);
                            break;

                        case IDC_ENC_CQMFILE_BROWSE:
                        {
                            OPENFILENAME ofn;
                            char tmp[MAX_PATH];

                            GetDlgItemText(hTabs[1], IDC_ENC_CQMFILE, tmp, MAX_PATH);

                            memset(&ofn, 0, sizeof(OPENFILENAME));
                            ofn.lStructSize = sizeof(OPENFILENAME);

                            ofn.hwndOwner = hMainDlg;
                            ofn.lpstrFilter = "Custom quant matrices files (*.cfg)\0*.cfg\0All files (*.*)\0*.*\0\0";
                            ofn.lpstrFile = tmp;
                            ofn.nMaxFile = MAX_PATH;
                            ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

                            if (GetOpenFileName(&ofn))
                                SetDlgItemText(hTabs[1], IDC_ENC_CQMFILE, tmp);
                            break;
                        }

#if X264VFW_USE_THREADS
                        case IDC_MT_DETERMINISTIC:
                            config->b_mt_deterministic = IsDlgButtonChecked(hTabs[2], IDC_MT_DETERMINISTIC);
                            break;
#endif

                        default:
                            return FALSE;
                    }
                    break;

                case EN_CHANGE:
                    switch (LOWORD(wParam))
                    {
                        case IDC_MAIN_RC_VAL:
                            switch (config->i_encoding_type)
                            {
                                case 0:
                                    break;

                                case 1:
                                    CHECKED_SET_MAX_INT(config->i_qp, hTabs[0], IDC_MAIN_RC_VAL, FALSE, 1, X264_QUANT_MAX);
                                    SendDlgItemMessage(hTabs[0], IDC_MAIN_RC_VAL_SLIDER, TBM_SETPOS, TRUE, config->i_qp);
                                    break;

                                case 2:
                                    CheckControlTextIsNumber(GetDlgItem(hTabs[0], IDC_MAIN_RC_VAL), FALSE, 1);
                                    config->i_rf_constant = (int)(GetDlgItemDouble(hTabs[0], IDC_MAIN_RC_VAL) * 10);
                                    if (config->i_rf_constant < 10)
                                        config->i_rf_constant = 10;
                                    else if (config->i_rf_constant > X264_QUANT_MAX * 10)
                                    {
                                        config->i_rf_constant = X264_QUANT_MAX * 10;
                                        SetDlgItemDouble(hTabs[0], IDC_MAIN_RC_VAL, config->i_rf_constant * 0.1, "%.1f");
                                        SendMessage(GetDlgItem(hTabs[0], IDC_MAIN_RC_VAL), EM_SETSEL, -2, -2);
                                    }
                                    SendDlgItemMessage(hTabs[0], IDC_MAIN_RC_VAL_SLIDER, TBM_SETPOS, TRUE, config->i_rf_constant);
                                    break;

                                case 3:
                                case 4:
                                    CHECKED_SET_MAX_INT(config->i_passbitrate, hTabs[0], IDC_MAIN_RC_VAL, FALSE, 1, X264_BITRATE_MAX);
                                    SendDlgItemMessage(hTabs[0], IDC_MAIN_RC_VAL_SLIDER, TBM_SETPOS, TRUE, config->i_passbitrate);
                                    break;

                                default:
                                    assert(0);
                                    break;
                            }
                            break;

                        case IDC_MAIN_STATS:
                            if (GetDlgItemText(hTabs[0], IDC_MAIN_STATS, config->stats, MAX_STATS_PATH) == 0)
                                strcpy(config->stats, ".\\x264.stats");
                            break;

                        case IDC_MAIN_EXTRA_CMDLINE:
                            if (GetDlgItemText(hTabs[0], IDC_MAIN_EXTRA_CMDLINE, config->extra_cmdline, MAX_CMDLINE) == 0)
                                strcpy(config->extra_cmdline, "");
                            break;

                        case IDC_MISC_SAR_W:
                            CHECKED_SET_MAX_INT(config->i_sar_width, hTabs[0], IDC_MISC_SAR_W, FALSE, 1, 9999);
                            break;

                        case IDC_MISC_SAR_H:
                            CHECKED_SET_MAX_INT(config->i_sar_height, hTabs[0], IDC_MISC_SAR_H, FALSE, 1, 9999);
                            break;

                        case IDC_ANALYSIS_REF:
                            CHECKED_SET_MAX_INT(config->i_refmax, hTabs[1], IDC_ANALYSIS_REF, FALSE, 1, 16);
                            break;

                        case IDC_ANALYSIS_MERANGE:
                            CHECKED_SET_MAX_INT(config->i_me_range, hTabs[1], IDC_ANALYSIS_MERANGE, FALSE, 4, 64);
                            break;

                        case IDC_ANALYSIS_PSY_RDO:
                            CHECKED_SET_MAX_FLOAT(config->f_psy_rdo, hTabs[1], IDC_ANALYSIS_PSY_RDO, FALSE, 0.00, 1000.00, 2, "%.2f");
                            break;

                        case IDC_ANALYSIS_MIN_KEYINT:
                            CHECKED_SET_MAX_INT(config->i_keyint_min, hTabs[1], IDC_ANALYSIS_MIN_KEYINT, FALSE, 1, config->i_keyint_max);
                            break;

                        case IDC_ANALYSIS_KEYINT:
                            CHECKED_SET_MAX_INT(config->i_keyint_max, hTabs[1], IDC_ANALYSIS_KEYINT, FALSE, config->i_keyint_min, 9999);
                            break;

                        case IDC_ANALYSIS_SCENECUT:
                            CHECKED_SET_MAX_INT(config->i_scenecut_threshold, hTabs[1], IDC_ANALYSIS_SCENECUT, FALSE, 0, 100);
                            break;

                        case IDC_ANALYSIS_BFRAMES:
                            CHECKED_SET_MAX_INT(config->i_bframe, hTabs[1], IDC_ANALYSIS_BFRAMES, FALSE, 0, X264_BFRAME_MAX);
                            break;

                        case IDC_ANALYSIS_B_BIAS:
                            CHECKED_SET_INT(config->i_bframe_bias, hTabs[1], IDC_ANALYSIS_B_BIAS, TRUE, -90, 100);
                            break;

                        case IDC_ENC_DEBLOCK_A:
                            CHECKED_SET_INT(config->i_inloop_a, hTabs[1], IDC_ENC_DEBLOCK_A, TRUE, -6, 6);
                            break;

                        case IDC_ENC_DEBLOCK_B:
                            CHECKED_SET_INT(config->i_inloop_b, hTabs[1], IDC_ENC_DEBLOCK_B, TRUE, -6, 6);
                            break;

                        case IDC_ENC_NR:
                            CHECKED_SET_MAX_INT(config->i_noise_reduction, hTabs[1], IDC_ENC_NR, FALSE, 0, 9999);
                            break;

                        case IDC_ENC_DEADZONE_INTRA:
                            CHECKED_SET_MAX_INT(config->i_intra_deadzone, hTabs[1], IDC_ENC_DEADZONE_INTRA, FALSE, 0, 32);
                            break;

                        case IDC_ENC_DEADZONE_INTER:
                            CHECKED_SET_MAX_INT(config->i_inter_deadzone, hTabs[1], IDC_ENC_DEADZONE_INTER, FALSE, 0, 32);
                            break;

                        case IDC_ENC_CQMFILE:
                            if (GetDlgItemText(hTabs[1], IDC_ENC_CQMFILE, config->cqmfile, MAX_PATH) == 0)
                                strcpy(config->cqmfile, "");
                            break;

                        case IDC_ENC_PSY_TRELLIS:
                            CHECKED_SET_MAX_FLOAT(config->f_psy_trellis, hTabs[1], IDC_ENC_PSY_TRELLIS, FALSE, 0.00, 1000.00, 2, "%.2f");
                            break;

                        case IDC_RC_VBV_MAXRATE:
                            CHECKED_SET_MAX_INT(config->i_vbv_maxrate, hTabs[2], IDC_RC_VBV_MAXRATE, FALSE, 0, 999999);
                            break;

                        case IDC_RC_VBV_BUFSIZE:
                            CHECKED_SET_MAX_INT(config->i_vbv_bufsize, hTabs[2], IDC_RC_VBV_BUFSIZE, FALSE, 0, 999999);
                            break;

                        case IDC_RC_VBV_INIT:
                            CHECKED_SET_MAX_INT(config->i_vbv_occupancy, hTabs[2], IDC_RC_VBV_INIT, FALSE, 0, 100);
                            break;

                        case IDC_RC_QPMIN:
                            CHECKED_SET_MAX_INT(config->i_qp_min, hTabs[2], IDC_RC_QPMIN, FALSE, 1, config->i_qp_max);
                            break;

                        case IDC_RC_QPMAX:
                            CHECKED_SET_MAX_INT(config->i_qp_max, hTabs[2], IDC_RC_QPMAX, FALSE, config->i_qp_min, X264_QUANT_MAX);
                            break;

                        case IDC_RC_QPSTEP:
                            CHECKED_SET_MAX_INT(config->i_qp_step, hTabs[2], IDC_RC_QPSTEP, FALSE, 1, X264_QUANT_MAX);
                            break;

                        case IDC_RC_IPRATIO:
                            CHECKED_SET_MAX_FLOAT(config->f_ipratio, hTabs[2], IDC_RC_IPRATIO, FALSE, 1.00, 1000.00, 2, "%.2f");
                            break;

                        case IDC_RC_PBRATIO:
                            CHECKED_SET_MAX_FLOAT(config->f_pbratio, hTabs[2], IDC_RC_PBRATIO, FALSE, 1.00, 1000.00, 2, "%.2f");
                            break;

                        case IDC_RC_CHROMA_QP_OFFSET:
                            CHECKED_SET_INT(config->i_chroma_qp_offset, hTabs[2], IDC_RC_CHROMA_QP_OFFSET, TRUE, -12, 12);
                            break;

                        case IDC_RC_QCOMP:
                            CHECKED_SET_MAX_FLOAT(config->f_qcomp, hTabs[2], IDC_RC_QCOMP, FALSE, 0.00, 1000.00, 2, "%.2f");
                            break;

                        case IDC_RC_CPLXBLUR:
                            CHECKED_SET_MAX_FLOAT(config->f_cplxblur, hTabs[2], IDC_RC_CPLXBLUR, FALSE, 0.00, 1000.00, 2, "%.2f");
                            break;

                        case IDC_RC_QBLUR:
                            CHECKED_SET_MAX_FLOAT(config->f_qblur, hTabs[2], IDC_RC_QBLUR, FALSE, 0.00, 1000.00, 2, "%.2f");
                            break;

                        case IDC_RC_RATETOL:
                            CHECKED_SET_MAX_FLOAT(config->f_ratetol, hTabs[2], IDC_RC_RATETOL, FALSE, 0.01, 1000.00, 2, "%.2f");
                            break;

                        case IDC_AQ_STRENGTH:
                            CHECKED_SET_MAX_FLOAT(config->f_aq_strength, hTabs[2], IDC_AQ_STRENGTH, FALSE, 0.00, 1000.00, 2, "%.2f");
                            break;

#if X264_PATCH_VAQ_MOD
                        case IDC_AQ_SENSITIVITY:
                            CHECKED_SET_MAX_FLOAT(config->f_aq_sensitivity, hTabs[2], IDC_AQ_SENSITIVITY, FALSE, 0.00, 1000.00, 2, "%.2f");
                            break;
#endif

#if X264VFW_USE_THREADS
#if !X264_PATCH_THREAD_POOL
                        case IDC_MT_THREADS:
                            CHECKED_SET_MAX_INT(config->i_threads, hTabs[2], IDC_MT_THREADS, FALSE, 0, X264_THREAD_MAX);
                            break;
#else
                        case IDC_MT_THREADS:
                            if (config->i_thread_queue == 0)
                            {
                                CHECKED_SET_MAX_INT(config->i_threads, hTabs[2], IDC_MT_THREADS, FALSE, 0, X264_THREAD_MAX);
                            }
                            else
                            {
                                CHECKED_SET_MAX_INT(config->i_threads, hTabs[2], IDC_MT_THREADS, FALSE, 0, config->i_thread_queue);
                            }
                            break;

                        case IDC_MT_THREAD_QUEUE:
                            CheckControlTextIsNumber(GetDlgItem(hTabs[2], IDC_MT_THREAD_QUEUE), FALSE, 0);
                            config->i_thread_queue = GetDlgItemInt(hTabs[2], IDC_MT_THREAD_QUEUE, NULL, FALSE);
                            if (config->i_thread_queue < config->i_threads && config->i_thread_queue != 0)
                                config->i_thread_queue = 0;
                            else if (config->i_thread_queue > X264_THREAD_MAX)
                            {
                                config->i_thread_queue = X264_THREAD_MAX;
                                SetDlgItemInt(hTabs[2], IDC_MT_THREAD_QUEUE, config->i_thread_queue, FALSE);
                                SendMessage(GetDlgItem(hTabs[2], IDC_MT_THREAD_QUEUE), EM_SETSEL, -2, -2);
                            }
                            break;
#endif
#endif

                        case IDC_VUI_CHROMALOC:
                            CHECKED_SET_MAX_INT(config->i_chromaloc, hTabs[2], IDC_VUI_CHROMALOC, FALSE, 0, 5);
                            break;

                        default:
                            return FALSE;
                    }
                    break;

                case EN_KILLFOCUS:
                    switch (LOWORD(wParam))
                    {
                        case IDC_MAIN_RC_VAL:
                            switch (config->i_encoding_type)
                            {
                                case 0:
                                    break;

                                case 1:
                                    CHECKED_SET_SHOW_INT(config->i_qp, hTabs[0], IDC_MAIN_RC_VAL, FALSE, 1, X264_QUANT_MAX);
                                    SendDlgItemMessage(hTabs[0], IDC_MAIN_RC_VAL_SLIDER, TBM_SETPOS, TRUE, config->i_qp);
                                    break;

                                case 2:
                                    CheckControlTextIsNumber(GetDlgItem(hTabs[0], IDC_MAIN_RC_VAL), FALSE, 1);
                                    config->i_rf_constant = (int)(GetDlgItemDouble(hTabs[0], IDC_MAIN_RC_VAL) * 10);
                                    if (config->i_rf_constant < 10)
                                        config->i_rf_constant = 10;
                                    else if (config->i_rf_constant > X264_QUANT_MAX * 10)
                                        config->i_rf_constant = X264_QUANT_MAX * 10;
                                    SetDlgItemDouble(hTabs[0], IDC_MAIN_RC_VAL, config->i_rf_constant * 0.1, "%.1f");
                                    SendDlgItemMessage(hTabs[0], IDC_MAIN_RC_VAL_SLIDER, TBM_SETPOS, TRUE, config->i_rf_constant);
                                    break;

                                case 3:
                                case 4:
                                    CHECKED_SET_SHOW_INT(config->i_passbitrate, hTabs[0], IDC_MAIN_RC_VAL, FALSE, 1, X264_BITRATE_MAX);
                                    SendDlgItemMessage(hTabs[0], IDC_MAIN_RC_VAL_SLIDER, TBM_SETPOS, TRUE, config->i_passbitrate);
                                    break;

                                default:
                                    assert(0);
                                    break;
                            }
                            break;

                        case IDC_MAIN_STATS:
                            if (GetDlgItemText(hTabs[0], IDC_MAIN_STATS, config->stats, MAX_STATS_PATH) == 0)
                                strcpy(config->stats, ".\\x264.stats");
                            SetDlgItemText(hTabs[0], IDC_MAIN_STATS, config->stats);
                            break;

                        case IDC_MAIN_EXTRA_CMDLINE:
                            if (GetDlgItemText(hTabs[0], IDC_MAIN_EXTRA_CMDLINE, config->extra_cmdline, MAX_CMDLINE) == 0)
                                strcpy(config->extra_cmdline, "");
                            SetDlgItemText(hTabs[0], IDC_MAIN_EXTRA_CMDLINE, config->extra_cmdline);
                            break;

                        case IDC_MISC_SAR_W:
                            CHECKED_SET_SHOW_INT(config->i_sar_width, hTabs[0], IDC_MISC_SAR_W, FALSE, 1, 9999);
                            break;

                        case IDC_MISC_SAR_H:
                            CHECKED_SET_SHOW_INT(config->i_sar_height, hTabs[0], IDC_MISC_SAR_H, FALSE, 1, 9999);
                            break;

                        case IDC_ANALYSIS_REF:
                            CHECKED_SET_SHOW_INT(config->i_refmax, hTabs[1], IDC_ANALYSIS_REF, FALSE, 1, 16);
                            break;

                        case IDC_ANALYSIS_MERANGE:
                            CHECKED_SET_SHOW_INT(config->i_me_range, hTabs[1], IDC_ANALYSIS_MERANGE, FALSE, 4, 64);
                            break;

                        case IDC_ANALYSIS_PSY_RDO:
                            CHECKED_SET_SHOW_FLOAT(config->f_psy_rdo, hTabs[1], IDC_ANALYSIS_PSY_RDO, FALSE, 0.00, 1000.00, 2, "%.2f");
                            break;

                        case IDC_ANALYSIS_MIN_KEYINT:
                            CHECKED_SET_SHOW_INT(config->i_keyint_min, hTabs[1], IDC_ANALYSIS_MIN_KEYINT, FALSE, 1, config->i_keyint_max);
                            break;

                        case IDC_ANALYSIS_KEYINT:
                            CHECKED_SET_SHOW_INT(config->i_keyint_max, hTabs[1], IDC_ANALYSIS_KEYINT, FALSE, config->i_keyint_min, 9999);
                            break;

                        case IDC_ANALYSIS_SCENECUT:
                            CHECKED_SET_SHOW_INT(config->i_scenecut_threshold, hTabs[1], IDC_ANALYSIS_SCENECUT, FALSE, 0, 100);
                            break;

                        case IDC_ANALYSIS_BFRAMES:
                            CHECKED_SET_SHOW_INT(config->i_bframe, hTabs[1], IDC_ANALYSIS_BFRAMES, FALSE, 0, X264_BFRAME_MAX);
                            break;

                        case IDC_ANALYSIS_B_BIAS:
                            CHECKED_SET_SHOW_INT(config->i_bframe_bias, hTabs[1], IDC_ANALYSIS_B_BIAS, TRUE, -90, 100);
                            break;

                        case IDC_ENC_DEBLOCK_A:
                            CHECKED_SET_SHOW_INT(config->i_inloop_a, hTabs[1], IDC_ENC_DEBLOCK_A, TRUE, -6, 6);
                            break;

                        case IDC_ENC_DEBLOCK_B:
                            CHECKED_SET_SHOW_INT(config->i_inloop_b, hTabs[1], IDC_ENC_DEBLOCK_B, TRUE, -6, 6);
                            break;

                        case IDC_ENC_NR:
                            CHECKED_SET_SHOW_INT(config->i_noise_reduction, hTabs[1], IDC_ENC_NR, FALSE, 0, 9999);
                            break;

                        case IDC_ENC_DEADZONE_INTRA:
                            CHECKED_SET_SHOW_INT(config->i_intra_deadzone, hTabs[1], IDC_ENC_DEADZONE_INTRA, FALSE, 0, 32);
                            break;

                        case IDC_ENC_DEADZONE_INTER:
                            CHECKED_SET_SHOW_INT(config->i_inter_deadzone, hTabs[1], IDC_ENC_DEADZONE_INTER, FALSE, 0, 32);
                            break;

                        case IDC_ENC_CQMFILE:
                            if (GetDlgItemText(hTabs[1], IDC_ENC_CQMFILE, config->cqmfile, MAX_PATH) == 0)
                                strcpy(config->cqmfile, "");
                            SetDlgItemText(hTabs[1], IDC_ENC_CQMFILE, config->cqmfile);
                            break;

                        case IDC_ENC_PSY_TRELLIS:
                            CHECKED_SET_SHOW_FLOAT(config->f_psy_trellis, hTabs[1], IDC_ENC_PSY_TRELLIS, FALSE, 0.00, 1000.00, 2, "%.2f");
                            break;

                        case IDC_RC_VBV_MAXRATE:
                            CHECKED_SET_SHOW_INT(config->i_vbv_maxrate, hTabs[2], IDC_RC_VBV_MAXRATE, FALSE, 0, 999999);
                            break;

                        case IDC_RC_VBV_BUFSIZE:
                            CHECKED_SET_SHOW_INT(config->i_vbv_bufsize, hTabs[2], IDC_RC_VBV_BUFSIZE, FALSE, 0, 999999);
                            break;

                        case IDC_RC_VBV_INIT:
                            CHECKED_SET_SHOW_INT(config->i_vbv_occupancy, hTabs[2], IDC_RC_VBV_INIT, FALSE, 0, 100);
                            break;

                        case IDC_RC_QPMIN:
                            CHECKED_SET_SHOW_INT(config->i_qp_min, hTabs[2], IDC_RC_QPMIN, FALSE, 1, config->i_qp_max);
                            break;

                        case IDC_RC_QPMAX:
                            CHECKED_SET_SHOW_INT(config->i_qp_max, hTabs[2], IDC_RC_QPMAX, FALSE, config->i_qp_min, X264_QUANT_MAX);
                            break;

                        case IDC_RC_QPSTEP:
                            CHECKED_SET_SHOW_INT(config->i_qp_step, hTabs[2], IDC_RC_QPSTEP, FALSE, 1, X264_QUANT_MAX);
                            break;

                        case IDC_RC_IPRATIO:
                            CHECKED_SET_SHOW_FLOAT(config->f_ipratio, hTabs[2], IDC_RC_IPRATIO, FALSE, 1.00, 1000.00, 2, "%.2f");
                            break;

                        case IDC_RC_PBRATIO:
                            CHECKED_SET_SHOW_FLOAT(config->f_pbratio, hTabs[2], IDC_RC_PBRATIO, FALSE, 1.00, 1000.00, 2, "%.2f");
                            break;

                        case IDC_RC_CHROMA_QP_OFFSET:
                            CHECKED_SET_SHOW_INT(config->i_chroma_qp_offset, hTabs[2], IDC_RC_CHROMA_QP_OFFSET, TRUE, -12, 12);
                            break;

                        case IDC_RC_QCOMP:
                            CHECKED_SET_SHOW_FLOAT(config->f_qcomp, hTabs[2], IDC_RC_QCOMP, FALSE, 0.00, 1000.00, 2, "%.2f");
                            break;

                        case IDC_RC_CPLXBLUR:
                            CHECKED_SET_SHOW_FLOAT(config->f_cplxblur, hTabs[2], IDC_RC_CPLXBLUR, FALSE, 0.00, 1000.00, 2, "%.2f");
                            break;

                        case IDC_RC_QBLUR:
                            CHECKED_SET_SHOW_FLOAT(config->f_qblur, hTabs[2], IDC_RC_QBLUR, FALSE, 0.00, 1000.00, 2, "%.2f");
                            break;

                        case IDC_RC_RATETOL:
                            CHECKED_SET_SHOW_FLOAT(config->f_ratetol, hTabs[2], IDC_RC_RATETOL, FALSE, 0.01, 1000.00, 2, "%.2f");
                            break;

                        case IDC_AQ_STRENGTH:
                            CHECKED_SET_SHOW_FLOAT(config->f_aq_strength, hTabs[2], IDC_AQ_STRENGTH, FALSE, 0.00, 1000.00, 2, "%.2f");
                            break;

#if X264_PATCH_VAQ_MOD
                        case IDC_AQ_SENSITIVITY:
                            CHECKED_SET_SHOW_FLOAT(config->f_aq_sensitivity, hTabs[2], IDC_AQ_SENSITIVITY, FALSE, 0.00, 1000.00, 2, "%.2f");
                            break;
#endif

#if X264VFW_USE_THREADS
#if !X264_PATCH_THREAD_POOL
                        case IDC_MT_THREADS:
                            CHECKED_SET_SHOW_INT(config->i_threads, hTabs[2], IDC_MT_THREADS, FALSE, 0, X264_THREAD_MAX);
                            break;
#else
                        case IDC_MT_THREADS:
                            if (config->i_thread_queue == 0)
                            {
                                CHECKED_SET_SHOW_INT(config->i_threads, hTabs[2], IDC_MT_THREADS, FALSE, 0, X264_THREAD_MAX);
                            }
                            else
                            {
                                CHECKED_SET_SHOW_INT(config->i_threads, hTabs[2], IDC_MT_THREADS, FALSE, 0, config->i_thread_queue);
                            }
                            break;

                        case IDC_MT_THREAD_QUEUE:
                            CheckControlTextIsNumber(GetDlgItem(hTabs[2], IDC_MT_THREAD_QUEUE), FALSE, 0);
                            config->i_thread_queue = GetDlgItemInt(hTabs[2], IDC_MT_THREAD_QUEUE, NULL, FALSE);
                            if (config->i_thread_queue < config->i_threads && config->i_thread_queue != 0)
                                config->i_thread_queue = config->i_threads;
                            else if (config->i_thread_queue > X264_THREAD_MAX)
                                config->i_thread_queue = X264_THREAD_MAX;
                            SetDlgItemInt(hTabs[2], IDC_MT_THREAD_QUEUE, config->i_thread_queue, FALSE);
                            break;
#endif
#endif

                        case IDC_VUI_CHROMALOC:
                            CHECKED_SET_SHOW_INT(config->i_chromaloc, hTabs[2], IDC_VUI_CHROMALOC, FALSE, 0, 5);
                            break;

                        default:
                            return FALSE;
                    }
                    break;

                default:
                    return FALSE;
            }
            break;

        case WM_NOTIFY:
        {
            LPNMUPDOWN lpnmud = (LPNMUPDOWN)lParam;

            if (!lpnmud)
                return FALSE;
            if (lpnmud->hdr.code != UDN_DELTAPOS)
                return FALSE;

            switch (lpnmud->hdr.idFrom)
            {
                case IDC_ANALYSIS_REF_SPIN:
                    config->i_refmax = X264_CLIP(config->i_refmax - lpnmud->iDelta, 1, 16);
                    SetDlgItemInt(hTabs[1], IDC_ANALYSIS_REF, config->i_refmax, FALSE);
                    break;

                case IDC_ANALYSIS_MERANGE_SPIN:
                    config->i_me_range = X264_CLIP(config->i_me_range - lpnmud->iDelta, 4, 64);
                    SetDlgItemInt(hTabs[1], IDC_ANALYSIS_MERANGE, config->i_me_range, FALSE);
                    break;

                case IDC_ANALYSIS_SCENECUT_SPIN:
                    config->i_scenecut_threshold = X264_CLIP(config->i_scenecut_threshold - lpnmud->iDelta, 0, 100);
                    SetDlgItemInt(hTabs[1], IDC_ANALYSIS_SCENECUT, config->i_scenecut_threshold, FALSE);
                    break;

                case IDC_ANALYSIS_BFRAMES_SPIN:
                    config->i_bframe = X264_CLIP(config->i_bframe - lpnmud->iDelta, 0, X264_BFRAME_MAX);
                    SetDlgItemInt(hTabs[1], IDC_ANALYSIS_BFRAMES, config->i_bframe, FALSE);
                    break;

                case IDC_ANALYSIS_B_BIAS_SPIN:
                    config->i_bframe_bias = X264_CLIP(config->i_bframe_bias - lpnmud->iDelta, -90, 100);
                    SetDlgItemInt(hTabs[1], IDC_ANALYSIS_B_BIAS, config->i_bframe_bias, TRUE);
                    break;

                case IDC_ENC_DEBLOCK_A_SPIN:
                    config->i_inloop_a = X264_CLIP(config->i_inloop_a - lpnmud->iDelta, -6, 6);
                    SetDlgItemInt(hTabs[1], IDC_ENC_DEBLOCK_A, config->i_inloop_a, TRUE);
                    break;

                case IDC_ENC_DEBLOCK_B_SPIN:
                    config->i_inloop_b = X264_CLIP(config->i_inloop_b - lpnmud->iDelta, -6, 6);
                    SetDlgItemInt(hTabs[1], IDC_ENC_DEBLOCK_B, config->i_inloop_b, TRUE);
                    break;

                case IDC_ENC_DEADZONE_INTRA_SPIN:
                    config->i_intra_deadzone = X264_CLIP(config->i_intra_deadzone - lpnmud->iDelta, 0, 32);
                    SetDlgItemInt(hTabs[1], IDC_ENC_DEADZONE_INTRA, config->i_intra_deadzone, FALSE);
                    break;

                case IDC_ENC_DEADZONE_INTER_SPIN:
                    config->i_inter_deadzone = X264_CLIP(config->i_inter_deadzone - lpnmud->iDelta, 0, 32);
                    SetDlgItemInt(hTabs[1], IDC_ENC_DEADZONE_INTER, config->i_inter_deadzone, FALSE);
                    break;

                case IDC_RC_VBV_INIT_SPIN:
                    config->i_vbv_occupancy = X264_CLIP(config->i_vbv_occupancy - lpnmud->iDelta, 0, 100);
                    SetDlgItemInt(hTabs[2], IDC_RC_VBV_INIT, config->i_vbv_occupancy, FALSE);
                    break;

                case IDC_RC_QPMIN_SPIN:
                    config->i_qp_min = X264_CLIP(config->i_qp_min - lpnmud->iDelta, 1, config->i_qp_max);
                    SetDlgItemInt(hTabs[2], IDC_RC_QPMIN, config->i_qp_min, FALSE);
                    break;

                case IDC_RC_QPMAX_SPIN:
                    config->i_qp_max = X264_CLIP(config->i_qp_max - lpnmud->iDelta, config->i_qp_min, X264_QUANT_MAX);
                    SetDlgItemInt(hTabs[2], IDC_RC_QPMAX, config->i_qp_max, FALSE);
                    break;

                case IDC_RC_QPSTEP_SPIN:
                    config->i_qp_step = X264_CLIP(config->i_qp_step - lpnmud->iDelta, 1, X264_QUANT_MAX);
                    SetDlgItemInt(hTabs[2], IDC_RC_QPSTEP, config->i_qp_step, FALSE);
                    break;

                case IDC_RC_CHROMA_QP_OFFSET_SPIN:
                    config->i_chroma_qp_offset = X264_CLIP(config->i_chroma_qp_offset - lpnmud->iDelta, -12, 12);
                    SetDlgItemInt(hTabs[2], IDC_RC_CHROMA_QP_OFFSET, config->i_chroma_qp_offset, TRUE);
                    break;

#if X264VFW_USE_THREADS
#if !X264_PATCH_THREAD_POOL
                case IDC_MT_THREADS_SPIN:
                    config->i_threads = X264_CLIP(config->i_threads - lpnmud->iDelta, 0, X264_THREAD_MAX);
                    SetDlgItemInt(hTabs[2], IDC_MT_THREADS, config->i_threads, FALSE);
                    break;
#else
                case IDC_MT_THREADS_SPIN:
                    if (config->i_thread_queue == 0)
                        config->i_threads = X264_CLIP(config->i_threads - lpnmud->iDelta, 0, X264_THREAD_MAX);
                    else
                        config->i_threads = X264_CLIP(config->i_threads - lpnmud->iDelta, 0, config->i_thread_queue);
                    SetDlgItemInt(hTabs[2], IDC_MT_THREADS, config->i_threads, FALSE);
                    break;

                case IDC_MT_THREAD_QUEUE_SPIN:
                    config->i_thread_queue = X264_CLIP(config->i_thread_queue - lpnmud->iDelta, 0, X264_THREAD_MAX);
                    if (config->i_thread_queue < config->i_threads && config->i_thread_queue != 0)
                        config->i_thread_queue = config->i_threads;
                    SetDlgItemInt(hTabs[2], IDC_MT_THREAD_QUEUE, config->i_thread_queue, FALSE);
                    break;
#endif
#endif

                case IDC_VUI_CHROMALOC_SPIN:
                    config->i_chromaloc = X264_CLIP(config->i_chromaloc - lpnmud->iDelta, 0, 5);
                    SetDlgItemInt(hTabs[2], IDC_VUI_CHROMALOC, config->i_chromaloc, FALSE);
                    break;

                default:
                    return FALSE;
            }
            break;
        }

        case WM_HSCROLL:
            if ((HWND)lParam == GetDlgItem(hTabs[0], IDC_MAIN_RC_VAL_SLIDER))
            {
                switch (config->i_encoding_type)
                {
                    case 0:
                        break;

                    case 1:
                        config->i_qp = SendDlgItemMessage(hTabs[0], IDC_MAIN_RC_VAL_SLIDER, TBM_GETPOS, 0, 0);
                        SetDlgItemInt(hTabs[0], IDC_MAIN_RC_VAL, config->i_qp, FALSE);
                        break;

                    case 2:
                        config->i_rf_constant = SendDlgItemMessage(hTabs[0], IDC_MAIN_RC_VAL_SLIDER, TBM_GETPOS, 0, 0);
                        SetDlgItemDouble(hTabs[0], IDC_MAIN_RC_VAL, config->i_rf_constant * 0.1, "%.1f");
                        break;

                    case 3:
                    case 4:
                        config->i_passbitrate = SendDlgItemMessage(hTabs[0], IDC_MAIN_RC_VAL_SLIDER, TBM_GETPOS, 0, 0);
                        SetDlgItemInt(hTabs[0], IDC_MAIN_RC_VAL, config->i_passbitrate, FALSE);
                        break;

                    default:
                        assert(0);
                        break;
                }
            }
            else
                return FALSE;
            break;

        default:
            return FALSE;
    }

    tabs_enable_items(config);
    return TRUE;
}

/* About box */
INT_PTR CALLBACK callback_about(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
        case WM_INITDIALOG:
        {
            char temp[1024];

            sprintf(temp, "Build date: %s %s\nlibx264 core %d%s", __DATE__, __TIME__, X264_BUILD, X264_VERSION);
            SetDlgItemText(hDlg, IDC_ABOUT_BUILD_LABEL, temp);
            break;
        }

        case WM_COMMAND:
            switch (HIWORD(wParam))
            {
                case BN_CLICKED:
                    if (LOWORD(wParam) == IDC_ABOUT_HOMEPAGE)
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
INT_PTR CALLBACK callback_err_console(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
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
                    case IDCANCEL:
                        DestroyWindow(hWnd);
                        break;

                    case IDC_ERRCONSOLE_COPYCLIP:
                        if (OpenClipboard(hWnd))
                        {
                            int i;
                            int num_lines = SendDlgItemMessage(hWnd, IDC_ERRCONSOLE_CONSOLE, LB_GETCOUNT, 0, 0);
                            int text_size;
                            char *buffer;
                            HGLOBAL clipbuffer;

                            if (num_lines <= 0)
                                break;

                            /* Calculate text size */
                            for (i = 0, text_size = 0; i < num_lines; i++)
                                text_size += SendDlgItemMessage(hWnd, IDC_ERRCONSOLE_CONSOLE, LB_GETTEXTLEN, (WPARAM)i, 0);

                            /* CR - LF for each line + terminating NULL */
                            text_size += 2 * num_lines + 1;

                            EmptyClipboard();
                            clipbuffer = GlobalAlloc(GMEM_MOVEABLE | GMEM_DDESHARE, text_size);
                            buffer = (char *)GlobalLock(clipbuffer);

                            /* Concatenate lines of text in the global buffer */
                            for (i = 0; i < num_lines; i++)
                            {
                                char msg_buf[1024];

                                SendDlgItemMessage(hWnd, IDC_ERRCONSOLE_CONSOLE, LB_GETTEXT, (WPARAM)i, (LPARAM)&msg_buf);
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
