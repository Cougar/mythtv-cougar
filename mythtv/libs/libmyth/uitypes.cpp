
#include <math.h>

#include <iostream>
using namespace std;

#include "uitypes.h"

#include "mythcontext.h"

LayerSet::LayerSet(const QString &name)
{
    m_name = name;
    m_context = -1;
    m_debug = false;
    allTypes = new vector<UIType *>;
}

LayerSet::~LayerSet()
{
    vector<UIType *>::iterator i = allTypes->begin();
    for (; i != allTypes->end(); i++)
    {
        UIType *type = (*i);
        if (type)
            delete type;
    }
    delete allTypes;
}

void LayerSet::AddType(UIType *type)
{
    type->SetDebug(m_debug);
    typeList[type->Name()] = type;
    allTypes->push_back(type);
    type->SetParent(this);
}

UIType *LayerSet::GetType(const QString &name)
{
    UIType *ret = NULL;
    if (typeList.contains(name))
        ret = typeList[name];

    return ret;
}

void LayerSet::Draw(QPainter *dr, int drawlayer, int context)
{
  if (m_context == context || m_context == -1)
  {
    vector<UIType *>::iterator i = allTypes->begin();
    for (; i != allTypes->end(); i++)
    {
        if (m_debug == true)
            cerr << "-LayerSet::Draw\n";
        UIType *type = (*i);
        type->Draw(dr, drawlayer, context);
    }
  }
}

// **************************************************************

UIType::UIType(const QString &name)
{
    m_name = name;
    m_debug = false;
    m_context = -1;
    m_order = -1;
    has_focus = false;
    takes_focus = false;
}

UIType::~UIType()
{
}

void UIType::Draw(QPainter *dr, int drawlayer, int context)
{
    dr->end();
    drawlayer = 0;
    context = 0;
}

void UIType::SetParent(LayerSet *parent) 
{ 
    m_parent = parent; 
}

QString UIType::Name() 
{ 
    return m_name; 
}

bool UIType::takeFocus()
{
    if(takes_focus)
    {
        has_focus = true;
        return true;
    }
    has_focus = false;
    return false;
}

void UIType::looseFocus()
{
    has_focus = false;
}



// **************************************************************

UIBarType::UIBarType(const QString &name, QString imgfile,
                     int dorder, QRect displayrect)
         : UIType(name)
{
    m_name = name;

    m_filename = imgfile;

    m_displaysize = displayrect;
    m_order = dorder;
    m_justification = (Qt::AlignLeft | Qt::AlignVCenter);
  
    m_textoffset = QPoint(0, 0);
    m_iconoffset = QPoint(0, 0);

}

UIBarType::~UIBarType()
{
} 

void UIBarType::SetText(int num, QString myText)
{
    textData[num] = myText;
}

void UIBarType::SetIcon(int num, QString myIcon)
{
    LoadImage(num, myIcon);
}

void UIBarType::SetIcon(int num, QPixmap myIcon)
{
    QImage sourceImg = myIcon.convertToImage();
    if (!sourceImg.isNull())
    {
        QImage scalerImg;

        scalerImg = sourceImg.smoothScale((int)(m_iconsize.x()),
                                               (int)(m_iconsize.y()));
        iconData[num].convertFromImage(scalerImg);
    }
    else
        iconData[num].resize(0, 0);
}

void UIBarType::LoadImage(int loc, QString myFile)
{
    if (m_size == 0)
    {
        cerr << "uitypes.cpp:UIBarType:LoadImage:m_size == 0";
        return;
    }
    QString filename = m_filename;
    if (loc != -1)
        filename = myFile;

    QString file;

    QString baseDir = gContext->GetInstallPrefix();
    QString themeDir = gContext->FindThemeDir("");
    themeDir = themeDir + gContext->GetSetting("Theme") + "/";
    baseDir = baseDir + "/share/mythtv/themes/default/";

    QFile checkFile(themeDir + filename);

    if (checkFile.exists())
        file = themeDir + filename;
    else
        file = baseDir + filename;
    checkFile.setName(file);
    if (!checkFile.exists())
        file = "/tmp/" + filename;

    checkFile.setName(file);
    if (!checkFile.exists())
        file = filename;

    if (m_debug == true)
        cerr << "     -Filename: " << file << endl;

    QImage *sourceImg = new QImage();
    if (sourceImg->load(file))
    {
        QImage scalerImg;
        int doX = 0;
        int doY = 0;
        if (m_orientation == 1)
        {
            doX = (int)((m_displaysize.width() / m_size) * m_wmult);
            doY = (int)(m_displaysize.height() * m_hmult);
        }
        else if (m_orientation == 2)
        {
            doX = (int)(m_displaysize.width() * m_wmult);
            doY = (int)((m_displaysize.height() / m_size) * m_hmult);
        }
        if (loc != -1)
        {
            doX = m_iconsize.x();
            doY = m_iconsize.y();
        }

        scalerImg = sourceImg->smoothScale(doX, doY);
        if (loc == -1)
            m_image.convertFromImage(scalerImg);
        else
            iconData[loc].convertFromImage(scalerImg);

        if (m_debug == true)
            cerr << "     -Image: " << file << " loaded.\n";
    }
    else
    {
      if (m_debug == true)
          cerr << "     -Image: " << file << " failed to load.\n";
      iconData[loc].resize(0, 0);
    }

    delete sourceImg;

}

void UIBarType::Draw(QPainter *dr, int drawlayer, int context)
{
  if (m_context == context || m_context == -1)
  {
    if (drawlayer == m_order)
    {
      if (m_debug == true)
         cerr << "    +UIBarType::Size is " << m_size << endl;

      if (m_size < 0)
         cerr << "uitypes.cpp:UIBarType:Size is < 0!\n";
 
      int xdrop = 0;
      int ydrop = 0;
      int drawx = 0;
      int drawy = 0;

      if (m_orientation == 1)
      {
         xdrop = (int)(m_displaysize.width() / m_size);
         ydrop = m_displaysize.height();
      }
      else
      {
         xdrop = m_displaysize.width();
         ydrop = (int)(m_displaysize.height() / m_size);
      }

      for (int i = 0; i < m_size; i++)
      {
        if (m_debug == true) 
            cerr << "    +UIBarType::Drawing Item # " << i << endl;
        QPoint fontdrop = m_font->shadowOffset;
        QString msg = textData[i];

        if (m_orientation == 1)
        {
            drawx = m_displaysize.left() + (int)(i * xdrop);
            drawy = m_displaysize.top();
        }
        else
        {
            drawx = m_displaysize.left();
            drawy = m_displaysize.top() + (int)(i * ydrop);
        }
        dr->drawPixmap(drawx, drawy, m_image);
        if (!iconData[i].isNull() && iconData[i].width() > 0)
        {
            dr->drawPixmap(drawx + m_iconoffset.x(), 
                           drawy + (int)(ydrop / 2) - (int)(m_iconsize.y() / 2), 
                           iconData[i]);
        }

        dr->setFont(m_font->face);
        if (fontdrop.x() != 0 || fontdrop.y() != 0)
        {
            dr->setBrush(m_font->dropColor);
            dr->setPen(QPen(m_font->dropColor, (int)(2 * m_wmult)));
            if (m_orientation == 1)
            {
                drawx = m_displaysize.left() + fontdrop.x() + (int)(i * xdrop);
                drawy = m_displaysize.top();
            }
            else
            {
                drawx = m_displaysize.left();
                drawy = m_displaysize.top() + fontdrop.y() + (int)(i * ydrop);
            }
            if (m_debug == true)
                cerr << "    +UIBarType::Drawing Shadow @ (" << drawx 
                     << ", " << drawy << ")" << endl;
            dr->drawText(drawx + m_textoffset.x(), drawy + m_textoffset.y(),
                           xdrop - m_textoffset.x(), ydrop - (int)(2 * m_textoffset.y()),
                           m_justification, msg);
        }

        dr->setBrush(m_font->color);
        dr->setPen(QPen(m_font->color, (int)(2 * m_wmult)));
        if (m_orientation == 1)
        {
            drawx = m_displaysize.left() + (int)(i * xdrop);
            drawy = m_displaysize.top();
        }
        else
        {
            drawx = m_displaysize.left();
            drawy = m_displaysize.top() + (int)(i * ydrop);
        }
        if (m_debug == true) 
        {
            cerr << "    +UIBarType::Drawing @ (" << drawx << ", " << drawy << ")" << endl;
            cerr << "     +UIBarType::Data = " << msg << endl;
        }
        dr->drawText(drawx + m_textoffset.x(), drawy + m_textoffset.y(),
                     xdrop - m_textoffset.x(), ydrop - (int)(2 * m_textoffset.y()),
                     m_justification, msg);

        if (m_debug == true)
            cerr << "   +UIBarType::Draw() <- inside Layer\n";
      }
    }
    else
        if (m_debug == true)
        {
             cerr << "   +UIBarType::Draw() <- outside (layer = " << drawlayer
                  << ", widget layer = " << m_order << "\n";
        }      
  }
}

