#include <qapplication.h>

#include "dialogbox.h"
#include "mythlistbutton.h"
#include "mythmainwindow.h"
#include "mythfontproperties.h"

MythDialogBox::MythDialogBox(const QString &text,
                     MythScreenStack *parent, const char *name) 
         : MythScreenType(parent, name)
{
    m_id = "";

    MythFontProperties fontProp;
    fontProp.face = QFont("Arial", 24, QFont::Bold);
    fontProp.color = QColor(Qt::white);
    fontProp.hasShadow = true;
    fontProp.shadowOffset = QPoint(4, 4);
    fontProp.shadowColor = QColor(Qt::black);
    fontProp.shadowAlpha = 64;

    QRect textRect = QRect(60, 60, 680, 240);

    MythUIText *label = new MythUIText(text, fontProp, textRect, textRect, 
                                       this, "label");
    label->SetJustification(Qt::WordBreak | Qt::AlignLeft | Qt::AlignTop);

    QRect listarea = QRect(60, 340, 680, 200);

    buttonList = new MythListButton(this, "listbutton", listarea, false, false);

    buttonList->SetFontActive(fontProp);
    fontProp.color = QColor(qRgb(128, 128, 128));
    buttonList->SetFontInactive(fontProp);

    buttonList->SetSpacing(10);
    buttonList->SetMargin(6);
    buttonList->SetDrawFromBottom(true);
    buttonList->SetTextFlags(Qt::AlignCenter);

    buttonList->SetActive(true);
}

void MythDialogBox::SetReturnEvent(MythScreenType *retscreen, 
                               const QString &resultid)
{
    m_retScreen = retscreen;
    m_id = resultid;
}

void MythDialogBox::AddButton(const QString &title)
{
    new MythListButtonItem(buttonList, title);
}

bool MythDialogBox::keyPressEvent(QKeyEvent *e)
{
    bool handled = false;
    QStringList actions;
    if (GetMythMainWindow()->TranslateKeyPress("qt", e, actions))
    {
        for (unsigned int i = 0; i < actions.size() && !handled; i++)
        {
            QString action = actions[i];
            handled = true;

            if (action == "UP")
                buttonList->MoveUp();
            else if (action == "DOWN")
                buttonList->MoveDown();
            else if (action == "ESCAPE" || action == "LEFT")
            {
                SendEvent(-1);
                m_ScreenStack->PopScreen();
            }
            else if (action == "SELECT" || action == "RIGHT")
            {
                MythListButtonItem *item = buttonList->GetItemCurrent();
                SendEvent(buttonList->GetItemPos(item));
                m_ScreenStack->PopScreen();
            }
            else
                handled = false;
        }
    }

    return handled;
}

void MythDialogBox::SendEvent(int res)
{
    if (!m_retScreen)
        return;

    DialogCompletionEvent *dce = new DialogCompletionEvent(m_id, res);
    QApplication::postEvent(m_retScreen, dce);
}
