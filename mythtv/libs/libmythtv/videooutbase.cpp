#include <cmath>

#include "videooutbase.h"
#include "osd.h"
#include "osdsurface.h"
#include "NuppelVideoPlayer.h"

#include "../libmyth/mythcontext.h"

#ifdef USING_XV
#include "videoout_xv.h"
#endif

#ifdef USING_IVTV
#include "videoout_ivtv.h"
#endif

#ifdef USING_XVMC
#include "videoout_xvmc.h"
#endif

#ifdef USING_DIRECTFB
#include "videoout_directfb.h"
#endif

#ifdef USING_DIRECTX
#include "videoout_dx.h"
#endif

#ifdef Q_OS_MACX
#include "videoout_quartz.h"
#endif

#include "videoout_null.h"

#include "dithertable.h"

#include "../libavcodec/avcodec.h"

#include "filtermanager.h"

VideoOutput *VideoOutput::InitVideoOut(VideoOutputType type)
{
    (void)type;

#ifdef USING_IVTV
    if (type == kVideoOutput_IVTV)
        return new VideoOutputIvtv();
#endif

#ifdef USING_XVMC
    if (type == kVideoOutput_XvMC)
        return new VideoOutputXvMC();
#endif

#ifdef USING_DIRECTFB
    return new VideoOutputDirectfb();
#endif

#ifdef USING_DIRECTX
    return new VideoOutputDX();
#endif

#ifdef USING_XV
    return new VideoOutputXv();
#endif

#ifdef Q_OS_MACX
    return new VideoOutputQuartz();
#endif

    VERBOSE(VB_IMPORTANT, "Not compiled with any useable video output method.");
    exit(-28);
}

VideoOutput::VideoOutput()
{
    letterbox = -1;
    rpos = 0;
    vpos = 0;

    vbuffers = NULL;
    numbuffers = 0;

    needrepaint = false;

    ZoomedIn = 0;
    ZoomedUp = 0;
    ZoomedRight = 0;

    piptmpbuf = NULL;
    pipscontext = NULL;

    piph_in = piph_out = -1;
    pipw_in = pipw_out = -1;

    desired_piph = 128;
    desired_pipw = 160;

    allowpreviewepg = true;

    w_mm = 400;
    h_mm = 300;

    myth_dsw = gContext->GetNumSetting("DisplaySizeWidth", 0);
    myth_dsh = gContext->GetNumSetting("DisplaySizeHeight", 0);

    m_deinterlacing = false;
    m_deintFiltMan = NULL;
    m_deintFilter = NULL;
    m_deinterlaceBeforeOSD = true;
}

VideoOutput::~VideoOutput()
{
    ShutdownPipResize();

    QMap<VideoFrame*, int>::iterator iter = vbufferMap.begin();
    for (;iter != vbufferMap.end(); iter++)
        if (iter.key()->qscale_table != NULL)
            delete [] iter.key()->qscale_table;

    if (vbuffers)
        delete [] vbuffers;
    if (m_deintFilter)
        delete m_deintFilter;
    if (m_deintFiltMan)
        delete m_deintFiltMan;
}

bool VideoOutput::Init(int width, int height, float aspect, WId winid,
                       int winx, int winy, int winw, int winh, WId embedid)
{
    (void)winid;
    (void)embedid;

    XJ_width = width;
    XJ_height = height;
    XJ_aspect = aspect;

    QString HorizScanMode = gContext->GetSetting("HorizScanMode", "overscan");
    QString VertScanMode = gContext->GetSetting("VertScanMode", "overscan");

    img_hscanf = gContext->GetNumSetting("HorizScanPercentage", 5) / 100.0;
    img_vscanf = gContext->GetNumSetting("VertScanPercentage", 5) / 100.0;

    img_xoff = gContext->GetNumSetting("xScanDisplacement", 0);
    img_yoff = gContext->GetNumSetting("yScanDisplacement", 0);

    PIPLocation = gContext->GetNumSetting("PIPLocation", 0);

    if (VertScanMode == "underscan")
    {
        img_vscanf = 0 - img_vscanf;
    }
    if (HorizScanMode == "underscan")
    {
        img_hscanf = 0 - img_hscanf;
    }

    if (winw && winh)
        VERBOSE(VB_PLAYBACK,
                QString("Over/underscan. V: %1, H: %2, XOff: %3, YOff: %4")
                .arg(img_vscanf).arg(img_hscanf).arg(img_xoff).arg(img_yoff));

    dispx = 0; dispy = 0;
    dispw = winw; disph = winh;

    imgx = winx; imgy = winy;
    imgw = XJ_width; imgh = XJ_height;

    if (imgw == 1920 && imgh == 1088)
        imgh = 1080; // ATSC 1920x1080

    brightness = gContext->GetNumSettingOnHost("PlaybackBrightness", 
                                               gContext->GetHostName(), 50);
    contrast = gContext->GetNumSettingOnHost("PlaybackContrast", 
                                             gContext->GetHostName(), 50);
    colour = gContext->GetNumSettingOnHost("PlaybackColour", 
                                           gContext->GetHostName(), 50);
    hue = gContext->GetNumSettingOnHost("PlaybackHue", 
                                        gContext->GetHostName(), 0);

    int asp_override = gContext->GetNumSetting("AspectOverride", 0);

    if (asp_override > 0) 
        AspectOverride(asp_override);

    embedding = false;

    return true;
}

