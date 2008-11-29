#include <limits.h>

//avcodec include
#include "avcodec.h"
#include "dsputil.h"
#include "mpegvideo.h"
#include "h264.h"
#include "vc1.h"

#undef NDEBUG
#include <assert.h>

#include "vdpau_render.h"

#define ARSIZE(_x_) (sizeof(_x_) / sizeof((_x_)[0]))

uint32_t num_reference_surfaces;

int VDPAU_mpeg_field_start(MpegEncContext *s)
{
    vdpau_render_state_t * render,* last, * next;
    int i;

    render = (vdpau_render_state_t*)s->current_picture.data[2];
    assert(render != NULL);
    if (render == NULL) {
        return -1; //make sure that this is render packet
    }

    /*  fill VdpPictureInfoMPEG1Or2 struct */
    render->info.mpeg.picture_structure = s->picture_structure;
    render->info.mpeg.picture_coding_type = s->pict_type;
    render->info.mpeg.intra_dc_precision = s->intra_dc_precision;
    render->info.mpeg.frame_pred_frame_dct = s->frame_pred_frame_dct;
    render->info.mpeg.concealment_motion_vectors = s->concealment_motion_vectors;
    render->info.mpeg.intra_vlc_format = s->intra_vlc_format;
    render->info.mpeg.alternate_scan = s->alternate_scan;
    render->info.mpeg.q_scale_type = s->q_scale_type;
    render->info.mpeg.top_field_first = s->top_field_first;
    render->info.mpeg.full_pel_forward_vector = s->full_pel[0];  // MPEG-1 only.  Set 0 for MPEG-2
    render->info.mpeg.full_pel_backward_vector = s->full_pel[1]; // MPEG-1 only.  Set 0 for MPEG-2
    render->info.mpeg.f_code[0][0] = s->mpeg_f_code[0][0];             // For MPEG-1 fill both horiz. & vert.
    render->info.mpeg.f_code[0][1] = s->mpeg_f_code[0][1];
    render->info.mpeg.f_code[1][0] = s->mpeg_f_code[1][0];
    render->info.mpeg.f_code[1][1] = s->mpeg_f_code[1][1];
    for (i = 0; i < 64; ++i) {
        render->info.mpeg.intra_quantizer_matrix[i] =  s->intra_matrix[i];
        render->info.mpeg.non_intra_quantizer_matrix[i] = s->inter_matrix[i];
    }

    render->info.mpeg.forward_reference = VDP_INVALID_HANDLE;
    render->info.mpeg.backward_reference = VDP_INVALID_HANDLE;

    switch(s->pict_type){
    case  FF_I_TYPE:
        return 0; // no prediction from other frames
    case  FF_B_TYPE:
        next = (vdpau_render_state_t*)s->next_picture.data[2];
        assert(next!=NULL);
        if (next == NULL) {
            return -1;
        }
        render->info.mpeg.backward_reference = next->surface;
        // no return here, going to set forward prediction
    case  FF_P_TYPE:
        last = (vdpau_render_state_t*)s->last_picture.data[2];
        if (last == NULL) { // FIXME: Does this test make sense?
            last = render; // predict second field from the first
        }
        render->info.mpeg.forward_reference = last->surface;
        return 0;
    }

    return -1;
}

int VDPAU_mpeg_picture_complete(MpegEncContext *s, const uint8_t *buf, int buf_size, int slice_count)
{
    vdpau_render_state_t * render;

    if (!(s->current_picture_ptr))
        return -1;

    render = (vdpau_render_state_t*)s->current_picture_ptr->data[2];
    assert(render != NULL);
    if (render == NULL) {
        return -1; // make sure that this is render packet
    }

    render->bitstreamBuffer.struct_version = VDP_BITSTREAM_BUFFER_VERSION;
    render->bitstreamBuffer.bitstream_bytes = buf_size;
    render->bitstreamBuffer.bitstream = buf;
    render->info.mpeg.slice_count = slice_count;

    if (slice_count > 0) {
        ff_draw_horiz_band(s, 0, s->avctx->height);
    }

    return 0;
}

