#include <set>
#include <map>
#include <vector>

#include <QImageReader>
#include <QApplication>
#include <QThread>
#include <QStringList>
#include <QUrl>

#include <mythtv/mythcontext.h>

#include <mythtv/libmythui/mythscreenstack.h>
#include <mythtv/libmythui/mythprogressdialog.h>
#include <mythtv/libmythui/mythdialogbox.h>

#include "globals.h"
#include "dbaccess.h"
#include "dirscan.h"
#include "metadatalistmanager.h"
#include "videoscan.h"

namespace
{
    template <typename DirListType>
    class dirhandler : public DirectoryHandler
    {
      public:
        dirhandler(DirListType &video_files,
                   const QStringList &image_extensions) :
            m_video_files(video_files)
        {
            for (QStringList::const_iterator p = image_extensions.begin();
                 p != image_extensions.end(); ++p)
            {
                m_image_ext.insert((*p).toLower());
            }
        }

        DirectoryHandler *newDir(const QString &dir_name,
                                 const QString &fq_dir_name)
        {
            (void) dir_name;
            (void) fq_dir_name;
            return this;
        }

        void handleFile(const QString &file_name,
                        const QString &fq_file_name,
                        const QString &extension,
			const QString &host)

        {
		    //VERBOSE(VB_GENERAL,QString("handleFile: %1 :: %2").arg(fq_file_name).arg(host));
            (void) file_name;
            if (m_image_ext.find(extension.toLower()) == m_image_ext.end())
            {
                m_video_files[fq_file_name].check = false;
                m_video_files[fq_file_name].host = host;
            }
        }

      private:
        typedef std::set<QString> image_ext;
        image_ext m_image_ext;
        DirListType &m_video_files;
    };
}

class MetadataListManager;
class MythUIProgressDialog;

class VideoScannerThread : public QThread
{
    Q_OBJECT
  public:
    VideoScannerThread() : m_RemoveAll(false), m_KeepAll(false)
    {
        m_dbmetadata = new MetadataListManager;
        MetadataListManager::metadata_list ml;
        MetadataListManager::loadAllFromDatabase(ml);
        m_dbmetadata->setList(ml);

        m_ListUnknown = gContext->GetNumSetting("VideoListUnknownFileTypes", 1);
    }

    ~VideoScannerThread()
    {
        delete m_dbmetadata;
    }

    void run()
    {
        QList<QByteArray> image_types = QImageReader::supportedImageFormats();
        QStringList imageExtensions;
        for (QList<QByteArray>::const_iterator p = image_types.begin();
             p != image_types.end(); ++p)
        {
            imageExtensions.push_back(QString(*p));
        }

        uint counter = 0;
        FileCheckList fs_files;
        failedSGHosts.clear();

        SendProgressEvent(counter, (uint)m_directories.size(),
                          tr("Searching for video files"));
        for (QStringList::const_iterator iter = m_directories.begin();
             iter != m_directories.end(); ++iter)
        {
            if (!buildFileList(*iter, imageExtensions, fs_files))
            {
                if (iter->startsWith("myth://"))
                {
                    QUrl sgurl = *iter;
                    QString host = sgurl.host();
                    QString path = sgurl.path();

                    failedSGHosts.append(host);

                    VERBOSE(VB_GENERAL, QString("Failed to scan :%1:").arg(*iter));
                }
            }

            SendProgressEvent(++counter);
        }

        PurgeList db_remove;
        verifyFiles(fs_files, db_remove);
        updateDB(fs_files, db_remove);
    }

    void SetDirs(const QStringList &dirs)
    {
        m_directories = dirs;
    }

    void SetProgressDialog(MythUIProgressDialog *dialog)
    {
        m_dialog = dialog;
    }

    QStringList GetFailedSGHosts(void)
    {
        return failedSGHosts;
    }

  private:

    struct CheckStruct
    {
        bool check;
        QString host;
    };

    typedef std::vector<std::pair<unsigned int, QString> > PurgeList;
    typedef std::map<QString, CheckStruct> FileCheckList;

    void promptForRemoval(unsigned int id, const QString &filename)
    {
        (void) filename;

        // TODO: use single DB connection for all calls
        if (m_RemoveAll)
            m_dbmetadata->purgeByID(id);

    //     QStringList buttonText;
    //     buttonText += QObject::tr("No");
    //     buttonText += QObject::tr("No to all");
    //     buttonText += QObject::tr("Yes");
    //     buttonText += QObject::tr("Yes to all");
    //
    //     DialogCode result = MythPopupBox::ShowButtonPopup(
    //         gContext->GetMainWindow(),
    //         QObject::tr("File Missing"),
    //         QObject::tr("%1 appears to be missing.\n"
    //                     "Remove it from the database?").arg(filename),
    //         buttonText, kDialogCodeButton0);
    //
    //     switch (result)
    //     {
    //         case kDialogCodeRejected:
    //         case kDialogCodeButton0:
    //         default:
    //             break;
    //         case kDialogCodeButton1:
    //             m_KeepAll = true;
    //             break;
    //         case kDialogCodeButton2:
    //             m_dbmetadata->purgeByID(id);
    //             break;
    //         case kDialogCodeButton3:
    //             m_RemoveAll = true;
    //             m_dbmetadata->purgeByID(id);
    //             break;
    //     };

        if (!m_KeepAll && !m_RemoveAll)
        {
            m_RemoveAll = true;
            m_dbmetadata->purgeByID(id);
        }
    }

