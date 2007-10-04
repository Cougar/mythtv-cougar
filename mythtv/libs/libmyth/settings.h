#ifndef SETTINGS_H
#define SETTINGS_H

#include <qobject.h>
#include <qstring.h>
#include <qdeepcopy.h>

#include <iostream>
#include <vector>
#include <map>

#include "mythexp.h"
#include "mythwidgets.h"
#include "mythdialogs.h"
#include "mythdbcon.h"

using namespace std;

//class QSqlDatabase;
class QWidget;
class ConfigurationGroup;
class QDir;
class QWidgetStack;
class Setting;

class MPUBLIC Storage
{
  public:
    Storage() {}
    virtual ~Storage() {}

    virtual void load(void) = 0;
    virtual void save(void) = 0;
    virtual void save(QString /*destination*/) { }
};

class MPUBLIC DBStorage : public Storage
{
  public:
    DBStorage(Setting *_setting, QString _table, QString _column) :
        setting(_setting), table(QDeepCopy<QString>(_table)),
        column(QDeepCopy<QString>(_column)) {}

    virtual ~DBStorage() {}

  protected:
    QString getColumn(void) const { return QDeepCopy<QString>(column); };
    QString getTable(void) const { return QDeepCopy<QString>(table); };

    Setting *setting;
    QString table;
    QString column;
};

class MPUBLIC SimpleDBStorage : public DBStorage
{
  public:
    SimpleDBStorage(Setting *_setting,
                    QString _table, QString _column) :
        DBStorage(_setting, _table, _column) {}
    virtual ~SimpleDBStorage() {};

    virtual void load(void);
    virtual void save(void);
    virtual void save(QString destination);

  protected:
    virtual QString whereClause(MSqlBindings&) = 0;
    virtual QString setClause(MSqlBindings& bindings);
};

class MPUBLIC TransientStorage : public Storage
{
  public:
    TransientStorage() {}
    virtual ~TransientStorage() {}

    virtual void load(void) { }
    virtual void save(void) { }
};

class MPUBLIC HostDBStorage : public SimpleDBStorage
{
  public:
    HostDBStorage(Setting *_setting, QString name);

  protected:
    virtual QString whereClause(MSqlBindings &bindings);
    virtual QString setClause(MSqlBindings &bindings);
};

class MPUBLIC GlobalDBStorage : public SimpleDBStorage
{
  public:
    GlobalDBStorage(Setting *_setting, QString name);

  protected:
    virtual QString whereClause(MSqlBindings &bindings);
    virtual QString setClause(MSqlBindings &bindings);
};

///////////////////////////////////////////////////////////////////////////////

class MPUBLIC Configurable : public QObject
{
    Q_OBJECT

  public:
    // Create and return a widget for configuring this entity
    virtual QWidget* configWidget(ConfigurationGroup *cg, QWidget* parent,
                                  const char* widgetName = 0);

    // A name for looking up the setting
    void setName(QString str) {
        configName = QDeepCopy<QString>(str);
        if (label == QString::null)
            setLabel(str);
    };
    QString getName(void) const { return QDeepCopy<QString>(configName); };
    virtual Setting* byName(const QString &name) = 0;

    // A label displayed to the user
    void setLabel(QString str) { label = QDeepCopy<QString>(str); }
    QString getLabel(void) const { return QDeepCopy<QString>(label); }
    void setLabelAboveWidget(bool l = true) { labelAboveWidget = l; }

    void setHelpText(QString str) { helptext = QDeepCopy<QString>(str); }
    QString getHelpText(void) const { return QDeepCopy<QString>(helptext); }

    void setVisible(bool b) { visible = b; };
    bool isVisible(void) const { return visible; };

    virtual void setEnabled(bool b) { enabled = b; }
    bool isEnabled() { return enabled; }

    Storage *GetStorage(void) { return storage; }

  public slots:
    virtual void enableOnSet(const QString &val);
    virtual void enableOnUnset(const QString &val);

  protected:
    Configurable(Storage *_storage) :
        labelAboveWidget(false), enabled(true), storage(_storage),
        configName(""), label(""), helptext(""), visible(true) { }
    virtual ~Configurable() { }

  protected:
    bool labelAboveWidget; 
    bool enabled;
    Storage *storage;
    QString configName;
    QString label;
    QString helptext;
    bool visible;
};

class MPUBLIC Setting : public Configurable
{
    Q_OBJECT

