#ifndef VIDEOOUT_QUARTZ_H_
#define VIDEOOUT_QUARTZ_H_

struct QuartzData;

#include "videooutbase.h"

class VideoOutputQuartz : public VideoOutput
{
  public:
    VideoOutputQuartz();
   ~VideoOutputQuartz();

    bool Init(int width, int height, float aspect, WId winid,
              int winx, int winy, int winw, int winh, WId embedid = 0);
    void PrepareFrame(VideoFrame *buffer, FrameScanType t);
    void Show(FrameScanType);

    void InputChanged(int width, int height, float aspect);
    void AspectChanged(float aspect);
    void Zoom(int direction);

    void EmbedInWidget(WId wid, int x, int y, int w, int h);
    void StopEmbedding(void);

    int GetRefreshRate(void);

    void DrawUnusedRects(void);

    void UpdatePauseFrame(void);
    void ProcessFrame(VideoFrame *frame, OSD *osd,
                      FilterChain *filterList,
                      NuppelVideoPlayer *pipPlayer);

  private:
    void Exit(void);
    bool CreateQuartzBuffers(void);
    void DeleteQuartzBuffers(void);

    bool         Started;

    QuartzData * data;

    VideoFrame * scratchFrame;
    VideoFrame   pauseFrame;
};

#endif
