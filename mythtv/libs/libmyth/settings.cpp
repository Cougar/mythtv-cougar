// C++ headers
#include <algorithm>
using namespace std;

// POSIX headers
#include <unistd.h>

// Qt widgets
#include <qlineedit.h>
#include <q3hbox.h>
#include <q3vbox.h>
#include <qlabel.h>
#include <qslider.h>
#include <qlcdnumber.h>
#include <qbuttongroup.h>
#include <qradiobutton.h>

// Qt utils
#include <qfile.h>
#include <qdatetime.h>
#include <qdir.h>

// MythTV headers
#define MYTHCONFIG
#include "settings.h"
#include "mythconfiggroups.h"
#undef MYTHCONFIG

#include "mythwidgets.h"
#include "mythcontext.h"
#include "DisplayRes.h"

/** \class Configurable
 *  \brief Configurable is the root of all the database aware widgets.
 *
 *   This is an abstract class and some methods must be implemented
 *   in children. byName(const &QString) is abstract. While 
 *   configWidget(ConfigurationGroup *, QWidget*, const char*)
 *   has an implementation, all it does is print an error message
 *   and return a NULL pointer.
 */

QWidget* Configurable::configWidget(ConfigurationGroup *cg, QWidget* parent,
                                    const char* widgetName) 
{
    (void)cg;
    (void)parent;
    (void)widgetName;
    VERBOSE(VB_IMPORTANT, "BUG: Configurable is visible, but has no "
            "configWidget");
    return NULL;
}

/** \brief This slot calls the virtual widgetInvalid(QObject*) method.
 *
 *  This should not be needed, anyone calling configWidget() should
 *  also be calling widgetInvalid() directly before configWidget()
 *  is called again on the Configurable. If widgetInvalid() is not
 *  called directly before the Configurable's configWidget() is
 *  called the Configurable may not update properly on screen, but
 *  if this is connected to from the widget's destroyed(QObject*)
 *  signal this will prevent a segfault from occurring.
 */
void Configurable::widgetDeleted(QObject *obj)
{
    widgetInvalid(obj);
}

/** \fn Configurable::enableOnSet(const QString &val)
 *  \brief This slot allows you to enable this configurable when a
 *         binary configurable is set to true.
 *  \param val signal value, should be "0" to disable, other to disable.
 */
void Configurable::enableOnSet(const QString &val)
{
    setEnabled( val != "0" );
}

/** \fn Configurable::enableOnUnset(const QString &val)
 *  \brief This slot allows you to enable this configurable when a
 *         binary configurable is set to false.
 *  \param val signal value, should be "0" to enable, other to disable.
 */
void Configurable::enableOnUnset(const QString &val)
{
    setEnabled( val == "0" );
}

QString Setting::getValue(void) const
{
    return settingValue;
}

void Setting::setValue(const QString &newValue)
{
    settingValue = newValue;
    SetChanged(true);
    emit valueChanged(settingValue);
}

int SelectSetting::findSelection(const QString &label, QString value) const
{
    value = (value.isEmpty()) ? label : value;

    for (uint i = 0; i < values.size(); i++)
    {
        if ((values[i] == value) && (labels[i] == label))
            return i;
    }

    return -1;
}

void SelectSetting::addSelection(const QString &label, QString value,
                                 bool select)
{
    value = (value.isEmpty()) ? label : value;
    
    int found = findSelection(label, value);
    if (found < 0)
    {
        labels.push_back(label);
        values.push_back(value);
        emit selectionAdded( label, value);
    }

    if (select || !isSet)
        setValue(value);
}

bool SelectSetting::removeSelection(const QString &label, QString value)
{
    value = (value.isEmpty()) ? label : value;

    int found = findSelection(label, value);
    if (found < 0)
        return false;

    bool wasSet = isSet;
    isSet = false;

    labels.erase(labels.begin() + found);
    values.erase(values.begin() + found);

    isSet = wasSet && labels.size();
    if (isSet)
    {
        current = (current > (uint)found) ? current - 1 : current;
        current = min(current, (uint) (labels.size() - 1));
    }

    emit selectionRemoved(label, value);

    return true;
}

void SelectSetting::fillSelectionsFromDir(const QDir& dir, bool absPath)
{
    QFileInfoList il = dir.entryInfoList();

    for (QFileInfoList::Iterator it = il.begin();
                                 it != il.end();
                               ++it )
    {
        QFileInfo &fi = *it;

        if (absPath)
            addSelection( fi.absFilePath() );
        else
            addSelection( fi.fileName() );
    }
}

