/* ============================================================
 * File  : mythnews.cpp
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

#include <iostream>

#include <qnetwork.h>
#include <qsqlquery.h>
#include <qdatetime.h>
#include <qpainter.h>
#include <qdir.h>
#include <qtimer.h>

#include "mythnews.h"

MythNews::MythNews(QSqlDatabase *db, MythMainWindow *parent,
                   const char *name )
    : MythDialog(parent, name), m_DB(db)
{
    qInitNetworkProtocols ();

    // Setup cache directory

    char *home = getenv("HOME");
    QString fileprefix = QString(home) + "/.mythtv";

    QDir dir(fileprefix);
    if (!dir.exists())
        dir.mkdir(fileprefix);
    fileprefix += "/MythNews";
    dir = QDir(fileprefix);
    if (!dir.exists())
        dir.mkdir(fileprefix);

    // Initialize variables

    m_InColumn     = 0;
    m_UISites      = 0;
    m_UIArticles   = 0;
    m_TimerTimeout = 10*60*1000; 

    timeFormat = gContext->GetSetting("TimeFormat", "h:mm AP");
    dateFormat = gContext->GetSetting("DateFormat", "ddd MMMM d");

    setNoErase();
    loadTheme();

    // Load sites from database

    QSqlQuery query("SELECT name, url, updated FROM newssites ORDER BY name",
                    db);
    if (!query.isActive()) {
        cerr << "MythNews: Error in loading Sites from DB" << endl;
    }
    else {
        QString name;
        QString url;
        QDateTime time;
        while ( query.next() ) {
            name = query.value(0).toString();
            url  = query.value(1).toString();
            time.setTime_t(query.value(2).toUInt());
            m_NewsSites.append(new NewsSite(name,url,time));
        }
    }

    for (NewsSite *site = m_NewsSites.first(); site; site = m_NewsSites.next())
    {
        UIListBtnTypeItem* item =
            new UIListBtnTypeItem(m_UISites, site->name());
        item->setData(site);
    }
    
    // Now do the actual work

    m_RetrieveTimer = new QTimer(this);
    connect(m_RetrieveTimer, SIGNAL(timeout()),
            this, SLOT(slotRetrieveNews()));
    m_UpdateFreq = gContext->GetNumSetting("NewsUpdateFrequency", 30);
    m_RetrieveTimer->start(m_TimerTimeout, false);

    slotRetrieveNews();

    slotSiteSelected(0);
}

MythNews::~MythNews()
{
    m_RetrieveTimer->stop();
    delete m_Theme;
}

void MythNews::loadTheme()
{
    m_Theme = new XMLParse();
    m_Theme->SetWMult(wmult);
    m_Theme->SetHMult(hmult);

    QDomElement xmldata;
    m_Theme->LoadTheme(xmldata, "news", "news-");

    for (QDomNode child = xmldata.firstChild(); !child.isNull();
         child = child.nextSibling()) {
        
        QDomElement e = child.toElement();
        if (!e.isNull()) {

            if (e.tagName() == "font") {
                m_Theme->parseFont(e);
            }
            else if (e.tagName() == "container") {
                QRect area;
                QString name;
                int context;
                m_Theme->parseContainer(e, name, context, area);

                if (name.lower() == "sites")
                    m_SitesRect = area;
                else if (name.lower() == "articles")
                    m_ArticlesRect = area;
                else if (name.lower() == "info")
                    m_InfoRect = area;
            }
            else {
                std::cerr << "Unknown element: " << e.tagName()
                          << std::endl;
                exit(-1);
            }
        }
    }

    LayerSet *container = m_Theme->GetSet("sites");
    if (!container) {
        std::cerr << "MythNews: Failed to get sites container." << std::endl;
        exit(-1);
    }
        
    m_UISites = (UIListBtnType*)container->GetType("siteslist");
    if (!m_UISites) {
        std::cerr << "MythNews: Failed to get sites list area." << std::endl;
        exit(-1);
    }
        
    connect(m_UISites, SIGNAL(itemSelected(UIListBtnTypeItem*)),
            SLOT(slotSiteSelected(UIListBtnTypeItem*)));

    container = m_Theme->GetSet("articles");
    if (!container) {
        std::cerr << "MythNews: Failed to get articles container."
                  << std::endl;
        exit(-1);
    }

    m_UIArticles = (UIListBtnType*)container->GetType("articleslist");
    if (!m_UIArticles) {
        std::cerr << "MythNews: Failed to get articles list area."
                  << std::endl;
        exit(-1);
    }
    
    connect(m_UIArticles, SIGNAL(itemSelected(UIListBtnTypeItem*)),
            SLOT(slotArticleSelected(UIListBtnTypeItem*)));
    
    m_UISites->SetActive(true);
    m_UIArticles->SetActive(false);
}


void MythNews::paintEvent(QPaintEvent *e)
{
    QRect r = e->rect();

    if (r.intersects(m_SitesRect))
        updateSitesView();
    if (r.intersects(m_ArticlesRect))
        updateArticlesView();
    if (r.intersects(m_InfoRect))
        updateInfoView();
}


void MythNews::updateSitesView()
{
    QPixmap pix(m_SitesRect.size());
    pix.fill(this, m_SitesRect.topLeft());
    QPainter p(&pix);

    LayerSet* container = m_Theme->GetSet("sites");
    if (container) {
        container->Draw(&p, 0, 0);
        container->Draw(&p, 1, 0);
        container->Draw(&p, 2, 0);
        container->Draw(&p, 3, 0);
        container->Draw(&p, 4, 0);
        container->Draw(&p, 5, 0);
        container->Draw(&p, 6, 0);
        container->Draw(&p, 7, 0);
        container->Draw(&p, 8, 0);
    }
    p.end();

    bitBlt(this, m_SitesRect.left(), m_SitesRect.top(),
           &pix, 0, 0, -1, -1, Qt::CopyROP);
}

void MythNews::updateArticlesView()
{
    QPixmap pix(m_ArticlesRect.size());
    pix.fill(this, m_ArticlesRect.topLeft());
    QPainter p(&pix);

    LayerSet* container = m_Theme->GetSet("articles");
    if (container) {
        container->Draw(&p, 0, 0);
        container->Draw(&p, 1, 0);
        container->Draw(&p, 2, 0);
        container->Draw(&p, 3, 0);
        container->Draw(&p, 4, 0);
        container->Draw(&p, 5, 0);
        container->Draw(&p, 6, 0);
        container->Draw(&p, 7, 0);
        container->Draw(&p, 8, 0);
    }
    p.end();

    bitBlt(this, m_ArticlesRect.left(), m_ArticlesRect.top(),
           &pix, 0, 0, -1, -1, Qt::CopyROP);
}

void MythNews::updateInfoView()
{
    QPixmap pix(m_InfoRect.size());
    pix.fill(this, m_InfoRect.topLeft());
    QPainter p(&pix);

    LayerSet* container = m_Theme->GetSet("info");
    if (container)
    {
        NewsSite    *site     = 0;
        NewsArticle *article  = 0;

        UIListBtnTypeItem *siteUIItem = m_UISites->GetItemCurrent();
        if (siteUIItem && siteUIItem->getData()) 
            site = (NewsSite*) siteUIItem->getData();
        
        UIListBtnTypeItem *articleUIItem = m_UIArticles->GetItemCurrent();
        if (articleUIItem && articleUIItem->getData())
            article = (NewsArticle*) articleUIItem->getData();
        
        if (m_InColumn == 1) {

            if (article)
            {
                UITextType *ttype =
                    (UITextType *)container->GetType("title");
                if (ttype)
                    ttype->SetText(article->title());

                ttype =
                    (UITextType *)container->GetType("description");
                if (ttype)
                    ttype->SetText(article->description());
            }
        }
        else {

            if (site)
            {
                UITextType *ttype =
                    (UITextType *)container->GetType("title");
                if (ttype)
                    ttype->SetText(site->name());

                ttype =
                    (UITextType *)container->GetType("description");
                if (ttype)
                    ttype->SetText(site->description());
            }
        }

        UITextType *ttype =
            (UITextType *)container->GetType("updated");
        if (ttype) {

            if (site)
            {
                QString text(tr("Updated") + "\n");
                QDateTime updated(site->lastUpdated());
                if (updated.toTime_t() != 0) {
                    text += site->lastUpdated().toString(dateFormat) + "\n";
                    text += site->lastUpdated().toString(timeFormat);
                }
                else
                    text += tr("Unknown");
                ttype->SetText(text);
            }
        }

        container->Draw(&p, 0, 0);
        container->Draw(&p, 1, 0);
        container->Draw(&p, 2, 0);
        container->Draw(&p, 3, 0);
        container->Draw(&p, 4, 0);
        container->Draw(&p, 5, 0);
        container->Draw(&p, 6, 0);
        container->Draw(&p, 7, 0);
        container->Draw(&p, 8, 0);
    }

    p.end();


    bitBlt(this, m_InfoRect.left(), m_InfoRect.top(),
           &pix, 0, 0, -1, -1, Qt::CopyROP);
}

void MythNews::keyPressEvent(QKeyEvent *e)
{
    if (!e) return;

    bool handled = false;
    QStringList actions;
    gContext->GetMainWindow()->TranslateKeyPress("News", e, actions);
   
    for (unsigned int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        handled = true;

        if (action == "UP")
            cursorUp();
        else if (action == "PAGEUP")
             cursorUp(true);
        else if (action == "DOWN")
            cursorDown();
        else if (action == "PAGEDOWN")
             cursorDown(true);
        else if (action == "LEFT")
            cursorLeft();
        else if (action == "RIGHT")
            cursorRight();
        else if (action == "RETRIEVENEWS")
            slotRetrieveNews();
        else if(action == "SELECT")
            slotViewArticle();
        else if (action == "CANCEL")
        {
            cancelRetrieve();
        }
        else
            handled = false;
    }

    if (!handled)
        MythDialog::keyPressEvent(e);
}

void MythNews::cursorUp(bool page)
{
    UIListBtnType::MovementUnit unit = page ? UIListBtnType::MovePage : UIListBtnType::MoveItem;

    if (m_InColumn == 0) {
        m_UISites->MoveUp(unit);
    }
    else {
        m_UIArticles->MoveUp(unit);
    }
}

void MythNews::cursorDown(bool page)
{
    UIListBtnType::MovementUnit unit = page ? UIListBtnType::MovePage : UIListBtnType::MoveItem;

    if (m_InColumn == 0) {
        m_UISites->MoveDown(unit);
    }
    else {
        m_UIArticles->MoveDown(unit);
    }
}

void MythNews::cursorRight()
{
    if (m_InColumn == 1) return;

    m_InColumn++;

    m_UISites->SetActive(false);
    m_UIArticles->SetActive(true);

    update(m_SitesRect);
    update(m_ArticlesRect);
    update(m_InfoRect);
}

void MythNews::cursorLeft()
{
    if (m_InColumn == 0) return;

    m_InColumn--;

    m_UISites->SetActive(true);
    m_UIArticles->SetActive(false);

    update(m_SitesRect);
    update(m_ArticlesRect);
    update(m_InfoRect);
}

void MythNews::slotRetrieveNews()
{
    if (m_NewsSites.count() == 0)
        return;

    cancelRetrieve();

    m_RetrieveTimer->stop();

    for (NewsSite* site = m_NewsSites.first(); site; site = m_NewsSites.next())
    {
        site->stop();
        connect(site, SIGNAL(finished(NewsSite*)),
                this, SLOT(slotNewsRetrieved(NewsSite*)));
    }

    for (NewsSite* site = m_NewsSites.first(); site; site = m_NewsSites.next())
    {
        if (site->timeSinceLastUpdate() > m_UpdateFreq)
            site->retrieve();
        else
            processAndShowNews(site);
    }

    m_RetrieveTimer->start(m_TimerTimeout, false);
}

void MythNews::slotNewsRetrieved(NewsSite* site)
{
    unsigned int updated = site->lastUpdated().toTime_t();

    QSqlQuery query("UPDATE newssites SET updated=" +
                    QString::number(updated) +
                    " WHERE name='" +
                    site->name() + "'",  m_DB);
    if (!query.isActive()) {
        cerr << "MythNews: Error in updating time in DB" << endl;
    }

    processAndShowNews(site);
}

void MythNews::cancelRetrieve()
{
    for (NewsSite* site = m_NewsSites.first(); site;
         site = m_NewsSites.next()) {
        site->stop();
        processAndShowNews(site);
    }
}

void MythNews::processAndShowNews(NewsSite* site)
{
    if (!site)
        return;

    site->process();

    UIListBtnTypeItem *siteUIItem = m_UISites->GetItemCurrent();
    if (!siteUIItem || !siteUIItem->getData())
        return;
    
    if (site == (NewsSite*) siteUIItem->getData()) {

        m_UIArticles->Reset();

        for (NewsArticle* article = site->articleList().first(); article;
             article = site->articleList().next()) {
            UIListBtnTypeItem* item =
                new UIListBtnTypeItem(m_UIArticles, article->title());
            item->setData(article);
        }

        update(m_ArticlesRect);
        update(m_InfoRect);
    } 
}

void MythNews::slotSiteSelected(UIListBtnTypeItem *item)
{
    if (!item || !item->getData())
        return;
    
    m_UIArticles->Reset();

    NewsSite *site = (NewsSite*) item->getData();

    for (NewsArticle* article = site->articleList().first(); article;
         article = site->articleList().next()) {
        UIListBtnTypeItem* item =
            new UIListBtnTypeItem(m_UIArticles, article->title());
        item->setData(article);
    }

    update(m_SitesRect);
    update(m_ArticlesRect);
    update(m_InfoRect);
}

void MythNews::slotArticleSelected(UIListBtnTypeItem*)
{
    update(m_ArticlesRect);
    update(m_InfoRect);
}

void MythNews::slotViewArticle()
{
    NewsArticle *article  = NULL;
    UIListBtnTypeItem *articleUIItem = m_UIArticles->GetItemCurrent();

    if (articleUIItem && articleUIItem->getData())
        article = (NewsArticle*) articleUIItem->getData();
    if (article)
    {
        QString cmd = gContext->GetSetting("WebBrowserCommand", 
                                           PREFIX "/bin/mythbrowser");
        QString zoom = QString(" -z %1")
                            .arg(gContext->GetNumSetting("WebBrowserZoomLevel",
                                                         200));
        cmd += zoom;
        cmd += article->articleURL();
        myth_system( cmd );
    }
}
