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
        return QString("value = '%1', data = '%1'").arg(getName()).arg(getValue());
    };
};

class RecordFilePrefix: public LineEditSetting, public BackendSetting {
public:
    RecordFilePrefix():
        BackendSetting("RecordFilePrefix") {
        setLabel("Directory to hold recordings");
        setValue("/mnt/store/");
        setHelpText("All recordings get stored in this directory.");
    };
};

class LiveBufferPrefix: public LineEditSetting, public BackendSetting {
public:
    LiveBufferPrefix():
        BackendSetting("LiveBufferDir") {
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

class BufferSize: public SliderSetting, public BackendSetting {
public:
    BufferSize():
        SliderSetting(1, 100, 1),
        BackendSetting("BufferSize") {

        setLabel("Live TV buffer (GB)");
        setValue(5);
        setHelpText("How large the live TV buffer is allowed to grow.");
    };
};

class MaxBufferFill: public SliderSetting, public BackendSetting {
public:
    MaxBufferFill():
        SliderSetting(1, 100, 1),
        BackendSetting("MaxBufferFill") {
        setValue(50);
        setHelpText("How full the live TV buffer is allowed to grow before "
                    "forcing an unpause.");
    };
};

BackendSettings::BackendSettings() {
    VerticalConfigurationGroup* group1 = new VerticalConfigurationGroup(false);
    group1->setLabel("General");
    group1->addChild(new RecordFilePrefix());
    group1->addChild(new LiveBufferPrefix());
    group1->addChild(new BufferSize());
    group1->addChild(new MaxBufferFill());
    group1->addChild(new TVFormat());
    group1->addChild(new VbiFormat());
    group1->addChild(new FreqTable());
    addChild(group1);
}