// **************************************************************

UIGuideType::UIGuideType(const QString &name, int order)
           : UIType(name)
{
    m_name = name;
    m_order = order;

    m_cutdown = true;
    m_count = 0;
    m_justification = (Qt::AlignLeft | Qt::AlignTop);
    m_textoffset = QPoint(0, 0);


    m_area = QRect(0, 0, 0, 0);
    m_screenloc = QPoint(0, 0);
    m_window = NULL;
    m_font = NULL;
    m_solidcolor = "";
    m_selcolor = "";
    m_seltype = 1;
    m_reccolor = "";
    m_filltype = 6;
    curArea = QRect(0, 0, 0, 0);
    m_count = 0;
    countMap.clear();
    categoryColors.clear();
    drawArea.clear();
    dataMap.clear();
    categoryMap.clear();
    recStatus.clear();
    arrowUsage.clear();
}

UIGuideType::~UIGuideType()
{
}

void UIGuideType::Draw(QPainter *dr, int drawlayer, int context)
{
  if (m_context == context || m_context == -1)
  {
    if (drawlayer == m_order)
    {
        if (m_count == 0)
        {
            cout << "uitypes.cpp:UIGuideType:m_count == 0\n";
            return;
        }
        if (m_debug)
            cerr << "UIGuideType::Draw\n";
 
        typedef QMap<int, QString> DataMap;
        QString msg = "";
        DataMap::Iterator it;
        DataMap::Iterator start;
        DataMap::Iterator end;
        int i = -1;
 
        start = dataMap.begin();
        end = dataMap.end();
        for (it = start; it != end; ++it)
        {
            i = it.key();

            if (recStatus[i] == 0)
                drawBackground(dr, i);
            else
                drawBox(dr, i, m_reccolor);

            drawText(dr, i);
            drawRecStatus(dr, i);
        }
        drawCurrent(dr);
    }
  }
}

QString UIGuideType::cutDown(QString info, QFont *testFont, int maxwidth, int maxheight)
{
    QFontMetrics fm(*testFont);
    QRect curFontSize;

    curFontSize = fm.boundingRect(0, 0, maxwidth, maxheight, m_justification, info);

    if (curFontSize.height() > maxheight)
    {
        QString testInfo = info;
        curFontSize = fm.boundingRect(0, 0, maxwidth, maxheight, m_justification, testInfo);
        int count = info.length();

        while (curFontSize.height() >= maxheight)
        {
            testInfo = info.left(count);
            curFontSize = fm.boundingRect(0, 0, maxwidth, maxheight, m_justification, testInfo);
            count = count--;
            if (count < 0)
                break;
        }
        testInfo = testInfo + "...";
        info = testInfo;
    }
    else if (curFontSize.width() > maxwidth)
    {
        int curFontWidth = fm.width(info);
        if (curFontWidth > maxwidth)
        {
            QString testInfo = "";
            curFontWidth = fm.width(testInfo);
            int tmaxwidth = maxwidth - fm.width("LLL");
            int count = 0;

            while (curFontWidth < tmaxwidth)
            {
                testInfo = info.left(count);
                curFontWidth = fm.width(testInfo);
                count = count + 1;
            }
            testInfo = testInfo + "...";
            info = testInfo;
        }
    }
    return info;

}

void UIGuideType::drawCurrent(QPainter *dr)
{
    int num = 0;
    int breakin = 1;
    QRect area = curArea;

    area.setTop(area.top() + breakin);
    area.setLeft(area.left() + breakin);
    area.setHeight(area.height() - (int)(2*breakin));
    area.setWidth(area.width() - (int)(2*breakin));

  if (m_seltype == 2)
  {
    if (m_filltype == 1)
    {
        dr->setBrush( QBrush( QColor(m_selcolor), Qt::Dense4Pattern) );
        dr->setPen(QPen(QColor(m_selcolor), (int)(2 * m_wmult)));
        dr->drawRoundRect(area);
    }
    else if (m_filltype == 2)
    {
        dr->setBrush(QColor(m_selcolor));
        dr->setPen(QPen(QColor(m_selcolor), (int)(2 * m_wmult)));
        dr->drawRoundRect(area);
    }
    else if (m_filltype == 3)
    {
        dr->setBrush( QBrush( QColor(m_selcolor), Qt::Dense4Pattern) );
        dr->setPen(QPen(QColor(m_selcolor), (int)(2 * m_wmult)));
        dr->fillRect(area, QBrush( QColor(m_selcolor), Qt::Dense4Pattern));
    }
    else if (m_filltype == 4)
    {
        dr->setBrush( QBrush(QColor(m_selcolor)) );
        dr->setPen(QPen(QColor(m_selcolor), (int)(2 * m_wmult)));
        dr->fillRect(area, QBrush(QColor(m_selcolor)));
    }
    else if (m_filltype == 5)
    {
        dr->setBrush(QBrush(m_selcolor));
        dr->setPen(QPen(QColor(m_selcolor), (int)(2 * m_wmult)));
        dr->fillRect(area, QBrush(m_selcolor));
    }
    else if (m_filltype == 6)
        Blender(dr, area, num, m_selcolor);
  }
  else
  {
    dr->setBrush(QBrush(m_selcolor));
    dr->setPen(QPen(QColor(m_selcolor), (int)(2 * m_wmult)));
    dr->drawLine(area.left(), area.top(), area.right(), area.top());
    dr->drawLine(area.left(), area.top(), area.left(), area.bottom());
    dr->drawLine(area.left(), area.bottom(), area.right(), area.bottom());
    dr->drawLine(area.right(), area.top(), area.right(), area.bottom());

    dr->drawLine(area.left(), area.top() + 1, area.right(), area.top() + 1);
    dr->drawLine(area.left() + 1, area.top(), area.left() + 1, area.bottom());
    dr->drawLine(area.left(), area.bottom() - 1, area.right(), area.bottom() - 1);
    dr->drawLine(area.right() - 1, area.top(), area.right() - 1, area.bottom());
  }
 }

void UIGuideType::drawRecStatus(QPainter *dr, int num)
{
    int breakin = 2;
    QRect area = drawArea[num];
    area.setTop(area.top() + breakin);
    area.setLeft(area.left() + breakin);
    area.setHeight(area.height() - (int)(2*breakin));
    area.setWidth(area.width() - (int)(2*breakin));
 
    if (recStatus[num] != 0)
    {
        QPixmap recImg = recImages[recStatus[num]];

        dr->drawPixmap(area.right() - recImg.width(), 
                       area.bottom() - recImg.height(), recImg);
    }
    if (arrowUsage[num] != 0)
    {
        QPixmap user;
        if (arrowUsage[num] == 1 || arrowUsage[num] == 3)
        {
            user = arrowImages[0];

             dr->drawPixmap(area.left(), 
                            area.top() + (int)(area.height() / 2) - (int)(user.height() / 2), 
                            user);
        }
        if (arrowUsage[num] == 2 || arrowUsage[num] == 3)
        {
            user = arrowImages[1];

            dr->drawPixmap(area.right() - user.width(),
                            area.top() + (int)(area.height() / 2) - (int)(user.height() / 2),
                            user);

        }
    }
}

