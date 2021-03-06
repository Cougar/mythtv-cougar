#include <QApplication>
#include <QDomDocument>

#include "mythverbose.h"
#include "mythdb.h"

#include "mythuihelper.h"
#include "mythfontproperties.h"
#include "mythmainwindow.h"

MythFontProperties::MythFontProperties() :
    m_color(QColor(Qt::white)), m_hasShadow(false), m_shadowAlpha(255),
    m_hasOutline(false), m_outlineAlpha(255), m_bFreeze(false)
{
    CalcHash();
}

void MythFontProperties::SetFace(const QFont &face)
{
    m_face = face;
    CalcHash();
}

void MythFontProperties::SetColor(const QColor &color)
{
    m_color = color;
    CalcHash();
}

void MythFontProperties::SetShadow(bool on, const QPoint &offset,
                                   const QColor &color, int alpha)
{
    m_hasShadow = on;
    m_shadowOffset = offset;
    m_shadowColor = color;
    m_shadowAlpha = alpha;
    CalcHash();
}

void MythFontProperties::SetOutline(bool on, const QColor &color,
                                    int size, int alpha)
{
    m_hasOutline = on;
    m_outlineColor = color;
    m_outlineSize = size;
    m_outlineAlpha = alpha;
    CalcHash();
}

void MythFontProperties::GetShadow(QPoint &offset, QColor &color, int &alpha) const
{
    offset = m_shadowOffset;
    color = m_shadowColor;
    alpha = m_shadowAlpha;
}

void MythFontProperties::GetOutline(QColor &color, int &size, int &alpha) const
{
    color = m_outlineColor;
    size = m_outlineSize;
    alpha = m_outlineAlpha;
}

void MythFontProperties::GetOffset(QPoint &offset) const
{
    offset = m_drawingOffset;
}

void MythFontProperties::CalcHash(void)
{
    if (m_bFreeze)
        return;

    m_hash = QString("%1%2%3%4").arg(m_face.toString())
                 .arg(m_color.name()).arg(m_hasShadow).arg(m_hasOutline);

    if (m_hasShadow)
        m_hash += QString("%1%2%3%4").arg(m_shadowOffset.x())
                 .arg(m_shadowOffset.y()).arg(m_shadowColor.name())
                 .arg(m_shadowAlpha);

    if (m_hasOutline)
        m_hash += QString("%1%2%3").arg(m_outlineColor.name())
                 .arg(m_outlineSize).arg(m_outlineAlpha);

    m_drawingOffset = QPoint(0, 0);

    if (m_hasOutline)
    {
        m_drawingOffset = QPoint(m_outlineSize, m_outlineSize);
    }

    if (m_hasShadow && !m_hasOutline)
    {
        if (m_shadowOffset.x() < 0)
            m_drawingOffset.setX(-m_shadowOffset.x());
        if (m_shadowOffset.y() < 0)
            m_drawingOffset.setY(-m_shadowOffset.y());
    }
    if (m_hasShadow && !m_hasOutline)
    {
        if (m_shadowOffset.x() < 0 && m_shadowOffset.x() < -m_outlineSize)
            m_drawingOffset.setX(-m_shadowOffset.x());
        if (m_shadowOffset.y() < 0 && m_shadowOffset.y() < -m_outlineSize)
            m_drawingOffset.setY(-m_shadowOffset.y());
    }
}

void MythFontProperties::Freeze(void)
{
    m_bFreeze = true;
}

void MythFontProperties::Unfreeze(void)
{
    m_bFreeze = false;
    CalcHash();
}

