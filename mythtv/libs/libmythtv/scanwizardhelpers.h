/* -*- Mode: c++ -*-
 * $Id$
 * vim: set expandtab tabstop=4 shiftwidth=4:
 *
 * Original Project
 *      MythTV      http://www.mythtv.org
 *
 * Author(s):
 *      John Pullan  (john@pullan.org)
 *
 * Description:
 *     Collection of classes to provide dvb channel scanning
 *     functionality
 *
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 * Or, point your browser to http://www.gnu.org/copyleft/gpl.html
 *
 */

#ifndef SCANWIZARDHELPERS_H
#define SCANWIZARDHELPERS_H

#include "settings.h"

class TransLabelSetting;
class ScanWizardScanner;
class ScanWizard;
class OptionalTypeSetting;

/// Max range of the ScanProgressPopup progress bar
#define PROGRESS_MAX  1000

class ScanSignalMeter: public ProgressSetting, public TransientStorage
{
  public:
    ScanSignalMeter(int steps): ProgressSetting(this, steps) {};
};

class ScanProgressPopup : public ConfigurationPopupDialog
{
    Q_OBJECT

  protected:
    ScanSignalMeter   *ss;
    ScanSignalMeter   *sn;
    ScanSignalMeter   *progressBar;

    TransLabelSetting *sl;
    TransLabelSetting *sta;

  public:
    ScanProgressPopup(ScanWizardScanner *parent, bool signalmonitors = true);
    ~ScanProgressPopup();
    void exec(ScanWizardScanner *parent);
    void signalToNoise(int value);
    void signalStrength(int value);
    void dvbLock(int value);
    void status(const QString& value);

    void progress(int value) { progressBar->setValue(value);}
    void incrementProgress() { progress(progressBar->getValue().toInt()+1);}
};

class ScannerEvent : public QCustomEvent
{
  public:
    enum TYPE 
    {
        ServiceScanComplete,
        Update,
        TableLoaded,
        ServicePct,
        DVBSNR,
        DVBSignalStrength,
        DVBLock,
        TuneComplete
    };
    enum TUNING
    {
        OK,
        ERROR_TUNE
    };

    ScannerEvent(TYPE t) : QCustomEvent(t + QEvent::User) { ; }

    QString strValue()              const { return str; }
    void    strValue(const QString& _str) { str = _str; }

    int     intValue()        const { return intvalue; }
    void    intValue(int _intvalue) { intvalue = _intvalue; }

    TYPE    eventType()       const { return (TYPE)(type()-QEvent::User); }

  protected:
    QString str;
    int     intvalue;
};

// ///////////////////////////////
// Settings Below Here
// ///////////////////////////////

class VideoSourceSetting: public ComboBoxSetting, public Storage
{
  public:
    VideoSourceSetting(int srcid) :
        ComboBoxSetting(this), sourceid(srcid)
    {
        setLabel(tr("Video Source"));
    }

    virtual void load();
    virtual void save() {}
    virtual void save(QString /*destination*/) { }

  private:
    int sourceid;
};

class MultiplexSetting : public ComboBoxSetting, public Storage
{
    Q_OBJECT
  protected:
    int nSourceID;

  public:
    MultiplexSetting() : ComboBoxSetting(this), nSourceID(0)
        { setLabel(tr("Transport")); }

    virtual void load() { refresh(); }
    virtual void save() { ; }
    virtual void save(QString /*destination*/) { }

    void refresh();
  public slots:
    void sourceID(const QString& str);
};

class IgnoreSignalTimeout : public CheckBoxSetting, public TransientStorage
{
  public:
    IgnoreSignalTimeout() : CheckBoxSetting(this)
    {
        setLabel(QObject::tr("Ignore Signal Timeout"));
        setHelpText(
            QObject::tr("This option allows you to slow down the scan for "
                        "broken drivers, such as the DVB drivers for the "
                        "Leadtek LR6650 DVB card."));
    }
};

class CaptureCardSetting : public ComboBoxSetting, public Storage
{
    Q_OBJECT
  protected:
    int nSourceID;

  public:
    CaptureCardSetting() : ComboBoxSetting(this), nSourceID(0)
    {
        setLabel(tr("Capture Card"));
    }
    virtual void load() { refresh(); }
    virtual void save() { ; }
    virtual void save(QString /*destination*/) { }

    void refresh();

  public slots:
    void sourceID(const QString& str);
};

