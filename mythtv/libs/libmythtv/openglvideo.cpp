// MythTV headers
#include "mythcontext.h"
#include "tv.h"
#include "openglvideo.h"
#include "openglcontext.h"
#include "myth_imgconvert.h"

// AVLib header
extern "C" {
#include "avcodec.h"
}

// OpenGL headers
#define GL_GLEXT_PROTOTYPES
#ifdef USING_X11
#define GLX_GLXEXT_PROTOTYPES
#define XMD_H 1
//#include <GL/glx.h>
#include <GL/gl.h>
#undef GLX_ARB_get_proc_address
#endif // USING_X11
//#include <GL/glxext.h>
//#include <GL/glext.h>
#include "util-opengl.h"

#define LOC QString("GLVid: ")
#define LOC_ERR QString("GLVid, Error: ")

class OpenGLFilter
{
    public:
        vector<GLuint> fragmentPrograms;
        uint           numInputs;
        vector<GLuint> frameBuffers;
        vector<GLuint> frameBufferTextures;
        DisplayBuffer  outputBuffer;
};

OpenGLVideo::OpenGLVideo() :
    gl_context(NULL),         video_dim(0,0),
    actual_video_dim(0,0),    viewportSize(0,0),
    masterViewportSize(0,0),  display_visible_rect(0,0,0,0),
    display_video_rect(0,0,0,0), video_rect(0,0,0,0),
    frameBufferRect(0,0,0,0), softwareDeinterlacer(QString::null),
    hardwareDeinterlacer(QString::null), hardwareDeinterlacing(false),
    useColourControl(false),  viewportControl(false),
    inputTextureSize(0,0),    currentFrameNum(0),
    inputUpdated(false),
    textureRects(false),      textureType(GL_TEXTURE_2D),
    helperTexture(0),         defaultUpsize(kGLFilterResize),
    convertSize(0,0),         convertBuf(NULL),
    videoResize(false),       videoResizeRect(0,0,0,0),
    gl_features(0),
    gl_letterbox_colour(kLetterBoxColour_Black)
{
}

OpenGLVideo::~OpenGLVideo()
{
    OpenGLContextLocker ctx_lock(gl_context);
    Teardown();
}

// locking ok
void OpenGLVideo::Teardown(void)
{
    ShutDownYUV2RGB();

    if (helperTexture)
        gl_context->DeleteTexture(helperTexture);
    helperTexture = 0;

    DeleteTextures(&inputTextures);
    DeleteTextures(&referenceTextures);

    while (!filters.empty())
    {
        RemoveFilter(filters.begin()->first);
        filters.erase(filters.begin());
    }
}

// locking ok
bool OpenGLVideo::Init(OpenGLContext *glcontext, bool colour_control,
                       QSize videoDim, QRect displayVisibleRect,
                       QRect displayVideoRect, QRect videoRect,
                       bool viewport_control, QString options, bool osd,
                       LetterBoxColour letterbox_colour)
{
    gl_context            = glcontext;
    if (!gl_context)
        return false;

    OpenGLContextLocker ctx_lock(gl_context);

    actual_video_dim      = videoDim;
    video_dim             = videoDim;
    if (video_dim.height() == 1088)
        video_dim.setHeight(1080);
    display_visible_rect  = displayVisibleRect;
    display_video_rect    = displayVideoRect;
    video_rect            = videoRect;
    masterViewportSize    = QSize(1920, 1080);
    frameBufferRect       = QRect(QPoint(0,0), video_dim);
    softwareDeinterlacer  = "";
    hardwareDeinterlacing = false;
    useColourControl      = colour_control;
    viewportControl       = viewport_control;
    inputTextureSize      = QSize(0,0);
    convertSize           = QSize(0,0);
    videoResize           = false;
    videoResizeRect       = QRect(0,0,0,0);
    currentFrameNum       = -1;
    inputUpdated          = false;
    gl_letterbox_colour   = letterbox_colour;

    gl_features = ParseOptions(options) & gl_context->GetFeatures();

    if (viewportControl)
    {
        gl_context->SetFeatures(gl_features);
        gl_context->SetSwapInterval(1);
        gl_context->SetFence();
    }

    if (options.contains("openglbicubic"))
        defaultUpsize = kGLFilterBicubic;

    if ((defaultUpsize != kGLFilterBicubic) && (gl_features & kGLExtRect))
        gl_context->GetTextureType(textureType, textureRects);

    SetViewPort(display_visible_rect.size());

    bool use_pbo = gl_features & kGLExtPBufObj;

    if (osd)
    {
        QSize osdsize = display_visible_rect.size();
        GLuint tex = CreateVideoTexture(osdsize, inputTextureSize, use_pbo);

        if (tex &&
            AddFilter(kGLFilterYUV2RGBA) &&
            AddFilter(kGLFilterResize))
        {
            inputTextures.push_back(tex);
        }
        else
        {
            Teardown();
        }
    }
    else
    {
        GLuint tex = CreateVideoTexture(actual_video_dim,
                                        inputTextureSize, use_pbo);

        if (tex && AddFilter(kGLFilterYUV2RGB))
        {
            inputTextures.push_back(tex);
        }
        else
        {
            Teardown();
        }
    }

    if (filters.empty())
    {
        if (osd)
        {
            Teardown();
            return false;
        }

        VERBOSE(VB_PLAYBACK, LOC +
                "OpenGL colour conversion failed.\n\t\t\t"
                "Falling back to software conversion.\n\t\t\t"
                "Any opengl filters will also be disabled.");

        GLuint bgra32tex = CreateVideoTexture(actual_video_dim,
                                             inputTextureSize, use_pbo);

        if (bgra32tex && AddFilter(kGLFilterResize))
        {
            inputTextures.push_back(bgra32tex);
        }
        else
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR + "Fatal error");
            Teardown();
            return false;
        }
    }

#ifdef MMX
    bool mmx = true;
#else
    bool mmx = false;
#endif

    CheckResize(false);

    VERBOSE(VB_PLAYBACK, LOC + 
            QString("Using packed textures with%1 mmx and with%2 PBOs")
            .arg(mmx ? "" : "out").arg(use_pbo ? "" : "out"));

    return true;
}

void OpenGLVideo::CheckResize(bool deinterlacing)
{
    // to improve performance on slower cards
    bool resize_up = (video_dim.height() < display_video_rect.height()) ||
                     (video_dim.width()  < display_video_rect.width());

    // to ensure deinterlacing works correctly
    bool resize_down = (video_dim.height() > display_video_rect.height()) &&
                        deinterlacing;

    if (resize_up && (defaultUpsize == kGLFilterBicubic))
    {
        RemoveFilter(kGLFilterResize);
        filters.erase(kGLFilterResize);
        AddFilter(kGLFilterBicubic);
        return;
    }

    if ((resize_up && (defaultUpsize == kGLFilterResize)) || resize_down)
    {
        RemoveFilter(kGLFilterBicubic);
        filters.erase(kGLFilterBicubic);
        AddFilter(kGLFilterResize);
        return;
    }

    if (!filters.count(kGLFilterYUV2RGBA))
    {
        RemoveFilter(kGLFilterResize);
        filters.erase(kGLFilterResize);
    }

    RemoveFilter(kGLFilterBicubic);
    filters.erase(kGLFilterBicubic);

    OptimiseFilters();
}

