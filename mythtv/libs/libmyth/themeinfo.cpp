#include <qfile.h>
#include <qdir.h>

#include "themeinfo.h"
#include "mythcontext.h"

ThemeInfo::ThemeInfo(QString theme)
{

    m_theme = new QFileInfo (theme);
    m_name = m_aspect = m_previewpath = "";
    m_type = THEME_UNKN;

    if (!parseThemeInfo())
    {
        // Fallback for old themes
        // N.B. This is temporary to allow theme maintainers
        // to catch up and implement a themeinfo.xml

        VERBOSE(VB_GENERAL, QString("The theme (%1) needs to be updated "
                                    " to include a themeinfo.xml file")
                                    .arg(m_theme->fileName()));

        m_name = m_theme->fileName();

        if (m_name.contains("-wide"))
            m_aspect = "16:9";
        else
            m_aspect = "4:3";

        if (QFile::exists(m_theme->absFilePath() + "/theme.xml"))
        {
            m_type = THEME_UI;
        }
        else if (QFile::exists(m_theme->absFilePath() + "/osd.xml"))
        {
            m_type = THEME_OSD;
        }
        else if (QFile::exists(m_theme->absFilePath() + "/mainmenu.xml"))
        {
            m_type = THEME_MENU;
        }

        m_previewpath = m_theme->absFilePath() + "/preview.jpg";
    }
}

ThemeInfo::~ThemeInfo()
{

    if (m_theme)
        delete m_theme;

}

bool ThemeInfo::parseThemeInfo()
{

    QDomDocument doc;

    QFile f(m_theme->absFilePath() + "/themeinfo.xml");

    if (!f.open(IO_ReadOnly))
    {
        VERBOSE(VB_FILE, QString("Unable to open themeinfo.xml "
                                      "for %1").arg(m_theme->absFilePath()));
        return false;
    }

    if ( !doc.setContent( &f ) ) {
        VERBOSE(VB_IMPORTANT, QString("Unable to parse themeinfo.xml "
                                      "for %1").arg(m_theme->fileName()));
        f.close();
        return false;
    }
    f.close();

    QDomElement docElem = doc.documentElement();

    for (QDomNode n = docElem.firstChild(); !n.isNull();
            n = n.nextSibling())
    {
        QDomElement e = n.toElement();
        if (!e.isNull())
        {
            if (e.tagName() == "name")
            {
                m_name = e.firstChild().toText().data();
            }
            else if (e.tagName() == "aspect")
            {
                m_aspect = e.firstChild().toText().data();
            }
            else if (e.tagName() == "types")
            {
                for (QDomNode child = e.firstChild(); !child.isNull();
                        child = child.nextSibling())
                {
                    QDomElement ce = child.toElement();
                    if (!ce.isNull())
                    {
                        if (ce.tagName() == "type")
                        {
                            QString type = ce.firstChild().toText().data();

                            if (type == "UI")
                            {
                                m_type |= THEME_UI;
                            }
                            else if (type == "OSD")
                            {
                                m_type |= THEME_OSD;
                            }
                            else if (type == "Menu")
                            {
                                m_type |= THEME_MENU;
                            }
                            else
                            {
                                VERBOSE(VB_IMPORTANT, QString("Invalid theme "
                                                      "type seen when parsing "
                                                      "%2")
                                                      .arg(m_theme->fileName()));
                            }
                        }
                    }
                }
            }
            else if (e.tagName() == "detail")
            {
                for (QDomNode child = e.firstChild(); !child.isNull();
                        child = child.nextSibling())
                {
                    QDomElement ce = child.toElement();
                    if (!ce.isNull())
                    {
                        if (ce.tagName() == "thumbnail")
                        {
                            QString thumbnail = ce.firstChild().toText().data();
                            m_previewpath = m_theme->absFilePath() + "/"
                                                + thumbnail;
                        }
                    }
                }
            }
        }
    }

    return true;
}

bool ThemeInfo::IsWide()
{

    if (m_aspect == "16:9" || m_aspect == "16:10")
        return true;

    return false;
}