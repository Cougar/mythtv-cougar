// -*- Mode: c++ -*-
/**
 * @file mythcontrols.cpp
 * @author Micah F. Galizia <mfgalizi@csd.uwo.ca>
 * @brief Main mythcontrols class.
 *
 * Note that the keybindings are fetched all at once, and cached for
 * this host.  This avoids pelting the database everytime the user
 * changes their selection.  This makes a HUGE difference in delay.
 *
 * Copyright (C) 2005 Micah Galizia
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA
 */

// Qt headers
#include <qnamespace.h>
#include <qstringlist.h>
#include <qapplication.h>
#include <q3buttongroup.h>
#include <q3deepcopy.h>
#include <QKeyEvent>

// MythTV headers
#include <mythtv/mythcontext.h>
#include <mythtv/libmythui/mythmainwindow.h>

// MythControls headers
#include "mythcontrols.h"
#include "keygrabber.h"

#define LOC QString("MythControls: ")
#define LOC_ERR QString("MythControls, Error: ")
#define CAPTION_CONTEXT QString("Contexts")
#define CAPTION_ACTION QString("Actions")
#define CAPTION_KEY QString("Keys")

/** \fn MythControls::MythControls(MythMainWindow*, bool&)
 *  \brief Creates a new MythControls wizard
 *  \param parent Pointer to the screen stack
 *  \param name The name of the window
 */
MythControls::MythControls(MythScreenStack *parent, const char *name)
    : MythScreenType (parent, name)
{
    m_currentView = kActionsByContext;
    m_leftList = m_rightList = NULL;
    m_description = m_leftDescription = m_rightDescription = NULL;
    m_bindings = NULL;

    m_contexts.setAutoDelete(true);

    m_leftListType  = kContextList;
    m_rightListType = kActionList;

    m_menuPopup = NULL;
}

MythControls::~MythControls()
{
    Teardown();
}

void MythControls::Teardown(void)
{
    if (m_bindings)
    {
        delete m_bindings;
        m_bindings = NULL;
    }

    m_contexts.clear();
}

/** \fn MythControls::Create(void)
 *  \brief Loads UI elements from theme
 *
 *  \return true if all UI elements load successfully.
 */
bool MythControls::Create(void)
{
    bool foundtheme = false;

    // Load the theme for this screen
    foundtheme = LoadWindowFromXML("controls-ui.xml", "controls", this);

    if (!foundtheme)
        return false;

    m_description = dynamic_cast<MythUIText *>
                (GetChild("description"));
    m_leftList = dynamic_cast<MythListButton *>
                (GetChild("leftlist"));
    m_rightList = dynamic_cast<MythListButton *>
                (GetChild("rightlist"));
    m_leftDescription = dynamic_cast<MythUIText *>
                (GetChild("leftdesc"));
    m_rightDescription = dynamic_cast<MythUIText *>
                (GetChild("rightdesc"));

    if (!m_description || !m_leftList || !m_rightList ||
            !m_leftDescription || !m_rightDescription)
    {
        VERBOSE(VB_IMPORTANT, "Theme is missing critical theme elements.");
        return false;
    }

    connect(m_leftList,  SIGNAL(itemSelected(MythListButtonItem*)),
            this, SLOT(LeftSelected(MythListButtonItem*)));
    connect(m_leftList,  SIGNAL(itemClicked(MythListButtonItem*)),
            this, SLOT(LeftPressed(MythListButtonItem*)));

    connect(m_rightList, SIGNAL(itemSelected(MythListButtonItem*)),
            this, SLOT(RightSelected(MythListButtonItem*)));
    connect(m_rightList,  SIGNAL(itemClicked(MythListButtonItem*)),
            this, SLOT(RightPressed(MythListButtonItem*)));
    connect(m_rightList, SIGNAL(TakingFocus()),
            this, SLOT(RefreshKeyInformation()));

    for (uint i = 0; i < Action::kMaximumNumberOfBindings; i++)
    {
        MythUIButton *button = dynamic_cast<MythUIButton *>
                (GetChild(QString("action_%1").arg(i)));

        if (!button)
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR +
                    QString("Unable to load action button action_%1").arg(i));

            return false;
        }

        connect(button, SIGNAL(buttonPressed()),
                this, SLOT(ActionButtonPressed()));

        m_actionButtons.append(button);
    }

    if (!BuildFocusList())
        VERBOSE(VB_IMPORTANT, "Failed to build a focuslist. Something is wrong");

    SetFocusWidget(m_leftList);
    m_leftList->SetCanTakeFocus();
    m_leftList->SetActive(true);
    m_rightList->SetCanTakeFocus();
    m_rightList->SetActive(false);

    LoadData(gContext->GetHostName());

    /* start off with the actions by contexts view */
    m_currentView = kActionsByContext;
    SetListContents(m_leftList, m_bindings->GetContexts(), true);
    UpdateRightList();

    return true;
}

