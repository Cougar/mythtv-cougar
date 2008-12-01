// c
#include <cstdlib>
#include <stdlib.h>
#include <unistd.h>

// qt
#include <QDir>
#include <QTimer>

// mythtv
#include <mythtv/mythcontext.h>
#include <mythtv/dialogbox.h>
#include <mythtv/libmythdb/mythdb.h>
#include <libmythui/mythuitext.h>
#include <libmythui/mythuibutton.h>
#include <libmythui/mythuiimage.h>
#include <libmythui/mythuibuttonlist.h>
#include <libmythui/mythmainwindow.h>
#include <libmythui/mythdialogbox.h>

// mytharchive
#include "videoselector.h"
#include "archiveutil.h"

using namespace std;

VideoSelector::VideoSelector(MythScreenStack *parent, QList<ArchiveItem *> *archiveList)
              :MythScreenType(parent, "VideoSelector")
{
    m_archiveList = archiveList;
    m_currentParentalLevel = 1;
    m_videoList = NULL;
}

VideoSelector::~VideoSelector(void)
{
    if (m_videoList)
        delete m_videoList;

    while (!m_selectedList.isEmpty())
         delete m_selectedList.takeFirst();
    m_selectedList.clear();
}

bool VideoSelector::Create(void)
{
    bool foundtheme = false;

    // Load the theme for this screen
    foundtheme = LoadWindowFromXML("mytharchive-ui.xml", "video_selector", this);

    if (!foundtheme)
        return false;

    try
    {
        m_okButton = GetMythUIButton("ok_button");
        m_cancelButton = GetMythUIButton("cancel_button");
        m_categorySelector = GetMythUIButtonList("category_selector");
        m_videoButtonList = GetMythUIButtonList("videolist");
        m_titleText = GetMythUIText("videotitle");
        m_plotText = GetMythUIText("videoplot");
        m_filesizeText = GetMythUIText("filesize");
        m_coverImage = GetMythUIImage("cover_image");
        m_warningText = GetMythUIText("warning_text");
        m_plText = GetMythUIText("parentallevel_text");
    }
    catch (QString &error)
    {
        VERBOSE(VB_IMPORTANT, "Cannot load screen 'video_selector'\n\t\t\t"
                              "Error was: " + error);
        return false;
    }

    m_okButton->SetText(tr("OK"));
    connect(m_okButton, SIGNAL(Clicked()), this, SLOT(OKPressed()));

    m_cancelButton->SetText(tr("Cancel"));
    connect(m_cancelButton, SIGNAL(Clicked()), this, SLOT(cancelPressed()));

    connect(m_categorySelector, SIGNAL(itemSelected(MythUIButtonListItem *)),
            this, SLOT(setCategory(MythUIButtonListItem *)));

    getVideoList();
    connect(m_videoButtonList, SIGNAL(itemSelected(MythUIButtonListItem *)),
            this, SLOT(titleChanged(MythUIButtonListItem *)));
    connect(m_videoButtonList, SIGNAL(itemClicked(MythUIButtonListItem *)),
            this, SLOT(toggleSelected(MythUIButtonListItem *)));

    if (!BuildFocusList())
        VERBOSE(VB_IMPORTANT, "Failed to build a focuslist. Something is wrong");

    SetFocusWidget(m_videoButtonList);

    updateSelectedList();
    updateVideoList();

    return true;
}

bool VideoSelector::keyPressEvent(QKeyEvent *event)
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
            showMenu();
        }
        else if (action == "1")
        {
            setParentalLevel(1);
        }
        else if (action == "2")
        {
            setParentalLevel(2);
        }
        else if (action == "3")
        {
            setParentalLevel(3);
        }
        else if (action == "4")
        {
            setParentalLevel(4);
        }
        else
            handled = false;
    }

    if (!handled && MythScreenType::keyPressEvent(event))
        handled = true;

    return handled;
}

void VideoSelector::showMenu()
{
    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");

    MythDialogBox *menuPopup = new MythDialogBox("Menu", popupStack, "actionmenu");

    if (menuPopup->Create())
        popupStack->AddScreen(menuPopup);

    menuPopup->SetReturnEvent(this, "action");

    menuPopup->AddButton(tr("Clear All"), SLOT(clearAll()));
    menuPopup->AddButton(tr("Select All"), SLOT(selectAll()));
    menuPopup->AddButton(tr("Cancel"), NULL);
}

void VideoSelector::selectAll()
{
    while (!m_selectedList.isEmpty())
         m_selectedList.takeFirst();
    m_selectedList.clear();

    VideoInfo *v;
    vector<VideoInfo *>::iterator i = m_videoList->begin();
    for ( ; i != m_videoList->end(); i++)
    {
        v = *i;
        m_selectedList.append(v);
    }

    updateVideoList();
}

void VideoSelector::clearAll()
{
    while (!m_selectedList.isEmpty())
         m_selectedList.takeFirst();
    m_selectedList.clear();

    updateVideoList();
}