int VDPAU_h264_set_reference_frames(H264Context *h)
{
    MpegEncContext * s = &h->s;
    vdpau_render_state_t * render, * render_ref;
    VdpReferenceFrameH264 * rf, * rf2;
    Picture * pic;
    int i, list;

    render = (vdpau_render_state_t*)s->current_picture_ptr->data[2];
    assert(render != NULL);
    if (render == NULL)
        return -1; //make sure that this is render packet

    rf = &render->info.h264.referenceFrames[0];
#define H264_RF_COUNT ARSIZE(render->info.h264.referenceFrames)

    for (list = 0; list < 2; ++list) {
        Picture **lp = list ? h->long_ref : h->short_ref;
        int ls = list ? h->long_ref_count : h->short_ref_count;

        for (i = 0; i < ls; ++i) {
            pic = lp[i];
            if (!pic || !pic->reference) {
                continue;
            }

            render_ref = (vdpau_render_state_t*)pic->data[2];
            assert(render_ref != NULL);
            if (render_ref == NULL)
                return -1; //make sure that this is render packet

            rf2 = &render->info.h264.referenceFrames[0];
            while (rf2 != rf) {
                if (
                    (rf2->surface == render_ref->surface)
                    && (rf2->is_long_term == pic->long_ref)
                    && (rf2->frame_idx == pic->frame_num)
                ) {
                    break;
                }
                ++rf2;
            }
            if (rf2 != rf) {
                rf2->top_is_reference |= (pic->reference & PICT_TOP_FIELD) ? VDP_TRUE : VDP_FALSE;
                rf2->bottom_is_reference |= (pic->reference & PICT_BOTTOM_FIELD) ? VDP_TRUE : VDP_FALSE;
                continue;
            }

            if (rf >= &render->info.h264.referenceFrames[H264_RF_COUNT]) {
                continue;
            }

            rf->surface = render_ref->surface;
            rf->is_long_term = pic->long_ref;
            rf->top_is_reference = (pic->reference & PICT_TOP_FIELD) ? VDP_TRUE : VDP_FALSE;
            rf->bottom_is_reference = (pic->reference & PICT_BOTTOM_FIELD) ? VDP_TRUE : VDP_FALSE;
            rf->field_order_cnt[0] = pic->field_poc[0];
            rf->field_order_cnt[1] = pic->field_poc[1];
            rf->frame_idx = pic->frame_num;

            ++rf;
        }
    }

    for (; rf < &render->info.h264.referenceFrames[H264_RF_COUNT]; ++rf) {
        rf->surface = VDP_INVALID_HANDLE;
        rf->is_long_term = 0;
        rf->top_is_reference = 0;
        rf->bottom_is_reference = 0;
        rf->field_order_cnt[0] = 0;
        rf->field_order_cnt[1] = 0;
        rf->frame_idx = 0;
    }

    return 0;
}

int VDPAU_h264_picture_complete(H264Context *h, const uint8_t *buf, int buf_size)
{
    MpegEncContext * s = &h->s;
    vdpau_render_state_t * render;
    int i;

    render = (vdpau_render_state_t*)s->current_picture_ptr->data[2];
    assert(render != NULL);
    if (render == NULL)
        return -1; //make sure that this is render packet

    render->info.h264.slice_count = h->slice_num;

    if (render->info.h264.slice_count < 1)
        return 0;

    for (int i = 0; i < 2; ++i) {
        int foc = s->current_picture_ptr->field_poc[i];
        if (foc == INT_MAX) {
            foc = 0;
        }
        render->info.h264.field_order_cnt[i] = foc;
    }

    render->info.h264.is_reference = s->current_picture_ptr->reference ? VDP_TRUE : VDP_FALSE;
    render->info.h264.frame_num = h->frame_num;
    render->info.h264.field_pic_flag = (s->picture_structure != PICT_FRAME) ? 1 : 0;
    render->info.h264.bottom_field_flag = (s->picture_structure == PICT_BOTTOM_FIELD) ? 1 : 0;
    render->info.h264.num_ref_frames = h->sps.ref_frame_count;
    render->info.h264.mb_adaptive_frame_field_flag = h->sps.mb_aff;
    render->info.h264.constrained_intra_pred_flag = h->pps.constrained_intra_pred;
    render->info.h264.weighted_pred_flag = h->pps.weighted_pred;
    render->info.h264.weighted_bipred_idc = h->pps.weighted_bipred_idc;
    render->info.h264.frame_mbs_only_flag = h->sps.frame_mbs_only_flag;
    render->info.h264.transform_8x8_mode_flag = h->pps.transform_8x8_mode;
    render->info.h264.chroma_qp_index_offset = h->pps.chroma_qp_index_offset[0];
    render->info.h264.second_chroma_qp_index_offset = h->pps.chroma_qp_index_offset[1];
    render->info.h264.pic_init_qp_minus26 = h->pps.init_qp - 26;
    render->info.h264.num_ref_idx_l0_active_minus1 = h->pps.ref_count[0] - 1;
    render->info.h264.num_ref_idx_l1_active_minus1 = h->pps.ref_count[1] - 1;
    render->info.h264.log2_max_frame_num_minus4 = h->sps.log2_max_frame_num - 4;
    render->info.h264.pic_order_cnt_type = h->sps.poc_type;
    render->info.h264.log2_max_pic_order_cnt_lsb_minus4 = h->sps.log2_max_poc_lsb - 4;
    render->info.h264.delta_pic_order_always_zero_flag = h->sps.delta_pic_order_always_zero_flag;
    render->info.h264.direct_8x8_inference_flag = h->sps.direct_8x8_inference_flag;
    render->info.h264.entropy_coding_mode_flag = h->pps.cabac;
    render->info.h264.pic_order_present_flag = h->pps.pic_order_present;
    render->info.h264.deblocking_filter_control_present_flag = h->pps.deblocking_filter_parameters_present;
    render->info.h264.redundant_pic_cnt_present_flag = h->pps.redundant_pic_cnt_present;
    memcpy(render->info.h264.scaling_lists_4x4, h->pps.scaling_matrix4, sizeof(render->info.h264.scaling_lists_4x4));
    memcpy(render->info.h264.scaling_lists_8x8, h->pps.scaling_matrix8, sizeof(render->info.h264.scaling_lists_8x8));

    render->bitstreamBuffer.struct_version = VDP_BITSTREAM_BUFFER_VERSION;
    render->bitstreamBuffer.bitstream_bytes = buf_size;
    render->bitstreamBuffer.bitstream = buf;

    ff_draw_horiz_band(s, 0, s->avctx->height);

    return 0;
}

