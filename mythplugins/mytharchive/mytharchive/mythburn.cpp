#include <unistd.h>

#include <stdlib.h>
#include <unistd.h>
#include <iostream>
#include <cstdlib>
#include <sys/wait.h>  // for WIFEXITED and WEXITSTATUS

// qt
#include <qdir.h>
#include <qapplication.h>
#include <QKeyEvent>
#include <QTextStream>

// myth
#include <mythtv/mythcontext.h>
#include <mythtv/mythwidgets.h>
#include <mythtv/libmythdb/mythdb.h>
#include <mythtv/mythdirs.h>
#include <libmythui/mythprogressdialog.h>
#include <libmythui/mythuihelper.h>
#include <libmythui/mythdialogbox.h>
#include <libmythui/mythuitext.h>
#include <libmythui/mythuibutton.h>
#include <libmythui/mythuicheckbox.h>
#include <libmythui/mythuibuttonlist.h>
#include <libmythui/mythuiprogressbar.h>

// mytharchive
#include "archiveutil.h"
#include "mythburn.h"
#include "editmetadata.h"
#include "fileselector.h"
#include "thumbfinder.h"
#include "recordingselector.h"
#include "videoselector.h"

MythBurn::MythBurn(MythScreenStack *parent,
                   MythScreenType *destinationScreen, MythScreenType *themeScreen,
                   ArchiveDestination archiveDestination, QString name)
         :MythScreenType(parent, name)
{
    m_destinationScreen = destinationScreen;
    m_themeScreen = themeScreen;
    m_archiveDestination = archiveDestination;

    // remove any old thumb images
    QString thumbDir = getTempDirectory() + "/config/thumbs";
    QDir dir(thumbDir);
    if (dir.exists())
        system("rm -rf " + thumbDir);

    m_bCreateISO = false;
    m_bDoBurn = false;
    m_bEraseDvdRw = false;
    m_saveFilename = "";
}

MythBurn::~MythBurn(void)
{
    saveConfiguration();

    while (!m_profileList.isEmpty())
         delete m_profileList.takeFirst();
    m_profileList.clear();

    while (!m_archiveList.isEmpty())
         delete m_archiveList.takeFirst();
    m_archiveList.clear();
}

bool MythBurn::Create(void)
{
    bool foundtheme = false;

    // Load the theme for this screen
    foundtheme = LoadWindowFromXML("mythburn-ui.xml", "mythburn", this);

    if (!foundtheme)
        return false;

    try
    {
        m_nextButton = GetMythUIButton("next_button");
        m_prevButton = GetMythUIButton("prev_button");
        m_cancelButton = GetMythUIButton("cancel_button");
        m_nofilesText = GetMythUIText("nofiles");
        m_archiveButtonList = GetMythUIButtonList("archivelist");
        m_addrecordingButton = GetMythUIButton("addrecording_button");
        m_addvideoButton = GetMythUIButton("addvideo_button");
        m_addfileButton = GetMythUIButton("addfile_button");
        m_maxsizeText = GetMythUIText("maxsize");
        m_minsizeText =  GetMythUIText("minsize");
        m_currentsizeErrorText = GetMythUIText("currentsize_error");
        m_currentsizeText = GetMythUIText("currentsize");
        m_sizeBar = GetMythUIProgressBar("size_bar");
    }
    catch (QString &error)
    {
        VERBOSE(VB_IMPORTANT, "Cannot load screen 'mythburn'\n\t\t\t"
                              "Error was: " + error);
        return false;
    }

    m_nextButton->SetText(tr("Finish"));
    connect(m_nextButton, SIGNAL(Clicked()), this, SLOT(handleNextPage()));

    m_prevButton->SetText(tr("Previous"));
    connect(m_prevButton, SIGNAL(Clicked()), this, SLOT(handlePrevPage()));

    m_cancelButton->SetText(tr("Cancel"));
    connect(m_cancelButton, SIGNAL(Clicked()), this, SLOT(handleCancel()));


    loadEncoderProfiles();
    loadConfiguration();

    updateArchiveList();

    m_addrecordingButton->SetText(tr("Add Recording"));
    connect(m_addrecordingButton, SIGNAL(Clicked()), this, SLOT(handleAddRecording()));

    m_addvideoButton->SetText(tr("Add Video"));
    connect(m_addvideoButton, SIGNAL(Clicked()), this, SLOT(handleAddVideo()));

    m_addfileButton->SetText(tr("Add File"));
    connect(m_addfileButton, SIGNAL(Clicked()), this, SLOT(handleAddFile()));

    if (!BuildFocusList())
        VERBOSE(VB_IMPORTANT, "Failed to build a focuslist. Something is wrong");

    SetFocusWidget(m_nextButton);

    return true;
}

