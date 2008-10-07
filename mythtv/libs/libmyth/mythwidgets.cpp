#include <iostream>
using namespace std;

#include <QStyle>
#include <QPainter>
#include <QCursor>
#include <QApplication>
#include <q3vbox.h>
#include <QLayout>
#include <QKeyEvent>
#include <QHideEvent>
#include <QFocusEvent>
#include <QMouseEvent>
#include <QEvent>

#include "mythwidgets.h"
#include "mythcontext.h"
#include "util.h"
#include "mythdialogs.h"
#include "virtualkeyboard.h"
#include "libmythdb/mythverbose.h"
#include "libmythui/mythmainwindow.h"

typedef VirtualKeyboard* QWidgetP;
static void qt_delete(QWidgetP &widget)
{
    if (widget)
    {
        widget->disconnect();
        widget->hide();
        widget->deleteLater();
        widget = NULL;
    }
}

MythComboBox::MythComboBox(bool rw, QWidget *parent, const char *name) :
    QComboBox(rw, parent, name),
    popup(NULL), helptext(QString::null), AcceptOnSelect(false),
    useVirtualKeyboard(true), allowVirtualKeyboard(rw),
    popupPosition(VK_POSBELOWEDIT), step(1)
{
    useVirtualKeyboard = gContext->GetNumSetting("UseVirtualKeyboard", 1);
}

MythComboBox::~MythComboBox()
{
    Teardown();
}

void MythComboBox::deleteLater(void)
{
    Teardown();
    QComboBox::deleteLater();
}

void MythComboBox::Teardown(void)
{
    qt_delete(popup);
}

void MythComboBox::setHelpText(const QString &help)
{
    bool changed = helptext != help;
    helptext = help;
    if (hasFocus() && changed)
        emit changeHelpText(help);
}

void MythComboBox::popupVirtualKeyboard(void)
{
    qt_delete(popup);

    popup = new VirtualKeyboard(gContext->GetMainWindow(), this);
    gContext->GetMainWindow()->detach(popup);
    popup->exec();

    qt_delete(popup);
}


void MythComboBox::keyPressEvent(QKeyEvent *e)
{
    bool handled = false, updated = false;
    QStringList actions;
    if ((!popup || !popup->isShown()) &&
        (gContext->TranslateKeyPress("qt", e, actions, !allowVirtualKeyboard)))
    {
        for (int i = 0; i < actions.size() && !handled; i++)
        {
            QString action = actions[i];
            handled = true;

            if (action == "UP")
            {
                focusNextPrevChild(false);
            }
            else if (action == "DOWN")
            {
                focusNextPrevChild(true);
            }
            else if (action == "LEFT")
            {
                if (currentIndex() == 0)
                    setCurrentIndex(count()-1);
                else if (count() > 0)
                    setCurrentIndex((currentIndex() - 1) % count());
                updated = true;
            }
            else if (action == "RIGHT")
            {
                if (count() > 0)
                    setCurrentIndex((currentIndex() + 1) % count());
                updated = true;
            }
            else if (action == "PAGEDOWN")
            {
                if (currentIndex() == 0)
                    setCurrentIndex(count() - (step % count()));
                else if (count() > 0)
                    setCurrentIndex(
                        (currentIndex() + count() - (step % count())) % count());
                updated = true;
            }
            else if (action == "PAGEUP")
            {
                if (count() > 0)
                    setCurrentIndex(
                        (currentIndex() + (step % count())) % count());
                updated = true;
            }
            else if (action == "SELECT" && AcceptOnSelect)
                emit accepted(currentIndex());
            else if (action == "SELECT" &&
                    (e->text().isEmpty() ||
                    (e->key() == Qt::Key_Enter) ||
                    (e->key() == Qt::Key_Return) ||
                    (e->key() == Qt::Key_Space)))
            {
                if (useVirtualKeyboard && allowVirtualKeyboard)
                    popupVirtualKeyboard();
                else
                   handled = true;
            }

            else
                handled = false;
        }
    }

    if (updated)
    {
        emit activated(currentIndex());
        emit activated(itemText(currentIndex()));
    }
    if (!handled)
    {
        if (editable())
            QComboBox::keyPressEvent(e);
        else
            e->ignore();
    }
}

void MythComboBox::focusInEvent(QFocusEvent *e)
{
    emit changeHelpText(helptext);
    emit gotFocus();

    QColor highlight = colorGroup().highlight();

    this->setPaletteBackgroundColor(highlight);

    if (lineEdit())
        lineEdit()->setPaletteBackgroundColor(highlight);

    QComboBox::focusInEvent(e);
}

