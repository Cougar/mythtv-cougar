/* -*- Mode: c++ -*-
 * vim: set expandtab tabstop=4 shiftwidth=4:
 *
 * Original Project
 *      MythTV      http://www.mythtv.org
 *
 * Copyright (c) 2004, 2005 John Pullan <john@pullan.org>
 * Copyright (c) 2005 - 2007 Daniel Kristjansson
 *
 * Description:
 *     Collection of classes to provide channel scanning functionallity
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

#ifndef _SCAN_WIZARD_CONFIG_H_
#define _SCAN_WIZARD_CONFIG_H_

// MythTV headers
#include "settings.h"
#include "inputselectorsetting.h"

class ScanWizard;
class VideoSourceSelector;
class ScanTypeSetting;
class ScanCountry;
class IgnoreSignalTimeout;

class PaneATSC;
class PaneAnalog;
class PaneDVBT;
class PaneDVBC;
class PaneDVBS;
class PaneDVBS2;
class PaneSingle;
class PaneDVBUtilsImport;

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
        FullScan_DVBT,
        // Scans starting on one frequency that adds each transport
        // seen in the Network Information Tables to the scan.
        NITAddScan_DVBT,
        NITAddScan_DVBS,
        NITAddScan_DVBC,
        // Scan of all transports already in the database
        FullTransportScan,
        // Scan of one transport already in the database
        TransportScan,
        // IPTV import of channels from M3U URL
        IPTVImport,
        // Imports lists from dvb-utils scanners
        DVBUtilsImport,
    };

    ScanTypeSetting() : ComboBoxSetting(this), hw_cardid(0)
        { setLabel(QObject::tr("Scan Type")); }

  protected slots:
    void SetInput(const QString &cardids_inputname);

  protected:
    uint    hw_cardid;
};

class ScanOptionalConfig : public TriggeredConfigurationGroup 
{
    Q_OBJECT

  public:
    ScanOptionalConfig(ScanTypeSetting *_scan_type);

    QString GetFrequencyStandard(void)       const;
    QString GetModulation(void)              const;
    QString GetFrequencyTable(void)          const;
    bool    DoIgnoreSignalTimeout(void)      const;
    QString GetFilename(void)                const;
    uint    GetMultiplex(void)               const;
    QMap<QString,QString> GetStartChan(void) const;

  public slots:
    void SetSourceID(const QString&);
    void triggerChanged(const QString&);

  private:
    ScanTypeSetting      *scanType;
    ScanCountry          *country;
    IgnoreSignalTimeout  *ignoreSignalTimeoutAll;
    PaneDVBT             *paneDVBT;
    PaneDVBS             *paneDVBS;
    PaneDVBS2            *paneDVBS2;
    PaneATSC             *paneATSC;
    PaneDVBC             *paneDVBC;
    PaneAnalog           *paneAnalog;
    PaneSingle           *paneSingle;
    PaneDVBUtilsImport   *paneDVBUtilsImport;
};

class ScanWizardConfig: public VerticalConfigurationGroup
{
    Q_OBJECT

  public:
    ScanWizardConfig(ScanWizard *_parent,
                     uint        default_sourceid,
                     uint        default_cardid,
                     QString     default_inputname);

    uint    GetSourceID(void)     const;
    QString GetModulation(void)   const { return scanConfig->GetModulation(); }
    int     GetScanType(void)     const { return scanType->getValue().toInt();}
    uint    GetCardID(void)       const { return input->GetCardID();          }
    QString GetInputName(void)    const { return input->GetInputName();       }
    QString GetFilename(void)     const { return scanConfig->GetFilename();   }
    uint    GetMultiplex(void)    const { return scanConfig->GetMultiplex();  }

    QString GetFrequencyStandard(void) const
        { return scanConfig->GetFrequencyStandard(); }
    QString GetFrequencyTable(void) const
        { return scanConfig->GetFrequencyTable(); }
    QMap<QString,QString> GetStartChan(void) const
        { return scanConfig->GetStartChan(); }
    bool    DoIgnoreSignalTimeout(void) const
        { return scanConfig->DoIgnoreSignalTimeout(); }

  protected:
    VideoSourceSelector *videoSource;
    InputSelector       *input;
    ScanTypeSetting     *scanType;
    ScanOptionalConfig  *scanConfig;
};

#endif // _SCAN_WIZARD_CONFIG_H_
