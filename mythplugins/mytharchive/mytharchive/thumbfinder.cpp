// c
#include <math.h>
#include <cstdlib>

// c++
#include <iostream>

// qt
#include <QApplication>
#include <QDomDocument>
#include <QFile>
#include <QDir>
//#include <QFileInfo>
#include <QPainter>

// myth
#include <mythtv/mythcontext.h>
#include <mythtv/mythdbcon.h>
#include <mythtv/libmythtv/programinfo.h>
#include <mythtv/libmythui/mythuihelper.h>
#include <libmythui/mythmainwindow.h>
#include <libmythui/mythdialogbox.h>
#include <mythtv/mythdirs.h>
#include <mythtv/util.h>
#include <libmythui/mythuitext.h>
#include <libmythui/mythuibutton.h>
#include <libmythui/mythuiimage.h>
#include <libmythui/mythuibuttonlist.h>

#ifndef INT64_C    // Used in ffmpeg headers to define some constants
#define INT64_C(v)   (v ## LL)
#endif

// mytharchive
#include "thumbfinder.h"

// the amount to seek before the required frame
#define PRE_SEEK_AMOUNT 50 

struct SeekAmount SeekAmounts[] =
{
    {"frame",       -1},
    {"1 second",     1},
    {"5 seconds",    5},
    {"10 seconds",  10},
    {"30 seconds",  30},
    {"1 minute",    60},
    {"5 minutes",  300},
    {"10 minutes", 600},
    {"Cut Point",   -2},
};

int SeekAmountsCount = sizeof(SeekAmounts) / sizeof(SeekAmounts[0]);

ThumbFinder::ThumbFinder(MythScreenStack *parent, ArchiveItem *archiveItem,
                         const QString &menuTheme)
            :MythScreenType(parent, "ThumbFinder")
{
    m_archiveItem = archiveItem;

    m_thumbDir = createThumbDir();

    // copy thumbList so we can abandon changes if required
    m_thumbList.clear();
    for (int x = 0; x < m_archiveItem->thumbList.size(); x++)
    {
        ThumbImage *thumb = new ThumbImage;
        *thumb = *m_archiveItem->thumbList.at(x);
        m_thumbList.append(thumb);
    }

    m_thumbCount = getChapterCount(menuTheme);

    m_currentSeek = 0;
    m_offset = 0;
    m_startTime = -1;
    m_startPTS = -1;
    m_currentPTS = -1;
    m_firstIFramePTS = -1;
}

void ThumbFinder::Init(void)
{
    getThumbImages();
}

ThumbFinder::~ThumbFinder()
{
    while (!m_thumbList.isEmpty())
         delete m_thumbList.takeFirst();
    m_thumbList.clear();

    closeAVCodec();
}

bool ThumbFinder::Create(void)
{
    bool foundtheme = false;

    // Load the theme for this screen
    foundtheme = LoadWindowFromXML("mythburn-ui.xml", "thumbfinder", this);

    if (!foundtheme)
        return false;

    bool err = false;
    UIUtilE::Assign(this, m_frameImage, "frameimage", &err);
    UIUtilE::Assign(this, m_positionImage, "positionimage", &err);
    UIUtilE::Assign(this, m_imageGrid, "thumblist", &err);
    UIUtilE::Assign(this, m_saveButton, "save_button", &err);
    UIUtilE::Assign(this, m_cancelButton, "cancel_button", &err);
    UIUtilE::Assign(this, m_frameButton, "frame_button", &err);
    UIUtilE::Assign(this, m_seekAmountText, "seekamount", &err);
    UIUtilE::Assign(this, m_currentPosText, "currentpos", &err);

    if (err)
    {
        VERBOSE(VB_IMPORTANT, "Cannot load screen 'mythburn'");
        return false;
    }

    connect(m_imageGrid, SIGNAL(itemSelected(MythUIButtonListItem *)),
            this, SLOT(gridItemChanged(MythUIButtonListItem *)));

    m_saveButton->SetText(tr("Save"));
    connect(m_saveButton, SIGNAL(Clicked()), this, SLOT(savePressed()));

    m_cancelButton->SetText(tr("Cancel"));
    connect(m_cancelButton, SIGNAL(Clicked()), this, SLOT(cancelPressed()));

    connect(m_frameButton, SIGNAL(Clicked()), this, SLOT(updateThumb()));

    if (!BuildFocusList())
        VERBOSE(VB_IMPORTANT, "Failed to build a focuslist. Something is wrong");

    SetFocusWidget(m_imageGrid);

    return true;
}

