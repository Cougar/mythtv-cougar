#include <cstdlib>

// Qt
#include <QApplication>
#include <QDir>
#include <QFileInfo>

// Myth
#include <mythtv/mythcontext.h>
#include <mythtv/mythdb.h>
#include <mythtv/uitypes.h>
#include <mythtv/libmythui/mythuihelper.h>
#include <libmythui/mythuitext.h>
#include <libmythui/mythuitextedit.h>
#include <libmythui/mythuibutton.h>
#include <libmythui/mythuiimage.h>
#include <libmythui/mythuibuttonlist.h>
#include <libmythui/mythmainwindow.h>
#include <libmythui/mythdialogbox.h>

// mytharchive
#include "fileselector.h"
#include "archiveutil.h"

////////////////////////////////////////////////////////////////

FileSelector::FileSelector(MythScreenStack *parent, QList<ArchiveItem *> *archiveList,
                           FSTYPE type, const QString &startDir, const QString &filemask)
             :MythScreenType(parent, "FileSelector")
{
    m_archiveList = archiveList;
    m_selectorType = type;
    m_filemask = filemask;
    m_curDirectory = startDir;
}

FileSelector::~FileSelector()
{
    while (!m_fileData.isEmpty())
        delete m_fileData.takeFirst();
    m_fileData.clear();
}

bool FileSelector::Create(void)
{
    bool foundtheme = false;

    // Load the theme for this screen
    foundtheme = LoadWindowFromXML("mytharchive-ui.xml", "file_selector", this);

    if (!foundtheme)
        return false;

    try
    {
        m_titleText = GetMythUIText("title_text", true);
        m_fileButtonList = GetMythUIButtonList("filelist");
        m_locationEdit = GetMythUITextEdit("location_edit");
        m_backButton = GetMythUIButton("back_button");
        m_homeButton = GetMythUIButton("home_button");
        m_okButton = GetMythUIButton("ok_button");
        m_cancelButton = GetMythUIButton("cancel_button");
    }
    catch (QString &error)
    {
        VERBOSE(VB_IMPORTANT, "Cannot load screen 'file_selector'\n\t\t\t"
                              "Error was: " + error);
        return false;
    }

    if (m_titleText)
    {
        switch (m_selectorType)
        {
            case FSTYPE_FILE:
                m_titleText->SetText(tr("Find File"));
                break;
            case FSTYPE_DIRECTORY:
                m_titleText->SetText(tr("Find Directory"));
                break;
            default:
                m_titleText->SetText(tr("Find Files"));
                break;
        }
    }

    m_okButton->SetText(tr("OK"));
    connect(m_okButton, SIGNAL(Clicked()), this, SLOT(OKPressed()));

    m_cancelButton->SetText(tr("Cancel"));
    connect(m_cancelButton, SIGNAL(Clicked()), this, SLOT(cancelPressed()));

    connect(m_locationEdit, SIGNAL(LosingFocus()),
            this, SLOT(locationEditLostFocus()));
    m_locationEdit->SetText(m_curDirectory);

    m_backButton->SetText(tr("Back"));
    connect(m_backButton, SIGNAL(Clicked()), this, SLOT(backPressed()));

    m_homeButton->SetText(tr("Home"));
    connect(m_homeButton, SIGNAL(Clicked()), this, SLOT(homePressed()));

    connect(m_fileButtonList, SIGNAL(itemClicked(MythUIButtonListItem *)),
            this, SLOT(itemClicked(MythUIButtonListItem *)));

    if (!BuildFocusList())
        VERBOSE(VB_IMPORTANT, "Failed to build a focuslist. Something is wrong");

    SetFocusWidget(m_fileButtonList);

    updateSelectedList();
    updateFileList();

    return true;
}

bool FileSelector::keyPressEvent(QKeyEvent *event)
{
    if (GetFocusWidget()->keyPressEvent(event))
        return true;

    bool handled = false;
    QStringList actions;
    gContext->GetMainWindow()->TranslateKeyPress("Global", event, actions);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        handled = true;

        if (action == "MENU")
        {
            
        }
        else
            handled = false;
    }

    if (!handled && MythScreenType::keyPressEvent(event))
        handled = true;

    return handled;
}

