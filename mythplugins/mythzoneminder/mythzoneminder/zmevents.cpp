/* ============================================================
 * This program is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General
 * Public License as published by the Free Software Foundation;
 * either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * ============================================================ */

#include <iostream>
#include <cstdlib>

// qt
#include <QKeyEvent>

// myth
#include "mythtv/mythcontext.h"
#include "mythtv/mythdbcon.h"
#include <libmythui/mythmainwindow.h>

// zoneminder
#include "zmevents.h"
#include "zmplayer.h"
#include "zmclient.h"

ZMEvents::ZMEvents(MythScreenStack *parent, const char *name)
         :MythScreenType(parent, name)
{
    gContext->addCurrentLocation("zoneminderevents");

    m_eventList = new vector<Event*>;
    m_eventGrid = NULL;

    m_layout = -1;
    m_currentDate = -1;
    m_currentCamera = -1;
}

ZMEvents::~ZMEvents()
{
    if (!m_eventList)
        delete m_eventList;

    // remember how the user wants to display the event list
    gContext->SaveSetting("ZoneMinderOldestFirst", (m_oldestFirst ? "1" : "0"));
    gContext->SaveSetting("ZoneMinderGridLayout",  m_layout);

    gContext->removeCurrentLocation();
}

bool ZMEvents::Create(void)
{
    bool foundtheme = false;

    // Load the theme for this screen
    foundtheme = LoadWindowFromXML("zoneminder-ui.xml", "zmevents", this);

    if (!foundtheme)
        return false;

    m_eventNoText = dynamic_cast<MythUIText *> (GetChild("eventno_text"));
    m_playButton = dynamic_cast<MythUIButton *> (GetChild("play_button"));
    m_deleteButton = dynamic_cast<MythUIButton *> (GetChild("delete_button"));
    m_cameraSelector = dynamic_cast<MythUIButtonList *> (GetChild("camera_selector"));
    m_dateSelector = dynamic_cast<MythUIButtonList *> (GetChild("date_selector"));

    if (!m_playButton || !m_deleteButton || !m_eventNoText || !m_cameraSelector || !m_dateSelector)
    {
        VERBOSE(VB_IMPORTANT, "Theme is missing critical theme elements.");
        return false;
    }

    if (!BuildFocusList())
        VERBOSE(VB_IMPORTANT, "Failed to build a focuslist. Something is wrong");

    getCameraList();
    getDateList();

    connect(m_cameraSelector, SIGNAL(itemSelected(MythUIButtonListItem*)),
            this, SLOT(cameraChanged()));
    connect(m_dateSelector, SIGNAL(itemSelected(MythUIButtonListItem*)),
            this, SLOT(dateChanged()));

    // play button
    if (m_playButton)
    {
        m_playButton->SetText(tr("Play"));
        connect(m_playButton, SIGNAL(Clicked()), this, SLOT(playPressed()));
    }

    // delete button
    if (m_deleteButton)
    {
        m_deleteButton->SetText(tr("Delete"));
        connect(m_deleteButton, SIGNAL(Clicked()), this, SLOT(deletePressed()));
    }

    getEventList();

    m_oldestFirst = (gContext->GetNumSetting("ZoneMinderOldestFirst", 1) == 1);
    setGridLayout(gContext->GetNumSetting("ZoneMinderGridLayout", 1));

    return true;
}

bool ZMEvents::keyPressEvent(QKeyEvent *event)
{
    if (GetFocusWidget()->keyPressEvent(event))
        return true;

    bool handled = false;
    QStringList actions;
    gContext->GetMainWindow()->TranslateKeyPress("TV Playback", event, actions);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        handled = true;

        if (action == "MENU")
        {
            showMenu();
        }
        else if (action == "ESCAPE")
        {
            if (GetFocusWidget() == m_eventGrid)
                SetFocusWidget(m_cameraSelector);
            else
                handled = false;
        }

        else if (action == "DELETE")
        {
            if (m_deleteButton)
                m_deleteButton->Push();
        }
        else if (action == "PAUSE")
        {
            if (m_playButton)
                m_playButton->Push();
        }
        else if (action == "INFO")
        {
            m_oldestFirst = !m_oldestFirst;
            getEventList();
        }
        else if (action == "1")
            setGridLayout(1);
        else if (action == "2")
            setGridLayout(2);
        else if (action == "3")
            setGridLayout(3);
        else
            handled = false;
    }

    if (!handled && MythScreenType::keyPressEvent(event))
        handled = true;

    return handled;
}