bool MythBurn::keyPressEvent(QKeyEvent *event)
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
            showMenu();
        }
        else if (action == "DELETEITEM")
        {
            removeItem();
        }
        else if (action == "INFO")
        {
            // FIXME show thumb selector
        }
        else if (action == "TOGGLECUT")
        {
            toggleUseCutlist();
        }
        else
            handled = false;
    }

    if (!handled && MythScreenType::keyPressEvent(event))
        handled = true;

    return handled;
}

void MythBurn::updateSizeBar(void)
{
    long long size = 0;
    ArchiveItem *a;
    for (int x = 0; x < m_archiveList.size(); x++)
    {
        a = m_archiveList.at(x);
        size += a->newsize; 
    }

    uint usedSpace = size / 1024 / 1024;

    QString tmpSize;

    m_sizeBar->SetTotal(m_archiveDestination.freeSpace / 1024);
    m_sizeBar->SetUsed(usedSpace);

    tmpSize = QString("%1 Mb").arg(m_archiveDestination.freeSpace / 1024);

    m_maxsizeText->SetText(tmpSize);

    m_minsizeText->SetText("0 Mb");

    tmpSize = QString("%1 Mb").arg(usedSpace);

    if (usedSpace > m_archiveDestination.freeSpace / 1024)
    {
        m_currentsizeText->Hide();

        m_currentsizeErrorText->SetText(tmpSize);
        m_currentsizeErrorText->Show();
    }
    else
    {
        m_currentsizeErrorText->Hide();

        m_currentsizeText->SetText(tmpSize);
        m_currentsizeText->Show();
    }
}

void MythBurn::loadEncoderProfiles()
{
    EncoderProfile *item = new EncoderProfile;
    item->name = "NONE";
    item->description = "";
    item->bitrate = 0.0f;
    m_profileList.append(item);

    // find the encoding profiles
    // first look in the ConfDir (~/.mythtv)
    QString filename = GetConfDir() + 
            "/MythArchive/ffmpeg_dvd_" + 
            ((gContext->GetSetting("MythArchiveVideoFormat", "pal")
                .lower() == "ntsc") ? "ntsc" : "pal") + ".xml";

    if (!QFile::exists(filename))
    {
        // not found yet so use the default profiles
        filename = GetShareDir() + 
            "mytharchive/encoder_profiles/ffmpeg_dvd_" + 
            ((gContext->GetSetting("MythArchiveVideoFormat", "pal")
                .lower() == "ntsc") ? "ntsc" : "pal") + ".xml";
    }

    VERBOSE(VB_IMPORTANT, QString("MythArchive: Loading encoding profiles from %1")
            .arg(filename));

    QDomDocument doc("mydocument");
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly))
        return;

    if (!doc.setContent( &file )) 
    {
        file.close();
        return;
    }
    file.close();

    QDomElement docElem = doc.documentElement();
    QDomNodeList profileNodeList = doc.elementsByTagName("profile");
    QString name, desc, bitrate;

    for (int x = 0; x < (int) profileNodeList.count(); x++)
    {
        QDomNode n = profileNodeList.item(x);
        QDomElement e = n.toElement();
        QDomNode n2 = e.firstChild();
        while (!n2.isNull())
        {
            QDomElement e2 = n2.toElement();
            if(!e2.isNull()) 
            {
                if (e2.tagName() == "name")
                    name = e2.text();
                if (e2.tagName() == "description")
                    desc = e2.text();
                if (e2.tagName() == "bitrate")
                    bitrate = e2.text();

            }
            n2 = n2.nextSibling();

        }

        EncoderProfile *item = new EncoderProfile;
        item->name = name;
        item->description = desc;
        item->bitrate = bitrate.toFloat();
        m_profileList.append(item);
    }
}

