/*
	audio.cpp

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	

*/

#include <qapplication.h>
#include <qfileinfo.h>

#include "mfd_events.h"

#include "audio.h"
#include "audiooutput.h"
#include "daapinput.h"

AudioPlugin::AudioPlugin(MFD *owner, int identity)
      :MFDServicePlugin(owner, identity, 2343, "audio")
{
    input = NULL;
    output = NULL;
    decoder = NULL;
    output_buffer_size = 256;
    elapsed_time = 0;
    is_playing = false;
    is_paused = false;
    audio_device = "/dev/dsp";  //  <--- ewww ... change that soon
    state_of_play_mutex = new QMutex(true);    
    metadata_server = parent->getMetadataServer();
    stopPlaylistMode();
}

void AudioPlugin::swallowOutputUpdate(int numb_seconds, int channels, int bitrate, int frequency)
{
    //
    //  Get the latest data about the state of play
    //

    int current_playlist = -1;
    int current_playlist_item = -1;

    playlist_mode_mutex.lock();
        play_data_mutex.lock();
            elapsed_time = numb_seconds;
            current_channels = channels;
            current_bitrate = bitrate;
            current_frequency = frequency;
            if(playlist_mode)
            {
                current_playlist = current_playlist_id;
                current_playlist_item = current_playlist_item_index;
            }
        play_data_mutex.unlock();
    playlist_mode_mutex.unlock();

    //
    //  Tell the clients what's going on
    //
    

    if(is_playing)
    {
        sendMessage(QString("playing %1 %2 %3 %4 %5 %6 %7 %8")
                            .arg(current_playlist)
                            .arg(current_playlist_item)
                            .arg(current_collection)
                            .arg(current_metadata)
                            .arg(elapsed_time)
                            .arg(current_channels)
                            .arg(current_bitrate)
                            .arg(current_frequency));
    }
}

void AudioPlugin::run()
{
    char my_hostname[2049];
    QString local_hostname = "unknown";

    if(gethostname(my_hostname, 2048) < 0)
    {
        warning("could not call gethostname()");
       
    }
    else
    {
        local_hostname = my_hostname;
    }

    ServiceEvent *se = new ServiceEvent(QString("services add macp %1 myth audio control on %2")
                                       .arg(port_number)
                                       .arg(local_hostname));
    QApplication::postEvent(parent, se);



    //
    //  Init our sockets
    //
    
    if( !initServerSocket())
    {
        fatal("audio could not initialize its core server socket");
        return;
    }
    
    while(keep_going)
    {
        //
        //  Update the status of our sockets and wait for something to happen
        //
        
        updateSockets();
        waitForSomethingToHappen();
        checkInternalMessages();
        
    }

    stopAudio();
}

void AudioPlugin::doSomething(const QStringList &tokens, int socket_identifier)
{
    bool ok = true;

    if(tokens.count() < 1)
    {
        ok = false;
    }
    else
    {
        if(tokens[0] == "play")
        {
            if(tokens.count() < 3)
            {
                ok = false;
            }
            else
            {
                if(tokens[1] == "url")
                {
                    QUrl play_url(tokens[2]);
                    if(!playUrl(play_url))
                    {
                        ok = false;
                    }
                }
                else if(tokens[1] == "item" || tokens[1] == "metadata")
                {
                    if(tokens.count() < 4)
                    {
                        ok = false;
                    }
                    else
                    {
                        bool parsed_ok = true;
                        int collection_id = tokens[2].toInt(&parsed_ok);
                        if(!parsed_ok)
                        {
                            ok = false;
                        }
                        else
                        {
                            int metadata_id = tokens[3].toInt(&parsed_ok);
                            if(!parsed_ok)
                            {
                                ok = false;
                            }
                            else
                            {
                                if(!playMetadata(collection_id, metadata_id))
                                {
                                    ok = false;
                                }
                            }
                        }
                    }
                }
                else if(
                        tokens[1] == "container" || 
                        tokens[1] == "playlist"  || 
                        tokens[1] == "list"
                        )
                {
                    if(tokens.count() < 4)
                    {
                        ok = false;
                    }
                    else
                    {
                        bool parsed_ok = true;
                        int collection_id = tokens[2].toInt(&parsed_ok);
                        if(!parsed_ok)
                        {
                            ok = false;
                        }
                        else
                        {
                            int playlist_id = tokens[3].toInt(&parsed_ok);
                            if(!parsed_ok)
                            {
                                ok = false;
                            }
                            else
                            {
                                if(tokens.count() >= 5)
                                {
                                    int index_position = tokens[4].toInt(&parsed_ok);
                                    if(!parsed_ok)
                                    {
                                        ok = false;
                                    }
                                    else
                                    {
                                        setPlaylistMode(collection_id, playlist_id, index_position);
                                        playFromPlaylist();
                                    }
                                }
                                else
                                {
                                    setPlaylistMode(collection_id, playlist_id);
                                    playFromPlaylist();
                                }
                            }
                        }
                    }
                    
                }
                else
                {
                    ok = false;
                }
            }
        }
        else if(tokens[0] == "pause")
        {
            if(tokens.count() < 2)
            {
                ok = false;
            }
            else if (tokens[1] == "on")
            {
                pauseAudio(true);
            }
            else if (tokens[1] == "off")
            {
                pauseAudio(false);
            }
            else
            {
                ok = false;
            }
        }
        else if(tokens[0] == "seek")
        {
            if(tokens.count() < 2)
            {
                ok = false;
            }
            else
            {
                bool convert_ok;
                int seek_amount = tokens[1].toInt(&convert_ok);
                if(convert_ok)
                {
                    seekAudio(seek_amount);
                }
                else
                {
                    ok = false;
                }
            }
        }
        else if(tokens[0] == "stop")
        {
            stopAudio();
        }
        else if(tokens[0] == "next")
        {
            nextPrevAudio(true);
        }
        else if(tokens[0] == "previous" || tokens[0] == "prev")
        {
            nextPrevAudio(false);
        }
        else
        {
            ok = false;
        }
    }

    if(!ok)
    {
        warning(QString("did not understand or had "
               "problems with these tokens: %1")
               .arg(tokens.join(" ")));
        huh(tokens, socket_identifier);
    }
}


