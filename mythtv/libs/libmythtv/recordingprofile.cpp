#include "recordingprofile.h"
#include "libmyth/mythcontext.h"
#include <qsqldatabase.h>
#include <qheader.h>
#include <qcursor.h>
#include <qlayout.h>
#include <iostream>

QString RecordingProfileParam::whereClause(void) {
  return QString("id = %1").arg(parentProfile.getProfileNum());
}

class CodecParam: public SimpleDBStorage,
                  public RecordingProfileSetting {
protected:
    CodecParam(const RecordingProfile& parentProfile,
                    QString name):
        SimpleDBStorage("codecparams", "value"),
        RecordingProfileSetting(parentProfile) {
        setName(name);
    };

    virtual QString setClause(void);
    virtual QString whereClause(void);
};

QString CodecParam::setClause(void) {
  return QString("profile = %1, name = '%2', value = '%3'")
    .arg(parentProfile.getProfileNum())
    .arg(getName())
    .arg(getValue());
}

QString CodecParam::whereClause(void) {
  return QString("profile = %1 AND name = '%2'")
    .arg(parentProfile.getProfileNum()).arg(getName());
}

class AudioCodecName: public ComboBoxSetting, public RecordingProfileParam {
public:
    AudioCodecName(const RecordingProfile& parent):
        RecordingProfileParam(parent, "audiocodec") {
        setLabel(QObject::tr("Codec"));
    };
};

class MP3Quality: public CodecParam, public SliderSetting {
public:
    MP3Quality(const RecordingProfile& parent):
        CodecParam(parent, "mp3quality"),
        SliderSetting(1,9,1) {
        setLabel(QObject::tr("MP3 Quality"));
        setValue(7);
	setHelpText(QObject::tr("The higher the slider number, the lower the "
                    "quality of the audio.  Better quality audio (lower "
                    "numbers) requires more CPU."));
    };
};

class SampleRate: public CodecParam, public ComboBoxSetting {
public:
    SampleRate(const RecordingProfile& parent, bool analog = true):
        CodecParam(parent, "samplerate") {
        setLabel(QObject::tr("Sampling rate"));
        if (analog)
        {
            addSelection("32000");
            addSelection("44100");
            addSelection("48000");
        }
        else
        {
            addSelection("48000");
            //addSelection("44100");
            //addSelection("32000");
        }
	setHelpText(QObject::tr("Sets the audio sampling rate for your DSP. "
                    "Ensure that you choose a sampling rate appropriate "
                    "for your device.  btaudio may only allow 32000."));
    };
};

class MPEG2audType: public CodecParam, public ComboBoxSetting {
public:
   MPEG2audType(const RecordingProfile& parent):
        CodecParam(parent, "mpeg2audtype") {
        setLabel(QObject::tr("Type"));
	setHelpText(QObject::tr("Sets the audio type"));
    };
};

class MPEG2audBitrateL1: public CodecParam, public ComboBoxSetting {
public:
   MPEG2audBitrateL1(const RecordingProfile& parent):
        CodecParam(parent, "mpeg2audbitratel1") {
        setLabel(QObject::tr("Bitrate"));
        
        addSelection("32 kbps", "32");
        addSelection("64 kbps", "64");
        addSelection("96 kbps", "96");
        addSelection("128 kbps", "128");
        addSelection("160 kbps", "160");
        addSelection("192 kbps", "192");
        addSelection("224 kbps", "224");
        addSelection("256 kbps", "256");
        addSelection("288 kbps", "288");
        addSelection("320 kbps", "320");
        addSelection("352 kbps", "352");
        addSelection("384 kbps", "384");
        addSelection("416 kbps", "416");
        addSelection("448 kbps", "448");
        setValue(13);
        setHelpText("Sets the audio bitrate");
    };
};

class MPEG2audBitrateL2: public CodecParam, public ComboBoxSetting {
public:
   MPEG2audBitrateL2(const RecordingProfile& parent):
        CodecParam(parent, "mpeg2audbitratel2") {
        setLabel(QObject::tr("Bitrate"));
        
        addSelection("32 kbps", "32");
        addSelection("48 kbps", "48");
        addSelection("56 kbps", "56");
        addSelection("64 kbps", "64");
        addSelection("80 kbps", "80");
        addSelection("96 kbps", "96");
        addSelection("112 kbps", "112");
        addSelection("128 kbps", "128");
        addSelection("160 kbps", "160");
        addSelection("192 kbps", "192");
        addSelection("224 kbps", "224");
        addSelection("256 kbps", "256");
        addSelection("320 kbps", "320");
        addSelection("384 kbps", "384");
        setValue(13);
        setHelpText(QObject::tr("Sets the audio bitrate"));
    };
};