class ScanCountry: public ComboBoxSetting, public TransientStorage
{
    Q_OBJECT
  public:
    enum Country
    {
        AU,
        FI,
        SE,
        UK,
        DE,
        ES,
    };

    ScanCountry();
};

class ScanTypeSetting : public ComboBoxSetting, public TransientStorage
{
    Q_OBJECT
  public:
    enum Type
    {
        Error_Open = 0,
        Error_Probe,
        // Scans that check each frequency in a predefined list
        FullScan_Analog,
        FullScan_ATSC,
        FullScan_OFDM,
        // Scans starting on one frequency that adds each transport
        // seen in the Network Information Tables to the scan.
        NITAddScan_OFDM,
        NITAddScan_QPSK,
        NITAddScan_QAM,
        // Scan of all transports already in the database
        FullTransportScan,
        // Scan of one transport already in the database
        TransportScan,
        // IPTV import of channels from M3U URL
        IPTVImport,
        // Imports lists from dvb-utils scanners
        DVBUtilsImport,
    };

    ScanTypeSetting() : ComboBoxSetting(this), nCaptureCard(-1)
    {
        setLabel(QObject::tr("Scan Type"));
        refresh("");
    }
  protected slots:
    void refresh(const QString&);
  protected:
    int nCaptureCard;
};

class ScanOptionalConfig : public TriggeredConfigurationGroup 
{
    Q_OBJECT
  public:
    ScanOptionalConfig(ScanWizard* wizard, ScanTypeSetting* scanType);

    ScanCountry         *country;
    IgnoreSignalTimeout *ignoreSignalTimeoutAll;

  protected slots:
    void triggerChanged(const QString&);
};

class ScanWizardScanType: public VerticalConfigurationGroup
{
    Q_OBJECT
    friend class ScanWizard;
  public:
    ScanWizardScanType(ScanWizard *_parent, int sourceid);

  protected:
    ScanWizard *parent;

    ScanOptionalConfig *scanConfig;
    CaptureCardSetting *capturecard;
    VideoSourceSetting *videoSource;
    ScanTypeSetting *scanType;
};

class LogList: public ListBoxSetting, public TransientStorage
{
  public:
    LogList();

    void updateText(const QString& status);
  protected:
    int n;
};

class ScanFrequencyTable: public ComboBoxSetting, public TransientStorage
{
  public:
    ScanFrequencyTable() : ComboBoxSetting(this)
    {
        addSelection(QObject::tr("Broadcast"),        "us",          true);
        addSelection(QObject::tr("Cable")     + " " +
                     QObject::tr("High"),             "uscablehigh", false);
        addSelection(QObject::tr("Cable HRC") + " " +
                     QObject::tr("High"),             "ushrchigh",   false);
        addSelection(QObject::tr("Cable IRC") + " " +
                     QObject::tr("High"),             "usirchigh",   false);
        addSelection(QObject::tr("Cable"),            "uscable",     false);
        addSelection(QObject::tr("Cable HRC"),        "ushrc",       false);
        addSelection(QObject::tr("Cable IRC"),        "usirc",       false);

        setLabel(QObject::tr("Frequency Table"));
        setHelpText(QObject::tr("Frequency table to use.") + " " +
                    QObject::tr(
                        "The option of scanning only \"High\" "
                        "frequency channels is useful because most "
                        "digital channels are on the higher frequencies."));
    }
};

class ScanATSCModulation: public ComboBoxSetting, public TransientStorage
{
  public:
    ScanATSCModulation() : ComboBoxSetting(this)
    {
        addSelection(QObject::tr("Terrestrial")+" (8-VSB)","vsb8",  true);
        addSelection(QObject::tr("Cable") + " (QAM-256)", "qam256", false);
        addSelection(QObject::tr("Cable") + " (QAM-128)", "qam128", false);
        addSelection(QObject::tr("Cable") + " (QAM-64)",  "qam64",  false);

        setLabel(QObject::tr("ATSC Modulation"));
        setHelpText(
            QObject::tr("ATSC modulation, 8-VSB, QAM-256, etc.") + " " +
            QObject::tr("Most cable systems in the United States use "
                        "QAM-256 or QAM-64, but some mixed systems "
                        "may use 8-VSB for over-the-air channels."));
    }
};

