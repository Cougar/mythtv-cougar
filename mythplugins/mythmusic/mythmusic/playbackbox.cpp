// ANSI C includes
#include <cstdlib>

// C++ includes
#include <iostream>
using namespace std;

// Qt includes
#include <qapplication.h>
#include <qregexp.h>

// MythTV plugin includes
#include <mythtv/mythcontext.h>
#include <mythtv/mythwidgets.h>
#include <mythtv/lcddevice.h>
#include <mythtv/mythmedia.h>
#include <mythtv/audiooutput.h>

// MythMusic includes
#include "metadata.h"
#include "constants.h"
#include "streaminput.h"
#include "decoder.h"
#include "cddecoder.h"
#include "playbackbox.h"
#include "databasebox.h"
#include "mainvisual.h"
#include "smartplaylist.h"
#include "search.h"

PlaybackBoxMusic::PlaybackBoxMusic(MythMainWindow *parent, QString window_name,
                                   QString theme_filename, 
                                   PlaylistsContainer *the_playlists,
                                   AllMusic *the_music,
                                   const QString &dev, const char *name)

                : MythThemedDialog(parent, window_name, theme_filename, name)
{
    //  A few internal variable defaults
 
    input = NULL;
    output = NULL;
    decoder = NULL;
    mainvisual = NULL;
    visual_mode_timer = NULL;
    lcd_update_timer = NULL;
    waiting_for_playlists_timer = NULL;
    speed_scroll_timer = NULL;
    playlist_tree = NULL;
    playlist_popup = NULL;
    progress = NULL;

    isplaying = false;
    tree_is_done = false;
    first_playlist_check = true;
    outputBufferSize = 256;
    currentTime = 0;
    maxTime = 0;
    scrollCount = 0;
    scrollingDown = false;
    setContext(0);
    visual_mode_timer = new QTimer(this);
    speed_scroll_timer = new QTimer(this);
    connect(speed_scroll_timer, SIGNAL(timeout()), this, SLOT(resetScrollCount()));

    visualizer_status = 0;
    curMeta = NULL;
    curSmartPlaylistCategory = "";
    curSmartPlaylistName = "";

    menufilters = gContext->GetNumSetting("MusicMenuFilters", 0);

    cd_reader_thread = NULL;
    cd_watcher = NULL;
    scan_for_cd = gContext->GetNumSetting("AutoPlayCD", 0);
    m_CDdevice = dev;

    // Set our pointers the playlists and the metadata containers

    all_playlists = the_playlists;
    all_music = the_music;

    // Get some user set options

    show_whole_tree = gContext->GetNumSetting("ShowWholeTree", 1);
    keyboard_accelerators = gContext->GetNumSetting("KeyboardAccelerators", 1);
    if (!keyboard_accelerators)
        show_whole_tree = false;

    showrating = gContext->GetNumSetting("MusicShowRatings", 0);
    listAsShuffled = gContext->GetNumSetting("ListAsShuffled", 0);
    cycle_visualizer = gContext->GetNumSetting("VisualCycleOnSongChange", 0);
    show_album_art = gContext->GetNumSetting("VisualAlbumArtOnSongChange", 0);
    random_visualizer = gContext->GetNumSetting("VisualRandomize", 0);

    m_pushedButton = NULL;

    // Through the magic of themes, our "GUI" already exists we just need to 
    // wire up it

    wireUpTheme();

    // Possibly (user-defined) control the volume

    volume_control = false;
    volume_display_timer = new QTimer(this);
    if (gContext->GetNumSetting("MythControlsVolume", 0))
    {
        volume_control = true;
        volume_display_timer->start(2000);
        connect(volume_display_timer, SIGNAL(timeout()),
                this, SLOT(hideVolume()));
    }

    // Figure out the shuffle mode

    QString playmode = gContext->GetSetting("PlayMode", "none");
    if (playmode.lower() == "random")
        setShuffleMode(SHUFFLE_RANDOM);
    else if (playmode.lower() == "intelligent")
        setShuffleMode(SHUFFLE_INTELLIGENT);
    else if (playmode.lower() == "album")
        setShuffleMode(SHUFFLE_ALBUM);
    else
        setShuffleMode(SHUFFLE_OFF);

    QString resumestring = gContext->GetSetting("ResumeMode", "off");
    if (resumestring.lower() == "off")
        resumemode = RESUME_OFF;
    else if (resumestring.lower() == "track")
        resumemode = RESUME_TRACK;
    else
        resumemode = RESUME_EXACT;

    QString repeatmode = gContext->GetSetting("RepeatMode", "all");
    if (repeatmode.lower() == "track")
        setRepeatMode(REPEAT_TRACK);
    else if (repeatmode.lower() == "all")
        setRepeatMode(REPEAT_ALL);
    else
        setRepeatMode(REPEAT_OFF);

    // Set some button values

    if (!keyboard_accelerators) 
    {
        if (pledit_button)
            pledit_button->setText(tr("Edit Playlist"));
        if (vis_button)
            vis_button->setText(tr("Visualize"));
        if (!assignFirstFocus())
        {
            VERBOSE(VB_IMPORTANT, "playbackbox.o: Could not find a button to "
                                  "assign focus to. What's in your theme?");
            exit(0);
        }
    }
    else 
    {
        if (pledit_button)
            pledit_button->setText(tr("3 Edit Playlist"));
        if (vis_button)
            vis_button->setText(tr("4 Visualize"));
    }

    if (class LCD *lcd = LCD::Get())
    {
        // Set please wait on the LCD
        QPtrList<LCDTextItem> textItems;
        textItems.setAutoDelete(true);
        
        textItems.append(new LCDTextItem(1, ALIGN_CENTERED, "Please Wait", 
                         "Generic"));
        lcd->switchToGeneric(&textItems);
    }

    // We set a timer to load the playlists. We do this for two reasons: 
    // (1) the playlists may not be fully loaded, and (2) even if they are 
    // fully loaded, they do take a while to write out a GenericTree for 
    // navigation use, and that slows down the appearance of this dialog
    // if we were to do it right here.

    waiting_for_playlists_timer = new QTimer(this);
    connect(waiting_for_playlists_timer, SIGNAL(timeout()), this, 
            SLOT(checkForPlaylists()));
    waiting_for_playlists_timer->start(50, TRUE);

    // Warm up the visualizer

    mainvisual = new MainVisual(this);
    if (visual_blackhole)
        mainvisual->setGeometry(visual_blackhole->getScreenArea());
    else
        mainvisual->setGeometry(screenwidth + 10, screenheight + 10, 160, 160);
    mainvisual->show();

    fullscreen_blank = false; 

    visual_modes = QStringList::split(';', gContext->GetSetting("VisualMode"));
    if (!visual_modes.count())
        visual_modes.push_front("Blank");

    current_visual = random_visualizer ? rand() % visual_modes.count() : 0;

    QString visual_delay = gContext->GetSetting("VisualModeDelay");
    bool delayOK;
    visual_mode_delay = visual_delay.toInt(&delayOK);
    if (!delayOK)
        visual_mode_delay = 0;
    if (visual_mode_delay > 0)
    {
        visual_mode_timer->start(visual_mode_delay * 1000);
        connect(visual_mode_timer, SIGNAL(timeout()), this, SLOT(visEnable()));
    }
    visualizer_status = 1;

    // Temporary workaround for visualizer Bad X Requests
    //
    // start on Blank, and then set the "real" mode after
    // the playlist timer fires. Seems to work.
    //
    // Suspicion: in most modes, SDL is not happy if the
    // window doesn't fully exist yet  (????)

    mainvisual->setVisual("Blank");

    // Ready to go. Let's update the foreground just to be safe.

    updateForeground();

    if (class LCD *lcd = LCD::Get())
    {
        lcd->switchToTime();
    }
}