  public:
    // Gets
    bool isChanged(void) const { return changed; }
    virtual QString getValue(void) const;

    // non-const Gets
    virtual Setting *byName(const QString &name)
        { return (name == configName) ? this : NULL; }

    // Sets
    void SetChanged(bool c) { changed = c;       }
    void setUnchanged(void) { SetChanged(false); }
    void setChanged(void)   { SetChanged(true);  }

  public slots:
    virtual void setValue(const QString &newValue);

  signals:
    void valueChanged(const QString&);

  protected:
    Setting(Storage *_storage) : Configurable(_storage), changed(false) {};
    virtual ~Setting() {};

  protected:
    QString settingValue;
    bool    changed;
};

///////////////////////////////////////////////////////////////////////////////

// Read-only display of a setting
class MPUBLIC LabelSetting : public Setting
{
  protected:
    LabelSetting(Storage *_storage) : Setting(_storage) { }
  public:
    virtual QWidget* configWidget(ConfigurationGroup *cg, QWidget* parent, 
                                  const char* widgetName = 0);
};

class MPUBLIC LineEditSetting : public Setting
{
  protected:
    LineEditSetting(Storage *_storage, bool readwrite = true) :
        Setting(_storage), edit(NULL), rw(readwrite), password_echo(false) { }

  public:
    virtual QWidget* configWidget(ConfigurationGroup *cg, QWidget* parent, 
                                  const char* widgetName = 0);

    void setRW(bool readwrite = true)
    {
        rw = readwrite;
        if (edit)
            edit->setRW(rw);
    }

    void setRO(void) { setRW(false); }

    virtual void setEnabled(bool b);
    virtual void setVisible(bool b);
    virtual void SetPasswordEcho(bool b);

  private:
    MythLineEdit* edit;
    bool rw;
    bool password_echo;
};

// TODO: set things up so that setting the value as a string emits
// the int signal also
class MPUBLIC IntegerSetting : public Setting
{
    Q_OBJECT

  protected:
    IntegerSetting(Storage *_storage) : Setting(_storage) { }

  public:
    int intValue(void) const {
        return settingValue.toInt();
    };
public slots:
    virtual void setValue(int newValue) {
        Setting::setValue(QString::number(newValue));
        emit valueChanged(newValue);
    };
signals:
    void valueChanged(int newValue);
};

class MPUBLIC BoundedIntegerSetting : public IntegerSetting
{
  protected:
    BoundedIntegerSetting(Storage *_storage, int _min, int _max, int _step) :
        IntegerSetting(_storage), min(_min), max(_max), step(_step) { }

  public:
    virtual void setValue(int newValue);

  protected:
    int min;
    int max;
    int step;
};

class MPUBLIC SliderSetting: public BoundedIntegerSetting {
protected:
    SliderSetting(Storage *_storage, int min, int max, int step) :
        BoundedIntegerSetting(_storage, min, max, step) { }
public:
    virtual QWidget* configWidget(ConfigurationGroup *cg, QWidget* parent, 
                                  const char* widgetName = 0);
};

class MPUBLIC SpinBoxSetting: public BoundedIntegerSetting
{
    Q_OBJECT

  public:
    SpinBoxSetting(Storage *_storage, int min, int max, int step, 
                   bool allow_single_step = false,
                   QString special_value_text = "");

    virtual QWidget *configWidget(ConfigurationGroup *cg, QWidget *parent, 
                                  const char *widgetName = 0);

    virtual void setValue(int newValue);

    void setFocus(void);
    void clearFocus(void);
    bool hasFocus(void) const;

    void SetRelayEnabled(bool enabled) { relayEnabled = enabled; }
    bool IsRelayEnabled(void) const { return relayEnabled; }

  signals:
    void valueChanged(const QString &name, int newValue);

  private slots:
    void relayValueChanged(int newValue);

  private:
    MythSpinBox *spinbox;
    bool         relayEnabled;
    bool         sstep;
    QString      svtext;
};

class MPUBLIC SelectSetting : public Setting
{
    Q_OBJECT

  protected:
    SelectSetting(Storage *_storage) :
        Setting(_storage), current(0), isSet(false) { }

  public:
    virtual int  findSelection(  const QString &label,
                                 QString        value  = QString::null) const;
    virtual void addSelection(   const QString &label,
                                 QString        value  = QString::null,
                                 bool           select = false);
    virtual bool removeSelection(const QString &label,
                                 QString        value  = QString::null);

