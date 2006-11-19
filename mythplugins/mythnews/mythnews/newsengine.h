/* ============================================================
 * File  : newsengine.h
 * Author: Renchi Raju <renchi@pooh.tam.uiuc.edu>
 * Date  : 2003-09-03
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

#ifndef NEWSENGINE_H
#define NEWSENGINE_H

#include <qstring.h>
#include <qptrlist.h>
#include <qobject.h>
#include <qdatetime.h>
#include <qcstring.h>

class QUrlOperator;
class QNetworkOperation;
class NewsSite;

// -------------------------------------------------------

class NewsArticle
{
public:

    typedef QPtrList<NewsArticle> List;

    NewsArticle(NewsSite *parent, const QString& title,
                const QString& desc, const QString& artURL,
                const QString& thumbnail, const QString& mediaURL,
                const QString& enclosure);
    ~NewsArticle();

    const QString& title() const { return m_title; }
    const QString& description() const { return m_desc; }
    const QString& articleURL() const { return m_articleURL; }
    const QString& thumbnail() const { return m_thumbnail; }
    const QString& mediaURL() const { return m_mediaURL; }
    const QString& enclosure() const { return m_enclosure; }

private:

    QString   m_title;
    QString   m_desc;
    NewsSite *m_parent;
    QString m_articleURL;
    QString   m_thumbnail;
    QString   m_mediaURL;
    QString   m_enclosure;
    QString   m_enclosureType;
};

// -------------------------------------------------------

class NewsSite : public QObject
{
    Q_OBJECT

public:

    enum State {
        Retrieving = 0,
        RetrieveFailed,
        WriteFailed,
        Success
    };

    typedef QPtrList<NewsSite> List;

    NewsSite(const QString& name, const QString& url,
             const QDateTime& updated);
    ~NewsSite();

    const QString&   url()  const;
    const QString&   name() const;
    QString          description() const;
    const QDateTime& lastUpdated() const;
    const QString&   imageURL() const;
    unsigned int timeSinceLastUpdate() const; // in minutes

    void insertNewsArticle(NewsArticle* item);
    void clearNewsArticles();
    NewsArticle::List& articleList();

    void retrieve();
    void stop();
    void process();

    bool     successful() const;
    QString  errorMsg() const;

private:

    QString    m_name;
    QString    m_url;
    QString    m_desc;
    QDateTime  m_updated;
    QString    m_destDir;
    QByteArray m_data;
    State      m_state;
    QString    m_errorString;
    QString    m_imageURL;

    NewsArticle::List m_articleList;
    QUrlOperator*     m_urlOp;

    void ReplaceHtmlChar( QString &s);

signals:

    void finished(NewsSite* item);

private slots:

    void slotFinished(QNetworkOperation*);
    void slotGotData(const QByteArray& data,
                     QNetworkOperation* op);
};

#endif /* NEWSENGINE_H */
