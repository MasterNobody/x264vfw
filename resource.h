//{{NO_DEPENDENCIES}}
// Microsoft Visual C++ generated include file.
// Used by resource.rc
//
#include "x264vfw_config.h"

#define IDD_ABOUT                       101
#define IDD_ERRCONSOLE                  102
#define IDD_CONFIG                      103
#define IDD_TAB_MAIN                    104
#define IDB_X264_LOGO                   105
#if X264VFW_USE_DECODER
#define IDB_FFMPEG_LOGO                 106
#endif
#define IDD_TAB_ANALYSIS_ENC            107
#define IDD_TAB_RC_OTHER                108
#define IDC_ABOUT_BUILD_LABEL           1001
#define IDC_ABOUT_HOMEPAGE              1002
#define IDC_ERRCONSOLE_CONSOLE          1003
#define IDC_ERRCONSOLE_COPYCLIP         1004
#define IDC_CONFIG_TABCONTROL           1005
#define IDC_CONFIG_USE_CMDLINE          1006
#define IDC_CONFIG_CMDLINE              1007
#define IDC_CONFIG_DEFAULTS             1008
#define IDC_MAIN_BUILD_LABEL            1009
#define IDC_MAIN_RC_MODE                1010
#define IDC_MAIN_RC_LABEL               1011
#define IDC_MAIN_RC_VAL                 1012
#define IDC_MAIN_RC_VAL_SLIDER          1013
#define IDC_MAIN_RC_LOW_LABEL           1014
#define IDC_MAIN_RC_HIGH_LABEL          1015
#define IDC_MAIN_STATS_UPDATE           1016
#define IDC_MAIN_STATS                  1017
#define IDC_MAIN_STATS_BROWSE           1018
#define IDC_MAIN_EXTRA_CMDLINE          1019
#define IDC_MISC_LEVEL                  1020
#define IDC_MISC_SAR_W                  1021
#define IDC_MISC_SAR_H                  1022
#define IDC_DEBUG_LOG_LEVEL             1023
#define IDC_DEBUG_PSNR                  1024
#define IDC_DEBUG_SSIM                  1025
#define IDC_DEBUG_NO_ASM                1026
#define IDC_VFW_FOURCC                  1027
#if X264VFW_USE_VIRTUALDUB_HACK
#define IDC_VFW_VD_HACK                 1028
#endif
#if X264VFW_USE_DECODER
#define IDC_VFW_DISABLE_DECODER         1029
#endif
#define IDC_ANALYSIS_8X8DCT             1030
#define IDC_ANALYSIS_I_I8X8             1031
#define IDC_ANALYSIS_I_I4X4             1032
#define IDC_ANALYSIS_PB_I8X8            1033
#define IDC_ANALYSIS_PB_I4X4            1034
#define IDC_ANALYSIS_PB_P8X8            1035
#define IDC_ANALYSIS_PB_P4X4            1036
#define IDC_ANALYSIS_PB_B8X8            1037
#define IDC_ANALYSIS_FAST_PSKIP         1038
#define IDC_ANALYSIS_REF                1039
#define IDC_ANALYSIS_REF_SPIN           1040
#define IDC_ANALYSIS_MIXED_REFS         1041
#define IDC_ANALYSIS_ME                 1042
#define IDC_ANALYSIS_MERANGE            1043
#define IDC_ANALYSIS_MERANGE_SPIN       1044
#define IDC_ANALYSIS_SUBME              1045
#define IDC_ANALYSIS_CHROMA_ME          1046
#define IDC_ANALYSIS_PSY_RDO            1047
#define IDC_ANALYSIS_MIN_KEYINT         1048
#define IDC_ANALYSIS_KEYINT             1049
#define IDC_ANALYSIS_SCENECUT           1050
#define IDC_ANALYSIS_SCENECUT_SPIN      1051
#define IDC_ANALYSIS_PRE_SCENECUT       1052
#define IDC_ANALYSIS_BFRAMES            1053
#define IDC_ANALYSIS_BFRAMES_SPIN       1054
#define IDC_ANALYSIS_B_ADAPT            1055
#define IDC_ANALYSIS_B_BIAS             1056
#define IDC_ANALYSIS_B_BIAS_SPIN        1057
#define IDC_ANALYSIS_DIRECT             1058
#define IDC_ANALYSIS_B_PYRAMID          1059
#define IDC_ANALYSIS_WEIGHTB            1060
#define IDC_ENC_DEBLOCK                 1061
#define IDC_ENC_DEBLOCK_A               1062
#define IDC_ENC_DEBLOCK_A_SPIN          1063
#define IDC_ENC_DEBLOCK_B               1064
#define IDC_ENC_DEBLOCK_B_SPIN          1065
#define IDC_ENC_INTERLACED              1066
#define IDC_ENC_CABAC                   1067
#define IDC_ENC_DCT_DECIMATE            1068
#define IDC_ENC_NR                      1069
#define IDC_ENC_TRELLIS                 1070
#define IDC_ENC_DEADZONE_INTRA          1071
#define IDC_ENC_DEADZONE_INTRA_SPIN     1072
#define IDC_ENC_DEADZONE_INTER          1073
#define IDC_ENC_DEADZONE_INTER_SPIN     1074
#define IDC_ENC_CQM                     1075
#define IDC_ENC_CQMFILE                 1076
#define IDC_ENC_CQMFILE_BROWSE          1077
#define IDC_ENC_PSY_TRELLIS             1078
#define IDC_RC_VBV_MAXRATE              1079
#define IDC_RC_VBV_BUFSIZE              1080
#define IDC_RC_VBV_INIT                 1081
#define IDC_RC_VBV_INIT_SPIN            1082
#define IDC_RC_QPMIN                    1083
#define IDC_RC_QPMIN_SPIN               1084
#define IDC_RC_QPMAX                    1085
#define IDC_RC_QPMAX_SPIN               1086
#define IDC_RC_QPSTEP                   1087
#define IDC_RC_QPSTEP_SPIN              1088
#define IDC_RC_IPRATIO                  1089
#define IDC_RC_PBRATIO                  1090
#define IDC_RC_CHROMA_QP_OFFSET         1091
#define IDC_RC_CHROMA_QP_OFFSET_SPIN    1092
#define IDC_RC_QCOMP                    1093
#define IDC_RC_CPLXBLUR                 1094
#define IDC_RC_QBLUR                    1095
#define IDC_RC_RATETOL                  1096
#define IDC_AQ_MODE                     1097
#define IDC_AQ_STRENGTH                 1098
#if X264_PATCH_VAQ_MOD
#define IDC_AQ_METRIC                   1099
#define IDC_AQ_SENSITIVITY              1100
#endif
#if X264VFW_USE_THREADS
#define IDC_MT_THREADS                  1101
#define IDC_MT_THREADS_SPIN             1102
#define IDC_MT_DETERMINISTIC            1103
#if X264_PATCH_THREAD_POOL
#define IDC_MT_THREAD_QUEUE             1104
#define IDC_MT_THREAD_QUEUE_SPIN        1105
#endif
#endif
#define IDC_VUI_OVERSCAN                1106
#define IDC_VUI_VIDEOFORMAT             1107
#define IDC_VUI_FULLRANGE               1108
#define IDC_VUI_COLORPRIM               1109
#define IDC_VUI_TRANSFER                1110
#define IDC_VUI_COLORMATRIX             1111
#define IDC_VUI_CHROMALOC               1112
#define IDC_VUI_CHROMALOC_SPIN          1113

// Next default values for new objects
// 
#ifdef APSTUDIO_INVOKED
#ifndef APSTUDIO_READONLY_SYMBOLS
#define _APS_NEXT_RESOURCE_VALUE        109
#define _APS_NEXT_COMMAND_VALUE         40001
#define _APS_NEXT_CONTROL_VALUE         1114
#define _APS_NEXT_SYMED_VALUE           101
#endif
#endif
