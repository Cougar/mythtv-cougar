// qt
#include <QDir>
#include <QFontMetrics>

// myth
#include <mythtv/mythcontext.h>
#include <mythtv/mythdbcon.h>
#include <mythtv/audiooutput.h>

// mythmusic
#include "importmusic.h"
#include "decoder.h"
#include "genres.h"
#include "metadata.h"
#include "cdrip.h"
#include "editmetadata.h"
#include "musicplayer.h"

#include <mythtv/libmythui/mythdialogbox.h>
#include <mythtv/libmythui/mythuitext.h>
#include <mythtv/libmythui/mythuiimage.h>
#include <mythtv/libmythui/mythuicheckbox.h>
#include <mythtv/libmythui/mythuitextedit.h>
#include <mythtv/libmythui/mythuibutton.h>
#include <mythtv/libmythui/mythuibuttonlist.h>

static bool copyFile(const QString &src, const QString &dst)
{
    const int bufferSize = 16*1024;

    QFile s(src);
    QFile d(dst);
    char buffer[bufferSize];
    int len;

    if (!s.open(QIODevice::ReadOnly))
        return false;

    if (!d.open(QIODevice::WriteOnly))
    {
        s.close();
        return false;
    }

    len = s.readBlock(buffer, bufferSize);
    do
    {
        d.writeBlock(buffer, len);
        len = s.readBlock(buffer, bufferSize);
    } while (len > 0);

    s.close();
    d.close();

    return true;
}

///////////////////////////////////////////////////////////////////////////////

FileScannerThread::FileScannerThread(ImportMusicDialog *parent)
{
    m_parent = parent;
}

void FileScannerThread::run()
{
    m_parent->doScan();
}

///////////////////////////////////////////////////////////////////////////////


ImportMusicDialog::ImportMusicDialog(MythScreenStack *parent)
                :MythScreenType(parent, "musicimportfiles")
{
    m_popupMenu = NULL;

    m_defaultCompilation = false;
    m_defaultCompArtist = "";
    m_defaultArtist = "";
    m_defaultAlbum = "";
    m_defaultGenre = "";
    m_defaultYear = 0;
    m_defaultRating = 0;
    m_haveDefaults = false;

    m_somethingWasImported = false;
    m_tracks = new vector<TrackInfo*>;
}

ImportMusicDialog::~ImportMusicDialog()
{
    gContext->SaveSetting("MythMusicLastImportDir", m_locationEdit->GetText());

    delete m_tracks;
}

void ImportMusicDialog::fillWidgets()
{
    if (m_tracks->size() > 0)
    {
        // update current
        m_currentText->SetText(QString("%1 of %2")
                .arg(m_currentTrack + 1).arg(m_tracks->size()));

        Metadata *meta = m_tracks->at(m_currentTrack)->metadata;
        m_filenameText->SetText(meta->Filename());
        m_compilationCheck->SetCheckState(meta->Compilation());
        m_compartistText->SetText(meta->CompilationArtist());
        m_artistText->SetText(meta->Artist());
        m_albumText->SetText(meta->Album());
        m_titleText->SetText(meta->Title());
        m_genreText->SetText(meta->Genre());
        m_yearText->SetText(QString::number(meta->Year()));
        m_trackText->SetText(QString::number(meta->Track()));
        if (m_tracks->at(m_currentTrack)->isNewTune)
        {
            m_coverartButton->SetVisible(false);
            m_statusText->SetText(tr("New File"));
        }
        else
        {
            m_coverartButton->SetVisible(true);
            m_statusText->SetText(tr("All Ready in Database"));
        }
    }
    else
    {
        // update current
        m_currentText->SetText(tr("Non found"));
        m_filenameText->Reset();
        m_compilationCheck->SetCheckState(false);
        m_compartistText->Reset();
        m_artistText->Reset();
        m_albumText->Reset();
        m_titleText->Reset();
        m_genreText->Reset();
        m_yearText->Reset();
        m_trackText->Reset();
        m_statusText->Reset();
        m_coverartButton->SetVisible(false);
    }
}

