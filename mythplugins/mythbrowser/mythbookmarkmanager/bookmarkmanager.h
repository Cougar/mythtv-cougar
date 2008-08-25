#ifndef BOOKMARKMANAGER_H
#define BOOKMARKMANAGER_H

#include <mythtv/uitypes.h>
#include <mythtv/xmlparse.h>
#include <mythtv/oldsettings.h>
#include <mythtv/mythwidgets.h>
#include <mythtv/mythdialogs.h>

// libmythui
#include <libmythui/mythlistbutton.h>
#include <libmythui/mythuibuttonlist.h>
#include <libmythui/mythscreentype.h>
#include <libmythui/mythdialogbox.h>

class MythBrowser;

class Bookmark
{
  public:
    Bookmark(void)
    {
        category = "";
        name = "";
        url = "";
        selected = false;
    }

    QString category;
    QString name;
    QString url;
    bool    selected;

    inline bool operator == (const Bookmark &b) const
    {
        return category == b.category && name == b.name && url == b.url;
    }
};

class BrowserConfig : public MythScreenType
{
  Q_OBJECT

  public:

    BrowserConfig(MythScreenStack *parent, const char *name = 0);
    ~BrowserConfig();

    bool Create(void);
    bool keyPressEvent(QKeyEvent *);

  private:
    MythUITextEdit   *m_commandEdit;
    MythUITextEdit   *m_zoomEdit;

    MythUIText       *m_descriptionText;
    MythUIText       *m_titleText;

    MythUIButton     *m_okButton;
    MythUIButton     *m_cancelButton;

  private slots:
    void slotSave(void);
    void slotFocusChanged(void);
};

class BookmarkManager : public MythScreenType
{
  Q_OBJECT

  public:
    BookmarkManager(MythScreenStack *parent, const char *name);
    ~BookmarkManager();

    bool Create(void);
    bool keyPressEvent(QKeyEvent *);
    void SetBrowser(MythBrowser *browser) { m_browser = browser; }

  private slots:
    void slotGroupSelected(MythListButtonItem *item);
    void slotBookmarkClicked(MythUIButtonListItem *item);
    void slotEditDialogExited(void);
    void slotDoDeleteCurrent(bool doDelete);
    void slotDoDeleteMarked(bool doDelete);

    void slotAddBookmark(void);
    void slotEditBookmark(void);
    void slotDeleteCurrent(void);
    void slotDeleteMarked(void);
    void slotShowCurrent(void);
    void slotShowMarked(void);
    void slotClearMarked(void);

  private:
    uint GetMarkedCount(void);
    void UpdateGroupList(void);
    void UpdateURLList(void);
    void ShowEditDialog(bool edit);
    void ReloadBookmarks(void);

    MythBrowser      *m_browser;

    QList<Bookmark*>  m_siteList;

    Bookmark          m_savedBookmark;

    MythUIButtonList *m_bookmarkList;
    MythListButton   *m_groupList;
    MythUIText       *m_messageText;

    MythDialogBox    *m_menuPopup;
};

#endif
