// -*- Mode: c++ -*-
/*******************************************************************
 * H264Parser
 *
 * Distributed as part of MythTV (www.mythtv.org)
 *
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License, or
 *      (at your option) any later version.
 *
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *
 *      You should have received a copy of the GNU General Public License
 *      along with this program; if not, write to the Free Software
 *      Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 ********************************************************************/

#ifndef H264PARSER_H
#define H264PARSER_H

#include <stdint.h>
#include "compat.h" // for uint on Darwin, MinGW

extern "C" {
#include <limits.h> // golomb.h should include this...
#include "golomb.h"
}

class H264Parser {
  public:

    // ITU-T Rec. H.264 table 7-1
    enum NAL_unit_type {
        UNKNOWN         = 0,
        SLICE           = 1,
        SLICE_DPA       = 2,
        SLICE_DPB       = 3,
        SLICE_DPC       = 4,
        SLICE_IDR       = 5,
        SEI             = 6,
        SPS             = 7,
        PPS             = 8,
        AU_DELIMITER    = 9,
        END_SEQUENCE    = 10,
        END_STREAM      = 11,
        FILLER_DATA     = 12,
        SPS_EXT         = 13,
        AUXILIARY_SLICE = 19
    };

    /*
      slice_type values in the range 5..9 specify, in addition to the
      coding type of the current slice, that all other slices of the
      current coded picture shall have a value of slice_type equal to
      the current value of slice_type or equal to the current value of
      slice_type – 5.
    */
    enum SLICE_type {
        SLICE_P = 0,
        SLICE_B = 1,
        SLICE_I = 2,
        SLICE_SP = 3,
        SLICE_SI = 4,
        SLICE_P_a = 5,
        SLICE_B_a = 6,
        SLICE_I_a = 7,
        SLICE_SP_a = 8,
        SLICE_SI_a = 9,
        SLICE_UNDEF = 10
    };

    enum frame_type {
        FRAME = 'F',
        FIELD_TOP = 'T',
        FIELD_BOTTOM = 'B'
    };

    H264Parser(void);
    ~H264Parser(void) {;}

    uint32_t addBytes(const uint8_t  *bytes,
                      const uint32_t  byte_count,
                      const uint64_t  stream_offset);
    void Reset(void);

    bool stateChanged(void) const { return state_changed; }

    // seenIDR implies that a SPS has also been seen
    bool seenIDR(void) const { return seen_IDR; }
    uint8_t lastNALtype(void) const { return NAL_type; }

    frame_type FieldType(void) const
        {
            if (bottom_field_flag == -1)
                return FRAME;
            else
                return bottom_field_flag ? FIELD_BOTTOM : FIELD_TOP;
        }

    bool onFrameStart(void) const { return on_frame; }
    bool onKeyFrameStart(void) const { return on_key_frame; }

    uint pictureWidth(void) const { return pic_width; }
    uint pictureHeight(void) const { return pic_height; }

    /** \brief Computes aspect ratio from picture size and sample aspect ratio
     */
    uint aspectRatio(void) const;

    uint64_t frameAUstreamOffset(void) const {return frame_start_offset;}
    uint64_t keyframeAUstreamOffset(void) const {return keyframe_start_offset;}

    static int isKeySlice(uint slice_type)
        {
            return (slice_type == SLICE_I   ||
                    slice_type == SLICE_SI  ||
                    slice_type == SLICE_I_a ||
                    slice_type == SLICE_SI_a);
        }

    static bool NALisSlice(uint8_t nal_type)
        {
            return (nal_type == SLICE ||
                    nal_type == SLICE_DPA ||
                    nal_type == SLICE_IDR);
        }

    void waitForIDR(bool wait) { wait_for_IDR = wait; }

  private:
    enum constants {EXTENDED_SAR = 255};

    bool is_first_VCL_NAL_unit(void);
    bool new_AU(void);
    bool decode_Header(GetBitContext *gb);
    void decode_SPS(GetBitContext *gb);
    void decode_PPS(GetBitContext * gb);
    void vui_parameters(GetBitContext * gb);

    bool       find_AU;
    bool       AU_pending;
    bool       state_changed;
    bool       seen_sps;
    bool       seen_IDR;

    uint32_t   sync_accumulator;
    GetBitContext gb;

    uint8_t    NAL_type;

    int        prev_frame_num, frame_num;
    uint       slice_type;
    int        prev_pic_parameter_set_id, pic_parameter_set_id;
    int8_t     prev_field_pic_flag, field_pic_flag;
    int8_t     prev_bottom_field_flag, bottom_field_flag;
    uint8_t    prev_nal_ref_idc, nal_ref_idc;
    uint8_t    prev_pic_order_cnt_type, pic_order_cnt_type;
    int        prev_pic_order_cnt_lsb, pic_order_cnt_lsb;
    int        prev_delta_pic_order_cnt_bottom, delta_pic_order_cnt_bottom;
    int        prev_delta_pic_order_cnt[2], delta_pic_order_cnt[2];
    uint8_t    prev_nal_unit_type, nal_unit_type;
    uint       prev_idr_pic_id, idr_pic_id;

    uint       log2_max_frame_num, log2_max_pic_order_cnt_lsb;
    uint       seq_parameter_set_id;

    uint8_t    delta_pic_order_always_zero_flag;
    uint8_t    separate_colour_plane_flag;
    int8_t     frame_mbs_only_flag;
    int8_t     pic_order_present_flag;
    int8_t     redundant_pic_cnt_present_flag;

    uint       num_ref_frames;
    uint       redundant_pic_cnt;
//    uint       pic_width_in_mbs, pic_height_in_map_units;
    uint       pic_width, pic_height;
    uint       frame_crop_left_offset;
    uint       frame_crop_right_offset;
    uint       frame_crop_top_offset;
    uint       frame_crop_bottom_offset;
    uint8_t    aspect_ratio_idc;
    uint       sar_width, sar_height;

    uint64_t   AU_offset, frame_start_offset, keyframe_start_offset;
    bool       on_frame, on_key_frame;

    bool       wait_for_IDR;
};

#endif /* H264PARSER_H */