bool ImportMusicDialog::keyPressEvent(QKeyEvent *event)
{
    if (GetFocusWidget() && GetFocusWidget()->keyPressEvent(event))
        return true;

    bool handled = false;
    QStringList actions;
    gContext->GetMainWindow()->TranslateKeyPress("Global", event, actions);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        handled = true;

        if (action == "LEFT")
        {
            m_prevButton->Push();
        }
        else if (action == "RIGHT")
        {
            m_nextButton->Push();
        }
        else if (action == "INFO")
        {
            showEditMetadataDialog();
        }
        else if (action == "MENU")
        {
            showMenu();
        }
        else if (action == "1")
        {
            setCompilation();
        }
        else if (action == "2")
        {
            setCompilationArtist();
        }
        else if (action == "3")
        {
            setArtist();
        }
        else if (action == "4")
        {
            setAlbum();
        }
        else if (action == "5")
        {
            setGenre();
        }
        else if (action == "6")
        {
            setYear();
        }
        else if (action == "7")
        {
            setRating();
        }
        else if (action == "8")
        {
            setTitleWordCaps();
        }
        else if (action == "9")
        {
            setTitleInitialCap();
        }
        else if (action == "0")
        {
            if (m_tracks->size() > 0 && !m_tracks->at(m_currentTrack)->isNewTune)
                showImportCoverArtDialog();
        }
        else
            handled = false;
    }

    if (!handled && MythScreenType::keyPressEvent(event))
        handled = true;

    return handled;
}

bool ImportMusicDialog::Create()
{
    if (!LoadWindowFromXML("music-ui.xml", "import_music", this))
        return false;

    m_locationEdit = dynamic_cast<MythUITextEdit *>(GetChild("location"));

    m_locationButton = dynamic_cast<MythUIButton *>(GetChild("directoryfinder"));
    if (m_locationButton)
        connect(m_locationButton, SIGNAL(Clicked()), SLOT(locationPressed()));

    m_scanButton = dynamic_cast<MythUIButton *>(GetChild("scan"));
    if (m_scanButton)
        connect(m_scanButton, SIGNAL(Clicked()), SLOT(startScan()));

    m_coverartButton = dynamic_cast<MythUIButton *>(GetChild("coverart"));
    if (m_coverartButton)
        connect(m_coverartButton, SIGNAL(Clicked()), SLOT(coverArtPressed()));

    m_filenameText = dynamic_cast<MythUIText *>(GetChild("filename"));
    m_compartistText = dynamic_cast<MythUIText *>(GetChild("compartist"));
    m_artistText = dynamic_cast<MythUIText *>(GetChild("artist"));
    m_albumText = dynamic_cast<MythUIText *>(GetChild("album"));
    m_titleText = dynamic_cast<MythUIText *>(GetChild("title"));
    m_genreText = dynamic_cast<MythUIText *>(GetChild("genre"));
    m_yearText = dynamic_cast<MythUIText *>(GetChild("year"));
    m_trackText = dynamic_cast<MythUIText *>(GetChild("track"));
    m_currentText = dynamic_cast<MythUIText *>(GetChild("position"));
    m_statusText = dynamic_cast<MythUIText *>(GetChild("status"));

    m_compilationCheck = dynamic_cast<MythUICheckBox *>
                                                (GetChild("compilation"));

    m_playButton = dynamic_cast<MythUIButton *>(GetChild("play"));
    if (m_playButton)
        connect(m_playButton, SIGNAL(Clicked()), SLOT(playPressed()));

    m_nextnewButton = dynamic_cast<MythUIButton *>(GetChild("nextnew"));
    if (m_nextnewButton)
        connect(m_nextnewButton, SIGNAL(Clicked()), SLOT(nextNewPressed()));

    m_addButton = dynamic_cast<MythUIButton *>(GetChild("add"));
    if (m_addButton)
        connect(m_addButton, SIGNAL(Clicked()), SLOT(addPressed()));

    m_addallnewButton = dynamic_cast<MythUIButton *>(GetChild("addallnew"));
    if (m_addallnewButton)
        connect(m_addallnewButton, SIGNAL(Clicked()), SLOT(addAllNewPressed()));

    m_nextButton = dynamic_cast<MythUIButton *>(GetChild("next"));
    if (m_nextButton)
        connect(m_nextButton, SIGNAL(Clicked()), SLOT(nextPressed()));

    m_prevButton = dynamic_cast<MythUIButton *>(GetChild("prev"));
    if (m_prevButton)
        connect(m_prevButton, SIGNAL(Clicked()), SLOT(prevPressed()));

    fillWidgets();

    BuildFocusList();

    m_locationEdit->SetText(gContext->GetSetting("MythMusicLastImportDir", "/"));

    return true;
}

