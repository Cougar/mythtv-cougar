/*
	phoneui.cpp

	(c) 2004 Paul Volkaerts
	
  Implementation of the main telephony user interface
*/
#include <qapplication.h>
#include <qfile.h>
#include <qdialog.h>   
#include <qcursor.h>
#include <qdir.h>
#include <qimage.h>

#include <stdlib.h>
#include <unistd.h>
#include <iostream>
using namespace std;

#include <linux/videodev.h>
#include <mythtv/mythcontext.h>
#include <mythtv/mythwidgets.h>
#include <mythtv/lcddevice.h>

#include "config.h"
#include "webcam.h"
#include "sipfsm.h"
#include "phoneui.h"
#include "vxml.h"

#define DEFAULTTHEMEPATH  PREFIX "/share/mythtv/themes/default/"




PhoneUIBox::PhoneUIBox(QSqlDatabase *db,
                     MythMainWindow *parent, QString window_name,
                     QString theme_filename, const char *name)

           : MythThemedDialog(parent, window_name, theme_filename, name)
{

    h263 = new H263Container();

    // Local Variables
    SelectHit = false;
    VideoOn = false;

    wireUpTheme();
    phoneUIStatusBar = new PhoneUIStatusBar(getUITextType("caller_text"),
                                            getUITextType("audio_stats_text"),
                                            getUITextType("video_stats_text"),
                                            getUITextType("bw_stats_text"),
                                            getUITextType("call_time_text"),
                                            getUITextType("status_msg_text"));

    bool reg;
    QString regAs;
    QString regTo;
    sipStack->GetRegistrationStatus(reg, regTo, regAs);
    if (reg)
        phoneUIStatusBar->DisplayNotification(QString("Registered to " + regTo + " as " + regAs), 5);
    else
        phoneUIStatusBar->DisplayNotification("Not Registered", 5);
  
    // Read the directory into the object structures
    DirContainer = new DirectoryContainer(db);
    DirContainer->Load();
    DirContainer->createTree();

    DirectoryList->setVisualOrdering(2); // Attribute 2 has ordering info
    DirectoryList->setTreeOrdering(2);
    DirectoryList->setIconSelector(3); // Attribute 3 has icon info

    DirContainer->writeTree();
    DirectoryList->assignTreeData(DirContainer->getTreeRoot());

    // Finally initialise the GUI box
    DirectoryList->showWholeTree(true);
    DirectoryList->colorSelectables(true);

    QValueList <int> branches_to_current_node;
    branches_to_current_node.append(0); //  Root node
    branches_to_current_node.append(0); //  Speed Dials!
    DirectoryList->moveToNodesFirstChild(branches_to_current_node);
    DirectoryList->refresh();

    updateForeground();

    // Update SIP Presence information
    sipStack->UiOpened(this);
    sipStack->UiWatch(DirContainer->ListAllEntries(true));

    // Possibly (user-defined) control the volume
    volume_control = NULL;
    volume_display_timer = new QTimer(this);
    VolumeMode = VOL_VOLUME;
    volume_icon->SetImage(DEFAULTTHEMEPATH "mp_volume_icon.png");
    volume_icon->LoadImage();
    if (gContext->GetNumSetting("MythControlsVolume", 0))
    {
        volume_control = new VolumeControl(true);
        connect(volume_display_timer, SIGNAL(timeout()), this, SLOT(hideVolume()));
    }
    
    rtpAudio = 0;
    rtpVideo = 0;

    powerDispTimer = new QTimer(this);
    connect(powerDispTimer, SIGNAL(timeout()), this, SLOT(DisplayMicSpkPower()));

    OnScreenClockTimer = new QTimer(this);
    connect(OnScreenClockTimer, SIGNAL(timeout()), this, SLOT(OnScreenClockTick()));
    ConnectTime = 0;

    // Create the local webcam and start it
    webcam = new Webcam();
    QString WebcamDevice = gContext->GetSetting("WebcamDevice");
    getResolution("CaptureResolution", wcWidth, wcHeight);
    getResolution("TxResolution", txWidth, txHeight);
    txVideoMode = videoResToCifMode(txWidth);

    screenwidth = 0;
    screenheight = 0;
    float wmult = 0, hmult = 0;
    gContext->GetScreenSettings(screenwidth, wmult, screenheight, hmult);
    fullScreen = false;
    rxVideoArea = receivedWebcamArea->getScreenArea();

    camBrightness = 32768; 
    camColour = 32768; 
    camContrast = 32768;
    localClient = 0;
    txClient = 0;
    if (WebcamDevice.length() > 0)
    {
        if (webcam->camOpen(WebcamDevice, wcWidth, wcHeight)) 
        {
            webcam->GetCurSize(&wcWidth, &wcHeight); // See what resolution it actually opened as
            camBrightness = webcam->GetBrightness();
            camContrast = webcam->GetContrast();
            camColour = webcam->GetColour();
            txFps = atoi((const char *)gContext->GetSetting("TransmitFPS"));
            localClient = webcam->RegisterClient(VIDEO_PALETTE_RGB32, 20, this);
        }
    }

    zoomFactor = 10;
    zoomWidth = wcWidth;
    zoomHeight = wcHeight;
    hPan = wPan = 0;

    State = -1;
    menuPopup = NULL;
    urlPopup = NULL;
    addEntryPopup = NULL;
    addDirectoryPopup = NULL;
    incallPopup = NULL;
    currentCallEntry = 0;


    // UK Ringback tone is 400Hz+450Hz with cadence 0.4s on 0.2s off 0.4s on 2s off
    // US Ringback tone is 440Hz+480Hz with cadence 2s on 4s off; in case anyone feels the need to localise this
    Tone oneRing(400, 7000, 400);   // 400Hz for 400ms, volume 7000
    oneRing.sum(450, 7000);         // 450Hz for 400ms, volume 7000
    Tone silenceA(200);
    Tone silenceB(2000);
    ringbackTone = new Tone(oneRing);
    *ringbackTone += silenceA;
    *ringbackTone += oneRing;
    *ringbackTone += silenceB;

    // Used to load voicemail messages in and play them
    vmail = 0;

    // Make up some DTMF tones to play out the speaker as confirmation to the user when sending DTMF
    Tone f1(697, 7000, 100);
    Tone f2(770, 7000, 100);
    Tone f3(852, 7000, 100);
    Tone f4(941, 7000, 100);

    toneDtmf[0] = new Tone(f4);   // Digits zero ..
    toneDtmf[0]->sum(1336, 7000);
    toneDtmf[1] = new Tone(f1);
    toneDtmf[1]->sum(1209, 7000);
    toneDtmf[2] = new Tone(f1);
    toneDtmf[2]->sum(1336, 7000);
    toneDtmf[3] = new Tone(f1);
    toneDtmf[3]->sum(1477, 7000);
    toneDtmf[4] = new Tone(f2);
    toneDtmf[4]->sum(1209, 7000);
    toneDtmf[5] = new Tone(f2);
    toneDtmf[5]->sum(1336, 7000);
    toneDtmf[6] = new Tone(f2);
    toneDtmf[6]->sum(1477, 7000);
    toneDtmf[7] = new Tone(f3);
    toneDtmf[7]->sum(1209, 7000);
    toneDtmf[8] = new Tone(f3);
    toneDtmf[8]->sum(1336, 7000);
    toneDtmf[9] = new Tone(f3);   // .. through nine
    toneDtmf[9]->sum(1477, 7000);
    toneDtmf[10] = new Tone(f4);   // *
    toneDtmf[10]->sum(1209, 7000);
    toneDtmf[11] = new Tone(f4);   // #
    toneDtmf[11]->sum(1477, 7000);

    // Generate a self-event to get current SIP Stack state
    QApplication::postEvent(this, new SipEvent(SipEvent::SipStateChange));

}


void PhoneUIBox::getResolution(QString setting, int &width, int &height)
{
    width = 352;
    height = 288;

    // Gets a settings parameter of the form 640x480 and decodes into width/height
    QString resolution = gContext->GetSetting(setting);
    if (resolution.length() > 0)
    {
        width = atoi((const char *)resolution);
        QString heightString = resolution.mid(resolution.find('x')+1);
        height = atoi((const char *)heightString);
    }
}

const char *PhoneUIBox::videoResToCifMode(int w)
{
    switch (w)
    {
    case 704: return "4CIF";
    case 352: return "CIF";
    case 176: return "QCIF";
    case 128: return "SQCIF";
    }
    return "AUDIOONLY";
}

void PhoneUIBox::videoCifModeToRes(QString cifMode, int &w, int &h)
{
    w = 176;
    h = 144;
    if (cifMode == "QCIF")
    {
        w = 176;
        h = 144;
    }
    if (cifMode == "SQCIF")
    {
        w = 128;
        h = 96;
    }
    if (cifMode == "4CIF")
    {
        w = 704;
        h = 576;
    }
}

