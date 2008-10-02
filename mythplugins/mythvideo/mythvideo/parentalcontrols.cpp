#include <map>

#include <mythtv/mythcontext.h>

#include <mythtv/libmythui/mythmainwindow.h>
#include <mythtv/libmythui/mythscreenstack.h>
#include <mythtv/libmythui/mythdialogbox.h>

#include "parentalcontrols.h"

namespace
{
    ParentalLevel::Level boundedParentalLevel(ParentalLevel::Level pl)
    {
        if (pl < ParentalLevel::plNone)
            return ParentalLevel::plNone;
        else if (pl > ParentalLevel::plHigh)
            return ParentalLevel::plHigh;

        return pl;
    }

    ParentalLevel::Level nextParentalLevel(ParentalLevel::Level cpl)
    {
        ParentalLevel::Level rpl(cpl);
        switch (cpl)
        {
            case ParentalLevel::plNone:
            { rpl = ParentalLevel::plLowest; break; }
            case ParentalLevel::plLowest: { rpl = ParentalLevel::plLow; break; }
            case ParentalLevel::plLow: { rpl = ParentalLevel::plMedium; break; }
            case ParentalLevel::plMedium:
            { rpl = ParentalLevel::plHigh; break; }
            case ParentalLevel::plHigh: { rpl = ParentalLevel::plHigh; break; }
        }

        return boundedParentalLevel(rpl);
    }

    ParentalLevel::Level prevParentalLevel(ParentalLevel::Level cpl)
    {
        ParentalLevel::Level rpl(cpl);
        switch (cpl)
        {
            case ParentalLevel::plNone: { rpl = ParentalLevel::plNone; break; }
            case ParentalLevel::plLowest:
            { rpl = ParentalLevel::plLowest; break; }
            case ParentalLevel::plLow: { rpl = ParentalLevel::plLowest; break; }
            case ParentalLevel::plMedium: { rpl = ParentalLevel::plLow; break; }
            case ParentalLevel::plHigh:
            { rpl = ParentalLevel::plMedium; break; }
        }

        return boundedParentalLevel(rpl);
    }

    ParentalLevel::Level toParentalLevel(int pl)
    {
        return boundedParentalLevel(static_cast<ParentalLevel::Level>(pl));
    }
}

ParentalLevel::ParentalLevel(Level pl) : m_level(pl),
    m_hitlimit(false)
{
}

ParentalLevel::ParentalLevel(int pl) :  m_hitlimit(false)
{
    m_level = toParentalLevel(pl);
}

ParentalLevel::ParentalLevel(const ParentalLevel &rhs) : m_hitlimit(false)
{
    *this = rhs;
}

ParentalLevel &ParentalLevel::operator=(const ParentalLevel &rhs)
{
    if (&rhs != this)
    {
        m_level = rhs.m_level;
    }

    return *this;
}

ParentalLevel &ParentalLevel::operator=(Level pl)
{
    m_level = boundedParentalLevel(pl);
    return *this;
}

ParentalLevel &ParentalLevel::operator++()
{
    Level last = m_level;
    m_level = nextParentalLevel(m_level);
    if (m_level == last)
        m_hitlimit = true;
    return *this;
}

ParentalLevel &ParentalLevel::operator+=(int amount)
{
    m_level = toParentalLevel(m_level + amount);
    return *this;
}

ParentalLevel &ParentalLevel::operator--()
{
    Level prev = m_level;
    m_level = prevParentalLevel(m_level);
    if (m_level == prev)
        m_hitlimit = true;
    return *this;
}

ParentalLevel &ParentalLevel::operator-=(int amount)
{
    m_level = toParentalLevel(m_level - amount);
    return *this;
}

ParentalLevel::Level ParentalLevel::GetLevel() const
{
    return m_level;
}

bool operator!=(const ParentalLevel &lhs, const ParentalLevel &rhs)
{
    return lhs.GetLevel() != rhs.GetLevel();
}

bool operator==(const ParentalLevel &lhs, const ParentalLevel &rhs)
{
    return lhs.GetLevel() == rhs.GetLevel();
}

bool operator<(const ParentalLevel &lhs, const ParentalLevel &rhs)
{
    return lhs.GetLevel() < rhs.GetLevel();
}

bool operator>(const ParentalLevel &lhs, const ParentalLevel &rhs)
{
    return lhs.GetLevel() > rhs.GetLevel();
}

bool operator<=(const ParentalLevel &lhs, const ParentalLevel &rhs)
{
    return lhs.GetLevel() <= rhs.GetLevel();
}

bool operator>=(const ParentalLevel &lhs, const ParentalLevel &rhs)
{
    return lhs.GetLevel() >= rhs.GetLevel();
}

namespace
{
    class PasswordManager
    {
      private:
        typedef std::map<ParentalLevel::Level, QString> pws;

      public:
        void Add(ParentalLevel::Level level, const QString &password)
        {
            m_passwords.insert(pws::value_type(level, password));
        }

        QStringList AtOrAbove(ParentalLevel::Level level)
        {
            QStringList ret;
            for (ParentalLevel i = level;
                    i <= ParentalLevel::plHigh && i.good(); ++i)
            {
                pws::const_iterator p = m_passwords.find(i.GetLevel());
                if (p != m_passwords.end() && p->second.length())
                    ret.push_back(p->second);
            }

            return ret;
        }

        QString FirstAtOrBelow(ParentalLevel::Level level)
        {
            QString ret;
            for (ParentalLevel i = level;
                    i >= ParentalLevel::plLow && i.good(); --i)
            {
                pws::const_iterator p = m_passwords.find(i.GetLevel());
                if (p != m_passwords.end() && p->second.length())
                {
                    ret = p->second;
                    break;
                }
            }

            return ret;
        }