bool AudioPlugin::playUrl(QUrl url)
{
    log(QString("attempting to play this url: \"%1\"")
                .arg(url), 9);

    //
    //  Before we do anything else, check to see if we can access this new
    //  url. If we can't, we don't want to stop anything that might already
    //  be playing.
    //
    
    if(!url.isValid())
    {
        warning(QString("passed an invalid url: %1").arg(url.toString()));
        return false;
    }
    if ( url.isLocalFile())
    {
        QString   path = url.path();

        //
        //  for filenames with # in them
        //

        if(url.hasRef())
        {
            path += "#";
            path += url.ref();
        }
        QFileInfo fi( path );
        if ( ! fi.exists() && fi.extension() != "cda") 
        {
            warning(QString("passed url to a file that does not exist: %1")
                    .arg(path));
            return false;
        }
    }
    else if(
            url.protocol() == "daap" ||
            url.protocol() == "cd"
           )
    {
        //
        //  Some way to check that this resource still exists?
        //
    }
    
    //
    //  If we are currently playing, we need to stop
    //
    

    state_of_play_mutex->lock();
        if(is_playing || is_paused)
        {
            state_of_play_mutex->unlock();
            stopAudio();
            state_of_play_mutex->lock();
        }

        bool start_output = false;

        if(!output)
        {
            //
            //  output may not exist because this is a first run
            //  or stopAudio() killed it off.
            //
        
            output = new MMAudioOutput(this, output_buffer_size * 1024, "/dev/dsp");
            output->setBufferSize(output_buffer_size * 1024);
            if(!output->initialize())
            {
                warning("could not initialize an output object");
                delete output;
                output = NULL;
                state_of_play_mutex->unlock();
                return false;
            }
            start_output = true;
        }
    
        //
        //  Choose the QIODevice (derived) to serve as input
        // 
    

        if(url.protocol() == "file" || url.protocol() == "cd")
        {
            QString filename = url.path();
            if(url.hasRef())
            {
                filename += "#";
                filename += url.ref();
            }
            input = new QFile(filename);
        }
        else if(url.protocol() == "daap")
        {
            input = new DaapInput(this, url);
        }
        else
        {
            warning(QString("have no idea how to play this protocol: %1")
                    .arg(url.toString()));
            delete output;
            output = NULL;
            input = NULL;
            state_of_play_mutex->unlock();
            return false;
        }

        if(decoder && !decoder->factory()->supports(url.path()))
        {
            decoder = 0;
        }
        
        if(!decoder)
        {
            QString filename = url.path();
            if(url.hasRef())
            {
                filename += "#";
                filename += url.ref();
            }
            
            decoder = Decoder::create(filename, input, output);

            if(!decoder)
            {
                warning(QString("passed unsupported url in unsupported format: %1")
                        .arg(filename));
                delete output;
                output = NULL;
                delete input;
                input = NULL;
                state_of_play_mutex->unlock();
                return false;
            }
            decoder->setBlockSize(globalBlockSize);
            decoder->setParent(this);
        }
        else
        {
            decoder->setInput(input);
            decoder->setOutput(output);
        }
        
        play_data_mutex.lock();
            elapsed_time = 0;
        play_data_mutex.unlock();
        
        if(decoder->initialize())
        {
            if(output)
            {
                if(start_output)
                {
                    output->start();
                }
                else
                {
                    output->resetTime();
                }
            }
            decoder->start();
            is_playing = true;
            is_paused = false;
            state_of_play_mutex->unlock();
            return true;
        }
        else
        {
            warning(QString("decoder barfed on "
                    "initialization of %1")
                    .arg(url.toString()));
            delete output;
            output = NULL;
            delete input;
            input = NULL;
            state_of_play_mutex->unlock();
            return false;
        }
        
    state_of_play_mutex->unlock();
    return false;
}

