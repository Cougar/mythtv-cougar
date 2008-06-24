#ifndef _CHANNEL_IMPORTER_HELPERS_H_
#define _CHANNEL_IMPORTER_HELPERS_H_

// POSIX headers
#include <stdint.h>
typedef unsigned uint;

// C++ headers
#include <vector>
using namespace std;

// Qt headers
#include <qstring.h>
#include <qdatetime.h>

// MythTV headers
#include "dtvmultiplex.h"

class ScanInfo
{
  public:
    ScanInfo();
    ScanInfo(uint _scanid, uint _cardid, uint _sourceid,
             bool _processed, const QDateTime &_scandate);

    static bool MarkProcessed(uint scanid);
    static bool DeleteScan(uint scanid);

  public:
    uint      scanid;
    uint      cardid;
    uint      sourceid;
    bool      processed;
    QDateTime scandate;
};

vector<ScanInfo> LoadScanList(void);
uint SaveScan(const ScanDTVTransportList &scan);
ScanDTVTransportList LoadScan(uint scanid);

#endif // _CHANNEL_IMPORTER_HELPERS_H_