void ImportMusicDialog::locationPressed()
{
    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");
    MythUIFileBrowser *fb = new MythUIFileBrowser(popupStack, m_locationEdit->GetText());
    // TODO Install a name filter on supported music formats
    fb->SetTypeFilter(QDir::AllDirs | QDir::Readable);
    if (fb->Create())
    {
        fb->SetReturnEvent(this, "locationchange");
        popupStack->AddScreen(fb);
    }
    else
        delete fb;
}

void ImportMusicDialog::coverArtPressed()
{
    showImportCoverArtDialog();
}

void ImportMusicDialog::playPressed()
{
    if (m_tracks->size() == 0)
        return;

    Metadata *meta = m_tracks->at(m_currentTrack)->metadata;

    gPlayer->playFile(*meta);
}

void ImportMusicDialog::prevPressed()
{
    if (m_currentTrack > 0)
    {
        m_currentTrack--;
        fillWidgets();
    }
}

void ImportMusicDialog::nextPressed()
{
    if (m_currentTrack < (int) m_tracks->size() - 1)
    {
        m_currentTrack++;
        fillWidgets();
    }
}

void ImportMusicDialog::addPressed()
{
    if (m_tracks->size() == 0)
        return;

    Metadata *meta = m_tracks->at(m_currentTrack)->metadata;

    // is the current track a new file?
    if (m_tracks->at(m_currentTrack)->isNewTune)
    {
        // get the save filename - this also creates the correct directory stucture
        QString saveFilename = Ripper::filenameFromMetadata(meta);

        // we need to manually copy the file extension
        QFileInfo fi(meta->Filename());
        saveFilename += "." + fi.extension(false);

        // copy the file to the new location
        if (!copyFile(meta->Filename(), saveFilename))
        {
            ShowOkPopup(tr("Copy Failed\nCould not copy file to: %1")
                                                        .arg(saveFilename));
            return;
        }

        meta->setFilename(saveFilename);

        // do we need to update the tags?
        if (m_tracks->at(m_currentTrack)->metadataHasChanged)
        {
            Decoder *decoder = Decoder::create(saveFilename, NULL, NULL, true);
            if (decoder)
            {
                decoder->commitMetadata(meta);
                delete decoder;
            }
        }

        // update the database
        meta->dumpToDatabase();
        m_somethingWasImported = true;

        m_tracks->at(m_currentTrack)->isNewTune = Ripper::isNewTune(
                meta->Artist(), meta->Album(), meta->Title());

        // update the UI
        fillWidgets();
    }
    else
        ShowOkPopup(tr("This track is already in the database"));
}

void ImportMusicDialog::addAllNewPressed()
{
    if (m_tracks->size() == 0)
        return;

    m_currentTrack = 0;
    int newCount = 0;

    while (m_currentTrack < (int) m_tracks->size())
    {
        fillWidgets();
    //        qApp->processEvents();

        if (m_tracks->at(m_currentTrack)->isNewTune)
        {
            addPressed();
            newCount++;
        }

//        qApp->processEvents();

        m_currentTrack++;
    }

    m_currentTrack--;

    ShowOkPopup(tr("%1 new tracks were added to the database").arg(newCount));
}

