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
    AudioOutputOSS(QString audiodevice, int laudio_bits, 
                   int laudio_channels, int laudio_samplerate);
    virtual ~AudioOutputOSS();

protected:

    // You need to implement the following functions
    virtual bool OpenDevice(void);
    virtual void CloseDevice(void);
    virtual void WriteAudio(unsigned char *aubuf, int size);
    virtual inline int getSpaceOnSoundcard(void);
    virtual inline int getBufferedOnSoundcard(void);

private:
    void SetFragSize(void);
    
    int audiofd;
    int numbadioctls;
};

#endif

