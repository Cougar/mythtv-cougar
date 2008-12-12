#include <iostream>

// qt
#include <qstring.h>

// myth
#include <mythtv/mythcontext.h>
#include <mythtv/mythdbcon.h>
#include <mythtv/mythdirs.h>

// mythbrowser
#include "bookmarkmanager.h"
#include "bookmarkeditor.h"
#include "browserdbutil.h"

#ifdef MYTHBROWSER_STANDALONE
#include "../mythbrowser/mythbrowser.h"
#endif

using namespace std;

// ---------------------------------------------------

BrowserConfig::BrowserConfig(MythScreenStack *parent, const char *name)
    : MythScreenType(parent, name),
      m_commandEdit(NULL),     m_zoomEdit(NULL),
      m_descriptionText(NULL), m_titleText(NULL),
      m_okButton(NULL),        m_cancelButton(NULL)
{
}

bool BrowserConfig::Create()
{
    bool foundtheme = false;

    // Load the theme for this screen
    foundtheme = LoadWindowFromXML("browser-ui.xml", "browserconfig", this);

    if (!foundtheme)
        return false;

    m_titleText = dynamic_cast<MythUIText *> (GetChild("title"));

    if (m_titleText)
          m_titleText->SetText(tr("MythBrowser Settings"));

    m_commandEdit = dynamic_cast<MythUITextEdit *> (GetChild("command"));
    m_zoomEdit = dynamic_cast<MythUITextEdit *> (GetChild("zoom"));

    m_descriptionText = dynamic_cast<MythUIText *> (GetChild("description"));

    m_okButton = dynamic_cast<MythUIButton *> (GetChild("ok"));
    m_cancelButton = dynamic_cast<MythUIButton *> (GetChild("cancel"));

    if (!m_commandEdit || !m_zoomEdit || !m_okButton || !m_cancelButton)
    {
        VERBOSE(VB_IMPORTANT, "Theme is missing critical theme elements.");
        return false;
    }

    m_commandEdit->SetText(gContext->GetSetting("WebBrowserCommand",
                           GetInstallPrefix() + "/bin/mythbrowser"));

    m_zoomEdit->SetText(gContext->GetSetting("WebBrowserZoomLevel", "1.4"));

    m_okButton->SetText(tr("Ok"));
    m_cancelButton->SetText(tr("Cancel"));

    connect(m_okButton, SIGNAL(Clicked()), this, SLOT(slotSave()));
    connect(m_cancelButton, SIGNAL(Clicked()), this, SLOT(Close()));

    connect(m_commandEdit,  SIGNAL(TakingFocus()), SLOT(slotFocusChanged()));
    connect(m_zoomEdit   ,  SIGNAL(TakingFocus()), SLOT(slotFocusChanged()));
    connect(m_okButton,     SIGNAL(TakingFocus()), SLOT(slotFocusChanged()));
    connect(m_cancelButton, SIGNAL(TakingFocus()), SLOT(slotFocusChanged()));

    if (!BuildFocusList())
        VERBOSE(VB_IMPORTANT, "Failed to build a focuslist. Something is wrong");

    SetFocusWidget(m_commandEdit);

    return true;
}

BrowserConfig::~BrowserConfig()
{
}

void BrowserConfig::slotSave(void)
{
    //FIXME should check the zoom level is a valid value 0.3 to 5.0
    gContext->SaveSetting("WebBrowserZoomLevel", m_zoomEdit->GetText());
    gContext->SaveSetting("WebBrowserCommand", m_commandEdit->GetText());

    Close();
}

bool BrowserConfig::keyPressEvent(QKeyEvent *event)
{
    if (GetFocusWidget()->keyPressEvent(event))
        return true;

    bool handled = false;

    if (!handled && MythScreenType::keyPressEvent(event))
        handled = true;

    return handled;
}

void BrowserConfig::slotFocusChanged(void)
{
    if (!m_descriptionText)
        return;

    QString msg = "";
    if (GetFocusWidget() == m_commandEdit)
        msg = tr("This is the command that will be used to show the web browser. "
                 "The default is to use MythBrowser.");
    else if (GetFocusWidget() == m_zoomEdit)
        msg = tr("This is the default text size that will be used. Valid values "
                 "are from 0.3 to 5.0 with 1.0 being normal size less than 1 is "
                 "smaller and greater than 1 is larger than normal size.");
    else if (GetFocusWidget() == m_cancelButton)
        msg = tr("Exit without saving settings");
    else if (GetFocusWidget() == m_okButton)
        msg = tr("Save settings and Exit");

    m_descriptionText->SetText(msg);
}