bool VideoOutput::SetupDeinterlace(bool interlaced)
{
    if (VideoOutputNull *null = dynamic_cast<VideoOutputNull *>(this))
    {
        (void)null;
        // null vidout doesn't deinterlace
        return false;
    }

    if (m_deinterlacing == interlaced)
        return m_deinterlacing;

    if (m_deintFiltMan)
    {
        delete m_deintFiltMan;
        m_deintFiltMan = NULL;
    }
    if (m_deintFilter)
    {
        delete m_deintFilter;
        m_deintFilter = NULL;
    }

    m_deinterlacing = interlaced;

    if (m_deinterlacing) 
    {
        m_deinterlaceBeforeOSD = true;

        VideoFrameType itmp = FMT_YV12;
        VideoFrameType otmp = FMT_YV12;
        int btmp;
        
        m_deintfiltername = gContext->GetSetting("DeinterlaceFilter", 
                                                 "linearblend");
        m_deintFiltMan = new FilterManager;
        m_deintFilter = NULL;

        if (ApproveDeintFilter(m_deintfiltername))
        {
            m_deintFilter = m_deintFiltMan->LoadFilters(
                m_deintfiltername, itmp, otmp, XJ_width, XJ_height, btmp);
        }

        if (m_deintFilter == NULL) 
        {
            VERBOSE(VB_IMPORTANT,QString("Couldn't load deinterlace filter %1")
                    .arg(m_deintfiltername));
            m_deinterlacing = false;
            m_deintfiltername = "";
        }

        VERBOSE(VB_PLAYBACK, QString("Using deinterlace method %1")
                .arg(m_deintfiltername));

        if (m_deintfiltername == "bobdeint")
            m_deinterlaceBeforeOSD = false;
    }

    return m_deinterlacing;
}

bool VideoOutput::NeedsDoubleFramerate() const
{
    // Bob deinterlace requires doubling framerate
    return (m_deintfiltername == "bobdeint" && m_deinterlacing);
}

bool VideoOutput::ApproveDeintFilter(const QString& filtername) const
{
    // Default to not supporting bob deinterlace
    return (filtername != "bobdeint");
}

void VideoOutput::AspectOverride(int override)
{
    switch(override)
    {
        default:
        case 0:
           break;
        case 1:
           XJ_aspect = (16.0 / 9);
           letterbox = 0;
           break;
        case 2:
           XJ_aspect = (4.0 / 3);
           letterbox = 1;
           break;
        case 3:
           XJ_aspect = (16.0 / 9);
           letterbox = 2;
           break;
        case 4:
           XJ_aspect = (4.0 / 3);
           letterbox = 3;
           break;
    }
}

void VideoOutput::AspectChanged(float aspect)
{
    XJ_aspect = aspect;
}

void VideoOutput::InputChanged(int width, int height, float aspect)
{
    XJ_width = width;
    XJ_height = height;
    XJ_aspect = aspect;

    video_buflock.lock();

    availableVideoBuffers.clear();
    vbufferMap.clear();

    for (int i = 0; i < numbuffers; i++)
    {
        availableVideoBuffers.enqueue(&(vbuffers[i]));
        vbufferMap[&(vbuffers[i])] = i;
    }

    usedVideoBuffers.clear();
    busyVideoBuffers.clear();

    availableVideoBuffers_wait.wakeAll();

    video_buflock.unlock();
}

void VideoOutput::EmbedInWidget(WId wid, int x, int y, int w, int h)
{
    (void)wid;

    olddispx = dispx;
    olddispy = dispy;
    olddispw = dispw;
    olddisph = disph;

    dispxoff = dispx = x;
    dispyoff = dispy = y;
    dispwoff = dispw = w;
    disphoff = disph = h;

    embedding = true;

    MoveResize();
}

void VideoOutput::StopEmbedding(void)
{
    dispx = olddispx;
    dispy = olddispy;
    dispw = olddispw;
    disph = olddisph;

    embedding = false;

    MoveResize();
}

void VideoOutput::DrawSlice(VideoFrame *frame, int x, int y, int w, int h)
{
    (void)frame;
    (void)x;
    (void)y;
    (void)w;
    (void)h;
}