    virtual void clearSelections(void);

    virtual void fillSelectionsFromDir(const QDir& dir, bool absPath=true);

signals:
    void selectionAdded(const QString& label, QString value);
    void selectionRemoved(const QString &label, const QString &value);
    void selectionsCleared(void);

public slots:

    virtual void setValue(const QString& newValue);
    virtual void setValue(int which);

    virtual QString getSelectionLabel(void) const;
    virtual int getValueIndex(QString value);

protected:
    typedef vector<QString> selectionList;
    selectionList labels;
    selectionList values;
    unsigned current;
    bool isSet;
};

class MPUBLIC SelectLabelSetting : public SelectSetting
{
  protected:
    SelectLabelSetting(Storage *_storage) : SelectSetting(_storage) { }

  public:
    virtual QWidget* configWidget(ConfigurationGroup *cg, QWidget* parent, 
                                  const char* widgetName = 0);
};

class MPUBLIC ComboBoxSetting: public SelectSetting {
    Q_OBJECT

protected:
    ComboBoxSetting(Storage *_storage, bool _rw = false, int _step = 1) :
        SelectSetting(_storage), rw(_rw), widget(NULL), step(_step) { }

public:
    virtual void setValue(QString newValue);
    virtual void setValue(int which);

    virtual QWidget* configWidget(ConfigurationGroup *cg, QWidget* parent, 
                                  const char* widgetName = 0);

    void setFocus() { if (widget) widget->setFocus(); }

    virtual void setEnabled(bool b);
    virtual void setVisible(bool b);

public slots:
    void addSelection(const QString &label,
                      QString value = QString::null,
                      bool select = false);
    bool removeSelection(const QString &label,
                         QString value = QString::null);

protected slots:
    void widgetDestroyed() { widget=NULL; };

private:
    bool rw;
    MythComboBox *widget;

protected:
    int step;
};

class MPUBLIC ListBoxSetting: public SelectSetting {
    Q_OBJECT
public:
    ListBoxSetting(Storage *_storage) :
        SelectSetting(_storage), widget(NULL),
        selectionMode(MythListBox::Single) { }

    virtual QWidget* configWidget(ConfigurationGroup *cg, QWidget* parent, 
                                  const char* widgetName = 0);

    void setFocus() { if (widget) widget->setFocus(); }
    void setSelectionMode(MythListBox::SelectionMode mode);
    void setCurrentItem(int i) { if (widget) widget->setCurrentItem(i); }
    void setCurrentItem(const QString& str)  { if (widget) widget->setCurrentItem(str); }

    virtual void setEnabled(bool b);

signals:
    void accepted(int);
    void menuButtonPressed(int);
    void editButtonPressed(int);
    void deleteButtonPressed(int);

  public slots:
    void addSelection(const QString &label,
                      QString        value  = QString::null,
                      bool           select = false);

protected slots:
    void setValueByIndex(int index);
    void widgetDestroyed() { widget=NULL; };
protected:
    MythListBox* widget;
    MythListBox::SelectionMode selectionMode;
};

class MPUBLIC RadioSetting : public SelectSetting
{
public:
    RadioSetting(Storage *_storage) : SelectSetting(_storage) { }
    virtual QWidget* configWidget(ConfigurationGroup *cg, QWidget* parent, 
                                  const char* widgetName = 0);
};

class MPUBLIC ImageSelectSetting: public SelectSetting {
    Q_OBJECT
public:
    ImageSelectSetting(Storage *_storage) : SelectSetting(_storage) { }
    virtual ~ImageSelectSetting();
    virtual QWidget* configWidget(ConfigurationGroup *cg, QWidget* parent, 
                                  const char* widgetName = 0);

    virtual void addImageSelection(const QString& label,
                                   QImage* image,
                                   QString value=QString::null,
                                   bool select=false);
protected slots:
    void imageSet(int);

protected:
    vector<QImage*> images;
    QLabel *imagelabel;
    float m_hmult, m_wmult;
};

class MPUBLIC BooleanSetting : public Setting
{
    Q_OBJECT

  public:
    BooleanSetting(Storage *_storage) : Setting(_storage) {}

    bool boolValue(void) const {
        return getValue().toInt() != 0;
    };
public slots:
    virtual void setValue(bool check) {
        if (check)
            Setting::setValue("1");
        else
            Setting::setValue("0");
        emit valueChanged(check);
    };
signals:
    void valueChanged(bool);
};

