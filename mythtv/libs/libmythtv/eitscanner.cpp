// -*- Mode: c++ -*-

#ifndef USING_DVB
#error USING_DVB must be defined to compile eitscanner.cpp
#endif

#include "tv_rec.h"
#include "dvbchannel.h"
#include "dvbsiparser.h"
#include "dvbtypes.h"

#include "eitscanner.h"
#include "eithelper.h"

/** \class EITScanner
 *  \brief Acts as glue between DVBChannel, DVBSIParser, and EITHelper.
 *
 *  This is the class where the "EIT Crawl" should be implemented.
 *
 */

EITScanner::EITScanner()
    : QObject(NULL, "EITScanner"),
      channel(NULL), parser(NULL), eitHelper(new EITHelper()),
      exitThread(false), rec(NULL), activeScan(false)
{
    pthread_create(&eventThread, NULL, SpawnEventLoop, this);
}

void EITScanner::TeardownAll(void)
{
    StopActiveScan();
    if (!exitThread)
    {
        exitThread = true;
        exitThreadCond.wakeAll();
        pthread_join(eventThread, NULL);
    }

    if (eitHelper)
    {
        eitHelper->deleteLater();
        eitHelper = NULL;
    }
}

void EITScanner::deleteLater(void)
{
    TeardownAll();
    QObject::deleteLater();
}

void EITScanner::SetPMTObject(const PMTObject *)
{
    eitHelper->ClearList();
}

/** \fn EITScanner::SpawnEventLoop(void*)
 *  \brief Thunk that allows scanner_thread pthread to
 *         call EITScanner::RunScanner().
 */
void *EITScanner::SpawnEventLoop(void *param)
{
    EITScanner *scanner = (EITScanner*) param;
    scanner->RunEventLoop();
    return NULL;
}

/** \fn EITScanner::RunEventLoop()
 *  \brief This runs the event loop for EITScanner until 'exitThread' is true.
 */
void EITScanner::RunEventLoop(void)
{
    exitThread = false;
    while (!exitThread)
    {
        if (channel)
        {
            int mplex = channel->GetMultiplexID();
            if ((mplex > 0) && parser && eitHelper->GetListSize())
                eitHelper->ProcessEvents(mplex);
        }

        if (activeScan && (QDateTime::currentDateTime() > activeScanNextTrig))
        {
            if (activeScanNextChan == activeScanChannels.end())
                activeScanNextChan = activeScanChannels.begin();
 
            if (!(*activeScanNextChan).isEmpty())
                rec->SetChannel(*activeScanNextChan, TVRec::kFlagEITScan);

            activeScanNextTrig = QDateTime::currentDateTime()
                .addSecs(activeScanTrigTime);
            activeScanNextChan++;
        }

        exitThreadCond.wait(200); // sleep up to 200 ms.
    }
}

/** \fn EITScanner::StartPassiveScan(DVBChannel*, DVBSIParser*)
 *  \brief Start inserting Event Information Tables from the multiplex
 *         we happen to be tuned to into the database.
 */
void EITScanner::StartPassiveScan(DVBChannel *_channel, DVBSIParser *_parser)
{
    eitHelper->ClearList();
    parser  = _parser;
    channel = _channel;
    connect(parser,    SIGNAL(EventsReady(QMap_Events*)),
            eitHelper, SLOT(HandleEITs(QMap_Events*)));
    connect(channel,   SIGNAL(UpdatePMTObject(const PMTObject *)),
            this,      SLOT(SetPMTObject(const PMTObject *)));
}

/** \fn EITScanner::StopPassiveScan()
 *  \brief Stops inserting Event Information Tables into DB.
 */
void EITScanner::StopPassiveScan(void)
{
    eitHelper->disconnect();
    eitHelper->ClearList();

    channel = NULL;
    parser  = NULL;
}

void EITScanner::StartActiveScan(TVRec *_rec, uint max_seconds_per_source)
{
    rec = _rec;

    if (!activeScanChannels.size())
    {
        MSqlQuery query(MSqlQuery::InitCon());
        query.prepare(
            "SELECT channum "
            "FROM channel, cardinput, capturecard "
            "WHERE cardinput.sourceid = channel.sourceid AND "
            "      capturecard.cardid = cardinput.cardid AND "
            "      channel.mplexid      IS NOT NULL      AND "
            "      cardinput.cardid   = :CARDID "
            "ORDER BY cardinput.sourceid, atscsrcid");
        query.bindValue(":CARDID", rec->GetCaptureCardNum());

        if (!query.exec() || !query.isActive())
        {
            MythContext::DBError("EITScanner::StartActiveScan", query);
            return;
        }

        while (query.next())
            activeScanChannels << query.value(0).toString();
        activeScanNextChan = activeScanChannels.begin();
    }

    VERBOSE(VB_SIPARSER, "StartActiveScan called with "<<
            activeScanChannels.size()<<" channels");

    if (activeScanChannels.size())
    {
        activeScanNextTrig = QDateTime::currentDateTime();
        activeScanTrigTime = max_seconds_per_source;
        activeScan = true;
    }
}

void EITScanner::StopActiveScan()
{
    activeScan = false;
    rec = NULL;
    StopPassiveScan();
}