void ImportMusicDialog::nextNewPressed()
{
    if (m_tracks->size() == 0)
        return;

    uint track = m_currentTrack + 1;
    while (track < m_tracks->size())
    {
        if (m_tracks->at(track)->isNewTune)
        {
            m_currentTrack = track;
            fillWidgets();
            break;
        }
        track++;
    }
}

void ImportMusicDialog::startScan()
{
    MythBusyDialog *busy = new MythBusyDialog(QObject::tr("Searching for music files"));

    FileScannerThread *scanner = new FileScannerThread(this);
    busy->start();
    scanner->start();

    while (!scanner->isFinished())
    {
        usleep(500);
//        qApp->processEvents();
    }

    delete scanner;

    m_currentTrack = 0;
    fillWidgets();

    busy->close();
    busy->deleteLater();
}

void ImportMusicDialog::doScan()
{
    m_tracks->clear();
    m_sourceFiles.clear();
    QString location = m_locationEdit->GetText();
    scanDirectory(location, m_tracks);
}

void ImportMusicDialog::scanDirectory(QString &directory, vector<TrackInfo*> *tracks)
{
    QDir d(directory);

    if (!d.exists())
        return;

    const QFileInfoList list = d.entryInfoList();
    if (list.isEmpty())
        return;

    QFileInfoList::const_iterator it = list.begin();
    const QFileInfo *fi;

    while (it != list.end())
    {
        fi = &(*it);
        ++it;
        if (fi->fileName() == "." || fi->fileName() == "..")
            continue;
        QString filename = fi->absoluteFilePath();
        if (fi->isDir())
            scanDirectory(filename, tracks);
        else
        {
            Decoder *decoder = Decoder::create(filename, NULL, NULL, true);
            if (decoder)
            {
                Metadata *metadata = decoder->getMetadata();
                if (metadata)
                {
                    TrackInfo * track = new TrackInfo;
                    track->metadata = metadata;
                    track->isNewTune = Ripper::isNewTune(metadata->Artist(),
                            metadata->Album(), metadata->Title());
                    track->metadataHasChanged = false;
                    tracks->push_back(track);
                    m_sourceFiles.append(filename);
                }

                delete decoder;
            }
        }
    }
}

void ImportMusicDialog::showEditMetadataDialog()
{
    if (m_tracks->size() == 0)
        return;

    Metadata *editMeta = m_tracks->at(m_currentTrack)->metadata;

    EditMetadataDialog editDialog(editMeta, gContext->GetMainWindow(),
                                  "edit_metadata", "music-", "edit metadata");
    editDialog.setSaveMetadataOnly();

    if (kDialogCodeRejected != editDialog.exec())
    {
        m_tracks->at(m_currentTrack)->metadataHasChanged = true;
        m_tracks->at(m_currentTrack)->isNewTune = Ripper::isNewTune(
                editMeta->Artist(), editMeta->Album(), editMeta->Title());
        fillWidgets();
    }
}

void ImportMusicDialog::showMenu()
{
    if (m_popupMenu)
        return;

    if (m_tracks->size() == 0)
        return;

    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");

    MythDialogBox *menu = new MythDialogBox("", popupStack, "importmusicmenu");

    if (menu->Create())
        popupStack->AddScreen(menu);
    else
    {
        delete menu;
        return;
    }

    menu->AddButton(tr("Save Defaults"), SLOT(saveDefaults()));

    if (m_haveDefaults)
    {
        menu->AddButton(tr("Change Compilation Flag"), SLOT(setCompilation()));
        menu->AddButton(tr("Change Compilation Artist"),
                                SLOT(setCompilationArtist()));
        menu->AddButton(tr("Change Artist"), SLOT(setArtist()));
        menu->AddButton(tr("Change Album"), SLOT(setAlbum()));
        menu->AddButton(tr("Change Genre"), SLOT(setGenre()));
        menu->AddButton(tr("Change Year"), SLOT(setYear()));
        menu->AddButton(tr("Change Rating"), SLOT(setRating()));
    }

    menu->AddButton(tr("Cancel"));
}