void VideoOutput::GetDrawSize(int &xoff, int &yoff, int &width, int &height)
{
    xoff = imgx;
    yoff = imgy;
    width = imgw;
    height = imgh;
}

void VideoOutput::MoveResize(void)
{
    int yoff, xoff;

    // Preset all image placement and sizing variables.
    imgx = 0; imgy = 0;
    imgw = XJ_width; imgh = XJ_height;
    xoff = img_xoff; yoff = img_yoff;
    dispxoff = dispx; dispyoff = dispy;
    dispwoff = dispw; disphoff = disph;
    if (imgw == 1920 && imgh == 1088)
        imgh = 1080; // ATSC 1920x1080

/*
    Here we apply playback over/underscanning and offsetting (if any apply).

    It doesn't make any sense to me to offset an image such that it is clipped.
    Therefore, we only apply offsets if there is an underscan or overscan which
    creates "room" to move the image around. That is, if we overscan, we can
    move the "viewport". If we underscan, we change where we place the image
    into the display window. If no over/underscanning is performed, you just
    get the full original image scaled into the full display area.
*/

    if (img_vscanf > 0) 
    {
        // Veritcal overscan. Move the Y start point in original image.
        imgy = (int)floor(0.5 + imgh * img_vscanf);
        imgh = (int)floor(0.5 + imgh * (1 - 2 * img_vscanf));

        // If there is an offset, apply it now that we have a room.
        // To move the image down, move the start point up.
        if (yoff > 0) 
        {
            // Can't offset the image more than we have overscanned.
            if (yoff > imgy)
                yoff = imgy;
            imgy -= yoff;
        }
        // To move the image up, move the start point down.
        if (yoff < 0) 
        {
            // Again, can't offset more than overscanned.
            if (abs(yoff) > imgy)
                yoff = 0 - imgy;
            imgy -= yoff;
        }
    }

    if (img_hscanf > 0) 
    {
        // Horizontal overscan. Move the X start point in original image.
        imgx = (int)floor(0.5 + XJ_width * img_hscanf);
        imgw = (int)floor(0.5 + XJ_width * (1 - 2 * img_hscanf));
        if (xoff > 0) 
        {
            if (xoff > imgx) 
                xoff = imgx;
            imgx -= xoff;
        }
        if (xoff < 0) 
        {
            if (abs(xoff) > imgx) 
                xoff = 0 - imgx;
            imgx -= xoff;
        }
    }

    float vscanf, hscanf;
    if (img_vscanf < 0) 
    {
        // Vertical underscan. Move the starting Y point in the display window.
        // Use the abolute value of scan factor.
        vscanf = fabs(img_vscanf);
        dispyoff = (int)floor(0.5 + disph * vscanf) + dispy;
        disphoff = (int)floor(0.5 + disph * (1 - 2 * vscanf));
        // Now offset the image within the extra blank space created by
        // underscanning.
        // To move the image down, increase the Y offset inside the display
        // window.
        if (yoff > 0) 
        {
            // Can't offset more than we have underscanned.
            if (yoff > dispyoff) 
                yoff = dispyoff;
            dispyoff += yoff;
        }
        if (yoff < 0) 
        {
            if (abs(yoff) > dispyoff) 
                yoff = 0 - dispyoff;
            dispyoff += yoff;
        }
    }

    if (img_hscanf < 0) 
    {
        hscanf = fabs(img_hscanf);
        dispxoff = (int)floor(0.5 + dispw * hscanf) + dispx;
        dispwoff = (int)floor(0.5 + dispw * (1 - 2 * hscanf));
        if (xoff > 0) 
        {
            if (xoff > dispxoff) 
                xoff = dispxoff;
            dispxoff += xoff;
        }

        if (xoff < 0) 
        {
            if (abs(xoff) > dispxoff) 
                xoff = 0 - dispxoff;
            dispxoff += xoff;
        }
    }

    // Manage aspect ratio and letterbox settings.  Code should take into
    // account the aspect ratios of both the video as well as the actual
    // screen to allow proper letterboxing to take place.
    
    //cout << "XJ aspect " << XJ_aspect << endl;
    //cout << "Display aspect " << GetDisplayAspect() << endl;
    //printf("Before: %dx%d%+d%+d\n", dispwoff, disphoff, dispxoff, 
    //    dispyoff);
    
    if (XJ_aspect <= 1.4) 
    {
        // Video is 4:3
        if (letterbox == -1)
            letterbox = kLetterbox_4_3;
    }
    else
    {
        // Video is 16:9
        if (letterbox == -1)
            letterbox = kLetterbox_16_9;
    }

    if (GetDisplayAspect() > XJ_aspect)
    {
        float pixNeeded = (h_mm * XJ_aspect) * ((float)dispwoff / w_mm);

        dispxoff += (dispwoff - (int)pixNeeded) / 2;
        dispwoff = (int)pixNeeded;
    }
    else
    {
        float pixNeeded = (w_mm / XJ_aspect) * ((float)disphoff / h_mm);

        dispyoff += (disphoff - (int)pixNeeded) / 2;
        disphoff = (int)pixNeeded;
    }

    if ((letterbox == kLetterbox_4_3_Zoom) ||
        (letterbox == kLetterbox_16_9_Zoom))
    {
        // Zoom mode
        //printf("Before zoom: %dx%d%+d%+d\n", dispwoff, disphoff,
        //dispxoff, dispyoff);
        // Expand by 4/3 and overscan
        dispxoff -= (dispwoff/6); // 1/6 of original is 1/8 of new
        dispwoff = dispwoff*4/3;
        dispyoff -= (disphoff/6);
        disphoff = disphoff*4/3;
    }

    if (letterbox == kLetterbox_16_9_Stretch)
    {
        //Stretch mode
        //Should be used to eliminate side bars on 4:3 material
        //encoded to 16:9. Basically crop a 16:9 to 4:3
        //printf("Before zoom: %dx%d%+d%+d\n", dispwoff, disphoff,
        //dispxoff, dispyoff);
        dispxoff -= (dispwoff/6); // 1/6 of original is 1/8 of new
        dispwoff = dispwoff*4/3;
    }

    if (ZoomedIn)
    {
        int oldDW = dispwoff;
        int oldDH = disphoff;
        dispwoff = (int)(dispwoff * (1 + (ZoomedIn * .01)));
        disphoff = (int)(disphoff * (1 + (ZoomedIn * .01)));

        dispxoff += (oldDW - dispwoff) / 2;
        dispyoff += (oldDH - disphoff) / 2;
    }

    if (ZoomedUp)
    {
        dispyoff = (int)(dispyoff * (1 - (ZoomedUp * .01)));
    }

    if (ZoomedRight)
    {
        dispxoff = (int)(dispxoff * (1 - (ZoomedRight * .01)));
    }

    //printf("After: %dx%d%+d%+d\n", dispwoff, disphoff, dispxoff, 
    //dispyoff);

 
    VERBOSE(VB_PLAYBACK,
            QString("Image size. dispxoff %1, dispyoff: %2, dispwoff: %3, "
                    "disphoff: %4")
            .arg(dispxoff).arg(dispyoff).arg(dispwoff).arg(disphoff));

    VERBOSE(VB_PLAYBACK,
            QString("Image size. imgx %1, imgy: %2, imgw: %3, imgh: %4")
           .arg(imgx).arg(imgy).arg(imgw).arg(imgh));

    DrawUnusedRects();
}

