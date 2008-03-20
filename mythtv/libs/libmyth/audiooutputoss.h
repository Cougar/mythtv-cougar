#ifndef AUDIOOUTPUTOSS
#define AUDIOOUTPUTOSS

#include <vector>
#include <qstring.h>
#include <qmutex.h>

#include "audiooutputbase.h"

using namespace std;


class AudioOutputOSS : public AudioOutputBase
{
  public:
    AudioOutputOSS(const AudioSettings &settings);
    virtual ~AudioOutputOSS();

    // Volume control
    virtual int GetVolumeChannel(int channel) const;
    virtual void SetVolumeChannel(int channel, int volume);

  protected:
    // You need to implement the following functions
    virtual bool OpenDevice(void);
    virtual void CloseDevice(void);
    virtual void WriteAudio(unsigned char *aubuf, int size);
    virtual int  GetSpaceOnSoundcard(void) const;
    virtual int  GetBufferedOnSoundcard(void) const;

  private:
    void VolumeInit(void);
    void VolumeCleanup(void);
    
    void SetFragSize(void);
    
    int audiofd;
    mutable int numbadioctls;

    // Volume related
    int mixerfd;
    int control;
};

#endif

