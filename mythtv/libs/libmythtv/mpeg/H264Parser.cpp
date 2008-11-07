// MythTV headers
#include "H264Parser.h"

extern "C" {
// from libavcodec
    extern const uint8_t *ff_find_start_code(const uint8_t * p, const uint8_t *end, uint32_t * state);
#include "avcodec.h"
}

// #include <math.h>

/*
  Most of the comments below were cut&paste from ITU-T Rec. H.264
  as found here:  http://www.itu.int/rec/T-REC-H.264/e
 */

/*
  Useful definitions:

  * access unit: A set of NAL units always containing exactly one
  primary coded picture. In addition to the primary coded picture, an
  access unit may also contain one or more redundant coded pictures
  or other NAL units not containing slices or slice data partitions
  of a coded picture. The decoding of an access unit always results
  in a decoded picture.

  * instantaneous decoding refresh (IDR) access unit: An access unit in
  which the primary coded picture is an IDR picture.

  * instantaneous decoding refresh (IDR) picture: A coded picture
  containing only slices with I or SI slice types that causes the
  decoding process to mark all reference pictures as "unused for
  reference" immediately after decoding the IDR picture. After the
  decoding of an IDR picture all following coded pictures in decoding
  order can be decoded without inter prediction from any picture
  decoded prior to the IDR picture. The first picture of each coded
  video sequence is an IDR picture.

  * NAL unit: A syntax structure containing an indication of the type
  of data to follow and bytes containing that data in the form of an
  RBSP interspersed as necessary with emulation prevention bytes.

  * raw byte sequence payload (RBSP): A syntax structure containing an
  integer number of bytes that is encapsulated in a NAL unit. An RBSP
  is either empty or has the form of a string of data bits containing
  syntax elements followed by an RBSP stop bit and followed by zero
  or more subsequent bits equal to 0.

  * raw byte sequence payload (RBSP) stop bit: A bit equal to 1 present
  within a raw byte sequence payload (RBSP) after a string of data
  bits. The location of the end of the string of data bits within an
  RBSP can be identified by searching from the end of the RBSP for
  the RBSP stop bit, which is the last non-zero bit in the RBSP.

  * parity: The parity of a field can be top or bottom.

  * picture: A collective term for a field or a frame.

  * picture parameter set: A syntax structure containing syntax
  elements that apply to zero or more entire coded pictures as
  determined by the pic_parameter_set_id syntax element found in each
  slice header.

  * primary coded picture: The coded representation of a picture to be
  used by the decoding process for a bitstream conforming to this
  Recommendation | International Standard. The primary coded picture
  contains all macroblocks of the picture. The only pictures that
  have a normative effect on the decoding process are primary coded
  pictures. See also redundant coded picture.

  * VCL: Video Coding Layer 

  - The VCL is specified to efficiently represent the content of the
  video data. The NAL is specified to format that data and provide
  header information in a manner appropriate for conveyance on a
  variety of communication channels or storage media. All data are
  contained in NAL units, each of which contains an integer number of
  bytes. A NAL unit specifies a generic format for use in both
  packet-oriented and bitstream systems. The format of NAL units for
  both packet-oriented transport and byte stream is identical except
  that each NAL unit can be preceded by a start code prefix and extra
  padding bytes in the byte stream format.

*/

H264Parser::H264Parser(void)
{
    Reset();
}

