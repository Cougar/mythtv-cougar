#include <qimage.h>
#include <qmap.h>
#include <qregexp.h>
#include <math.h>

#include <iostream>
using namespace std;

#include "yuv2rgb.h"
#include "osdtypes.h"
#include "ttfont.h"
#include "osdsurface.h"

#include "mythcontext.h"

OSDSet::OSDSet(const QString &name, bool cache, int screenwidth, 
               int screenheight, float wmult, float hmult, int frint)
      : QObject()
{
    m_wantsupdates = false;
    m_lastupdate = 0;
    m_needsupdate = false;

    m_name = name;
    m_cache = cache;

    m_frameint = frint;

    m_hasdisplayed = false;
    m_displaying = false;
    m_timeleft = 0;
    m_allowfade = true;

    m_screenwidth = screenwidth;
    m_screenheight = screenheight;
    m_wmult = wmult;
    m_hmult = hmult;

    m_notimeout = false;
    m_fadetime = -1;
    m_maxfade = -1;
 
    m_xmove = 0;
    m_ymove = 0;

    m_xoff = 0;
    m_yoff = 0;

    m_priority = 5;
    currentOSDFunctionalType = 0;

    allTypes = new vector<OSDType *>;
}

OSDSet::OSDSet(const OSDSet &other)
      : QObject()
{
    m_screenwidth = other.m_screenwidth;
    m_screenheight = other.m_screenheight;
    m_frameint = other.m_frameint;
    m_wmult = other.m_wmult;
    m_hmult = other.m_hmult;
    m_cache = other.m_cache;
    m_name = other.m_name;
    m_notimeout = other.m_notimeout;
    m_hasdisplayed = other.m_hasdisplayed;
    m_timeleft = other.m_timeleft;
    m_displaying = other.m_displaying;
    m_fadetime = other.m_fadetime;
    m_maxfade = other.m_maxfade;
    m_priority = other.m_priority;
    m_xmove = other.m_xmove;
    m_ymove = other.m_ymove;
    m_xoff = other.m_xoff;
    m_yoff = other.m_yoff;
    m_allowfade = other.m_allowfade;
    currentOSDFunctionalType = 0;

    allTypes = new vector<OSDType *>;

    vector<OSDType *>::iterator iter = other.allTypes->begin();
    for (;iter != other.allTypes->end(); iter++)
    {
        OSDType *type = (*iter);
        if (OSDTypeText *item = dynamic_cast<OSDTypeText*>(type))
        {
            OSDTypeText *newtext = new OSDTypeText(*item);
            AddType(newtext);
        }
        else if (OSDTypePositionImage *item =
                  dynamic_cast<OSDTypePositionImage*>(type))
        {
            OSDTypePositionImage *newrect = new OSDTypePositionImage(*item);
            AddType(newrect);
        }
        else if (OSDTypeImage *item = dynamic_cast<OSDTypeImage*>(type))
        {
            OSDTypeImage *newimage = new OSDTypeImage(*item);
            AddType(newimage);
        }
        else if (OSDTypeBox *item = dynamic_cast<OSDTypeBox*>(type))
        {
            OSDTypeBox *newbox = new OSDTypeBox(*item);
            AddType(newbox);
        }
        else if (OSDTypePositionRectangle *item = 
                  dynamic_cast<OSDTypePositionRectangle*>(type))
        {
            OSDTypePositionRectangle *newrect = 
                               new OSDTypePositionRectangle(*item);
            AddType(newrect);
        }
        else
        {
            cerr << "Unknown conversion\n";
        }
    }
}

OSDSet::~OSDSet()
{
    vector<OSDType *>::iterator i = allTypes->begin();
    for (; i != allTypes->end(); i++)
    {
        OSDType *type = (*i);
        if (type)
            delete type;
    }
    delete allTypes;
}
 
void OSDSet::AddType(OSDType *type)
{
    typeList[type->Name()] = type;
    allTypes->push_back(type);
    type->SetParent(this);
}

void OSDSet::Reinit(int screenwidth, int screenheight, int xoff, int yoff,
                    int displaywidth, int displayheight, 
                    float wmult, float hmult, int frint)
{
    float wchange = wmult / m_wmult;
    float hchange = hmult / m_hmult;

    m_frameint = frint;

    m_screenwidth = screenwidth;
    m_screenheight = screenheight;
    m_wmult = wmult;
    m_hmult = hmult;

    vector<OSDType *>::iterator iter = allTypes->begin();
    for (;iter != allTypes->end(); iter++)
    {
        OSDType *type = (*iter);
        if (OSDTypeText *item = dynamic_cast<OSDTypeText*>(type))
        {
            item->Reinit(wchange, hchange);
        }
        else if (OSDTypePositionImage *item =
                 dynamic_cast<OSDTypePositionImage*>(type))
        {
            item->Reinit(wchange, hchange, wmult, hmult);
        }
        else if (OSDTypePosSlider *item = dynamic_cast<OSDTypePosSlider*>(type))
        {
            item->Reinit(wchange, hchange, wmult, hmult);
        }
        else if (OSDTypeFillSlider *item = 
                 dynamic_cast<OSDTypeFillSlider*>(type))
        {
            item->Reinit(wchange, hchange, wmult, hmult);
        }
        else if (OSDTypeEditSlider *item =
                 dynamic_cast<OSDTypeEditSlider*>(type))
        {
            item->Reinit(wchange, hchange, wmult, hmult);
        }
        else if (OSDTypeImage *item = dynamic_cast<OSDTypeImage*>(type))
        {
            item->Reinit(wmult, hmult);
        }
        else if (OSDTypeBox *item = dynamic_cast<OSDTypeBox*>(type))
        {
            item->Reinit(wchange, hchange);
        }
        else if (OSDTypePositionRectangle *item =
                  dynamic_cast<OSDTypePositionRectangle*>(type))
        {
            item->Reinit(wchange, hchange);
        }
        else if (OSDTypeCC *item = dynamic_cast<OSDTypeCC*>(type))
        {
            item->Reinit(xoff, yoff, displaywidth, displayheight);
        }
        else
        {
            cerr << "Unknown conversion\n";
        }
    }

}

