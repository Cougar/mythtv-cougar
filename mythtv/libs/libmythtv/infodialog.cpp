#include <qlayout.h>
#include <qpushbutton.h>
#include <qlabel.h>
#include <qdatetime.h>
#include <qcursor.h>
#include <qcheckbox.h>
#include <qvgroupbox.h>
#include <qapplication.h>

#include "infodialog.h"
#include "infostructs.h"
#include "programinfo.h"
#include "settings.h"
#include "mythcontext.h"

using namespace libmyth;

InfoDialog::InfoDialog(MythContext *context, ProgramInfo *pginfo, 
                       QWidget *parent, const char *name)
          : QDialog(parent, name)
{
    m_context = context;

    int screenheight = 0, screenwidth = 0;
    float wmult = 0, hmult = 0;

    m_context->GetScreenSettings(screenwidth, wmult, screenheight, hmult);

    int bigfont = m_context->GetBigFontSize();
    int mediumfont = m_context->GetMediumFontSize();

    setGeometry(0, 0, screenwidth, screenheight);
    setFixedSize(QSize(screenwidth, screenheight));

    setFont(QFont("Arial", (int)(mediumfont * hmult), QFont::Bold));
    setCursor(QCursor(Qt::BlankCursor));

    QVBoxLayout *vbox = new QVBoxLayout(this, (int)(20 * wmult));

    QGridLayout *grid = new QGridLayout(vbox, 4, 2, (int)(10 * wmult));
    
    QLabel *titlefield = new QLabel(pginfo->title, this);
    titlefield->setFont(QFont("Arial", (int)(bigfont * hmult), QFont::Bold));

    QLabel *date = getDateLabel(pginfo);

    QLabel *subtitlelabel = new QLabel("Episode:", this);
    QLabel *subtitlefield = new QLabel(pginfo->subtitle, this);
    subtitlefield->setAlignment(Qt::WordBreak | Qt::AlignLeft | Qt::AlignTop);
    QLabel *descriptionlabel = new QLabel("Description:", this);
    QLabel *descriptionfield = new QLabel(pginfo->description, this);
    descriptionfield->setAlignment(Qt::WordBreak | Qt::AlignLeft | 
                                   Qt::AlignTop);

    descriptionfield->setMinimumWidth((int)(800 * wmult) - 
                                      descriptionlabel->width() - 100);
    grid->addMultiCellWidget(titlefield, 0, 0, 0, 1, Qt::AlignLeft);
    grid->addMultiCellWidget(date, 1, 1, 0, 1, Qt::AlignLeft);
    grid->addWidget(subtitlelabel, 2, 0, Qt::AlignLeft | Qt::AlignTop);
    grid->addWidget(subtitlefield, 2, 1, Qt::AlignLeft | Qt::AlignTop);
    grid->addWidget(descriptionlabel, 3, 0, Qt::AlignLeft | Qt::AlignTop);
    grid->addWidget(descriptionfield, 3, 1, Qt::AlignLeft | Qt::AlignTop);

    grid->setColStretch(1, 2);
    grid->setRowStretch(3, 1);

    //vbox->addStretch(1);

    QFrame *f = new QFrame(this);
    f->setFrameStyle(QFrame::HLine | QFrame::Plain);
    f->setLineWidth((int)(4 * hmult));
    vbox->addWidget(f);    

    QVBoxLayout *middleBox = new QVBoxLayout(vbox);

    norec = new QCheckBox("Don't record this program", this);
    middleBox->addWidget(norec);
    connect(norec, SIGNAL(clicked()), this, SLOT(norecPressed()));

    recone = new QCheckBox("Record only this showing of the program", this);
    middleBox->addWidget(recone);
    connect(recone, SIGNAL(clicked()), this, SLOT(reconePressed()));

    programtype = pginfo->IsProgramRecurring();

    QString msg = "Shouldn't show up.";
    if (programtype == 1)
        msg = "Record this program in this timeslot every day";
    else if (programtype == 2)
        msg = "Record this program in this timeslot every week";
    rectimeslot = new QCheckBox(msg, this);
    if (programtype != 0)
    {
        middleBox->addWidget(rectimeslot);
        connect(rectimeslot, SIGNAL(clicked()), this, 
                SLOT(rectimeslotPressed()));
    }
    else
        rectimeslot->hide();

    recchannel = new QCheckBox("Record this program whenever it's shown on this channel", this);
    middleBox->addWidget(recchannel);
    connect(recchannel, SIGNAL(clicked()), this, SLOT(recchannelPressed()));

    recevery = new QCheckBox("Record this program whenever it's shown anywhere", this);
    middleBox->addWidget(recevery);
    connect(recevery, SIGNAL(clicked()), this, SLOT(receveryPressed()));

    vbox->activate();

    recordstatus = pginfo->GetProgramRecordingStatus();

    if (recordstatus == kTimeslotRecord && programtype == 0)
    {
        printf("error, somehow set to record timeslot and it doesn't seem to have one\n");
        recordstatus = kSingleRecord;
    }

    if (recordstatus == kSingleRecord)
        recone->setChecked(true);
    else if (recordstatus == kTimeslotRecord)
        rectimeslot->setChecked(true);
    else if (recordstatus == kChannelRecord)
        recchannel->setChecked(true);
    else if (recordstatus == kAllRecord)
        recevery->setChecked(true);
    else
        norec->setChecked(true);
    
    myinfo = pginfo;
     
    showFullScreen();
    setActiveWindow();
}

