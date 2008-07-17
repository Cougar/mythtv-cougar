// -*- Mode: c++ -*-
// Qt headers
#include <qstring.h>

// MythTV headers
#include <mythtv/mythcontext.h>

// MythControls headers
#include "keygrabber.h"

KeyGrabPopupBox::KeyGrabPopupBox(MythScreenStack *parent)
    : MythScreenType (parent, "keygrabberdialog")
{
    m_waitingForKeyRelease = m_keyReleaseSeen = false;
    m_capturedKey = QString::null;
    m_okButton = m_cancelButton = NULL;
    m_messageText = NULL;
}

KeyGrabPopupBox::~KeyGrabPopupBox()
{
}

bool KeyGrabPopupBox::Create(void)
{
    bool foundtheme = false;

    // Load the theme for this screen
    foundtheme = LoadWindowFromXML("controls-ui.xml", "keygrabpopup", this);

    if (!foundtheme)
        return false;

    m_messageText = dynamic_cast<MythUIText *> (GetChild("message"));
    m_okButton = dynamic_cast<MythUIButton *> (GetChild("ok"));
    m_cancelButton = dynamic_cast<MythUIButton *> (GetChild("cancel"));

    if (!m_messageText || !m_okButton || !m_cancelButton)
    {
        VERBOSE(VB_IMPORTANT, "Theme is missing critical elements.");
        return false;
    }

    QString label = QString("%1\n\n%2").arg(tr("Press A Key")).arg(tr("Waiting for key press"));

    m_messageText->SetText(label);
    m_okButton->SetText(tr("Ok"));
    m_cancelButton->SetText(tr("Cancel"));

    connect(m_okButton, SIGNAL(buttonPressed()), SLOT(SendResult()));
    connect(m_cancelButton, SIGNAL(buttonPressed()), SLOT(Close()));

    m_okButton->SelectState(MythUIButton::Disabled);
    m_cancelButton->SelectState(MythUIButton::Disabled);

    return true;
}

bool KeyGrabPopupBox::keyPressEvent(QKeyEvent *event)
{
    // If no capturing has occured yet, then start waiting for key release
    m_waitingForKeyRelease |= !m_keyReleaseSeen;

    bool handled = false;

    if (!m_waitingForKeyRelease)
    {
        if (GetFocusWidget()->keyPressEvent(event))
            handled = true;
    }
    else
    {
        int keycode = event->key();

        // Modifier keypress, ignore until we see the complete combo
        if ((keycode >= Qt::Key_Shift) && (keycode <= Qt::Key_AltGr))
            return true;

        m_waitingForKeyRelease = false;
        m_keyReleaseSeen       = true;

        QString key_name = QString(QKeySequence(event->key()));
        if (!key_name.isEmpty() && !key_name.isNull())
        {
            QString modifiers = "";

            /* key modifier strings as defined by the QT docs */
            if (event->modifiers() & Qt::ShiftModifier)
                modifiers += "Shift+";
            if (event->modifiers() & Qt::ControlModifier)
                modifiers += "Ctrl+";
            if (event->modifiers() & Qt::AltModifier)
                modifiers += "Alt+";
            if (event->modifiers() & Qt::MetaModifier)
                modifiers += "Meta+";

            key_name = modifiers + key_name;
        }

        if (key_name.isEmpty())
        {
            m_messageText->SetText(tr("Pressed key not recognized"));
        }
        else
        {
            m_capturedKey = key_name;
            m_messageText->SetText(tr("Add key '%1'?").arg(key_name));
        }

        m_okButton->SelectState(MythUIButton::Normal);
        m_cancelButton->SelectState(MythUIButton::Normal);

        BuildFocusList();

        SetFocusWidget(m_okButton);

        handled = true;
    }

    if (!handled && MythScreenType::keyPressEvent(event))
        handled = true;

    return handled;
}

void KeyGrabPopupBox::SendResult()
{
    emit HaveResult(m_capturedKey);
    Close();
}