void MythBurn::toggleUseCutlist(void)
{
    MythUIButtonListItem *item = m_archiveButtonList->GetItemCurrent();
    ArchiveItem *a = (ArchiveItem *) item->getData();

    if (!a)
        return; 

    if (!a->hasCutlist)
        return;

    a->useCutlist = !a->useCutlist;

    if (a->hasCutlist)
        item->setText(a->useCutlist ? tr("Using Cutlist") : tr("Not Using Cutlist"), "cutlist");
    else
        item->setText(tr("No Cut List"), "cutlist");

    recalcItemSize(a);
    updateSizeBar();
}

void MythBurn::handleNextPage()
{
    if (m_archiveList.size() == 0)
    {
        ShowOkPopup(tr("You need to add at least one item to archive!"));
        return;
    }

    runScript();
}

void MythBurn::handlePrevPage()
{
    Close();
}

void MythBurn::handleCancel()
{
    m_destinationScreen->Close();
    m_themeScreen->Close();
    Close();
}

QString MythBurn::loadFile(const QString &filename)
{
    QString res = "";

    QFile file(filename);

    if (!file.exists())
        return "";

    if (file.open( QIODevice::ReadOnly ))
    {
        QTextStream stream(&file);

        while ( !stream.atEnd() )
        {
            res = res + stream.readLine();
        }
        file.close();
    }
    else
        return "";

    return res;
}

void MythBurn::updateArchiveList(void)
{
    QString message = tr("Retrieving File Information. Please Wait...");

    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");

    MythUIBusyDialog *busyPopup = new 
            MythUIBusyDialog(message, popupStack, "mythburnbusydialog");

    if (busyPopup->Create())
        popupStack->AddScreen(busyPopup, false);
    else
    {
        delete busyPopup;
        busyPopup = false;
    }

    qApp->processEvents();

    m_archiveButtonList->Reset();

    if (m_archiveList.size() == 0)
    {
        m_nofilesText->Show();
    }
    else
    {
        ArchiveItem *a;
        for (int x = 0; x < m_archiveList.size(); x++)
        {
            qApp->processEvents();
            a = m_archiveList.at(x);

            // get duration of this file
            if (a->duration == 0)
                getFileDetails(a);

            // get default encoding profile if needed

            if (a->encoderProfile == NULL)
                a->encoderProfile = getDefaultProfile(a);

            recalcItemSize(a);

            MythUIButtonListItem* item = new MythUIButtonListItem(m_archiveButtonList, a->title);
            item->setData(a);
            item->setText(a->subtitle, "subtitle");
            item->setText(a->startDate + " " + a->startTime, "date");
            item->setText(formatSize(a->newsize / 1024, 2), "size");
            if (a->hasCutlist)
                item->setText(a->useCutlist ? tr("Using Cutlist") : tr("Not Using Cutlist"), "cutlist");
            else
                item->setText(tr("No Cut List"), "cutlist");

            item->setText(a->encoderProfile->name, "profile");
        }

        m_nofilesText->Hide();

        m_archiveButtonList->SetItemCurrent(m_archiveButtonList->GetItemFirst());
    }

    updateSizeBar();

    if (busyPopup)
        busyPopup->Close();
}

bool MythBurn::isArchiveItemValid(const QString &type, const QString &filename)
{
    if (type == "Recording")
    {
        QString baseName = getBaseName(filename);

        MSqlQuery query(MSqlQuery::InitCon());
        query.prepare("SELECT title FROM recorded WHERE basename = :FILENAME");
        query.bindValue(":FILENAME", baseName);
        query.exec();
        if (query.isActive() && query.size())
            return true;
        else
        {
            doRemoveArchiveItem(filename);
            VERBOSE(VB_IMPORTANT, QString("MythArchive: Recording not found (%1)").arg(filename));
        }
    }
    else if (type == "Video")
    {
        MSqlQuery query(MSqlQuery::InitCon());
        query.prepare("SELECT title FROM videometadata WHERE filename = :FILENAME");
        query.bindValue(":FILENAME", filename);
        query.exec();
        if (query.isActive() && query.size())
            return true;
        else
        {
            doRemoveArchiveItem(filename);
            VERBOSE(VB_IMPORTANT, QString("MythArchive: Video not found (%1)").arg(filename));
        }
    }
    else if (type == "File")
    {
        if (QFile::exists(filename))
            return true;
        else
        {
            doRemoveArchiveItem(filename);
            VERBOSE(VB_IMPORTANT, QString("MythArchive: File not found (%1)").arg(filename));
        }
    }

    VERBOSE(VB_IMPORTANT, "MythArchive: Archive item removed from list");

    return false;
}

