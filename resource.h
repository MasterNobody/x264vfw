#include "x264vfw_config.h"

#ifndef IDC_STATIC
#define IDC_STATIC (-1)
#endif

#define IDD_ABOUT                               101
#define IDD_LOG                                 102
#define IDD_HELP                                103
#define IDD_CONFIG                              104
#define IDB_X264_LOGO                           105
#if defined(HAVE_FFMPEG)
#define IDB_FFMPEG_LOGO                         106
#endif
#define ABOUT_IDC_BUILD_LABEL                   1001
#define ABOUT_IDC_HOMEPAGE                      1002
#define LOG_IDC_CONSOLE                         1003
#define LOG_IDC_OK                              1004
#define LOG_IDC_COPY                            1005
#define HELP_IDC_CONSOLE                        1006
#define HELP_IDC_OK                             1007
#define HELP_IDC_COPY                           1008
#define IDC_PRESET                              1009
#define IDC_TUNING                              1010
#define IDC_PROFILE                             1011
#define IDC_LEVEL                               1012
#define IDC_TUNE_FASTDECODE                     1013
#define IDC_TUNE_ZEROLATENCY                    1014
#define IDC_RC_MODE                             1015
#define IDC_RC_LABEL                            1016
#define IDC_RC_VAL                              1017
#define IDC_RC_VAL_SLIDER                       1018
#define IDC_RC_LOW_LABEL                        1019
#define IDC_RC_HIGH_LABEL                       1020
#define IDC_STATS_CREATE                        1021
#define IDC_STATS_UPDATE                        1022
#define IDC_STATS_FILE                          1023
#define IDC_STATS_BROWSE                        1024
#define IDC_OUTPUT_MODE                         1025
#define IDC_VFW_FOURCC                          1026
#if X264VFW_USE_VIRTUALDUB_HACK
#define IDC_VFW_VD_HACK                         1027
#endif
#define IDC_OUTPUT_FILE                         1028
#define IDC_OUTPUT_BROWSE                       1029
#define IDC_ENCODER_LABEL                       1030
#define IDC_SAR_W                               1031
#define IDC_SAR_H                               1032
#define IDC_LOG_LEVEL                           1033
#define IDC_PSNR                                1034
#define IDC_SSIM                                1035
#define IDC_NO_ASM                              1036
#if defined(HAVE_FFMPEG) && X264VFW_USE_DECODER
#define IDC_VFW_DISABLE_DECODER                 1037
#endif
#define IDC_EXTRA_CMDLINE                       1038
#define IDC_EXTRA_HELP                          1039
#define IDC_DEFAULTS                            1040
#define IDC_BUILD_DATE                          1041
