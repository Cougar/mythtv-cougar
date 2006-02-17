// Standard UNIX C headers
#include <fcntl.h>
#include <sys/ioctl.h>

// MythTV headers
#include "cardutil.h"
#include "videosource.h"
#include "mythcontext.h"
#include "mythdbcon.h"

#ifdef USING_DVB
#include "dvbchannel.h"
#include "dvbdev.h"
#endif

#ifdef USING_V4L
#include "videodev_myth.h"
#endif

/** \fn CardUtil::IsCardTypePresent(const QString&)
 *  \brief Returns true if the card type is present
 *  \param strType card type being checked for
 */
bool CardUtil::IsCardTypePresent(const QString &strType)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT count(cardtype) "
                  "FROM capturecard, cardinput "
                  "WHERE cardinput.cardid = capturecard.cardid AND "
                  "      capturecard.cardtype = :CARDTYPE AND "
                  "      capturecard.hostname = :HOSTNAME");
    query.bindValue(":CARDTYPE", strType);
    query.bindValue(":HOSTNAME", gContext->GetHostName());

    if (query.exec() && query.isActive() && query.size() > 0)
    {
        query.next();
        int count = query.value(0).toInt();

        if (count > 0)
            return true;
    }

    return false;
}

/** \fn CardUtil::GetDVBType(uint, QString&, QString&)
 *  \brief Returns the card type from the video device
 *  \param device    DVB videodevice to be checked
 *  \param name      Returns the probed card name
 *  \param card_type Returns the card type as a string
 *  \return the card type
 */
enum CardUtil::CARD_TYPES CardUtil::GetDVBType(
    uint device, QString &name, QString &card_type)
{
    (void)device;
    (void)name;
    (void)card_type;

    CARD_TYPES nRet = ERROR_OPEN;
#ifdef USING_DVB
    int fd_frontend = open(dvbdevice(DVB_DEV_FRONTEND, device),
                           O_RDWR | O_NONBLOCK);
    if (fd_frontend >= 0)
    {
        struct dvb_frontend_info info;
        nRet = ERROR_PROBE;
        if (ioctl(fd_frontend, FE_GET_INFO, &info) >= 0)
        {
            name = info.name;
            switch(info.type)
            {
            case FE_QAM:
                nRet = QAM;
                card_type = "QAM";
                break;
            case FE_QPSK:
                nRet = QPSK;
                card_type = "QPSK";
                break;
            case FE_OFDM:
                nRet = OFDM;
                card_type = "OFDM";
                break;
#if (DVB_API_VERSION_MINOR == 1)
            case FE_ATSC:
                nRet = ATSC;
                card_type = "ATSC";
                break;
#endif
            }
        }
        close(fd_frontend);
    } 
#endif
    return nRet;
}

/** \fn CardUtil::HasDVBCRCBug(uint)
 *  \brief Returns true if and only if the device munges 
 *         PAT/PMT tables, and then doesn't fix the CRC.
 *
 *   Currently the list of broken DVB hardware and drivers includes:
 *   "Philips TDA10046H DVB-T", "VLSI VES1x93 DVB-S", and "ST STV0299 DVB-S"
 *
 *  \param device video dev to be checked
 *  \return true iff the device munges tables, so that they fail a CRC check.
 */
bool CardUtil::HasDVBCRCBug(uint device)
{
    QString name(""), type("");
    GetDVBType(device, name, type);
    return ((name == "Philips TDA10046H DVB-T") || // munges PMT
            (name == "VLSI VES1x93 DVB-S")      || // munges PMT
            (name == "DST DVB-S")               || // munges PAT
            (name == "ST STV0299 DVB-S"));         // munges PAT
}

/** \fn CardUtil::GetCardType(uint, QString&, QString&)
 *  \brief Returns the card type from the video device
 *  \param nCardID   cardid of card to be checked
 *  \param name      Returns the probed card name
 *  \param card_type Returns the card type as a string
 *  \return the card type from CARD_TYPES enum
 */
