
#include <qsqldatabase.h>
#include "settings.h"
#include "mythcontext.h"
#include "channeleditor.h"

#include <qapplication.h>
#include <qlayout.h>
#include <qdialog.h>
#include <qcursor.h>

#include <mythwidgets.h>
#include <mythdialogs.h>
#include <mythwizard.h>

#include "videosource.h"
#include "channelsettings.h"

/*******************
 TODO:
    - Add HelpText for items!!!
    - I(nfo) menu
        - Add delete channel to menu.
        - Hide channels without channel number option,
    - ID() for new channel.
    - Add the possibility of a 'Load' button between
        cancel and back/next.
 *******************/

ChannelWizard::ChannelWizard(int id, QSqlDatabase* _db)
             : ConfigurationWizard(), db(_db) {
    setLabel("Channel Options");

    // Must be first.
    addChild(cid = new ChannelID());
    cid->setValue(id);
        
    ChannelOptionsCommon* common = new ChannelOptionsCommon(*cid);
    addChild(common);

    QString cardtype = getCardtype();
    if (cardtype != "DVB" || id == 0) {
        ChannelOptionsV4L* v4l = new ChannelOptionsV4L(*cid);
        addChild(v4l);
    }

    if (cardtype == "DVB" || id == 0)
    {
        ChannelOptionsDVB* dvb = new ChannelOptionsDVB(*cid);
        ChannelOptionsDVBPids* pids = new ChannelOptionsDVBPids(*cid);
        addChild(dvb);
        addChild(pids);
    }
}

QString ChannelWizard::getCardtype() {
    QSqlQuery query = db->exec(QString("SELECT cardtype"
        " FROM capturecard, cardinput, channel"
        " WHERE channel.chanid=%1"
        " AND channel.sourceid = cardinput.sourceid"
        " AND cardinput.cardid = capturecard.cardid")
        .arg(cid->getValue()));
    if (query.isActive() && query.numRowsAffected() > 0)
    {
        query.next();
        return query.value(0).toString();
    } else
        return "";
}

void ChannelListSetting::fillSelections(const QString& sourceid) 
{
    currentSourceID = sourceid;
    clearSelections();
    addSelection("(New Channel)");
    QString querystr = QString("SELECT name, channum, chanid FROM channel");
    if (sourceid != "")
        querystr += QString(" WHERE sourceid='%1'").arg(sourceid);
        
    if (currentSortMode == "Channel Name")
        querystr += " ORDER BY name";
    else if (currentSortMode == "Channel Number")
        querystr += " ORDER BY channum";

    QSqlQuery query = db->exec(querystr);
    if (query.isActive() && query.numRowsAffected() > 0)
        while(query.next()) {
            QString name = query.value(0).toString();
            QString channum = query.value(1).toString();
            QString chanid = query.value(2).toString();
             
            if (channum == "" && currentHideMode) continue;

            if (name == "") name = "(Unnamed : " + chanid + ")";

            if (currentSortMode == "Channel Name") {
                if (channum != "") name += " (" + channum + ")";
            } else if (currentSortMode == "Channel Number") {
                if (channum != "")
                    name = channum + ". " + name;
                else
                    name = "??. " + name;
            }

            addSelection(name, chanid);
        }
}

class SourceSetting: public ComboBoxSetting {
public:
    SourceSetting(): ComboBoxSetting() {
        setLabel("Video Source");
        addSelection("(All)","");
    };

    void save(QSqlDatabase* db) { (void)db; };
    void load(QSqlDatabase* db) {
        QSqlQuery query = db->exec(QString(
            "SELECT name, sourceid"
            " FROM videosource"));
        if (query.isActive() && query.numRowsAffected() > 0)
            while(query.next())
                addSelection(query.value(0).toString(),
                             query.value(1).toString());
    };
};

class SortMode: public ComboBoxSetting, public TransientStorage {
public:
    SortMode(): ComboBoxSetting() {
        setLabel("Sort Mode");
        addSelection("Channel Name");
        addSelection("Channel Number");
    };
};

class NoChanNumHide: public CheckBoxSetting, public TransientStorage {
public:
    NoChanNumHide() {
        setLabel("Hide channels without channel number.");
    };
};

ChannelEditor::ChannelEditor():
    VerticalConfigurationGroup(), ConfigurationDialog() {

    setLabel("Channels");
    addChild(list = new ChannelListSetting());

    SortMode* sort = new SortMode();
    SourceSetting* source = new SourceSetting();
    NoChanNumHide* hide = new NoChanNumHide();

    sort->setValue(sort->getValueIndex(list->getSortMode()));
    source->setValue(sort->getValueIndex(list->getSourceID()));
    hide->setValue(list->getHideMode());

    addChild(sort);
    addChild(source);
    addChild(hide);
    connect(source, SIGNAL(valueChanged(const QString&)),
            list, SLOT(fillSelections(const QString&)));
    connect(sort, SIGNAL(valueChanged(const QString&)),
            list, SLOT(setSortMode(const QString&)));
    connect(hide, SIGNAL(valueChanged(bool)),
            list, SLOT(setHideMode(bool)));
    connect(list, SIGNAL(accepted(int)), this, SLOT(edit(int)));
    connect(list, SIGNAL(menuButtonPressed(int)), this, SLOT(menu(int)));
}

int ChannelEditor::exec(QSqlDatabase* _db) {
    db = _db;
    while (ConfigurationDialog::exec(db) == QDialog::Accepted)  {}
    return QDialog::Rejected;
}

void ChannelEditor::edit() {
    ChannelWizard cw(id,db);
    cw.exec(db);
    dialog->setFocus();
}

void ChannelEditor::edit(int /*iSelected*/) {
    id = list->getValue().toInt();
    edit();
}

void ChannelEditor::del() {
    int val = MythPopupBox::show2ButtonPopup(gContext->GetMainWindow(), "",
                                             "Are you sure you would like to "
                                             "delete this channel?", 
                                             "Yes, delete the channel", 
                                             "No, don't", 2);

    if (val == 0) {
        QSqlQuery query;
    
        query = db->exec(QString("DELETE channel FROM channel "
                                 "WHERE chanid ='%1'").arg(id));
        if (!query.isActive())
            MythContext::DBError("ChannelEditor Delete Channel", query);

        query = db->exec(QString("DELETE dvb_channel FROM dvb_channel "
                                 "WHERE chanid ='%1'").arg(id));
        if (!query.isActive())
            MythContext::DBError("ChannelEditor Delete DVBChannel", query);
        list->fillSelections(list->getSourceID());
    }
}

void ChannelEditor::menu(int /*iSelected*/) {
    id = list->getValue().toInt();
    if (id == 0) {
       edit();
    } else {
        int val = MythPopupBox::show2ButtonPopup(gContext->GetMainWindow(),
                                                 "",
                                                 "Channel Menu",
                                                 "Edit..",
                                                 "Delete..", 1);

        if (val == 0)
            emit edit();
        else if (val == 1)
            emit del();
    }
}

