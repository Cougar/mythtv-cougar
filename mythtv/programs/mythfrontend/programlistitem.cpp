#include <qimage.h>
#include <qpixmap.h>
#include <qapplication.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>

#include "tv.h"
#include "infostructs.h"
#include "programlistitem.h"

void MyListView::keyPressEvent(QKeyEvent *e)
{
    if (e->key() == Key_Space) 
    {
        if ( currentItem() && !currentItem()->isEnabled() )
        {
        }
        else
        {   
            emit spacePressed( currentItem() );
            return;
        }
    }

    QListView::keyPressEvent(e);
}

ProgramListItem::ProgramListItem(QListView *parent, ProgramInfo *lpginfo,
                                 RecordingInfo *lrecinfo, int numcols,
                                 TV *ltv, QString lprefix)
               : QListViewItem(parent)
{
    prefix = lprefix;
    tv = ltv;
    pginfo = lpginfo;
    recinfo = lrecinfo;
    pixmap = NULL;

    setText(0, pginfo->channum);
    setText(1, pginfo->startts.toString("MMMM d h:mm AP"));
    setText(2, pginfo->title);

    if (numcols == 4)
    {
        string filename;

        recinfo->GetRecordFilename(prefix.ascii(), filename);

        struct stat64 st;
 
        long long size = 0;
        if (stat64(filename.c_str(), &st) == 0)
            size = st.st_size;
        long int mbytes = size / 1024 / 1024;
        QString filesize = QString("%1 MB").arg(mbytes);

        setText(3, filesize);
    }
}

QPixmap *ProgramListItem::getPixmap(void)
{
    if (pixmap)
        return pixmap;

    string filename;
    recinfo->GetRecordFilename(prefix.ascii(), filename);
    filename += ".png";

    pixmap = new QPixmap();

    int screenheight = QApplication::desktop()->height();
    int screenwidth = QApplication::desktop()->width();
    
    float wmult = screenwidth / 800.0;
    float hmult = screenheight / 600.0;

    QImage *tmpimage = new QImage();

    if (tmpimage->load(filename.c_str()))
    {
        if (screenwidth != 800 || screenheight != 600)
        {
            QImage tmp2 = tmpimage->smoothScale(160 * wmult, 120 * hmult);
            pixmap->convertFromImage(tmp2);
        }
        else
            pixmap->convertFromImage(*tmpimage);

        delete tmpimage;

        return pixmap;
    }

cout << "generating pixmap\n";

    int len = 0;
    int video_width, video_height;
    unsigned char *data = (unsigned char *)tv->GetScreenGrab(recinfo, 65, len,
                                                             video_width,
                                                             video_height);

    if (data)
    {
cout << "converting to png\n";
        QImage img(data, video_width, video_height, 32, NULL, 65536 * 65536, 
                   QImage::LittleEndian);
        img = img.smoothScale(160, 120);

        img.save(filename.c_str(), "PNG");

cout << "done\n";
        delete [] data;
    }

cout << "loading\n";
    if (tmpimage->load(filename.c_str()))
    {
        if (screenwidth != 800 || screenheight != 600)
        {
            QImage tmp2 = tmpimage->smoothScale(160 * wmult, 120 * hmult);
            pixmap->convertFromImage(tmp2);
        }
        else
            pixmap->convertFromImage(*tmpimage);

        delete tmpimage;
    
cout << "done\n";    
        return pixmap;
    }

    delete tmpimage;
    delete pixmap;

    return NULL;
}

void ProgramListItem::paintCell(QPainter *p, const QColorGroup &cg,
                                int column, int width, int alignment)
{
    QColorGroup _cg(cg);
    QColor c = _cg.text();

    if (!pginfo->recording)
    {
        _cg.setColor(QColorGroup::Text, Qt::gray);
        _cg.setColor(QColorGroup::HighlightedText, Qt::gray);
    }
    else if (pginfo->conflicting)
    {
        _cg.setColor(QColorGroup::Text, Qt::red);
        _cg.setColor(QColorGroup::HighlightedText, Qt::red);
    }

    QListViewItem::paintCell(p, _cg, column, width, alignment);

    _cg.setColor(QColorGroup::Text, c);
}