PlaybackBoxMusic::~PlaybackBoxMusic(void)
{
    savePosition(currentTime);

    stopAll();

    if (output)
    {
        delete output;
        output = NULL;
    }

    if (progress)
    {
        progress->Close();
        delete progress;
        progress = NULL;
    }

    if (cd_reader_thread)
    {
        cd_watcher->stop();
        cd_reader_thread->wait();
        delete cd_reader_thread;
    }

    if (playlist_tree)
        delete playlist_tree;

    if (shufflemode == SHUFFLE_INTELLIGENT)
        gContext->SaveSetting("PlayMode", "intelligent");
    else if (shufflemode == SHUFFLE_RANDOM)
        gContext->SaveSetting("PlayMode", "random");
    else if (shufflemode == SHUFFLE_ALBUM)
        gContext->SaveSetting("PlayMode", "album");
    else
        gContext->SaveSetting("PlayMode", "none");

    if (repeatmode == REPEAT_TRACK)
        gContext->SaveSetting("RepeatMode", "track");
    else if (repeatmode == REPEAT_ALL)
        gContext->SaveSetting("RepeatMode", "all");
    else
        gContext->SaveSetting("RepeatMode", "none");
    if (class LCD *lcd = LCD::Get())
        lcd->switchToTime();
}

bool PlaybackBoxMusic::onMediaEvent(MythMediaDevice*)
{
    return scan_for_cd;
}

void PlaybackBoxMusic::resetScrollCount()
{
     scrollCount = 0;
     scrollingDown = false;
}

void PlaybackBoxMusic::keyPressEvent(QKeyEvent *e)
{
    bool handled = false;

    resetTimer();

    QStringList actions;
    gContext->GetMainWindow()->TranslateKeyPress("Music", e, actions);

    int scrollAmt = 1;

    for (unsigned int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        handled = true;

        if (action == "NEXTTRACK")
        {
            if (next_button)
                next_button->push();
            else
                next();
        }
        else if (action == "PREVTRACK")
        {
            if (prev_button)
                prev_button->push();
            else
                previous();
        }
        else if (action == "FFWD")
        { 
            if (ff_button)
                ff_button->push();
            else
                seekforward();
        }
        else if (action == "RWND")
        { 
            if (rew_button)
                rew_button->push();
            else
                seekback();
        }
        else if (action == "PAUSE")
        { 
            if (isplaying)
            {
                if (pause_button)
                    pause_button->push();
                else
                    pause();
            }
            else
            {
                if (play_button)
                    play_button->push();
                else
                    play();
            }
        }
        else if (action == "PLAY")
        { 
            if (play_button)
                play_button->push();
            else
                play();
        }
        else if (action == "STOP")
        {
            if (stop_button)
                stop_button->push();
            else
                stop();

            currentTime = 0;
        }
        else if (action == "THMBUP")
            increaseRating();
        else if (action == "THMBDOWN")
            decreaseRating();
        else if (action == "1")
        {
            if (shuffle_button)
                shuffle_button->push();
            else
                toggleShuffle();
        }
        else if (action == "2")
        {
            if (repeat_button)
                repeat_button->push();
            else
                toggleRepeat();
        }
        else if (action == "3")
        {
            if (pledit_button)
                pledit_button->push();
            else
                editPlaylist();
        }
        else if (action == "CYCLEVIS")
        {
            CycleVisualizer();
            bannerEnable(tr("Visualization: ") + visual_modes[current_visual], 4000);
        }
        else if (action == "BLANKSCR")
            toggleFullBlankVisualizer();
        else if (action == "VOLUMEDOWN") 
            changeVolume(false);
        else if (action == "VOLUMEUP")
            changeVolume(true);
        else if (action == "MUTE")
            toggleMute();
        else if (action == "MENU" && visualizer_status != 2)
        {
            menufilters = false;
            showMenu();
        }
        else if (action == "FILTER" && visualizer_status != 2)
        {
            menufilters = true;
            showMenu();
        }
        else if (action == "INFO")
            if (visualizer_status == 2) 
                bannerToggle(curMeta);
            else
                showEditMetadataDialog();
        else
            handled = false;
    }

    if (handled)
        return;

    if (visualizer_status == 2)
    {
        for (unsigned int i = 0; i < actions.size() && !handled; i++)
        {
            QString action = actions[i];
            if (action == "ESCAPE" || action == "4")
            {
                visualizer_status = 1;

                mainvisual->setVisual("Blank");
                if (visual_blackhole)
                    mainvisual->setGeometry(visual_blackhole->getScreenArea());
                else
                    mainvisual->setGeometry(screenwidth + 10, 
                                            screenheight + 10, 
                                            160, 160);
                setUpdatesEnabled(true);
                mainvisual->setVisual(visual_modes[current_visual]);
                bannerDisable();

                if (!m_parent->IsExitingToMain())
                    handled = true;
            }
            else
            {
                // pass the key press on to the visualiser
                if (mainvisual->visual())
                    mainvisual->visual()->handleKeyPress(action);
            }
        }
    }
    else if (keyboard_accelerators)
    {
        for (unsigned int i = 0; i < actions.size() && !handled; i++)
        {
            QString action = actions[i];
            handled = true;

            if (action == "UP")
            {
                if (scrollingDown)
                    resetScrollCount();

                scrollCount++;
                if (scrollCount > 19 && scrollCount < 30)
                    scrollAmt = 10;
                else if (scrollCount > 29)
                    scrollAmt = 50;

                if (!music_tree_list->moveUpByAmount(scrollAmt, true) && scrollAmt > 1)
                    resetScrollCount();

                speed_scroll_timer->stop();
                speed_scroll_timer->start(500, true);

            }
            else if (action == "DOWN")
            {
                if (!scrollingDown)
                {
                    resetScrollCount();
                    scrollingDown = true;
                }

                scrollCount++;
                if (scrollCount > 19 && scrollCount < 30)
                    scrollAmt = 10;
                else if (scrollCount > 29)
                    scrollAmt = 50;

                if (!music_tree_list->moveDownByAmount(scrollAmt, true) && scrollCount > 1)
                    resetScrollCount();

                speed_scroll_timer->stop();
                speed_scroll_timer->start(500, true);
            }
            else if (action == "LEFT")
                music_tree_list->popUp();
            else if (action == "RIGHT")
                    music_tree_list->pushDown();
            else if (action == "4")
            {
                if (vis_button)
                    vis_button->push();
                else
                    visEnable();
            }
            else if (action == "SELECT")
            {
                music_tree_list->select();
                if (visualizer_status > 0 && cycle_visualizer)
                    CycleVisualizer();
            }
            else if (action == "REFRESH") 
            {
                music_tree_list->syncCurrentWithActive();
                music_tree_list->forceLastBin();
                music_tree_list->refresh();
            }
            else if (action == "PAGEUP")
                music_tree_list->pageUp();
            else if (action == "PAGEDOWN")
                music_tree_list->pageDown();
            else if (action == "INCSEARCH")
                music_tree_list->incSearchStart();
            else if (action == "INCSEARCHNEXT")
                music_tree_list->incSearchNext();
            else
                handled = false;
        }
    }
    else
    {
        for (unsigned int i = 0; i < actions.size() && !handled; i++)
        {
            QString action = actions[i];
            handled = true;

            if (action == "UP" || action == "LEFT")
                nextPrevWidgetFocus(false);
            else if (action == "DOWN" || action == "RIGHT")
                nextPrevWidgetFocus(true);
            else if (action == "SELECT")
            {
                activateCurrent();
                music_tree_list->syncCurrentWithActive();
            }
            else
                handled = false;
        }
    }

    if (!handled)
        MythThemedDialog::keyPressEvent(e);
}

void PlaybackBoxMusic::handlePush(QString buttonname)
{
    if (m_pushedButton)
        m_pushedButton->unPush();

    if (buttonname == "play_button")
    {
        play();
        m_pushedButton = play_button;
    }
    else if (buttonname == "pause_button")
    {
        pause();
        m_pushedButton = pause_button;
    }
    else if (buttonname == "stop_button")
    {
        stop();
        m_pushedButton = stop_button;
    }
}