QLabel *InfoDialog::getDateLabel(ProgramInfo *pginfo)
{
    QDateTime startts = pginfo->startts;
    QDateTime endts = pginfo->endts;

    QString dateformat = m_context->settings()->GetSetting("DateFormat");
    if (dateformat == "")
        dateformat = "ddd MMMM d";
    QString timeformat = m_context->settings()->GetSetting("TimeFormat");
    if (timeformat == "")
        timeformat = "h:mm AP";

    QString timedate = endts.date().toString(dateformat) + QString(", ") +
                       startts.time().toString(timeformat) + QString(" - ") +
                       endts.time().toString(timeformat);

    QLabel *date = new QLabel(timedate, this);

    return date;
}

void InfoDialog::norecPressed(void)
{
    if (!norec->isChecked())
        norec->setChecked(true);

    if (recone->isChecked())
        recone->setChecked(false);
    if (rectimeslot->isChecked())
        rectimeslot->setChecked(false);
    if (recevery->isChecked())
        recevery->setChecked(false);
    if (recchannel->isChecked())
        recchannel->setChecked(false);
}

void InfoDialog::reconePressed(void)
{
    if (!recone->isChecked())
    {
        norec->setChecked(true);
    }
    else
    {
        if (norec->isChecked())
            norec->setChecked(false);
        if (rectimeslot->isChecked())
            rectimeslot->setChecked(false);
        if (recchannel->isChecked())
            recchannel->setChecked(false);
        if (recevery->isChecked())
            recevery->setChecked(false);
    }
}

void InfoDialog::rectimeslotPressed(void)
{
    if (!rectimeslot->isChecked())
    {
        norec->setChecked(true);
    }
    else
    {
        if (norec->isChecked())
            norec->setChecked(false);
        if (recone->isChecked())
            recone->setChecked(false);
        if (recchannel->isChecked())
            recchannel->setChecked(false);
        if (recevery->isChecked())
            recevery->setChecked(false);
    }
}

void InfoDialog::recchannelPressed(void)
{   
    if (!recchannel->isChecked())
    {
        norec->setChecked(true);
    }
    else 
    {
        if (norec->isChecked())
            norec->setChecked(false);
        if (recone->isChecked())
            recone->setChecked(false);
        if (rectimeslot->isChecked())
            rectimeslot->setChecked(false);
        if (recevery->isChecked())
            recevery->setChecked(false);
    }
}

void InfoDialog::receveryPressed(void)
{
    if (!recevery->isChecked())
    {
        norec->setChecked(true);
    }
    else 
    {
        if (norec->isChecked())
            norec->setChecked(false);
        if (recone->isChecked())
            recone->setChecked(false);
        if (rectimeslot->isChecked())
            rectimeslot->setChecked(false);
        if (recchannel->isChecked())
            recchannel->setChecked(false);
    }
}

void InfoDialog::hideEvent(QHideEvent *e)
{
    okPressed();
    QDialog::hideEvent(e);
}

void InfoDialog::okPressed(void)
{
    RecordingType currentSelected = kNotRecording;
    if (recone->isChecked())
        currentSelected = kSingleRecord;
    else if (rectimeslot->isChecked())
        currentSelected = kTimeslotRecord;
    else if (recchannel->isChecked())
        currentSelected = kChannelRecord;
    else if (recevery->isChecked())
        currentSelected = kAllRecord;

    if (currentSelected != recordstatus)
        myinfo->ApplyRecordStateChange(currentSelected);   
}
