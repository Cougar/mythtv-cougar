#ifndef SETTINGS_H_
#define SETTINGS_H_
/*
	settings.h

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	A place to set settings

*/

#include <qstring.h>
#include <qstringlist.h>
#include <qsqldatabase.h>

#include "../config.h"

#ifdef MYTHLIB_SUPPORT 
#include <mythtv/mythcontext.h>
#endif

class Settings
{
  public:

    Settings();
    ~Settings();

    int    openDatabase(QSqlDatabase *db);

    QString GetSetting(const QString &key, const QString &defaultval = "");
    QString getSetting(const QString &key, const QString &defaultval = "");

    int     GetNumSetting(const QString &key, int defaultval = 0);
    int     getNumSetting(const QString &key, int defaultval = 0);

    QStringList GetListSetting(const QString &key, const QStringList &defaultval = QStringList());
    QStringList getListSetting(const QString &key, const QStringList &defaultval = QStringList());

    QString GetHostName();
    QString getHostName();

  private:
  
    
};

extern Settings *mfdContext;

#endif  // settings_h_

