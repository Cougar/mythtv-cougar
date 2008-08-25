#ifndef BROWSERDBUTIL_H_
#define BROWSERDBUTIL_H_

// mythbrowser
#include "bookmarkmanager.h"

bool UpgradeBrowserDatabaseSchema(void);

bool FindInDB(const QString &category, const QString& name);
bool InsertInDB(Bookmark *site);
bool InsertInDB(const QString &category, const QString &name, const QString &url);

bool RemoveFromDB(Bookmark *site);
bool RemoveFromDB(const QString &category, const QString &name);

int  GetCategoryList(QStringList &list);
int  GetSiteList(QList<Bookmark*>  &siteList);

#endif