// ---------------------------------------------------

BookmarkManager::BookmarkManager(MythScreenStack *parent, const char *name)
               : MythScreenType(parent, name)
{
#ifdef MYTHBROWSER_STANDALONE
    m_browser = NULL;
#endif
    m_bookmarkList = NULL;
    m_groupList = NULL;
    m_messageText = NULL;
    m_menuPopup = NULL;
}

bool BookmarkManager::Create(void)
{
    bool foundtheme = false;

    // Load the theme for this screen
    foundtheme = LoadWindowFromXML("browser-ui.xml", "bookmarkmanager", this);

    if (!foundtheme)
        return false;

    m_groupList = dynamic_cast<MythUIButtonList *>(GetChild("grouplist"));
    m_bookmarkList = dynamic_cast<MythUIButtonList *>(GetChild("bookmarklist"));

    // optional text area warning user hasn't set any bookmarks yet
    m_messageText = dynamic_cast<MythUIText *>(GetChild("messagetext"));
    if (m_messageText)
        m_messageText->SetText(tr("No bookmarks defined.\n\n"
                "Use the 'Add Bookmark' menu option to add new bookmarks"));

    if (!m_groupList || !m_bookmarkList)
    {
        VERBOSE(VB_IMPORTANT, "Theme is missing critical theme elements.");
        return false;
    }

    m_groupList->SetActive(true);
    m_bookmarkList->SetActive(false);

    GetSiteList(m_siteList);
    UpdateGroupList();
    UpdateURLList();

    connect(m_groupList, SIGNAL(itemSelected(MythUIButtonListItem*)),
            this, SLOT(slotGroupSelected(MythUIButtonListItem*)));

    connect(m_bookmarkList, SIGNAL(itemClicked(MythUIButtonListItem*)),
            this, SLOT(slotBookmarkClicked(MythUIButtonListItem*)));

    m_groupList->SetActive(true);

    if (!BuildFocusList())
        VERBOSE(VB_IMPORTANT, "Failed to build a focuslist. Something is wrong");

    SetFocusWidget(m_groupList);

    return true;
}

BookmarkManager::~BookmarkManager()
{
    while (!m_siteList.isEmpty())
        delete m_siteList.takeFirst();
}

void BookmarkManager::UpdateGroupList(void)
{
    m_groupList->Reset();
    QStringList groups;
    for (int x = 0; x < m_siteList.count(); x++)
    {
        Bookmark *site = m_siteList.at(x);

        if (groups.indexOf(site->category) == -1)
        {
            groups.append(site->category);
            new MythUIButtonListItem(m_groupList, site->category);
        }
    }
}

void BookmarkManager::UpdateURLList(void)
{
    m_bookmarkList->Reset();

    if (m_messageText)
        m_messageText->SetVisible((m_siteList.count() == 0));

    MythUIButtonListItem *item = m_groupList->GetItemCurrent();
    if (!item)
        return;

    QString group = item->text();

    for (int x = 0; x < m_siteList.count(); x++)
    {
        Bookmark *site = m_siteList.at(x);

        if (group == site->category)
        {
            MythUIButtonListItem *item = new MythUIButtonListItem(
                    m_bookmarkList, "", "", true, MythUIButtonListItem::NotChecked);
            item->setText(site->name, "name");
            item->setText(site->url, "url");
            item->SetData(qVariantFromValue(site));
            item->setChecked(site->selected ?
                    MythUIButtonListItem::FullChecked : MythUIButtonListItem::NotChecked);
        }
    }
}

uint BookmarkManager::GetMarkedCount(void)
{
    uint count = 0;

    for (int x = 0; x < m_siteList.size(); x++)
    {
        Bookmark *site = m_siteList.at(x);
        if (site && site->selected)
            count++;
    }

    return count;
}