enum CardUtil::CARD_TYPES CardUtil::GetCardType(uint nCardID, QString &name,
                                                QString &card_type)
{
    CARD_TYPES nRet = ERROR_OPEN;
    QString strDevice;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT videodevice, cardtype "
                  "FROM capturecard "
                  "WHERE capturecard.cardid = :CARDID");
    query.bindValue(":CARDID", nCardID);

    if (query.exec() && query.isActive() && query.size() > 0)
    {
        query.next();
        strDevice = query.value(0).toString();
        card_type = query.value(1).toString().upper();
    }
    else
        return nRet;

    if (card_type == "V4L")
        nRet = V4L;
    else if (card_type == "MPEG")
        nRet = MPEG;
    else if (card_type == "FIREWIRE")
        nRet = FIREWIRE;
    else if (card_type == "HDTV")
        nRet = HDTV;
#ifdef USING_DVB
    else if (card_type == "DVB")
        nRet = GetDVBType(strDevice.toInt(), name, card_type);
#else
    (void)name;
#endif
    return nRet;
}

/** \fn CardUtil::GetCardType(uint, QString&)
 *  \brief Returns the card type from the video device
 *  \param nCardID   cardid of card to be checked
 *  \param name      Returns the probed card name
 *  \return the card type
 */
enum CardUtil::CARD_TYPES CardUtil::GetCardType(uint nCardID, QString &name)
{
    QString card_type;
    return CardUtil::GetCardType(nCardID, name, card_type);
}

/** \fn CardUtil::GetCardType(uint)
 *  \brief Returns the card type from the video device
 *  \param nCardID   cardid of card to be checked
 *  \return the card type
 */
enum CardUtil::CARD_TYPES CardUtil::GetCardType(uint nCardID)
{
    QString name, card_type;
    return CardUtil::GetCardType(nCardID, name, card_type);
}

/** \fn CardUtil::IsDVBCardType(const QString)
 *  \brief Returns true iff the card_type is one of the DVB types.
 */
bool CardUtil::IsDVBCardType(const QString card_type)
{
    QString ct = card_type.upper();
    return (ct == "DVB") || (ct == "QAM") || (ct == "QPSK") ||
        (ct == "OFDM") || (ct == "ATSC");
}

/** \fn CardUtil::GetVideoDevice(uint)
 *  \brief Returns the card type from the video device
 *  \param nCardID   cardid of card to be checked
 *  \return videodevice corresponding to cardid
 */
QString CardUtil::GetVideoDevice(uint cardid)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT videodevice "
                  "FROM capturecard "
                  "WHERE capturecard.cardid = :CARDID");
    query.bindValue(":CARDID", cardid);

    if (!query.exec() || !query.isActive())
        MythContext::DBError("CardUtil::GetVideoDevice() 1", query);
    else if (query.next())
        return query.value(0).toString();

    return QString::null;
}

QString CardUtil::GetVideoDevice(uint cardid, uint sourceid)
{
    QString device = QString::null;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT videodevice "
                  "FROM capturecard, cardinput "
                  "WHERE sourceid         = :SOURCEID AND "
                  "      cardinput.cardid = :CARDID   AND "
                  "      ( ( cardinput.childcardid != '0' AND "
                  "          cardinput.childcardid  = capturecard.cardid ) OR "
                  "        ( cardinput.childcardid  = '0' AND "
                  "          cardinput.cardid       = capturecard.cardid ) )");
    query.bindValue(":CARDID", cardid);
    query.bindValue(":SOURCEID", sourceid);

    if (!query.exec() || !query.isActive())
        MythContext::DBError("CardUtil::GetVideoDevice() 2", query);
    else if (query.next())
        device = query.value(0).toString();

    return device;    
}

/** \fn CardUtil::GetCardID(const QString&, QString)
 *  \brief Returns the cardid of the card that uses the specified
 *         videodevice, and optionally a non-local hostname.
 *  \param videodevice Video device we want card id for
 *  \param hostname    Host on which device resides, only
 *                     required if said host is not the localhost
 */
int CardUtil::GetCardID(const QString &videodevice, QString hostname)
{
    if (hostname == QString::null)
        hostname = gContext->GetHostName();

    MSqlQuery query(MSqlQuery::InitCon());
    QString thequery =
        QString("SELECT cardid FROM capturecard "
                "WHERE videodevice = '%1' AND "
                "      hostname = '%2'")
        .arg(videodevice).arg(hostname);

    query.prepare(thequery);
    if (!query.exec() || !query.isActive())
        MythContext::DBError("CardUtil::GetCardID()", query);
    else if (query.next())
        return query.value(0).toInt();

    return -1;
}