EncoderProfile *MythBurn::getDefaultProfile(ArchiveItem *item)
{
    if (!item)
        return m_profileList.at(0);

    EncoderProfile *profile = NULL;

    // is the file an mpeg2 file?
    if (item->videoCodec.lower() == "mpeg2video")
    {
        // does the file already have a valid DVD resolution?
        if (gContext->GetSetting("MythArchiveVideoFormat", "pal").lower() == "ntsc")
        {
            if ((item->videoWidth == 720 && item->videoHeight == 480) ||
                (item->videoWidth == 704 && item->videoHeight == 480) ||
                (item->videoWidth == 352 && item->videoHeight == 480) ||
                (item->videoWidth == 352 && item->videoHeight == 240))
            {
                // don't need to re-encode
                profile = m_profileList.at(0);
            }
        }
        else
        {
            if ((item->videoWidth == 720 && item->videoHeight == 576) ||
                (item->videoWidth == 704 && item->videoHeight == 576) ||
                (item->videoWidth == 352 && item->videoHeight == 576) ||
                (item->videoWidth == 352 && item->videoHeight == 288))
            {
                // don't need to re-encode
                profile = m_profileList.at(0);
            }
        }
    }

    if (!profile)
    {
        // file needs re-encoding - use default profile setting
        QString defaultProfile = 
                gContext->GetSetting("MythArchiveDefaultEncProfile", "SP");

        for (int x = 0; x < m_profileList.size(); x++)
            if (m_profileList.at(x)->name == defaultProfile)
                profile = m_profileList.at(x);
    }

    return profile;
}

void MythBurn::createConfigFile(const QString &filename)
{
    QDomDocument doc("mythburn");

    QDomElement root = doc.createElement("mythburn");
    doc.appendChild(root);

    QDomElement job = doc.createElement("job");
    job.setAttribute("theme", m_theme);
    root.appendChild(job);

    QDomElement media = doc.createElement("media");
    job.appendChild(media);

    // now loop though selected archive items and add them to the xml file
    ArchiveItem *a;
    for (int x = 0; x < m_archiveList.size(); x++)
    {
        a = m_archiveList.at(x);

        QDomElement file = doc.createElement("file");
        file.setAttribute("type", a->type.lower() );
        file.setAttribute("usecutlist", a->useCutlist);
        file.setAttribute("filename", a->filename);
        file.setAttribute("encodingprofile", a->encoderProfile->name);
        if (a->editedDetails)
        {
            QDomElement details = doc.createElement("details");
            file.appendChild(details);
            details.setAttribute("title", a->title);
            details.setAttribute("subtitle", a->subtitle);
            details.setAttribute("startdate", a->startDate);
            details.setAttribute("starttime", a->startTime);
            QDomText desc = doc.createTextNode(a->description);
            details.appendChild(desc);
        }

        if (a->thumbList.size() > 0)
        {
            QDomElement thumbs = doc.createElement("thumbimages");
            file.appendChild(thumbs);

            for (int x = 0; x < a->thumbList.size(); x++)
            {
                QDomElement thumb = doc.createElement("thumb");
                thumbs.appendChild(thumb);
                ThumbImage *thumbImage = a->thumbList.at(x);
                thumb.setAttribute("caption", thumbImage->caption);
                thumb.setAttribute("filename", thumbImage->filename);
                thumb.setAttribute("frame", (int) thumbImage->frame);
            }
        }

        media.appendChild(file);
    }

    // add the options to the xml file
    QDomElement options = doc.createElement("options");
    options.setAttribute("createiso", m_bCreateISO);
    options.setAttribute("doburn", m_bDoBurn);
    options.setAttribute("mediatype", m_archiveDestination.type);
    options.setAttribute("dvdrsize", m_archiveDestination.freeSpace);
    options.setAttribute("erasedvdrw", m_bEraseDvdRw);
    options.setAttribute("savefilename", m_saveFilename);
    job.appendChild(options);

    // finally save the xml to the file
    QFile f(filename);
    if (!f.open(QIODevice::WriteOnly))
    {
        VERBOSE(VB_IMPORTANT, QString("MythBurn::createConfigFile: "
                "Failed to open file for writing - %1")
                .arg(filename.toLocal8Bit().constData()));
        return;
    }

    QTextStream t(&f);
    t << doc.toString(4);
    f.close();
}

