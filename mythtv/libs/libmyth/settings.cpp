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
#include <qwizard.h>
#include <qbuttongroup.h>
#include <qradiobutton.h>
#include <qcheckbox.h>
#include <qwidgetstack.h>
#include <qdialog.h>
#include <qtabdialog.h>

QWidget* VerticalConfigurationGroup::configWidget(QWidget* parent,
                                                  const char* widgetName) {
    
    //QVGroupBox* widget = new QVGroupBox(parent, widgetName);
    QGroupBox* widget = new QGroupBox(parent, widgetName);
    QVBoxLayout* layout = new QVBoxLayout(widget, 20);
    widget->setTitle(getLabel());
    for(unsigned i = 0 ; i < children.size() ; ++i)
        if (children[i]->isVisible())
            layout->add(children[i]->configWidget(widget, NULL));
        
    return widget;
}

QWidget* StackedConfigurationGroup::configWidget(QWidget* parent,
                                                 const char* widgetName) {
    QWidgetStack* widget = new QWidgetStack(parent, widgetName);

    for(unsigned i = 0 ; i < children.size() ; ++i)
        if (children[i]->isVisible())
            widget->addWidget(children[i]->configWidget(widget, NULL), i);

    widget->raiseWidget(top);

    connect(this, SIGNAL(raiseWidget(int)),
            widget, SLOT(raiseWidget(int)));

    return widget;
}

QWidget* TabbedConfigurationGroup::configWidget(QWidget* parent,
                                                const char* widgetName) {
    QTabDialog* widget = new QTabDialog(parent, widgetName);
    
    for(unsigned i = 0 ; i < children.size() ; ++i)
        if (children[i]->isVisible())
            widget->addTab(children[i]->configWidget(widget), children[i]->getLabel());

    return widget;
};

void StackedConfigurationGroup::raise(Configurable* child) {
    for(unsigned i = 0 ; i < children.size() ; ++i)
        if (children[i] == child) {
            top = i;
            emit raiseWidget((int)i);
            return;
        }
    cerr << "BUG: StackedConfigurationGroup::raise(): unrecognized child " << child << endl;
}

QWidget* LineEditSetting::configWidget(QWidget* parent,
                                       const char *widgetName) {
    QWidget* widget = new QHBox(parent, widgetName);

    QLabel* label = new QLabel(widget, QString(widgetName) + "-label");
    label->setText(getLabel() + ":");

    QLineEdit* edit = new MythLineEdit(settingValue, widget,
                                    QString(widgetName) + "-edit");
    edit->setText( getValue() );

    connect(this, SIGNAL(valueChanged(const QString&)),
            edit, SLOT(setText(const QString&)));
    connect(edit, SIGNAL(textChanged(const QString&)),
            this, SLOT(setValue(const QString&)));

    return widget;
}

QWidget* SliderSetting::configWidget(QWidget* parent,
                                     const char* widgetName) {
    QWidget* widget = new QHBox(parent, widgetName);

    QLabel* label = new QLabel(widget, QString(widgetName) + "-label");
    label->setText(getLabel() + ":");

    QSlider* slider = new MythSlider(widget, QString(widgetName) + "-slider");
    slider->setMinValue(min);
    slider->setMaxValue(max);
    slider->setOrientation(QSlider::Horizontal);
    slider->setLineStep(step);
    slider->setValue(intValue());

    QLCDNumber* lcd = new QLCDNumber(widget, QString(widgetName) + "-lcd");
    lcd->setMode(QLCDNumber::DEC);
    lcd->setSegmentStyle(QLCDNumber::Flat);
    lcd->display(intValue());

    connect(slider, SIGNAL(valueChanged(int)), lcd, SLOT(display(int)));
    connect(slider, SIGNAL(valueChanged(int)), this, SLOT(setValue(int)));
    connect(this, SIGNAL(valueChanged(int)), slider, SLOT(setValue(int)));

    return widget;
}

QWidget* SpinBoxSetting::configWidget(QWidget* parent,
                                      const char* widgetName) {

    QWidget* box = new QHBox(parent, widgetName);

    QLabel* label = new QLabel(box);
    label->setText(getLabel() + ":");

    QSpinBox* spinbox = new MythSpinBox(box);
    spinbox->setMinValue(min);
    spinbox->setMaxValue(max);
    spinbox->setLineStep(step);
    spinbox->setValue(intValue());

    connect(spinbox, SIGNAL(valueChanged(int)), this, SLOT(setValue(int)));
    connect(this, SIGNAL(valueChanged(int)), spinbox, SLOT(setValue(int)));

    return box;
}

