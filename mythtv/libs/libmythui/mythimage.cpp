#include <cassert>

#include "mythimage.h"
#include "mythmainwindow.h"
#include "mythcontext.h"

MythImage::MythImage(MythPainter *parent)
{
    assert(parent);

    m_Parent = parent;
    m_Changed = false;

    m_RefCount = 0;
}

MythImage::~MythImage()
{
    m_Parent->DeleteFormatImage(this);
}

// these technically should be locked, but all deletion should be happening in the UI thread, and nowhere else.
void MythImage::UpRef(void)
{
    m_RefCount++;
}

bool MythImage::DownRef(void)
{
    m_RefCount--;
    if (m_RefCount < 0)
    {
        delete this;
        return true;
    }
    return false;
}

void MythImage::Assign(const QImage &img)
{
    *(QImage *)this = img;
    SetChanged();
}

void MythImage::Assign(const QPixmap &pix)
{
    Assign(pix.convertToImage());
}

MythImage *MythImage::FromQImage(QImage **img)
{
    if (!img || !*img)
        return NULL;

    MythImage *ret = GetMythPainter()->GetFormatImage();
    ret->Assign(**img);
    delete *img;
    *img = NULL;
    return ret;
}

// FIXME: Get rid of LoadScaleImage
bool MythImage::Load(const QString &filename)
{
    QImage *im = gContext->LoadScaleImage(filename);
    if (im)
    {
        Assign(*im);
        delete im;
        return true;
    }

    return false;
}