void UIGuideType::drawBox(QPainter *dr, int num, QString color)
{
    int breakin = 1;
    QRect area = drawArea[num];
    area.setTop(area.top() + breakin);
    area.setLeft(area.left() + breakin);
    area.setHeight(area.height() - (int)(2*breakin));
    area.setWidth(area.width() - (int)(2*breakin));

    if (m_filltype == 1)
    {
        dr->setBrush( QBrush( QColor(color), Qt::Dense4Pattern) );
        dr->setPen(QPen(QColor(color), (int)(2 * m_wmult)));
        dr->drawRoundRect(area);
    }
    else if (m_filltype == 2)
    {
        dr->setBrush(QColor(color));
        dr->setPen(QPen(QColor(color), (int)(2 * m_wmult)));
        dr->drawRoundRect(area);
    }
    else if (m_filltype == 3)
    {
        dr->setBrush( QBrush( QColor(color), Qt::Dense4Pattern) );
        dr->setPen(QPen(QColor(color), (int)(2 * m_wmult)));
        dr->fillRect(area, QBrush( QColor(color), Qt::Dense4Pattern));
    }
    else if (m_filltype == 4)
    {
        dr->setBrush( QBrush(QColor(color)) );
        dr->setPen(QPen(QColor(color), (int)(2 * m_wmult)));
        dr->fillRect(area, QBrush(QColor(color)));
    }
    else if (m_filltype == 5)
    {
        dr->setBrush(QBrush(color));
        dr->setPen(QPen(QColor(color), (int)(2 * m_wmult)));
        dr->fillRect(area, QBrush(color));
    }
    else if (m_filltype == 6)
        Blender(dr, area, num, color);
 }

void UIGuideType::drawBackground(QPainter *dr, int num)
{
    int breakin = 1;
    QRect area = drawArea[num];
    area.setTop(area.top() + breakin);
    area.setLeft(area.left() + breakin);
    area.setHeight(area.height() - (int)(2*breakin));
    area.setWidth(area.width() - (int)(2*breakin));

    if (m_filltype == 1)
    {
        dr->setBrush( QBrush( QColor(m_solidcolor), Qt::Dense4Pattern) );
        dr->setPen(QPen(QColor(m_solidcolor), (int)(2 * m_wmult)));
        dr->drawRoundRect(area);
    }
    else if (m_filltype == 2)
    {
        dr->setBrush(QColor(m_solidcolor));
        dr->setPen(QPen(QColor(m_solidcolor), (int)(2 * m_wmult)));
        dr->drawRoundRect(area);
    }
    else if (m_filltype == 3)
    {
        dr->setBrush( QBrush( QColor(m_solidcolor), Qt::Dense4Pattern) );
        dr->setPen(QPen(QColor(m_solidcolor), (int)(2 * m_wmult)));
        dr->fillRect(area, QBrush( QColor(m_solidcolor), Qt::Dense4Pattern));

        dr->setBrush( QBrush(QColor("#0000ee")) );
        dr->setPen(QPen(QColor("#0000ee"), (int)(2 * m_wmult)));
        dr->drawLine(area.left() - 1, area.top() - 1, area.right() + 1, area.top() - 1);
        dr->drawLine(area.left(), area.top(), area.right(), area.top());

        dr->setBrush( QBrush(QColor("#0000aa")) );
        dr->setPen(QPen(QColor("#0000aa"), (int)(2 * m_wmult)));
        dr->drawLine(area.left() - 1, area.top(), area.left() - 1, area.bottom());
        dr->drawLine(area.left(), area.top() + 1, area.left(), area.bottom() - 1);

        dr->setBrush( QBrush(QColor("#000033")) );
        dr->setPen(QPen(QColor("#000033"), (int)(2 * m_wmult)));
        dr->drawLine(area.left() - 1, area.bottom() + 1, area.right() + 1, area.bottom() + 1);
        dr->drawLine(area.left(), area.bottom(), area.right(), area.bottom());
        dr->drawLine(area.right() + 1, area.top() + 1, area.right() + 1, area.bottom());
        dr->drawLine(area.right(), area.top(), area.right(), area.bottom());
    }
    else if (m_filltype == 4)
    {
        dr->setBrush( QBrush(QColor(m_solidcolor)) );
        dr->setPen(QPen(QColor(m_solidcolor), (int)(2 * m_wmult)));
        dr->fillRect(area, QBrush(QColor(m_solidcolor)));

        dr->setBrush( QBrush(QColor("#0000bb")) );
        dr->setPen(QPen(QColor("#0000bb"), (int)(2 * m_wmult)));

        dr->drawLine(area.left() - 1, area.top() - 1, area.right() + 1, area.top() - 1);

        dr->setBrush( QBrush(QColor("#000099")) );
        dr->setPen(QPen(QColor("#000099"), (int)(2 * m_wmult)));
        dr->drawLine(area.left() - 1, area.top(), area.left() - 1, area.bottom());

        dr->setBrush( QBrush(QColor("#000033")) );
        dr->setPen(QPen(QColor("#000033"), (int)(2 * m_wmult)));
        dr->drawLine(area.left() - 1, area.bottom() + 1, area.right() + 1, area.bottom() + 1);
        dr->drawLine(area.right() + 1, area.top() + 1, area.right() + 1, area.bottom());

    }
    else if (m_filltype == 5)
    {
        dr->setBrush(QBrush(categoryColors[categoryMap[num]], Qt::Dense4Pattern));
        dr->setPen(QPen(QColor(categoryColors[categoryMap[num]]), (int)(2 * m_wmult)));
        dr->fillRect(area, QBrush( QColor(categoryColors[categoryMap[num]]), Qt::Dense4Pattern));
    }
    else if (m_filltype == 6 && categoryMap[num] != "Other")
         Blender(dr, area, num);
}

void UIGuideType::Blender(QPainter *dr, QRect area, int num, QString force)
{
    if (!m_window)
    {
        cout << "UIGuideType:Blender:Error! m_window undefined, set with SetWindow(MythDialog *)\n";
        return;
    }
    QString color = categoryColors[categoryMap[num]];
    if (force.length() > 0)
        color = force;
    int alpha = 96;
    unsigned int *data = NULL;

    QBrush br = QBrush(color);
    QRgb blendcolor = br.color().rgb();
    blendcolor = qRgba(qRed(blendcolor), qGreen(blendcolor),
                 qBlue(blendcolor), alpha);
 
    QPixmap orig(area.width() + 1, area.height() + 1);
    orig.fill(m_window, m_screenloc.x() + area.left(), 
                        m_screenloc.y() + area.top());

    QImage bgimage = orig.convertToImage();

    for (int tmpy = 0; tmpy <= area.height(); tmpy++)
    {
         data = (unsigned int *)bgimage.scanLine(tmpy);
         for (int tmpx = 0; tmpx <= area.width(); tmpx++)
         {
              QRgb pixelcolor = data[tmpx];
              data[tmpx] = blendColors(pixelcolor, blendcolor,
                           alpha);
         }
    }

    dr->drawImage(area.left(), area.top(), bgimage);
}

void UIGuideType::drawText(QPainter *dr, int num)
{
    QRect area = drawArea[num];
    QString msg;
    if (categoryMap[num].stripWhiteSpace().length() > 0)
        msg = dataMap[num] + " (" + categoryMap[num] + ")";
    else  
        msg = dataMap[num];

    QPoint fontdrop = m_font->shadowOffset;

    area.setLeft(area.left() + m_textoffset.x());
    area.setTop(area.top() + m_textoffset.y());
    area.setHeight(area.height() - m_textoffset.x());
    area.setWidth(area.width() - m_textoffset.y());

    if (arrowUsage[num] == 1 || arrowUsage[num] == 3)
    {
        area.setLeft(area.left() + arrowImages[0].width());
    }
    if (arrowUsage[num] == 2 || arrowUsage[num] == 3)
    {
        area.setRight(area.right() - arrowImages[1].width());
    }
    

    if (m_cutdown == true)
        msg = cutDown(msg, &(m_font->face), area.width(), area.height());
    if (m_cutdown == true && m_debug == true)
        cerr << "    +UIGuideType::CutDown Called.\n";

    dr->setFont(m_font->face);
    if (fontdrop.x() != 0 || fontdrop.y() != 0)
    {
        if (m_debug == true)
        cerr << "    +UIGuideType::Drawing shadow @ (" 
             << area.left() + fontdrop.x() << ", "
             << area.top() + fontdrop.y() << ")\n";
        dr->setBrush(m_font->dropColor);
        dr->setPen(QPen(m_font->dropColor, (int)(2 * m_wmult)));
        dr->drawText(area.left() + fontdrop.x(),
                     area.top() + fontdrop.y(),
                     area.width(),
                     area.height(), m_justification, msg);
    }

    dr->setBrush(m_font->color);
    dr->setPen(QPen(m_font->color, (int)(2 * m_wmult)));
    if (m_debug == true)
        cerr << "    +UIGuideType::Drawing @ ("
             << area.left() << ", " << area.top()
             << ")" << endl;

 
    if (m_debug == true)
        cerr << "    +UIGuideType::Data = " << msg << endl;

    dr->drawText(area.left(), area.top(), area.width(), area.height(),
                 m_justification, msg);
 
}