void PhoneUIBox::keyPressEvent(QKeyEvent *e)
{
    bool handled = false;
    QStringList actions;
    gContext->GetMainWindow()->TranslateKeyPress("Phone", e, actions);

    for (unsigned int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        handled = true;
    
        if (action == "1")
            keypadPressed('1');
        else if (action == "2")
            keypadPressed('2');
        else if (action == "3")
            keypadPressed('3');
        else if (action == "4")
            keypadPressed('4');
        else if (action == "5")
            keypadPressed('5');
        else if (action == "6")
            keypadPressed('6');
        else if (action == "7")
            keypadPressed('7');
        else if (action == "8")
            keypadPressed('8');
        else if (action == "9")
            keypadPressed('9');
        else if (action == "0")
            keypadPressed('0');
        else if (action == "HASH")
            keypadPressed('#');
        else if (action == "STAR")
            keypadPressed('*');
        else if (action == "MENU")
            MenuButtonPushed();
        // Volume controls
        else if (action == "VOLUMEDOWN") 
            changeVolume(false);
        else if (action == "VOLUMEUP")
            changeVolume(true);
        else if (action == "MUTE")
            toggleMute();

        else if (action == "FULLSCRN")
        {
            setUpdatesEnabled(fullScreen);
            if (fullScreen)
            {
                rxVideoArea = receivedWebcamArea->getScreenArea();
                updateForeground();
            }
            else
            {
                rxVideoArea = QRect(0, 0, screenwidth, screenheight);
                // Paint screen black in case video image does not fill it
                QPixmap Pixmap(screenwidth, screenheight);
                Pixmap.fill(Qt::black);
                bitBlt(this, 0, 0, &Pixmap);
            }
            fullScreen = !fullScreen;
        }

        // Video Controls
        else if ((action == "ZOOMOUT") && (VideoOn))
        {
            if (zoomFactor < 10)
            {
                zoomFactor++;
                zoomWidth  = ((wcWidth-128) * zoomFactor / 10) + 128;  // Zoom evenly with a minimum of 128 width
                zoomWidth  = (zoomWidth >> 3) << 3;                    // Make boundaries easier for scaling algorithm
                zoomHeight = ((wcHeight-96) * zoomFactor / 10) + 96;   // Zoom evenly with a minimum of 96 height
                zoomHeight = (zoomHeight >> 3) << 3;                   // Make boundaries easier for scaling algorithm
            }
        }
        else if ((action == "ZOOMIN") && (VideoOn))
        {
            if (zoomFactor > 0)
            {
                zoomFactor--;
                zoomWidth  = ((wcWidth-128) * zoomFactor / 10) + 128;  
                zoomWidth  = (zoomWidth >> 3) << 3;                    
                zoomHeight = ((wcHeight-96) * zoomFactor / 10) + 96;  
                zoomHeight = (zoomHeight >> 3) << 3;                   
            }
        }

        // Video pan options
        else if ((action == "LEFT") && (VideoOn) && ((!volume_status) || (volume_status->getOrder() == -1)))
        {
            if (wPan > -10)
                wPan--;
        }
        else if ((action == "RIGHT") && (VideoOn) && ((!volume_status) || (volume_status->getOrder() == -1)))
        {
            if (wPan < 10)
                wPan++;
        }
        else if ((action == "UP") && (VideoOn) && ((!volume_status) || (volume_status->getOrder() == -1)))
        {
            if (hPan > -10)
                hPan--;
        }
        else if ((action == "DOWN") && (VideoOn) && ((!volume_status) || (volume_status->getOrder() == -1)))
        {
            if (hPan < 10)
                hPan++;
        }

        else if ((action == "DOWN") && (volume_status) && (volume_status->getOrder() != -1)) // Changing volume
            changeVolumeControl(false);
        else if ((action == "UP") && (volume_status) && (volume_status->getOrder() != -1)) // Changing volume
            changeVolumeControl(true);
        else if ((action == "RIGHT") && (volume_status) && (volume_status->getOrder() != -1)) // Changing volume
            changeVolume(true);
        else if ((action == "LEFT") && (volume_status) && (volume_status->getOrder() != -1)) // Changing volume
            changeVolume(false);

        else if (action == "UP") 
            DirectoryList->moveUp();
        else if (action == "DOWN")
            DirectoryList->moveDown();
        else if (action == "LEFT")
            DirectoryList->popUp();
        else if (action == "RIGHT")
            DirectoryList->pushDown();
        else if ((action == "SELECT") && (State == SIP_IDLE))
        {
            SelectHit = true;
            DirectoryList->select();
        }
        else if (action == "REFRESH")
        {
            DirectoryList->syncCurrentWithActive();
            DirectoryList->forceLastBin();
            DirectoryList->refresh();
        }
        else if (action == "HANGUP")
        {
            HangUp();
        }
        else if (action == "ESCAPE")
        {
            HangUp(); // Just in case; to make sure we never leave a call active when we quit!
            handled = false;
        }
        else
            handled = false;
    }

    if (!handled)
        MythThemedDialog::keyPressEvent(e);
}


void PhoneUIBox::customEvent(QCustomEvent *event)
{
    switch ((int)event->type()) 
    {
    case WebcamEvent::FrameReady:
        {
            WebcamEvent *we = (WebcamEvent *)event;
            if (we->getClient() == localClient)
                DrawLocalWebcamImage();
            else if (we->getClient() == txClient)
                TransmitLocalWebcamImage();
        }
        break;

    case RtpEvent::RxVideoFrame:
        {
            ProcessRxVideoFrame();
        }
        break;

    case SipEvent::SipStateChange:
        {
            ProcessSipStateChange();
        }
        break;

    case SipEvent::SipNotification:
        {
            ProcessSipNotification();
        }
        break;

    }

    QWidget::customEvent(event);
}


void PhoneUIBox::PlaceorAnswerCall(QString url, QString name, QString Mode, bool onLocalLan)
{
    switch (State)
    {
    case SIP_IDLE:
        sipStack->PlaceNewCall(Mode, url, name, onLocalLan);

        // Add an entry into the Placed Calls dir
        if (currentCallEntry)
            delete currentCallEntry;
        currentCallEntry = new CallRecord(name, url, false, (QDateTime::currentDateTime()).toString());
        phoneUIStatusBar->updateMidCallCaller(((name != 0) && (name.length() > 0)) ? name : url);
        break;

    case SIP_ICONNECTING:
        sipStack->AnswerRingingCall(Mode, onLocalLan);
        break;
    default:
        break;
    }
}


void PhoneUIBox::keypadPressed(char k)
{
    if (rtpAudio)
    {
        rtpAudio->sendDtmf(k);
        if (k == '*')
            rtpAudio->PlayToneToSpeaker(toneDtmf[10]->getAudio(), toneDtmf[0]->getSamples());
        else if (k == '#')
            rtpAudio->PlayToneToSpeaker(toneDtmf[11]->getAudio(), toneDtmf[0]->getSamples());
        else 
            rtpAudio->PlayToneToSpeaker(toneDtmf[k-'0']->getAudio(), toneDtmf[0]->getSamples());
    }

    else if (State == SIP_IDLE)
    {
        doUrlPopup(k, true);
    }
}


void PhoneUIBox::HangUp()
{
    sipStack->HangupCall();
}


void PhoneUIBox::StartVideo(int lPort, QString remoteIp, int remoteVideoPort, int videoPayload, QString rxVidRes)
{
    videoCifModeToRes(rxVidRes, rxWidth, rxHeight);

    rtpVideo = new rtp (this, lPort, remoteIp, remoteVideoPort, videoPayload, -1, "", "", RTP_TX_VIDEO, RTP_RX_VIDEO);

    if (h263->H263StartEncoder(txWidth, txHeight, txFps) && 
        h263->H263StartDecoder(rxWidth, rxHeight))
    {
        txClient = webcam->RegisterClient(VIDEO_PALETTE_YUV420P, txFps, this);
        VideoOn = true;
    }
    else
    {
        h263->H263StopEncoder();
        h263->H263StopDecoder();
    }
}

void PhoneUIBox::StopVideo()
{
    if (VideoOn)
    {
        h263->H263StopEncoder();
        h263->H263StopDecoder();

        VideoOn = false;
    }
    if (txClient)
        webcam->UnregisterClient(txClient);
    txClient = 0;

    if (rtpVideo)
        delete rtpVideo;
    rtpVideo = 0;
}


void PhoneUIBox::ChangeVideoTxResolution()
{
    if (VideoOn)
    {
        h263->H263StopEncoder();
        h263->H263StartEncoder(txWidth, txHeight, txFps);
    }
}


void PhoneUIBox::ChangeVideoRxResolution()
{
    if (VideoOn)
    {
        h263->H263StopDecoder();
        h263->H263StartDecoder(rxWidth, rxHeight);
    }
}


void PhoneUIBox::MenuButtonPushed()
{
    if (State == SIP_CONNECTED) // In a call, show the adjust parameters menu
        showVolume(true);
    else
        doMenuPopup(); // Otherwise show the traditional menu
}


void PhoneUIBox::DrawLocalWebcamImage()
{
    unsigned char *rgb32Frame = webcam->GetVideoFrame(localClient);
    if (rgb32Frame != 0)
    {
        if (!fullScreen) // In full-screen mode local webcam does not get displayed, only remote webcam
        {
            // Digital Zoom/pan parameters
            int zx = (wcWidth-zoomWidth)/2;
            zx += (zx*wPan/10);
            zx = (zx >> 1) << 1; // Make sure its even
            int zy = (wcHeight-zoomHeight)/2;
            zy += (zy*hPan/10);
            zy = (zy >> 1) << 1; // Make sure its even
        
            QPixmap Pixmap;
            QImage ScaledImage;
        
            QImage Image(rgb32Frame, wcWidth, wcHeight, 32, (QRgb *)0, 0, QImage::LittleEndian);
        
            QRect puthere = localWebcamArea->getScreenArea();
            if (zoomFactor == 10) // No Zoom; just scale the webcam image to the local window size
            {
                ScaledImage = Image.scale(puthere.width(), puthere.height(), QImage::ScaleMin);
            }
            else
            {
                QImage zoomedImage = Image.copy(zx, zy, zoomWidth, zoomHeight);
                ScaledImage = zoomedImage.scale(puthere.width(), puthere.height(), QImage::ScaleMin);
            }
            
            // Draw the local webcam image
            Pixmap = ScaledImage;
            bitBlt(this, puthere.x(), puthere.y(), &Pixmap);
        }
        webcam->FreeVideoBuffer(localClient, rgb32Frame);
    }
}



