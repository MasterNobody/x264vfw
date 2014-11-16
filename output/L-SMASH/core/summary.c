/*****************************************************************************
 * summary.c:
 *****************************************************************************
 * Copyright (C) 2010-2014 L-SMASH project
 *
 * Authors: Takashi Hirata <silverfilain@gmail.com>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *****************************************************************************/

/* This file is available under an ISC license. */

#include "common/internal.h" /* must be placed first */

#include <stdlib.h>
#include <string.h>

#include "box.h"

#include "codecs/mp4a.h"
#include "codecs/mp4sys.h"
#include "codecs/description.h"

/***************************************************************************
    summary and AudioSpecificConfig relative tools
***************************************************************************/

/* create AudioSpecificConfig as memory block from summary, and set it into that summary itself */
int lsmash_setup_AudioSpecificConfig( lsmash_audio_summary_t *summary )
{
    if( !summary )
        return LSMASH_ERR_FUNCTION_PARAM;
    lsmash_bs_t* bs = lsmash_bs_create();
    if( !bs )
        return LSMASH_ERR_MEMORY_ALLOC;
    mp4a_AudioSpecificConfig_t *asc =
        mp4a_create_AudioSpecificConfig( summary->aot,
                                         summary->frequency,
                                         summary->channels,
                                         summary->sbr_mode,
                                         NULL,/*FIXME*/
                                         0 /*FIXME*/);
    if( !asc )
    {
        lsmash_bs_cleanup( bs );
        return LSMASH_ERR_NAMELESS;
    }
    mp4a_put_AudioSpecificConfig( bs, asc );
    void *new_asc;
    uint32_t new_length;
    new_asc = lsmash_bs_export_data( bs, &new_length );
    mp4a_remove_AudioSpecificConfig( asc );
    lsmash_bs_cleanup( bs );
    if( !new_asc )
        return LSMASH_ERR_NAMELESS;
    lsmash_codec_specific_t *specific = lsmash_malloc_zero( sizeof(lsmash_codec_specific_t) );
    if( !specific )
    {
        lsmash_free( new_asc );
        return LSMASH_ERR_MEMORY_ALLOC;
    }
    specific->type              = LSMASH_CODEC_SPECIFIC_DATA_TYPE_UNKNOWN;
    specific->format            = LSMASH_CODEC_SPECIFIC_FORMAT_UNSTRUCTURED;
    specific->destruct          = (lsmash_codec_specific_destructor_t)lsmash_free;
    specific->size              = new_length;
    specific->data.unstructured = lsmash_memdup( new_asc, new_length );
    if( !specific->data.unstructured
     || lsmash_add_entry( &summary->opaque->list, specific ) < 0 )
    {
        lsmash_free( new_asc );
        lsmash_destroy_codec_specific_data( specific );
        return LSMASH_ERR_MEMORY_ALLOC;
    }
    return 0;
}

lsmash_summary_t *lsmash_create_summary( lsmash_summary_type summary_type )
{
    size_t summary_size;
    switch( summary_type )
    {
        case LSMASH_SUMMARY_TYPE_VIDEO :
            summary_size = sizeof(lsmash_video_summary_t);
            break;
        case LSMASH_SUMMARY_TYPE_AUDIO :
            summary_size = sizeof(lsmash_audio_summary_t);
            break;
        default :
            summary_size = sizeof(lsmash_summary_t);
            return NULL;
    }
    lsmash_summary_t *summary = (lsmash_summary_t *)lsmash_malloc_zero( summary_size );
    if( !summary )
        return NULL;
    summary->opaque = (lsmash_codec_specific_list_t *)lsmash_malloc_zero( sizeof(lsmash_codec_specific_list_t) );
    if( !summary->opaque )
    {
        lsmash_free( summary );
        return NULL;
    }
    summary->summary_type   = summary_type;
    summary->data_ref_index = 1;
    return summary;
}