void UIGuideType::ResetRow(unsigned int row)
{
    if (!countMap.contains(row))
        return;

    countMap[row] = -1;
    for (unsigned int i = (unsigned int)(row * 100); 
         i < (unsigned int)((unsigned int)(row * 100) + 99); i++)
    {
        if (!dataMap.contains(i))
            return;
        dataMap.remove(i);
        recStatus.remove(i);
        drawArea.remove(i);
        categoryMap.remove(i);
    }
}

void UIGuideType::SetProgramInfo(unsigned int row, int num, QRect area, 
                                 QString title, QString genre, int arrow, int recFlag)
{
    if (num > countMap[row])
        countMap[row] = num;
    dataMap[(int)(row * 100) + num] = title;
    recStatus[(int)(row * 100) + num] = recFlag;
    drawArea[(int)(row * 100) + num] = area;
    categoryMap[(int)(row * 100) + num] = genre;
    arrowUsage[(int)(row * 100) + num] = arrow;
    if (row > m_count)
        m_count = row;
}

void UIGuideType::LoadImage(int recType, QString file)
{
    QString tfile = "";
    QString baseDir = gContext->GetInstallPrefix();
    QString themeDir = gContext->FindThemeDir("");
    themeDir = themeDir + gContext->GetSetting("Theme") + "/";
    baseDir = baseDir + "/share/mythtv/themes/default/";

    QFile checkFile(themeDir + file);

    if (checkFile.exists())
        tfile = themeDir + file;
    else
        tfile = baseDir + file;
    checkFile.setName(tfile);
    if (!checkFile.exists())
        tfile = "/tmp/" + file;

    checkFile.setName(tfile);
    if (!checkFile.exists())
        tfile = file;

    if (m_debug == true)
        cerr << "     -Filename: " << file << endl;

    QPixmap img;
    if (m_hmult == 1 && m_wmult == 1)
    {
        img.load(tfile);
    }
    else
    {
        QImage *sourceImg = new QImage();
        if (sourceImg->load(tfile))
        {
            QImage scalerImg;
            int doX = sourceImg->width();
            int doY = sourceImg->height();

            scalerImg = sourceImg->smoothScale((int)(doX * m_wmult),
                                              (int)(doY * m_hmult));
            img.convertFromImage(scalerImg);
            if (m_debug == true)
                    cerr << "     -Image: " << file << " loaded.\n";
        }
        else
        {
            if (m_debug == true)
                cerr << "     -Image: " << file << " failed to load.\n";
        }
        delete sourceImg;
    }
    recImages[recType] = img;
}

void UIGuideType::SetArrow(int dir, QString file)
{
    QString tfile = "";
    QString baseDir = gContext->GetInstallPrefix();
    QString themeDir = gContext->FindThemeDir("");
    themeDir = themeDir + gContext->GetSetting("Theme") + "/";
    baseDir = baseDir + "/share/mythtv/themes/default/";

    QFile checkFile(themeDir + file);

    if (checkFile.exists())
        tfile = themeDir + file;
    else
        tfile = baseDir + file;
    checkFile.setName(tfile);
    if (!checkFile.exists())
        tfile = "/tmp/" + file;

    checkFile.setName(tfile);
    if (!checkFile.exists())
        tfile = file;

    if (m_debug == true)
        cerr << "     -Filename: " << file << endl;

    QPixmap img;
    if (m_hmult == 1 && m_wmult == 1)
    {
        img.load(tfile);
    }
    else
    {
        QImage *sourceImg = new QImage();
        if (sourceImg->load(tfile))
        {
            QImage scalerImg;
            int doX = sourceImg->width();
            int doY = sourceImg->height();

            scalerImg = sourceImg->smoothScale((int)(doX * m_wmult),
                                              (int)(doY * m_hmult));
            img.convertFromImage(scalerImg);
            if (m_debug == true)
                    cerr << "     -Image: " << file << " loaded.\n";
        }
        else
        {
            if (m_debug == true)
                cerr << "     -Image: " << file << " failed to load.\n";
        }
        delete sourceImg;
    }
    arrowImages[dir] = img;
}

// **************************************************************


UIListType::UIListType(const QString &name, QRect area, int dorder)
          : UIType(name)
{
    m_name = name;
    m_area = area;
    m_order = dorder;
    m_active = false;
    m_columns = 0;
    m_current = -1;
    m_count = 0;
    m_justification = 0;
    m_uarrow = false;
    m_darrow = false;
    m_fill_type = -1;
}

UIListType::~UIListType()
{
}