void PhoneUIBox::TransmitLocalWebcamImage()
{
    unsigned char *yuvFrame = webcam->GetVideoFrame(txClient);
    if (yuvFrame != 0)
    {
        // If we are transmitting video, process the YUV image
        if (VideoOn && rtpVideo)
        {
            // Digital Zoom/pan parameters
            int zx = (wcWidth-zoomWidth)/2;
            zx += (zx*wPan/10);
            zx = (zx >> 1) << 1; // Make sure its even
            int zy = (wcHeight-zoomHeight)/2;
            zy += (zy*hPan/10);
            zy = (zy >> 1) << 1; // Make sure its even
    
            int encLen;
            if (zoomFactor == 10) // No Zoom; just scale the webcam image to the transmit size
            {
                scaleYuvImage(yuvFrame, wcWidth, wcHeight, txWidth, txHeight, yuvBuffer2);
            }
            else
            {
                cropYuvImage(yuvFrame, wcWidth, wcHeight, zx, zy, zoomWidth, zoomHeight, yuvBuffer1);
                scaleYuvImage(yuvBuffer1, zoomWidth, zoomHeight, txWidth, txHeight, yuvBuffer2);
            }
            uchar *encFrame = h263->H263EncodeFrame(yuvBuffer2, &encLen);
            VIDEOBUFFER *vb = rtpVideo->getVideoBuffer(encLen);
            if (vb)
            {
                if (encLen > (int)sizeof(vb->video))
                {
                    cout << "SIP: Encoded H.323 frame size is " << encLen << "; too big for buffer\n";
                    rtpVideo->freeVideoBuffer(vb);
                }
                else
                {
                    memcpy(vb->video, encFrame, encLen); // Optimisation to get rid of this copy may be possible, check H.263 stack
                    vb->len = encLen;
                    vb->w = txWidth;
                    vb->h = txHeight;
                    if (!rtpVideo->queueVideo(vb))
                    {
                        cout << "Could not queue RTP Video frame for transmission\n";
                        rtpVideo->freeVideoBuffer(vb);
                    }
                }
            }
        }
        webcam->FreeVideoBuffer(txClient, yuvFrame);
    }
}


void PhoneUIBox::ProcessRxVideoFrame()
{
    QPixmap Pixmap;
    QImage ScaledImage;
    VIDEOBUFFER *v;

    if (VideoOn && rtpVideo && (v = rtpVideo->getRxedVideo()))
    {
        if ((rxWidth != v->w) || (rxHeight != v->h))
        {
            cout << "SIP: Rx Image size changed from " << rxWidth << "x" << rxHeight << " to " << v->w << "x" << v->h << endl;
            rxWidth = v->w;
            rxHeight = v->h;
            ChangeVideoRxResolution();
        }

        uchar *decRgbFrame = h263->H263DecodeFrame(v->video, v->len, rxRgbBuffer, sizeof(rxRgbBuffer));
        if (decRgbFrame)
        {
            QImage rxImage(rxRgbBuffer, v->w, v->h, 32, (QRgb *)0, 0, QImage::LittleEndian);

            if ((v->w != rxVideoArea.width()) || (v->h != rxVideoArea.height()))
            {
                ScaledImage = rxImage.scale(rxVideoArea.width(), rxVideoArea.height(), QImage::ScaleMin);
                Pixmap = ScaledImage;
            }
            else
                Pixmap = rxImage;
            bitBlt(this, rxVideoArea.x(), rxVideoArea.y(), &Pixmap);

        }
        rtpVideo->freeVideoBuffer(v);
    }
}


void PhoneUIBox::ProcessSipStateChange()
{
    bool inAudioOnly;
    int OldState = State;

    // Poll the FSM for network events
    State = sipStack->GetSipState();

    // Handle state transitions
    if (State != OldState)
    {
        // Any change of state will cancel playing of ringback tone; and cancel playing of voicemails
        if (ringbackTone->Playing())
            ringbackTone->Stop();
        if (vmail)
            delete vmail;
        vmail = 0;

        // We were displaying the answer dialog, make sure its gone
        if (OldState == SIP_ICONNECTING)
            closeCallPopup();

        if (State == SIP_ICONNECTING)
        {
            QString callerUser, callerName, callerUrl, callerDisplay;
            sipStack->GetIncomingCaller(callerUser, callerName, callerUrl, inAudioOnly);

            if (callerName.length()>0)
                callerDisplay = callerName;
            else if (callerUser.length()>0)
                callerDisplay = callerUser;
            else 
                callerDisplay = "";

            // Show caller on status bar once connected
            phoneUIStatusBar->updateMidCallCaller(callerDisplay);

            // Add an entry into the Received Calls dir
            QDateTime now = QDateTime::currentDateTime();
            QString ts = now.toString();
            if (currentCallEntry)
                delete currentCallEntry;
            currentCallEntry = new CallRecord(callerName, callerUrl, true, ts);

            DirEntry *entry = DirContainer->FindMatchingDirectoryEntry(currentCallEntry->getUri());
            bool AutoanswerEnabled = gContext->GetNumSetting("SipAutoanswer",1);
            if (AutoanswerEnabled)
                PlaceorAnswerCall(entry->getUri(), entry->getNickName(), txVideoMode, true);
            else
            {
                // Popup the caller's details
                closeCallPopup(); // Check we were not displaying one (e.g. user was getting ready to dial)
                if (entry)
                    doCallPopup(entry, "Answer", inAudioOnly);
                else
                {
                    DirEntry dummyEntry(callerDisplay, callerUrl, "", "", "");
                    doCallPopup(&dummyEntry, "Answer", inAudioOnly);
                }
            }
        }
        else if (State == SIP_IDLE)
        {
            if (currentCallEntry)
            {
                currentCallEntry->setDuration(ConnectTime);
                DirContainer->AddToCallHistory(currentCallEntry, true);
                DirectoryList->refresh();
            }
            currentCallEntry = 0;
            ConnectTime = 0;
        }
        else if (State == SIP_CONNECTED)
        {
            OnScreenClockTimer->start(1000);
            phoneUIStatusBar->DisplayInCallStats(true);
            startRTP();
        }

        if (OldState == SIP_CONNECTED) // Disconnecting
        {
            OnScreenClockTimer->stop();
            // Stop the RTP connection
            if (rtpAudio != 0)
            {
                powerDispTimer->stop();
                micAmplitude->setRepeat(0);
                spkAmplitude->setRepeat(0);
                delete rtpAudio;
                rtpAudio = 0;
            }
            else
                cerr << "RTP device was not open\n";
            if (rtpVideo != 0)
                StopVideo();
        }

        switch(State)
        {
        case SIP_IDLE:           phoneUIStatusBar->DisplayCallState("No Active Calls");                break;
        case SIP_CONNECTED:      break;
        case SIP_CONNECTED_VXML: phoneUIStatusBar->DisplayCallState("Caller is Leaving Voicemail");    break;
        case SIP_OCONNECTING1:   phoneUIStatusBar->DisplayCallState("Trying to Contact Remote Party"); break;
        case SIP_ICONNECTING:    phoneUIStatusBar->DisplayCallState("Incoming Call");                  break;
        case SIP_DISCONNECTING:  phoneUIStatusBar->DisplayCallState("Hanging Up");                     break;
        case SIP_OCONNECTING2:   // Shows notification from Sip Stack, e.g. Trying or Ringing
        default:                 
            break;
        }
    }
}


void PhoneUIBox::ProcessSipNotification()
{
    // If the SIP stack has something to tell the user, then display that first
    QString NotifyType, NotifyUrl, NotifyParam1, NotifyParam2;
    while (sipStack->GetNotification(NotifyType, NotifyUrl, NotifyParam1, NotifyParam2))
    {
        // See if the notification is a received STATUS messages in response to making a call
        if (NotifyType == "CALLSTATUS")
        {
            switch (atoi(NotifyParam1))
            {
            case 0:
                break;
    
            case 180: // 180 Ringing
                {
                    QString spk = gContext->GetSetting("AudioOutputDevice");
                    ringbackTone->Play(spk, true);
                }

                // fall through
    
            default:
                phoneUIStatusBar->DisplayCallState(NotifyParam2);
                break;
            }
        }

        // See if the notification is a change in presence status of a remote client
        else if (NotifyType == "PRESENCE")
        {
            int newStatus = ICON_PRES_UNKNOWN;
            if (NotifyParam1 == "offline")
                newStatus = ICON_PRES_OFFLINE;
            else if (NotifyParam1 == "open")
                newStatus = ICON_PRES_ONLINE;
            else if (NotifyParam1 == "inactive")
                newStatus = ICON_PRES_AWAY;
            DirContainer->ChangePresenceStatus(NotifyUrl, newStatus, NotifyParam2, true);
            DirectoryList->refresh();
        }

        // See if the notification is an incoming IM message
        else if (NotifyType == "IM")
        {
            doIMPopup(NotifyUrl, NotifyParam1, NotifyParam2);
        }

        else 
            cerr << "SIP: Unknown Notify type " << NotifyType << endl;
    }
}