void H264Parser::Reset(void)
{
    state_changed = false;
    seen_sps = seen_IDR = false;

    sync_accumulator = 0xffffffff;
    find_AU = false;
    AU_pending = false;

    NAL_type = UNKNOWN;

    frame_num = prev_frame_num = -1;
    slice_type = SLICE_UNDEF;
    prev_pic_parameter_set_id = pic_parameter_set_id = -1;
    prev_field_pic_flag = field_pic_flag = -1;
    prev_bottom_field_flag = bottom_field_flag = -1;
    prev_nal_ref_idc = nal_ref_idc = 0;
    prev_pic_order_cnt_type = pic_order_cnt_type = 
    prev_pic_order_cnt_lsb = pic_order_cnt_lsb = 0;
    prev_delta_pic_order_cnt_bottom = delta_pic_order_cnt_bottom = 0;
    prev_delta_pic_order_cnt[0] = delta_pic_order_cnt[0] = 0;
    prev_delta_pic_order_cnt[1] = delta_pic_order_cnt[1] = 0;
    prev_nal_unit_type = nal_unit_type = 0;
    prev_idr_pic_id = idr_pic_id = 0;

    log2_max_frame_num = log2_max_pic_order_cnt_lsb = 0;
    seq_parameter_set_id = 0;

    delta_pic_order_always_zero_flag = 0;
    separate_colour_plane_flag = 0;
    frame_mbs_only_flag = -1;
    pic_order_present_flag = -1;
    redundant_pic_cnt_present_flag = 0;

    num_ref_frames = 0;
    redundant_pic_cnt = 0;

//    pic_width_in_mbs = pic_height_in_map_units = 0;
    pic_width = pic_height = 0;
    frame_crop_left_offset = frame_crop_right_offset = 0;
    frame_crop_top_offset = frame_crop_bottom_offset = 0;
    sar_width = sar_height = 0;

    AU_offset = frame_start_offset = keyframe_start_offset = 0;
    on_frame = on_key_frame = false;
}


bool H264Parser::new_AU(void)
{
    /*
      An access unit consists of one primary coded picture, zero or more
      corresponding redundant coded pictures, and zero or more non-VCL NAL
      units. The association of VCL NAL units to primary or redundant coded
      pictures is described in subclause 7.4.1.2.5.
      
      The first access unit in the bitstream starts with the first NAL unit
      of the bitstream.
      
      The first of any of the following NAL units after the last VCL NAL
      unit of a primary coded picture specifies the start of a new access
      unit.
      
      –    access unit delimiter NAL unit (when present)
      –    sequence parameter set NAL unit (when present)
      –    picture parameter set NAL unit (when present)
      –    SEI NAL unit (when present)
      –    NAL units with nal_unit_type in the range of 14 to 18, inclusive
      –    first VCL NAL unit of a primary coded picture (always present)
    */
    
    /*
      7.4.1.2.4 Detection of the first VCL NAL unit of a primary coded
      picture This subclause specifies constraints on VCL NAL unit syntax
      that are sufficient to enable the detection of the first VCL NAL unit
      of each primary coded picture.
      
      Any coded slice NAL unit or coded slice data partition A NAL unit of
      the primary coded picture of the current access unit shall be
      different from any coded slice NAL unit or coded slice data partition
      A NAL unit of the primary coded picture of the previous access unit in
      one or more of the following ways.
      
      - frame_num differs in value. The value of frame_num used to
      test this condition is the value of frame_num that appears in
      the syntax of the slice header, regardless of whether that value
      is inferred to have been equal to 0 for subsequent use in the
      decoding process due to the presence of
      memory_management_control_operation equal to 5.
          Note: If the current picture is an IDR picture FrameNum and
          PrevRefFrameNum are set equal to 0.
      - pic_parameter_set_id differs in value.
      - field_pic_flag differs in value.
      - bottom_field_flag is present in both and differs in value.
      - nal_ref_idc differs in value with one of the nal_ref_idc values
      being equal to 0.
      - pic_order_cnt_type is equal to 0 for both and either
      pic_order_cnt_lsb differs in value, or delta_pic_order_cnt_bottom
      differs in value.
      - pic_order_cnt_type is equal to 1 for both and either
      delta_pic_order_cnt[0] differs in value, or
      delta_pic_order_cnt[1] differs in value.
      - nal_unit_type differs in value with one of the nal_unit_type values
      being equal to 5.
      - nal_unit_type is equal to 5 for both and idr_pic_id differs in
      value.

      NOTE – Some of the VCL NAL units in redundant coded pictures or some
      non-VCL NAL units (e.g. an access unit delimiter NAL unit) may also
      be used for the detection of the boundary between access units, and
      may therefore aid in the detection of the start of a new primary
      coded picture.

    */

    bool       result = false;

    if (prev_frame_num != -1)
    {
        // Need previous slice information for comparison

        if (NAL_type == AU_DELIMITER ||
            NAL_type == SPS ||
            NAL_type == PPS ||
            NAL_type == SEI ||
            (NAL_type > SPS_EXT && NAL_type < AUXILIARY_SLICE))
            result = true;
        else if (NAL_type != SLICE_IDR && frame_num != prev_frame_num)
            result = true;
        else if (prev_pic_parameter_set_id != -1 &&
                 pic_parameter_set_id != prev_pic_parameter_set_id)
            result = true;
        else if (field_pic_flag != prev_field_pic_flag)
            result = true;
        else if ((bottom_field_flag != -1 && prev_bottom_field_flag != -1) &&
                 bottom_field_flag != prev_bottom_field_flag)
            result = true;
        else if ((nal_ref_idc == 0 || prev_nal_ref_idc == 0) &&
                 nal_ref_idc != prev_nal_ref_idc)
            result = true;
        else if ((pic_order_cnt_type == 0 && prev_pic_order_cnt_type == 0) &&
                 (pic_order_cnt_lsb != prev_pic_order_cnt_lsb ||
                  delta_pic_order_cnt_bottom !=
                  prev_delta_pic_order_cnt_bottom))
            result = true;
        else if ((pic_order_cnt_type == 1 && prev_pic_order_cnt_type == 1) &&
                 (delta_pic_order_cnt[0] != prev_delta_pic_order_cnt[0] ||
                  delta_pic_order_cnt[1] != prev_delta_pic_order_cnt[1]))
            result = true;
        else if ((nal_unit_type == SLICE_IDR ||
                  prev_nal_unit_type == SLICE_IDR) &&
                 nal_unit_type != prev_nal_unit_type)
            result = true;
        else if ((nal_unit_type == SLICE_IDR &&
                  prev_nal_unit_type == SLICE_IDR) &&
                 idr_pic_id != prev_idr_pic_id)
            result = true;
    }

    prev_frame_num = frame_num;
    prev_pic_parameter_set_id = pic_parameter_set_id;
    prev_field_pic_flag = field_pic_flag;
    prev_bottom_field_flag = bottom_field_flag;
    prev_nal_ref_idc = nal_ref_idc;
    prev_pic_order_cnt_lsb = pic_order_cnt_lsb;
    prev_delta_pic_order_cnt_bottom = delta_pic_order_cnt_bottom;
    prev_delta_pic_order_cnt[0] = delta_pic_order_cnt[0];
    prev_delta_pic_order_cnt[1] = delta_pic_order_cnt[1];
    prev_nal_unit_type = nal_unit_type;
    prev_idr_pic_id = idr_pic_id;

    return result;
}