bool ThumbFinder::keyPressEvent(QKeyEvent *event)
{
    if (GetFocusWidget()->keyPressEvent(event))
        return true;

    bool handled = false;
    QStringList actions;
    gContext->GetMainWindow()->TranslateKeyPress("Archive", event, actions);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        handled = true;

        if (action == "MENU")
        {
            NextPrevWidgetFocus(true);
            return true;
        }

        if (action == "ESCAPE")
        {
            showMenu();
            return true;
        }

        if (action == "0" || action == "1" || action == "2" || action == "3" ||
            action == "4" || action == "5" || action == "6" || action == "7" ||
            action == "8" || action == "9")
        {
            m_imageGrid->SetItemCurrent(action.toInt());
            int itemNo = m_imageGrid->GetCurrentPos();
            ThumbImage *thumb = m_thumbList.at(itemNo);
            if (thumb)
                seekToFrame(thumb->frame);
            return true;
        }

        if (GetFocusWidget() == m_frameButton)
        {
            if (action == "UP")
            {
                changeSeekAmount(true);
            }
            else if (action == "DOWN")
            {
                changeSeekAmount(false);
            }
            else if (action == "LEFT")
            {
                seekBackward();
            }
            else if (action == "RIGHT")
            {
                seekForward();
            }
            else if (action == "SELECT")
            {
                updateThumb();
            }
            else
                handled = false;
        }
        else
            handled = false;
    }

    if (!handled && MythScreenType::keyPressEvent(event))
        handled = true;

    return handled;
}

int  ThumbFinder::getChapterCount(const QString &menuTheme)
{
    QString filename = GetShareDir() + "mytharchive/themes/" + 
            menuTheme + "/theme.xml";
    QDomDocument doc("mydocument");
    QFile file(filename);
    cout << "loading file from: " << qPrintable(filename) << endl;
    if (!file.open(QIODevice::ReadOnly))
    {
        VERBOSE(VB_IMPORTANT, "Failed to open theme file: " + filename);
        return 0; //??
    }
    if (!doc.setContent(&file))
    {
        file.close();
        VERBOSE(VB_IMPORTANT, "Failed to parse theme file: " + filename);
        return 0;
    }
    file.close();

    QDomNodeList chapterNodeList = doc.elementsByTagName("chapter");
    cout << "chapterNodeList.count(): " << chapterNodeList.count() << endl;
    cout << "chapterNodeList.size(): " << chapterNodeList.size() << endl;
    return chapterNodeList.count();
}

void ThumbFinder::loadCutList()
{
    ProgramInfo *progInfo = getProgramInfoForFile(m_archiveItem->filename);

    if (progInfo && m_archiveItem->hasCutlist)
    {
        progInfo->GetCutList(m_deleteMap);
        delete progInfo;
    }
}

void ThumbFinder::savePressed()
{
    // copy the thumb details to the archiveItem
    while (!m_archiveItem->thumbList.isEmpty())
         delete m_archiveItem->thumbList.takeFirst();
    m_archiveItem->thumbList.clear();

    for (int x = 0; x < m_thumbList.size(); x++)
    {
        ThumbImage *thumb = new ThumbImage;
        *thumb = *m_thumbList.at(x);
        m_archiveItem->thumbList.append(thumb);
    }

    Close();
}

void ThumbFinder::cancelPressed()
{
    Close();
}

void ThumbFinder::updateCurrentPos()
{
    int64_t pos = m_currentPTS - m_firstIFramePTS;
    int64_t frame = pos / m_frameTime;

    if (m_currentPosText)
        m_currentPosText->SetText(frameToTime(frame, true));

    updatePositionBar(frame);
}

void ThumbFinder::changeSeekAmount(bool up)
{
    if (up)
    {
        m_currentSeek++;
        if (m_currentSeek >= SeekAmountsCount)
            m_currentSeek = 0;
    }
    else
    {
        m_currentSeek--;
        if (m_currentSeek < 0)
            m_currentSeek = SeekAmountsCount - 1;
    }

    m_seekAmountText->SetText(SeekAmounts[m_currentSeek].name);
}

void ThumbFinder::gridItemChanged(MythUIButtonListItem *item)
{
    (void) item;

    int itemNo = m_imageGrid->GetCurrentPos();
    ThumbImage *thumb = m_thumbList.at(itemNo);
    if (thumb)
          seekToFrame(thumb->frame);
}