uint CardUtil::GetChildCardID(uint cardid)
{
    // check if child card definition does exist
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "SELECT cardid "
        "FROM capturecard "
        "WHERE parentid = :CARDID");
    query.bindValue(":CARDID", cardid);

    int ret = 0;
    if (!query.exec() || !query.isActive())
        MythContext::DBError("CaptureCard::GetChildCardID()", query);
    else if (query.next())
        ret = query.value(0).toInt();

    return ret;
}

uint CardUtil::GetParentCardID(uint cardid)
{
    // check if child card definition does exists
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "SELECT parentid "
        "FROM capturecard "
        "WHERE cardid = :CARDID");
    query.bindValue(":CARDID", cardid);

    int ret = 0;
    if (!query.exec() || !query.isActive())
        MythContext::DBError("CaptureCard::GetParentCardID()", query);
    else if (query.next())
        ret = query.value(0).toInt();

    return ret;
}

/** \fn CardUtil::GetVideoDevice(uint, QString&, QString&)
 *  \brief Returns the the video device associated with the card id
 *  \param nCardID card id to check
 *  \param device the returned device
 *  \param vbi the returned vbi device
 *  \return true on success
 */
bool CardUtil::GetVideoDevice(uint nCardID, QString& device, QString& vbi)
{
    bool fRet=false;
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT videodevice, vbidevice "
                  "FROM capturecard "
                  "WHERE capturecard.cardid = :CARDID");
    query.bindValue(":CARDID", nCardID);

    if (!query.exec() || !query.isActive())
        MythContext::DBError("CardUtil::GetVideoDevice()", query);
    else if (query.next())
    {
        device = query.value(0).toString();
        vbi = query.value(1).toString();
        fRet = true;
    }
    return fRet;
}

/** \fn CardUtil::IsDVB(uint)
 *  \brief Returns true if the card is a DVB card
 *  \param nCardID card id to check
 *  \return true if the card is a DVB one
 */
bool CardUtil::IsDVB(uint nCardID)
{
    bool fRet = false;
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT cardtype "
                  "FROM capturecard "
                  "WHERE capturecard.cardid= :CARDID");
    query.bindValue(":CARDID", nCardID);

    if (!query.exec() || !query.isActive())
        MythContext::DBError("CardUtil::IsDVB()", query);
    else if (query.next())
        fRet = (query.value(0).toString() == "DVB");

    return fRet;
}

/** \fn CardUtil::GetDISEqCType(uint)
 *  \brief Returns the disqec type associated with a DVB card
 *  \param nCardID card id to check
 *  \return the disqec type
 */
enum DISEQC_TYPES CardUtil::GetDISEqCType(uint nCardID)
{
    int iRet = 0;
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT dvb_diseqc_type "
                  "FROM capturecard "
                  "WHERE capturecard.cardid = :CARDID");
    query.bindValue(":CARDID", nCardID);

    if (!query.exec() || !query.isActive())
        MythContext::DBError("CardUtil::GetDISEqCType()", query);
    else if (query.next())
        iRet = query.value(0).toInt();

    return (DISEQC_TYPES)iRet;
}

/** \fn CardUtil::GetDefaultInput(uint)
 *  \brief Returns the default input for the card
 *  \param nCardID card id to check
 *  \return the default input
 */
QString CardUtil::GetDefaultInput(uint nCardID)
{
    QString str = QString::null;
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT defaultinput "
                  "FROM capturecard "
                  "WHERE capturecard.cardid = :CARDID");
    query.bindValue(":CARDID", nCardID);

    if (!query.exec() || !query.isActive())
        MythContext::DBError("CardUtil::GetDefaultInput()", query);
    else if (query.next())
        str = query.value(0).toString();

    return str;
}

