#ifndef VIDEOOUT_QUARTZ_H_
#define VIDEOOUT_QUARTZ_H_

class DVDV;
struct QuartzData;

#include "videooutbase.h"

class VideoOutputQuartz : public VideoOutput
{
  public:
    VideoOutputQuartz(MythCodecID av_codec_id, void *codec_priv);
   ~VideoOutputQuartz();

    bool Init(int width, int height, float aspect, WId winid,
              int winx, int winy, int winw, int winh, WId embedid = 0);

    void ProcessFrame(VideoFrame *frame, OSD *osd,
                      FilterChain *filterList,
                      const PIPMap &pipPlayers,
                      FrameScanType scan);
    void PrepareFrame(VideoFrame *buffer, FrameScanType t);
    void Show(FrameScanType);

    void SetVideoFrameRate(float playback_fps);
    bool InputChanged(const QSize &input_size,
                      float        aspect,
                      MythCodecID  av_codec_id,
                      void        *codec_private);
    void VideoAspectRatioChanged(float aspect);
    void MoveResize(void);
    void Zoom(ZoomDirection direction);
    void ToggleAdjustFill(AdjustFillMode adjustFill);

    void EmbedInWidget(WId wid, int x, int y, int w, int h);
    void StopEmbedding(void);

    int GetRefreshRate(void);

    void DrawUnusedRects(bool sync = true);

    void UpdatePauseFrame(void);

    void SetDVDVDecoder(DVDV *dvdvdec);

    static QStringList GetAllowedRenderers(MythCodecID myth_codec_id,
                                           const QSize &video_dim);

    static MythCodecID GetBestSupportedCodec(
        uint width, uint height,
        uint osd_width, uint osd_height,
        uint stream_type, uint fourcc);

  private:
    void Exit(void);
    bool CreateQuartzBuffers(void);
    void DeleteQuartzBuffers(void);

    bool         Started;
    QuartzData  *data;
    VideoFrame   pauseFrame;
    MythCodecID  myth_codec_id;
};

#endif