void VDPAU_h264_set_reference_frames_count(H264Context *h)
{
    num_reference_surfaces = h->sps.ref_frame_count;
}

#undef printf

int VDPAU_vc1_decode_picture(MpegEncContext *s, AVCodecContext *avctx, VC1Context *v, const uint8_t *buf, int buf_size)
{
   // VC1Context *v = avctx->priv_data;
    vdpau_render_state_t * render,* last, * next;

    render = (vdpau_render_state_t*)s->current_picture.data[2];
    assert(render != NULL);
    if (render == NULL) {
        return -1; //make sure that this is render packet
    }
    memset(&(render->info), 0 , sizeof(VdpPictureInfoVC1));
    memset(&(render->bitstreamBuffer), 0, sizeof(VdpBitstreamBuffer));

    /*  fill LvPictureInfoVC1 struct */
    render->info.vc1.frame_coding_mode = v->fcm;
    render->info.vc1.postprocflag = v->postprocflag;
    render->info.vc1.pulldown = v->broadcast;
    render->info.vc1.interlace = v->interlace;
    render->info.vc1.tfcntrflag = v->tfcntrflag;
    render->info.vc1.finterpflag = v->finterpflag;
    render->info.vc1.psf = v->psf;
    render->info.vc1.dquant = v->dquant;
    render->info.vc1.panscan_flag = v->panscanflag;
    render->info.vc1.refdist_flag = v->refdist_flag;
    render->info.vc1.quantizer = v->quantizer_mode;
    render->info.vc1.extended_mv = v->extended_mv;
    render->info.vc1.extended_dmv = v->extended_dmv;
    render->info.vc1.overlap = v->overlap;
    render->info.vc1.vstransform = v->vstransform;
    render->info.vc1.loopfilter = v->s.loop_filter;
    render->info.vc1.fastuvmc = v->fastuvmc;
    render->info.vc1.range_mapy_flag = v->range_mapy_flag;
    render->info.vc1.range_mapy = v->range_mapy;
    render->info.vc1.range_mapuv_flag = v->range_mapuv_flag;
    render->info.vc1.range_mapuv = v->range_mapuv;
    /* Specific to simple/main profile only */
    render->info.vc1.multires = v->multires;
    render->info.vc1.syncmarker = v->s.resync_marker;
    render->info.vc1.rangered = v->rangered;
    render->info.vc1.maxbframes = v->s.max_b_frames;
    /* Presently, making these as 0 */
    render->info.vc1.deblockEnable = 0;
    render->info.vc1.pquant = 0;

    render->info.vc1.forward_reference = VDP_INVALID_HANDLE;
    render->info.vc1.backward_reference = VDP_INVALID_HANDLE;

    switch(s->pict_type){
    case  FF_I_TYPE:
        render->info.vc1.picture_type = 0;
        break;
    case  FF_B_TYPE:
        if (v->bi_type) {
            render->info.vc1.picture_type = 4;
        }
        else {
            render->info.vc1.picture_type = 3;
        }
        break;
    case  FF_P_TYPE:
        render->info.vc1.picture_type = 1;
        break;
    case  FF_BI_TYPE:
        render->info.vc1.picture_type = 4;
        break;
    default:
        return -1;
    }

    switch(s->pict_type){
    case  FF_I_TYPE:
    case  FF_BI_TYPE:
        break;
    case  FF_B_TYPE:
        next = (vdpau_render_state_t*)s->next_picture.data[2];
        assert(next!=NULL);
        if (next == NULL) {
            return -1;
        }
        render->info.vc1.backward_reference = next->surface;
        // no break here, going to set forward prediction
    case  FF_P_TYPE:
        last = (vdpau_render_state_t*)s->last_picture.data[2];
        if (last == NULL) { // FIXME: Does this test make sense?
            last = render; // predict second field from the first
        }
        render->info.vc1.forward_reference = last->surface;
        break;
    default:
        return -1;
    }

    render->bitstreamBuffer.struct_version = VDP_BITSTREAM_BUFFER_VERSION;
    render->bitstreamBuffer.bitstream_bytes = buf_size;
    render->bitstreamBuffer.bitstream = buf;
    // FIXME: I am not sure about how MPlayer calculates slice number.
    render->info.vc1.slice_count = 1;

    ff_draw_horiz_band(s, 0, s->avctx->height);

    return 0;
}