void UIListType::Draw(QPainter *dr, int drawlayer, int context)
{
  if (m_context == context || m_context == -1)
  {
    if (drawlayer == m_order)
    {
        if (m_fill_type == 1 && m_active == true)
            dr->fillRect(m_fill_area, QBrush(m_fill_color, Qt::Dense4Pattern));

        QString tempWrite;
            int left = 0;
        fontProp *tmpfont = NULL;
        if (m_active == true)
            tmpfont = &m_fontfcns[m_fonts["active"]];
        else
            tmpfont = &m_fontfcns[m_fonts["inactive"]];
        bool lastShown = true;
        QPoint fontdrop = tmpfont->shadowOffset;

        dr->setFont(tmpfont->face);

        if (m_debug == true)
            cerr << "   +UIListType::Draw() <- within Layer\n";

        for (int i = 0; i < m_count; i++)
        {
            left = m_area.left();
            for (int j = 1; j <= m_columns; j++)
            {
                if (j > 1 && lastShown == true)
                    left = left + columnWidth[j - 1] + m_pad;

                if (m_debug == true)
                 {
                    cerr << "      -Column #" << j << ", Column Context: " << columnContext[j]
                         << ", Draw Context: " << context << endl;
                }
                if (columnContext[j] != context && columnContext[j] != -1)
                {
                    lastShown = false;
                }
                else
                {
                    tempWrite = listData[i + (int)(100*j)];

                    if (tempWrite != "***FILLER***")
                    {
                        if (columnWidth[j] > 0)
                            tempWrite = cutDown(tempWrite, &(tmpfont->face), columnWidth[j]);
                    }
                    else
                        tempWrite = "";

                    if (fontdrop.x() != 0 || fontdrop.y() != 0)
                    {
                        dr->setBrush(tmpfont->dropColor);
                        dr->setPen(QPen(tmpfont->dropColor, (int)(2 * m_wmult)));
                        dr->drawText((int)(left + fontdrop.x()),
                                     (int)(m_area.top() + (int)(i*m_selheight) + fontdrop.y()),
                                     m_area.width(), m_selheight, m_justification, tempWrite);
                    }
                    dr->setBrush(tmpfont->color);
                    dr->setPen(QPen(tmpfont->color, (int)(2 * m_wmult)));
                    if (forceFonts[i] != "")
                    {
                        dr->setFont(m_fontfcns[forceFonts[i]].face);
                        QColor myColor = m_fontfcns[forceFonts[i]].color;
                        dr->setBrush(myColor);
                        dr->setPen(QPen(myColor, (int)(2 * m_wmult)));
                    }
                    dr->drawText(left, m_area.top() + (int)(i*m_selheight),
                                 m_area.width(), m_selheight, m_justification, tempWrite);
                    dr->setFont(tmpfont->face);
                    if (m_debug == true)
                        cerr << "   +UIListType::Draw() Data: " << tempWrite << "\n";
                    lastShown = true;
                 }
              }
          }

          if (m_uarrow == true)
              dr->drawPixmap(m_uparrow_loc, m_uparrow);
          if (m_darrow == true)
              dr->drawPixmap(m_downarrow_loc, m_downarrow);
    }
    else if (drawlayer == 8 && m_current >= 0)
    {
        QString tempWrite;
        int left = 0;
        int i = m_current;
        fontProp *tmpfont = NULL;
        if (m_active == true)
            tmpfont = &m_fontfcns[m_fonts["selected"]];
        else
            tmpfont = &m_fontfcns[m_fonts["inactive"]];
        bool lastShown = true;
        QPoint fontdrop = tmpfont->shadowOffset;

        dr->setFont(tmpfont->face);
        dr->drawPixmap(m_area.left() + m_selection_loc.x(),
                         m_area.top() + m_selection_loc.y() + (int)(m_current * m_selheight),
                         m_selection);

        left = m_area.left();
        for (int j = 1; j <= m_columns; j++)
        {
             if (j > 1 && lastShown == true)
                    left = left + columnWidth[j - 1] + m_pad;

             if (m_debug == true)
             {
                 cerr << "      -Column #" << j << ", Column Context: " << columnContext[j]
                      << ", Draw Context: " << context << endl;
             }
             if (columnContext[j] != context && columnContext[j] != -1)
             {
                 lastShown = false;
             }
             else
             {
                 tempWrite = listData[i + (int)(100*j)];

                 if (columnWidth[j] > 0)
                     tempWrite = cutDown(tempWrite, &(tmpfont->face), columnWidth[j]);
        
                 if (fontdrop.x() != 0 || fontdrop.y() != 0)
                 {
                     dr->setBrush(tmpfont->dropColor);
                     dr->setPen(QPen(tmpfont->dropColor, (int)(2 * m_wmult)));
                     dr->drawText((int)(left + fontdrop.x()),
                                  (int)(m_area.top() + (int)(i*m_selheight) + fontdrop.y()),
                                  m_area.width(), m_selheight, m_justification, tempWrite);
                 }
                 dr->setBrush(tmpfont->color);
                 dr->setPen(QPen(tmpfont->color, (int)(2 * m_wmult)));
                 if (forceFonts[i] != "")
                 {   
                     dr->setFont(m_fontfcns[forceFonts[i]].face);
                     QColor myColor = m_fontfcns[forceFonts[i]].color;
                     dr->setBrush(myColor);
                     dr->setPen(QPen(myColor, (int)(2 * m_wmult)));
                 }

                 dr->drawText(left, m_area.top() + (int)(i*m_selheight),
                              m_area.width(), m_selheight, m_justification, tempWrite);

                 dr->setFont(tmpfont->face);
             }
        }
    }
    else
    {
        if (m_debug == true)
            cerr << "   +UIListType::Draw() <- outside (layer = " << drawlayer
                 << ", widget layer = " << m_order << "\n";
    }
  }
}

QString UIListType::cutDown(QString info, QFont *testFont, int maxwidth)
{
    QFontMetrics fm(*testFont);

    int curFontWidth = fm.width(info);
    if (curFontWidth > maxwidth)
    {
        QString testInfo = "";
        curFontWidth = fm.width(testInfo);
        int tmaxwidth = maxwidth - fm.width("LLL");
        int count = 0;

        while (curFontWidth < tmaxwidth)
        {
            testInfo = info.left(count);
            curFontWidth = fm.width(testInfo);
            count = count + 1;
        }
        testInfo = testInfo + "...";
        info = testInfo;
    }
    return info;

}

void UIListType::SetItemText(int num, int column, QString data)
{
    if (column > m_columns)
        m_columns = column;
    listData[(int)(num + (int)(column * 100))] = data;
}

QString UIListType::GetItemText(int num, int column)
{
    QString ret;
    ret = listData[(int)(num + (int)(column * 100))];
    return ret;
}

void UIListType::SetItemText(int num, QString data)
{
    m_columns = 1;
    listData[num + 100] = data;
}

// *****************************************************************

UIImageType::UIImageType(const QString &name, const QString &filename, int dorder, QPoint displaypos)
           : UIType(name)
{
    m_isvalid = false;
    m_flex = false;
    img = QPixmap();

    m_filename = filename;
    m_displaypos = displaypos;
    m_order = dorder;
    m_force_x = -1;
    m_force_y = -1;
    m_drop_x = 0;
    m_drop_y = 0;
    m_show = false;
}

UIImageType::~UIImageType()
{
}

void UIImageType::LoadImage()
{
    QString file;
    int transparentFlag = gContext->GetNumSetting("PlayBoxTransparency", 1);
    if (m_flex == true)
    {
        if (transparentFlag == 1)
            m_filename = "trans-" + m_filename;
        else
            m_filename = "solid-" + m_filename;
    }

    QString baseDir = gContext->GetInstallPrefix();
    QString themeDir = gContext->FindThemeDir("");
    themeDir = themeDir + gContext->GetSetting("Theme") + "/";
    baseDir = baseDir + "/share/mythtv/themes/default/";

    QFile checkFile(themeDir + m_filename);

    if (checkFile.exists())
        file = themeDir + m_filename;
    else
        file = baseDir + m_filename;
    checkFile.setName(file);
    if (!checkFile.exists())
        file = "/tmp/" + m_filename;

    checkFile.setName(file);
    if (!checkFile.exists())
        file = m_filename;

    if (m_debug == true)
        cerr << "     -Filename: " << file << endl;

    if (m_hmult == 1 && m_wmult == 1 && m_force_x == -1 && m_force_y == -1)
    {
        if (img.load(file))
            m_show = true;
    }
    else
    {
        QImage *sourceImg = new QImage();
        if (sourceImg->load(file))
        {
            QImage scalerImg;
            int doX = sourceImg->width();
            int doY = sourceImg->height();
            if (m_force_x != -1)
            {
                doX = m_force_x;
                if (m_debug == true)
                    cerr << "         +Force X: " << doX << endl;
            }
            if (m_force_y != -1)
            {
                doY = m_force_y;
                if (m_debug == true)
                    cerr << "         +Force Y: " << doY << endl;
            }

            scalerImg = sourceImg->smoothScale((int)(doX * m_wmult),
                                               (int)(doY * m_hmult));
            m_show = true;
            img.convertFromImage(scalerImg);
            if (m_debug == true)
                    cerr << "     -Image: " << file << " loaded.\n";
        }
        else
        {
            m_show = false;
            if (m_debug == true)
                cerr << "     -Image: " << file << " failed to load.\n";
        }
        delete sourceImg;
    }
}

void UIImageType::Draw(QPainter *dr, int drawlayer, int context)
{
  if (m_context == context || m_context == -1)
    if (drawlayer == m_order)
    {
      if (!img.isNull() && m_show == true)
      {
        if (m_debug == true)
        {
            cerr << "   +UIImageType::Draw() <- inside Layer\n";
            cerr << "       -Drawing @ (" << m_displaypos.x() << ", " << m_displaypos.y() << ")" << endl;
            cerr << "       -Skip Section: (" << m_drop_x << ", " << m_drop_y << ")\n";
        }
        dr->drawPixmap(m_displaypos.x(), m_displaypos.y(), img, m_drop_x, m_drop_y);
      } else if (m_debug == true)
            cerr << "   +UIImageType::Draw() <= Image is null\n";
      
    }
    else
        if (m_debug == true)
        {
            cerr << "   +UIImageType::Draw() <- outside (layer = " << drawlayer
                 << ", widget layer = " << m_order << "\n";
        }
}

// ******************************************************************

UITextType::UITextType(const QString &name, fontProp *font,
                       const QString &text, int dorder, QRect displayrect)
           : UIType(name)
{
    m_message = text;
    m_font = font;
    m_displaysize = displayrect;
    m_cutdown = true;
    m_order = dorder;
    m_justification = (Qt::AlignLeft | Qt::AlignTop);
}

