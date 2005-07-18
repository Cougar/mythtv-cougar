#include "gamehandler.h"
#include "rominfo.h"

#include <qobject.h>
#include <qptrlist.h>
#include <qstringlist.h>
#include <iostream>
#include <qdir.h>
#include "unzip.h"

#include <mythtv/mythcontext.h>
#include <mythtv/mythdbcon.h>
#include <mythtv/mythdialogs.h>


using namespace std;

GameHandler::~GameHandler()
{
}

static QPtrList<GameHandler> *handlers = 0;

bool existsHandler(const QString name)
{
    GameHandler *handler = handlers->first();
    while(handler)
    {
        if (handler->SystemName() == name)
        {
            return TRUE;
        }
        
        handler = handlers->next();
    }

    return FALSE;
}

static void checkHandlers(void)
{
    // If a handlers list doesn't currently exist create one. Otherwise
    // clear the existing list so that we can regenerate a new one.
    if (! handlers)
    {   
        handlers = new QPtrList<GameHandler>;
    }
    else {
        handlers->clear();
    }

    MSqlQuery query(MSqlQuery::InitCon());
    query.exec("SELECT DISTINCT playername FROM gameplayers WHERE playername <> '';");
    while (query.next()) 
    {   
        QString name = query.value(0).toString();
        GameHandler::registerHandler(GameHandler::newHandler(name));
    }   

}

GameHandler* GameHandler::getHandler(uint i)
{
    return handlers->at(i);
}

void GameHandler::updateSettings(GameHandler *handler)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.exec("SELECT rompath, workingpath, commandline, screenshots, gameplayerid, gametype, extensions, spandisks  FROM gameplayers WHERE playername = \"" + handler->SystemName() + "\";");

    query.next();
    handler->rompath = query.value(0).toString();
    handler->workingpath = query.value(1).toString();
    handler->commandline = query.value(2).toString();
    handler->screenshots = query.value(3).toString();
    handler->gameplayerid = query.value(4).toInt();
    handler->gametype = query.value(5).toString();
    handler->validextensions = QStringList::split(",", query.value(6).toString().stripWhiteSpace().remove(" "));
    handler->spandisks = query.value(7).toInt();

}

GameHandler* GameHandler::newInstance = 0;

GameHandler* GameHandler::newHandler(QString name)
{
    newInstance = new GameHandler();
    newInstance->systemname = name;

    updateSettings(newInstance);

    return newInstance;
}

// Creates/rebuilds the handler list and then returns the count.
uint GameHandler::count(void)
{
    checkHandlers();
    return handlers->count();
}

bool GameHandler::validRom(QString RomName,GameHandler *handler)
{
    // Add proper checks here
    return TRUE;
}

// Return the crc32 info for this rom. (ripped mostly from the old neshandler.cpp source)
uLong crcinfo(QString romname, int offset) 
{
    // Get CRC of file
    char block[32768];
    uLong crc = crc32(0, Z_NULL, 0);

    unzFile zf;
    if ((zf = unzOpen(romname)))
    {
        int FoundFile;
        for (FoundFile = unzGoToFirstFile(zf); FoundFile == UNZ_OK;
             FoundFile = unzGoToNextFile(zf))
        {
            if (unzOpenCurrentFile(zf) == UNZ_OK)
            {

                // Skip past iNes header
                if (offset > 0)
                    unzReadCurrentFile(zf, block, offset);

                // Get CRC of rom data
                int count;
                while ((count = unzReadCurrentFile(zf, block, 32768)))
                {
                    crc = crc32(crc, (Bytef *)block, (uInt)count);
                }
                unzCloseCurrentFile(zf);
            }
        }
        unzClose(zf);
    }
    else
    {
        QFile f(romname);
        if (f.open(IO_ReadOnly))
        {
            if (offset > 0)
                f.readBlock(block, offset);

            // Get CRC of rom data
            Q_LONG count;
            while ((count = f.readBlock(block, 32768)))
            {
                crc = crc32(crc, (Bytef *)block, (uInt)count);
            }

            f.close();
        }
    }

    return crc;
}