void FileSelector::itemClicked(MythUIButtonListItem *item)
{
    if (!item)
        return;

    FileData *fileData = (FileData*)item->getData();

    if (fileData->directory)
    {
        if (fileData->filename == "..")
        {
            // move up on directory
            int pos = m_curDirectory.lastIndexOf('/');
            if (pos > 0)
                m_curDirectory = m_curDirectory.left(pos);
            else
                m_curDirectory = "/";
        }
        else
        {
            if (!m_curDirectory.endsWith("/"))
                m_curDirectory += "/";
            m_curDirectory += fileData->filename;
        }
        updateFileList();
    }
    else
    {
        if (m_selectorType == FSTYPE_FILELIST)
        {
            QString fullPath = m_curDirectory;
            if (!fullPath.endsWith("/"))
                fullPath += "/";
            fullPath += fileData->filename;

            if (item->state() == MythUIButtonListItem::FullChecked)
            {
                m_selectedList.removeAll(fullPath);
                item->setChecked(MythUIButtonListItem::NotChecked);
            }
            else
            {
                if (m_selectedList.indexOf(fullPath) == -1)
                    m_selectedList.append(fullPath);
                item->setChecked(MythUIButtonListItem::FullChecked);
            }
        }
    }
}

QString FileSelector::getSelected(void)
{
    return m_curDirectory;
}

void FileSelector::locationEditLostFocus()
{
    m_curDirectory = m_locationEdit->GetText();
    updateFileList();
}

void FileSelector::backPressed()
{
    // move up one directory
    int pos = m_curDirectory.lastIndexOf('/');
    if (pos > 0)
        m_curDirectory = m_curDirectory.left(pos);
    else
        m_curDirectory = "/";

    updateFileList();
}

void FileSelector::homePressed()
{
    char *home = getenv("HOME");
    m_curDirectory = home;

    updateFileList();
}

void FileSelector::OKPressed()
{
    if (m_selectorType == FSTYPE_FILELIST && m_archiveList)
    {
        // loop though selected files and add them to the list
        QString f;
        ArchiveItem *a;

        // remove any items that have been removed from the list
        QList<ArchiveItem *> tempAList;
        for (int x = 0; x < m_archiveList->size(); x++)
        {
            a = m_archiveList->at(x);
            bool found = false;

            for (int y = 0; y < m_selectedList.size(); y++)
            {
                f = m_selectedList.at(y);
                if (a->type != "File" || a->filename == f)
                {
                    found = true;
                    break;
                }
            }

            if (!found)
                tempAList.append(a);
        }

        for (int x = 0; x < tempAList.size(); x++)
            m_archiveList->removeAll(tempAList.at(x));

        // remove any items that are already in the list
        QStringList tempSList;
        for (int x = 0; x < m_selectedList.size(); x++)
        {
            f = m_selectedList.at(x);

            for (int y = 0; y < m_archiveList->size(); y++)
            {
                a = m_archiveList->at(y);
                if (a->filename == f)
                {
                    tempSList.append(f);
                    break;
                }
            }
        }

        for (int x = 0; x < tempSList.size(); x++)
            m_selectedList.removeAll(tempSList.at(x));

        // add all that are left
        for (int x = 0; x < m_selectedList.size(); x++)
        {
            f = m_selectedList.at(x);

            QFile file(f);
            if (file.exists())
            {
                QString title = f;
                int pos = f.lastIndexOf('/');
                if (pos > 0)
                    title = f.mid(pos + 1);

                a = new ArchiveItem;
                a->type = "File";
                a->title = title;
                a->subtitle = "";
                a->description = "";
                a->startDate = "";
                a->startTime = "";
                a->size = (long long)file.size();
                a->filename = f;
                a->hasCutlist = false;
                a->useCutlist = false;
                a->duration = 0;
                a->cutDuration = 0;
                a->videoWidth = 0;
                a->videoHeight = 0;
                a->fileCodec = "";
                a->videoCodec = "";
                a->encoderProfile = NULL;
                a->editedDetails = false;
                m_archiveList->append(a);
            }
        }
    }
    else
    {
        MythUIButtonListItem *item = m_fileButtonList->GetItemCurrent();
        FileData *fileData = (FileData*)item->getData();

        if (m_selectorType == FSTYPE_DIRECTORY)
        {
            if (!fileData->directory)
            {
                ShowOkPopup(tr("The selected item is not a directory!"));
                return;
            }

            if (fileData->filename != "..")
            {
                if (!m_curDirectory.endsWith("/"))
                    m_curDirectory += "/";
                m_curDirectory += fileData->filename;
            }
        }
        else
        {
            if (fileData->directory)
            {
                if (!m_curDirectory.endsWith("/"))
                    m_curDirectory += "/";
            }
            else
            {
                if (!m_curDirectory.endsWith("/"))
                    m_curDirectory += "/";
                m_curDirectory += fileData->filename;
            }
        }
    }

    if (m_selectorType == FSTYPE_FILELIST)
        emit haveResult(true);
    else
        emit haveResult(getSelected());
    Close();
}

