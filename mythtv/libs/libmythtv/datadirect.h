#ifndef DATADIRECT_H_
#define DATADIRECT_H_

#include <qstringlist.h>
#include <qdatetime.h>
#include <qxml.h>
#include <qsqldatabase.h>
#include <qsqlquery.h>

class DataDirectProcessor;

class DataDirectStation 
{
  public:
    DataDirectStation() { clearValues(); }

    void clearValues() 
    {
       stationid = "";
       callsign = "";
       stationname = "";
       affiliate = "";
       fccchannelnumber = "";   
    }   

    QString stationid; // 12
    QString callsign; // 10
    QString stationname; // 40
    QString affiliate; // 25
    QString fccchannelnumber; // 8
};

class DataDirectLineup 
{
  public:
    DataDirectLineup() { clearValues(); }

    void clearValues() {
        lineupid = "";
        name = "";
        displayname = "";
        type = "";
        postal = "";
        device = "";
    }   
         
    QString lineupid;
    QString name;
    QString displayname;
    QString type;
    QString postal;
    QString device;
};

class DataDirectLineupMap 
{
  public:
    DataDirectLineupMap() { clearValues(); }

    void clearValues() 
    {
        lineupid = "";
        stationid = "";
        channel = "";
    }        

    QString lineupid; // 100
    QString stationid; // 12
    QString channel; // 5
// QString channelMinor; // 3
// QDate mapFrom;
// QDate mapTo;
// QDate onAirFrom;
// QDate onAirTo;
};

class DataDirectSchedule 
{
  public:
    DataDirectSchedule() { clearValues(); }

    void clearValues() 
    {
        programid = "";
        stationid = "";
        time = QDateTime();
        duration = QTime();
        repeat = false;
        stereo = false;
        subtitled = false;
        hdtv = false;
        closecaptioned = false;
        tvrating = "";
        partnumber = 0;
        parttotal = 0;
    }   

    QString programid; // 12
    QString stationid; // 12
    QDateTime time; 
    QTime duration;
    bool repeat;
    bool stereo;
    bool subtitled;
    bool hdtv;
    bool closecaptioned;
    QString tvrating;
    int partnumber;
    int parttotal;
};

class DataDirectProgram 
{
  public:
    DataDirectProgram() { clearValues(); }

    void clearValues() 
    {
        programid = "";
        seriesid = "";
        title = "";
        subtitle = "";
        description = "";
        mpaaRating = "";
        starRating = "";
        duration = QTime();
        year = "";
        showtype = "";
        colorcode = "";
        originalAirDate = QDate();
        syndicatedEpisodeNumber = "";
    }       

    QString programid; // 12
    QString seriesid; // 12
    QString title; // 120
    QString subtitle; // 150
    QString description; // 255
    QString mpaaRating; // 5
    QString starRating; // 5 
    QTime duration;
    QString year; // 4
    QString showtype; // 30
    QString colorcode; // 20
    QDate originalAirDate; // 20
    QString syndicatedEpisodeNumber; // 20
    // advisories ?
};

class DataDirectProductionCrew 
{
  public:
    DataDirectProductionCrew() { clearValues(); }

    void clearValues() 
    {
        programid = "";
        role = "";
        givenname = "";
        surname = "";
        fullname = "";
    }
   
    QString programid; // 12
    QString role; // 30
    QString givenname; // 20
    QString surname; // 20
    QString fullname; // 41
};

class DataDirectGenre 
{
  public:
    DataDirectGenre() { clearValues(); }

    void clearValues() 
    {
        programid = "";
        gclass = "";
        relevance = "";
    }

    QString programid; // 12
    QString gclass; // 30
    QString relevance; // 1
};

class DDStructureParser: public QXmlDefaultHandler 
{
  public:
    DDStructureParser(DataDirectProcessor& _ddparent): parent(_ddparent) {}

    bool startElement(const QString &pnamespaceuri, const QString &plocalname, 
                      const QString &pqname, const QXmlAttributes &pxmlatts);

    bool endElement(const QString &pnamespaceuri, const QString &plocalname, 
                    const QString &pqname);

    bool characters(const QString &pchars);

    bool startDocument();
    bool endDocument();

  private:
    DataDirectProcessor& parent;    

    QString currtagname;

    DataDirectStation curr_station;
    DataDirectLineup curr_lineup;
    DataDirectLineupMap curr_lineupmap;
    DataDirectSchedule curr_schedule;
    DataDirectProgram curr_program;
    DataDirectProductionCrew curr_productioncrew;
    DataDirectGenre curr_genre;
    QString lastprogramid;    
};   

class DataDirectProcessor
{
  public:
    DataDirectProcessor() 
    {
        stations.clear();
        lineups.clear();
        lineupmaps.clear();
    }
  
    QValueList<DataDirectStation> getStations(void) const { return stations; }
    QValueList<DataDirectLineup> getLineups(void) const { return lineups; }           
    QValueList<DataDirectLineupMap> getLineupMaps(void) const 
                                                       { return lineupmaps; }
  
    QString getUserID() const { return userid; }
    QString getPassword() const { return password; }
    QString getLineup() const { return selectedlineupid; }

    void setUserID(QString uid) { userid = uid; };
    void setPassword(QString pwd) { password = pwd; };
    void setLineup(QString lid) { selectedlineupid = lid; };

    QDateTime getActualListingsFrom() const { return actuallistingsfrom; }
    QDateTime getActualListingsTo() const { return actuallistingsto; }

    void setActualListingsFrom(QDateTime palf) { actuallistingsfrom = palf; };
    void setActualListingsTo(QDateTime palt) { actuallistingsto = palt; };

    void retrieveStationsAndLineups();

    void createTempTables();
    void populateStationsLineupsTables();
    void populateMainDDTables();
    void updateStationViewTable();
    void updateProgramViewTable(int sourceid);

    void grabLineupsOnly();
    void grabData(bool plineupsonly, QDateTime pstartdate, QDateTime penddate);
    void grabAllData(void);

    void parseLineups();
    void parseStations();

    QValueList<DataDirectStation> stations;
    QValueList<DataDirectLineup> lineups;
    QValueList<DataDirectLineupMap> lineupmaps;
     
  private:
    QString selectedlineupid;
    QString userid;
    QString password;

    QString lastrunuserid;
    QString lastrunpassword; 
    QDateTime actuallistingsfrom;
    QDateTime actuallistingsto;

    void createATempTable(const QString &ptablename, 
                          const QString &ptablestruct);
};
 
#endif