void VideoOutput::Zoom(int direction)
{
    switch (direction)
    {
        case kZoomHome:
                ZoomedIn = 0;
                ZoomedUp = 0;
                ZoomedRight = 0;
                break;
        case kZoomIn:
                if (ZoomedIn < 200)
                    ZoomedIn += 10;
                else
                    ZoomedIn = 0;
                break;
        case kZoomOut:
                if (ZoomedIn > -50)
                    ZoomedIn -= 10;
                else
                    ZoomedIn = 0;
                break;
        case kZoomUp:
                if (ZoomedUp < 90)
                    ZoomedUp += 10;
                break;
        case kZoomDown:
                if (ZoomedUp > -90)
                    ZoomedUp -= 10;
                break;
        case kZoomLeft:
                if (ZoomedRight < 90)
                    ZoomedRight += 10;
                break;
        case kZoomRight:
                if (ZoomedRight > -90)
                    ZoomedRight -= 10;
                break;
    }
}

void VideoOutput::ToggleLetterbox(int letterboxMode)
{
    if (letterboxMode == kLetterbox_Toggle)
    {
        if (++letterbox >= kLetterbox_END)
            letterbox = 0;
    }
    else
    {
        letterbox = letterboxMode;
    }

    switch (letterbox) 
    {
        default:
        case kLetterbox_4_3:
        case kLetterbox_4_3_Zoom:      AspectChanged(4.0 / 3);
                                       break;
        case kLetterbox_16_9:
        case kLetterbox_16_9_Zoom:
        case kLetterbox_16_9_Stretch:  AspectChanged(16.0 / 9);
                                       break;
    }
}

int VideoOutput::ValidVideoFrames(void)
{
    video_buflock.lock();
    int ret = (int)usedVideoBuffers.count();
    video_buflock.unlock();

    return ret;
}

int VideoOutput::FreeVideoFrames(void)
{
    return (int)availableVideoBuffers.count();
}

bool VideoOutput::EnoughFreeFrames(void)
{
    return (FreeVideoFrames() >= needfreeframes);
}

