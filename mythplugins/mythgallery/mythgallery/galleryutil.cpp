/* ============================================================
 * File  : exifutil.cpp
 * Description : 
 * 

 * This program is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General
 * Public License as published bythe Free Software Foundation;
 * either version 2, or (at your option)
 * any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * ============================================================ */

#include <qfileinfo.h>
#include <qdir.h>

#include <mythtv/mythcontext.h>
#include <mythtv/mythdbcon.h>
#include <mythtv/util.h>

#include "config.h"
#include "constants.h"
#include "galleryutil.h"
#include "thumbgenerator.h"

#ifdef EXIF_SUPPORT
#include <libexif/exif-data.h>
#include <libexif/exif-entry.h>
// include "exif.hpp"
#endif // EXIF_SUPPORT

#define LOC QString("GalleryUtil:")
#define LOC_ERR QString("GalleryUtil, Error:")

static QFileInfo MakeUnique(const QFileInfo &dest);
static QFileInfo MakeUniqueDirectory(const QFileInfo &dest);
static bool FileCopy(const QFileInfo &src, const QFileInfo &dst);
static bool FileMove(const QFileInfo &src, const QFileInfo &dst);
static bool FileDelete(const QFileInfo &file);

bool GalleryUtil::isImage(const char* filePath)
{
    QFileInfo fi(filePath);
    return !fi.isDir() && IMAGE_FILENAMES.find(fi.extension(FALSE)) != -1;
}

bool GalleryUtil::isMovie(const char* filePath)
{
    QFileInfo fi(filePath);
    return !fi.isDir() && MOVIE_FILENAMES.find(fi.extension(FALSE)) != -1;
}

long GalleryUtil::GetNaturalRotation(const char* filePath)
{
    long rotateAngle = 0;

    try
    {
#ifdef EXIF_SUPPORT
        char *exifvalue = new char[1024];
        ExifData *data = exif_data_new_from_file (filePath);
        if (data)
        {
            for (int i = 0; i < EXIF_IFD_COUNT; i++)
            {
                    ExifEntry *entry = exif_content_get_entry (data->ifd[i],
                                                        EXIF_TAG_ORIENTATION);
                    if (entry)
                    {
#if NEW_LIB_EXIF
                        exif_entry_get_value(entry, exifvalue, 1023);
                        QString value = exifvalue;
#else
                        QString value = exif_entry_get_value(entry);
#endif
                        if (value == "left - bottom")
                        {
                          rotateAngle = -90;
                        }
                        else if (value == "right - top")
                        {
                          rotateAngle = 90;
                        }
                        break;
                    }
            }
            exif_data_free(data);
        }
        else
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR +
                    QString("Could not load exif data from '%1'")
                    .arg(filePath));
        }
        
        delete [] exifvalue;
        
        /*
        Exiv2::ExifData exifData;
         int rc = exifData.read(filePath);
        if (!rc)
        {
            Exiv2::ExifKey key = Exiv2::ExifKey("Exif.Image.Orientation");
            Exiv2::ExifData::iterator pos = exifData.findKey(key);
            if (pos != exifData.end())
            {
                long orientation = pos->toLong();
                switch (orientation)
                {
                    case 6:
                        rotateAngle = 90;
                        break;
                    case 8:
                        rotateAngle = -90;
                        break;
                    default:
                        rotateAngle = 0;
                        break;
                }
            }
        }
        */
#endif // EXIF_SUPPORT
    }
    catch (...)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR +
                QString("Failed to extract EXIF headers from '%1'")
                .arg(filePath));
    }

    return rotateAngle;
}

bool GalleryUtil::LoadDirectory(ThumbList& itemList, const QString& dir,
                                int sortorder, bool recurse,
                                ThumbDict *itemDict, ThumbGenerator* thumbGen)
{
    QString blah = dir;
    QDir d(blah);
    QString currDir = d.absPath();

    bool isGallery;
    QFileInfoList gList = d.entryInfoList("serial*.dat", QDir::Files);
    isGallery = (gList.count() != 0);

    // Create .thumbcache dir if neccesary
    if (thumbGen)
        thumbGen->getThumbcacheDir(currDir);

    d.setNameFilter(MEDIA_FILENAMES);
    d.setSorting((QDir::SortFlag)sortorder);

    d.setMatchAllDirs(true);
    QFileInfoList list = d.entryInfoList();

    if (list.isEmpty())
        return false;

    QFileInfoList::const_iterator it = list.begin();
    const QFileInfo *fi;

    if (thumbGen) 
    {
        thumbGen->cancel();
        thumbGen->setDirectory(currDir, isGallery);
    }

    while (it != list.end())
    {
        fi = &(*it);
        ++it;
        if (fi->fileName() == "." || fi->fileName() == "..")
            continue;

        // remove these already-resized pictures.
        if (isGallery && (
                (fi->fileName().find(".thumb.") > 0) ||
                (fi->fileName().find(".sized.") > 0) ||
                (fi->fileName().find(".highlight.") > 0)))
            continue;

        if (fi->isDir() && recurse) 
        {
            GalleryUtil::LoadDirectory(
                itemList, QDir::cleanDirPath(fi->absFilePath()),
                sortorder, true, itemDict, thumbGen);
        }
        else 
        {
            ThumbItem *item = new ThumbItem(
                fi->fileName(),
                QDir::cleanDirPath(fi->absFilePath()), fi->isDir());

            itemList.append(item);

            if (itemDict)
                itemDict->insert(item->GetName(), item);

            if (thumbGen)
                thumbGen->addFile(item->GetName());
        }
    }

    return isGallery;
}

