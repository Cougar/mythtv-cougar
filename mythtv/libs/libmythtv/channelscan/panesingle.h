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

#ifndef _PANE_SINGLE_H_
#define _PANE_SINGLE_H_

// MythTV headers
#include "channelscanmiscsettings.h"
#include "multiplexsetting.h"

class PaneSingle : public VerticalConfigurationGroup
{
  public:
    PaneSingle() :
        VerticalConfigurationGroup(false, false, true, false),
        transport_setting(new MultiplexSetting()),
        ignore_signal_timeout(new IgnoreSignalTimeout())
    {
        addChild(transport_setting);
        addChild(ignore_signal_timeout);
    }

    int  GetMultiplex(void) const
        { return transport_setting->getValue().toInt(); }
    bool ignoreSignalTimeout(void) const
        { return ignore_signal_timeout->getValue().toInt(); }

    void SetSourceID(uint sourceid)
        { transport_setting->SetSourceID(sourceid); }

  protected:
    MultiplexSetting        *transport_setting;
    IgnoreSignalTimeout     *ignore_signal_timeout;
};

#endif // _PANE_SINGLE_H_