void SelectSetting::clearSelections(void) {
    labels.clear();
    values.clear();
    isSet = false;
    emit selectionsCleared();
}

void SelectSetting::setValue(const QString &newValue)
{
    int found = getValueIndex(newValue);
    if (found < 0)
    {
        addSelection(newValue, newValue, true);
    }
    else
    {
        current = found;
        isSet   = true;
        Setting::setValue(newValue);
    }
}

void SelectSetting::setValue(int which)
{
    if ((which >= ((int) values.size())) || (which < 0))
    {
        VERBOSE(VB_IMPORTANT, "SelectSetting::setValue(): "
                "invalid index: " << which << " size: "<<values.size());
    }
    else
    {
        current = which;
        isSet   = true;
        Setting::setValue(values[current]);
    }
}

QString SelectSetting::getSelectionLabel(void) const
{
    if (!isSet || (current >= values.size()))
        return QString::null;

    return labels[current];
}

/** \fn SelectSetting::getValueIndex(QString)
 *  \brief Returns index of value in SelectSetting, or -1 if not found.
 */
int SelectSetting::getValueIndex(QString value)
{
    int ret = 0;

    selectionList::const_iterator it = values.begin();
    for (; it != values.end(); ++it, ++ret)
    {
        if (*it == value)
            return ret;
    }

    return -1;
}

bool SelectSetting::ReplaceLabel(const QString &new_label, const QString &value)
{
    int i = getValueIndex(value);

    if (i >= 0)
        labels[i] = new_label;

    return (i >= 0);
}

QWidget* LabelSetting::configWidget(ConfigurationGroup *cg, QWidget* parent,
                                    const char* widgetName) {
    (void)cg;

    QWidget* widget = new Q3HBox(parent, widgetName);
    widget->setBackgroundOrigin(QWidget::WindowOrigin);

    if (getLabel() != "")
    {
        QLabel* label = new QLabel(widget);
        label->setText(getLabel() + ":     ");
        label->setBackgroundOrigin(QWidget::WindowOrigin);
    }

    QLabel* value = new QLabel(widget);
    value->setText(getValue());
    value->setBackgroundOrigin(QWidget::WindowOrigin);

    connect(this, SIGNAL(valueChanged(const QString&)),
            value, SLOT(setText(const QString&)));

    return widget;
}

QWidget* LineEditSetting::configWidget(ConfigurationGroup *cg, QWidget* parent,
                                       const char *widgetName) {
    Q3HBox *widget;

    if (labelAboveWidget)
    {
        widget = dynamic_cast<Q3HBox*>(new Q3VBox(parent, widgetName));
        widget->setSizePolicy(QSizePolicy(QSizePolicy::Minimum, 
                                          QSizePolicy::Maximum));
    }
    else
        widget = new Q3HBox(parent, widgetName);
    widget->setBackgroundOrigin(QWidget::WindowOrigin);

    if (getLabel() != "")
    {
        QLabel* label = new QLabel(widget);
        label->setText(getLabel() + ":     ");
        label->setBackgroundOrigin(QWidget::WindowOrigin);
    }

    bxwidget = widget;
    connect(bxwidget, SIGNAL(destroyed(QObject*)),
            this,     SLOT(widgetDeleted(QObject*)));

    edit = new MythLineEdit(settingValue, widget,
                                          QString(widgetName) + "-edit");
    edit->setHelpText(getHelpText());
    edit->setBackgroundOrigin(QWidget::WindowOrigin);
    edit->setText( getValue() );

    connect(this, SIGNAL(valueChanged(const QString&)),
            edit, SLOT(setText(const QString&)));
    connect(edit, SIGNAL(textChanged(const QString&)),
            this, SLOT(setValue(const QString&)));

    if (cg)
        connect(edit, SIGNAL(changeHelpText(QString)), cg, 
                SIGNAL(changeHelpText(QString)));

    setRW(rw);
    SetPasswordEcho(password_echo);

    return bxwidget;
}

void LineEditSetting::widgetInvalid(QObject *obj)
{
    if (bxwidget == obj)
    {
        bxwidget = NULL;
        edit     = NULL;
    }
}

void LineEditSetting::setEnabled(bool b)
{
    Configurable::setEnabled(b);
    if (edit)
        edit->setEnabled(b);
}

void LineEditSetting::setVisible(bool b)
{
    Configurable::setVisible(b);
    if (edit)
    {
        //QWidget *parent = edit->parentWidget();
        if (b)
            edit->show();
        else
            edit->hide();
    }
}

