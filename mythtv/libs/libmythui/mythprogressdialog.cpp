
// MythTV headers
#include "mythprogressdialog.h"
#include "mythcontext.h"

MythUIBusyDialog::MythUIBusyDialog(const QString &message,
                             MythScreenStack *parent, const char *name) 
         : MythScreenType(parent, name, false)
{
    if (!message.isEmpty())
        m_message = message;
    else
        m_message = tr("Please Wait ...");
    m_messageText = NULL;
}

bool MythUIBusyDialog::Create(void)
{
    if (!CopyWindowFromBase("MythBusyDialog", this))
        return false;

    m_messageText = dynamic_cast<MythUIText *>(GetChild("message"));

    if (m_messageText)
        m_messageText->SetText(m_message);

    return true;
}

void MythUIBusyDialog::Close(void)
{
    GetScreenStack()->PopScreen();
}

//---------------------------------------------------------

MythUIProgressDialog::MythUIProgressDialog(const QString &message,
                             MythScreenStack *parent, const char *name) 
         : MythScreenType(parent, name, false)
{
    m_count = m_total = 0;
    m_message = message;
    m_messageText = NULL;
}

bool MythUIProgressDialog::Create(void)
{
    if (!CopyWindowFromBase("MythProgressDialog", this))
        return false;

    m_messageText = dynamic_cast<MythUIText *>(GetChild("message"));
    m_progressText = dynamic_cast<MythUIText *>(GetChild("progresstext"));
//    m_progressBar = dynamic_cast<MythUIProgressBar *>(GetChild("progressbar"));

    if (m_messageText)
        m_messageText->SetText(m_message);

    return true;
}

void MythUIProgressDialog::SetTotal(uint total)
{
    m_total = total;
    UpdateProgress();
}

void MythUIProgressDialog::SetCount(uint count)
{
    m_count = count;
    UpdateProgress();
}

void MythUIProgressDialog::UpdateProgress()
{
    if (m_total == 0)
        return;

    if (m_count > m_total)
    {
        VERBOSE(VB_IMPORTANT, QString("Progress count (%1) is higher "
                                      "than total (%2)")
                                      .arg(m_count)
                                      .arg(m_total));
        return;
    }

//     if (m_progressBar)
//     {
//         m_progressBar->SetTotal(total);
//         m_progressBar->SetCount(count);
//     }

    uint percentage = (uint)(((float)m_count/(float)m_total) * 100.0);

    if (m_progressText)
        m_progressText->SetText(QString("%1%").arg(percentage));
}