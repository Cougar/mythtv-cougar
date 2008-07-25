// -*- Mode: c++ -*-

#ifndef _IPTV_URL_FETCHER_H_
#define _IPTV_URL_FETCHER_H_

// C++ headers
#include <vector>
using namespace std;

// Qt headers
#include <q3networkprotocol.h>
#include <QString>

class Q3NetworkOperation;
class Q3UrlOperator;

class URLFetcher : public QObject
{
    Q_OBJECT

  private:
    URLFetcher(const QString &url);
    ~URLFetcher() {}

  public:
    static QString FetchData(const QString &url, bool inQtThread);

  public slots:
    void deleteLater(void);

  private slots:
    void Finished(Q3NetworkOperation *op);
    void Data(const QByteArray &data, Q3NetworkOperation *op);

  private:
    Q3UrlOperator            *op;
    Q3NetworkProtocol::State  state;
    vector<unsigned char>    buf;
};

#endif // _IPTV_URL_FETCHER_H_