void LineEditSetting::SetPasswordEcho(bool b)
{
    password_echo = b;
    if (edit)
        edit->setEchoMode(b ? QLineEdit::Password : QLineEdit::Normal);
}

void LineEditSetting::setHelpText(const QString &str)
{
    if (edit)
        edit->setHelpText(str);
    Setting::setHelpText(str);
}

void BoundedIntegerSetting::setValue(int newValue)
{
    newValue = std::max(std::min(newValue, max), min);
    IntegerSetting::setValue(newValue);
}

QWidget* SliderSetting::configWidget(ConfigurationGroup *cg, QWidget* parent,
                                     const char* widgetName) {
    Q3HBox* widget;
    if (labelAboveWidget) 
    {
        widget = dynamic_cast<Q3HBox*>(new Q3VBox(parent, widgetName));
        widget->setSizePolicy(QSizePolicy(QSizePolicy::Minimum, 
                                          QSizePolicy::Maximum));
    } 
    else
        widget = new Q3HBox(parent, widgetName);

    widget->setBackgroundOrigin(QWidget::WindowOrigin);

    if (getLabel() != "")
    {
        QLabel* label = new QLabel(widget, QString(widgetName) + "-label");
        label->setText(getLabel() + ":     ");
        label->setBackgroundOrigin(QWidget::WindowOrigin);
    }

    MythSlider* slider = new MythSlider(widget, 
                                        QString(widgetName) + "-slider");
    slider->setHelpText(getHelpText());
    slider->setMinValue(min);
    slider->setMaxValue(max);
    slider->setOrientation( Qt::Horizontal );
    slider->setLineStep(step);
    slider->setValue(intValue());
    slider->setBackgroundOrigin(QWidget::WindowOrigin);

    QLCDNumber* lcd = new QLCDNumber(widget, QString(widgetName) + "-lcd");
    lcd->setMode(QLCDNumber::DEC);
    lcd->setSegmentStyle(QLCDNumber::Flat);
    lcd->display(intValue());

    connect(slider, SIGNAL(valueChanged(int)), lcd, SLOT(display(int)));
    connect(slider, SIGNAL(valueChanged(int)), this, SLOT(setValue(int)));
    connect(this, SIGNAL(valueChanged(int)), slider, SLOT(setValue(int)));

    if (cg)
        connect(slider, SIGNAL(changeHelpText(QString)), cg,
                SIGNAL(changeHelpText(QString)));

    return widget;
}

SpinBoxSetting::SpinBoxSetting(
    Storage *_storage, int _min, int _max, int _step, 
    bool _allow_single_step, QString _special_value_text) :
    BoundedIntegerSetting(_storage, _min, _max, _step),
    spinbox(NULL), relayEnabled(true),
    sstep(_allow_single_step), svtext("")
{
    if (!_special_value_text.isEmpty())
        svtext = _special_value_text;

    IntegerSetting *iset = (IntegerSetting *) this;
    connect(iset, SIGNAL(valueChanged(     int)),
            this, SLOT(  relayValueChanged(int)));
}

QWidget* SpinBoxSetting::configWidget(ConfigurationGroup *cg, QWidget* parent,
                                      const char* widgetName) {
    Q3HBox* box;
    if (labelAboveWidget) 
    {
        box = dynamic_cast<Q3HBox*>(new Q3VBox(parent, widgetName));
        box->setSizePolicy(QSizePolicy(QSizePolicy::Minimum, 
                                       QSizePolicy::Maximum));
    } 
    else
        box = new Q3HBox(parent, widgetName);
    box->setBackgroundOrigin(QWidget::WindowOrigin);

    if (getLabel() != "")
    {
        QLabel* label = new QLabel(box);
        label->setBackgroundOrigin(QWidget::WindowOrigin);
        label->setText(getLabel() + ":     ");
    }

    bxwidget = box;
    connect(bxwidget, SIGNAL(destroyed(QObject*)),
            this,     SLOT(widgetDeleted(QObject*)));

    spinbox = new MythSpinBox(box, QString(widgetName) + "MythSpinBox", sstep);
    spinbox->setHelpText(getHelpText());
    spinbox->setBackgroundOrigin(QWidget::WindowOrigin);
    spinbox->setMinValue(min);
    spinbox->setMaxValue(max);
    // only set step size if greater than default (1), otherwise
    // this will screw up the single-step/jump behavior of the MythSpinBox
    if (1 < step)
        spinbox->setLineStep(step);
    spinbox->setValue(intValue());
    if (!svtext.isEmpty())
        spinbox->setSpecialValueText(svtext);

    connect(spinbox, SIGNAL(valueChanged(int)), this, SLOT(setValue(int)));

    if (cg)
        connect(spinbox, SIGNAL(changeHelpText(QString)), cg,
                SIGNAL(changeHelpText(QString)));

    return bxwidget;
}