void PhoneUIBox::OnScreenClockTick()
{
    int pIn=0, pMiss=0, pLate=0, bIn=0, bOut=0, bPlayed=0, pOut = 0;        
    int bvIn=0, bvOut=0;
    if (rtpAudio)
    {
        ConnectTime++; 
        phoneUIStatusBar->updateMidCallTime(ConnectTime);
    
        rtpAudio->getRxStats(pIn, pMiss, pLate, bIn, bPlayed, bOut);
        rtpAudio->getTxStats(pOut);
        phoneUIStatusBar->updateMidCallAudioStats(pIn, pMiss, pLate, pOut);
    }

    if (rtpVideo)
    {
        pIn=0, pMiss=0, pLate=0, bPlayed=0, pOut = 0;        
        rtpVideo->getRxStats(pIn, pMiss, pLate, bvIn, bPlayed, bvOut);
        rtpVideo->getTxStats(pOut);
        phoneUIStatusBar->updateMidCallVideoStats(pIn, pMiss, pLate, pOut);
        bIn += bvIn;
        bOut += bvOut;
    }

    phoneUIStatusBar->updateMidCallBandwidth(bIn, bOut);
}


void PhoneUIBox::startRTP()
{
    if ((rtpAudio == 0) && (rtpVideo == 0))
    {
        QString remoteIp;
        int remoteAudioPort, remoteVideoPort;
        int audioPayload, videoPayload, dtmfPayload;
        QString audioCodec, videoCodec, rxVideoResolution;
        sipStack->GetSipSDPDetails(remoteIp, remoteAudioPort, audioPayload, audioCodec, dtmfPayload, remoteVideoPort, videoPayload, videoCodec, rxVideoResolution);
        int laPort = atoi((const char *)gContext->GetSetting("AudioLocalPort"));
        int lvPort = atoi((const char *)gContext->GetSetting("VideoLocalPort"));
        QString spk = gContext->GetSetting("AudioOutputDevice");
        QString mic = gContext->GetSetting("MicrophoneDevice");
        rtpAudio = new rtp (this, laPort, remoteIp, remoteAudioPort, audioPayload, dtmfPayload, mic, spk);
        phoneUIStatusBar->updateMidCallAudioCodec(audioCodec);
        powerDispTimer->start(100);
        if (videoPayload != -1)
        {
            StartVideo(lvPort, remoteIp, remoteVideoPort, videoPayload, rxVideoResolution);
            phoneUIStatusBar->updateMidCallVideoCodec(videoCodec);
        }
    }
    else
        cerr << "RTP device left open\n";
}






void PhoneUIBox::doMenuPopup()
{
    if (menuPopup)
        return;

    GenericTree *Current = DirectoryList->getCurrentNode();
    if (Current == 0)
    {
        cerr << "Mythphone: Can't get your context\n";
        return;
    }

    int selType = Current->getAttribute(0);

    menuPopup = new MythPopupBox(gContext->GetMainWindow(), "MENU_popup");

    QButton *b1 = 0;

    switch (selType)
    {
    case TA_DIR:
    case TA_VMAIL:
        menuPopup->addLabel("Directory", MythPopupBox::Large);
        b1 = menuPopup->addButton(tr("Add someone to your Directory "), this, SLOT(menuAddContact()));
        break;
    case TA_SPEEDDIALENTRY:
        menuPopup->addLabel("Speed Dials", MythPopupBox::Large);
        b1 = menuPopup->addButton(tr("Edit this Entry"), this, SLOT(menuEntryEdit()));
        menuPopup->addButton(tr("Remove from Speed Dials"), this, SLOT(menuSpeedDialRemove()));
        menuPopup->addButton(tr("Add someone to your Directory "), this, SLOT(menuAddContact()));
        break;
    case TA_CALLHISTENTRY:
        menuPopup->addLabel("Call History", MythPopupBox::Large);
        b1 = menuPopup->addButton(tr("Save this in the Directory"), this, SLOT(menuHistorySave()));
        menuPopup->addButton(tr("Clear the Call History"), this, SLOT(menuHistoryClear()));
        break;
    case TA_DIRENTRY:
        menuPopup->addLabel("Directory", MythPopupBox::Large);
        b1 = menuPopup->addButton(tr("Edit this Entry"), this, SLOT(menuEntryEdit()));
        menuPopup->addButton(tr("Make this a Speeddial"), this, SLOT(menuEntryMakeSpeedDial()));
        menuPopup->addButton(tr("Delete this Entry"), this, SLOT(menuEntryDelete()));
        menuPopup->addButton(tr("Add someone to your Directory "), this, SLOT(menuAddContact()));
        break;
    case TA_VMAIL_ENTRY:
        menuPopup->addLabel("Voicemail", MythPopupBox::Large);
        b1 = menuPopup->addButton(tr("Delete this Voicemail"), this, SLOT(vmailEntryDelete()));
        menuPopup->addButton(tr("Delete all Voicemails"), this, SLOT(vmailEntryDeleteAll()));
        break;
    default:
        delete menuPopup;
        menuPopup = NULL;
        return;
        break;
    }

    menuPopup->ShowPopupAtXY(180, 40, this, SLOT(closeMenuPopup()));
    if (b1)
        b1->setFocus();
}


void PhoneUIBox::menuCallUrl()
{
    doUrlPopup(0, true);
}

void PhoneUIBox::menuAddContact()
{
    doAddEntryPopup(0);
}

void PhoneUIBox::menuSpeedDialRemove()
{
    GenericTree *Current = DirectoryList->getCurrentNode();
    if (Current != 0)
    {
        DirEntry *Entry = DirContainer->fetchDirEntryById(Current->getAttribute(1));
        if (Entry != 0)
        {
            if (Entry->isSpeedDial())
            {
                DirectoryList->popUp();
                DirContainer->removeSpeedDial(Entry);
                DirectoryList->refresh();
            }
        }
        else
            cerr << "mythphone: Error finding your directory entry\n";
    }
    else
        cerr << "mythphone: Error getting info from the tree\n";
    closeMenuPopup();
}

void PhoneUIBox::menuHistorySave()
{
    GenericTree *Current = DirectoryList->getCurrentNode();
    if (Current != 0)
    {
        CallRecord *crEntry = DirContainer->fetchCallRecordById(Current->getAttribute(1));
        DirEntry *entry = DirContainer->FindMatchingDirectoryEntry(crEntry->getUri());
        if (crEntry != 0)
        {
            if (entry != 0)
            {
                // Tell the user one exists
                DialogBox *NoDeviceDialog = new DialogBox(gContext->GetMainWindow(),
                                                  QObject::tr("\n\nA directory entry already exists with this URL."));
                NoDeviceDialog->AddButton(QObject::tr("OK"));
                NoDeviceDialog->exec();
                delete NoDeviceDialog;
                closeMenuPopup();
            }
            else
                doAddEntryPopup(0, crEntry->getDisplayName(), crEntry->getUri());
        }
        else
            cerr << "mythphone: Error finding your call history entry\n";
    }
    else
        cerr << "mythphone: Error getting info from the tree\n";
}


void PhoneUIBox::menuHistoryClear()
{
    // If we are highlighting a call history entry, move up the tree
    // before we delete it
    GenericTree *Current = DirectoryList->getCurrentNode();
    int selType = Current->getAttribute(0);
    if (selType == TA_CALLHISTENTRY)
        DirectoryList->popUp();

    DirContainer->clearCallHistory();
    DirectoryList->refresh();
    closeMenuPopup();
}


void PhoneUIBox::menuEntryEdit()
{
    GenericTree *Current = DirectoryList->getCurrentNode();
    if (Current != 0)
    {
        DirEntry *Entry = DirContainer->fetchDirEntryById(Current->getAttribute(1));
        if (Entry != 0)
        {
            doAddEntryPopup(Entry);
        }
        else
            cerr << "mythphone: Error finding your directory entry\n";
    }
    else
        cerr << "mythphone: Error getting info from the tree\n";
}

void PhoneUIBox::menuEntryMakeSpeedDial()
{
    GenericTree *Current = DirectoryList->getCurrentNode();
    if (Current != 0)
    {
        DirEntry *Entry = DirContainer->fetchDirEntryById(Current->getAttribute(1));
        if (Entry != 0)
        {
            if (!Entry->isSpeedDial())
            {
                DirContainer->setSpeedDial(Entry);
                DirectoryList->refresh();
            }
        }
        else
            cerr << "mythphone: Error finding your directory entry\n";
    }
    else
        cerr << "mythphone: Error getting info from the tree\n";
    closeMenuPopup();
}

void PhoneUIBox::menuEntryDelete()
{
    GenericTree *Current = DirectoryList->getCurrentNode();
    if (Current != 0)
    {
        DirEntry *Entry = DirContainer->fetchDirEntryById(Current->getAttribute(1));
        if (Entry != 0)
        {
            DirectoryList->popUp();
            DirContainer->deleteFromTree(Current, Entry);
            DirectoryList->refresh();
        }
        else
            cerr << "mythphone: Error finding your directory entry\n";
    }
    else
        cerr << "mythphone: Error getting info from the tree\n";
    closeMenuPopup();
}