QString ThumbFinder::createThumbDir(void)
{
    QString thumbDir = getTempDirectory() + "config/thumbs";

    // make sure the thumb directory exists
    QDir dir(thumbDir);
    if (!dir.exists())
    {
        dir.mkdir(thumbDir);
        system(qPrintable("chmod 777 " + thumbDir));
    }

    int x = 0;
    QString res;
    do
    {
        x++;
        res = QString(thumbDir + "/%1").arg(x);
        dir.setPath(res);
    } while (dir.exists());

    dir.mkdir(res);
    system(qPrintable("chmod 777 " + res));

    return  res;
}

void ThumbFinder::updateThumb(void)
{
    int itemNo = m_imageGrid->GetCurrentPos();
    MythUIButtonListItem *item = m_imageGrid->GetItemCurrent();

    ThumbImage *thumb = m_thumbList.at(itemNo);
    if (!thumb)
        return;

    // copy current frame image to the selected thumb image
    QString imageFile = thumb->filename;
    QFile dst(imageFile);
    QFile src(m_frameFile);
    copy(dst, src);

    item->SetImage(imageFile);

    // update the image grid item
    int64_t pos = (int) ((m_currentPTS - m_startPTS) / m_frameTime);
    thumb->frame = pos - m_offset;
    if (itemNo != 0)
    {
        thumb->caption = frameToTime(thumb->frame);
        item->setText(thumb->caption);
    }

    m_imageGrid->SetRedraw();
}

QString ThumbFinder::frameToTime(int64_t frame, bool addFrame)
{
    int hour, min, sec;
    QString str;

    sec = (int) (frame / m_fps);
    frame = frame - (int) (sec * m_fps);
    min = sec / 60;
    sec %= 60;
    hour = min / 60;
    min %= 60;

    if (addFrame)
        str = str.sprintf("%01d:%02d:%02d.%02d", hour, min, sec, (int) frame);
    else
        str = str.sprintf("%02d:%02d:%02d", hour, min, sec);
    return str;
}

bool ThumbFinder::getThumbImages()
{
    if (!getFileDetails(m_archiveItem))
    {
        VERBOSE(VB_IMPORTANT, QString("ThumbFinder:: Failed to get file details for %1") 
                              .arg(m_archiveItem->filename));
        return false;
    }

    if (m_archiveItem->type == "Recording")
        loadCutList();

    if (!initAVCodec(m_archiveItem->filename))
        return false;

    // calculate the file duration taking the cut list into account
    m_finalDuration = calcFinalDuration();

    QString origFrameFile = m_frameFile;

    m_updateFrame = true;
    getFrameImage();

    int chapterLen;
    if (m_thumbCount)
        chapterLen = m_finalDuration / m_thumbCount;
    else
        chapterLen = m_finalDuration;

    QString thumbList = "";
    m_updateFrame = false;

    // add title thumb
    m_frameFile = m_thumbDir + "/title.jpg";
    ThumbImage *thumb = NULL;

    if (m_thumbList.size() > 0)
    {
        // use the thumb details in the thumbList if already available
        thumb = m_thumbList.at(0);
    }

    if (!thumb)
    {
        // no thumb available create a new one
        thumb = new ThumbImage;
        thumb->filename = m_frameFile;
        thumb->frame = (int64_t) 0;
        thumb->caption = "Title";
        m_thumbList.append(thumb);
    }
    else
        m_frameFile = thumb->filename;

    seekToFrame(thumb->frame);
    getFrameImage();

    new MythUIButtonListItem(m_imageGrid, thumb->caption, thumb->filename);

    qApp->processEvents();

    for (int x = 1; x <= m_thumbCount; x++)
    {
        m_frameFile = QString(m_thumbDir + "/chapter-%1.jpg").arg(x);

        thumb = NULL;

        if (m_archiveItem->thumbList.size() > x)
        {
            // use the thumb details in the archiveItem if already available
            thumb = m_archiveItem->thumbList.at(x);
        }

        if (!thumb)
        {
            QString time;
            int chapter, hour, min, sec;

            chapter = chapterLen * (x - 1);
            hour = chapter / 3600;
            min = (chapter % 3600) / 60;
            sec = chapter % 60;
            time = time.sprintf("%02d:%02d:%02d", hour, min, sec);

            int64_t frame = (int64_t) (chapter * ceil(m_fps));

            // no thumb available create a new one
            thumb = new ThumbImage;
            thumb->filename = m_frameFile;
            thumb->frame = frame;
            thumb->caption = time;
            m_thumbList.append(thumb);
        }
        else
            m_frameFile = thumb->filename;

        seekToFrame(thumb->frame);
        qApp->processEvents();
        getFrameImage();
        qApp->processEvents();
        new MythUIButtonListItem(m_imageGrid, thumb->caption, thumb->filename);
        qApp->processEvents();
    }

    m_frameFile = origFrameFile;
    seekToFrame(0);

    m_updateFrame = true;

    m_imageGrid->SetRedraw();

    SetFocusWidget(m_imageGrid);

    return true;
}

