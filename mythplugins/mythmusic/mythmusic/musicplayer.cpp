// ANSI C includes
#include <cstdlib>

// C++ includes
#include <iostream>
#include <Q3ValueList>

using namespace std;

// qt
#include <qapplication.h>
#include <q3url.h>
#include <qwidget.h>

// mythtv
#include <mythtv/mythcontext.h>
#include <mythtv/audiooutput.h>
#include <mythtv/mythdbcon.h>

// mythmusic
#include "musicplayer.h"
#include "decoder.h"
#include "cddecoder.h"
#include "constants.h"
#include "mainvisual.h"
#include "miniplayer.h"
#include "playlist.h"

// how long to wait before updating the lastplay and playcount fields
#define LASTPLAY_DELAY 15

MusicPlayer  *gPlayer = NULL;

////////////////////////////////////////////////////////////////

MusicPlayer::MusicPlayer(QObject *parent, const QString &dev)
    :QObject(parent)
{
    m_CDdevice = dev;
    m_decoder = NULL;
    m_input = NULL;
    m_output = NULL;

    m_playlistTree = NULL;
    m_currentNode = NULL;
    m_currentMetadata = NULL;

    m_listener = NULL;
    m_visual = NULL;

    m_isAutoplay = false;
    m_isPlaying = false;
    m_canShowPlayer = true;
    m_wasPlaying = true;
    m_updatedLastplay = false;

    m_playSpeed = 1.0;

    QString playmode = gContext->GetSetting("PlayMode", "none");
    if (playmode.lower() == "random")
        setShuffleMode(SHUFFLE_RANDOM);
    else if (playmode.lower() == "intelligent")
        setShuffleMode(SHUFFLE_INTELLIGENT);
    else if (playmode.lower() == "album")
        setShuffleMode(SHUFFLE_ALBUM);
    else if (playmode.lower() == "artist")
        setShuffleMode(SHUFFLE_ARTIST);
    else
        setShuffleMode(SHUFFLE_OFF);

    QString repeatmode = gContext->GetSetting("RepeatMode", "all");
    if (repeatmode.lower() == "track")
        setRepeatMode(REPEAT_TRACK);
    else if (repeatmode.lower() == "all")
        setRepeatMode(REPEAT_ALL);
    else
        setRepeatMode(REPEAT_OFF);

    QString resumestring = gContext->GetSetting("ResumeMode", "off");
    if (resumestring.lower() == "off")
        m_resumeMode = RESUME_OFF;
    else if (resumestring.lower() == "track")
        m_resumeMode = RESUME_TRACK;
    else
        m_resumeMode = RESUME_EXACT;

    m_lastplayDelay = gContext->GetNumSetting("MusicLastPlayDelay", LASTPLAY_DELAY);

    m_autoShowPlayer = (gContext->GetNumSetting("MusicAutoShowPlayer", 1) > 0);

    gContext->addListener(this);
}

MusicPlayer::~MusicPlayer()
{
    if (!hasClient())
        savePosition();

    gContext->removeListener(this);

    stop(true);

    if (m_playlistTree)
        delete m_playlistTree;

    if (m_currentMetadata)
    {
        delete m_currentMetadata;
        m_currentMetadata = NULL;
    }

    if (m_shuffleMode == SHUFFLE_INTELLIGENT)
        gContext->SaveSetting("PlayMode", "intelligent");
    else if (m_shuffleMode == SHUFFLE_RANDOM)
        gContext->SaveSetting("PlayMode", "random");
    else if (m_shuffleMode == SHUFFLE_ALBUM)
        gContext->SaveSetting("PlayMode", "album");
    else if (m_shuffleMode == SHUFFLE_ARTIST)
        gContext->SaveSetting("PlayMode", "artist");
    else
        gContext->SaveSetting("PlayMode", "none");

    if (m_repeatMode == REPEAT_TRACK)
        gContext->SaveSetting("RepeatMode", "track");
    else if (m_repeatMode == REPEAT_ALL)
        gContext->SaveSetting("RepeatMode", "all");
    else
        gContext->SaveSetting("RepeatMode", "none");

    gContext->SaveSetting("MusicAutoShowPlayer", (m_autoShowPlayer ? "1" : "0"));
}