uint32_t H264Parser::addBytes(const uint8_t  *bytes,
                              const uint32_t  byte_count,
                              const uint64_t  stream_offset)
{
    const uint8_t *byteP = bytes;
    const uint8_t *endP = bytes + byte_count;

    uint8_t        first_byte;

    state_changed = false;

    while (byteP < endP)
    {
        byteP = ff_find_start_code(byteP, endP, &sync_accumulator);

        if ((sync_accumulator & 0xffffff00) == 0x00000100)
        {
/*
  nal_unit_type specifies the type of RBSP data structure contained in
  the NAL unit as specified in Table 7-1. VCL NAL units
  are specified as those NAL units having nal_unit_type
  equal to 1 to 5, inclusive. All remaining NAL units
  are called non-VCL NAL units:

  0  Unspecified
  1  Coded slice of a non-IDR picture slice_layer_without_partitioning_rbsp( )
  2  Coded slice data partition A slice_data_partition_a_layer_rbsp( )
  3  Coded slice data partition B slice_data_partition_b_layer_rbsp( )
  4  Coded slice data partition C slice_data_partition_c_layer_rbsp( )
  5  Coded slice of an IDR picture slice_layer_without_partitioning_rbsp( )
  6  Supplemental enhancement information (SEI) 5 sei_rbsp( )
  7  Sequence parameter set (SPS) seq_parameter_set_rbsp( )
  8  Picture parameter set pic_parameter_set_rbsp( )
  9  Access unit delimiter access_unit_delimiter_rbsp( )
  10 End of sequence end_of_seq_rbsp( )
  11 End of stream end_of_stream_rbsp( )
*/
            first_byte = *(byteP - 1);
            NAL_type = first_byte & 0x1f;
            nal_ref_idc = (first_byte >> 5) & 0x3;

            if (NALisSlice(NAL_type) || NAL_type == SPS || NAL_type == PPS)
            {
                /*
                  bitstream buffer, must be FF_INPUT_BUFFER_PADDING_SIZE
                  bytes larger then the actual read bits
                */
                if (byteP + 1 + FF_INPUT_BUFFER_PADDING_SIZE < endP)
                {
                    init_get_bits(&gb, byteP, 8 * (endP - byteP));

                    if (NAL_type == SPS)
                        decode_SPS(&gb);
                    else if (NAL_type == PPS)
                        decode_PPS(&gb);
                    else
                        find_AU = decode_Header(&gb);

                    byteP += (get_bits_count(&gb) / 8);
                }
            }

            if (find_AU && new_AU())
            {
                /* After finding a new AU, don't look for another one
                   until we decode a SLICE */
                find_AU = false;
                AU_pending = true;
                AU_offset = stream_offset;
            }
            
            if (AU_pending && NALisSlice(NAL_type))
            {
                /* Once we know the slice type of a new AU, we can
                 * determine if it is a keyframe or just a frame */

                AU_pending = false;
                state_changed = true;

                on_frame = true;
                frame_start_offset = AU_offset;

                if (seen_IDR && isKeySlice(slice_type))
                {
                    on_key_frame = true;
                    keyframe_start_offset = AU_offset;
                }
                else
                    on_key_frame = false;
            }
            else
                on_frame = on_key_frame = false;

            return byteP - bytes;
        }
    }

    return byteP - bytes;
}

