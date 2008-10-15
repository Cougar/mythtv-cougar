#include <stdlib.h>
#include <iostream>

// qt
#include <QEvent>

// myth
#include "mythverbose.h"
#include "mythcontext.h"
#include "libmythui/mythmainwindow.h"

// mythbrowser
#include "webpage.h"

#include "bookmarkeditor.h"
#include "mythbrowser.h"

using namespace std;

MythBrowser::MythBrowser (MythScreenStack *parent, const char *name,
                  QStringList &urlList, float zoom)
    : MythScreenType (parent, name),
      m_urlList(urlList),  m_pageList(NULL),
      m_progressBar(NULL), m_titleText(NULL),
      m_statusText(NULL),  m_currentBrowser(-1),
      m_zoom(zoom),        m_menuPopup(NULL)
{
}

MythBrowser::~MythBrowser()
{
    while (!m_browserList.isEmpty())
        delete m_browserList.takeFirst();
}

bool MythBrowser::Create(void)
{
    bool foundtheme = false;

    // Load the theme for this screen
    foundtheme = LoadWindowFromXML("browser-ui.xml", "browser", this);

    if (!foundtheme)
        return false;

    MythUIWebBrowser *browser = dynamic_cast<MythUIWebBrowser *> (GetChild("webbrowser"));
    m_progressBar = dynamic_cast<MythUIProgressBar *>(GetChild("progressbar"));
    m_statusText = dynamic_cast<MythUIText *>(GetChild("status"));
    m_titleText = dynamic_cast<MythUIText *>(GetChild("title"));
    m_pageList = dynamic_cast<MythUIButtonList *>(GetChild("pagelist"));

    if (!browser || !m_pageList)
    {
        VERBOSE(VB_IMPORTANT, "Theme is missing critical theme elements.");
        return false;
    }

    connect(m_pageList, SIGNAL(itemSelected(MythUIButtonListItem*)),
            this, SLOT(slotTabSelected(MythUIButtonListItem*)));

    // this is the template for all other browser tabs
    WebPage *page = new WebPage(this, browser);

    m_browserList.append(page);
    page->getBrowser()->SetZoom(m_zoom);
    page->SetActive(true);

    connect(page, SIGNAL(loadProgress(int)),
            this, SLOT(slotLoadProgress(int)));
    connect(page, SIGNAL(statusBarMessage(const QString&)),
            this, SLOT(slotStatusBarMessage(const QString&)));

    m_pageList->SetActive(false);

    if (m_progressBar)
        m_progressBar->SetTotal(100);

    if (!BuildFocusList())
        VERBOSE(VB_IMPORTANT, "Failed to build a focuslist. Something is wrong");

    SetFocusWidget(browser);

    slotOpenURL(m_urlList[0]);

    for (int x = 1; x < m_urlList.size(); x++)
        slotAddTab(m_urlList[x], false);

    switchTab(0);

    return true;
}

MythUIWebBrowser* MythBrowser::activeBrowser(void)
{
    if (m_currentBrowser >=0 && m_currentBrowser < m_browserList.size())
        return m_browserList[m_currentBrowser]->getBrowser();
    else
        return m_browserList[0]->getBrowser();
}

void MythBrowser::slotEnterURL(void)
{
    activeBrowser()->SetActive(false);

    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");

    QString message = tr("Enter URL");


    MythTextInputDialog *dialog = new MythTextInputDialog(popupStack, message);

    if (dialog->Create())
       popupStack->AddScreen(dialog);

     connect(dialog, SIGNAL(haveResult(QString)),
            SLOT(slotOpenURL(QString)), Qt::QueuedConnection);

     connect(dialog, SIGNAL(Exiting()), SLOT(slotExitingMenu()));
}