void FileSelector::cancelPressed()
{
    if (m_selectorType == FSTYPE_FILELIST)
        emit haveResult(true);
    else
        emit haveResult("");
    Close();
}

void FileSelector::updateSelectedList()
{
    if (!m_archiveList)
        return;

    while (!m_selectedList.isEmpty())
        m_selectedList.takeFirst();
    m_selectedList.clear();

    FileData *f;
    ArchiveItem *a;
    for (int x = 0; x < m_archiveList->size(); x++)
    {
        a = m_archiveList->at(x);
        for (int y = 0; y < m_fileData.size(); y++)
        {
            f = m_fileData.at(y);
            if (f->filename == a->filename)
            {
                if (m_selectedList.indexOf(f->filename) == -1)
                    m_selectedList.append(f->filename);
                break;
            }
        }
    }
}

void FileSelector::updateFileList()
{
    if (!m_fileButtonList)
        return;

    m_fileButtonList->Reset();
    while (!m_fileData.isEmpty())
         delete m_fileData.takeFirst();
    m_fileData.clear();

    QDir d;

    d.setPath(m_curDirectory);
    if (d.exists())
    {
        // first get a list of directory's in the current directory
        QStringList filters;
        filters << "*";
        QFileInfoList list = d.entryInfoList(filters, QDir::Dirs, QDir::Name);
        QFileInfo fi;

        for (int x = 0; x < list.size(); x++)
        {
            fi = list.at(x);
            if (fi.fileName() != ".")
            {
                FileData  *data = new FileData;
                data->selected = false;
                data->directory = true;
                data->filename = fi.fileName();
                data->size = 0;
                m_fileData.append(data);

                // add a row to the MythUIButtonList
                MythUIButtonListItem* item = new
                        MythUIButtonListItem(m_fileButtonList, data->filename);
                item->setCheckable(false);
                item->SetImage("ma_folder.png");
                item->setData(data);
            }
        }

        if (m_selectorType != FSTYPE_DIRECTORY)
        {
            // second get a list of file's in the current directory
            filters.clear();
            filters = m_filemask.split(" ", QString::SkipEmptyParts);
            list = d.entryInfoList(filters, QDir::Files, QDir::Name);
            for (int x = 0; x < list.size(); x++)
            {
                fi = list.at(x);
                FileData  *data = new FileData;
                data->selected = false;
                data->directory = false;
                data->filename = fi.fileName();
                data->size = fi.size();
                m_fileData.append(data);

                // add a row to the UIListBtnArea
                MythUIButtonListItem* item = new MythUIButtonListItem(
                        m_fileButtonList,
                        data->filename + " (" + formatSize(data->size / 1024, 2) + ")");

                if (m_selectorType == FSTYPE_FILELIST)
                {
                    item->setCheckable(true);

                    QString fullPath = m_curDirectory;
                    if (!fullPath.endsWith("/"))
                        fullPath += "/";
                    fullPath += data->filename;

                    if (m_selectedList.indexOf(fullPath) != -1)
                    {
                        item->setChecked(MythUIButtonListItem::FullChecked);
                    }
                    else
                    {
                        item->setChecked(MythUIButtonListItem::NotChecked);
                    }
                }
                else
                    item->setCheckable(false);

                item->setData(data);
            }
        }
        m_locationEdit->SetText(m_curDirectory);
    }
    else
    {
        m_locationEdit->SetText("/");
        VERBOSE(VB_IMPORTANT, "MythArchive:  current directory does not exist!");
    }
}