/** \fn MythControls::ChangeButtonFocus(int)
 *  \brief Change button focus in a particular direction
 *  \param direction +1 moves focus to the right, -1 moves to the left,
 *                   and 0 changes the focus to the first button.
 */
void MythControls::ChangeButtonFocus(int direction)
{
    if ((m_leftListType != kContextList) || (m_rightListType != kActionList))
        return;

    if (direction == 0)
    {
        SetFocusWidget(m_actionButtons.at(0));
        m_rightList->SetActive(false);
        return;
    }
}

/**
 *  \brief Slot handling a button being pressed in the left list
 */
void MythControls::LeftPressed(MythListButtonItem *item)
{
    NextPrevWidgetFocus(true);
}

/**
 *  \brief Slot handling a button being pressed in the left list
 */
void MythControls::RightPressed(MythListButtonItem *item)
{
    if (m_currentView == kActionsByContext)
        ChangeButtonFocus(0);
}

/**
 *  \brief Slot handling a button being pressed in the left list
 */
void MythControls::ActionButtonPressed()
{
    QString key = GetCurrentKey();
    if (!key.isEmpty())
    {
        QString label = tr("Modify Action");

        MythScreenStack *popupStack =
                                GetMythMainWindow()->GetStack("popup stack");

        m_menuPopup =
                new MythDialogBox(label, popupStack, "actionmenu");

        if (m_menuPopup->Create())
            popupStack->AddScreen(m_menuPopup);

        m_menuPopup->SetReturnEvent(this, "action");

        m_menuPopup->AddButton(tr("Set Binding"));
        m_menuPopup->AddButton(tr("Remove Binding"));
        m_menuPopup->AddButton(tr("Cancel"));
    }
    else // for blank keys, no reason to ask what to do
        GrabKey();
}

/// \brief Change the view.
void MythControls::ChangeView(void)
{
    QString label = tr("Change View");

    MythScreenStack *popupStack =
                            GetMythMainWindow()->GetStack("popup stack");

    m_menuPopup =
            new MythDialogBox(label, popupStack, "mcviewmenu");

    if (m_menuPopup->Create())
        popupStack->AddScreen(m_menuPopup);

    m_menuPopup->SetReturnEvent(this, "view");

    m_menuPopup->AddButton(tr("Actions By Context"));
    m_menuPopup->AddButton(tr("Contexts By Key"));
    m_menuPopup->AddButton(tr("Keys By Context"));
    m_menuPopup->AddButton(tr("Cancel"));

}

bool MythControls::keyPressEvent(QKeyEvent *event)
{
    // Always send keypress events to the currently focused widget first
    if (GetFocusWidget()->keyPressEvent(event))
        return true;

    bool handled = false;
    bool escape = false;
    QStringList actions;
    gContext->GetMainWindow()->TranslateKeyPress("Controls", event, actions);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        handled = true;

        if (action == "MENU" || action == "INFO")
        {
            QString label = tr("Options");

            MythScreenStack *popupStack =
                                    GetMythMainWindow()->GetStack("popup stack");

            m_menuPopup =
                    new MythDialogBox(label, popupStack, "optionmenu");

            if (m_menuPopup->Create())
                popupStack->AddScreen(m_menuPopup);

            m_menuPopup->SetReturnEvent(this, "option");

            m_menuPopup->AddButton(tr("Save"));
            m_menuPopup->AddButton(tr("Change View"));
            m_menuPopup->AddButton(tr("Cancel"));
        }
        else if (action == "ESCAPE")
        {
            escape = true;

            handled = false;

            if (m_bindings->HasChanges())
            {
                /* prompt user to save changes */
                QString label = tr("Exiting, but there are unsaved changes."
                                   "Which would you prefer?");

                MythScreenStack *popupStack =
                                        GetMythMainWindow()->GetStack("popup stack");

                m_menuPopup =
                        new MythDialogBox(label, popupStack, "exitmenu");

                if (m_menuPopup->Create())
                    popupStack->AddScreen(m_menuPopup);

                m_menuPopup->SetReturnEvent(this, "exit");

                m_menuPopup->AddButton(tr("Save then Exit"));
                m_menuPopup->AddButton(tr("Exit without saving changes"));
            }
            else
                GetScreenStack()->PopScreen();
        }
        else
            handled = false;
    }

    if (MythScreenType::keyPressEvent(event))
        handled = true;

    return handled;
}