class MPUBLIC CheckBoxSetting: public BooleanSetting {
public:
    CheckBoxSetting(Storage *_storage) :
        BooleanSetting(_storage), widget(NULL) { }
    virtual QWidget* configWidget(ConfigurationGroup *cg, QWidget* parent,
                                  const char* widgetName = 0);
    virtual void setEnabled(bool b);
protected:
    MythCheckBox *widget;
};

class MPUBLIC PathSetting : public ComboBoxSetting
{
public:
    PathSetting(Storage *_storage, bool _mustexist):
        ComboBoxSetting(_storage, true), mustexist(_mustexist) { }

    // TODO: this should support globbing of some sort
    virtual void addSelection(const QString& label,
                              QString value=QString::null,
                              bool select=false);

    // Use a combobox for now, maybe a modified file dialog later
    //virtual QWidget* configWidget(ConfigurationGroup *cg, QWidget* parent, const char* widgetName = 0);

protected:
    bool mustexist;
};

class MPUBLIC HostnameSetting : public Setting
{
  public:
    HostnameSetting(Storage *_storage);
};

class MPUBLIC ChannelSetting : public SelectSetting
{
  public:
    ChannelSetting(Storage *_storage) : SelectSetting(_storage)
    {
        setLabel("Channel");
    };

    static void fillSelections(SelectSetting* setting);
    virtual void fillSelections() {
        fillSelections(this);
    };
};

class QDate;
class MPUBLIC DateSetting : public Setting
{
    Q_OBJECT

  public:
    DateSetting(Storage *_storage) : Setting(_storage) { }

    QDate dateValue(void) const;

    virtual QWidget* configWidget(ConfigurationGroup* cg, QWidget* parent,
                                  const char* widgetName = 0);

 public slots:
    void setValue(const QDate& newValue);
};

class QTime;
class MPUBLIC TimeSetting : public Setting
{
    Q_OBJECT

  public:
    TimeSetting(Storage *_storage) : Setting(_storage) { }
    QTime timeValue(void) const;

    virtual QWidget* configWidget(ConfigurationGroup* cg, QWidget* parent,
                                  const char* widgetName = 0);

 public slots:
    void setValue(const QTime& newValue);
};

class MPUBLIC AutoIncrementDBSetting :
    public IntegerSetting, public DBStorage
{
  public:
    AutoIncrementDBSetting(QString _table, QString _column) :
        IntegerSetting(this), DBStorage(this, _table, _column)
    {
        setValue(0);
    }

    virtual void load() { };
    virtual void save();
    virtual void save(QString destination);
};

class MPUBLIC ButtonSetting: public Setting
{
    Q_OBJECT

  public:
    ButtonSetting(Storage *_storage, QString _name = "button") :
        Setting(_storage), name(QDeepCopy<QString>(_name)), button(NULL) { }

    virtual QWidget* configWidget(ConfigurationGroup* cg, QWidget* parent,
                                  const char* widgetName=0);

    virtual void setEnabled(bool b);

signals:
    void pressed();
    void pressed(QString name);

protected slots:
    void SendPressedString();

protected:
    QString name;
    MythPushButton *button;
};

class MPUBLIC ProgressSetting : public IntegerSetting
{
  public:
    ProgressSetting(Storage *_storage, int _totalSteps) :
        IntegerSetting(_storage), totalSteps(_totalSteps) { }

    QWidget* configWidget(ConfigurationGroup* cg, QWidget* parent,
                          const char* widgetName = 0);

private:
    int totalSteps;
};

///////////////////////////////////////////////////////////////////////////////

class MPUBLIC TransButtonSetting :
    public ButtonSetting, public TransientStorage
{
  public:
    TransButtonSetting(QString name = "button") :
        ButtonSetting(this, name), TransientStorage() { }
};

class MPUBLIC TransLabelSetting :
    public LabelSetting, public TransientStorage
{
  public:
    TransLabelSetting() : LabelSetting(this), TransientStorage() { }
};

class MPUBLIC TransLineEditSetting :
    public LineEditSetting, public TransientStorage
{
  public:
    TransLineEditSetting(bool rw = true) :
        LineEditSetting(this, rw), TransientStorage() { }
};

class MPUBLIC TransCheckBoxSetting :
    public CheckBoxSetting, public TransientStorage
{
  public:
    TransCheckBoxSetting() : CheckBoxSetting(this), TransientStorage() { }
};