QString CardUtil::GetInputName(uint cardid, uint sourceid)
{
    QString str = QString::null;
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT inputname "
                  "FROM cardinput "
                  "WHERE sourceid = :SOURCEID AND "
                  "      cardid   = :CARDID");
    query.bindValue(":SOURCEID", sourceid);
    query.bindValue(":CARDID",   cardid);

    if (!query.exec() || !query.isActive())
        MythContext::DBError("CardUtil::GetInputName()", query);
    else if (query.next())
        str = query.value(0).toString();

    return str;
}

bool CardUtil::GetTimeouts(uint cardid,
                           uint &signal_timeout, uint &channel_timeout)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "SELECT signal_timeout, channel_timeout "
        "FROM capturecard "
        "WHERE cardid = :CARDID");
    query.bindValue(":CARDID", cardid);

    if (!query.exec() || !query.isActive())
        MythContext::DBError("CardUtil::GetTimeouts()", query);
    else if (query.next())
    {
        signal_timeout  = (uint) max(query.value(0).toInt(), 250);
        channel_timeout = (uint) max(query.value(1).toInt(), 500);
        return true;
    }

    return false;
}

bool CardUtil::IgnoreEncrypted(uint cardid, const QString &input_name)
{
    bool freetoair = true;
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "SELECT freetoaironly "
        "FROM cardinput "
        "WHERE cardid    = :CARDID AND "
        "      inputname = :INPUTNAME");
    query.bindValue(":CARDID",    cardid);
    query.bindValue(":INPUTNAME", input_name);

    if (!query.exec() || !query.isActive())
        MythContext::DBError("CardUtil::IgnoreEncrypted()", query);
    else if (query.next())
        freetoair = query.value(0).toBool();

    return freetoair;
}

bool CardUtil::TVOnly(uint cardid, const QString &input_name)
{
    bool radioservices = true;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "SELECT radioservices "
        "FROM cardinput "
        "WHERE cardid    = :CARDID AND "
        "      inputname = :INPUTNAME");
    query.bindValue(":CARDID",    cardid);
    query.bindValue(":INPUTNAME", input_name);

    if (!query.exec() || !query.isActive())
        MythContext::DBError("CardUtil::TVOnly()", query);
    else if (query.next())
        radioservices = query.value(0).toBool();

    return !radioservices;
}

bool CardUtil::hasV4L2(int videofd)
{
    (void) videofd;
#ifdef USING_V4L
    struct v4l2_capability vcap;
    bzero(&vcap, sizeof(vcap));

    return ((ioctl(videofd, VIDIOC_QUERYCAP, &vcap) >= 0) &&
            (vcap.capabilities & V4L2_CAP_VIDEO_CAPTURE));
#else // if !USING_V4L
    return false;
#endif // !USING_V4L
}

InputNames CardUtil::probeV4LInputs(int videofd, bool &ok)
{
    (void) videofd;

    InputNames list;
    ok = false;

#ifdef USING_V4L
    bool usingv4l2 = hasV4L2(videofd);

    // V4L v2 query
    struct v4l2_input vin;
    bzero(&vin, sizeof(vin));
    while (usingv4l2 && (ioctl(videofd, VIDIOC_ENUMINPUT, &vin) >= 0))
    {
        QString input((char *)vin.name);
        list[vin.index] = input;
        vin.index++;
    }
    if (vin.index)
    {
        ok = true;
        return list;
    }

    // V4L v1 query
    struct video_capability vidcap;
    bzero(&vidcap, sizeof(vidcap));
    if (ioctl(videofd, VIDIOCGCAP, &vidcap) != 0)
    {
        QString msg = QObject::tr("Could not query inputs.");
        VERBOSE(VB_IMPORTANT, msg + ENO);
        list[-1] = msg;
        vidcap.channels = 0;
    }

    for (int i = 0; i < vidcap.channels; i++)
    {
        struct video_channel test;
        bzero(&test, sizeof(test));
        test.channel = i;

        if (ioctl(videofd, VIDIOCGCHAN, &test) != 0)
        {
            VERBOSE(VB_IMPORTANT,
                    QString("Could determine name of input #%1"
                            "\n\t\t\tNot adding it to the list.")
                    .arg(test.channel) + ENO);
            continue;
        }

        list[i] = test.name;
    }

    // Create an input on single input cards that don't advertise input
    if (!list.size())
        list[0] = "Television";

    ok = true;
#else // if !USING_V4L
    list[-1] += QObject::tr("ERROR, Compile with V4L support to query inputs");
#endif // !USING_V4L
    return list;
}