/** \fn MythControls::LeftSelected(MythListButtonItem*)
 *  \brief Refreshes the right list when an item in the
 *         left list is selected
 */
void MythControls::LeftSelected(MythListButtonItem*)
{
    UpdateRightList();
}

/** \fn MythControls::RightSelected(MythListButtonItem*)
 *  \brief Refreshes key information when an item in the
 *         right list is selected
 */
void MythControls::RightSelected(MythListButtonItem*)
{
    RefreshKeyInformation();
}


/** \brief Set the contents of a list.
 *  \param uilist The list being changed.
 *  \param contents The contents of the list.
 *  \param arrows True to draw with arrows, otherwise arrows are not drawn.
 */
void MythControls::SetListContents(
    MythListButton *uilist, const QStringList &contents, bool arrows)
{
    // remove all strings from the current list
    uilist->Reset();

    // add each new string
    for (int i = 0; i < contents.size(); i++)
    {
        MythListButtonItem *item = new MythListButtonItem(uilist, contents[i]);
        item->setDrawArrow(arrows);
    }
}

/** \brief Update the right list. */
void MythControls::UpdateRightList(void)
{
    // get the selected item in the right list.
    MythListButtonItem *item = m_leftList->GetItemCurrent();

    if (item)
    {
        QString rtstr = item->text();

        switch(m_currentView)
        {
        case kActionsByContext:
            SetListContents(m_rightList, *(m_contexts[rtstr]));
            break;
        case kKeysByContext:
            SetListContents(m_rightList, m_bindings->GetContextKeys(rtstr));
            break;
        case kContextsByKey:
            SetListContents(m_rightList, m_bindings->GetKeyContexts(rtstr));
            break;
        }
    }
}

/** \fn MythControls::RefreshKeyInformation(void)
 *  \brief Updates the list of keys that are shown and the
 *         description of the action.
 */
void MythControls::RefreshKeyInformation(void)
{
    for (uint i = 0; i < Action::kMaximumNumberOfBindings; i++)
        m_actionButtons.at(i)->SetText("");

    if (GetFocusWidget() == m_leftList)
    {
        m_description->SetText("");
        return;
    }

    const QString context = GetCurrentContext();
    const QString action  = GetCurrentAction();

    QString desc = m_bindings->GetActionDescription(context, action);
    m_description->SetText(desc);

    QStringList keys = m_bindings->GetActionKeys(context, action);
    for (int i = 0; (i < keys.count()) &&
             (i < Action::kMaximumNumberOfBindings); i++)
    {
        m_actionButtons.at(i)->SetText(keys[i]);
    }
}


/** \brief Get the currently selected context string.
 *
 *   If no context is selected, an empty string is returned.
 *
 *  \return The currently selected context string.
 */
QString MythControls::GetCurrentContext(void)
{
    if (m_leftListType == kContextList)
        return m_leftList->GetItemCurrent()->text();

    if (GetFocusWidget() == m_leftList)
        return QString::null;

    QString desc = m_rightList->GetItemCurrent()->text();
    int loc = desc.find(" => ");
    if (loc == -1)
        return QString::null; // Should not happen

    if (m_rightListType == kContextList)
        return desc.left(loc);

    return desc.mid(loc + 4);
}

/** \brief Get the currently selected action string.
 *
 *   If no action is selected, an empty string is returned.
 *
 *  \return The currently selected action string.
 */
QString MythControls::GetCurrentAction(void)
{
    if (m_leftListType == kActionList)
    {
        if (m_leftList && m_leftList->GetItemCurrent())
            return Q3DeepCopy<QString>(m_leftList->GetItemCurrent()->text());
        return QString::null;
    }

    if (GetFocusWidget() == m_leftList)
        return QString::null;

    if (!m_rightList || !m_rightList->GetItemCurrent())
        return QString::null;

    QString desc = m_rightList->GetItemCurrent()->text();
    if (m_leftListType == kContextList && m_rightListType == kActionList)
        return Q3DeepCopy<QString>(desc);

    int loc = desc.find(" => ");
    if (loc == -1)
        return QString::null; // should not happen..

    if (m_rightListType == kActionList)
        return desc.left(loc);

    QString rv = desc.mid(loc+4);
    if (rv == "<none>")
        return QString::null;

    return rv;
}

/** \brief Returns the focused button, or
 *         Action::kMaximumNumberOfBindings if no buttons are focued.
 */
