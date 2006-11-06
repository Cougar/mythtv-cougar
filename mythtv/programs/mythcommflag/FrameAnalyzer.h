/*
 * FrameAnalyzer
 *
 * Provide a generic interface for plugging in frame analysis algorithms.
 */

#ifndef __FRAMEANALYZER_H__
#define __FRAMEANALYZER_H__

/* Base class for commercial flagging frame analyzers. */

#include <limits.h>
#include <qmap.h>

typedef struct VideoFrame_ VideoFrame;
class NuppelVideoPlayer;

class FrameAnalyzer
{
public:
    virtual ~FrameAnalyzer(void) { }

    virtual const char *name(void) const = 0;

    /* Analyze a frame. */
    enum analyzeFrameResult {
        ANALYZE_OK,         /* Analysis OK */
        ANALYZE_ERROR,      /* Recoverable error */
        ANALYZE_FINISHED,   /* Analysis complete, don't need more frames. */
        ANALYZE_FATAL,      /* Don't use this analyzer anymore. */
    };

    virtual enum analyzeFrameResult nuppelVideoPlayerInited(
            NuppelVideoPlayer *nvp, long long nframes) {
        (void)nvp;
        (void)nframes;
        return ANALYZE_OK;
    };

    /*
     * Populate *pNextFrame with the next frame number desired by this
     * analyzer.
     */
    static const long long ANYFRAME = LONG_LONG_MAX;
    static const long long NEXTFRAME = -1;
    virtual enum analyzeFrameResult analyzeFrame(const VideoFrame *frame,
            long long frameno, long long *pNextFrame /* [out] */) = 0;

    virtual int finished(long long nframes, bool final) {
        (void)nframes;
        (void)final;
        return 0;
    }
    virtual int reportTime(void) const { return 0; }

    /* 0-based frameno => nframes */
    typedef QMap<long long, long long> FrameMap;
};

namespace frameAnalyzer {

void frameAnalyzerReportMap(const FrameAnalyzer::FrameMap *frameMap,
        float fps, const char *comment);

void frameAnalyzerReportMapms(const FrameAnalyzer::FrameMap *frameMap,
        float fps, const char *comment);

long long frameAnalyzerMapSum(const FrameAnalyzer::FrameMap *frameMap);

}; /* namespace */

#endif  /* !__FRAMEANALYZER_H__ */

/* vim: set expandtab tabstop=4 shiftwidth=4: */