void MythComboBox::focusOutEvent(QFocusEvent *e)
{
    this->unsetPalette();

    if (lineEdit())
    {
        lineEdit()->unsetPalette();

        // commit change if the user was editting an entry
        QString curText = currentText();
        int i;
        bool foundItem = false;

        for(i = 0; i < count(); i++)
            if (curText == text(i))
                foundItem = true;

        if (!foundItem)
        {
            insertItem(curText);
            setCurrentIndex(count()-1);
        }
    }

    QComboBox::focusOutEvent(e);
}

void MythCheckBox::keyPressEvent(QKeyEvent* e)
{
    bool handled = false;
    QStringList actions;
    if (gContext->TranslateKeyPress("qt", e, actions))
    {
        for ( int i = 0; i < actions.size() && !handled; i++)
        {
            QString action = actions[i];
            handled = true;

            if (action == "UP")
                focusNextPrevChild(false);
            else if (action == "DOWN")
                focusNextPrevChild(true);
            else if (action == "LEFT" || action == "RIGHT" || action == "SELECT")
                toggle();
            else
                handled = false;
        }
    }

    if (!handled)
        e->ignore();
}

void MythCheckBox::setHelpText(const QString &help)
{
    bool changed = helptext != help;
    helptext = help;
    if (hasFocus() && changed)
        emit changeHelpText(help);
}

void MythCheckBox::focusInEvent(QFocusEvent *e)
{
    emit changeHelpText(helptext);

    QColor highlight = colorGroup().highlight();

    this->setPaletteBackgroundColor(highlight);
    QCheckBox::focusInEvent(e);
}

void MythCheckBox::focusOutEvent(QFocusEvent *e)
{
    this->unsetPalette();
    QCheckBox::focusOutEvent(e);
}

void MythRadioButton::keyPressEvent(QKeyEvent* e)
{
    bool handled = false;
    QStringList actions;
    if (gContext->TranslateKeyPress("qt", e, actions))
    {
        for (int i = 0; i < actions.size() && !handled; i++)
        {
            QString action = actions[i];
            handled = true;

            if (action == "UP")
                focusNextPrevChild(false);
            else if (action == "DOWN")
                focusNextPrevChild(true);
            else if (action == "LEFT" || action == "RIGHT")
                toggle();
            else
                handled = false;
        }
    }

    if (!handled)
        e->ignore();
}

void MythRadioButton::setHelpText(const QString &help)
{
    bool changed = helptext != help;
    helptext = help;
    if (hasFocus() && changed)
        emit changeHelpText(help);
}

void MythRadioButton::focusInEvent(QFocusEvent *e)
{
    emit changeHelpText(helptext);

    QColor highlight = colorGroup().highlight();

    this->setPaletteBackgroundColor(highlight);
    QRadioButton::focusInEvent(e);
}

void MythRadioButton::focusOutEvent(QFocusEvent *e)
{
    this->unsetPalette();
    QRadioButton::focusOutEvent(e);
}


void MythSpinBox::setHelpText(const QString &help)
{
    bool changed = helptext != help;
    helptext = help;
    if (hasFocus() && changed)
        emit changeHelpText(help);
}

void MythSpinBox::keyPressEvent(QKeyEvent* e)
{
    bool handled = false;
    QStringList actions;
    if (gContext->TranslateKeyPress("qt", e, actions))
    {
        for (int i = 0; i < actions.size() && !handled; i++)
        {
            QString action = actions[i];
            handled = true;

            if (action == "UP")
                focusNextPrevChild(false);
            else if (action == "DOWN")
                focusNextPrevChild(true);
            else if (action == "LEFT")
                singlestep ? setValue(value()-1) : stepDown();
            else if (action == "RIGHT")
                singlestep ? setValue(value()+1) : stepUp();
            else if (action == "PAGEDOWN")
                stepDown();
            else if (action == "PAGEUP")
                stepUp();
            else if (action == "SELECT")
                handled = true;
            else
                handled = false;
        }
    }

    if (!handled)
        QSpinBox::keyPressEvent(e);
}

void MythSpinBox::focusInEvent(QFocusEvent *e)
{
    emit changeHelpText(helptext);

    QColor highlight = colorGroup().highlight();

    this->setPaletteBackgroundColor(highlight);
    QSpinBox::focusInEvent(e);
}

void MythSpinBox::focusOutEvent(QFocusEvent *e)
{
    this->unsetPalette();
    QSpinBox::focusOutEvent(e);
}

void MythSlider::keyPressEvent(QKeyEvent* e)
{
    bool handled = false;
    QStringList actions;
    if (gContext->TranslateKeyPress("qt", e, actions))
    {
        for (int i = 0; i < actions.size() && !handled; i++)
        {
            QString action = actions[i];
            handled = true;

            if (action == "UP")
                focusNextPrevChild(false);
            else if (action == "DOWN")
                focusNextPrevChild(true);
            else if (action == "LEFT")
                setValue(value() - lineStep());
            else if (action == "RIGHT")
                setValue(value() + lineStep());
            else if (action == "SELECT")
                handled = true;
            else
                handled = false;
        }
    }

    if (!handled)
        QSlider::keyPressEvent(e);
}