class MPEG2audVolume: public CodecParam, public SliderSetting {
public:
    MPEG2audVolume(const RecordingProfile& parent):
        CodecParam(parent, "mpeg2audvolume"),
        SliderSetting(0,100,1) {

        setLabel(QObject::tr("Volume (%)"));
        setValue(90);
        setHelpText(QObject::tr("Volume of the recording "));
    };
};

class MPEG2AudioBitrateSettings: public VerticalConfigurationGroup,
                                 public TriggeredConfigurationGroup {
public:
    MPEG2AudioBitrateSettings(const RecordingProfile& parent)
           : VerticalConfigurationGroup(false)
    {
        setLabel(QObject::tr("Bitrate Settings"));
        setUseLabel(false);
        MPEG2audType* audType = new MPEG2audType(parent);
        addChild(audType);
        setTrigger(audType);

        ConfigurationGroup* audbr = new VerticalConfigurationGroup(false);
        audbr->addChild(new MPEG2audBitrateL1(parent));
        audbr->setLabel("Layer I");
        addTarget("Layer I", audbr);
        audType->addSelection("Layer I");

        audbr = new VerticalConfigurationGroup(false);
        audbr->addChild(new MPEG2audBitrateL2(parent));
        audbr->setLabel("Layer II");
        addTarget("Layer II", audbr);
        audType->addSelection("Layer II");
        audType->setValue(1);
    };
};

class AudioCompressionSettings: public VerticalConfigurationGroup,
                                public TriggeredConfigurationGroup {
public:
    AudioCompressionSettings(const RecordingProfile& parent, QString profName)
           : VerticalConfigurationGroup(false)
    {
        QString labelName;
        if (profName.isNull())
            labelName = QString(QObject::tr("Audio Quality"));
        else
            labelName = profName + "->" + QObject::tr("Audio Quality");
        setName(labelName);
        setUseLabel(false);

        codecName = new AudioCodecName(parent);
        addChild(codecName);
        setTrigger(codecName);

        ConfigurationGroup* params = new VerticalConfigurationGroup(false);
        params->setLabel("MP3");
        params->addChild(new SampleRate(parent));
        params->addChild(new MP3Quality(parent));
        addTarget("MP3", params);

        params = new VerticalConfigurationGroup(false);
        params->setLabel("MPEG-2 Hardware Encoder");
        params->addChild(new SampleRate(parent, false));
        params->addChild(new MPEG2AudioBitrateSettings(parent));
        params->addChild(new MPEG2audVolume(parent));
        addTarget("MPEG-2 Hardware Encoder", params);

        params = new VerticalConfigurationGroup(false);
        params->setLabel("Uncompressed");
        params->addChild(new SampleRate(parent));
        addTarget("Uncompressed", params);
    };

    void selectCodecs(QString groupType)
    {
        if(!groupType.isNull())
        {
            if(groupType == "MPEG")
               codecName->addSelection("MPEG-2 Hardware Encoder");
            else
            {
                // V4L, TRANSCODE (and any undefined types)
                codecName->addSelection("MP3");
                codecName->addSelection("Uncompressed");
            }
        }
        else
        {
            codecName->addSelection("MP3");
            codecName->addSelection("Uncompressed");
            codecName->addSelection("MPEG-2 Hardware Encoder");
        }
    }
private:
    AudioCodecName* codecName;
};

class VideoCodecName: public ComboBoxSetting, public RecordingProfileParam {
public:
    VideoCodecName(const RecordingProfile& parent):
        RecordingProfileParam(parent, "videocodec") {
        setLabel(QObject::tr("Codec"));
    };
};

class RTjpegQuality: public CodecParam, public SliderSetting {
public:
    RTjpegQuality(const RecordingProfile& parent):
        CodecParam(parent, "rtjpegquality"),
        SliderSetting(1,255,1) {
        setLabel(QObject::tr("RTjpeg Quality"));
        setValue(170);
	setHelpText(QObject::tr("Higher is better quality."));
    };
};

