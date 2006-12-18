/*****************************************************************************
 * = NAME
 * audiooutputca.cpp
 *
 * = DESCRIPTION
 * Core Audio glue for Mac OS X.
 * This plays MythTV audio through the default output device on OS X.
 *
 * = REVISION
 * $Id$
 *
 * = AUTHORS
 * Jeremiah Morris
 *****************************************************************************/

#include <CoreServices/CoreServices.h>
#include <CoreAudio/CoreAudio.h>
#include <AudioUnit/AudioUnit.h>

using namespace std;

#include "mythcontext.h"
#include "audiooutputca.h"
#include "config.h"

// this holds Core Audio member variables
struct CoreAudioData {
    AudioUnit      mOutputUnit;
};

// This callback communicates with Core Audio.
OSStatus AuCA_AURender(void *inRefCon,
                       AudioUnitRenderActionFlags *ioActionFlags,
                       const AudioTimeStamp *inTimeStamp,
                       UInt32 inBusNumber,
                       UInt32 inNumberFrames,
                       AudioBufferList *ioData);


/** \class AudioOutputCA
 *  \brief Implements Core Audio (Mac OS X Hardware Abstraction Layer) output.
 */

AudioOutputCA::AudioOutputCA(QString laudio_main_device,
                             QString laudio_passthru_device,
                             int laudio_bits, int laudio_channels,
                             int laudio_samplerate,
                             AudioOutputSource lsource,
                             bool lset_initial_vol, bool laudio_passthru)
    : AudioOutputBase(laudio_main_device, laudio_passthru_device,
                      laudio_bits,        laudio_channels,
                      laudio_samplerate,  lsource,
                      lset_initial_vol,   laudio_passthru),
      d(new CoreAudioData()),
      bufferedBytes(0)
{
    // Create private data
    d->mOutputUnit = NULL;

    Reconfigure(laudio_bits, laudio_channels,
                laudio_samplerate, laudio_passthru);
}

AudioOutputCA::~AudioOutputCA()
{
    KillAudio();

    delete d;
}

bool AudioOutputCA::OpenDevice()
{
    // Get default output device
    ComponentDescription desc;
    desc.componentType = kAudioUnitType_Output;
    desc.componentSubType = kAudioUnitSubType_DefaultOutput;
    desc.componentManufacturer = kAudioUnitManufacturer_Apple;
    desc.componentFlags = 0;
    desc.componentFlagsMask = 0;

    Component comp = FindNextComponent(NULL, &desc);
    if (comp == NULL)
    {
        Error(QString("FindNextComponent failed"));
        return false;
    }

    OSStatus err = OpenAComponent(comp, &d->mOutputUnit);
    if (err)
    {
        Error(QString("OpenAComponent returned %1").arg((long)err));
        return false;
    }

    // Attach callback to default output
    AURenderCallbackStruct input;
    input.inputProc = AuCA_AURender;
    input.inputProcRefCon = this;

    err = AudioUnitSetProperty(d->mOutputUnit,
                               kAudioUnitProperty_SetRenderCallback,
                               kAudioUnitScope_Input,
                               0, &input, sizeof(input));
    if (err)
    {
        Error(QString("AudioUnitSetProperty (callback) returned %1")
                      .arg((long)err));
        return false;
    }

    // base class does this after OpenDevice, but we need it now
    audio_bytes_per_sample = audio_channels * audio_bits / 8;

    // Set up the audio output unit
    AudioStreamBasicDescription conv_in_desc;
    bzero(&conv_in_desc, sizeof(AudioStreamBasicDescription));
    conv_in_desc.mSampleRate       = audio_samplerate;
    conv_in_desc.mFormatID         = kAudioFormatLinearPCM;
    conv_in_desc.mFormatFlags      = kLinearPCMFormatFlagIsSignedInteger;
#ifdef WORDS_BIGENDIAN
    conv_in_desc.mFormatFlags     |= kLinearPCMFormatFlagIsBigEndian;
#endif
    conv_in_desc.mBytesPerPacket   = audio_bytes_per_sample;
    conv_in_desc.mFramesPerPacket  = 1;
    conv_in_desc.mBytesPerFrame    = audio_bytes_per_sample;
    conv_in_desc.mChannelsPerFrame = audio_channels;
    conv_in_desc.mBitsPerChannel   = audio_bits;

    err = AudioUnitSetProperty(d->mOutputUnit,
                               kAudioUnitProperty_StreamFormat,
                               kAudioUnitScope_Input,
                               0, &conv_in_desc,
                               sizeof(AudioStreamBasicDescription));
    if (err)
    {
        Error(QString("AudioUnitSetProperty returned %1").arg((long)err));
        return false;
    }

    // We're all set up - start the audio output unit
    ComponentResult res = AudioUnitInitialize(d->mOutputUnit);
    if (res)
    {
        Error(QString("AudioUnitInitialize returned %1").arg((long)res));
        return false;
    }

    err = AudioOutputUnitStart(d->mOutputUnit);
    if (err)
    {
        Error(QString("AudioOutputUnitStart returned %1").arg((long)err));
        return false;
    }

    if (internal_vol && set_initial_vol)
    {
        QString controlLabel = gContext->GetSetting("MixerControl", "PCM");
        controlLabel += "MixerVolume";
        SetCurrentVolume(gContext->GetNumSetting(controlLabel, 80));
    }

    return true;
}

