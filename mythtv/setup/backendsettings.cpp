#include "backendsettings.h"
#include "libmyth/mythcontext.h"
#include "libmyth/settings.h"

class BackendSetting: public SimpleDBStorage, virtual public Configurable {
public:
    BackendSetting(QString name):
        SimpleDBStorage("settings", "data") {
        setName(name);
    };

protected:
    virtual QString whereClause(void) {
        return QString("value = '%1'").arg(getName());
    };

    virtual QString setClause(void) {
        return QString("value = '%1', data = '%2'").arg(getName()).arg(getValue());
    };
};

class BackendHostSetting: public SimpleDBStorage, virtual public Configurable {
public:
    BackendHostSetting(QString name):
        SimpleDBStorage("settings", "data") {
        setName(name);
    };

protected:
    virtual QString whereClause(void) {
        return QString("value = '%1' AND hostname = '%2'")
                       .arg(getName()).arg(gContext->GetHostName());
    };

    virtual QString setClause(void) {
        return QString("value = '%1', data = '%2', hostname = '%3'")
                       .arg(getName()).arg(getValue())
                       .arg(gContext->GetHostName());
    };
};

class LocalServerIP: public LineEditSetting, public BackendHostSetting {
public:
    LocalServerIP():
        BackendHostSetting("BackendServerIP") {
            QString hostname = gContext->GetHostName();
            QString label = "IP address for " + gContext->GetHostName();
            setLabel(label);
            setValue("127.0.0.1");
            setHelpText("Enter the IP address of this machine.  Use an "
                        "externally accessible address (ie, not 127.0.0.1) "
                        "if you are going to be running a frontend on a "
                        "different machine than this one.");
    };
};

class LocalServerPort: public LineEditSetting, public BackendHostSetting {
public:
    LocalServerPort():
        BackendHostSetting("BackendServerPort") {
            setLabel("Port the server runs on");
            setValue("6543");
            setHelpText("Unless you've got good reason to, don't change this.");
    };
};

class LocalStatusPort: public LineEditSetting, public BackendHostSetting {
public:
    LocalStatusPort():
        BackendHostSetting("BackendStatusPort") {
            setLabel("Port the server ");
            setValue("6544");
            setHelpText("Port which the server will listen to for HTTP "
                        "requests.  Currently, it shows a little status "
                        "information.");
    };
};

class MasterServerIP: public LineEditSetting, public BackendSetting {
public:
    MasterServerIP():
        BackendSetting("MasterServerIP") {
            setLabel("Master Server IP address");
            setValue("127.0.0.1");
            setHelpText("The IP address of the master backend server. All "
                        "frontend and non-master backend machines will connect "
                        "to this server.  If you only have one backend, this "
                        "should be the same IP address as above.");
    };
};

class MasterServerPort: public LineEditSetting, public BackendSetting {
public:
    MasterServerPort():
        BackendSetting("BackendServerPort") {
            setLabel("Port the master server runs on");
            setValue("6543");
            setHelpText("Unless you've got good reason to, don't change this.");
    };
};

class RecordFilePrefix: public LineEditSetting, public BackendHostSetting {
public:
    RecordFilePrefix():
        BackendHostSetting("RecordFilePrefix") {
        setLabel("Directory to hold recordings");
        setValue("/mnt/store/");
        setHelpText("All recordings get stored in this directory.");
    };
};

class LiveBufferPrefix: public LineEditSetting, public BackendHostSetting {
public:
    LiveBufferPrefix():
        BackendHostSetting("LiveBufferDir") {
        setLabel("Directory to hold the Live-TV buffers");
        setValue("/mnt/store/");
        setHelpText("All Live-TV buffers will get stored in this directory. "
                    "These buffers are used to allow you to pause, fast "
                    "forward and rewind through live TV.");
    };
};

class TVFormat: public ComboBoxSetting, public BackendSetting {
public:
    TVFormat():
        BackendSetting("TVFormat") {
        setLabel("TV format");
        addSelection("NTSC");
        addSelection("PAL");
        addSelection("SECAM");
        addSelection("PAL-NC");
        addSelection("PAL-M");
        addSelection("PAL-N");
        addSelection("NTSC-JP");
    };
};