class RTjpegLumaFilter: public CodecParam, public SpinBoxSetting {
public:
    RTjpegLumaFilter(const RecordingProfile& parent):
        CodecParam(parent, "rtjpeglumafilter"),
        SpinBoxSetting(0,31,1) {
        setLabel(QObject::tr("Luma filter"));
        setValue(0);
	setHelpText(QObject::tr("Lower is better."));
    };
};

class RTjpegChromaFilter: public CodecParam, public SpinBoxSetting {
public:
    RTjpegChromaFilter(const RecordingProfile& parent):
        CodecParam(parent, "rtjpegchromafilter"),
        SpinBoxSetting(0,31,1) {
        setLabel(QObject::tr("Chroma filter"));
        setValue(0);
	setHelpText(QObject::tr("Lower is better."));
    };
};

class MPEG4bitrate: public CodecParam, public SliderSetting {
public:
    MPEG4bitrate(const RecordingProfile& parent):
        CodecParam(parent, "mpeg4bitrate"),
        SliderSetting(100,8000,100) {

        setLabel(QObject::tr("Bitrate"));
        setValue(2200);
	setHelpText(QObject::tr("Bitrate in kilobits/second.  2200Kbps is "
                    "approximately 1 Gigabyte per hour."));
    };
};

class MPEG4ScaleBitrate: public CodecParam, public CheckBoxSetting {
public:
    MPEG4ScaleBitrate(const RecordingProfile& parent):
        CodecParam(parent, "mpeg4scalebitrate") {
        setLabel(QObject::tr("Scale bitrate for frame size"));
        setValue(true);
	setHelpText(QObject::tr("If set, the MPEG4 bitrate will be used for "
                    "640x480.  If other resolutions are used, the "
                    "bitrate will be scaled appropriately."));
    };
};

class MPEG4MinQuality: public CodecParam, public SliderSetting {
public:
    MPEG4MinQuality(const RecordingProfile& parent):
        CodecParam(parent, "mpeg4minquality"),
        SliderSetting(1,31,1) {

        setLabel(QObject::tr("Minimum quality"));
        setValue(15);
	setHelpText(QObject::tr("Modifying the default may have severe "
                    "consequences."));
    };
};

class MPEG4MaxQuality: public CodecParam, public SliderSetting {
public:
    MPEG4MaxQuality(const RecordingProfile& parent):
        CodecParam(parent, "mpeg4maxquality"),
        SliderSetting(1,31,1) {

        setLabel(QObject::tr("Maximum quality"));
        setValue(2);
	setHelpText(QObject::tr("Modifying the default may have severe "
                    "consequences."));
    };
};

class MPEG4QualDiff: public CodecParam, public SliderSetting {
public:
    MPEG4QualDiff(const RecordingProfile& parent):
        CodecParam(parent, "mpeg4qualdiff"),
        SliderSetting(1,31,1) {

        setLabel(QObject::tr("Max quality difference between frames"));
        setValue(3);
        setHelpText(QObject::tr("Modifying the default may have severe "
                    "consequences."));
    };
};

class MPEG4OptionVHQ: public CodecParam, public CheckBoxSetting {
public:
    MPEG4OptionVHQ(const RecordingProfile& parent):
        CodecParam(parent, "mpeg4optionvhq") {
        setLabel(QObject::tr("Enable high-quality encoding"));
        setValue(false);
        setHelpText(QObject::tr("If set, the MPEG4 encoder will use "
                    "'high-quality' encoding options.  This requires much "
                    "more processing, but can result in better video."));
    };
};

class MPEG4Option4MV: public CodecParam, public CheckBoxSetting {
public:
    MPEG4Option4MV(const RecordingProfile& parent):
        CodecParam(parent, "mpeg4option4mv") {
        setLabel(QObject::tr("Enable 4MV encoding"));
        setValue(false);
        setHelpText(QObject::tr("If set, the MPEG4 encoder will use '4MV' "
                    "motion-vector encoding.  This requires "
                    "much more processing, but can result in better "
                    "video. It is highly recommended that the HQ option is "
                    "enabled if 4MV is enabled."));
    };
};

class MPEG2bitrate: public CodecParam, public SliderSetting {
public:
    MPEG2bitrate(const RecordingProfile& parent):
        CodecParam(parent, "mpeg2bitrate"),
        SliderSetting(1000,16000,100) {

        setLabel(QObject::tr("Bitrate"));
        setValue(4500);
        setHelpText(QObject::tr("Bitrate in kilobits/second.  2200Kbps is "
                    "approximately 1 Gigabyte per hour."));
    };
};