class ScanATSCChannelFormat: public ComboBoxSetting, public TransientStorage
{
  public:
    ScanATSCChannelFormat() : ComboBoxSetting(this)
    {
        addSelection(QObject::tr("(5_1) Underscore"), "%1_%2", true);
        addSelection(QObject::tr("(5-1) Minus"),      "%1-%2", false);
        addSelection(QObject::tr("(5.1) Period"),     "%1.%2", false);
        addSelection(QObject::tr("(501) Zero"),       "%10%2", false);
        addSelection(QObject::tr("(51) None"),        "%1%2",  false);
        setLabel(QObject::tr("Channel Separator"));
        setHelpText(QObject::tr("What to use to separate ATSC major "
                                "and minor channels."));
    }
};

class ScanOldChannelTreatment: public ComboBoxSetting, public TransientStorage
{
  public:
    ScanOldChannelTreatment() : ComboBoxSetting(this)
    {
        addSelection(QObject::tr("Minimal Updates"),    "minimal");
        addSelection(QObject::tr("Rename to Match"),    "rename");
        addSelection(QObject::tr("Delete"),             "delete");
        setLabel(QObject::tr("Existing Channel Treatment"));
        setHelpText(QObject::tr("How to treat existing channels."));
    }
};

class ScanFrequency: public LineEditSetting, public TransientStorage
{
  public:
    ScanFrequency() : LineEditSetting(this)
    {
        setLabel(QObject::tr("Frequency"));
        setHelpText(QObject::tr("Frequency (Option has no default)\n"
                                "The frequency for this channel in Hz."));
    };
};

class ScanSymbolRate: public LineEditSetting, public TransientStorage
{
  public:
    ScanSymbolRate() : LineEditSetting(this)
    {
        setLabel(QObject::tr("Symbol Rate"));
        setHelpText(QObject::tr("Symbol Rate (Option has no default)"));
    };
};

class ScanPolarity: public ComboBoxSetting, public TransientStorage
{
  public:
    ScanPolarity() : ComboBoxSetting(this)
    {
        setLabel(QObject::tr("Polarity"));
        setHelpText(QObject::tr("Polarity (Option has no default)"));
        addSelection(QObject::tr("Horizontal"), "h",true);
        addSelection(QObject::tr("Vertical"), "v");
        addSelection(QObject::tr("Right Circular"), "r");
        addSelection(QObject::tr("Left Circular"), "l");
    };
};

class ScanInversion: public ComboBoxSetting, public TransientStorage
{
  public:
    ScanInversion() : ComboBoxSetting(this)
    {
        setLabel(QObject::tr("Inversion"));
        setHelpText(QObject::tr(
                        "Inversion (Default: Auto):\n"
                        "Most cards can autodetect this now, so "
                        "leave it at Auto unless it won't work."));
        addSelection(QObject::tr("Auto"), "a",true);
        addSelection(QObject::tr("On"), "1");
        addSelection(QObject::tr("Off"), "0");
    };
};

class ScanBandwidth: public ComboBoxSetting, public TransientStorage
{
  public:
    ScanBandwidth() : ComboBoxSetting(this)
    {
        setLabel(QObject::tr("Bandwidth"));
        setHelpText(QObject::tr("Bandwidth (Default: Auto)\n"));
        addSelection(QObject::tr("Auto"),"a",true);
        addSelection(QObject::tr("6 MHz"),"6");
        addSelection(QObject::tr("7 MHz"),"7");
        addSelection(QObject::tr("8 MHz"),"8");
    };
};

class ScanModulationSetting: public ComboBoxSetting
{
  public:
    ScanModulationSetting(Storage *_storage) : ComboBoxSetting(_storage)
    {
        addSelection(QObject::tr("Auto"),"auto",true);
        addSelection("QPSK","qpsk");
#ifdef FE_GET_EXTENDED_INFO
        addSelection("8PSK","8psk");
#endif
        addSelection("QAM 16","qam_16");
        addSelection("QAM 32","qam_32");
        addSelection("QAM 64","qam_64");
        addSelection("QAM 128","qam_128");
        addSelection("QAM 256","qam_256");
    };
};

class ScanModulation: public ScanModulationSetting, public TransientStorage
{
  public:
    ScanModulation() : ScanModulationSetting(this)
    {
        setLabel(QObject::tr("Modulation"));
        setHelpText(QObject::tr("Modulation (Default: Auto)"));
    };
};

class ScanConstellation: public ScanModulationSetting,
                         public TransientStorage
{
  public:
    ScanConstellation() : ScanModulationSetting(this)
    {
        setLabel(QObject::tr("Constellation"));
        setHelpText(QObject::tr("Constellation (Default: Auto)"));
    };
};