    void verifyFiles(FileCheckList &files, PurgeList &remove)
    {
        int counter = 0;
        FileCheckList::iterator iter;

        SendProgressEvent(counter, (uint)m_dbmetadata->getList().size(),
                          tr("Verifying video files"));

        // For every file we know about, check to see if it still exists.
        for (MetadataListManager::metadata_list::const_iterator p =
             m_dbmetadata->getList().begin();
             p != m_dbmetadata->getList().end(); ++p)
        {
            QString lname = (*p)->Filename();
            QString lhost = (*p)->Host();
            if (lname != QString::null)
            {
                iter = files.find(lname);
                if (iter != files.end())
                {
                    // If it's both on disk and in the database we're done with
                    // it.
                    iter->second.check= true;
                }
                else
                {
                    // If it's only in the database mark it as such for removal
                    // later and NOT a remote backend.
                    if (lhost == "")
                    {
                        remove.push_back(std::make_pair((*p)->ID(), lname));
                    }
                    else
                    {
                        if (!failedSGHosts.contains(lhost))
                        {
                            VERBOSE(VB_GENERAL, QString("Removing file SG(%1) :%2: ").arg(lhost).arg(lname));
                            remove.push_back(std::make_pair((*p)->ID(), lname));
                        }
                        else
                            VERBOSE(VB_GENERAL, QString("SG(%1) not available. Not removing file :%2: ").arg(lhost).arg(lname));
                    }
                }
            }

            SendProgressEvent(++counter);
        }
    }

    void updateDB(const FileCheckList &add, const PurgeList &remove)
    {
        uint counter = 0;
        SendProgressEvent(counter, (uint)(add.size() + remove.size()),
                          tr("Updating video database"));

        for (FileCheckList::const_iterator p = add.begin(); p != add.end(); ++p)
        {
            // add files not already in the DB
            if (!p->second.check)
            {
                Metadata newFile(p->first, VIDEO_TRAILER_DEFAULT,
                                 VIDEO_COVERFILE_DEFAULT,
                                 Metadata::FilenameToTitle(p->first),
                                 VIDEO_YEAR_DEFAULT,
                                 VIDEO_INETREF_DEFAULT, VIDEO_DIRECTOR_DEFAULT,
                                 VIDEO_PLOT_DEFAULT, 0.0, VIDEO_RATING_DEFAULT,
                                 0, 0, ParentalLevel::plLowest);

                VERBOSE(VB_GENERAL, QString("Adding : %1 : %2").arg(newFile.Host()).arg(newFile.Filename()));
                newFile.setHost(p->second.host);

                newFile.dumpToDatabase();
            }
            SendProgressEvent(++counter);
        }

        for (PurgeList::const_iterator p = remove.begin(); p != remove.end();
                ++p)
        {
            promptForRemoval(p->first, p->second);
            SendProgressEvent(++counter);
        }
    }

    bool buildFileList(const QString &directory,
                                        const QStringList &imageExtensions,
                                        FileCheckList &filelist)
    {
        VERBOSE(VB_GENERAL, QString("buildFileList directory = %1").arg(directory));
        FileAssociations::ext_ignore_list ext_list;
        FileAssociations::getFileAssociation().getExtensionIgnoreList(ext_list);

        dirhandler<FileCheckList> dh(filelist, imageExtensions);
        return ScanVideoDirectory(directory, &dh, ext_list, m_ListUnknown);
    }

    void SendProgressEvent(uint progress, uint total = 0,
            QString messsage = QString())
    {
        if (!m_dialog)
            return;

        ProgressUpdateEvent *pue = new ProgressUpdateEvent(progress, total,
                                                           messsage);
        QApplication::postEvent(m_dialog, pue);
    }


    bool m_ListUnknown;
    bool m_RemoveAll;
    bool m_KeepAll;
    QStringList m_directories;
    QStringList failedSGHosts;

    MetadataListManager *m_dbmetadata;
    MythUIProgressDialog *m_dialog;
};

VideoScanner::VideoScanner()
{
    m_scanThread = new VideoScannerThread();
}

VideoScanner::~VideoScanner()
{
    if (m_scanThread && m_scanThread->wait())
        delete m_scanThread;
}

void VideoScanner::doScan(const QStringList &dirs)
{
    if (m_scanThread->isRunning())
        return;

    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");

    MythUIProgressDialog *progressDlg = new MythUIProgressDialog("",
            popupStack, "videoscanprogressdialog");

    if (progressDlg->Create())
    {
        popupStack->AddScreen(progressDlg, false);
        connect(m_scanThread, SIGNAL(finished()),
                progressDlg, SLOT(Close()));
        connect(m_scanThread, SIGNAL(finished()),
                SLOT(finishedScan()));
    }
    else
    {
        delete progressDlg;
        progressDlg = NULL;
    }

    m_scanThread->SetDirs(dirs);
    m_scanThread->SetProgressDialog(progressDlg);
    m_scanThread->start();
}

void VideoScanner::finishedScan()
{
    emit finished();

    QStringList failedHosts = m_scanThread->GetFailedSGHosts();
    if (failedHosts.size() > 0)
    {
        QString msg = tr("Failed to Scan SG Video Hosts") + ":\n\n";

        for (int i = 0; i < failedHosts.size(); ++i)
            msg += " " + failedHosts.at(i);

        msg += "\n" + tr("If they no longer exist please remove them") + "\n\n";

        ShowOkPopup(msg);
    }

}

////////////////////////////////////////////////////////////////////////

#include "videoscan.moc"