UITextType::~UITextType()
{
}

void UITextType::SetText(const QString &text)
{
    m_message = text;
}

void UITextType::Draw(QPainter *dr, int drawlayer, int context)
{
  if (m_context == context || m_context == -1)
    if (drawlayer == m_order)
    {
        QPoint fontdrop = m_font->shadowOffset;
        QString msg = m_message;
        dr->setFont(m_font->face);
        if (m_cutdown == true)
            msg = cutDown(msg, &(m_font->face), m_displaysize.width(), m_displaysize.height());
        if (m_cutdown == true && m_debug == true)
            cerr << "    +UITextType::CutDown Called.\n";

        if (fontdrop.x() != 0 || fontdrop.y() != 0)
        {
            if (m_debug == true)
                cerr << "    +UITextType::Drawing shadow @ (" 
                     << (int)(m_displaysize.left() + fontdrop.x()) << ", "
                     << (int)(m_displaysize.top() + fontdrop.y()) << ")" << endl;
            dr->setBrush(m_font->dropColor);
            dr->setPen(QPen(m_font->dropColor, (int)(2 * m_wmult)));
            dr->drawText((int)(m_displaysize.left() + fontdrop.x()),
                           (int)(m_displaysize.top() + fontdrop.y()),
                           m_displaysize.width(), 
                           m_displaysize.height(), m_justification, msg);
        }

        dr->setBrush(m_font->color);
        dr->setPen(QPen(m_font->color, (int)(2 * m_wmult)));
        if (m_debug == true)
                cerr << "    +UITextType::Drawing @ (" 
                     << (int)(m_displaysize.left()) << ", " << (int)(m_displaysize.top()) 
                     << ")" << endl;
        dr->drawText(m_displaysize.left(), m_displaysize.top(), 
                      m_displaysize.width(), m_displaysize.height(), m_justification, msg);
        if (m_debug == true)
        {
            cerr << "   +UITextType::Draw() <- inside Layer\n";
            cerr << "       -Message: " << m_message << " (cut: " << msg << ")" <<  endl;
        }
    }
    else
        if (m_debug == true)
        {
             cerr << "   +UITextType::Draw() <- outside (layer = " << drawlayer
                  << ", widget layer = " << m_order << "\n";
        }
}

QString UITextType::cutDown(QString info, QFont *testFont, int maxwidth, int maxheight)
{
    QFontMetrics fm(*testFont);
    QRect curFontSize;

    curFontSize = fm.boundingRect(0, 0, maxwidth, maxheight, m_justification, info);

    if (curFontSize.height() > (int)(1.5 * maxheight))
    {
        QString testInfo = info;
        curFontSize = fm.boundingRect(0, 0, maxwidth, maxheight, m_justification, testInfo);
        int count = info.length();

        while (curFontSize.height() >= maxheight)
        {
            testInfo = info.left(count);
            curFontSize = fm.boundingRect(0, 0, maxwidth, maxheight, m_justification, testInfo);
            count = count--;
            if (count < 0)
                break;
        }
        testInfo = testInfo + "...";
        info = testInfo;
    }
    else if (curFontSize.width() > maxwidth)
    {
        int curFontWidth = fm.width(info);
        if (curFontWidth > maxwidth)
        {
            QString testInfo = "";
            curFontWidth = fm.width(testInfo);
            int tmaxwidth = maxwidth - fm.width("LLL");
            int count = 0;

            while (curFontWidth < tmaxwidth)
            {
                testInfo = info.left(count);
                curFontWidth = fm.width(testInfo);
                count = count + 1;
            }
            testInfo = testInfo + "...";
            info = testInfo;
        }
    }
    return info;

}

// ******************************************************************

UIStatusBarType::UIStatusBarType(QString &name, QPoint loc, int dorder)
               : UIType(name)
{
    m_location = loc;
    m_order = dorder;
}

UIStatusBarType::~UIStatusBarType()
{
}

void UIStatusBarType::Draw(QPainter *dr, int drawlayer, int context)
{
  if (m_context == context || m_context == -1)
    if (drawlayer == m_order)
    {
 	if (m_debug == true)
            cerr << "   +UIStatusBarType::Draw() <- within Layer\n";
     
        int width = (int)((double)((double)m_container.width() - (double)(2*m_fillerSpace))
                    * (double)((double)m_used / (double)m_total));

	if (m_debug == true)
            cerr << "       -Width = " << width << "\n";

        dr->drawPixmap(m_location.x(), m_location.y(), m_container);
        dr->drawPixmap(m_location.x(), m_location.y(), m_filler, 0, 0, width + m_fillerSpace);
    }
}

// *********************************************************************

GenericTree::GenericTree()
{
    init();
}

GenericTree::GenericTree(const QString a_string)
{
    init();
    my_string = a_string;
}

GenericTree::GenericTree(QString a_string, QString a_type, int an_int)
{
    init();
    my_string = a_string;
    my_type = a_type;
    my_int = an_int;
}

GenericTree::GenericTree(int an_int)
{
    init();
    my_int = an_int;
}

void GenericTree::init()
{
    my_parent = NULL;
    my_string = "";
    my_type = "";
    my_stringlist.clear();
    my_int = 0;
    my_subnodes.clear();
    //my_subnodes.setAutoDelete(true);
}

GenericTree* GenericTree::addNode(QString a_string)
{
    GenericTree *new_node = new GenericTree(a_string);
    new_node->setParent(this);
    my_subnodes.append(new_node);

    return new_node;
}

GenericTree* GenericTree::addNode(QString a_string, int an_int)
{
    GenericTree *new_node = new GenericTree(a_string);
    new_node->setInt(an_int);
    new_node->setParent(this);
    my_subnodes.append(new_node);
    return new_node;
}

GenericTree* GenericTree::addNode(QString a_string, QString a_type, int an_int)
{
    GenericTree *new_node = new GenericTree(a_string);
    new_node->setInt(an_int);
    new_node->setType(a_type);
    new_node->setParent(this);
    my_subnodes.append(new_node);
    return new_node;
}

int GenericTree::calculateDepth(int start)
{
    int current_depth;
    int found_depth;
    current_depth = start + 1;
    QPtrListIterator<GenericTree> it( my_subnodes );
    GenericTree *my_kids;

    while( (my_kids = it.current()) != 0)
    {
        found_depth = my_kids->calculateDepth(start + 1);
        if(found_depth > current_depth)
        {
            current_depth = found_depth;
        }
        ++it;
    }
    return current_depth;
}

GenericTree* GenericTree::findLeaf()
{
    if(my_subnodes.count() > 0)
    {
        return my_subnodes.getFirst()->findLeaf();
    }
    return this;
}


GenericTree* GenericTree::findNode(QValueList<int> route_of_branches)
{
    //
    //  Starting from *this* node (which will often be root)
    //  find a set of branches that have id's that match
    //  the collection passed in route_of_branches. Return
    //  the end point of those branches (which will often be
    //  a leaf node).
    //
    //  In practical terms, mythmusic will use this to force the
    //  playback screen's ManagedTreeList to move to a given track
    //  in a given playlist (for example).
    //

    return recursiveNodeFinder(route_of_branches);
}

GenericTree* GenericTree::recursiveNodeFinder(QValueList<int> route_of_branches)
{
    if(checkNode(route_of_branches))
    {
        return this;
    }
    else
    {
        QPtrListIterator<GenericTree> it( my_subnodes );
        GenericTree *my_kids;
        while( (my_kids = it.current()) != 0)
        {
            GenericTree *sub_checker = my_kids->recursiveNodeFinder(route_of_branches);
            if(sub_checker)
            {
                return sub_checker;
            }
            else
            {
                ++it;
            }
        }
    }
    return NULL;
}

bool GenericTree::checkNode(QValueList<int> route_of_branches)
{
    bool found_it = true;
    GenericTree *parent_finder = this;

    for(int i = route_of_branches.count() - 1; i > -1 ; --i)
    {
        if(!(parent_finder->getInt() == (*route_of_branches.at(i))))
        {
            found_it = false;
        }
        if(i > 0)
        {
            if(parent_finder->getParent())
            {
                parent_finder = parent_finder->getParent();
            }
            else
            {
                found_it = false;
            }
        }
    }
    return found_it;
}

