#ifndef _XMLTVPARSER_H_
#define _XMLTVPARSER_H_

// Qt headers
#include <q3valuelist.h>
#include <qstring.h>
#include <q3url.h>
#include <qdom.h>
#include <qmap.h>

class ProgInfo;
class ChanInfo;

class XMLTVParser
{
  public:
    XMLTVParser() : isJapan(false) {}

    ChanInfo *parseChannel(QDomElement &element, Q3Url baseUrl);
    ProgInfo *parseProgram(QDomElement &element, int localTimezoneOffset);
    bool parseFile(
        QString filename, Q3ValueList<ChanInfo> *chanlist,
        QMap<QString, Q3ValueList<ProgInfo> > *proglist);


  public:
    bool isJapan;
};

#endif // _XMLTVPARSER_H_