void MythSlider::setHelpText(const QString &help)
{
    bool changed = helptext != help;
    helptext = help;
    if (hasFocus() && changed)
        emit changeHelpText(help);
}

void MythSlider::focusInEvent(QFocusEvent *e)
{
    emit changeHelpText(helptext);

    QColor highlight = colorGroup().highlight();

    this->setPaletteBackgroundColor(highlight);
    QSlider::focusInEvent(e);
}

void MythSlider::focusOutEvent(QFocusEvent *e)
{
    this->unsetPalette();
    QSlider::focusOutEvent(e);
}

MythLineEdit::MythLineEdit(QWidget *parent, const char* widgetName) :
    QLineEdit(parent, widgetName),
    popup(NULL), helptext(QString::null), rw(true),
    useVirtualKeyboard(true),
    allowVirtualKeyboard(true),
    popupPosition(VK_POSBELOWEDIT)
{
    useVirtualKeyboard = gContext->GetNumSetting("UseVirtualKeyboard", 1);
}

MythLineEdit::MythLineEdit(
    const QString &contents, QWidget *parent, const char* widgetName) :
    QLineEdit(contents, parent, widgetName),
    popup(NULL), helptext(QString::null), rw(true),
    useVirtualKeyboard(true),
    allowVirtualKeyboard(true),
    popupPosition(VK_POSBELOWEDIT)
{
    useVirtualKeyboard = gContext->GetNumSetting("UseVirtualKeyboard", 1);
}

MythLineEdit::~MythLineEdit()
{
    Teardown();
}

void MythLineEdit::deleteLater(void)
{
    Teardown();
    QLineEdit::deleteLater();
}

void MythLineEdit::Teardown(void)
{
    qt_delete(popup);
}

void MythLineEdit::popupVirtualKeyboard(void)
{
    qt_delete(popup);

    popup = new VirtualKeyboard(gContext->GetMainWindow(), this);
    gContext->GetMainWindow()->detach(popup);
    popup->exec();

    qt_delete(popup);
}

void MythLineEdit::keyPressEvent(QKeyEvent *e)
{
    bool handled = false;
    QStringList actions;
    if ((!popup || !popup->isShown()) &&
        gContext->TranslateKeyPress("qt", e, actions, false))
    {
        for (int i = 0; i < actions.size() && !handled; i++)
        {
            QString action = actions[i];
            handled = true;

            if (action == "UP")
                focusNextPrevChild(false);
            else if (action == "DOWN")
                focusNextPrevChild(true);
            else if (action == "SELECT" &&
                    (e->text().isEmpty() ||
                    (e->key() == Qt::Key_Enter) ||
                    (e->key() == Qt::Key_Return)))
            {
                if (useVirtualKeyboard && allowVirtualKeyboard && rw)
                    popupVirtualKeyboard();
                else
                    handled = false;
            }
            else if (action == "SELECT" && e->text().isEmpty() )
                e->ignore();
            else
                handled = false;
        }
    }

    if (!handled)
        if (rw || e->key() == Qt::Key_Escape || e->key() == Qt::Key_Left
               || e->key() == Qt::Key_Return || e->key() == Qt::Key_Right)
            QLineEdit::keyPressEvent(e);
}

void MythLineEdit::setText(const QString &text)
{
    // Don't mess with the cursor position; it causes
    // counter-intuitive behaviour due to interactions with the
    // communication with the settings stuff

    int pos = cursorPosition();
    QLineEdit::setText(text);
    setCursorPosition(pos);
}

QString MythLineEdit::text(void)
{
    return QLineEdit::text();
}

void MythLineEdit::setHelpText(const QString &help)
{
    bool changed = helptext != help;
    helptext = help;
    if (hasFocus() && changed)
        emit changeHelpText(help);
}

void MythLineEdit::focusInEvent(QFocusEvent *e)
{
    emit changeHelpText(helptext);

    QColor highlight = colorGroup().highlight();

    this->setPaletteBackgroundColor(highlight);
    QLineEdit::focusInEvent(e);
}

void MythLineEdit::focusOutEvent(QFocusEvent *e)
{
    this->unsetPalette();
    if (popup && popup->isShown() && !popup->hasFocus())
        popup->hide();
    QLineEdit::focusOutEvent(e);
}

void MythLineEdit::hideEvent(QHideEvent *e)
{
    if (popup && popup->isShown())
        popup->hide();
    QLineEdit::hideEvent(e);
}

void MythLineEdit::mouseDoubleClickEvent(QMouseEvent *e)
{
    QLineEdit::mouseDoubleClickEvent(e);
}

MythRemoteLineEdit::MythRemoteLineEdit(QWidget * parent, const char * name)
                  : Q3TextEdit(parent, name)
{
    my_font = NULL;
    m_lines = 1;
    this->Init();
}