bool VideoOutput::EnoughDecodedFrames(void)
{
    return (ValidVideoFrames() >= needprebufferframes);
}

bool VideoOutput::EnoughPrebufferedFrames(void)
{
    return (ValidVideoFrames() >= keepprebufferframes);
}

VideoFrame *VideoOutput::GetNextFreeFrame(void)
{
    video_buflock.lock();
    VideoFrame *next = availableVideoBuffers.dequeue();

    while (next && busyVideoBuffers.findRef(next) != -1)
    {
        VERBOSE(VB_GENERAL,QString("GetNextFreeFrame() served a busy frame. "
                                   "Dropping. #Frames=%1/%2.")
                .arg(availableVideoBuffers.count() + usedVideoBuffers.count())
                .arg(numbuffers));
        next = availableVideoBuffers.dequeue();
    }

    // only way this should be triggered if we're in unsafe mode
    if (!next)
    {
        next = usedVideoBuffers.dequeue();
        busyVideoBuffers.removeRef(next);
        if (EnoughFreeFrames())
            availableVideoBuffers_wait.wakeAll();
    }

    video_buflock.unlock();

    return next;
}

void VideoOutput::ReleaseFrame(VideoFrame *frame)
{
    vpos = vbufferMap[frame];

    video_buflock.lock();
    usedVideoBuffers.enqueue(frame);
    busyVideoBuffers.append(frame);
    video_buflock.unlock();
}

void VideoOutput::DiscardFrame(VideoFrame *frame)
{
    video_buflock.lock();
    availableVideoBuffers.enqueue(frame);
    if (EnoughFreeFrames())
        availableVideoBuffers_wait.wakeAll();
    video_buflock.unlock();
}

VideoFrame *VideoOutput::GetLastDecodedFrame(void)
{
    return &(vbuffers[vpos]);
}

VideoFrame *VideoOutput::GetLastShownFrame(void)
{
    return &(vbuffers[rpos]);
}

void VideoOutput::StartDisplayingFrame(void)
{
    rpos = vbufferMap[usedVideoBuffers.head()];
}

void VideoOutput::DoneDisplayingFrame(void)
{
    video_buflock.lock();

    VideoFrame *buf = usedVideoBuffers.dequeue();
    if (buf)
    {
        availableVideoBuffers.enqueue(buf);
        busyVideoBuffers.removeRef(buf);
        if (EnoughFreeFrames())
            availableVideoBuffers_wait.wakeAll();
    }

    video_buflock.unlock();
}

int VideoOutput::ChangePictureAttribute(int attribute, int newValue)
{
    (void)attribute;
    (void)newValue;
    return -1;
}

int VideoOutput::ChangeBrightness(bool up)
{
    int result;

    result = this->ChangePictureAttribute(kPictureAttribute_Brightness, 
                                          brightness + ((up) ? 1 : -1) );

    brightness = (result == -1) ? brightness : result;

    return brightness;
}

int VideoOutput::ChangeContrast(bool up)
{
    int result;

    result = this->ChangePictureAttribute(kPictureAttribute_Contrast, 
                                          contrast + ((up) ? 1 : -1) );

    contrast = (result == -1) ? contrast : result;

    return contrast;
}

int VideoOutput::ChangeColour(bool up)
{
    int result;

    result = this->ChangePictureAttribute(kPictureAttribute_Colour, 
                                          colour + ((up) ? 1 : -1) );
    colour = (result == -1) ? colour : result;

    return colour;
}

int VideoOutput::ChangeHue(bool up)
{
    int result;

    result = this->ChangePictureAttribute(kPictureAttribute_Hue, 
                                          hue + ((up) ? 1 : -1) );
    hue = (result == -1) ? hue : result;

    return hue;
}

void VideoOutput::InitBuffers(int numdecode, bool extra_for_pause, 
                              int need_free, int needprebuffer_normal,
                              int needprebuffer_small, int keepprebuffer)
{
    int numcreate = numdecode + ((extra_for_pause) ? 1 : 0);

    vbuffers = new VideoFrame[numcreate + 1];

    for (int i = 0; i < numdecode; i++)
    {
        availableVideoBuffers.enqueue(&(vbuffers[i]));
        vbufferMap[&(vbuffers[i])] = i;
    }

    for (int i = 0; i < numcreate; i++)
    {
        vbuffers[i].codec = FMT_NONE;
        vbuffers[i].width = vbuffers[i].height = 0;
        vbuffers[i].bpp = vbuffers[i].size = 0;
        vbuffers[i].buf = NULL;
        vbuffers[i].timecode = 0;
        vbuffers[i].frameNumber = 0;
        vbuffers[i].qscale_table = NULL;
        vbuffers[i].qstride = 0;
        vbuffers[i].interlaced_frame = -1;
        vbuffers[i].top_field_first = 1;
    }

    numbuffers = numdecode;
    needfreeframes = need_free;
    needprebufferframes = needprebuffer_normal;
    needprebufferframes_normal = needprebuffer_normal;
    needprebufferframes_small = needprebuffer_small;
    keepprebufferframes = keepprebuffer;

    availableVideoBuffers_wait.wakeAll();
}

