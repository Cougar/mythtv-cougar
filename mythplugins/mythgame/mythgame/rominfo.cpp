#include <qfile.h>

#include <mythtv/mythcontext.h>
#include <mythtv/mythdbcon.h>

#include "rominfo.h"
#include "romedit.h"

#define LOC_ERR QString("MythGame:ROMINFO Error: ")
#define LOC QString("MythGame:ROMINFO: ")

bool operator==(const RomInfo& a, const RomInfo& b)
{
    if (a.Romname() == b.Romname())
        return true;
    return false;
}


void RomInfo::edit_rominfo()
{
    QString rom_ver = Version();

    GameEditDialog romeditdlg(Romname());

    DialogCode res = romeditdlg.exec();

    if (kDialogCodeRejected == res)
        return;

    if (res)
    {
        MSqlQuery query(MSqlQuery::InitCon());
        query.prepare("SELECT gamename,genre,year,country,publisher,favorite "
                      "FROM gamemetadata "
                      "WHERE gametype = :GAMETYPE "
                      "AND romname = :ROMNAME");

        query.bindValue(":GAMETYPE", GameType());
        query.bindValue(":ROMNAME", Romname());

        if (!query.exec())
        {   
            MythContext::DBError("RomInfo::edit_rominfo", query);
            return;
        }

        if (query.isActive() && query.size() > 0);
        {
            QString t_gamename, t_genre, t_year, t_country, t_publisher;
            bool t_favourite;

            query.next();
            t_gamename = query.value(0).toString();
            t_genre = query.value(1).toString();
            t_year = query.value(2).toString();
            t_country = query.value(3).toString();
            t_publisher = query.value(4).toString();
            t_favourite = query.value(5).toBool();
    
            if ((t_gamename != Gamename()) || (t_genre != Genre()) || (t_year != Year()) 
               || (t_country != Country()) || (t_publisher != Publisher()) || (t_favourite != Favorite()))
            {
                query.prepare("UPDATE gamemetadata SET version = 'CUSTOM' "
                              "WHERE gametype = :GAMETYPE "
                              "AND romname = :ROMNAME");
                query.bindValue(":GAMETYPE", GameType());
                query.bindValue(":ROMNAME", Romname());

                if (!query.exec())
                {   
                    MythContext::DBError("RomInfo::edit_rominfo", query);
                    return;
                }
            }
        }
   }
}

// Return the count of how many times this appears in the db already
int romInDB(QString rom, QString gametype)
{
    MSqlQuery query(MSqlQuery::InitCon());

    int count = 0;

    query.prepare("SELECT count(*) FROM gamemetadata "
                  "WHERE gametype = :GAMETYPE "
                  "AND romname = :ROMNAME");

    query.bindValue(":GAMETYPE", gametype);
    query.bindValue(":ROMNAME", rom);

    if (!query.exec())
    {
        MythContext::DBError("romInDB", query);
        return -1;
    }


    if (query.isActive() && query.size() > 0);
    {
        query.next();
        count = query.value(0).toInt();
    };

    return count;
}


bool RomInfo::FindImage(QString BaseFileName, QString *result)
{
    QStringList graphic_formats;
    graphic_formats.append("png");
    graphic_formats.append("gif");
    graphic_formats.append("jpg");
    graphic_formats.append("jpeg");
    graphic_formats.append("xpm");
    graphic_formats.append("bmp");
    graphic_formats.append("pnm");
    graphic_formats.append("tif");
    graphic_formats.append("tiff");

    int dotLocation = BaseFileName.findRev('.');
    if(dotLocation == -1)
    {
        BaseFileName.append('.');
        dotLocation = BaseFileName.length();
    }


    BaseFileName.truncate(dotLocation + 1);
    for (QStringList::Iterator i = graphic_formats.begin(); i != graphic_formats.end(); i++)
    {
        *result = BaseFileName + *i;
        if(QFile::exists(*result))
            return true;
    }
    return false;
}