void PlaybackBoxMusic::showMenu()
{
    if (playlist_popup)
        return;

    playlist_popup = new MythPopupBox(gContext->GetMainWindow(),
                                      "playlist_popup");

    if (menufilters)
    {
        QLabel *caption = playlist_popup->addLabel(tr("Change Filter"), MythPopupBox::Large);
        caption->setAlignment(Qt::AlignCenter);
    }

    QButton *button = playlist_popup->addButton(tr("Smart playlists"), this,
                              SLOT(showSmartPlaylistDialog()));

    QLabel *splitter = playlist_popup->addLabel(" ", MythPopupBox::Small);
    splitter->setLineWidth(2);
    splitter->setFrameShape(QFrame::HLine);
    splitter->setFrameShadow(QFrame::Sunken);
    splitter->setMaximumHeight((int) (5 * hmult));
    splitter->setMaximumHeight((int) (5 * hmult));

    playlist_popup->addButton(tr("Search"), this,
                              SLOT(showSearchDialog()));
    playlist_popup->addButton(tr("From CD"), this,
                              SLOT(fromCD()));
    playlist_popup->addButton(tr("All Tracks"), this,
                              SLOT(allTracks()));
    if (curMeta)
    {
        playlist_popup->addButton(tr("Tracks by current Artist"), this,
                                  SLOT(byArtist()));
        playlist_popup->addButton(tr("Tracks from current Album"), this,
                                  SLOT(byAlbum()));
        playlist_popup->addButton(tr("Tracks from current Genre"), this,
                                  SLOT(byGenre()));
        playlist_popup->addButton(tr("Tracks from current Year"), this,
                                  SLOT(byYear()));
    }
    
    playlist_popup->ShowPopup(this, SLOT(closePlaylistPopup()));

    button->setFocus();
}

void PlaybackBoxMusic::closePlaylistPopup()
{
    if (!playlist_popup)
        return;

    playlist_popup->hide();
    delete playlist_popup;
    playlist_popup = NULL;
}

void PlaybackBoxMusic::allTracks()
{
   if (!playlist_popup)
        return;

   closePlaylistPopup();
   updatePlaylistFromQuickPlaylist("ORDER BY music_artists.artist_name, album_name, track");
}

void PlaybackBoxMusic::fromCD()
{
    if (!playlist_popup)
        return;

    updatePlaylistFromCD();
    closePlaylistPopup();
}

void PlaybackBoxMusic::showSmartPlaylistDialog()
{
   if (!playlist_popup)
        return;

    closePlaylistPopup();
    
    SmartPlaylistDialog dialog(gContext->GetMainWindow(), "smartplaylistdialog");
    dialog.setSmartPlaylist(curSmartPlaylistCategory, curSmartPlaylistName);

    int res = dialog.ExecPopup();
    
    if (res > 0)
    {
        dialog.getSmartPlaylist(curSmartPlaylistCategory, curSmartPlaylistName);
        updatePlaylistFromSmartPlaylist();
    }
}

void PlaybackBoxMusic::showSearchDialog()
{
   if (!playlist_popup)
        return;

    closePlaylistPopup();

    SearchDialog dialog(gContext->GetMainWindow(), "searchdialog");

    int res = dialog.ExecPopupAtXY(-1, 20);

    if (res != -1)
    {
          QString whereClause;
          dialog.getWhereClause(whereClause);
          updatePlaylistFromQuickPlaylist(whereClause);
    }
}

void PlaybackBoxMusic::byArtist()
{
    if (!playlist_popup || !curMeta)
        return;

    QString value = formattedFieldValue(curMeta->Artist().utf8());
    QString whereClause = "WHERE music_artists.artist_name = " + value +
                          " ORDER BY album_name, track"; 

    closePlaylistPopup();
    updatePlaylistFromQuickPlaylist(whereClause);
}

void PlaybackBoxMusic::byAlbum()
{
    if (!playlist_popup || !curMeta)
        return;

    QString value = formattedFieldValue(curMeta->Album().utf8());
    QString whereClause = "WHERE album_name = " + value + 
                          " ORDER BY track";
    closePlaylistPopup();
    updatePlaylistFromQuickPlaylist(whereClause);
}

void PlaybackBoxMusic::byGenre()
{
   if (!playlist_popup || !curMeta)
        return;

    QString value = formattedFieldValue(curMeta->Genre().utf8()); 
    QString whereClause = "WHERE genre = " + value +
                          " ORDER BY music_artists.artist_name, album_name, track";   
    closePlaylistPopup();
    updatePlaylistFromQuickPlaylist(whereClause);
}

void PlaybackBoxMusic::byYear()
{
   if (!playlist_popup || !curMeta)
        return;

    QString value = formattedFieldValue(curMeta->Year()); 
    QString whereClause = "WHERE music_songs.year = " + value + 
                          " ORDER BY music_artists.artist_name, album_name, track";
    closePlaylistPopup();
    updatePlaylistFromQuickPlaylist(whereClause);
}

void PlaybackBoxMusic::updatePlaylistFromQuickPlaylist(QString whereClause)
{
    doUpdatePlaylist(whereClause);
}

void PlaybackBoxMusic::doUpdatePlaylist(QString whereClause)
{
    bool bRemoveDups;
    InsertPLOption insertOption;
    PlayPLOption playOption;
    int curTrackID, trackCount;

    if (!menufilters && !getInsertPLOptions(insertOption, playOption, bRemoveDups))
        return;

    QValueList <int> branches_to_current_node;
    trackCount = music_tree_list->getCurrentNode()->siblingCount();

    // store path to current track
    if (curMeta)
    {
        QValueList <int> *a_route;
        a_route = music_tree_list->getRouteToActive();
        branches_to_current_node = *a_route;
        curTrackID = curMeta->ID();
    }
    else
    {
        // No current metadata, so when we come back we'll try and play the 
        // first thing on the active queue
        branches_to_current_node.clear();
        branches_to_current_node.append(0); //  Root node
        branches_to_current_node.append(1); //  We're on a playlist (not "My Music")
        branches_to_current_node.append(0); //  Active play Queue
        curTrackID = 0;
    }

    visual_mode_timer->stop();

    if (whereClause != "")
    {
        // update playlist from quick playlist
        if (menufilters)
            all_playlists->getActive()->fillSonglistFromQuery(
                        whereClause, false, PL_FILTERONLY, curTrackID);
        else
            all_playlists->getActive()->fillSonglistFromQuery(
                        whereClause, bRemoveDups, insertOption, curTrackID);
    }
    else
    {
        // update playlist from smart playlist
        if (menufilters)
            all_playlists->getActive()->fillSonglistFromSmartPlaylist(
                    curSmartPlaylistCategory, curSmartPlaylistName,
                    false, PL_FILTERONLY, curTrackID);
        else
        {
            all_playlists->getActive()->fillSonglistFromSmartPlaylist(
                    curSmartPlaylistCategory, curSmartPlaylistName,
                    bRemoveDups, insertOption, curTrackID);
        }
    }

    if (visual_mode_delay > 0)
        visual_mode_timer->start(visual_mode_delay * 1000);

    constructPlaylistTree();

    if (!menufilters)
    {
        switch (playOption)
        {
            case PL_CURRENT:
                // try to move to the current track
                if (!music_tree_list->tryToSetActive(branches_to_current_node))
                    playFirstTrack();
                break;

            case PL_FIRST:
                playFirstTrack();
                break;

            case PL_FIRSTNEW:
            {
                switch (insertOption)
                {
                    case PL_REPLACE:
                        playFirstTrack();
                        break;

                    case PL_INSERTATEND:
                    {
                        GenericTree *node = NULL;
                        pause();
                        if (music_tree_list->tryToSetActive(branches_to_current_node))
                        {
                            node = music_tree_list->getCurrentNode()->getParent();
                            if (node)
                            {
                                node = node->getChildAt((uint) trackCount);
                            }
                        }

                        if (node)
                        {
                            music_tree_list->setCurrentNode(node);
                            music_tree_list->select();
                        }
                        else
                            playFirstTrack();

                        break;
                    }

                    case PL_INSERTAFTERCURRENT:
                        pause();
                        if (music_tree_list->tryToSetActive(branches_to_current_node))
                            next();
                        else
                            playFirstTrack();
                        break;

                    default:
                        playFirstTrack();
                }

                break;
            }
        }
    }

    music_tree_list->refresh();
}

