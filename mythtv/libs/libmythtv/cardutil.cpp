// Standard UNIX C headers
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>

// MythTV headers
#include "cardutil.h"
#include "videosource.h"
#include "mythcontext.h"
#include "mythdbcon.h"
#include "dvbchannel.h"
#include "diseqcsettings.h"

#ifdef USING_DVB
#include "dvbtypes.h"
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

QString CardUtil::ProbeDVBType(uint device)
{
    QString ret = "ERROR_UNKNOWN";
    (void) device;

#ifdef USING_DVB
    QString dvbdev = CardUtil::GetDeviceName(DVB_DEV_FRONTEND, device);
    int fd_frontend = open(dvbdev.ascii(), O_RDONLY | O_NONBLOCK);
    if (fd_frontend < 0)
    {
        VERBOSE(VB_IMPORTANT, "Can't open DVB frontend (" + dvbdev + ").");
        return ret;
    }

    struct dvb_frontend_info info;
    int err = ioctl(fd_frontend, FE_GET_INFO, &info);
    if (err < 0)
    {
        close(fd_frontend);
        VERBOSE(VB_IMPORTANT, "FE_GET_INFO ioctl failed (" + dvbdev + ").");
        return ret;
    }
    close(fd_frontend);

    DTVTunerType type(info.type);
    ret = (type.toString() != "UNKNOWN") ? type.toString().upper() : ret;
#endif // USING_DVB

    return ret;
}

/** \fn CardUtil::ProbeDVBFrontendName(uint)
 *  \brief Returns the card type from the video device
 */
QString CardUtil::ProbeDVBFrontendName(uint device)
{
    QString ret = "ERROR_UNKNOWN";
    (void) device;

#ifdef USING_DVB
    QString dvbdev = CardUtil::GetDeviceName(DVB_DEV_FRONTEND, device);
    int fd_frontend = open(dvbdev.ascii(), O_RDWR | O_NONBLOCK);
    if (fd_frontend < 0)
        return "ERROR_OPEN";

    struct dvb_frontend_info info;
    int err = ioctl(fd_frontend, FE_GET_INFO, &info);
    if (err < 0)
    {
        close(fd_frontend);
        return "ERROR_PROBE";
    }

    ret = info.name;

    close(fd_frontend);
#endif // USING_DVB

    return ret;
}

/** \fn CardUtil::HasDVBCRCBug(uint)
 *  \brief Returns true if and only if the device munges 
 *         PAT/PMT tables, and then doesn't fix the CRC.
 *
 *   Currently the list of broken DVB hardware and drivers includes:
 *   "VLSI VES1x93 DVB-S", and "ST STV0299 DVB-S"
 *
 *  Note: "DST DVB-S" was on this list but has been verified to not
 *        mess up the PAT using Linux 2.6.18.1.
 *
 *  Note: "Philips TDA10046H DVB-T" was on this list but has been
 *        verified to not mess up the PMT with a recent kernel and
 *        firmware (See http://svn.mythtv.org/trac/ticket/3541).
 *
 *  \param device Open DVB frontend device file descriptor to be checked
 *  \return true iff the device munges tables, so that they fail a CRC check.
 */
bool CardUtil::HasDVBCRCBug(uint device)
{
    QString name = ProbeDVBFrontendName(device);
    return ((name == "VLSI VES1x93 DVB-S")      || // munges PMT
            (name == "ST STV0299 DVB-S"));         // munges PAT
}

uint CardUtil::GetMinSignalMonitoringDelay(uint device)
{
    QString name = ProbeDVBFrontendName(device);
    if (name.find("DVB-S") >= 0)
        return 300;
    if (name == "DiBcom 3000P/M-C DVB-T")
        return 100;
    return 25;
}