MythRemoteLineEdit::MythRemoteLineEdit(const QString & contents,
                                       QWidget * parent, const char * name)
                  : Q3TextEdit(parent, name)
{
    my_font = NULL;
    m_lines = 1;
    this->Init();
    setText(contents);
}

MythRemoteLineEdit::MythRemoteLineEdit(QFont *a_font, QWidget * parent,
                                       const char * name)
                  : Q3TextEdit(parent, name)
{
    my_font = a_font;
    m_lines = 1;
    this->Init();
}

MythRemoteLineEdit::MythRemoteLineEdit(int lines, QWidget * parent,
                                       const char * name)
                  : Q3TextEdit(parent, name)
{
    my_font = NULL;
    m_lines = lines;
    this->Init();
}

void MythRemoteLineEdit::Init()
{
    //  Bunch of default values
    cycle_timer = new QTimer();
    shift = false;
    active_cycle = false;
    current_choice = "";
    current_set = "";

    //  We need to start out in PlainText format
    //  and only toggle RichText on if we are in
    //  the middle of a character cycle. That's
    //  the only way to do all this in a way which
    //  works across most 3.x.x versions of Qt.
    setTextFormat(Qt::PlainText);

    cycle_time = 3000;

    pre_cycle_text_upto = "";
    pre_cycle_text_from = "";
    pre_cycle_para = 0;
    pre_cycle_pos  = 0;

    col_unselected.setRgb(100, 100, 100);
    col_selected.setRgb(0, 255, 255);
    col_special.setRgb(255, 0, 0);

    assignHexColors();

    //  Try and make sure it doesn't ever change
    setWordWrap(Q3TextEdit::NoWrap);
    Q3ScrollView::setVScrollBarMode(Q3ScrollView::AlwaysOff);
    Q3ScrollView::setHScrollBarMode(Q3ScrollView::AlwaysOff);

    if (my_font)
        setFont(*my_font);

    QFontMetrics fontsize(font());

    setMinimumHeight(fontsize.height() * 5 / 4);
    setMaximumHeight(fontsize.height() * m_lines * 5 / 4);

    connect(cycle_timer, SIGNAL(timeout()), this, SLOT(endCycle()));

    popup = NULL;
    useVirtualKeyboard = gContext->GetNumSetting("UseVirtualKeyboard", 1);
    popupPosition = VK_POSBELOWEDIT;
}

void MythRemoteLineEdit::startCycle(QString current_choice, QString set)
{
    int end_paragraph;
    int end_position;
    int dummy;
    int dummy_two;

    if (active_cycle)
    {
        cerr << "libmyth: MythRemoteLineEdit was asked to start a cycle when a cycle was already active." << endl;
    }
    else
    {
        cycle_timer->start(cycle_time, true);
        active_cycle = true;
        //  Amazingly, Qt (version < 3.1.1) only lets us pull
        //  text out in segments by fiddling around
        //  with selecting it. Oh well.
        getCursorPosition(&pre_cycle_para, &pre_cycle_pos);
        selectAll(true);
        getSelection(&dummy, &dummy_two, &end_paragraph, &end_position);
        setSelection(pre_cycle_para, pre_cycle_pos, end_paragraph, end_position, 0);
        pre_cycle_text_from = selectedText();
        setSelection(0, 0, pre_cycle_para, pre_cycle_pos, 0);
        pre_cycle_text_upto = selectedText();
        selectAll(false);
        setCursorPosition(pre_cycle_para, pre_cycle_pos);
        updateCycle(current_choice, set); // Show the user their options
    }
}

void MythRemoteLineEdit::setCharacterColors(QColor unselected, QColor selected,
                                            QColor special)
{
    col_unselected = unselected;
    col_selected = selected;
    col_special = special;
    assignHexColors();
}