class VbiFormat: public ComboBoxSetting, public BackendSetting {
public:
    VbiFormat():
        BackendSetting("VbiFormat") {
        setLabel("VBI format");
        addSelection("None");
        addSelection("PAL Teletext");
        addSelection("NTSC Closed Caption");
	setHelpText("VBI stands for Vertical Blanking Interrupt.  VBI is used "
                    "to carry Teletext and Closed Captioning data.");
    };
};

class FreqTable: public ComboBoxSetting, public BackendSetting {
public:
    FreqTable():
        BackendSetting("FreqTable") {
        setLabel("Channel frequency table");
        addSelection("us-cable");
        addSelection("us-bcast");
        addSelection("us-cable-hrc");
        addSelection("japan-bcast");
        addSelection("japan-cable");
        addSelection("europe-west");
        addSelection("europe-east");
        addSelection("italy");
        addSelection("newzealand");
        addSelection("australia");
        addSelection("ireland");
        addSelection("france");
        addSelection("china-bcast");
        addSelection("southafrica");
        addSelection("argentina");
        addSelection("canada-cable");
        addSelection("australia-optus");
        setHelpText("Select the appropriate frequency table for your "
                    "system.  If you have an antenna, use a \"-bcast\" "
                    "frequency.");
    };
};

class BufferSize: public SliderSetting, public BackendHostSetting {
public:
    BufferSize():
        SliderSetting(1, 100, 1),
        BackendHostSetting("BufferSize") {

        setLabel("Live TV buffer (GB)");
        setValue(5);
        setHelpText("How large the live TV buffer is allowed to grow.");
    };
};

class MaxBufferFill: public SliderSetting, public BackendHostSetting {
public:
    MaxBufferFill():
        SliderSetting(1, 100, 1),
        BackendHostSetting("MaxBufferFill") {
        setValue(50);
        setHelpText("How full the live TV buffer is allowed to grow before "
                    "forcing an unpause.");
    };
};

class TimeOffset: public ComboBoxSetting, public BackendSetting {
public:
    TimeOffset():
        BackendSetting("TimeOffset") {
        setLabel("Time offset for XMLTV listings");
        addSelection("(None)", "");
        addSelection("-0100");
        addSelection("-0200");
        addSelection("-0300");
        addSelection("-0400");
        addSelection("-0500");
        addSelection("-0600");
        addSelection("-0700");
        addSelection("-0800");
        addSelection("-0900");
        addSelection("-1000");
        addSelection("-1100");
        addSelection("-1200");
        addSelection("+0100");
        addSelection("+0200");
        addSelection("+0300");
        addSelection("+0400");
        addSelection("+0500");
        addSelection("+0600");
        addSelection("+0700");
        addSelection("+0800");
        addSelection("+0900");
        addSelection("+1000");
        addSelection("+1100");
        addSelection("+1200");
        setHelpText("If your local timezone does not match the timezone "
                    "returned by XMLTV, use this setting to have "
                    "mythfilldatabase adjust the program start and end times.");
    };
};

BackendSettings::BackendSettings() {
    VerticalConfigurationGroup* server = new VerticalConfigurationGroup(false);
    server->setLabel("Host Address Backend Setup");
    server->addChild(new LocalServerIP());
    server->addChild(new LocalServerPort());
    server->addChild(new LocalStatusPort());
    server->addChild(new MasterServerIP());
    server->addChild(new MasterServerPort());
    addChild(server);

    VerticalConfigurationGroup* group1 = new VerticalConfigurationGroup(false);
    group1->setLabel("Host-specific Backend Setup");
    group1->addChild(new RecordFilePrefix());
    group1->addChild(new LiveBufferPrefix());
    group1->addChild(new BufferSize());
    group1->addChild(new MaxBufferFill());
    addChild(group1);

    VerticalConfigurationGroup* group2 = new VerticalConfigurationGroup(false);
    group2->setLabel("Global Backend Setup");
    group2->addChild(new TVFormat());
    group2->addChild(new VbiFormat());
    group2->addChild(new FreqTable());
    group2->addChild(new TimeOffset());
    addChild(group2);
}