void GameHandler::GetMetadata(GameHandler *handler, QString rom, QString* Genre, int* Year,
                              QString* Country, QString* CRC32)
{
    uLong crc;

    if (handler->GameType() == "NES")
        crc = crcinfo(rom, 16);
    else
        crc = crcinfo(rom,0);

    *CRC32 = QString("%1").arg( crc, 0, 16 );
    if (*CRC32 == "0")
        *CRC32 = "";

    *Year = 0;
    *Country = QObject::tr("Unknown");
    *Genre = QObject::tr("Unknown");
    
}

// Recurse through the directory and gather a count on how many files there are to process.
// This is used for the progressbar info.
int GameHandler::buildFileCount(QString directory, GameHandler *handler)
{
    int filecount = 0;
    QDir RomDir(directory);
    const QFileInfoList* List = RomDir.entryInfoList();
    for (QFileInfoListIterator it(*List); it; ++it)
    {   
        QFileInfo Info(*it.current());
        QString RomName = Info.fileName();

        if (RomName == "." ||
            RomName == "..")
        {   
            continue;
        }

        if (Info.isDir())
        {   
            filecount += buildFileCount(Info.filePath(), handler);
            continue;
        }
        else
        {   
            if (handler->validextensions.count() > 0)
            {   
                QRegExp r;

                r.setPattern("^" + Info.extension( FALSE ) + "$");
                r.setCaseSensitive(false);
                QStringList result = handler->validextensions.grep(r);
                if (result.isEmpty()) {
                    continue;
                }
            }
            filecount++;

        }
    }

    return filecount;

}

void updateDiskCount(QString romname, int diskcount)
{
    MSqlQuery query(MSqlQuery::InitCon());
    QString thequery = QString("UPDATE gamemetadata SET diskcount = %1 WHERE romname = \"%2\"; ")
                              .arg(diskcount)
                              .arg(romname);
    query.exec(thequery);

}