QString GalleryUtil::GetCaption(const QString &filePath)
{
    QString caption("");

    try
    {
#ifdef EXIF_SUPPORT
        char *exifvalue = new char[1024];
        ExifData *data = exif_data_new_from_file (filePath);
        if (data)
        {
            for (int i = 0; i < EXIF_IFD_COUNT; i++)
            {
                ExifEntry *entry = exif_content_get_entry (data->ifd[i],
                                                    EXIF_TAG_USER_COMMENT);
                if (entry)
                {
#if NEW_LIB_EXIF
                    exif_entry_get_value(entry, exifvalue, 1023);
                    caption = exifvalue;
#else           
                    caption = exif_entry_get_value(entry);
#endif          
                    // Found one, done
                    if(!caption.isEmpty())
                       break;
                }

                entry = exif_content_get_entry (data->ifd[i],
                                                EXIF_TAG_IMAGE_DESCRIPTION);
                if (entry)
                {
#if NEW_LIB_EXIF
                    exif_entry_get_value(entry, exifvalue, 1023);
                    caption = exifvalue;
#else           
                    caption = exif_entry_get_value(entry);
#endif          
                    // Found one, done
                    if(!caption.isEmpty())
                       break;
                }
            }
            exif_data_free(data);
        }
        else
        {
           VERBOSE(VB_IMPORTANT, LOC_ERR +
                   QString("Could not load exif data from '%1'")
                   .arg(filePath));
        }
        
        delete [] exifvalue;
#endif // EXIF_SUPPORT
    }
    catch (...)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR +
                QString("Failed to extract EXIF headers from '%1'")
                .arg(filePath));
    }

    return caption;
}

bool GalleryUtil::Copy(const QFileInfo &src, QFileInfo &dst)
{
    if (src.isDir())
        return CopyDirectory(src, dst);

    dst = MakeUnique(dst);

    if (!FileCopy(src, dst))
        return false;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("INSERT INTO gallerymetadata (image, angle) "
                  "SELECT :IMAGENEW , angle "
                  "FROM gallerymetadata "
                  "WHERE image = :IMAGEOLD");
    query.bindValue(":IMAGENEW", dst.absFilePath());
    query.bindValue(":IMAGEOLD", src.absFilePath());
    if (query.exec())
        return true;

    // try to undo copy on DB failure
    FileDelete(dst);
    return false;
}

bool GalleryUtil::Move(const QFileInfo &src, QFileInfo &dst)
{
    if (src.isDir())
        return MoveDirectory(src, dst);

    dst = MakeUnique(dst);

    if (!FileMove(src, dst))
        return false;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("UPDATE gallerymetadata "
                  "SET image = :IMAGENEW "
                  "WHERE image = :IMAGEOLD");
    query.bindValue(":IMAGENEW", dst.absFilePath());
    query.bindValue(":IMAGEOLD", src.absFilePath());
    if (query.exec())
        return true;

    // try to undo move on DB failure
    FileMove(dst, src);
    return false;
}

bool GalleryUtil::Delete(const QFileInfo &file)
{
    if (!file.exists())
        return false;

    if (file.isDir())
        return DeleteDirectory(file);

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("DELETE FROM gallerymetadata "
                  "WHERE image = :IMAGE ;");
    query.bindValue(":IMAGE", file.absFilePath());
    if (query.exec())
        return FileDelete(file);

    return false;
}