bool BookmarkManager::keyPressEvent(QKeyEvent *event)
{
    if (GetFocusWidget()->keyPressEvent(event))
        return true;

    bool handled = false;
    QStringList actions;
    gContext->GetMainWindow()->TranslateKeyPress("qt", event, actions);

    for (int i = 0; i < actions.size() && !handled; i++)
    {

        QString action = actions[i];
        handled = true;

        if (action == "MENU")
        {
            QString label = tr("Actions");

            MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");

            m_menuPopup = new MythDialogBox(label, popupStack, "actionmenu");

            if (!m_menuPopup->Create())
            {
                delete m_menuPopup;
                m_menuPopup = NULL;
                return true;
            }

            m_menuPopup->SetReturnEvent(this, "action");

            m_menuPopup->AddButton(tr("Add Bookmark"), SLOT(slotAddBookmark()));

            if (m_bookmarkList->GetItemCurrent())
            {
                m_menuPopup->AddButton(tr("Edit Bookmark"), SLOT(slotEditBookmark()));
                m_menuPopup->AddButton(tr("Delete Bookmark"), SLOT(slotDeleteCurrent()));
                m_menuPopup->AddButton(tr("Show bookmark"), SLOT(slotShowCurrent()));
            }

            if (GetMarkedCount() > 0)
            {
                m_menuPopup->AddButton(tr("Delete Marked"), SLOT(slotDeleteMarked()));

#ifdef MYTHBROWSER_STANDALONE
                if (m_browser)
                    m_menuPopup->AddButton(tr("Show Marked"), SLOT(slotShowMarked()));
#else
                m_menuPopup->AddButton(tr("Show Marked"), SLOT(slotShowMarked()));
#endif
                m_menuPopup->AddButton(tr("Clear Marked"), SLOT(slotClearMarked()));
            }

            m_menuPopup->AddButton(tr("Cancel"));

            popupStack->AddScreen(m_menuPopup);
        }
        else if (action == "INFO")
        {
            MythUIButtonListItem *item = m_bookmarkList->GetItemCurrent();

            if (item)
            {
                Bookmark *site = qVariantValue<Bookmark*>(item->GetData());

                if (item->state() == MythUIButtonListItem::NotChecked)
                {
                    item->setChecked(MythUIButtonListItem::FullChecked);
                    if (site)
                        site->selected = true;
                }
                else
                {
                    item->setChecked(MythUIButtonListItem::NotChecked);
                    if (site)
                        site->selected = false;
                }
            }
        }
        else if (action == "DELETE")
            slotDeleteCurrent();
        else if (action == "EDIT")
            slotEditBookmark();
        else
            handled = false;
    }

    if (!handled && MythScreenType::keyPressEvent(event))
        handled = true;

    return handled;
}

void BookmarkManager::slotGroupSelected(MythUIButtonListItem *item)
{
    (void) item;

    UpdateURLList();
    m_bookmarkList->Refresh();
}

void BookmarkManager::slotBookmarkClicked(MythUIButtonListItem *item)
{
    if (!item)
        return;

    Bookmark *site = qVariantValue<Bookmark*>(item->GetData());
    if (!site)
        return;

#ifdef MYTHBROWSER_STANDALONE
    m_browser->slotOpenURL(site->url);
    Close();
#else
    QString cmd = gContext->GetSetting("WebBrowserCommand",
                    GetInstallPrefix() + "/bin/mythbrowser");
    cmd += QString(" -z %1 ").arg(
                    gContext->GetSetting("WebBrowserZoomLevel", "1.4"));

    m_savedBookmark = *site;

    cmd += site->url;
    gContext->GetMainWindow()->AllowInput(false);
    cmd.replace("&","\\&");
    cmd.replace(";","\\;");
    myth_system(cmd, MYTH_SYSTEM_DONT_BLOCK_PARENT);
    gContext->GetMainWindow()->AllowInput(true);

    // we need to reload the bookmarks incase the user added/deleted
    // any while in MythBrowser
    ReloadBookmarks();
#endif
}

void BookmarkManager::ShowEditDialog(bool edit)
{
    Bookmark *site = NULL;

    if (edit)
    {
        MythUIButtonListItem *item = m_bookmarkList->GetItemCurrent();

        if (item && item->GetData().isValid())
        {
            site = qVariantValue<Bookmark*>(item->GetData());
            m_savedBookmark = *site;
        }
        else
        {
            VERBOSE(VB_IMPORTANT, "BookmarkManager: Something is wrong. "
                                  "Asked to edit a non existent bookmark!");
            return;
        }
    }


    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    BookmarkEditor *editor = new BookmarkEditor(&m_savedBookmark, edit, mainStack,
                                                        "bookmarkeditor");

    connect(editor, SIGNAL(Exiting()), this, SLOT(slotEditDialogExited()));

    if (editor->Create())
        mainStack->AddScreen(editor);
}

void BookmarkManager::slotEditDialogExited(void)
{
    ReloadBookmarks();
}

