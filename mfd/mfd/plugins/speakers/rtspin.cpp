/*
	rtspin.cpp

	(c) 2005 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
    Methods for the thread that actually receives rtsp and outputs to audio
	device
*/

#include <iostream>
using namespace std;

#include <liveMedia.hh>
#include <GroupsockHelper.hh>
#include <BasicUsageEnvironment.hh>

#include <mythtv/audiooutput.h>

#include "speakers.h"
#include "rtspin.h"
#include "settings.h"

static void afterReading(
                            void* clientData, 
                            unsigned frameSize,
                            unsigned,   //  numTruncatedBytes
                            struct timeval presentationTime,
                            unsigned    // durationInMicroseconds
                        )
{
    RtspIn *rtsp_in_object = (RtspIn *)clientData;
    rtsp_in_object->handleAfterReading(frameSize, presentationTime);
}


static void onSourceClosure(void* clientData)
{
    RtspIn *rtsp_in_object = (RtspIn *)clientData;
    rtsp_in_object->handleSourceClosure();
}

/*
---------------------------------------------------------------------
*/

RtspIn::RtspIn(
                Speakers *owner, 
                const QString &l_rtsp_url, 
                unsigned l_rtp_incoming_buffer_size
              )
{
    parent= owner;
    keep_going = true;
    rtsp_url = l_rtsp_url;
    
    scheduler = NULL;
    env = NULL;
    rtsp_client = NULL;
    media_session = NULL;
    sub_session = NULL;
    blocking_flag = 0;
    rtp_incoming_buffer_size = l_rtp_incoming_buffer_size;
    rtp_incoming_buffer = new unsigned char[rtp_incoming_buffer_size];

    //
    //  Need an audio output object to actually stuff the PCM data into.
    //
    
    QString adevice = mfdContext->GetSetting("AudioDevice");
    if (adevice.length() < 1)
    {
        adevice = mfdContext->getSetting("AudioOutputDevice");
        if (adevice.length() < 1)
        {
            warning("You have neither an AudioDevice nor an AudioOutputDevice "
                    "in your settings table, "
                    "will try /dev/dsp");
            adevice = "/dev/dsp";
        }
    }

    audio_output = AudioOutput::OpenAudio(adevice, 16, 2, 44100, AUDIOOUTPUT_MUSIC, false);
    audio_output->setBufferSize(256 * 1024);
    audio_output->SetBlocking(false);

}


void RtspIn::run()
{
    //
    //  Most of this is just lifted from liveMedia test programs,
    //  specifically playCommon and openRTSP
    //


    //
    //  Setup the liveMedia task scheduler and usage environment. Note, this
    //  things all run in a single thread (ie. this QThread).
    //

    scheduler = BasicTaskScheduler::createNew();
    env = BasicUsageEnvironment::createNew(*scheduler);

    //
    //  Create the liveMedia RTSP client
    //

    rtsp_client = RTSPClient::createNew(
                                        *env, 
                                        0,  // Change to 1 for console debugging
                                        "MythTV speakers"
                                       );
                                       
    if (rtsp_client == NULL)
    {
        warning(QString("failed to create an rtsp session from this url: %1")
                .arg(rtsp_url));
        cleanUp();
        return;
    }

    //
    //  Do a DESCRIBE request, which sets some internal stuff in the
    //  liveMedia RTSPClient and returns a char* description of the SDP
    //

    char *sdp_description = rtsp_client->describeURL(rtsp_url);

    //
    //  Using the returned sdp description, create a liveMedia MediaSession
    //    
    
    media_session = MediaSession::createNew(*env, sdp_description);
    delete[] sdp_description;
    
    if (!media_session)
    {
        warning(QString("failed to create a valid media session from this url: %1")
                .arg(rtsp_url));
        cleanUp();
        return;
    }
      
    //
    //  There should be only one subsession (16 by 2 by 44100 PCM data
    //  coming out of the rtspout object in the audio plugin).
    //  
    
    
    MediaSubsessionIterator iter(*media_session);
    sub_session = iter.next();

    if (!sub_session)
    {
        warning(QString("no subsession in SDP from %1, giving up")
                .arg(rtsp_url));
        cleanUp();
        return;
    }

    if (iter.next() != NULL)
    {
        warning(QString("more than one subsession available from %1, "
                        "hoping the first one works").arg(rtsp_url));
    }
    
    //
    //  Initiate the sub session
    //
    
    if (!sub_session->initiate())
    {
        warning(QString("failed to initiate first subsession from "
                        "SDP in %1, giving up")
                        .arg(rtsp_url));
        cleanUp();
        return;
    }

    int socket_fd = sub_session->rtpSource()->RTPgs()->socketNum();
    increaseReceiveBufferTo( *env, socket_fd, 100000 );
    
    //
    //  Setup the subsession
    //

    if (!rtsp_client->setupMediaSubsession(*sub_session, false, false))
    {
        warning(QString("failed to setup first subsession from "
                        "SDP in %1, givinp up")
                        .arg(rtsp_url));
        cleanUp();
        return;
    }
 
    //
    //  Explain what's about to happen, and on which ports
    //
    
    log(QString("about to tune to %1 with media type %2 on ports %3-%4")
        .arg(rtsp_url)
        .arg(sub_session->codecName())
        .arg(sub_session->clientPortNum())
        .arg(sub_session->clientPortNum() + 1)
        , 6);


    //
    //  Ensure playing will occur in the liveMedia event loop
    //
    
    if ( ! rtsp_client->playMediaSession(*media_session))
    {
        warning(QString("failed to initiate playback of %1, giving up")
                .arg(rtsp_url));
        cleanUp();
        return;
    }
    
    while(keep_going)
    {

        //
        //  This will block until new data arrives (unless our stop()
        //  command sets blocking_flag to non-0 in another thread)
        //
        //  The crux of what's going on below is that once some data
        //  arrives, the afterReading function fires with the new data.
        //

        blocking_flag = 0;
        sub_session->readSource()->getNextFrame(
                                                rtp_incoming_buffer,
                                                rtp_incoming_buffer_size, 
                                                afterReading,
                                                this,
                                                onSourceClosure,
                                                this 
                                               );

        TaskScheduler& scheduler = sub_session->readSource()->envir().taskScheduler();
        scheduler.doEventLoop(&blocking_flag);
    }

    cleanUp();
    
    //
    //  Wake up the parent (Speaker object). That will make the parent check
    //  if we are still running, and since we're now quitting, it can delete
    //  us.
    //

    parent->wakeUp();
}