void VideoOutput::ClearAfterSeek(void)
{
    video_buflock.lock();

    for (int i = 0; i < numbuffers; i++)
        vbuffers[i].timecode = 0;

    while (usedVideoBuffers.count() > 1)
    {
        VideoFrame *buffer = usedVideoBuffers.dequeue();
        availableVideoBuffers.enqueue(buffer);
    }

    if (usedVideoBuffers.count() > 0)
    {
        VideoFrame *buffer = usedVideoBuffers.dequeue();
        availableVideoBuffers.enqueue(buffer);
        vpos = vbufferMap[buffer];
        rpos = vpos;
    }
    else
    {
        vpos = rpos = 0;
    }

    if (EnoughFreeFrames())
        availableVideoBuffers_wait.wakeAll();

    busyVideoBuffers.clear();

    video_buflock.unlock();
}

void VideoOutput::DoPipResize(int pipwidth, int pipheight)
{
    if (pipwidth != desired_piph || pipheight != desired_pipw)
    {
        ShutdownPipResize();

        piptmpbuf = new unsigned char[desired_piph * desired_pipw * 3 / 2];

        piph_in = pipheight;
        pipw_in = pipwidth;

        piph_out = desired_piph;
        pipw_out = desired_pipw;

        pipscontext = img_resample_init(pipw_out, piph_out, pipw_in, piph_in);
    }
}

void VideoOutput::ShutdownPipResize(void)
{
    if (piptmpbuf)
        delete [] piptmpbuf;
    if (pipscontext)
        img_resample_close(pipscontext);

    piptmpbuf = NULL;
    pipscontext = NULL;

    piph_in = piph_out = -1;
    pipw_in = pipw_out = -1;
}

void VideoOutput::ShowPip(VideoFrame *frame, NuppelVideoPlayer *pipplayer)
{
    if (!pipplayer)
        return;

    int pipw, piph;

    VideoFrame *pipimage = pipplayer->GetCurrentFrame(pipw, piph);

    if (!pipimage || !pipimage->buf || pipimage->codec != FMT_YV12)
    {
        pipplayer->ReleaseCurrentFrame(pipimage);
        return;
    }

    int xoff;
    int yoff;

    unsigned char *pipbuf = pipimage->buf;

    if (pipw != desired_pipw || piph != desired_piph)
    {
        DoPipResize(pipw, piph);

        if (piptmpbuf && pipscontext)
        {
            AVPicture img_in, img_out;

            avpicture_fill(&img_out, (uint8_t *)piptmpbuf, PIX_FMT_YUV420P,
                           pipw_out, piph_out);
            avpicture_fill(&img_in, (uint8_t *)pipimage->buf, PIX_FMT_YUV420P,
                           pipw, piph);

            img_resample(pipscontext, &img_out, &img_in);

            pipw = pipw_out;
            piph = piph_out;

            pipbuf = piptmpbuf;
        }
    }

    switch (PIPLocation)
    {
        default:
        case kPIPTopLeft:
                xoff = 30;
                yoff = 40;
                break;
        case kPIPBottomLeft:
                xoff = 30;
                yoff = frame->height - piph - 40;
                break;
        case kPIPTopRight:
                xoff = frame->width - pipw - 30;
                yoff = 40;
                break;
        case kPIPBottomRight:
                xoff = frame->width - pipw - 30;
                yoff = frame->height - piph - 40;
                break;
    }

    for (int i = 0; i < piph; i++)
    {
        memcpy(frame->buf + (i + yoff) * frame->width + xoff,
               pipbuf + i * pipw, pipw);
    }

    xoff /= 2;
    yoff /= 2;

    unsigned char *uptr = frame->buf + frame->width * frame->height;
    unsigned char *vptr = frame->buf + frame->width * frame->height * 5 / 4;
    int vidw = frame->width / 2;

    unsigned char *pipuptr = pipbuf + pipw * piph;
    unsigned char *pipvptr = pipbuf + pipw * piph * 5 / 4;
    pipw /= 2;

    for (int i = 0; i < piph / 2; i ++)
    {
        memcpy(uptr + (i + yoff) * vidw + xoff, pipuptr + i * pipw, pipw);
        memcpy(vptr + (i + yoff) * vidw + xoff, pipvptr + i * pipw, pipw);
    }

    pipplayer->ReleaseCurrentFrame(pipimage);
}

