/* ============================================================
 * File  : iconview.h
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

#ifndef ICONVIEW_H
#define ICONVIEW_H

#include <vector>

// Qt headers
#include <QStringList>
#include <QList>
#include <QHash>

// MythTV headers
#include <mythtv/libmythui/mythscreentype.h>
#include <mythtv/libmythui/mythuitext.h>
#include <mythtv/libmythui/mythuibuttonlist.h>
#include <mythtv/libmythui/mythuiimage.h>
#include <mythtv/libmythui/mythdialogbox.h>
#include <mythtv/mythmedia.h>

// MythGallery headers
#include "thumbview.h"

using namespace std;

class ThumbGenerator;
class MediaMonitor;

Q_DECLARE_METATYPE(ThumbItem*)

class IconView : public MythScreenType
{
    Q_OBJECT

  public:
    IconView(MythScreenStack *parent, const char *name,
             const QString &galleryDir, MythMediaDevice *initialDevice);
    ~IconView();

    bool Create(void);
    bool keyPressEvent(QKeyEvent *);
    void customEvent(QEvent*);

    QString GetError(void) { return m_errorStr; }

  private:
    void SetupMediaMonitor(void);

    void LoadDirectory(const QString &dir);

    bool HandleEscape(void);
    bool HandleMediaEscape(MediaMonitor*);
    bool HandleSubDirEscape(const QString &parent);

    bool HandleMediaDeviceSelect(ThumbItem *item);
    bool HandleImageSelect(const QString &action);

    void HandleMainMenu(void);
    void HandleSubMenuMetadata(void);
    void HandleSubMenuMark(void);
    void HandleSubMenuFile(void);

  private slots:
    void HandleRotateCW(void);
    void HandleRotateCCW(void);
    void HandleDeleteCurrent(void);
    void HandleSlideShow(void);
    void HandleRandomShow(void);
    void HandleSettings(void);
    void HandleImport(void);
    void HandleShowDevices(void);
    void HandleCopyHere(void);
    void HandleMoveHere(void);
    void HandleDelete(void);
    void HandleDeleteMarked(void);
    void HandleClearMarked(void);
    void HandleSelectAll(void);
    void HandleMkDir(void);
    void HandleRename(void);

    void DoMkDir(QString folderName);
    void DoDeleteMarked(bool doDelete);
    void DoRename(QString folderName);
    void DoDeleteCurrent(bool doDelete);

  private:
    void LoadThumbnail(ThumbItem *item);
    void ImportFromDir(const QString &fromDir, const QString &toDir);
    void CopyMarkedFiles(bool move = false);
    void ShowOKDialog(const QString &message, const char *slot, bool showCancel = false);
    ThumbItem *GetCurrentThumb(void);

    QList<ThumbItem*>           m_itemList;
    QHash<QString, ThumbItem*>  m_itemHash;
    QStringList         m_itemMarked;
    QString             m_galleryDir;
    vector<int>         m_history;

    MythUIButtonList   *m_imageList;
    MythUIText         *m_captionText;
    MythUIText         *m_noImagesText;
    MythDialogBox      *m_menuPopup;
    MythScreenStack    *m_popupStack;

    bool                m_isGallery;
    bool                m_showDevices;
    QString             m_currDir;
    MythMediaDevice    *m_currDevice;

    ThumbGenerator     *m_thumbGen;

    int                 m_showcaption;
    int                 m_sortorder;
    bool                m_useOpenGL;
    bool                m_recurse;
    QStringList         m_paths;

    QString             m_errorStr;

  public slots:
    void mediaStatusChanged(MediaStatus oldStatus, MythMediaDevice *pMedia);
    void HandleItemSelect(MythUIButtonListItem *);
    void UpdateText(MythUIButtonListItem *);

    friend class FileCopyThread;
};


#endif /* ICONVIEW_H */