OSDType *OSDSet::GetType(const QString &name)
{
    OSDType *ret = NULL;
    if (typeList.contains(name))
        ret = typeList[name];

    return ret;
}

void OSDSet::ClearAllText(void)
{
    vector<OSDType *>::iterator iter = allTypes->begin();
    for (; iter != allTypes->end(); iter++)
    {
        OSDType *type = (*iter);
        if (OSDTypeText *item = dynamic_cast<OSDTypeText *>(type))
            item->SetText(QString(""));
    }
}

void OSDSet::SetTextByRegexp(QMap<QString, QString> &regexpMap)
{
    vector<OSDType *>::iterator iter = allTypes->begin();
    for (; iter != allTypes->end(); iter++)
    {
        OSDType *type = (*iter);
        if (OSDTypeText *item = dynamic_cast<OSDTypeText *>(type))
        {
            QMap<QString, QString>::Iterator riter = regexpMap.begin();
            QString new_text = item->GetDefaultText();
            QString full_regex;

            if ((new_text == "") &&
                (regexpMap.contains(item->Name())))
            {
                new_text = regexpMap[item->Name()];
            }
            else
                for (; riter != regexpMap.end(); riter++)
                {
                    full_regex = "%" + riter.key().upper() + "%";
                    new_text.replace(QRegExp(full_regex), riter.data());
                }

            if (new_text != "")
                item->SetText(new_text);
        }
    }
}

void OSDSet::Display(bool onoff, int osdFunctionalType)
{
    if (onoff)
    {
        m_notimeout = true;
        m_displaying = true;
        m_timeleft = 1;
        m_fadetime = -1;
        m_xoff = 0;
        m_yoff = 0;
    }
    else
    {
        m_displaying = false;
    }

    if (currentOSDFunctionalType != osdFunctionalType && 
        currentOSDFunctionalType != 0)
    {
        emit OSDClosed(currentOSDFunctionalType);
    }

    currentOSDFunctionalType = osdFunctionalType;
}

void OSDSet::DisplayFor(int time, int osdFunctionalType)
{
    m_timeleft = time;
    m_displaying = true;
    m_fadetime = -1;
    m_notimeout = false;
    m_xoff = 0;
    m_yoff = 0;

    if (currentOSDFunctionalType != osdFunctionalType && 
        currentOSDFunctionalType != 0)
    {
        emit OSDClosed(currentOSDFunctionalType);
    }

    currentOSDFunctionalType = osdFunctionalType;
}
 
void OSDSet::FadeFor(int time)
{
    if (m_allowfade)
    {
        m_timeleft = -1;
        m_fadetime = time;
        m_maxfade = time;
        m_displaying = true;
        m_notimeout = false;
    }
}

void OSDSet::Hide(void)
{
    m_timeleft = 1;
    m_notimeout = false;
}