class MPUBLIC TransComboBoxSetting :
    public ComboBoxSetting, public TransientStorage
{
  public:
    TransComboBoxSetting(bool rw = false, int _step = 1) :
        ComboBoxSetting(this, rw, _step), TransientStorage() { }
};

class MPUBLIC TransSpinBoxSetting :
    public SpinBoxSetting, public TransientStorage
{
  public:
    TransSpinBoxSetting(int minv, int maxv, int step,
                        bool allow_single_step = false,
                        QString special_value_text = "") :
        SpinBoxSetting(this, minv, maxv, step,
                       allow_single_step, special_value_text) { }
};

///////////////////////////////////////////////////////////////////////////////

class MPUBLIC HostSlider : public SliderSetting, public HostDBStorage
{
  public:
    HostSlider(const QString &name, int min, int max, int step) :
        SliderSetting(this, min, max, step),
        HostDBStorage(this, name) { }
};

class MPUBLIC HostSpinBox: public SpinBoxSetting, public HostDBStorage
{
  public:
    HostSpinBox(const QString &name, int min, int max, int step, 
                  bool allow_single_step = false) :
        SpinBoxSetting(this, min, max, step, allow_single_step),
        HostDBStorage(this, name) { }
};

class MPUBLIC HostCheckBox : public CheckBoxSetting, public HostDBStorage
{
  public:
    HostCheckBox(const QString &name) :
        CheckBoxSetting(this), HostDBStorage(this, name) { }
    virtual ~HostCheckBox() { ; }
};

class MPUBLIC HostComboBox : public ComboBoxSetting, public HostDBStorage
{
  public:
    HostComboBox(const QString &name, bool rw = false) :
        ComboBoxSetting(this, rw), HostDBStorage(this, name) { }
    virtual ~HostComboBox() { ; }
};

class MPUBLIC HostRefreshRateComboBox : public HostComboBox
{
    Q_OBJECT
  public:
    HostRefreshRateComboBox(const QString &name, bool rw = false) :
        HostComboBox(name, rw) { }
    virtual ~HostRefreshRateComboBox() { ; }

  public slots:
    virtual void ChangeResolution(const QString& resolution);

  private:
    static const vector<short> GetRefreshRates(const QString &resolution);
};

class MPUBLIC HostTimeBox : public ComboBoxSetting, public HostDBStorage
{
  public:
    HostTimeBox(const QString &name, const QString &defaultTime = "00:00",
                const int interval = 1) :
        ComboBoxSetting(this, false, 30 / interval),
        HostDBStorage(this, name)
    {
        int hour;
        int minute;
        QString timeStr;

        for (hour = 0; hour < 24; hour++)
        {
            for (minute = 0; minute < 60; minute += interval)
            {
                timeStr = timeStr.sprintf("%02d:%02d", hour, minute);
                addSelection(timeStr, QDeepCopy<QString>(timeStr),
                             timeStr == defaultTime);
            }
        }
    }
};

class MPUBLIC HostLineEdit: public LineEditSetting, public HostDBStorage
{
  public:
    HostLineEdit(const QString &name, bool rw = true) :
        LineEditSetting(this, rw), HostDBStorage(this, name) { }
};

class MPUBLIC HostImageSelect : public ImageSelectSetting, public HostDBStorage
{
  public:
    HostImageSelect(const QString &name) :
        ImageSelectSetting(this), HostDBStorage(this, name) { }
};

///////////////////////////////////////////////////////////////////////////////

class MPUBLIC GlobalSlider : public SliderSetting, public GlobalDBStorage
{
  public:
    GlobalSlider(const QString &name, int min, int max, int step) :
        SliderSetting(this, min, max, step), GlobalDBStorage(this, name) { }
};

class MPUBLIC GlobalSpinBox : public SpinBoxSetting, public GlobalDBStorage
{
  public:
    GlobalSpinBox(const QString &name, int min, int max, int step,
                   bool allow_single_step = false) :
        SpinBoxSetting(this, min, max, step, allow_single_step),
        GlobalDBStorage(this, name) { }
};

class MPUBLIC GlobalCheckBox : public CheckBoxSetting, public GlobalDBStorage
{
  public:
    GlobalCheckBox(const QString &name) :
        CheckBoxSetting(this), GlobalDBStorage(this, name) { }
};