MythFontProperties *MythFontProperties::ParseFromXml(QDomElement &element,
                                                     MythUIType *parent,
                                                     bool addToGlobal)
{
    // Crappy, but cached.  Move to GlobalFontMap?
    QString fontSizeType = GetMythDB()->GetSetting("ThemeFontSizeType", "default");

    bool fromBase = false;
    MythFontProperties *newFont = new MythFontProperties();
    newFont->Freeze();

    QString name = element.attribute("name", "");
    if (name.isNull() || name.isEmpty())
    {
        VERBOSE(VB_IMPORTANT, "Font needs a name");
        return NULL;
    }

    if (addToGlobal && GetGlobalFontMap()->Contains(name))
    {
        VERBOSE(VB_FILE, QString("Warning, already have a global font "
                                 "called: %1").arg(name));
        return NULL;
    }

    QString base = element.attribute("from", "");

    if (!base.isEmpty())
    {
        MythFontProperties *tmp = NULL;

        if (parent)
            tmp = parent->GetFont(base);

        if (!tmp)
            tmp = GetGlobalFontMap()->GetFont(base);

        if (!tmp)
        {
            VERBOSE(VB_IMPORTANT,
                    QString("Specified base font '%1' does not "
                            "exist for font %2").arg(base).arg(name));
            return NULL;
        }

        *newFont = *tmp;
        fromBase = true;
    }

    int size, sizeSmall, sizeBig;
    size = sizeSmall = sizeBig = -1;

    QString face = element.attribute("face", "");
    if (face.isNull() || face.isEmpty())
    {
        if (!fromBase)
        {
            VERBOSE(VB_IMPORTANT, "Font needs a face");
            return NULL;
        }
    }
    else
    {
        newFont->m_face.setFamily(face);
        // NOTE: exactMatch() is broken and always returns false
//         if (!newFont->m_face.exactMatch())
//         {
//             QFont tmp = QApplication::font();
//             newFont->m_face.setFamily(tmp.family());
//         }
    }

    QString hint = element.attribute("stylehint", "");
    if (!hint.isNull() && !hint.isEmpty())
    {
        newFont->m_face.setStyleHint((QFont::StyleHint)hint.toInt());
    }

    for (QDomNode child = element.firstChild(); !child.isNull();
         child = child.nextSibling())
    {
        QDomElement info = child.toElement();
        if (!info.isNull())
        {
            if (info.tagName() == "size")
            {
                size = getFirstText(info).toInt();
            }
            else if (info.tagName() == "size:small")
            {
                sizeSmall = getFirstText(info).toInt();
            }
            else if (info.tagName() == "size:big")
            {
                sizeBig = getFirstText(info).toInt();
            }
            else if (info.tagName() == "color")
            {
                newFont->m_color = QColor(getFirstText(info));
            }
            else if (info.tagName() == "dropcolor" ||
                     info.tagName() == "shadowcolor")
            {
                newFont->m_shadowColor = QColor(getFirstText(info));
            }
            else if (info.tagName() == "shadow" ||
                     info.tagName() == "shadowoffset")
            {
                newFont->m_hasShadow = true;
                newFont->m_shadowOffset = parsePoint(info);
            }
            else if (info.tagName() == "shadowalpha")
            {
                newFont->m_shadowAlpha = getFirstText(info).toInt();
            }
            else if (info.tagName() == "outlinecolor")
            {
                newFont->m_outlineColor = QColor(getFirstText(info));
            }
            else if (info.tagName() == "outlinesize")
            {
                newFont->m_hasOutline = true;
                newFont->m_outlineSize = getFirstText(info).toInt();
            }
            else if (info.tagName() == "outlinealpha")
            {
                newFont->m_outlineAlpha = getFirstText(info).toInt();
            }
            else if (info.tagName() == "bold")
            {
                newFont->m_face.setBold(parseBool(info));
            }
            else if (info.tagName() == "italics")
            {
                newFont->m_face.setItalic(parseBool(info));
            }
            else if (info.tagName() == "underline")
            {
                newFont->m_face.setUnderline(parseBool(info));
            }
            else
            {
                VERBOSE(VB_IMPORTANT, QString("Unknown tag %1 in font")
                                              .arg(info.tagName()));
                return NULL;
            }
        }
    }

    if (sizeSmall > 0 && fontSizeType == "small")
    {
        size = sizeSmall;
    }
    else if (sizeBig > 0 && fontSizeType == "big")
    {
        size = sizeBig;
    }

    if (size <= 0 && !fromBase)
    {
        VERBOSE(VB_IMPORTANT, "Error, font size must be > 0");
        return NULL;
    }
    else if (size > 0)
        newFont->m_face.setPointSize(GetMythMainWindow()->NormalizeFontSize(size));

    newFont->Unfreeze();

    if (addToGlobal)
    {
        GetGlobalFontMap()->AddFont(name, newFont);
    }

    return newFont;
}

static FontMap *gFontMap = NULL;

// FIXME: remove
QMap<QString, fontProp> globalFontMap;

MythFontProperties *FontMap::GetFont(const QString &text)
{
    if (text.isEmpty())
        return NULL;

    if (m_FontMap.contains(text))
        return &(m_FontMap[text]);
    return NULL;
}

bool FontMap::AddFont(const QString &text, MythFontProperties *font)
{
    if (!font || text.isEmpty())
        return false;

    if (m_FontMap.contains(text))
    {
        VERBOSE(VB_IMPORTANT, QString("Already have a font: %1").arg(text));
        return false;
    }

    m_FontMap[text] = *font;

    {
        /* FIXME backwards compat, remove */
        fontProp oldf;

        oldf.face = font->m_face;
        oldf.color = font->m_color;
        if (font->m_hasShadow)
        {
            oldf.dropColor = font->m_shadowColor;
            oldf.shadowOffset = font->m_shadowOffset;
        }

        globalFontMap[text] = oldf;
    }

    return true;
}

bool FontMap::Contains(const QString &text)
{
    return m_FontMap.contains(text);
}

void FontMap::Clear(void)
{
    m_FontMap.clear();

    //FIXME: remove
    globalFontMap.clear();
}

FontMap *FontMap::GetGlobalFontMap(void)
{
    if (!gFontMap)
        gFontMap = new FontMap();
    return gFontMap;
}

FontMap *GetGlobalFontMap(void)
{
    return FontMap::GetGlobalFontMap();
}

