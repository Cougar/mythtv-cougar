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

#ifndef _CHANNEL_SCANNER_CLI_H_
#define _CHANNEL_SCANNER_CLI_H_

// POSIX headers
#include <pthread.h>

// Qt headers
#include <qstring.h>

// MythTV headers
#include "settings.h"
#include "channelscanner.h"

class ChannelScannerCLI : public ChannelScanner
{
  public:
    ChannelScannerCLI(bool doScanSaveOnly, bool promptsOk);
    virtual ~ChannelScannerCLI();

    virtual void HandleEvent(const ScannerEvent *scanEvent);

    bool IsDone(void) const { return done; }

  protected:
    virtual void InformUser(const QString &error);
    virtual void Process(const ScanDTVTransportList&);
    virtual void MonitorProgress(bool lock, bool strength,
                                 bool snr, bool rotor);
  private:
    bool    done;
    bool    onlysavescan;
    bool    interactive;
    bool    status_lock;
    uint    status_complete;
    float   status_snr;
    QString status_text;
    QString status_last_log;
};

#endif // _CHANNEL_SCANNER_CLI_H_
