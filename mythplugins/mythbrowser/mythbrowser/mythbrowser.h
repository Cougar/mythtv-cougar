#ifndef MYTHBROWSER_H
#define MYTHBROWSER_H

#include <QUrl>

#include <libmythui/mythuiwebbrowser.h>
#include <libmythui/mythuibuttonlist.h>
#include <libmythui/mythscreentype.h>
#include <libmythui/mythdialogbox.h>
#include <libmythui/mythuiprogressbar.h>

#include "bookmarkmanager.h"

class WebPage;

class MythBrowser : public MythScreenType
{
  Q_OBJECT

  public:
    MythBrowser(MythScreenStack *parent, const char *name,
           QStringList &urlList, float zoom);
    ~MythBrowser();

    bool Create(void);
    bool keyPressEvent(QKeyEvent *);

  public slots:
    void slotOpenURL(const QString &url);

  protected slots:
    void slotZoomIn();
    void slotZoomOut();

    void slotBack();
    void slotForward();

    void slotEnterURL(void);

    void slotAddTab(const QString &url = "", bool doSwitch = true);
    void slotDeleteTab(void);

    void slotShowBookmarks(void);
    void slotAddBookmark(void);

    void slotLoadStarted(void);
    void slotLoadFinished(bool OK);
    void slotLoadProgress(int progress);
    void slotTitleChanged(const QString &title);
    void slotStatusBarMessage(const QString &text);
    void slotTabSelected(MythUIButtonListItem *item);
    void slotTabLosingFocus(void);
    void slotIconChanged(void);
    void slotExitingMenu(void);

  private:
    MythUIWebBrowser* activeBrowser(void);

    void switchTab(int newTab);

    QStringList               m_urlList;

    MythUIButtonList         *m_pageList;
    QList<WebPage*>           m_browserList;
    MythUIProgressBar        *m_progressBar;
    MythUIText               *m_titleText;
    MythUIText               *m_statusText;

    int       m_currentBrowser;
    QUrl      m_url;
    float     m_zoom;

    Bookmark  m_editBookmark;

    MythDialogBox *m_menuPopup;

    friend class WebPage;
};

#endif