/*
  7.4.3 Slice header semantics
*/
bool H264Parser::decode_Header(GetBitContext *gb)
{
    uint first_mb_in_slice;

    if (log2_max_frame_num == 0 || pic_order_present_flag == -1)
    {
        // SPS or PPS has not been parsed yet
        return false;
    }

    /*
      first_mb_in_slice specifies the address of the first macroblock
      in the slice. When arbitrary slice order is not allowed as
      specified in Annex A, the value of first_mb_in_slice is
      constrained as follows.

      – If separate_colour_plane_flag is equal to 0, the value of
      first_mb_in_slice shall not be less than the value of
      first_mb_in_slice for any other slice of the current picture
      that precedes the current slice in decoding order.

      – Otherwise (separate_colour_plane_flag is equal to 1), the value of
      first_mb_in_slice shall not be less than the value of
      first_mb_in_slice for any other slice of the current picture
      that precedes the current slice in decoding order and has the
      same value of colour_plane_id.
     */
    first_mb_in_slice = get_ue_golomb(gb);

    /*
      slice_type specifies the coding type of the slice according to
      Table 7-6.   e.g. P, B, I, SP, SI

      When nal_unit_type is equal to 5 (IDR picture), slice_type shall
      be equal to 2, 4, 7, or 9 (I or SI)
     */
    slice_type = get_ue_golomb(gb);

    /*
      pic_parameter_set_id specifies the picture parameter set in
      use. The value of pic_parameter_set_id shall be in the range of
      0 to 255, inclusive.
     */
    pic_parameter_set_id = get_ue_golomb(gb);
    
    /*
      separate_colour_plane_flag equal to 1 specifies that the three
      colour components of the 4:4:4 chroma format are coded
      separately. separate_colour_plane_flag equal to 0 specifies that
      the colour components are not coded separately.  When
      separate_colour_plane_flag is not present, it shall be inferred
      to be equal to 0. When separate_colour_plane_flag is equal to 1,
      the primary coded picture consists of three separate components,
      each of which consists of coded samples of one colour plane (Y,
      Cb or Cr) that each use the monochrome coding syntax. In this
      case, each colour plane is associated with a specific
      colour_plane_id value.
     */
    if (separate_colour_plane_flag)
        get_bits(gb, 2);  // colour_plane_id

    /*
      frame_num is used as an identifier for pictures and shall be
      represented by log2_max_frame_num_minus4 + 4 bits in the
      bitstream....

      If the current picture is an IDR picture, frame_num shall be equal to 0.
    */

    frame_num = get_bits(gb, log2_max_frame_num);
    if (NAL_type == SLICE_IDR)
    {
        frame_num = 0;
        seen_IDR = true;
    }

    /*
      field_pic_flag equal to 1 specifies that the slice is a slice of a
      coded field. field_pic_flag equal to 0 specifies that the slice is a
      slice of a coded frame. When field_pic_flag is not present it shall be
      inferred to be equal to 0.

      bottom_field_flag equal to 1 specifies that the slice is part of a
      coded bottom field. bottom_field_flag equal to 0 specifies that the
      picture is a coded top field. When this syntax element is not present
      for the current slice, it shall be inferred to be equal to 0.
    */

    if (!frame_mbs_only_flag)
    {
        field_pic_flag = get_bits1(gb);
        bottom_field_flag = field_pic_flag ? get_bits1(gb) : 0;
    }
    else
    {
        field_pic_flag = 0;
        bottom_field_flag = -1;
    }

    /*
      idr_pic_id identifies an IDR picture. The values of idr_pic_id
      in all the slices of an IDR picture shall remain unchanged. When
      two consecutive access units in decoding order are both IDR
      access units, the value of idr_pic_id in the slices of the first
      such IDR access unit shall differ from the idr_pic_id in the
      second such IDR access unit. The value of idr_pic_id shall be in
      the range of 0 to 65535, inclusive.
     */
    if (nal_unit_type == SLICE_IDR)
        idr_pic_id = get_ue_golomb(gb);

    /*
      pic_order_cnt_lsb specifies the picture order count modulo
      MaxPicOrderCntLsb for the top field of a coded frame or for a coded
      field. The size of the pic_order_cnt_lsb syntax element is
      log2_max_pic_order_cnt_lsb_minus4 + 4 bits. The value of the
      pic_order_cnt_lsb shall be in the range of 0 to MaxPicOrderCntLsb – 1,
      inclusive.

      delta_pic_order_cnt_bottom specifies the picture order count
      difference between the bottom field and the top field of a coded
      frame.
    */
    if (pic_order_cnt_type == 0)
    {
        pic_order_cnt_lsb = get_bits(gb, log2_max_pic_order_cnt_lsb);

        if (pic_order_present_flag && !field_pic_flag)
            delta_pic_order_cnt_bottom = get_se_golomb(gb);
        else
            delta_pic_order_cnt_bottom = 0;
    }
    else
        delta_pic_order_cnt_bottom = 0;

    /*
      delta_pic_order_cnt[ 0 ] specifies the picture order count
      difference from the expected picture order count for the top
      field of a coded frame or for a coded field as specified in
      subclause 8.2.1. The value of delta_pic_order_cnt[ 0 ] shall be
      in the range of -231 to 231 - 1, inclusive. When this syntax
      element is not present in the bitstream for the current slice,
      it shall be inferred to be equal to 0.

      delta_pic_order_cnt[ 1 ] specifies the picture order count
      difference from the expected picture order count for the bottom
      field of a coded frame specified in subclause 8.2.1. The value
      of delta_pic_order_cnt[ 1 ] shall be in the range of -231 to 231
      - 1, inclusive. When this syntax element is not present in the
      bitstream for the current slice, it shall be inferred to be
      equal to 0.
     */
    if (pic_order_cnt_type == 1 && !delta_pic_order_always_zero_flag)
    {
        delta_pic_order_cnt[0] = get_se_golomb(gb);

        if (pic_order_present_flag && !field_pic_flag)
            delta_pic_order_cnt[1] = get_se_golomb(gb);
        else
            delta_pic_order_cnt[1] = 0;
    }
    else
        delta_pic_order_cnt[0] = 0;

    /*
      redundant_pic_cnt shall be equal to 0 for slices and slice data
      partitions belonging to the primary coded picture. The
      redundant_pic_cnt shall be greater than 0 for coded slices and
      coded slice data partitions in redundant coded pictures.  When
      redundant_pic_cnt is not present, its value shall be inferred to
      be equal to 0. The value of redundant_pic_cnt shall be in the
      range of 0 to 127, inclusive.
    */

    redundant_pic_cnt = redundant_pic_cnt_present_flag ? get_ue_golomb(gb) : 0;

    return true;
}

