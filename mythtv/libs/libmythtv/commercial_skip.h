#ifndef __COMMERCIAL_SKIP_H__
#define __COMMERCIAL_SKIP_H__

#include <qmap.h>

#include "mythcontext.h"

#define COMMERCIAL_SKIP_OFF     0x0
#define COMMERCIAL_SKIP_BLANKS  0x1
#define COMMERCIAL_SKIP_SCENE   0x2
#define COMMERCIAL_SKIP_LOGO    0x4

extern "C" {
#include "frame.h"
}

class CommDetect
{
  public:
    CommDetect(int w, int h, double fps, int method);
    ~CommDetect(void);

    void Init(int w, int h, double fps, int method);

    void ProcessNextFrame(VideoFrame *frame, long long frame_number = -1);

    bool FrameIsBlank(void);
    bool SceneHasChanged(void);

    void SetBlankFrameDetection(bool onOff)  { detectBlankFrames = onOff; }
    void SetSceneChangeDetection(bool onOff) { detectSceneChanges = onOff; }

    void SetAggressiveDetection(bool onOff)  { aggressiveDetection = onOff; }
    void SetCommSkipAllBlanks(bool onOff)  { skipAllBlanks = onOff; }

    void ClearAllMaps(void);
    void GetCommBreakMap(QMap<long long, int> &marks);
    void SetBlankFrameMap(QMap<long long, int> &blanks);
    void GetBlankFrameMap(QMap<long long, int> &blanks,
                          long long start_frame = -1);
    void GetBlankCommMap(QMap<long long, int> &comms);
    void GetBlankCommBreakMap(QMap<long long, int> &breaks);
    void GetSceneChangeMap(QMap<long long, int> &scenes,
                           long long start_frame = -1);

    void BuildMasterCommList(void);
    void BuildBlankFrameCommList(void);
    void BuildSceneChangeCommList(void);

    void MergeBlankCommList(void);
    void DeleteCommAtFrame(QMap<long long, int> &commMap, long long frame);

    bool FrameIsInCommBreak(long long f, QMap<long long, int> &breakMap);

    void DumpMap(QMap<long long, int> &map);

  private:
    bool CheckFrameIsBlank(void);
    bool CheckSceneHasChanged(void);

    int GetAvgBrightness(void);

    bool aggressiveDetection;

    int commDetectMethod;

    int width;
    int height;
    int border;
    double frame_rate;
    double fpm;
    bool blankFramesOnly;

    long long framesProcessed;

    bool detectBlankFrames;
    bool detectSceneChanges;

    bool skipAllBlanks;

    unsigned char *frame_ptr;

    QMap<long long, int> blankFrameMap;
    QMap<long long, int> blankCommMap;
    QMap<long long, int> blankCommBreakMap;
    QMap<long long, int> sceneChangeMap;
    QMap<long long, int> sceneChangeCommMap;
    QMap<long long, int> commBreakMap;

    bool frameIsBlank;
    bool sceneHasChanged;

    bool lastFrameWasBlank;
    bool lastFrameWasSceneChange;

    int histogram[256];
    int lastHistogram[256];
};

#endif