bool OpenGLVideo::OptimiseFilters(void)
{
    glfilt_map_t::reverse_iterator it;

    // add/remove required frame buffer objects
    // and link filters
    uint buffers_needed = 1;
    bool last_filter    = true;
    for (it = filters.rbegin(); it != filters.rend(); it++)
    {
        if (!last_filter)
        {
            it->second->outputBuffer = kFrameBufferObject;
            uint buffers_have = it->second->frameBuffers.size();
            int buffers_diff = buffers_needed - buffers_have;
            if (buffers_diff > 0)
            {
                uint tmp_buf, tmp_tex;
                QSize fb_size = GetTextureSize(video_dim);
                for (int i = 0; i < buffers_diff; i++)
                {
                    if (!AddFrameBuffer(tmp_buf, fb_size, tmp_tex, video_dim))
                        return false;
                    else
                    {
                        it->second->frameBuffers.push_back(tmp_buf);
                        it->second->frameBufferTextures.push_back(tmp_tex);
                    }
                }
            }
            else if (buffers_diff < 0)
            {
                for (int i = 0; i > buffers_diff; i--)
                {
                    OpenGLFilter *filt = it->second;

                    gl_context->DeleteFrameBuffer(
                        filt->frameBuffers.back());
                    gl_context->DeleteTexture(
                        filt->frameBufferTextures.back());

                    filt->frameBuffers.pop_back();
                    filt->frameBufferTextures.pop_back();
                }
            }
        }
        else
        {
            it->second->outputBuffer = kDefaultBuffer;
            last_filter = false;
        }
        buffers_needed = it->second->numInputs;
    }

    SetFiltering();

    return true;
}

// locking ok
void OpenGLVideo::SetFiltering(void)
{
    // filter settings included for performance only
    // no (obvious) quality improvement over GL_LINEAR throughout
    if (filters.empty() || filters.size() == 1)
    {
        SetTextureFilters(&inputTextures, GL_LINEAR, GL_CLAMP_TO_EDGE);
        return;
    }

    SetTextureFilters(&inputTextures, GL_NEAREST, GL_CLAMP_TO_EDGE);

    glfilt_map_t::reverse_iterator rit;
    int last_filter = 0;

    for (rit = filters.rbegin(); rit != filters.rend(); rit++)
    {
        if (last_filter == 1)
        {
            SetTextureFilters(&(rit->second->frameBufferTextures),
                              GL_LINEAR, GL_CLAMP_TO_EDGE);
        }
        else if (last_filter > 1)
        {
            SetTextureFilters(&(rit->second->frameBufferTextures),
                              GL_NEAREST, GL_CLAMP_TO_EDGE);
        }
    }
}

// locking ok
bool OpenGLVideo::AddFilter(OpenGLFilterType filter)
{
    if (filters.count(filter))
        return true;

    bool success = true;

    VERBOSE(VB_PLAYBACK, LOC + QString("Creating %1 filter.")
            .arg(FilterToString(filter)));

    OpenGLFilter *temp = new OpenGLFilter();

    temp->numInputs = 1;
    GLuint program = 0;

    if (filter == kGLFilterBicubic)
    {
        if (helperTexture)
            gl_context->DeleteTexture(helperTexture);
 
        helperTexture = gl_context->CreateHelperTexture();
        if (!helperTexture)
            success = false;
    }

    if (filter != kGLFilterNone && filter != kGLFilterResize)
    {
        program = AddFragmentProgram(filter);
        if (!program)
            success = false;
        else
            temp->fragmentPrograms.push_back(program);
    }

    temp->outputBuffer       = kDefaultBuffer;

    temp->frameBuffers.clear();
    temp->frameBufferTextures.clear();

    filters[filter] = temp;

    success &= OptimiseFilters();

    if (success)
        return true;

    RemoveFilter(filter);
    filters.erase(filter);

    return false;
}

// locking ok
bool OpenGLVideo::RemoveFilter(OpenGLFilterType filter)
{
    if (!filters.count(filter))
        return true;

    VERBOSE(VB_PLAYBACK, LOC + QString("Removing %1 filter")
            .arg(FilterToString(filter)));

    vector<GLuint> temp;
    vector<GLuint>::iterator it;

    temp = filters[filter]->fragmentPrograms;
    for (it = temp.begin(); it != temp.end(); it++)
        gl_context->DeleteFragmentProgram(*it);
    filters[filter]->fragmentPrograms.clear();

    temp = filters[filter]->frameBuffers;
    for (it = temp.begin(); it != temp.end(); it++)
        gl_context->DeleteFrameBuffer(*it);
    filters[filter]->frameBuffers.clear();

    DeleteTextures(&(filters[filter]->frameBufferTextures));

    delete filters[filter];
    filters[filter] = NULL;

    return true;
}

// locking ok
void OpenGLVideo::TearDownDeinterlacer(void)
{
    if (!filters.count(kGLFilterYUV2RGB))
        return;

    OpenGLFilter *tmp = filters[kGLFilterYUV2RGB];

    if (tmp->fragmentPrograms.size() == 3)
    {
        gl_context->DeleteFragmentProgram(tmp->fragmentPrograms[2]);
        tmp->fragmentPrograms.pop_back();
    }

    if (tmp->fragmentPrograms.size() == 2)
    {
        gl_context->DeleteFragmentProgram(tmp->fragmentPrograms[1]);
        tmp->fragmentPrograms.pop_back();
    }

    DeleteTextures(&referenceTextures);
}

bool OpenGLVideo::AddDeinterlacer(const QString &deinterlacer)
{
    OpenGLContextLocker ctx_lock(gl_context);

    if (!filters.count(kGLFilterYUV2RGB))
        return false;

    if (hardwareDeinterlacer == deinterlacer)
        return true;

    TearDownDeinterlacer();

    bool success = true;

    uint ref_size = 2;

    if (deinterlacer == "openglbobdeint" ||
        deinterlacer == "openglonefield" ||
        deinterlacer == "opengldoubleratefieldorder")
    {
        ref_size = 0;
    }

    if (ref_size > 0)
    {
        bool use_pbo = gl_features & kGLExtPBufObj;

        for (; ref_size > 0; ref_size--)
        {
            GLuint tex = CreateVideoTexture(actual_video_dim, inputTextureSize, use_pbo);
            if (tex)
            {
                referenceTextures.push_back(tex);
            }
            else
            {
                success = false;
            }
        }
    }

    uint prog1 = AddFragmentProgram(kGLFilterYUV2RGB,
                                    deinterlacer, kScan_Interlaced);
    uint prog2 = AddFragmentProgram(kGLFilterYUV2RGB,
                                    deinterlacer, kScan_Intr2ndField);

    if (prog1 && prog2)
    {
        filters[kGLFilterYUV2RGB]->fragmentPrograms.push_back(prog1);
        filters[kGLFilterYUV2RGB]->fragmentPrograms.push_back(prog2);
    }
    else
    {
        success = false;
    }

    if (success)
    {
        CheckResize(hardwareDeinterlacing);
        hardwareDeinterlacer = deinterlacer;
        return true;
    }

    hardwareDeinterlacer = "";
    TearDownDeinterlacer();
 
    return false;
}

