#ifndef RECORDINGPROFILE_H
#define RECORDINGPROFILE_H

#include "libmyth/settings.h"
#include "libmyth/mythwidgets.h"

const QString availProfiles[] =
      {"Default", "Live TV", "High Quality", "Low Quality", "" };

class RecordingProfile;
class VideoCompressionSettings;
class AudioCompressionSettings;

class SelectManagedListItem;


// Any setting associated with a recording profile
class RecordingProfileSetting: virtual public Setting {
protected:
    RecordingProfileSetting(const RecordingProfile& _parent):
        parentProfile(_parent) {};
    const RecordingProfile& parentProfile;
};

// A parameter associated with the profile itself
class RecordingProfileParam: public SimpleDBStorage,
                             public RecordingProfileSetting {
protected:
    RecordingProfileParam(const RecordingProfile& parentProfile, QString name):
        SimpleDBStorage("recordingprofiles", name),
        RecordingProfileSetting(parentProfile) {
        setName(name);
    };

    virtual QString whereClause(void);
};

class ImageSize;

class RecordingProfile: public ConfigurationWizard {
protected:
    class ID: virtual public IntegerSetting,
              public AutoIncrementStorage {
    public:
        ID():
            AutoIncrementStorage("recordingprofiles", "id") {
            setVisible(false);
        };

        // Should never be called because this setting is not visible
        virtual QWidget* configWidget(ConfigurationGroup *cg,
                                      QWidget* parent = NULL,
                                      const char* widgetName = NULL) {
            (void)cg; (void)parent; (void)widgetName;
            return NULL;
        };
    };

    class Name: public LineEditSetting, public RecordingProfileParam {
    public:
        Name(const RecordingProfile& parent):
            LineEditSetting(false),
            RecordingProfileParam(parent, "name") {

            setLabel(QObject::tr("Profile name"));
        };
    };
public:
    RecordingProfile(QString profName = NULL);

    virtual void loadByID(int id);
    virtual bool loadByCard(QString name, int cardid);
    virtual bool loadByGroup(QString name, QString group);

    static void fillSelections(SelectSetting* setting,
                               int group);
    static void fillSelections(SelectManagedListItem* setting,
                               int group);                           
                               
    QString groupType();
    void setCodecTypes();
    int getProfileNum(void) const {
        return id->intValue();
    };

    QString getName(void) const { return name->getValue(); };
    static QString getName(int id);
    void setName(QString newName) { name->setValue(newName); name->setRW(); };
    const ImageSize& getImageSize(void) const { return *imageSize; };
    
private:
    ID* id;
    Name* name;
    ImageSize* imageSize;
    VideoCompressionSettings *vc;
    AudioCompressionSettings *ac;
};

class RecordingProfileEditor: public ListBoxSetting, public ConfigurationDialog {
    Q_OBJECT
public:
    RecordingProfileEditor(int id, QString profName);

    virtual int exec();
    virtual void load();
    virtual void save() { };

protected slots:
    void open(int id);

protected:
    int group;
    QString labelName;
};

#endif