bool GalleryUtil::Rename(const QString &currDir, const QString &oldName,
                         const QString &newName)
{
    // make sure there isn't already a file/directory with the same name
    QFileInfo fi(currDir + '/' + newName);
    if (fi.exists())
        return false;

    fi.setFile(currDir + '/' + oldName);
    if (fi.isDir())
        return RenameDirectory(currDir, oldName, newName);

    // rename the file
    QDir cdir(currDir);
    if (!cdir.rename(oldName, newName))
        return false;

    // rename the file's thumbnail if it exists
    if (QFile::exists(currDir + "/.thumbcache/" + oldName))
    {
        QDir d(currDir + "/.thumbcache/");
        d.rename(oldName, newName);
    }

    int prefixLen = gContext->GetSetting("GalleryDir").length();
    QString path = gContext->GetConfDir() + "/MythGallery";
    path += currDir.right(currDir.length() - prefixLen);
    path += QString("/.thumbcache/");
    if (QFile::exists(path + oldName))
    {
        QDir d(path);
        d.rename(oldName, newName);
    }

    // fix up the metadata in the database
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("UPDATE gallerymetadata "
                  "SET image = :IMAGENEW "
                  "WHERE image = :IMAGEOLD");
    query.bindValue(":IMAGENEW", QString(currDir + '/' + newName));
    query.bindValue(":IMAGEOLD", QString(currDir + '/' + oldName));
    if (query.exec())
        return true;

    // try to undo rename on DB failure
    cdir.rename(newName, oldName);
    return false;
}

QSize GalleryUtil::ScaleToDest(const QSize &src, const QSize &dest, bool scaleMax)
{
    QSize sz = src;

    // calculate screen pixel aspect ratio
    double pixelAspect = MythGetPixelAspectRatio();

    // calculate image aspect ratio
    double imageAspect = 1.0;
    if ((sz.width() > 0) && (sz.height() > 0))
        imageAspect = (double)sz.width() / (double)sz.height();

    int scaleWidth;
    int scaleHeight;
    if (scaleMax)
    {
        // scale-max to dest width for most images
        scaleWidth = dest.width();
        scaleHeight = (int)((float)dest.width() * pixelAspect / imageAspect);
        if (scaleHeight < dest.height())
        {
            // scale-max to dest height for extra wide images
            scaleWidth = (int)((float)dest.height() * imageAspect / pixelAspect);
            scaleHeight = dest.height();
        }
    }
    else
    {
        // scale-min to dest height for most images
        scaleWidth = (int)((float)dest.height() * imageAspect / pixelAspect);
        scaleHeight = dest.height();
        if (scaleWidth > dest.width())
        {
            // scale-min to dest width for extra wide images
            scaleWidth = dest.width();
            scaleHeight = (int)((float)dest.width() * pixelAspect / imageAspect);
        }
    }

    sz.scale(scaleWidth, scaleHeight, Qt::KeepAspectRatio);
    return sz;
}

bool GalleryUtil::CopyDirectory(const QFileInfo src, QFileInfo &dst)
{
    QDir srcDir(src.absFilePath());

    dst = MakeUniqueDirectory(dst);
    if (!dst.exists())
    {
        srcDir.mkdir(dst.absFilePath());
        dst.refresh();
    }

    if (!dst.exists() || !dst.isDir())
        return false;

    bool ok = true;
    QDir dstDir(dst.absFilePath());
    QFileInfoList list = srcDir.entryInfoList();
    QFileInfoList::const_iterator it = list.begin();
    for (; it != list.end(); ++it)
    {
        const QString fn = it->fileName();
        if (fn != "." && fn != "..")
        {
            QFileInfo dfi(dstDir, fn);
            ok &= Copy(*it, dfi);
        }
    }

    return ok;
}

bool GalleryUtil::MoveDirectory(const QFileInfo src, QFileInfo &dst)
{
    QDir srcDir(src.absFilePath());

    dst = MakeUniqueDirectory(dst);
    if (!dst.exists())
    {
        srcDir.mkdir(dst.absFilePath());
        dst.refresh();
    }

    if (!dst.exists() || !dst.isDir())
        return false;

    bool ok = true;
    QDir dstDir(dst.absFilePath());
    QFileInfoList list = srcDir.entryInfoList();
    QFileInfoList::const_iterator it = list.begin();
    for (; it != list.end(); ++it)
    {
        const QString fn = it->fileName();
        if (fn != "." && fn != "..")
        {
            QFileInfo dfi(dstDir, fn);
            ok &= Move(*it, dfi);
        }
    }

    return ok && FileDelete(src);
}

bool GalleryUtil::DeleteDirectory(const QFileInfo &dir)
{
    if (!dir.exists())
        return false;

    QDir srcDir(dir.absFilePath());
    QFileInfoList list = srcDir.entryInfoList();
    QFileInfoList::const_iterator it = list.begin();
    for (; it != list.end(); ++it)
    {
        const QString fn = it->fileName();
        if (fn != "." && fn != "..")
            Delete(*it);
    }

    return FileDelete(dir);
}