QString CardUtil::ProbeSubTypeName(uint cardid, const QString &inputname)
{
    QString type = GetRawCardType(cardid, inputname);
    if ("DVB" != type)
        return type;

    QString device = GetVideoDevice(cardid, inputname);

    if (device.isEmpty())
        return "ERROR_OPEN";

    return ProbeDVBType(device.toUInt());
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

QString get_on_source(const QString &to_get, uint cardid, uint sourceid)
{
    MSqlQuery query(MSqlQuery::InitCon());
    if (sourceid)
    {
        query.prepare(
            QString("SELECT %1 ").arg(to_get) +
            "FROM capturecard, cardinput "
            "WHERE sourceid         = :SOURCEID AND "
            "      cardinput.cardid = :CARDID   AND "
            "      ( ( cardinput.childcardid != '0' AND "
            "          cardinput.childcardid  = capturecard.cardid ) OR "
            "        ( cardinput.childcardid  = '0' AND "
            "          cardinput.cardid       = capturecard.cardid ) )");
        query.bindValue(":SOURCEID", sourceid);
    }
    else
    {
        query.prepare(
            QString("SELECT %1 ").arg(to_get) +
            "FROM capturecard "
            "WHERE capturecard.cardid = :CARDID");
    }
    query.bindValue(":CARDID", cardid);

    if (!query.exec() || !query.isActive())
        MythContext::DBError("CardUtil::get_on_source", query);
    else if (query.next())
        return query.value(0).toString();

    return QString::null;
}

QString get_on_input(const QString &to_get,
                     uint cardid, const QString &input)
{
    MSqlQuery query(MSqlQuery::InitCon());
    if (!input.isEmpty())
    {
        query.prepare(
            QString("SELECT %1 ").arg(to_get) +
            "FROM capturecard, cardinput "
            "WHERE inputname        = :INNAME AND "
            "      cardinput.cardid = :CARDID AND "
            "      ( ( cardinput.childcardid != '0' AND "
            "          cardinput.childcardid  = capturecard.cardid ) OR "
            "        ( cardinput.childcardid  = '0' AND "
            "          cardinput.cardid       = capturecard.cardid ) )");
        query.bindValue(":INNAME", input);
    }
    else
    {
        query.prepare(
            QString("SELECT %1 ").arg(to_get) +
            "FROM capturecard "
            "WHERE capturecard.cardid = :CARDID");
    }
    query.bindValue(":CARDID", cardid);

    if (!query.exec() || !query.isActive())
        MythContext::DBError("CardUtil::get_on_input", query);
    else if (query.next())
        return query.value(0).toString();

    return QString::null;
}

bool set_on_source(const QString &to_set, uint cardid, uint sourceid,
                   const QString value)
{
    QString tmp = get_on_source("capturecard.cardid", cardid, sourceid);
    if (tmp.isEmpty())
        return false;

    bool ok;
    uint input_cardid = tmp.toUInt(&ok);
    if (!ok)
        return false;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        QString("UPDATE capturecard SET %1 = :VALUE ").arg(to_set) +
        "WHERE cardid = :CARDID");
    query.bindValue(":CARDID", input_cardid);
    query.bindValue(":VALUE",  value);

    if (query.exec())
        return true;

    MythContext::DBError("CardUtil::set_on_source", query);
    return false;
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

/** \fn CardUtil::IsDVB(uint, const QString&)
 *  \brief Returns true if the card is a DVB card
 *  \param cardid card id to check
 *  \param inputname input name of input to check
 *  \return true if the card is a DVB one
 */
bool CardUtil::IsDVB(uint cardid, const QString &inputname)
{
    return "DVB" == GetRawCardType(cardid, inputname);
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

QStringList CardUtil::GetInputNames(uint cardid, uint sourceid)
{
    QStringList list;
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT inputname "
                  "FROM cardinput "
                  "WHERE sourceid = :SOURCEID AND "
                  "      cardid   = :CARDID");
    query.bindValue(":SOURCEID", sourceid);
    query.bindValue(":CARDID",   cardid);

    if (!query.exec() || !query.isActive())
    {
        MythContext::DBError("CardUtil::GetInputNames()", query);
    }
    else
    {
        while (query.next())
            list = query.value(0).toString();
    }

    return list;
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

bool CardUtil::IsInNeedOfExternalInputConf(uint cardid)
{
    DiSEqCDev dev;
    DiSEqCDevTree *diseqc_tree = dev.FindTree(cardid);

    bool needsConf = false;
    if (diseqc_tree)
        needsConf = diseqc_tree->IsInNeedOfConf();

    return needsConf;
}

uint CardUtil::GetQuickTuning(uint cardid, const QString &input_name)
{
    uint quicktune = 0;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "SELECT quicktune "
        "FROM cardinput "
        "WHERE cardid    = :CARDID AND "
        "      inputname = :INPUTNAME");
    query.bindValue(":CARDID",    cardid);
    query.bindValue(":INPUTNAME", input_name);

    if (!query.exec() || !query.isActive())
        MythContext::DBError("CardUtil::GetQuickTuning()", query);
    else if (query.next())
        quicktune = query.value(0).toUInt();

    return quicktune;
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

bool CardUtil::GetV4LInfo(
    int videofd, QString &card, QString &driver, uint32_t &version)
{
    card = driver = QString::null;
    version = 0;

    if (videofd < 0)
        return false;

#ifdef USING_V4L
    // First try V4L2 query
    struct v4l2_capability capability;
    bzero(&capability, sizeof(struct v4l2_capability));
    if (ioctl(videofd, VIDIOC_QUERYCAP, &capability) >= 0)
    {
        card.setAscii((char*)capability.card);
        driver.setAscii((char*)capability.driver);
        version = capability.version;
    }
    else // Fallback to V4L1 query
    {
        struct video_capability capability;
        if (ioctl(videofd, VIDIOCGCAP, &capability) >= 0)
            card.setAscii((char*)capability.name);
    }
#endif // !USING_V4L

    return !card.isEmpty();
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

InputNames CardUtil::GetConfiguredDVBInputs(uint cardid)
{
    InputNames list;
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "SELECT cardinputid, inputname "
        "FROM cardinput "
        "WHERE cardid = :CARDID");
    query.bindValue(":CARDID", cardid);

    if (!query.exec() || !query.isActive())
        MythContext::DBError("CardUtil::GetConfiguredDVBInputs", query);
    else
    {
        while (query.next())
            list[query.value(0).toUInt()] = query.value(1).toString();
    }
    return list;
}

QStringList CardUtil::probeInputs(QString device, QString cardtype)
{
    QStringList ret;

    if (("FIREWIRE"  == cardtype) ||
        ("FREEBOX"   == cardtype) ||
        ("DBOX2"     == cardtype) ||
        ("HDHOMERUN" == cardtype))
    {
        ret += "MPEG2TS";
    }
    else if ("DVB" == cardtype)
        ret += probeDVBInputs(device);
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

QStringList CardUtil::probeDVBInputs(QString device)
{
    QStringList ret;

#ifdef USING_DVB
    int cardid = CardUtil::GetCardID(device);
    if (!cardid)
        return ret;

    InputNames list = GetConfiguredDVBInputs(cardid);
    InputNames::iterator it;
    for (it = list.begin(); it != list.end(); ++it)
    {
        if (it.key())
            ret += *it;
    }
#else
    (void) device;
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

QString CardUtil::GetDeviceLabel(uint cardid,
                                 QString cardtype,
                                 QString videodevice)
{
    QString label = QString::null;

    if (cardtype == "DBOX2")
    {
        MSqlQuery query(MSqlQuery::InitCon());
        query.prepare(
            "SELECT dbox2_host, dbox2_port, dbox2_httpport "
            "FROM capturecard "
            "WHERE cardid = :CARDID");
        query.bindValue(":CARDID", cardid);

        if (!query.exec() || !query.isActive() || !query.next())
            label = "[ DB ERROR ]";
        else
            label = QString("[ DBOX2 : IP %1 Port %2 HttpPort %3 ]")
                .arg(query.value(0).toString())
                .arg(query.value(1).toString())
                .arg(query.value(2).toString());
    }
    else if (cardtype == "HDHOMERUN")
    {
        MSqlQuery query(MSqlQuery::InitCon());
        query.prepare(
            "SELECT dbox2_port "
            "FROM capturecard "
            "WHERE cardid = :CARDID");
        query.bindValue(":CARDID", cardid);

        if (!query.exec() || !query.isActive() || !query.next())
            label = "[ DB ERROR ]";
        else
            label = QString("[ HDHomeRun : ID %1 Port %2 ]")
                .arg(videodevice).arg(query.value(0).toString());
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
    bool is_dtv = !IsEncoder(cardtype) && !IsUnscanable(cardtype);

    if (("FIREWIRE"  == cardtype) ||
        ("FREEBOX"   == cardtype) ||
        ("DBOX2"     == cardtype) ||
        ("HDHOMERUN" == cardtype))
    {
        inputs += "MPEG2TS";
    }
    else if ("DVB" != cardtype)
        inputs += probeV4LInputs(device);

    QString dev_label = (parentid) ? " -> " : "";
    dev_label += GetDeviceLabel(cardid, cardtype, device);

    QStringList::iterator it = inputs.begin();
    for (; it != inputs.end(); ++it)
    {
        CardInput* cardinput = new CardInput(is_dtv, false, false, cardid);
        cardinput->loadByInput(rcardid, (*it));
        cardinput->SetChildCardID((parentid) ? cardid : 0);
        inputLabels.push_back(
            dev_label + QString(" (%1) -> %2")
            .arg(*it).arg(cardinput->getSourceName()));
        cardInputs.push_back(cardinput);
    }

#ifdef USING_DVB
    if ("DVB" == cardtype)
    {
        InputNames list;
        list[0] = "DVBInput";
        bool needs_conf = IsInNeedOfExternalInputConf(rcardid);
        if (needs_conf)
            list = GetConfiguredDVBInputs(rcardid);

        InputNames::const_iterator it;
        for (it = list.begin(); it != list.end(); ++it)
        {
            CardInput *cardinput = new CardInput(is_dtv, true, false, rcardid);
            cardinput->loadByInput(rcardid, *it);
            cardinput->SetChildCardID(parentid ? cardid : 0);
            inputLabels.push_back(
                dev_label + QString(" (%1) -> %2")
                .arg(*it).arg(cardinput->getSourceName()));
            cardInputs.push_back(cardinput);            
        }
        
        // plus add one "new" input
        if (needs_conf)
        {
            CardInput *newcard = new CardInput(is_dtv, true, true, rcardid);
            QString newname = QString("DVBInput #%1").arg(list.size() + 1);
            newcard->loadByInput(rcardid, newname);
            newcard->SetChildCardID((parentid) ? cardid : 0);
            inputLabels.push_back(dev_label + " " + QObject::tr("New Input"));
            cardInputs.push_back(newcard);
        }
    }
#endif // USING_DVB

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

bool CardUtil::DeleteCard(uint cardid)
{
    if (!cardid)
        return true;

#ifdef USING_DVB
    // delete device tree
    DiSEqCDevTree tree;
    tree.Load(cardid);
    if (!tree.Root())
    {
        tree.SetRoot(NULL);
        tree.Store(cardid);
    }
#endif

    // delete any children
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "SELECT cardid "
        "FROM capturecard "
        "WHERE parentid = :CARDID");
    query.bindValue(":CARDID", cardid);
    if (!query.exec() || !query.isActive())
    {
        MythContext::DBError("DeleteCard -- find child cards", query);
        return false;
    }

    bool ok = true;
    while (query.next())
        ok &= DeleteCard(query.value(0).toUInt());

    if (!ok)
        return false;

    // delete all connected inputs
    query.prepare(
        "DELETE FROM cardinput "
        "WHERE cardid = :CARDID");
    query.bindValue(":CARDID", cardid);
    if (!query.exec())
    {
        MythContext::DBError("DeleteCard -- delete inputs", query);
        return false;
    }
        
    // delete the card itself
    query.prepare(
        "DELETE FROM capturecard "
        "WHERE cardid = :CARDID");
    query.bindValue(":CARDID", cardid);
    if (!query.exec())
    {
        MythContext::DBError("DeleteCard -- deleting capture card", query);
        return false;
    }

    return true;
}

QString CardUtil::GetDeviceName(dvb_dev_type_t type, uint cardnum)
{
    if (DVB_DEV_FRONTEND == type)
        return QString("/dev/dvb/adapter%1/frontend0").arg(cardnum);
    else if (DVB_DEV_DVR == type)
        return QString("/dev/dvb/adapter%1/dvr0").arg(cardnum);
    else if (DVB_DEV_DEMUX == type)
        return QString("/dev/dvb/adapter%1/demux0").arg(cardnum);
    else if (DVB_DEV_CA == type)
        return QString("/dev/dvb/adapter%1/ca0").arg(cardnum);
    else if (DVB_DEV_AUDIO == type)
        return QString("/dev/dvb/adapter%1/audio0").arg(cardnum);
    else if (DVB_DEV_VIDEO == type)
        return QString("/dev/dvb/adapter%1/video0").arg(cardnum);

    return "";
}