void ImportMusicDialog::saveDefaults(void)
{
    Metadata *data = m_tracks->at(m_currentTrack)->metadata;
    m_defaultCompilation = data->Compilation();
    m_defaultCompArtist = data->CompilationArtist();
    m_defaultArtist = data->Artist();
    m_defaultAlbum = data->Album();
    m_defaultGenre = data->Genre();
    m_defaultYear = data->Year();
    m_defaultRating = data->Rating();

    m_haveDefaults = true;
}

void ImportMusicDialog::setCompilation(void)
{
    if (!m_haveDefaults)
        return;

    Metadata *data = m_tracks->at(m_currentTrack)->metadata;

    if (m_defaultCompilation)
    {
        data->setCompilation(m_defaultCompilation);
        data->setCompilationArtist(m_defaultCompArtist);
    }
    else
    {
        data->setCompilation(m_defaultCompilation);
        data->setCompilationArtist(m_defaultArtist);
    }

    fillWidgets();
}

void ImportMusicDialog::setCompilationArtist(void)
{
    if (!m_haveDefaults)
        return;

    Metadata *data = m_tracks->at(m_currentTrack)->metadata;
    data->setCompilationArtist(m_defaultCompArtist);

    fillWidgets();
}

void ImportMusicDialog::setArtist(void)
{
    if (!m_haveDefaults)
        return;

    Metadata *data = m_tracks->at(m_currentTrack)->metadata;
    data->setArtist(m_defaultArtist);

    m_tracks->at(m_currentTrack)->isNewTune = Ripper::isNewTune(
            data->Artist(), data->Album(), data->Title());

    fillWidgets();
}

void ImportMusicDialog::setAlbum(void)
{
    if (!m_haveDefaults)
        return;

    Metadata *data = m_tracks->at(m_currentTrack)->metadata;
    data->setAlbum(m_defaultAlbum);

    m_tracks->at(m_currentTrack)->isNewTune = Ripper::isNewTune(
            data->Artist(), data->Album(), data->Title());

    fillWidgets();
}

void ImportMusicDialog::setYear(void)
{
    if (!m_haveDefaults)
        return;

    Metadata *data = m_tracks->at(m_currentTrack)->metadata;
    data->setYear(m_defaultYear);

    fillWidgets();
}

void ImportMusicDialog::setGenre(void)
{
    if (!m_haveDefaults)
        return;

    Metadata *data = m_tracks->at(m_currentTrack)->metadata;
    data->setGenre(m_defaultGenre);

    fillWidgets();
}

void ImportMusicDialog::setRating(void)
{
    if (!m_haveDefaults)
        return;

    Metadata *data = m_tracks->at(m_currentTrack)->metadata;
    data->setRating(m_defaultRating);
}

void ImportMusicDialog::setTitleInitialCap(void)
{
    Metadata *data = m_tracks->at(m_currentTrack)->metadata;
    QString title = data->Title();
    bool bFoundCap = false;

    for (int x = 0; x < title.length(); x++)
    {
        if (title[x].isLetter())
        {
            if (bFoundCap == false)
            {
                title[x] = title[x].upper();
                bFoundCap = true;
            }
            else
                title[x] = title[x].lower();
        }
    }

    data->setTitle(title);
    fillWidgets();
}

void ImportMusicDialog::setTitleWordCaps(void)
{
    Metadata *data = m_tracks->at(m_currentTrack)->metadata;
    QString title = data->Title();
    bool bInWord = false;

    for (int x = 0; x < title.length(); x++)
    {
        if (title[x].isSpace())
            bInWord = false;
        else
        {
            if (title[x].isLetter())
            {
                if (!bInWord)
                {
                    title[x] = title[x].upper();
                    bInWord = true;
                }
                else
                    title[x] = title[x].lower();
            }
        }
    }

    data->setTitle(title);
    fillWidgets();
}

