#ifndef _WEATHERUTILS_H_
#define _WEATHERUTILS_H_

// QT headers
#include <QMap>
#include <QMultiHash>
#include <QString>
#include <QDomElement>
#include <QFile>

// MythTV headers
#include <mythcontext.h>

#define SI_UNITS 0
#define ENG_UNITS 1
#define DEFAULT_UPDATE_TIMEOUT (5*60*1000)
#define DEFAULT_SCRIPT_TIMEOUT (60*1000)

typedef unsigned char units_t;
typedef QMap<QString, QString> DataMap;

class TypeListInfo
{
  public:
    TypeListInfo(const QString &_name)
        : name(_name), location(QString::null), src(NULL)
    {
        name.detach();
    }
    TypeListInfo(const QString &_name, const QString &_location)
        : name(_name), location(_location), src(NULL)
    {
        name.detach();
        location.detach();
    }
    TypeListInfo(const QString &_name, const QString &_location,
                 struct ScriptInfo *_src)
        : name(_name), location(_location), src(_src)
    {
        name.detach();
        location.detach();
    }

  public:
    QString name;
    QString location;
    struct ScriptInfo *src;
};
typedef QMultiHash<QString, TypeListInfo> TypeListMap;

class ScreenListInfo
{
  public:
    TypeListInfo GetCurrentTypeList(void) const;

  public:
    QString name;
    TypeListMap types;
    QStringList dataTypes;
    QString helptxt;
    QStringList sources;
    units_t units;
    bool hasUnits;
    bool multiLoc;
};

Q_DECLARE_METATYPE(ScreenListInfo *); 

typedef QMap<QString, ScreenListInfo *> ScreenListMap;

ScreenListMap loadScreens();
QStringList loadScreen(QDomElement ScreenListInfo);

#endif