void MythBurn::loadConfiguration(void)
{
    m_theme = gContext->GetSetting("MythBurnMenuTheme", "");
    m_bCreateISO = (gContext->GetSetting("MythBurnCreateISO", "0") == "1");
    m_bDoBurn = (gContext->GetSetting("MythBurnBurnDVDr", "1") == "1");
    m_bEraseDvdRw = (gContext->GetSetting("MythBurnEraseDvdRw", "0") == "1");

    while (!m_archiveList.isEmpty())
         delete m_archiveList.takeFirst();
    m_archiveList.clear();

    // load selected file list
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT type, title, subtitle, description, startdate, "
                  "starttime, size, filename, hascutlist, duration, "
                  "cutduration, videowidth, videoheight, filecodec, "
                  "videocodec, encoderprofile FROM archiveitems "
                  "ORDER BY intid;");

    if (!query.exec())
    {
        MythDB::DBError("archive item insert", query);
        return;
    }

    while (query.next())
    {
        ArchiveItem *a = new ArchiveItem;
        a->type = query.value(0).toString();
        a->title = query.value(1).toString();
        a->subtitle = query.value(2).toString();
        a->description = query.value(3).toString();
        a->startDate = query.value(4).toString();
        a->startTime = query.value(5).toString();
        a->size = query.value(6).toLongLong();
        a->filename = query.value(7).toString();
        a->hasCutlist = (query.value(8).toInt() == 1);
        a->useCutlist = false;
        a->duration = query.value(9).toInt();
        a->cutDuration = query.value(10).toInt();
        a->videoWidth = query.value(11).toInt();
        a->videoHeight = query.value(12).toInt();
        a->fileCodec = query.value(13).toString();
        a->videoCodec = query.value(14).toString();
        a->encoderProfile = getProfileFromName(query.value(15).toString());
        a->editedDetails = false;
        m_archiveList.append(a);
    }
}

EncoderProfile *MythBurn::getProfileFromName(const QString &profileName)
{
    for (int x = 0; x < m_profileList.size(); x++)
        if (m_profileList.at(x)->name == profileName)
            return m_profileList.at(x);

    return NULL;
}

void MythBurn::saveConfiguration(void)
{
    // remove all old archive items from DB
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("DELETE FROM archiveitems;");
    query.exec();

    // save new list of archive items to DB
    ArchiveItem *a;
    for (int x = 0; x < m_archiveList.size(); x++)
    {
        a = m_archiveList.at(x);

        query.prepare("INSERT INTO archiveitems (type, title, subtitle, "
                "description, startdate, starttime, size, filename, hascutlist, "
                "duration, cutduration, videowidth, videoheight, filecodec,"
                "videocodec, encoderprofile) "
                "VALUES(:TYPE, :TITLE, :SUBTITLE, :DESCRIPTION, :STARTDATE, "
                ":STARTTIME, :SIZE, :FILENAME, :HASCUTLIST, :DURATION, "
                ":CUTDURATION, :VIDEOWIDTH, :VIDEOHEIGHT, :FILECODEC, "
                ":VIDEOCODEC, :ENCODERPROFILE);");
        query.bindValue(":TYPE", a->type);
        query.bindValue(":TITLE", a->title);
        query.bindValue(":SUBTITLE", a->subtitle);
        query.bindValue(":DESCRIPTION", a->description);
        query.bindValue(":STARTDATE", a->startDate);
        query.bindValue(":STARTTIME", a->startTime);
        query.bindValue(":SIZE", a->size);
        query.bindValue(":FILENAME", a->filename);
        query.bindValue(":HASCUTLIST", a->hasCutlist);
        query.bindValue(":DURATION", a->duration);
        query.bindValue(":CUTDURATION", a->cutDuration);
        query.bindValue(":VIDEOWIDTH", a->videoWidth);
        query.bindValue(":VIDEOHEIGHT", a->videoHeight);
        query.bindValue(":FILECODEC", a->fileCodec);
        query.bindValue(":VIDEOCODEC", a->videoCodec);
        query.bindValue(":ENCODERPROFILE", a->encoderProfile->name);

        if (!query.exec())
            MythDB::DBError("archive item insert", query);
    }
}

