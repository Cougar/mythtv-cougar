#include "../config.h"

#ifdef WMA_AUDIO_SUPPORT
/*
    MythTV WMA Decoder
    Written by Kevin Kuphal

    Special thanks to 
       ffmpeg team for libavcodec and libavformat
       qemacs team for their av support which I used to understand the libraries
       getid3.sourceforget.net project for the ASF information used here
        
    This library decodes Windows Media (WMA/ASF) files into PCM data 
    returned to the MythMusic output buffer.  

    Revision History
        - Initial release
        - 1/9/2004 - Improved seek support
*/

#include <assert.h>
#include <iostream>
#include <string>
#include <qobject.h>
#include <qiodevice.h>
#include <qfile.h>

using namespace std;

#include "avfdecoder.h"
#include "constants.h"
#include "buffer.h"
#include "output.h"
#include "recycler.h"
#include "metadata.h"

#include <mythtv/mythcontext.h>

typedef struct {
    uint32_t v1;
    uint16_t v2;
    uint16_t v3;
    uint8_t v4[8];
} GUID;

// These definitions and functions are used to support the ASF parsing
// Submitted to libavformat team for inclusion but used here for simplicity
// Should be removed if/when libavformat includes the changes

static const GUID extended_content_header = 
{
    0xD2D0A440, 0xE307, 0x11D2, 
    { 0x97, 0xF0, 0x00, 0xA0, 0xC9, 0x5E, 0xA8, 0x50 },
};

/*
static void get_guid(ByteIOContext *s, GUID *g)
{
    int i;

    g->v1 = get_le32(s);
    g->v2 = get_le16(s);
    g->v3 = get_le16(s);
    for(i=0;i<8;i++)
        g->v4[i] = get_byte(s);
}

static void get_str16_nolen(ByteIOContext *pb, int len, char *buf, int buf_size)
{
    int c;
    char *q;

    q = buf;
    while (len > 0) 
    {
        c = get_le16(pb);
        if ((q - buf) < buf_size - 1)
            *q++ = c;
        len-=2;
    }
    *q = '\0';
}

*/

// Begin the class definition

avfDecoder::avfDecoder(const QString &file, DecoderFactory *d, QIODevice *i, 
                       Output *o) 
          : Decoder(d, i, o)
{
    filename = file;
    inited = FALSE;
    user_stop = FALSE;
    stat = 0;
    bks = 0;
    done = FALSE;
    finish = FALSE;
    len = 0;
    freq = 0;
    bitrate = 0;
    seekTime = -1.0;
    totalTime = 0.0;
    chan = 0;
    output_size = 0;
    output_buf = 0;
    output_bytes = 0;
    output_at = 0;

    ic = NULL;
    oc = NULL;
    ifmt = NULL;
    ap = &params;
    pkt = &pkt1;
}

avfDecoder::~avfDecoder(void)
{
    if (inited)
        deinit();

    if (output_buf)
        delete [] output_buf;
    output_buf = 0;
}

void avfDecoder::stop()
{
    user_stop = TRUE;
}

void avfDecoder::flush(bool final)
{
    ulong min = final ? 0 : bks;

    while ((!done && !finish && seekTime <= 0) && output_bytes > min) 
    {
        output()->recycler()->mutex()->lock();

        while ((!done && !finish && seekTime <= 0) && 
               output()->recycler()->full()) 
        {
            mutex()->unlock();

            output()->recycler()->cond()->wait(output()->recycler()->mutex());

            mutex()->lock();
            done = user_stop;
        }

        if (user_stop || finish) 
        {
            inited = FALSE;
            done = TRUE;
        } 
        else 
        {
            ulong sz = output_bytes < bks ? output_bytes : bks;
            Buffer *b = output()->recycler()->get();

            memcpy(b->data, output_buf, sz);
            if (sz != bks) 
                memset(b->data + sz, 0, bks - sz);

            b->nbytes = bks;
            b->rate = bitrate;
            output_size += b->nbytes;
            output()->recycler()->add();

            output_bytes -= sz;
            memmove(output_buf, output_buf + sz, output_bytes);
            output_at = output_bytes;
        }

        if (output()->recycler()->full()) 
            output()->recycler()->cond()->wakeOne();

        output()->recycler()->mutex()->unlock();
    }
}