void SpinBoxSetting::widgetInvalid(QObject *obj)
{
    if (bxwidget == obj)
    {
        bxwidget = NULL;
        spinbox  = NULL;
    }
}

void SpinBoxSetting::setValue(int newValue)
{
    newValue = std::max(std::min(newValue, max), min);
    if (spinbox && (spinbox->value() != newValue))
    {
        //int old = intValue();
        spinbox->setValue(newValue);
    }
    else if (intValue() != newValue)
    {
        BoundedIntegerSetting::setValue(newValue);
    }
}

void SpinBoxSetting::setFocus(void)
{
    if (spinbox)
        spinbox->setFocus();
}

void SpinBoxSetting::clearFocus(void)
{
    if (spinbox)
        spinbox->clearFocus();
}

bool SpinBoxSetting::hasFocus(void) const
{
    if (spinbox)
        return spinbox->hasFocus();

    return false;
}

void SpinBoxSetting::relayValueChanged(int newValue)
{
    if (relayEnabled)
        emit valueChanged(configName, newValue);
}

void SpinBoxSetting::setHelpText(const QString &str)
{
    if (spinbox)
        spinbox->setHelpText(str);
    BoundedIntegerSetting::setHelpText(str);
}

QWidget* SelectLabelSetting::configWidget(ConfigurationGroup *cg,
                                          QWidget* parent,
                                          const char* widgetName) {
    (void)cg;

    Q3HBox* widget;
    if (labelAboveWidget) 
    {
        widget = dynamic_cast<Q3HBox*>(new Q3VBox(parent, widgetName));
        widget->setSizePolicy(QSizePolicy(QSizePolicy::Minimum, 
                                          QSizePolicy::Maximum));
    } 
    else
        widget = new Q3HBox(parent, widgetName);

    widget->setBackgroundOrigin(QWidget::WindowOrigin);

    if (getLabel() != "")
    {
        QLabel* label = new QLabel(widget);
        label->setText(getLabel() + ":     ");
        label->setBackgroundOrigin(QWidget::WindowOrigin);
    }

    QLabel* value = new QLabel(widget);
    value->setText(labels[current]);
    value->setBackgroundOrigin(QWidget::WindowOrigin);

    connect(this, SIGNAL(valueChanged(const QString&)),
            value, SLOT(setText(const QString&)));

    return widget;
}

QWidget* ComboBoxSetting::configWidget(ConfigurationGroup *cg, QWidget* parent,
                                       const char* widgetName) {
    Q3HBox* box;
    if (labelAboveWidget) 
    {
        box = dynamic_cast<Q3HBox*>(new Q3VBox(parent, widgetName));
        box->setSizePolicy(QSizePolicy(QSizePolicy::Minimum, 
                                       QSizePolicy::Maximum));
    } 
    else
        box = new Q3HBox(parent, widgetName);

    box->setBackgroundOrigin(QWidget::WindowOrigin);

    if (getLabel() != "")
    {
        QLabel* label = new QLabel(box);
        label->setText(getLabel() + ":     ");
        label->setBackgroundOrigin(QWidget::WindowOrigin);
        box->setStretchFactor(label, 0);
    }

    bxwidget = box;
    connect(bxwidget, SIGNAL(destroyed(QObject*)),
            this,     SLOT(widgetDeleted(QObject*)));

    widget = new MythComboBox(rw, box);
    widget->setHelpText(getHelpText());
    widget->setBackgroundOrigin(QWidget::WindowOrigin);
    box->setStretchFactor(widget, 1);

    for(unsigned int i = 0 ; i < labels.size() ; ++i)
        widget->insertItem(labels[i]);

    if (isSet)
        widget->setCurrentItem(current);

    if (1 < step)
        widget->setStep(step);

    if (rw)
    {
        connect(widget, SIGNAL(highlighted(const QString &)),
                this, SLOT(setValue(const QString &)));
        connect(widget, SIGNAL(activated(const QString &)),
                this, SLOT(setValue(const QString &)));
    }
    else
    {
        connect(widget, SIGNAL(highlighted(int)),
                this, SLOT(setValue(int)));
        connect(widget, SIGNAL(activated(int)),
                this, SLOT(setValue(int)));
    }

    connect(this, SIGNAL(selectionsCleared()),
            widget, SLOT(clear()));

    if (cg)
        connect(widget, SIGNAL(changeHelpText(QString)), cg,
                SIGNAL(changeHelpText(QString)));

    return bxwidget;
}

