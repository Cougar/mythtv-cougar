#include <cmath>

#include "osd.h"
#include "osdsurface.h"
#include "NuppelVideoPlayer.h"
#include "videodisplayprofile.h"
#include "decoderbase.h"

#include "mythcontext.h"
#include "mythverbose.h"

#ifdef USING_XV
#include "videoout_xv.h"
#endif

#ifdef USING_IVTV
#include "videoout_ivtv.h"
#endif

#ifdef USING_DIRECTFB
#include "videoout_directfb.h"
#endif

#ifdef USING_DIRECTX
#include "videoout_dx.h"
#endif

#ifdef USING_D3D
#include "videoout_d3d.h"
#endif

#ifdef Q_OS_MACX
#include "videoout_quartz.h"
#endif

#ifdef USING_OPENGL_VIDEO
#include "videoout_opengl.h"
#endif

#include "videoout_null.h"

#include "dithertable.h"

extern "C" {
#include "avcodec.h"
#include "libswscale/swscale.h"
}

#include "filtermanager.h"

#include "videooutbase.h"

#define LOC QString("VideoOutput: ")
#define LOC_ERR QString("VideoOutput, Error: ")

static QString to_comma_list(const QStringList &list);

/**
 * \brief  Depending on compile-time configure settings and run-time
 *         renderer settings, create a relevant VideoOutput subclass.
 * \return instance of VideoOutput if successful, NULL otherwise.
 */
VideoOutput *VideoOutput::Create(
        const QString &decoder,   MythCodecID  codec_id,
        void          *codec_priv,
        PIPState pipState,
        const QSize   &video_dim, float        video_aspect,
        WId            win_id,    const QRect &display_rect,
        WId            embed_id)
{
    (void) codec_priv;

    QStringList renderers;

#ifdef USING_IVTV
    renderers += VideoOutputIvtv::GetAllowedRenderers(codec_id, video_dim);
#endif // USING_IVTV

#ifdef USING_DIRECTFB
    renderers += VideoOutputDirectfb::GetAllowedRenderers(codec_id, video_dim);
#endif // USING_DIRECTFB

#ifdef USING_D3D
    renderers += VideoOutputD3D::GetAllowedRenderers(codec_id, video_dim);
#endif

#ifdef USING_DIRECTX
    renderers += VideoOutputDX::GetAllowedRenderers(codec_id, video_dim);
#endif // USING_DIRECTX

#ifdef USING_XV
    const QStringList xvlist =
        VideoOutputXv::GetAllowedRenderers(codec_id, video_dim);
    renderers += xvlist;
#endif // USING_XV

#ifdef Q_OS_MACX
    const QStringList osxlist =
        VideoOutputQuartz::GetAllowedRenderers(codec_id, video_dim);
    renderers += osxlist;
#endif // Q_OS_MACX

#ifdef USING_OPENGL_VIDEO
    renderers += VideoOutputOpenGL::GetAllowedRenderers(codec_id, video_dim);
#endif // USING_OPENGL_VIDEO

    VERBOSE(VB_PLAYBACK, LOC + "Allowed renderers: " +
            to_comma_list(renderers));

    renderers = VideoDisplayProfile::GetFilteredRenderers(decoder, renderers);

    VERBOSE(VB_PLAYBACK, LOC + "Allowed renderers (filt: " + decoder + "): " +
            to_comma_list(renderers));

    QString renderer = QString::null;
    if (renderers.size() > 0)
    {
        VideoDisplayProfile vprof;
        vprof.SetInput(video_dim);

        QString tmp = vprof.GetVideoRenderer();
        if (vprof.IsDecoderCompatible(decoder) && renderers.contains(tmp))
        {
            renderer = tmp;
            VERBOSE(VB_PLAYBACK, LOC + "Preferred renderer: " + renderer);
        }
    }

    if (renderer.isEmpty())
        renderer = VideoDisplayProfile::GetBestVideoRenderer(renderers);

    while (!renderers.empty())
    {
        VERBOSE(VB_PLAYBACK, LOC +
                QString("Trying video renderer: '%1'").arg(renderer));
        int index = renderers.indexOf(renderer);
        if (index >= 0)
            renderers.removeAt(index);
        else
            break;

        VideoOutput *vo = NULL;
#ifdef USING_IVTV
        if (renderer == "ivtv")
            vo = new VideoOutputIvtv();
#endif // USING_IVTV

#ifdef USING_DIRECTFB
        if (renderer == "directfb")
            vo = new VideoOutputDirectfb();
#endif // USING_DIRECTFB

#ifdef USING_D3D
        if (renderer == "direct3d")
            vo = new VideoOutputD3D();
#endif // USING_D3D

#ifdef USING_DIRECTX
        if (renderer == "directx")
            vo = new VideoOutputDX();
#endif // USING_DIRECTX

#ifdef Q_OS_MACX
        if (osxlist.contains(renderer))
            vo = new VideoOutputQuartz(codec_id, codec_priv);
#endif // Q_OS_MACX

#ifdef USING_OPENGL_VIDEO
        if (renderer == "opengl")
            vo = new VideoOutputOpenGL();
#endif // USING_OPENGL_VIDEO

#ifdef USING_XV
        if (xvlist.contains(renderer))
            vo = new VideoOutputXv(codec_id);
#endif // USING_XV

        if (vo)
        {
            vo->SetPIPState(pipState);
            if (vo->Init(
                    video_dim.width(), video_dim.height(), video_aspect,
                    win_id, display_rect.x(), display_rect.y(),
                    display_rect.width(), display_rect.height(), embed_id))
            {
                return vo;
            }

            delete vo;
            vo = NULL;
        }

        renderer = VideoDisplayProfile::GetBestVideoRenderer(renderers);
    }

    VERBOSE(VB_IMPORTANT, LOC_ERR +
            "Not compiled with any useable video output method.");

    return NULL;
}