uint MythControls::GetCurrentButton(void)
{
    for (uint i = 0; i < Action::kMaximumNumberOfBindings; i++)
    {
        MythUIButton *button = m_actionButtons.at(i);
        MythUIType *uitype = GetFocusWidget();
        if (uitype == button)
            return i;
    }

    return Action::kMaximumNumberOfBindings;
}

/** \brief Get the currently selected key string
 *
 *   If no key is selected, an empty string is returned.
 *
 *  \return The currently selected key string
 */
QString MythControls::GetCurrentKey(void)
{
    if (m_leftListType == kKeyList)
        return m_leftList->GetItemCurrent()->text();

    if (GetFocusWidget() == m_leftList)
        return QString::null;

    if ((m_leftListType == kContextList) && (m_rightListType == kActionList))
    {
        QString context = GetCurrentContext();
        QString action = GetCurrentAction();
        uint b = GetCurrentButton();
        QStringList keys = m_bindings->GetActionKeys(context, action);

        if (b < (uint)keys.count())
            return keys[b];

        return QString::null;
    }

    QString desc = m_rightList->GetItemCurrent()->text();
    int loc = desc.find(" => ");
    if (loc == -1)
        return QString::null; // Should not happen


    if (m_rightListType == kKeyList)
        return desc.left(loc);

    return desc.mid(loc + 4);
}

/** \fn MythControls::LoadData(const QString&)
 *  \brief Load the settings for a particular host.
 *  \param hostname The host to load settings for.
 */
void MythControls::LoadData(const QString &hostname)
{
    /* create the key bindings and the tree */
    m_bindings = new KeyBindings(hostname);
    m_sortedContexts = m_bindings->GetContexts();

    /* Alphabetic order, but jump and global at the top  */
    m_sortedContexts.sort();
    m_sortedContexts.remove(ActionSet::kJumpContext);
    m_sortedContexts.remove(ActionSet::kGlobalContext);
    m_sortedContexts.insert(m_sortedContexts.begin(),
                            ActionSet::kGlobalContext);
    m_sortedContexts.insert(m_sortedContexts.begin(),
                            ActionSet::kJumpContext);

    QStringList actions;
    for (int i = 0; i < m_sortedContexts.size(); i++)
    {
        actions = m_bindings->GetActions(m_sortedContexts[i]);
        actions.sort();
        m_contexts.insert(m_sortedContexts[i], new QStringList(actions));
    }
}

/** \fn MythControls::DeleteKey(void)
 *  \brief Delete the currently active key to action mapping
 *
 *   TODO FIXME This code needs work to support deleteKey
 *              in any mode exc. Context/Action
 */
void MythControls::DeleteKey(void)
{
    QString context = GetCurrentContext();
    QString key     = GetCurrentKey();
    QString action  = GetCurrentAction();

    if (context.isEmpty() || key.isEmpty() || action.isEmpty())
    {
        VERBOSE(VB_IMPORTANT, "Unable to delete binding, missing information");
        return;
    }

    if (m_bindings->RemoveActionKey(context, action, key))
    {
        RefreshKeyInformation();
        return;
    }

    QString label = tr("This action is manditory and needs at least one key "
                       "bound to it. Instead, try rebinding with another key.");

    MythScreenStack *popupStack =
                            GetMythMainWindow()->GetStack("popup stack");

    m_menuPopup =
            new MythDialogBox(label, popupStack, "mandatorydelete");

    if (m_menuPopup->Create())
        popupStack->AddScreen(m_menuPopup);

    m_menuPopup->SetReturnEvent(this, "mandatorydelete");

    m_menuPopup->AddButton(tr("Ok"));
}

/** \fn ResolveConflict(ActionID*, int, key)
 *  \brief Resolve a potential conflict
 *  \return true if the conflict should be bound, false otherwise.
 */
void MythControls::ResolveConflict(ActionID *conflict, int error_level,
                                    QString key)
{
    if (!conflict)
        return;

    QString label = tr("This key binding conflicts with %1 in the %2 context.")
        .arg(conflict->GetAction()).arg(conflict->GetContext());

    if (KeyBindings::kKeyBindingError == error_level)
        label.append(tr(" Unable to bind key."));
    else
        label.append(tr(" Do you want to bind it anyway?"));

    MythScreenStack *popupStack =
                            GetMythMainWindow()->GetStack("popup stack");

    m_menuPopup =
            new MythDialogBox(label, popupStack, "conflictmenu");

    if (m_menuPopup->Create())
        popupStack->AddScreen(m_menuPopup);

    m_menuPopup->SetReturnEvent(this, "conflict");

    if (KeyBindings::kKeyBindingError == error_level)
    {
        m_menuPopup->AddButton(tr("Ok"));
    }
    else
    {
        m_menuPopup->AddButton(tr("Cancel"));
        m_menuPopup->AddButton(tr("Bind Key"), (void*)&key);
    }

    delete conflict;
}

