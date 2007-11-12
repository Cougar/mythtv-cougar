#include <cstdlib>
// Qt
#include <qdir.h>
#include <qapplication.h>
#include <qfileinfo.h>
#include <qsqldatabase.h>

// Myth
#include <mythtv/mythcontext.h>
#include <mythtv/mythdbcon.h>
#include <mythtv/uitypes.h>

// mythmusic
#include "directoryfinder.h"

////////////////////////////////////////////////////////////////

DirectoryFinder::DirectoryFinder(const QString &startDir, 
                           MythMainWindow *parent, 
                           const char *name)
                :MythThemedDialog(parent, "directory_finder", "music-", name)
{
    m_curDirectory = startDir;
    wireUpTheme();
}

DirectoryFinder::~DirectoryFinder()
{
}

QString DirectoryFinder::getSelected(void)
{
    return m_curDirectory;
}

void DirectoryFinder::keyPressEvent(QKeyEvent *e)
{
    bool handled = false;

    QStringList actions;
    gContext->GetMainWindow()->TranslateKeyPress("Global", e, actions);

    for (unsigned int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        handled = true;

        if (action == "SELECT")
        {
            if (getCurrentFocusWidget() == m_fileList)
            {
                UIListBtnTypeItem *item = m_fileList->GetItemCurrent();
                int index = (intptr_t) item->getData();
                if (m_directoryList[index] == "..")
                {
                    // move up on directory
                    int pos = m_curDirectory.findRev('/');
                    if (pos > 0)
                        m_curDirectory = m_curDirectory.left(pos);
                    else
                        m_curDirectory = "/";
                }
                else
                {
                    if (!m_curDirectory.endsWith("/"))
                        m_curDirectory += "/";
                    m_curDirectory += m_directoryList[index];
                }

                updateFileList();
            }
            else
                activateCurrent();
        }
        else if (action == "UP")
        {
            if (getCurrentFocusWidget() == m_fileList)
            {
                m_fileList->MoveUp(UIListBtnType::MoveItem);
                m_fileList->refresh();
            }
            else
                nextPrevWidgetFocus(false);
        }
        else if (action == "DOWN")
        {
            if (getCurrentFocusWidget() == m_fileList)
            {
                m_fileList->MoveDown(UIListBtnType::MoveItem);
                m_fileList->refresh();
            }
            else
                nextPrevWidgetFocus(true);
        }
        else if (action == "LEFT")
        {
            nextPrevWidgetFocus(false);
        }
        else if (action == "RIGHT")
        {
            nextPrevWidgetFocus(true);
        }
        else if (action == "PAGEUP")
        {
            if (getCurrentFocusWidget() == m_fileList)
            {
                m_fileList->MoveUp(UIListBtnType::MovePage);
                m_fileList->refresh();
            }
        }
        else if (action == "PAGEDOWN")
        {
            if (getCurrentFocusWidget() == m_fileList)
            {
                m_fileList->MoveDown(UIListBtnType::MovePage);
                m_fileList->refresh();
            }
        }
        else
            handled = false;
    }

    if (!handled)
        MythThemedDialog::keyPressEvent(e);
}

void DirectoryFinder::wireUpTheme()
{
    m_fileList = getUIListBtnType("filelist");

    m_locationEdit = getUIRemoteEditType("location_edit");
    if (m_locationEdit)
    {
        m_locationEdit->createEdit(this);
        connect(m_locationEdit, SIGNAL(loosingFocus()), 
                this, SLOT(locationEditLostFocus()));
    }

    // ok button
    m_okButton = getUITextButtonType("ok_button");
    if (m_okButton)
    {
        m_okButton->setText(tr("OK"));
        connect(m_okButton, SIGNAL(pushed()), this, SLOT(OKPressed()));
    }

    // cancel button
    m_cancelButton = getUITextButtonType("cancel_button");
    if (m_cancelButton)
    {
        m_cancelButton->setText(tr("Cancel"));
        connect(m_cancelButton, SIGNAL(pushed()), this, SLOT(cancelPressed()));
    }

    // back button
    m_backButton = getUITextButtonType("back_button");
    if (m_backButton)
    {
        m_backButton->setText(tr("Back"));
        connect(m_backButton, SIGNAL(pushed()), this, SLOT(backPressed()));
    }

    // home button
    m_homeButton = getUITextButtonType("home_button");
    if (m_homeButton)
    {
        m_homeButton->setText(tr("Home"));
        connect(m_homeButton, SIGNAL(pushed()), this, SLOT(homePressed()));
    }

    if (!m_fileList || !m_locationEdit || !m_backButton || !m_okButton 
         || !m_cancelButton || !m_homeButton)
    {
        cout << "DirectoryFinder: Your theme is missing some UI elements! Bailing out." << endl;
        QTimer::singleShot(100, this, SLOT(done(int)));
    }

    // load pixmaps
    m_directoryPixmap = gContext->LoadScalePixmap("mm_folder.png");

    buildFocusList();
    assignFirstFocus();
    updateFileList();
}

void DirectoryFinder::locationEditLostFocus()
{
    m_curDirectory = m_locationEdit->getText();
    updateFileList();
}

void DirectoryFinder::backPressed()
{
    // move up one directory
    int pos = m_curDirectory.findRev('/');
    if (pos > 0)
        m_curDirectory = m_curDirectory.left(pos);
    else
        m_curDirectory = "/";

    updateFileList();
}

void DirectoryFinder::homePressed()
{
    char *home = getenv("HOME");
    m_curDirectory = home;

    updateFileList();
}

void DirectoryFinder::OKPressed()
{
    UIListBtnTypeItem *item = m_fileList->GetItemCurrent();
    int index  = (intptr_t) item->getData();

    if (m_directoryList[index] != "..")
    {
        if (!m_curDirectory.endsWith("/"))
            m_curDirectory += "/";
        m_curDirectory += m_directoryList[index];
    }

    done(Accepted);
}

void DirectoryFinder::cancelPressed()
{
    reject();
}

void DirectoryFinder::updateFileList()
{
    if (!m_fileList)
        return;

    m_fileList->Reset();
    m_directoryList.clear();

    QDir d;

    d.setPath(m_curDirectory);

    if (!d.exists())
    {
        cout << "DirectoryFinder: current directory does not exist!" << endl;
        m_locationEdit->setText("/");
        m_curDirectory = "/";
        d.setPath("/");
    }

    const QFileInfoList *list = d.entryInfoList("*", QDir::Dirs, QDir::Name);
    if (!list)
    {
        m_directoryList.append("..");

        // add a row to the UIListBtnArea
        UIListBtnTypeItem* item = new UIListBtnTypeItem(
                m_fileList, "..");
        item->setCheckable(false);
        item->setPixmap(m_directoryPixmap);
        item->setData((void*) 0);
    }
    else
    {
        QFileInfoListIterator it(*list);
        QFileInfo *fi;
        int index = 0;
        while ( (fi = it.current()) != 0 )
        {
            if (fi->fileName() != ".")
            {
                m_directoryList.append(fi->fileName());

                // add a row to the UIListBtnArea
                UIListBtnTypeItem* item = new UIListBtnTypeItem(
                        m_fileList, fi->fileName());
                item->setCheckable(false);
                item->setPixmap(m_directoryPixmap);
                item->setData((void*) index++);
            }
            ++it;
        }
    }

    m_locationEdit->setText(m_curDirectory);

    m_fileList->refresh();
}
