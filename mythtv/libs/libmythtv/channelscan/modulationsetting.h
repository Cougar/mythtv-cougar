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

#ifndef _MODULATION_SETTING_H_
#define _MODULATION_SETTING_H_

#include "settings.h"

class ScanATSCModulation: public ComboBoxSetting, public TransientStorage
{
  public:
    ScanATSCModulation() : ComboBoxSetting(this)
    {
        addSelection(QObject::tr("Terrestrial")+" (8-VSB)","vsb8",  true);
        addSelection(QObject::tr("Cable") + " (QAM-256)", "qam256", false);
        addSelection(QObject::tr("Cable") + " (QAM-128)", "qam128", false);
        addSelection(QObject::tr("Cable") + " (QAM-64)",  "qam64",  false);

        setLabel(QObject::tr("Modulation"));
        setHelpText(
            QObject::tr("Modulation, 8-VSB, QAM-256, etc.") + " " +
            QObject::tr("Most cable systems in the United States use "
                        "QAM-256 or QAM-64, but some mixed systems "
                        "may use 8-VSB for over-the-air channels."));
    }
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

#endif // _MODULATION_SETTING_H_

