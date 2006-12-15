/*
    archiveutil.h

    some shared functions and types
*/

#ifndef ARCHIVEUTIL_H_
#define ARCHIVEUTIL_H_

#include <qstring.h>

class ProgramInfo;

typedef enum
{
    AD_DVD_SL = 0,
    AD_DVD_DL = 1,
    AD_DVD_RW = 2,
    AD_FILE   = 3
} ARCHIVEDESTINATION;

typedef struct ArchiveDestination
{
    ARCHIVEDESTINATION type;
    QString name;
    QString description;
    long long freeSpace;
}_ArchiveDestination;

extern struct ArchiveDestination ArchiveDestinations[];
extern int ArchiveDestinationsCount;

typedef struct
{
    QString name;
    QString description;
    float bitrate;
} EncoderProfile;

typedef struct ThumbImage
{
    QString caption;
    QString filename;
    int64_t frame;
} ThumbImage;

typedef struct
{
    int     id;
    QString type;
    QString title;
    QString subtitle;
    QString description;
    QString startDate;
    QString startTime;
    QString filename;
    long long size;
    long long newsize;
    int duration;
    EncoderProfile *encoderProfile;
    QString fileCodec;
    QString videoCodec;
    int videoWidth, videoHeight;
    bool hasCutlist;
    bool useCutlist;
    bool editedDetails;
    QPtrList<ThumbImage> thumbList;
} ArchiveItem;

QString formatSize(long long sizeKB, int prec = 2);
QString getTempDirectory(bool showError = false);
void checkTempDirectory();
bool extractDetailsFromFilename(const QString &inFile,
                                QString &chanID, QString &startTime);
ProgramInfo *getProgramInfoForFile(const QString &inFile);
bool getFileDetails(ArchiveItem *a);
QString getBaseName(const QString &filename);
void showWarningDialog(const QString msg);

#endif