void ImportMusicDialog::showImportCoverArtDialog(void)
{
    if (m_tracks->size() == 0)
        return;

    QFileInfo fi(m_sourceFiles.at(m_currentTrack));

    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    ImportCoverArtDialog *import = new ImportCoverArtDialog(mainStack,
                                        fi.dirPath(true),
                                        m_tracks->at(m_currentTrack)->metadata);

    if (import->Create())
        mainStack->AddScreen(import);
    else
        delete import;
}

void ImportMusicDialog::customEvent(QEvent *event)
{
    if (event->type() == kMythDialogBoxCompletionEventType)
    {
        DialogCompletionEvent *dce = dynamic_cast<DialogCompletionEvent*>(event);

        if (!dce)
            return;

        const QString resultid = dce->GetId();

        if (resultid == "locationchange")
        {
            m_locationEdit->SetText(dce->GetResultText());
            startScan();
        }
    }
}

///////////////////////////////////////////////////////////////////////////////

ImportCoverArtDialog::ImportCoverArtDialog(MythScreenStack *parent,
                                           const QString &sourceDir,
                                           Metadata *metadata)
    :MythScreenType(parent, "import_coverart")
{
    m_sourceDir = sourceDir;
    m_metadata = metadata;

    scanDirectory();
}

ImportCoverArtDialog::~ImportCoverArtDialog()
{

}

bool ImportCoverArtDialog::keyPressEvent(QKeyEvent *event)
{
    if (GetFocusWidget() && GetFocusWidget()->keyPressEvent(event))
        return true;

    bool handled = false;
    QStringList actions;
    gContext->GetMainWindow()->TranslateKeyPress("Global", event, actions);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        handled = true;

        if (action == "LEFT")
        {
            m_prevButton->Push();
        }
        else if (action == "RIGHT")
        {
            m_nextButton->Push();
        }
        else
            handled = false;
    }

    if (!handled && MythScreenType::keyPressEvent(event))
        handled = true;

    return handled;
}

bool ImportCoverArtDialog::Create()
{
    if (!LoadWindowFromXML("music-ui.xml", "import_coverart", this))
        return false;

    m_filenameText = dynamic_cast<MythUIText *>(GetChild("file"));
    m_currentText = dynamic_cast<MythUIText *>(GetChild("position"));
    m_statusText = dynamic_cast<MythUIText *>(GetChild("status"));
    m_destinationText = dynamic_cast<MythUIText *>(GetChild("destination"));

    m_coverartImage = dynamic_cast<MythUIImage *>(GetChild("coverart"));

    m_copyButton = dynamic_cast<MythUIButton *>(GetChild("copyButton"));
    if (m_copyButton)
        connect(m_copyButton, SIGNAL(Clicked()), this, SLOT(copyPressed()));

    m_exitButton = dynamic_cast<MythUIButton *>(GetChild("exit"));
    if (m_exitButton)
        connect(m_exitButton, SIGNAL(Clicked()), this, SLOT(reject()));

    m_prevButton = dynamic_cast<MythUIButton *>(GetChild("prev"));
    if (m_prevButton)
        connect(m_prevButton, SIGNAL(Clicked()), this, SLOT(prevPressed()));

    m_nextButton = dynamic_cast<MythUIButton *>(GetChild("next"));
    if (m_nextButton)
        connect(m_nextButton, SIGNAL(Clicked()), this, SLOT(nextPressed()));

    m_typeList = dynamic_cast<MythUIButtonList *>(GetChild("type"));
    if (m_typeList)
    {
        new MythUIButtonListItem(m_typeList, tr("Front Cover"),
                                 qVariantFromValue(0));
        new MythUIButtonListItem(m_typeList, tr("Back Cover"),
                                 qVariantFromValue(1));
        new MythUIButtonListItem(m_typeList, tr("CD"),
                                 qVariantFromValue(2));
        new MythUIButtonListItem(m_typeList, tr("Inlay"),
                                 qVariantFromValue(3));
        new MythUIButtonListItem(m_typeList, tr("<Unknown>"),
                                 qVariantFromValue(4));

        connect(m_typeList, SIGNAL(itemSelected(MythUIButtonListItem *)),
                SLOT(selectorChanged()));
    }

    return true;
}