bool avfDecoder::initialize()
{
    bks = blockSize();

    inited = user_stop = done = finish = FALSE;
    len = freq = bitrate = 0;
    stat = chan = 0;
    output_size = 0;
    seekTime = -1.0;
    totalTime = 0.0;

    filename = ((QFile *)input())->name();
   
    if (!output_buf)
        output_buf = new char[globalBufferSize];
    output_at = 0;
    output_bytes = 0;

    // open device
    // register av codecs
    av_register_all();

    // open the media file
    // this should populate the input context
    if (av_open_input_file(&ic, filename, ifmt, 0, ap) < 0)
        return FALSE;    

    // determine the stream format
    // this also populates information needed for metadata
    if (av_find_stream_info(ic) < 0) 
        return FALSE;

    // Store the audio codec of the stream
    audio_dec = &ic->streams[0]->codec;

    // Store the input format of the context
    ifmt = ic->iformat;

    // Determine the output format
    // Given we are outputing to a sound card, this will always
    // be a PCM format
    fmt = guess_format("audio_device", NULL, NULL);
    if (!fmt)
        return FALSE;

    // Populate the output context
    // Create the output stream and attach to output context
    // Set various parameters to match the input format
    oc = (AVFormatContext *)av_mallocz(sizeof(AVFormatContext));
    oc->oformat = fmt;

    dec_st = av_new_stream(oc,0);
    dec_st->codec.codec_type = CODEC_TYPE_AUDIO;
    dec_st->codec.codec_id = oc->oformat->audio_codec;
    dec_st->codec.sample_rate = audio_dec->sample_rate;
    dec_st->codec.channels = audio_dec->channels;
    dec_st->codec.bit_rate = audio_dec->bit_rate;
    av_set_parameters(oc, NULL);

    // Prepare the decoding codec
    // The format is different than the codec
    // While we could get fed a WAV file, it could contain a number
    // of different codecs
    codec = avcodec_find_decoder(audio_dec->codec_id);
    if (!codec)
        return FALSE;
    if (avcodec_open(audio_dec,codec) < 0)
        return FALSE;
    totalTime = (ic->duration / AV_TIME_BASE) * 1000;

    if (output())
    {
        output()->configure(audio_dec->sample_rate, 
                            audio_dec->channels, 
                            16, 
                            audio_dec->sample_rate * 
                            audio_dec->channels * 16);
    }

    inited = TRUE;
    return TRUE;
}

void avfDecoder::seek(double pos)
{
    seekTime = pos;
}

void avfDecoder::deinit()
{
    inited = user_stop = done = finish = FALSE;
    len = freq = bitrate = 0;
    stat = chan = 0;
    output_size = 0;
    setInput(0);
    setOutput(0);

    // Cleanup here
    if(ic)
    {
        av_close_input_file(ic);
        ic = NULL;
    }
    if(oc)
    {
        av_free(oc);
        oc = NULL;
    }
}