void PlaybackBoxMusic::playFirstTrack()
{
    QValueList <int> branches_to_current_node;

    stop();
    wipeTrackInfo();
    branches_to_current_node.clear();
    branches_to_current_node.append(0); //  Root node
    branches_to_current_node.append(1); //  We're on a playlist (not "My Music")
    branches_to_current_node.append(0); //  Active play Queue
    music_tree_list->moveToNodesFirstChild(branches_to_current_node);
}

void PlaybackBoxMusic::updatePlaylistFromCD()
{
    if (!cd_reader_thread)
    {
        cd_reader_thread = new ReadCDThread(m_CDdevice,
                                            all_playlists, all_music);
        cd_reader_thread->start();
    }

    if (!cd_watcher)
    {
        cd_watcher = new QTimer(this);
        connect(cd_watcher, SIGNAL(timeout()), this, SLOT(occasionallyCheckCD()));
        cd_watcher->start(1000);
    }

}

void PlaybackBoxMusic::postUpdate()
{
   QValueList <int> branches_to_current_node;

   if (visual_mode_delay > 0)
       visual_mode_timer->start(visual_mode_delay * 1000);

   constructPlaylistTree();

   stop();
   wipeTrackInfo();

   // move to first track in list
   branches_to_current_node.clear();
   branches_to_current_node.append(0); //  Root node
   branches_to_current_node.append(1); //  We're on a playlist (not "My Music")
   branches_to_current_node.append(0); //  Active play Queue
   music_tree_list->moveToNodesFirstChild(branches_to_current_node);
   music_tree_list->refresh();
}

void PlaybackBoxMusic::occasionallyCheckCD()
{
    if (cd_reader_thread->getLock()->locked())
        return;

    if (!scan_for_cd) {
        cd_watcher->stop();
        delete cd_watcher;
        cd_watcher = NULL;

        cd_reader_thread->wait();
        delete cd_reader_thread;
        cd_reader_thread = NULL;
    }

    if (!scan_for_cd || cd_reader_thread->statusChanged())
    {
        all_playlists->clearCDList();
        all_playlists->getActive()->ripOutAllCDTracksNow();

        if (all_music->getCDTrackCount())
        {
            visual_mode_timer->stop();

            all_playlists->getActive()->removeAllTracks();
            all_playlists->getActive()->fillSongsFromCD();

        }

        postUpdate();
    }

    if (scan_for_cd && !cd_reader_thread->running())
        cd_reader_thread->start();
}

void PlaybackBoxMusic::updatePlaylistFromSmartPlaylist()
{
    doUpdatePlaylist("");
}

void PlaybackBoxMusic::showEditMetadataDialog()
{
    if (!curMeta)
    {
        return;
    }

    // store the current track metadata in case the track changes
    // while we show the edit dialog
    GenericTree *node = music_tree_list->getCurrentNode();
    Metadata *editMeta = all_music->getMetadata( node->getInt() );

    EditMetadataDialog editDialog(editMeta, gContext->GetMainWindow(),
                      "edit_metadata", "music-", "edit metadata");
    if (editDialog.exec())
    {
        // update the metadata copy stored in all_music
        if (all_music->updateMetadata(editMeta->ID(), editMeta))
        {
           // update the displayed track info
           if (node)
           {
               bool errorFlag;
               node->setString(all_music->getLabel(editMeta->ID(), &errorFlag));
               music_tree_list->refresh();

               // make sure the track hasn't changed
               if (curMeta->ID() == editMeta->ID())
               {
                    *curMeta = editMeta;
                    updateTrackInfo(curMeta);
               }
           }
        }

        MythBusyDialog busy(QObject::tr("Rebuilding music tree"));
        busy.start();

        // Get a reference to the current track
        QValueList <int> branches_to_current_node;

        QValueList <int> *a_route;
        a_route = music_tree_list->getRouteToActive();
        branches_to_current_node = *a_route;

        // reload music
        all_music->startLoading();
        while (!all_music->doneLoading())
        {
            qApp->processEvents();
            usleep(50000);
        }
        all_playlists->postLoad();

        constructPlaylistTree();

        if (music_tree_list->tryToSetActive(branches_to_current_node))
        {
            //  All is well
        }
        else
        {
            // should never happen
            stop();
            wipeTrackInfo();
            branches_to_current_node.clear();
            branches_to_current_node.append(0); //  Root node
            branches_to_current_node.append(1); //  We're on a playlist (not "My Music")
            branches_to_current_node.append(0); //  Active play Queue
            music_tree_list->moveToNodesFirstChild(branches_to_current_node);
        }

        GenericTree *node = music_tree_list->getCurrentNode();
        curMeta = all_music->getMetadata(node->getInt());

        setShuffleMode(shufflemode);

        music_tree_list->refresh();

        busy.Close();
    }
}

void PlaybackBoxMusic::checkForPlaylists()
{
    // This is only done off a timer on startup

    if (first_playlist_check)
    {
        first_playlist_check = false;
        repaint();
    }
    else
    {
        if (all_playlists->doneLoading() &&
            all_music->doneLoading())
        {
            if (progress)
            {
                progress->Close();
                delete progress;
                progress = NULL;
                progress_type = kProgressNone;
            }

            if (tree_is_done)
            {
                if (scan_for_cd)
                    updatePlaylistFromCD();

                music_tree_list->showWholeTree(show_whole_tree);
                QValueList <int> branches_to_current_node;
                branches_to_current_node.append(0); //  Root node
                branches_to_current_node.append(1); //  We're on a playlist (not "My Music")
                branches_to_current_node.append(0); //  Active play Queue

                if (resumemode > RESUME_OFF)
                    restorePosition();
                else
                    music_tree_list->moveToNodesFirstChild(branches_to_current_node);

                music_tree_list->refresh();
                if (show_whole_tree)
                    setContext(1);
                else
                    setContext(2);
                updateForeground();
                mainvisual->setVisual(visual_modes[current_visual]);

                if (curMeta) 
                    updateTrackInfo(curMeta);

                return;     // Do not restart Timer
            }
            else
            {
                constructPlaylistTree();
            }
        }
        else
        {
            if (!all_music->doneLoading())
            {
                // Only bother with progress dialogue
                // if we have a reasonable number of tracks
                if (all_music->count() >= 250)
                {
                    if (!progress)
                    {
                        progress = new MythProgressDialog(
                            QObject::tr("Loading Music"), all_music->count());
                        progress_type = kProgressMusic;
                    }
                    progress->setProgress(all_music->countLoaded());
                }
            } 
            else if (progress_type == kProgressMusic)
            {
                if (progress)
                {
                    progress->Close();
                    delete progress;
                }
                progress = NULL;
                progress_type = kProgressNone;
            }
        }
    }

    waiting_for_playlists_timer->start(100, TRUE); // Restart Timer
}

void PlaybackBoxMusic::changeVolume(bool up_or_down)
{
    if (volume_control && output)
    {
        if (up_or_down)
            output->AdjustCurrentVolume(2);
        else
            output->AdjustCurrentVolume(-2);
        showVolume(true);
    }
}

void PlaybackBoxMusic::toggleMute()
{
    if (volume_control && output)
    {
        output->ToggleMute();
        showVolume(true);
    }
}

void PlaybackBoxMusic::showProgressBar()
{

    if (progress_bar) {

             progress_bar->SetTotal(maxTime);
             progress_bar->SetUsed(currentTime);
    }

}
void PlaybackBoxMusic::showVolume(bool on_or_off)
{
    float volume_level;
    if (volume_control && output)
    {
        if (volume_status)
        {
            if (on_or_off)
            {
                volume_status->SetUsed(output->GetCurrentVolume());
                volume_status->SetOrder(0);
                volume_status->refresh();
                volume_display_timer->changeInterval(2000);
                if (class LCD *lcd = LCD::Get())
                    lcd->switchToVolume("Music");

                if (output->GetMute())
                    volume_level = 0.0;
                else
                    volume_level = (float)output->GetCurrentVolume() / 
                                   (float)100;

                if (class LCD *lcd = LCD::Get())
                    lcd->setVolumeLevel(volume_level);
            }
            else
            {
                if (volume_status->getOrder() != -1)
                {
                    volume_status->SetOrder(-1);
                    volume_status->refresh();

                    if (curMeta)
                        setTrackOnLCD(curMeta);
                }
            }
        }
    }
}