void MythBrowser::slotAddTab(const QString &url, bool doSwitch)
{
    QString name = QString("browser%1").arg(m_browserList.size() + 1);
    WebPage *page = new WebPage(this, m_browserList[0]->getBrowser()->GetArea(), name);
    page->getBrowser()->SetZoom(m_zoom);

    if (url != "")
    {
        QString newUrl = url;
        if (!url.startsWith("http://") && !url.startsWith("https://") &&
                !url.startsWith("file:/") )
            newUrl.prepend("http://");
        page->getBrowser()->LoadPage(newUrl);
    }

    page->SetActive(false);

    connect(page, SIGNAL(loadProgress(int)),
            this, SLOT(slotLoadProgress(int)));
    connect(page, SIGNAL(statusBarMessage(const QString&)),
            this, SLOT(slotStatusBarMessage(const QString&)));

    m_browserList.append(page);

    if (doSwitch)
        m_pageList->SetItemCurrent(m_browserList.size() -1);
}

void MythBrowser::slotDeleteTab(void)
{
    if (m_browserList.size() < 2)
        return;

    if (m_currentBrowser >= 0 && m_currentBrowser < m_browserList.size())
    {
        int tab = m_currentBrowser;
        m_currentBrowser = -1;
        WebPage *page = m_browserList.takeAt(tab);
        delete page;

        if (tab >= m_browserList.size())
            tab = m_browserList.size() - 1;

        switchTab(tab);
    }
}

void MythBrowser::switchTab(int newTab)
{
    if (newTab == m_currentBrowser)
        return;

    if (newTab < 0 || newTab >= m_browserList.size())
        return;

    if (m_currentBrowser >= 0 && m_currentBrowser < m_browserList.size())
        m_browserList[m_currentBrowser]->SetActive(false);

    m_browserList[newTab]->SetActive(true);

    m_currentBrowser = newTab;

    if (GetFocusWidget() != m_pageList)
        SetFocusWidget(activeBrowser());
}

void MythBrowser::slotOpenURL(const QString &url)
{
    QString sUrl = url;
    if (!sUrl.startsWith("http://") && !sUrl.startsWith("https://") &&
            !sUrl.startsWith("file:/") )
        sUrl.prepend("http://");

    activeBrowser()->LoadPage(QUrl(sUrl));
}

void MythBrowser::slotZoomOut()
{
    activeBrowser()->ZoomOut();
}

void MythBrowser::slotZoomIn()
{
    activeBrowser()->ZoomIn();
}

void MythBrowser::slotBack()
{
    activeBrowser()->Back();
}

void MythBrowser::slotForward()
{
    activeBrowser()->Forward();
}

void MythBrowser::slotShowBookmarks()
{
    activeBrowser()->SetActive(false);

    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    BookmarkManager *manager = new BookmarkManager(mainStack, "bookmarkmanager");

    if (manager->Create())
        mainStack->AddScreen(manager);

    manager->SetBrowser(this);

    connect(manager, SIGNAL(Exiting()), SLOT(slotExitingMenu()));
}

void MythBrowser::slotAddBookmark()
{
    activeBrowser()->SetActive(false);

    m_editBookmark.category = "";
    m_editBookmark.name = m_pageList->GetValue();
    m_editBookmark.url = activeBrowser()->GetUrl().toString();

    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    BookmarkEditor *editor = new BookmarkEditor(&m_editBookmark,
            true, mainStack, "bookmarkeditor");


    if (editor->Create())
        mainStack->AddScreen(editor);

    connect(editor, SIGNAL(Exiting()), SLOT(slotExitingMenu()));
}

void MythBrowser::slotLoadStarted(void)
{
    MythUIButtonListItem *item = m_pageList->GetItemCurrent();
    if (item)
    {
        item->setText(tr("Loading..."));
        m_pageList->Update();
    }
}

void MythBrowser::slotLoadFinished(bool OK)
{
    (void) OK;

    if (m_progressBar)
        m_progressBar->SetUsed(0);

    slotIconChanged();
}

void MythBrowser::slotLoadProgress(int progress)
{
    if (m_progressBar)
        m_progressBar->SetUsed(progress);
}

void MythBrowser::slotTitleChanged(const QString &title)
{
    MythUIButtonListItem *item = m_pageList->GetItemCurrent();
    if (item)
    {
        item->setText(title);
        m_pageList->Update();
    }
}