void ImportCoverArtDialog::selectorChanged()
{
    updateStatus();
}

void ImportCoverArtDialog::copyPressed()
{
    if (m_filelist.size() > 0)
    {
        copyFile(m_filelist[m_currentFile], m_saveFilename);
        updateStatus();
    }
}

void ImportCoverArtDialog::prevPressed()
{
    if (m_currentFile > 0)
    {
        m_currentFile--;
        updateTypeSelector();
        updateStatus();
    }
}

void ImportCoverArtDialog::nextPressed()
{
    if (m_currentFile < (int) m_filelist.size() - 1)
    {
        m_currentFile++;
        updateTypeSelector();
        updateStatus();
    }
}

void ImportCoverArtDialog::scanDirectory()
{
    QDir d(m_sourceDir);

    if (!d.exists())
        return;

    QString nameFilter = gContext->GetSetting("AlbumArtFilter",
                                              "*.png;*.jpg;*.jpeg;*.gif;*.bmp");

    QFileInfoList list = d.entryInfoList(nameFilter);
    if (list.isEmpty())
        return;

    QFileInfoList::const_iterator it = list.begin();
    const QFileInfo *fi;

    while (it != list.end())
    {
        fi = &(*it);
        ++it;
        if (fi->fileName() == "." || fi->fileName() == "..")
            continue;
        QString filename = fi->absFilePath();
        if (!fi->isDir())
        {
            m_filelist.append(filename);
        }
    }

    m_currentFile = 0;
    updateTypeSelector();
    updateStatus();
}

void ImportCoverArtDialog::updateStatus()
{
    if (m_filelist.size() > 0)
    {
        if (m_currentText)
            m_currentText->SetText(QString("%1 of %2").arg(m_currentFile + 1)
                                                      .arg(m_filelist.size()));
        m_filenameText->SetText(m_filelist[m_currentFile]);
        m_coverartImage->SetFilename(m_filelist[m_currentFile]);
        m_coverartImage->Load();

        QString saveFilename = Ripper::filenameFromMetadata(m_metadata, false);
        QFileInfo fi(saveFilename);
        QString saveDir = fi.dirPath(true);

        fi.setFile(m_filelist[m_currentFile]);
        switch (m_typeList->GetItemCurrent()->GetData().toInt())
        {
            case 0:
                saveFilename = "front." + fi.extension(false);
                break;
            case 1:
                saveFilename = "back." + fi.extension(false);
                break;
            case 2:
                saveFilename = "cd." + fi.extension(false);
                break;
            case 3:
                saveFilename = "inlay." + fi.extension(false);
                break;
            default:
                saveFilename = fi.fileName();
        }

        m_saveFilename = saveDir + "/" + saveFilename;
        m_destinationText->SetText(m_saveFilename);

        if (QFile::exists(m_saveFilename))
            m_statusText->SetText(tr("File Already Exists"));
        else
            m_statusText->SetText(tr("New File"));
    }
    else
    {
        if (m_currentText)
            m_currentText->Reset();
        m_statusText->Reset();
        m_filenameText->Reset();
        m_coverartImage->Reset();
        m_destinationText->Reset();
    }
}

void ImportCoverArtDialog::updateTypeSelector()
{
    if (m_filelist.size() == 0)
        return;

    QString filename = m_filelist[m_currentFile];
    QFileInfo fi(filename);
    filename = fi.fileName();

    if (filename.contains("front", Qt::CaseInsensitive) > 0)
        m_typeList->SetValue(tr("Front Cover"));
    else if (filename.contains("back", Qt::CaseInsensitive) > 0)
        m_typeList->SetValue(tr("Back Cover"));
    else if (filename.contains("inlay", Qt::CaseInsensitive) > 0)
        m_typeList->SetValue(tr("Inlay"));
    else if (filename.contains("cd", Qt::CaseInsensitive) > 0)
        m_typeList->SetValue(tr("CD"));
    else
        m_typeList->SetValue(tr("<Unknown>"));
}