class MPUBLIC GlobalComboBox : public ComboBoxSetting, public GlobalDBStorage
{
  public:
    GlobalComboBox(const QString &name, bool rw = false) :
        ComboBoxSetting(this, rw), GlobalDBStorage(this, name) { }
};

class MPUBLIC GlobalLineEdit : public LineEditSetting, public GlobalDBStorage
{
  public:
    GlobalLineEdit(const QString &name, bool rw = true) :
        LineEditSetting(this, rw), GlobalDBStorage(this, name) { }
};

class MPUBLIC GlobalImageSelect :
   public ImageSelectSetting, public GlobalDBStorage
{
  public:
    GlobalImageSelect(const QString &name) :
        ImageSelectSetting(this), GlobalDBStorage(this, name) { }
};

class MPUBLIC GlobalTimeBox : public ComboBoxSetting, public GlobalDBStorage
{
  public:
    GlobalTimeBox(const QString &name, const QString &defaultTime = "00:00",
                  const int interval = 1) :
        ComboBoxSetting(this, false, 30 / interval),
        GlobalDBStorage(this, name)
    {
        int hour;
        int minute;
        QString timeStr;

        for (hour = 0; hour < 24; hour++)
        {
            for (minute = 0; minute < 60; minute += interval)
            {
                timeStr = timeStr.sprintf("%02d:%02d", hour, minute);
                addSelection(timeStr, QDeepCopy<QString>(timeStr),
                             timeStr == defaultTime);
            }
        }
    }
};

///////////////////////////////////////////////////////////////////////////////

class MPUBLIC ConfigurationGroup : public Setting, public Storage
{
    Q_OBJECT
  public:
    ConfigurationGroup(bool luselabel = true, bool luseframe = true,
                       bool lzeroMargin = false, bool lzeroSpace = false) :
        Setting(this),
        uselabel(luselabel),     useframe(luseframe),
        zeroMargin(lzeroMargin), zeroSpace(lzeroSpace)
    {
    }

    virtual void deleteLater(void);

    void addChild(Configurable* child) {
        children.push_back(child);
    };

    virtual Setting *byName(const QString &name);

    virtual void load();

    virtual void save();
    virtual void save(QString destination);

    void setUseLabel(bool useit) { uselabel = useit; }
    void setUseFrame(bool useit) { useframe = useit; }

    void setOptions(bool luselabel = true, bool luseframe = true,
                    bool lzeroMargin = false, bool lzeroSpace = false)
    {
        uselabel = luselabel; useframe = luseframe;
        zeroMargin = lzeroMargin; zeroSpace = lzeroSpace;
    }

  signals:
    void changeHelpText(QString);
    
  protected:
    virtual ~ConfigurationGroup();

  protected:
    typedef vector<Configurable*> childList;
    childList children;
    bool uselabel;
    bool useframe;
    bool zeroMargin;
    bool zeroSpace;
};

class MPUBLIC VerticalConfigurationGroup : public ConfigurationGroup
{
  public:
    VerticalConfigurationGroup(
        bool luselabel   = true,  bool luseframe  = true,
        bool lzeroMargin = false, bool lzeroSpace = false) :
        ConfigurationGroup(luselabel, luseframe, lzeroMargin, lzeroSpace),
        widget(NULL), confgrp(NULL), layout(NULL)
    {
    }

    virtual QWidget *configWidget(ConfigurationGroup *cg,
                                  QWidget            *parent,
                                  const char         *widgetName);

    bool replaceChild(Configurable *old_child, Configurable *new_child);
    void repaint(void);

  protected:
    /// You need to call deleteLater to delete QObject
    virtual ~VerticalConfigurationGroup() { }

  private:
    vector<QWidget*>    childwidget;
    QGroupBox          *widget;
    ConfigurationGroup *confgrp;
    QVBoxLayout        *layout;
};

class MPUBLIC HorizontalConfigurationGroup : public ConfigurationGroup
{
  public:
    HorizontalConfigurationGroup(
        bool luselabel   = true,  bool luseframe  = true,
        bool lzeroMargin = false, bool lzeroSpace = false) :
        ConfigurationGroup(luselabel, luseframe, lzeroMargin, lzeroSpace)
    {
    }

    virtual QWidget *configWidget(ConfigurationGroup *cg,
                                  QWidget            *parent,
                                  const char         *widgetName);

  protected:
    /// You need to call deleteLater to delete QObject
    virtual ~HorizontalConfigurationGroup() { }
};