/*
 * libavcodec used for example
 */
void H264Parser::decode_SPS(GetBitContext * gb)
{
    int profile_idc, chroma_format_idc;

    seen_sps = true;

    profile_idc = get_bits(gb, 8); // profile_idc
    get_bits1(gb);      // constraint_set0_flag
    get_bits1(gb);      // constraint_set1_flag
    get_bits1(gb);      // constraint_set2_flag
    get_bits1(gb);      // constraint_set3_flag
    get_bits(gb, 4);    // reserved
    get_bits(gb, 8);    // level_idc
    get_ue_golomb(gb);  // sps_id

    if (profile_idc >= 100)
    { // high profile
        if ((chroma_format_idc = get_ue_golomb(gb)) == 3) // chroma_format_idc
            separate_colour_plane_flag = (get_bits1(gb) == 1);

        get_ue_golomb(gb);     // bit_depth_luma_minus8
        get_ue_golomb(gb);     // bit_depth_chroma_minus8
        get_bits1(gb);         // qpprime_y_zero_transform_bypass_flag

        if (get_bits1(gb))     // seq_scaling_matrix_present_flag
        {
            for (int idx = 0; idx < ((chroma_format_idc != 3) ? 8 : 12); ++idx)
            {
                get_bits1(gb);  // scaling_list
            }
        }
    }

    /*
      log2_max_frame_num_minus4 specifies the value of the variable
      MaxFrameNum that is used in frame_num related derivations as
      follows:

       MaxFrameNum = 2( log2_max_frame_num_minus4 + 4 )
     */
    log2_max_frame_num = get_ue_golomb(gb) + 4;

    int  offset_for_non_ref_pic;
    int  offset_for_top_to_bottom_field;
    uint tmp;
    bool gaps_in_frame_num_allowed_flag;

    /*
      pic_order_cnt_type specifies the method to decode picture order
      count (as specified in subclause 8.2.1). The value of
      pic_order_cnt_type shall be in the range of 0 to 2, inclusive.
     */
    pic_order_cnt_type = get_ue_golomb(gb);
    if (pic_order_cnt_type == 0)
    {
        /*
          log2_max_pic_order_cnt_lsb_minus4 specifies the value of the
          variable MaxPicOrderCntLsb that is used in the decoding
          process for picture order count as specified in subclause
          8.2.1 as follows:

          MaxPicOrderCntLsb = 2( log2_max_pic_order_cnt_lsb_minus4 + 4 )
          
          The value of log2_max_pic_order_cnt_lsb_minus4 shall be in
          the range of 0 to 12, inclusive.
         */
        log2_max_pic_order_cnt_lsb = get_ue_golomb(gb) + 4;
    }
    else if (pic_order_cnt_type == 1)
    {
        /*
          delta_pic_order_always_zero_flag equal to 1 specifies that
          delta_pic_order_cnt[ 0 ] and delta_pic_order_cnt[ 1 ] are
          not present in the slice headers of the sequence and shall
          be inferred to be equal to
          0. delta_pic_order_always_zero_flag
         */
        delta_pic_order_always_zero_flag = get_bits1(gb);
        /*
          offset_for_non_ref_pic is used to calculate the picture
          order count of a non-reference picture as specified in
          8.2.1. The value of offset_for_non_ref_pic shall be in the
          range of -231 to 231 - 1, inclusive.
         */
        offset_for_non_ref_pic = get_se_golomb(gb);
        /*
          offset_for_top_to_bottom_field is used to calculate the
          picture order count of a bottom field as specified in
          subclause 8.2.1. The value of offset_for_top_to_bottom_field
          shall be in the range of -231 to 231 - 1, inclusive.
         */
        offset_for_top_to_bottom_field = get_se_golomb(gb);
        /*
          offset_for_ref_frame[ i ] is an element of a list of
          num_ref_frames_in_pic_order_cnt_cycle values used in the
          decoding process for picture order count as specified in
          subclause 8.2.1. The value of offset_for_ref_frame[ i ]
          shall be in the range of -231 to 231 - 1, inclusive.
         */
        tmp = get_ue_golomb(gb);
        for (uint idx = 0; idx < tmp; ++idx)
            get_se_golomb(gb);  // offset_for_ref_frame[i]
    }

    /*
      num_ref_frames specifies the maximum number of short-term and
      long-term reference frames, complementary reference field pairs,
      and non-paired reference fields that may be used by the decoding
      process for inter prediction of any picture in the
      sequence. num_ref_frames also determines the size of the sliding
      window operation as specified in subclause 8.2.5.3. The value of
      num_ref_frames shall be in the range of 0 to MaxDpbSize (as
      specified in subclause A.3.1 or A.3.2), inclusive.
     */
    num_ref_frames = get_ue_golomb(gb);
    /*
      gaps_in_frame_num_value_allowed_flag specifies the allowed
      values of frame_num as specified in subclause 7.4.3 and the
      decoding process in case of an inferred gap between values of
      frame_num as specified in subclause 8.2.5.2.
     */
    gaps_in_frame_num_allowed_flag = get_bits1(gb);

    /*
      pic_width_in_mbs_minus1 plus 1 specifies the width of each
      decoded picture in units of macroblocks.  16 macroblocks in a row
     */
    pic_width = (get_ue_golomb(gb) + 1) * 16;
    /*
      pic_height_in_map_units_minus1 plus 1 specifies the height in
      slice group map units of a decoded frame or field.  16
      macroblocks in each column.
     */
    pic_height = (get_ue_golomb(gb) + 1) * 16;

    /*
      frame_mbs_only_flag equal to 0 specifies that coded pictures of
      the coded video sequence may either be coded fields or coded
      frames. frame_mbs_only_flag equal to 1 specifies that every
      coded picture of the coded video sequence is a coded frame
      containing only frame macroblocks.
     */
    frame_mbs_only_flag = get_bits1(gb);
    if (!frame_mbs_only_flag)
    {
        pic_height *= 2;
        /*
          mb_adaptive_frame_field_flag equal to 0 specifies no
          switching between frame and field macroblocks within a
          picture. mb_adaptive_frame_field_flag equal to 1 specifies
          the possible use of switching between frame and field
          macroblocks within frames. When mb_adaptive_frame_field_flag
          is not present, it shall be inferred to be equal to 0.
         */
        get_bits1(gb); // mb_adaptive_frame_field_flag
    }

    get_bits1(gb);     // direct_8x8_inference_flag

    /*
      frame_cropping_flag equal to 1 specifies that the frame cropping
      offset parameters follow next in the sequence parameter
      set. frame_cropping_flag equal to 0 specifies that the frame
      cropping offset parameters are not present.
     */
    if (get_bits1(gb)) // frame_cropping_flag
    {
        frame_crop_left_offset = get_ue_golomb(gb);
        frame_crop_right_offset = get_ue_golomb(gb);
        frame_crop_top_offset = get_ue_golomb(gb);
        frame_crop_bottom_offset = get_ue_golomb(gb);
    }

    /*
      vui_parameters_present_flag equal to 1 specifies that the
      vui_parameters( ) syntax structure as specified in Annex E is
      present. vui_parameters_present_flag equal to 0 specifies that
      the vui_parameters( ) syntax structure as specified in Annex E
      is not present.
     */
    if (get_bits1(gb)) // vui_parameters_present_flag
        vui_parameters(gb);
}