QStringList CardUtil::probeInputs(QString device, QString cardtype,
                                  int diseqctype)
{
    QStringList ret;

    if (("FIREWIRE" == cardtype) || ("DBOX2" == cardtype))
        ret += "MPEG2TS";
    else if ("DVB" == cardtype)
        ret += probeDVBInputs(device, diseqctype);
    else
        ret += probeV4LInputs(device);

    ret += probeChildInputs(device);

    return ret;
}

QStringList CardUtil::probeV4LInputs(QString device)
{
    bool ok;
    QStringList ret;
    int videofd = open(device.ascii(), O_RDWR);
    if (videofd < 0)
    {
        ret += QObject::tr("Could not open '%1' "
                           "to probe its inputs.").arg(device);
        return ret;
    }
    InputNames list = CardUtil::probeV4LInputs(videofd, ok);
    close(videofd);

    if (!ok)
    {
        ret += list[-1];
        return ret;
    }

    InputNames::iterator it;
    for (it = list.begin(); it != list.end(); ++it)
    {
        if (it.key() >= 0)
            ret += *it;
    }

    return ret;
}

QStringList CardUtil::probeDVBInputs(QString device, int diseqc_type)
{
    QStringList ret;
#ifdef USING_DVB
    if (diseqc_type < 0)
    {
        int cardid = CardUtil::GetCardID(device);
        if (cardid <= 0)
            return ret;
        diseqc_type = GetDISEqCType(cardid);
    }

    QValueList<DVBDiSEqCInput> dvbinput;
    dvbinput = fillDVBInputsDiSEqC(diseqc_type);

    QValueList<DVBDiSEqCInput>::iterator it;
    for (it = dvbinput.begin(); it != dvbinput.end(); ++it)
        ret += (*it).input;
#else
    ret += QObject::tr("ERROR, Compile with DVB support to query inputs");
#endif
    return ret;
}

QStringList CardUtil::probeChildInputs(QString device)
{
    QStringList ret;

    int cardid = CardUtil::GetCardID(device);
    if (cardid <= 0)
        return ret;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT videodevice, cardtype "
                  "FROM capturecard "
                  "WHERE parentid = :CARDID");
    query.bindValue(":CARDID", cardid);

    if (!query.exec() || !query.isActive())
        return ret;

    while (query.next())
        ret += probeInputs(query.value(0).toString(),
                           query.value(1).toString());

    return ret;
}

QValueList<DVBDiSEqCInput>
CardUtil::fillDVBInputsDiSEqC(int dvb_diseqc_type)
{
    QValueList<DVBDiSEqCInput> list;

    QString stxt = "DiSEqC Switch Input %1";
    QString mtxt = "DiSEqC v1.2 Motor Position %1";
    QString itxt = "DiSEqC v1.3 Input %1";
    QString l21txt = "SW21 Input %1";
    QString l64txt = "SW64 Input %1";

    uint i;
    switch (dvb_diseqc_type)
    {
        case 1: case 2: case 3:
            for (i = 0; i < 2; ++i)
                list.append(DVBDiSEqCInput(
                                stxt.arg(i+1), QString::number(i), ""));
            break;
        case 4: case 5:
            for (i = 0; i < 4; ++i)
                list.append(DVBDiSEqCInput(
                                stxt.arg(i+1), QString::number(i), ""));
            break;
        case 6:
            for (i = 1; i < 50; ++i)
                list.append(DVBDiSEqCInput(
                                mtxt.arg(i), "", QString::number(i)));
            break;
        case 7:
            for (i = 1; i < 20; ++i)
                list.append(DVBDiSEqCInput(
                                itxt.arg(i), "", QString::number(i)));
            break;
        case 8:
            for (i = 0; i < 10; ++i)
                list.append(DVBDiSEqCInput(
                                stxt.arg(i+1,2), QString::number(i), ""));
            break;
        case 9:
            for (i = 0; i < 2; ++i)
                list.append(DVBDiSEqCInput(
                                l21txt.arg(i+1,2), QString::number(i), ""));
            break;
        case 10:
            for (i = 0; i < 3; ++i)
                list.append(DVBDiSEqCInput(
                                l64txt.arg(i+1,2), QString::number(i), ""));
            break;
        default:
            list.append(DVBDiSEqCInput(
                            QString("DVBInput"), QString(""), QString("")));
    }

    return list;
}