void PlaybackBoxMusic::resetTimer()
{
    if (visual_mode_delay > 0)
        visual_mode_timer->changeInterval(visual_mode_delay * 1000);
}

void PlaybackBoxMusic::play()
{

    if (isplaying)
        stop();

    if (curMeta)
        playfile = curMeta->Filename();
    else
    {
        // Perhaps we can descend to something playable?
        wipeTrackInfo();
        return;
    }

    QUrl sourceurl(playfile);
    QString sourcename(playfile);

    if (!output)
        openOutputDevice();

    if (output->GetPause())
    {
        pause();
        return;
    }

    if (!sourceurl.isLocalFile()) 
    {
        StreamInput streaminput(sourceurl);
        streaminput.setup();
        input = streaminput.socket();
    } 
    else
        input = new QFile(playfile);
    
    if (decoder && !decoder->factory()->supports(sourcename))
    {
        decoder->removeListener(this);
        decoder = 0;
    }

    if (!decoder)
    {
        decoder = Decoder::create(sourcename, input, output);

        if (!decoder) 
        {
            printf("mythmusic: unsupported fileformat\n");
            stopAll();
            return;
        }
        if (sourcename.contains("cda") == 1)
            dynamic_cast<CdDecoder*>(decoder)->setDevice(m_CDdevice);

        decoder->setBlockSize(globalBlockSize);
        decoder->addListener(this);
    } 
    else 
    {
        decoder->setInput(input);
        decoder->setFilename(sourcename);
        decoder->setOutput(output);
    }

    currentTime = 0;

    mainvisual->setDecoder(decoder);
    mainvisual->setOutput(output);
    mainvisual->setMetadata(curMeta);

    if (decoder->initialize()) 
    {
        if (output)
        {
            output->Reset();
        }

        decoder->start();

        if (resumemode == RESUME_EXACT && gContext->GetNumSetting("MusicBookmarkPosition", 0) > 0)
        {
            seek(gContext->GetNumSetting("MusicBookmarkPosition", 0));
            gContext->SaveSetting("MusicBookmarkPosition", 0);
        }

        bannerEnable(curMeta, show_album_art);
        isplaying = true;
        curMeta->setLastPlay();
        curMeta->incPlayCount();
    }
}

void PlaybackBoxMusic::visEnable()
{
    if (!visualizer_status != 2 && isplaying)
    {
        setUpdatesEnabled(false);
        mainvisual->setGeometry(0, 0, screenwidth, screenheight);
        visualizer_status = 2;
    }

    bannerDisable();
}

void PlaybackBoxMusic::bannerEnable(QString text, int millis)
{
    if (visualizer_status != 2) 
        return;

    mainvisual->showBanner(text, millis);
}

void PlaybackBoxMusic::bannerEnable(Metadata *mdata, bool fullScreen)
{
    mainvisual->showBanner(mdata, fullScreen, visualizer_status, 8000);
}

void PlaybackBoxMusic::bannerToggle(Metadata *mdata) 
{
    if (mainvisual->bannerIsShowing())
        bannerDisable();
    else
        bannerEnable(mdata, false);
}

void PlaybackBoxMusic::bannerDisable()
{
    mainvisual->hideBanner();
}

void PlaybackBoxMusic::CycleVisualizer()
{
    // Only change the visualizer if there is more than 1 visualizer
    // and the user currently has a visualizer active
    if (visual_modes.count() > 1 && visualizer_status > 0)
    {
        if (random_visualizer)
        {
            unsigned int next_visualizer;

            //Find a visual thats not like the previous visual
            do
                next_visualizer = rand() % visual_modes.count();
            while (next_visualizer == current_visual);
            current_visual = next_visualizer;
        }
        else
        {
            //Change to the next selected visual 
            current_visual = (current_visual + 1) % visual_modes.count();
        }

        //Change to the new visualizer
        resetTimer();
        mainvisual->setVisual("Blank");
        mainvisual->setVisual(visual_modes[current_visual]);
    }
    else if (visual_modes.count() == 1 && visual_modes[current_visual] == "AlbumArt" && 
             visualizer_status > 0) 
    {
        // If only the AlbumArt visualization is selected, then go ahead and
        // restart the visualization.  This will give AlbumArt the opportunity
        // to change images if there are multiple images available.
        resetTimer();
        mainvisual->setVisual("Blank");
        mainvisual->setVisual(visual_modes[current_visual]);
    }
}

void PlaybackBoxMusic::setTrackOnLCD(Metadata *mdata)
{
    LCD *lcd = LCD::Get();
    if (!lcd)
        return;

    // Set the Artist and Tract on the LCD
    lcd->switchToMusic(mdata->Artist(), 
                       mdata->Album(), 
                       mdata->Title());
}

void PlaybackBoxMusic::pause(void)
{
    if (output) 
    {
        isplaying = !isplaying;
        output->Pause(!isplaying); //Note pause doesn't take effect instantly
    }
    // wake up threads
    if (decoder) 
    {
        decoder->lock();
        decoder->cond()->wakeAll();
        decoder->unlock();
    }

}

void PlaybackBoxMusic::stopDecoder(void)
{
    if (decoder && decoder->running()) 
        decoder->stop();

    if (decoder) 
    {
        decoder->lock();
        decoder->cond()->wakeAll();
        decoder->unlock();
    }

    if (decoder)
        decoder->wait();
}

void PlaybackBoxMusic::stop(void)
{
    stopDecoder();

    if (output)
    {
        if (output->GetPause())
        {
            pause();
        }
        output->Reset();
    }

    mainvisual->setDecoder(0);
    mainvisual->setOutput(0);
    mainvisual->deleteMetadata();

    delete input;
    input = 0;

    QString time_string = getTimeString(maxTime, 0);

    if (time_text)
        time_text->SetText(time_string);
    if (info_text)
        info_text->SetText("");

    isplaying = false;
}

void PlaybackBoxMusic::stopAll()
{
    if (class LCD *lcd = LCD::Get())
    {
        lcd->switchToTime();
    }

    stop();

    if (decoder) 
    {
        decoder->removeListener(this);
        decoder = 0;
    }
}

void PlaybackBoxMusic::previous()
{
    if (repeatmode == REPEAT_ALL)
    {
        if (music_tree_list->prevActive(true, show_whole_tree))
            music_tree_list->activate();
    }
    else
    {
        if (music_tree_list->prevActive(false, show_whole_tree))
            music_tree_list->activate();
    }
     
    if (visualizer_status > 0 && cycle_visualizer)
        CycleVisualizer();
}

void PlaybackBoxMusic::next()
{
    if (repeatmode == REPEAT_ALL)
    {
        // Grab the next track after this one. First flag is to wrap around
        // to the beginning of the list. Second decides if we will traverse up 
        // and down the tree.
        if (music_tree_list->nextActive(true, show_whole_tree))
            music_tree_list->activate();
        else 
            end();
    }
    else
    {
        if (music_tree_list->nextActive(false, show_whole_tree))
            music_tree_list->activate(); 
        else 
            end();
    }
     
    if (visualizer_status > 0 && cycle_visualizer)
        CycleVisualizer();
}

void PlaybackBoxMusic::nextAuto()
{
    stopDecoder();

    isplaying = false;

    if (repeatmode == REPEAT_TRACK)
        play();
    else 
        next();
}

void PlaybackBoxMusic::seekforward()
{
    int nextTime = currentTime + 5;
    if (nextTime > maxTime)
        nextTime = maxTime;
    seek(nextTime);
}

void PlaybackBoxMusic::seekback()
{
    int nextTime = currentTime - 5;
    if (nextTime < 0)
        nextTime = 0;
    seek(nextTime);
}

