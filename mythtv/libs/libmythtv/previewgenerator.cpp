// C headers
#include <cmath>

// POSIX headers
#include <sys/types.h> // for stat
#include <sys/stat.h>  // for stat
#include <unistd.h>    // for stat
#include <sys/time.h>
#include <sys/resource.h>

// Qt headers
#include <qfileinfo.h>
#include <qimage.h>

// MythTV headers
#include "RingBuffer.h"
#include "NuppelVideoPlayer.h"
#include "previewgenerator.h"
#include "tv_rec.h"
#include "mythsocket.h"

#define LOC QString("Preview: ")
#define LOC_ERR QString("Preview Error: ")
#define LOC_WARN QString("Preview Warning: ")

/** \class PreviewGenerator
 *  \brief This class creates a preview image of a recording.
 *
 *   The usage is simple: First, pass a ProgramInfo whose pathname points
 *   to a local or remote recording to the constructor. Then call either
 *   Start(void) or Run(void) to generate the preview.
 *
 *   Start(void) will create a thread that processes the request,
 *   creating a sockets the the backend if the recording is not local.
 *
 *   Run(void) will process the request in the current thread, and it
 *   uses the MythContext's server and event sockets if the recording
 *   is not local.
 *
 *   The PreviewGenerator will send Qt signals when the preview is ready
 *   and when the preview thread finishes running if Start(void) was called.
 */

/** \fn PreviewGenerator::PreviewGenerator(const ProgramInfo*,bool)
 *  \brief Constructor
 *
 *   ProgramInfo::pathname must include recording prefix, so that
 *   the file can be found on the file system for local preview
 *   generation. When called by the backend 'local_only' should be set
 *   to true, otherwise the backend may deadlock if the PreviewGenerator
 *   can not find the file.
 *
 *  \param pginfo     ProgramInfo for the recording we want a preview of.
 *  \param local_only If set to true, the preview will only be generated
 *                    if the file is local.
 */
PreviewGenerator::PreviewGenerator(const ProgramInfo *pginfo,
                                   bool local_only)
    : programInfo(*pginfo), localOnly(local_only), isConnected(false),
      createSockets(false), serverSock(NULL)
{
    if (IsLocal())
        return;

    // Try to find a local means to access file...
    QString localFN  = programInfo.GetPlaybackURL(false, true);
    if (!(localFN.left(1) == "/" && QFileInfo(localFN).exists()))
        return; // didn't find file locally, must use remote backend

    // Found file locally, so set the new pathname..
    QString msg = QString(
        "'%1' is not local, "
        "\n\t\t\treplacing with '%2', which is local.")
        .arg(programInfo.pathname).arg(localFN);
    VERBOSE(VB_RECORD, LOC + msg);
    programInfo.pathname = localFN;
}

PreviewGenerator::~PreviewGenerator()
{
    TeardownAll();
}

void PreviewGenerator::TeardownAll(void)
{
    if (!isConnected)
        return;

    const QString filename = programInfo.pathname + ".png";

    MythTimer t;
    t.start();
    for (bool done = false; !done;)
    {
        previewLock.lock();
        if (isConnected)
            emit previewThreadDone(filename, done);
        else
            done = true;
        previewLock.unlock();
        usleep(5000);
    }
    VERBOSE(VB_PLAYBACK, LOC + "previewThreadDone took "<<t.elapsed()<<"ms");
    disconnectSafe();
}

void PreviewGenerator::deleteLater()
{
    TeardownAll();
    QObject::deleteLater();
}

void PreviewGenerator::AttachSignals(QObject *obj)
{
    QMutexLocker locker(&previewLock);
    connect(this, SIGNAL(previewThreadDone(const QString&,bool&)),
            obj,  SLOT(  previewThreadDone(const QString&,bool&)));
    connect(this, SIGNAL(previewReady(const ProgramInfo*)),
            obj,  SLOT(  previewReady(const ProgramInfo*)));
    isConnected = true;
}

/** \fn PreviewGenerator::disconnectSafe(void)
 *  \brief disconnects signals while holding previewLock, ensuring that
 *         no one will receive a signal from this class after this call.
 */