void PhoneUIBox::vmailEntryDelete()
{
    // If we are highlighting a voicemail entry, move up the tree
    // before we delete it
    GenericTree *Current = DirectoryList->getCurrentNode();
    int selType = Current->getAttribute(0);
    QString vmailName = Current->getString();
    if (selType == TA_VMAIL_ENTRY)
        DirectoryList->popUp();

    DirContainer->deleteVoicemail(vmailName);
    DirectoryList->refresh();
    closeMenuPopup();
}

void PhoneUIBox::vmailEntryDeleteAll()
{
    // If we are highlighting a voicemail entry, move up the tree
    // before we delete it
    GenericTree *Current = DirectoryList->getCurrentNode();
    int selType = Current->getAttribute(0);
    if (selType == TA_VMAIL_ENTRY)
        DirectoryList->popUp();

    DirContainer->clearAllVoicemail();
    DirectoryList->refresh();
    closeMenuPopup();
}

void PhoneUIBox::closeMenuPopup()
{
    if (!menuPopup)
        return;

    menuPopup->hide();
    delete menuPopup;
    menuPopup = NULL;
}

void PhoneUIBox::doUrlPopup(char key, bool digitsOrUrl)
{
    if (urlPopup)
        return;

    QString text = "";
    if (key)
        text += key;

    urlPopup = new MythPopupBox(gContext->GetMainWindow(), "URL_popup");
    if (digitsOrUrl)
    {
        urlField = new MythLineEdit(urlPopup);
        urlRemoteField = 0;
        urlPopup->addWidget(urlField);
        urlField->setText(text);
        urlField->setCursorPosition(text.length());
        urlField->setFocus();
    }
    else
    {
        urlRemoteField = new MythRemoteLineEdit(urlPopup);
        urlField = 0;
        urlPopup->addWidget(urlRemoteField);
        urlRemoteField->setFocus();
    }
    urlPopup->addButton(tr("Place Videocall Now"), this, SLOT(dialUrlVideo()));
    urlPopup->addButton(tr("Place Voice-Only Call Now"), this, SLOT(dialUrlVoice()));
    if (digitsOrUrl)
        urlPopup->addButton(tr("Switch from digits to URL input"), this, SLOT(dialUrlSwitchToUrl()));
    else
        urlPopup->addButton(tr("Switch from URL to Digits input"), this, SLOT(dialUrlSwitchToDigits()));

    urlPopup->ShowPopupAtXY(200, 60, this, SLOT(closeUrlPopup()));
}

void PhoneUIBox::closeUrlPopup()
{
    if (!urlPopup)
        return;

    urlPopup->hide();
    delete urlPopup;
    urlPopup = NULL;
}

void PhoneUIBox::dialUrlVideo()
{
    PlaceorAnswerCall(urlField==0 ? urlRemoteField->text() : urlField->text(), "", txVideoMode);
    closeUrlPopup();                           

    // If we got here via the menu popup
    // then close that now - we're finiished with it
    if (menuPopup)
        closeMenuPopup();
}


void PhoneUIBox::dialUrlVoice()
{
    PlaceorAnswerCall(urlField==0 ? urlRemoteField->text() : urlField->text(), "", "AUDIOONLY");
    closeUrlPopup();

    // If we got here via the menu popup
    // then close that now - we're finiished with it
    if (menuPopup)
        closeMenuPopup();
}

void PhoneUIBox::dialUrlSwitchToDigits()
{
    closeUrlPopup();
    doUrlPopup(0, true);
}

void PhoneUIBox::dialUrlSwitchToUrl()
{
    closeUrlPopup();
    doUrlPopup(0, false);
}

void PhoneUIBox::doIMPopup(QString otherParty, QString callId, QString Msg)
{
    if (imPopup)
        scrollIMText(Msg, true);
    else
    {
        imPopup = new MythPopupBox(gContext->GetMainWindow(), "IM_popup");
        QLabel *title = imPopup->addLabel("IM: "+otherParty, MythPopupBox::Medium);
        title->setAlignment(Qt::AlignHCenter);
        for (int i=0; i<MAX_DISPLAY_IM_MSGS; i++)
            imLine[i] = imPopup->addLabel("", MythPopupBox::Small, true);
        displayedIMMsgs = 0;
        if (callId.length() > 0)
        {
            imCallid = callId;
            scrollIMText(Msg, true);
        }
        else
            imCallid = "";
        imUrl = otherParty;
        imReplyField = new MythRemoteLineEdit(imPopup);
        imPopup->addWidget(imReplyField);
        imReplyField->setFocus();
        imPopup->addButton(tr("Send IM"), this, SLOT(imSendReply()));
        
        imPopup->ShowPopupAtXY(200, 100, this, SLOT(closeIMPopup()));
    }
}

void PhoneUIBox::scrollIMText(QString Msg, bool msgReceived)
{
    if (imPopup)
    {
        // See if we need to scroll
        if (displayedIMMsgs >= MAX_DISPLAY_IM_MSGS)
        {
            for (int i=0;i<displayedIMMsgs-1;i++)
            {
                imLine[i]->setPaletteForegroundColor(imLine[i+1]->paletteForegroundColor());
                imLine[i]->setText(imLine[i+1]->text());
            }
        }
        else
            displayedIMMsgs++;
        
        // Display latest msg 
        if (msgReceived)
            imLine[displayedIMMsgs-1]->setPaletteForegroundColor(Qt::white);
        else
            imLine[displayedIMMsgs-1]->setPaletteForegroundColor(Qt::yellow);
        imLine[displayedIMMsgs-1]->setText(Msg);
    }
}

void PhoneUIBox::closeIMPopup()
{
    if (!imPopup)
        return;

    imPopup->hide();
    delete imPopup;
    imPopup = NULL;
}

void PhoneUIBox::imSendReply()
{
    if (!imPopup)
        return;
    imCallid = sipStack->UiSendIMMessage(imUrl, imCallid, imReplyField->text());
    scrollIMText(imReplyField->text(), false);
    imReplyField->setText("");
    imReplyField->setFocus();
}


void PhoneUIBox::doAddEntryPopup(DirEntry *edit, QString nn, QString Url)
{
    if (addEntryPopup)
        return;

    addEntryPopup = new MythPopupBox(gContext->GetMainWindow(), "add_entry_popup");

    if (edit == 0) // If editing, currently we don't allow fields to change that will affect the displayed tree structure
    {
        addEntryPopup->addLabel("Nickname", MythPopupBox::Small);
        entryNickname = new MythRemoteLineEdit(addEntryPopup);
        addEntryPopup->addWidget(entryNickname);
    }
    else
    {
        entryNickname = 0;
        addEntryPopup->addLabel(edit->getNickName(), MythPopupBox::Large);
    }

    addEntryPopup->addLabel("First Name (Optional)", MythPopupBox::Small);
    entryFirstname = new MythRemoteLineEdit(addEntryPopup);
    addEntryPopup->addWidget(entryFirstname);

    addEntryPopup->addLabel("Surname (Optional)", MythPopupBox::Small);
    entrySurname = new MythRemoteLineEdit(addEntryPopup);
    addEntryPopup->addWidget(entrySurname);

    addEntryPopup->addLabel("URL", MythPopupBox::Small);
    entryUrl = new MythRemoteLineEdit(addEntryPopup);
    addEntryPopup->addWidget(entryUrl);

    if (edit == 0) // If editing, currently we don't allow fields to change that will affect the displayed tree structure
    {
        entrySpeed = new MythCheckBox(addEntryPopup);
        entrySpeed->setText("Speed Dial");
        addEntryPopup->addWidget(entrySpeed);
    }

    entryOnHomeLan = new MythCheckBox(addEntryPopup);
    entryOnHomeLan->setText("Client is on My Home LAN");
    addEntryPopup->addWidget(entryOnHomeLan);

#ifdef PHOTO
    entryPhoto = new MythComboBox(false, addEntryPopup);
    addEntryPopup->addLabel("Default Photo", MythPopupBox::Small);
    addEntryPopup->addWidget(entryPhoto);
#endif

    if (edit == 0) // If editing, currently we don't allow fields to change that will affect the displayed tree structure
    {
        addEntryPopup->addLabel("To Directory", MythPopupBox::Small);
        entryDir = new MythComboBox(false, addEntryPopup);
        addEntryPopup->addWidget(entryDir);
    }

    if (edit != 0)
        addEntryPopup->addButton(tr("Save Changes"), this, SLOT(entryAddSelected()));
    else
        addEntryPopup->addButton(tr("ADD"), this, SLOT(entryAddSelected()));

    // This shouldnt be needed but for some reason the
    // window is too short
    addEntryPopup->addLabel("", MythPopupBox::Small);

    addEntryPopup->ShowPopupAtXY(220, 20, this, SLOT(closeAddEntryPopup()));

#ifdef PHOTO
    // Fill the photos from the directory
    QDir photos(DEFAULTTHEMEPATH, "mp_photo-*", QDir::Name, QDir::Files);
    const QFileInfoList *il = photos.entryInfoList();
    int PhotoIndex = 0;
    if (il)
    {
        QFileInfoListIterator it(*il);
        QFileInfo *fi;
        for (int i=0; (fi = it.current()) != 0; ++it, i++)
        {
            QString PhotoFile = fi->baseName();
            PhotoFile = PhotoFile.mid(9); // Remove "mp_photo-"
            entryPhoto->insertItem(PhotoFile);
            if ((edit) && (PhotoFile == edit->getPhotoFile()))
                PhotoIndex = i;
        }
    }
    entryPhoto->setCurrentItem(PhotoIndex);
#endif

    // Set defaults and fill out listboxes
    if (edit == 0)
    {
        QStrList DirList = DirContainer->getDirectoryList();
        DirList.append("new");
        entryDir->insertStrList(DirList);
    }

    if (edit == 0)
    {
        entryNickname->setText(nn);
        entryFirstname->setText("");
        entrySurname->setText("");
        entryUrl->setText(Url);
        entryNickname->setFocus();
    }
    else
    {
        entryFirstname->setText(edit->getFirstName());
        entrySurname->setText(edit->getSurname());
        entryUrl->setText(edit->getUri());
        entryOnHomeLan->setChecked(edit->getOnHomeLan());
        entryFirstname->setFocus();
    }

    EntrytoEdit = edit;
}