void MythRemoteLineEdit::updateCycle(QString current_choice, QString set)
{
    int index;
    QString aString, bString;

    //  Show the characters in the current set being cycled
    //  through, with the current choice in a different color. If the current
    //  character is uppercase X (interpreted as desctructive
    //  backspace) or an underscore (interpreted as a space)
    //  then show these special cases in yet another color.

    if (shift)
    {
        set = set.toUpper();
        current_choice = current_choice.toUpper();
    }

    bString  = "<B>";
    if (current_choice == "_" || current_choice == "X")
    {
        bString += "<FONT COLOR=\"#";
        bString += hex_special;
        bString += "\">";
        bString += current_choice;
        bString += "</FONT>";
    }
    else
    {
        bString += "<FONT COLOR=\"#";
        bString += hex_selected;
        bString += "\">";
        bString += current_choice;
        bString += "</FONT>";
    }
    bString += "</B>";

    index = set.indexOf(current_choice);
    int length = set.length();
    if (index < 0 || index > length)
    {
        cerr << "libmyth: MythRemoteLineEdit passed a choice of \"" << (const char *)current_choice << "\" which is not in set \"" << (const char *)set << "\"" << endl;
        setText("????");
        return;
    }
    else
    {
        set.replace(index, current_choice.length(), bString);
    }

    QString esc_upto =  pre_cycle_text_upto;
    QString esc_from =  pre_cycle_text_from;

    esc_upto.replace("<", "&lt;").replace(">", "&gt;").replace("\n", "<br>");
    esc_from.replace("<", "&lt;").replace(">", "&gt;").replace("\n", "<br>");

    aString = esc_upto;
    aString += "<FONT COLOR=\"#";
    aString += hex_unselected;
    aString += "\">[";
    aString += set;
    aString += "]</FONT>";
    aString += esc_from;
    setTextFormat(Qt::RichText);
    setText(aString);
    setCursorPosition(pre_cycle_para, pre_cycle_pos + set.length());
    update();
    setCursorPosition(pre_cycle_para, pre_cycle_pos);

    //  If current selection is delete,
    //  select the character that may well
    //  get deleted

    if (current_choice == "X" && pre_cycle_pos > 0)
    {
        setSelection(pre_cycle_para, pre_cycle_pos - 1, pre_cycle_para, pre_cycle_pos, 0);
    }
}

void MythRemoteLineEdit::assignHexColors()
{
    char text[1024];

    sprintf(text, "%.2X%.2X%.2X", col_unselected.red(), col_unselected.green(), col_unselected.blue());
    hex_unselected = text;

    sprintf(text, "%.2X%.2X%.2X", col_selected.red(), col_selected.green(), col_selected.blue());
    hex_selected = text;

    sprintf(text, "%.2X%.2X%.2X", col_special.red(), col_special.green(), col_special.blue());
    hex_special = text;
}

void MythRemoteLineEdit::endCycle()
{
    QString aString;

    if (active_cycle)
    {
        //  The timer ran out or the user pressed a key
        //  outside of the current set of choices
        if (current_choice == "_")       //  Space
        {
            aString  = pre_cycle_text_upto;
            aString += " ";
            aString += pre_cycle_text_from;
        }
        else if (current_choice == "X") // destructive backspace
        {
            //  Deal with special case in a way
            //  that all 3.x.x versions of Qt
            //  can handle

            if (pre_cycle_text_upto.length() > 0)
            {
                aString = pre_cycle_text_upto.left(pre_cycle_text_upto.length() - 1);
            }
            else
            {
                aString = "";
            }
            aString += pre_cycle_text_from;
            pre_cycle_pos--;
        }
        else if (shift)
        {
            aString  = pre_cycle_text_upto;
            aString += current_choice.toUpper();
            aString += pre_cycle_text_from;
        }
        else
        {
            aString  = pre_cycle_text_upto;
            aString += current_choice;
            aString += pre_cycle_text_from;
        }

        setTextFormat(Qt::PlainText);
        setText(aString);
        setCursorPosition(pre_cycle_para, pre_cycle_pos + 1);
        active_cycle = false;
        current_choice = "";
        current_set = "";
    }
    emit(textChanged(this->text()));
}

void MythRemoteLineEdit::setText(const QString& text)
{
    int para, pos;

    //  Isaac had this in the original version
    //  of MythLineEdit, and I'm sure he had
    //  a reason ...
    getCursorPosition(&para, &pos);
    Q3TextEdit::setText(text);
    setCursorPosition(para, pos);
}

QString MythRemoteLineEdit::text(void)
{
    return Q3TextEdit::text();
}