void GameHandler::buildFileList(QString directory, GameHandler *handler, 
                                MSqlQuery *query, MythProgressDialog *pdial, int indepth, int filecount)
{
    QString thequery;
    QString queryvalues;

    QString lastrom = "";
    QRegExp multiDiskRGXP = QRegExp( ".[0-4].*$", TRUE, FALSE );
    int diskcount = 0;
    QString firstname;
    QString basename;

    QDir RomDir(directory);
    RomDir.setSorting( QDir:: DirsFirst | QDir::Name );
    const QFileInfoList* List = RomDir.entryInfoList();

    for (QFileInfoListIterator it(*List); it; ++it)
    {
        QFileInfo Info(*it.current());
        QString RomName = Info.fileName();
        QString GameName = Info.baseName(TRUE);

        if (RomName == "." ||
            RomName == "..")
        {
            continue;
        }

        if (Info.isDir())
        {
            buildFileList(Info.filePath(), handler, query, pdial, indepth, filecount);
            continue;
        }
        else
        {

            if (handler->validextensions.count() > 0)
            {
                QRegExp r;

                r.setPattern("^" + Info.extension( FALSE ) + "$");
                r.setCaseSensitive(false);
                QStringList result = handler->validextensions.grep(r);
                if (result.isEmpty()) {
                    continue;
                }
            }

            if (validRom(Info.filePath(),handler))
            {
                QString Genre(QObject::tr("Unknown"));
                QString Country(QObject::tr("Unknown"));
                QString CRC32 = "";
                int Year = 0;

                if (indepth) 
                    GetMetadata(handler, Info.filePath(), &Genre, &Year, &Country, &CRC32);

                // cout << "Found Rom : (" << handler->SystemName() << ") " << CRC32 << " - " << RomName << endl;

                int displayrom;

                // If the rom is already in the DB then  don't display the duplicate
                // Probably convert this to use a QMap and update the database after the map is filled
                // just like mythvideo does.
                if (romInDB(RomName, handler->GameType()))
                    displayrom = 0;
                else
                    displayrom = 1;

                if (handler->SpanDisks()) 
                {
                    if (RomName.contains(multiDiskRGXP))
                    {
                        basename = GameName.left(GameName.findRev("."));

                        if (basename == lastrom)
                        {
                            displayrom = 0;
                            diskcount++;
                            if (diskcount > 1)
                            {   
                                updateDiskCount(firstname,diskcount);
                            }
                        }
                        else
                        {
                            firstname = RomName;
                            lastrom = basename;
                            diskcount = 1;
                        }
                        GameName = GameName.left(GameName.findRev("."));
                           
                    }
                }

                // Put the game into the database.
                // Had to break the values up into 2 pieces since QString only allows 9 arguments and we had 10
                thequery = QString("INSERT INTO gamemetadata "
                    "(system, romname, gamename, genre, year, gametype, rompath,country,crc_value,diskcount,display) ");
                queryvalues = QString ("VALUES (\"%1\", \"%2\", \"%3\", \"%4\", %5, \"%6\",")
                    .arg(handler->SystemName())
                    .arg(Info.fileName().latin1())
                    .arg(GameName.latin1())
                    .arg(Genre.latin1())
                    .arg(Year)
                    .arg(handler->GameType());

                queryvalues.append( QString("\"%1\", \"%2\", \"%3\", 1 ,\"%4\");") 
                    .arg(Info.dirPath().latin1())
                    .arg(Country.latin1())
                    .arg(CRC32)
                    .arg(displayrom));

                thequery.append(queryvalues);
                query->exec(thequery);

                filecount++;
                pdial->setProgress(filecount);

            }
            // else skip it
        }
    }
}

void GameHandler::processGames(GameHandler *handler)
{
    QString thequery;
    int maxcount = 0;
    MSqlQuery query(MSqlQuery::InitCon());

    // Remove all metadata entries from the tables, all correct values will be
    // added as they are found.  This is done so that entries that may no longer be
    // available or valid are removed each time the game list is remade.

    // If we are not going to call processGames from anywhere but processAllGames then the following
    // Deletion can likely be removed and processGames and processAllGames merged together.

    thequery = "DELETE FROM gamemetadata WHERE system = \"" + handler->SystemName() + "\";";
    query.exec(thequery);

    if ((handler->SystemRomPath()) && (handler->GameType() != "PC"))
    {
        QDir d(handler->SystemRomPath());
        if (d.exists())
            maxcount = buildFileCount(handler->SystemRomPath(),handler);
        else 
        {
            cout << "Rom Path does not exist : " << handler->SystemRomPath() << endl;
            return;
        }
    }
    else
        maxcount = 100;

    MythProgressDialog pdial(QObject::tr("Looking for " + handler->SystemName() + " game(s)..."), maxcount);
    pdial.setProgress(0);

    if (handler->GameType() == "PC") 
    {
        thequery = QString("INSERT INTO gamemetadata "
                           "(system, romname, gamename, genre, year, gametype, country, "
                           "diskcount, display) "
                           "VALUES (\"%1\", \"%2\", \"%3\", \"Unknown\", 1970 , \"%4\", "
                           "\"Unknown\",1,1);")
                           .arg(handler->SystemName())
                           .arg(handler->SystemName())
                           .arg(handler->SystemName())
                           .arg(handler->GameType());

        query.exec(thequery);

        pdial.setProgress(maxcount);
        // cout << "PC Game " << handler->SystemName() << endl;
    }
    else
    {   
        buildFileList(handler->SystemRomPath(),handler,&query,&pdial,
                            handler->SpanDisks(),gContext->GetSetting("GameDeepScan").toInt());
    }

    pdial.Close();
}

