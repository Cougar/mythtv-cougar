#include <cassert>

#include <qapplication.h>
#include <qpixmap.h>
#include <qpainter.h>
#include <qgl.h>
#include <qcache.h>
#include <qintcache.h>

#include "mythpainter_ogl.h"
#include "mythfontproperties.h"

class StringImageCache : public QCache<MythImage>
{
  public:
    StringImageCache() : QCache<MythImage>(25, 31) 
    {  setAutoDelete(true); }
};

class IntImageCache : public QIntCache<MythImage>
{
  public:
    IntImageCache(MythOpenGLPainter *parent) 
          : QIntCache<MythImage>(25, 31) 
    { m_parent = parent; setAutoDelete(false); }

  protected:
    void deleteItem(MythImage *im) { printf("deleting item\n"); m_parent->DeleteFormatImage(im); }

    MythOpenGLPainter *m_parent;   
};

MythOpenGLPainter::MythOpenGLPainter() 
                 : MythPainter()
{
    m_StringImageCache = new StringImageCache();
    m_IntImageCache = new IntImageCache(this);
}

MythOpenGLPainter::~MythOpenGLPainter()
{
    delete m_StringImageCache;
    delete m_IntImageCache;
}

void MythOpenGLPainter::Begin(QWidget *parent)
{
    assert(parent);

    MythPainter::Begin(parent);

    QGLWidget *realParent = dynamic_cast<QGLWidget *>(parent);
    assert(realParent);

    realParent->makeCurrent();
    glShadeModel(GL_FLAT);
    glViewport(0, 0, parent->width(), parent->height());
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, 800, 600, 0, -999999, 999999);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glEnable(GL_TEXTURE_2D);
}

void MythOpenGLPainter::End(void)
{
    QGLWidget *realParent = dynamic_cast<QGLWidget *>(m_Parent);
    assert(realParent);

    realParent->makeCurrent();
    glFlush();
    realParent->swapBuffers();

    MythPainter::End();
}

// returns the highest number closest to v, which is a power of 2
// NB! assumes 32 bit ints
int MythOpenGLPainter::NearestGLTextureSize(int v)
{
    int n = 0, last = 0;

    for (int s = 0; s < 32; ++s) 
    {
        if (((v >> s) & 1) == 1) 
        {
            ++n;
            last = s;
        }
    }

    if (n > 1)
        return 1 << (last + 1);

    return 1 << last;
}

void MythOpenGLPainter::RemoveImageFromCache(MythImage *im)
{
    if (m_ImageIntMap.contains(im))
    {
        GLuint textures[1];
        textures[0] = m_ImageIntMap[im];

        glDeleteTextures(1, textures);
        m_ImageIntMap.erase(im);
    }
}

void MythOpenGLPainter::BindTextureFromCache(MythImage *im, 
                                             bool alphaonly)
{
    if (m_ImageIntMap.contains(im))
    {
        long val = m_ImageIntMap[im];

        if (!im->IsChanged())
        {
            m_IntImageCache->find(val);
            glBindTexture(GL_TEXTURE_2D, val);
            return;
        }
        else
        {
            RemoveImageFromCache(im);
        }
    }

    im->SetChanged(false);

    // Scale the pixmap if needed. GL textures needs to have the
    // dimensions 2^n+2(border) x 2^m+2(border).
    QImage tx;
    int tx_w = NearestGLTextureSize(im->width());
    int tx_h = NearestGLTextureSize(im->height());
    if (tx_w != im->width() || tx_h !=  im->height())
        tx = QGLWidget::convertToGLFormat(im->scale(tx_w, tx_h));
    else
        tx = QGLWidget::convertToGLFormat(*im);

    GLuint format = GL_RGBA8;
    if (alphaonly)
        format = GL_ALPHA;

    GLuint tx_id;
    glGenTextures(1, &tx_id);
    glBindTexture(GL_TEXTURE_2D, tx_id);
    glHint(GL_GENERATE_MIPMAP_HINT, GL_NICEST);
    glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_TRUE);
    glTexImage2D(GL_TEXTURE_2D, 0, format, tx.width(), tx.height(), 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, tx.bits());

    m_ImageIntMap[im] = tx_id;
    m_IntImageCache->insert((long)tx_id, im);
}

void MythOpenGLPainter::DrawImage(const QRect &r, MythImage *im, 
                                  const QRect &src, int alpha)
{
    glClearDepth(1.0f);

    // see if we have this pixmap cached as a texture - if not cache it
    BindTextureFromCache(im);

    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                    GL_LINEAR_MIPMAP_LINEAR);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glPushAttrib(GL_CURRENT_BIT);

    glColor4f(1.0, 1.0, 1.0, alpha / 255.0);
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);

    glBegin(GL_QUADS);
    {
        double x1 = src.x() / (double)im->width();
        double x2 = x1 + src.width() / (double)im->width();
        double y1 = src.y() / (double)im->height();
        double y2 = y1 + src.height() / (double)im->height();

        glTexCoord2f(x1, y2); glVertex2i(r.x(), r.y());
        glTexCoord2f(x2, y2); glVertex2i(r.x() + r.width(), r.y());
        glTexCoord2f(x2, y1); glVertex2i(r.x() + r.width(), r.y() + r.height());
        glTexCoord2f(x1, y1); glVertex2i(r.x(), r.y()+r.height());
    }
    glEnd();

    glDisable(GL_BLEND);
    glDisable(GL_TEXTURE_2D);
    glPopAttrib();
}