class ScanFecSetting: public ComboBoxSetting
{
  public:
    ScanFecSetting(Storage *_storage) : ComboBoxSetting(_storage)
    {
        addSelection(QObject::tr("Auto"),"auto",true);
        addSelection(QObject::tr("None"),"none");
        addSelection("1/2");
        addSelection("2/3");
        addSelection("3/4");
        addSelection("4/5");
        addSelection("5/6");
        addSelection("6/7");
        addSelection("7/8");
        addSelection("8/9");
    }
};

class ScanFec: public ScanFecSetting, public TransientStorage
{
  public:
    ScanFec() : ScanFecSetting(this)
    {
        setLabel(QObject::tr("FEC"));
        setHelpText(QObject::tr(
                        "Forward Error Correction (Default: Auto)"));
    }
};

class ScanCodeRateLP: public ScanFecSetting, public TransientStorage
{
  public:
    ScanCodeRateLP() : ScanFecSetting(this)
    {
        setLabel(QObject::tr("LP Coderate"));
        setHelpText(QObject::tr("Low Priority Code Rate (Default: Auto)"));
    }
};

class ScanCodeRateHP: public ScanFecSetting, public TransientStorage
{
  public:
    ScanCodeRateHP() : ScanFecSetting(this)
    {
        setLabel(QObject::tr("HP Coderate"));
        setHelpText(QObject::tr("High Priority Code Rate (Default: Auto)"));
    };
};

class ScanGuardInterval: public ComboBoxSetting, public TransientStorage
{
  public:
    ScanGuardInterval() : ComboBoxSetting(this)
    {
        setLabel(QObject::tr("Guard Interval"));
        setHelpText(QObject::tr("Guard Interval (Default: Auto)"));
        addSelection(QObject::tr("Auto"),"auto");
        addSelection("1/4");
        addSelection("1/8");
        addSelection("1/16");
        addSelection("1/32");
    };
};

class ScanTransmissionMode: public ComboBoxSetting, public TransientStorage
{
  public:
    ScanTransmissionMode() : ComboBoxSetting(this)
    {
        setLabel(QObject::tr("Trans. Mode"));
        setHelpText(QObject::tr("Transmission Mode (Default: Auto)"));
        addSelection(QObject::tr("Auto"),"a");
        addSelection("2K","2");
        addSelection("8K","8");
    };
};

class ScanHierarchy: public ComboBoxSetting, public TransientStorage
{
    public:
    ScanHierarchy() : ComboBoxSetting(this)
    {
        setLabel(QObject::tr("Hierarchy"));
        setHelpText(QObject::tr("Hierarchy (Default: Auto)"));
        addSelection(QObject::tr("Auto"),"a");
        addSelection(QObject::tr("None"), "n");
        addSelection("1");
        addSelection("2");
        addSelection("4");
    };
};

class OFDMPane : public HorizontalConfigurationGroup
{
  public:
    OFDMPane() : HorizontalConfigurationGroup(false, false, true, true)
    {
        setUseFrame(false);
        VerticalConfigurationGroup *left =
            new VerticalConfigurationGroup(false,true,true,false);
        VerticalConfigurationGroup *right =
            new VerticalConfigurationGroup(false,true,true,false);
        left->addChild(pfrequency       = new ScanFrequency());
        left->addChild(pbandwidth       = new ScanBandwidth());
        left->addChild(pinversion       = new ScanInversion());
        left->addChild(pconstellation   = new ScanConstellation());
        right->addChild(pcoderate_lp    = new ScanCodeRateLP());
        right->addChild(pcoderate_hp    = new ScanCodeRateHP());
        right->addChild(ptrans_mode     = new ScanTransmissionMode());
        right->addChild(pguard_interval = new ScanGuardInterval());
        right->addChild(phierarchy      = new ScanHierarchy());
        addChild(left);
        addChild(right);
    }

    QString frequency()      { return pfrequency->getValue(); }
    QString bandwidth()      { return pbandwidth->getValue(); }
    QString inversion()      { return pinversion->getValue(); }
    QString constellation()  { return pconstellation->getValue(); }
    QString coderate_lp()    { return pcoderate_lp->getValue(); }
    QString coderate_hp()    { return pcoderate_hp->getValue(); }
    QString trans_mode()     { return ptrans_mode->getValue(); }
    QString guard_interval() { return pguard_interval->getValue(); }
    QString hierarchy()      { return phierarchy->getValue(); }