void PlaybackBoxMusic::seek(int pos)
{
    if (output)
    {
        output->Reset();
        output->SetTimecode(pos*1000);

        if (decoder && decoder->running()) 
        {
            decoder->lock();
            decoder->seek(pos);

            if (mainvisual) 
            {
                mainvisual->mutex()->lock();
                mainvisual->prepare();
                mainvisual->mutex()->unlock();
            }

            decoder->unlock();
        }

        if (!isplaying)
        {
            currentTime = pos;
            if (time_text)
                time_text->SetText(getTimeString(pos, maxTime));

            showProgressBar();

            if (class LCD *lcd = LCD::Get())
            {
                float percent_heard = maxTime <= 0 ? 0.0 : ((float)pos /
                                      (float)maxTime);

                QString lcd_time_string = getTimeString(pos, maxTime);

                // if the string is longer than the LCD width, remove all spaces
                if (lcd_time_string.length() > lcd->getLCDWidth())
                    lcd_time_string.remove(' ');

                lcd->setMusicProgress(lcd_time_string, percent_heard);
            }
        }
    }
}

void PlaybackBoxMusic::setShuffleMode(unsigned int mode)
{
    shufflemode = mode;

    switch (shufflemode)
    {
        case SHUFFLE_INTELLIGENT:
            if(shuffle_button)
            {
                if (keyboard_accelerators)
                    shuffle_button->setText(tr("1 Shuffle: Smart"));
                else
                    shuffle_button->setText(tr("Shuffle: Smart"));
            }
            music_tree_list->scrambleParents(true);

            if (class LCD *lcd = LCD::Get())
                lcd->setMusicShuffle(LCD::MUSIC_SHUFFLE_SMART);

            bannerEnable(tr("Shuffle: Smart"), 4000);
            break;
        case SHUFFLE_RANDOM:
            if(shuffle_button)
            {
                if (keyboard_accelerators)
                    shuffle_button->setText(tr("1 Shuffle: Rand"));
                else
                    shuffle_button->setText(tr("Shuffle: Rand"));
            }
            music_tree_list->scrambleParents(true);

            if (class LCD *lcd = LCD::Get())
                lcd->setMusicShuffle(LCD::MUSIC_SHUFFLE_RAND);

            bannerEnable(tr("Shuffle: Rand"), 4000);
            break;
        case SHUFFLE_ALBUM:
            if(shuffle_button)
            {
                if (keyboard_accelerators)
                    shuffle_button->setText(tr("1 Shuffle: Album"));
                else
                    shuffle_button->setText(tr("Shuffle: Album"));
            }
            music_tree_list->scrambleParents(true);

            if (class LCD *lcd = LCD::Get())
                lcd->setMusicShuffle(LCD::MUSIC_SHUFFLE_ALBUM);

            bannerEnable(tr("Shuffle: Album"), 4000);
            break;
        default:
            if(shuffle_button)
            {
                if (keyboard_accelerators)
                    shuffle_button->setText(tr("1 Shuffle: None"));
                else
                    shuffle_button->setText(tr("Shuffle: None"));
            }
            music_tree_list->scrambleParents(false);

            if (class LCD *lcd = LCD::Get())
                lcd->setMusicShuffle(LCD::MUSIC_SHUFFLE_NONE);

            bannerEnable(tr("Shuffle: None"), 4000);
            break;
    }
    music_tree_list->setTreeOrdering(shufflemode + 1);
    if (listAsShuffled)
        music_tree_list->setVisualOrdering(shufflemode + 1);
    else
        music_tree_list->setVisualOrdering(1);
    music_tree_list->refresh();

    if (isplaying)
        setTrackOnLCD(curMeta);
}

void PlaybackBoxMusic::toggleShuffle(void)
{
    setShuffleMode(++shufflemode % MAX_SHUFFLE_MODES);
}

void PlaybackBoxMusic::increaseRating()
{
    if(!curMeta)
        return;

    // Rationale here is that if you can't get visual feedback on ratings 
    // adjustments, you probably should not be changing them

    if (showrating)
    {
        curMeta->incRating();
        if (ratings_image)
            ratings_image->setRepeat(curMeta->Rating());
    }
}

void PlaybackBoxMusic::decreaseRating()
{
    if(!curMeta)
        return;

    if (showrating)
    {
        curMeta->decRating();
        if (ratings_image)
            ratings_image->setRepeat(curMeta->Rating());
    }
}

void PlaybackBoxMusic::setRepeatMode(unsigned int mode)
{
    repeatmode = mode;

    if (!repeat_button)
        return;

    switch (repeatmode)
    {
        case REPEAT_ALL:
            if (keyboard_accelerators)
                repeat_button->setText(tr("2 Repeat: All"));
            else
                repeat_button->setText(tr("Repeat: All"));

            if (class LCD *lcd = LCD::Get())
                lcd->setMusicRepeat (LCD::MUSIC_REPEAT_ALL);

            bannerEnable(tr("Repeat: All"), 4000);
            break;
        case REPEAT_TRACK:
            if (keyboard_accelerators)
                repeat_button->setText(tr("2 Repeat: Track"));
            else
                repeat_button->setText(tr("Repeat: Track"));

            if (class LCD *lcd = LCD::Get())
                lcd->setMusicRepeat (LCD::MUSIC_REPEAT_TRACK);

            bannerEnable(tr("Repeat: Track"), 4000);
            break;
        default:
            if (keyboard_accelerators)
                repeat_button->setText(tr("2 Repeat: None"));
            else
                repeat_button->setText(tr("Repeat: None"));

            if (class LCD *lcd = LCD::Get())
                lcd->setMusicRepeat (LCD::MUSIC_REPEAT_NONE);

            bannerEnable(tr("Repeat: None"), 4000);
            break;
    }
}

void PlaybackBoxMusic::savePosition(uint position)
{
    QValueList <int> branches_to_current_node;

    if (curMeta)
    {
        QValueList <int> *a_route;
        a_route = music_tree_list->getRouteToActive();
        branches_to_current_node = *a_route;
    }
    else
    {
        branches_to_current_node.clear();
        branches_to_current_node.append(0); //  Root node
        branches_to_current_node.append(1); //  We're on a playlist (not "My Music")
        branches_to_current_node.append(0); //  Active play Queue
        position = 0;
    }

    QString s = "";
    QValueListIterator <int> it;
    for (it = branches_to_current_node.begin(); it != branches_to_current_node.end(); ++it)
        s += "," + QString::number(*it);

    s.remove(0, 1);

    gContext->SaveSetting("MusicBookmark", s);
    gContext->SaveSetting("MusicBookmarkPosition", position);
}

void PlaybackBoxMusic::restorePosition()
{
    QValueList <int> branches_to_current_node;
    QString s = gContext->GetSetting("MusicBookmark", "");

    if (s != "")
    {
        QStringList list = QStringList::split(",", s);

        for (QStringList::Iterator it = list.begin(); it != list.end(); ++it)
            branches_to_current_node.append((*it).toInt());

        // try to restore the saved position
        if (music_tree_list->tryToSetActive(branches_to_current_node))
        {
            music_tree_list->select();
            return;
        }
    }

    branches_to_current_node.clear();
    branches_to_current_node.append(0);
    branches_to_current_node.append(1);
    branches_to_current_node.append(0);
    music_tree_list->moveToNodesFirstChild(branches_to_current_node);
}

void PlaybackBoxMusic::toggleRepeat()
{
    setRepeatMode(++repeatmode % MAX_REPEAT_MODES);
}

void PlaybackBoxMusic::constructPlaylistTree()
{
    if (playlist_tree)
        delete playlist_tree;

    playlist_tree = new GenericTree(tr("playlist root"), 0);
    playlist_tree->setAttribute(0, 0);
    playlist_tree->setAttribute(1, 0);
    playlist_tree->setAttribute(2, 0);
    playlist_tree->setAttribute(3, 0);

    // We ask the playlist object to write out the whole tree (all playlists 
    // and all music). It will set attributes for nodes in the tree, such as 
    // whether a node is selectable, how it can be ordered (normal, random, 
    // intelligent, album), etc. 

    GenericTree *active_playlist_node = all_playlists->writeTree(playlist_tree);
    music_tree_list->assignTreeData(playlist_tree);
    music_tree_list->setCurrentNode(active_playlist_node);
    tree_is_done = true;
}