void PreviewGenerator::disconnectSafe(void)
{
    QMutexLocker locker(&previewLock);
    QObject::disconnect(this, NULL, NULL, NULL);
    isConnected = false;
}

/** \fn PreviewGenerator::Start(void)
 *  \brief This call starts a thread that will create a preview.
 */
void PreviewGenerator::Start(void)
{
    pthread_create(&previewThread, NULL, PreviewRun, this);
    // detach, so we don't have to join thread to free thread local mem.
    pthread_detach(previewThread);
}

/** \fn PreviewGenerator::Run(void)
 *  \brief This call creates a preview without starting a new thread.
 */
void PreviewGenerator::Run(void)
{
    if (IsLocal())
    {
        LocalPreviewRun();
    }
    else if (!localOnly)
    {
        RemotePreviewRun();
    }
    else
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + QString("Run() file not local: '%1'")
                .arg(programInfo.pathname));
    }
}

void *PreviewGenerator::PreviewRun(void *param)
{
    // Lower scheduling priority, to avoid problems with recordings.
    if (setpriority(PRIO_PROCESS, 0, 9))
        VERBOSE(VB_IMPORTANT, LOC + "Setting priority failed." + ENO);
    PreviewGenerator *gen = (PreviewGenerator*) param;
    gen->createSockets = true;
    gen->Run();
    gen->deleteLater();
    return NULL;
}

bool PreviewGenerator::RemotePreviewSetup(void)
{
    QString server = gContext->GetSetting("MasterServerIP", "localhost");
    int port       = gContext->GetNumSetting("MasterServerPort", 6543);

    serverSock = gContext->ConnectServer(NULL, server, port);
    return serverSock;
}

void PreviewGenerator::RemotePreviewRun(void)
{
    QStringList strlist = "QUERY_GENPIXMAP";
    programInfo.ToStringList(strlist);
    bool ok = false;

    if (createSockets)
    {
        if (!RemotePreviewSetup())
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR + "Failed to open sockets.");
            return;
        }

        if (serverSock)
        {
            serverSock->writeStringList(strlist);
            ok = serverSock->readStringList(strlist, false);
        }

        RemotePreviewTeardown();
    }
    else
    {
        ok = gContext->SendReceiveStringList(strlist);
    }

    if (ok)
    {
        QMutexLocker locker(&previewLock);
        emit previewReady(&programInfo);
    }
}

void PreviewGenerator::RemotePreviewTeardown(void)
{
    if (serverSock)
    {
        serverSock->DownRef();
        serverSock = NULL;
    }
}

bool PreviewGenerator::SavePreview(QString filename,
                                   const unsigned char *data,
                                   uint width, uint height, float aspect)
{
    if (!data || !width || !height)
        return false;

    const QImage img((unsigned char*) data,
                     width, height, 32, NULL, 65536 * 65536,
                     QImage::LittleEndian);

    float ppw = gContext->GetNumSetting("PreviewPixmapWidth", 160);
    float pph = gContext->GetNumSetting("PreviewPixmapHeight", 120);

    aspect = (aspect <= 0) ? ((float) width) / height : aspect;

    if (aspect > ppw / pph)
        pph = rint(ppw / aspect);
    else
        ppw = rint(pph * aspect);

    QImage small_img = img.smoothScale((int) ppw, (int) pph);

    if (small_img.save(filename.ascii(), "PNG"))
    {
        chmod(filename.ascii(), 0666); // Let anybody update it
        return true;
    }

    // Save failed; if file exists, try saving to .new and moving over
    QString newfile = filename + ".new";
    if (QFileInfo(filename.ascii()).exists() &&
        small_img.save(newfile.ascii(), "PNG"))
    {
        chmod(newfile.ascii(), 0666);
        rename(newfile.ascii(), filename.ascii());
        return true;
    }

    // Couldn't save, nothing else I can do?
    return false;
}