void RtspIn::stop()
{
    keep_going_mutex.lock();
        keep_going = false;
        blocking_flag = ~0;
    keep_going_mutex.unlock();
}

void RtspIn::handleAfterReading(unsigned frameSize, struct timeval /* presentationTime */)
{
    //
    //  Set the blocking flag to non-zero so that the liveMedia event loop
    //  "falls through" in our main keep_going loop.
    //

    blocking_flag = ~0;
    
    //
    //  Deal with something that should not be able to happen
    //
    
    if(frameSize > rtp_incoming_buffer_size)
    {
        warning(QString("asked liveMedia not to give us more than %1 "
                        "bytes, yet it gave us %2. Quitting in the "
                        "interests of not segfaulting")
                        .arg(rtp_incoming_buffer_size)
                        .arg(frameSize));
        stop();
        return;
    }

    //
    //  RTP uses network byte ordering (big endian) in all cases. But PCM
    //  should be little endian, so we swap it around
    //  

    unsigned numValues = frameSize/2;
    short* value = (short*)rtp_incoming_buffer;
    for (unsigned i = 0; i < numValues; ++i)
    {
        short const orig = value[i];
        value[i] = ((orig&0xFF)<<8) | ((orig&0xFF00)>>8);
    }

    bool result = audio_output->AddSamples( (char *) rtp_incoming_buffer, frameSize / 4, -1);

    //
    //  Should be paying attention to the presentation time here. For now,
    //  just through away samples if the audio card's buffer is full
    //

    if (!result)
    {
        warning("throwing away audio samples, fix presentation time code");
    }
    

}

void RtspIn::handleSourceClosure()
{
    //
    //  Server going away
    //
    
    warning("RtspIn told source (server) is going away, so is stopping");
    stop();
}

void RtspIn::cleanUp()
{

    if (media_session && rtsp_client)
    {
        MediaSubsessionIterator iter(*media_session);
        MediaSubsession* subsession;

        while ((subsession = iter.next()) != NULL) 
        {
            //
            //  Teardowns can take a while, especially if the other end has
            //  died. We only wait for them to happen if our parent (the
            //  speaker object) is not trying to shut down.
            //
            
            if(parent->keepGoing())
            {
                rtsp_client->teardownMediaSubsession(*subsession);
            }
        }
    }

    UsageEnvironment* env = NULL;
    TaskScheduler* scheduler = NULL;

    if (media_session != NULL) 
    {
        env = &(media_session->envir());
        scheduler = &(env->taskScheduler());
        Medium::close(media_session);
    }

    
    if (rtsp_client)
    {
        Medium::close(rtsp_client);
    }
    if (env)
    {
        env->reclaim();
    }
    if (scheduler)
    {
        delete scheduler;
        scheduler = NULL;
    }
}

void RtspIn::warning(const QString &warn_message)
{
    if (parent)
    {
        parent->warning(warn_message);
    }
}

void RtspIn::log(const QString &log_message, int verbosity)
{
    if (parent)
    {
        parent->log(log_message, verbosity);
    }
}

RtspIn::~RtspIn()
{
    if (rtp_incoming_buffer)
    {
        delete [] rtp_incoming_buffer;
        rtp_incoming_buffer = NULL;
    }
    if (audio_output)
    {
        delete audio_output;
        audio_output = NULL;
    }
}
