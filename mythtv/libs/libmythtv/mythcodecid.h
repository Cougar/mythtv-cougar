#ifndef _MYTH_CODEC_ID_H_
#define _MYTH_CODEC_ID_H_

#include <QString>

typedef enum
{
// if you add anything to this list please update
// myth2av_codecid, and NuppelVideoPlayer::GetEncodingType()
    kCodec_NONE = 0,

    kCodec_NORMAL_BEGIN = kCodec_NONE,

    kCodec_NUV_RTjpeg,
    kCodec_NUV_MPEG4,

    kCodec_MPEG1,
    kCodec_MPEG2,
    kCodec_H263,
    kCodec_MPEG4,
    kCodec_H264,
    kCodec_VC1,
    kCodec_WMV3,

    kCodec_NORMAL_END,

    kCodec_STD_XVMC_BEGIN = kCodec_NORMAL_END,

    kCodec_MPEG1_XVMC,
    kCodec_MPEG2_XVMC,
    kCodec_H263_XVMC,
    kCodec_MPEG4_XVMC,
    kCodec_H264_XVMC,

    kCodec_MPEG1_IDCT,
    kCodec_MPEG2_IDCT,
    kCodec_H263_IDCT,
    kCodec_MPEG4_IDCT,
    kCodec_H264_IDCT,

    kCodec_STD_XVMC_END,

    kCodec_VLD_BEGIN = kCodec_STD_XVMC_END,

    kCodec_MPEG1_VLD,
    kCodec_MPEG2_VLD,
    kCodec_H263_VLD,
    kCodec_MPEG4_VLD,
    kCodec_H264_VLD,

    kCodec_VLD_END,

    kCodec_DVDV_BEGIN = kCodec_VLD_END,

    kCodec_MPEG1_DVDV,
    kCodec_MPEG2_DVDV,
    kCodec_H263_DVDV,
    kCodec_MPEG4_DVDV,
    kCodec_H264_DVDV,

    kCodec_DVDV_END,

    kCodec_VDPAU_BEGIN = kCodec_DVDV_END,

    kCodec_MPEG1_VDPAU,
    kCodec_MPEG2_VDPAU,
    kCodec_H263_VDPAU,
    kCodec_MPEG4_VDPAU,
    kCodec_H264_VDPAU,
    kCodec_VC1_VDPAU,
    kCodec_WMV3_VDPAU,

    kCodec_VDPAU_END,

} MythCodecID;

QString toString(MythCodecID codecid);
int myth2av_codecid(MythCodecID codec_id, bool &vld, bool &idct, bool &mc,
                    bool &vdpau);
inline int myth2av_codecid(MythCodecID codec_id)
{
    bool vld, idct, mc, vdpau;
    return myth2av_codecid(codec_id, vld, idct, mc, vdpau);
}

#endif // _MYTH_CODEC_ID_H_