int VideoOutput::DisplayOSD(VideoFrame *frame, OSD *osd, int stride)
{
    int retval = -1;

    //struct timeval one, two, three, four;
    //struct timeval done = {0, 0}, dtwo = {0, 0};

    if (osd)
    {
        //gettimeofday(&one, NULL);
        OSDSurface *surface = osd->Display();
        //gettimeofday(&two, NULL);
        //timersub(&two, &one, &done);

        //gettimeofday(&three, NULL);
        if (surface)
        {
            switch (frame->codec)
            {
                case FMT_YV12:
                {
                    unsigned char *yuvptr = frame->buf;
                    BlendSurfaceToYV12(surface, yuvptr, stride);
                    break;
                }
                case FMT_AI44:
                {
                    unsigned char *ai44ptr = frame->buf;
                    if (surface->Changed())
                        BlendSurfaceToI44(surface, ai44ptr, true, stride);
                    break;
                }
                case FMT_IA44:
                {
                    unsigned char *ia44ptr = frame->buf;
                    if (surface->Changed())
                        BlendSurfaceToI44(surface, ia44ptr, false, stride);
                    break;
                }
                case FMT_ARGB32:
                {
                    unsigned char *argbptr = frame->buf;
                    if (surface->Changed())
                        BlendSurfaceToARGB(surface, argbptr, stride);
                    break;
                }
                default:
                    break;
            }
            retval = surface->Changed() ? 1 : 0;
        }
        //gettimeofday(&four, NULL);
        //timersub(&four, &three, &dtwo);
    }

    //cout << done.tv_usec << " " << dtwo.tv_usec << endl;

    return retval;
}

void VideoOutput::BlendSurfaceToYV12(OSDSurface *surface, unsigned char *yuvptr,
                                     int stride)
{
    blendtoyv12_8_fun blender = blendtoyv12_8_init(surface);

    unsigned char *uptrdest = yuvptr + surface->width * surface->height;
    unsigned char *vptrdest = uptrdest + surface->width * surface->height / 4;

    (void)stride;

    QMemArray<QRect> rects = surface->usedRegions.rects();
    QMemArray<QRect>::Iterator it = rects.begin();
    for (; it != rects.end(); ++it)
    {
        QRect drawRect = *it;

        int startcol, startline, endcol, endline;
        startcol = drawRect.left();
        startline = drawRect.top();
        endcol = drawRect.right();
        endline = drawRect.bottom();

        unsigned char *src, *usrc, *vsrc;
        unsigned char *dest, *udest, *vdest;
        unsigned char *alpha;

        int yoffset;

        for (int y = startline; y <= endline; y++)
        {
            yoffset = y * surface->width;

            src = surface->y + yoffset + startcol;
            dest = yuvptr + yoffset + startcol;
            alpha = surface->alpha + yoffset + startcol;

            for (int x = startcol; x <= endcol; x++)
            {
                if (x + 8 >= endcol)
                {
                    if (*alpha != 0)
                        *dest = blendColorsAlpha(*src, *dest, *alpha);
                    src++;
                    dest++;
                    alpha++;
                }
                else
                {
                    blender(src, dest, alpha, false);
                    src += 8;
                    dest += 8;
                    alpha += 8;
                    x += 7;
                }
            }

            alpha = surface->alpha + yoffset + startcol;

            if (y % 2 == 0)
            {
                usrc = surface->u + yoffset / 4 + startcol / 2;
                udest = uptrdest + yoffset / 4 + startcol / 2;

                vsrc = surface->v + yoffset / 4 + startcol / 2;
                vdest = vptrdest + yoffset / 4 + startcol / 2;

                for (int x = startcol; x <= endcol; x += 2)
                {
                    alpha = surface->alpha + yoffset + x;

                    if (x + 16 >= endcol)
                    {
                        if (*alpha != 0)
                        {
                            *udest = blendColorsAlpha(*usrc, *udest, *alpha);
                            *vdest = blendColorsAlpha(*vsrc, *vdest, *alpha);
                        }

                        usrc++;
                        udest++;
                        vsrc++;
                        vdest++;
                    }
                    else
                    {
                        blender(usrc, udest, alpha, true);
                        blender(vsrc, vdest, alpha, true);
                        usrc += 8;
                        udest += 8;
                        vsrc += 8;
                        vdest += 8;
                        x += 14;
                    }
                }
            }
        }
    }
}