// locking ok
uint OpenGLVideo::AddFragmentProgram(OpenGLFilterType name,
                                     QString deint, FrameScanType field)
{
    if (!(gl_features & kGLExtFragProg))
    {
        VERBOSE(VB_PLAYBACK, LOC_ERR + "Fragment programs not supported");
        return 0;
    }

    QString program = GetProgramString(name, deint, field);

    uint ret;
    if (gl_context->CreateFragmentProgram(program, ret))
        return ret;

    return 0;
}

// locking ok
bool OpenGLVideo::AddFrameBuffer(uint &framebuffer, QSize fb_size,
                                 uint &texture, QSize vid_size)
{
    if (!(gl_features & kGLExtFBufObj))
    {
        VERBOSE(VB_PLAYBACK, LOC_ERR + "Framebuffer binding not supported.");
        return false;
    }

    texture = gl_context->CreateTexture(fb_size, vid_size, false, textureType);

    bool ok = gl_context->CreateFrameBuffer(framebuffer, texture);

    if (!ok)
        gl_context->DeleteTexture(texture);

    return ok;
}

// locking ok
void OpenGLVideo::SetViewPort(const QSize &viewPortSize)
{
    uint w = max(viewPortSize.width(),  video_dim.width());
    uint h = max(viewPortSize.height(), video_dim.height());

    viewportSize = QSize(w, h);

    if (!viewportControl)
        return;

    VERBOSE(VB_PLAYBACK, LOC + QString("Viewport: %1x%2")
            .arg(w).arg(h));
    gl_context->SetViewPort(viewportSize);
}

// locking ok
uint OpenGLVideo::CreateVideoTexture(QSize size, QSize &tex_size,
                                     bool use_pbo)
{
    QSize temp = GetTextureSize(size);
    uint tmp_tex = gl_context->CreateTexture(temp, size, use_pbo,
                                             textureType);

    if (!tmp_tex)
    {
        VERBOSE(VB_PLAYBACK, LOC_ERR + "Could not create texture.");
        return 0;
    }

    tex_size = temp;

    VERBOSE(VB_PLAYBACK, LOC + QString("Created texture (%1x%2)")
            .arg(temp.width()).arg(temp.height()));

    return tmp_tex;
}

// locking ok
QSize OpenGLVideo::GetTextureSize(const QSize &size)
{
    if (textureRects)
        return size;

    int w = 64;
    int h = 64;

    while (w < size.width())
    {
        w *= 2;
    }

    while (h < size.height())
    {
        h *= 2;
    }

    return QSize(w, h);
}

