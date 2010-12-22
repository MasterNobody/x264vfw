/*****************************************************************************
 * mp4.c: mp4 muxer using L-SMASH
 *****************************************************************************
 * Copyright (C) 2003-2010 x264 project
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *          Loren Merritt <lorenm@u.washington.edu>
 *          Yusuke Nakamura <muken.the.vfrmaniac@gmail.com>
 *          Takashi Hirata <silverfilain@gmail.com>
 *          golgol7777 <golgol7777@gmail.com>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111, USA.
 *****************************************************************************/

#include "output.h"
#include "mp4/isom.h"
#include "mp4/importer.h" /* FIXME: will be replaced with summary.h */

/*******************/

#define MP4_LOG_ERROR( ... )                x264vfw_cli_log( p_mp4->opt.p_private, "mp4", X264_LOG_ERROR, __VA_ARGS__ )
#define MP4_LOG_WARNING( ... )              x264vfw_cli_log( p_mp4->opt.p_private, "mp4", X264_LOG_WARNING, __VA_ARGS__ )
#define MP4_LOG_INFO( ... )                 x264vfw_cli_log( p_mp4->opt.p_private, "mp4", X264_LOG_INFO, __VA_ARGS__ )

#define MP4_FAIL_IF_ERR( cond, ... )\
if( cond )\
{\
    MP4_LOG_ERROR( __VA_ARGS__ );\
    return -1;\
}

/* For close_file() */
#define MP4_LOG_IF_ERR( cond, ... )\
if( cond )\
{\
    MP4_LOG_ERROR( __VA_ARGS__ );\
}

/* For open_file() */
#define MP4_FAIL_IF_ERR_EX1( cond, ... )\
if( cond )\
{\
    x264vfw_cli_log( opt->p_private, "mp4", X264_LOG_ERROR, __VA_ARGS__ );\
    return -1;\
}

#define MP4_FAIL_IF_ERR_EX2( cond, ... )\
if( cond )\
{\
    remove_mp4_hnd( p_mp4 );\
    x264vfw_cli_log( opt->p_private, "mp4", X264_LOG_ERROR, __VA_ARGS__ );\
    return -1;\
}

/*******************/

typedef struct
{
    cli_output_opt_t opt;

    isom_root_t *p_root;
    int brand_3gpp;
    int brand_qt;
    uint32_t i_track;
    uint32_t i_sample_entry;
    uint64_t i_time_inc;
    int64_t i_start_offset;
    uint32_t i_sei_size;
    uint8_t *p_sei_buffer;
    int i_numframe;
    int64_t i_init_delta;
    int i_delay_frames;
    int i_dts_compress_multiplier;
    int b_no_pasp;
    int b_use_recovery;
} mp4_hnd_t;

/*******************/

static void remove_mp4_hnd( hnd_t handle )
{
    mp4_hnd_t *p_mp4 = handle;
    if( !p_mp4 )
        return;
    if( p_mp4->p_sei_buffer )
    {
        free( p_mp4->p_sei_buffer );
        p_mp4->p_sei_buffer = NULL;
    }
    if( p_mp4->p_root )
    {
        isom_destroy_root( p_mp4->p_root );
        p_mp4->p_root = NULL;
    }
    free( p_mp4 );
}

/*******************/