void MythRemoteLineEdit::keyPressEvent(QKeyEvent *e)
{
    bool handled = false;
    QStringList actions;

    if ((!popup || !popup->isShown()) &&
          gContext->TranslateKeyPress("qt", e, actions, false))
    {
        for (int i = 0; i < actions.size() && !handled; i++)
        {
            QString action = actions[i];
            handled = true;

            if (action == "UP")
            {
                endCycle();
                // Need to call very base one because
                // QTextEdit reimplements it to tab
                // through links (even if you're in
                // PlainText Mode !!)
                QWidget::focusNextPrevChild(false);
                emit tryingToLooseFocus(false);
            }
            else if (action == "DOWN")
            {
                endCycle();
                QWidget::focusNextPrevChild(true);
                emit tryingToLooseFocus(true);
            }
            else if ((action == "SELECT") &&
                     (!active_cycle) &&
                     ((e->text().isEmpty()) ||
                      (e->key() == Qt::Key_Enter) ||
                      (e->key() == Qt::Key_Return)))
            {
                if (useVirtualKeyboard)
                    popupVirtualKeyboard();
            }
            else
                handled = false;
        }
    }

    if (handled)
        return;

    if (popup && popup->isShown())
    {
        endCycle();
        Q3TextEdit::keyPressEvent(e);
        emit textChanged(this->text());
        return;
    }

    switch (e->key())
    {
        case Qt::Key_Enter:
        case Qt::Key_Return:
            handled = true;
            endCycle();
            e->ignore();
            break;

            //  Only eat Qt::Key_Space if we are in a cycle

        case Qt::Key_Space:
            if (active_cycle)
            {
                handled = true;
                endCycle();
                e->ignore();
            }
            break;

            //  If you want to mess arround with other ways to allocate
            //  key presses you can just add entries between the quotes

        case Qt::Key_1:
            cycleKeys("_X%-/.?()1");
            handled = true;
            break;

        case Qt::Key_2:
            cycleKeys("abc2");
            handled = true;
            break;

        case Qt::Key_3:
            cycleKeys("def3");
            handled = true;
            break;

        case Qt::Key_4:
            cycleKeys("ghi4");
            handled = true;
            break;

        case Qt::Key_5:
            cycleKeys("jkl5");
            handled = true;
            break;

        case Qt::Key_6:
            cycleKeys("mno6");
            handled = true;
            break;

        case Qt::Key_7:
            cycleKeys("pqrs7");
            handled = true;
            break;

        case Qt::Key_8:
            cycleKeys("tuv8");
            handled = true;
            break;

        case Qt::Key_9:
            cycleKeys("wxyz90");
            handled = true;
            break;

        case Qt::Key_0:
            toggleShift();
            handled = true;
            break;
    }

    if (!handled)
    {
        endCycle();
        Q3TextEdit::keyPressEvent(e);
        emit textChanged(this->text());
    }
}

void MythRemoteLineEdit::setCycleTime(float desired_interval)
{
    if (desired_interval < 0.5 || desired_interval > 10.0)
    {
        cerr << "libmyth: Did not accept key cycle interval of " << desired_interval << " seconds" << endl;
    }
    else
    {
        cycle_time = (int) (desired_interval * 1000.0);
    }
}

void MythRemoteLineEdit::cycleKeys(QString cycle_list)
{
    int index;

    if (active_cycle)
    {
        if (cycle_list == current_set)
        {
            //  Regular movement through existing set
            cycle_timer->changeInterval(cycle_time);
            index = current_set.indexOf(current_choice);
            int length = current_set.length();
            if (index + 1 >= length)
            {
                index = -1;
            }
            current_choice = current_set.mid(index + 1, 1);
            updateCycle(current_choice, current_set);
        }
        else
        {
            //  Previous cycle was still active, but user moved
            //  to another keypad key
            endCycle();
            current_choice = cycle_list.left(1);
            current_set = cycle_list;
            cycle_timer->changeInterval(cycle_time);
            startCycle(current_choice, current_set);
        }
    }
    else
    {
        //  First press with no cycle of any type active
        current_choice = cycle_list.left(1);
        current_set = cycle_list;
        startCycle(current_choice, current_set);
    }
}

void MythRemoteLineEdit::toggleShift()
{
    //  Toggle uppercase/lowercase and
    //  update the cycle display if it's
    //  active
    QString temp_choice = current_choice;
    QString temp_set = current_set;

    if (shift)
    {
        shift = false;
    }
    else
    {
        shift = true;
        temp_choice = current_choice.toUpper();
        temp_set = current_set.toUpper();
    }
    if (active_cycle)
    {
        updateCycle(temp_choice, temp_set);
    }
}

void MythRemoteLineEdit::setHelpText(const QString &help)
{
    bool changed = helptext != help;
    helptext = help;
    if (hasFocus() && changed)
        emit changeHelpText(help);
}

void MythRemoteLineEdit::focusInEvent(QFocusEvent *e)
{
    emit changeHelpText(helptext);
    emit gotFocus();    //perhaps we need to save a playlist?

    QColor highlight = colorGroup().highlight();

    this->setPaletteBackgroundColor(highlight);

    Q3TextEdit::focusInEvent(e);
}

void MythRemoteLineEdit::focusOutEvent(QFocusEvent *e)
{
    this->unsetPalette();

    if (popup && popup->isShown() && !popup->hasFocus())
        popup->hide();

    emit lostFocus();
    Q3TextEdit::focusOutEvent(e);
}

MythRemoteLineEdit::~MythRemoteLineEdit()
{
    Teardown();
}

void MythRemoteLineEdit::deleteLater(void)
{
    Teardown();
    Q3TextEdit::deleteLater();
}

void MythRemoteLineEdit::Teardown(void)
{
    if (cycle_timer)
    {
        cycle_timer->disconnect();
        cycle_timer->deleteLater();
        cycle_timer = NULL;
    }

    qt_delete(popup);
}

void MythRemoteLineEdit::popupVirtualKeyboard(void)
{
    qt_delete(popup);

    popup = new VirtualKeyboard(gContext->GetMainWindow(), this);
    gContext->GetMainWindow()->detach(popup);
    popup->exec();

    qt_delete(popup);
}

