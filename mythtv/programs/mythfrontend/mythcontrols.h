// -*- Mode: c++ -*-
/**
 * @file mythcontrols.h
 * @author Micah F. Galizia <mfgalizi@csd.uwo.ca>
 * @brief Main header for mythcontrols
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
#ifndef MYTHCONTROLS_H
#define MYTHCONTROLS_H

// QT
#include <QList>
#include <QHash>

// MythUI
#include <mythscreentype.h>

#include "keybindings.h"

class MythUIText;
class MythUIButtonList;
class MythUIImage;
class MythDialogBox;

typedef enum { kActionsByContext, kKeysByContext, kContextsByKey, } ViewType;

/**
 *  \class MythControls
 *
 *  \brief The myth controls configuration class.
 */
class MythControls : public MythScreenType
{
    Q_OBJECT

  public:

    MythControls(MythScreenStack *parent, const char *name);
    ~MythControls();

    bool Create(void);
    bool keyPressEvent(QKeyEvent *);
    void customEvent(QEvent*);

    typedef enum
    {
        kContextList,
        kKeyList,
        kActionList
    } ListType;

    // Gets
    QString GetCurrentContext(void);
    QString GetCurrentAction(void);
    QString GetCurrentKey(void);

  protected:
    void Teardown(void);

    // Commands
    bool    LoadUI(void);
    void    LoadData(const QString &hostname);
    void    ChangeButtonFocus(int direction);
    void    ChangeView(void);
    void    SetListContents(MythUIButtonList *uilist,
                            const QStringList & contents,
                            bool arrows = false);
    void    UpdateRightList(void);

    void GrabKey(void);
    void DeleteKey(void);
    void Save(void) { m_bindings->CommitChanges(); }

    // Gets
    uint    GetCurrentButton(void);

    // Functions
    void ResolveConflict(ActionID *conflict, int error_level,
                         const QString &key);
    QString GetTypeDesc(ListType type) const;

  private slots:
    void LeftSelected(MythUIButtonListItem*);
    void RightSelected(MythUIButtonListItem*);
    void LeftPressed(MythUIButtonListItem*);
    void RightPressed(MythUIButtonListItem*);
    void ActionButtonPressed();
    void RefreshKeyInformation(void);
    void AddKeyToAction(QString key, bool ignoreconflict = false);

  private:
    ViewType          m_currentView;
    MythUIType        *m_focusedUIElement;
    MythUIButtonList    *m_leftList;
    MythUIButtonList    *m_rightList;
    MythUIText        *m_description;
    MythUIText        *m_leftDescription;
    MythUIText        *m_rightDescription;
    QList<MythUIButton*> m_actionButtons;
    MythDialogBox     *m_menuPopup;

    KeyBindings       *m_bindings;
    QStringList        m_sortedContexts; ///< sorted list of contexts
    /// actions for a given context
    QHash<QString, QStringList> m_contexts;
    ListType           m_leftListType;
    ListType           m_rightListType;
};


#endif /* MYTHCONTROLS_H */