void MythBurn::showMenu()
{
    if (m_archiveList.size() == 0)
        return;

    MythUIButtonListItem *item = m_archiveButtonList->GetItemCurrent();
    ArchiveItem *curItem = (ArchiveItem *) item->getData();

    if (!curItem)
        return;

    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");

    MythDialogBox *menuPopup = new MythDialogBox("Menu", popupStack, "actionmenu");

    if (menuPopup->Create())
        popupStack->AddScreen(menuPopup);

    menuPopup->SetReturnEvent(this, "action");

    if (curItem->hasCutlist)
    {
        if (curItem->useCutlist)
            menuPopup->AddButton(tr("Don't Use Cutlist"), SLOT(toggleUseCutlist()));
        else
            menuPopup->AddButton(tr("Use Cutlist"), SLOT(toggleUseCutlist()));
    }

    menuPopup->AddButton(tr("Remove Item"), SLOT(removeItem()));
    menuPopup->AddButton(tr("Edit Details"), SLOT(editDetails()));
    menuPopup->AddButton(tr("Change Encoding Profile"), SLOT(changeProfile()));
    //menuPopup->AddButton(tr("Edit Thumbnails"), SLOT(editThumbnails()));
    menuPopup->AddButton(tr("Cancel"), NULL);
}

void MythBurn::removeItem()
{
    MythUIButtonListItem *item = m_archiveButtonList->GetItemCurrent();
    ArchiveItem *curItem = (ArchiveItem *) item->getData();

    if (!curItem)
        return;

    m_archiveList.removeAll(curItem);

    updateArchiveList();
}

void MythBurn::editDetails()
{
    MythUIButtonListItem *item = m_archiveButtonList->GetItemCurrent();
    ArchiveItem *curItem = (ArchiveItem *) item->getData();

    if (!curItem)
        return;

    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    EditMetadataDialog *editor = new EditMetadataDialog(mainStack, curItem);

    connect(editor, SIGNAL(haveResult(bool, ArchiveItem *)),
            this, SLOT(editorClosed(bool, ArchiveItem *)));

    if (editor->Create())
        mainStack->AddScreen(editor);
}

void MythBurn::editorClosed(bool ok, ArchiveItem *item)
{
    MythUIButtonListItem *gridItem = m_archiveButtonList->GetItemCurrent();

    if (ok && item && gridItem)
    {
        // update the grid to reflect any changes
        gridItem->setText(item->title);
        gridItem->setText(item->subtitle, "subtitle");
        gridItem->setText(item->startDate + " " + item->startTime, "date");
    }
}

void MythBurn::changeProfile()
{
    MythUIButtonListItem *item = m_archiveButtonList->GetItemCurrent();
    ArchiveItem *curItem = (ArchiveItem *) item->getData();

    if (!curItem)
        return;

    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");

    ProfileDialog *profileDialog = new ProfileDialog(popupStack, curItem, m_profileList);

    if (profileDialog->Create())
    {
        popupStack->AddScreen(profileDialog, false);
        connect(profileDialog, SIGNAL(haveResult(int)),
                this, SLOT(profileChanged(int)));
    }
}

void MythBurn::profileChanged(int profileNo)
{
    if (profileNo > m_profileList.size() - 1)
        return;

    EncoderProfile *profile = m_profileList.at(profileNo);

    MythUIButtonListItem *item = m_archiveButtonList->GetItemCurrent();
    if (!item)
        return;

    ArchiveItem *archiveItem = (ArchiveItem *) item->getData();
    if (!archiveItem)
        return;

    archiveItem->encoderProfile = profile;

    item->setText(profile->name, "profile");
    item->setText(formatSize(archiveItem->newsize / 1024, 2), "size");

    updateSizeBar();
}

void MythBurn::runScript()
{
    QString tempDir = getTempDirectory();
    QString logDir = tempDir + "logs";
    QString configDir = tempDir + "config";
    QString commandline;

    // remove existing progress.log if present
    if (QFile::exists(logDir + "/progress.log"))
        QFile::remove(logDir + "/progress.log");

    // remove cancel flag file if present
    if (QFile::exists(logDir + "/mythburncancel.lck"))
        QFile::remove(logDir + "/mythburncancel.lck");

    createConfigFile(configDir + "/mydata.xml");
    commandline = "python " + GetShareDir() + "mytharchive/scripts/mythburn.py";
    commandline += " -j " + configDir + "/mydata.xml";             // job file
    commandline += " -l " + logDir + "/progress.log";              // progress log
    commandline += " > "  + logDir + "/mythburn.log 2>&1 &";       //Logs

    gContext->SaveSetting("MythArchiveLastRunStatus", "Running");

    int state = system(commandline);

    if (state != 0) 
    {
        ShowOkPopup(tr("It was not possible to create the DVD. "
                       " An error occured when running the scripts"));
    }

    m_destinationScreen->Close();
    m_themeScreen->Close();
    Close();
}

