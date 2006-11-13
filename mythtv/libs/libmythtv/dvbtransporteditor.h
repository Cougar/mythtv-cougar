/*
 * $Id$
 * vim: set expandtab tabstop=4 shiftwidth=4:
 *
 * Original Project
 *      MythTV      http://www.mythtv.org
 *
 * Author(s):
 *      John Pullan  (john@pullan.org)
 *      Taylor Jacob (rtjacob@earthlink.net)
 *
 * Description:
 *     Collection of classes to provide dvb a transport editor
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

#ifndef DVBTRANSPORTEDITOR_H
#define DVBTRANSPORTEDITOR_H

#include <qobject.h>
#include "settings.h"

class DTVTStandard;
class ATSCModulation;
class DvbTFrequency;
class DvbTSymbolrate;
class DvbTPolarity;
class DvbTFec;
class DvbTModulation;
class DvbTInversion;
class DvbTBandwidth;
class DvbTConstellation;
class DvbTCoderateLP;
class DvbTCoderateHP;
class DvbTTransmissionMode;
class DvbTGuardInterval;
class DvbTHierarchy;

/*
 *  Objects added for new DVB Transport Editing section
 */

class DVBTID: public AutoIncrementDBSetting {
public:
    DVBTID() : AutoIncrementDBSetting("dtv_multiplex", "mplexid"),
          field("mplexid"),table("dtv_multiplex")
    {
        setVisible(false);
        setName("DVBTID");
    };

    const QString& getField(void) const {
        return field;
    };

protected:
    QString field,table;
};

class DVBTransportList : public ListBoxSetting, public Storage
{
    Q_OBJECT
  public:
    DVBTransportList() : ListBoxSetting(this) {}

    void load() { fillSelections(); }
    void save(void) { }
    void save(QString /*destination*/) { }

public slots:
    void fillSelections(void);
    void sourceID(const QString& str) { strSourceID=str; fillSelections();}

private:
    QString strSourceID;
};

class DVBTSourceSetting;
//Page for selecting a transport to be created/edited
class DVBTransportsEditor : public QObject, public ConfigurationDialog
{
    Q_OBJECT
public:
    DVBTransportsEditor();

    void load(void) { cfgGrp->load(); }

    virtual int exec();

public slots:
    void menu(int);
    void del();
    void del(int);
    void edit();
    void edit(int);
    void videoSource(const QString& str);

private:
    DVBTransportList* m_list;
    DVBTSourceSetting* m_videoSource;
    DVBTID *m_id;
    int m_nID;
};

class DVBTransportWizard : public QObject, public ConfigurationWizard
{
    Q_OBJECT
public:
    DVBTransportWizard(int id, unsigned _nVideoSorceID);

private:
    DVBTID *dvbtid;
};

class DVBTransportPage: public HorizontalConfigurationGroup
{
    Q_OBJECT
public:
    DVBTransportPage(const DVBTID& id,unsigned nType);
protected:
    const DVBTID& id;

private:

    DTVTStandard*      dtvStandard;
    ATSCModulation    *atscmodulation;
    DvbTFrequency* frequency;
    DvbTSymbolrate* symbolrate;
    DvbTPolarity* polarity;
    DvbTFec* fec;
    DvbTModulation* modulation;
    DvbTInversion* inversion;

    DvbTBandwidth* bandwidth;
    DvbTConstellation* constellation;
    DvbTCoderateLP* coderate_lp;
    DvbTCoderateHP* coderate_hp;
    DvbTTransmissionMode* trans_mode;
    DvbTGuardInterval* guard_interval;
    DvbTHierarchy* hierarchy;
};

#endif //DVBTRANSPORTEDITOR_H