int MythOpenGLPainter::CalcAlpha(int alpha1, int alpha2)
{
    return (int)(alpha1 * (alpha2 / 255.0));
}

// XXX: Need to hash string with font properties
MythImage *MythOpenGLPainter::GetImageFromString(const QString &msg, 
                                                 int flags, const QRect &r, 
                                                 const MythFontProperties &font)
{
    MythImage *im;

    if ((im = m_StringImageCache->find(msg)))
        return im;

    im = GetFormatImage();

    qApp->lock();

    int w = NearestGLTextureSize(r.width());
    int h = NearestGLTextureSize(r.height());

    QPixmap pm(QSize(w, h));
    pm.fill();

    QPainter tmp(&pm);
    tmp.setFont(font.face);
    tmp.setPen(Qt::black);
    tmp.drawText(0, 0, r.width(), r.height(), flags, msg);
    tmp.end();

    im->Assign(pm.convertToImage().convertDepth(8, Qt::MonoOnly |
                                                   Qt::ThresholdDither |
                                                   Qt::AvoidDither));

    qApp->unlock();

    int numColors = im->numColors();
    QRgb *colorTable = im->colorTable();

    for (int i = 0; i < numColors; i++)
    {
        int alpha = 255 - qRed(colorTable[i]);
        colorTable[i] = qRgba(0, 0, 0, alpha);
    }

    m_StringImageCache->insert(msg, im);

    return im;
}

void MythOpenGLPainter::ReallyDrawText(QColor color, const QRect &r, int alpha)
{
    glPushAttrib(GL_CURRENT_BIT);

    alpha = CalcAlpha(qAlpha(color.rgb()), alpha);

    glColor4f(color.red() / 255.0, color.green() / 255.0, color.blue() / 255.0,
              alpha / 255.0);
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);

    glBegin(GL_QUADS);
    {
        double x1 = 0;
        double x2 = 1;
        double y1 = 0;
        double y2 = 1;

        glTexCoord2f(x1, y2); glVertex2i(r.x(), r.y());
        glTexCoord2f(x2, y2); glVertex2i(r.x() + r.width(), r.y());
        glTexCoord2f(x2, y1); glVertex2i(r.x() + r.width(), r.y() + r.height());
        glTexCoord2f(x1, y1); glVertex2i(r.x(), r.y()+r.height());
    }
    glEnd();

    glDisable(GL_BLEND);
    glDisable(GL_TEXTURE_2D);
    glPopAttrib();
}

void MythOpenGLPainter::DrawText(const QRect &r, const QString &msg,
                                 int flags, const MythFontProperties &font, 
                                 int alpha)
{
    glClearDepth(1.0f);

    MythImage *im = GetImageFromString(msg, flags, r, font);

    if (!im)
        return;

    // see if we have this pixmap cached as a texture - if not cache it
    BindTextureFromCache(im, true);

    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                    GL_LINEAR_MIPMAP_LINEAR);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    QRect newRect = r;
    newRect.setWidth(im->width());
    newRect.setHeight(im->height());

    if (font.hasShadow)
    {
        QRect a = newRect;
        a.moveBy(font.shadowOffset.x(), font.shadowOffset.y());

        ReallyDrawText(font.shadowColor, a, CalcAlpha(font.shadowAlpha, alpha));
    }

    if (font.hasOutline)
    {
        int outalpha = alpha;
        //if (alpha < 255)
            outalpha /= 16;

        QRect a = newRect;
        a.moveBy(0 - font.outlineSize, 0 - font.outlineSize);
        ReallyDrawText(font.outlineColor, a, outalpha);

        for (int i = (0 - font.outlineSize + 1); i <= font.outlineSize; i++)
        {
            a.moveBy(1, 0);
            ReallyDrawText(font.outlineColor, a, outalpha);
        }

        for (int i = (0 - font.outlineSize + 1); i <= font.outlineSize; i++)
        {
            a.moveBy(0, 1);
            ReallyDrawText(font.outlineColor, a, outalpha);
        }

        for (int i = (0 - font.outlineSize + 1); i <= font.outlineSize; i++)
        {
            a.moveBy(-1, 0);
            ReallyDrawText(font.outlineColor, a, outalpha);
        }

        for (int i = (0 - font.outlineSize + 1); i <= font.outlineSize; i++)
        {
            a.moveBy(0, -1);
            ReallyDrawText(font.outlineColor, a, outalpha);
        }
    }

    ReallyDrawText(font.color, newRect, alpha);
}

MythImage *MythOpenGLPainter::GetFormatImage()
{
    return new MythImage(this);
}

void MythOpenGLPainter::DeleteFormatImage(MythImage *im)
{
    RemoveImageFromCache(im);
}

