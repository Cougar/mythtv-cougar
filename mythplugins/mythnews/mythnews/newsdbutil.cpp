
#include <mythtv/mythdbcon.h>

#include "newsdbutil.h"

bool findInDB(const QString& name)
{
    bool val = false;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT name FROM newssites WHERE name = :NAME ;");
    query.bindValue(":NAME", name);
    if (!query.exec() || !query.isActive()) {
        MythContext::DBError("new find in db", query);
        return val;
    }

    val = query.numRowsAffected() > 0;

    return val;
}

bool insertInDB(NewsSiteItem* site)
{
    if (!site) return false;

    return insertInDB(site->name, site->url, site->ico, site->category);
}

bool insertInDB(const QString &name, const QString &url,
                          const QString &icon, const QString &category)
{
    if (findInDB(name))
        return false;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("INSERT INTO newssites (name,category,url,ico) "
            " VALUES( :NAME, :CATEGORY, :URL, :ICON );");
    query.bindValue(":NAME", name);
    query.bindValue(":CATEGORY", category);
    query.bindValue(":URL", url);
    query.bindValue(":ICON", icon);
    if (!query.exec() || !query.isActive()) {
        MythContext::DBError("news: inserting in DB", query);
        return false;
    }

    return (query.numRowsAffected() > 0);
}

bool removeFromDB(NewsSiteItem* site)
{
    if (!site) return false;

    return removeFromDB(site->name);
}

bool removeFromDB(const QString &name)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("DELETE FROM newssites WHERE name = :NAME ;");
    query.bindValue(":NAME", name);
    if (!query.exec() || !query.isActive()) {
        MythContext::DBError("news: delete from db", query);
        return false;
    }

    return (query.numRowsAffected() > 0);
}