void VideoOutput::BlendSurfaceToI44(OSDSurface *surface, unsigned char *yuvptr,
                                    bool ifirst, int stride)
{
    int ashift = ifirst ? 0 : 4;
    int amask = ifirst ? 0x0f : 0xf0;

    int ishift = ifirst ? 4 : 0;
    int imask = ifirst ? 0xf0 : 0x0f; 

    if (stride < 0)
        stride = XJ_width;

    dithertoia44_8_fun ditherer = dithertoia44_8_init(surface);
    dither8_context *dcontext = init_dithertoia44_8_context(ifirst);

    memset(yuvptr, 0x0, stride * XJ_height);

    QMemArray<QRect> rects = surface->usedRegions.rects();
    QMemArray<QRect>::Iterator it = rects.begin();
    for (; it != rects.end(); ++it)
    {
        QRect drawRect = *it;

        int startcol, startline, endcol, endline;
        startcol = drawRect.left();
        startline = drawRect.top();
        endcol = drawRect.right();
        endline = drawRect.bottom();

        unsigned char *src;
        unsigned char *dest;
        unsigned char *alpha;

        const unsigned char *dmp;

        int yoffset;
        int destyoffset;

        int grey;

        for (int y = startline; y <= endline; y++)
        {
            yoffset = y * surface->width;
            destyoffset = y * stride;

            src = surface->y + yoffset + startcol;
            dest = yuvptr + destyoffset + startcol;
            alpha = surface->alpha + yoffset + startcol;

            dmp = DM[(y) & (DM_HEIGHT - 1)];

            for (int x = startcol; x <= endcol; x++)
            {
                if (x + 8 >= endcol)
                {
                    if (*alpha != 0)
                    {
                        grey = *src + ((dmp[(x & (DM_WIDTH - 1))] << 2) >> 4);
                        grey = (grey - (grey >> 4)) >> 4;

                        *dest = (((*alpha >> 4) << ashift) & amask) |
                                (((grey) << ishift) & imask);
                    }
                    else
                        *dest = 0;

                    src++;
                    dest++;
                    alpha++;
                }
                else
                {
                    ditherer(src, dest, alpha, dmp, x, dcontext);
                    src += 8;
                    dest += 8;
                    alpha += 8;
                    x += 7;
                }
            }
        }
    }

    delete_dithertoia44_8_context(dcontext);
}

void VideoOutput::BlendSurfaceToARGB(OSDSurface *surface, 
                                     unsigned char *argbptr, int stride)
{
    if (stride < 0)
        stride = XJ_width;

    blendtoargb_8_fun blender = blendtoargb_8_init(surface);
    unsigned char *cm = surface->cm;

    memset(argbptr, 0x0, stride * XJ_height);

    QMemArray<QRect> rects = surface->usedRegions.rects();
    QMemArray<QRect>::Iterator it = rects.begin();
    for (; it != rects.end(); ++it)
    {
        QRect drawRect = *it;

        int startcol, startline, endcol, endline;
        startcol = drawRect.left();
        startline = drawRect.top();
        endcol = drawRect.right();
        endline = drawRect.bottom();

        unsigned char *src, *usrcbase, *vsrcbase, *usrc, *vsrc;
        unsigned char *dest;
        unsigned char *alpha;

        int yoffset;
        int destyoffset;

        int cb, cr, r_add, g_add, b_add;
        int r, g, b, y0;

        for (int y = startline; y <= endline; y++)
        {
            yoffset = y * surface->width;
            destyoffset = y * stride;

            src = surface->y + yoffset + startcol;
            dest = argbptr + destyoffset + startcol * 4;
            alpha = surface->alpha + yoffset + startcol;

            usrcbase = surface->u + (y / 2) * (surface->width / 2);
            vsrcbase = surface->v + (y / 2) * (surface->width / 2);

            for (int x = startcol; x <= endcol; x++)
            {
                usrc = usrcbase + x / 2;
                vsrc = vsrcbase + x / 2;

                if (x + 8 >= endcol)
                {
                    if (*alpha != 0)
                    {
                        YUV_TO_RGB1(*usrc, *vsrc);
                        YUV_TO_RGB2(r, g, b, *src);
                        RGBA_OUT(dest, r, g, b, *alpha);
                    }
                    src++;
                    alpha++;
                    dest += 4;
                }
                else
                {
                    blender(surface, src, usrc, vsrc, alpha, dest);
                    src += 8;
                    dest += 32;
                    alpha += 8;
                    x += 7;
                } 
            }
        }
    }
}

void VideoOutput::CopyFrame(VideoFrame *to, VideoFrame *from)
{
    if (to == NULL || from == NULL)
        return;

    // guaranteed to be correct sizes.
    memcpy(to->buf, from->buf, from->size);

/* XXX: Broken.
    if (from->qstride > 0 && from->qscale_table != NULL)
    {
        int tablesize = from->qstride * ((from->height + 15) / 16);

        if (to->qstride != from->qstride || to->qscale_table == NULL)
        {
            to->qstride = from->qstride;
            if (to->qscale_table)
                delete [] to->qscale_table;

            to->qscale_table = new unsigned char[tablesize];
        }

        memcpy(to->qscale_table, from->qscale_table, tablesize);
    }
*/
}

