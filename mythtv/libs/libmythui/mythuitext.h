#ifndef MYTHUI_TEXT_H_
#define MYTHUI_TEXT_H_

#include "mythuitype.h"

#include <qimage.h>


#include "mythuitype.h"

class MythFontProperties;

class MythUIText : public MythUIType
{
  public:
    MythUIText(const QString &text, const MythFontProperties &font,
               QRect displayRect, QRect altDisplayRect,
               MythUIType *parent, const char *name);
    ~MythUIText();

    void SetText(const QString &text);
    QString GetText(void);
    QString GetDefaultText(void);

    void SetFontProperties(const MythFontProperties &fontProps);

    void UseAlternateArea(bool useAlt);

    void SetJustification(int just);
    int GetJustification(void);
    void SetCutDown(bool cut);

    void SetDisplayArea(const QRect &rect);

    virtual void Pulse(void);

    virtual void Draw(MythPainter *p, int xoffset, int yoffset, 
                      int alphaMod = 255);

    void CycleColor(QColor startColor, QColor endColor, int numSteps);
    void StopCycling();

  protected:
    int m_Justification;
    QRect m_OrigDisplayRect;
    QRect m_AltDisplayRect;

    QString m_Message;
    QString m_CutMessage;
    QString m_DefaultMessage;

    bool m_Cutdown;

    MythFontProperties* m_Font;

    bool m_colorCycling;
    QColor m_startColor, m_endColor;
    int m_numSteps, m_curStep;
    float curR, curG, curB;
    float incR, incG, incB;
};

#endif