bool AudioPlugin::playMetadata(int collection_id, int metadata_id)
{
    //
    //  See if this id's lead to a real metadata object before we mess with
    //  anything else
    //
    
    metadata_server->lockMetadata();
    Metadata *metadata_to_play = 
            metadata_server->getMetadataByContainerAndId(
                                                            collection_id,
                                                            metadata_id
                                                        );
    
    if(!metadata_to_play)
    {
        warning(QString("was asked to play container %1 "
                        "item %2, but it does not exist")
                        .arg(collection_id)
                        .arg(metadata_id));
        metadata_server->unlockMetadata();
        return false;
    }

    //
    //  Make sure we can cast this to Audio metadata
    //
    
    if(metadata_to_play->getType() == MDT_audio)
    {
        AudioMetadata *audio_metadata_to_play = (AudioMetadata *)metadata_to_play;

        //
        //  Note that some metadata values should change
        //
        
        audio_metadata_to_play->setLastPlayed(QDateTime::currentDateTime());
        audio_metadata_to_play->setPlayCount(audio_metadata_to_play->getPlayCount() + 1);
        audio_metadata_to_play->setChanged(true);

        QUrl url_to_play = audio_metadata_to_play->getUrl();
        metadata_server->unlockMetadata();
        bool result = playUrl(url_to_play);
        if(result)
        {
            play_data_mutex.lock();
                current_collection = collection_id;
                current_metadata = metadata_id;
            play_data_mutex.unlock();
        }
        return result;
    }
    else
    {
        metadata_server->unlockMetadata();
        warning(QString("was asked to play container %1 "
                        "item %2, which is not audio content")
                        .arg(collection_id)
                        .arg(metadata_id));
        return false;
    }
}

void AudioPlugin::stopAudio()
{
    state_of_play_mutex->lock();

        if (decoder && decoder->running())
        {
            decoder->mutex()->lock();
                decoder->stop();
            decoder->mutex()->unlock();
        }
    
        if (output && output->running())
        {
            output->mutex()->lock();
                output->stop();
            output->mutex()->unlock();
        }
    
        //
        //  wake them up 
        //
    
        if (decoder)
        {
            decoder->mutex()->lock();
                decoder->cond()->wakeAll();
            decoder->mutex()->unlock();
        }

        if (output)
        {
            output->recycler()->mutex()->lock();
                output->recycler()->cond()->wakeAll();
            output->recycler()->mutex()->unlock();
        }

        if (decoder)
        {
            decoder->wait();
        }

        if (output)
        {
            output->wait();
        }

        if (output)
        {
            delete output;
            output = 0;
        }
    
        if(input)
        {
            delete input;
            input = NULL;
        }
        
        is_playing = false;
        is_paused = false;

    state_of_play_mutex->unlock();

    sendMessage("stop");
    
}

void AudioPlugin::pauseAudio(bool true_or_false)
{
    state_of_play_mutex->lock();
    
        if(!is_playing)
        {
            //
            //  you can't be paused if you're not playing!
            //
            
            true_or_false = false;
            
        }
    
        if(true_or_false == is_paused)
        {
            //
            //  already in the right state of pause
            //
        }
        else
        {
            is_paused = true_or_false;
            if (output)
            {
                output->mutex()->lock();
                    output->pause();
                output->mutex()->unlock();
            }
            if (decoder)
            {
                decoder->mutex()->lock();
                    decoder->cond()->wakeAll();
                decoder->mutex()->unlock();
            }
            if (output)
            {
                output->recycler()->mutex()->lock();
                    output->recycler()->cond()->wakeAll();
                output->recycler()->mutex()->unlock();
            }
        }
       
        //
        //   Tell every connected client the state of the pause
        //
        if(is_paused)
        {
            sendMessage("pause on");
        }
        else
        {
            sendMessage("pause off");
        }
    state_of_play_mutex->unlock();
}

void AudioPlugin::seekAudio(int seek_amount)
{
    state_of_play_mutex->lock();

        int position = elapsed_time + seek_amount;
        if(position < 0)
        {
            position = 0;
        }

        if (output && output->running())
        {
            output->mutex()->lock();
            output->seek(position);
    
            if (decoder && decoder->running())
            {
                decoder->mutex()->lock();
                decoder->seek(position);
    
                /*
                if (mainvisual)
                {
                    mainvisual->mutex()->lock();
                    mainvisual->prepare();
                    mainvisual->mutex()->unlock();
                }
                */
    
                decoder->mutex()->unlock();
            }
            output->mutex()->unlock();
        }
    state_of_play_mutex->unlock();
}