int GenericTree::getChildPosition(GenericTree *which_child)
{
    return my_subnodes.findRef(which_child);
}

int GenericTree::getPosition()
{
    if(my_parent)
    {
        return my_parent->getChildPosition(this);
    }
    return 0;
}

int GenericTree::siblingCount()
{
    if(my_parent)
    {
        return my_parent->childCount();
    }
    return 1;
}

void GenericTree::printTree(int margin)
{
    for(int i = 0; i < margin; i++)
    {
        cout << " " ;
    }
    cout << "GenericTreeNode: my int is " << my_int << ", my string is \"" << my_string << "\"" << endl;
    
    //
    //  recurse through my children
    //

    QPtrListIterator<GenericTree> it( my_subnodes );
    GenericTree *my_kids;
    while( (my_kids = it.current()) != 0)
    {
        my_kids->printTree(margin + 4);
        ++it;
    }
}

GenericTree* GenericTree::getChildAt(uint reference)
{
    if(reference >= my_subnodes.count())
    {
        cerr << "uitypes.o: out of bounds request to GenericTree::getChildAt()" << endl;
        return NULL;
    }
    return my_subnodes.at(reference);
}

GenericTree* GenericTree::prevSibling(int number_up)
{

    if(!my_parent)
    {
        //  
        //  I'm root = no siblings
        //
        
        return NULL;
    }

    int my_position = my_parent->getChildPosition(this);

    if(my_position < number_up)
    {
        //
        //  not enough siblings "above" me
        //
        
        return NULL;
    }
    
    return my_parent->getChildAt(my_position - number_up);
}

GenericTree* GenericTree::nextSibling(int number_down)
{
    if(!my_parent)
    {
        //  
        //  I'm root = no siblings
        //
        
        return NULL;
    }

    int my_position = my_parent->getChildPosition(this);

    if(my_position + number_down >= my_parent->childCount())
    {
        //
        //  not enough siblings "below" me
        //
        
        return NULL;
    }
    
    return my_parent->getChildAt(my_position + number_down);
}

GenericTree* GenericTree::getParent()
{
    if(my_parent)
    {
        return my_parent;
    }
    return NULL;
}

GenericTree::~GenericTree()
{
}

// ********************************************************************

UIManagedTreeListType::UIManagedTreeListType(const QString & name)
                     : UIType(name)
{
    bins = 0;
    bin_corners.clear();
    my_tree_data = NULL;
    current_node = NULL;
    tree_depth = 0;
    m_justification = (Qt::AlignLeft | Qt::AlignVCenter);
    active_bin = 0;
}

UIManagedTreeListType::~UIManagedTreeListType()
{
    //
    //  Funny, I have yet to see this appear
    //  (even when it isn't commented out)
    //
    cout << "Rage, rage against the dying of the light" << endl;
}

void UIManagedTreeListType::Draw(QPainter *p, int drawlayer, int context)
{
    context = context;  // Would we ever want to use that?

    if(drawlayer != m_order)
    {
        // not my turn
        return;
    }

    if(!current_node)
    {
        cerr << "uitypes.o: I can't draw a UIManagedTreeListType if there is no current_node" << endl;
        return;
    }

    //
    //  Draw each bin, using current_node and working up
    //  and/or down to tell us what to draw in each bin.
    //


    for(int i = bins; i > 0; --i)
    {
        GenericTree *hotspot_node = current_node;

        if(i < active_bin)
        {
            for(int j = 0; j < bins - i; j++)
            {
                if(hotspot_node)
                {
                    if(hotspot_node->getParent())
                    {
                        hotspot_node = hotspot_node->getParent();
                    }
                }
            }
        }
        if(i > active_bin)
        {
            for(int j = 0; j < i - active_bin; j++)
            {
                if(hotspot_node)
                {
                    if(hotspot_node->childCount() > 0)
                    {
                        hotspot_node = hotspot_node->getChildAt(0);
                    }
                    else
                    {
                        hotspot_node = NULL;
                    }
                }
            }
        }
        
        if(hotspot_node)
        {
            fontProp *tmpfont = NULL;

            if(i == active_bin)
            {
                tmpfont = &m_fontfcns[m_fonts["selected"]];
                p->setFont(tmpfont->face);
                p->setPen(QPen(tmpfont->color, (int)(2 * m_wmult)));
            }
            else
            {
                tmpfont = &m_fontfcns[m_fonts["inactive"]];
                p->setFont(tmpfont->face);
                p->setPen(QPen(tmpfont->color, (int)(2 * m_wmult)));
            }

            int x_location = bin_corners[i].left();
            int y_location = bin_corners[i].top() + (bin_corners[i].height() / 2)
                             + (QFontMetrics(tmpfont->face).height() / 2);

            if(i == bins)
            {

                //
                //  if required, move the hotspot up or down
                //  (beginning of list, end of list, etc.)
                //
                
                int position_in_list = hotspot_node->getPosition();
                int number_in_list = hotspot_node->siblingCount();
    
                int number_of_slots = 0;
                int another_y_location = y_location - QFontMetrics(tmpfont->face).height();
                while (another_y_location - QFontMetrics(tmpfont->face).height() > bin_corners[i].top())
                {
                    another_y_location -= QFontMetrics(tmpfont->face).height();
                    ++number_of_slots;
                }
                
                if(position_in_list < number_of_slots)
                {
                    for(int j = 0; j < number_of_slots - position_in_list; j++)
                    {
                        y_location -= QFontMetrics(tmpfont->face).height();
                    }
                }
                else if((number_in_list - position_in_list) <= number_of_slots &&
                   position_in_list > number_of_slots)
                {
                    // MOVE DOWN
            
                    if(number_in_list >= number_of_slots * 2 + 1)
                    { 
                        for(int j = 0; j <= number_of_slots - (number_in_list - position_in_list); j++)
                        {
                            y_location += QFontMetrics(tmpfont->face).height();
                        }
                    }
                    else
                    {
                        for(int j = 0; j <= position_in_list - (number_of_slots + 1); j++)
                        {
                            y_location += QFontMetrics(tmpfont->face).height();
                        }
                    }
                }
                
            }
            

            QString msg = hotspot_node->getString();
            msg = cutDown(msg, &(tmpfont->face), bin_corners[i].width(), bin_corners[i].height());
            p->drawText(x_location, y_location, msg);

            if(i == active_bin)
            {
                //
                //  Draw the highlight pixmap
                //  NB: should be resizing this elsewhere
                //

                QImage temp_image = highlight_image.convertToImage();
                QPixmap temp_pixmap;
                temp_pixmap.convertFromImage(temp_image.smoothScale(bin_corners[i].width(), QFontMetrics(tmpfont->face).height()));
                p->drawPixmap(x_location, y_location - QFontMetrics(tmpfont->face).height() + QFontMetrics(tmpfont->face).descent(), temp_pixmap);
            }

            
            
            

            if(i == active_bin)
            {
                tmpfont = &m_fontfcns[m_fonts["active"]];
                p->setFont(tmpfont->face);
                p->setPen(QPen(tmpfont->color, (int)(2 * m_wmult)));
            }
            else
            {
                tmpfont = &m_fontfcns[m_fonts["inactive"]];
                p->setFont(tmpfont->face);
                p->setPen(QPen(tmpfont->color, (int)(2 * m_wmult)));
            }
            
            //
            //  Do the ones above
            //
            
            int numb_above = 1;
            int still_yet_another_y_location = y_location - QFontMetrics(tmpfont->face).height();
            while (still_yet_another_y_location - QFontMetrics(tmpfont->face).height() > bin_corners[i].top())
            {
                GenericTree *above = hotspot_node->prevSibling(numb_above);
                if(above)
                {
                    msg = above->getString();
                    msg = cutDown(msg, &(tmpfont->face), bin_corners[i].width(), bin_corners[i].height());
                    p->drawText(x_location, still_yet_another_y_location, msg);
                }    
                still_yet_another_y_location -= QFontMetrics(tmpfont->face).height();
                numb_above++;
            }
            

            //
            //  Do the ones below
            //
            
            int numb_below = 1;
            y_location += QFontMetrics(tmpfont->face).height();
            while (y_location < bin_corners[i].bottom())
            {
                GenericTree *below = hotspot_node->nextSibling(numb_below);
                if(below)
                {
                    msg = below->getString();
                    msg = cutDown(msg, &(tmpfont->face), bin_corners[i].width(), bin_corners[i].height());
                    p->drawText(x_location, y_location, msg);
                }    
                y_location += QFontMetrics(tmpfont->face).height();
                numb_below++;
            }

        }
        else
        {
            //
            //  This bin is empty
            //
            // p->eraseRect(bin_corners[i]);
        }
    }


    //
    //  Debugging, draw edges around bins
    //
    /*
    p->setPen(QColor(255,0,0));
    CornerMap::Iterator it;
    for ( it = bin_corners.begin(); it != bin_corners.end(); ++it )
    {
        p->drawRect(it.data());
    }
    */
}