// locking ok
void OpenGLVideo::UpdateInputFrame(const VideoFrame *frame, bool soft_bob)
{
    OpenGLContextLocker ctx_lock(gl_context);

    if (frame->width  != actual_video_dim.width()  ||
        frame->height != actual_video_dim.height() ||
        frame->width  < 1 ||
        frame->height < 1)
    {
        ShutDownYUV2RGB();
        return;
    }

    if (filters.count(kGLFilterYUV2RGB) && (frame->codec == FMT_YV12))
    {
        if (hardwareDeinterlacing)
            RotateTextures();

        gl_context->UpdateTexture(inputTextures[0], frame->buf,
                                  frame->offsets, frame->pitches, FMT_YV12,
                                  frame->interlaced_frame && !soft_bob);
        inputUpdated = true;
        return;
    }

    // software yuv2rgb
    if (convertSize != actual_video_dim)
    {
        ShutDownYUV2RGB();

        VERBOSE(VB_PLAYBACK, LOC + "Init software conversion.");

        convertSize = actual_video_dim;
        convertBuf = new unsigned char[
            (actual_video_dim.width() * actual_video_dim.height() * 4) + 128];
    }

    if (convertBuf)
    {
        AVPicture img_in, img_out;

        avpicture_fill(&img_out, (uint8_t *)convertBuf, PIX_FMT_BGRA,
                       convertSize.width(), convertSize.height());
        avpicture_fill(&img_in, (uint8_t *)frame->buf, PIX_FMT_YUV420P,
                       convertSize.width(), convertSize.height());

#if ENABLE_SWSCALE
    myth_sws_img_convert(
#else
    img_convert(
#endif
                    &img_out, PIX_FMT_BGRA,
                    &img_in,  PIX_FMT_YUV420P,
                    convertSize.width(), convertSize.height());

        int offset = 0;
        gl_context->UpdateTexture(inputTextures[0], convertBuf,
                                  &offset, &offset, FMT_BGRA);
    }

    inputUpdated = true;
}

// locking ok
void OpenGLVideo::UpdateInput(const unsigned char *buf, const int *offsets,
                              int format, QSize size,
                              const unsigned char *alpha)
{
    OpenGLContextLocker ctx_lock(gl_context);

    if (size.width()  != actual_video_dim.width()  ||
        size.height() != actual_video_dim.height() ||
        format != FMT_YV12 || !alpha)
        return;

    int pitches[3] = {size.width(), size.width() >> 1, size.width() >> 1};

    gl_context->UpdateTexture(inputTextures[0], buf,
                              offsets, pitches, FMT_YV12,
                              false, alpha);

    inputUpdated = true;
}

// locking ok
void OpenGLVideo::ShutDownYUV2RGB(void)
{
    if (convertBuf)
    {
        delete convertBuf;
        convertBuf = NULL;
    }
    convertSize = QSize(0,0);
}

// locking ok
// TODO shouldn't this take a QSize, not QRect?
void OpenGLVideo::SetVideoResize(const QRect &rect)
{
    OpenGLContextLocker ctx_lock(gl_context);

    bool abort = ((rect.right()  > video_dim.width())  ||
                  (rect.bottom() > video_dim.height()) ||
                  (rect.width()  > video_dim.width())  ||
                  (rect.height() > video_dim.height()));

    // if resize == existing frame, no need to carry on

    abort |= !rect.left() && !rect.top() && (rect.size() == video_dim);

    if (!abort)
    {
        videoResize     = true;
        videoResizeRect = rect;
        return;
    }

    DisableVideoResize();
}

// locking ok
void OpenGLVideo::DisableVideoResize(void)
{
    OpenGLContextLocker ctx_lock(gl_context);

    videoResize     = false;
    videoResizeRect = QRect(0, 0, 0, 0);
}

void OpenGLVideo::CalculateResize(float &left,  float &top,
                                  float &right, float &bottom)
{
    // FIXME video aspect == display aspect

    if ((video_dim.height() <= 0) || (video_dim.width() <= 0))
        return;

    float height     = display_visible_rect.height();
    float new_top    = height - ((float)videoResizeRect.bottom() /
                                 (float)video_dim.height()) * height;
    float new_bottom = height - ((float)videoResizeRect.top() /
                                 (float)video_dim.height()) * height;

    left   = (((float) videoResizeRect.left() / (float) video_dim.width()) *
              display_visible_rect.width());
    right  = (((float) videoResizeRect.right() / (float) video_dim.width()) *
              display_visible_rect.width());

    top    = new_top;
    bottom = new_bottom;
}

// locking ok
void OpenGLVideo::SetDeinterlacing(bool deinterlacing)
{
    if (deinterlacing == hardwareDeinterlacing)
        return;

    hardwareDeinterlacing = deinterlacing;

    OpenGLContextLocker ctx_lock(gl_context);
    CheckResize(hardwareDeinterlacing);
}

void OpenGLVideo::SetSoftwareDeinterlacer(const QString &filter)
{
    softwareDeinterlacer = filter;
    softwareDeinterlacer.detach();
}

// locking ok
void OpenGLVideo::PrepareFrame(FrameScanType scan, bool softwareDeinterlacing,
                               long long frame, bool draw_border)
{
    if (inputTextures.empty() || filters.empty())
        return;

    OpenGLContextLocker ctx_lock(gl_context);

    // enable correct texture type
    gl_context->EnableTextures(inputTextures[0]);

    vector<GLuint> inputs = inputTextures;
    QSize inputsize = inputTextureSize;
    QSize realsize  = GetTextureSize(video_dim);

    glfilt_map_t::iterator it;
    for (it = filters.begin(); it != filters.end(); it++)
    {
        OpenGLFilterType type = it->first;
        OpenGLFilter *filter = it->second;

        // skip colour conversion for osd already in frame buffer
        if (!inputUpdated && type == kGLFilterYUV2RGBA)
        {
            inputs = filter->frameBufferTextures;
            inputsize = realsize;
            continue;
        }

        // texture coordinates
        float t_right = (float)video_dim.width();
        float t_bottom  = (float)video_dim.height();
        float t_top = 0.0f;
        float t_left = 0.0f;
        float trueheight = (float)video_dim.height();

        // only apply overscan on last filter
        if (filter->outputBuffer == kDefaultBuffer)
        {
            t_left   = (float)video_rect.left();
            t_right  = (float)video_rect.width() + t_left;
            t_top    = (float)video_rect.top();
            t_bottom = (float)video_rect.height() + t_top;
        }

        if (!textureRects &&
            (inputsize.width() > 0) && (inputsize.height() > 0))
        {
            t_right  /= inputsize.width();
            t_left   /= inputsize.width();
            t_bottom /= inputsize.height();
            t_top    /= inputsize.height();
            trueheight /= inputsize.height();
        }

        // software bobdeint
        if ((softwareDeinterlacer == "bobdeint") &&
            softwareDeinterlacing &&
            (filter->outputBuffer == kDefaultBuffer))
        {
            float bob = (trueheight / (float)video_dim.height()) / 4.0f;
            if (scan == kScan_Intr2ndField)
            {
                t_top /= 2;
                t_bottom /= 2;
                t_bottom += bob;
                t_top    += bob;
            }
            if (scan == kScan_Interlaced)
            {
                t_top = (trueheight / 2) + (t_top / 2);
                t_bottom = (trueheight / 2) + (t_bottom / 2);
                t_bottom -= bob;
                t_top    -= bob;
            }
        }

        // vertex coordinates
        QRect display = (filter->frameBuffers.empty() ||  
                         filter->outputBuffer == kDefaultBuffer) ? 
                         display_video_rect : frameBufferRect; 
        QRect visible = (filter->frameBuffers.empty() ||
                         filter->outputBuffer == kDefaultBuffer) ?
                         display_visible_rect : frameBufferRect;

        float vleft  = display.left();
        float vright = display.right();
        float vtop   = display.top();
        float vbot   = display.bottom();

        // hardware bobdeint
        if (filter->outputBuffer == kDefaultBuffer &&
            hardwareDeinterlacing &&
            hardwareDeinterlacer == "openglbobdeint")
        {
            float bob = ((float)display.height() / (float)video_dim.height())
                        / 2.0f;
            if (scan == kScan_Interlaced)
            {
                vbot -= bob;
                vtop -= bob;
            }
            if (scan == kScan_Intr2ndField)
            {
                vbot += bob;
                vtop += bob;
            }
        }

        // resize for interactive tv
        if (videoResize && filter->outputBuffer == kDefaultBuffer)
            CalculateResize(vleft, vtop, vright, vbot);

        // invert horizontally if last filter 
        if (it == filters.begin())
        {
            // flip vertical positioning to translate from X coordinate system 
            // to opengl coordinate system 
            vtop = (visible.height()- 1) - display.top();
            vbot = vtop - (display.height() - 1);
        } 
        else if (it != filters.begin() && 
                (filter->frameBuffers.empty() ||
                filter->outputBuffer == kDefaultBuffer))
        {
            // this is the last filter and we have already inverted the video frame 
            // now need to adjust for vertical offsets 
            vbot = (visible.height()- 1) - display.top();
            vtop = vbot - (display.height() - 1);
        }

        // bind correct frame buffer (default onscreen) and set viewport
        switch (filter->outputBuffer)
        {
            case kDefaultBuffer:
                // clear the buffer
                if (viewportControl)
                {
                    if (gl_letterbox_colour == kLetterBoxColour_Gray25)
                        glClearColor(0.5f, 0.5f, 0.5f, 0.0f);
                    glClear(GL_COLOR_BUFFER_BIT);
                    gl_context->SetViewPort(display_visible_rect.size());
                }
                else
                {
                    gl_context->SetViewPort(masterViewportSize);
                }

                break;

            case kFrameBufferObject:
                if (!filter->frameBuffers.empty())
                {
                    gl_context->BindFramebuffer(filter->frameBuffers[0]);
                    gl_context->SetViewPort(frameBufferRect.size());
                }
                break;

            default:
                continue;
        }

        if (draw_border &&
            (filter->frameBuffers.empty() ||
            filter->outputBuffer == kDefaultBuffer))
        {
            gl_context->EnableFragmentProgram(0);
            gl_context->DisableTextures();
            glColor3f(0.5f, 0.0f, 0.0f); // deep red colour
            glBegin(GL_QUADS);
            glVertex2f(vleft  - 10, vtop + 10);
            glVertex2f(vright + 10, vtop + 10);
            glVertex2f(vright + 10, vbot - 10);
            glVertex2f(vleft  - 10, vbot - 10);
            glEnd();
            glColor3f(1.0f, 1.0f, 1.0f); // prevents tinting of texture
        }

        // bind correct textures
        uint active_tex = 0;
        for (; active_tex < inputs.size(); active_tex++)
        {
            gl_context->ActiveTexture(GL_TEXTURE0 + active_tex);
            glBindTexture(textureType, inputs[active_tex]);
        }

        if (!referenceTextures.empty() &&
            hardwareDeinterlacing &&
            type == kGLFilterYUV2RGB)
        {
            uint max = inputs.size() + referenceTextures.size();
            uint ref = 0;
            for (; active_tex < max; active_tex++, ref++)
            {
                gl_context->ActiveTexture(GL_TEXTURE0 + active_tex);
                glBindTexture(textureType, referenceTextures[ref]);
            }
        }

        if (helperTexture && type == kGLFilterBicubic)
        {
            gl_context->ActiveTexture(GL_TEXTURE0 + active_tex);
            glBindTexture(GL_TEXTURE_1D/*N.B.*/, helperTexture);
        }

        // enable fragment program and set any environment variables
        GLuint program = 0;
        if ((type != kGLFilterNone) && (type != kGLFilterResize))
        {
            GLuint prog_ref = 0;

            if (type == kGLFilterYUV2RGB)
            {
                if (hardwareDeinterlacing &&
                    filter->fragmentPrograms.size() == 3)
                {
                    if (scan == kScan_Interlaced)
                        prog_ref = 1;
                    else if (scan == kScan_Intr2ndField)
                        prog_ref = 2;
                }
            }
            program = filter->fragmentPrograms[prog_ref];
        }
 
        gl_context->EnableFragmentProgram(program);

        if (useColourControl &&
            (type == kGLFilterYUV2RGB || type == kGLFilterYUV2RGBA))
        {
            gl_context->SetColourParams();
        }

        // enable blending for osd
        if (type == kGLFilterResize && filters.count(kGLFilterYUV2RGBA))
            glEnable(GL_BLEND);

        // draw quad
        glBegin(GL_QUADS);
        glTexCoord2f(t_left, t_top);
        glVertex2f(vleft,  vtop);

        glTexCoord2f(t_right, t_top);
        glVertex2f(vright, vtop);

        glTexCoord2f(t_right, t_bottom);
        glVertex2f(vright, vbot);

        glTexCoord2f(t_left, t_bottom);
        glVertex2f(vleft,  vbot);
        glEnd();

        // disable blending
        if (type == kGLFilterResize && filters.count(kGLFilterYUV2RGBA))
            glDisable(GL_BLEND);

        // switch back to default framebuffer
        if (filter->outputBuffer != kDefaultBuffer)
            gl_context->BindFramebuffer(0);

        inputs = filter->frameBufferTextures;
        inputsize = realsize;
    }

    currentFrameNum = frame;
    inputUpdated = false;
}

void OpenGLVideo::RotateTextures(void)
{
   if (referenceTextures.size() < 2)
        return;

    GLuint tmp = referenceTextures[referenceTextures.size() - 1];

    for (uint i = referenceTextures.size() - 1; i > 0;  i--)
        referenceTextures[i] = referenceTextures[i - 1];

    referenceTextures[0] = inputTextures[0];
    inputTextures[0] = tmp;
}

void OpenGLVideo::DeleteTextures(vector<uint> *textures)
{
    if ((*textures).empty())
        return;

    for (uint i = 0; i < (*textures).size(); i++)
        gl_context->DeleteTexture((*textures)[i]);
    (*textures).clear();
}

// locking ok
void OpenGLVideo::SetTextureFilters(vector<GLuint> *textures,
                                    int filt, int wrap)
{
    if (textures->empty())
        return;

    for (uint i = 0; i < textures->size(); i++)
        gl_context->SetTextureFilters((*textures)[i], filt, wrap);
}

// locking ok
OpenGLFilterType OpenGLVideo::StringToFilter(const QString &filter)
{
    OpenGLFilterType ret = kGLFilterNone;

    if (filter.contains("master"))
        ret = kGLFilterYUV2RGB;
    else if (filter.contains("osd"))
        ret = kGLFilterYUV2RGBA;
    else if (filter.contains("resize"))
        ret = kGLFilterResize;
    else if (filter.contains("bicubic"))
        ret = kGLFilterBicubic;

    return ret;
}

// locking ok
QString OpenGLVideo::FilterToString(OpenGLFilterType filt)
{
    switch (filt)
    {
        case kGLFilterNone:
            break;
        case kGLFilterYUV2RGB:
            return "master";
        case kGLFilterYUV2RGBA:
            return "osd";
        case kGLFilterResize:
            return "resize";
        case kGLFilterBicubic:
            return "bicubic";
    }

    return "";
}

static const QString attrib_fast = 
"ATTRIB tex  = fragment.texcoord[0];\n";

static const QString var_alpha =
"TEMP alpha;\n";

static const QString tex_alpha = 
"TEX alpha, tex, texture[3], %1;\n";

static const QString tex_fast =
"TEX res, tex, texture[0], %1;\n";

static const QString param_colour =
"PARAM  adj  = program.env[0];\n";

static const QString calc_colour_fast =
"SUB res, res, 0.5;\n"
"MAD res, res, adj.zzzy, adj.wwwx;\n";

static const QString end_alpha =
"MOV result.color.a, alpha.a;\n";

static const QString var_fast =
"TEMP tmp, res;\n";

static const QString calc_fast_alpha = 
"MOV result.color.a, res.g;\n";

static const QString end_fast =
"SUB tmp, res.rbgg, { 0.5, 0.5 };\n"
"MAD res, res.a, 1.164, -0.063;\n"
"MAD res, { 0, -.392, 2.017 }, tmp.xxxw, res;\n"
"MAD result.color, { 1.596, -.813, 0, 0 }, tmp.yyyw, res;\n";

static const QString end_fast_alpha =
"SUB tmp, res.rbgg, { 0.5, 0.5 };\n"
"MAD res, res.a, 1.164, -0.063;\n"
"MAD res, { 0, -.392, 2.017 }, tmp.xxxw, res;\n"
"MAD result.color.rgb, { 1.596, -.813, 0, 0 }, tmp.yyyw, res;\n";

static const QString var_deint =
"TEMP other, current, mov, prev;\n";

static const QString field_calc =
"MUL prev, tex.yyyy, %2;\n"
"FRC prev, prev;\n"
"SUB prev, prev, 0.5;\n";

static const QString bobdeint[2] = {
field_calc +
"ADD other, tex, {0.0, %3, 0.0, 0.0};\n"
"TEX other, other, texture[0], %1;\n"
"CMP res, prev, res, other;\n",
field_calc +
"SUB other, tex, {0.0, %3, 0.0, 0.0};\n"
"TEX other, other, texture[0], %1;\n"
"CMP res, prev, other, res;\n"
};

static const QString deint_end_top =
"CMP other, mov, current, other;\n"
"CMP res, prev, current, other;\n";

static const QString deint_end_bot =
"CMP other, mov, current, other;\n"
"CMP res, prev, other, current;\n";

static const QString motion_calc =
"ABS mov, mov;\n"
"SUB mov, mov, 0.07;\n";

static const QString motion_top =
"SUB mov, prev, current;\n" + motion_calc;

static const QString motion_bot =
"SUB mov, res, current;\n" + motion_calc;

static const QString doublerateonefield[2] = {
"TEX current, tex, texture[1], %1;\n"
"TEX prev, tex, texture[2], %1;\n"
"ADD other, tex, {0.0, %3, 0.0, 0.0};\n"
"TEX other, other, texture[1], %1;\n"
+ motion_top + field_calc + deint_end_top,

"TEX current, tex, texture[1], %1;\n"
"SUB other, tex, {0.0, %3, 0.0, 0.0};\n"
"TEX other, other, texture[1], %1;\n"
+ motion_bot + field_calc + deint_end_bot
};

static const QString linearblend[2] = {
"TEX current, tex, texture[1], %1;\n"
"TEX prev, tex, texture[2], %1;\n"
"ADD other, tex, {0.0, %3, 0.0, 0.0};\n"
"TEX other, other, texture[1], %1;\n"
"SUB mov, tex, {0.0, %3, 0.0, 0.0};\n"
"TEX mov, mov, texture[1], %1;\n"
"LRP other, 0.5, other, mov;\n"
+ motion_top + field_calc + deint_end_top,

"TEX current, tex, texture[1], %1;\n"
"SUB other, tex, {0.0, %3, 0.0, 0.0};\n"
"TEX other, other, texture[1], %1;\n"
"ADD mov, tex, {0.0, %3, 0.0, 0.0};\n"
"TEX mov, mov, texture[1], %1;\n"
"LRP other, 0.5, other, mov;\n"
+ motion_bot + field_calc + deint_end_bot
};

static const QString kerneldeint[2] = {
"TEX current, tex, texture[1], %1;\n"
"TEX prev, tex, texture[2], %1;\n"
+ motion_top +
"MUL other, 0.125, prev;\n"
"MAD other, 0.125, current, other;\n"
"ADD prev, tex, {0.0, %3, 0.0, 0.0};\n"
"TEX prev, prev, texture[1], %1;\n"
"MAD other, 0.5, prev, other;\n"
"SUB prev, tex, {0.0, %3, 0.0, 0.0};\n"
"TEX prev, prev, texture[1], %1;\n"
"MAD other, 0.5, prev, other;\n"
"ADD prev, tex, {0.0, %4, 0.0, 0.0};\n"
"TEX mov, prev, texture[1], %1;\n"
"MAD other, -0.0625, mov, other;\n"
"TEX mov, prev, texture[2], %1;\n"
"MAD other, -0.0625, mov, other;\n"
"SUB prev, tex, {0.0, %4, 0.0, 0.0};\n"
"TEX mov, prev, texture[1], %1;\n"
"MAD other, -0.0625, mov, other;\n"
"TEX mov, prev, texture[2], %1;\n"
"MAD other, -0.0625, mov, other;\n"
+ field_calc + deint_end_top,

"TEX current, tex, texture[1], %1;\n"
+ motion_bot +
"MUL other, 0.125, res;\n"
"MAD other, 0.125, current, other;\n"
"ADD prev, tex, {0.0, %3, 0.0, 0.0};\n"
"TEX prev, prev, texture[1], %1;\n"
"MAD other, 0.5, prev, other;\n"
"SUB prev, tex, {0.0, %3, 0.0, 0.0};\n"
"TEX prev, prev, texture[1], %1;\n"
"MAD other, 0.5, prev, other;\n"
"ADD prev, tex, {0.0, %4, 0.0, 0.0};\n"
"TEX mov, prev, texture[1], %1;\n"
"MAD other, -0.0625, mov, other;\n"
"TEX mov, prev, texture[0], %1;\n"
"MAD other, -0.0625, mov, other;\n"
"SUB prev, tex, {0.0, %4, 0.0, 0.0};\n"
"TEX mov, prev, texture[1], %1;\n"
"MAD other, -0.0625, mov, other;\n"
"TEX mov, prev, texture[0], %1;\n"
"MAD other, -0.0625, mov, other;\n"
+ field_calc + deint_end_bot
};

static const QString yadif_setup =
"TEMP a,b,c,e,f,g,h,j,k,l;\n"
"TEMP a1,b1,f1,g1,h1,i1,j1,l1,m1,n1;\n"
"ALIAS d1 = f;\n"
"ALIAS k1 = g;\n"
"ALIAS c1 = prev;\n"
"ALIAS e1 = mov;\n"
"ALIAS p0 = res;\n"
"ALIAS p1 = c;\n"
"ALIAS p3 = h;\n"
"ALIAS spred1 = a;\n"
"ALIAS spred2 = b;\n"
"ALIAS spred3 = c;\n"
"ALIAS spred4 = e;\n"
"ALIAS spred5 = f;\n"
"ALIAS sscore = g;\n"
"ALIAS score1 = h;\n"
"ALIAS score2 = j;\n"
"ALIAS score3 = k;\n"
"ALIAS score4 = l;\n"
"ALIAS if1 = a1;\n"
"ALIAS if2 = b1;\n"
"TEMP p2, p4;\n"
"ALIAS diff1 = a;\n"
"ALIAS diff2 = b;\n"
"TEMP diff0;\n";

static const QString yadif_spatial_sample =
"ADD tmp, tex, {%5, %3, 0.0, 0.0};\n"
"TEX e1, tmp, texture[1], %1;\n"
"ADD tmp, tmp, {%5, 0.0, 0.0, 0.0};\n"
"TEX f1, tmp, texture[1], %1;\n"
"ADD tmp, tmp, {%5, 0.0, 0.0, 0.0};\n"
"TEX g1, tmp, texture[1], %1;\n"
"SUB tmp, tmp, {0.0, %4, 0.0, 0.0};\n"
"TEX n1, tmp, texture[1], %1;\n"
"SUB tmp, tmp, {%5, 0.0, 0.0, 0.0};\n"
"TEX m1, tmp, texture[1], %1;\n"
"SUB tmp, tmp, {%5, 0.0, 0.0, 0.0};\n"
"TEX l1, tmp, texture[1], %1;\n"

"SUB tmp, tex, {%5, %3, 0.0, 0.0};\n"
"TEX j1, tmp, texture[1], %1;\n"
"SUB tmp, tmp, {%5, 0.0, 0.0, 0.0};\n"
"TEX i1, tmp, texture[1], %1;\n"
"SUB tmp, tmp, {%5, 0.0, 0.0, 0.0};\n"
"TEX h1, tmp, texture[1], %1;\n"
"ADD tmp, tmp, {0.0, %4, 0.0, 0.0};\n"
"TEX a1, tmp, texture[1], %1;\n"
"ADD tmp, tmp, {%5, 0.0, 0.0, 0.0};\n"
"TEX b1, tmp, texture[1], %1;\n"
"ADD tmp, tmp, {%5, 0.0, 0.0, 0.0};\n"
"TEX c1, tmp, texture[1], %1;\n";

static const QString yadif_calc =
"LRP p0, 0.5, c, h;\n"
"MOV p1, f;\n"
"LRP p2, 0.5, d, i;\n"
"MOV p3, g;\n"
"LRP p4, 0.5, e, j;\n"

"SUB diff0, d, i;\n"
"ABS diff0, diff0;\n"
"SUB tmp, a, f;\n"
"ABS tmp, tmp;\n"
"SUB diff1, b, g;\n"
"ABS diff1, diff1;\n"
"LRP diff1, 0.5, diff1, tmp;\n"
"SUB tmp, k, f;\n"
"ABS tmp, tmp;\n"
"SUB diff2, g, l;\n"
"ABS diff2, diff2;\n"
"LRP diff2, 0.5, diff2, tmp;\n"
"MAX diff0, diff0, diff1;\n"
"MAX diff0, diff0, diff2;\n"

// mode < 2
"SUB tmp, p0, p1;\n"
"SUB other, p4, p3;\n"
"MIN spred1, tmp, other;\n"
"MAX spred2, tmp, other;\n"
"SUB tmp, p2, p1;\n"
"SUB other, p2, p3;\n"
"MAX spred1, spred1, tmp;\n"
"MAX spred1, spred1, other;\n"
"MIN spred2, spred2, tmp;\n"
"MIN spred2, spred2, other;\n"
"MAX spred1, spred2, -spred1;\n"
"MAX diff0, diff0, spred1;\n"

// spatial prediction
"LRP spred1, 0.5, d1, k1;\n"
"LRP spred2, 0.5, c1, l1;\n"
"LRP spred3, 0.5, b1, m1;\n"
"LRP spred4, 0.5, e1, j1;\n"
"LRP spred5, 0.5, f1, i1;\n"

"SUB sscore, c1, j1;\n"
"ABS sscore, sscore;\n"
"SUB tmp, d1, k1;\n"
"ABS tmp, tmp;\n"
"ADD sscore, sscore, tmp;\n"
"SUB tmp, e1, l1;\n"
"ABS tmp, tmp;\n"
"ADD sscore, sscore, tmp;\n"
"SUB sscore, sscore, 1.0;\n"

"SUB score1, b1, k1;\n"
"ABS score1, score1;\n"
"SUB tmp, c1, l1;\n"
"ABS tmp, tmp;\n"
"ADD score1, score1, tmp;\n"
"SUB tmp, d1, m1;\n"
"ABS tmp, tmp;\n"
"ADD score1, score1, tmp;\n"

"SUB score2, a1, l1;\n"
"ABS score2, score2;\n"
"SUB tmp, b1, m1;\n"
"ABS tmp, tmp;\n"
"ADD score2, score2, tmp;\n"
"SUB tmp, c1, n1;\n"
"ABS tmp, tmp;\n"
"ADD score2, score2, tmp;\n"

"SUB score3, d1, i1;\n"
"ABS score3, score3;\n"
"SUB tmp, e1, j1;\n"
"ABS tmp, tmp;\n"
"ADD score3, score3, tmp;\n"
"SUB tmp, f1, k1;\n"
"ABS tmp, tmp;\n"
"ADD score3, score3, tmp;\n"

"SUB score4, e1, h1;\n"
"ABS score4, score4;\n"
"SUB tmp, f1, i1;\n"
"ABS tmp, tmp;\n"
"ADD score4, score4, tmp;\n"
"SUB tmp, g1, j1;\n"
"ABS tmp, tmp;\n"
"ADD score4, score4, tmp;\n"
"SUB if1, sscore, score1;\n"
"SUB if2, score1, score2;\n"
"CMP if2, if1, -1.0, if2;\n"
"CMP spred1, if1, spred1, spred2;\n"
"CMP spred1, if2, spred1, spred3;\n"
"CMP sscore, if1, sscore, score1;\n"
"CMP sscore, if2, sscore, score2;\n"
"SUB if1, sscore, score3;\n"
"SUB if2, score3, score4;\n"
"CMP if2, if1, -1.0, if2;\n"
"CMP spred1, if1, spred1, spred4;\n"
"CMP spred1, if2, spred1, spred5;\n"
"ADD spred4, p2, diff0;\n"
"SUB spred5, p2, diff0;\n"
"SUB if1, spred4, spred1;\n"
"SUB if2, spred1, spred5;\n"
"CMP spred1, if1, spred4, spred1;\n"
"CMP spred1, if2, spred5, spred1;\n";

static const QString yadif[2] = {
yadif_setup +
"TEMP d;\n"
"ALIAS i = current;\n"
"TEX current, tex, texture[1], %1;\n"
"TEX d, tex, texture[2], %1;\n"
"ADD tmp, tex, {0.0, %3, 0.0, 0.0};\n"
"TEX a, tmp, texture[2], %1;\n"
"TEX f, tmp, texture[1], %1;\n"
"TEX k, tmp, texture[0], %1;\n"
"ADD tmp, tex, {0.0, %4, 0.0, 0.0};\n"
"TEX c, tmp, texture[2], %1;\n"
"TEX h, tmp, texture[1], %1;\n"
"SUB tmp, tex, {0.0, %3, 0.0, 0.0};\n"
"TEX b, tmp, texture[2], %1;\n"
"TEX g, tmp, texture[1], %1;\n"
"TEX l, tmp, texture[0], %1;\n"
"SUB tmp, tex, {0.0, %4, 0.0, 0.0};\n"
"TEX e, tmp, texture[2], %1;\n"
"TEX j, tmp, texture[1], %1;\n"
+ yadif_spatial_sample
+ yadif_calc
+ field_calc +
"CMP res, prev, current, spred1;\n"
,
yadif_setup +
"TEMP i;\n"
"ALIAS d = current;\n"
"TEX current, tex, texture[1], %1;\n"
"TEX i, tex, texture[0], %1;\n"
"ADD tmp, tex, {0.0, %3, 0.0, 0.0};\n"
"TEX a, tmp, texture[2], %1;\n"
"TEX f, tmp, texture[1], %1;\n"
"TEX k, tmp, texture[0], %1;\n"
"ADD tmp, tex, {0.0, %4, 0.0, 0.0};\n"
"TEX c, tmp, texture[1], %1;\n"
"TEX h, tmp, texture[0], %1;\n"
"SUB tmp, tex, {0.0, %3, 0.0, 0.0};\n"
"TEX b, tmp, texture[2], %1;\n"
"TEX g, tmp, texture[1], %1;\n"
"TEX l, tmp, texture[0], %1;\n"
"SUB tmp, tex, {0.0, %4, 0.0, 0.0};\n"
"TEX e, tmp, texture[1], %1;\n"
"TEX j, tmp, texture[0], %1;\n"
+ yadif_spatial_sample
+ yadif_calc
+ field_calc +
"CMP res, prev, spred1, current;\n"
};

static const QString bicubic =
"TEMP coord, coord2, cdelta, parmx, parmy, a, b, c, d;\n"
"MAD coord.xy, fragment.texcoord[0], {%6, %7}, {0.5, 0.5};\n"
"TEX parmx, coord.x, texture[1], 1D;\n"
"TEX parmy, coord.y, texture[1], 1D;\n"
"MUL cdelta.xz, parmx.rrgg, {-%5, 0, %5, 0};\n"
"MUL cdelta.yw, parmy.rrgg, {0, -%3, 0, %3};\n"
"ADD coord, fragment.texcoord[0].xyxy, cdelta.xyxw;\n"
"ADD coord2, fragment.texcoord[0].xyxy, cdelta.zyzw;\n"
"TEX a, coord.xyxy, texture[0], 2D;\n"
"TEX b, coord.zwzw, texture[0], 2D;\n"
"TEX c, coord2.xyxy, texture[0], 2D;\n"
"TEX d, coord2.zwzw, texture[0], 2D;\n"
"LRP a, parmy.b, a, b;\n"
"LRP c, parmy.b, c, d;\n"
"LRP result.color, parmx.b, a, c;\n";

QString OpenGLVideo::GetProgramString(OpenGLFilterType name,
                                      QString deint, FrameScanType field)
{
    QString ret =
        "!!ARBfp1.0\n"
        "OPTION ARB_precision_hint_fastest;\n";

    switch (name)
    {
        case kGLFilterYUV2RGB:
        {
            bool need_tex = true;
            QString deint_bit = "";
            if (deint != "")
            {
                uint tmp_field = 0;
                if (field == kScan_Intr2ndField)
                    tmp_field = 1;
                if (deint == "openglbobdeint" ||
                    deint == "openglonefield" ||
                    deint == "opengldoubleratefieldorder")
                {
                    deint_bit = bobdeint[tmp_field];
                }
                else if (deint == "opengldoublerateonefield")
                {
                    deint_bit = doublerateonefield[tmp_field];
                    if (!tmp_field) { need_tex = false; }
                }
                else if (deint == "opengllinearblend" ||
                         deint == "opengldoubleratelinearblend")
                {
                    deint_bit = linearblend[tmp_field];
                    if (!tmp_field) { need_tex = false; }
                }
                else if (deint == "openglkerneldeint" ||
                         deint == "opengldoubleratekerneldeint")
                {
                    deint_bit = kerneldeint[tmp_field];
                    if (!tmp_field) { need_tex = false; }
                }
                else if (deint == "openglyadif" ||
                         deint == "opengldoublerateyadif")
                {
                    deint_bit = yadif[tmp_field];
                    need_tex = false;
                }
                else
                {
                    VERBOSE(VB_PLAYBACK, LOC +
                        "Unrecognised OpenGL deinterlacer");
                }
            }

            ret += attrib_fast;
            ret += useColourControl ? param_colour : "";
            ret += (deint != "") ? var_deint : "";
            ret += var_fast + (need_tex ? tex_fast : "");
            ret += deint_bit;
            ret += useColourControl ? calc_colour_fast : "";
            ret += end_fast;
        }
            break;
        case kGLFilterYUV2RGBA:

            ret += attrib_fast;
            ret += useColourControl ? param_colour : "";
            ret += var_fast + tex_fast + calc_fast_alpha;
            ret += useColourControl ? calc_colour_fast : "";
            ret += end_fast_alpha;

            break;

        case kGLFilterNone:
        case kGLFilterResize:
            break;

        case kGLFilterBicubic:
 
            ret += bicubic;
            break;

        default:
            VERBOSE(VB_PLAYBACK, LOC_ERR + "Unknown fragment program.");
            break;
    }

    QString temp = textureRects ? "RECT" : "2D";
    ret.replace("%1", temp);

    float lineHeight = 1.0f;
    float colWidth   = 1.0f;
    QSize fb_size = GetTextureSize(video_dim);

    if (!textureRects &&
       (inputTextureSize.height() > 0))
    {
        lineHeight /= inputTextureSize.height();
        colWidth   /= inputTextureSize.width();
    }

    float fieldSize = 1.0f / (lineHeight * 2.0);

    ret.replace("%2", temp.setNum(fieldSize, 'f', 8));
    ret.replace("%3", temp.setNum(lineHeight, 'f', 8));
    ret.replace("%4", temp.setNum(lineHeight * 2.0, 'f', 8));
    ret.replace("%5", temp.setNum(colWidth, 'f', 8));
    ret.replace("%6", temp.setNum((float)fb_size.width(), 'f', 1));
    ret.replace("%7", temp.setNum((float)fb_size.height(), 'f', 1));

    ret += "END";

    VERBOSE(VB_PLAYBACK, LOC + QString("Created %1 fragment program %2")
                .arg(FilterToString(name)).arg(deint));

    return ret;
}

uint OpenGLVideo::ParseOptions(QString options)
{
    uint ret = kGLMaxFeat - 1;

    QStringList list = options.split(",");

    if (list.empty())
        return ret;

    for (QStringList::Iterator i = list.begin();
         i != list.end(); ++i)
    {
        QString name = (*i).section('=', 0, 0);
        QString opts = (*i).section('=', 1);

        if (name == "opengloptions")
        {
            if (opts.contains("nofinish"))
                ret -= kGLFinish;
            if (opts.contains("nofence"))
            {
                ret -= kGLAppleFence;
                ret -= kGLNVFence;
            }
            if (opts.contains("noswap"))
            {
                ret -= kGLGLXSwap;
                ret -= kGLWGLSwap;
                ret -= kGLAGLSwap;
            }
            if (opts.contains("nopbo"))
                ret -= kGLExtPBufObj;
            if (opts.contains("nopbuf"))
                ret -= kGLXPBuffer;
            if (opts.contains("nofbo"))
                ret -= kGLExtFBufObj;
            if (opts.contains("nofrag"))
                ret -= kGLExtFragProg;
            if (opts.contains("norect"))
                ret -= kGLExtRect;
            return ret;
        }
    }

    return ret;
}

