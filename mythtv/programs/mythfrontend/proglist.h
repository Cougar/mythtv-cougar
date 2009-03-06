#ifndef PROGLIST_H_
#define PROGLIST_H_

// Qt headers
#include <QDateTime>
#include <QKeyEvent>

// MythTV headers
#include "mythscreentype.h"
#include "programinfo.h"
#include "programlist.h"

#include "mythwidgets.h"

enum ProgListType {
    plUnknown = 0,
    plTitle = 1,
    plTitleSearch,
    plKeywordSearch,
    plPeopleSearch,
    plPowerSearch,
    plSQLSearch,
    plNewListings,
    plMovies,
    plCategory,
    plChannel,
    plTime,
    plRecordid,
    plStoredSearch
};

class MythUIText;
class MythUIButtonList;
class ProgLister;

class PhrasePopup : public MythScreenType
{
    Q_OBJECT

  public:
    PhrasePopup(MythScreenStack *parentStack,
                ProgLister *parent,
                RecSearchType searchType,
                const QStringList &list,
                const QString &currentValue);

    bool Create();

  signals:
    void haveResult(QString item);

  private slots:
    void okClicked(void);
    void deleteClicked(void);
    void recordClicked(void);
    void phraseClicked(MythUIButtonListItem *item);
    void phraseSelected(MythUIButtonListItem *item);
    void editChanged(void);

  private:
    ProgLister      *m_parent;
    RecSearchType    m_searchType;
    QStringList      m_list;
    QString          m_currentValue;

    MythUIText       *m_titleText;
    MythUIButtonList *m_phraseList;
    MythUITextEdit   *m_phraseEdit;
    MythUIButton     *m_okButton;
    MythUIButton     *m_deleteButton;
    MythUIButton     *m_recordButton;
};

class TimePopup : public MythScreenType
{
    Q_OBJECT

  public:
    TimePopup(MythScreenStack *parentStack, ProgLister *parent);

    bool Create();

  signals:
    void haveResult(QDateTime time);

  private slots:
    void okClicked(void);

  private:
    ProgLister      *m_parent;
    QStringList      m_list;
    QString          m_currentValue;

    MythUIButtonList *m_dateList;
    MythUIButtonList *m_timeList;
    MythUIButton     *m_okButton;
};

class PowerSearchPopup : public MythScreenType
{
    Q_OBJECT

  public:
    PowerSearchPopup(MythScreenStack *parentStack,
                ProgLister *parent,
                RecSearchType searchType,
                const QStringList &list,
                const QString &currentValue);

    bool Create();

  signals:
    void haveResult(QString item);

  private slots:
    void editClicked(void);
    void deleteClicked(void);
    void recordClicked(void);
    void phraseClicked(MythUIButtonListItem *item);
    void phraseSelected(MythUIButtonListItem *item);

  private:
    ProgLister      *m_parent;
    RecSearchType    m_searchType;
    QStringList      m_list;
    QString          m_currentValue;

    MythUIText       *m_titleText;
    MythUIButtonList *m_phraseList;
    MythUITextEdit   *m_phraseEdit;
    MythUIButton     *m_editButton;
    MythUIButton     *m_deleteButton;
    MythUIButton     *m_recordButton;
};

class EditPowerSearchPopup : public MythScreenType
{
    Q_OBJECT

  public:
    EditPowerSearchPopup(MythScreenStack *parentStack, ProgLister *parent,
                         const QString &currentValue);

    bool Create();

  private slots:
    void okClicked(void);

  private:
    void initLists(void);

    ProgLister      *m_parent;
    QStringList      m_categories;
    QStringList      m_genres;
    QStringList      m_channels;

    QString          m_currentValue;

    MythUITextEdit   *m_titleEdit;
    MythUITextEdit   *m_subtitleEdit;
    MythUITextEdit   *m_descEdit;
    MythUIButtonList *m_categoryList;
    MythUIButtonList *m_genreList;
    MythUIButtonList *m_channelList;

    MythUIButton     *m_okButton;
};

class ProgLister : public MythScreenType
{
  friend class PhrasePopup;
  friend class TimePopup;
  friend class PowerSearchPopup;
  friend class EditPowerSearchPopup;

  Q_OBJECT

  public:
    ProgLister(MythScreenStack *parent, ProgListType pltype,
               const QString &view, const QString &from);
    ~ProgLister();

    bool Create(void);
    bool keyPressEvent(QKeyEvent *);
    void customEvent(QEvent*);

  protected slots:
    void Init(void);
    void prevView(void);
    void nextView(void);
    void setViewFromTime(QDateTime searchTime);
    void select(void);
    void edit(void);
    void customEdit(void);
    void remove(void);
    void upcoming(void);
    void details(void);
    void chooseView(void);
    void updateInfo(MythUIButtonListItem *item);
    void setViewFromList(QString item);
    void doRemove(bool ok);

  protected:
    void quickRecord(void);

  private:
    void fillViewList(const QString &view);
    void fillItemList(bool restorePosition);
    void updateButtonList(void);

    bool powerStringToSQL(const QString &qphrase, QString &output,
                          MSqlBindings &bindings);

    void updateKeywordInDB(const QString &text, const QString &oldValue);

    ProgListType m_type;
    QString      m_addTables;
    QDateTime    m_startTime;
    QDateTime    m_searchTime;
    QString      m_dayFormat;
    QString      m_hourFormat;
    QString      m_timeFormat;
    QString      m_fullDateFormat;
    QString      m_channelOrdering;
    QString      m_channelFormat;

    RecSearchType m_searchType;

    int           m_curView;
    QStringList   m_viewList;
    QStringList   m_viewTextList;

    ProgramList   m_itemList;
    ProgramList   m_schedList;

    QStringList   m_typeList;
    QStringList   m_genreList;
    QStringList   m_stationList;

    bool m_allowEvents;
    bool m_titleSort;
    bool m_reverseSort;
    bool m_useGenres;

    MythUIText       *m_schedText;
    MythUIText       *m_curviewText;
    MythUIButtonList *m_progList;
    MythUIText       *m_messageText;
};

#endif