void GameHandler::processAllGames(void)
{
    checkHandlers();

    // Since we are processing all game handlers with a freshly built handler list
    // we can safely remove all existing gamemetadata first so that we don't have any 
    // left overs from removed or renamed handlers.
    MSqlQuery query(MSqlQuery::InitCon());
    QString thequery = "DELETE FROM gamemetadata;";
    query.exec(thequery);

    GameHandler *handler = handlers->first();
    while(handler)
    {
	updateSettings(handler);
        handler->processGames(handler);
        handler = handlers->next();
    }
}

GameHandler* GameHandler::GetHandler(RomInfo *rominfo)
{
    if (!rominfo)
        return NULL;

    GameHandler *handler = handlers->first();
    while(handler)
    {   
        if(rominfo->System() == handler->SystemName())
        {   
            return handler;
        }
        handler = handlers->next();
    }
    return handler;
}

GameHandler* GameHandler::GetHandlerByName(QString systemname)
{
    if (!systemname)
        return NULL;

    GameHandler *handler = handlers->first();
    while(handler)
    {
        if(handler->SystemName() == systemname)
        {
            return handler;
        }
        handler = handlers->next();
    }
    return handler;
}

void GameHandler::Launchgame(RomInfo *romdata, QString systemname)
{
    GameHandler *handler;
    if (systemname) 
    {
        handler = GetHandlerByName(systemname);
    }
    else if (!(handler = GetHandler(romdata)))
    {
        // Couldn't get handler so abort.
        return;
    }
    QString exec = handler->SystemCmdLine();

    if (handler->GameType() != "PC")
    {
        QString arg = "\"" + romdata->Rompath() +
                      "/" + romdata->Romname() + "\"";

        // If they specified a %s in the commandline place the romname
        // in that location, otherwise tack it on to the end of
        // the command.
        if (exec.contains("%s") || handler->SpanDisks())
        {
            exec = exec.replace(QRegExp("%s"),arg);

            if (handler->SpanDisks())
            {
                QRegExp rxp = QRegExp( "%d[0-4]", TRUE, FALSE );

                if (exec.contains(rxp)) 
                {
                    if (romdata->DiskCount() > 1) 
                    {
                        QString basename = romdata->Gamename().left(romdata->Gamename().findRev("."));
                        QString extension = romdata->getExtension();
                        QString rom;
                        QString diskid[] = { "%d0", "%d1", "%d2", "%d3", "%d4", "%d5", "%d6" };

                        for (int disk = 1; disk <= romdata->DiskCount(); disk++)
                        {
                            rom = QString("\"%1/%2.%3.%4\"")
                                          .arg(romdata->Rompath())
                                          .arg(basename)
                                          .arg(disk)
                                          .arg(extension);
                            exec = exec.replace(QRegExp(diskid[disk]),rom);
                        }
                    } else
                    {   // If there is only one disk make sure we replace %d1 just like %s
                        exec = exec.replace(QRegExp("%d1"),arg);
                    }
                }
            }
        }
        else
        {
            exec = exec + " \"" + 
                   romdata->Rompath() + "/" +
                   romdata->Romname() + "\"";
        }
    }

    QString savedir = QDir::currentDirPath ();
    QDir d;
    if (handler->SystemWorkingPath()) {
        if (!d.cd(handler->SystemWorkingPath()))
        {
            cout << "Failed to change to specified Working Directory : " << handler->SystemWorkingPath() << endl;
        }
    }
       
    cout << "Launching Game : " << handler->SystemName() << " : " << exec << " : " << endl;

    myth_system(exec);

    (void)d.cd(savedir);      
}

RomInfo* GameHandler::CreateRomInfo(RomInfo* parent)
{
    GameHandler *handler;
    if((handler = GetHandler(parent)))
        return new RomInfo(*parent);
    return NULL;
}

void GameHandler::registerHandler(GameHandler *handler)
{
    handlers->append(handler);
}

