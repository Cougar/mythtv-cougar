// QT
#include <QTimer>
#include <QDomDocument>

// Myth headers
#include "mythverbose.h"

// MythUI headers
#include "mythmainwindow.h"
#include "mythuigroup.h"

#include "mythuibutton.h"

MythUIButton::MythUIButton(MythUIType *parent, const QString &name)
            : MythUIType(parent, name)
{
    m_clickTimer = new QTimer();
    m_clickTimer->setSingleShot(true);

    m_Pushed = false;
    m_Lockable = false;

    m_Text = NULL;
    m_BackgroundState = NULL;

    connect(m_clickTimer, SIGNAL(timeout()), SLOT(UnPush()));

    connect(this, SIGNAL(TakingFocus()), SLOT(Select()));
    connect(this, SIGNAL(LosingFocus()), SLOT(Deselect()));
    connect(this, SIGNAL(Enabling()), SLOT(Enable()));
    connect(this, SIGNAL(Disabling()), SLOT(Disable()));

    SetCanTakeFocus(true);
}

MythUIButton::~MythUIButton()
{
    if (m_clickTimer)
        delete m_clickTimer;
}

void MythUIButton::SetInitialStates()
{
    m_BackgroundState = dynamic_cast<MythUIStateType*>(GetChild("buttonstate"));

    if (!m_BackgroundState)
        VERBOSE(VB_IMPORTANT, QString("Button %1 is missing required "
                                      "elements").arg(objectName()));

    SetState("active");

    if (m_Text && m_Message.isEmpty())
        m_Message = m_Text->GetDefaultText();
}

void MythUIButton::Reset()
{
    MythUIType::Reset();
}

void MythUIButton::Select()
{
    if (!IsEnabled() || m_Pushed)
        return;

    SetState("selected");
}

void MythUIButton::Deselect()
{
    if (m_Pushed)
        return;

    if (IsEnabled())
        SetState("active");
    else
        SetState("disabled");
}

void MythUIButton::Enable()
{
    SetState("active");
}

void MythUIButton::Disable()
{
    SetState("disabled");
}

void MythUIButton::SetState(QString state)
{
    if (m_state == state)
        return;

    if (m_Pushed && state != "pushed")
        UnPush();

    m_state = state;

    if (!m_BackgroundState)
        return;

    m_BackgroundState->DisplayState(m_state);

    MythUIGroup *activeState = dynamic_cast<MythUIGroup*>
                                    (m_BackgroundState->GetCurrentState());
    if (activeState)
        m_Text = dynamic_cast<MythUIText*>(activeState->GetChild("text"));

    if (m_Text)
    {
        m_Text->SetFontState(m_state);
        m_Text->SetText(m_Message);
    }
}

bool MythUIButton::keyPressEvent(QKeyEvent *e)
{
    QStringList actions;
    bool handled = false;
    GetMythMainWindow()->TranslateKeyPress("Global", e, actions);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        handled = true;

        if (action == "SELECT")
        {
            if (IsEnabled())
            {
                if (m_Pushed)
                    UnPush();
                else
                    Push();
            }
        }
        else
            handled = false;
    }

    return handled;
}

/** \brief Mouse click/movement handler, recieves mouse gesture events from the
 *         QApplication event loop. Should not be used directly.
 *
 *  \param uitype The mythuitype receiving the event
 *  \param event Mouse event
 */
void MythUIButton::gestureEvent(MythUIType *uitype, MythGestureEvent *event)
{
    if (event->gesture() == MythGestureEvent::Click)
    {
        if (!IsEnabled())
            return;

        if (m_Pushed)
            UnPush();
        else
            Push();
    }
}

void MythUIButton::Push(bool lock)
{
    m_Pushed = true;
    SetState("pushed");
    if (!lock && !m_Lockable)
        m_clickTimer->start(500);
    emit Clicked();
}

void MythUIButton::UnPush()
{
    if (!m_Pushed)
        return;

    m_clickTimer->stop();

    m_Pushed = false;

    if (m_HasFocus)
        SetState("selected");
    else if (m_Enabled)
        SetState("active");
    else
        SetState("disabled");

    if (m_Lockable)
        emit Clicked();
}

void MythUIButton::SetText(const QString &msg)
{
    if (m_Message == msg)
        return;

    m_Message = msg;

    MythUIGroup *activeState = dynamic_cast<MythUIGroup*>
                                    (m_BackgroundState->GetCurrentState());
    if (activeState)
        m_Text = dynamic_cast<MythUIText*>(activeState->GetChild("text"));

    if (m_Text)
        m_Text->SetText(msg);
}

QString MythUIButton::GetText() const
{
    return m_Message;
}

QString MythUIButton::GetDefaultText() const
{
    return m_Text->GetDefaultText();
}

bool MythUIButton::ParseElement(QDomElement &element)
{
    if (element.tagName() == "value")
    {
        QString message = getFirstText(element);
        SetText(message);
    }
    else
        return MythUIType::ParseElement(element);

    return true;
}

void MythUIButton::CreateCopy(MythUIType *parent)
{
    MythUIButton *button = new MythUIButton(parent, objectName());
    button->CopyFrom(this);
}

void MythUIButton::CopyFrom(MythUIType *base)
{
    MythUIButton *button = dynamic_cast<MythUIButton *>(base);
    if (!button)
    {
        VERBOSE(VB_IMPORTANT,
                        "MythUIButton::CopyFrom: Dynamic cast of base failed");
        return;
    }

    m_Message = button->m_Message;
    m_Lockable = button->m_Lockable;

    MythUIType::CopyFrom(base);

    SetInitialStates();
}

void MythUIButton::Finalize()
{
    SetInitialStates();
}