class MPUBLIC GridConfigurationGroup : public ConfigurationGroup
{
  public:
    GridConfigurationGroup(uint col,
                           bool uselabel   = true,  bool useframe  = true,
                           bool zeroMargin = false, bool zeroSpace = false) :
        ConfigurationGroup(uselabel, useframe, zeroMargin, zeroSpace), 
        columns(col)
    {
    }

    virtual QWidget *configWidget(ConfigurationGroup *cg,
                                  QWidget            *parent,
                                  const char         *widgetName);

  protected:
    /// You need to call deleteLater to delete QObject
    virtual ~GridConfigurationGroup() { }

  private:
    uint columns;
};

class MPUBLIC StackedConfigurationGroup : public ConfigurationGroup
{
    Q_OBJECT

  public:
    StackedConfigurationGroup(
        bool uselabel   = true,  bool useframe  = true,
        bool zeroMargin = false, bool zeroSpace = false) :
        ConfigurationGroup(uselabel, useframe, zeroMargin, zeroSpace),
        widget(NULL), confgrp(NULL), top(0), saveAll(true)
    {
    }

    virtual QWidget* configWidget(ConfigurationGroup *cg, QWidget* parent,
                                  const char* widgetName = 0);

    void raise(Configurable* child);
    virtual void save(void);
    virtual void save(QString destination);

    // save all children, or only the top?
    void setSaveAll(bool b) { saveAll = b; };

    void addChild(Configurable*);
    void removeChild(Configurable*);

  signals:
    void raiseWidget(int);

  protected:
    /// You need to call deleteLater to delete QObject
    virtual ~StackedConfigurationGroup() { }

  protected:
    vector<QWidget*>    childwidget;
    QWidgetStack       *widget;
    ConfigurationGroup *confgrp;
    uint                top;
    bool                saveAll;
};


class MPUBLIC TriggeredConfigurationGroup : public ConfigurationGroup
{
    Q_OBJECT

  public:
    TriggeredConfigurationGroup(
        bool uselabel         = true,  bool useframe        = true,
        bool zeroMargin       = false, bool zeroSpace       = false,
        bool stack_uselabel   = true,  bool stack_useframe  = true,
        bool stack_zeroMargin = false, bool stack_zeroSpace = false) :
        ConfigurationGroup(uselabel, useframe, zeroMargin, zeroSpace),
        stackUseLabel(stack_uselabel),     stackUseFrame(stack_useframe),
        stackZeroMargin(stack_zeroMargin), stackZeroSpace(stack_zeroSpace),
        isVertical(true),                  isSaveAll(true),
        configLayout(NULL),                configStack(NULL),
        trigger(NULL),                     widget(NULL)
    {
    }

    // Commands

    virtual void addChild(Configurable *child);

    void addTarget(QString triggerValue, Configurable *target);
    void removeTarget(QString triggerValue);

    virtual QWidget *configWidget(ConfigurationGroup *cg, 
                                  QWidget            *parent,
                                  const char         *widgetName);

    virtual Setting *byName(const QString &settingName);

    virtual void load(void);
    virtual void save(void);
    virtual void save(QString destination);

    void repaint(void);

    // Sets

    void SetVertical(bool vert);

    virtual void setSaveAll(bool b)
    {
        if (configStack)
            configStack->setSaveAll(b);
        isSaveAll = b;
    }

    void setTrigger(Configurable *_trigger);

  protected slots:
    virtual void triggerChanged(const QString &value)
    {
        if (configStack)
            configStack->raise(triggerMap[value]);
    }

  protected:
    /// You need to call deleteLater to delete QObject
    virtual ~TriggeredConfigurationGroup() { }
    void VerifyLayout(void);

  protected:
    bool stackUseLabel;
    bool stackUseFrame;
    bool stackZeroMargin;
    bool stackZeroSpace;
    bool isVertical;
    bool isSaveAll;
    ConfigurationGroup          *configLayout;
    StackedConfigurationGroup   *configStack;
    Configurable                *trigger;
    QMap<QString,Configurable*>  triggerMap;
    QWidget                     *widget;
};
    
class MPUBLIC TabbedConfigurationGroup : public ConfigurationGroup
{
    Q_OBJECT

  public:
    TabbedConfigurationGroup() :
        ConfigurationGroup(true, true, false, false) { }

    virtual QWidget* configWidget(ConfigurationGroup *cg,
                                  QWidget            *parent,
                                  const char         *widgetName);
};

