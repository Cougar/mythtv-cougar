#include <qstringlist.h>

#include <iostream>

using namespace std;

#include "playbacksock.h"
#include "programinfo.h"
#include "server.h"
#include "mainserver.h"

#include "libmyth/mythcontext.h"
#include "libmyth/util.h"

PlaybackSock::PlaybackSock(MainServer *parent, MythSocket *lsock, 
                           QString lhostname, bool wantevents)
{
    m_parent = parent;
    QString localhostname = gContext->GetHostName();

    refCount = 0;

    sock = lsock;
    hostname = lhostname;
    events = wantevents;
    ip = "";
    backend = false;
    expectingreply = false;

    disconnected = false;
    blockshutdown = true;

    if (hostname == localhostname)
        local = true;
    else
        local = false;
}

PlaybackSock::~PlaybackSock()
{
    sock->DownRef();
}

void PlaybackSock::UpRef(void)
{
    refCount++;
}

bool PlaybackSock::DownRef(void)
{
    refCount--;
    if (refCount < 0)
    {
        m_parent->DeletePBS(this);
        return true;
    }
    return false;
}

bool PlaybackSock::SendReceiveStringList(QStringList &strlist)
{
    sock->Lock();
    sock->UpRef();

    sockLock.lock();
    expectingreply = true;

    sock->writeStringList(strlist);
    bool ok = sock->readStringList(strlist);

    while (ok && strlist[0] == "BACKEND_MESSAGE")
    {
        // oops, not for us
        QString message = strlist[1];
        QString extra = strlist[2];

        MythEvent me(message, extra);
        gContext->dispatch(me);

        ok = sock->readStringList(strlist);
    }

    expectingreply = false;
    sockLock.unlock();

    sock->Unlock();
    sock->DownRef();

    return ok;
}

/**
 *  \brief Appends host's dir's total and used space in kilobytes.
 */
void PlaybackSock::GetDiskSpace(QStringList &o_strlist)
{
    QStringList strlist = QString("QUERY_FREE_SPACE");

    SendReceiveStringList(strlist);

    o_strlist += strlist;

}

int PlaybackSock::CheckRecordingActive(const ProgramInfo *pginfo)
{
    QStringList strlist = QString("CHECK_RECORDING");
    pginfo->ToStringList(strlist);

    SendReceiveStringList(strlist);

    return strlist[0].toInt();
}

int PlaybackSock::StopRecording(const ProgramInfo *pginfo)
{
    QStringList strlist = QString("STOP_RECORDING");
    pginfo->ToStringList(strlist);

    SendReceiveStringList(strlist);

    return strlist[0].toInt();
}

int PlaybackSock::DeleteRecording(const ProgramInfo *pginfo, bool forceMetadataDelete)
{
    QStringList strlist;

    if (forceMetadataDelete)
        strlist = QString("FORCE_DELETE_RECORDING");
    else
        strlist = QString("DELETE_RECORDING");

    pginfo->ToStringList(strlist);

    SendReceiveStringList(strlist);

    return strlist[0].toInt();
}

void PlaybackSock::FillProgramInfo(ProgramInfo *pginfo, QString &playbackhost)
{
    QStringList strlist = QString("FILL_PROGRAM_INFO");
    strlist << playbackhost;
    pginfo->ToStringList(strlist);

    SendReceiveStringList(strlist);

    pginfo->FromStringList(strlist, 0);
}

void PlaybackSock::GenPreviewPixmap(const ProgramInfo *pginfo)
{
    QStringList strlist = QString("QUERY_GENPIXMAP");
    pginfo->ToStringList(strlist);

    SendReceiveStringList(strlist);
}

QString PlaybackSock::PixmapLastModified(const ProgramInfo *pginfo)
{
    QStringList strlist = QString("QUERY_PIXMAP_LASTMODIFIED");
    pginfo->ToStringList(strlist);

    SendReceiveStringList(strlist);

    return strlist[0];
}

