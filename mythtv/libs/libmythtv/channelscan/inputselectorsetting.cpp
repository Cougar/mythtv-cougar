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

#include "inputselectorsetting.h"
#include "cardutil.h"

InputSelector::InputSelector(
    uint _default_cardid, const QString &_default_inputname) :
    ComboBoxSetting(this), sourceid(0), default_cardid(_default_cardid),
    default_inputname(Q3DeepCopy<QString>(_default_inputname))
{
    setLabel(tr("Input"));
}

void InputSelector::load(void)
{
    clearSelections();

    if (!sourceid)
        return;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "SELECT capturecard.cardid, cardtype, videodevice, inputname "
        "FROM capturecard, cardinput, videosource "
        "WHERE cardinput.sourceid = videosource.sourceid AND "
        "      hostname           = :HOSTNAME            AND "
        "      cardinput.sourceid = :SOURCEID            AND "
        "      cardinput.cardid   = capturecard.cardid");

    query.bindValue(":HOSTNAME", gContext->GetHostName());
    query.bindValue(":SOURCEID", sourceid);

    if (!query.exec() || !query.isActive())
    {
        MythContext::DBError("InputSelector::load()", query);
        return;
    }

    uint which = 0, cnt = 0;
    for (; query.next(); cnt++)
    {
        uint    cardid     = query.value(0).toUInt();
        QString inputname  = query.value(3).toString();

        QString desc = CardUtil::GetDeviceLabel(
            cardid, query.value(1).toString(), query.value(2).toString());

        desc += QString(" (%1)").arg(inputname);

        QString key = QString("%1:%2").arg(cardid).arg(inputname);

        addSelection(desc, key);

        which = (default_cardid == cardid) ? cnt : which;
    }

    if (cnt)
        setValue(which);
}

void InputSelector::SetSourceID(const QString &_sourceid)
{
    if (sourceid != _sourceid.toUInt())
    {
        sourceid = _sourceid.toUInt();
        load();
    }
}

uint InputSelector::GetCardID(void) const
{
    uint    cardid    = 0;
    QString inputname = QString::null;

    Parse(getValue(), cardid, inputname);

    return cardid;
}

QString InputSelector::GetInputName(void) const
{
    uint    cardid    = 0;
    QString inputname = QString::null;

    Parse(getValue(), cardid, inputname);

    return inputname;
}

bool InputSelector::Parse(const QString &cardid_inputname,
                          uint          &cardid,
                          QString       &inputname)
{
    cardid    = 0;
    inputname = QString::null;

    int sep0 = cardid_inputname.find(':');
    if (sep0 < 1)
        return false;

    cardid    = cardid_inputname.left(sep0).toUInt();
    inputname = cardid_inputname.mid(sep0 + 1);

    return true;
}