  protected:
    ScanFrequency        *pfrequency;
    ScanInversion        *pinversion;
    ScanBandwidth        *pbandwidth;
    ScanConstellation    *pconstellation;
    ScanCodeRateLP       *pcoderate_lp;
    ScanCodeRateHP       *pcoderate_hp;
    ScanTransmissionMode *ptrans_mode;
    ScanGuardInterval    *pguard_interval;
    ScanHierarchy        *phierarchy;
};
#ifdef FE_GET_EXTENDED_INFO
class DVBS2Pane : public HorizontalConfigurationGroup
{
  public:
    DVBS2Pane() : HorizontalConfigurationGroup(false,false,true,false) 
    {
        setUseFrame(false);
        VerticalConfigurationGroup *left =
            new VerticalConfigurationGroup(false,true);
        VerticalConfigurationGroup *right =
            new VerticalConfigurationGroup(false,true);
        left->addChild( pfrequency  = new ScanFrequency());
        left->addChild( ppolarity   = new ScanPolarity());
        left->addChild( psymbolrate = new ScanSymbolRate());
        right->addChild(pfec        = new ScanFec());
        right->addChild(pmodulation = new ScanModulation());
        right->addChild(pinversion  = new ScanInversion());
        addChild(left);
        addChild(right);     
    }

    QString frequency(void)  const { return pfrequency->getValue();  }
    QString symbolrate(void) const { return psymbolrate->getValue(); }
    QString inversion(void)  const { return pinversion->getValue();  }
    QString fec(void)        const { return pfec->getValue();        }
    QString polarity(void)   const { return ppolarity->getValue();   }
    QString modulation(void) const { return pmodulation->getValue(); }

  protected:
    ScanFrequency  *pfrequency;
    ScanSymbolRate *psymbolrate;
    ScanInversion  *pinversion;
    ScanFec        *pfec;
    ScanPolarity   *ppolarity;
    ScanModulation *pmodulation;
};
#endif

class QPSKPane : public HorizontalConfigurationGroup
{
  public:
    QPSKPane() : HorizontalConfigurationGroup(false, false, true, false)
    {
        setUseFrame(false);
        VerticalConfigurationGroup *left =
            new VerticalConfigurationGroup(false,true);
        VerticalConfigurationGroup *right =
            new VerticalConfigurationGroup(false,true);
        left->addChild(pfrequency  = new ScanFrequency());
        left->addChild(ppolarity   = new ScanPolarity());
        left->addChild(psymbolrate = new ScanSymbolRate());
        right->addChild(pfec       = new ScanFec());
        right->addChild(pinversion = new ScanInversion());
        addChild(left);
        addChild(right);     
    }

    QString frequency()  { return pfrequency->getValue(); }
    QString symbolrate() { return psymbolrate->getValue(); }
    QString inversion()  { return pinversion->getValue(); }
    QString fec()        { return pfec->getValue(); }
    QString polarity()   { return ppolarity->getValue(); }

  protected:
    ScanFrequency  *pfrequency;
    ScanSymbolRate *psymbolrate;
    ScanInversion  *pinversion;
    ScanFec        *pfec;
    ScanPolarity   *ppolarity;
};

class QAMPane : public HorizontalConfigurationGroup
{
  public:
    QAMPane() : HorizontalConfigurationGroup(false, false, true, false)
    {
        setUseFrame(false);
        VerticalConfigurationGroup *left =
            new VerticalConfigurationGroup(false,true);
        VerticalConfigurationGroup *right =
            new VerticalConfigurationGroup(false,true);
        left->addChild(pfrequency   = new ScanFrequency());
        left->addChild(psymbolrate  = new ScanSymbolRate());
        left->addChild(pinversion   = new ScanInversion());
        right->addChild(pmodulation = new ScanModulation());
        right->addChild(pfec        = new ScanFec());
        addChild(left);
        addChild(right);     
    }

    QString frequency()  { return pfrequency->getValue(); }
    QString symbolrate() { return psymbolrate->getValue(); }
    QString inversion()  { return pinversion->getValue(); }
    QString fec()        { return pfec->getValue(); }
    QString modulation() { return pmodulation->getValue(); }

  protected:
    ScanFrequency  *pfrequency;
    ScanSymbolRate *psymbolrate;
    ScanInversion  *pinversion;
    ScanModulation *pmodulation;
    ScanFec        *pfec;
};

