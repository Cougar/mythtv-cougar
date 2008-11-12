#ifndef _XMLTVPARSER_H_
#define _XMLTVPARSER_H_

// Qt headers
#include <QMap>
#include <QList>
#include <QString>

class ProgInfo;
class ChanInfo;
class QUrl;
class QDomElement;

class XMLTVParser
{
  public:
    XMLTVParser() : isJapan(false) {}

    ChanInfo *parseChannel(QDomElement &element, QUrl &baseUrl);
    ProgInfo *parseProgram(QDomElement &element, int localTimezoneOffset);
    bool parseFile(
        QString filename, QList<ChanInfo> *chanlist,
        QMap<QString, QList<ProgInfo> > *proglist);


  public:
    bool isJapan;
};

#endif // _XMLTVPARSER_H_
