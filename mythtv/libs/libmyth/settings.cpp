#include "settings.h"
#include "mythwidgets.h"
#include <qsqldatabase.h>
#include <qlineedit.h>
#include <qhbox.h>
#include <qvbox.h>
#include <qlayout.h>
#include <qlabel.h>
#include <qslider.h>
#include <qspinbox.h>
#include <qlcdnumber.h>
#include <qcombobox.h>
#include <qgroupbox.h>
#include <qvgroupbox.h>
#include <qbuttongroup.h>
#include <qradiobutton.h>
#include <qcheckbox.h>
#include <qwidgetstack.h>
#include <qtabdialog.h>
#include <qfile.h>
#include <qlistview.h>
#include <qapplication.h>
#include <qwidget.h>
#include <unistd.h>
#include <qdatetime.h>
#include <qdir.h>

#include "mythcontext.h"
#include "mythwizard.h"

QWidget* Configurable::configWidget(ConfigurationGroup *cg, QWidget* parent,
                                    const char* widgetName) 
{
    (void)cg;
    (void)parent;
    (void)widgetName;
    cout << "BUG: Configurable is visible, but has no configWidget\n";
    return NULL;
}

ConfigurationGroup::~ConfigurationGroup()  {
    for(childList::iterator i = children.begin() ;
        i != children.end() ;
        ++i )
        delete *i;
}

Setting* ConfigurationGroup::byName(QString name) {
    for(childList::iterator i = children.begin() ;
        i != children.end() ;
        ++i ) {
        
        Setting* c = (*i)->byName(name);
        if (c != NULL)
            return c;
    }
    
    return NULL;
}

void ConfigurationGroup::load(QSqlDatabase* db) {
    for(childList::iterator i = children.begin() ;
        i != children.end() ;
        ++i )
        (*i)->load(db);
}

void ConfigurationGroup::save(QSqlDatabase* db) {
    for(childList::iterator i = children.begin() ;
        i != children.end() ;
        ++i )
        (*i)->save(db);
}

QWidget* VerticalConfigurationGroup::configWidget(ConfigurationGroup *cg, 
                                                  QWidget* parent,
                                                  const char* widgetName) 
{    
    QGroupBox* widget = new QGroupBox(parent, widgetName);
    widget->setBackgroundOrigin(QWidget::WindowOrigin);

    if (!useframe)
        widget->setFrameShape(QFrame::NoFrame);

    QVBoxLayout *layout = NULL;

    float wmult = 0, hmult = 0;

    gContext->GetScreenSettings(wmult, hmult);

    if (uselabel)
    {
        layout = new QVBoxLayout(widget, (int)(28 * hmult));
        // This makes weird and bad things happen in qt -mdz 2002/12/28
        //widget->setInsideMargin(20);
        widget->setTitle(getLabel());
    }
    else
        layout = new QVBoxLayout(widget, (int)(10 * hmult));

    for(unsigned i = 0 ; i < children.size() ; ++i)
        if (children[i]->isVisible())
            layout->add(children[i]->configWidget(cg, widget, NULL));
      
    if (cg)
    {
        connect(this, SIGNAL(changeHelpText(QString)), cg,
                SIGNAL(changeHelpText(QString)));
    } 

    return widget;
}

QWidget* HorizontalConfigurationGroup::configWidget(ConfigurationGroup *cg, 
                                                    QWidget* parent, 
                                                    const char* widgetName) 
{
    QGroupBox* widget = new QGroupBox(parent, widgetName);
    widget->setBackgroundOrigin(QWidget::WindowOrigin);

    if (!useframe)
        widget->setFrameShape(QFrame::NoFrame);

    QHBoxLayout *layout = NULL;

    float wmult = 0, hmult = 0;

    gContext->GetScreenSettings(wmult, hmult);

    if (uselabel)
    {
        layout = new QHBoxLayout(widget, (int)(28 * hmult));
        // This makes weird and bad things happen in qt -mdz 2002/12/28
        //widget->setInsideMargin(20);
        widget->setTitle(getLabel());
    }
    else
        layout = new QHBoxLayout(widget, (int)(10 * hmult));

    for(unsigned i = 0 ; i < children.size() ; ++i)
        if (children[i]->isVisible())
            layout->add(children[i]->configWidget(cg, widget, NULL));

    if (cg)
    {
        connect(this, SIGNAL(changeHelpText(QString)), cg,
                SIGNAL(changeHelpText(QString)));
    }

    return widget;
}

