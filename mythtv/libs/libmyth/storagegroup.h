#ifndef _STORAGEGROUP_H
#define _STORAGEGROUP_H

#include <QStringList>

#include "settings.h"
#include "mythwidgets.h"

class MPUBLIC StorageGroup: public ConfigurationWizard
{
  public:
    StorageGroup(const QString group = "", const QString hostname = "");

    void    Init(const QString group = "Default",
                 const QString hostname = "");

    QString getName(void) const
        { QString tmp = m_groupname; tmp.detach(); return tmp; }

    QStringList GetDirList(void) const
        { QStringList tmp = m_dirlist; tmp.detach(); return tmp; }

    QStringList GetFileList(QString Path);
    bool FileExists(QString filename);
    QStringList GetFileInfo(QString filename);

    QString FindRecordingFile(QString filename);
    QString FindRecordingDir(QString filename);

    QString FindNextDirMostFree(void);

    static void CheckAllStorageGroupDirs(void);

    static const char *kDefaultStorageDir;
    static const QStringList kSpecialGroups;

    static QStringList getRecordingsGroups(void);
    static QStringList getGroupDirs(QString groupname, QString host);

  private:
    QString      m_groupname;
    QString      m_hostname;
    QStringList  m_dirlist;
};

class MPUBLIC StorageGroupEditor :
    public QObject, public ConfigurationDialog
{
    Q_OBJECT
  public:
    StorageGroupEditor(QString group);
    virtual DialogCode exec(void);
    virtual void Load(void);
    virtual void Save(void) { }
    virtual void Save(QString) { }
    virtual MythDialog* dialogWidget(MythMainWindow* parent,
                                     const char* widgetname=0);

  protected slots:
    void open(QString name);
    void doDelete(void);

  protected:
    QString         m_group;
    ListBoxSetting *listbox;
    QString         lastValue;
};

class MPUBLIC StorageGroupListEditor :
    public QObject, public ConfigurationDialog
{
    Q_OBJECT
  public:
    StorageGroupListEditor(void);
    virtual DialogCode exec(void);
    virtual void Load(void);
    virtual void Save(void) { }
    virtual void Save(QString) { }
    virtual MythDialog* dialogWidget(MythMainWindow* parent,
                                     const char* widgetname=0);

  protected slots:
    void open(QString name);
    void doDelete(void);

  protected:
    ListBoxSetting *listbox;
    QString         lastValue;
};

#endif

/* vim: set expandtab tabstop=4 shiftwidth=4: */
