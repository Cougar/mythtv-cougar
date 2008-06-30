/*
 *  Class MythTerminal
 *
 *  Copyright (C) Daniel Kristjansson 2008
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

// MythTV headers
#include "mythterminal.h"
#include "mythcontext.h"

MythTerminal::MythTerminal(QString _program, QStringList _arguments) :
    lock(true), running(false),
    process(new QProcess()), program(_program), arguments(_arguments),
    curLabel(""), curValue(0), filter(new MythTerminalKeyFilter())
{
    addSelection(curLabel, QString::number(curValue));

    process->setProcessChannelMode(QProcess::MergedChannels);
    connect(process, SIGNAL(readyRead()),
            this,    SLOT(  ProcessHasText()));

    connect(process, SIGNAL(finished(int, QProcess::ExitStatus)),
            this,    SLOT(  ProcessFinished(int, QProcess::ExitStatus)));

    connect(filter,  SIGNAL(KeyPressd(QKeyEvent*)),
            this,    SLOT(  ProcessSendKeyPress(QKeyEvent*)));
    SetEventFilter(filter);
}

void MythTerminal::TeardownAll(void)
{
    if (process)
    {
        QMutexLocker locker(&lock);
        if (running)
            Kill();
        process->disconnect();
    }

    if (filter)
    {
        filter->disconnect();
    }

    if (process)
    {
        process->deleteLater();
        process = NULL;
    }

    if (filter)
    {
        filter->deleteLater();
        filter = NULL;
    }
}

void MythTerminal::AddText(const QString &_str)
{
    QMutexLocker locker(&lock);
    QString str = _str;
    while (str.length())
    {
        int nlf = str.find("\r\n");
        nlf = (nlf < 0) ? str.find("\r") : nlf;
        nlf = (nlf < 0) ? str.find("\n") : nlf;

        QString curStr = (nlf >= 0) ? str.left(nlf) : str;
        if (curStr.length())
        {
            curLabel += curStr;
            ReplaceLabel(curLabel, QString::number(curValue));
        }

        if (nlf >= 0)
        {
            addSelection(curLabel = "", QString::number(++curValue));
            str = str.mid(nlf + 1);
        }
        else
        {
            str = "";
        }
    }
    widget->setEnabled(true);
    widget->setFocus();
}

void MythTerminal::Start(void)
{
    QMutexLocker locker(&lock);
    process->start(program, arguments);
    running = true;
}

void MythTerminal::Kill(void)
{
    QMutexLocker locker(&lock);
    process->kill();
    running = false;
}

bool MythTerminal::IsDone(void) const
{
    QMutexLocker locker(&lock);
    return QProcess::NotRunning == process->state();
}

void MythTerminal::ProcessHasText(void)
{
    QMutexLocker locker(&lock);
    int64_t len = process->bytesAvailable();

    if (len <= 0)
        return;

    QByteArray buf = process->read(len);
    AddText(QString(buf));
}

void MythTerminal::ProcessSendKeyPress(QKeyEvent *e)
{
    QMutexLocker locker(&lock);
    if (running && process && e->text().length())
    {
        AddText(e->text().local8Bit());
        if (e->ascii()=='\n' || e->ascii()=='\r')
            process->write("\r\n");
        else
            process->write(e->text().local8Bit());
    }
}

void MythTerminal::ProcessFinished(
    int exitCode, QProcess::ExitStatus exitStatus)
{
    QMutexLocker locker(&lock);
    AddText(tr("*** Exited with status: %1 ***").arg(exitCode));
    setEnabled(false);
    running = false;
}

////////////////////////////////////////////////////////////////////////

bool MythTerminalKeyFilter::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::KeyPress)
    {
        QKeyEvent *e = (QKeyEvent*)(event);
        QStringList actions;
        if (gContext->TranslateKeyPress("qt", e, actions, false))
        {
            if (actions.contains("LEFT") || actions.contains("RIGHT"))
            {
                return QObject::eventFilter(obj, event);
            }
            else
            {
                emit KeyPressd(e);
                e->accept();
                return true;
            }
        }
        else
        {
            emit KeyPressd(e);
            e->accept();
            return true;
        }
    }
    else
    {
        return QObject::eventFilter(obj, event);
    }
}

