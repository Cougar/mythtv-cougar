// -*- Mode: c++ -*-

#ifndef MYTHSTORAGE_H
#define MYTHSTORAGE_H

// Qt headers
#include <QString>

// MythTV headers
#include "mythexp.h"
#include "mythdbcon.h"

class StorageUser
{
  public:
    virtual void SetValue(QString&) = 0;
    virtual QString GetValue(void) const = 0;
};

class MPUBLIC Storage
{
  public:
    Storage() { }
    virtual ~Storage() { }

    virtual void Load(void) = 0;
    virtual void Save(void) = 0;
    virtual void Save(QString /*destination*/) { }
};

class MPUBLIC DBStorage : public Storage
{
  public:
    DBStorage(StorageUser *_user, QString _table, QString _column) :
        user(_user), tablename(_table), columnname(_column) { }

    virtual ~DBStorage() { }

  protected:
    QString GetColumnName(void) const { return columnname; }
    QString GetTableName(void)  const { return tablename;  }

    StorageUser *user;
    QString      tablename;
    QString      columnname;
};

class MPUBLIC SimpleDBStorage : public DBStorage
{
  public:
    SimpleDBStorage(StorageUser *_user,
                    QString _table, QString _column) :
        DBStorage(_user, _table, _column), initval(QString::null) {}
    virtual ~SimpleDBStorage() { }

    virtual void Load(void);
    virtual void Save(void);
    virtual void Save(QString destination);

  protected:
    virtual QString GetWhereClause(MSqlBindings&) const = 0;
    virtual QString GetSetClause(MSqlBindings& bindings) const;

  protected:
    QString initval;
};

class MPUBLIC TransientStorage : public Storage
{
  public:
    TransientStorage() { }
    virtual ~TransientStorage() { }

    virtual void Load(void) { }
    virtual void Save(void) { }
};

class MPUBLIC HostDBStorage : public SimpleDBStorage
{
  public:
    HostDBStorage(StorageUser *_user, const QString &name);

  protected:
    virtual QString GetWhereClause(MSqlBindings &bindings) const;
    virtual QString GetSetClause(MSqlBindings &bindings) const;

  protected:
    QString settingname;
};

class MPUBLIC GlobalDBStorage : public SimpleDBStorage
{
  public:
    GlobalDBStorage(StorageUser *_user, const QString &name);

  protected:
    virtual QString GetWhereClause(MSqlBindings &bindings) const;
    virtual QString GetSetClause(MSqlBindings &bindings) const;

  protected:
    QString settingname;
};

///////////////////////////////////////////////////////////////////////////////

#endif // MYTHSTORAGE_H