void ComboBoxSetting::widgetInvalid(QObject *obj)
{
    if (bxwidget == obj)
    {
        bxwidget = NULL;
        widget   = NULL;
    }
}

void ComboBoxSetting::setEnabled(bool b)
{
    Configurable::setEnabled(b);
    if (widget)
        widget->setEnabled(b);
}

void ComboBoxSetting::setVisible(bool b)
{
    Configurable::setVisible(b);
    if (widget)
    {
        //QWidget *parent = widget->parentWidget();
        if (b)
            widget->show();
        else
            widget->hide();
    }
}

void ComboBoxSetting::setValue(QString newValue)
{
    for (uint i = 0; i < values.size(); i++)
    {
        if (values[i] == newValue)
        {
            setValue(i);
            break;
        }
    }

    if (rw)
    {
        Setting::setValue(newValue);
        if (widget)
            widget->setCurrentItem(current);
    }
};

void ComboBoxSetting::setValue(int which)
{
    if (widget)
        widget->setCurrentItem(which);
    SelectSetting::setValue(which);
};

void ComboBoxSetting::addSelection(
    const QString &label, QString value, bool select)
{
    if ((findSelection(label, value) < 0) && widget)
    {
        widget->insertItem(label);
    }

    SelectSetting::addSelection(label, value, select);

    if (widget && isSet)
        widget->setCurrentItem(current);
}

bool ComboBoxSetting::removeSelection(const QString &label, QString value)
{
    SelectSetting::removeSelection(label, value);
    if (!widget)
        return true;

    for (uint i = 0; ((int) i) < widget->count(); i++)
    {
        if (widget->text(i) == label)
        {
            widget->removeItem(i);
            if (isSet)
                widget->setCurrentItem(current);
            return true;
        }
    }

    return false;
}

void ComboBoxSetting::setHelpText(const QString &str)
{
    if (widget)
        widget->setHelpText(str);
    SelectSetting::setHelpText(str);
}

void HostRefreshRateComboBox::ChangeResolution(const QString& resolution)
{
    clearSelections();
    
    const vector<short> list = GetRefreshRates(resolution);
    addSelection(QObject::tr("Any"), "0");
    int hz50 = -1, hz60 = -1;
    for (uint i=0; i<list.size(); ++i)
    {        
        QString sel = QString::number(list[i]);
        addSelection(sel+" Hz", sel);
        hz50 = (50 == list[i]) ? i : hz50;
        hz60 = (60 == list[i]) ? i : hz60;
    }
    
    setValue(0);
    if ("640x480" == resolution || "720x480" == resolution)
        setValue(hz60+1);
    if ("640x576" == resolution || "720x576" == resolution)
        setValue(hz50+1);
    
    setEnabled(list.size());
}

const vector<short> HostRefreshRateComboBox::GetRefreshRates(const QString &res)
{
    QStringList slist = QStringList::split("x", res);
    int w = 0, h = 0;
    bool ok0 = false, ok1 = false;
    if (2 == slist.size())
    {
        w = slist[0].toInt(&ok0);
        h = slist[1].toInt(&ok1);
    }

    DisplayRes *display_res = DisplayRes::GetDisplayRes();
    if (display_res && ok0 && ok1)
        return display_res->GetRefreshRates(w, h);    

    vector<short> list;
    return list;
}

void PathSetting::addSelection(const QString& label,
                               QString value,
                               bool select) {
    QString pathname = label;
    if (value != QString::null)
        pathname = value;

    if (mustexist && !QFile(pathname).exists())
        return;

    ComboBoxSetting::addSelection(label, value, select);
}

QTime TimeSetting::timeValue(void) const {
    return QTime::fromString(getValue(), Qt::ISODate);
}

void TimeSetting::setValue(const QTime& newValue) {
    Setting::setValue(newValue.toString(Qt::ISODate));
}

QString DateSetting::getValue(void) const
{
    return settingValue;
}

QDate DateSetting::dateValue(void) const {
    return QDate::fromString(getValue(), Qt::ISODate);
}

void DateSetting::setValue(const QDate& newValue) {
    Setting::setValue(newValue.toString(Qt::ISODate));
}

QWidget* RadioSetting::configWidget(ConfigurationGroup *cg, QWidget* parent,
                                    const char* widgetName) {
    Q3ButtonGroup* widget = new Q3ButtonGroup(parent, widgetName);
    widget->setBackgroundOrigin(QWidget::WindowOrigin);
    widget->setTitle(getLabel());

    for( unsigned i = 0 ; i < labels.size() ; ++i ) {
        QRadioButton* button = new QRadioButton(widget, NULL);
        button->setBackgroundOrigin(QWidget::WindowOrigin);
        button->setText(labels[i]);
        if (isSet && i == current)
            button->setDown(true);
    }

    cg = cg;

    return widget;
}

