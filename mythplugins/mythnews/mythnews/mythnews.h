/* ============================================================
 * File  : mythnews.h
 * Author: Renchi Raju <renchi@pooh.tam.uiuc.edu>
 * Date  : 2003-08-30
 * Description :
 *
 * Copyright 2003 by Renchi Raju

 * This program is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General
 * Public License as published bythe Free Software Foundation;
 * either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * ============================================================ */

#ifndef MYTHNEWS_H
#define MYTHNEWS_H

#include <qsqldatabase.h>

#include <mythtv/uitypes.h>
#include <mythtv/xmlparse.h>
#include <mythtv/mythdialogs.h>

#include "newsengine.h"

class QTimer;
class UIListBtnType;

class MythNews : public MythDialog
{
    Q_OBJECT

public:

    MythNews(QSqlDatabase *db, MythMainWindow *parent,
             const char *name = 0);
    ~MythNews();

private:

    void loadTheme();
    void loadWindow(QDomElement &element);
    void paintEvent(QPaintEvent *e);

    void updateSitesView();
    void updateArticlesView();
    void updateInfoView();
    
    void keyPressEvent(QKeyEvent *e);
    void cursorUp();
    void cursorDown();
    void cursorRight();
    void cursorLeft();

    void cancelRetrieve();
    void processAndShowNews(NewsSite *site);

    QSqlDatabase  *m_DB;
    XMLParse      *m_Theme;

    UIListBtnType *m_UISites;
    UIListBtnType *m_UIArticles;
    QRect          m_SitesRect;
    QRect          m_ArticlesRect;
    QRect          m_InfoRect;
    unsigned int   m_InColumn;

    NewsSite::List m_NewsSites;

    QTimer        *m_RetrieveTimer;
    int            m_TimerTimeout;
    unsigned int   m_UpdateFreq;

    QString        timeFormat;
    QString        dateFormat;

private slots:

    void slotRetrieveNews();
    void slotNewsRetrieved(NewsSite* site);

    void slotSiteSelected(int itemPos);
    void slotArticleSelected(int itemPos);
};

#endif /* MYTHNEWS_H */