void MythRemoteLineEdit::insert(QString text)
{
    Q3TextEdit::insert(text);
    emit textChanged(this->text());
}

void MythRemoteLineEdit::del()
{
    doKeyboardAction(Q3TextEdit::ActionDelete);
    emit textChanged(this->text());
}

void MythRemoteLineEdit::backspace()
{
    doKeyboardAction(Q3TextEdit::ActionBackspace);
    emit textChanged(this->text());
}

MythPushButton::MythPushButton(const QString &ontext, const QString &offtext,
                               QWidget *parent, bool isOn, bool aa)
                               : QPushButton(ontext, parent)
{
    setBackgroundOrigin(WindowOrigin);
    arrowAccel = aa;

    onText = ontext;
    offText = offtext;

    setToggleButton(true);

    if (isOn)
        setText(onText);
    else
        setText(offText);

    setOn(isOn);
}

void MythPushButton::setHelpText(const QString &help)
{
    bool changed = helptext != help;
    helptext = help;
    if (hasFocus() && changed)
        emit changeHelpText(help);
}

void MythPushButton::keyPressEvent(QKeyEvent *e)
{
    bool handled = false;
    QStringList actions;
    keyPressActions.clear();

    if (gContext->TranslateKeyPress("qt", (QKeyEvent *)e, actions))
    {
        keyPressActions = actions;

        for (int i = 0; i < actions.size() && !handled; i++)
        {
            QString action = actions[i];
            if (action == "SELECT")
            {
                if (isToggleButton())
                    toggleText();
                setDown(true);
                emit pressed();
                handled = true;
            }
            else if (arrowAccel)
            {
                if (action == "LEFT")
                {
                    parent()->event(e);
                    handled = true;
                }
                else if (action == "RIGHT")
                {
                    if (isToggleButton())
                        toggleText();
                    setDown(true);
                    emit pressed();
                    handled = true;
                }
            }
        }
    }

    if (!handled)
        QPushButton::keyPressEvent(e);
}

void MythPushButton::keyReleaseEvent(QKeyEvent *e)
{
    bool handled = false;
    QStringList actions = keyPressActions;
    for (int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        if (action == "SELECT")
        {
            QKeyEvent tempe(QEvent::KeyRelease, Qt::Key_Space, ' ', 0, " ");
            QPushButton::keyReleaseEvent(&tempe);
            handled = true;
        }
    }

    if (!handled)
        QPushButton::keyReleaseEvent(e);
}

void MythPushButton::toggleText(void)
{
    if (!isToggleButton())
        return;

    if (isOn())
        setText(offText);
    else
        setText(onText);
}

void MythPushButton::focusInEvent(QFocusEvent *e)
{
    emit changeHelpText(helptext);

    QColor highlight = colorGroup().highlight();

    this->setPaletteBackgroundColor(highlight);
    QPushButton::focusInEvent(e);
}

void MythPushButton::focusOutEvent(QFocusEvent *e)
{
    this->unsetPalette();
    QPushButton::focusOutEvent(e);
}

MythListBox::MythListBox(QWidget *parent, const QString &name) :
    QListWidget(parent)
{
    setObjectName(name);
}

void MythListBox::ensurePolished(void) const
{
    QListWidget::ensurePolished();

    QPalette pal = palette();
    QColorGroup::ColorRole role = QColorGroup::Highlight;
    pal.setColor(QPalette::Active, role, pal.active().button());
    pal.setColor(QPalette::Inactive, role, pal.active().button());
    pal.setColor(QPalette::Disabled, role, pal.active().button());

    const_cast<MythListBox*>(this)->setPalette(pal);
}

void MythListBox::setCurrentItem(const QString& matchText, bool caseSensitive,
                                 bool partialMatch)
{
    for (unsigned i = 0 ; i < (unsigned)count() ; ++i)
    {
        if (partialMatch)
        {
            if (caseSensitive)
            {
                if (text(i).startsWith(matchText))
                {
                    setCurrentRow(i);
                    break;
                }
            }
            else
            {
                if (text(i).toLower().startsWith(matchText.toLower()))
                {
                    setCurrentRow(i);
                    break;
                }
            }
        }
        else
        {
            if (caseSensitive)
            {
                if (text(i) == matchText)
                {
                    setCurrentRow(i);
                    break;
                }
            }
            else
            {
                if (text(i).toLower() == matchText.toLower())
                {
                    setCurrentRow(i);
                    break;
                }
            }
        }
    }
}