class MPEG2maxBitrate: public CodecParam, public SliderSetting {
public:
    MPEG2maxBitrate(const RecordingProfile& parent):
        CodecParam(parent, "mpeg2maxbitrate"),
        SliderSetting(1000,16000,100) {

        setLabel(QObject::tr("Max. Bitrate"));
        setValue(6000);
        setHelpText(QObject::tr("Maximum Bitrate in kilobits/second.  "
                    "2200Kbps is approximately 1 Gigabyte per hour."));
    };
};

class MPEG2streamType: public CodecParam, public ComboBoxSetting {
public:
    MPEG2streamType(const RecordingProfile& parent):
        CodecParam(parent, "mpeg2streamtype") {
        setLabel(QObject::tr("Stream Type"));
        
        addSelection("MPEG-2 PS");
        addSelection("MPEG-2 TS");
        addSelection("MPEG-1 VCD");
        addSelection("PES AV");
        addSelection("PES V");
        addSelection("PES A");
        addSelection("DVD");
        addSelection("DVD-Special 1");
        addSelection("DVD-Special 2");
        setValue(0);
        setHelpText(QObject::tr("Sets the type of stream generated by "
                    "your PVR."));
    };
};

class MPEG2aspectRatio: public CodecParam, public ComboBoxSetting {
public:
    MPEG2aspectRatio(const RecordingProfile& parent):
        CodecParam(parent, "mpeg2aspectratio") {
        setLabel(QObject::tr("Aspect Ratio"));
        
        addSelection(QObject::tr("Square"), "Square");
        addSelection("4:3");
        addSelection("16:9");
        addSelection("2.21:1");
        setValue(1);
        setHelpText(QObject::tr("Sets the aspect ratio of stream generated "
                    "by your PVR."));
    };
};

class HardwareMJPEGQuality: public CodecParam, public SliderSetting {
public:
    HardwareMJPEGQuality(const RecordingProfile& parent):
        CodecParam(parent, "hardwaremjpegquality"),
        SliderSetting(0, 100, 1) {
        setLabel(QObject::tr("Quality"));
        setValue(100);
    };
};

class HardwareMJPEGHDecimation: public CodecParam, public ComboBoxSetting {
public:
    HardwareMJPEGHDecimation(const RecordingProfile& parent):
        CodecParam(parent, "hardwaremjpeghdecimation") {
        setLabel(QObject::tr("Horizontal Decimation"));
        addSelection("1");
        addSelection("2");
        addSelection("4");
        setValue(2);
    };
};

class HardwareMJPEGVDecimation: public CodecParam, public ComboBoxSetting {
public:
    HardwareMJPEGVDecimation(const RecordingProfile& parent):
        CodecParam(parent, "hardwaremjpegvdecimation") {
        setLabel(QObject::tr("Vertical Decimation"));
        addSelection("1");
        addSelection("2");
        addSelection("4");
        setValue(2);
    };
};

class VideoCompressionSettings: public VerticalConfigurationGroup,
                                public TriggeredConfigurationGroup {
public:
    VideoCompressionSettings(const RecordingProfile& parent, QString profName)
             : VerticalConfigurationGroup(false),
               TriggeredConfigurationGroup(false)
    {
        QString labelName;
        if (profName.isNull())
            labelName = QObject::tr("Video Compression");
        else
            labelName = profName + "->" + QObject::tr("Video Compression");
        setName(labelName);
        setUseLabel(false);

        codecName = new VideoCodecName(parent);
        addChild(codecName);
        setTrigger(codecName);

        ConfigurationGroup* params = new VerticalConfigurationGroup();
        params->setLabel(QObject::tr("RTjpeg Parameters"));
        params->addChild(new RTjpegQuality(parent));
        params->addChild(new RTjpegLumaFilter(parent));
        params->addChild(new RTjpegChromaFilter(parent));

        addTarget("RTjpeg", params);

        params = new VerticalConfigurationGroup(false);
        params->setLabel(QObject::tr("MPEG-4 Parameters"));
        params->addChild(new MPEG4bitrate(parent));
        params->addChild(new MPEG4MaxQuality(parent));
        params->addChild(new MPEG4MinQuality(parent));
        params->addChild(new MPEG4QualDiff(parent));
        params->addChild(new MPEG4ScaleBitrate(parent));
        params->addChild(new MPEG4OptionVHQ(parent));
        params->addChild(new MPEG4Option4MV(parent));

        addTarget("MPEG-4", params);

        params = new VerticalConfigurationGroup();
        params->setLabel(QObject::tr("Hardware MJPEG Parameters"));
        params->addChild(new HardwareMJPEGQuality(parent));
        params->addChild(new HardwareMJPEGHDecimation(parent));
        params->addChild(new HardwareMJPEGVDecimation(parent));
 
        addTarget("Hardware MJPEG", params);

        params = new VerticalConfigurationGroup(false);
        params->setLabel(QObject::tr("MPEG-2 Hardware Encoder"));
        params->addChild(new MPEG2streamType(parent));
        params->addChild(new MPEG2aspectRatio(parent));
        params->addChild(new MPEG2bitrate(parent));
        params->addChild(new MPEG2maxBitrate(parent));

        addTarget("MPEG-2 Hardware Encoder", params);
    }

    void selectCodecs(QString groupType)
    {
        if(!groupType.isNull())
        {
            if(groupType == "MPEG")
               codecName->addSelection("MPEG-2 Hardware Encoder");
            else if(groupType == "MJPEG")
                codecName->addSelection("Hardware MJPEG");
            else
            {
                // V4L, TRANSCODE (and any undefined types)
                codecName->addSelection("RTjpeg");
                codecName->addSelection("MPEG-4");
            }
        }
        else
        {
            codecName->addSelection("RTjpeg");
            codecName->addSelection("MPEG-4");
            codecName->addSelection("Hardware MJPEG");
            codecName->addSelection("MPEG-2 Hardware Encoder");
        }
    }

private:
    VideoCodecName* codecName;
};

