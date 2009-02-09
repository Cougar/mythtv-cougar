#include <cassert>

#include <QDomDocument>

#include "mythverbose.h"

#include "mythscreentype.h"
#include "mythscreenstack.h"
#include "mythmainwindow.h"

MythScreenType::MythScreenType(MythScreenStack *parent, const QString &name,
                               bool fullscreen)
              : MythUIType(parent, name)
{
    assert(parent);

    m_FullScreen = fullscreen;
    m_CurrentFocusWidget = NULL;

    m_ScreenStack = parent;
    m_IsDeleting = false;

    // Can be overridden, of course, but default to full sized.
    m_Area = GetMythMainWindow()->GetUIScreenRect();
}

MythScreenType::MythScreenType(MythUIType *parent, const QString &name,
                               bool fullscreen)
              : MythUIType(parent, name)
{
    m_FullScreen = fullscreen;
    m_CurrentFocusWidget = NULL;

    m_ScreenStack = NULL;
    m_IsDeleting = false;

    m_Area = GetMythMainWindow()->GetUIScreenRect();
}

MythScreenType::~MythScreenType()
{
    m_CurrentFocusWidget = NULL;
    emit Exiting();
}

bool MythScreenType::IsFullscreen(void) const
{
    return m_FullScreen;
}

void MythScreenType::SetFullscreen(bool full)
{
    m_FullScreen = full;
}

MythUIType *MythScreenType::GetFocusWidget(void) const
{
    return m_CurrentFocusWidget;
}

bool MythScreenType::SetFocusWidget(MythUIType *widget)
{
    if (!widget || !widget->IsVisible())
    {
        QMap<int, MythUIType *>::iterator it = m_FocusWidgetList.begin();
        MythUIType *current;

        while (it != m_FocusWidgetList.end())
        {
            current = *it;

            if (current->CanTakeFocus() && current->IsVisible())
            {
                widget = current;
                break;
            }
            ++it;
        }
    }

    if (!widget)
        return false;

    if (m_CurrentFocusWidget)
        m_CurrentFocusWidget->LoseFocus();
    m_CurrentFocusWidget = widget;
    m_CurrentFocusWidget->TakeFocus();

    return true;
}

bool MythScreenType::NextPrevWidgetFocus(bool up)
{
    if (!m_CurrentFocusWidget || m_FocusWidgetList.isEmpty())
        return SetFocusWidget(NULL);

    bool reachedCurrent = false;
    bool looped = false;

    QMap<int, MythUIType *>::iterator it = m_FocusWidgetList.begin();
    MythUIType *current;

    // There is probably a more efficient way to do this, but the list
    // is never going to be that big so it will do for now
    if (up)
    {
        while (it != m_FocusWidgetList.end())
        {
            current = *it;

            if ((looped || reachedCurrent) &&
                current->IsVisible() && current->IsEnabled())
                return SetFocusWidget(current);

            if (current == m_CurrentFocusWidget)
                reachedCurrent = true;

            ++it;

            if (it == m_FocusWidgetList.end())
            {
                if (looped)
                    return false;
                else
                {
                    looped = true;
                    it = m_FocusWidgetList.begin();
                }
            }
        }
    }
    else
    {
        it = m_FocusWidgetList.end() - 1;
        while (it != m_FocusWidgetList.begin() - 1)
        {
            current = *it;

            if ((looped || reachedCurrent) &&
                current->IsVisible() && current->IsEnabled())
                return SetFocusWidget(current);

            if (current == m_CurrentFocusWidget)
                reachedCurrent = true;

            --it;

            if (it == m_FocusWidgetList.begin() - 1)
            {
                if (looped)
                    return false;
                else
                {
                    looped = true;
                    it = m_FocusWidgetList.end() - 1;
                }
            }
        }
    }

    return false;
}

bool MythScreenType::BuildFocusList(void)
{
    m_FocusWidgetList.clear();
    m_CurrentFocusWidget = NULL;

    AddFocusableChildrenToList(m_FocusWidgetList);

    if (m_FocusWidgetList.size() > 0)
    {
        SetFocusWidget();
        return true;
    }

    return false;
}

MythScreenStack *MythScreenType::GetScreenStack(void) const
{
    return m_ScreenStack;
}

void MythScreenType::aboutToHide(void)
{
}

void MythScreenType::aboutToShow(void)
{
}

bool MythScreenType::IsDeleting(void) const
{
    return m_IsDeleting;
}