void ZMEvents::getEventList(void)
{
    if (class ZMClient *zm = ZMClient::get())
    {
        QString monitorName = "<ANY>";
        QString date = "<ANY>";

        if (m_cameraSelector->GetValue() != tr("All Cameras"))
            monitorName = m_cameraSelector->GetValue();

        if (m_dateSelector->GetValue() != tr("All Dates"))
            date = m_dateList[m_dateSelector->GetCurrentPos() - 1];

        zm->getEventList(monitorName, m_oldestFirst, date, m_eventList);

        updateUIList();
    }
}

void ZMEvents::updateUIList()
{
    if (!m_eventGrid)
        return;

    m_eventGrid->Reset();

    for (uint i = 0; i < m_eventList->size(); i++)
    {
        Event *event = m_eventList->at(i);

        MythUIButtonListItem *item = new MythUIButtonListItem(m_eventGrid,
                "", NULL, true, MythUIButtonListItem::NotChecked);

        item->SetText(event->eventName);
        item->SetText(event->monitorName, "camera" );
        item->SetText(event->startTime, "time");
        item->SetText(event->length, "length");
    }

    m_eventGrid->SetItemCurrent(m_eventGrid->GetItemFirst());
    eventChanged(m_eventGrid->GetItemCurrent());
}

void ZMEvents::cameraChanged()
{
    if (m_currentCamera == m_cameraSelector->GetCurrentPos())
        return;

    m_currentCamera = m_cameraSelector->GetCurrentPos();

    getEventList();
}

void ZMEvents::dateChanged()
{
    if (m_currentDate == m_dateSelector->GetCurrentPos())
        return;

    m_currentDate = m_dateSelector->GetCurrentPos();

    getEventList();
}

void ZMEvents::eventChanged(MythUIButtonListItem *item)
{
    (void) item;

    if (m_eventNoText)
    {
        if (m_eventGrid->GetCount() > 0)
            m_eventNoText->SetText(QString("%1/%2")
                    .arg(m_eventGrid->GetCurrentPos() + 1).arg(m_eventGrid->GetCount()));
        else
            m_eventNoText->SetText("0/0");
    }

    // update the images for all the visible items
    for (int x = m_eventGrid->GetTopItemPos();
         x < m_eventGrid->GetTopItemPos() + (int)m_eventGrid->GetVisibleCount(); x++)
    {
        if (x < 0 || x > (int)m_eventGrid->GetCount() - 1)
            continue;

        MythUIButtonListItem *gridItem = m_eventGrid->GetItemAt(x);
        if (gridItem && !gridItem->getImage())
        {
            if (x < 0 || x > (int)m_eventList->size() - 1)
                continue;

            Event *event = m_eventList->at(x);
            if (event)
            {
                QImage image;
                if (class ZMClient *zm = ZMClient::get())
                {
                    zm->getAnalyseFrame(event->monitorID,
                                        event->eventID,
                                        0, image);
                    if (!image.isNull())
                    {
                        MythImage *mimage = GetMythPainter()->GetFormatImage();
                        mimage->Assign(image);
                        gridItem->setImage(mimage);
                        mimage->SetChanged();
                    }
                }
            }
        }
    }
}

void ZMEvents::playPressed(void)
{
    if (!m_eventList || m_eventList->size() == 0)
        return;

    m_savedPosition = m_eventGrid->GetCurrentPos();
    Event *event = m_eventList->at(m_savedPosition);
    if (event)
    {
        MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

        ZMPlayer *player = new ZMPlayer(mainStack, "ZMPlayer",
                                        m_eventList, &m_savedPosition);

        connect(player, SIGNAL(Exiting()), this, SLOT(playerExited()));

        if (player->Create())
            mainStack->AddScreen(player);
    }
}

void ZMEvents::playerExited(void)
{
    // refresh the grid and restore the saved position

    if (m_savedPosition > (int)m_eventList->size() - 1)
        m_savedPosition = m_eventList->size() - 1;

    updateUIList();
    m_eventGrid->SetItemCurrent(m_savedPosition);
}

void ZMEvents::deletePressed(void)
{
    if (!m_eventList || m_eventList->size() == 0)
        return;

    m_savedPosition = m_eventGrid->GetCurrentPos();
    Event *event = m_eventList->at(m_savedPosition);
    if (event)
    {
        if (class ZMClient *zm = ZMClient::get())
            zm->deleteEvent(event->eventID);

        MythUIButtonListItem *item = m_eventGrid->GetItemCurrent();
        if (item)
            delete item;

        vector<Event*>::iterator it;
        for (it = m_eventList->begin(); it != m_eventList->end(); it++)
        {
            if (*it == event)
            {
                m_eventList->erase(it);
                break;
            }
        }
    }
}