void MythBrowser::slotIconChanged(void)
{
    MythUIButtonListItem *item = m_pageList->GetItemCurrent();
    if (!item)
        return;

    QIcon icon = activeBrowser()->GetIcon();

    if (icon.isNull())
    {
        //FIXME use a default icon here
        item->setImage(NULL);
    }
    else
    {
        if (item)
        {
            QPixmap pixmap = icon.pixmap(32, 32);
            QImage image(pixmap);
            image = image.smoothScale(32,32);
            MythImage *mimage = GetMythPainter()->GetFormatImage();
            mimage->Assign(image);

            item->setImage(mimage);
        }
    }

    m_pageList->Update();
}

void MythBrowser::slotStatusBarMessage(const QString &text)
{
    if (m_statusText)
        m_statusText->SetText(text);
}

void MythBrowser::slotTabSelected(MythUIButtonListItem *item)
{
    if (!item)
        return;

    switchTab(m_pageList->GetCurrentPos());
    slotStatusBarMessage(item->text());
}

void MythBrowser::slotTabLosingFocus(void)
{
    SetFocusWidget(activeBrowser());
}

bool MythBrowser::keyPressEvent(QKeyEvent *event)
{
    // Always send keypress events to the currently focused widget first
    if (GetFocusWidget()->keyPressEvent(event))
        return true;

    bool handled = false;
    QStringList actions;
    gContext->GetMainWindow()->TranslateKeyPress("Browser", event, actions);

    for (int i = 0; i < actions.size() && !handled; i++)
    {

        QString action = actions[i];
        handled = true;

        if (action == "MENU")
        {
            activeBrowser()->SetActive(false);
            slotStatusBarMessage("");

            QString label = tr("Actions");

            MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");

            m_menuPopup = new MythDialogBox(label, popupStack, "actionmenu");

            if (m_menuPopup->Create())
                popupStack->AddScreen(m_menuPopup);

            m_menuPopup->SetReturnEvent(this, "action");

            connect(m_menuPopup, SIGNAL(Exiting()), SLOT(slotExitingMenu()));

            m_menuPopup->AddButton(tr("Enter URL"), SLOT(slotEnterURL()));

            if (activeBrowser()->CanGoBack())
                m_menuPopup->AddButton(tr("Back"), SLOT(slotBack()));

            if (activeBrowser()->CanGoForward())
                m_menuPopup->AddButton(tr("Forward"), SLOT(slotForward()));

            m_menuPopup->AddButton(tr("Zoom In"), SLOT(slotZoomIn()));
            m_menuPopup->AddButton(tr("Zoom Out"), SLOT(slotZoomOut()));
            m_menuPopup->AddButton(tr("New Tab"), SLOT(slotAddTab()));

            if (m_browserList.size() > 1)
                m_menuPopup->AddButton(tr("Delete Tab"), SLOT(slotDeleteTab()));

            m_menuPopup->AddButton(tr("Edit Bookmarks"), SLOT(slotShowBookmarks()));
            m_menuPopup->AddButton(tr("Add Bookmark"), SLOT(slotAddBookmark()));

            m_menuPopup->AddButton(tr("Cancel"));
        }
        else if (action == "INFO")
        {
            if (GetFocusWidget() == m_pageList)
                SetFocusWidget(activeBrowser());
            else
                SetFocusWidget(m_pageList);
        }

        else if (action == "ESCAPE")
        {
            GetScreenStack()->PopScreen(false, true);
        }
        else if (action == "PREVTAB")
        {
            int pos = m_pageList->GetCurrentPos();
            if (pos > 0)
               m_pageList->SetItemCurrent(--pos);
        }
        else if (action == "NEXTTAB")
        {
            int pos = m_pageList->GetCurrentPos();
            if (pos < m_pageList->GetCount() - 1)
               m_pageList->SetItemCurrent(++pos);
        }
        else if (action == "DELETETAB")
        {
            slotDeleteTab();
        }
        else
            handled = false;
    }

    if (!handled && MythScreenType::keyPressEvent(event))
        handled = true;

    return handled;
}

void MythBrowser::slotExitingMenu(void)
{
    if (GetFocusWidget() == activeBrowser())
        activeBrowser()->SetActive(true);
}