void PlaybackBoxMusic::editPlaylist()
{
    // Get a reference to the current track

    QValueList <int> branches_to_current_node;

    if (curMeta)
    {
        QValueList <int> *a_route;
        a_route = music_tree_list->getRouteToActive();
        branches_to_current_node = *a_route;
    }
    else
    {
        // No current metadata, so when we come back we'll try and play the 
        // first thing on the active queue

        branches_to_current_node.clear();
        branches_to_current_node.append(0); //  Root node
        branches_to_current_node.append(1); //  We're on a playlist (not "My Music")
        branches_to_current_node.append(0); //  Active play Queue
    }

    visual_mode_timer->stop();
    DatabaseBox dbbox(all_playlists, all_music, gContext->GetMainWindow(),
                      m_CDdevice, "music_select", "music-", "database box");

    if (cd_watcher)
        cd_watcher->stop();

    dbbox.exec();
    if (visual_mode_delay > 0)
        visual_mode_timer->start(visual_mode_delay * 1000);

    // OK, we're back ....
    // now what do we do? see if we can find the same track at the same level

    constructPlaylistTree();
    if (music_tree_list->tryToSetActive(branches_to_current_node))
    {
        //  All is well
    }
    else
    {
        stop();
        wipeTrackInfo();
        branches_to_current_node.clear();
        branches_to_current_node.append(0); //  Root node
        branches_to_current_node.append(1); //  We're on a playlist (not "My Music")
        branches_to_current_node.append(0); //  Active play Queue
        music_tree_list->moveToNodesFirstChild(branches_to_current_node);
    }
    music_tree_list->refresh();

    if (scan_for_cd && cd_watcher)
        cd_watcher->start(1000);
}

void PlaybackBoxMusic::closeEvent(QCloseEvent *event)
{
    stopAll();

    hide();
    event->accept();
}

void PlaybackBoxMusic::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
}

void PlaybackBoxMusic::customEvent(QCustomEvent *event)
{
    switch ((int)event->type()) 
    {
        case OutputEvent::Playing:
        {
            if (curMeta)
                updateTrackInfo(curMeta);
            statusString = tr("Playing stream.");
            break;
        }

        case OutputEvent::Buffering:
        {
            statusString = tr("Buffering stream.");
            break;
        }

        case OutputEvent::Paused:
        {
            statusString = tr("Stream paused.");
            break;
        }

        case OutputEvent::Info:
        {
            OutputEvent *oe = (OutputEvent *) event;

            int rs;
            currentTime = rs = oe->elapsedSeconds();

            QString time_string = getTimeString(rs, maxTime);

            showProgressBar();
            
            if (curMeta)
            {
                if (class LCD *lcd = LCD::Get())
                {
                    float percent_heard = maxTime<=0?0.0:((float)rs / 
                                                          (float)curMeta->Length()) * 1000.0;

                    QString lcd_time_string = time_string; 

                    // if the string is longer than the LCD width, remove all spaces
                    if (time_string.length() > lcd->getLCDWidth())
                        lcd_time_string.remove(' ');

                    lcd->setMusicProgress(lcd_time_string, percent_heard);
                }
            }

            QString info_string;

            //  Hack around for cd bitrates
            if (oe->bitrate() < 2000)
            {
                info_string.sprintf("%d "+tr("kbps")+ "   %.1f "+ tr("kHz")+ "   %s "+ tr("ch"),
                                   oe->bitrate(), float(oe->frequency()) / 1000.0,
                                   oe->channels() > 1 ? "2" : "1");
            }
            else
            {
                info_string.sprintf("%.1f "+ tr("kHz")+ "   %s "+ tr("ch"),
                                   float(oe->frequency()) / 1000.0,
                                   oe->channels() > 1 ? "2" : "1");
            }

            if (curMeta)
            {
                if (time_text)
                    time_text->SetText(time_string);
                if (info_text)
                    info_text->SetText(info_string);
                if (current_visualization_text)
                {
                    current_visualization_text->SetText(visual_modes[current_visual]);
                    current_visualization_text->refresh();
                }
            }

            break;
        }
        case OutputEvent::Error:
        {
            statusString = tr("Output error.");

            OutputEvent *aoe = (OutputEvent *) event;

            VERBOSE(VB_IMPORTANT, QString("%1 %2").arg(statusString)
                .arg(*aoe->errorMessage()));
            MythPopupBox::showOkPopup(gContext->GetMainWindow(), 
                                      statusString,
                                      QString("MythMusic has encountered the following error:\n%1")
                                      .arg(*aoe->errorMessage()));
            stopAll();

            break;
        }
        case DecoderEvent::Stopped:
        {
            statusString = tr("Stream stopped.");

            break;
        }
        case DecoderEvent::Finished:
        {
            statusString = tr("Finished playing stream.");
            nextAuto();
            break;
        }
        case DecoderEvent::Error:
        {
            stopAll();
            QApplication::sendPostedEvents();

            statusString = tr("Decoder error.");

            DecoderEvent *dxe = (DecoderEvent *) event;

            VERBOSE(VB_IMPORTANT, QString("%1 %2").arg(statusString)
                .arg(*dxe->errorMessage()));
            MythPopupBox::showOkPopup(gContext->GetMainWindow(), 
                                      statusString,
                                      QString("MythMusic has encountered the following error:\n%1")
                                      .arg(*dxe->errorMessage()));
            break;
        }
    }

    QWidget::customEvent(event);
}

void PlaybackBoxMusic::wipeTrackInfo()
{
        if (title_text)
            title_text->SetText("");
        if (artist_text)
            artist_text->SetText("");
        if (album_text)
            album_text->SetText("");
        if (time_text)
            time_text->SetText("");
        if (info_text)
            info_text->SetText("");
        if (ratings_image)
            ratings_image->setRepeat(0);
        if (current_visualization_text)
            current_visualization_text->SetText("");
}

void PlaybackBoxMusic::updateTrackInfo(Metadata *mdata)
{
    if (title_text)
        title_text->SetText(mdata->FormatTitle());
    if (artist_text)
        artist_text->SetText(mdata->FormatArtist());
    if (album_text)
        album_text->SetText(mdata->Album());

    setTrackOnLCD(mdata);
}

void PlaybackBoxMusic::openOutputDevice(void)
{
    QString adevice;

    if (gContext->GetSetting("MusicAudioDevice") == "default")
        adevice = gContext->GetSetting("AudioOutputDevice");
    else
        adevice = gContext->GetSetting("MusicAudioDevice");

    // TODO: Error checking that device is opened correctly!
    output = AudioOutput::OpenAudio(adevice, "default", 16, 2, 44100,
                                    AUDIOOUTPUT_MUSIC, true,
                                    false /* AC3/DTS pass through */);
    output->setBufferSize(outputBufferSize * 1024);
    output->SetBlocking(false);
    output->addListener(this);
    output->addListener(mainvisual);
    output->addVisual(mainvisual);    
}

void PlaybackBoxMusic::handleTreeListSignals(int node_int, IntVector *attributes)
{
    if (attributes->size() < 4)
    {
        VERBOSE(VB_IMPORTANT, "playbackbox.o: Worringly, a managed tree "
                "list is handing back item attributes of the wrong size");
        return;
    }

    if (attributes->at(0) == 1)
    {
        //  It's a track

        curMeta = all_music->getMetadata(node_int);

        updateTrackInfo(curMeta);

        maxTime = curMeta->Length() / 1000;

        QString time_string = getTimeString(maxTime, 0);
        
        if (showrating)
        {
            if(ratings_image)
                ratings_image->setRepeat(curMeta->Rating());
        }

        if (output && output->GetPause())
            stop();

        if (m_pushedButton && m_pushedButton->Name() == "play_button")
        {
            // Play button already pushed, so don't push it again.
            play();
        }
        else if (play_button)
            play_button->push();
        else
            play();
    }
    else
    {
        curMeta = NULL;
        wipeTrackInfo();
    }
}