void ZMEvents::getCameraList(void)
{
    if (class ZMClient *zm = ZMClient::get())
    {
        QStringList cameraList;
        zm->getCameraList(cameraList);
        if (!m_cameraSelector)
            return;

        new MythUIButtonListItem(m_cameraSelector, tr("All Cameras"));

        for (int x = 1; x <= cameraList.count(); x++)
        {
            new MythUIButtonListItem(m_cameraSelector, cameraList[x-1]);
        }
    }
}

void ZMEvents::getDateList(void)
{
    if (class ZMClient *zm = ZMClient::get())
    {
        QString monitorName = "<ANY>";

        if (m_cameraSelector->GetValue() != tr("All Cameras"))
        {
            monitorName = m_cameraSelector->GetValue();
        }

        zm->getEventDates(monitorName, m_oldestFirst, m_dateList);

        QString dateFormat = gContext->GetSetting("ZoneMinderDateFormat", "ddd - dd/MM");

        new MythUIButtonListItem(m_dateSelector, tr("All Dates"));

        for (int x = 0; x < m_dateList.count(); x++)
        {
            QDate date = QDate::fromString(m_dateList[x], Qt::ISODate);
            new MythUIButtonListItem(m_dateSelector, date.toString(dateFormat));
        }
    }
}

void ZMEvents::setGridLayout(int layout)
{
    if (layout < 1 || layout > 3)
        layout = 1;

    if (layout == m_layout)
        return;

    if (m_eventGrid)
        m_eventGrid->Reset();

    m_layout = layout;

    // iterate though the children showing/hiding them as appropriate
    QString name;
    QString layoutName = QString("layout%1").arg(layout);
    QList<MythUIType *> *children = GetAllChildren();

    for (int x = 0; x < children->size(); x++)
    {
        MythUIType *type = children->at(x);
        name = type->objectName();
        if (name.startsWith("layout"))
        {
            if (name.startsWith(layoutName))
                type->SetVisible(true);
            else
                type->SetVisible(false);
        }
    }

    // get the correct grid
    m_eventGrid = dynamic_cast<MythUIButtonList *> (GetChild(layoutName + "_eventlist"));

    if (m_eventGrid)
    {
        connect(m_eventGrid, SIGNAL(itemSelected( MythUIButtonListItem*)),
                this, SLOT(eventChanged(MythUIButtonListItem*)));
        connect(m_eventGrid, SIGNAL(itemClicked( MythUIButtonListItem*)),
                this, SLOT(playPressed()));

        updateUIList();

        BuildFocusList();

        SetFocusWidget(m_eventGrid);
    }
    else
    {
        VERBOSE(VB_IMPORTANT, QString("Theme is missing grid layout (%1).")
                                      .arg(layoutName + "_eventlist"));
        Close();
    }
}

void ZMEvents::showMenu()
{
    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");

    m_menuPopup = new MythDialogBox("Menu", popupStack, "actionmenu");

    if (m_menuPopup->Create())
        popupStack->AddScreen(m_menuPopup);

    m_menuPopup->SetReturnEvent(this, "action");

    m_menuPopup->AddButton(tr("Refresh"), SLOT(getEventList()));
    m_menuPopup->AddButton(tr("Change View"), SLOT(changeView()));
    m_menuPopup->AddButton(tr("Delete All"), SLOT(deleteAll()));
}

void ZMEvents::changeView(void)
{
    setGridLayout(m_layout + 1);
}

void ZMEvents::deleteAll(void)
{
    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");

    QString title = tr("Delete All Events?");
    QString msg = tr("Deleting %1 events in this view.").arg(m_eventGrid->GetCount());

    MythConfirmationDialog *dialog = new MythConfirmationDialog(
            popupStack, title + '\n' + msg, true);

    if (dialog->Create())
        popupStack->AddScreen(dialog);

    connect(dialog, SIGNAL(haveResult(bool)),
            SLOT(doDeleteAll(bool)), Qt::QueuedConnection);
}

void ZMEvents::doDeleteAll(bool doDelete)
{
    if (!doDelete)
        return;

    //delete all events
    if (class ZMClient *zm = ZMClient::get())
    {
        zm->deleteEventList(m_eventList);

        getEventList();
    }
}
