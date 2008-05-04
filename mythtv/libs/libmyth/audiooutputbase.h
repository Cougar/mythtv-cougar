#ifndef AUDIOOUTPUTBASE
#define AUDIOOUTPUTBASE

// POSIX headers
#include <pthread.h>
#include <sys/time.h> // for struct timeval

// C++ headers
#include <iostream>
using namespace std;

// Qt headers
#include <qstring.h>
#include <qmutex.h>

// MythTV headers
#include "audiooutput.h"
#include "samplerate.h"

namespace soundtouch {
class SoundTouch;
};
class FreeSurround;
class AudioOutputDigitalEncoder;
struct AVCodecContext;

class AudioOutputBase : public AudioOutput
{
 public:
    AudioOutputBase(const AudioSettings &settings);
    virtual ~AudioOutputBase();

    // reconfigure sound out for new params
    virtual void Reconfigure(const AudioSettings &settings);

    // do AddSamples calls block?
    virtual void SetBlocking(bool blocking);

    // dsprate is in 100 * samples/second
    virtual void SetEffDsp(int dsprate);

    virtual void SetStretchFactor(float factor);
    virtual float GetStretchFactor(void) const;

    virtual void Reset(void);

    // timecode is in milliseconds.
    virtual bool AddSamples(char *buffer, int samples, long long timecode);
    virtual bool AddSamples(char *buffers[], int samples, long long timecode);

    virtual void SetTimecode(long long timecode);
    virtual bool IsPaused(void) const { return audio_actually_paused; }
    virtual void Pause(bool paused);

    // Wait for all data to finish playing
    virtual void Drain(void);

    virtual int GetAudiotime(void) const;
    virtual int GetAudioBufferedTime(void) const;

    // Send output events showing current progress
    virtual void Status(void);

    virtual void SetSourceBitrate(int rate);

    virtual void GetBufferStatus(uint &fill, uint &total) const;

    //  Only really used by the AudioOutputNULL object

    virtual void bufferOutputData(bool y){ buffer_output_data_for_use = y; }
    virtual int readOutputData(unsigned char *read_buffer, int max_length);

    static const uint kAudioSourceInputSize  = 16384;
    static const uint kAudioSourceOutputSize = (16384*6);
    static const uint kAudioTempBufSize      = (16384*6);
    /// Audio Buffer Size -- should be divisible by 12,10,8,6,4,2..
    static const uint kAudioRingBufferSize   = 1536000;

 protected:
    // You need to implement the following functions
    virtual bool OpenDevice(void) = 0;
    virtual void CloseDevice(void) = 0;
    virtual void WriteAudio(unsigned char *aubuf, int size) = 0;
    virtual int  GetSpaceOnSoundcard(void) const = 0;
    virtual int  GetBufferedOnSoundcard(void) const = 0;
    /// You need to call this from any implementation in the dtor.
    void KillAudio(void);

    // The following functions may be overridden, but don't need to be
    virtual bool StartOutputThread(void);
    virtual void StopOutputThread(void);

    int GetAudioData(unsigned char *buffer, int buf_size, bool fill_buffer);

    void _AddSamples(void *buffer, bool interleaved, int samples, long long timecode);

    void OutputAudioLoop(void);
    static void *kickoffOutputAudioLoop(void *player);
    void SetAudiotime(void);
    int WaitForFreeSpace(int len);

    int audiolen(bool use_lock) const; // number of valid bytes in audio buffer
    int audiofree(bool use_lock) const; // number of free bytes in audio buffer

    void UpdateVolume(void);

    void SetStretchFactorLocked(float factor);

    int GetBaseAudioTime()                    const { return audiotime;       }
    int GetBaseAudBufTimeCode()               const { return audbuf_timecode; }
    soundtouch::SoundTouch *GetSoundStretch() const { return pSoundStretch;   }
    void SetBaseAudioTime(const int inAudioTime) { audiotime = inAudioTime; }

  protected:
    int effdsp; // from the recorded stream
    int effdspstretched; // from the recorded stream

    // Basic details about the audio stream
    int audio_channels;
    int audio_bytes_per_sample;
    int audio_bits;
    int audio_samplerate;
    mutable int audio_buffer_unused; ///< set on error conditions
    int fragment_size;
    long soundcard_buffer_size;
    QString audio_main_device;
    QString audio_passthru_device;

    bool audio_passthru;

    float audio_stretchfactor;
    AVCodecContext *audio_codec;
    AudioOutputSource source;

    bool killaudio;

    bool pauseaudio, audio_actually_paused, was_paused;
    bool set_initial_vol;
    bool buffer_output_data_for_use; //  used by AudioOutputNULL

    int configured_audio_channels;

 private:
    // resampler
    bool need_resampler;
    SRC_STATE *src_ctx;

    // timestretch
    soundtouch::SoundTouch    *pSoundStretch;
    AudioOutputDigitalEncoder *encoder;
    FreeSurround              *upmixer;

    int source_audio_channels;
    int source_audio_bytes_per_sample;
    bool needs_upmix;
    int surround_mode;

    bool blocking; // do AddSamples calls block?

    int lastaudiolen;
    long long samples_buffered;

    bool audio_thread_exists;
    pthread_t audio_thread;

    /** adjustments to audiotimecode, waud, and raud can only be made
        while holding this lock */
    mutable pthread_mutex_t audio_buflock;

    /** condition is signaled when the buffer gets more free space.
        Must be holding audio_buflock to use. */
    pthread_cond_t audio_bufsig;

    /** must hold avsync_lock to read or write 'audiotime' and
        'audiotime_updated' */
    mutable pthread_mutex_t avsync_lock;

    /// timecode of audio leaving the soundcard (same units as timecodes)
    long long audiotime;
    struct timeval audiotime_updated; // ... which was last updated at this time

    /* Audio circular buffer */
    int raud, waud;     /* read and write positions */
    /// timecode of audio most recently placed into buffer
    long long audbuf_timecode;

    int numlowbuffer;

    QMutex killAudioLock;

    long current_seconds;
    long source_bitrate;

    // All actual buffers
    SRC_DATA src_data;
    uint memory_corruption_test0;
    float src_in[kAudioSourceInputSize];
    uint memory_corruption_test1;
    float src_out[kAudioSourceOutputSize];
    uint memory_corruption_test2;
    short tmp_buff[kAudioTempBufSize];
    uint memory_corruption_test3;
    /** main audio buffer */
    unsigned char audiobuffer[kAudioRingBufferSize];
    uint memory_corruption_test4;
};

#endif

