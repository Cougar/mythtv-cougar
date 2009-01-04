// Myth headers
#include "mythverbose.h"

// MythUI headers
#include "mythmainwindow.h"
#include "mythgesture.h"

#include "mythuicheckbox.h"

MythUICheckBox::MythUICheckBox(MythUIType *parent, const QString &name)
            : MythUIType(parent, name)
{
    m_currentCheckState = MythUIStateType::Off;
    m_state = "active";

    m_BackgroundState = m_CheckState = NULL;

    connect(this, SIGNAL(TakingFocus()), this, SLOT(Select()));
    connect(this, SIGNAL(LosingFocus()), this, SLOT(Deselect()));
    connect(this, SIGNAL(Enabling()), this, SLOT(Enable()));
    connect(this, SIGNAL(Disabling()), this, SLOT(Disable()));

    SetCanTakeFocus(true);
}

MythUICheckBox::~MythUICheckBox()
{
}

void MythUICheckBox::SetInitialStates()
{
    m_BackgroundState = dynamic_cast<MythUIStateType*>(GetChild("background"));
    m_CheckState = dynamic_cast<MythUIStateType*>(GetChild("checkstate"));

    if (!m_CheckState || !m_BackgroundState)
        VERBOSE(VB_IMPORTANT, QString("Checkbox %1 is missing required "
                                      "elements").arg(objectName()));

    if (m_CheckState)
        m_CheckState->DisplayState(m_currentCheckState);

    if (m_BackgroundState)
        m_BackgroundState->DisplayState(m_state);
}


void MythUICheckBox::toggleCheckState()
{
    bool onOff = false;

    if (m_currentCheckState != MythUIStateType::Full)
    {
        m_currentCheckState = MythUIStateType::Full;
        onOff = true;
    }
    else
    {
        m_currentCheckState = MythUIStateType::Off;
        onOff = false;
    }

    if (m_CheckState)
        m_CheckState->DisplayState(m_currentCheckState);

    emit toggled(onOff);
    emit valueChanged();
}

void MythUICheckBox::SetCheckState(MythUIStateType::StateType state)
{
    m_currentCheckState = state;
    m_CheckState->DisplayState(state);
}

MythUIStateType::StateType MythUICheckBox::GetCheckState()
{
    return m_currentCheckState;
}

void MythUICheckBox::Select()
{
    if (!IsEnabled())
        return;

    m_state = "selected";
    m_BackgroundState->DisplayState(m_state);
}

void MythUICheckBox::Deselect()
{
    if (IsEnabled())
        m_state = "active";
    else
        m_state = "disabled";
    m_BackgroundState->DisplayState(m_state);
}

void MythUICheckBox::Enable()
{
    m_state = "active";
    m_BackgroundState->DisplayState(m_state);
}

void MythUICheckBox::Disable()
{
    m_state = "disabled";
    m_BackgroundState->DisplayState(m_state);
}

/** \brief Mouse click/movement handler, recieves mouse gesture events from the
 *         QApplication event loop. Should not be used directly.
 *
 *  \param uitype The mythuitype receiving the event
 *  \param event Mouse event
 */
void MythUICheckBox::gestureEvent(MythUIType *uitype, MythGestureEvent *event)
{
    if (event->gesture() == MythGestureEvent::Click)
    {
        if (IsEnabled())
            toggleCheckState();
    }
}

/** \brief Key event handler
 *
 *  \param event Keypress event
 */
bool MythUICheckBox::keyPressEvent(QKeyEvent *event)
{
    QStringList actions;
    bool handled = false;
    GetMythMainWindow()->TranslateKeyPress("Global", event, actions);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        handled = true;

        if (action == "SELECT")
            toggleCheckState();
        else
            handled = false;
    }

    return handled;
}

void MythUICheckBox::CreateCopy(MythUIType *parent)
{
    MythUICheckBox *checkbox = new MythUICheckBox(parent, objectName());
    checkbox->CopyFrom(this);
}

void MythUICheckBox::CopyFrom(MythUIType *base)
{
    MythUICheckBox *button = dynamic_cast<MythUICheckBox *>(base);
    if (!button)
    {
        VERBOSE(VB_IMPORTANT, "MythUICheckBox::CopyFrom: Dynamic cast of base "
                              "failed");
        return;
    }

    MythUIType::CopyFrom(base);

    SetInitialStates();
}

void MythUICheckBox::Finalize()
{
    SetInitialStates();
}