bool PlaybackSock::CheckFile(ProgramInfo *pginfo)
{
    QStringList strlist = "QUERY_CHECKFILE";
    strlist << QString::number(0); // don't check slaves
    pginfo->ToStringList(strlist);

    SendReceiveStringList(strlist);

    bool exists = strlist[0].toInt();
    pginfo->pathname = strlist[1];
    return exists;
}

bool PlaybackSock::IsBusy(int capturecardnum)
{
    QStringList strlist = QString("QUERY_REMOTEENCODER %1").arg(capturecardnum);

    strlist << "IS_BUSY";

    SendReceiveStringList(strlist);

    bool state = strlist[0].toInt();
    return state;
}

/** \fn PlaybackSock::GetEncoderState(int)
 *   Returns the maximum bits per second the recorder can produce.
 *  \param capturecardnum Recorder ID in the database.
 */
int PlaybackSock::GetEncoderState(int capturecardnum)
{
    QStringList strlist = QString("QUERY_REMOTEENCODER %1").arg(capturecardnum);
    strlist << "GET_STATE";

    SendReceiveStringList(strlist);

    int state = strlist[0].toInt();
    return state;
}

long long PlaybackSock::GetMaxBitrate(int capturecardnum)
{
    QStringList strlist = QString("QUERY_REMOTEENCODER %1").arg(capturecardnum);
    strlist << "GET_MAX_BITRATE";

    SendReceiveStringList(strlist);

    long long ret = decodeLongLong(strlist, 0);
    return ret;
}

/** \fn *PlaybackSock::GetRecording(int)
 *  \brief Returns the ProgramInfo being used by any current recording.
 *
 *   Caller is responsible for deleting the ProgramInfo when done with it.
 *  \param capturecardnum cardid of recorder
 */
ProgramInfo *PlaybackSock::GetRecording(int capturecardnum)
{
    QStringList strlist = QString("QUERY_REMOTEENCODER %1")
        .arg(capturecardnum);

    strlist << "GET_CURRENT_RECORDING";

    SendReceiveStringList(strlist);

    ProgramInfo *info = new ProgramInfo();
    info->FromStringList(strlist, 0);
    return info;
}

bool PlaybackSock::EncoderIsRecording(int capturecardnum, const ProgramInfo *pginfo)
{
    QStringList strlist = QString("QUERY_REMOTEENCODER %1").arg(capturecardnum);
    strlist << "MATCHES_RECORDING";
    pginfo->ToStringList(strlist);

    SendReceiveStringList(strlist);

    bool ret = strlist[0].toInt();
    return ret;
}

RecStatusType PlaybackSock::StartRecording(int capturecardnum, 
                                           const ProgramInfo *pginfo)
{
    QStringList strlist = QString("QUERY_REMOTEENCODER %1").arg(capturecardnum);
    strlist << "START_RECORDING";
    pginfo->ToStringList(strlist);

    SendReceiveStringList(strlist);

    return RecStatusType(strlist[0].toInt());
}

void PlaybackSock::RecordPending(int capturecardnum, const ProgramInfo *pginfo,
                                 int secsleft)
{
    QStringList strlist = QString("QUERY_REMOTEENCODER %1").arg(capturecardnum);
    strlist << "RECORD_PENDING";
    strlist << QString::number(secsleft);
    pginfo->ToStringList(strlist);

    SendReceiveStringList(strlist);
}

int PlaybackSock::SetSignalMonitoringRate(int capturecardnum,
                                          int rate, int notifyFrontend)
{
    QStringList strlist = QString("QUERY_REMOTEENCODER %1").arg(capturecardnum);
    strlist << "SET_SIGNAL_MONITORING_RATE";
    strlist << QString::number(rate);
    strlist << QString::number(notifyFrontend);

    SendReceiveStringList(strlist);

    int ret = strlist[0].toInt();
    return ret;
}

void PlaybackSock::SetNextLiveTVDir(int capturecardnum, QString dir)
{
    QStringList strlist =
        QString("SET_NEXT_LIVETV_DIR %1 %2").arg(capturecardnum).arg(dir);

    SendReceiveStringList(strlist);
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
