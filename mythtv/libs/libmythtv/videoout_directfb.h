#ifndef VIDEOOUT_DIRECTFB_H
#define VIDEOOUT_DIRECTFB_H

#include <qobject.h>

struct DirectfbData;

#include "videooutbase.h"

extern "C" {
#include <directfb.h>
}

class NuppelVideoPlayer;

class VideoOutputDirectfb: public VideoOutput
{
  public:
    VideoOutputDirectfb();
    ~VideoOutputDirectfb();

    bool Init(int width, int height, float aspect, WId winid,
              int winx, int winy, int winw, int winh, WId embedid = 0);
    void PrepareFrame(VideoFrame *buffer);
    void Show(FrameScanType);

    void InputChanged(int width, int height, float aspect);
	void AspectChanged(float aspect);
    void Zoom(int direction);

	float GetDisplayAspect(void);
	void VideoOutputDirectfb::MoveResize();

    //void EmbedInWidget(unsigned long wid, int x, int y, int w, int h);
    //void StopEmbedding(void);

    int GetRefreshRate(void);

    void DrawUnusedRects(void);

    void UpdatePauseFrame(void);
    void ProcessFrame(VideoFrame *frame, OSD *osd,
                      FilterChain *filterList,
                      NuppelVideoPlayer *pipPlayer);

    int ChangePictureAttribute(int attributeType, int newValue);

  private:

	QObject *widget;

    DirectfbData *data;

    bool XJ_started;

    VideoFrame *scratchFrame;
    VideoFrame pauseFrame;

    bool CreateDirectfbBuffers(DFBSurfaceDescription);
    void DeleteDirectfbBuffers(void);
    bool InitOsdSurface(OSD *osd);
};

DFBEnumerationResult LayerCallback(unsigned int id,
  DFBDisplayLayerDescription desc, void *data);

static inline void * memcpy_pic(unsigned char * dst, unsigned char * src, int bytesPerLine, int height, int dstStride, int srcStride)
{
        int i;
        void *retval=dst;

        if(dstStride == srcStride) memcpy(dst, src, srcStride*height);
        else
        {
                for(i=0; i<height; i++)
                {
                        memcpy(dst, src, bytesPerLine);
                        src+= srcStride;
                        dst+= dstStride;
                }
        }

        return retval;
}
#endif