bool ThumbFinder::initAVCodec(const QString &inFile)
{
    av_register_all();

    m_inputFC = NULL;

    // Open recording
    VERBOSE(VB_JOBQUEUE, QString("ThumbFinder: ") +
            QString("Opening '%1'").arg(inFile));

    QByteArray inFileBA = inFile.toLocal8Bit();

    int ret = av_open_input_file(
        &m_inputFC, inFileBA.constData(), NULL, 0, NULL);

    if (ret)
    {
        VERBOSE(VB_IMPORTANT, QString("ThumbFinder, Error: ") +
                "Couldn't open input file" + ENO);
        return false;
    }

    // Getting stream information
    if ((ret = av_find_stream_info(m_inputFC)) < 0)
    {
        VERBOSE(VB_IMPORTANT,
                QString("Couldn't get stream info, error #%1").arg(ret));
        av_close_input_file(m_inputFC);
        m_inputFC = NULL;
        return false;
    }
    av_estimate_timings(m_inputFC, 0);
    dump_format(m_inputFC, 0, inFileBA.constData(), 0);

    // find the first video stream
    m_videostream = -1;

    for (uint i = 0; i < m_inputFC->nb_streams; i++)
    {
        AVStream *st = m_inputFC->streams[i];
        if (m_inputFC->streams[i]->codec->codec_type == CODEC_TYPE_VIDEO)
        {
            m_startTime = -1; 
            if (m_inputFC->streams[i]->start_time != (int) AV_NOPTS_VALUE)
                m_startTime = m_inputFC->streams[i]->start_time;
            else
            {
                VERBOSE(VB_IMPORTANT, "ThumbFinder: Failed to get start time");
                return false;
            }

            m_videostream = i;
            m_frameWidth = st->codec->width;
            m_frameHeight = st->codec->height;
            if (st->r_frame_rate.den && st->r_frame_rate.num)
                m_fps = av_q2d(st->r_frame_rate);
            else
                m_fps = 1/av_q2d(st->time_base);
            break;
        }
    }

    if (m_videostream == -1)
    {
        VERBOSE(VB_IMPORTANT, "Couldn't find a video stream");
        return false;
    }

    // get the codec context for the video stream
    m_codecCtx = m_inputFC->streams[m_videostream]->codec;
    m_codecCtx->debug_mv = 0;
    m_codecCtx->debug = 0;
    m_codecCtx->workaround_bugs = 1;
    m_codecCtx->lowres = 0;
    m_codecCtx->idct_algo = FF_IDCT_AUTO;
    m_codecCtx->skip_frame = AVDISCARD_DEFAULT;
    m_codecCtx->skip_idct = AVDISCARD_DEFAULT;
    m_codecCtx->skip_loop_filter = AVDISCARD_DEFAULT;
    m_codecCtx->error_resilience = FF_ER_CAREFUL;
    m_codecCtx->error_concealment = 3;

    // get decoder for video stream
    m_codec = avcodec_find_decoder(m_codecCtx->codec_id);

    if (m_codec == NULL)
    {
        VERBOSE(VB_IMPORTANT, "ThumbFinder: Couldn't find codec for video stream");
        return false;
    }

    // open codec
    if (avcodec_open(m_codecCtx, m_codec) < 0)
    {
        VERBOSE(VB_IMPORTANT, "ThumbFinder: Couldn't open codec for video stream");
        return false;
    }

    // allocate temp buffer
    int bufflen = m_frameWidth * m_frameHeight * 4;
    m_outputbuf = new unsigned char[bufflen];

    m_frame = avcodec_alloc_frame();

    m_frameFile = getTempDirectory() + "work/frame.jpg";
    //cout << "m_frameFile initAV: " << m_frameFile << endl;
    return true;
}