void MythBurn::handleAddRecording()
{
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    RecordingSelector *selector = new RecordingSelector(mainStack, &m_archiveList);

    connect(selector, SIGNAL(haveResult(bool)),
            this, SLOT(selectorClosed(bool)));

    if (selector->Create())
        mainStack->AddScreen(selector);
}

void MythBurn::selectorClosed(bool ok)
{
    if (ok)
        updateArchiveList();
}

void MythBurn::handleAddVideo()
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT title FROM videometadata");
    query.exec();
    if (query.isActive() && query.size())
    {
    }
    else
    {
        MythPopupBox::showOkPopup(gContext->GetMainWindow(), QObject::tr("Video Selector"),
                                  QObject::tr("You don't have any videos!"));
        return;
    }

    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    VideoSelector *selector = new VideoSelector(mainStack, &m_archiveList);

    connect(selector, SIGNAL(haveResult(bool)),
            this, SLOT(selectorClosed(bool)));

    if (selector->Create())
        mainStack->AddScreen(selector);
}

void MythBurn::handleAddFile()
{
    QString filter = gContext->GetSetting("MythArchiveFileFilter",
                                          "*.mpg *.mpeg *.mov *.avi *.nuv");

    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    FileSelector *selector = new FileSelector(mainStack, &m_archiveList,
                                             FSTYPE_FILELIST, "/", filter);

    connect(selector, SIGNAL(haveResult(bool)),
            this, SLOT(selectorClosed(bool)));

    if (selector->Create())
        mainStack->AddScreen(selector);
}

///////////////////////////////////////////////////////////////////////////////

ProfileDialog::ProfileDialog(MythScreenStack *parent, ArchiveItem *archiveItem,
                             QList<EncoderProfile *> profileList)
              : MythScreenType(parent, "functionpopup")
{
    m_archiveItem = archiveItem;
    m_profileList = profileList;
}

bool ProfileDialog::Create()
{
    if (!LoadWindowFromXML("mythburn-ui.xml", "profilepopup", this))
        return false;

    try
    {
        m_captionText = GetMythUIText("caption_text");
        m_descriptionText = GetMythUIText("description_text");
        m_oldSizeText = GetMythUIText("oldsize_text");
        m_newSizeText = GetMythUIText("newsize_text");
        m_profile_list = GetMythUIButtonList("profile_list");
        m_okButton = GetMythUIButton("ok_button");
    }
    catch (QString &error)
    {
        VERBOSE(VB_IMPORTANT, "Cannot load screen 'profilepopup'/r/t/t/t"
                              "Error was: " + error);
        return false;
    }

    for (int x = 0; x < m_profileList.size(); x++)
    {
        MythUIButtonListItem *item = new 
                MythUIButtonListItem(m_profile_list, m_profileList.at(x)->name);
        item->setData(m_profileList.at(x));
    }

    connect(m_profile_list, SIGNAL(itemSelected(MythUIButtonListItem*)),
            this, SLOT(profileChanged(MythUIButtonListItem*)));


    m_profile_list->MoveToNamedPosition(m_archiveItem->encoderProfile->name);

    m_captionText->SetText(m_archiveItem->title);
    m_oldSizeText->SetText(formatSize(m_archiveItem->size / 1024, 2));
    m_okButton->SetText(tr("Ok"));

    connect(m_okButton, SIGNAL(Clicked()), this, SLOT(save()));

    if (!BuildFocusList())
        VERBOSE(VB_IMPORTANT, "Failed to build a focuslist. Something is wrong");

    SetFocusWidget(m_profile_list);

    return true;
}

void ProfileDialog::profileChanged(MythUIButtonListItem *item)
{
    if (!item)
        return;

    EncoderProfile *profile = (EncoderProfile*)item->getData();
    if (!profile)
        return;

    m_descriptionText->SetText(profile->description);

    m_archiveItem->encoderProfile = profile;

    // calc new size
    recalcItemSize(m_archiveItem);

    m_newSizeText->SetText(formatSize(m_archiveItem->newsize / 1024, 2));
}


void ProfileDialog::save(void)
{
    emit haveResult(m_profile_list->GetCurrentPos());

    Close();
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