void MusicPlayer::setListener(QObject *listener)
{
    if (m_listener && m_output)
        m_output->removeListener(m_listener);

    if (m_listener && m_decoder)
        m_decoder->removeListener(m_listener);

    m_listener = listener;

    if (m_listener && m_output)
        m_output->addListener(m_listener);

    if (m_listener && m_decoder)
        m_decoder->addListener(m_listener);

    (listener == NULL) ? m_isAutoplay = true :  m_isAutoplay = false;
}

void MusicPlayer::setVisual(MainVisual *visual)
{
    if (m_visual && m_output)
    {
        m_output->removeListener(m_visual);
        m_output->removeVisual(m_visual);
    }

    m_visual = visual;

    if (m_visual && m_output)
    {
        m_output->addListener(m_visual);
        m_output->addVisual(m_visual);
    }
}

void MusicPlayer::playFile(const Metadata &meta)
{
    playFile(meta.Filename());
    m_currentMetadata = new Metadata(meta);
    m_currentNode = NULL;
}

void MusicPlayer::playFile(const QString &filename)
{
    m_currentFile = filename;
    play();
}

void MusicPlayer::stop(bool stopAll)
{
    stopDecoder();

    if (m_output)
    {
        if (m_output->IsPaused())
        {
            pause();
        }
        m_output->Reset();
    }

    if (m_input)
        delete m_input;
    m_input = NULL;

    m_isPlaying = false;

    if (stopAll && m_decoder)
    {
        m_decoder->removeListener(this);
        if (m_listener)
            m_decoder->removeListener(m_listener);

        delete m_decoder;
        m_decoder = NULL;
        m_listener = NULL;
    }

    if (stopAll && m_output)
    {
        m_output->removeListener(this);
        if (m_listener)
            m_output->removeListener(m_listener);

        if (m_visual)
        {
            m_output->removeListener(m_visual);
            m_output->removeVisual(m_visual);
        }
        delete m_output;
        m_output = NULL;
        m_visual = NULL;
    }
}

void MusicPlayer::pause(void)
{
    if (m_output) 
    {
        m_isPlaying = !m_isPlaying;
        m_output->Pause(!m_isPlaying);
    }
    // wake up threads
    if (m_decoder) 
    {
        m_decoder->lock();
        m_decoder->cond()->wakeAll();
        m_decoder->unlock();
    }
}

void MusicPlayer::play(void)
{
    stopDecoder();

    if (!m_output)
        openOutputDevice();

    if (m_input)
        delete m_input;

    m_input = new QFile(m_currentFile);

    if (m_decoder && !m_decoder->factory()->supports(m_currentFile))
    {
        m_decoder->removeListener(this);

        if (m_listener)
            m_decoder->removeListener(m_listener);

        delete m_decoder;
        m_decoder = NULL;
    }

    if (!m_decoder)
    {
        m_decoder = Decoder::create(m_currentFile, m_input, m_output, true);
        if (!m_decoder)
        {
            VERBOSE(VB_IMPORTANT, "MusicPlayer: Failed to create decoder for playback");
            return;
        }

        if (m_currentFile.contains("cda") == 1)
            dynamic_cast<CdDecoder*>(m_decoder)->setDevice(m_CDdevice);

        m_decoder->setBlockSize(2 * 1024);

        m_decoder->addListener(this);

        if (m_listener)
            m_decoder->addListener(m_listener);
    }
    else
    {
        m_decoder->setInput(m_input);
        m_decoder->setFilename(m_currentFile);
        m_decoder->setOutput(m_output);
    }

    if (m_decoder->initialize())
    {
        if (m_output)
            m_output->Reset();

        m_decoder->start();

        m_isPlaying = true;

        if (m_currentNode)
        {
            if (m_currentNode->getInt() > 0)
            {
                m_currentMetadata = Metadata::getMetadataFromID(m_currentNode->getInt());
                m_updatedLastplay = false;
            }
            else
            {
                // CD track
                CdDecoder *cddecoder = dynamic_cast<CdDecoder*>(m_decoder);
                if (m_decoder)
                    m_currentMetadata = cddecoder->getMetadata(-m_currentNode->getInt());
            }
        }
    }
}