void PhoneUIBox::closeAddEntryPopup()
{
    if (!addEntryPopup)
        return;

    addEntryPopup->hide();
    delete addEntryPopup;
    addEntryPopup = NULL;
}



void PhoneUIBox::entryAddSelected()
{
    if (EntrytoEdit == 0) // Add
    {
        QString Directory = entryDir->currentText();
        if (Directory == "new")
        {
            doAddDirectoryPopup();
            return;
        }
        else
            addNewDirectoryEntry(entryNickname->text(),
                                 entryUrl->text(),
                                 entryDir->currentText(),
                                 entryFirstname->text(),
                                 entrySurname->text(),
                                 "",
                                 entrySpeed->isChecked(),
                                 entryOnHomeLan->isChecked());
    }
    else // Edit
        addNewDirectoryEntry(0,
                            entryUrl->text(),
                            0,
                            entryFirstname->text(),
                            entrySurname->text(),
                            "",
                            false,
                            entryOnHomeLan->isChecked());

    closeAddEntryPopup();
    // If we got here via the menu popup
    // then close that now - we're finiished with it
    if (menuPopup)
        closeMenuPopup();
}



void PhoneUIBox::addNewDirectoryEntry(QString Name, QString Url, QString Dir, QString fn, QString sn, QString ph, bool isSpeed, bool OnHomeLan)
{

    if (EntrytoEdit == 0)
    {
        // First add to DB, which will automatically add the DIR if needed
        DirEntry *newEntry = new DirEntry(Name, Url, fn, sn, ph, OnHomeLan);
        newEntry->setSpeedDial(isSpeed);
        DirContainer->AddEntry(newEntry, Dir, true);
        DirectoryList->refresh();
        // TODO ... before this, make sure it doesn't already
        // exist.
    }

    // EDITing an existing entry
    else
    {
        DirContainer->ChangeEntry(EntrytoEdit, Name, Url, fn, sn, ph, OnHomeLan);
        DirectoryList->refresh();
    }

}




void PhoneUIBox::doAddDirectoryPopup()
{
    if (addDirectoryPopup)
        return;

    addDirectoryPopup = new MythPopupBox(gContext->GetMainWindow(), "add_directory_popup");

    newDirName = new MythRemoteLineEdit(addDirectoryPopup);
    addDirectoryPopup->addWidget(newDirName);
    addDirectoryPopup->addButton(tr("ADD DIRECTORY"), this, SLOT(directoryAddSelected()));

    addDirectoryPopup->ShowPopupAtXY(240, 90, this, SLOT(closeAddDirectoryPopup()));
    newDirName->setFocus();
}

void PhoneUIBox::closeAddDirectoryPopup()
{
    if (!addDirectoryPopup)
        return;

    addDirectoryPopup->hide();
    delete addDirectoryPopup;
    addDirectoryPopup = NULL;
}



void PhoneUIBox::directoryAddSelected()
{
    if (!addDirectoryPopup)
        return;

    // If we got here via adding a new directory entry
    // then carry on with what we were doing.
    if (addEntryPopup)
    {
        if (EntrytoEdit == 0) // Add
            addNewDirectoryEntry(entryNickname->text(),
                                 entryUrl->text(),
                                 newDirName->text(),
                                 entryFirstname->text(),
                                 entrySurname->text(),
                                 "",
                                 entrySpeed->isChecked(),
                                 entryOnHomeLan->isChecked());
        else // Edit
            addNewDirectoryEntry(0,
                                 entryUrl->text(),
                                 0,
                                 entryFirstname->text(),
                                 entrySurname->text(),
                                 "",
                                 false,
                                 entryOnHomeLan->isChecked());
        closeAddDirectoryPopup();
        closeAddEntryPopup();
        closeMenuPopup();
    }
    else
        closeAddDirectoryPopup();

    // If we got here via the URL entry popup
    // then carry close that too
    if (urlPopup)
        closeUrlPopup();
}


void PhoneUIBox::doCallPopup(DirEntry *entry, QString DialorAnswer, bool audioOnly)
{
    if (!incallPopup)
    {
        incallPopup = new MythPopupBox(gContext->GetMainWindow(), "Business Card");
    
        callLabelName = incallPopup->addLabel(entry->getNickName(), MythPopupBox::Large);
        incallPopup->addLabel(entry->getFullName());
        callLabelUrl = incallPopup->addLabel(entry->getUri());

        entryIsOnLocalLan = entry->getOnHomeLan();
    
        // Get an ordered list of calls to/from this person
        CallHistory RecentCalls;
        DirContainer->getRecentCalls(entry, RecentCalls);
        if (RecentCalls.count() > 0)
        {
            incallPopup->addLabel("Latest Calls:", MythPopupBox::Small);
            drawCallPopupCallHistory(incallPopup, RecentCalls.last());
            drawCallPopupCallHistory(incallPopup, RecentCalls.prev());
            drawCallPopupCallHistory(incallPopup, RecentCalls.prev());
        }
    
        if (!audioOnly)
        {
            QButton *button1 = incallPopup->addButton(DialorAnswer + " Videocall", this, SLOT(incallDialVideoSelected()));
            button1->setFocus();
        }
        QButton *button2 = incallPopup->addButton(DialorAnswer + " Voice-Only", this, SLOT(incallDialVoiceSelected()));
        if (DialorAnswer == "Dial")
            incallPopup->addButton("Send an Instant Message", this, SLOT(incallSendIMSelected()));
        
        if (audioOnly)
            button2->setFocus();
        incallPopup->ShowPopup(this, SLOT(closeCallPopup()));
    }
}


void PhoneUIBox::closeCallPopup()
{
    if (!incallPopup)
        return;

    incallPopup->hide();
    delete incallPopup;
    incallPopup = NULL;
}

void PhoneUIBox::incallDialVideoSelected()
{
    PlaceorAnswerCall(callLabelUrl->text(), callLabelName->text(), txVideoMode, entryIsOnLocalLan);
    closeCallPopup();
}
 
void PhoneUIBox::incallDialVoiceSelected()
{
    PlaceorAnswerCall(callLabelUrl->text(), callLabelName->text(), "AUDIOONLY", entryIsOnLocalLan);
    closeCallPopup();
}

void PhoneUIBox::incallSendIMSelected()
{
    QString OtherParty = callLabelUrl->text();
    closeCallPopup();
    doIMPopup(OtherParty, "", "");
}
 

void PhoneUIBox::drawCallPopupCallHistory(MythPopupBox *popup, CallRecord *call)
{
    if ((call) && (call->getTimestamp().length() > 0))
    {
        QString label;

        if (!call->isIncoming())
            label = "You Called ";
        else if (call->getDuration() != 0)
            label = "They Called ";
        else
            label = "You missed their call ";

        QDateTime dt = QDateTime::fromString(call->getTimestamp());
        if (dt.date() == QDateTime::currentDateTime().date())
            label += "Today ";
        else if (dt.date().addDays(1) == QDateTime::currentDateTime().date())
            label += "Yesterday ";
        else
            label += dt.toString("dd-MMM ");
        label += "at ";
        label += dt.toString("hh:mm");
        if (call->getDuration() > 0)
        {
            QString Duration;
            Duration.sprintf(" for %d min", call->getDuration()/60);
            label += Duration;
        }
        popup->addLabel(label);
    }
}

