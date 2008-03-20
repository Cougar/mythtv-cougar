#ifndef AUDIOOUTPUTWIN
#define AUDIOOUTPUTWIN

// Qt headers
#include <qstring.h>

// MythTV headers
#include "audiooutputbase.h"

class AudioOutputWinPrivate;

class AudioOutputWin : public AudioOutputBase
{
  friend class AudioOutputWinPrivate;
  public:
    AudioOutputWin(const AudioSettings &settings);
    virtual ~AudioOutputWin();

    // Volume control
    virtual int  GetVolumeChannel(int channel) const;
    virtual void SetVolumeChannel(int channel, int volume);

  protected:
    virtual bool OpenDevice(void);
    virtual void CloseDevice(void);
    virtual void WriteAudio(unsigned char *aubuf, int size);
    virtual int  GetSpaceOnSoundcard(void) const;
    virtual int  GetBufferedOnSoundcard(void) const;

  protected:
    AudioOutputWinPrivate *m_priv;
    long                   m_nPkts;
    int                    m_CurrentPkt;
    unsigned char        **m_OutPkts;

    static const uint      kPacketCnt;
};

#endif // AUDIOOUTPUTWIN