void MythControls::GrabKey(void)
{
    /* grab a key from the user */
    MythScreenStack *popupStack =
                            GetMythMainWindow()->GetStack("popup stack");

    KeyGrabPopupBox *keyGrabPopup = new KeyGrabPopupBox(popupStack);

    if (keyGrabPopup->Create())
        popupStack->AddScreen(keyGrabPopup);

    keyGrabPopup->SetReturnEvent(this, "keygrab");
}

/** \fn MythControls::AddKeyToAction(void)
 *  \brief Add a key to the currently selected action.
 *
 *  TODO FIXME This code needs work to support deleteKey
 *             in any mode exc. Context/Action
 *  TODO FIXME This code needs work to deal with multiple
 *             binding conflicts.
 */
void MythControls::AddKeyToAction(QString key, bool ignoreconflict)
{
    QString     action  = GetCurrentAction();
    QString     context = GetCurrentContext();
    QStringList keys    = m_bindings->GetActionKeys(context, action);

    // Don't recreating an existing binding...
    int binding_index = GetCurrentButton();
    if ((binding_index >= (int)Action::kMaximumNumberOfBindings) ||
        ((binding_index < keys.size()) && (keys[binding_index] == key)))
    {
        return;
    }

    if (!ignoreconflict)
    {
        // Check for first of the potential conflicts.
        int err_level;
        ActionID *conflict = m_bindings->GetConflict(context, key, err_level);
        if (conflict)
        {
            ResolveConflict(conflict, err_level, key);

            return;
        }
    }

    if (binding_index < keys.count())
        m_bindings->ReplaceActionKey(context, action, key,
                                     keys[binding_index]);
    else
        m_bindings->AddActionKey(context, action, key);

    RefreshKeyInformation();
}

void MythControls::customEvent(QEvent *event)
{

    if (event->type() == kMythDialogBoxCompletionEventType)
    {
        DialogCompletionEvent *dce =
                                dynamic_cast<DialogCompletionEvent*>(event);

        QString resultid= dce->GetId();
        int buttonnum  = dce->GetResult();

        if (resultid == "action")
        {
            if (buttonnum == 0)
                GrabKey();
            else if (buttonnum == 1)
                DeleteKey();
        }
        else if (resultid == "option")
        {
            if (buttonnum == 0)
                Save();
            else if (buttonnum == 1)
                ChangeView();
        }
        else if (resultid == "exit")
        {
            if (buttonnum == 0)
                Save();

            GetMythMainWindow()->GetMainStack()->PopScreen();
        }
        else if (resultid == "view")
        {
            QStringList contents;
            QString leftcaption, rightcaption;

            if (buttonnum == 0)
            {
                leftcaption = tr(CAPTION_CONTEXT);
                rightcaption = tr(CAPTION_ACTION);
                m_currentView = kActionsByContext;
                contents = m_bindings->GetContexts();
            }
            else if (buttonnum == 1)
            {
                leftcaption = tr(CAPTION_CONTEXT);
                rightcaption = tr(CAPTION_KEY);
                m_currentView = kKeysByContext;
                contents = m_bindings->GetContexts();
            }
            else if (buttonnum == 2)
            {
                leftcaption = tr(CAPTION_KEY);
                rightcaption = tr(CAPTION_CONTEXT);
                m_currentView = kContextsByKey;
                contents = m_bindings->GetKeys();
            }
            else
                return;

            m_leftDescription->SetText(leftcaption);
            m_rightDescription->SetText(rightcaption);

            SetListContents(m_leftList, contents, true);
            RefreshKeyInformation();
            UpdateRightList();

            if (GetFocusWidget() != m_leftList)
                SetFocusWidget(m_leftList);
        }
        else if (resultid == "keygrab")
        {
            if (buttonnum == 0)
            {
                QString key = *(QString *)dce->GetResultData();
                AddKeyToAction(key);
            }
        }
        else if (resultid == "conflict")
        {
            if (buttonnum == 1)
            {
                QString key = *(QString *)dce->GetResultData();
                AddKeyToAction(key, true);
            }
        }

        m_menuPopup = NULL;
    }

}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