void PhoneUIBox::changeVolume(bool up_or_down)
{
    if (volume_control)
    {
        switch (VolumeMode)
        {
        default:
        case VOL_VOLUME:
            volume_control->AdjustCurrentVolume(up_or_down ? 2 : -2);
            break;
        case VOL_MICVOLUME:
            break;
        case VOL_BRIGHTNESS:
            camBrightness += (up_or_down ? 2048 : -2048);
            if (camBrightness > 65535)
                camBrightness = 65535;
            if (camBrightness < 0)
                camBrightness = 0;
            camBrightness = webcam->SetBrightness(camBrightness);
            break;
        case VOL_CONTRAST:
            camContrast += (up_or_down ? 2048 : -2048);
            if (camContrast > 65535)
                camContrast = 65535;
            if (camContrast < 0)
                camContrast = 0;
            camContrast = webcam->SetContrast(camContrast);
            break;
        case VOL_COLOUR:
            camColour += (up_or_down ? 2048 : -2048);
            if (camColour > 65535)
                camColour = 65535;
            if (camColour < 0)
                camColour = 0;
            camColour = webcam->SetColour(camColour);
            break;
        case VOL_TXSIZE:
            switch (txWidth)
            {
            case 704: 
                txWidth = (up_or_down ? 704 : 352);
                txHeight = (up_or_down ? 576 : 288);
                break;
            default:
            case 352: 
                txWidth = (up_or_down ? 704 : 176);
                txHeight = (up_or_down ? 576 : 144);
                break;
            case 176: 
                txWidth = (up_or_down ? 352 : 128);
                txHeight = (up_or_down ? 288 : 96);
                break;
            case 128: 
                txWidth = (up_or_down ? 176 : 128);
                txHeight = (up_or_down ? 144 : 96);
                break;
            }
            txVideoMode = videoResToCifMode(txWidth);
            ChangeVideoTxResolution();
            break;
        case VOL_TXRATE:
            txFps += (up_or_down ? 1 : -1);
            if (txFps > 30)
                txFps = 30;
            if (txFps < 1)
                txFps = 1;
            //webcam->SetTargetFps(txFps);
            break;
        }
        showVolume(true);
    }
}

void PhoneUIBox::changeVolumeControl(bool up_or_down)
{
    if ((volume_control) && (volume_status) && (volume_status->getOrder() != -1))
    {
        // Currently only volume & brightness but need to add mic-volume, hue etc
        switch (VolumeMode)
        {
        default:
        case VOL_VOLUME:     VolumeMode = (up_or_down ? VOL_MICVOLUME : VOL_TXRATE);    break;
        case VOL_MICVOLUME:  VolumeMode = (up_or_down ? VOL_BRIGHTNESS : VOL_VOLUME);   break;
        case VOL_BRIGHTNESS: VolumeMode = (up_or_down ? VOL_CONTRAST : VOL_MICVOLUME);  break;
        case VOL_CONTRAST:   VolumeMode = (up_or_down ? VOL_COLOUR : VOL_BRIGHTNESS);   break;
        case VOL_COLOUR:     VolumeMode = (up_or_down ? VOL_TXSIZE : VOL_CONTRAST);     break;
        case VOL_TXSIZE:     VolumeMode = (up_or_down ? VOL_TXRATE : VOL_COLOUR);       break;
        case VOL_TXRATE:     VolumeMode = (up_or_down ? VOL_VOLUME : VOL_TXSIZE);       break;
        }

        switch (VolumeMode)
        {
        default:
        case VOL_VOLUME:     volume_icon->SetImage(DEFAULTTHEMEPATH "mp_volume_icon.png");     
                             volume_setting->SetText("Volume");
                             volume_value->SetText("");
                             break;
        case VOL_MICVOLUME:  volume_icon->SetImage(DEFAULTTHEMEPATH "mp_microphone_icon.png"); 
                             volume_setting->SetText("Mic Volume (not impl.)");
                             volume_value->SetText("");
                             break;
        case VOL_BRIGHTNESS: volume_icon->SetImage(DEFAULTTHEMEPATH "mp_brightness_icon.png"); 
                             volume_setting->SetText("Brightness");
                             volume_value->SetText("");
                             break;
        case VOL_CONTRAST:   volume_icon->SetImage(DEFAULTTHEMEPATH "mp_contrast_icon.png");   
                             volume_setting->SetText("Contrast");
                             volume_value->SetText("");
                             break;
        case VOL_COLOUR:     volume_icon->SetImage(DEFAULTTHEMEPATH "mp_colour_icon.png");     
                             volume_setting->SetText("Colour");
                             volume_value->SetText("");
                             break;
        case VOL_TXSIZE:     volume_icon->SetImage(DEFAULTTHEMEPATH "mp_framesize_icon.png");  
                             volume_setting->SetText("Transmit Video Size");
                             volume_value->SetText(getVideoFrameSizeText());
                             break;
        case VOL_TXRATE:     volume_icon->SetImage(DEFAULTTHEMEPATH "mp_framerate_icon.png");  
                             volume_setting->SetText("Transmit Video FPS");
                             volume_value->SetText(QString::number(txFps));
                             break;
        }

        volume_icon->LoadImage();
        showVolume(true);
    }
}

void PhoneUIBox::toggleMute()
{
    if (rtpAudio)
        rtpAudio->toggleMute();
}

QString PhoneUIBox::getVideoFrameSizeText()
{
    QString fsText = QString::number(txWidth) + "x" + QString::number(txHeight);
    return fsText;
}

void PhoneUIBox::showVolume(bool on_or_off)
{
    if (volume_control)
    {
        if (volume_status)
        {
            if (on_or_off)
            {
                switch (VolumeMode)
                {
                case VOL_VOLUME:
                default:
                    volume_status->SetUsed(volume_control->GetCurrentVolume());
                    break;
                case VOL_MICVOLUME:
                    volume_status->SetUsed(50);
                    break;
                case VOL_BRIGHTNESS:
                    volume_status->SetUsed((camBrightness * 100) / 65535);
                    break;
                case VOL_CONTRAST:
                    volume_status->SetUsed((camContrast * 100) / 65535);
                    break;
                case VOL_COLOUR:
                    volume_status->SetUsed((camColour * 100) / 65535);
                    break;
                case VOL_TXSIZE:
                    switch (txWidth)
                    {
                    default:
                    case 704: volume_status->SetUsed(100); break;
                    case 352: volume_status->SetUsed(66); break;
                    case 176: volume_status->SetUsed(33); break;
                    case 128: volume_status->SetUsed(0); break;
                    }
                    volume_value->SetText(getVideoFrameSizeText());
                    break;
                case VOL_TXRATE:
                    volume_status->SetUsed((txFps * 100) / 30);
                    volume_value->SetText(QString::number(txFps));
                    break;
                }
                volume_bkgnd->SetOrder(4);
                volume_bkgnd->refresh();
                volume_status->SetOrder(5);
                volume_status->refresh();
                volume_icon->SetOrder(5);
                volume_icon->refresh();
                volume_setting->SetOrder(6);
                volume_setting->refresh();
                volume_value->SetOrder(6);
                volume_value->refresh();
                volume_info->SetOrder(6);
                volume_info->refresh();

                volume_display_timer->start(3000, true);
            }
            else
            {
                if (volume_status->getOrder() != -1)
                {
                    volume_bkgnd->SetOrder(-1);
                    volume_bkgnd->refresh();
                    volume_status->SetOrder(-1);
                    volume_status->refresh();
                    volume_icon->SetOrder(-1);
                    volume_icon->refresh();
                    volume_icon->SetImage(DEFAULTTHEMEPATH "mp_volume_icon.png");
                    volume_icon->LoadImage();
                    volume_setting->SetOrder(-1);
                    volume_setting->refresh();
                    volume_setting->SetText("Volume");
                    volume_value->SetOrder(-1);
                    volume_value->refresh();
                    volume_value->SetText("");
                    volume_info->SetOrder(-1);
                    volume_info->refresh();
                    VolumeMode = VOL_VOLUME;
                }
            }
        }
    }
}

void PhoneUIBox::DisplayMicSpkPower()
{
    if (rtpAudio != 0)
    {
        short micPower = 0;
        short spkPower = 0;
        rtpAudio->getPower(micPower, spkPower);
        micAmplitude->setRepeat(((int)micPower*19)/32768);
        spkAmplitude->setRepeat(((int)spkPower*19)/32768);
    }
}

void PhoneUIBox::wireUpTheme()
{

    DirectoryList = getUIManagedTreeListType("directorytreelist");
    if (!DirectoryList)
    {
        cerr << "phoneui.o: Couldn't find a Directory box in your theme\n";
        exit(0);
    }
    connect(DirectoryList, SIGNAL(nodeSelected(int, IntVector*)),
            this, SLOT(handleTreeListSignals(int, IntVector*)));

    volume_status = NULL;

    micAmplitude = getUIRepeatedImageType("mic_amplitude");
    spkAmplitude = getUIRepeatedImageType("spk_amplitude");
    micAmplitude->setRepeat(15); // This is to work-round a bug in myththemeddialog where if the repeat
    spkAmplitude->setRepeat(15); // has never been bigger than 0, max_repeat = 0 and it calculates the geometry as 0
    micAmplitude->setRepeat(0);  // thus causing it to be confused
    spkAmplitude->setRepeat(0);

    volume_status = getUIStatusBarType("volume_status");
    if (volume_status)
    {
        volume_status->SetTotal(100);
        volume_status->SetOrder(-1);
    }
    volume_icon = getUIImageType("volumeicon");
    if (volume_icon)
        volume_icon->SetOrder(-1);
    volume_bkgnd = getUIImageType("volumebkgnd");
    if (volume_bkgnd)
        volume_bkgnd->SetOrder(-1);
    volume_setting = getUITextType("volume_setting_text");
    if (volume_setting)
        volume_setting->SetOrder(-1);
    volume_setting->SetText("Volume");
    volume_value = getUITextType("volume_value_text");
    if (volume_value)
        volume_value->SetOrder(-1);
    volume_value->SetText("");
    volume_info = getUITextType("volume_info_text");
    if (volume_info)
        volume_info->SetOrder(-1);
    volume_info->SetText("Up/Down - Change       Left/Right - Adjust");
    
    localWebcamArea = getUIBlackHoleType("local_webcam_blackhole");
    receivedWebcamArea = getUIBlackHoleType("mp_received_video_blackhole");

}