void MythListBox::keyPressEvent(QKeyEvent* e)
{
    bool handled = false;
    QStringList actions;
    if (gContext->TranslateKeyPress("qt", e, actions))
    {
        for (int i = 0; i < actions.size() && !handled; i++)
        {
            QString action = actions[i];
            if (action == "UP" || action == "DOWN" || action == "PAGEUP" ||
                action == "PAGEDOWN" || action == "LEFT" || action == "RIGHT")
            {
                int key;
                if (action == "UP")
                {
                    // Qt::Key_Up at top of list allows focus to move to other widgets
                    if (currentItem() == 0)
                    {
                        focusNextPrevChild(false);
                        handled = true;
                        continue;
                    }

                    key = Qt::Key_Up;
                }
                else if (action == "DOWN")
                {
                    // Qt::Key_down at bottom of list allows focus to move to other widgets
                    if (currentRow() == (int) count() - 1)
                    {
                        focusNextPrevChild(true);
                        handled = true;
                        continue;
                    }

                    key = Qt::Key_Down;
                }
                else if (action == "LEFT")
                {
                    focusNextPrevChild(false);
                    handled = true;
                    continue;
                }
                else if (action == "RIGHT")
                {
                    focusNextPrevChild(true);
                    handled = true;
                    continue;
                }
                else if (action == "PAGEUP")
                    key = Qt::Key_PageUp;
                else if (action == "PAGEDOWN")
                    key = Qt::Key_PageDown;
                else
                    key = Qt::Key_unknown;

                QKeyEvent ev(QEvent::KeyPress, key, 0, Qt::NoButton);
                QListWidget::keyPressEvent(&ev);
                handled = true;
            }
            else if (action == "0" || action == "1" || action == "2" ||
                     action == "3" || action == "4" || action == "5" ||
                     action == "6" || action == "7" || action == "8" ||
                     action == "9")
            {
                int percent = action.toInt() * 10;
                int nextItem = percent * count() / 100;
                if (!itemVisible(nextItem))
                    setTopRow(nextItem);
                setCurrentRow(nextItem);
                handled = true;
            }
            else if (action == "PREVVIEW")
            {
                int nextItem = currentRow();
                if (nextItem > 0)
                    nextItem--;
                while (nextItem > 0 && text(nextItem)[0] == ' ')
                    nextItem--;
                if (!itemVisible(nextItem))
                    setTopRow(nextItem);
                setCurrentRow(nextItem);
                handled = true;
            }
            else if (action == "NEXTVIEW")
            {
                int nextItem = currentRow();
                if (nextItem < (int)count() - 1)
                    nextItem++;
                while (nextItem < (int)count() - 1 && text(nextItem)[0] == ' ')
                    nextItem++;
                if (!itemVisible(nextItem))
                    setTopRow(nextItem);
                setCurrentRow(nextItem);
                handled = true;
            }
            else if (action == "MENU")
                emit menuButtonPressed(currentRow());
            else if (action == "EDIT")
                emit editButtonPressed(currentRow());
            else if (action == "DELETE")
                emit deleteButtonPressed(currentRow());
            else if (action == "SELECT")
                emit accepted(currentRow());
        }
    }

    if (!handled)
        e->ignore();
}

void MythListBox::setHelpText(const QString &help)
{
    bool changed = helptext != help;
    helptext = help;
    if (hasFocus() && changed)
        emit changeHelpText(help);
}

void MythListBox::focusOutEvent(QFocusEvent *e)
{
    QPalette pal = palette();
    QColorGroup::ColorRole role = QColorGroup::Highlight;
    pal.setColor(QPalette::Active, role, pal.active().button());
    pal.setColor(QPalette::Inactive, role, pal.active().button());
    pal.setColor(QPalette::Disabled, role, pal.active().button());

    setPalette(pal);
    QListWidget::focusOutEvent(e);
}

void MythListBox::focusInEvent(QFocusEvent *e)
{
    setPalette(QPalette());

    emit changeHelpText(helptext);
    QListWidget::focusInEvent(e);
}


void MythListBox::setTopRow(uint row)
{
    QListWidgetItem *widget = item(row);
    if (widget)
        scrollToItem(widget, QAbstractItemView::PositionAtTop);
}

void MythListBox::insertItem(const QString &label)
{
    addItem(label);
}

void MythListBox::insertStringList(const QStringList &label_list)
{
    addItems(label_list);
}

void MythListBox::removeRow(uint row)
{
    QListWidgetItem *widget = takeItem(row);
    if (widget)
        delete widget;
}

void MythListBox::changeItem(const QString &new_text, uint row)
{
    QListWidgetItem *widget = item(row);
    if (widget)
        widget->setText(new_text);
}

int MythListBox::index(const QList<QListWidgetItem*> &list)
{
    return (list.empty()) ? -1 : row(list[0]);
}

QString MythListBox::text(uint row) const
{
    QListWidgetItem *widget = item(row);
    return (widget) ? widget->text() : QString::null;
}

bool MythListBox::itemVisible(uint row) const
{
    QListWidgetItem *widget = item(row);
    return (widget) ? !isItemHidden(widget) : false;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