/**
 * \class VideoOutput
 * \brief This class serves as the base class for all video output methods.
 *
 * The basic use is:
 * \code
 * VideoOutputType type = kVideoOutput_Default;
 * vo = VideoOutput::InitVideoOut(type);
 * vo->Init(width, height, aspect ...);
 *
 * // Then create two threads.
 * // In the decoding thread
 * while (decoding)
 * {
 *     if (vo->WaitForAvailable(1000)
 *     {
 *         frame = vo->GetNextFreeFrame(); // remove frame from "available"
 *         av_lib_process(frame);   // do something to fill it.
 *         // call DrawSlice()      // if you need piecemeal processing
 *                                  // by VideoOutput use DrawSlice
 *         vo->ReleaseFrame(frame); // enqueues frame in "used" queue
 *     }
 * }
 *  
 * // In the displaying thread
 * while (playing)
 * {
 *     if (vo->EnoughPrebufferedFrames())
 *     {
 *         // Sets "Last Shown Frame" to head of "used" queue
 *         vo->StartDisplayingFrame();
 *         // Get pointer to "Last Shown Frame"
 *         frame = vo->GetLastShownFrame();
 *         // add OSD, do any filtering, etc.
 *         vo->ProcessFrame(frame, osd, filters, pict-in-pict);
 *         // tells show what frame to be show, do other last minute stuff
 *         vo->PrepareFrame(frame, scan);
 *         // here you wait until it's time to show the frame
 *         // Show blits the last prepared frame to the screen
 *         // as quickly as possible.
 *         vo->Show(scan);
 *         // remove frame from the head of "used",
 *         // vo must get it into "available" eventually.
 *         vo->DoneDisplayingFrame();
 *     }
 * }
 * delete vo;
 * \endcode
 *
 *  Note: Show() may be called multiple times between PrepareFrame() and
 *        DoneDisplayingFrame(). But if a frame is ever removed from
 *        available via GetNextFreeFrame(), you must either call
 *        DoneDisplayFrame() or call DiscardFrame(VideoFrame*) on it.
 *
 *  Note: ProcessFrame() may be called multiple times on a frame, to
 *        update an OSD for example.
 *
 *  The VideoBuffers class handles the buffer tracking,
 *  see it for more details on the states a buffer can 
 *  take before it becomes available for reuse.
 *
 * \see VideoBuffers, NuppelVideoPlayer
 */

/**
 * \fn VideoOutput::VideoOutput()
 * \brief This constructor for VideoOutput must be followed by an 
 *        Init(int,int,float,WId,int,int,int,int,WId) call.
 */
VideoOutput::VideoOutput() :
    // DB Settings
    db_display_dim(0,0),
    db_aspectoverride(kAspect_Off),     db_adjustfill(kAdjustFill_Off),
    db_letterbox_colour(kLetterBoxColour_Black),
    db_deint_filtername(QString::null),
    db_use_picture_controls(false),
    db_vdisp_profile(NULL),

    // Picture-in-Picture stuff
    pip_desired_display_size(160,128),  pip_display_size(0,0),
    pip_video_size(0,0),
    pip_tmp_buf(NULL),                  pip_tmp_buf2(NULL),
    pip_scaling_context(NULL),

    // Video resizing (for ITV)
    vsz_enabled(false),
    vsz_desired_display_rect(0,0,0,0),  vsz_display_size(0,0),
    vsz_video_size(0,0),
    vsz_tmp_buf(NULL),                  vsz_scale_context(NULL),

    // Deinterlacing
    m_deinterlacing(false),             m_deintfiltername("linearblend"),
    m_deintFiltMan(NULL),               m_deintFilter(NULL),
    m_deinterlaceBeforeOSD(true),

    // Various state variables
    errored(false),
    framesPlayed(0),
    supported_attributes(kPictureAttributeSupported_None)
{
    bzero(&pip_tmp_image, sizeof(pip_tmp_image));
    db_display_dim = QSize(gContext->GetNumSetting("DisplaySizeWidth",  0),
                           gContext->GetNumSetting("DisplaySizeHeight", 0));

    db_pict_attr[kPictureAttribute_Brightness] =
        gContext->GetNumSetting("PlaybackBrightness", 50);
    db_pict_attr[kPictureAttribute_Contrast] =
        gContext->GetNumSetting("PlaybackContrast",   50);
    db_pict_attr[kPictureAttribute_Colour] =
        gContext->GetNumSetting("PlaybackColour",     50);
    db_pict_attr[kPictureAttribute_Hue] =
        gContext->GetNumSetting("PlaybackHue",         0);

    db_aspectoverride = (AspectOverrideMode)
        gContext->GetNumSetting("AspectOverride",      0);
    db_adjustfill = (AdjustFillMode)
        gContext->GetNumSetting("AdjustFill",          0);
    db_letterbox_colour = (LetterBoxColour)
        gContext->GetNumSetting("LetterboxColour",     0);
    db_use_picture_controls =
        gContext->GetNumSetting("UseOutputPictureControls", 0);

    if (!gContext->IsDatabaseIgnored())
        db_vdisp_profile = new VideoDisplayProfile();

    windows.push_back(VideoOutWindow());
}

/**
 * \fn VideoOutput::~VideoOutput()
 * \brief Shuts down video output.
 */
VideoOutput::~VideoOutput()
{
    ShutdownPipResize();

    ShutdownVideoResize();

    if (m_deintFilter)
        delete m_deintFilter;
    if (m_deintFiltMan)
        delete m_deintFiltMan;
    if (db_vdisp_profile)
        delete db_vdisp_profile;
}