void PhoneUIBox::handleTreeListSignals(int , IntVector *attributes)
{
    if (!SelectHit) // This fn also gets called when the window is first drawn
    {
        SelectHit = false;
        return;
    }

    // SELECT on a directory/speed-dial --- dial this entry
    if ((attributes->at(0) == TA_DIRENTRY) ||
        (attributes->at(0) == TA_SPEEDDIALENTRY))
    {
        DirEntry *entry = DirContainer->fetchDirEntryById(attributes->at(1));
        if (entry)
            doCallPopup(entry, "Dial", false);
        else
            cerr << "Cannot find entry to dial\n";
    }

    // SELECT on a call record --- dial this entry
    else if (attributes->at(0) == TA_CALLHISTENTRY)
    {
        CallRecord *rec = DirContainer->fetchCallRecordById(attributes->at(1));
        DirEntry *entry = DirContainer->FindMatchingDirectoryEntry(rec->getUri());
        if (entry)
            doCallPopup(entry, "Dial", false);
        else
        {
            DirEntry dummyEntry(rec->getDisplayName(), rec->getUri(), "", "", "");
            doCallPopup(&dummyEntry, "Dial", false);
        }
    }

    // SELECT on a voicemail --- play it
    else if (attributes->at(0) == TA_VMAIL_ENTRY)
    {
        char *homeDir = getenv("HOME");
        GenericTree *node = DirectoryList->getCurrentNode();
        QString fileName = QString(homeDir) + "/.mythtv/MythPhone/Voicemail/" + node->getString() + ".wav";
        wavfile *vmailWav = new wavfile();
        if (vmailWav->load(fileName))
        {
            if (vmail)
                delete vmail;
            vmail = new Tone(*vmailWav);
            QString spk = gContext->GetSetting("AudioOutputDevice");
            vmail->Play(spk, false);
        }
        delete vmailWav;
    }
}


PhoneUIBox::~PhoneUIBox(void)
{
    // Just in case we are still mid-call; kill the RTP threads
    if (rtpAudio != 0)
        delete rtpAudio;
    if (rtpVideo != 0)
        StopVideo();

    sipStack->UiStopWatchAll();
    sipStack->UiClosed();
    if (localClient != 0)
        webcam->UnregisterClient(localClient);
    if (txClient != 0)
        webcam->UnregisterClient(txClient);

    webcam->camClose();
    if (volume_control)
        delete volume_control;
    delete DirContainer;
    delete ringbackTone;
    if (vmail)
        delete vmail;
    for (uint c=0; c<(sizeof(toneDtmf)/sizeof(toneDtmf[0])); c++)
        delete (toneDtmf[c]);
    delete h263;

    delete phoneUIStatusBar;
    delete powerDispTimer;
    delete OnScreenClockTimer;
    delete volume_display_timer;
}



/**********************************************************************
PhoneUIStatusBar

This class controls display of the one-line status bar at the foot 
of the screen
**********************************************************************/


PhoneUIStatusBar::PhoneUIStatusBar(UITextType *a, UITextType *b, UITextType *c, UITextType *d, UITextType *e, UITextType *f, 
                                   QObject *parent, const char *name) : QObject(parent, name)  
{
    callerText     = a;
    audioStatsText = b;
    videoStatsText = c;
    bwStatsText    = d;
    callTimeText   = e;
    statusMsgText  = f;

    statsVideoCodec = "";
    statsAudioCodec = "";
    audLast_pIn = 0;
    audLast_pLoss = 0;
    audLast_pTotal = 0;
    vidLast_pIn = 0;
    vidLast_pLoss = 0;
    vidLast_pTotal = 0;
    modeInCallStats = false;
    modeNotification = false;
    callStateString = "";
    callerText->SetText("");
    callTimeText->SetText("");
    audioStatsText->SetText("");
    videoStatsText->SetText("");
    bwStatsText->SetText("");
    statusMsgText->SetText("");
    lastPoll = QTime::currentTime();
    last_bOut = 0;

    notificationTimer = new QTimer(this);
    connect(notificationTimer, SIGNAL(timeout()), this, SLOT(notificationTimeout()));
}
                 
PhoneUIStatusBar::~PhoneUIStatusBar()
{
    delete notificationTimer;
}
                 
void PhoneUIStatusBar::DisplayInCallStats(bool initialise)
{
    if (initialise)
    {
        statsVideoCodec = "";
        statsAudioCodec = "";
        audLast_pIn = 0;
        audLast_pLoss = 0;
        audLast_pTotal = 0;
        vidLast_pIn = 0;
        vidLast_pLoss = 0;
        vidLast_pTotal = 0;
        lastPoll = QTime::currentTime();
        last_bOut = 0;
    }

    modeInCallStats = true;
    if (!modeNotification)
    {
        callerText->SetText(callerString);
        callTimeText->SetText(TimeString);
        audioStatsText->SetText(audioStatsString);
        videoStatsText->SetText(videoStatsString);
        bwStatsText->SetText(bwStatsString);
        statusMsgText->SetText("");
    }
}

void PhoneUIStatusBar::DisplayCallState(QString s)
{
    modeInCallStats = false;
    callStateString = s;
    if (!modeNotification)
    {
        callerText->SetText("");
        callTimeText->SetText("");
        audioStatsText->SetText("");
        videoStatsText->SetText("");
        bwStatsText->SetText("");
        statusMsgText->SetText(s);
    }
}

void PhoneUIStatusBar::DisplayNotification(QString s, int Seconds)
{
    modeNotification = true;
    callerText->SetText("");
    callTimeText->SetText("");
    audioStatsText->SetText("");
    videoStatsText->SetText("");
    bwStatsText->SetText("");
    statusMsgText->SetText(s);

    notificationTimer->start(Seconds*1000, true);
}

void PhoneUIStatusBar::notificationTimeout()
{
    modeNotification = false;
    if (modeInCallStats)
        DisplayInCallStats(false);
    else
        DisplayCallState(callStateString);
}

void PhoneUIStatusBar::updateMidCallCaller(QString t)
{
    callerString = t;
    if (modeInCallStats && !modeNotification)
        callerText->SetText(t);
}                

void PhoneUIStatusBar::updateMidCallTime(int Seconds)
{
    int Hours = Seconds / 60 / 60;
    int Mins = (Seconds - (Hours*60*60)) / 60;
    int Secs = (Seconds - (Hours*60*60) - (Mins*60));
    TimeString.sprintf("%d:%02d:%02d", Hours, Mins, Secs);

    if (modeInCallStats && !modeNotification)
        callTimeText->SetText(TimeString);
}

void PhoneUIStatusBar::updateMidCallAudioStats(int pIn, int pMiss, int pLate, int pOut)
{
    audioStatsString = statsAudioCodec;
    (void)pOut;

    // Estimate line quality
    int pLoss = pMiss + pLate;
    int pTotal = pLoss + pIn;
    if (pIn == audLast_pIn)
        audioStatsString += "; Not Receiving";
    else
    {
        int totalPercentPacketLoss = (pLoss*100)/pTotal;
        int lastPeriodPacketLoss = ((pLoss-audLast_pLoss)*100)/(pTotal-audLast_pTotal);
        QString LossString;
        LossString.sprintf("; Loss %d%%/%d%%", lastPeriodPacketLoss, totalPercentPacketLoss);
        audioStatsString += LossString;
    }
    if (modeInCallStats && !modeNotification)
        audioStatsText->SetText(audioStatsString);
    audLast_pIn = pIn;
    audLast_pLoss = pLoss;
    audLast_pTotal = pTotal;
}

void PhoneUIStatusBar::updateMidCallVideoStats(int pIn, int pMiss, int pLate, int pOut)
{
    videoStatsString = statsVideoCodec;
    (void)pOut;

    // Estimate line quality
    int pLoss = pMiss + pLate;
    int pTotal = pLoss + pIn;
    if (pIn == vidLast_pIn)
        videoStatsString += "; Not Receiving";
    else
    {
        int totalPercentPacketLoss = (pLoss*100)/pTotal;
        int lastPeriodPacketLoss = ((pLoss-vidLast_pLoss)*100)/(pTotal-vidLast_pTotal);
        QString LossString;
        LossString.sprintf("; Loss %d%%/%d%%", lastPeriodPacketLoss, totalPercentPacketLoss);
        videoStatsString += LossString;
    }

    if (modeInCallStats && !modeNotification)
        videoStatsText->SetText(videoStatsString);
    vidLast_pIn = pIn;
    vidLast_pLoss = pLoss;
    vidLast_pTotal = pTotal;
}

void PhoneUIStatusBar::updateMidCallBandwidth(int bIn, int bOut)
{
    (void)bIn;
    int duration = lastPoll.msecsTo(QTime::currentTime());
    lastPoll = QTime::currentTime();

    bwStatsString.sprintf("B/w: %dk", (bOut-last_bOut)*8/duration);
    last_bOut = bOut;

    if (modeInCallStats && !modeNotification)
        bwStatsText->SetText(bwStatsString);
}

void PhoneUIStatusBar::updateMidCallVideoCodec(QString c)
{
    statsVideoCodec = c;
}
                 
void PhoneUIStatusBar::updateMidCallAudioCodec(QString c)
{
    statsAudioCodec = c;
}                                    
                 