static int close_file( hnd_t handle, int64_t largest_pts, int64_t second_largest_pts )
{
    mp4_hnd_t *p_mp4 = handle;

    if( !p_mp4 )
        return 0;

    if( p_mp4->p_root )
    {
        /* Flush the rest of samples and add the last sample_delta. */
        uint32_t last_delta = largest_pts - second_largest_pts;
        MP4_LOG_IF_ERR( isom_flush_pooled_samples( p_mp4->p_root, p_mp4->i_track, (last_delta ? last_delta : 1) * p_mp4->i_time_inc ),
                        "failed to flush the rest of samples.\n" );

        /*
         * Declare the explicit time-line mapping.
         * A segment_duration is given by movie timescale, while a media_time that is the start time of this segment
         * is given by not the movie timescale but rather the media timescale.
         * The reason is that ISO media have two time-lines, presentation and media time-line,
         * and an edit maps the presentation time-line to the media time-line.
         * According to QuickTime file format specification and the actual playback in QuickTime Player,
         * if the Edit Box doesn't exist in the track, the ratio of the summation of sample durations and track's duration becomes
         * the track's media_rate so that the entire media can be used by the track.
         * So, we add Edit Box here to avoid this implicit media_rate could distort track's presentation timestamps slightly.
         * Note: Any demuxers should follow the Edit List Box if it exists.
         */
        uint32_t mvhd_timescale = isom_get_movie_timescale( p_mp4->p_root );
        uint32_t mdhd_timescale = isom_get_media_timescale( p_mp4->p_root, p_mp4->i_track );
        double actual_duration  = 0;
        if( mdhd_timescale != 0 ) /* avoid zero division */
        {
            actual_duration = (double)((largest_pts + last_delta) * p_mp4->i_time_inc) * mvhd_timescale / mdhd_timescale;
            MP4_LOG_IF_ERR( isom_create_explicit_timeline_map( p_mp4->p_root, p_mp4->i_track, actual_duration, p_mp4->i_start_offset * p_mp4->i_time_inc, ISOM_NORMAL_EDIT ),
                            "failed to set timeline map for video.\n" );
        }
        else
            MP4_LOG_ERROR( "mdhd timescale is broken.\n" );

        MP4_LOG_IF_ERR( isom_update_bitrate_info( p_mp4->p_root, p_mp4->i_track, p_mp4->i_sample_entry ),
                        "failed to update bitrate information for video.\n" );

        MP4_LOG_IF_ERR( isom_finish_movie( p_mp4->p_root, NULL ), "failed to finish movie.\n" );

        /* Write media data size here. */
        MP4_LOG_IF_ERR( isom_write_mdat_size( p_mp4->p_root ), "failed to write mdat size.\n" );
    }

    remove_mp4_hnd( p_mp4 ); /* including isom_destroy_root( p_mp4->p_root ); */

    return 0;
}

static int open_file( char *psz_filename, hnd_t *p_handle, cli_output_opt_t *opt )
{
    mp4_hnd_t *p_mp4;

    *p_handle = NULL;
    FILE *fh = fopen( psz_filename, "wb" );
    MP4_FAIL_IF_ERR_EX1( !fh, "cannot open output file `%s'.\n", psz_filename );
    int b_regular = x264_is_regular_file( fh );
    fclose( fh );
    MP4_FAIL_IF_ERR_EX1( !b_regular, "MP4 output is incompatible with non-regular file `%s'\n", psz_filename );

    p_mp4 = malloc( sizeof(mp4_hnd_t) );
    MP4_FAIL_IF_ERR_EX1( !p_mp4, "failed to allocate memory for muxer information.\n" );
    memset( p_mp4, 0, sizeof(mp4_hnd_t) );

    memcpy( &p_mp4->opt, opt, sizeof(cli_output_opt_t) );
    p_mp4->b_no_pasp = 0;
    p_mp4->b_use_recovery = 0;

    p_mp4->p_root = isom_create_movie( psz_filename );
    MP4_FAIL_IF_ERR_EX2( !p_mp4->p_root, "failed to create root.\n" );

    char* ext = get_filename_extension( psz_filename );
    if( !strcmp( ext, "3gp" ) )
        p_mp4->brand_3gpp = 1;
    else if( !strcmp( ext, "3g2" ) )
        p_mp4->brand_3gpp = 2;

    *p_handle = p_mp4;

    return 0;
}