void avfDecoder::run()
{
    int mem_len;
    char *s;

    mutex()->lock();

    if (!inited) 
    {
        mutex()->unlock();
        return;
    }

    mutex()->unlock();

    av_read_play(ic);
    while (!done && !finish && !user_stop) 
    {
        mutex()->lock();

        // Look to see if user has requested a seek
        if (seekTime >= 0.0) 
        {
            if (av_seek_frame(ic, 0, (int64_t)(seekTime * AV_TIME_BASE)) < 0)
            {
                cerr << "avfdecoder.o: error seeking" << endl;
            }

            seekTime = -1.0;
        }

        // Read a packet from the input context
        // if (av_read_packet(ic, pkt) < 0)
        if (av_read_frame(ic, pkt) < 0)
        {
            message("decoder error");
            mutex()->unlock();
            finish = TRUE;
            break;
        }

        // Get the pointer to the data and its length
        ptr = pkt->data;
        len = pkt->size;
        mutex()->unlock();


        while (len > 0 && !done && !finish && !user_stop && seekTime <= 0.0)  
        {
            mutex()->lock();
            // Decode the stream to the output codec
            // Samples is the output buffer
            // data_size is the size in bytes of the frame
            // ptr is the input buffer
            // len is the size of the input buffer
            dec_len = avcodec_decode_audio(audio_dec, samples, &data_size, 
                                           ptr, len);    
                                           
            if (dec_len < 0) 
            {
                mutex()->unlock();
                break;
            }

            s = (char *)samples;
            mutex()->unlock();

            while (data_size > 0 && !done && !finish && !user_stop && 
                   seekTime <= 0.0) 
             {
                mutex()->lock();
                // Store and check the size
                // It is possible the returned data is larger than
                // the output buffer.  If so, flush the buffer and
                // limit the data written to the buffer size
                mem_len = data_size;
                if ((output_at + data_size) > globalBufferSize)
                {
                    // if (output()) { flush(); }
                    mem_len = globalBufferSize - output_at;
                }

                // Copy the data to the output buffer
                memcpy((char *)(output_buf + output_at), s, mem_len);

                // Increment the output pointer and count
                output_at += mem_len;
                output_bytes += mem_len;

                // Move the input buffer pointer and mark off
                // what we sent to the output buffer
                data_size -= mem_len;
                s += mem_len;
            
                if (output())
                    flush();

                mutex()->unlock();
            }

            mutex()->lock();
            flush();
            ptr += dec_len;
            len -= dec_len;
            mutex()->unlock();
        }
        av_free_packet(pkt);
    }

    flush(TRUE);

    mutex()->lock();
    if(finish)
    {
        message("decoder finish");
    }
    else if(done)
    {
        message("decoder stop");
    }
    mutex()->unlock();

    deinit();
}

AudioMetadata* avfDecoder::getMetadata()
{

    QString artist = "", album = "", title = "", genre = "";
    int year = 0, tracknum = 0, length = 0;

    // This function gets called during the search for music
    // data and may not have initialized the library
    av_register_all();

    // Open the specified file and populate the metadata info
    if (av_open_input_file(&ic, filename, ifmt, 0, ap) < 0)
        return NULL;
    if (av_find_stream_info(ic) < 0)
        return NULL;

    av_estimate_timings(ic);

    artist += (char *)ic->author;
    album += (char *)ic->album;
    title += (char *)ic->title;
    genre += (char *)ic->genre;
    year = ic->year;
    tracknum = ic->track;
    length = (ic->duration / AV_TIME_BASE) * 1000;

    AudioMetadata *retdata = new AudioMetadata(
                                                filename, 
                                                artist, 
                                                album, 
                                                title, 
                                                genre,
                                                year, 
                                                tracknum, 
                                                length
                                              );
    retdata->setBitrate(ic->bit_rate);
    if(ic)
    {
        av_close_input_file(ic);
        ic = NULL;
    }

    if(oc)
    {
        av_free(oc);
        oc = NULL;
    }

    return retdata;
}    

bool avfDecoderFactory::supports(const QString &source) const
{
    return (source.right(extension().length()).lower() == extension());
}

const QString &avfDecoderFactory::extension() const
{
    static QString ext(".wma");
    return ext;
}

const QString &avfDecoderFactory::description() const
{
    static QString desc(QObject::tr("Windows Media Audio"));
    return desc;
}

Decoder *avfDecoderFactory::create(const QString &file, QIODevice *input, 
                                  Output *output, bool deletable)
{
    if (deletable)
        return new avfDecoder(file, this, input, output);

    static avfDecoder *decoder = 0;
    if (!decoder) 
    {
        decoder = new avfDecoder(file, this, input, output);
    } 
    else 
    {
        decoder->setInput(input);
        decoder->setOutput(output);
    }

    return decoder;
}

#endif