void PlaybackBoxMusic::toggleFullBlankVisualizer()
{
    if (fullscreen_blank)
    {
        fullscreen_blank = false;

        //
        //  If we are already full screen and 
        //  blank, go back to regular dialog
        //

        if(visual_blackhole)
            mainvisual->setGeometry(visual_blackhole->getScreenArea());
        else
            mainvisual->setGeometry(screenwidth + 10, screenheight + 10, 
                                    160, 160);
        mainvisual->setVisual(visual_modes[current_visual]);
        bannerDisable();
        visualizer_status = 1;
        if(visual_mode_delay > 0)
        {
            visual_mode_timer->start(visual_mode_delay * 1000);
        }
        if (current_visualization_text)
        {
            current_visualization_text->SetText(visual_modes[current_visual]);
            current_visualization_text->refresh();
        }
        setUpdatesEnabled(true);
    }
    else
    {
        //
        //  Otherwise, go full screen blank
        //

        fullscreen_blank = true;
        mainvisual->setVisual("Blank");
        mainvisual->setGeometry(0, 0, screenwidth, screenheight);
        visualizer_status = 2;
        visual_mode_timer->stop();
        setUpdatesEnabled(false);
        bannerDisable();
    }
}

void PlaybackBoxMusic::end()
{
    if (class LCD *lcd = LCD::Get()) 
        lcd->switchToTime ();
}

void PlaybackBoxMusic::wireUpTheme()
{
    // The self managed music tree list
    //
    // Complain if we can't find this
    music_tree_list = getUIManagedTreeListType("musictreelist");
    if (!music_tree_list)
    {
        VERBOSE(VB_IMPORTANT, "playbackbox.o: Couldn't find a music tree list "
                              "in your theme");
        exit(0);
    }
    connect(music_tree_list, SIGNAL(nodeSelected(int, IntVector*)), 
            this, SLOT(handleTreeListSignals(int, IntVector*)));

    // All the other GUI elements are **optional**
    title_text = getUITextType("title_text");
    artist_text = getUITextType("artist_text");
    time_text = getUITextType("time_text");
    info_text = getUITextType("info_text");
    album_text = getUITextType("album_text");
    ratings_image = getUIRepeatedImageType("ratings_image");
    current_visualization_text = getUITextType("current_visualization_text");
    progress_bar = getUIStatusBarType("progress_bar");
    volume_status = getUIStatusBarType("volume_status");
    if (volume_status)
    {
        volume_status->SetTotal(100);
        volume_status->SetOrder(-1);
    }
    visual_blackhole = getUIBlackHoleType("visual_blackhole");

    //  Buttons
    prev_button = getUIPushButtonType("prev_button");
    if (prev_button)
        connect(prev_button, SIGNAL(pushed()), this, SLOT(previous()));

    rew_button = getUIPushButtonType("rew_button");
    if (rew_button)
        connect(rew_button, SIGNAL(pushed()), this, SLOT(seekback()));

    pause_button = getUIPushButtonType("pause_button");
    pause_button->setLockOn();
    if (pause_button)
        connect(pause_button, SIGNAL(pushed(QString)), this,
        SLOT(handlePush(QString)));

    play_button = getUIPushButtonType("play_button");
    play_button->setLockOn();
    if (play_button)
        connect(play_button, SIGNAL(pushed(QString)), this,
        SLOT(handlePush(QString)));

    stop_button = getUIPushButtonType("stop_button");
    stop_button->setLockOn();
    if (stop_button)
        connect(stop_button, SIGNAL(pushed(QString)), this,
        SLOT(handlePush(QString)));

    ff_button = getUIPushButtonType("ff_button");
    if (ff_button)
        connect(ff_button, SIGNAL(pushed()), this, SLOT(seekforward()));

    next_button = getUIPushButtonType("next_button");
    if (next_button)
        connect(next_button, SIGNAL(pushed()), this, SLOT(next()));

    shuffle_button = getUITextButtonType("shuffle_button");
    if (shuffle_button)
        connect(shuffle_button, SIGNAL(pushed()), this, SLOT(toggleShuffle()));

    repeat_button = getUITextButtonType("repeat_button");
    if (repeat_button)
        connect(repeat_button, SIGNAL(pushed()), this, SLOT(toggleRepeat()));

    pledit_button = getUITextButtonType("pledit_button");
    if (pledit_button)
        connect(pledit_button, SIGNAL(pushed()), this, SLOT(editPlaylist()));

    vis_button = getUITextButtonType("vis_button");
    if (vis_button)
        connect(vis_button, SIGNAL(pushed()), this, SLOT(visEnable()));
}

bool PlaybackBoxMusic::getInsertPLOptions(InsertPLOption &insertOption,
                                          PlayPLOption &playOption, bool &bRemoveDups)
{
    MythPopupBox *popup = new MythPopupBox(gContext->GetMainWindow(),
                                      "playlist_popup");

    QLabel *caption = popup->addLabel(tr("Update Playlist Options"), MythPopupBox::Medium);
    caption->setAlignment(Qt::AlignCenter);

    QButton *button = popup->addButton(tr("Replace"));
    popup->addButton(tr("Insert after current track"));
    popup->addButton(tr("Append to end"));
    button->setFocus();

    QLabel *splitter = popup->addLabel(" ", MythPopupBox::Small);
    splitter->setLineWidth(2);
    splitter->setFrameShape(QFrame::HLine);
    splitter->setFrameShadow(QFrame::Sunken);
    splitter->setMinimumHeight((int) (25 * hmult));
    splitter->setMaximumHeight((int) (25 * hmult));

    // only give the user a choice of the track to play if shuffle mode is off
    MythComboBox *playCombo = NULL;
    if (shufflemode == SHUFFLE_OFF)
    {
        playCombo = new MythComboBox(false, popup, "play_combo" );
        playCombo->insertItem(tr("Continue playing current track"));
        playCombo->insertItem(tr("Play first track"));
        playCombo->insertItem(tr("Play first new track"));
        playCombo->setBackgroundOrigin(ParentOrigin);
        popup->addWidget(playCombo);
    }
    
    MythCheckBox *dupsCheck = new MythCheckBox(popup);
    dupsCheck->setText(tr("Remove Duplicates"));
    dupsCheck->setChecked(false);
    dupsCheck->setBackgroundOrigin(ParentOrigin);
    popup->addWidget(dupsCheck);

    int res = popup->ExecPopup();
    switch (res)
    {
        case 0:
            insertOption = PL_REPLACE;
            break;
        case 1:
            insertOption = PL_INSERTAFTERCURRENT;
            break;
        case 2:
            insertOption = PL_INSERTATEND;
            break;
    }

    bRemoveDups = dupsCheck->isChecked();


    // if shuffle mode != SHUFFLE_OFF we always try to continue playing
    // the current track
    if (shufflemode == SHUFFLE_OFF)
    {
        switch (playCombo->currentItem())
        {
            case 0:
                playOption = PL_CURRENT;
                break;
            case 1:
                playOption = PL_FIRST;
                break;
            case 2:
                playOption = PL_FIRSTNEW;
                break;
            default:
                playOption = PL_CURRENT;
        }
    }
    else
        playOption = PL_CURRENT;

    delete popup;

    return (res >= 0);
}

QString PlaybackBoxMusic::getTimeString(int exTime, int maxTime)
{
    QString time_string;

    int eh = exTime / 3600;
    int em = (exTime / 60) % 60;
    int es = exTime % 60;

    int maxh = maxTime / 3600;
    int maxm = (maxTime / 60) % 60;
    int maxs = maxTime % 60;

    if (maxTime <= 0) 
    {
        if (eh > 0) 
            time_string.sprintf("%d:%02d:%02d", eh, em, es);
        else 
            time_string.sprintf("%02d:%02d", em, es);
    } 
    else
    {
        if (maxh > 0)
            time_string.sprintf("%d:%02d:%02d / %02d:%02d:%02d", eh, em,
                    es, maxh, maxm, maxs);
        else
            time_string.sprintf("%02d:%02d / %02d:%02d", em, es, maxm, 
                    maxs);
    }

    return time_string;
}