QWidget* CheckBoxSetting::configWidget(ConfigurationGroup *cg, QWidget* parent,
                                       const char* widgetName) {
    widget = new MythCheckBox(parent, widgetName);
    connect(widget, SIGNAL(destroyed(QObject*)),
            this,   SLOT(widgetDeleted(QObject*)));

    widget->setHelpText(getHelpText());
    widget->setBackgroundOrigin(QWidget::WindowOrigin);
    widget->setText(getLabel());
    widget->setChecked(boolValue());

    connect(widget, SIGNAL(toggled(bool)),
            this, SLOT(setValue(bool)));
    connect(this, SIGNAL(valueChanged(bool)),
            widget, SLOT(setChecked(bool)));

    if (cg)
        connect(widget, SIGNAL(changeHelpText(QString)), cg,
                SIGNAL(changeHelpText(QString)));

    return widget;
}

void CheckBoxSetting::widgetInvalid(QObject *obj)
{
    widget = (widget == obj) ? NULL : widget;
}

void CheckBoxSetting::setEnabled(bool fEnabled)
{
    BooleanSetting::setEnabled(fEnabled);
    if (widget)
        widget->setEnabled(fEnabled);
}

void CheckBoxSetting::setHelpText(const QString &str)
{
    if (widget)
        widget->setHelpText(str);
    BooleanSetting::setHelpText(str);
}

void AutoIncrementDBSetting::save(QString table)
{
    if (intValue() == 0) 
    {
        // Generate a new, unique ID
        QString querystr = QString("INSERT INTO " + table + " (" 
                + column + ") VALUES (0);");

        MSqlQuery query(MSqlQuery::InitCon());
        query.exec(querystr);

        if (!query.isActive() || query.numRowsAffected() < 1) 
        {
            MythContext::DBError("inserting row", query);
            return;
        }
        // XXX -- HACK BEGIN:
        // lastInsertID fails with "QSqlQuery::value: not positioned on a valid record"
        // if we get a invalid QVariant we workaround the problem by taking advantage
        // of mysql always incrementing the auto increment pointer
        // this breaks if someone modifies the auto increment pointer
        //setValue(query.lastInsertId().toInt());

        QVariant var = query.lastInsertId();

        if (var.type())
            setValue(var.toInt());
        else
        {
            querystr = QString("SELECT MAX(" + column + ") FROM " + table + ";");
            if (query.exec(querystr) && query.next())
            {
                int lii = query.value(0).toInt();
                lii = lii ? lii : 1;
                setValue(lii);
            }
            else
                VERBOSE(VB_IMPORTANT, "Can't determine the Id of the last insert"
                        "QSqlQuery.lastInsertId() failed, the workaround failed too!");
        }
        // XXX -- HACK END:
    }
}

void AutoIncrementDBSetting::save() 
{
    save(table);
}

void ListBoxSetting::setEnabled(bool b)
{
    Configurable::setEnabled(b);
    if (widget)
        widget->setEnabled(b);
}

void ListBoxSetting::clearSelections(void)
{
    SelectSetting::clearSelections();
    if (widget)
        widget->clear();
}

void ListBoxSetting::addSelection(
    const QString &label, QString value, bool select)
{
    SelectSetting::addSelection(label, value, select);
    if (widget)
    {
        widget->insertItem(label);
        //widget->triggerUpdate(true);
    }
};

bool ListBoxSetting::ReplaceLabel(
    const QString &new_label, const QString &value)
{
    int i = getValueIndex(value);

    if ((i >= 0) && SelectSetting::ReplaceLabel(label, value) && widget)
    {
        widget->changeItem(new_label, i);
        return true;
    }

    return false;
}