static int set_param( hnd_t handle, x264_param_t *p_param )
{
    mp4_hnd_t *p_mp4 = handle;
    uint64_t i_media_timescale;

    p_mp4->i_delay_frames = p_param->i_bframe ? (p_param->i_bframe_pyramid ? 2 : 1) : 0;
    p_mp4->i_dts_compress_multiplier = !!p_mp4->opt.use_dts_compress * p_mp4->i_delay_frames + 1;

    i_media_timescale = p_param->i_timebase_den * p_mp4->i_dts_compress_multiplier;
    p_mp4->i_time_inc = p_param->i_timebase_num * p_mp4->i_dts_compress_multiplier;
    MP4_FAIL_IF_ERR( i_media_timescale > UINT32_MAX, "MP4 media timescale %"PRIu64" exceeds maximum\n", i_media_timescale );

    /* Set brands. */
    uint32_t brands[9] = { ISOM_BRAND_TYPE_ISOM, ISOM_BRAND_TYPE_MP41, ISOM_BRAND_TYPE_MP42, 0, 0, 0, 0, 0, 0 };
    uint32_t minor_version = 0;
    uint32_t brand_count = 3;
    if( p_mp4->brand_3gpp == 1 )
        brands[brand_count++] = ISOM_BRAND_TYPE_3GP6;
    else if( p_mp4->brand_3gpp == 2 )
    {
        brands[brand_count++] = ISOM_BRAND_TYPE_3GP6;
        brands[brand_count++] = ISOM_BRAND_TYPE_3G2A;
        minor_version = 0x00010000;
    }
    if( p_mp4->b_use_recovery )
    {
        brands[brand_count++] = ISOM_BRAND_TYPE_AVC1;   /* sdtp/sgpd/sbgp */
        brands[brand_count++] = ISOM_BRAND_TYPE_ISO4;   /* cslg */
        brands[brand_count++] = ISOM_BRAND_TYPE_QT;     /* tapt/cslg/stps/sdtp */
        p_mp4->brand_qt = 1;
    }
    MP4_FAIL_IF_ERR( isom_set_brands( p_mp4->p_root, brands[2+p_mp4->brand_3gpp], minor_version, brands, brand_count ),
                     "failed to set brands / ftyp.\n" );

    /* Set max duration per chunk. */
    MP4_FAIL_IF_ERR( isom_set_max_chunk_duration( p_mp4->p_root, 0.5 ),
                     "failed to set max duration per chunk.\n" );

    /* Create a video track. */
    p_mp4->i_track = isom_create_track( p_mp4->p_root, ISOM_MEDIA_HANDLER_TYPE_VIDEO );
    MP4_FAIL_IF_ERR( !p_mp4->i_track, "failed to create a video track.\n" );

    /* Set timescale. */
    MP4_FAIL_IF_ERR( isom_set_movie_timescale( p_mp4->p_root, 600 ),
                     "failed to set movie timescale.\n" );
    MP4_FAIL_IF_ERR( isom_set_media_timescale( p_mp4->p_root, p_mp4->i_track, i_media_timescale ),
                     "failed to set media timescale for video.\n" );

    /* Set handler name. */
    MP4_FAIL_IF_ERR( isom_set_media_handler_name( p_mp4->p_root, p_mp4->i_track, "X264 Video Media Handler" ),
                     "failed to set media hander name for video.\n" );
    if( p_mp4->brand_qt )
        MP4_FAIL_IF_ERR( isom_set_data_handler_name( p_mp4->p_root, p_mp4->i_track, "X264 URL Data Handler" ),
                         "failed to set data hander name for video.\n" );

    if( p_mp4->b_use_recovery )
        MP4_FAIL_IF_ERR( isom_create_grouping( p_mp4->p_root, p_mp4->i_track, ISOM_GROUP_TYPE_ROLL ),
                         "failed to create roll recovery grouping\n" );

    /* Add a sample entry. */
    /* FIXME: I think these sample_entry relative stuff should be more encapsulated, by using video_summary. */
    p_mp4->i_sample_entry = isom_add_sample_entry( p_mp4->p_root, p_mp4->i_track, ISOM_CODEC_TYPE_AVC1_VIDEO, NULL );
    MP4_FAIL_IF_ERR( !p_mp4->i_sample_entry,
                     "failed to add sample entry for video.\n" );
    MP4_FAIL_IF_ERR( isom_add_btrt( p_mp4->p_root, p_mp4->i_track, p_mp4->i_sample_entry ),
                     "failed to add btrt.\n" );
    MP4_FAIL_IF_ERR( isom_set_sample_resolution( p_mp4->p_root, p_mp4->i_track, p_mp4->i_sample_entry, (uint16_t)p_param->i_width, (uint16_t)p_param->i_height ),
                     "failed to set sample resolution.\n" );
    uint64_t dw = p_param->i_width << 16;
    uint64_t dh = p_param->i_height << 16;
    if( p_param->vui.i_sar_width && p_param->vui.i_sar_height )
    {
        double sar = (double)p_param->vui.i_sar_width / p_param->vui.i_sar_height;
        if( sar > 1.0 )
            dw *= sar;
        else
            dh /= sar;
        if( !p_mp4->b_no_pasp )
        {
            MP4_FAIL_IF_ERR( isom_set_sample_aspect_ratio( p_mp4->p_root, p_mp4->i_track, p_mp4->i_sample_entry, p_param->vui.i_sar_width, p_param->vui.i_sar_height ),
                             "failed to set sample aspect ratio.\n" );
            MP4_FAIL_IF_ERR( isom_set_scaling_method( p_mp4->p_root, p_mp4->i_track, p_mp4->i_sample_entry, ISOM_SCALING_METHOD_MEET, 0, 0 ),
                             "failed to set scaling method.\n" );
        }
    }
    MP4_FAIL_IF_ERR( isom_set_track_presentation_size( p_mp4->p_root, p_mp4->i_track, dw, dh ),
                     "failed to set presentation size.\n" );
    if( p_mp4->brand_qt && !p_mp4->b_no_pasp )
        MP4_FAIL_IF_ERR( isom_set_track_aperture_modes( p_mp4->p_root, p_mp4->i_track, p_mp4->i_sample_entry ),
                         "failed to set track aperture mode.\n" );

    return 0;
}