void RomInfo::setField(QString field, QString data)
{
    if (field == "system")
        system = data;
    else if (field == "gamename")
        gamename = data;
    else if (field == "genre")
        genre = data;
    else if (field == "year")
        year = data;
    else if (field == "favorite")
        favorite = data.toInt();
    else if (field == "rompath")
        rompath = data;
    else if (field == "country")
        country = data;
    else if (field == "publisher")
        publisher = data;
    else if (field == "crc_value")
        crc_value = data;
    else if (field == "diskcount")
        diskcount = data.toInt();
    else if (field == "gametype")
        gametype = data;
    else if (field == "romcount")
        romcount = data.toInt();
    else
        VERBOSE(VB_GENERAL, LOC_ERR + QString("Invalid field %1").arg(field));

}

void RomInfo::setFavorite()
{
    favorite = 1 - favorite;

    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("UPDATE gamemetadata SET favorite = :FAV "
                  "WHERE romname = :ROMNAME");

    query.bindValue(":FAV", favorite);
    query.bindValue(":ROMNAME",romname);

    if (!query.exec())
    {   
        MythContext::DBError("RomInfo::setFavorite", query);
    }
}

QString RomInfo::getExtension()
{
    int pos = Romname().findRev(".");
    if (pos == -1)
        return NULL;

    pos = Romname().length() - pos;
    pos--;

    QString ext = Romname().right(pos);
    return ext;
}

void RomInfo::fillData()
{
    if (gamename == "")
    {
        return;
    }

    MSqlQuery query(MSqlQuery::InitCon());

    QString systemtype;
    if (system != "") {
        systemtype  += " AND system = :SYSTEM ";
    }

    QString thequery = "SELECT system,gamename,genre,year,romname,favorite,"
                       "rompath,country,crc_value,diskcount,gametype,publisher,"
                       "version FROM gamemetadata WHERE gamename = :GAMENAME " 
                       + systemtype + " ORDER BY diskcount DESC";

    query.prepare(thequery);
    query.bindValue(":SYSTEM", system);
    query.bindValue(":GAMENAME", gamename);
    query.exec();

    if (query.isActive() && query.size() > 0);
    {
        query.next();

        setSystem(query.value(0).toString());
        setGamename(query.value(1).toString());
        setGenre(query.value(2).toString());
        setYear(query.value(3).toString());
        setRomname(query.value(4).toString());
        setField("favorite",query.value(5).toString());
        setRompath(query.value(6).toString());
        setCountry(query.value(7).toString());
        setCRC_VALUE(query.value(8).toString());
        setDiskCount(query.value(9).toInt());
        setGameType(query.value(10).toString());
        setPublisher(query.value(11).toString());
        setVersion(query.value(12).toString());
    }

    query.prepare("SELECT screenshots FROM gameplayers "
                  "WHERE playername = :SYSTEM");
    query.bindValue(":SYSTEM",system);
    query.exec();

    if (query.isActive() && query.size() > 0);
    {
        query.next();
        if (!query.value(0).toString().isEmpty()) 
        {
            QString Image = query.value(0).toString() + "/" + romname;
            if (FindImage(query.value(0).toString() + "/" + romname, &Image))
                setImagePath(Image);
            else
                setImagePath("");
        }
    }

    setRomCount(romInDB(romname,gametype));

    // If we have more than one instance of this rom in the DB fill in all
    // systems available to play it.
    if (RomCount() > 1) 
    {
        query.prepare("SELECT DISTINCT system FROM gamemetadata "
                      "WHERE romname = :ROMNAME");
        query.bindValue(":ROMNAME", Romname());
        query.exec();

        if (query.isActive() && query.size() > 0);
        {
            while(query.next())
            {
                if (allsystems == "")
                    allsystems = query.value(0).toString();
                else
                    allsystems += "," + query.value(0).toString();
            }
        }
    } else 
        allsystems = system;

}