///////////////////////////////////////////////////////////////////////////////

class MPUBLIC ConfigPopupDialogWidget : public MythPopupBox
{
    Q_OBJECT
  public:
    ConfigPopupDialogWidget(MythMainWindow* parent, const char *widgetName) :
        MythPopupBox(parent, widgetName) { }

    virtual void keyPressEvent(QKeyEvent* e);
    void accept() { MythPopupBox::accept(); }
    void reject() { MythPopupBox::reject(); }

  protected:
    /// You need to call deleteLater to delete QObject
    virtual ~ConfigPopupDialogWidget() { }
};

class MPUBLIC ConfigurationPopupDialog : public VerticalConfigurationGroup
{
    Q_OBJECT

  public:
    ConfigurationPopupDialog() :
        VerticalConfigurationGroup(), dialog(NULL), label(NULL) { }

    virtual void deleteLater(void);

    virtual MythDialog *dialogWidget(
        MythMainWindow *parent, const char* widgetName);

    int exec(bool saveOnAccept = true);

    virtual void setLabel(QString str);

  public slots:
    void accept(void) { if (dialog) dialog->accept(); }
    void reject(void) { if (dialog) dialog->reject(); }

  signals:
    void popupDone(int);

  protected:
    /// You need to call deleteLater to delete QObject
    virtual ~ConfigurationPopupDialog() { }

  protected:
    ConfigPopupDialogWidget *dialog;
    QLabel                  *label;
};

///////////////////////////////////////////////////////////////////////////////

class MPUBLIC ConfigurationDialogWidget : public MythDialog
{
    Q_OBJECT

  public:
    ConfigurationDialogWidget(MythMainWindow *parent, 
                              const char     *widgetName) :
        MythDialog(parent, widgetName) { }

    virtual void keyPressEvent(QKeyEvent* e);

  signals:
    void editButtonPressed(void);
    void deleteButtonPressed(void);
};

/** \class ConfigurationDialog
 *  \brief A ConfigurationDialog that uses a ConfigurationGroup
 *         all children on one page in a vertical layout.
 */
class MPUBLIC ConfigurationDialog : public Storage
{
  public:
    ConfigurationDialog() : dialog(NULL), cfgGrp(new ConfigurationGroup()) { }
    virtual ~ConfigurationDialog() { cfgGrp->deleteLater(); }

    // Make a modal dialog containing configWidget
    virtual MythDialog *dialogWidget(MythMainWindow *parent,
                                     const char     *widgetName);

    // Show a dialogWidget, and save if accepted
    virtual int exec(bool saveOnExec = true, bool doLoad = true);

    virtual void load(void) { cfgGrp->load(); }
    virtual void save(void) { cfgGrp->save(); }
    virtual void save(QString destination) { cfgGrp->save(destination); }

    virtual void addChild(Configurable *child);

    virtual Setting *byName(const QString &settingName)
        { return cfgGrp->byName(settingName); }

    void setLabel(const QString &label);

  protected:
    typedef vector<Configurable*> ChildList;

    ChildList           cfgChildren;
    MythDialog         *dialog;
    ConfigurationGroup *cfgGrp;
};

/** \class ConfigurationWizard
 *  \brief A ConfigurationDialog that uses a ConfigurationGroup
 *         with one child per page.
 */
class MPUBLIC ConfigurationWizard : public ConfigurationDialog
{
  public:
    ConfigurationWizard() : ConfigurationDialog() {}

    virtual MythDialog *dialogWidget(MythMainWindow *parent,
                                     const char *widgetName);
};

/** \class JumpConfigurationWizard
 *  \brief A jump wizard is a group with one child per page, and jump buttons
 */
class MPUBLIC JumpConfigurationWizard :
    public QObject, public ConfigurationWizard
{
    Q_OBJECT

  public:
    virtual MythDialog *dialogWidget(MythMainWindow *parent,
                                     const char     *widgetName);

  protected slots:
    void showPage(QString);

  protected:
    /// You need to call deleteLater to delete QObject
    virtual ~JumpConfigurationWizard() { }

  protected:
    vector<QWidget*> childWidgets;
};

class MPUBLIC JumpPane : public VerticalConfigurationGroup
{
    Q_OBJECT

  public:
    JumpPane(const QStringList &labels, const QStringList &helptext);

  signals:
    void pressed(QString);
};

#endif