void OSDSet::Draw(OSDSurface *surface, bool actuallydraw)
{
    if (actuallydraw)
    {
        vector<OSDType *>::iterator i = allTypes->begin();
        for (; i != allTypes->end(); i++)
        {
            OSDType *type = (*i);
            type->Draw(surface, m_fadetime, m_maxfade, m_xoff, m_yoff);

            if (m_wantsupdates)
                m_lastupdate = (m_timeleft + 999999) / 1000000;
        }
    }

    m_hasdisplayed = true;

    if (m_notimeout)
        return;

    if (m_timeleft > 0)
    {
        m_timeleft -= m_frameint;
        if (m_timeleft < 0)
            m_timeleft = 0;
    }

    if (m_fadetime > 0)
    {
        m_fadetime -= m_frameint;

        if (m_xmove || m_ymove)
        {
            m_xoff += (m_xmove * m_frameint * 30) / 1000000;
            m_yoff += (m_ymove * m_frameint * 30) / 1000000;
            m_fadetime -= (4 * m_frameint);
        }

        if (m_fadetime <= 0) 
        {
            m_fadetime = 0;
            if (currentOSDFunctionalType)
            {
                emit OSDClosed(currentOSDFunctionalType);
                currentOSDFunctionalType = 0;
            }
        }
    }

    if (m_timeleft <= 0 && m_fadetime <= 0)
        m_displaying = false;
    else if (m_wantsupdates && 
             ((m_timeleft + 999999) / 1000000 != m_lastupdate))
        m_needsupdate = true;
    else
        m_needsupdate = false; 
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ 

OSDType::OSDType(const QString &name)
{
    m_name = name;
}

OSDType::~OSDType()
{
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

OSDTypeText::OSDTypeText(const QString &name, TTFFont *font, 
                         const QString &text, QRect displayrect)
           : OSDType(name)
{
    m_message = text;
    m_default_msg = text;
    m_font = font;

    m_altfont = NULL;
    
    m_displaysize = displayrect;
    m_multiline = false;
    m_centered = false;
    m_right = false;
    m_usingalt = false;
}

OSDTypeText::OSDTypeText(const OSDTypeText &other)
           : OSDType(other.m_name)
{
    m_displaysize = other.m_displaysize;
    m_message = other.m_message;
    m_default_msg = other.m_default_msg;
    m_font = other.m_font;
    m_altfont = other.m_altfont;
    m_centered = other.m_centered;
    m_multiline = other.m_multiline;
    m_usingalt = other.m_usingalt;
    m_right = other.m_right;
}

OSDTypeText::~OSDTypeText()
{
}

void OSDTypeText::SetAltFont(TTFFont *font)
{
    m_altfont = font;
}

void OSDTypeText::SetText(const QString &text)
{
    m_message = text;
}

void OSDTypeText::SetDefaultText(const QString &text)
{
    m_message = text;
    m_default_msg = text;
}

void OSDTypeText::Reinit(float wchange, float hchange)
{
    int width = (int)(m_displaysize.width() * wchange);
    int height = (int)(m_displaysize.height() * hchange);
    int x = (int)(m_displaysize.x() * wchange);
    int y = (int)(m_displaysize.y() * hchange);

    m_displaysize = QRect(x, y, width, height);
}

void OSDTypeText::Draw(OSDSurface *surface, int fade, int maxfade, int xoff, 
                       int yoff)
{
    int textlength = 0;

    if (m_message == QString::null)
        return;

    if (m_message.contains("%d"))
        m_parent->SetWantsUpdates(true);

    m_font->CalcWidth(m_message, &textlength);

    int maxlength = m_displaysize.width();

    if (m_multiline)
    {
        QString tmp_msg = m_message;
        tmp_msg.replace(QRegExp("%BR%"), "\n");
        tmp_msg.replace(QRegExp("\n")," \n ");

        QStringList wordlist = QStringList::split(" ", tmp_msg.ascii());
        int length = 0;
        int lines = 0;

        QString line = "";

        QStringList::iterator it = wordlist.begin();
        for (; it != wordlist.end(); ++it)
        {
            QString word = *it;
            if (word == "%d") 
            {
                m_parent->SetWantsUpdates(true);
                int timeleft = (m_parent->GetTimeLeft() + 999999) / 1000000;
                if (timeleft > 99)
                    timeleft = 99;
                if (timeleft < 0)
                    timeleft = 0;
                
                word = QString::number(timeleft);    
            }

            if (!length && word == "\n")
                continue;

            m_font->CalcWidth(word, &textlength);
            if ((textlength + m_font->SpaceWidth() + length > maxlength) ||
                (word == "\n"))
            {
                QRect drawrect = m_displaysize;
                drawrect.setTop(m_displaysize.top() + 
                                m_font->Size() * (lines) * 3 / 2);
                DrawString(surface, drawrect, line, fade, maxfade, xoff, yoff);
                length = 0;

                line = "";
                lines++;

                if (word == "\n")
                {
                    word = "";
                    textlength = 0;
                }
            }

            if (length == 0)
            {
                length = textlength;
                line = word;
            }
            else
            {
                line += " " + word;
                length += textlength + m_font->SpaceWidth();
            }
        }

        QRect drawrect = m_displaysize;
        drawrect.setTop(m_displaysize.top() + m_font->Size() * (lines) * 3 / 2);
        DrawString(surface, drawrect, line, fade, maxfade, xoff, yoff);
    }           
    else        
        DrawString(surface, m_displaysize, m_message, fade, maxfade, xoff, 
                   yoff);
}           
  
void OSDTypeText::DrawString(OSDSurface *surface, QRect rect, 
                             const QString &text, int fade, int maxfade, 
                             int xoff, int yoff)
{
    if (m_centered || m_right)
    {
        int textlength = 0;
        m_font->CalcWidth(text, &textlength);

        int xoffset = rect.width() - textlength;
        if (m_centered)
            xoffset /= 2;

        if (xoffset > 0)
            rect.moveBy(xoffset, 0);
    }

    rect.moveBy(xoff, yoff);

    int x = rect.left();
    int y = rect.top();
    int maxx = rect.right();
    int maxy = rect.bottom();

    if (maxx > surface->width)
        maxx = surface->width;

    if (maxy > surface->height)
        maxy = surface->height;

    int alphamod = 255;

    if (maxfade > 0 && fade >= 0)
        alphamod = (int)((((float)(fade) / maxfade) * 256.0) + 0.5);

    TTFFont *font = m_font;
    if (m_usingalt && m_altfont)
        font = m_altfont;

    font->DrawString(surface, x, y, text, maxx, maxy, alphamod);
} 

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

OSDTypeImage::OSDTypeImage(const QString &name, const QString &filename, 
                           QPoint displaypos, float wmult, float hmult, 
                           int scalew, int scaleh)
            : OSDType(name)
{
    m_drawwidth = -1;
    m_onlyusefirst = false;

    m_filename = filename;
    m_displaypos = displaypos;

    m_yuv = m_alpha = NULL;
    m_isvalid = false;
    m_imagesize = QRect(0, 0, 0, 0);

    m_scalew = scalew;
    m_scaleh = scaleh;
    m_wmult = wmult;
    m_hmult = hmult;

    LoadImage(filename, wmult, hmult, scalew, scaleh);
}

OSDTypeImage::OSDTypeImage(const OSDTypeImage &other)
            : OSDType(other.m_name)
{
    m_drawwidth = other.m_drawwidth;
    m_onlyusefirst = other.m_onlyusefirst;

    m_filename = other.m_filename;
    m_displaypos = other.m_displaypos;
    m_imagesize = other.m_imagesize;
    m_isvalid = other.m_isvalid;
    m_name = other.m_name;
    m_scalew = other.m_scalew;
    m_scaleh = other.m_scaleh;

    m_alpha = m_yuv = NULL;
    if (m_isvalid)
    {
        int size = m_imagesize.width() * m_imagesize.height() * 3 / 2;
        m_yuv = new unsigned char[size];
        memcpy(m_yuv, other.m_yuv, size);


        size = m_imagesize.width() * m_imagesize.height();
        m_alpha = new unsigned char[size];
        memcpy(m_alpha, other.m_alpha, size);

        m_ybuffer = m_yuv;
        m_ubuffer = m_yuv + (m_imagesize.width() * m_imagesize.height());
        m_vbuffer = m_yuv + (m_imagesize.width() * m_imagesize.height() * 
                             5 / 4);
    } 
}

OSDTypeImage::OSDTypeImage(const QString &name)
            : OSDType(name)
{
    m_drawwidth = -1;
    m_onlyusefirst = false;

    m_yuv = NULL;
    m_alpha = NULL;
    m_ybuffer = NULL;
    m_ubuffer = NULL;
    m_vbuffer = NULL;
    m_isvalid = false;
    m_filename = "";
}

OSDTypeImage::~OSDTypeImage()
{
    if (m_yuv)
        delete [] m_yuv;
    if (m_alpha)
        delete [] m_alpha;
}

void OSDTypeImage::Reinit(float wmult, float hmult)
{
    int x = (int)(m_displaypos.x() * wmult / m_wmult);
    int y = (int)(m_displaypos.y() * hmult / m_hmult);

    m_displaypos.setX(x);
    m_displaypos.setY(y);

    m_wmult = wmult;
    m_hmult = hmult;

    LoadImage(m_filename, m_wmult, m_hmult, m_scalew, m_scaleh);
}

void OSDTypeImage::LoadImage(const QString &filename, float wmult, float hmult,
                             int scalew, int scaleh)
{
    if (m_isvalid)
    {
        if (m_yuv)
            delete [] m_yuv;
        if (m_alpha)
            delete [] m_alpha;

        m_isvalid = false;
        m_yuv = NULL;
        m_alpha = NULL;
    }

    if (filename.length() < 2)
        return;

    QImage tmpimage(filename);

    if (tmpimage.width() == 0)
        return;

    if (scalew > 0 && m_scalew > 0)
        scalew = m_scalew;
    if (scaleh > 0 && m_scaleh > 0)
        scaleh = m_scaleh;

    int width = 0, height = 0;

    if (scalew > 0) 
        width = ((int)(scalew * wmult) / 2) * 2;
    else
        width = ((int)(tmpimage.width() * wmult) / 2) * 2;

    if (scaleh > 0)
        height = ((int)(scaleh * hmult) / 2) * 2;
    else
        height = ((int)(tmpimage.height() * hmult) / 2) * 2;

    QImage tmp2 = tmpimage.smoothScale(width, height);

    m_isvalid = true;

    m_yuv = new unsigned char[width * height * 3 / 2];
    m_ybuffer = m_yuv;
    m_ubuffer = m_yuv + (width * height);
    m_vbuffer = m_yuv + (width * height * 5 / 4);

    m_alpha = new unsigned char[width * height];

    rgb32_to_yuv420p(m_ybuffer, m_ubuffer, m_vbuffer, m_alpha, tmp2.bits(),
                     width, height, tmp2.width());

    m_imagesize = QRect(0, 0, width, height);
}

void OSDTypeImage::Draw(OSDSurface *surface, int fade, int maxfade, int xoff, 
                        int yoff)
{
    if (!m_isvalid)
        return;

    unsigned char *dest, *destalpha, *src, *srcalpha;
    unsigned char *udest, *vdest, *usrc, *vsrc;
    int alpha, iwidth, width;

    iwidth = width = m_imagesize.width();
    int height = m_imagesize.height();

    if (m_drawwidth >= 0)
        width = m_drawwidth;

    int ystart = m_displaypos.y();
    int xstart = m_displaypos.x();

    xstart += xoff;
    ystart += yoff;

    ystart = (ystart / 2) * 2;
    xstart = ((xstart + 1) / 2) * 2;

    if (height + ystart > surface->height)
        height = surface->height - ystart - 1;
    if (width + xstart > surface->width)
        width = surface->width - xstart - 1;

    if (width == 0 || height == 0)
        return;

    int startline = 0;
    int startcol = 0;

    if (ystart < 0)
    {
        startline = 0 - ystart;
        ystart = 0;
    }

    if (xstart < 0)
    {
        startcol = 0 - xstart;
        xstart = 0;
    }

    QRect destRect = QRect(xstart, ystart, width, height);
    bool needblend = false;

    if (m_onlyusefirst || surface->IntersectsDrawn(destRect))
        needblend = true;
    surface->AddRect(destRect);
    
    int ysrcwidth;
    int ydestwidth;

    int uvsrcwidth;
    int uvdestwidth;

    int alphamod = 255;
    bool transdest = false;

    int newalpha = 0;

    if (maxfade > 0 && fade >= 0)
        alphamod = (int)((((float)(fade) / maxfade) * 256.0) + 0.5);

    if (!needblend)
    {
        for (int y = startline; y < height; y++)
        {
            ysrcwidth = y * iwidth;
            ydestwidth = (y + ystart - startline) * surface->width;

            memcpy(surface->y + xstart + ydestwidth,
                   m_ybuffer + startcol + ysrcwidth, width);

            destalpha = surface->alpha + xstart + ydestwidth;

            for (int x = startcol; x < width; x++)
            {
                alpha = *(m_alpha + x + ysrcwidth);
  
                if (alpha == 0)
                    *destalpha = 0;
                else
                    *destalpha = ((alpha * alphamod) + 0x80) >> 8;

                destalpha++;
            }
        }

        iwidth /= 2;
        width /= 2;
        height /= 2;

        ystart /= 2;
        xstart /= 2;

        startline /= 2;
        startcol /= 2;

        for (int y = startline; y < height; y++)
        {
            uvsrcwidth = y * iwidth;
            uvdestwidth = (y + ystart - startline) * (surface->width / 2);

            memcpy(surface->u + xstart + uvdestwidth,
                   m_ubuffer + startcol + uvsrcwidth, width);
            memcpy(surface->v + xstart + uvdestwidth,
                   m_vbuffer + startcol + uvsrcwidth, width);
        }

        return;
    }

    int startingx = startcol;
    if (m_onlyusefirst)
        startingx = 0;

    // overlap with something we've already drawn..
    for (int y = startline; y < height; y++)
    {
        ysrcwidth = y * iwidth;
        ydestwidth = (y + ystart - startline) * surface->width;

        dest = surface->y + xstart + ydestwidth;
        destalpha = surface->alpha + xstart + ydestwidth;
        src = m_ybuffer + ysrcwidth + startingx;

        srcalpha = m_alpha + ysrcwidth + startingx;

        uvsrcwidth = ysrcwidth / 4;
        uvdestwidth = ydestwidth / 4;

        udest = surface->u + xstart / 2 + uvdestwidth;
        usrc = m_ubuffer + uvsrcwidth + startingx / 2;

        vdest = surface->v + xstart / 2 + uvdestwidth;
        vsrc = m_vbuffer + uvsrcwidth + startingx / 2;

        for (int x = startcol; x < width; x++)
        {
            alpha = *srcalpha;

            if (alpha == 0)
                goto imagedrawend;

            alpha = ((alpha * alphamod) + 0x80) >> 8;

            newalpha = alpha;

            if (*destalpha != 0)
            {
                transdest = false;
                newalpha = surface->pow_lut[alpha][*destalpha];
                *dest = blendColorsAlpha(*src, *dest, newalpha);
                *destalpha = *destalpha + ((alpha * (255 - *destalpha)) / 255);
            }
            else
            {
                transdest = true;
                *dest = *src;
                *destalpha = alpha;
            }

            if ((y % 2 == 0) && (x % 2 == 0))
            {
                if (!transdest)
                {
                    *udest = blendColorsAlpha(*usrc, *udest, newalpha);
                    *vdest = blendColorsAlpha(*vsrc, *vdest, newalpha);
                }
                else
                {
                    *udest = *usrc;
                    *vdest = *vsrc;
                }
            }

imagedrawend:
            if ((y % 2 == 0) && (x % 2 == 0))
            {
                udest++;
                vdest++;
                if (!m_onlyusefirst)
                {
                    usrc++;
                    vsrc++;
                }
            }

            dest++;
            destalpha++;
            if (!m_onlyusefirst)
            {
                src++;
                srcalpha++;
            }
        }
    }
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ 

OSDTypePosSlider::OSDTypePosSlider(const QString &name,
                                   const QString &filename,
                                   QRect displayrect, float wmult,
                                   float hmult, int scalew, int scaleh)
               : OSDTypeImage(name, filename, displayrect.topLeft(), wmult,
                              hmult, scalew, scaleh)
{
    m_maxval = 1000;
    m_curval = 0;
    m_displayrect = displayrect;
}

OSDTypePosSlider::~OSDTypePosSlider()
{
}

void OSDTypePosSlider::Reinit(float wchange, float hchange, float wmult,
                              float hmult)
{
    int width = (int)(m_displayrect.width() * wchange);
    int height = (int)(m_displayrect.height() * hchange);
    int x = (int)(m_displayrect.x() * wchange);
    int y = (int)(m_displayrect.y() * hchange);

    m_displayrect = QRect(x, y, width, height);

    OSDTypeImage::Reinit(wmult, hmult);
}

void OSDTypePosSlider::SetPosition(int pos)
{
    m_curval = pos;
    if (m_curval > 1000)
        m_curval = 1000;
    if (m_curval < 0)
        m_curval = 0;

    int xpos = (int)((m_displayrect.width() / 1000.0) * m_curval);
    int width = m_imagesize.width() / 2;

    xpos = m_displayrect.left() + xpos - width;

    m_displaypos.setX(xpos);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

OSDTypeFillSlider::OSDTypeFillSlider(const QString &name, 
                                     const QString &filename,
                                     QRect displayrect, float wmult, 
                                     float hmult, int scalew, int scaleh)
                 : OSDTypeImage(name, filename, displayrect.topLeft(), wmult, 
                                hmult, scalew, scaleh)
{
    m_maxval = 1000;
    m_curval = 0;
    m_drawwidth = 0;
    m_onlyusefirst = true;
    m_displayrect = displayrect;
}

OSDTypeFillSlider::~OSDTypeFillSlider()
{
}

void OSDTypeFillSlider::Reinit(float wchange, float hchange, float wmult,
                               float hmult)
{
    int width = (int)(m_displayrect.width() * wchange);
    int height = (int)(m_displayrect.height() * hchange);
    int x = (int)(m_displayrect.x() * wchange);
    int y = (int)(m_displayrect.y() * hchange);

    m_displayrect = QRect(x, y, width, height);

    OSDTypeImage::Reinit(wmult, hmult);
}

void OSDTypeFillSlider::SetPosition(int pos)
{
    m_curval = pos;
    if (m_curval > 1000)
        m_curval = 1000;
    if (m_curval < 0)
        m_curval = 0;

    m_drawwidth = (int)((m_displayrect.width() / 1000.0) * m_curval);
}

void OSDTypeFillSlider::Draw(OSDSurface *surface, int fade, int maxfade, 
                             int xoff, int yoff)
{
    if (!m_isvalid)
        return;

    OSDTypeImage::Draw(surface, fade, maxfade, xoff, yoff);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

OSDTypeEditSlider::OSDTypeEditSlider(const QString &name,
                                     const QString &bluefilename,
                                     const QString &redfilename,
                                     QRect displayrect, float wmult,
                                     float hmult, int scalew, int scaleh)
                 : OSDTypeImage(name)
{
    m_maxval = 1000;
    m_curval = 0;
    m_displayrect = displayrect;
    m_drawwidth = displayrect.width();

    m_drawMap = new int[m_drawwidth + 1];
    for (int i = 0; i < m_drawwidth; i++)
         m_drawMap[i] = 0;

    m_displaypos = displayrect.topLeft();
    
    m_yuv = m_alpha = NULL;
    m_isvalid = false;

    m_ryuv = m_ralpha = NULL;
    m_risvalid = false;

    m_redname = redfilename;
    m_bluename = bluefilename;

    m_scalew = scalew;
    m_scaleh = scaleh;

    LoadImage(m_redname, wmult, hmult, scalew, scaleh);
    if (m_isvalid)
    {
        m_risvalid = m_isvalid;
        m_ralpha = m_alpha;
        m_ryuv = m_yuv;
        m_rimagesize = m_imagesize;
        m_rybuffer = m_ybuffer;
        m_rubuffer = m_ubuffer;
        m_rvbuffer = m_vbuffer;

        m_isvalid = false;
        m_alpha = m_yuv = NULL;
    }

    LoadImage(m_bluename, wmult, hmult, scalew, scaleh);
}

OSDTypeEditSlider::~OSDTypeEditSlider()
{
    delete [] m_drawMap;

    if (m_ryuv)
        delete [] m_ryuv;
    if (m_ralpha)
        delete [] m_ralpha;
}

void OSDTypeEditSlider::Reinit(float wchange, float hchange, float wmult,
                               float hmult)
{
    int width = (int)(m_displayrect.width() * wchange);
    int height = (int)(m_displayrect.height() * hchange);
    int x = (int)(m_displayrect.x() * wchange);
    int y = (int)(m_displayrect.y() * hchange);

    m_displayrect = QRect(x, y, width, height);
    m_drawwidth = m_displayrect.width();

    delete [] m_drawMap;
    
    m_drawMap = new int[m_drawwidth + 1];
    for (int i = 0; i < m_drawwidth; i++)
         m_drawMap[i] = 0;

    m_displaypos = m_displayrect.topLeft();

    if (m_ryuv)
        delete [] m_ryuv;
    if (m_ralpha)
        delete [] m_ralpha;

    m_wmult = wmult;
    m_hmult = hmult;

    LoadImage(m_redname, wmult, hmult, m_scalew, m_scaleh);
    if (m_isvalid)
    {
        m_risvalid = m_isvalid;
        m_ralpha = m_alpha;
        m_ryuv = m_yuv;
        m_rimagesize = m_imagesize;
        m_rybuffer = m_ybuffer;
        m_rubuffer = m_ubuffer;
        m_rvbuffer = m_vbuffer;

        m_isvalid = false;
        m_alpha = m_yuv = NULL;
    }

    LoadImage(m_bluename, wmult, hmult, m_scalew, m_scaleh);
}

void OSDTypeEditSlider::ClearAll(void)
{
    for (int i = 0; i < m_drawwidth; i++)
        m_drawMap[i] = 0;
}

void OSDTypeEditSlider::SetRange(int start, int end)
{
    start = (int)((m_drawwidth / 1000.0) * start);
    end = (int)((m_drawwidth / 1000.0) * end);

    if (start < 0)
        start = 0;
    if (start >= m_drawwidth) 
        start = m_drawwidth - 1;
    if (end < 0)
        end = 0;
    if (end >= m_drawwidth)
        end = m_drawwidth - 1;

    if (end < start)
    {
        int tmp = start;
        start = end;
        end = tmp;
    }

    for (int i = start; i < end; i++)
        m_drawMap[i] = 1;
}

void OSDTypeEditSlider::Draw(OSDSurface *surface, int fade, int maxfade,
                             int xoff, int yoff)
{           
    if (!m_isvalid || !m_risvalid)
        return;
            
    unsigned char *dest, *destalpha, *src, *rsrc, *srcalpha, *rsrcalpha;
    unsigned char *udest, *vdest, *usrc, *rusrc, *vsrc, *rvsrc;
    int a; 
            
    int iwidth, riwidth, width;
    iwidth = m_imagesize.width();
    riwidth = m_rimagesize.width();
    width = m_drawwidth;
    int height = m_imagesize.height();

    int ystart = m_displaypos.y();
    int xstart = m_displaypos.x();
            
    xstart += xoff;
    ystart += yoff;

    ystart = (ystart / 2) * 2;
    xstart = (xstart / 2) * 2;

    if (height + ystart > surface->height)
        height = surface->height - ystart - 1;
    if (width + xstart > surface->width)
        width = surface->width - xstart - 1;

    if (width == 0 || height == 0)
        return;

    int startline = 0;
    int startcol = 0;

    if (ystart < 0)
    {
        startline = 0 - ystart;
        ystart = 0;
    }

    if (xstart < 0)
    {
        startcol = 0 - xstart;
        xstart = 0;
    }

    QRect destRect = QRect(xstart, ystart, width, height);
    surface->AddRect(destRect);
 
    int ysrcwidth;              
    int rysrcwidth;
    int ydestwidth;
   
    int uvsrcwidth;
    int ruvsrcwidth;
    int uvdestwidth;
 
    int alphamod = 255;
    bool transdest = false;
    int newalpha = 0;
    
    if (maxfade > 0 && fade >= 0)
        alphamod = (int)((((float)(fade) / maxfade) * 256.0) + 0.5);

    unsigned char *alpha;
    unsigned char *ybuf;
    unsigned char *ubuf;
    unsigned char *vbuf;

    for (int y = startline; y < height; y++)
    {
        ysrcwidth = y * iwidth; 
        rysrcwidth = y * riwidth;
        ydestwidth = (y + ystart - startline) * surface->width;

        dest = surface->y + xstart + ydestwidth;
        destalpha = surface->alpha + xstart + ydestwidth;
        src = m_ybuffer + ysrcwidth;
        rsrc = m_rybuffer + rysrcwidth;

        srcalpha = m_alpha + ysrcwidth;
        rsrcalpha = m_ralpha + rysrcwidth;

        uvdestwidth = ydestwidth / 4;
        uvsrcwidth = ysrcwidth / 4;
        ruvsrcwidth = rysrcwidth / 4;

        udest = surface->u + xstart / 2 + uvdestwidth;
        usrc = m_ubuffer + uvsrcwidth;
        rusrc = m_rubuffer + ruvsrcwidth;

        vdest = surface->v + xstart / 2 + uvdestwidth;
        vsrc = m_vbuffer + uvsrcwidth;
        rvsrc = m_rvbuffer + ruvsrcwidth;

        for (int x = startcol; x < width; x++)
        {
            if (m_drawMap[x] == 0)
            {
                alpha = srcalpha;
                ybuf = src;
                ubuf = usrc;
                vbuf = vsrc; 
            }
            else
            {
                alpha = rsrcalpha;
                ybuf = rsrc;
                ubuf = rusrc;
                vbuf = rvsrc;
            }

            if (*alpha == 0)
                goto editsliderdrawend;

            a = *alpha;

            a = ((a * alphamod) + 0x80) >> 8;
            newalpha = a;

            if (*destalpha != 0)
            {
                transdest = false;
                newalpha = surface->pow_lut[a][*destalpha];
                *dest = blendColorsAlpha(*ybuf, *dest, newalpha);
                *destalpha = *destalpha + ((a * (255 - *destalpha)) / 255);
            }
            else
            {
                transdest = true;
                *dest = *ybuf;
                *destalpha = a;
            }

            if ((y % 2 == 0) && (x % 2 == 0))
            {
                if (!transdest)
                {
                    *udest = blendColorsAlpha(*ubuf, *udest, newalpha);
                    *vdest = blendColorsAlpha(*vbuf, *vdest, newalpha);
                }
                else
                {
                    *udest = *ubuf;
                    *vdest = *vbuf;
                }
            }

editsliderdrawend:
            if ((y % 2 == 0) && (x % 2 == 0))
            {
                udest++;
                vdest++;
            }

            dest++;
            destalpha++;
        }
    }
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

OSDTypeBox::OSDTypeBox(const QString &name, QRect displayrect) 
          : OSDType(name)
{
    size = displayrect;
}

OSDTypeBox::OSDTypeBox(const OSDTypeBox &other)
          : OSDType(other.m_name)
{
    size = other.size;
}

OSDTypeBox::~OSDTypeBox()
{
}

void OSDTypeBox::Reinit(float wchange, float hchange)
{
    int width = (int)(size.width() * wchange);
    int height = (int)(size.height() * hchange);
    int x = (int)(size.x() * wchange);
    int y = (int)(size.y() * hchange);

    size = QRect(x, y, width, height);
}

void OSDTypeBox::Draw(OSDSurface *surface, int fade, int maxfade, int xoff, 
                      int yoff)
{
    unsigned char *dest, *destalpha;
    unsigned char alpha = 192;

    QRect disprect = size;
    disprect.moveBy(xoff, yoff);

    int ystart = disprect.top();
    int yend = disprect.bottom();
    int xstart = disprect.left();
    int xend = disprect.right();

    if (xstart < 0)
        xstart = 0;
    if (xend > surface->width)
        xend = surface->width;
    if (ystart < 0)
        ystart = 0;
    if (yend > surface->height)
        yend = surface->height;

    int height = yend - ystart + 1, width = xend - xstart + 1;

    QRect destRect = QRect(xstart, ystart, width, height); 
    bool needblend = false;

    if (surface->IntersectsDrawn(destRect))
        needblend = true;
    surface->AddRect(destRect);

    int alphamod = 255;
    if (maxfade > 0 && fade >= 0)
        alphamod = (int)((((float)(fade) / maxfade) * 256.0) + 0.5);

    alpha = ((alpha * alphamod) + 0x809) >> 8;
    int newalpha, ydestwidth;

    if (!needblend)
    {
        for (int y = ystart; y < yend; y++)
        {
            ydestwidth = y * surface->width;
            
            memset(surface->y + xstart + ydestwidth, 0, width);
            memset(surface->alpha + xstart + ydestwidth, alpha, width);
        }

        return;
    }

    for (int y = ystart; y < yend; y++)
    {
        dest = surface->y + y * surface->width + xstart;
        destalpha = surface->alpha + y * surface->width + xstart;

        for (int x = xstart; x < xend; x++)
        {
            if (*destalpha != 0)
            {
                newalpha = surface->pow_lut[alpha][*destalpha];
                *dest = blendColorsAlpha(0, *dest, newalpha);
                *destalpha = *destalpha + ((alpha * (255 - *destalpha)) / 255);
            }
            else
            {
                *dest = 0;
                *destalpha = alpha;
            }

            dest++;
            destalpha++;
        }
    }
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

OSDTypePositionIndicator::OSDTypePositionIndicator(void)
{
    m_numpositions = 0;
    m_curposition = -1;
    m_offset = 0;
}

OSDTypePositionIndicator::OSDTypePositionIndicator(
                                      const OSDTypePositionIndicator &other)
{
    m_numpositions = other.m_numpositions;
    m_curposition = other.m_curposition;
    m_offset = other.m_offset;
}

OSDTypePositionIndicator::~OSDTypePositionIndicator()
{
}

void OSDTypePositionIndicator::SetPosition(int pos)
{
    m_curposition = pos + m_offset;
    if (m_curposition >= m_numpositions)
        m_curposition = m_numpositions - 1;
}

void OSDTypePositionIndicator::PositionUp(void)
{
    if (m_curposition > m_offset)
        m_curposition--;
}

void OSDTypePositionIndicator::PositionDown(void)
{
    if (m_curposition < m_numpositions - 1)
        m_curposition++;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~


OSDTypePositionRectangle::OSDTypePositionRectangle(const QString &name)
                        : OSDType(name), OSDTypePositionIndicator()
{
}       
   
OSDTypePositionRectangle::OSDTypePositionRectangle(
                                        const OSDTypePositionRectangle &other) 
                        : OSDType(other.m_name), OSDTypePositionIndicator(other)
{
    for (int i = 0; i < m_numpositions; i++)
    {
        QRect tmp = other.positions[i];
        positions.push_back(tmp);
    }
}

OSDTypePositionRectangle::~OSDTypePositionRectangle()
{
}

void OSDTypePositionRectangle::Reinit(float wchange, float hchange)
{
    for (int i = 0; i < m_numpositions; i++)
    {
        QRect tmp = positions[i];

        int width = (int)(tmp.width() * wchange);
        int height = (int)(tmp.height() * hchange);
        int x = (int)(tmp.x() * wchange);
        int y = (int)(tmp.y() * hchange);

        positions[i].setX(x);
        positions[i].setY(y);
        positions[i].setWidth(width);
        positions[i].setHeight(height);
    }
}

void OSDTypePositionRectangle::AddPosition(QRect rect)
{
    positions.push_back(rect);
    m_numpositions++;
}

void OSDTypePositionRectangle::Draw(OSDSurface *surface, int fade, int maxfade, 
                                    int xoff, int yoff)
{
    fade = fade;
    maxfade = maxfade;
    xoff = xoff;
    yoff = yoff;

    if (m_curposition < 0 || m_curposition >= m_numpositions)
        return;

    QRect rect = positions[m_curposition];

    unsigned char *src;
    int ystart = rect.top();
    int yend = rect.bottom();
    int xstart = rect.left();
    int xend = rect.right();

    int height = yend - ystart + 1, width = xend - xstart + 1;

    QRect destRect = QRect(xstart, ystart, width, height);
    surface->AddRect(destRect);

    for (int y = ystart; y < yend; y++)
    {
        if (y < 0 || y >= surface->height)
            continue;

        for (int x = xstart; x < xstart + 2; x++)
        {
            if (x < 0 || x >= surface->width)
                continue;

            src = surface->y + x + y * surface->width;
            *src = 255;
        }

        for (int x = xend - 2; x < xend; x++)
        {
            if (x < 0 || x >= surface->width)
                continue;

            src = surface->y + x + y * surface->width;
            *src = 255;
        }
    }

    for (int x = xstart; x < xend; x++)
    {
        if (x < 0 || x >= surface->width)
            continue;

        for (int y = ystart; y < ystart + 2; y++)
        {
            if (y < 0 || y >= surface->height)
                continue;

            src = surface->y + x + y * surface->width;
            *src = 255;
        }
        for (int y = yend - 2; y < yend; y++)
        {
            if (y < 0 || y >= surface->height)
                continue;

            src = surface->y + x + y * surface->width;
            *src = 255;
        }
    }
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

OSDTypePositionImage::OSDTypePositionImage(const QString &name)
                    : OSDTypeImage(name), OSDTypePositionIndicator()
{
}

OSDTypePositionImage::OSDTypePositionImage(const OSDTypePositionImage &other)
                    : OSDTypeImage(other), OSDTypePositionIndicator(other)
{
    for (int i = 0; i < m_numpositions; i++)
    {
        QPoint tmp = other.positions[i];
        positions.push_back(tmp);
    }
}

OSDTypePositionImage::~OSDTypePositionImage()
{
}

void OSDTypePositionImage::Reinit(float wchange, float hchange, float wmult, 
                                  float hmult)
{
    OSDTypeImage::Reinit(wmult, hmult);

    for (int i = 0; i < m_numpositions; i++)
    {
        QPoint tmp = positions[i];

        int x = (int)(tmp.x() * wchange);
        int y = (int)(tmp.y() * hchange);

        positions[i].setX(x);
        positions[i].setY(y);
    }
}

void OSDTypePositionImage::AddPosition(QPoint pos)
{
    positions.push_back(pos);
    m_numpositions++;
}

void OSDTypePositionImage::Draw(OSDSurface *surface, int fade, int maxfade,
                                int xoff, int yoff)
{
    if (m_curposition < 0 || m_curposition >= m_numpositions)
        return;

    QPoint pos = positions[m_curposition];

    OSDTypeImage::SetPosition(pos);
    OSDTypeImage::Draw(surface, fade, maxfade, xoff, yoff);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

OSDTypeCC::OSDTypeCC(const QString &name, TTFFont *font, int xoff, int yoff,
                     int dispw, int disph)
         : OSDType(name)
{
    m_font = font;
    m_textlist = NULL;
    xoffset = xoff;
    yoffset = yoff;
    displaywidth = dispw;
    displayheight = disph;

    QRect rect = QRect(0, 0, 0, 0);
    m_box = new OSDTypeBox("cc_background", rect);
}

OSDTypeCC::~OSDTypeCC()
{
    ClearAllCCText();
    delete m_box;
}

void OSDTypeCC::Reinit(int x, int y, int dispw, int disph)
{
    xoffset = x;
    yoffset = y;
    displaywidth = dispw;
    displayheight = disph;
}

void OSDTypeCC::AddCCText(const QString &text, int x, int y, int color, 
                          bool teletextmode)
{
    ccText *cc = new ccText();
    cc->text = text;
    cc->x = x;
    cc->y = y;
    cc->color = color;
    cc->teletextmode = teletextmode;

    if (!m_textlist)
        m_textlist = new vector<ccText *>;

    m_textlist->push_back(cc);
}

void OSDTypeCC::ClearAllCCText()
{
    if (m_textlist)
    {
        vector<ccText*>::iterator i = m_textlist->begin();
        for (; i != m_textlist->end(); i++)
        {
            ccText *cc = (*i);
            if (cc)
                delete cc;
        }
        delete m_textlist;
        m_textlist = NULL;
    }
}

void OSDTypeCC::Draw(OSDSurface *surface, int fade, int maxfade, int xoff, 
                     int yoff)
{
    // not used
    fade = fade;
    maxfade = maxfade;
    xoff = xoff;
    yoff = yoff;

    vector<ccText*>::iterator i = m_textlist->begin();
    for (; i != m_textlist->end(); i++)
    {
        ccText *cc = (*i);

        if (cc && (cc->text != QString::null))
        {
            int textlength = 0;
            m_font->CalcWidth(cc->text, &textlength);

            int x, y;
            if (cc->teletextmode)
            {
                // position as if we use a fixed size font
                // on a 24 row / 40 char grid (teletext expects us to)
                x = cc->y * displaywidth / 40 + xoffset;
                y = cc->x * displayheight / 25 + yoffset;
            }
            else
            {
                x = (cc->x + 1) * displaywidth / 34 + xoffset;
                y = cc->y * displayheight / 18 + yoffset;
            }

            int maxx = x + textlength;
            int maxy = y + m_font->Size() * 3 / 2;

            if (maxx > surface->width)
                maxx = surface->width;

            if (maxy > surface->height)
                maxy = surface->height;

            QRect rect = QRect(0, 0, textlength + 4, 
                               (m_font->Size() * 3 / 2) + 3);
            m_box->SetRect(rect);
            m_box->Draw(surface, 0, 0, x - 2, y - 2);

            m_font->DrawString(surface, x, y + 2, cc->text, maxx, maxy, 255); 
        }
    }
}
