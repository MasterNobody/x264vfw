/*****************************************************************************
 * config.c: vfw x264 encoder
 *****************************************************************************
 * Copyright (C) 2003 Laurent Aimar
 * $Id: config.c,v 1.1 2004/06/03 19:27:09 fenrir Exp $
 *
 * Authors: Justin Clay
 *          Laurent Aimar <fenrir@via.ecp.fr>
 *          Antony Boucher <proximodo@free.fr>
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
#include <commctrl.h>

/* Registry */
#define X264VFW_REG_KEY    HKEY_CURRENT_USER
#define X264VFW_REG_PARENT "Software\\GNU"
#ifdef _WIN64
#define X264VFW_REG_CHILD  "x264vfw64"
#else
/* Not "x264vfw" because of GordianKnot compatibility */
#define X264VFW_REG_CHILD  "x264"
#endif
#define X264VFW_REG_CLASS  "config"

/* Description */
#define X264VFW_NAME       "x264vfw"
#define X264VFW_DEF_TEXT   "Are you sure you want to load default values?"

/* Registery handling */
typedef struct
{
    const char * const reg_value;
    int * const config_int;
    const char * const default_str;
    const named_str_t * const list;
    const int list_count;
} reg_named_str_t;

typedef struct
{
    const char * const reg_value;
    int * const config_int;
    const int default_int;
    const int min_int;
    const int max_int;
} reg_int_t;

typedef struct
{
    const char * const reg_value;
    char * const config_str;
    const char * const default_str;
    const int max_len; /* Maximum string length, including the terminating NULL char */
} reg_str_t;

CONFIG reg;

extern const named_str_t preset_table[COUNT_PRESET];

extern const named_str_t tune_table[COUNT_TUNE];

extern const named_str_t profile_table[COUNT_PROFILE];

extern const named_int_t level_table[COUNT_LEVEL];

extern const named_fourcc_t fourcc_table[COUNT_FOURCC];

extern const char * const muxer_names[];

static const reg_named_str_t reg_named_str_table[] =
{
    /* Basic */
    { "preset",  &reg.i_preset,  "medium", preset_table,  COUNT_PRESET  },
    { "tuning",  &reg.i_tuning,  "",       tune_table,    COUNT_TUNE    },
    { "profile", &reg.i_profile, "",       profile_table, COUNT_PROFILE }
};

static const reg_int_t reg_int_table[] =
{
    /* Basic */
    { "avc_level",       &reg.i_level,           0,   0,  COUNT_LEVEL - 1  },
    { "fastdecode",      &reg.b_fastdecode,      0,   0,  1                },
    { "zerolatency",     &reg.b_zerolatency,     0,   0,  1                },
    /* Rate control */
    { "encoding_type",   &reg.i_encoding_type,   4,   0,  4                }, /* Take into account GordianKnot workaround */
    { "quantizer",       &reg.i_qp,              23,  1,  MAX_QUANT        },
    { "ratefactor",      &reg.i_rf_constant,     230, 10, MAX_QUANT * 10   },
    { "passbitrate",     &reg.i_passbitrate,     800, 1,  MAX_BITRATE      },
    { "pass_number",     &reg.i_pass,            1,   1,  2                },
    { "fast1pass",       &reg.b_fast1pass,       0,   0,  1                },
    { "createstats",     &reg.b_createstats,     0,   0,  1                },
    { "updatestats",     &reg.b_updatestats,     1,   0,  1                },
    /* Output */
    { "output_mode",     &reg.i_output_mode,     0,   0,  1                },
    { "fourcc_num",      &reg.i_fourcc,          0,   0,  COUNT_FOURCC - 1 },
#if X264VFW_USE_VIRTUALDUB_HACK
    { "vd_hack",         &reg.b_vd_hack,         0,   0,  1                },
#endif
    /* Sample Aspect Ratio */
    { "sar_width",       &reg.i_sar_width,       1,   1,  9999             },
    { "sar_height",      &reg.i_sar_height,      1,   1,  9999             },
    /* Debug */
    { "log_level",       &reg.i_log_level,       2,   0,  4                },
    { "psnr",            &reg.b_psnr,            1,   0,  1                },
    { "ssim",            &reg.b_ssim,            1,   0,  1                },
    { "no_asm",          &reg.b_no_asm,          0,   0,  1                },
    /* Decoder && AVI Muxer */
#if defined(HAVE_FFMPEG) && X264VFW_USE_DECODER
    { "disable_decoder", &reg.b_disable_decoder, 0,   0,  1                }
#endif
};

static const reg_str_t reg_str_table[] =
{
    /* Rate control */
    { "statsfile",     reg.stats,         ".\\x264.stats", MAX_STATS_PATH  },
    /* Output */
    { "output_file",   reg.output_file,   "",              MAX_OUTPUT_PATH },
    /* Extra command line */
    { "extra_cmdline", reg.extra_cmdline, "",              MAX_CMDLINE     }
};

static double GetDlgItemDouble(HWND hDlg, int nIDDlgItem)
{
    char temp[1024];

    if (GetDlgItemText(hDlg, nIDDlgItem, temp, 1024) == 0)
        strcpy(temp, "");
    return atof(temp);
}

static void SetDlgItemDouble(HWND hDlg, int nIDDlgItem, double dblValue, const char *format)
{
    char temp[1024];

    sprintf(temp, format, dblValue);
    SetDlgItemText(hDlg, nIDDlgItem, temp);
}

static int pos2scale(int i_pos)
{
    int res;
    if (i_pos <= 100)
        res = i_pos;
    else
    {
        int i = 1;
        i_pos -= 10;
        while (i_pos > 90)
        {
            i_pos -= 90;
            i *= 10;
        }
        res = (i_pos + 10) * i;
    }
    return res;
}