void BookmarkManager::ReloadBookmarks(void)
{
    GetSiteList(m_siteList);
    UpdateGroupList();

    m_groupList->MoveToNamedPosition(m_savedBookmark.category);
    UpdateURLList();

    // try to set the current item to name
    MythUIButtonListItem *item;
    for (int x = 0; x < m_bookmarkList->GetCount(); x++)
    {
        item = m_bookmarkList->GetItemAt(x);
        if (item && item->GetData().isValid())
        {
            Bookmark *site = qVariantValue<Bookmark*>(item->GetData());
            if (site && (*site == m_savedBookmark))
            {
                m_bookmarkList->SetItemCurrent(item);
                break;
            }
        }
    }
}

void BookmarkManager::slotAddBookmark(void)
{
    ShowEditDialog(false);
}

void BookmarkManager::slotEditBookmark(void)
{
    ShowEditDialog(true);
}

void BookmarkManager::slotDeleteCurrent(void)
{
    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");

    QString message = tr("Are you sure you want to delete the selected bookmark");

    MythConfirmationDialog *dialog = new MythConfirmationDialog(popupStack, message, true);

    if (dialog->Create())
        popupStack->AddScreen(dialog);

    connect(dialog, SIGNAL(haveResult(bool)),
            this, SLOT(slotDoDeleteCurrent(bool)));
}

void BookmarkManager::slotDoDeleteCurrent(bool doDelete)
{
    if (!doDelete)
        return;

    MythUIButtonListItem *item = m_bookmarkList->GetItemCurrent();
    if (item)
    {
        QString category = "";
        Bookmark *site = qVariantValue<Bookmark*>(item->GetData());
        if (site)
        {
            category = site->category;
            RemoveFromDB(site);
        }

        GetSiteList(m_siteList);
        UpdateGroupList();

        if (category != "")
            m_groupList->MoveToNamedPosition(category);

        UpdateURLList();
    }
}

void BookmarkManager::slotDeleteMarked(void)
{
    if (GetMarkedCount() == 0)
        return;

     MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");

    QString message = tr("Are you sure you want to delete the marked bookmarks");

    MythConfirmationDialog *dialog = new MythConfirmationDialog(popupStack, message, true);

    if (dialog->Create())
        popupStack->AddScreen(dialog);

    connect(dialog, SIGNAL(haveResult(bool)),
            this, SLOT(slotDoDeleteMarked(bool)));
}

void BookmarkManager::slotDoDeleteMarked(bool doDelete)
{
    if (!doDelete)
        return;

    QString category = m_groupList->GetValue();

    for (int x = 0; x < m_siteList.size(); x++)
    {
        Bookmark *site = m_siteList.at(x);
        if (site && site->selected)
            RemoveFromDB(site);
    }

    GetSiteList(m_siteList);
    UpdateGroupList();

    if (category != "")
        m_groupList->MoveToNamedPosition(category);

    UpdateURLList();
}

void BookmarkManager::slotShowCurrent(void)
{
    MythUIButtonListItem *item = m_bookmarkList->GetItemCurrent();
    if (item)
        slotBookmarkClicked(item);
}

void BookmarkManager::slotShowMarked(void)
{
    if (GetMarkedCount() == 0)
        return;

    MythUIButtonListItem *item = m_bookmarkList->GetItemCurrent();
    if (item && item->GetData().isValid())
    {
       Bookmark *site = qVariantValue<Bookmark*>(item->GetData());
       m_savedBookmark = *site;
    }

    QString cmd = gContext->GetSetting("WebBrowserCommand", GetInstallPrefix() + "/bin/mythbrowser");
    cmd += QString(" -z %1 ").arg(gContext->GetSetting("WebBrowserZoomLevel", "1.4"));

    for (int x = 0; x < m_siteList.size(); x++)
    {
        Bookmark *site = m_siteList.at(x);
        if (site && site->selected)
            cmd += " " + site->url;
    }

    cmd.replace("&","\\&");
    cmd.replace(";","\\;");

    gContext->GetMainWindow()->AllowInput(false);
    myth_system(cmd, MYTH_SYSTEM_DONT_BLOCK_PARENT);
    gContext->GetMainWindow()->AllowInput(true);

    // we need to reload the bookmarks incase the user added/deleted
    // any while in MythBrowser
    ReloadBookmarks();
}

void BookmarkManager::slotClearMarked(void)
{
    for (int x = 0; x < m_bookmarkList->GetCount(); x++)
    {
        MythUIButtonListItem *item = m_bookmarkList->GetItemAt(x);
        if (item)
        {
            item->setChecked(MythUIButtonListItem::NotChecked);

            Bookmark *site = qVariantValue<Bookmark*>(item->GetData());
            if (site)
                site->selected = false;
        }
    }
}