void MythScreenType::SetDeleting(bool deleting)
{
    m_IsDeleting = deleting;
}

bool MythScreenType::Create(void)
{
    return true;
}

void MythScreenType::Init(void)
{
}

void MythScreenType::Close(void)
{
    GetScreenStack()->PopScreen(this);
}

void MythScreenType::SetTextFromMap(QMap<QString, QString> &infoMap)
{
    QList<MythUIType *> *children = GetAllChildren();

    MythUIText *textType;
    QMutableListIterator<MythUIType *> i(*children);
    while (i.hasNext())
    {
        MythUIType *type = i.next();
        if (!type->IsVisible())
            continue;

        textType = dynamic_cast<MythUIText *> (type);
        if (textType && infoMap.contains(textType->objectName()))
        {
            QString newText = textType->GetTemplateText();
            if (newText.isEmpty())
                newText = textType->GetDefaultText();
            QRegExp regexp("%(\\|(.))?([^\\|]+)(\\|(.))?%");
            regexp.setMinimal(true);
            if (newText.contains(regexp))
            {
                int pos = 0;
                QString tempString = newText;
                while ((pos = regexp.indexIn(newText, pos)) != -1)
                {
                    QString key = regexp.cap(3).toLower().trimmed();
                    QString replacement;
                    if (!infoMap.value(key).isEmpty())
                    {
                        replacement = QString("%1%2%3")
                                                .arg(regexp.cap(2))
                                                .arg(infoMap.value(key))
                                                .arg(regexp.cap(5));
                    }
                    tempString.replace(regexp.cap(0), replacement);
                    pos += regexp.matchedLength();
                }
                newText = tempString;
            }
            else
                newText = infoMap.value(textType->objectName());

            textType->SetText(newText);
        }
    }
}

void MythScreenType::ResetMap(QMap<QString, QString> &infoMap)
{
    if (infoMap.isEmpty())
        return;

    QList<MythUIType *> *children = GetAllChildren();

    MythUIText *textType;
    QMutableListIterator<MythUIType *> i(*children);
    while (i.hasNext())
    {
        MythUIType *type = i.next();
        if (!type->IsVisible())
            continue;

        textType = dynamic_cast<MythUIText *> (type);
        if (textType && infoMap.contains(textType->objectName()))
            textType->Reset();
    }
}

bool MythScreenType::keyPressEvent(QKeyEvent *event)
{
    if (m_CurrentFocusWidget && m_CurrentFocusWidget->keyPressEvent(event))
        return true;

    bool handled = false;
    QStringList actions;
    GetMythMainWindow()->TranslateKeyPress("Global", event, actions);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        handled = true;

        if (action == "LEFT")
            NextPrevWidgetFocus(false);
        else if (action == "RIGHT")
            NextPrevWidgetFocus(true);
        else if (action == "UP")
            NextPrevWidgetFocus(false);
        else if (action == "DOWN")
            NextPrevWidgetFocus(true);
        else if (action == "ESCAPE")
            GetScreenStack()->PopScreen();
        else
            handled = false;
    }

    return handled;
}

bool MythScreenType::ParseElement(QDomElement &element)
{
    if (element.tagName() == "area")
    {
        MythRect rect = parseRect(element, false);
        MythRect rectN = parseRect(element);
        QRect screenArea = GetMythMainWindow()->GetUIScreenRect();

        if (rect.x() == -1)
            rectN.setX((screenArea.width() - rectN.width()) / 2);

        if (rect.y() == -1)
            rectN.setY((screenArea.height() - rectN.height()) / 2);

        SetArea(rectN);

        if (m_Area.width() < screenArea.width() ||
            m_Area.height() < screenArea.height())
        {
            m_FullScreen = false;
        }
        else
        {
            m_FullScreen = true;
        }
    }
    else
        return false;

    return true;
}

void MythScreenType::CopyFrom(MythUIType *base)
{
    MythScreenType *st = dynamic_cast<MythScreenType *>(base);
    if (!st)
    {
        VERBOSE(VB_IMPORTANT, "ERROR, bad parsing");
        return;
    }

    m_FullScreen = st->m_FullScreen;
    m_IsDeleting = false;

    MythUIType::CopyFrom(base);

    BuildFocusList();
};

void MythScreenType::CreateCopy(MythUIType *)
{
    VERBOSE(VB_IMPORTANT, "CreateCopy called on screentype - bad.");
}