void MusicPlayer::stopDecoder(void)
{
    if (m_decoder && m_decoder->running())
    {
        m_decoder->lock();
        m_decoder->stop();
        m_decoder->unlock();
    }

    if (m_decoder) 
    {
        m_decoder->lock();
        m_decoder->cond()->wakeAll();
        m_decoder->unlock();
    }

    if (m_decoder)
        m_decoder->wait();

    if (m_currentMetadata)
    {
        if (m_currentMetadata->hasChanged())
            m_currentMetadata->persist();
        delete m_currentMetadata;
    }
    m_currentMetadata = NULL;
}

void MusicPlayer::openOutputDevice(void)
{
    QString adevice;

    if (gContext->GetSetting("MusicAudioDevice") == "default")
        adevice = gContext->GetSetting("AudioOutputDevice");
    else
        adevice = gContext->GetSetting("MusicAudioDevice");

    // TODO: Error checking that device is opened correctly!
    m_output = AudioOutput::OpenAudio(adevice, "default", 16, 2, 44100,
                                    AUDIOOUTPUT_MUSIC, true, false);
    m_output->setBufferSize(256 * 1024);
    m_output->SetBlocking(false);

    m_output->addListener(this);

    if (m_listener)
        m_output->addListener(m_listener);

    if (m_visual)
    {
        m_output->addListener((QObject*) m_visual);
        m_output->addVisual(m_visual);
    }
}

void MusicPlayer::next(void)
{
    if (!m_currentNode)
        return;

    GenericTree *node = m_currentNode->nextSibling(1, ((int) m_shuffleMode) + 1);
    if (node)
    {
        m_currentNode = node;
    }
    else
    {
        if (m_repeatMode == REPEAT_ALL)
        {
            // start playing again from first track
            GenericTree *parent = m_currentNode->getParent();
            if (parent)
            {
                node = parent->getChildAt(0, ((int) m_shuffleMode) + 1);
                if (node)
                    m_currentNode = node;
                else
                    return; // stop()
            }
        }
        else
            return; // stop()
    }

    QString filename = getFilenameFromID(node->getInt());
    if (!filename.isEmpty())
        playFile(filename);
    else
        stop();
}

void MusicPlayer::previous(void)
{
    if (!m_currentNode)
        return;

    GenericTree *node = m_currentNode->prevSibling(1, ((int) m_shuffleMode) + 1);
    if (node)
    {
        m_currentNode = node;
        QString filename = getFilenameFromID(node->getInt());
        if (!filename.isEmpty())
            playFile(filename);
        else
            return;//stop();
    }
    else
    {
        // FIXME take repeat mode into account
        return; //stop();
    }
}

void MusicPlayer::nextAuto(void)
{
    if (!m_isAutoplay)
        return;

    if (!m_currentNode)
        return;

    if (m_repeatMode == REPEAT_TRACK)
    {
        play();
        return;
    }
    else
        next();

    if (m_canShowPlayer && m_autoShowPlayer)
    {
        MiniPlayer *popup = new MiniPlayer(gContext->GetMainWindow(), this);
        popup->showPlayer(10);
        popup->deleteLater();
        popup = NULL;
    }
}

