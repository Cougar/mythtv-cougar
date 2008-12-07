#ifndef MYTHDIALOGBOX_H_
#define MYTHDIALOGBOX_H_

#include <QEvent>

#include "mythscreentype.h"
#include "mythuibuttonlist.h"
#include "mythuitextedit.h"
#include "mythuibutton.h"

class MythUIButtonListItem;
class MythUIButtonList;

const int kMythDialogBoxCompletionEventType = 34111;

/**
 *  \class DialogCompletionEvent
 *
 *  \brief Event dispatched from MythUI modal dialogs to a listening class
 *         containing a result of some form
 *
 *  The result may be in the format of an int, text or a void pointer
 *  dependant on the dialog type and information it is conveying.
 */
class MPUBLIC DialogCompletionEvent : public QEvent
{
  public:
    DialogCompletionEvent(const QString &id, int result, QString text,
                          QVariant data)
        : QEvent((QEvent::Type)kMythDialogBoxCompletionEventType),
          m_id(id), m_result(result), m_resultText(text), m_resultData(data) { }

    QString GetId() { return m_id; }
    int GetResult() { return m_result; }
    QString GetResultText() { return m_resultText; }
    void *GetResultData() { return m_resultData.value<void *>(); }
    QVariant GetData() { return m_resultData; }

  private:
    QString m_id;
    int m_result;
    QString m_resultText;
    QVariant m_resultData;
};

/**
 *  \class MythDialogBox
 *
 *  \brief Basic menu dialog, message and a list of options
 *
 *  Sends out a DialogCompletionEvent event and the Selected() signal
 *  containing the result when the user selects the Ok button.
 */
class MPUBLIC MythDialogBox : public MythScreenType
{
    Q_OBJECT
  public:
    MythDialogBox(const QString &text,
                  MythScreenStack *parent, const char *name,
                  bool fullscreen = false);
    MythDialogBox(const QString &title, const QString &text,
                  MythScreenStack *parent, const char *name,
                  bool fullscreen = false);

    virtual bool Create(void);

    void SetReturnEvent(QObject *retobject, const QString &resultid);

    void AddButton(const QString &title, QVariant data = 0);
    void AddButton(const QString &title, const char *slot);

    virtual bool keyPressEvent(QKeyEvent *event);

  public slots:
    void Select(MythUIButtonListItem* item);

  signals:
    void Selected();

  protected:
    void SendEvent(int res, QString text = "", QVariant data = 0);

    MythUIText       *m_titlearea;
    MythUIText       *m_textarea;
    MythUIButtonList *m_buttonList;
    QObject          *m_retObject;
    QString m_id;
    bool m_useSlots;

    bool    m_fullscreen;
    QString m_title;
    QString m_text;
};

/**
 *  \class MythConfirmationDialog
 *
 *  \brief Dialog asking for user confirmation. Ok and optional Cancel button.
 *
 *  Sends out a DialogCompletionEvent event and the haveResult() signal
 *  containing the result.
 */
class MPUBLIC MythConfirmationDialog : public MythScreenType
{
    Q_OBJECT

  public:
    MythConfirmationDialog(MythScreenStack *parent, const QString &message,
                           bool showCancel = true);

    bool Create(void);
    void SetReturnEvent(QObject *retobject, const QString &resultid);
    void SetData(QVariant data) { m_resultData = data; }

    bool keyPressEvent(QKeyEvent *event);

 signals:
     void haveResult(bool);

  private:
    void sendResult(bool);
    QString m_message;
    bool m_showCancel;
    QObject *m_retObject;
    QString m_id;
    QVariant m_resultData;

  private slots:
    void Confirm(void);
    void Cancel();
};

/**
 *  \class MythTextInputDialog
 *
 *  \brief Dialog prompting the user to enter a text string
 *
 *  Sends out a DialogCompletionEvent event and the haveResult() signal
 *  containing the result when the user selects the Ok button.
 */
class MPUBLIC MythTextInputDialog : public MythScreenType
{
    Q_OBJECT

  public:
    MythTextInputDialog(MythScreenStack *parent, const QString &message,
                        InputFilter filter = FilterNone,
                        bool isPassword = false,
                        const QString &defaultValue = "");

    bool Create(void);
    void SetReturnEvent(QObject *retobject, const QString &resultid);

  signals:
     void haveResult(QString);

  private:
    MythUITextEdit *m_textEdit;
    QString m_message;
    QString m_defaultValue;
    InputFilter m_filter;
    bool m_isPassword;
    QObject *m_retObject;
    QString m_id;

  private slots:
    void sendResult();
};


/**
 * \class MythUISearchDialog
 * \brief Provide a dialog to quickly find an entry in a list
 *
 * You pass a QStringList containing the list you want to search.
 * As the user enters a text string in the edit the list of entries
 * changes to show only the entries that match the input string. You
 * have the option to search anywhere in the string or only the start.
 *
 * When the user either clicks an entry in the list or presses the OK
 * button the dialog will either send a DialogCompletionEvent or an
 * haveResult(QString) signal will be generated. Both pass the selected
 * string back to the caller.
 */
class MPUBLIC MythUISearchDialog : public MythScreenType
{
  Q_OBJECT

  public:
    MythUISearchDialog(MythScreenStack *parent,
                     const QString &message,
                     const QStringList &list,
                     bool  matchAnywhere = false,
                     const QString &defaultValue = "");

    bool Create(void);
    void SetReturnEvent(QObject *retobject, const QString &resultid);

  signals:
     void haveResult(QString);

  private:
    MythUIButtonList *m_itemList;
    MythUITextEdit   *m_textEdit;
    MythUIText       *m_titleText;
    MythUIText       *m_matchesText;

    QString           m_title;
    QString           m_defaultValue;
    QStringList       m_list;
    bool              m_matchAnywhere;

    QObject          *m_retObject;
    QString           m_id;

  private slots:
    void slotSendResult(void);
    void slotUpdateList(void);
};

MPUBLIC void ShowOkPopup(const QString &message);

#endif