void H264Parser::decode_PPS(GetBitContext * gb)
{
    /*
      pic_parameter_set_id identifies the picture parameter set that
      is referred to in the slice header. The value of
      pic_parameter_set_id shall be in the range of 0 to 255,
      inclusive.
     */
    pic_parameter_set_id = get_ue_golomb(gb);
    /*
      seq_parameter_set_id refers to the active sequence parameter
      set. The value of seq_parameter_set_id shall be in the range of
      0 to 31, inclusive.
     */
    seq_parameter_set_id = get_ue_golomb(gb);
    get_bits1(gb); // entropy_coding_mode_flag;
    /*
      pic_order_present_flag equal to 1 specifies that the picture
      order count related syntax elements are present in the slice
      headers as specified in subclause 7.3.3. pic_order_present_flag
      equal to 0 specifies that the picture order count related syntax
      elements are not present in the slice headers.
     */
    pic_order_present_flag = get_bits1(gb);
    
#if 0 // Rest not currently needed, and requires <math.h>
    uint num_slice_groups = get_ue_golomb(gb) + 1;
    if (num_slice_groups > 1) // num_slice_groups (minus 1)
    {
        uint idx;

        switch (get_ue_golomb(gb)) // slice_group_map_type
        {
          case 0:
            for (idx = 0; idx < num_slice_groups; ++idx)
                get_ue_golomb(gb); // run_length_minus1[idx]
            break;
          case 1:
            for (idx = 0; idx < num_slice_groups; ++idx)
            {
                get_ue_golomb(gb); // top_left[idx]
                get_ue_golomb(gb); // bottom_right[idx]
            }
            break;
          case 3:
          case 4:
          case 5:
            get_bits1(gb);     // slice_group_change_direction_flag
            get_ue_golomb(gb); // slice_group_change_rate_minus1
            break;
          case 6:
            uint pic_size_in_map_units = get_ue_golomb(gb) + 1;
            uint num_bits = (int)ceil(log2(num_slice_groups));
            for (idx = 0; idx < pic_size_in_map_units; ++idx)
            {
                get_bits(gb, num_bits); //slice_group_id[idx]
            }
        }
    }
        
    get_ue_golomb(gb); // num_ref_idx_10_active_minus1
    get_ue_golomb(gb); // num_ref_idx_11_active_minus1
    get_bits1(gb);     // weighted_pred_flag;
    get_bits(gb, 2);   // weighted_bipred_idc
    get_se_golomb(gb); // pic_init_qp_minus26
    get_se_golomb(gb); // pic_init_qs_minus26
    get_se_golomb(gb); // chroma_qp_index_offset
    get_bits1(gb);     // deblocking_filter_control_present_flag
    get_bits1(gb);     // constrained_intra_pref_flag
    redundant_pic_cnt_present_flag = get_bits1(gb);
#endif
}