QWidget* ListBoxSetting::configWidget(ConfigurationGroup *cg, QWidget* parent, 
                                      const char* widgetName)
{
    QWidget* box = new Q3VBox(parent, widgetName);
    box->setBackgroundOrigin(QWidget::WindowOrigin);

    if (getLabel() != "")
    {
        QLabel* label = new QLabel(box);
        label->setText(getLabel());
        label->setBackgroundOrigin(QWidget::WindowOrigin);
    }

    bxwidget = box;
    connect(bxwidget, SIGNAL(destroyed(QObject*)),
            this,     SLOT(widgetDeleted(QObject*)));

    widget = new MythListBox(box);
    widget->setBackgroundOrigin(QWidget::WindowOrigin);
    widget->setHelpText(getHelpText());
    if (eventFilter)
        widget->installEventFilter(eventFilter);

    for(unsigned int i = 0 ; i < labels.size() ; ++i) {
        widget->insertItem(labels[i]);
        if (isSet && current == i)
            widget->setCurrentItem(i);
    }

    connect(this, SIGNAL(selectionsCleared()),
            widget, SLOT(clear()));
    connect(widget, SIGNAL(accepted(int)),
            this, SIGNAL(accepted(int)));
    connect(widget, SIGNAL(menuButtonPressed(int)),
            this, SIGNAL(menuButtonPressed(int)));
    connect(widget, SIGNAL(editButtonPressed(int)),
            this, SIGNAL(editButtonPressed(int)));
    connect(widget, SIGNAL(deleteButtonPressed(int)),
            this, SIGNAL(deleteButtonPressed(int)));
    connect(this, SIGNAL(valueChanged(const QString&)),
            widget, SLOT(setCurrentItem(const QString&)));
    connect(widget, SIGNAL(highlighted(int)),
            this, SLOT(setValueByIndex(int)));

    if (cg)
        connect(widget, SIGNAL(changeHelpText(QString)), cg,
                SIGNAL(changeHelpText(QString)));

    widget->setFocus();
    widget->setSelectionMode(selectionMode);

    return bxwidget;
}

void ListBoxSetting::widgetInvalid(QObject *obj)
{
    if (bxwidget == obj)
    {
        bxwidget = NULL;
        widget   = NULL;
    }
}

void ListBoxSetting::setSelectionMode(MythListBox::SelectionMode mode)
{
   selectionMode = mode;
   if (widget)
       widget->setSelectionMode(selectionMode);
}

void ListBoxSetting::setValueByIndex(int index)
{
    if (((uint)index) < values.size())
        setValue(values[index]);
}

void ListBoxSetting::setHelpText(const QString &str)
{
    if (widget)
        widget->setHelpText(str);
    SelectSetting::setHelpText(str);
}

void ImageSelectSetting::addImageSelection(const QString& label,
                                           QImage* image,
                                           QString value,
                                           bool select) {
    images.push_back(image);
    addSelection(label, value, select);
}

ImageSelectSetting::~ImageSelectSetting()
{
    Teardown();
}

void ImageSelectSetting::deleteLater(void)
{
    Teardown();
    SelectSetting::deleteLater();
}

void ImageSelectSetting::Teardown(void)
{
    while (images.size())
    {
        QImage *tmp = images.back();
        images.pop_back();
        delete tmp;
    }
    bxwidget   = NULL;
    imagelabel = NULL;
    combo      = NULL;
}

void ImageSelectSetting::imageSet(int num)
{
    if (num >= (int)images.size())
        return;

    if (!images[current])
        return;

    QImage temp = *(images[current]);
    temp = temp.scaled((int)(184 * m_hmult), (int)(138 * m_hmult),
                        Qt::KeepAspectRatio);

    QPixmap tmppix(temp);
    imagelabel->setPixmap(tmppix);
}