QWidget* StackedConfigurationGroup::configWidget(ConfigurationGroup *cg, 
                                                 QWidget* parent,
                                                 const char* widgetName) 
{
    QWidgetStack* widget = new QWidgetStack(parent, widgetName);
    widget->setBackgroundOrigin(QWidget::WindowOrigin);

    for(unsigned i = 0 ; i < children.size() ; ++i)
        if (children[i]->isVisible())
            widget->addWidget(children[i]->configWidget(cg, widget,
                                                        NULL), i);

    widget->raiseWidget(top);

    connect(this, SIGNAL(raiseWidget(int)),
            widget, SLOT(raiseWidget(int)));

    if (cg)
    {
        connect(this, SIGNAL(changeHelpText(QString)), cg,
                SIGNAL(changeHelpText(QString)));
    }

    return widget;
}

QWidget* TabbedConfigurationGroup::configWidget(ConfigurationGroup *cg, 
                                                QWidget* parent,
                                                const char* widgetName) 
{
    QTabDialog* widget = new QTabDialog(parent, widgetName);
    widget->setBackgroundOrigin(QWidget::WindowOrigin);
    
    for(unsigned i = 0 ; i < children.size() ; ++i)
        if (children[i]->isVisible())
            widget->addTab(children[i]->configWidget(cg, widget), 
                           children[i]->getLabel());

    if (cg)
    {
        connect(this, SIGNAL(changeHelpText(QString)), cg,
                SIGNAL(changeHelpText(QString)));
    }

    return widget;
};

void StackedConfigurationGroup::raise(Configurable* child) {
    for(unsigned i = 0 ; i < children.size() ; ++i)
        if (children[i] == child) {
            top = i;
            emit raiseWidget((int)i);
            return;
        }
    cout << "BUG: StackedConfigurationGroup::raise(): unrecognized child " << child << " on setting " << getName() << '/' << getLabel() << endl;
}

void StackedConfigurationGroup::save(QSqlDatabase* db) {
    if (saveAll)
        ConfigurationGroup::save(db);
    else if (top < children.size())
        children[top]->save(db);
}

void TriggeredConfigurationGroup::setTrigger(Configurable* _trigger) {
    trigger = _trigger;
    // Make sure the stack is after the trigger
    addChild(configStack = new StackedConfigurationGroup());

    connect(trigger, SIGNAL(valueChanged(const QString&)),
            this, SLOT(triggerChanged(const QString&)));
}

void TriggeredConfigurationGroup::addTarget(QString triggerValue, Configurable* target) {
    configStack->addChild(target);
    triggerMap[triggerValue] = target;
}

void SelectSetting::addSelection(const QString& label, QString value, bool select) {
    if (value == QString::null)
        value = label;
    
    bool found = false;
    for(unsigned i = 0 ; i < values.size() ; ++i)
        if ((values[i] == value) &&
            (labels[i] == label)) {
            found = true;
            break;
        }

    if (!found)
    {
        labels.push_back(label);
        values.push_back(value);
    
        emit selectionAdded(label, value);
    
        if (select || !isSet)
            setValue(value);
    }
}

void SelectSetting::fillSelectionsFromDir(const QDir& dir, bool absPath) {
     const QFileInfoList *il = dir.entryInfoList();
     if (!il)
         return;

     QFileInfoListIterator it( *il );
     QFileInfo *fi;

     for(; (fi = it.current()) != 0; ++it)
         if (absPath)
             addSelection(fi->absFilePath());
         else
             addSelection(fi->fileName());
}