void lsmash_cleanup_summary( lsmash_summary_t *summary )
{
    if( !summary )
        return;
    if( summary->opaque )
    {
        for( lsmash_entry_t *entry = summary->opaque->list.head; entry; )
        {
            lsmash_entry_t *next = entry->next;
            lsmash_destroy_codec_specific_data( (lsmash_codec_specific_t *)entry->data );
            lsmash_free( entry );
            entry = next;
        }
        lsmash_free( summary->opaque );
    }
    lsmash_free( summary );
}

int lsmash_add_codec_specific_data( lsmash_summary_t *summary, lsmash_codec_specific_t *specific )
{
    if( !summary || !summary->opaque || !specific )
        return LSMASH_ERR_FUNCTION_PARAM;
    lsmash_codec_specific_t *dup = isom_duplicate_codec_specific_data( specific );
    if( !dup )
        return LSMASH_ERR_NAMELESS;
    if( lsmash_add_entry( &summary->opaque->list, dup ) < 0 )
    {
        lsmash_destroy_codec_specific_data( dup );
        return LSMASH_ERR_MEMORY_ALLOC;
    }
    return 0;
}

uint32_t lsmash_count_summary( lsmash_root_t *root, uint32_t track_ID )
{
    if( isom_check_initializer_present( root ) < 0 || track_ID == 0 )
        return 0;
    isom_trak_t *trak = isom_get_trak( root->file->initializer, track_ID );
    if( !trak
     || !trak->mdia
     || !trak->mdia->mdhd
     || !trak->mdia->hdlr
     || !trak->mdia->minf
     || !trak->mdia->minf->stbl
     || !trak->mdia->minf->stbl->stsd )
        return 0;
    return trak->mdia->minf->stbl->stsd->list.entry_count;
}

lsmash_summary_t *lsmash_get_summary( lsmash_root_t *root, uint32_t track_ID, uint32_t description_number )
{
    if( isom_check_initializer_present( root ) < 0 || track_ID == 0 || description_number == 0 )
        return NULL;
    isom_trak_t *trak = isom_get_trak( root->file->initializer, track_ID );
    if( !trak
     || !trak->mdia
     || !trak->mdia->mdhd
     || !trak->mdia->hdlr
     || !trak->mdia->minf
     || !trak->mdia->minf->stbl
     || !trak->mdia->minf->stbl->stsd )
        return NULL;
    isom_minf_t *minf = trak->mdia->minf;
    isom_stsd_t *stsd = minf->stbl->stsd;
    uint32_t i = 1;
    for( lsmash_entry_t *entry = stsd->list.head; entry; entry = entry->next )
    {
        if( i != description_number )
        {
            ++i;
            continue;
        }
        isom_sample_entry_t *sample_entry = entry->data;
        if( !sample_entry )
            return NULL;
        if( minf->vmhd )
            return isom_create_video_summary_from_description( sample_entry );
        else if( minf->smhd )
            return isom_create_audio_summary_from_description( sample_entry );
        else
            return NULL;
    }
    return NULL;
}

