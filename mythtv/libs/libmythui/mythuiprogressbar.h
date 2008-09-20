#ifndef MYTHUI_PROGRESSBAR_H_
#define MYTHUI_PROGRESSBAR_H_

#include "mythuitype.h"
#include "mythuiimage.h"

class MythFontProperties;

/** \class MythUIProgressBar
 *
 * \brief Progress bar widget.
 *
 */
class MythUIProgressBar : public MythUIType
{
  public:
    MythUIProgressBar(MythUIType *parent, const QString &name);
   ~MythUIProgressBar();

    void Reset(void);

    enum LayoutType { LayoutVertical, LayoutHorizontal };
    enum EffectType { EffectReveal, EffectSlide, EffectAnimate };

    void SetStart(int);
    void SetUsed(int);
    void SetTotal(int);

  protected:
    virtual bool ParseElement(QDomElement &element);
    virtual void CopyFrom(MythUIType *base);
    virtual void CreateCopy(MythUIType *parent);

    LayoutType m_layout;
    EffectType m_effect;

    int m_total;
    int m_start;
    int m_current;

    void CalculatePosition(void);
};

#endif
