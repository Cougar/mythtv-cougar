/*
	webcam.h

	(c) 2003 Paul Volkaerts
	
    header for the main interface screen
*/

#ifndef WEBCAM_H_
#define WEBCAM_H_

#include <qsqldatabase.h>
#include <qregexp.h>
#include <qtimer.h>
#include <qptrlist.h>
#include <qthread.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>

#include <linux/videodev.h>
#include <mythtv/mythwidgets.h>
#include <mythtv/dialogbox.h>


#define RGB24_LEN(w,h)      ((w) * (h) * 3)
#define RGB32_LEN(w,h)      ((w) * (h) * 4)
#define YUV420P_LEN(w,h)    (((w) * (h) * 3) / 2)
#define YUV422P_LEN(w,h)    ((w) * (h) * 2)

// YUV --> RGB Conversion macros
#define _S(a)		(a)>255 ? 255 : (a)<0 ? 0 : (a)
#define _R(y,u,v) (0x2568*(y)  			       + 0x3343*(u)) /0x2000
#define _G(y,u,v) (0x2568*(y) - 0x0c92*(v) - 0x1a1e*(u)) /0x2000
#define _B(y,u,v) (0x2568*(y) + 0x40cf*(v))					     /0x2000



class Webcam : public QObject
{

  Q_OBJECT

  public:

    Webcam(QObject *parent = 0, const char * = 0);
    virtual ~Webcam(void);
    bool camOpen(QString WebcamName, int width, int height);
    void camClose(void);
    bool SetPalette(int palette);
    int  GetPalette(void) { return (vPic.palette); };
    int  SetBrightness(int v);
    int  SetContrast(int v);
    int  SetColour(int v);
    int  SetHue(int v);
    int  SetTargetFps(int f);
    int  GetActualFps();
    int  GetBrightness(void) { return (vPic.brightness);};
    int  GetColour(void) { return (vPic.colour);};
    int  GetContrast(void) { return (vPic.contrast);};
    int  GetHue(void) { return (vPic.hue);};
    QString GetName(void) { return vCaps.name; };
    void GetMaxSize(int *x, int *y) { *y=vCaps.maxheight; *x=vCaps.maxwidth; };
    void GetMinSize(int *x, int *y) { *y=vCaps.minheight; *x=vCaps.minwidth; };
    void GetCurSize(int *x, int *y) { *y=vWin.height; *x=vWin.width; };
    int isGreyscale(void) { return ((vCaps.type & VID_TYPE_MONOCHROME) ? true : false); };
    int grabImage(void);

  signals:
    void webcamFrameReady(uchar *yuvBuff, int w, int h);

  public slots:
    void grabTimerExpiry();
    void fpsMeasureTimerExpiry();

  private:
    void SetSize(int width, int height);

    void readCaps(void);

    int hDev;
    QString DevName;
    unsigned char *picbuff1;
    unsigned char *picbuff2;
    unsigned char *dispbuff;
    int imageLen;
    int frameSize;
    int fps;
    int actualFps;
    int frames_last_period;

    QTimer  *grabTimer;              // Timer controlling grab frame rate
    QTimer  *fpsMeasureTimer;        // Timer measuring actual FPS

    // Camera-reported capabilities
    struct video_capability vCaps;
    struct video_window vWin;
    struct video_picture vPic;
    struct video_clip vClips;


};


#endif