void AudioPlugin::handleInternalMessage(QString the_message)
{
    //
    //  This is where we handle internal messages (coming from the decoder)
    //
    
    if(the_message == "decoder error")
    {
        //  Don't do anything about this at the moment
    }
    if(the_message == "decoder stop")
    {
        //
        //  Well, ok ... stop means we really stopped (not that we just
        //  reached the end of a file), nothing to do
        //

        stopAudio();
    }
    if(the_message == "decoder finish")
    {
        //
        //  If we're in playlist mode, find the next thing to play.
        //  Otherwise, destruct everything.
        //

        if(playlist_mode)
        {
            playFromPlaylist(1);
        }
        else
        {
            stopAudio();
        }
    }
}

void AudioPlugin::setPlaylistMode(int container, int id, int index)
{

    playlist_mode_mutex.lock();
        playlist_mode = true;
        current_playlist_container = container;
        current_playlist_id = id;
        current_playlist_item_index = index;
        log(QString("entering playlist mode "
                    "(container=%1, playlist=%2)")
                    .arg(container)
                    .arg(id), 7);
    playlist_mode_mutex.unlock();
}

void AudioPlugin::stopPlaylistMode()
{
    playlist_mode_mutex.lock();
        playlist_mode = false;
        current_playlist_container = -1;
        current_playlist_id = -1;
        current_playlist_item_index = -1;
        log(QString("leaving playlist mode"), 7);
    playlist_mode_mutex.unlock();
}

void AudioPlugin::playFromPlaylist(int augment_index)
{
    uint playlist_size = 0;
    if(!playlist_mode)
    {
        warning("platFromPlaylist() called, but we're not in playlist mode");
        return;
    }
    
    QUrl url_to_play_next;

    playlist_mode_mutex.lock();

    if(augment_index)
    {
        current_playlist_item_index += augment_index;
    }

    //
    //  find the playlist in question
    //

    metadata_server->lockMetadata();

    Playlist *playlist_to_play = 
             metadata_server->getPlaylistByContainerAndId(
                 
                               current_playlist_container,
                               current_playlist_id
                                                         );        
                                                          
    if(!playlist_to_play)
    {
        warning(QString("asked to play playlist %1 in "
                        "container %2, but that "
                        "doesn't exist")
                        .arg(current_playlist_id)
                        .arg(current_playlist_container));
        metadata_server->unlockMetadata();
        playlist_mode_mutex.unlock();
        stopPlaylistMode();
        stopAudio();
        return;
    }
    
    playlist_size = playlist_to_play->getCount();
        
    //
    //  Get the list of references.
    //
        
    QValueList<int> song_list = playlist_to_play->getList();

    //
    //  At some point, do something about shuffle modes here
    //

    if(song_list.count() == 0)
    {
        //
        //  No point in trying to cycle through a playlist of 0 length
        //
        
        log("stopping playlist mode, playlist has no songs", 8);
        metadata_server->unlockMetadata();
        playlist_mode_mutex.unlock();
        stopPlaylistMode();
        stopAudio();
        return;
    }
   

    if(current_playlist_item_index >= (int) song_list.count())
    {
        current_playlist_item_index = 0;
    }
        
    if(current_playlist_item_index < 0)
    {
        current_playlist_item_index = (int) song_list.count() - 1;
    }
        
    //
    //  Get the relevant reference
    //
        
    uint song_to_play = song_list[current_playlist_item_index];
    uint collection_to_play_from = current_playlist_container;

    metadata_server->unlockMetadata();        
    playlist_mode_mutex.unlock();

    if(!playMetadata(collection_to_play_from, song_to_play))
    {
        log(QString("in playlist mode, could not find item number "
                    "%1 in playlist %2 (of size %3)")
                    .arg(current_playlist_item_index)
                    .arg(current_playlist_container)
                    .arg(playlist_size), 8);
        stopPlaylistMode();
        stopAudio();
    }
}

void AudioPlugin::nextPrevAudio(bool forward)
{
    if(forward)
    {
        playFromPlaylist(1);
    }
    else
    {
        playFromPlaylist(-1);
    }
}

AudioPlugin::~AudioPlugin()
{
    if(output)
    {
        delete output;
        output = NULL;        
    }
    
    if(input)
    {
        delete input;
        input = NULL;
    }
    
    if(state_of_play_mutex)
    {
        delete state_of_play_mutex;
        state_of_play_mutex = NULL;
    }
}