QString UIManagedTreeListType::cutDown(QString info, QFont *testFont, int maxwidth, int maxheight)
{
    QFontMetrics fm(*testFont);
    QRect curFontSize;

    curFontSize = fm.boundingRect(0, 0, maxwidth, maxheight, m_justification, info);

    if (curFontSize.height() > maxheight)
    {
        QString testInfo = info;
        curFontSize = fm.boundingRect(0, 0, maxwidth, maxheight, m_justification, testInfo);
        int count = info.length();

        while (curFontSize.height() >= maxheight)
        {
            testInfo = info.left(count);
            curFontSize = fm.boundingRect(0, 0, maxwidth, maxheight, m_justification, testInfo);
            count = count--;
            if (count < 0)
                break;
        }
        testInfo = testInfo + "...";
        info = testInfo;
    }
    else if (curFontSize.width() > maxwidth)
    {
        int curFontWidth = fm.width(info);
        if (curFontWidth > maxwidth)
        {
            QString testInfo = "";
            curFontWidth = fm.width(testInfo);
            int tmaxwidth = maxwidth - fm.width("LLL");
            int count = 0;

            while (curFontWidth < tmaxwidth)
            {
                testInfo = info.left(count);
                curFontWidth = fm.width(testInfo);
                count = count + 1;
            }
            testInfo = testInfo + "...";
            info = testInfo;
        }
    }
    return info;

}

void UIManagedTreeListType::moveToNode(QValueList<int> route_of_branches)
{
    current_node = my_tree_data->findNode(route_of_branches);
    if(!current_node)
    {
        current_node = my_tree_data->findLeaf();
    }
    emit nodeSelected(current_node->getType(), current_node->getInt());
}

void UIManagedTreeListType::assignTreeData(GenericTree *a_tree)
{
    my_tree_data = a_tree;
    
    //
    //  Trying to keep this as generic as possible, we
    //  need to figure out depth from the root node of this
    //  tree. That way, draw() knows what to draw.
    //

    tree_depth = my_tree_data->calculateDepth(0);

    //
    //  By default, current_node is first branch at every
    //  level down till we hit a leaf node.
    //
    
    current_node = my_tree_data->findLeaf();
    active_bin = bins;
    //current_node->printTree(0);

}

void UIManagedTreeListType::sayHelloWorld()
{
    cout << "From a UIManagedTreeListType Object: Hello World" << endl ;
}

void UIManagedTreeListType::popUp()
{
    //
    //  Move the active node to the
    //  current active node's parent  
    //
    
    if(!current_node->getParent()->getParent())
    {
        //
        //  I be ultimate root
        //
        
        return;
    }
    
    if(active_bin > 1)
    {
        --active_bin;
        current_node = current_node->getParent();
        emit nodeEntered(current_node->getType(), current_node->getInt());
    }
    else if(active_bin < bins)
    {
        ++active_bin;
    }


    QRect redraw_coords = bin_corners[2];
    redraw_coords.moveBy(area.left(),area.top());
    emit requestUpdate(redraw_coords);

    redraw_coords = bin_corners[1];
    redraw_coords.moveBy(area.left(),area.top());
    emit requestUpdate(redraw_coords);

}

void UIManagedTreeListType::pushDown()
{
    //
    //  Move the active node to the
    //  current active node's first child
    //

    if(current_node->childCount() < 1)
    {
        //
        //  I be leaf
        return;
    }

    if(active_bin < bins)
    {
        ++active_bin;
        current_node = current_node->getChildAt(0);
        emit nodeEntered(current_node->getType(), current_node->getInt());
    }
    else if(active_bin > 1)
    {
        --active_bin;
    }

    QRect redraw_coords = bin_corners[2];
    redraw_coords.moveBy(area.left(),area.top());
    emit requestUpdate(redraw_coords);

    redraw_coords = bin_corners[1];
    redraw_coords.moveBy(area.left(),area.top());
    emit requestUpdate(redraw_coords);

}

void UIManagedTreeListType::moveUp()
{
    //
    //  Move the active node to the
    //  current active node's previous
    //  sibling  
    //

    GenericTree *new_node = current_node->prevSibling(1);
    if(new_node)
    {
        current_node = new_node;
        for(int i = active_bin; i <= bins; i++)
        {

            QRect redraw_coords = bin_corners[i];
            redraw_coords.moveBy(area.left(),area.top());
            emit requestUpdate(redraw_coords);
            emit nodeEntered(current_node->getType(), current_node->getInt());
        }
    }
}

void UIManagedTreeListType::moveDown()
{
    //
    //  Move the active node to the
    //  current active node's next
    //  sibling
    //

    GenericTree *new_node = current_node->nextSibling(1);
    if(new_node)
    {
        current_node = new_node;
        for(int i = active_bin; i <= bins; i++)
        {
            QRect redraw_coords = bin_corners[i];
            redraw_coords.moveBy(area.left(),area.top());
            emit requestUpdate(redraw_coords);
            emit nodeEntered(current_node->getType(), current_node->getInt());
        }
    }
}

// ********************************************************************

UIPushButtonType::UIPushButtonType(const QString &name, QPixmap on, QPixmap off, QPixmap pushed)
                     : UIType(name)
{
    on_pixmap = on;
    off_pixmap = off;
    pushed_pixmap = pushed;
}


void UIPushButtonType::Draw(QPainter *p, int drawlayer, int context)
{
    context = context;  // Would we ever want to use that?

    if(drawlayer != m_order)
    {
        // not my turn
        return;
    }

    if(has_focus)
    {
        p->drawPixmap(m_displaypos.x(), m_displaypos.y(), on_pixmap);
    }
    else
    {
        p->drawPixmap(m_displaypos.x(), m_displaypos.y(), off_pixmap);
    }
    
}

void UIPushButtonType::push()
{
    cout << "Yamma been pushed" << endl ;
}

// ********************************************************************

UITextButtonType::UITextButtonType(const QString &name, QPixmap on, QPixmap off, QPixmap pushed)
                     : UIType(name)
{
    on_pixmap = on;
    off_pixmap = off;
    pushed_pixmap = pushed;
    m_text = "";
}


void UITextButtonType::Draw(QPainter *p, int drawlayer, int context)
{
    context = context;  // Would we ever want to use that?

    if(drawlayer != m_order)
    {
        // not my turn
        return;
    }

    if(has_focus)
    {
        p->drawPixmap(m_displaypos.x(), m_displaypos.y(), on_pixmap);
    }
    else
    {
        p->drawPixmap(m_displaypos.x(), m_displaypos.y(), off_pixmap);
    }
    p->setFont(m_font->face);
    p->setBrush(m_font->color);
    p->setPen(QPen(m_font->color, (int)(2 * m_wmult)));
    p->drawText(m_displaypos.x(),
                m_displaypos.y(),
                off_pixmap.width(),
                off_pixmap.height(),
                Qt::AlignCenter,
                m_text);
/*
    p->drawText(area.left(), area.top(), area.width(), area.height(),
                 m_justification, msg);
*/
    
}

void UITextButtonType::push()
{
    cout << "Yamma been pushed" << endl ;
}

