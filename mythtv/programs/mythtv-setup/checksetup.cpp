// checksetup.cpp
//
// Some functions to do simple sanity checks on the MythTV setup.
// CheckSetup() is currently meant for the mythtv-setup program,
// but the other functions could probably be called from anywhere.
// They all return true if any problems are found, and add to a
// caller-supplied QString a message describing the problem.

#include <QDir>

#include "mythdb.h"
#include "mythcontext.h"
#include "util.h"

/// Check that a directory path exists and is writable

static bool checkPath(QString path, QStringList &probs)
{
    QDir dir(path);
    if (!dir.exists())
    {
        probs.push_back(QObject::tr("Path \"%1\" doesn't exist.").arg(path));
        return true;
    }

    QFile test(path.append("/.test"));
    if (test.open(QIODevice::WriteOnly))
        test.remove();
    else
    {
        probs.push_back(QObject::tr("Unable to create file \"%1\" - directory "
                        "is not writable?").arg(path));
        return true;
    }

    return false;
}

/// Do the Storage Group filesystem paths exist? Are they writable?
/// Is the Live TV filesystem large enough?

bool checkStoragePaths(QStringList &probs)
{
    bool problemFound = false;

    QString recordFilePrefix =
            gContext->GetSetting("RecordFilePrefix", "EMPTY");

    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("SELECT count(groupname) FROM storagegroup;");
    if (!query.exec() || !query.isActive() || query.size() < 1)
    {
        MythDB::DBError("checkStoragePaths", query);
        return false;
    }

    query.next();
    if (query.value(0).toInt() == 0)
    {
        QString trMesg =
                QObject::tr("No Storage Group directories are defined.  You "
                            "must add at least one directory to the Default "
                            "Storage Group where new recordings will be "
                            "stored.");
        probs.push_back(trMesg);
        VERBOSE(VB_IMPORTANT, trMesg);
        return true;
    }

    query.prepare("SELECT groupname, dirname "
                  "FROM storagegroup "
                  "WHERE hostname = :HOSTNAME;");
    query.bindValue(":HOSTNAME", gContext->GetHostName());
    if (!query.exec() || !query.isActive() || query.size() < 1)
    {
        MythDB::DBError("checkStoragePaths", query);
        return false;
    }

    QDir checkDir("");
    while (query.next())
    {
        QString sgDir = query.value(0).toString();
        QStringList tokens = query.value(1).toString().split(",");
        int curToken = 0;
        while (curToken < tokens.size())
        {
            checkDir.setPath(tokens[curToken]);
            if (checkPath(tokens[curToken], probs))
            {
                problemFound = true;
            }
            curToken++;
        }
    }

    return problemFound;
}


// I keep forgetting to change the preset (starting channel) when I add cards,
// so this checks that the assigned channel (which may be the default of 3)
// actually exists. This should save a few beginner Live TV problems

bool checkChannelPresets(QStringList &probs)
{
    bool problemFound = false;

    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("SELECT cardid, startchan, sourceid, inputname"
                  " FROM cardinput;");

    if (!query.exec() || !query.isActive())
    {
        MythDB::DBError("checkChannelPresets", query);
        return false;
    }

    while (query.next())
    {
        int cardid    = query.value(0).toInt();
        QString startchan = query.value(1).toString();
        int sourceid  = query.value(2).toInt();

        if (query.value(1).toString() == "")    // Logic from tv_rec.cpp
            startchan = "3";

        MSqlQuery channelExists(MSqlQuery::InitCon());
        QString   channelQuery;
        channelQuery = QString("SELECT chanid FROM channel"
                               " WHERE channum='%1' AND sourceid=%2;")
                              .arg(startchan).arg(sourceid);
        channelExists.prepare(channelQuery);

        if (!channelExists.exec() || !channelExists.isActive())
        {
            MythDB::DBError("checkChannelPresets", channelExists);
            return problemFound;
        }

        if (channelExists.size() == 0)
        {
            probs.push_back(QObject::tr("Card %1 (type %2) is set to start on "
                            "channel %3, which does not exist.")
                    .arg(cardid).arg(query.value(3).toString()).arg(startchan));
            problemFound = true;
        }
    }

    return problemFound;
}

/// Build up a string of common problems that the
/// user should correct in the MythTV-Setup program

bool CheckSetup(QStringList &problems)
{
    return checkStoragePaths(problems)
        || checkChannelPresets(problems);
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