class AutoTranscode: public CodecParam, public CheckBoxSetting {
public:
    AutoTranscode(const RecordingProfile& parent):
        CodecParam(parent, "autotranscode") {
        setLabel(QObject::tr("Automatically transcode after recording"));
        setValue(false);
    };
};

class ImageSize: public HorizontalConfigurationGroup {
public:
    class Width: public SpinBoxSetting, public CodecParam {
    public:
        Width(const RecordingProfile& parent, int maxwidth=704):
            SpinBoxSetting(160,maxwidth,16),
            CodecParam(parent, "width") {
            setLabel(QObject::tr("Width"));
            setValue(480);
        };
    };

    class Height: public SpinBoxSetting, public CodecParam {
    public:
        Height(const RecordingProfile& parent, int maxheight=576):
            SpinBoxSetting(160,maxheight,16),
            CodecParam(parent, "height") {
            setLabel(QObject::tr("Height"));
            setValue(480);
        };
    };

    ImageSize(const RecordingProfile& parent, QString tvFormat,
              QString profName) 
         : HorizontalConfigurationGroup(false) 
    {
        QString labelName;
        if (profName.isNull())
            labelName = QObject::tr("Image size");
        else
            labelName = profName + "->" + QObject::tr("Image size");
        setLabel(labelName);

        setUseLabel(false);

        QString fullsize, halfsize;
        int maxwidth, maxheight;
        if (tvFormat.lower() == "ntsc" || tvFormat.lower() == "ntsc-jp") {
            maxwidth = 720;
            maxheight = 480;

        } else {
            maxwidth = 768;
            maxheight = 576;
        }

        addChild(new Width(parent,maxwidth));
        addChild(new Height(parent,maxheight));
    };
};

RecordingProfile::RecordingProfile(QString profName)
{
    // This must be first because it is needed to load/save the other settings
    addChild(id = new ID());

    ConfigurationGroup* profile = new VerticalConfigurationGroup(false);
    QString labelName;
    if (profName.isNull())
        labelName = QString(QObject::tr("Profile"));
    else
        labelName = profName + "->" + QObject::tr("Profile");
    profile->setLabel(labelName);
    profile->addChild(name = new Name(*this));
    profile->addChild(new AutoTranscode(*this));
    addChild(profile);

    QString tvFormat = gContext->GetSetting("TVFormat");
    addChild(new ImageSize(*this, tvFormat, profName));
    vc = new VideoCompressionSettings(*this, profName);
    addChild(vc);

    ac = new AudioCompressionSettings(*this, profName);
    addChild(ac);
};

void RecordingProfile::loadByID(QSqlDatabase* db, int profileId) {
    id->setValue(profileId);
    load(db);
}