void SelectSetting::clearSelections(void) {
    labels.clear();
    values.clear();
    isSet = false;
    emit selectionsCleared();
}

void SelectSetting::setValue(const QString& newValue)  {
    bool found = false;
    for(unsigned i = 0 ; i < values.size() ; ++i)
        if (values[i] == newValue) {
            current = i;
            found = true;
            isSet = true;
            break;
        }
    if (found)
        Setting::setValue(newValue);
    else
        addSelection(newValue, newValue, true);
}

QWidget* LabelSetting::configWidget(ConfigurationGroup *cg, QWidget* parent,
                                    const char* widgetName) {
    (void)cg;

    QWidget* widget = new QHBox(parent, widgetName);
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
    QHBox *widget;

    if (labelAboveWidget)
    {
        widget = dynamic_cast<QHBox*>(new QVBox(parent, widgetName));
        widget->setSizePolicy(QSizePolicy(QSizePolicy::Minimum, 
                                          QSizePolicy::Maximum));
    }
    else
        widget = new QHBox(parent, widgetName);
    widget->setBackgroundOrigin(QWidget::WindowOrigin);

    if (getLabel() != "")
    {
        QLabel* label = new QLabel(widget);
        label->setText(getLabel() + ":     ");
        label->setBackgroundOrigin(QWidget::WindowOrigin);
    }

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

    edit->setRW(rw);

    return widget;
}