QString CardUtil::GetDeviceLabel(uint cardid,
                                 QString cardtype,
                                 QString videodevice)
{
    QString label = QString::null;

    if (cardtype == "FIREWIRE")
    {
        MSqlQuery query(MSqlQuery::InitCon());
        query.prepare(
            "SELECT firewire_port, firewire_node "
            "FROM capturecard "
            "WHERE cardid = :CARDID");
        query.bindValue(":CARDID", cardid);

        if (!query.exec() || !query.isActive() || !query.size())
            label = "[ DB ERROR ]";
        else
            label = QString("[ FIREWIRE : Port %2 Node %3 ]")
                .arg(query.value(0).toString())
                .arg(query.value(1).toString());
 
    }
    else if (cardtype == "DBOX2")
    {
        MSqlQuery query(MSqlQuery::InitCon());
        query.prepare(
            "SELECT dbox2_host, dbox2_port, dbox2_httpport "
            "FROM capturecard "
            "WHERE cardid = :CARDID");
        query.bindValue(":CARDID", cardid);

        if (!query.exec() || !query.isActive() || !query.size())
            label = "[ DB ERROR ]";
        else
            label = QString("[ DBOX2 : IP %1 Port %2 HttpPort %3 ]")
                .arg(query.value(0).toString())
                .arg(query.value(1).toString())
                .arg(query.value(2).toString());
    }
    else
    {
        label = QString("[ %1 : %2 ]").arg(cardtype).arg(videodevice);
    }

    return label;
}

void CardUtil::GetCardInputs(
    int                 cardid,
    QString             device,
    QString             cardtype,
    QStringList        &inputLabels,
    vector<CardInput*> &cardInputs,
    int                 parentid)
{
    int rcardid = (parentid) ? parentid : cardid;
    QStringList inputs;

    if (("FIREWIRE" == cardtype) || ("DBOX2" == cardtype))
        inputs += "MPEG2TS";
    else if ("DVB" != cardtype)
        inputs += probeV4LInputs(device);

    QString dev_label = (parentid) ? " -> " : "";
    dev_label += GetDeviceLabel(cardid, cardtype, device);

    QStringList::iterator it = inputs.begin();
    for (; it != inputs.end(); ++it)
    {
        CardInput* cardinput = new CardInput(true);
        cardinput->loadByInput(rcardid, (*it));
        cardinput->SetChildCardID((parentid) ? cardid : 0);
        inputLabels.push_back(
            dev_label + QString(" (%1) -> %2")
            .arg(*it).arg(cardinput->getSourceName()));
        cardInputs.push_back(cardinput);
    }

    if ("DVB" == cardtype)
    {
        QValueList<DVBDiSEqCInput> dvbinputs;
        int diseq_type = 0; // TODO
        dvbinputs = fillDVBInputsDiSEqC(diseq_type);
        QValueList<DVBDiSEqCInput>::iterator it;
        for (it = dvbinputs.begin(); it != dvbinputs.end(); ++it)
        {
            CardInput* cardinput = new CardInput(true);
            cardinput->loadByInput(rcardid, (*it).input);
            cardinput->fillDiseqcSettingsInput((*it).position,(*it).port);
            cardinput->SetChildCardID((parentid) ? cardid : 0);
            inputLabels.push_back(
                dev_label + QString(" (%1) -> %2")
                .arg((*it).input).arg(cardinput->getSourceName()));
            cardInputs.push_back(cardinput);            
        }
    }

    if (parentid)
        return;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT cardid, videodevice, cardtype "
                  "FROM capturecard "
                  "WHERE parentid = :CARDID");
    query.bindValue(":CARDID", cardid);

    if (!query.exec() || !query.isActive())
        return;

    while (query.next())
    {
        GetCardInputs(query.value(0).toUInt(),
                      query.value(1).toString(),
                      query.value(2).toString(),
                      inputLabels, cardInputs, cardid);
    }
}