static int write_headers( hnd_t handle, x264_nal_t *p_nal )
{
    mp4_hnd_t *p_mp4 = handle;

    uint32_t sps_size = p_nal[0].i_payload - 4;
    uint32_t pps_size = p_nal[1].i_payload - 4;
    uint32_t sei_size = p_nal[2].i_payload;

    uint8_t *sps = p_nal[0].p_payload + 4;
    uint8_t *pps = p_nal[1].p_payload + 4;
    uint8_t *sei = p_nal[2].p_payload;

    MP4_FAIL_IF_ERR( isom_set_avc_config( p_mp4->p_root, p_mp4->i_track, p_mp4->i_sample_entry, 1, sps[1], sps[2], sps[3], 3, 1, X264_BIT_DEPTH-8, X264_BIT_DEPTH-8 ),
                     "failed to set avc config.\n" );
    /* SPS */
    MP4_FAIL_IF_ERR( isom_add_sps_entry( p_mp4->p_root, p_mp4->i_track, p_mp4->i_sample_entry, sps, sps_size ),
                     "failed to add sps.\n" );
    /* PPS */
    MP4_FAIL_IF_ERR( isom_add_pps_entry( p_mp4->p_root, p_mp4->i_track, p_mp4->i_sample_entry, pps, pps_size ),
                     "failed to add pps.\n" );
    /* SEI */
    p_mp4->p_sei_buffer = malloc( sei_size );
    MP4_FAIL_IF_ERR( !p_mp4->p_sei_buffer,
                     "failed to allocate sei transition buffer.\n" );
    memcpy( p_mp4->p_sei_buffer, sei, sei_size );
    p_mp4->i_sei_size = sei_size;

    /* Write ftyp. */
    MP4_FAIL_IF_ERR( isom_write_ftyp( p_mp4->p_root ),
                     "failed to write brands / ftyp.\n" );

    /* Write mdat header. */
    MP4_FAIL_IF_ERR( isom_add_mdat( p_mp4->p_root ), "failed to add mdat.\n" );

    return sei_size + sps_size + pps_size;
}

static int write_frame( hnd_t handle, uint8_t *p_nalu, int i_size, x264_picture_t *p_picture )
{
    mp4_hnd_t *p_mp4 = handle;
    uint64_t dts, cts;

    if( !p_mp4->i_numframe )
        p_mp4->i_start_offset = p_picture->i_dts * -1;

    isom_sample_t *p_sample = isom_create_sample( i_size + p_mp4->i_sei_size );
    MP4_FAIL_IF_ERR( !p_sample,
                     "failed to create a video sample data.\n" );

    if( p_mp4->p_sei_buffer )
    {
        memcpy( p_sample->data, p_mp4->p_sei_buffer, p_mp4->i_sei_size );
        free( p_mp4->p_sei_buffer );
        p_mp4->p_sei_buffer = NULL;
    }

    memcpy( p_sample->data + p_mp4->i_sei_size, p_nalu, i_size );
    p_mp4->i_sei_size = 0;

    if( p_mp4->opt.use_dts_compress )
    {
        if( p_mp4->i_numframe == 1 )
            p_mp4->i_init_delta = (p_picture->i_dts + p_mp4->i_start_offset) * p_mp4->i_time_inc;
        dts = p_mp4->i_numframe > p_mp4->i_delay_frames
            ? p_picture->i_dts * p_mp4->i_time_inc
            : p_mp4->i_numframe * (p_mp4->i_init_delta / p_mp4->i_dts_compress_multiplier);
        cts = p_picture->i_pts * p_mp4->i_time_inc;
    }
    else
    {
        dts = (p_picture->i_dts + p_mp4->i_start_offset) * p_mp4->i_time_inc;
        cts = (p_picture->i_pts + p_mp4->i_start_offset) * p_mp4->i_time_inc;
    }

    p_sample->dts = dts;
    p_sample->cts = cts;
    p_sample->index = p_mp4->i_sample_entry;
    p_sample->prop.sync_point = p_picture->b_keyframe;

    /* Write data per sample. */
    MP4_FAIL_IF_ERR( isom_write_sample( p_mp4->p_root, p_mp4->i_track, p_sample ),
                     "failed to write a video frame.\n" );

    p_mp4->i_numframe++;

    return i_size;
}

const cli_output_t mp4_output = { open_file, set_param, write_headers, write_frame, close_file };