void H264Parser::vui_parameters(GetBitContext * gb)
{
    /*
      aspect_ratio_info_present_flag equal to 1 specifies that
      aspect_ratio_idc is present. aspect_ratio_info_present_flag
      equal to 0 specifies that aspect_ratio_idc is not present.
     */
    if (get_bits1(gb)) //aspect_ratio_info_present_flag
    {
        /*
          aspect_ratio_idc specifies the value of the sample aspect
          ratio of the luma samples. Table E-1 shows the meaning of
          the code. When aspect_ratio_idc indicates Extended_SAR, the
          sample aspect ratio is represented by sar_width and
          sar_height. When the aspect_ratio_idc syntax element is not
          present, aspect_ratio_idc value shall be inferred to be
          equal to 0.
         */
        uint8_t aspect_ratio_idc = get_bits(gb, 8);

        switch (aspect_ratio_idc)
        {
          case 0:
            // Unspecified
            break;
          case 1:
            // 1:1
            /*
              1280x720 16:9 frame without overscan
              1920x1080 16:9 frame without overscan (cropped from 1920x1088)
              640x480 4:3 frame without overscan
             */
            break;
          case 2:
            // 12:11
            /*
              720x576 4:3 frame with horizontal overscan
              352x288 4:3 frame without overscan
             */
            break;
          case 3:
            // 10:11
            /*
              720x480 4:3 frame with horizontal overscan
              352x240 4:3 frame without overscan
             */
            break;
          case 4:
            // 16:11
            /*
              720x576 16:9 frame with horizontal overscan
              540x576 4:3 frame with horizontal overscan
             */
            break;
          case 5:
            // 40:33
            /*
              720x480 16:9 frame with horizontal overscan
              540x480 4:3 frame with horizontal overscan
             */
            break;
          case 6:
            // 24:11
            /*
              352x576 4:3 frame without overscan
              540x576 16:9 frame with horizontal overscan
             */
            break;
          case 7:
            // 20:11
            /*
              352x480 4:3 frame without overscan
              480x480 16:9 frame with horizontal overscan
             */
            break;
          case 8:
            // 32:11
            /*
              352x576 16:9 frame without overscan
             */
            break;
          case 9:
            // 80:33
            /*
              352x480 16:9 frame without overscan
             */
            break;
          case 10:
            // 18:11
            /*
              480x576 4:3 frame with horizontal overscan
             */
            break;
          case 11:
            // 15:11
            /*
              480x480 4:3 frame with horizontal overscan
             */
            break;
          case 12:
            // 64:33
            /*
              540x576 16:9 frame with horizontal overscan
             */
            break;
          case 13:
            // 160:99
            /*
              540x576 16:9 frame with horizontal overscan
             */
            break;
          case EXTENDED_SAR:
            sar_width  = get_bits(gb, 16);
            sar_height = get_bits(gb, 16);
            break;
        }
    }
    else
        sar_width = sar_height = 0;
}
