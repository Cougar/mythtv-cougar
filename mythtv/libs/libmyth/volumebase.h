#ifndef VOLUMEBASE
#define VOLUMEBASE

#include <iostream>
using namespace std;

#include <qstring.h>
#include "mythcontext.h"

typedef enum {
    kMuteOff = 0,
    kMuteLeft,
    kMuteRight,
    kMuteAll,
} MuteState;

class MPUBLIC VolumeBase
{
  public:
    VolumeBase();    
    virtual ~VolumeBase() {};

    virtual uint GetCurrentVolume(void) const;
    virtual void SetCurrentVolume(int value);
    virtual void AdjustCurrentVolume(int change);
    virtual void ToggleMute(void);

    virtual MuteState GetMuteState(void) const;
    virtual MuteState SetMuteState(MuteState);

    static MuteState NextMuteState(MuteState);

  protected:

    virtual int GetVolumeChannel(int channel) const = 0; // Returns 0-100
    virtual void SetVolumeChannel(int channel, int volume) = 0; // range 0-100 for vol

    void UpdateVolume(void);
    void SyncVolume(void);

    bool internal_vol;

 private:
    
    int volume;
    MuteState current_mute_state;

};

#endif