void MusicPlayer::customEvent(QEvent *event)
{
    if (m_isAutoplay)
    {
        switch ((int)event->type())
        {
            case OutputEvent::Error:
            {
                OutputEvent *aoe = (OutputEvent *) event;

                VERBOSE(VB_IMPORTANT, QString("Output Error - %1")
                        .arg(*aoe->errorMessage()));
                MythPopupBox::showOkPopup(gContext->GetMainWindow(),
                        "Output Error:",
                        QString("MythMusic has encountered the following error:\n%1")
                                .arg(*aoe->errorMessage()));
                stop(true);

                break;
            }

            case DecoderEvent::Finished:
            {
                nextAuto();
                break;
            }

            case DecoderEvent::Error:
            {
                stop(true);

                QApplication::sendPostedEvents();

                DecoderEvent *dxe = (DecoderEvent *) event;

                VERBOSE(VB_IMPORTANT, QString("Decoder Error - %1")
                        .arg(*dxe->errorMessage()));
                MythPopupBox::showOkPopup(gContext->GetMainWindow(), 
                                        "Decoder Error",
                                        QString("MythMusic has encountered the following error:\n%1")
                                        .arg(*dxe->errorMessage()));
                break;
            }

            case MythEvent::MythEventMessage:
            {
                MythEvent *me = (MythEvent *) event;
                if (me->Message().left(14) == "PLAYBACK_START")
                {
                    m_wasPlaying = m_isPlaying;
                    QString hostname = me->Message().mid(15);

                    if (hostname == gContext->GetHostName())
                    {
                        if (m_isPlaying)
                            savePosition();
                        stop(true);
                    }
                }

                if (me->Message().left(12) == "PLAYBACK_END")
                {
                    if (m_wasPlaying)
                    {
                        QString hostname = me->Message().mid(13);
                        if (hostname == gContext->GetHostName())
                        {
                            play();
                            seek(gContext->GetNumSetting("MusicBookmarkPosition", 0));
                            gContext->SaveSetting("MusicBookmark", "");
                            gContext->SaveSetting("MusicBookmarkPosition", 0);
                        }

                        m_wasPlaying = false;
                    }
                }

                break;
            }
        }
    }


    if ((int)event->type() == OutputEvent::Info)
    {
        OutputEvent *oe = (OutputEvent *) event;
        m_currentTime = oe->elapsedSeconds();

        if (!m_updatedLastplay)
        {
            // we update the lastplay and playcount after playing for m_lastplayDelay seconds
            // or half the total track time
            if ((m_currentMetadata &&  m_currentTime > (m_currentMetadata->Length() / 1000) / 2) ||
                 m_currentTime >= m_lastplayDelay)
            updateLastplay();
        }
    }

    QObject::customEvent(event);
}

QString MusicPlayer::getFilenameFromID(int id)
{
    QString filename = "";

    if (id > 0)
    {
        QString aquery = "SELECT CONCAT_WS('/', "
                        "music_directories.path, music_songs.filename) AS filename "
                        "FROM music_songs "
                        "LEFT JOIN music_directories ON music_songs.directory_id=music_directories.directory_id "
                        "WHERE music_songs.song_id = :ID";

        MSqlQuery query(MSqlQuery::InitCon());
        query.prepare(aquery);
        query.bindValue(":ID", id);
        if (!query.exec() || query.numRowsAffected() < 1)
            MythContext::DBError("get filename", query);

        if (query.isActive() && query.size() > 0)
        {
            query.first();
            filename = query.value(0).toString();
            if (!filename.contains("://"))
                filename = Metadata::GetStartdir() + filename;
        }
    }
    else
    {
        // cd track
        CdDecoder *cddecoder = dynamic_cast<CdDecoder*>(m_decoder);
        if (cddecoder)
        {
            Metadata *meta = cddecoder->getMetadata(-id);
            if (meta)
                filename = meta->Filename();
        }
    }
    return filename;
}

GenericTree *MusicPlayer::constructPlaylist(void)
{
    QString position = "";

    if (m_playlistTree)
    {
        position = getRouteToCurrent();
        delete m_playlistTree;
    }

    m_playlistTree = new GenericTree(tr("playlist root"), 0);
    m_playlistTree->setAttribute(0, 0);
    m_playlistTree->setAttribute(1, 0);
    m_playlistTree->setAttribute(2, 0);
    m_playlistTree->setAttribute(3, 0);
    m_playlistTree->setAttribute(4, 0);

    GenericTree *active_playlist_node =
            gMusicData->all_playlists->writeTree(m_playlistTree);

    if (position != "" ) //|| m_currentNode == NULL)
        restorePosition(position);

    return active_playlist_node;
}

QString MusicPlayer::getRouteToCurrent(void)
{
    QStringList route;

    if (m_currentNode)
    {
        GenericTree *climber = m_currentNode;

        route.push_front(QString::number(climber->getInt()));
        while((climber = climber->getParent()))
        {
            route.push_front(QString::number(climber->getInt()));
        }
    }
    return route.join(",");
}

void MusicPlayer::savePosition(void)
{
    if (m_resumeMode != RESUME_OFF)
    {
        gContext->SaveSetting("MusicBookmark", getRouteToCurrent());
        if (m_resumeMode == RESUME_EXACT)
            gContext->SaveSetting("MusicBookmarkPosition", m_currentTime);
    }
}