int lsmash_compare_summary( lsmash_summary_t *a, lsmash_summary_t *b )
{
    if( !a || !b )
        return LSMASH_ERR_FUNCTION_PARAM;
    if( a->summary_type != b->summary_type
     || !lsmash_check_box_type_identical( a->sample_type, b->sample_type ) )
        return 1;
    if( a->summary_type == LSMASH_SUMMARY_TYPE_VIDEO )
    {
        lsmash_video_summary_t *in_video  = (lsmash_video_summary_t *)a;
        lsmash_video_summary_t *out_video = (lsmash_video_summary_t *)b;
        if( in_video->width  != out_video->width
         || in_video->height != out_video->height
         || in_video->depth  != out_video->depth
         || in_video->par_h  != out_video->par_h
         || in_video->par_v  != out_video->par_v
         || memcmp( in_video->compressorname, out_video->compressorname, strlen( in_video->compressorname ) )
         || in_video->clap.width.n             != out_video->clap.width.n
         || in_video->clap.width.d             != out_video->clap.width.d
         || in_video->clap.height.n            != out_video->clap.height.n
         || in_video->clap.height.d            != out_video->clap.height.d
         || in_video->clap.horizontal_offset.n != out_video->clap.horizontal_offset.n
         || in_video->clap.horizontal_offset.d != out_video->clap.horizontal_offset.d
         || in_video->clap.vertical_offset.n   != out_video->clap.vertical_offset.n
         || in_video->clap.vertical_offset.d   != out_video->clap.vertical_offset.d
         || in_video->color.primaries_index != out_video->color.primaries_index
         || in_video->color.transfer_index  != out_video->color.transfer_index
         || in_video->color.matrix_index    != out_video->color.matrix_index
         || in_video->color.full_range      != out_video->color.full_range )
            return 1;
    }
    else if( a->summary_type == LSMASH_SUMMARY_TYPE_AUDIO )
    {
        lsmash_audio_summary_t *in_audio  = (lsmash_audio_summary_t *)a;
        lsmash_audio_summary_t *out_audio = (lsmash_audio_summary_t *)b;
        if( in_audio->frequency        != out_audio->frequency
         || in_audio->channels         != out_audio->channels
         || in_audio->sample_size      != out_audio->sample_size
         || in_audio->samples_in_frame != out_audio->samples_in_frame )
            return 1;
    }
    return isom_compare_opaque_extensions( a, b );
}