QWidget* ComboBoxSetting::configWidget(QWidget* parent,
                                       const char* widgetName) {
    QWidget* box = new QHBox(parent, widgetName);

    QLabel* label = new QLabel(box);
    label->setText(getLabel() + ":");
    QComboBox* widget = new MythComboBox(rw, box);

    for(unsigned int i = 0 ; i < labels.size() ; ++i) {
        widget->insertItem(labels[i]);
    }
    if (isSet)
        widget->setCurrentItem(current);

    connect(widget, SIGNAL(highlighted(int)),
            this, SLOT(setValue(int)));

    return box;
}

QWidget* RadioSetting::configWidget(QWidget* parent,
                                    const char* widgetName) {
    QButtonGroup* widget = new QButtonGroup(parent, widgetName);
    widget->setTitle(getLabel());

    for( unsigned i = 0 ; i < labels.size() ; ++i ) {
        QRadioButton* button = new QRadioButton(widget, NULL);
        button->setText(labels[i]);
        if (isSet && i == current)
            button->setDown(true);
    }

    return widget;
}

QWidget* CheckBoxSetting::configWidget(QWidget* parent,
                                       const char* widgetName) {
    
    QCheckBox* widget = new QCheckBox(parent, widgetName);
    widget->setText(getLabel());
    widget->setChecked(boolValue());

    connect(widget, SIGNAL(toggled(bool)),
            this, SLOT(setValue(bool)));
    connect(this, SIGNAL(valueChanged(bool)),
            widget, SLOT(setChecked(bool)));

    return widget;
}

QDialog* ConfigurationDialog::dialogWidget(QWidget* parent,
                                           const char* widgetName) {
    QDialog* dialog = new QDialog(parent, widgetName, TRUE);
    QVBoxLayout* layout = new QVBoxLayout(dialog);
    layout->addWidget(configWidget(dialog));

    return dialog;
}

void ConfigurationDialog::exec(QSqlDatabase* db) {
    load(db);

    QDialog* dialog = dialogWidget(NULL);
    dialog->show();

    if (dialog->exec() == QDialog::Accepted)
        save(db);

    delete dialog;
}

QDialog* ConfigurationWizard::dialogWidget(QWidget* parent,
                                           const char* widgetName) {
    QWizard* wizard = new QWizard(parent, widgetName, TRUE, 0);

    wizard->resize(600, 480); // xxx

    unsigned i;
    for(i = 0 ; i < children.size() ; ++i)
        if (children[i]->isVisible()) {
            QWidget* child = children[i]->configWidget(parent);
            wizard->addPage(child, children[i]->getLabel());
            if (i == children.size()-1)
                // Last page always has finish enabled for now
                // stuff should have sane defaults anyway
                wizard->setFinishEnabled(child, true);
        }
        
    return wizard;
}

QWidget* ConfigurationWizard::configWidget(QWidget* parent,
                                           const char* widgetName) {
    return dialogWidget(parent, widgetName);
}

void SimpleDBStorage::load(QSqlDatabase* db) {
    QString querystr = QString("SELECT %1 FROM %2 WHERE %3")
        .arg(column).arg(table).arg(whereClause());
    QSqlQuery query = db->exec(querystr);

    if (query.isActive() && query.numRowsAffected() > 0) {
        query.next();
        QVariant result = query.value(0);
        setValue(query.value(0).toString());
    }
}

void SimpleDBStorage::save(QSqlDatabase* db) {
    QString querystr = QString("SELECT * FROM %1 WHERE %2")
        .arg(table).arg(whereClause());
    QSqlQuery query = db->exec(querystr);

    if (query.isActive() && query.numRowsAffected() > 0) {
        // Row already exists
        querystr = QString("UPDATE %1 SET %2 WHERE %4")
            .arg(table).arg(setClause()).arg(whereClause());
        query = db->exec(querystr);
    } else {
        // Row does not exist yet
        querystr = QString("INSERT INTO %1 SET %2")
            .arg(table).arg(setClause());
        query = db->exec(querystr);
    }
}

void AutoIncrementStorage::save(QSqlDatabase* db) {
    if (intValue() == 0) {
        // Generate a new, unique ID
        QString query = QString("INSERT INTO %1 (%2) VALUES (0)").arg(table).arg(column);
        QSqlQuery result = db->exec(query);
        if (!result.isActive() || result.numRowsAffected() < 1) {
            cerr << "Failed to insert new entry for ("
                 << table << "," << column << ")\n";
            return;
        }
        result = db->exec("SELECT LAST_INSERT_ID()");
        if (!result.isActive() || result.numRowsAffected() < 1) {
            cerr << "Failed to fetch last insert id" << endl;
            return;
        }

        result.next();
        setValue(result.value(0).toInt());
    }
}