void MusicPlayer::restorePosition(const QString &position)
{
    Q3ValueList <int> branches_to_current_node;

    if (position != "")
    {
        QStringList list = QStringList::split(",", position);

        for (QStringList::Iterator it = list.begin(); it != list.end(); ++it)
            branches_to_current_node.append((*it).toInt());

        //try to restore the position
        m_currentNode = m_playlistTree->findNode(branches_to_current_node);

        if (m_currentNode)
            return;
    }

    // failed to find the position so go to the first track in the playlist
    branches_to_current_node.clear();
    branches_to_current_node.append(0);
    branches_to_current_node.append(1);
    branches_to_current_node.append(0);
    m_currentNode = m_playlistTree->findNode(branches_to_current_node);
    if (m_currentNode)
    {
        m_currentNode = m_currentNode->getChildAt(0, -1);
        if (m_currentNode)
        {
            m_currentFile = getFilenameFromID(m_currentNode->getInt());
            if (m_currentFile != "")
                play();
        }
    }
}

void MusicPlayer::seek(int pos)
{
    if (m_output)
    {
        m_output->Reset();
        m_output->SetTimecode(pos*1000);

        if (m_decoder && m_decoder->running())
        {
            m_decoder->lock();
            m_decoder->seek(pos);
            m_decoder->unlock();
        }
    }
}

void MusicPlayer::showMiniPlayer(void)
{
    if (m_canShowPlayer)
    {
        MiniPlayer *popup = new MiniPlayer(gContext->GetMainWindow(), this);
        popup->exec();
        popup->deleteLater();
        popup = NULL;
    }
}

Metadata *MusicPlayer::getCurrentMetadata(void)
{
    if (m_currentMetadata)
        return m_currentMetadata;

    if (!m_currentNode)
        return NULL;

    m_currentMetadata = Metadata::getMetadataFromID(m_currentNode->getInt());

    return m_currentMetadata;
}

void MusicPlayer::refreshMetadata(void)
{
    if (m_currentMetadata)
    {
        delete m_currentMetadata;
        m_currentMetadata = NULL;
    }

    getCurrentMetadata();
}

MusicPlayer::RepeatMode MusicPlayer::toggleRepeatMode(void)
{
    switch (m_repeatMode)
    {
        case REPEAT_OFF:
            m_repeatMode = REPEAT_TRACK;
            break;
        case REPEAT_TRACK:
            m_repeatMode = REPEAT_ALL;
            break;
        case REPEAT_ALL:
            m_repeatMode = REPEAT_OFF;
           break;
        default:
            m_repeatMode = REPEAT_OFF;
            break;
    }

    return m_repeatMode;
}

MusicPlayer::ShuffleMode MusicPlayer::toggleShuffleMode(void)
{
    switch (m_shuffleMode)
    {
        case SHUFFLE_OFF:
            m_shuffleMode = SHUFFLE_RANDOM;
            break;
        case SHUFFLE_RANDOM:
            m_shuffleMode = SHUFFLE_INTELLIGENT;
            break;
        case SHUFFLE_INTELLIGENT:
            m_shuffleMode = SHUFFLE_ALBUM;
           break;
        case SHUFFLE_ALBUM:
            m_shuffleMode = SHUFFLE_ARTIST;
           break;
        case SHUFFLE_ARTIST:
            m_shuffleMode = SHUFFLE_OFF;
           break;
        default:
            m_shuffleMode = SHUFFLE_OFF;
            break;
    }

    return m_shuffleMode;
}

void MusicPlayer::updateLastplay()
{
    // FIXME this is ugly having to keep two metadata objects in sync
    if (m_currentNode && m_currentNode->getInt() > 0)
    {
        if (m_currentMetadata)
        {
            m_currentMetadata->incPlayCount();
            m_currentMetadata->setLastPlay();
        }
        // if all_music is still in scope we need to keep that in sync
        if (gMusicData->all_music)
        {
            Metadata *mdata = gMusicData->all_music->getMetadata(m_currentNode->getInt());
            if (mdata)
            {
                mdata->incPlayCount();
                mdata->setLastPlay();
            }
        }
    }

    m_updatedLastplay = true;
}

void MusicPlayer::setSpeed(float newspeed)
{
    if (m_output)
    {
        m_playSpeed = newspeed;
        m_output->SetStretchFactor(m_playSpeed);
    }
}

void MusicPlayer::incSpeed()
{
    m_playSpeed += 0.05;
    setSpeed(m_playSpeed);
}

void MusicPlayer::decSpeed()
{
    m_playSpeed -= 0.05;
    setSpeed(m_playSpeed);
}