lsmash_codec_support_flag lsmash_check_codec_support( lsmash_codec_type_t sample_type )
{
    static struct codec_support_table_tag
    {
        lsmash_codec_type_t       type;
        lsmash_codec_support_flag flags;
    } codec_support_table[160] = { { LSMASH_CODEC_TYPE_INITIALIZER, LSMASH_CODEC_SUPPORT_FLAG_NONE } };
    if( lsmash_check_codec_type_identical( codec_support_table[0].type, LSMASH_CODEC_TYPE_UNSPECIFIED ) )
    {
        /* Initialize the table. */
        int i = 0;
#define ADD_CODEC_SUPPORT_TABLE_ELEMENT( type, flags ) \
    codec_support_table[i++] = (struct codec_support_table_tag){ type, flags }
        ADD_CODEC_SUPPORT_TABLE_ELEMENT( ISOM_CODEC_TYPE_AC_3_AUDIO,    LSMASH_CODEC_SUPPORT_FLAG_REMUX );
        ADD_CODEC_SUPPORT_TABLE_ELEMENT( ISOM_CODEC_TYPE_ALAC_AUDIO,    LSMASH_CODEC_SUPPORT_FLAG_REMUX );
        ADD_CODEC_SUPPORT_TABLE_ELEMENT( ISOM_CODEC_TYPE_DTSC_AUDIO,    LSMASH_CODEC_SUPPORT_FLAG_REMUX );
        ADD_CODEC_SUPPORT_TABLE_ELEMENT( ISOM_CODEC_TYPE_DTSH_AUDIO,    LSMASH_CODEC_SUPPORT_FLAG_REMUX );
        ADD_CODEC_SUPPORT_TABLE_ELEMENT( ISOM_CODEC_TYPE_DTSL_AUDIO,    LSMASH_CODEC_SUPPORT_FLAG_REMUX );
        ADD_CODEC_SUPPORT_TABLE_ELEMENT( ISOM_CODEC_TYPE_DTSE_AUDIO,    LSMASH_CODEC_SUPPORT_FLAG_REMUX );
        ADD_CODEC_SUPPORT_TABLE_ELEMENT( ISOM_CODEC_TYPE_EC_3_AUDIO,    LSMASH_CODEC_SUPPORT_FLAG_REMUX );
        ADD_CODEC_SUPPORT_TABLE_ELEMENT( ISOM_CODEC_TYPE_MP4A_AUDIO,    LSMASH_CODEC_SUPPORT_FLAG_REMUX );
        ADD_CODEC_SUPPORT_TABLE_ELEMENT( ISOM_CODEC_TYPE_SAMR_AUDIO,    LSMASH_CODEC_SUPPORT_FLAG_REMUX );
        ADD_CODEC_SUPPORT_TABLE_ELEMENT( ISOM_CODEC_TYPE_SAWB_AUDIO,    LSMASH_CODEC_SUPPORT_FLAG_REMUX );
        ADD_CODEC_SUPPORT_TABLE_ELEMENT( QT_CODEC_TYPE_23NI_AUDIO,      LSMASH_CODEC_SUPPORT_FLAG_REMUX );
        ADD_CODEC_SUPPORT_TABLE_ELEMENT( QT_CODEC_TYPE_MAC3_AUDIO,      LSMASH_CODEC_SUPPORT_FLAG_REMUX );
        ADD_CODEC_SUPPORT_TABLE_ELEMENT( QT_CODEC_TYPE_MAC6_AUDIO,      LSMASH_CODEC_SUPPORT_FLAG_REMUX );
        ADD_CODEC_SUPPORT_TABLE_ELEMENT( QT_CODEC_TYPE_NONE_AUDIO,      LSMASH_CODEC_SUPPORT_FLAG_REMUX );
        ADD_CODEC_SUPPORT_TABLE_ELEMENT( QT_CODEC_TYPE_QCLP_AUDIO,      LSMASH_CODEC_SUPPORT_FLAG_DEMUX );
        ADD_CODEC_SUPPORT_TABLE_ELEMENT( QT_CODEC_TYPE_AGSM_AUDIO,      LSMASH_CODEC_SUPPORT_FLAG_REMUX );
        ADD_CODEC_SUPPORT_TABLE_ELEMENT( QT_CODEC_TYPE_ALAC_AUDIO,      LSMASH_CODEC_SUPPORT_FLAG_REMUX );
        ADD_CODEC_SUPPORT_TABLE_ELEMENT( QT_CODEC_TYPE_ALAW_AUDIO,      LSMASH_CODEC_SUPPORT_FLAG_REMUX );
        ADD_CODEC_SUPPORT_TABLE_ELEMENT( QT_CODEC_TYPE_FL32_AUDIO,      LSMASH_CODEC_SUPPORT_FLAG_REMUX );
        ADD_CODEC_SUPPORT_TABLE_ELEMENT( QT_CODEC_TYPE_FL64_AUDIO,      LSMASH_CODEC_SUPPORT_FLAG_REMUX );
        ADD_CODEC_SUPPORT_TABLE_ELEMENT( QT_CODEC_TYPE_IN24_AUDIO,      LSMASH_CODEC_SUPPORT_FLAG_REMUX );
        ADD_CODEC_SUPPORT_TABLE_ELEMENT( QT_CODEC_TYPE_IN32_AUDIO,      LSMASH_CODEC_SUPPORT_FLAG_REMUX );
        ADD_CODEC_SUPPORT_TABLE_ELEMENT( QT_CODEC_TYPE_LPCM_AUDIO,      LSMASH_CODEC_SUPPORT_FLAG_REMUX );
        ADD_CODEC_SUPPORT_TABLE_ELEMENT( QT_CODEC_TYPE_MP4A_AUDIO,      LSMASH_CODEC_SUPPORT_FLAG_REMUX );
        ADD_CODEC_SUPPORT_TABLE_ELEMENT( QT_CODEC_TYPE_RAW_AUDIO,       LSMASH_CODEC_SUPPORT_FLAG_REMUX );
        ADD_CODEC_SUPPORT_TABLE_ELEMENT( QT_CODEC_TYPE_SOWT_AUDIO,      LSMASH_CODEC_SUPPORT_FLAG_REMUX );
        ADD_CODEC_SUPPORT_TABLE_ELEMENT( QT_CODEC_TYPE_TWOS_AUDIO,      LSMASH_CODEC_SUPPORT_FLAG_REMUX );
        ADD_CODEC_SUPPORT_TABLE_ELEMENT( QT_CODEC_TYPE_ULAW_AUDIO,      LSMASH_CODEC_SUPPORT_FLAG_REMUX );
        ADD_CODEC_SUPPORT_TABLE_ELEMENT( QT_CODEC_TYPE_FULLMP3_AUDIO,   LSMASH_CODEC_SUPPORT_FLAG_REMUX );
        ADD_CODEC_SUPPORT_TABLE_ELEMENT( QT_CODEC_TYPE_ADPCM2_AUDIO,    LSMASH_CODEC_SUPPORT_FLAG_REMUX );
        ADD_CODEC_SUPPORT_TABLE_ELEMENT( QT_CODEC_TYPE_ADPCM17_AUDIO,   LSMASH_CODEC_SUPPORT_FLAG_REMUX );
        ADD_CODEC_SUPPORT_TABLE_ELEMENT( QT_CODEC_TYPE_GSM49_AUDIO,     LSMASH_CODEC_SUPPORT_FLAG_REMUX );
        ADD_CODEC_SUPPORT_TABLE_ELEMENT( QT_CODEC_TYPE_NOT_SPECIFIED,   LSMASH_CODEC_SUPPORT_FLAG_REMUX );
        ADD_CODEC_SUPPORT_TABLE_ELEMENT( ISOM_CODEC_TYPE_AVC1_VIDEO,    LSMASH_CODEC_SUPPORT_FLAG_REMUX );
        ADD_CODEC_SUPPORT_TABLE_ELEMENT( ISOM_CODEC_TYPE_AVC3_VIDEO,    LSMASH_CODEC_SUPPORT_FLAG_REMUX );
        ADD_CODEC_SUPPORT_TABLE_ELEMENT( ISOM_CODEC_TYPE_HVC1_VIDEO,    LSMASH_CODEC_SUPPORT_FLAG_REMUX );
        ADD_CODEC_SUPPORT_TABLE_ELEMENT( ISOM_CODEC_TYPE_HEV1_VIDEO,    LSMASH_CODEC_SUPPORT_FLAG_REMUX );
        ADD_CODEC_SUPPORT_TABLE_ELEMENT( ISOM_CODEC_TYPE_MP4V_VIDEO,    LSMASH_CODEC_SUPPORT_FLAG_MUX );
        ADD_CODEC_SUPPORT_TABLE_ELEMENT( ISOM_CODEC_TYPE_VC_1_VIDEO,    LSMASH_CODEC_SUPPORT_FLAG_REMUX );
        ADD_CODEC_SUPPORT_TABLE_ELEMENT( QT_CODEC_TYPE_2VUY_VIDEO,      LSMASH_CODEC_SUPPORT_FLAG_REMUX );
        ADD_CODEC_SUPPORT_TABLE_ELEMENT( QT_CODEC_TYPE_DV10_VIDEO,      LSMASH_CODEC_SUPPORT_FLAG_REMUX );
        ADD_CODEC_SUPPORT_TABLE_ELEMENT( QT_CODEC_TYPE_DVOO_VIDEO,      LSMASH_CODEC_SUPPORT_FLAG_REMUX );
        ADD_CODEC_SUPPORT_TABLE_ELEMENT( QT_CODEC_TYPE_APCH_VIDEO,      LSMASH_CODEC_SUPPORT_FLAG_REMUX );
        ADD_CODEC_SUPPORT_TABLE_ELEMENT( QT_CODEC_TYPE_APCN_VIDEO,      LSMASH_CODEC_SUPPORT_FLAG_REMUX );
        ADD_CODEC_SUPPORT_TABLE_ELEMENT( QT_CODEC_TYPE_APCS_VIDEO,      LSMASH_CODEC_SUPPORT_FLAG_REMUX );
        ADD_CODEC_SUPPORT_TABLE_ELEMENT( QT_CODEC_TYPE_APCO_VIDEO,      LSMASH_CODEC_SUPPORT_FLAG_REMUX );
        ADD_CODEC_SUPPORT_TABLE_ELEMENT( QT_CODEC_TYPE_AP4H_VIDEO,      LSMASH_CODEC_SUPPORT_FLAG_REMUX );
        ADD_CODEC_SUPPORT_TABLE_ELEMENT( QT_CODEC_TYPE_DVC_VIDEO,       LSMASH_CODEC_SUPPORT_FLAG_REMUX );
        ADD_CODEC_SUPPORT_TABLE_ELEMENT( QT_CODEC_TYPE_DVCP_VIDEO,      LSMASH_CODEC_SUPPORT_FLAG_REMUX );
        ADD_CODEC_SUPPORT_TABLE_ELEMENT( QT_CODEC_TYPE_DVPP_VIDEO,      LSMASH_CODEC_SUPPORT_FLAG_REMUX );
        ADD_CODEC_SUPPORT_TABLE_ELEMENT( QT_CODEC_TYPE_DV5N_VIDEO,      LSMASH_CODEC_SUPPORT_FLAG_REMUX );
        ADD_CODEC_SUPPORT_TABLE_ELEMENT( QT_CODEC_TYPE_DV5P_VIDEO,      LSMASH_CODEC_SUPPORT_FLAG_REMUX );
        ADD_CODEC_SUPPORT_TABLE_ELEMENT( QT_CODEC_TYPE_DVH2_VIDEO,      LSMASH_CODEC_SUPPORT_FLAG_REMUX );
        ADD_CODEC_SUPPORT_TABLE_ELEMENT( QT_CODEC_TYPE_DVH3_VIDEO,      LSMASH_CODEC_SUPPORT_FLAG_REMUX );
        ADD_CODEC_SUPPORT_TABLE_ELEMENT( QT_CODEC_TYPE_DVH5_VIDEO,      LSMASH_CODEC_SUPPORT_FLAG_REMUX );
        ADD_CODEC_SUPPORT_TABLE_ELEMENT( QT_CODEC_TYPE_DVH6_VIDEO,      LSMASH_CODEC_SUPPORT_FLAG_REMUX );
        ADD_CODEC_SUPPORT_TABLE_ELEMENT( QT_CODEC_TYPE_DVHP_VIDEO,      LSMASH_CODEC_SUPPORT_FLAG_REMUX );
        ADD_CODEC_SUPPORT_TABLE_ELEMENT( QT_CODEC_TYPE_DVHQ_VIDEO,      LSMASH_CODEC_SUPPORT_FLAG_REMUX );
        ADD_CODEC_SUPPORT_TABLE_ELEMENT( QT_CODEC_TYPE_FLIC_VIDEO,      LSMASH_CODEC_SUPPORT_FLAG_REMUX );
        ADD_CODEC_SUPPORT_TABLE_ELEMENT( QT_CODEC_TYPE_H261_VIDEO,      LSMASH_CODEC_SUPPORT_FLAG_REMUX );
        ADD_CODEC_SUPPORT_TABLE_ELEMENT( QT_CODEC_TYPE_H263_VIDEO,      LSMASH_CODEC_SUPPORT_FLAG_REMUX );
        ADD_CODEC_SUPPORT_TABLE_ELEMENT( QT_CODEC_TYPE_JPEG_VIDEO,      LSMASH_CODEC_SUPPORT_FLAG_REMUX );
        ADD_CODEC_SUPPORT_TABLE_ELEMENT( QT_CODEC_TYPE_MJPA_VIDEO,      LSMASH_CODEC_SUPPORT_FLAG_REMUX );
        ADD_CODEC_SUPPORT_TABLE_ELEMENT( QT_CODEC_TYPE_MJPB_VIDEO,      LSMASH_CODEC_SUPPORT_FLAG_REMUX );
        ADD_CODEC_SUPPORT_TABLE_ELEMENT( QT_CODEC_TYPE_PNG_VIDEO,       LSMASH_CODEC_SUPPORT_FLAG_REMUX );
        ADD_CODEC_SUPPORT_TABLE_ELEMENT( QT_CODEC_TYPE_RAW_VIDEO,       LSMASH_CODEC_SUPPORT_FLAG_REMUX );
        ADD_CODEC_SUPPORT_TABLE_ELEMENT( QT_CODEC_TYPE_RLE_VIDEO,       LSMASH_CODEC_SUPPORT_FLAG_REMUX );
        ADD_CODEC_SUPPORT_TABLE_ELEMENT( QT_CODEC_TYPE_RPZA_VIDEO,      LSMASH_CODEC_SUPPORT_FLAG_REMUX );
        ADD_CODEC_SUPPORT_TABLE_ELEMENT( QT_CODEC_TYPE_TGA_VIDEO,       LSMASH_CODEC_SUPPORT_FLAG_REMUX );
        ADD_CODEC_SUPPORT_TABLE_ELEMENT( QT_CODEC_TYPE_TIFF_VIDEO,      LSMASH_CODEC_SUPPORT_FLAG_REMUX );
        ADD_CODEC_SUPPORT_TABLE_ELEMENT( QT_CODEC_TYPE_ULRA_VIDEO,      LSMASH_CODEC_SUPPORT_FLAG_REMUX );
        ADD_CODEC_SUPPORT_TABLE_ELEMENT( QT_CODEC_TYPE_ULRG_VIDEO,      LSMASH_CODEC_SUPPORT_FLAG_REMUX );
        ADD_CODEC_SUPPORT_TABLE_ELEMENT( QT_CODEC_TYPE_ULY0_VIDEO,      LSMASH_CODEC_SUPPORT_FLAG_REMUX );
        ADD_CODEC_SUPPORT_TABLE_ELEMENT( QT_CODEC_TYPE_ULY2_VIDEO,      LSMASH_CODEC_SUPPORT_FLAG_REMUX );
        ADD_CODEC_SUPPORT_TABLE_ELEMENT( QT_CODEC_TYPE_ULH0_VIDEO,      LSMASH_CODEC_SUPPORT_FLAG_REMUX );
        ADD_CODEC_SUPPORT_TABLE_ELEMENT( QT_CODEC_TYPE_ULH2_VIDEO,      LSMASH_CODEC_SUPPORT_FLAG_REMUX );
        ADD_CODEC_SUPPORT_TABLE_ELEMENT( QT_CODEC_TYPE_V210_VIDEO,      LSMASH_CODEC_SUPPORT_FLAG_REMUX );
        ADD_CODEC_SUPPORT_TABLE_ELEMENT( QT_CODEC_TYPE_V216_VIDEO,      LSMASH_CODEC_SUPPORT_FLAG_REMUX );
        ADD_CODEC_SUPPORT_TABLE_ELEMENT( QT_CODEC_TYPE_V308_VIDEO,      LSMASH_CODEC_SUPPORT_FLAG_REMUX );
        ADD_CODEC_SUPPORT_TABLE_ELEMENT( QT_CODEC_TYPE_V408_VIDEO,      LSMASH_CODEC_SUPPORT_FLAG_REMUX );
        ADD_CODEC_SUPPORT_TABLE_ELEMENT( QT_CODEC_TYPE_V410_VIDEO,      LSMASH_CODEC_SUPPORT_FLAG_REMUX );
        ADD_CODEC_SUPPORT_TABLE_ELEMENT( QT_CODEC_TYPE_YUV2_VIDEO,      LSMASH_CODEC_SUPPORT_FLAG_REMUX );
        ADD_CODEC_SUPPORT_TABLE_ELEMENT( LSMASH_CODEC_TYPE_UNSPECIFIED, LSMASH_CODEC_SUPPORT_FLAG_NONE );
    }
    for( int i = 0; !lsmash_check_codec_type_identical( codec_support_table[i].type, LSMASH_CODEC_TYPE_UNSPECIFIED ); i++ )
        if( lsmash_check_codec_type_identical( sample_type, codec_support_table[i].type ) )
            return codec_support_table[i].flags;
    return LSMASH_CODEC_SUPPORT_FLAG_NONE;
}