void PreviewGenerator::LocalPreviewRun(void)
{
    programInfo.MarkAsInUse(true, "preview_generator");

    float aspect = 0;
    int   secsin = (gContext->GetNumSetting("PreviewPixmapOffset", 64) +
                    gContext->GetNumSetting("RecordPreRoll",       0));
    int   len, width, height, sz;

    len = width = height = sz = 0;
    unsigned char *data = (unsigned char*)
        GetScreenGrab(&programInfo, programInfo.pathname, secsin,
                      sz, width, height, aspect);
    
    bool ok = SavePreview(programInfo.pathname + ".png",
                          data, width, height, aspect);
    if (ok)
    {
        QMutexLocker locker(&previewLock);
        emit previewReady(&programInfo);
    }

    if (data)
        delete[] data;

    programInfo.MarkAsInUse(false);

    // local update failed, try remote...
    if (!ok && !localOnly)
    {
        VERBOSE(VB_IMPORTANT, LOC_WARN + "Failed to save preview."
                "\n\t\t\tYou may need to check user and group ownership"
                "\n\t\t\ton your frontend and backend for quicker previews."
                "\n\n\t\t\tAttempting to regenerate preview on backend.\n");

        RemotePreviewRun();
    }
}

bool PreviewGenerator::IsLocal(void) const
{
    return QFileInfo(programInfo.pathname).exists();
}

/** \fn PreviewGenerator::GetScreenGrab(const ProgramInfo*,const QString&,int,int&,int&,int&,float&)
 *  \brief Returns a PIX_FMT_RGBA32 buffer containg a frame from the video.
 *
 *  \param pginfo       Recording to grab from.
 *  \param filename     File containing recording.
 *  \param secondsin    Seconds into the video to seek before
 *                      capturing a frame.
 *  \param bufferlen    Returns size of buffer returned (in bytes).
 *  \param video_width  Returns width of frame grabbed.
 *  \param video_height Returns height of frame grabbed.
 *  \param video_aspect Returns aspect ratio of frame grabbed.
 *  \return Buffer allocated with new containing frame in RGBA32 format if
 *          successful, NULL otherwise.
 */
char *PreviewGenerator::GetScreenGrab(
    const ProgramInfo *pginfo, const QString &filename, int secondsin,
    int &bufferlen,
    int &video_width, int &video_height, float &video_aspect)
{
    (void) pginfo;
    (void) filename;
    (void) secondsin;
    (void) bufferlen;
    (void) video_width;
    (void) video_height;
    char *retbuf = NULL;
    bufferlen = 0;
#ifdef USING_FRONTEND
    if (!MSqlQuery::testDBConnection())
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Previewer could not connect to DB.");
        return NULL;
    }

    // pre-test local files for existence and size. 500 ms speed-up...
    if (filename.left(1)=="/")
    {
        QFileInfo info(filename);
        bool invalid = !info.exists() || !info.isReadable() || !info.isFile();
        if (!invalid)
        {
            // Check size too, QFileInfo can not handle large files
            struct stat status;
            stat(filename.ascii(), &status);
            // Using off_t requires a lot of 32/64 bit checking.
            // So just get the size in blocks.
            unsigned long long bsize = status.st_blksize;
            unsigned long long nblk  = status.st_blocks;
            unsigned long long approx_size = nblk * bsize;
            invalid = (approx_size < 8*1024);
        }
        if (invalid)
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR + "Previewer file " +
                    QString("'%1'").arg(filename) + " is not valid.");
            return NULL;
        }
    }

    RingBuffer *rbuf = new RingBuffer(filename, false, false, 0);
    if (!rbuf->IsOpen())
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Previewer could not open file: " +
                QString("'%1'").arg(filename));
        delete rbuf;
        return NULL;
    }

    NuppelVideoPlayer *nvp = new NuppelVideoPlayer("Preview", pginfo);
    nvp->SetRingBuffer(rbuf);

    retbuf = nvp->GetScreenGrab(secondsin, bufferlen,
                                video_width, video_height, video_aspect);

    delete nvp;
    delete rbuf;

#else // USING_FRONTEND
    QString msg = "Backend compiled without USING_FRONTEND !!!!";
    VERBOSE(VB_IMPORTANT, LOC_ERR + msg);
#endif // USING_FRONTEND
    return retbuf;
}