bool GalleryUtil::RenameDirectory(const QString &currDir, const QString &oldName, 
                                const QString &newName)
{
    // rename the directory
    QDir cdir(currDir);
    if (!cdir.rename(oldName, newName))
        return false;

    // rename the directory's thumbnail if it exists in the parent directory
    if (QFile::exists(currDir + "/.thumbcache/" + oldName))
    {
        QDir d(currDir + "/.thumbcache/");
        d.rename(oldName, newName);
    }

    // also look in HOME directory for any thumbnails
    int prefixLen = gContext->GetSetting("GalleryDir").length();
    QString path = gContext->GetConfDir() + "/MythGallery";
    path += currDir.right(currDir.length() - prefixLen) + '/';
    if (QFile::exists(path + oldName))
    {
        QDir d(path);
        d.rename(oldName, newName);

        // rename this directory's thumbnail
        path += QString(".thumbcache/");
        if (QFile::exists(path + oldName))
        {
            QDir d(path);
            d.rename(oldName, newName);
        }
    }

    // fix up the metadata in the database
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT image, angle FROM gallerymetadata "
                  "WHERE image LIKE :IMAGEOLD");
    query.bindValue(":IMAGEOLD", QString(currDir + '/' + oldName + '%'));
    if (query.exec())
    {
        while (query.next())
        {
            QString oldImage = query.value(0).asString();
            QString newImage = oldImage;
            newImage = newImage.replace(currDir + '/' + oldName,
                                        currDir + '/' + newName);

            MSqlQuery subquery(MSqlQuery::InitCon());
            subquery.prepare("UPDATE gallerymetadata "
                        "SET image = :IMAGENEW "
                        "WHERE image = :IMAGEOLD");
            subquery.bindValue(":IMAGENEW", newImage);
            subquery.bindValue(":IMAGEOLD", oldImage);
            subquery.exec();
        }
    }

    return true;
}

static QFileInfo MakeUnique(const QFileInfo &dest)
{
    QFileInfo newDest = dest;

    for (uint i = 0; newDest.exists(); i++)
    {
        QString basename = QString("%1_%2.%3")
            .arg(dest.baseName()).arg(i).arg(dest.extension());

        newDest.setFile(dest.dir(), basename);

        VERBOSE(VB_GENERAL, LOC_ERR +
                QString("Need to find a new name for '%1' trying '%2'")
                .arg(dest.absFilePath()).arg(newDest.absFilePath()));
    }

    return newDest;
}

static QFileInfo MakeUniqueDirectory(const QFileInfo &dest)
{
    QFileInfo newDest = dest;

    for (uint i = 0; newDest.exists() && !newDest.isDir(); i++)
    {
        QString fullname = QString("%1_%2").arg(dest.absFilePath()).arg(i);
        newDest.setFile(fullname);

        VERBOSE(VB_GENERAL, LOC_ERR +
                QString("Need to find a new name for '%1' trying '%2'")
                .arg(dest.absFilePath()).arg(newDest.absFilePath()));
    }

    return newDest;
}

static bool FileCopy(const QFileInfo &src, const QFileInfo &dst)
{
    const int bufferSize = 16*1024;

    QFile s(src.absFilePath());
    QFile d(dst.absFilePath());
    char buffer[bufferSize];
    int len;

    if (!s.open(QIODevice::ReadOnly))
        return false;

    if (!d.open(QIODevice::WriteOnly))
    {
        s.close();
        return false;
    }

    len = s.readBlock(buffer, bufferSize);
    do
    {
        d.writeBlock(buffer, len);
        len = s.readBlock(buffer, bufferSize);
    } while (len > 0);

    s.close();
    d.close();

    return true;
}

static bool FileMove(const QFileInfo &src, const QFileInfo &dst)
{
    // attempt to rename the file,
    // this will fail if files are on different partitions
    if (rename(src.absFilePath().local8Bit(),
               dst.absFilePath().local8Bit()) == 0)
    {
        return true;
    }

    // src and dst are on different mount points, move manually.
    if (errno == EXDEV)
    {
        if (FileCopy(src, dst))
            return FileDelete(src);
    }

    return false;
}

static bool FileDelete(const QFileInfo &file)
{
    if (!file.isDir())
        return QFile::remove(file.absFilePath());

    // delete .thumbcache
    QDir srcDir(file.absFilePath());
    QFileInfo tc(srcDir, ".thumbcache");
    GalleryUtil::Delete(tc);

    srcDir.rmdir(srcDir.absPath());

    return true;
}
