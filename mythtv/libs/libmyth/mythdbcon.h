#ifndef MYTHDBCON_H_
#define MYTHDBCON_H_

#include <qsqldatabase.h>
#include <qmutex.h>
#include <qstring.h>

#include <iostream>
using namespace std;

#include "mythcontext.h"

// small wrapper class for QSqlDatabase
class MythSqlDatabase
{
  public:
    MythSqlDatabase(const QString &name)
    {
        m_name = name;
        m_db = QSqlDatabase::addDatabase("QMYSQL3", name);

        if (!m_db)
        {
            cerr << "Unable to init db connection: " << name << endl;
            return;
        }

        if (!gContext->OpenDatabase(m_db))
        {
            cerr << "Unable to open db connect: " << name << endl;
            QSqlDatabase::removeDatabase(name);
            m_db = NULL;
            return;
        }

        m_dblock = new QMutex();
    }

   ~MythSqlDatabase()
    {
        if (m_db)
        {
            m_db->close();
            QSqlDatabase::removeDatabase(m_name);
            m_db = NULL;
        }

        if (m_dblock)
            delete m_dblock;
    }

    bool isOpen(void) 
    { 
        if (m_db && m_db->isOpen()) 
            return true;
        return false;
    }

    QSqlDatabase *db(void) { return m_db; }
    QMutex *mutex(void) { return m_dblock; }
    
    void lock(void) { m_dblock->lock(); }
    void unlock(void) { m_dblock->unlock(); }

  private:
    QString m_name;
    QMutex *m_dblock;
    QSqlDatabase *m_db;
};

#endif    