/**
 * \fn VideoOutput::Init(int,int,float,WId,int,int,int,int,WId)
 * \brief Performs most of the initialization for VideoOutput.
 * \return true if successful, false otherwise.
 */
bool VideoOutput::Init(int width, int height, float aspect, WId winid,
                       int winx, int winy, int winw, int winh, WId embedid)
{
    (void)winid;
    (void)embedid;

    bool mainSuccess = windows[0].Init(
        QSize(width, height), aspect,
        QRect(winx, winy, winw, winh),
        db_aspectoverride, db_adjustfill);

    if (db_vdisp_profile)
        db_vdisp_profile->SetInput(windows[0].GetVideoDim());

    return mainSuccess;
}

void VideoOutput::InitOSD(OSD *osd)
{
    if (db_vdisp_profile && !db_vdisp_profile->IsOSDFadeEnabled())
        osd->DisableFade();
}

QString VideoOutput::GetFilters(void) const
{
    if (db_vdisp_profile)
        return db_vdisp_profile->GetFilters();
    return QString::null;
}

void VideoOutput::SetVideoFrameRate(float playback_fps)
{
    if (db_vdisp_profile)
        db_vdisp_profile->SetOutput(playback_fps);
}

/**
 * \fn VideoOutput::SetDeinterlacingEnabled(bool)
 * \brief Attempts to enable/disable deinterlacing using
 *        existing deinterlace method when enabling.
 */
bool VideoOutput::SetDeinterlacingEnabled(bool enable)
{
    if (enable && m_deinterlacing)
        return m_deinterlacing;

    // if enable and no deinterlacer allocated, attempt allocate one
    if (enable && (!m_deintFiltMan || !m_deintFilter))
        return SetupDeinterlace(enable);

    m_deinterlacing = enable;
    return m_deinterlacing;
}

/**
 * \fn VideoOutput::SetupDeinterlace(bool,const QString&)
 * \brief Attempts to enable or disable deinterlacing.
 * \return true if successful, false otherwise.
 * \param overridefilter optional, explicitly use this nondefault deint filter
 */