int ThumbFinder::checkFramePosition(int frameNumber)
{
    if (m_deleteMap.isEmpty() || !m_archiveItem->useCutlist)
        return frameNumber;

    int diff = 0;
    QMap<long long, int>::Iterator it = m_deleteMap.find(frameNumber);

    for (it = m_deleteMap.begin(); it != m_deleteMap.end(); ++it)
    {
        int start = it.key();
        ++it;
        int end = it.key();

        if (start <= frameNumber + diff)
            diff += end - start;
    }

    m_offset = diff;
    return frameNumber + diff;
}

bool ThumbFinder::seekToFrame(int frame, bool checkPos)
{
    // make sure the frame is not in a cut point
    if (checkPos)
        frame = checkFramePosition(frame);

    // seek to a position PRE_SEEK_AMOUNT frames before the required frame
    int64_t timestamp = m_startTime + (frame * m_frameTime) - (PRE_SEEK_AMOUNT * m_frameTime);
    int64_t requiredPTS = m_startPTS + (frame * m_frameTime);

    if (timestamp < m_startTime)
        timestamp = m_startTime;

    if (av_seek_frame(m_inputFC, m_videostream, timestamp, AVSEEK_FLAG_ANY) < 0)
    {
        VERBOSE(VB_IMPORTANT, "ThumbFinder::SeekToFrame: seek failed") ;
        return false;
    }

    avcodec_flush_buffers(m_codecCtx);
    getFrameImage(true, requiredPTS);

    return true;
}

bool ThumbFinder::seekForward()
{
    int inc;
    int64_t currentFrame = (m_currentPTS - m_startPTS) / m_frameTime;
    int64_t newFrame;

    inc = SeekAmounts[m_currentSeek].amount;

    if (inc == -1)
        inc = 1;
    else if (inc == -2)
    {
        int pos = 0;
        QMap<long long, int>::Iterator it;
        for (it = m_deleteMap.begin(); it != m_deleteMap.end(); ++it)
        {
            if (it.key() > currentFrame)
            {
                pos = it.key();
                break;
            }
        }
        // seek to next cutpoint
        m_offset = 0;
        seekToFrame(pos, false);
        return true;
    }
    else
        inc = (int) (inc * ceil(m_fps));

    newFrame = currentFrame + inc - m_offset;
    if (newFrame == currentFrame + 1)
        getFrameImage(false);
    else
        seekToFrame(newFrame);

    return true;
}

bool ThumbFinder::seekBackward()
{
    int inc;
    int64_t newFrame;
    int64_t currentFrame = (m_currentPTS - m_startPTS) / m_frameTime;

    inc = SeekAmounts[m_currentSeek].amount;
    if (inc == -1)
        inc = -1;
    else if (inc == -2)
    {
        // seek to previous cut point
        QMap<long long, int>::Iterator it;
        int pos = 0;
        for (it = m_deleteMap.begin(); it != m_deleteMap.end(); ++it)
        {
            if (it.key() >= currentFrame)
                break;

            pos = it.key();
        }

        // seek to next cutpoint
        m_offset = 0;
        seekToFrame(pos, false);
        return true;
    }
    else
        inc = (int) (-inc * ceil(m_fps));

    newFrame = currentFrame + inc - m_offset;
    seekToFrame(newFrame);

    return true;
}

