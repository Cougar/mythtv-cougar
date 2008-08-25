#ifndef BOOKMARKEDITOR_H
#define BOOKMARKEDITOR_H

// myth
#include <mythtv/libmythui/mythscreentype.h>
#include <mythtv/libmythui/mythdialogbox.h>
#include <mythtv/libmythui/mythuibutton.h>
#include <mythtv/libmythui/mythuitext.h>
#include <mythtv/libmythui/mythuitextedit.h>

// mythbrowser


class Bookmark;

/** \class BookmarkEditor
 *  \brief Site category, name and URL edit screen.
 */
class BookmarkEditor : public MythScreenType
{
    Q_OBJECT

  public:

    BookmarkEditor(Bookmark *site, bool edit, MythScreenStack *parent,
                   const char *name);
    ~BookmarkEditor();

    bool Create(void);
    bool keyPressEvent(QKeyEvent *event);

  private:
    Bookmark   *m_site;
    QString     m_siteName;
    QString     m_siteCategory;
    bool        m_editing;

    MythUIText     *m_titleText;

    MythUITextEdit *m_categoryEdit;
    MythUITextEdit *m_nameEdit;
    MythUITextEdit *m_urlEdit;

    MythUIButton *m_okButton;
    MythUIButton *m_cancelButton;
    MythUIButton *m_findCategoryButton;

    MythUISearchDialog *m_searchDialog;

  private slots:
    void slotFindCategory(void);
    void slotCategoryFound(QString category);

    void Save(void);
    void Exit(void);
};

#endif