bool VideoOutput::SetupDeinterlace(bool interlaced, 
                                   const QString& overridefilter)
{
    PIPState pip_state = windows[0].GetPIPState();

    if (pip_state > kPIPOff && pip_state < kPBPLeft)
        return false;

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
        
        if (db_vdisp_profile)
            m_deintfiltername = db_vdisp_profile->GetFilteredDeint(overridefilter);
        else
            m_deintfiltername = "";

        m_deintFiltMan = new FilterManager;
        m_deintFilter = NULL;

        if (!m_deintfiltername.isEmpty())
        {
            if (!ApproveDeintFilter(m_deintfiltername))
            {
                VERBOSE(VB_IMPORTANT,
                        QString("Failed to approve '%1' deinterlacer "
                        "as a software deinterlacer")
                        .arg(m_deintfiltername));
                m_deintfiltername = QString::null;
            }
            else
            {
                int threads = db_vdisp_profile ?
                                db_vdisp_profile->GetMaxCPUs() : 1;
                const QSize video_dim = windows[0].GetVideoDim();
                int width  = video_dim.width();
                int height = video_dim.height();
                m_deintFilter = m_deintFiltMan->LoadFilters(
                    m_deintfiltername, itmp, otmp,
                    width, height, btmp, threads);
                windows[0].SetVideoDim(QSize(width, height));
            }
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

/** \fn VideoOutput::FallbackDeint(void)
 *  \brief Fallback to non-frame-rate-doubling deinterlacing method.
 */
void VideoOutput::FallbackDeint(void)
{
    SetupDeinterlace(false);
    if (db_vdisp_profile)
        SetupDeinterlace(true, db_vdisp_profile->GetFallbackDeinterlacer());
}

/** \fn VideoOutput::BestDeint(void)
 *  \brief Change to the best deinterlacing method.
 */
void VideoOutput::BestDeint(void)
{
    SetupDeinterlace(false);
    SetupDeinterlace(true);
}

/** \fn VideoOutput::IsExtraProcessingRequired(void) const
 *  \brief Should Prepare() and Show() and ProcessFrame be called
 *         twice for every Frameloop().
 *
 *   All adaptive full framerate deinterlacers require an extra
 *   ProcessFrame() call.
 *
 *  \return true if deint name contains doubleprocess 
 */
bool VideoOutput::IsExtraProcessingRequired(void) const
{
    return (m_deintfiltername.contains("doubleprocess")) && m_deinterlacing;
}
/**
 * \fn VideoOutput::NeedsDoubleFramerate() const
 * \brief Should Prepare() and Show() be called twice for every ProcessFrame().
 *
 * \return m_deintfiltername == "bobdeint" && m_deinterlacing
 */
bool VideoOutput::NeedsDoubleFramerate() const
{
    // Bob deinterlace requires doubling framerate
    return ((m_deintfiltername.contains("bobdeint") ||
             m_deintfiltername.contains("doublerate") ||
             m_deintfiltername.contains("doubleprocess")) &&
             m_deinterlacing);
}

bool VideoOutput::IsBobDeint(void) const
{
    return (m_deinterlacing && m_deintfiltername == "bobdeint");
}

/**
 * \fn VideoOutput::ApproveDeintFilter(const QString& filtername) const
 * \brief Approves all deinterlace filters, except ones which
 *        must be supported by a specific video output class.
 */
bool VideoOutput::ApproveDeintFilter(const QString& filtername) const
{
    // Default to not supporting bob deinterlace
    return (!filtername.contains("bobdeint") &&
            !filtername.contains("doublerate") &&
            !filtername.contains("opengl") &&
            !filtername.contains("vdpau"));
}



/**
 * \fn VideoOutput::VideoAspectRatioChanged(float aspect)
 * \brief Calls SetVideoAspectRatio(float aspect),
 *        then calls MoveResize() to apply changes.
 * \param aspect video aspect ratio to use
 */
void VideoOutput::VideoAspectRatioChanged(float aspect)
{
    vector<VideoOutWindow>::iterator it = windows.begin();
    for (; it != windows.end(); ++it)
        (*it).VideoAspectRatioChanged(aspect);
}

/**
 * \fn VideoOutput::InputChanged(const QSize&, float, MythCodecID, void*)
 * \brief Tells video output to discard decoded frames and wait for new ones.
 * \bug We set the new width height and aspect ratio here, but we should
 *      do this based on the new video frames in Show().
 */
bool VideoOutput::InputChanged(const QSize &input_size,
                               float        aspect,
                               MythCodecID  myth_codec_id,
                               void        *codec_private)
{
    vector<VideoOutWindow>::iterator it = windows.begin();
    for (; it != windows.end(); ++it)
        (*it).InputChanged(input_size, aspect, myth_codec_id, codec_private);

    if (db_vdisp_profile)
        db_vdisp_profile->SetInput(windows[0].GetVideoDim());

    BestDeint();
    
    DiscardFrames(true);

    return true;
}
/**
* \brief Resize Display Window
*/
void VideoOutput::ResizeDisplayWindow(const QRect &rect, bool save_visible_rect)
{
    windows[0].ResizeDisplayWindow(rect, save_visible_rect);
}

/**
 * \brief Tells video output to embed video in an existing window.
 * \param x   X location where to locate video
 * \param y   Y location where to locate video
 * \param w   width of video
 * \param h   height of video
 * \sa StopEmbedding()
 */
void VideoOutput::EmbedInWidget(int x, int y, int w, int h)
{
    windows[0].EmbedInWidget(QRect(x, y, w, h));
}

/**
 * \fn VideoOutput::StopEmbedding(void)
 * \brief Tells video output to stop embedding video in an existing window.
 * \sa EmbedInWidget(WId, int, int, int, int)
 */ 
void VideoOutput::StopEmbedding(void)
{
    windows[0].StopEmbedding();
}

/**
 * \fn VideoOutput::DrawSlice(VideoFrame*, int, int, int, int)
 * \brief Informs video output of new data for frame,
 *        used for XvMC acceleration.
 */
void VideoOutput::DrawSlice(VideoFrame *frame, int x, int y, int w, int h)
{
    (void)frame;
    (void)x;
    (void)y;
    (void)w;
    (void)h;
}

void VideoOutput::GetOSDBounds(QRect &total, QRect &visible,
                               float &visible_aspect,
                               float &font_scaling,
                               float themeaspect) const
{
    total = GetTotalOSDBounds();
    visible = GetVisibleOSDBounds(visible_aspect, font_scaling, themeaspect);
}

/**
 * \fn VideoOutput::GetVisibleOSDBounds(float&,float&,float) const
 * \brief Returns visible portions of total OSD bounds
 * \param visible_aspect physical aspect ratio of bounds returned
 * \param font_scaling   scaling to apply to fonts
 */
QRect VideoOutput::GetVisibleOSDBounds(
    float &visible_aspect, float &font_scaling, float themeaspect) const
{
    if (!hasFullScreenOSD())
    {
        return windows[0].GetVisibleOSDBounds(
            visible_aspect, font_scaling, themeaspect);
    }

    QRect dvr = windows[0].GetDisplayVisibleRect();

    // This rounding works for I420 video...
    QSize dvr2 = QSize(dvr.width()  & ~0x3,
                       dvr.height() & ~0x1);

    float dispPixelAdj = 1.0f;
    if (dvr2.height() && dvr2.width())
        dispPixelAdj = (GetDisplayAspect() * dvr2.height()) / dvr2.width();
    visible_aspect = themeaspect / dispPixelAdj;
    font_scaling   = 1.0f;
    return QRect(QPoint(0,0), dvr2);
}

/**
 * \fn VideoOutput::GetTotalOSDBounds(void) const
 * \brief Returns total OSD bounds
 */
QRect VideoOutput::GetTotalOSDBounds(void) const
{
    if (!hasFullScreenOSD())
        return windows[0].GetTotalOSDBounds();

    QRect dvr = windows[0].GetDisplayVisibleRect();
    QSize dvr2 = QSize(dvr.width()  & ~0x3,
                       dvr.height() & ~0x1);

    return QRect(QPoint(0,0), dvr2);
}

bool VideoOutput::AllowPreviewEPG(void) const
{
    return windows[0].IsPreviewEPGAllowed();
}

/**
 * \fn VideoOutput::MoveResize(void)
 * \brief performs all the calculations for video framing and any resizing.
 *
 * First we apply playback over/underscanning and offsetting,
 * then we letterbox settings, and finally we apply manual
 * scale & move properties for "Zoom Mode".
 *
 * \sa Zoom(ZoomDirection), ToggleAdjustFill(int)
 */
void VideoOutput::MoveResize(void)
{
    windows[0].MoveResize();
}

/**
 * \fn VideoOutput::Zoom(ZoomDirection)
 * \brief Sets up zooming into to different parts of the video, the zoom
 *        is actually applied in MoveResize().
 * \sa ToggleAdjustFill(AdjustFillMode)
 */
void VideoOutput::Zoom(ZoomDirection direction)
{
    windows[0].Zoom(direction);
}

/**
 * \fn VideoOutput::ToggleAspectOverride(AspectOverrideMode)
 * \brief Enforce different aspect ration than detected,
 *        then calls VideoAspectRatioChanged(float)
 *        to apply them.
 * \sa Zoom(ZoomDirection), ToggleAdjustFill(AdjustFillMode)
 */
void VideoOutput::ToggleAspectOverride(AspectOverrideMode aspectMode)
{
    windows[0].ToggleAspectOverride(aspectMode);
}

/**
 * \fn VideoOutput::ToggleAdjustFill(AdjustFillMode)
 * \brief Sets up letterboxing for various standard video frame and
 *        monitor dimensions, then calls MoveResize()
 *        to apply them.
 * \sa Zoom(ZoomDirection), ToggleAspectOverride(AspectOverrideMode)
 */
void VideoOutput::ToggleAdjustFill(AdjustFillMode adjustFill)
{
    windows[0].ToggleAdjustFill(adjustFill);
}

int VideoOutput::ChangePictureAttribute(
    PictureAttribute attributeType, bool direction)
{
    int curVal = GetPictureAttribute(attributeType);
    if (curVal < 0)
        return -1;

    int newVal = curVal + ((direction) ? +1 : -1);

    if (kPictureAttribute_Hue == attributeType)
        newVal = newVal % 100;

    newVal = min(max(newVal, 0), 100);

    return SetPictureAttribute(attributeType, newVal);
}

/**
 * \fn VideoOutput::SetPictureAttribute(PictureAttribute, int)
 * \brief Sets a specified picture attribute.
 * \param attribute Picture attribute to set.
 * \param newValue  Value to set attribute to.
 * \return Set value if it succeeds, -1 if it does not.
 */
int VideoOutput::SetPictureAttribute(PictureAttribute attribute, int newValue)
{
    (void)attribute;
    (void)newValue;
    return -1;
}

int VideoOutput::GetPictureAttribute(PictureAttribute attributeType) const
{
    PictureSettingMap::const_iterator it = db_pict_attr.find(attributeType);
    if (it == db_pict_attr.end())
        return -1;
    return *it;
}

void VideoOutput::InitPictureAttributes(void)
{
    PictureSettingMap::const_iterator it = db_pict_attr.begin();
    for (; it != db_pict_attr.end(); ++it)
        SetPictureAttribute(it.key(), *it);
}

void VideoOutput::SetPictureAttributeDBValue(
    PictureAttribute attributeType, int newValue)
{
    QString dbName = QString::null;
    if (kPictureAttribute_Brightness == attributeType)
        dbName = "PlaybackBrightness";
    else if (kPictureAttribute_Contrast == attributeType)
        dbName = "PlaybackContrast";
    else if (kPictureAttribute_Colour == attributeType)
        dbName = "PlaybackColour";
    else if (kPictureAttribute_Hue == attributeType)
        dbName = "PlaybackHue";

    if (!dbName.isEmpty())
        gContext->SaveSetting(dbName, newValue);

    db_pict_attr[attributeType] = newValue;
}

/**
\ brief return OSD renderer type for this videoOutput
*/
QString VideoOutput::GetOSDRenderer(void) const
{
    return db_vdisp_profile->GetOSDRenderer();
}

/*
 * \brief Determines PIP Window size and Position.
 */
QRect VideoOutput::GetPIPRect(
    PIPLocation location, NuppelVideoPlayer *pipplayer, bool do_pixel_adj) const
{
    return windows[0].GetPIPRect(location, pipplayer, do_pixel_adj);
}

/**
 * \fn VideoOutput::DoPipResize(int,int)
 * \brief Sets up Picture in Picture image resampler.
 * \param pipwidth  input width
 * \param pipheight input height
 * \sa ShutdownPipResize(), ShowPIPs(VideoFrame*,const PIPMap&)
 */
void VideoOutput::DoPipResize(int pipwidth, int pipheight)
{
    QSize vid_size = QSize(pipwidth, pipheight);
    if (vid_size == pip_desired_display_size)
        return;

    ShutdownPipResize();

    pip_video_size   = vid_size;
    pip_display_size = pip_desired_display_size;

    int sz = pip_display_size.height() * pip_display_size.width() * 3 / 2;
    pip_tmp_buf = new unsigned char[sz];
    pip_tmp_buf2 = new unsigned char[sz];

#if ENABLE_SWSCALE
    pip_scaling_context = sws_getCachedContext(pip_scaling_context,
                              pip_video_size.width(), pip_video_size.height(),
                              PIX_FMT_YUV420P,
                              pip_display_size.width(),
                              pip_display_size.height(),
                              PIX_FMT_YUV420P, SWS_FAST_BILINEAR,
                              NULL, NULL, NULL);
#else
    pip_scaling_context = img_resample_init(
        pip_display_size.width(), pip_display_size.height(),
        pip_video_size.width(),   pip_video_size.height());
#endif
}

/**
 * \fn VideoOutput::ShutdownPipResize()
 * \brief Shuts down Picture in Picture image resampler.
 * \sa VideoOutput::DoPipResize(int,int),
 *     ShowPIPs(VideoFrame*,const PIPMap&)
 */
void VideoOutput::ShutdownPipResize(void)
{
    if (pip_tmp_buf)
    {
        delete [] pip_tmp_buf;
        pip_tmp_buf   = NULL;
    }

    if (pip_tmp_buf2)
    {
        delete [] pip_tmp_buf;
        pip_tmp_buf = NULL;
    }

    if (pip_scaling_context)
    {
#if ENABLE_SWSCALE
        sws_freeContext(pip_scaling_context);
#else
        img_resample_close(pip_scaling_context);
#endif
        pip_scaling_context = NULL;
    }

    pip_video_size   = QSize(0,0);
    pip_display_size = QSize(0,0);
}

void VideoOutput::ShowPIPs(VideoFrame *frame, const PIPMap &pipPlayers)
{
    PIPMap::const_iterator it = pipPlayers.begin();
    for (; it != pipPlayers.end(); ++it)
        ShowPIP(frame, it.key(), *it);
}

/**
 * \fn VideoOutput::ShowPIP(VideoFrame*,NuppelVideoPlayer*,PIPLocation)
 * \brief Composites PiP image onto a video frame.
 *
 *  Note: This only works with memory backed VideoFrames,
 *        that is not XvMC, OpenGL, VDPAU, etc.
 *
 * \param frame     Frame to composite PiP onto.
 * \param pipplayer Picture-in-Picture NVP.
 * \param loc       Location of this PiP on the frame.
 */
void VideoOutput::ShowPIP(VideoFrame        *frame,
                          NuppelVideoPlayer *pipplayer,
                          PIPLocation        loc)
{
    if (!pipplayer)
        return;

    const float video_aspect           = windows[0].GetVideoAspect();
    const QRect display_video_rect     = windows[0].GetDisplayVideoRect();
    const QRect video_rect             = windows[0].GetVideoRect();
    const QRect display_visible_rect   = windows[0].GetDisplayVisibleRect();
    const QSize video_disp_dim         = windows[0].GetVideoDispDim();

    int pipw, piph;
    VideoFrame *pipimage       = pipplayer->GetCurrentFrame(pipw, piph);
    const bool  pipActive      = pipplayer->IsPIPActive();
    const bool  pipVisible     = pipplayer->IsPIPVisible();
    const float pipVideoAspect = pipplayer->GetVideoAspect();
    const QSize pipVideoDim    = pipplayer->GetVideoBufferSize();

    // If PiP is not initialized to values we like, silently ignore the frame.
    if ((video_aspect <= 0) || (pipVideoAspect <= 0) || 
        (frame->height <= 0) || (frame->width <= 0) ||
        !pipimage || !pipimage->buf || pipimage->codec != FMT_YV12)
    {
        pipplayer->ReleaseCurrentFrame(pipimage);
        return;
    }

    if (!pipVisible)
    {
        pipplayer->ReleaseCurrentFrame(pipimage);
        return;
    }

    QRect position = GetPIPRect(loc, pipplayer);
    pip_desired_display_size = position.size();

    // Scale the image if we have to...
    unsigned char *pipbuf = pipimage->buf;
    if (pipw != pip_desired_display_size.width() ||
        piph != pip_desired_display_size.height())
    {
        DoPipResize(pipw, piph);

        bzero(&pip_tmp_image, sizeof(pip_tmp_image));

        if (pip_tmp_buf && pip_scaling_context)
        {
            AVPicture img_in, img_out;

            avpicture_fill(
                &img_out, (uint8_t *)pip_tmp_buf, PIX_FMT_YUV420P,
                pip_display_size.width(), pip_display_size.height());

            avpicture_fill(&img_in, (uint8_t *)pipimage->buf, PIX_FMT_YUV420P,
                           pipw, piph);

#if ENABLE_SWSCALE
            sws_scale(pip_scaling_context, img_in.data, img_in.linesize, 0,
                      piph, img_out.data, img_out.linesize);
#else
            img_resample(pip_scaling_context, &img_out, &img_in);
#endif
          
            if (pipActive)
            {
                AVPicture img_padded;
                avpicture_fill(
                    &img_padded, (uint8_t *)pip_tmp_buf2, PIX_FMT_YUV420P,
                    pip_display_size.width(), pip_display_size.height());

                int color[3] = { 20, 0, 200 }; //deep red YUV format
                av_picture_pad(&img_padded, &img_out,
                               pip_display_size.height(),
                               pip_display_size.width(),
                               PIX_FMT_YUV420P, 10, 10, 10, 10, color);
                
                pipbuf = pip_tmp_buf2;
            }
            else
            {
                pipbuf = pip_tmp_buf;
            }

            pipw = pip_display_size.width();
            piph = pip_display_size.height();

            init(&pip_tmp_image,
                 FMT_YV12,
                 pipbuf,
                 pipw, piph,
                 pipimage->bpp, sizeof(pipbuf));
        }
    }

    int xoff = position.left();
    int yoff = position.top();
    uint xoff2[3]  = { xoff, xoff>>1, xoff>>1 };
    uint yoff2[3]  = { yoff, yoff>>1, yoff>>1 };

    uint pip_height = pip_tmp_image.height;
    uint height[3] = { pip_height, pip_height>>1, pip_height>>1 };
    
    for (int p = 0; p < 3; p++)
    {
        for (uint h = 2; h < height[p]; h++)
        {
            memcpy((frame->buf + frame->offsets[p]) + (h + yoff2[p]) * 
                   frame->pitches[p] + xoff2[p],
                   (pip_tmp_image.buf + pip_tmp_image.offsets[p]) + h *
                   pip_tmp_image.pitches[p], pip_tmp_image.pitches[p]);
        }
    }

    // we're done with the frame, release it
    pipplayer->ReleaseCurrentFrame(pipimage);
}

/**
 * \brief Sets up Picture in Picture image resampler.
 * \param inDim  input width and height
 * \param outDim output width and height
 * \sa ShutdownPipResize(), ShowPIPs(VideoFrame*,const PIPMap&)
 */
void VideoOutput::DoVideoResize(const QSize &inDim, const QSize &outDim)
{
    if ((inDim == vsz_video_size) && (outDim == vsz_display_size))
        return;

    ShutdownVideoResize();

    vsz_video_size   = inDim;
    vsz_display_size = outDim;

    int sz = vsz_display_size.height() * vsz_display_size.width() * 3 / 2;
    vsz_tmp_buf = new unsigned char[sz];

#if ENABLE_SWSCALE
    vsz_scale_context = sws_getCachedContext(vsz_scale_context,
                              vsz_video_size.width(), vsz_video_size.height(),
                              PIX_FMT_YUV420P,
                              vsz_display_size.width(),
                              vsz_display_size.height(),
                              PIX_FMT_YUV420P, SWS_FAST_BILINEAR,
                              NULL, NULL, NULL);
#else
    vsz_scale_context = img_resample_init(
        vsz_display_size.width(), vsz_display_size.height(),
        vsz_video_size.width(),   vsz_video_size.height());
#endif
}

void VideoOutput::ResizeVideo(VideoFrame *frame)
{
    if (vsz_desired_display_rect.isNull() || frame->codec !=  FMT_YV12)
        return;

    QRect resize = vsz_desired_display_rect;
    QSize frameDim(frame->width, frame->height);

    // if resize is outside existing frame, abort
    bool abort =
        (resize.right() > frame->width || resize.bottom() > frame->height ||
         resize.width() > frame->width || resize.height() > frame->height);
    // if resize == existing frame, no need to carry on
    abort |= !resize.left() && !resize.top() && (resize.size() == frameDim);

    if (abort)
    {
        vsz_enabled = false;
        ShutdownVideoResize();
        vsz_desired_display_rect.setRect(0,0,0,0);
        return;
    }

    DoVideoResize(frameDim, resize.size());

    if (vsz_tmp_buf && vsz_scale_context)
    {
        AVPicture img_in, img_out;

        avpicture_fill(&img_out, (uint8_t *)vsz_tmp_buf, PIX_FMT_YUV420P,
                       resize.width(), resize.height());
        avpicture_fill(&img_in, (uint8_t *)frame->buf, PIX_FMT_YUV420P,
                       frame->width, frame->height);
#if ENABLE_SWSCALE
        sws_scale(vsz_scale_context, img_in.data, img_in.linesize, 0,
                      frame->height, img_out.data, img_out.linesize);
#else
        img_resample(vsz_scale_context, &img_out, &img_in);
#endif
    }

    int xoff = resize.left();
    int yoff = resize.top();
    int resw = resize.width();

    // Copy Y (intensity values)
    for (int i = 0; i < resize.height(); i++)
    {
        memcpy(frame->buf + (i + yoff) * frame->width + xoff,
               vsz_tmp_buf + i * resw, resw);
    }

    // Copy U & V (half plane chroma values)
    xoff /= 2;
    yoff /= 2;

    unsigned char *uptr = frame->buf + frame->width * frame->height;
    unsigned char *vptr = frame->buf + frame->width * frame->height * 5 / 4;
    int vidw = frame->width / 2;

    unsigned char *videouptr = vsz_tmp_buf + resw * resize.height();
    unsigned char *videovptr = vsz_tmp_buf + resw * resize.height() * 5 / 4;
    resw /= 2;
    for (int i = 0; i < resize.height() / 2; i ++)
    {
        memcpy(uptr + (i + yoff) * vidw + xoff, videouptr + i * resw, resw);
        memcpy(vptr + (i + yoff) * vidw + xoff, videovptr + i * resw, resw);
    }
}

AspectOverrideMode VideoOutput::GetAspectOverride(void) const
{
    return windows[0].GetAspectOverride();
}

AdjustFillMode VideoOutput::GetAdjustFill(void) const
{
    return windows[0].GetAdjustFill();
}

float VideoOutput::GetDisplayAspect(void) const
{
    return windows[0].GetDisplayAspect();
}

bool VideoOutput::IsVideoScalingAllowed(void) const
{
    return windows[0].IsVideoScalingAllowed();
}

void VideoOutput::ShutdownVideoResize(void)
{
    if (vsz_tmp_buf)
    {
        delete [] vsz_tmp_buf;
        vsz_tmp_buf = NULL;
    }

    if (vsz_scale_context)
    {
#if ENABLE_SWSCALE
        sws_freeContext(vsz_scale_context);
#else
        img_resample_close(vsz_scale_context);
#endif
        vsz_scale_context = NULL;
    }

    vsz_video_size   = QSize(0,0);
    vsz_display_size = QSize(0,0);
    vsz_enabled      = false;
}

void VideoOutput::SetVideoResize(const QRect &videoRect)
{
    if (!videoRect.isValid()    ||
         videoRect.width()  < 1 || videoRect.height() < 1 ||
         videoRect.left()   < 0 || videoRect.top()    < 0)
    {
        vsz_enabled = false;
        ShutdownVideoResize();
        vsz_desired_display_rect.setRect(0,0,0,0);
    }
    else
    {
        vsz_enabled = true;
        vsz_desired_display_rect = videoRect;
    }
}

/**
 * \brief Disable or enable underscan/overscan
 */
void VideoOutput::SetVideoScalingAllowed(bool change)
{
    windows[0].SetVideoScalingAllowed(change);
}

/**
 * \fn VideoOutput::DisplayOSD(VideoFrame*,OSD *,int,int)
 * \brief If the OSD has changed, this will convert the OSD buffer
 *        to the OSDSurface's color format.
 *
 *  If the destination format is either IA44 or AI44 the osd is
 *  converted to greyscale.
 *
 * \return 1 if changed, -1 on error and 0 otherwise
 */ 
int VideoOutput::DisplayOSD(VideoFrame *frame, OSD *osd, int stride,
                            int revision)
{
    if (!osd)
        return -1;

    if (vsz_enabled)
        ResizeVideo(frame);

    QSize video_dim = windows[0].GetVideoDim();

    OSDSurface *surface = osd->Display();
    if (!surface)
        return -1;

    bool changed = (-1 == revision) ?
        surface->Changed() : (surface->GetRevision()!=revision);

    switch (frame->codec)
    {
        case FMT_YV12: // works for YUV & YVU 420 formats due to offsets
        {
            surface->BlendToYV12(frame->buf + frame->offsets[0],
                                 frame->buf + frame->offsets[1],
                                 frame->buf + frame->offsets[2],
                                 frame->pitches[0],
                                 frame->pitches[1],
                                 frame->pitches[2]);
            break;
        }
        case FMT_AI44:
        {
            if (stride < 0)
                stride = video_dim.width(); // 8 bits per pixel
            if (changed)
                surface->DitherToAI44(frame->buf, stride, video_dim.height());
            break;
        }
        case FMT_IA44:
        {
            if (stride < 0)
                    stride = video_dim.width(); // 8 bits per pixel
            if (changed)
                surface->DitherToIA44(frame->buf, stride, video_dim.height());
            break;
        }
        case FMT_ARGB32:
        {
            if (stride < 0)
                stride = video_dim.width()*4; // 32 bits per pixel
            if (changed)
                surface->BlendToARGB(frame->buf, stride, video_dim.height());
            break;
        }
        default:
            break;
    }
    return (changed) ? 1 : 0;
}

/**
 * \fn VideoOutput::CopyFrame(VideoFrame*, const VideoFrame*)
 * \brief Copies frame data from one VideoFrame to another.
 * 
 *  Note: The frames must have the same width, height, and format.
 * \param to   The destination frame.
 * \param from The source frame 
 */
void VideoOutput::CopyFrame(VideoFrame *to, const VideoFrame *from)
{
    if (to == NULL || from == NULL)
        return;

    to->frameNumber = from->frameNumber;

    // guaranteed to be correct sizes.
    if (from->size == to->size)
        memcpy(to->buf, from->buf, from->size);
    else if ((to->pitches[0] == from->pitches[0]) &&
             (to->pitches[1] == from->pitches[1]) &&
             (to->pitches[2] == from->pitches[2]))
    {
        memcpy(to->buf + to->offsets[0], from->buf + from->offsets[0],
               from->pitches[0] * from->height);
        memcpy(to->buf + to->offsets[1], from->buf + from->offsets[1],
               from->pitches[1] * (from->height>>1));
        memcpy(to->buf + to->offsets[2], from->buf + from->offsets[2],
               from->pitches[2] * (from->height>>1));        
    }
    else
    {
        uint f[3] = { from->height,   from->height>>1, from->height>>1, };
        uint t[3] = { to->height,     to->height>>1,   to->height>>1,   };
        uint h[3] = { min(f[0],t[0]), min(f[1],t[1]),  min(f[2],t[2]),  };
        for (uint i = 0; i < 3; i++)
        {
            for (uint j = 0; j < h[i]; j++)
            {
                memcpy(to->buf   + to->offsets[i]   + (j * to->pitches[i]),
                       from->buf + from->offsets[i] + (j * from->pitches[i]),
                       min(from->pitches[i], to->pitches[i]));
            }
        }
    }

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

bool VideoOutput::MoveScaleDVDButton(QRect button, QSize &scale,
                                     QPoint &position, QRect &crop)
{
    float hscale, vscale, tmp = 0.0;
    QRect vis_osd = GetVisibleOSDBounds(tmp, tmp, tmp);
    QRect tot_osd = GetTotalOSDBounds();
    QRect vid_rec = windows[0].GetVideoRect();

    if ((vis_osd == vid_rec) ||
         vid_rec.width() < 1 ||
         vid_rec.height() < 1)
        return false;

    if (hasFullScreenOSD())
    {
        QRect dvr_rec = windows[0].GetDisplayVideoRect();

        vscale = (float)dvr_rec.width() / (float)vid_rec.width();
        hscale = (float)dvr_rec.height() / (float)vid_rec.height();

        QMatrix m;
        m.translate(dvr_rec.left(), dvr_rec.top());
        m.scale(vscale, hscale);
        crop = m.mapRect(button);
        QPoint cut = QPoint((crop.left() < 0) ? -crop.left() : 0,
                            (crop.top() < 0) ? -crop.top() : 0);
        scale = crop.size();
        crop  = crop.intersected(tot_osd);
        position = QPoint(crop.left(), crop.top());
        crop.moveTopLeft(cut);
        return true;
    }

    crop = QRect(0,0,0,0);
    vscale = (float)tot_osd.width() / (float)vid_rec.width();
    hscale = (float)tot_osd.height() / (float)vid_rec.height();

    scale = QSize((int)ceil(vscale * (float)button.width()),
                  (int)ceil(hscale * (float)button.height()));
    int btnX = (int)(vscale * (float)button.left());
    int btnY = (int)(hscale * (float)button.top());
    int xoff = tot_osd.left() - vis_osd.left();
    int yoff = tot_osd.top() - vis_osd.top();
    position = QPoint(btnX + xoff, btnY + yoff);
    return true;
}

void VideoOutput::SetPIPState(PIPState setting)
{
    windows[0].SetPIPState(setting);
}


static QString to_comma_list(const QStringList &list)
{
    QString ret = "";
    for (QStringList::const_iterator it = list.begin(); it != list.end(); ++it)
        ret += *it + ",";

    if (ret.length())
        return ret.left(ret.length()-1);

    return "";
}

bool VideoOutput::IsEmbedding(void)
{
    return windows[0].IsEmbedding();
}

void VideoOutput::ExposeEvent(void)
{
    windows[0].SetNeedRepaint(true);
}