class ATSCPane : public VerticalConfigurationGroup
{
  public:
    ATSCPane() : VerticalConfigurationGroup(false, false, true, false)
    {
        addChild(atsc_table            = new ScanFrequencyTable());
        addChild(atsc_modulation       = new ScanATSCModulation());
        addChild(atsc_format           = new ScanATSCChannelFormat());
        addChild(old_channel_treatment = new ScanOldChannelTreatment());
    }

    QString atscFreqTable(void)  const { return atsc_table->getValue();      }
    QString atscModulation(void) const { return atsc_modulation->getValue(); }
    QString atscFormat(void)     const { return atsc_format->getValue();     }
    bool DoDeleteChannels(void) const
        { return old_channel_treatment->getValue() == "delete"; }
    bool DoRenameChannels(void) const
        { return old_channel_treatment->getValue() == "rename"; }

    void SetDefaultFormat(QString d)
    {
        int val = atsc_format->getValueIndex(d);
        atsc_format->setValue(val);
    }

  protected:
    ScanFrequencyTable      *atsc_table;
    ScanATSCModulation      *atsc_modulation;
    ScanATSCChannelFormat   *atsc_format;
    ScanOldChannelTreatment *old_channel_treatment;
};

class STPane : public VerticalConfigurationGroup
{
    Q_OBJECT
  public:
    STPane() :
        VerticalConfigurationGroup(false, false, true, false),
        transport_setting(new MultiplexSetting()),
        atsc_format(new ScanATSCChannelFormat()),
        old_channel_treatment(new ScanOldChannelTreatment()),
        ignore_signal_timeout(new IgnoreSignalTimeout())
    {
        addChild(transport_setting);
        addChild(atsc_format);
        addChild(old_channel_treatment);
        addChild(ignore_signal_timeout);
    }

    QString atscFormat(void) const { return atsc_format->getValue(); }
    bool DoDeleteChannels(void) const
        { return old_channel_treatment->getValue() == "delete"; }
    bool DoRenameChannels(void) const
        { return old_channel_treatment->getValue() == "rename"; }
    int  GetMultiplex(void) const
        { return transport_setting->getValue().toInt(); }
    bool ignoreSignalTimeout(void) const
        { return ignore_signal_timeout->getValue().toInt(); }

  public slots:
    void sourceID(const QString &str)
        { transport_setting->sourceID(str); }

  protected:
    MultiplexSetting        *transport_setting;
    ScanATSCChannelFormat   *atsc_format;
    ScanOldChannelTreatment *old_channel_treatment;
    IgnoreSignalTimeout     *ignore_signal_timeout;
};

class DVBUtilsImportPane : public VerticalConfigurationGroup
{
  public:
    DVBUtilsImportPane() :
        VerticalConfigurationGroup(false,false,true,false),
        filename(new TransLineEditSetting()),
        atsc_format(new ScanATSCChannelFormat()),
        old_channel_treatment(new ScanOldChannelTreatment()),
        ignore_signal_timeout(new IgnoreSignalTimeout())
    {
        filename->setLabel(tr("File location"));
        filename->setHelpText(tr("Location of the channels.conf file."));
        addChild(filename);

        addChild(atsc_format);
        addChild(old_channel_treatment);
        addChild(ignore_signal_timeout);
    }

    QString GetFilename(void)   const { return filename->getValue();    }
    QString GetATSCFormat(void) const { return atsc_format->getValue(); }

    bool DoDeleteChannels(void) const
        { return old_channel_treatment->getValue() == "delete"; }
    bool DoRenameChannels(void) const
        { return old_channel_treatment->getValue() == "rename"; }
    bool DoIgnoreSignalTimeout(void) const
        { return ignore_signal_timeout->getValue().toInt(); }

  private:
    TransLineEditSetting    *filename;
    ScanATSCChannelFormat   *atsc_format;
    ScanOldChannelTreatment *old_channel_treatment;
    IgnoreSignalTimeout     *ignore_signal_timeout;
};

class ErrorPane : public HorizontalConfigurationGroup
{
  public:
    ErrorPane(const QString &error) :
        HorizontalConfigurationGroup(false, false, true, false)
    {
        TransLabelSetting* label = new TransLabelSetting();
        label->setValue(error);
        addChild(label);
    }
};

class BlankSetting: public TransLabelSetting
{
  public:
    BlankSetting() : TransLabelSetting()
    {
        setLabel("");
    }
};

#endif // SCANWIZARDHELPERS_H
