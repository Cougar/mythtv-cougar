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

#include <qwidget.h>
#include <qsqldatabase.h>

#include <cstdlib>

#include "settings.h"
#include "mythwidgets.h"
#include "mythwizard.h"
#include "mythcontext.h"
#include "videosource.h"

class DTVTStandard;
class DvbTATSCModulation;
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

class DVBTID: virtual public IntegerSetting, public AutoIncrementStorage {
public:
    DVBTID() : AutoIncrementStorage("dtv_multiplex", "mplexid"),
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

class DVBTransportList: public ListBoxSetting {
    Q_OBJECT
public:
    DVBTransportList() {}

    void save(QSqlDatabase* _db) { (void)_db; };
    void load(QSqlDatabase* _db) {
        db = _db;
        fillSelections();
    };
public slots:
    void fillSelections(void);
    void sourceID(const QString& str) { strSourceID=str; fillSelections();}

private:
    QSqlDatabase* db;
    QString strSourceID;
};

class DVBTSourceSetting;
//Page for selecting a transport to be created/edited
class DVBTransportsEditor: public VerticalConfigurationGroup ,
                               public ConfigurationDialog
{
    Q_OBJECT
public:
    DVBTransportsEditor();

    void load(QSqlDatabase* _db)
    {
         m_db = _db;
         VerticalConfigurationGroup::load(m_db);
    };

    virtual int exec(QSqlDatabase* db);

public slots:
    void menu(int);
    void del();
    void edit();
    void edit(int);
    void videoSource(const QString& str);

private:
    QSqlDatabase* m_db;
    DVBTransportList* m_list;
    DVBTSourceSetting* m_videoSource;
    DVBTID *m_id;
    int m_nID;
};

class DVBTransportWizard: public ConfigurationWizard
{
    Q_OBJECT
public:
    DVBTransportWizard(int id, unsigned _nVideoSorceID, QSqlDatabase* _db);

private:
    DVBTID *dvbtid;
    QSqlDatabase* db;
};

class DVBTransportPage: public HorizontalConfigurationGroup
{
    Q_OBJECT
public:
    DVBTransportPage(const DVBTID& id,unsigned nType);
protected:
    const DVBTID& id;

private:
    QSqlDatabase* db;

    DTVTStandard*      dtvStandard;
    DvbTATSCModulation* atscmodulation;
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
