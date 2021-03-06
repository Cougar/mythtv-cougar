#ifndef MYTHUI_IMAGE_H_
#define MYTHUI_IMAGE_H_

#include <QDateTime>

#include "mythuitype.h"
#include "mythimage.h"

class MythScreenType;

/**
 * \class MythUIImage
 *
 * \brief Image widget, displays a single image or multiple images in sequence
 */
class MPUBLIC MythUIImage : public MythUIType
{
  public:
    MythUIImage(const QString &filepattern, int low, int high, int delayms,
                MythUIType *parent, const QString &name);
    MythUIImage(const QString &filename, MythUIType *parent, const QString &name);
    MythUIImage(MythUIType *parent, const QString &name);
   ~MythUIImage();

    // Must be followed by a call to Load() to load the image.
    void SetFilename(const QString &filename);
    void SetFilepattern(const QString &filepattern, int low, int high);

    void SetImageCount(int low, int high);
    void SetImage(MythImage *img) __attribute__ ((deprecated));
    void SetImages(QVector<MythImage *> &images) __attribute__ ((deprecated));

    QString GenImageLabel(const QString &filename, int w, int h);
    QString GenImageLabel(int w, int h);

    void Reset(void);
    bool Load(void);

    bool IsGradient(void) const { return m_gradient; }

    virtual void Pulse(void);

    virtual void LoadNow(void);

  protected:
    virtual void DrawSelf(MythPainter *p, int xoffset, int yoffset,
                          int alphaMod, QRect clipRect);

    void Init(void);
    void Clear(void);

    virtual bool ParseElement(QDomElement &element);
    virtual void CopyFrom(MythUIType *base);
    virtual void CreateCopy(MythUIType *parent);
    virtual void Finalize(void);

    void SetDelay(int delayms);

    void SetSize(int width, int height);
    void SetSize(const QSize &size);
    void ForceSize(const QSize &size);

    void SetCropRect(int x, int y, int width, int height);
    void SetCropRect(const MythRect &rect);

    QString m_Filename;
    QString m_OrigFilename;

    QVector<MythImage *> m_Images;

    MythRect m_cropRect;
    QSize m_ForceSize;

    int m_Delay;
    int m_LowNum;
    int m_HighNum;

    unsigned int m_CurPos;
    QTime m_LastDisplay;

    bool m_NeedLoad;

    bool m_isReflected;
    ReflectAxis m_reflectAxis;
    int  m_reflectShear;
    int  m_reflectScale;
    int  m_reflectLength;
    int  m_reflectSpacing;

    MythImage *m_maskImage;
    bool m_isMasked;

    bool m_gradient;
    QColor m_gradientStart;
    QColor m_gradientEnd;
    uint m_gradientAlpha;
    FillDirection m_gradientDirection;

    bool m_preserveAspect;

    bool m_isGreyscale;

    friend class MythThemeBase;
    friend class MythUIButtonListItem;
    friend class MythUIProgressBar;
    friend class MythUITextEdit;
};

#endif
