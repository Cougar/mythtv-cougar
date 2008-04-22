
// POSIX headers
#include <sys/wait.h> // for WIF macros
#include <unistd.h>

// ANSI C headers
#include <cstdlib>

// C++ headers
#include <iostream>

// qt
#include <qdir.h>
#include <qdom.h>

// myth
#include <mythtv/mythcontext.h>
#include <libmythtv/programinfo.h>
#include <mythtv/dialogbox.h>

// mytharchive
#include "archiveutil.h"


struct ArchiveDestination ArchiveDestinations[] =
{
    {AD_DVD_SL,   "Single Layer DVD", "Single Layer DVD (4482Mb)", 4482*1024},
    {AD_DVD_DL,   "Dual Layer DVD",   "Dual Layer DVD (8964Mb)",   8964*1024},
    {AD_DVD_RW,   "DVD +/- RW",       "Rewritable DVD",            4482*1024},
    {AD_FILE,     "File",             "Any file accessable from "
            "your filesystem.",          -1},
};

int ArchiveDestinationsCount = sizeof(ArchiveDestinations) / sizeof(ArchiveDestinations[0]);

QString formatSize(long long sizeKB, int prec)
{
    if (sizeKB>1024*1024*1024) // Terabytes
    {
        double sizeGB = sizeKB/(1024*1024*1024.0);
        return QString("%1 TB").arg(sizeGB, 0, 'f', (sizeGB>10)?0:prec);
    }
    else if (sizeKB>1024*1024) // Gigabytes
    {
        double sizeGB = sizeKB/(1024*1024.0);
        return QString("%1 GB").arg(sizeGB, 0, 'f', (sizeGB>10)?0:prec);
    }
    else if (sizeKB>1024) // Megabytes
    {
        double sizeMB = sizeKB/1024.0;
        return QString("%1 MB").arg(sizeMB, 0, 'f', (sizeMB>10)?0:prec);
    }
    // Kilobytes
    return QString("%1 KB").arg(sizeKB);
}

QString getTempDirectory(bool showError)
{
    QString tempDir = gContext->GetSetting("MythArchiveTempDir", "");

    if (tempDir == "" && showError)
        MythPopupBox::showOkPopup(gContext->GetMainWindow(), 
                                  QObject::tr("Myth Archive"),
                                  QObject::tr("Cannot find the MythArchive work directory.\n"
                                          "Have you set the correct path in the settings?"));

    if (tempDir == "")
        return "";

    // make sure the temp directory setting ends with a trailing "/"
    if (!tempDir.endsWith("/"))
    {
        tempDir += "/";
        gContext->SaveSetting("MythArchiveTempDir", tempDir);
    }

    return tempDir;
}

void checkTempDirectory()
{
    QString tempDir = getTempDirectory();
    QString logDir = tempDir + "logs";
    QString configDir = tempDir + "config";
    QString workDir = tempDir + "work";

    // make sure the 'work', 'logs', and 'config' directories exist
    QDir dir(tempDir);
    if (!dir.exists())
    {
        dir.mkdir(tempDir);
        system("chmod 777 " + tempDir);
    }

    dir = QDir(workDir);
    if (!dir.exists())
    {
        dir.mkdir(workDir);
        system("chmod 777 " + workDir);
    }

    dir = QDir(logDir);
    if (!dir.exists())
    {
        dir.mkdir(logDir);
        system("chmod 777 " + logDir);
    }
    dir = QDir(configDir);
    if (!dir.exists())
    {
        dir.mkdir(configDir);
        system("chmod 777 " + configDir);
    }
}

QString getBaseName(const QString &filename)
{
    QString baseName = filename;
    int pos = filename.findRev('/');
    if (pos > 0)
        baseName = filename.mid(pos + 1);

    return baseName;
}