static int scale2pos(int i_scale)
{
    int res;
    if (i_scale <= 100)
        res = i_scale;
    else
    {
        int i = 900;
        res = 100;
        i_scale -= 100;
        while (i_scale > i)
        {
            res += 90;
            i_scale -= i;
            i *= 10;
        }
        i /= 90;
        res += (i_scale + (i >> 1)) / i;
    }
    return res;
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

void config_defaults(CONFIG *config)
{
    int     i;

    EnterCriticalSection(&g_CS);
    memset(&reg, 0, sizeof(CONFIG));

    /* Save all named params */
    for (i = 0; i < sizeof(reg_named_str_table) / sizeof(reg_named_str_t); i++)
    {
        int j;
        for (j = 0; j < reg_named_str_table[i].list_count; j++)
            if (!strcasecmp(reg_named_str_table[i].default_str, reg_named_str_table[i].list[j].value))
                *reg_named_str_table[i].config_int = j;
    }

    /* Set all integers */
    for (i = 0; i < sizeof(reg_int_table) / sizeof(reg_int_t); i++)
        *reg_int_table[i].config_int = reg_int_table[i].default_int;
    for (i = 0; i < sizeof(reg_int_table) / sizeof(reg_int_t); i++)
        *reg_int_table[i].config_int = X264_CLIP(*reg_int_table[i].config_int, reg_int_table[i].min_int, reg_int_table[i].max_int);

    /* Set all strings */
    for (i = 0; i < sizeof(reg_str_table) / sizeof(reg_str_t); i++)
        strcpy(reg_str_table[i].config_str, reg_str_table[i].default_str);

    GordianKnotWorkaround(reg.i_encoding_type);
    memcpy(config, &reg, sizeof(CONFIG));
    LeaveCriticalSection(&g_CS);
}

void config_reg_load(CONFIG *config)
{
    HKEY    hKey;
    DWORD   i_size;
    int     i;

    if (RegOpenKeyEx(X264VFW_REG_KEY, X264VFW_REG_PARENT "\\" X264VFW_REG_CHILD, 0, KEY_READ, &hKey) != ERROR_SUCCESS)
    {
        config_defaults(config);
        return;
    }
    EnterCriticalSection(&g_CS);
    memset(&reg, 0, sizeof(CONFIG));

    /* Read all named params */
    for (i = 0; i < sizeof(reg_named_str_table) / sizeof(reg_named_str_t); i++)
    {
        char temp[1024];
        int j;
        for (j = 0; j < reg_named_str_table[i].list_count; j++)
            if (!strcasecmp(reg_named_str_table[i].default_str, reg_named_str_table[i].list[j].value))
                *reg_named_str_table[i].config_int = j;
        i_size = 1024;
        if (RegQueryValueEx(hKey, reg_named_str_table[i].reg_value, 0, 0, (LPBYTE)temp, &i_size) == ERROR_SUCCESS)
        {
            for (j = 0; j < reg_named_str_table[i].list_count; j++)
                if (!strcasecmp(temp, reg_named_str_table[i].list[j].value))
                    *reg_named_str_table[i].config_int = j;
        }
    }

    /* Read all integers */
    for (i = 0; i < sizeof(reg_int_table) / sizeof(reg_int_t); i++)
    {
        i_size = sizeof(int);
        if (RegQueryValueEx(hKey, reg_int_table[i].reg_value, 0, 0, (LPBYTE)reg_int_table[i].config_int, &i_size) != ERROR_SUCCESS)
            *reg_int_table[i].config_int = reg_int_table[i].default_int;
    }
    for (i = 0; i < sizeof(reg_int_table) / sizeof(reg_int_t); i++)
        *reg_int_table[i].config_int = X264_CLIP(*reg_int_table[i].config_int, reg_int_table[i].min_int, reg_int_table[i].max_int);

    /* Read all strings */
    for (i = 0; i < sizeof(reg_str_table) / sizeof(reg_str_t); i++)
    {
        i_size = reg_str_table[i].max_len;
        if (RegQueryValueEx(hKey, reg_str_table[i].reg_value, 0, 0, (LPBYTE)reg_str_table[i].config_str, &i_size) != ERROR_SUCCESS)
            strcpy(reg_str_table[i].config_str, reg_str_table[i].default_str);
    }

    GordianKnotWorkaround(reg.i_encoding_type);
    memcpy(config, &reg, sizeof(CONFIG));
    LeaveCriticalSection(&g_CS);
    RegCloseKey(hKey);
}

void config_reg_save(CONFIG *config)
{
    HKEY    hKey;
    DWORD   dwDisposition;
    int     i;

    if (RegCreateKeyEx(X264VFW_REG_KEY, X264VFW_REG_PARENT "\\" X264VFW_REG_CHILD, 0, X264VFW_REG_CLASS, REG_OPTION_NON_VOLATILE, KEY_WRITE, 0, &hKey, &dwDisposition) != ERROR_SUCCESS)
        return;
    EnterCriticalSection(&g_CS);
    memcpy(&reg, config, sizeof(CONFIG));
    GordianKnotWorkaround(reg.i_encoding_type);

    /* Save all named params */
    for (i = 0; i < sizeof(reg_named_str_table) / sizeof(reg_named_str_t); i++)
    {
        const char *temp = *reg_named_str_table[i].config_int >= 0 && *reg_named_str_table[i].config_int < reg_named_str_table[i].list_count
                           ? reg_named_str_table[i].list[*reg_named_str_table[i].config_int].value
                           : "";
        RegSetValueEx(hKey, reg_named_str_table[i].reg_value, 0, REG_SZ, (LPBYTE)temp, strlen(temp) + 1);
    }

    /* Save all integers */
    for (i = 0; i < sizeof(reg_int_table) / sizeof(reg_int_t); i++)
        RegSetValueEx(hKey, reg_int_table[i].reg_value, 0, REG_DWORD, (LPBYTE)reg_int_table[i].config_int, sizeof(int));

    /* Save all strings */
    for (i = 0; i < sizeof(reg_str_table) / sizeof(reg_str_t); i++)
        RegSetValueEx(hKey, reg_str_table[i].reg_value, 0, REG_SZ, (LPBYTE)reg_str_table[i].config_str, strlen(reg_str_table[i].config_str) + 1);

    LeaveCriticalSection(&g_CS);
    RegCloseKey(hKey);
}

static void dlg_enable_items(CONFIG_DATA *cfg_data)
{
    CONFIG *config = &cfg_data->config;
    HWND hMainDlg = cfg_data->hMainDlg;
    /* Rate control */
    ShowWindow(GetDlgItem(hMainDlg, IDC_RC_LABEL), config->i_encoding_type > 0);
    ShowWindow(GetDlgItem(hMainDlg, IDC_RC_VAL), config->i_encoding_type > 0);
    ShowWindow(GetDlgItem(hMainDlg, IDC_RC_VAL_SLIDER), config->i_encoding_type > 0);
    ShowWindow(GetDlgItem(hMainDlg, IDC_RC_LOW_LABEL), config->i_encoding_type > 0);
    ShowWindow(GetDlgItem(hMainDlg, IDC_RC_HIGH_LABEL), config->i_encoding_type > 0);
    ShowWindow(GetDlgItem(hMainDlg, IDC_STATS_CREATE), config->i_encoding_type != 4);
    ShowWindow(GetDlgItem(hMainDlg, IDC_STATS_UPDATE), config->i_encoding_type == 4 && config->i_pass > 1);
    EnableWindow(GetDlgItem(hMainDlg, IDC_STATS_FILE), config->i_encoding_type == 4 || config->b_createstats);
    EnableWindow(GetDlgItem(hMainDlg, IDC_STATS_BROWSE), config->i_encoding_type == 4 || config->b_createstats);
    /* Output */
    EnableWindow(GetDlgItem(hMainDlg, IDC_OUTPUT_FILE), config->i_output_mode == 1);
    EnableWindow(GetDlgItem(hMainDlg, IDC_OUTPUT_BROWSE), config->i_output_mode == 1);
    /* Debug */
    EnableWindow(GetDlgItem(hMainDlg, IDC_PSNR), config->i_encoding_type > 0 && config->i_log_level >= 3);
    EnableWindow(GetDlgItem(hMainDlg, IDC_SSIM), config->i_encoding_type > 0 && config->i_log_level >= 3);
}

static void dlg_update_items(CONFIG_DATA *cfg_data)
{
    CONFIG *config = &cfg_data->config;
    HWND hMainDlg = cfg_data->hMainDlg;
    char temp[1024];

    /* Basic */
    if (SendMessage(GetDlgItem(hMainDlg, IDC_PRESET), CB_GETCOUNT, 0, 0) == 0)
    {
        int i;
        for (i = 0; i < COUNT_PRESET; i++)
            SendDlgItemMessage(hMainDlg, IDC_PRESET, CB_ADDSTRING, 0, (LPARAM)preset_table[i].name);
    }
    SendDlgItemMessage(hMainDlg, IDC_PRESET, CB_SETCURSEL, config->i_preset, 0);
    if (SendMessage(GetDlgItem(hMainDlg, IDC_TUNING), CB_GETCOUNT, 0, 0) == 0)
    {
        int i;
        for (i = 0; i < COUNT_TUNE; i++)
            SendDlgItemMessage(hMainDlg, IDC_TUNING, CB_ADDSTRING, 0, (LPARAM)tune_table[i].name);
    }
    SendDlgItemMessage(hMainDlg, IDC_TUNING, CB_SETCURSEL, config->i_tuning, 0);
    if (SendMessage(GetDlgItem(hMainDlg, IDC_PROFILE), CB_GETCOUNT, 0, 0) == 0)
    {
        int i;
        for (i = 0; i < COUNT_PROFILE; i++)
            SendDlgItemMessage(hMainDlg, IDC_PROFILE, CB_ADDSTRING, 0, (LPARAM)profile_table[i].name);
    }
    SendDlgItemMessage(hMainDlg, IDC_PROFILE, CB_SETCURSEL, config->i_profile, 0);
    if (SendMessage(GetDlgItem(hMainDlg, IDC_LEVEL), CB_GETCOUNT, 0, 0) == 0)
    {
        int i;
        for (i = 0; i < COUNT_LEVEL; i++)
            SendDlgItemMessage(hMainDlg, IDC_LEVEL, CB_ADDSTRING, 0, (LPARAM)level_table[i].name);
    }
    SendDlgItemMessage(hMainDlg, IDC_LEVEL, CB_SETCURSEL, config->i_level, 0);
    CheckDlgButton(hMainDlg, IDC_TUNE_FASTDECODE, config->b_fastdecode);
    CheckDlgButton(hMainDlg, IDC_TUNE_ZEROLATENCY, config->b_zerolatency);
    /* Rate control */
    if (SendMessage(GetDlgItem(hMainDlg, IDC_RC_MODE), CB_GETCOUNT, 0, 0) == 0)
    {
        SendDlgItemMessage(hMainDlg, IDC_RC_MODE, CB_ADDSTRING, 0, (LPARAM)"Single pass - lossless");
        SendDlgItemMessage(hMainDlg, IDC_RC_MODE, CB_ADDSTRING, 0, (LPARAM)"Single pass - quantizer-based (CQP)");
        SendDlgItemMessage(hMainDlg, IDC_RC_MODE, CB_ADDSTRING, 0, (LPARAM)"Single pass - ratefactor-based (CRF)");
        SendDlgItemMessage(hMainDlg, IDC_RC_MODE, CB_ADDSTRING, 0, (LPARAM)"Single pass - bitrate-based (ABR)");
        SendDlgItemMessage(hMainDlg, IDC_RC_MODE, CB_ADDSTRING, 0, (LPARAM)"Multipass - 1st pass");
        SendDlgItemMessage(hMainDlg, IDC_RC_MODE, CB_ADDSTRING, 0, (LPARAM)"Multipass - 1st pass (fast)");
        SendDlgItemMessage(hMainDlg, IDC_RC_MODE, CB_ADDSTRING, 0, (LPARAM)"Multipass - Nth pass");
    }
    SendMessage(GetDlgItem(hMainDlg, IDC_RC_VAL), EM_LIMITTEXT, 8, 0);
    switch (config->i_encoding_type)
    {
        case 0: /* 1 pass, lossless */
            SendDlgItemMessage(hMainDlg, IDC_RC_MODE, CB_SETCURSEL, 0, 0);
            SetDlgItemText(hMainDlg, IDC_RC_LABEL, "");
            SetDlgItemText(hMainDlg, IDC_RC_LOW_LABEL, "");
            SetDlgItemText(hMainDlg, IDC_RC_HIGH_LABEL, "");
            SetDlgItemInt(hMainDlg, IDC_RC_VAL, 0, FALSE);
            SendDlgItemMessage(hMainDlg, IDC_RC_VAL_SLIDER, TBM_SETRANGEMIN, TRUE, 0);
            SendDlgItemMessage(hMainDlg, IDC_RC_VAL_SLIDER, TBM_SETRANGEMAX, TRUE, 0);
            SendDlgItemMessage(hMainDlg, IDC_RC_VAL_SLIDER, TBM_SETPOS, TRUE, 0);
            break;

        case 1: /* 1 pass, quantizer-based (CQP) */
            SendDlgItemMessage(hMainDlg, IDC_RC_MODE, CB_SETCURSEL, 1, 0);
            SetDlgItemText(hMainDlg, IDC_RC_LABEL, "Quantizer");
            SetDlgItemText(hMainDlg, IDC_RC_LOW_LABEL, "1 (High quality)");
            sprintf(temp, "(Low quality) %d", MAX_QUANT);
            SetDlgItemText(hMainDlg, IDC_RC_HIGH_LABEL, temp);
            SetDlgItemInt(hMainDlg, IDC_RC_VAL, config->i_qp, FALSE);
            SendDlgItemMessage(hMainDlg, IDC_RC_VAL_SLIDER, TBM_SETRANGEMIN, TRUE, 1);
            SendDlgItemMessage(hMainDlg, IDC_RC_VAL_SLIDER, TBM_SETRANGEMAX, TRUE, MAX_QUANT);
            SendDlgItemMessage(hMainDlg, IDC_RC_VAL_SLIDER, TBM_SETPOS, TRUE, config->i_qp);
            break;

        case 2: /* 1 pass, ratefactor-based (CRF) */
            SendDlgItemMessage(hMainDlg, IDC_RC_MODE, CB_SETCURSEL, 2, 0);
            SetDlgItemText(hMainDlg, IDC_RC_LABEL, "Ratefactor");
            SetDlgItemText(hMainDlg, IDC_RC_LOW_LABEL, "1.0 (High quality)");
            sprintf(temp, "(Low quality) %d.0", MAX_QUANT);
            SetDlgItemText(hMainDlg, IDC_RC_HIGH_LABEL, temp);
            SetDlgItemDouble(hMainDlg, IDC_RC_VAL, config->i_rf_constant * 0.1, "%.1f");
            SendDlgItemMessage(hMainDlg, IDC_RC_VAL_SLIDER, TBM_SETRANGEMIN, TRUE, 10);
            SendDlgItemMessage(hMainDlg, IDC_RC_VAL_SLIDER, TBM_SETRANGEMAX, TRUE, MAX_QUANT * 10);
            SendDlgItemMessage(hMainDlg, IDC_RC_VAL_SLIDER, TBM_SETPOS, TRUE, config->i_rf_constant);
            break;

        case 3: /* 1 pass, bitrate-based (ABR) */
            SendDlgItemMessage(hMainDlg, IDC_RC_MODE, CB_SETCURSEL, 3, 0);
            SetDlgItemText(hMainDlg, IDC_RC_LABEL, "Average bitrate (kbit/s)");
            SetDlgItemText(hMainDlg, IDC_RC_LOW_LABEL, "1");
            sprintf(temp, "%d", MAX_BITRATE);
            SetDlgItemText(hMainDlg, IDC_RC_HIGH_LABEL, temp);
            SetDlgItemInt(hMainDlg, IDC_RC_VAL, config->i_passbitrate, FALSE);
            SendDlgItemMessage(hMainDlg, IDC_RC_VAL_SLIDER, TBM_SETRANGEMIN, TRUE, 1);
            SendDlgItemMessage(hMainDlg, IDC_RC_VAL_SLIDER, TBM_SETRANGEMAX, TRUE, scale2pos(MAX_BITRATE));
            SendDlgItemMessage(hMainDlg, IDC_RC_VAL_SLIDER, TBM_SETPOS, TRUE, scale2pos(config->i_passbitrate));
            break;

        case 4: /* 2 pass */
            if (config->i_pass > 1)
                SendDlgItemMessage(hMainDlg, IDC_RC_MODE, CB_SETCURSEL, 6, 0);
            else if (config->b_fast1pass)
                SendDlgItemMessage(hMainDlg, IDC_RC_MODE, CB_SETCURSEL, 5, 0);
            else
                SendDlgItemMessage(hMainDlg, IDC_RC_MODE, CB_SETCURSEL, 4, 0);
            SetDlgItemText(hMainDlg, IDC_RC_LABEL, "Target bitrate (kbit/s)");
            SetDlgItemText(hMainDlg, IDC_RC_LOW_LABEL, "1");
            sprintf(temp, "%d", MAX_BITRATE);
            SetDlgItemText(hMainDlg, IDC_RC_HIGH_LABEL, temp);
            SetDlgItemInt(hMainDlg, IDC_RC_VAL, config->i_passbitrate, FALSE);
            SendDlgItemMessage(hMainDlg, IDC_RC_VAL_SLIDER, TBM_SETRANGEMIN, TRUE, 1);
            SendDlgItemMessage(hMainDlg, IDC_RC_VAL_SLIDER, TBM_SETRANGEMAX, TRUE, scale2pos(MAX_BITRATE));
            SendDlgItemMessage(hMainDlg, IDC_RC_VAL_SLIDER, TBM_SETPOS, TRUE, scale2pos(config->i_passbitrate));
            break;

        default:
            assert(0);
            break;
    }
    CheckDlgButton(hMainDlg, IDC_STATS_CREATE, config->b_createstats);
    CheckDlgButton(hMainDlg, IDC_STATS_UPDATE, config->b_updatestats);
    SendMessage(GetDlgItem(hMainDlg, IDC_STATS_FILE), EM_LIMITTEXT, MAX_STATS_PATH - 1, 0);
    SetDlgItemText(hMainDlg, IDC_STATS_FILE, config->stats);
    /* Output */
    if (SendMessage(GetDlgItem(hMainDlg, IDC_OUTPUT_MODE), CB_GETCOUNT, 0, 0) == 0)
    {
        SendDlgItemMessage(hMainDlg, IDC_OUTPUT_MODE, CB_ADDSTRING, 0, (LPARAM)"VFW");
        SendDlgItemMessage(hMainDlg, IDC_OUTPUT_MODE, CB_ADDSTRING, 0, (LPARAM)"File");
    }
    SendDlgItemMessage(hMainDlg, IDC_OUTPUT_MODE, CB_SETCURSEL, config->i_output_mode, 0);
    if (SendMessage(GetDlgItem(hMainDlg, IDC_VFW_FOURCC), CB_GETCOUNT, 0, 0) == 0)
    {
        int i;
        for (i = 0; i < COUNT_FOURCC; i++)
            SendDlgItemMessage(hMainDlg, IDC_VFW_FOURCC, CB_ADDSTRING, 0, (LPARAM)fourcc_table[i].name);
    }
    SendDlgItemMessage(hMainDlg, IDC_VFW_FOURCC, CB_SETCURSEL, config->i_fourcc, 0);
#if X264VFW_USE_VIRTUALDUB_HACK
    CheckDlgButton(hMainDlg, IDC_VFW_VD_HACK, config->b_vd_hack);
#endif
    SendMessage(GetDlgItem(hMainDlg, IDC_OUTPUT_FILE), EM_LIMITTEXT, MAX_OUTPUT_PATH - 1, 0);
    SetDlgItemText(hMainDlg, IDC_OUTPUT_FILE, config->output_file);
    /* Encoder */
    sprintf(temp, "libx264 core %d%s", X264_BUILD, X264_VERSION);
    SetDlgItemText(hMainDlg, IDC_ENCODER_LABEL, temp);
    /* Sample Aspect Ratio */
    SendMessage(GetDlgItem(hMainDlg, IDC_SAR_W), EM_LIMITTEXT, 8, 0);
    SetDlgItemInt(hMainDlg, IDC_SAR_W, config->i_sar_width, FALSE);
    SendMessage(GetDlgItem(hMainDlg, IDC_SAR_H), EM_LIMITTEXT, 8, 0);
    SetDlgItemInt(hMainDlg, IDC_SAR_H, config->i_sar_height, FALSE);
    /* Debug */
    if (SendMessage(GetDlgItem(hMainDlg, IDC_LOG_LEVEL), CB_GETCOUNT, 0, 0) == 0)
    {
        SendDlgItemMessage(hMainDlg, IDC_LOG_LEVEL, CB_ADDSTRING, 0, (LPARAM)"None");
        SendDlgItemMessage(hMainDlg, IDC_LOG_LEVEL, CB_ADDSTRING, 0, (LPARAM)"Error");
        SendDlgItemMessage(hMainDlg, IDC_LOG_LEVEL, CB_ADDSTRING, 0, (LPARAM)"Warning");
        SendDlgItemMessage(hMainDlg, IDC_LOG_LEVEL, CB_ADDSTRING, 0, (LPARAM)"Info");
        SendDlgItemMessage(hMainDlg, IDC_LOG_LEVEL, CB_ADDSTRING, 0, (LPARAM)"Debug");
    }
    SendDlgItemMessage(hMainDlg, IDC_LOG_LEVEL, CB_SETCURSEL, config->i_log_level, 0);
    CheckDlgButton(hMainDlg, IDC_PSNR, config->b_psnr);
    CheckDlgButton(hMainDlg, IDC_SSIM, config->b_ssim);
    CheckDlgButton(hMainDlg, IDC_NO_ASM, config->b_no_asm);
    /* Decoder && AVI Muxer */
#if defined(HAVE_FFMPEG) && X264VFW_USE_DECODER
    CheckDlgButton(hMainDlg, IDC_VFW_DISABLE_DECODER, config->b_disable_decoder);
#endif
    /* Extra command line*/
    SendMessage(GetDlgItem(hMainDlg, IDC_EXTRA_CMDLINE), EM_LIMITTEXT, MAX_CMDLINE - 1, 0);
    SetDlgItemText(hMainDlg, IDC_EXTRA_CMDLINE, config->extra_cmdline);
    /* Build date */
    sprintf(temp, "Build date: %s %s", __DATE__, __TIME__);
    SetDlgItemText(hMainDlg, IDC_BUILD_DATE, temp);
}

/* Assigns tooltips */
static BOOL CALLBACK enum_tooltips(HWND hDlg, LPARAM lParam)
{
    char help[1024];

    /* The tooltip for a control is named the same as the control itself */
    if (LoadString(g_hInst, GetDlgCtrlID(hDlg), help, 1024))
    {
        TOOLINFO ti;

        ti.cbSize = sizeof(TOOLINFO);
        ti.uFlags = TTF_SUBCLASS | TTF_IDISHWND;
        ti.hwnd = GetParent(hDlg);
        ti.uId = (LPARAM)hDlg;
        ti.lpszText = help;

        SendMessage((HWND)lParam, TTM_ADDTOOL, 0, (LPARAM)&ti);
    }

    return TRUE;
}

static void CheckControlTextIsNumber(HWND hDlgItem, int bSigned, int iDecimalPlacesNum)
{
    char text_old[MAX_PATH];
    char text_new[MAX_PATH];
    char *src, *dest;
    DWORD start, end, num, pos;
    int bChanged = FALSE;
    int bCopy = FALSE;
    int q = !bSigned;

    SendMessage(hDlgItem, EM_GETSEL, (WPARAM)&start, (LPARAM)&end);
    num = SendMessage(hDlgItem, WM_GETTEXT, MAX_PATH, (LPARAM)text_old);
    src = text_old;
    dest = text_new;
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
        SendMessage(hDlgItem, WM_SETTEXT, 0, (LPARAM)text_new);
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

static int get_extension_index(const char *filename, const char *list)
{
    int index = 0;
    const char *ext = filename + strlen(filename);
    while (*ext != '.' && ext > filename)
        ext--;
    ext += *ext == '.';
    while (*list)
    {
        index++;
        if (!strcasecmp(ext, list))
            return index;
        list += strlen(list) + 1;
    }
    return 0;
}

/* Main window (configuration) */
INT_PTR CALLBACK callback_main(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CONFIG_DATA *cfg_data = (CONFIG_DATA *)GetWindowLongPtr(hDlg, GWLP_USERDATA);
    CONFIG *config;
    HWND hMainDlg;

    if (uMsg == WM_INITDIALOG)
    {
        HWND hTooltip;

        SetWindowLongPtr(hDlg, GWLP_USERDATA, lParam);
        cfg_data = (CONFIG_DATA *)lParam;
        cfg_data->hMainDlg = hDlg;
        cfg_data->b_dlg_updated = FALSE;

        if ((hTooltip = CreateWindow(TOOLTIPS_CLASS, NULL, TTS_ALWAYSTIP, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, hDlg, NULL, g_hInst, NULL)))
        {
            SetWindowPos(hTooltip, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
            SendMessage(hTooltip, TTM_SETMAXTIPWIDTH, 0, 400);
            EnumChildWindows(hDlg, enum_tooltips, (LPARAM)hTooltip);
        }

        dlg_enable_items(cfg_data);
        dlg_update_items(cfg_data);
        cfg_data->b_dlg_updated = TRUE;
        ShowWindow(hDlg, SW_SHOWNORMAL);
        return TRUE;
    }
    else if (uMsg == WM_DESTROY)
    {
        if (cfg_data && cfg_data->hHelpDlg)
            DestroyWindow(cfg_data->hHelpDlg);
    }
    if (!(cfg_data && cfg_data->b_dlg_updated))
        return FALSE;
    config = &cfg_data->config;
    hMainDlg = cfg_data->hMainDlg;
    switch (uMsg)
    {
        case WM_COMMAND:
            switch (HIWORD(wParam))
            {
                case LBN_SELCHANGE:
                    switch (LOWORD(wParam))
                    {
                        case IDC_PRESET:
                            config->i_preset = SendDlgItemMessage(hMainDlg, IDC_PRESET, CB_GETCURSEL, 0, 0);
                            break;

                        case IDC_TUNING:
                            config->i_tuning = SendDlgItemMessage(hMainDlg, IDC_TUNING, CB_GETCURSEL, 0, 0);
                            break;

                        case IDC_PROFILE:
                            config->i_profile = SendDlgItemMessage(hMainDlg, IDC_PROFILE, CB_GETCURSEL, 0, 0);
                            break;

                        case IDC_LEVEL:
                            config->i_level = SendDlgItemMessage(hMainDlg, IDC_LEVEL, CB_GETCURSEL, 0, 0);
                            break;

                        case IDC_RC_MODE:
                            config->i_pass = 1;
                            config->b_fast1pass = FALSE;
                            switch (SendDlgItemMessage(hMainDlg, IDC_RC_MODE, CB_GETCURSEL, 0, 0))
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
                            cfg_data->b_dlg_updated = FALSE;
                            dlg_update_items(cfg_data);

                            /* Ugly hack for fixing visualization bug of IDC_RC_VAL_SLIDER */
                            ShowWindow(GetDlgItem(hMainDlg, IDC_RC_VAL_SLIDER), FALSE);
                            ShowWindow(GetDlgItem(hMainDlg, IDC_RC_VAL_SLIDER), config->i_encoding_type > 0);

                            cfg_data->b_dlg_updated = TRUE;
                            break;

                        case IDC_OUTPUT_MODE:
                            config->i_output_mode = SendDlgItemMessage(hMainDlg, IDC_OUTPUT_MODE, CB_GETCURSEL, 0, 0);
                            break;

                        case IDC_VFW_FOURCC:
                            config->i_fourcc = SendDlgItemMessage(hMainDlg, IDC_VFW_FOURCC, CB_GETCURSEL, 0, 0);
                            break;

                        case IDC_LOG_LEVEL:
                            config->i_log_level = SendDlgItemMessage(hMainDlg, IDC_LOG_LEVEL, CB_GETCURSEL, 0, 0);
                            break;

                        default:
                            return FALSE;
                    }
                    break;

                case BN_CLICKED:
                    switch (LOWORD(wParam))
                    {
                        case IDOK:
                            cfg_data->b_save = TRUE;
                            EndDialog(hMainDlg, LOWORD(wParam));
                            break;

                        case IDCANCEL:
                            cfg_data->b_save = FALSE;
                            EndDialog(hMainDlg, LOWORD(wParam));
                            break;

                        case IDC_TUNE_FASTDECODE:
                            config->b_fastdecode = IsDlgButtonChecked(hMainDlg, IDC_TUNE_FASTDECODE);
                            break;

                        case IDC_TUNE_ZEROLATENCY:
                            config->b_zerolatency = IsDlgButtonChecked(hMainDlg, IDC_TUNE_ZEROLATENCY);
                            break;

                        case IDC_STATS_CREATE:
                            config->b_createstats = IsDlgButtonChecked(hMainDlg, IDC_STATS_CREATE);
                            break;

                        case IDC_STATS_UPDATE:
                            config->b_updatestats = IsDlgButtonChecked(hMainDlg, IDC_STATS_UPDATE);
                            break;

                        case IDC_STATS_BROWSE:
                        {
                            OPENFILENAME ofn;
                            char tmp[MAX_STATS_SIZE];

                            if (GetDlgItemText(hMainDlg, IDC_STATS_FILE, tmp, MAX_STATS_SIZE) == 0)
                                strcpy(tmp, "");

                            memset(&ofn, 0, sizeof(OPENFILENAME));
                            ofn.lStructSize = sizeof(OPENFILENAME);

                            ofn.hwndOwner = hMainDlg;
                            ofn.lpstrFilter = "Stats files (*.stats)\0*.stats\0All files (*.*)\0*.*\0\0";
                            ofn.nFilterIndex = tmp[0] ? get_extension_index(tmp, "stats\0\0") : 1;
                            ofn.nFilterIndex = ofn.nFilterIndex ? ofn.nFilterIndex : 2;
                            ofn.lpstrFile = tmp;
                            ofn.nMaxFile = MAX_STATS_SIZE;
                            ofn.Flags = OFN_PATHMUSTEXIST;
                            ofn.lpstrDefExt = "";

                            if (config->i_pass <= 1)
                                ofn.Flags |= OFN_OVERWRITEPROMPT;
                            else
                                ofn.Flags |= OFN_FILEMUSTEXIST;

                            if ((config->i_pass <= 1 && GetSaveFileName(&ofn)) ||
                                (config->i_pass > 1 && GetOpenFileName(&ofn)))
                                SetDlgItemText(hMainDlg, IDC_STATS_FILE, tmp);
                            break;
                        }

#if X264VFW_USE_VIRTUALDUB_HACK
                        case IDC_VFW_VD_HACK:
                            config->b_vd_hack = IsDlgButtonChecked(hMainDlg, IDC_VFW_VD_HACK);
                            break;
#endif

                        case IDC_OUTPUT_BROWSE:
                        {
                            OPENFILENAME ofn;
                            char tmp[MAX_OUTPUT_SIZE];

                            if (GetDlgItemText(hMainDlg, IDC_OUTPUT_FILE, tmp, MAX_OUTPUT_SIZE) == 0)
                                strcpy(tmp, "");

                            memset(&ofn, 0, sizeof(OPENFILENAME));
                            ofn.lStructSize = sizeof(OPENFILENAME);

                            ofn.hwndOwner = hMainDlg;
                            ofn.lpstrFilter = "*.avi\0*.avi\0*.flv\0*.flv\0*.mkv\0*.mkv\0*.mp4\0*.mp4\0*.h264\0*.h264\0All files (*.*)\0*.*\0\0";
                            ofn.nFilterIndex = tmp[0] ? get_extension_index(tmp, "avi\0flv\0mkv\0mp4\0h264\0\0") : 1;
                            ofn.nFilterIndex = ofn.nFilterIndex ? ofn.nFilterIndex : 6;
                            ofn.lpstrFile = tmp;
                            ofn.nMaxFile = MAX_OUTPUT_SIZE;
                            ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT;
                            ofn.lpstrDefExt = "";

                            if (GetSaveFileName(&ofn))
                                SetDlgItemText(hMainDlg, IDC_OUTPUT_FILE, tmp);
                            break;
                        }

                        case IDC_PSNR:
                            config->b_psnr = IsDlgButtonChecked(hMainDlg, IDC_PSNR);
                            break;

                        case IDC_SSIM:
                            config->b_ssim = IsDlgButtonChecked(hMainDlg, IDC_SSIM);
                            break;

                        case IDC_NO_ASM:
                            config->b_no_asm = IsDlgButtonChecked(hMainDlg, IDC_NO_ASM);
                            break;

#if defined(HAVE_FFMPEG) && X264VFW_USE_DECODER
                        case IDC_VFW_DISABLE_DECODER:
                            config->b_disable_decoder = IsDlgButtonChecked(hMainDlg, IDC_VFW_DISABLE_DECODER);
                            break;
#endif

                        case IDC_EXTRA_HELP:
                            if (!cfg_data->hHelpDlg)
                                cfg_data->hHelpDlg = CreateDialog(g_hInst, MAKEINTRESOURCE(IDD_HELP), GetDesktopWindow(), callback_help);
                            if (cfg_data->hHelpDlg)
                            {
                                ShowWindow(cfg_data->hHelpDlg, SW_SHOWNORMAL);
                                BringWindowToTop(cfg_data->hHelpDlg);
                            }
                            break;

                        case IDC_DEFAULTS:
                            if (MessageBox(hMainDlg, X264VFW_DEF_TEXT, X264VFW_NAME, MB_YESNO) == IDYES)
                            {
                                config_defaults(config);
                                cfg_data->b_dlg_updated = FALSE;
                                dlg_enable_items(cfg_data);
                                dlg_update_items(cfg_data);
                                cfg_data->b_dlg_updated = TRUE;
                            }
                            break;

                        default:
                            return FALSE;
                    }
                    break;

                case EN_CHANGE:
                    switch (LOWORD(wParam))
                    {
                        case IDC_RC_VAL:
                            switch (config->i_encoding_type)
                            {
                                case 0:
                                    break;

                                case 1:
                                    CHECKED_SET_MAX_INT(config->i_qp, hMainDlg, IDC_RC_VAL, FALSE, 1, MAX_QUANT);
                                    SendDlgItemMessage(hMainDlg, IDC_RC_VAL_SLIDER, TBM_SETPOS, TRUE, config->i_qp);
                                    break;

                                case 2:
                                    CheckControlTextIsNumber(GetDlgItem(hMainDlg, IDC_RC_VAL), FALSE, 1);
                                    config->i_rf_constant = (int)(GetDlgItemDouble(hMainDlg, IDC_RC_VAL) * 10);
                                    if (config->i_rf_constant < 10)
                                        config->i_rf_constant = 10;
                                    else if (config->i_rf_constant > MAX_QUANT * 10)
                                    {
                                        config->i_rf_constant = MAX_QUANT * 10;
                                        SetDlgItemDouble(hMainDlg, IDC_RC_VAL, config->i_rf_constant * 0.1, "%.1f");
                                        SendMessage(GetDlgItem(hMainDlg, IDC_RC_VAL), EM_SETSEL, -2, -2);
                                    }
                                    SendDlgItemMessage(hMainDlg, IDC_RC_VAL_SLIDER, TBM_SETPOS, TRUE, config->i_rf_constant);
                                    break;

                                case 3:
                                case 4:
                                    CHECKED_SET_MAX_INT(config->i_passbitrate, hMainDlg, IDC_RC_VAL, FALSE, 1, MAX_BITRATE);
                                    SendDlgItemMessage(hMainDlg, IDC_RC_VAL_SLIDER, TBM_SETPOS, TRUE, scale2pos(config->i_passbitrate));
                                    break;

                                default:
                                    assert(0);
                                    break;
                            }
                            break;

                        case IDC_STATS_FILE:
                            if (GetDlgItemText(hMainDlg, IDC_STATS_FILE, config->stats, MAX_STATS_PATH) == 0)
                                strcpy(config->stats, ".\\x264.stats");
                            break;

                        case IDC_OUTPUT_FILE:
                            if (GetDlgItemText(hMainDlg, IDC_OUTPUT_FILE, config->output_file, MAX_OUTPUT_PATH) == 0)
                                strcpy(config->output_file, "");
                            break;

                        case IDC_SAR_W:
                            CHECKED_SET_MAX_INT(config->i_sar_width, hMainDlg, IDC_SAR_W, FALSE, 1, 9999);
                            break;

                        case IDC_SAR_H:
                            CHECKED_SET_MAX_INT(config->i_sar_height, hMainDlg, IDC_SAR_H, FALSE, 1, 9999);
                            break;

                        case IDC_EXTRA_CMDLINE:
                            if (GetDlgItemText(hMainDlg, IDC_EXTRA_CMDLINE, config->extra_cmdline, MAX_CMDLINE) == 0)
                                strcpy(config->extra_cmdline, "");
                            break;

                        default:
                            return FALSE;
                    }
                    break;

                case EN_KILLFOCUS:
                    switch (LOWORD(wParam))
                    {
                        case IDC_RC_VAL:
                            switch (config->i_encoding_type)
                            {
                                case 0:
                                    break;

                                case 1:
                                    CHECKED_SET_SHOW_INT(config->i_qp, hMainDlg, IDC_RC_VAL, FALSE, 1, MAX_QUANT);
                                    SendDlgItemMessage(hMainDlg, IDC_RC_VAL_SLIDER, TBM_SETPOS, TRUE, config->i_qp);
                                    break;

                                case 2:
                                    CheckControlTextIsNumber(GetDlgItem(hMainDlg, IDC_RC_VAL), FALSE, 1);
                                    config->i_rf_constant = (int)(GetDlgItemDouble(hMainDlg, IDC_RC_VAL) * 10);
                                    if (config->i_rf_constant < 10)
                                        config->i_rf_constant = 10;
                                    else if (config->i_rf_constant > MAX_QUANT * 10)
                                        config->i_rf_constant = MAX_QUANT * 10;
                                    SetDlgItemDouble(hMainDlg, IDC_RC_VAL, config->i_rf_constant * 0.1, "%.1f");
                                    SendDlgItemMessage(hMainDlg, IDC_RC_VAL_SLIDER, TBM_SETPOS, TRUE, config->i_rf_constant);
                                    break;

                                case 3:
                                case 4:
                                    CHECKED_SET_SHOW_INT(config->i_passbitrate, hMainDlg, IDC_RC_VAL, FALSE, 1, MAX_BITRATE);
                                    SendDlgItemMessage(hMainDlg, IDC_RC_VAL_SLIDER, TBM_SETPOS, TRUE, scale2pos(config->i_passbitrate));
                                    break;

                                default:
                                    assert(0);
                                    break;
                            }
                            break;

                        case IDC_STATS_FILE:
                            if (GetDlgItemText(hMainDlg, IDC_STATS_FILE, config->stats, MAX_STATS_PATH) == 0)
                                strcpy(config->stats, ".\\x264.stats");
                            SetDlgItemText(hMainDlg, IDC_STATS_FILE, config->stats);
                            break;

                        case IDC_OUTPUT_FILE:
                            if (GetDlgItemText(hMainDlg, IDC_OUTPUT_FILE, config->output_file, MAX_OUTPUT_PATH) == 0)
                                strcpy(config->output_file, "");
                            SetDlgItemText(hMainDlg, IDC_OUTPUT_FILE, config->output_file);
                            break;

                        case IDC_SAR_W:
                            CHECKED_SET_SHOW_INT(config->i_sar_width, hMainDlg, IDC_SAR_W, FALSE, 1, 9999);
                            break;

                        case IDC_SAR_H:
                            CHECKED_SET_SHOW_INT(config->i_sar_height, hMainDlg, IDC_SAR_H, FALSE, 1, 9999);
                            break;

                        case IDC_EXTRA_CMDLINE:
                            if (GetDlgItemText(hMainDlg, IDC_EXTRA_CMDLINE, config->extra_cmdline, MAX_CMDLINE) == 0)
                                strcpy(config->extra_cmdline, "");
                            SetDlgItemText(hMainDlg, IDC_EXTRA_CMDLINE, config->extra_cmdline);
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
            if ((HWND)lParam == GetDlgItem(hMainDlg, IDC_RC_VAL_SLIDER))
            {
                switch (config->i_encoding_type)
                {
                    case 0:
                        break;

                    case 1:
                        config->i_qp = SendDlgItemMessage(hMainDlg, IDC_RC_VAL_SLIDER, TBM_GETPOS, 0, 0);
                        SetDlgItemInt(hMainDlg, IDC_RC_VAL, config->i_qp, FALSE);
                        break;

                    case 2:
                        config->i_rf_constant = SendDlgItemMessage(hMainDlg, IDC_RC_VAL_SLIDER, TBM_GETPOS, 0, 0);
                        SetDlgItemDouble(hMainDlg, IDC_RC_VAL, config->i_rf_constant * 0.1, "%.1f");
                        break;

                    case 3:
                    case 4:
                        config->i_passbitrate = pos2scale(SendDlgItemMessage(hMainDlg, IDC_RC_VAL_SLIDER, TBM_GETPOS, 0, 0));
                        config->i_passbitrate = X264_CLIP(config->i_passbitrate, 1, MAX_BITRATE);
                        SetDlgItemInt(hMainDlg, IDC_RC_VAL, config->i_passbitrate, FALSE);
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

    dlg_enable_items(cfg_data);
    return TRUE;
}

/* About window */
INT_PTR CALLBACK callback_about(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
        case WM_INITDIALOG:
        {
            char temp[1024];

            sprintf(temp, "Build date: %s %s\nlibx264 core %d%s", __DATE__, __TIME__, X264_BUILD, X264_VERSION);
            SetDlgItemText(hDlg, ABOUT_IDC_BUILD_LABEL, temp);
            return TRUE;
        }

        case WM_COMMAND:
            switch (HIWORD(wParam))
            {
                case BN_CLICKED:
                    if (LOWORD(wParam) == ABOUT_IDC_HOMEPAGE)
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

/* Log window */
static void update_log_layout(HWND hDlg)
{
    RECT rcDlg, rcNewDlg, rcItem, rcNewItem;
    rcDlg.left   = 0;
    rcDlg.top    = 0;
    rcDlg.right  = LOG_DLG_WIDTH;
    rcDlg.bottom = LOG_DLG_HEIGHT;
    MapDialogRect(hDlg, &rcDlg);
    GetClientRect(hDlg, &rcNewDlg);
    //LOG_IDC_CONSOLE
    rcItem.left   = LOG_CONS_X;
    rcItem.top    = LOG_CONS_Y;
    rcItem.right  = LOG_CONS_X + LOG_CONS_WIDTH;
    rcItem.bottom = LOG_CONS_Y + LOG_CONS_HEIGHT;
    MapDialogRect(hDlg, &rcItem);
    rcNewItem.left   = rcItem.left   + rcNewDlg.left   - rcDlg.left;
    rcNewItem.top    = rcItem.top    + rcNewDlg.top    - rcDlg.top;
    rcNewItem.right  = rcItem.right  + rcNewDlg.right  - rcDlg.right;
    rcNewItem.bottom = rcItem.bottom + rcNewDlg.bottom - rcDlg.bottom;
    MoveWindow(GetDlgItem(hDlg, LOG_IDC_CONSOLE), rcNewItem.left, rcNewItem.top, rcNewItem.right - rcNewItem.left, rcNewItem.bottom - rcNewItem.top, TRUE);
    //LOG_IDC_OK
    rcItem.left   = LOG_OK_X;
    rcItem.top    = LOG_OK_Y;
    rcItem.right  = LOG_OK_X + LOG_OK_WIDTH;
    rcItem.bottom = LOG_OK_Y + LOG_OK_HEIGHT;
    MapDialogRect(hDlg, &rcItem);
    rcNewItem.left   = rcItem.left   + (rcNewDlg.left - rcDlg.left + rcNewDlg.right - rcDlg.right + 1) / 2;
    rcNewItem.top    = rcItem.top    + rcNewDlg.bottom - rcDlg.bottom;
    rcNewItem.right  = rcItem.right  + (rcNewDlg.left - rcDlg.left + rcNewDlg.right - rcDlg.right + 1) / 2;
    rcNewItem.bottom = rcItem.bottom + rcNewDlg.bottom - rcDlg.bottom;
    MoveWindow(GetDlgItem(hDlg, LOG_IDC_OK), rcNewItem.left, rcNewItem.top, rcNewItem.right - rcNewItem.left, rcNewItem.bottom - rcNewItem.top, TRUE);
    //LOG_IDC_COPY
    rcItem.left   = LOG_COPY_X;
    rcItem.top    = LOG_COPY_Y;
    rcItem.right  = LOG_COPY_X + LOG_COPY_WIDTH;
    rcItem.bottom = LOG_COPY_Y + LOG_COPY_HEIGHT;
    MapDialogRect(hDlg, &rcItem);
    rcNewItem.left   = rcItem.left   + (rcNewDlg.left - rcDlg.left + rcNewDlg.right - rcDlg.right + 1) / 2;
    rcNewItem.top    = rcItem.top    + rcNewDlg.bottom - rcDlg.bottom;
    rcNewItem.right  = rcItem.right  + (rcNewDlg.left - rcDlg.left + rcNewDlg.right - rcDlg.right + 1) / 2;
    rcNewItem.bottom = rcItem.bottom + rcNewDlg.bottom - rcDlg.bottom;
    MoveWindow(GetDlgItem(hDlg, LOG_IDC_COPY), rcNewItem.left, rcNewItem.top, rcNewItem.right - rcNewItem.left, rcNewItem.bottom - rcNewItem.top, TRUE);
}

INT_PTR CALLBACK callback_log(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
        case WM_INITDIALOG:
            return FALSE;

        case WM_COMMAND:
            if (HIWORD(wParam) == BN_CLICKED)
            {
                switch (LOWORD(wParam))
                {
                    case IDOK:
                    case IDCANCEL:
                    case LOG_IDC_OK:
                        ShowWindow(hDlg, SW_HIDE);
                        break;

                    case LOG_IDC_COPY:
                        if (OpenClipboard(hDlg))
                        {
                            int num_lines = SendDlgItemMessage(hDlg, LOG_IDC_CONSOLE, LB_GETCOUNT, 0, 0);

                            EmptyClipboard();
                            if (num_lines > 0)
                            {
                                int i;
                                int text_size = 0;
                                HGLOBAL clipbuffer;

                                /* Calculate text size */
                                for (i = 0; i < num_lines; i++)
                                {
                                    int len = SendDlgItemMessage(hDlg, LOG_IDC_CONSOLE, LB_GETTEXTLEN, (WPARAM)i, 0);
                                    text_size += len >= 0 ? len : 0;
                                }

                                /* CR - LF for each line + terminating NULL */
                                text_size += 2 * num_lines + 1;

                                clipbuffer = GlobalAlloc(GMEM_MOVEABLE | GMEM_DDESHARE, text_size * sizeof(char));
                                if (clipbuffer)
                                {
                                    char *buffer = (char *)GlobalLock(clipbuffer);

                                    if (buffer)
                                    {
                                        /* Concatenate lines of text in the global buffer */
                                        for (i = 0; i < num_lines; i++)
                                        {
                                            char *msg_buf = buffer;

                                            if (SendDlgItemMessage(hDlg, LOG_IDC_CONSOLE, LB_GETTEXT, (WPARAM)i, (LPARAM)msg_buf) <= 0)
                                                strcpy(msg_buf, "");
                                            strcat(msg_buf, "\r\n");
                                            buffer += strlen(msg_buf);
                                        }
                                        *buffer = 0; /* NULL-terminate the buffer */

                                        GlobalUnlock(clipbuffer);
                                        if (!SetClipboardData(CF_TEXT, clipbuffer))
                                            GlobalFree(clipbuffer);
                                    }
                                    else
                                        GlobalFree(clipbuffer);
                                }
                            }
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

        case WM_SIZE:
            if (wParam != SIZE_MINIMIZED)
                update_log_layout(hDlg);
            break;

        case WM_SIZING:
        {
            RECT *rc = (RECT *)lParam;
            int width = rc->right - rc->left;
            int height = rc->bottom - rc->top;
            if (width < LOG_DLG_MIN_WIDTH)
                switch (wParam)
                {
                    case WMSZ_LEFT:
                    case WMSZ_TOPLEFT:
                    case WMSZ_BOTTOMLEFT:
                        rc->left = rc->right - LOG_DLG_MIN_WIDTH;
                        break;

                    default:
                        rc->right = rc->left + LOG_DLG_MIN_WIDTH;
                        break;
                }
            if (height < LOG_DLG_MIN_HEIGHT)
                switch (wParam)
                {
                    case WMSZ_TOP:
                    case WMSZ_TOPLEFT:
                    case WMSZ_TOPRIGHT:
                        rc->top = rc->bottom - LOG_DLG_MIN_HEIGHT;
                        break;

                    default:
                        rc->bottom = rc->top + LOG_DLG_MIN_HEIGHT;
                        break;
                }
            break;
        }

        default:
            return FALSE;
    }

    return TRUE;
}

/* Help window */
static char const *strtable_lookup(const char * const table[], int index)
{
    int i = 0;
    while (table[i])
        i++;
    return ((index >= 0 && index < i) ? table[index] : "???");
}

static char *stringify_names(char *buf, const char * const names[])
{
    int i = 0;
    char *p = buf;
    for (p[0] = 0; names[i]; i++)
    {
        p += sprintf(p, "%s", names[i]);
        if (names[i+1])
            p += sprintf(p, ", ");
    }
    return buf;
}

static void Help(char *buffer, int longhelp)
{
#define H0(...) do { sprintf(buffer, __VA_ARGS__); buffer += strlen(buffer); } while (0)
#define H1(...) if (longhelp>=1) do { sprintf(buffer, __VA_ARGS__); buffer += strlen(buffer); } while (0)
#define H2(...) if (longhelp==2) do { sprintf(buffer, __VA_ARGS__); buffer += strlen(buffer); } while (0)
    char buf[50];
    x264_param_t defaults_value;
    x264_param_t *defaults = &defaults_value;

    x264_param_default(&defaults_value);
    H0( "Presets:\r\n" );
    H0( "\r\n" );
    H0( "      --profile               Force the limits of an H.264 profile\r\n"
        "                                  Overrides all settings.\r\n" );
    H2( "                                  - baseline:\r\n"
        "                                    --no-8x8dct --bframes 0 --no-cabac\r\n"
        "                                    --cqm flat --weightp 0\r\n"
        "                                    No interlaced.\r\n"
        "                                    No lossless.\r\n"
        "                                  - main:\r\n"
        "                                    --no-8x8dct --cqm flat\r\n"
        "                                    No lossless.\r\n"
        "                                  - high:\r\n"
        "                                    No lossless.\r\n" );
        else H0( "                                  - baseline,main,high\r\n" );
    H0( "      --preset                Use a preset to select encoding settings [medium]\r\n"
        "                                  Overridden by user settings.\r\n" );
    H2( "                                  - ultrafast:\r\n"
        "                                    --no-8x8dct --aq-mode 0 --b-adapt 0\r\n"
        "                                    --bframes 0 --no-cabac --no-deblock\r\n"
        "                                    --no-mbtree --me dia --no-mixed-refs\r\n"
        "                                    --partitions none --rc-lookahead 0 --ref 1\r\n"
        "                                    --scenecut 0 --subme 0 --trellis 0\r\n"
        "                                    --no-weightb --weightp 0\r\n"
        "                                  - superfast:\r\n"
        "                                    --no-mbtree --me dia --no-mixed-refs\r\n"
        "                                    --partitions i8x8,i4x4 --rc-lookahead 0\r\n"
        "                                    --ref 1 --subme 1 --trellis 0 --weightp 0\r\n"
        "                                  - veryfast:\r\n"
        "                                    --no-mixed-refs --rc-lookahead 10\r\n"
        "                                    --ref 1 --subme 2 --trellis 0 --weightp 0\r\n"
        "                                  - faster:\r\n"
        "                                    --no-mixed-refs --rc-lookahead 20\r\n"
        "                                    --ref 2 --subme 4 --weightp 1\r\n"
        "                                  - fast:\r\n"
        "                                    --rc-lookahead 30 --ref 2 --subme 6\r\n"
        "                                  - medium:\r\n"
        "                                    Default settings apply.\r\n"
        "                                  - slow:\r\n"
        "                                    --b-adapt 2 --direct auto --me umh\r\n"
        "                                    --rc-lookahead 50 --ref 5 --subme 8\r\n"
        "                                  - slower:\r\n"
        "                                    --b-adapt 2 --direct auto --me umh\r\n"
        "                                    --partitions all --rc-lookahead 60\r\n"
        "                                    --ref 8 --subme 9 --trellis 2\r\n"
        "                                  - veryslow:\r\n"
        "                                    --b-adapt 2 --bframes 8 --direct auto\r\n"
        "                                    --me umh --merange 24 --partitions all\r\n"
        "                                    --ref 16 --subme 10 --trellis 2\r\n"
        "                                    --rc-lookahead 60\r\n"
        "                                  - placebo:\r\n"
        "                                    --bframes 16 --b-adapt 2 --direct auto\r\n"
        "                                    --no-fast-pskip --me tesa --merange 24\r\n"
        "                                    --partitions all --rc-lookahead 60\r\n"
        "                                    --ref 16 --subme 10 --trellis 2\r\n" );
    else H0( "                                  - ultrafast,superfast,veryfast,faster,fast\r\n"
             "                                  - medium,slow,slower,veryslow,placebo\r\n" );
    H0( "      --tune                  Tune the settings for a particular type of source\r\n"
        "                              or situation\r\n"
        "                                  Overridden by user settings.\r\n"
        "                                  Multiple tunings are separated by commas.\r\n"
        "                                  Only one psy tuning can be used at a time.\r\n" );
    H2( "                                  - film (psy tuning):\r\n"
        "                                    --deblock -1:-1 --psy-rd <unset>:0.15\r\n"
        "                                  - animation (psy tuning):\r\n"
        "                                    --bframes {+2} --deblock 1:1\r\n"
        "                                    --psy-rd 0.4:<unset> --aq-strength 0.6\r\n"
        "                                    --ref {Double if >1 else 1}\r\n"
        "                                  - grain (psy tuning):\r\n"
        "                                    --aq-strength 0.5 --no-dct-decimate\r\n"
        "                                    --deadzone-inter 6 --deadzone-intra 6\r\n"
        "                                    --deblock -2:-2 --ipratio 1.1 \r\n"
        "                                    --pbratio 1.1 --psy-rd <unset>:0.25\r\n"
        "                                    --qcomp 0.8\r\n"
        "                                  - stillimage (psy tuning):\r\n"
        "                                    --aq-strength 1.2 --deblock -3:-3\r\n"
        "                                    --psy-rd 2.0:0.7\r\n"
        "                                  - psnr (psy tuning):\r\n"
        "                                    --aq-mode 0 --no-psy\r\n"
        "                                  - ssim (psy tuning):\r\n"
        "                                    --aq-mode 2 --no-psy\r\n"
        "                                  - fastdecode:\r\n"
        "                                    --no-cabac --no-deblock --no-weightb\r\n"
        "                                    --weightp 0\r\n"
        "                                  - zerolatency:\r\n"
        "                                    --bframes 0 --force-cfr --no-mbtree\r\n"
        "                                    --sync-lookahead 0 --sliced-threads\r\n"
        "                                    --rc-lookahead 0\r\n" );
    else H0( "                                  - psy tunings: film,animation,grain,\r\n"
             "                                                 stillimage,psnr,ssim\r\n"
             "                                  - other tunings: fastdecode,zerolatency\r\n" );
    H2( "      --slow-firstpass        Don't force these faster settings with --pass 1:\r\n"
        "                                  --no-8x8dct --me dia --partitions none\r\n"
        "                                  --ref 1 --subme {2 if >2 else unchanged}\r\n"
        "                                  --trellis 0 --fast-pskip\r\n" );
    else H1( "      --slow-firstpass        Don't force faster settings with --pass 1\r\n" );
    H0( "\r\n" );
    H0( "Frame-type options:\r\n" );
    H0( "\r\n" );
    H0( "  -I, --keyint <integer>      Maximum GOP size [%d]\r\n", defaults->i_keyint_max );
    H2( "  -i, --min-keyint <integer>  Minimum GOP size [auto]\r\n" );
    H2( "      --no-scenecut           Disable adaptive I-frame decision\r\n" );
    H2( "      --scenecut <integer>    How aggressively to insert extra I-frames [%d]\r\n", defaults->i_scenecut_threshold );
    H2( "      --intra-refresh         Use Periodic Intra Refresh instead of IDR frames\r\n" );
    H1( "  -b, --bframes <integer>     Number of B-frames between I and P [%d]\r\n", defaults->i_bframe );
    H1( "      --b-adapt <integer>     Adaptive B-frame decision method [%d]\r\n"
        "                                  Higher values may lower threading efficiency.\r\n"
        "                                  - 0: Disabled\r\n"
        "                                  - 1: Fast\r\n"
        "                                  - 2: Optimal (slow with high --bframes)\r\n", defaults->i_bframe_adaptive );
    H2( "      --b-bias <integer>      Influences how often B-frames are used [%d]\r\n", defaults->i_bframe_bias );
    H1( "      --b-pyramid <string>    Keep some B-frames as references [%s]\r\n"
        "                                  - none: Disabled\r\n"
        "                                  - strict: Strictly hierarchical pyramid\r\n"
        "                                  - normal: Non-strict (not Blu-ray compatible)\r\n",
        strtable_lookup( x264_b_pyramid_names, defaults->i_bframe_pyramid ) );
    H1( "      --open-gop <string>     Use recovery points to close GOPs [none]\r\n"
        "                                  - none: Use standard closed GOPs\r\n"
        "                                  - display: Base GOP length on display order\r\n"
        "                                             (not Blu-ray compatible)\r\n"
        "                                  - coded: Base GOP length on coded order\r\n"
        "                              Only available with b-frames\r\n" );
    H1( "      --no-cabac              Disable CABAC\r\n" );
    H1( "  -r, --ref <integer>         Number of reference frames [%d]\r\n", defaults->i_frame_reference );
    H1( "      --no-deblock            Disable loop filter\r\n" );
    H1( "  -f, --deblock <alpha:beta>  Loop filter parameters [%d:%d]\r\n",
                                       defaults->i_deblocking_filter_alphac0, defaults->i_deblocking_filter_beta );
    H2( "      --slices <integer>      Number of slices per frame; forces rectangular\r\n"
        "                              slices and is overridden by other slicing options\r\n" );
    else H1( "      --slices <integer>      Number of slices per frame\r\n" );
    H2( "      --slice-max-size <integer> Limit the size of each slice in bytes\r\n");
    H2( "      --slice-max-mbs <integer> Limit the size of each slice in macroblocks\r\n");
    H0( "      --tff                   Enable interlaced mode (top field first)\r\n" );
    H0( "      --bff                   Enable interlaced mode (bottom field first)\r\n" );
    H2( "      --constrained-intra     Enable constrained intra prediction.\r\n" );
    H2( "      --fake-interlaced       Flag stream as interlaced but encode progressive.\r\n"
        "                              Makes it possible to encode 25p and 30p Blu-Ray\r\n"
        "                              streams. Ignored in interlaced mode.\r\n" );
    H0( "\r\n" );
    H0( "Ratecontrol:\r\n" );
    H0( "\r\n" );
    H1( "  -q, --qp <integer>          Force constant QP (0-51, 0=lossless)\r\n" );
    H0( "  -B, --bitrate <integer>     Set bitrate (kbit/s)\r\n" );
    H0( "      --crf <float>           Quality-based VBR (0-51, 0=lossless) [%.1f]\r\n", defaults->rc.f_rf_constant );
    H1( "      --rc-lookahead <integer> Number of frames for frametype lookahead [%d]\r\n", defaults->rc.i_lookahead );
    H0( "      --vbv-maxrate <integer> Max local bitrate (kbit/s) [%d]\r\n", defaults->rc.i_vbv_max_bitrate );
    H0( "      --vbv-bufsize <integer> Set size of the VBV buffer (kbit) [%d]\r\n", defaults->rc.i_vbv_buffer_size );
    H2( "      --vbv-init <float>      Initial VBV buffer occupancy [%.1f]\r\n", defaults->rc.f_vbv_buffer_init );
    H2( "      --crf-max <float>       With CRF+VBV, limit RF to this value\r\n"
        "                                  May cause VBV underflows!\r\n" );
    H2( "      --qpmin <integer>       Set min QP [%d]\r\n", defaults->rc.i_qp_min );
    H2( "      --qpmax <integer>       Set max QP [%d]\r\n", defaults->rc.i_qp_max );
    H2( "      --qpstep <integer>      Set max QP step [%d]\r\n", defaults->rc.i_qp_step );
    H2( "      --ratetol <float>       Tolerance of ABR ratecontrol and VBV [%.1f]\r\n", defaults->rc.f_rate_tolerance );
    H2( "      --ipratio <float>       QP factor between I and P [%.2f]\r\n", defaults->rc.f_ip_factor );
    H2( "      --pbratio <float>       QP factor between P and B [%.2f]\r\n", defaults->rc.f_pb_factor );
    H2( "      --chroma-qp-offset <integer>  QP difference between chroma and luma [%d]\r\n", defaults->analyse.i_chroma_qp_offset );
    H2( "      --aq-mode <integer>     AQ method [%d]\r\n"
        "                                  - 0: Disabled\r\n"
        "                                  - 1: Variance AQ (complexity mask)\r\n"
        "                                  - 2: Auto-variance AQ (experimental)\r\n", defaults->rc.i_aq_mode );
    H1( "      --aq-strength <float>   Reduces blocking and blurring in flat and\r\n"
        "                              textured areas. [%.1f]\r\n", defaults->rc.f_aq_strength );
    H1( "\r\n" );
    H0( "  -p, --pass <integer>        Enable multipass ratecontrol\r\n"
        "                                  - 1: First pass, creates stats file\r\n"
        "                                  - 2: Last pass, does not overwrite stats file\r\n" );
    H2( "                                  - 3: Nth pass, overwrites stats file\r\n" );
    H1( "      --stats <string>        Filename for 2 pass stats [\"%s\"]\r\n", defaults->rc.psz_stat_out );
    H2( "      --no-mbtree             Disable mb-tree ratecontrol.\r\n");
    H2( "      --qcomp <float>         QP curve compression [%.2f]\r\n", defaults->rc.f_qcompress );
    H2( "      --cplxblur <float>      Reduce fluctuations in QP (before curve compression) [%.1f]\r\n", defaults->rc.f_complexity_blur );
    H2( "      --qblur <float>         Reduce fluctuations in QP (after curve compression) [%.1f]\r\n", defaults->rc.f_qblur );
    H2( "      --zones <zone0>/<zone1>/...  Tweak the bitrate of regions of the video\r\n" );
    H2( "                              Each zone is of the form\r\n"
        "                                  <start frame>,<end frame>,<option>\r\n"
        "                                  where <option> is either\r\n"
        "                                      q=<integer> (force QP)\r\n"
        "                                  or  b=<float> (bitrate multiplier)\r\n" );
    H1( "\r\n" );
    H1( "Analysis:\r\n" );
    H1( "\r\n" );
    H1( "  -A, --partitions <string>   Partitions to consider [\"p8x8,b8x8,i8x8,i4x4\"]\r\n"
        "                                  - p8x8, p4x4, b8x8, i8x8, i4x4\r\n"
        "                                  - none, all\r\n"
        "                                  (p4x4 requires p8x8. i8x8 requires --8x8dct.)\r\n" );
    H1( "      --direct <string>       Direct MV prediction mode [\"%s\"]\r\n"
        "                                  - none, spatial, temporal, auto\r\n",
                                       strtable_lookup( x264_direct_pred_names, defaults->analyse.i_direct_mv_pred ) );
    H2( "      --no-weightb            Disable weighted prediction for B-frames\r\n" );
    H1( "      --weightp <integer>     Weighted prediction for P-frames [%d]\r\n"
        "                                  - 0: Disabled\r\n"
        "                                  - 1: Blind offset\r\n"
        "                                  - 2: Smart analysis\r\n", defaults->analyse.i_weighted_pred );
    H1( "      --me <string>           Integer pixel motion estimation method [\"%s\"]\r\n",
                                       strtable_lookup( x264_motion_est_names, defaults->analyse.i_me_method ) );
    H2( "                                  - dia: diamond search, radius 1 (fast)\r\n"
        "                                  - hex: hexagonal search, radius 2\r\n"
        "                                  - umh: uneven multi-hexagon search\r\n"
        "                                  - esa: exhaustive search\r\n"
        "                                  - tesa: hadamard exhaustive search (slow)\r\n" );
    else H1( "                                  - dia, hex, umh\r\n" );
    H2( "      --merange <integer>     Maximum motion vector search range [%d]\r\n", defaults->analyse.i_me_range );
    H2( "      --mvrange <integer>     Maximum motion vector length [-1 (auto)]\r\n" );
    H2( "      --mvrange-thread <int>  Minimum buffer between threads [-1 (auto)]\r\n" );
    H1( "  -m, --subme <integer>       Subpixel motion estimation and mode decision [%d]\r\n", defaults->analyse.i_subpel_refine );
    H2( "                                  - 0: fullpel only (not recommended)\r\n"
        "                                  - 1: SAD mode decision, one qpel iteration\r\n"
        "                                  - 2: SATD mode decision\r\n"
        "                                  - 3-5: Progressively more qpel\r\n"
        "                                  - 6: RD mode decision for I/P-frames\r\n"
        "                                  - 7: RD mode decision for all frames\r\n"
        "                                  - 8: RD refinement for I/P-frames\r\n"
        "                                  - 9: RD refinement for all frames\r\n"
        "                                  - 10: QP-RD - requires trellis=2, aq-mode>0\r\n" );
    else H1( "                                  decision quality: 1=fast, 10=best.\r\n"  );
    H1( "      --psy-rd                Strength of psychovisual optimization [\"%.1f:%.1f\"]\r\n"
        "                                  #1: RD (requires subme>=6)\r\n"
        "                                  #2: Trellis (requires trellis, experimental)\r\n",
                                       defaults->analyse.f_psy_rd, defaults->analyse.f_psy_trellis );
    H2( "      --no-psy                Disable all visual optimizations that worsen\r\n"
        "                              both PSNR and SSIM.\r\n" );
    H2( "      --no-mixed-refs         Don't decide references on a per partition basis\r\n" );
    H2( "      --no-chroma-me          Ignore chroma in motion estimation\r\n" );
    H1( "      --no-8x8dct             Disable adaptive spatial transform size\r\n" );
    H1( "  -t, --trellis <integer>     Trellis RD quantization. Requires CABAC. [%d]\r\n"
        "                                  - 0: disabled\r\n"
        "                                  - 1: enabled only on the final encode of a MB\r\n"
        "                                  - 2: enabled on all mode decisions\r\n", defaults->analyse.i_trellis );
    H2( "      --no-fast-pskip         Disables early SKIP detection on P-frames\r\n" );
    H2( "      --no-dct-decimate       Disables coefficient thresholding on P-frames\r\n" );
    H1( "      --nr <integer>          Noise reduction [%d]\r\n", defaults->analyse.i_noise_reduction );
    H2( "\r\n" );
    H2( "      --deadzone-inter <int>  Set the size of the inter luma quantization deadzone [%d]\r\n", defaults->analyse.i_luma_deadzone[0] );
    H2( "      --deadzone-intra <int>  Set the size of the intra luma quantization deadzone [%d]\r\n", defaults->analyse.i_luma_deadzone[1] );
    H2( "                                  Deadzones should be in the range 0 - 32.\r\n" );
    H2( "      --cqm <string>          Preset quant matrices [\"flat\"]\r\n"
        "                                  - jvt, flat\r\n" );
    H1( "      --cqmfile <string>      Read custom quant matrices from a JM-compatible file\r\n" );
    H2( "                                  Overrides any other --cqm* options.\r\n" );
    H2( "      --cqm4 <list>           Set all 4x4 quant matrices\r\n"
        "                                  Takes a comma-separated list of 16 integers.\r\n" );
    H2( "      --cqm8 <list>           Set all 8x8 quant matrices\r\n"
        "                                  Takes a comma-separated list of 64 integers.\r\n" );
    H2( "      --cqm4i, --cqm4p, --cqm8i, --cqm8p\r\n"
        "                              Set both luma and chroma quant matrices\r\n" );
    H2( "      --cqm4iy, --cqm4ic, --cqm4py, --cqm4pc\r\n"
        "                              Set individual quant matrices\r\n" );
    H2( "\r\n" );
    H2( "Video Usability Info (Annex E):\r\n" );
    H2( "The VUI settings are not used by the encoder but are merely suggestions to\r\n" );
    H2( "the playback equipment. See doc/vui.txt for details. Use at your own risk.\r\n" );
    H2( "\r\n" );
    H2( "      --overscan <string>     Specify crop overscan setting [\"%s\"]\r\n"
        "                                  - undef, show, crop\r\n",
                                       strtable_lookup( x264_overscan_names, defaults->vui.i_overscan ) );
    H2( "      --videoformat <string>  Specify video format [\"%s\"]\r\n"
        "                                  - component, pal, ntsc, secam, mac, undef\r\n",
                                       strtable_lookup( x264_vidformat_names, defaults->vui.i_vidformat ) );
    H2( "      --fullrange <string>    Specify full range samples setting [\"%s\"]\r\n"
        "                                  - off, on\r\n",
                                       strtable_lookup( x264_fullrange_names, defaults->vui.b_fullrange ) );
    H2( "      --colorprim <string>    Specify color primaries [\"%s\"]\r\n"
        "                                  - undef, bt709, bt470m, bt470bg\r\n"
        "                                    smpte170m, smpte240m, film\r\n",
                                       strtable_lookup( x264_colorprim_names, defaults->vui.i_colorprim ) );
    H2( "      --transfer <string>     Specify transfer characteristics [\"%s\"]\r\n"
        "                                  - undef, bt709, bt470m, bt470bg, linear,\r\n"
        "                                    log100, log316, smpte170m, smpte240m\r\n",
                                       strtable_lookup( x264_transfer_names, defaults->vui.i_transfer ) );
    H2( "      --colormatrix <string>  Specify color matrix setting [\"%s\"]\r\n"
        "                                  - undef, bt709, fcc, bt470bg\r\n"
        "                                    smpte170m, smpte240m, GBR, YCgCo\r\n",
                                       strtable_lookup( x264_colmatrix_names, defaults->vui.i_colmatrix ) );
    H2( "      --chromaloc <integer>   Specify chroma sample location (0 to 5) [%d]\r\n",
                                       defaults->vui.i_chroma_loc );

    H2( "      --nal-hrd <string>      Signal HRD information (requires vbv-bufsize)\r\n"
        "                                  - none, vbr, cbr (cbr not allowed in .mp4)\r\n" );
    H2( "      --pic-struct            Force pic_struct in Picture Timing SEI\r\n" );

    H0( "\r\n" );
    H0( "Input/Output:\r\n" );
    H0( "\r\n" );
    H0( "  -o, --output                Specify output file\r\n" );
    H1( "      --muxer <string>        Specify output container format [\"%s\"]\r\n"
        "                                  - %s\r\n", muxer_names[0], stringify_names( buf, muxer_names ) );
    H0( "      --sar width:height      Specify Sample Aspect Ratio\r\n" );
    H0( "      --fps <float|rational>  Specify framerate\r\n" );
    H0( "      --level <string>        Specify level (as defined by Annex A)\r\n" );
    H1( "\r\n" );
    H1( "  -v, --verbose               Print stats for each frame\r\n" );
    H0( "      --quiet                 Quiet Mode\r\n" );
    H1( "      --psnr                  Enable PSNR computation\r\n" );
    H1( "      --ssim                  Enable SSIM computation\r\n" );
    H1( "      --threads <integer>     Force a specific number of threads\r\n" );
    H2( "      --sliced-threads        Low-latency but lower-efficiency threading\r\n" );
    H2( "      --sync-lookahead <integer> Number of buffer frames for threaded lookahead\r\n" );
    H2( "      --non-deterministic     Slightly improve quality of SMP, at the cost of repeatability\r\n" );
    H2( "      --asm <integer>         Override CPU detection\r\n" );
    H2( "      --no-asm                Disable all CPU optimizations\r\n" );
    H2( "      --dump-yuv <string>     Save reconstructed frames\r\n" );
    H2( "      --sps-id <integer>      Set SPS and PPS id numbers [%d]\r\n", defaults->i_sps_id );
    H2( "      --aud                   Use access unit delimiters\r\n" );
    H2( "      --force-cfr             Force constant framerate timestamp generation\r\n" );
    H0( "\r\n" );
}

static void update_help_layout(HWND hDlg)
{
    RECT rcDlg, rcNewDlg, rcItem, rcNewItem;
    rcDlg.left   = 0;
    rcDlg.top    = 0;
    rcDlg.right  = HELP_DLG_WIDTH;
    rcDlg.bottom = HELP_DLG_HEIGHT;
    MapDialogRect(hDlg, &rcDlg);
    GetClientRect(hDlg, &rcNewDlg);
    //HELP_IDC_CONSOLE
    rcItem.left   = HELP_CONS_X;
    rcItem.top    = HELP_CONS_Y;
    rcItem.right  = HELP_CONS_X + HELP_CONS_WIDTH;
    rcItem.bottom = HELP_CONS_Y + HELP_CONS_HEIGHT;
    MapDialogRect(hDlg, &rcItem);
    rcNewItem.left   = rcItem.left   + rcNewDlg.left   - rcDlg.left;
    rcNewItem.top    = rcItem.top    + rcNewDlg.top    - rcDlg.top;
    rcNewItem.right  = rcItem.right  + rcNewDlg.right  - rcDlg.right;
    rcNewItem.bottom = rcItem.bottom + rcNewDlg.bottom - rcDlg.bottom;
    MoveWindow(GetDlgItem(hDlg, HELP_IDC_CONSOLE), rcNewItem.left, rcNewItem.top, rcNewItem.right - rcNewItem.left, rcNewItem.bottom - rcNewItem.top, TRUE);
    //HELP_IDC_OK
    rcItem.left   = HELP_OK_X;
    rcItem.top    = HELP_OK_Y;
    rcItem.right  = HELP_OK_X + HELP_OK_WIDTH;
    rcItem.bottom = HELP_OK_Y + HELP_OK_HEIGHT;
    MapDialogRect(hDlg, &rcItem);
    rcNewItem.left   = rcItem.left   + (rcNewDlg.left - rcDlg.left + rcNewDlg.right - rcDlg.right + 1) / 2;
    rcNewItem.top    = rcItem.top    + rcNewDlg.bottom - rcDlg.bottom;
    rcNewItem.right  = rcItem.right  + (rcNewDlg.left - rcDlg.left + rcNewDlg.right - rcDlg.right + 1) / 2;
    rcNewItem.bottom = rcItem.bottom + rcNewDlg.bottom - rcDlg.bottom;
    MoveWindow(GetDlgItem(hDlg, HELP_IDC_OK), rcNewItem.left, rcNewItem.top, rcNewItem.right - rcNewItem.left, rcNewItem.bottom - rcNewItem.top, TRUE);
    //HELP_IDC_COPY
    rcItem.left   = HELP_COPY_X;
    rcItem.top    = HELP_COPY_Y;
    rcItem.right  = HELP_COPY_X + HELP_COPY_WIDTH;
    rcItem.bottom = HELP_COPY_Y + HELP_COPY_HEIGHT;
    MapDialogRect(hDlg, &rcItem);
    rcNewItem.left   = rcItem.left   + (rcNewDlg.left - rcDlg.left + rcNewDlg.right - rcDlg.right + 1) / 2;
    rcNewItem.top    = rcItem.top    + rcNewDlg.bottom - rcDlg.bottom;
    rcNewItem.right  = rcItem.right  + (rcNewDlg.left - rcDlg.left + rcNewDlg.right - rcDlg.right + 1) / 2;
    rcNewItem.bottom = rcItem.bottom + rcNewDlg.bottom - rcDlg.bottom;
    MoveWindow(GetDlgItem(hDlg, HELP_IDC_COPY), rcNewItem.left, rcNewItem.top, rcNewItem.right - rcNewItem.left, rcNewItem.bottom - rcNewItem.top, TRUE);
}

INT_PTR CALLBACK callback_help(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
        case WM_INITDIALOG:
        {
            HFONT hfont = (HFONT)GetStockObject(SYSTEM_FIXED_FONT);
            char *buffer = malloc(65536);

            if (hfont)
                SendDlgItemMessage(hDlg, HELP_IDC_CONSOLE, WM_SETFONT, (WPARAM)hfont, MAKELPARAM(TRUE, 0));
            if (buffer)
            {
                strcpy(buffer, "");
                Help(buffer, 2);
                SendDlgItemMessage(hDlg, HELP_IDC_CONSOLE, WM_SETTEXT, 0, (LPARAM)buffer);
                free(buffer);
            }
            return FALSE;
        }

        case WM_COMMAND:
            if (HIWORD(wParam) == BN_CLICKED)
            {
                switch (LOWORD(wParam))
                {
                    case IDOK:
                    case IDCANCEL:
                    case HELP_IDC_OK:
                        ShowWindow(hDlg, SW_HIDE);
                        break;

                    case HELP_IDC_COPY:
                        if (OpenClipboard(hDlg))
                        {
                            int text_size = SendDlgItemMessage(hDlg, HELP_IDC_CONSOLE, WM_GETTEXTLENGTH, 0, 0);

                            EmptyClipboard();
                            if (text_size > 0)
                            {
                                HGLOBAL clipbuffer;

                                /* Add terminating NULL */
                                text_size += 1;

                                clipbuffer = GlobalAlloc(GMEM_MOVEABLE | GMEM_DDESHARE, text_size * sizeof(char));
                                if (clipbuffer)
                                {
                                    char *buffer = (char *)GlobalLock(clipbuffer);

                                    if (buffer)
                                    {
                                        if (SendDlgItemMessage(hDlg, HELP_IDC_CONSOLE, WM_GETTEXT, (WPARAM)text_size, (LPARAM)buffer) <= 0)
                                            strcpy(buffer, "");

                                        GlobalUnlock(clipbuffer);
                                        if (!SetClipboardData(CF_TEXT, clipbuffer))
                                            GlobalFree(clipbuffer);
                                    }
                                    else
                                        GlobalFree(clipbuffer);
                                }
                            }
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

        case WM_SIZE:
            if (wParam != SIZE_MINIMIZED)
                update_help_layout(hDlg);
            break;

        case WM_SIZING:
        {
            RECT *rc = (RECT *)lParam;
            int width = rc->right - rc->left;
            int height = rc->bottom - rc->top;
            if (width < HELP_DLG_MIN_WIDTH)
                switch (wParam)
                {
                    case WMSZ_LEFT:
                    case WMSZ_TOPLEFT:
                    case WMSZ_BOTTOMLEFT:
                        rc->left = rc->right - HELP_DLG_MIN_WIDTH;
                        break;

                    default:
                        rc->right = rc->left + HELP_DLG_MIN_WIDTH;
                        break;
                }
            if (height < HELP_DLG_MIN_HEIGHT)
                switch (wParam)
                {
                    case WMSZ_TOP:
                    case WMSZ_TOPLEFT:
                    case WMSZ_TOPRIGHT:
                        rc->top = rc->bottom - HELP_DLG_MIN_HEIGHT;
                        break;

                    default:
                        rc->bottom = rc->top + HELP_DLG_MIN_HEIGHT;
                        break;
                }
            break;
        }

        default:
            return FALSE;
    }

    return TRUE;
}