void AudioOutputCA::CloseDevice()
{
    if (d->mOutputUnit)
    {
        AudioOutputUnitStop(d->mOutputUnit);
        AudioUnitUninitialize(d->mOutputUnit);
        AudioUnitReset(d->mOutputUnit, kAudioUnitScope_Input, NULL);
        CloseComponent(d->mOutputUnit);
        d->mOutputUnit = NULL;
    }
}

/** Object-oriented part of callback */
bool AudioOutputCA::RenderAudio(unsigned char *aubuf,
                                int size, unsigned long long timestamp)
{
    if (pauseaudio || killaudio)
    {
        audio_actually_paused = true;
        return false;
    }

    /* This callback is called when the sound system requests
       data.  We don't want to block here, because that would
       just cause dropouts anyway, so we always return whatever
       data is available.  If we haven't received enough, either
       because we've finished playing or we have a buffer
       underrun, we play silence to fill the unused space.  */

    int written_size = GetAudioData(aubuf, size, false);
    if (written_size && (size > written_size))
    {
        // play silence on buffer underrun
        bzero(aubuf + written_size, size - written_size);
    }

    /* update audiotime (bufferedBytes is read by getBufferedOnSoundcard) */
    UInt64 nanos = AudioConvertHostTimeToNanos(
                        timestamp - AudioGetCurrentHostTime());
    bufferedBytes = (int)((nanos / 1000000000.0) *    // secs
                          (effdsp / 100.0) *          // samples/sec
                          audio_bytes_per_sample);    // bytes/sample
    SetAudiotime();

    return (written_size > 0);
}

void AudioOutputCA::WriteAudio(unsigned char *aubuf, int size)
{
    (void)aubuf;
    (void)size;
    return;     // unneeded and unused in CA
}

int AudioOutputCA::getSpaceOnSoundcard(void)
{
    return 0;   // unneeded and unused in CA
}

int AudioOutputCA::getBufferedOnSoundcard(void)
{
    return bufferedBytes;
}

/** Reimplement the base class's version of GetAudiotime()
 *  so that we don't use gettimeofday or pthread mutexes.
 */
int AudioOutputCA::GetAudiotime(void)
{
    int ret;

    if (GetBaseAudioTime() == 0)
        return 0;

    UInt64 hostNanos = AudioConvertHostTimeToNanos(AudioGetCurrentHostTime());
    ret = (hostNanos / 1000000) - CA_audiotime_updated;
	
    ret += GetBaseAudioTime();

    return ret;
}

/** Reimplement base's SetAudiotime()
 *  without gettimeofday() or pthread mutexes.
 */
void AudioOutputCA::SetAudiotime(void)
{
    if (GetBaseAudBufTimeCode() == 0)
        return;

    int soundcard_buffer = 0;
    int totalbuffer;


    soundcard_buffer = getBufferedOnSoundcard();
    totalbuffer = audiolen(false) + soundcard_buffer;
 
    if (GetSoundStretch())
        totalbuffer += (int)((GetSoundStretch()->numUnprocessedSamples() *
                              audio_bytes_per_sample) / audio_stretchfactor);
 
    SetBaseAudioTime(GetBaseAudBufTimeCode() - (int)(totalbuffer * 100000.0 /
                                   (audio_bytes_per_sample * effdspstretched)));
 
    // We also store here the host time stamp of when the update occurred.
    // That way when asked later for the audio time we can use
    // the sum of the output time code info calculated here,
    // and the delta since when it was calculated and when we asked.

    // Note that since we're using a single 32bit variable here
    // there is no need to use a lock or similar, since the 32 bit
    // accesses are atomic. Also we use AudioHost time here
    // so that we're all referring to the same clock.

    UInt64 hostNanos = AudioConvertHostTimeToNanos(AudioGetCurrentHostTime());
    CA_audiotime_updated = hostNanos / 1000000;
}

void AudioOutputCA::StartOutputThread(void)
{
    return;     // no thread for CA
}

void AudioOutputCA::StopOutputThread(void)
{
    return;     // no thread for CA
}

/* This callback provides converted audio data to the default output device. */
OSStatus AuCA_AURender(void *inRefCon,
                       AudioUnitRenderActionFlags *ioActionFlags,
                       const AudioTimeStamp *inTimeStamp,
                       UInt32 inBusNumber,
                       UInt32 inNumberFrames,
                       AudioBufferList *ioData)
{
    (void)inBusNumber;
    (void)inNumberFrames;

    AudioOutputCA *inst = (AudioOutputCA *)inRefCon;

    if (!inst->RenderAudio((unsigned char *)(ioData->mBuffers[0].mData),
                           ioData->mBuffers[0].mDataByteSize,
                           inTimeStamp->mHostTime))
    {
        // play silence if RenderAudio returns false
        bzero(ioData->mBuffers[0].mData, ioData->mBuffers[0].mDataByteSize);
        *ioActionFlags = kAudioUnitRenderAction_OutputIsSilence;
    }
    return noErr;
}

int AudioOutputCA::GetVolumeChannel(int channel)
{
    // FIXME: this only returns global volume
    (void)channel;
    Float32 volume;

    if (!AudioUnitGetParameter(d->mOutputUnit,
                               kHALOutputParam_Volume,
                               kAudioUnitScope_Global, 0, &volume))
        return (int)lroundf(volume * 100.0);

    return 0;    // error case
}

void AudioOutputCA::SetVolumeChannel(int channel, int volume)
{
    // FIXME: this only sets global volume
    (void)channel;
     AudioUnitSetParameter(d->mOutputUnit, kHALOutputParam_Volume,
                           kAudioUnitScope_Global, 0, (volume * 0.01), 0);
}