void VideoSelector::toggleSelected(MythUIButtonListItem *item)
{
    if (item->state() == MythUIButtonListItem::FullChecked)
    {
        int index = m_selectedList.indexOf((VideoInfo *) item->getData());
        if (index != -1)
            m_selectedList.takeAt(index);
        item->setChecked(MythUIButtonListItem::NotChecked);
    }
    else
    {
        int index = m_selectedList.indexOf((VideoInfo *) item->getData());
        if (index == -1)
            m_selectedList.append((VideoInfo *) item->getData());

        item->setChecked(MythUIButtonListItem::FullChecked);
    }
}

void VideoSelector::titleChanged(MythUIButtonListItem *item)
{
    VideoInfo *v;

    v = (VideoInfo *) item->getData();

    if (!v)
        return;

    if (m_titleText)
        m_titleText->SetText(v->title);

    if (m_plotText)
        m_plotText->SetText(v->plot);

    if (m_coverImage)
    {
        if (v->coverfile != "" && v->coverfile != "No Cover")
        {
            m_coverImage->SetFilename(v->coverfile);
            m_coverImage->Load();
        }
        else
        {
            m_coverImage->SetFilename("blank.png");
            m_coverImage->Load();
        }
    }

    if (m_filesizeText)
    {
        if (v->size == 0)
        {
            QFile file(v->filename);
            if (file.exists())
                v->size = (unsigned long long)file.size();
            else
                VERBOSE(VB_IMPORTANT,
                        QString("VideoSelector: Cannot find file: %1")
                                .arg(v->filename.toLocal8Bit().constData()));
        }

        m_filesizeText->SetText(formatSize(v->size / 1024));
    }
}