QWidget* SliderSetting::configWidget(ConfigurationGroup *cg, QWidget* parent,
                                     const char* widgetName) {
    QHBox* widget;
    if (labelAboveWidget) 
    {
        widget = dynamic_cast<QHBox*>(new QVBox(parent, widgetName));
        widget->setSizePolicy(QSizePolicy(QSizePolicy::Minimum, 
                                          QSizePolicy::Maximum));
    } 
    else
        widget = new QHBox(parent, widgetName);

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
    slider->setOrientation(QSlider::Horizontal);
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

QWidget* SpinBoxSetting::configWidget(ConfigurationGroup *cg, QWidget* parent,
                                      const char* widgetName) {
    QHBox* box;
    if (labelAboveWidget) 
    {
        box = dynamic_cast<QHBox*>(new QVBox(parent, widgetName));
        box->setSizePolicy(QSizePolicy(QSizePolicy::Minimum, 
                                       QSizePolicy::Maximum));
    } 
    else
        box = new QHBox(parent, widgetName);
    box->setBackgroundOrigin(QWidget::WindowOrigin);

    if (getLabel() != "")
    {
        QLabel* label = new QLabel(box);
        label->setBackgroundOrigin(QWidget::WindowOrigin);
        label->setText(getLabel() + ":     ");
    }

    MythSpinBox* spinbox = new MythSpinBox(box, QString(widgetName) + 
                                           "MythSpinBox", sstep);
    spinbox->setHelpText(getHelpText());
    spinbox->setBackgroundOrigin(QWidget::WindowOrigin);
    spinbox->setMinValue(min);
    spinbox->setMaxValue(max);
    // only set step size if greater than default (1), otherwise
    // this will screw up the single-step/jump behavior of the MythSpinBox
    if (1 < step)
	spinbox->setLineStep(step);
    spinbox->setValue(intValue());

    connect(spinbox, SIGNAL(valueChanged(int)), this, SLOT(setValue(int)));
    connect(this, SIGNAL(valueChanged(int)), spinbox, SLOT(setValue(int)));

    if (cg)
        connect(spinbox, SIGNAL(changeHelpText(QString)), cg,
                SIGNAL(changeHelpText(QString)));

    return box;
}

QWidget* SelectLabelSetting::configWidget(ConfigurationGroup *cg,
                                          QWidget* parent,
                                          const char* widgetName) {
    (void)cg;

    QHBox* widget;
    if (labelAboveWidget) 
    {
        widget = dynamic_cast<QHBox*>(new QVBox(parent, widgetName));
        widget->setSizePolicy(QSizePolicy(QSizePolicy::Minimum, 
                                          QSizePolicy::Maximum));
    } 
    else
        widget = new QHBox(parent, widgetName);

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
    QHBox* box;
    if (labelAboveWidget) 
    {
        box = dynamic_cast<QHBox*>(new QVBox(parent, widgetName));
        box->setSizePolicy(QSizePolicy(QSizePolicy::Minimum, 
                                       QSizePolicy::Maximum));
    } 
    else
        box = new QHBox(parent, widgetName);

    box->setBackgroundOrigin(QWidget::WindowOrigin);

    if (getLabel() != "")
    {
        QLabel* label = new QLabel(box);
        label->setText(getLabel() + ":     ");
        label->setBackgroundOrigin(QWidget::WindowOrigin);
        box->setStretchFactor(label, 0);
    }

    widget = new MythComboBox(rw, box);
    widget->setHelpText(getHelpText());
    widget->setBackgroundOrigin(QWidget::WindowOrigin);
    box->setStretchFactor(widget, 1);

    for(unsigned int i = 0 ; i < labels.size() ; ++i)
        widget->insertItem(labels[i]);

    if (isSet)
        widget->setCurrentItem(current);

    if (rw)
        connect(widget, SIGNAL(highlighted(const QString &)),
                this, SLOT(setValue(const QString &)));
    else
        connect(widget, SIGNAL(highlighted(int)),
                this, SLOT(setValue(int)));

    connect(widget, SIGNAL(destroyed()),
            this, SLOT(widgetDestroyed()));
    connect(this, SIGNAL(selectionsCleared()),
            widget, SLOT(clear()));

    if (cg)
        connect(widget, SIGNAL(changeHelpText(QString)), cg,
                SIGNAL(changeHelpText(QString)));

    return box;
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

QWidget* TimeSetting::configWidget(ConfigurationGroup* cg, QWidget* parent,
                                   const char* widgetName) {
    //QString timeformat = gContext->GetSetting("TimeFormat", "h:mm AP");
    (void)cg;
    (void)parent;
    (void)widgetName;
    return NULL;
}

QDate DateSetting::dateValue(void) const {
    return QDate::fromString(getValue(), Qt::ISODate);
}

void DateSetting::setValue(const QDate& newValue) {
    Setting::setValue(newValue.toString(Qt::ISODate));
}

QWidget* DateSetting::configWidget(ConfigurationGroup* cg, QWidget* parent,
                                   const char* widgetName) {
    //QString dateformat = gContext->GetSetting("DateFormat", "ddd MMMM d");
    (void)cg;
    (void)parent;
    (void)widgetName;
    return NULL;
}

QWidget* RadioSetting::configWidget(ConfigurationGroup *cg, QWidget* parent,
                                    const char* widgetName) {
    QButtonGroup* widget = new QButtonGroup(parent, widgetName);
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
    MythCheckBox* widget = new MythCheckBox(parent, widgetName);
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

void ConfigurationDialogWidget::keyPressEvent(QKeyEvent* e) 
{
    bool handled = false;
    QStringList actions;
    if (gContext->GetMainWindow()->TranslateKeyPress("qt", e, actions))
    {
        for (unsigned int i = 0; i < actions.size() && !handled; i++)
        {
            QString action = actions[i];
            handled = true;

            if (action == "SELECT")
                accept();
            else if (action == "ESCAPE")
                reject();
            else
                handled = false;
        }
    }

    if (!handled)
        MythDialog::keyPressEvent(e);
}

MythDialog* ConfigurationDialog::dialogWidget(MythMainWindow *parent,
                                              const char *widgetName) 
{
    dialog = new ConfigurationDialogWidget(parent, widgetName);

    float wmult = 0, hmult = 0;

    gContext->GetScreenSettings(wmult, hmult);

    QVBoxLayout* layout = new QVBoxLayout(dialog, (int)(20 * hmult));
    layout->addWidget(configWidget(NULL, dialog));

    return dialog;
}

int ConfigurationDialog::exec(QSqlDatabase* db, bool saveOnAccept, bool doLoad) 
{
    if (doLoad)
        load(db);

    MythDialog* dialog = dialogWidget(gContext->GetMainWindow());
    dialog->Show();

    int ret;

    if ((ret = dialog->exec()) == QDialog::Accepted && saveOnAccept)
        save(db);

    delete dialog;

    return ret;
}

MythDialog* ConfigurationWizard::dialogWidget(MythMainWindow *parent,
                                              const char *widgetName) {
    MythWizard* wizard = new MythWizard(parent, widgetName);
    dialog = wizard;

    connect(this, SIGNAL(changeHelpText(QString)), wizard,
            SLOT(setHelpText(QString)));

    unsigned i;
    for(i = 0 ; i < children.size() ; ++i)
        if (children[i]->isVisible()) {
            QWidget* child = children[i]->configWidget(this, parent);

            wizard->addPage(child, children[i]->getLabel());
            if (i == children.size()-1)
                // Last page always has finish enabled.  Stuff should
                // have sane defaults.
                wizard->setFinishEnabled(child, true);
        }
        
    return wizard;
}

void SimpleDBStorage::load(QSqlDatabase* db) {
    QString querystr = QString("SELECT %1 FROM %2 WHERE %3;")
        .arg(column).arg(table).arg(whereClause());
    QSqlQuery query = db->exec(querystr);

    if (query.isActive() && query.numRowsAffected() > 0) {
        query.next();
        QString result = query.value(0).toString();
        if (result != QString::null) {
          result = QString::fromUtf8(query.value(0).toString());
          setValue(result);
          setUnchanged();
        }
   }
 }

void SimpleDBStorage::save(QSqlDatabase* db) {
    if (!isChanged())
        return;

    QString querystr = QString("SELECT * FROM %1 WHERE %2;")
        .arg(table).arg(whereClause());
    QSqlQuery query = db->exec(querystr);

    if (query.isActive() && query.numRowsAffected() > 0) {
        // Row already exists
        querystr = QString("UPDATE %1 SET %2 WHERE %3;")
            .arg(table).arg(setClause()).arg(whereClause());
        query = db->exec(querystr);
        if (!query.isActive())
            MythContext::DBError("simpledbstorage update", querystr);
    } else {
        // Row does not exist yet
        querystr = QString("INSERT INTO %1 SET %2;")
            .arg(table).arg(setClause());
        query = db->exec(querystr);
        if (!query.isActive())
            MythContext::DBError("simpledbstorage update", querystr);
    }
}

void AutoIncrementStorage::save(QSqlDatabase* db) {
    if (intValue() == 0) {
        // Generate a new, unique ID
        QString query = QString("INSERT INTO %1 (%2) VALUES (0);").arg(table).arg(column);
        QSqlQuery result = db->exec(query);
        if (!result.isActive() || result.numRowsAffected() < 1) {
            MythContext::DBError("inserting row", result);
            return;
        }
        result = db->exec("SELECT LAST_INSERT_ID();");
        if (!result.isActive() || result.numRowsAffected() < 1) {
            MythContext::DBError("selecting last insert id", result);
            return;
        }

        result.next();
        setValue(result.value(0).toInt());
    }
}

QWidget* ListBoxSetting::configWidget(ConfigurationGroup *cg, QWidget* parent, 
                                      const char* widgetName) {
    QWidget* box = new QVBox(parent, widgetName);
    box->setBackgroundOrigin(QWidget::WindowOrigin);

    if (getLabel() != "")
    {
        QLabel* label = new QLabel(box);
        label->setText(getLabel());
        label->setBackgroundOrigin(QWidget::WindowOrigin);
    }

    widget = new MythListBox(box);
    widget->setBackgroundOrigin(QWidget::WindowOrigin);
    widget->setHelpText(getHelpText());

    for(unsigned int i = 0 ; i < labels.size() ; ++i) {
        widget->insertItem(labels[i]);
        if (isSet && current == i)
            widget->setCurrentItem(i);
    }

    connect(widget, SIGNAL(destroyed()),
            this, SLOT(widgetDestroyed()));
    connect(this, SIGNAL(selectionsCleared()),
            widget, SLOT(clear()));
    connect(widget, SIGNAL(accepted(int)),
            this, SIGNAL(accepted(int)));
    connect(widget, SIGNAL(menuButtonPressed(int)),
            this, SIGNAL(menuButtonPressed(int)));
    connect(this, SIGNAL(valueChanged(const QString&)),
            widget, SLOT(setCurrentItem(const QString&)));
    connect(widget, SIGNAL(highlighted(const QString&)),
            this, SLOT(setValueByLabel(const QString&)));

    if (cg)
        connect(widget, SIGNAL(changeHelpText(QString)), cg,
                SIGNAL(changeHelpText(QString)));

    widget->setFocus();

    return box;
}

void ListBoxSetting::setValueByLabel(const QString& label) {
    for(unsigned i = 0 ; i < labels.size() ; ++i)
        if (labels[i] == label) {
            setValue(values[i]);
            return;
        }
    cerr << "BUG: ListBoxSetting::setValueByLabel called for unknown label " << label << endl;
}

void ImageSelectSetting::addImageSelection(const QString& label,
                                           QImage* image,
                                           QString value,
                                           bool select) {
    images.push_back(image);
    addSelection(label, value, select);
}

ImageSelectSetting::~ImageSelectSetting() {
    while (images.size() > 0) {
        delete images.back();
        images.pop_back();
    }
}

void ImageSelectSetting::imageSet(int num)
{
    if (num >= (int)images.size())
        return;

    if (!images[current])
        return;

    QImage temp = *(images[current]);
    temp = temp.smoothScale((int)(184 * m_hmult), (int)(138 * m_hmult),
                            QImage::ScaleMin);

    QPixmap tmppix(temp);
    imagelabel->setPixmap(tmppix);
}

QWidget* ImageSelectSetting::configWidget(ConfigurationGroup *cg, 
                                          QWidget* parent,
                                          const char* widgetName) 
{
    int width = 0, height = 0;

    gContext->GetScreenSettings(width, m_wmult, height, m_hmult);

    QHBox* box;
    if (labelAboveWidget) 
    {
        box = dynamic_cast<QHBox*>(new QVBox(parent, widgetName));
        box->setSizePolicy(QSizePolicy(QSizePolicy::Minimum, 
                                       QSizePolicy::Maximum));
    } 
    else
        box = new QHBox(parent, widgetName);

    box->setBackgroundOrigin(QWidget::WindowOrigin);

    if (getLabel() != "")
    {
        QLabel* label = new QLabel(box);
        label->setText(getLabel() + ":");
        label->setBackgroundOrigin(QWidget::WindowOrigin);
    }

    MythComboBox *widget = new MythComboBox(false, box);
    widget->setBackgroundOrigin(QWidget::WindowOrigin);

    QLabel *testlabel = new QLabel(box);
    testlabel->setText("  ");
    testlabel->setBackgroundOrigin(QWidget::WindowOrigin);

    imagelabel = new QLabel(box);
    imagelabel->setBackgroundOrigin(QWidget::WindowOrigin);

    for (unsigned int i = 0 ; i < images.size() ; ++i)
        widget->insertItem(labels[i]);

    if (isSet)
        widget->setCurrentItem(current);
    else
        current = 0;
   
    if (images.size() != 0 && images[current])
    { 
        QImage temp = *(images[current]);
        temp = temp.smoothScale((int)(184 * m_hmult), (int)(138 * m_hmult), 
                                QImage::ScaleMin);
 
        QPixmap tmppix(temp);
        imagelabel->setPixmap(tmppix);
    }
    else
    {
        QPixmap tmppix((int)(184 * m_hmult), (int)(138 * m_hmult));
        tmppix.fill(black);

        imagelabel->setPixmap(tmppix);
    }

    connect(widget, SIGNAL(highlighted(int)), this, SLOT(setValue(int)));
    connect(widget, SIGNAL(highlighted(int)), this, SLOT(imageSet(int)));

    connect(this, SIGNAL(selectionsCleared()),
            widget, SLOT(clear()));

    if (cg)
        connect(widget, SIGNAL(changeHelpText(QString)), cg, 
                SIGNAL(changeHelpText(QString)));

    return box;
}

HostnameSetting::HostnameSetting(void)  {
    setVisible(false);
    
    setValue(gContext->GetHostName());
}

void ChannelSetting::fillSelections(QSqlDatabase* db, SelectSetting* setting) {

    // this should go somewhere else, in something that knows about
    // channels and how they're stored in the database.  We're just a
    // selector.

    QSqlQuery result = db->exec("SELECT name, chanid FROM channel;");
    if (result.isActive() && result.numRowsAffected() > 0)
        while (result.next())
            setting->addSelection(result.value(0).toString(),
                                  QString::number(result.value(1).toInt()));
}

QWidget* ButtonSetting::configWidget(ConfigurationGroup* cg, QWidget* parent,
                                     const char* widgetName)
{
    (void) cg;
    MythPushButton* widget = new MythPushButton(parent, widgetName);

    widget->setText(getLabel());
    //widget->setHelpText(getHelpText());

    connect(widget, SIGNAL(pressed()), this, SIGNAL(pressed()));

    if (cg)
        connect(widget, SIGNAL(changeHelpText(QString)),
                cg, SIGNAL(changeHelpText(QString)));

    return widget;
}

QWidget* ProgressSetting::configWidget(ConfigurationGroup* cg, QWidget* parent,
                                       const char* widgetName) {
    (void)cg;

    QHBox* widget = new QHBox(parent,widgetName);
    widget->setBackgroundOrigin(QWidget::WindowOrigin);

    if (getLabel() != "")
    {
        QLabel* label = new QLabel(getLabel() + "     :", widget, widgetName);
        label->setBackgroundOrigin(QWidget::WindowOrigin);
    }

    QProgressBar* progress = new QProgressBar(totalSteps, widget, widgetName);
    progress->setBackgroundOrigin(QWidget::WindowOrigin);

    connect(this, SIGNAL(valueChanged(int)), progress, SLOT(setProgress(int)));
    progress->setProgress(intValue());
    return widget;
}

void ConfigPopupDialogWidget::keyPressEvent(QKeyEvent* e) {
    switch(e->key()) {
    case Key_Escape:
        reject();
        break;
    default:
        MythDialog::keyPressEvent(e);
    }
}

MythDialog* ConfigurationPopupDialog::dialogWidget(MythMainWindow* parent,
                                                   const char* widgetName)
{
    dialog = new ConfigPopupDialogWidget(parent, widgetName);
    dialog->setBackgroundOrigin(QWidget::WindowOrigin);

    if (getLabel() != "") {
        QHBox* box = new QHBox(dialog);
        box->setBackgroundOrigin(QWidget::WindowOrigin);
        box->setSizePolicy(QSizePolicy(QSizePolicy::Minimum, 
                                       QSizePolicy::Maximum));

        QLabel* label = new QLabel(box);
        label->setText(getLabel());
        label->setBackgroundOrigin(QWidget::WindowOrigin);
        label->setAlignment(Qt::AlignHCenter);
        label->setSizePolicy(QSizePolicy(QSizePolicy::Minimum, 
                                         QSizePolicy::Maximum));

        dialog->addWidget(box);
    }

    QWidget* widget = configWidget(NULL, dialog);
    dialog->addWidget(widget);
    widget->setFocus();

    return dialog;
}

int ConfigurationPopupDialog::exec(QSqlDatabase* db, bool saveOnAccept)
{
    load(db);

    dialog = (ConfigPopupDialogWidget*)dialogWidget(gContext->GetMainWindow());
    dialog->ShowPopup(this);

    int ret;

    if ((ret = dialog->exec()) == QDialog::Accepted && saveOnAccept)
        save(db);

    return ret;
}

