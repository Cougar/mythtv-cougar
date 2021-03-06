/* -*- Mode: c++ -*-
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
 *     functionallity
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

#ifndef _SCANWIZARDSCANNER_H_
#define _SCANWIZARDSCANNER_H_

// Standard UNIX C headers
#include <pthread.h>

// Qt headers
#include <qstring.h>
#include <QEvent>

// MythTV headers
#include "settings.h"
#include "dtvconfparser.h"
#include "signalmonitorlistener.h"

class ScanWizard;
class AnalogScan;
class IPTVChannelFetcher;
class LogList;
class SIScan;
class ScanProgressPopup;

class ChannelBase;
class V4LChannel;
class DVBChannel;
class SignalMonitorValue;

class ScanWizardScanner :
    public VerticalConfigurationGroup,
    public DVBSignalMonitorListener
{
    Q_OBJECT

    friend void *spawn_popup(void*);

  public:
    ScanWizardScanner(void);
    virtual void deleteLater(void)
        { Teardown(); VerticalConfigurationGroup::deleteLater(); }

    void Scan(int            scantype,
              uint           cardid,
              const QString &inputname,
              uint           sourceid,
              bool           do_delete_channels,
              bool           do_rename_channels,
              bool           do_ignore_signal_timeout,
              // stuff needed for particular scans
              uint           mplexid,
              const QMap<QString,QString> &startChan,
              const QString &freq_std,
              const QString &mod,
              const QString &tbl,
              const QString &atsc_format);

    void ImportDVBUtils(uint sourceid, int cardtype, const QString &file);
    void ImportM3U(     uint cardid, const QString &inputname, uint sourceid);

    // SignalMonitorListener
    virtual void AllGood(void) { }
    virtual void StatusSignalLock(const SignalMonitorValue&);
    virtual void StatusSignalStrength(const SignalMonitorValue&);

    // DVBSignalMonitorListener
    virtual void StatusSignalToNoise(const SignalMonitorValue&);
    virtual void StatusBitErrorRate(const SignalMonitorValue&) { }
    virtual void StatusUncorrectedBlocks(const SignalMonitorValue&) { }
    virtual void StatusRotorPosition(const SignalMonitorValue&) { }

  protected slots:
    void CancelScan(void) { Teardown(); }
    void scanComplete(void);
    void transportScanComplete(void);
    void updateText(const QString& status);
    void updateStatusText(const QString& status);

    void serviceScanPctComplete(int pct);

  protected:
    ~ScanWizardScanner();
    void Teardown(void);

    void PreScanCommon(int scantype, uint cardid,
                       const QString &inputname,
                       uint sourceid, bool do_ignore_signal_timeout);

    void dvbLock(int);
    void dvbSNR(int);
    void dvbSignalStrength(int);
    void customEvent(QEvent *e);

    void MonitorProgress(bool lock, bool strength, bool snr);
    void RunPopup(void);
    void StopPopup(void);

  public:
    static QString kTitle;

  private:
    LogList           *log;
    ChannelBase       *channel;

    ScanProgressPopup  *popupProgress;
    pthread_t           popup_thread;
    mutable QMutex      popupLock;

    SIScan             *scanner;
    IPTVChannelFetcher *freeboxScanner;

    uint               nVideoSource;

    // dvb-utils imported channels
    DTVChannelList channels;
};

#endif // _SCANWIZARDSCANNER_H_

