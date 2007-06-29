#ifndef CDDECODER_H_
#define CDDECODER_H_

#include "decoder.h"

#include <mythtv/mythconfig.h>

#ifdef CONFIG_DARWIN
#include <vector>
#else
#include <cdaudio.h>
extern "C" {
#include <cdda_interface.h>
#include <cdda_paranoia.h>
}
#endif

class Metadata;

class CdDecoder : public Decoder
{
  public:
    CdDecoder(const QString &file, DecoderFactory *, QIODevice *, AudioOutput *);
    virtual ~CdDecoder(void);

    bool initialize();
    double lengthInSeconds();
    void seek(double);
    void stop();

    int getNumTracks(void);
    int getNumCDAudioTracks(void);

    // The following need to allocate a new Metadata object each time,
    // because their callers (e.g. databasebox.cpp) free the returned value
    Metadata *getMetadata(int track);
    Metadata *getMetadata(void);
    Metadata *getLastMetadata(void);

    void commitMetadata(Metadata *mdata);
    void      setDevice(const QString &dev)  { devicename = dev; }
    void      setCDSpeed(int speed);

  private:
    void run();

    void flush(bool = FALSE);
    void deinit();

    bool inited, user_stop;


    QString            devicename;

#ifdef CONFIG_DARWIN
    void CdDecoder::lookupCDDB(const QString &hexID, uint tracks);

    uint32_t           m_diskID;        ///< For CDDB1/FreeDB lookup
    uint               m_firstTrack,    ///< First AUDIO track
                       m_lastTrack,     ///< Last  AUDIO track
                       m_leadout;       ///< End of last track
    double             m_lengthInSecs;
    vector<int>        m_tracks;        ///< Start block offset of each track
    vector<Metadata*>  m_mData;         ///< After lookup, details of each trk
#else
    int stat;
    char *output_buf;
    ulong output_bytes, output_at;

    unsigned int bks;
    bool done, finish;
    long len, freq, bitrate;
    int chan;
    unsigned long output_size;
    double totalTime, seekTime;

    int settracknum;
    int tracknum;

    cdrom_drive *device;
    cdrom_paranoia *paranoia;

    long int start;
    long int end;
    long int curpos;
#endif
};

#endif