QWidget* ImageSelectSetting::configWidget(ConfigurationGroup *cg, 
                                          QWidget* parent,
                                          const char* widgetName) 
{
    int width = 0, height = 0;

    gContext->GetScreenSettings(width, m_wmult, height, m_hmult);

    Q3HBox* box;
    if (labelAboveWidget) 
    {
        box = dynamic_cast<Q3HBox*>(new Q3VBox(parent, widgetName));
        box->setSizePolicy(QSizePolicy(QSizePolicy::Minimum, 
                                       QSizePolicy::Maximum));
    } 
    else
        box = new Q3HBox(parent, widgetName);

    box->setBackgroundOrigin(QWidget::WindowOrigin);

    if (getLabel() != "")
    {
        QLabel* label = new QLabel(box);
        label->setText(getLabel() + ":");
        label->setBackgroundOrigin(QWidget::WindowOrigin);
    }

    MythComboBox *widget = new MythComboBox(false, box);
    widget->setBackgroundOrigin(QWidget::WindowOrigin);
    combo = widget;

    QLabel *testlabel = new QLabel(box);
    testlabel->setText("  ");
    testlabel->setBackgroundOrigin(QWidget::WindowOrigin);

    bxwidget = box;
    connect(bxwidget, SIGNAL(destroyed(QObject*)),
            this,     SLOT(widgetDeleted(QObject*)));

    imagelabel = new QLabel(box);
    imagelabel->setBackgroundOrigin(QWidget::WindowOrigin);

    for (unsigned int i = 0 ; i < images.size() ; ++i)
        widget->insertItem(labels[i]);

    if (isSet)
        widget->setCurrentItem(current);
    else
        current = 0;

    if (images.size() != 0 && current < images.size() && images[current])
    { 
        QImage temp = *(images[current]);
        temp = temp.scaled((int)(184 * m_hmult), (int)(138 * m_hmult),
                            Qt::KeepAspectRatio);
 
        QPixmap tmppix(temp);
        imagelabel->setPixmap(tmppix);
    }
    else
    {
        QPixmap tmppix((int)(184 * m_hmult), (int)(138 * m_hmult));
        tmppix.fill(Qt::black);

        imagelabel->setPixmap(tmppix);
    }

    connect(widget, SIGNAL(highlighted(int)), this, SLOT(setValue(int)));
    connect(widget, SIGNAL(highlighted(int)), this, SLOT(imageSet(int)));
    connect(widget, SIGNAL(activated(int)), this, SLOT(setValue(int)));
    connect(widget, SIGNAL(activated(int)), this, SLOT(imageSet(int)));

    connect(this, SIGNAL(selectionsCleared()),
            widget, SLOT(clear()));

    if (cg)
        connect(widget, SIGNAL(changeHelpText(QString)), cg, 
                SIGNAL(changeHelpText(QString)));

    return bxwidget;
}

void ImageSelectSetting::widgetInvalid(QObject *obj)
{
    if (bxwidget == obj)
    {
        bxwidget   = NULL;
        imagelabel = NULL;
        combo      = NULL;
    }
}

void ImageSelectSetting::setHelpText(const QString &str)
{
    if (combo)
        combo->setHelpText(str);
    SelectSetting::setHelpText(str);
}

HostnameSetting::HostnameSetting(Storage *storage) : Setting(storage)
{
    setVisible(false);
    
    setValue(gContext->GetHostName());
}

void ChannelSetting::fillSelections(SelectSetting* setting) {

    // this should go somewhere else, in something that knows about
    // channels and how they're stored in the database.  We're just a
    // selector.

    MSqlQuery query(MSqlQuery::InitCon()); 
    query.prepare("SELECT name, chanid FROM channel;");
    if (query.exec() && query.isActive() && query.size() > 0)
        while (query.next())
            setting->addSelection(query.value(0).toString(),
                                  QString::number(query.value(1).toInt()));
}

QWidget* ButtonSetting::configWidget(ConfigurationGroup* cg, QWidget* parent,
                                     const char* widgetName)
{
    (void) cg;
    button = new MythPushButton(parent, widgetName);
    connect(button, SIGNAL(destroyed(QObject*)),
            this,   SLOT(widgetDeleted(QObject*)));

    button->setText(getLabel());
    button->setHelpText(getHelpText());

    connect(button, SIGNAL(pressed()), this, SIGNAL(pressed()));
    connect(button, SIGNAL(pressed()), this, SLOT(SendPressedString()));

    if (cg)
        connect(button, SIGNAL(changeHelpText(QString)),
                cg, SIGNAL(changeHelpText(QString)));

    return button;
}

void ButtonSetting::widgetInvalid(QObject *obj)
{
    button = (button == obj) ? NULL : button;
}

void ButtonSetting::SendPressedString(void)
{
    emit pressed(name);
}

void ButtonSetting::setEnabled(bool fEnabled)
{
    Configurable::setEnabled(fEnabled);
    if (button)
        button->setEnabled(fEnabled);
}

void ButtonSetting::setHelpText(const QString &str)
{
    if (button)
        button->setHelpText(str);
    Setting::setHelpText(str);
}

QWidget* ProgressSetting::configWidget(ConfigurationGroup* cg, QWidget* parent,
                                       const char* widgetName) {
    (void)cg;

    Q3HBox* widget = new Q3HBox(parent,widgetName);
    widget->setBackgroundOrigin(QWidget::WindowOrigin);

    if (getLabel() != "")
    {
        QLabel* label = new QLabel(getLabel() + "     :", widget, widgetName);
        label->setBackgroundOrigin(QWidget::WindowOrigin);
    }

    Q3ProgressBar* progress = new Q3ProgressBar(totalSteps, widget, widgetName);
    progress->setBackgroundOrigin(QWidget::WindowOrigin);

    connect(this, SIGNAL(valueChanged(int)), progress, SLOT(setProgress(int)));
    progress->setProgress(intValue());
    return widget;
}