bool extractDetailsFromFilename(const QString &inFile,
                                QString &chanID, QString &startTime)
{
    VERBOSE(VB_JOBQUEUE, "Extracting details from: " + inFile);

    QString baseName = getBaseName(inFile);

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT chanid, starttime FROM recorded "
            "WHERE basename = :BASENAME");
    query.bindValue(":BASENAME", baseName);

    query.exec();
    if (query.isActive() && query.size())
    {
        query.first();
        chanID = query.value(0).toString();
        startTime= query.value(1).toString();
    }
    else
    {
        VERBOSE(VB_JOBQUEUE, 
                QString("Cannot find details for %1").arg(inFile));
        return false;
    }

    VERBOSE(VB_JOBQUEUE, QString("chanid: %1 starttime:%2 ").arg(chanID).arg(startTime));

    return true;
}

ProgramInfo *getProgramInfoForFile(const QString &inFile)
{
    ProgramInfo *pinfo = NULL;
    QString chanID, startTime;
    bool bIsMythRecording = false;

    bIsMythRecording = extractDetailsFromFilename(inFile, chanID, startTime);

    if (bIsMythRecording)
    {
        pinfo = ProgramInfo::GetProgramFromRecorded(chanID, startTime);

        if (pinfo)
            pinfo->pathname = pinfo->GetPlaybackURL(false, true);
    }

    if (!pinfo)
    {
        // file is not a myth recording or is no longer in the db
        pinfo = new ProgramInfo();
        pinfo->pathname = inFile;
        pinfo->isVideo = true;
        VERBOSE(VB_JOBQUEUE, "File is not a Myth recording.");
    }
    else
        VERBOSE(VB_JOBQUEUE, "File is a Myth recording.");

    return pinfo;
}

bool getFileDetails(ArchiveItem *a)
{
    QString tempDir = gContext->GetSetting("MythArchiveTempDir", "");

    if (!tempDir.endsWith("/"))
        tempDir += "/";

    QString inFile;
    int lenMethod = 0;
    if (a->type == "Recording")
    {
        inFile = a->filename;
        lenMethod = 2;
    }
    else
    {
        inFile = a->filename;
    }

    inFile.replace("\'", "\\\'");
    inFile.replace("\"", "\\\"");
    inFile.replace("`", "\\`");

    QString outFile = tempDir + "/work/file.xml";

    // call mytharchivehelper to get files stream info etc.
    QString command = QString("mytharchivehelper -i \"%1\" \"%2\" %3 > /dev/null 2>&1")
            .arg(inFile).arg(outFile).arg(lenMethod);

    int res = system(command);
    if (WIFEXITED(res))
        res = WEXITSTATUS(res);
    if (res != 0)
        return false;

    QDomDocument doc("mydocument");
    QFile file(outFile);
    if (!file.open(QIODevice::ReadOnly))
        return false;

    if (!doc.setContent( &file )) 
    {
        file.close();
        return false;
    }
    file.close();

    // get file type and duration
    QDomElement docElem = doc.documentElement();
    QDomNodeList nodeList = doc.elementsByTagName("file");
    if (nodeList.count() < 1)
        return false;
    QDomNode n = nodeList.item(0);
    QDomElement e = n.toElement();
    a->fileCodec = e.attribute("type");
    a->duration = e.attribute("duration").toInt();
    a->cutDuration = e.attribute("cutduration").toInt();

    // get frame size and video codec
    nodeList = doc.elementsByTagName("video");
    if (nodeList.count() < 1)
        return false;
    n = nodeList.item(0);
    e = n.toElement();
    a->videoCodec = e.attribute("codec");
    a->videoWidth = e.attribute("width").toInt();
    a->videoHeight = e.attribute("height").toInt();

    return true;
}

void showWarningDialog(const QString msg)
{
    DialogBox *dialog = new DialogBox(gContext->GetMainWindow(), msg);
    dialog->AddButton(QObject::tr("OK"));
    dialog->exec();
    dialog->deleteLater();
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