void VideoSelector::OKPressed()
{
    // loop though selected videos and add them to the list
    VideoInfo *v;
    ArchiveItem *a;

    // remove any items that have been removed from the list
    QList<ArchiveItem *> tempAList;
    for (int x = 0; x < m_archiveList->size(); x++)
    {
        a = m_archiveList->at(x);
        bool found = false;

        for (int y = 0; y < m_selectedList.size(); y++)
        {
            v = m_selectedList.at(y);
            if (a->type != "Video" || a->filename == v->filename)
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
    QList<VideoInfo *> tempSList;
    for (int x = 0; x < m_selectedList.size(); x++)
    {
        v = m_selectedList.at(x);

        for (int y = 0; y < m_archiveList->size(); y++)
        {
            a = m_archiveList->at(y);
            if (a->filename == v->filename)
            {
                tempSList.append(v);
                break;
            }
        }
    }

    for (int x = 0; x < tempSList.size(); x++)
        m_selectedList.removeAll(tempSList.at(x));

    // add all that are left
    for (int x = 0; x < m_selectedList.size(); x++)
    {
        v = m_selectedList.at(x);
        a = new ArchiveItem;
        a->type = "Video";
        a->title = v->title;
        a->subtitle = "";
        a->description = v->plot;
        a->startDate = "";
        a->startTime = "";
        a->size = v->size;
        a->filename = v->filename;
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

    emit haveResult(true);
    Close();
}

void VideoSelector::cancelPressed()
{
    emit haveResult(false);
    Close();
}

void VideoSelector::updateVideoList(void)
{
    if (!m_videoList)
        return;

    m_videoButtonList->Reset();

    if (m_categorySelector)
    {
        VideoInfo *v;
        vector<VideoInfo *>::iterator i = m_videoList->begin();
        for ( ; i != m_videoList->end(); i++)
        {
            v = *i;

            if (v->category == m_categorySelector->GetValue() ||
                m_categorySelector->GetValue() == tr("All Videos"))
            {
                if (v->parentalLevel <= m_currentParentalLevel)
                {
                    MythUIButtonListItem* item = new MythUIButtonListItem(
                            m_videoButtonList, v->title);
                    item->setCheckable(true);
                    if (m_selectedList.indexOf((VideoInfo *) v) != -1)
                    {
                        item->setChecked(MythUIButtonListItem::FullChecked);
                    }
                    else
                    {
                        item->setChecked(MythUIButtonListItem::NotChecked);
                    }

                    item->setData(v);
                }
            }
        }
    }

    if (m_videoButtonList->GetCount() > 0)
    {
        m_videoButtonList->SetItemCurrent(m_videoButtonList->GetItemFirst());
        titleChanged(m_videoButtonList->GetItemCurrent());
        m_warningText->Hide();
    }
    else
    {
        m_warningText->Show();
        m_titleText->SetText("");
        m_plotText->SetText("");
        m_coverImage->SetFilename("blank.png");
        m_coverImage->Load();
        m_filesizeText->SetText("");
    }
}

vector<VideoInfo *> *VideoSelector::getVideoListFromDB(void)
{
    // get a list of category's
    typedef QMap<int, QString> CategoryMap;
    CategoryMap categoryMap;
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT intid, category FROM videocategory");
    query.exec();
    if (query.isActive() && query.size())
    {
        while (query.next())
        {
            int id = query.value(0).toInt();
            QString category = query.value(1).toString();
            categoryMap.insert(id, category);
        }
    }

    vector<VideoInfo*> *videoList = new vector<VideoInfo*>;

    query.prepare("SELECT intid, title, plot, length, filename, coverfile, "
                  "category, showlevel "
                  "FROM videometadata ORDER BY title");
    query.exec();
    if (query.isActive() && query.size())
    {
        QString artist, genre;
        while (query.next())
        {
            VideoInfo *info = new VideoInfo;

            info->id = query.value(0).toInt();
            info->title = query.value(1).toString();
            info->plot = query.value(2).toString();
            info->size = 0; //query.value(3).toInt();
            info->filename = query.value(4).toString();
            info->coverfile = query.value(5).toString();
            info->category = categoryMap[query.value(6).toInt()];
            info->parentalLevel = query.value(7).toInt();
            if (info->category == "")
                info->category = "(None)";
            videoList->push_back(info);
        }
    }
    else
    {
        VERBOSE(VB_IMPORTANT, "VideoSelector: Failed to get any video's");
        return NULL;
    }

    return videoList;
}

void VideoSelector::getVideoList(void)
{
    VideoInfo *v;
    m_videoList = getVideoListFromDB();
    QStringList categories;

    if (m_videoList && m_videoList->size() > 0)
    {
        vector<VideoInfo *>::iterator i = m_videoList->begin();
        for ( ; i != m_videoList->end(); i++)
        {
            v = *i;

            if (categories.indexOf(v->category) == -1)
                categories.append(v->category);
        }
    }
    else
    {
        QTimer::singleShot(100, this, SLOT(cancelPressed()));
        return;
    }

    // sort and add categories to selector
    categories.sort();

    if (m_categorySelector)
    {
        new MythUIButtonListItem(m_categorySelector, tr("All Videos"));
        m_categorySelector->SetItemCurrent(0);

        for (int x = 0; x < categories.count(); x++)
        {
            new MythUIButtonListItem(m_categorySelector, categories[x]);
        }
    }

    setCategory(0);
}

void VideoSelector::setCategory(MythUIButtonListItem *item)
{
    (void)item;
    updateVideoList();
}

void VideoSelector::updateSelectedList()
{
    if (!m_videoList)
        return;

    m_selectedList.clear();

    ArchiveItem *a;
    VideoInfo *v;
    for (int x = 0; x < m_archiveList->size(); x++)
    {
        a = m_archiveList->at(x);
        for (uint y = 0; y < m_videoList->size(); y++)
        {
            v = m_videoList->at(y);
            if (v->filename == a->filename)
            {
                if (m_selectedList.indexOf(v) == -1)
                    m_selectedList.append(v);
                break;
            }
        }
    }
}

void VideoSelector::setParentalLevel(int which_level)
{
    if (which_level < 1)
        which_level = 1;

    if (which_level > 4)
        which_level = 4;

    if ((which_level > m_currentParentalLevel) && !checkParentPassword())
        which_level = m_currentParentalLevel;


    if (m_currentParentalLevel != which_level)
    {
        m_currentParentalLevel = which_level;
        updateVideoList();
        m_plText->SetText(QString::number(which_level));
    }
}

bool VideoSelector::checkParentPassword()
{
    QDateTime curr_time = QDateTime::currentDateTime();
    QString last_time_stamp = gContext->GetSetting("VideoPasswordTime");
    QString password = gContext->GetSetting("VideoAdminPassword");
    if (password.length() < 1)
    {
        return true;
    }

    //  See if we recently (and succesfully) asked for a password
    if (last_time_stamp.length() < 1)
    {
        //  Probably first time used
    }
    else
    {
        QDateTime last_time = QDateTime::fromString(last_time_stamp,
                                                    Qt::TextDate);
        if (last_time.secsTo(curr_time) < 120)
        {
            //  Two minute window
            last_time_stamp = curr_time.toString(Qt::TextDate);
            gContext->SetSetting("VideoPasswordTime", last_time_stamp);
            gContext->SaveSetting("VideoPasswordTime", last_time_stamp);
            return true;
        }
    }

    //  See if there is a password set
    if (password.length() > 0)
    {
        bool ok = false;
        MythPasswordDialog *pwd = new MythPasswordDialog(tr("Parental Pin:"),
                &ok,
                password,
                gContext->GetMainWindow());
        pwd->exec();
        pwd->deleteLater();
        if (ok)
        {
            //  All is good
            last_time_stamp = curr_time.toString(Qt::TextDate);
            gContext->SetSetting("VideoPasswordTime", last_time_stamp);
            gContext->SaveSetting("VideoPasswordTime", last_time_stamp);
            return true;
        }
    }
    else
    {
        return true;
    }

    return false;
}