bool ThumbFinder::getFrameImage(bool needKeyFrame, int64_t requiredPTS)
{
    AVPacket pkt;
    AVPicture orig;
    AVPicture retbuf; 
    bzero(&orig, sizeof(AVPicture));
    bzero(&retbuf, sizeof(AVPicture));

    av_init_packet(&pkt);

    int frameFinished = 0;
    int keyFrame;
    int frameCount = 0;
    bool gotKeyFrame = false;

    while (av_read_frame(m_inputFC, &pkt) >= 0 && !frameFinished)
    {
        if (pkt.stream_index == m_videostream)
        {
            frameCount++;

            keyFrame = pkt.flags & PKT_FLAG_KEY;

            if (m_startPTS == -1 && pkt.dts != (int64_t)AV_NOPTS_VALUE)
            {
                m_startPTS = pkt.dts;
                m_frameTime = pkt.duration;
            }

            if (keyFrame)
                gotKeyFrame = true;

            if (!gotKeyFrame && needKeyFrame)
            {
                av_free_packet(&pkt);
                continue;
            }

            if (m_firstIFramePTS == -1)
                m_firstIFramePTS = pkt.dts;

            avcodec_decode_video(m_codecCtx, m_frame, &frameFinished, pkt.data, pkt.size);

            if (requiredPTS != -1 && pkt.dts != (int64_t)AV_NOPTS_VALUE && pkt.dts < requiredPTS)
                frameFinished = false;

            m_currentPTS = pkt.dts;
        }

        av_free_packet(&pkt);
    }

    if (frameFinished)
    {
        avpicture_fill(&retbuf, m_outputbuf, PIX_FMT_RGBA32, m_frameWidth, m_frameHeight);

        avpicture_deinterlace((AVPicture*)m_frame, (AVPicture*)m_frame,
                                m_codecCtx->pix_fmt, m_frameWidth, m_frameHeight);

        img_convert(&retbuf, PIX_FMT_RGBA32, 
                        (AVPicture*) m_frame, m_codecCtx->pix_fmt, m_frameWidth, m_frameHeight);

        QImage img(m_outputbuf, m_frameWidth, m_frameHeight,
                   QImage::Format_RGB32);

        QByteArray ffile = m_frameFile.toLocal8Bit();
        if (!img.save(ffile.constData(), "JPEG"))
        {
            VERBOSE(VB_IMPORTANT, "Failed to save thumb: " + m_frameFile);
        }
        if (m_updateFrame)
        {
            m_frameImage->SetFilename(m_frameFile);
            m_frameImage->Load();
        }

        updateCurrentPos();
    }

    return true;
}

void ThumbFinder::closeAVCodec()
{
    if (m_outputbuf)
        delete[] m_outputbuf;

    // free the frame
    av_free(m_frame);

    // close the codec
    avcodec_close(m_codecCtx);

    // close the video file
    av_close_input_file(m_inputFC);
}

void ThumbFinder::showMenu()
{
    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");
    MythDialogBox *menuPopup = new MythDialogBox("Menu", popupStack, "actionmenu");

    if (menuPopup->Create())
        popupStack->AddScreen(menuPopup);

    menuPopup->SetReturnEvent(this, "action");

    menuPopup->AddButton(tr("Exit, Save Thumbnails"), SLOT(savePressed()));
    menuPopup->AddButton(tr("Exit, Don't Save Thumbnails"), SLOT(cancelPressed()));
    menuPopup->AddButton(tr("Cancel"), NULL);
}

void ThumbFinder::updatePositionBar(int64_t frame)
{
    if (!m_positionImage)
        return;

    QSize size = m_positionImage->GetArea().size();
    QPixmap *pixmap = new QPixmap(size.width(), size.height());

    QPainter p(pixmap);
    QBrush brush(Qt::green);

    p.setBrush(brush);
    p.setPen(Qt::NoPen);
    p.fillRect(0, 0, size.width(), size.height(), brush);

    QMap<long long, int>::Iterator it;

    brush.setColor(Qt::red);
    double startdelta, enddelta;

    for (it = m_deleteMap.begin(); it != m_deleteMap.end(); ++it)
    {
        if (it.key() != 0)
            startdelta = (m_archiveItem->duration * m_fps) / it.key();
        else
            startdelta = size.width();

        ++it;
        if (it.key() != 0)
            enddelta = (m_archiveItem->duration * m_fps) / it.key();
        else
            enddelta = size.width();
        int start = (int) (size.width() / startdelta);
        int end = (int) (size.width() / enddelta);
        p.fillRect(start - 1, 0, end - start, size.height(), brush);
    }

    if (frame == 0)
        frame = 1;
    brush.setColor(Qt::yellow);
    int pos = (int) (size.width() / ((m_archiveItem->duration * m_fps) / frame));
    p.fillRect(pos, 0, 3, size.height(), brush);

    MythImage *image = GetMythMainWindow()->GetCurrentPainter()->GetFormatImage();
    image->Assign(*pixmap);
    m_positionImage->SetImage(image);

    p.end();
    delete pixmap;
}

int ThumbFinder::calcFinalDuration()
{
    if (m_archiveItem->type == "Recording")
    {
        if (m_archiveItem->useCutlist)
        {
            QMap<long long, int>::Iterator it;

            int start, end, cutLen = 0;

            for (it = m_deleteMap.begin(); it != m_deleteMap.end(); ++it)
            {
                start = it.key();
                ++it;
                end = it.key();
                cutLen += end - start;
            }
            return m_archiveItem->duration - (int) (cutLen / m_fps);
        }
    }

    return m_archiveItem->duration;
}