bool RecordingProfile::loadByCard(QSqlDatabase* db, QString name, int cardid) {
    QString hostname = gContext->GetHostName();
    int id = 0;
    QString query = QString("SELECT recordingprofiles.id, "
                   "profilegroups.hostname, profilegroups.is_default FROM "
                   "recordingprofiles,profilegroups,capturecard WHERE "
                   "profilegroups.id = recordingprofiles.profilegroup "
                   "AND capturecard.cardid = %1 "
                   "AND capturecard.cardtype = profilegroups.cardtype "
                   "AND recordingprofiles.name = '%2';").arg(cardid).arg(name);
    QSqlQuery result = db->exec(query);
    if (result.isActive() && result.numRowsAffected() > 0) {
        while (result.next())
        {
            if(result.value(1).toString() == hostname)
            {
                id = result.value(0).toInt();
                break;
            }
            else if(result.value(2).toInt() == 1)
                id = result.value(0).toInt();
        }
    }
    if (id)
    {
        loadByID(db, id);
        return true;
    }
    else
        return false;
}

bool RecordingProfile::loadByGroup(QSqlDatabase* db, QString name, QString group) {
    QString query = QString("SELECT recordingprofiles.id FROM "
                 "recordingprofiles, profilegroups WHERE "
                 "recordingprofiles.profilegroup = profilegroups.id AND "
                 "profilegroups.name = '%1' AND recordingprofiles.name = '%2';")
                 .arg(group).arg(name);
    QSqlQuery result = db->exec(query);
    if (result.isActive() && result.numRowsAffected() > 0) {
        result.next();
        loadByID(db, result.value(0).toInt());
        return true;
    } else {
        return false;
    }
}

void RecordingProfile::setCodecTypes(QSqlDatabase *db)
{
    vc->selectCodecs(groupType(db));
    ac->selectCodecs(groupType(db));
}

void RecordingProfileEditor::open(int id) {
    QString profName = RecordingProfile::getName(db, id);
    if (profName.isNull())
        profName = labelName;
    else
        profName = labelName + "->" + profName;
    RecordingProfile* profile = new RecordingProfile(profName);

    profile->loadByID(db,id);
    profile->setCodecTypes(db);

    if (profile->exec(db) == QDialog::Accepted)
        profile->save(db);
    delete profile;
};

RecordingProfileEditor::RecordingProfileEditor(QSqlDatabase* _db,
                          int id, QString profName)
{
    labelName = profName;
    db = _db;
    group = id;
    if(! labelName.isNull())
        this->setLabel(labelName);
}

void RecordingProfileEditor::load(QSqlDatabase* db) {
    clearSelections();
    //addSelection("(Create new profile)", "0");
    RecordingProfile::fillSelections(db, this, group);
}

int RecordingProfileEditor::exec(QSqlDatabase* db) {
    while (ConfigurationDialog::exec(db) == QDialog::Accepted)
        open(getValue().toInt());

    return QDialog::Rejected;
}

void RecordingProfile::fillSelections(QSqlDatabase* db, SelectSetting* setting,
                                      int group) {
    if (group == 0)
    {
       for(int i = 0; availProfiles[i] != ""; i++)
           setting->addSelection(availProfiles[i], availProfiles[i]);
    }
    else
    {
        QString query = QString("SELECT name, id FROM recordingprofiles "
                                "WHERE profilegroup = %1 ORDER BY id;")
                                .arg(group);
       QSqlQuery result = db->exec(query);
        if (result.isActive() && result.numRowsAffected() > 0)
            while (result.next())
                setting->addSelection(result.value(0).toString(),
                                      result.value(1).toString());
    }
}

QString RecordingProfile::groupType(QSqlDatabase *db)
{
    if (db != NULL)
    {
        QString query = QString("SELECT profilegroups.cardtype FROM "
                            "profilegroups, recordingprofiles WHERE "
                            "profilegroups.id = recordingprofiles.profilegroup "
                            "AND recordingprofiles.id = %1;")
                            .arg(getProfileNum());
        QSqlQuery result = db->exec(query);
        if (result.isActive() && result.numRowsAffected() > 0)
        {
            result.next();
            return (result.value(0).toString());
        }
    }
    return NULL;
}

QString RecordingProfile::getName(QSqlDatabase *db, int id)
{
    if (db != NULL)
    {
        QString query = QString("SELECT name FROM recordingprofiles WHERE "
                                "id = %1;").arg(id);
        QSqlQuery result = db->exec(query);
        if (result.isActive() && result.numRowsAffected() > 0)
        {
            result.next();
            return (result.value(0).toString());
        }
    }
    return NULL;
}