      private:
        pws m_passwords;
    };
}

class ParentalLevelChangeCheckerPrivate : public QObject
{
    Q_OBJECT

  public:
    ParentalLevelChangeCheckerPrivate(QObject *lparent) : QObject(lparent)
    {
        m_pm.Add(ParentalLevel::plHigh,
                gContext->GetSetting("VideoAdminPassword"));
        m_pm.Add(ParentalLevel::plMedium,
                gContext->GetSetting("VideoAdminPasswordThree"));
        m_pm.Add(ParentalLevel::plLow,
                gContext->GetSetting("VideoAdminPasswordTwo"));
    }

    void Check(ParentalLevel::Level fromLevel, ParentalLevel::Level toLevel)
    {
        m_fromLevel = fromLevel;
        m_toLevel = toLevel;
        if (DoCheck())
        {
            emit SigDone(true, toLevel);
        }
    }

  signals:
    void SigDone(bool passwordValid, ParentalLevel::Level toLevel);

  private:
    // returns true if no completion is required
    bool DoCheck()
    {
        ParentalLevel which_level(m_toLevel);

        // No password for level 1 and you can always switch down from your
        // current level.
        if (which_level == ParentalLevel::plLowest ||
            which_level <= ParentalLevel(m_fromLevel))
            return true;

        // If there isn't a password at the current level, and
        // none of the levels below, we are done.
        // The assumption is that if you password protected lower levels,
        // and a higher level does not have a password it is something
        // you've overlooked (rather than intended).
        if (!m_pm.FirstAtOrBelow(which_level.GetLevel()).length())
            return true;

        // See if we recently (and successfully) asked for a password
        QString last_time_stamp = gContext->GetSetting("VideoPasswordTime");
        int last_parent_lvl = gContext->GetNumSetting("VideoPasswordLevel", -1);

        if (!last_time_stamp.length() || last_parent_lvl == -1)
        {
            VERBOSE(VB_IMPORTANT,
                    QString("%1: Could not read password/pin time "
                            "stamp. This is only an issue if it "
                            "happens repeatedly.").arg(__FILE__));
        }
        else
        {
            QDateTime curr_time = QDateTime::currentDateTime();
            QDateTime last_time =
                    QDateTime::fromString(last_time_stamp, Qt::ISODate);

            if (ParentalLevel(last_parent_lvl) >= which_level &&
                last_time.secsTo(curr_time) < 120)
            {
                // Two minute window
                last_time_stamp = curr_time.toString(Qt::ISODate);
                gContext->SetSetting("VideoPasswordTime", last_time_stamp);
                gContext->SaveSetting("VideoPasswordTime", last_time_stamp);
                return true;
            }
        }

        m_validPasswords = m_pm.AtOrAbove(which_level.GetLevel());

        // If there isn't a password for this level or higher levels, treat
        // the next lower password as valid. This is only done so people
        // cannot lock themselves out of the setup.
        if (!m_validPasswords.size())
        {
            QString pw = m_pm.FirstAtOrBelow(which_level.GetLevel());
            if (pw.length())
                m_validPasswords.push_back(pw);
        }

        // There are no suitable passwords.
        if (!m_validPasswords.size())
            return true;

        // If we got here, there is a password, and there's no backing down.
        MythScreenStack *popupStack =
                GetMythMainWindow()->GetStack("popup stack");

        MythTextInputDialog *pwd =
                new MythTextInputDialog(popupStack,
                        QObject::tr("Parental Pin:"), FilterNone, true);

        connect(pwd, SIGNAL(haveResult(QString)),
                SLOT(OnPasswordEntered(QString)));
        connect(pwd, SIGNAL(Exiting()), SLOT(OnPasswordExit()));

        if (pwd->Create())
            popupStack->AddScreen(pwd, false);

        return false;
    }

  private slots:
    void OnPasswordEntered(QString password)
    {
        bool ok = false;

        for (QStringList::iterator p = m_validPasswords.begin();
                p != m_validPasswords.end(); ++p)
        {
            if (password == *p)
            {
                ok = true;
                QString time_stamp =
                        QDateTime::currentDateTime().toString(Qt::ISODate);

                gContext->SaveSetting("VideoPasswordTime", time_stamp);
                gContext->SaveSetting("VideoPasswordLevel", m_toLevel);

                break;
            }
        }

        emit SigDone(ok, ok ? m_toLevel : m_fromLevel);
    }

    void OnPasswordExit()
    {
        emit SigDone(false, m_fromLevel);
    }

  private:
    ParentalLevel::Level m_fromLevel;
    ParentalLevel::Level m_toLevel;
    PasswordManager m_pm;
    QStringList m_validPasswords;
};

ParentalLevelChangeChecker::ParentalLevelChangeChecker()
{
    m_private = new ParentalLevelChangeCheckerPrivate(this);
    connect(m_private, SIGNAL(SigDone(bool, ParentalLevel::Level)),
            SLOT(OnResultReady(bool, ParentalLevel::Level)));
}

void ParentalLevelChangeChecker::Check(ParentalLevel::Level fromLevel,
        ParentalLevel::Level toLevel)
{
    m_private->Check(fromLevel, toLevel);
}

void ParentalLevelChangeChecker::OnResultReady(bool passwordValid,
        ParentalLevel::Level newLevel)
{
    emit SigResultReady(passwordValid, newLevel);
}

#include "parentalcontrols.moc"
