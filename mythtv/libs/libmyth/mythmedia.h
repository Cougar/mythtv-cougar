#ifndef MYTH_MEDIA_H
#define MYTH_MEDIA_H

#include <qobject.h>
#include <qstring.h>

typedef enum {
    MEDIASTAT_ERROR,
    MEDIASTAT_UNKNOWN,
    MEDIASTAT_OPEN,
    MEDIASTAT_USEABLE,    
    MEDIASTAT_NOTMOUNTED,
    MEDIASTAT_MOUNTED
} MediaStatus;

typedef enum {
    MEDIATYPE_UNKNOWN=1,
    MEDIATYPE_DATA=2,
    MEDIATYPE_MIXED=4,
    MEDIATYPE_AUDIO=8,
    MEDIATYPE_DVD=16,
    MEDIATYPE_VCD=32
} MediaType;

typedef enum {
    MEDIAERR_OK,
    MEDIAERR_FAILED,
    MEDIAERR_UNSUPPORTED
} MediaError;

class MythMediaDevice : public QObject
{
    Q_OBJECT
 public:
    MythMediaDevice(QObject* par, const char* DevicePath, bool SuperMount, 
                    bool AllowEject);
    virtual ~MythMediaDevice() {};

    const QString& getMountPath() const { return m_MountPath; }
    void setMountPath(const char *path) { m_MountPath = path; }

    const QString& getDevicePath() const { return m_DevicePath; }
    void setDevicePath(const char *devPath) { m_DevicePath = devPath; }

    MediaStatus getStatus() const { return m_Status; }

    const QString& getVolumeID() const { return m_VolumeID; }
    const QString& getKeyID() const { return m_KeyID; }

    bool getAllowEject() const { return m_AllowEject; }
    void setAllowEject(bool allowEject) { m_AllowEject = allowEject; }

    bool getLocked() const { return m_Locked; }

    int getDeviceHandle() const { return m_DeviceHandle; }

    bool isDeviceOpen() const;
    
    MediaType getMediaType() const { return m_MediaType; }

    bool isSuperMount() const { return m_SuperMount; }
    void setSuperMount(bool SuperMount) { m_SuperMount = SuperMount; }

    virtual MediaError  testMedia() { return MEDIAERR_UNSUPPORTED; }
    virtual bool openDevice();
    virtual bool closeDevice();
    virtual MediaStatus checkMedia() = 0; // Derived classes MUST implement this.
    virtual MediaError eject() { return MEDIAERR_UNSUPPORTED; }
    virtual MediaError lock();
    virtual MediaError unlock();
    virtual bool performMountCmd( bool DoMount );    
    
    bool mount() {  return performMountCmd(true); }
    bool unmount() { return performMountCmd(false); }
    bool isMounted(bool bVerify = false);
    
    static const char* MediaStatusStrings[];
    static const char* MediaTypeStrings[];
    static const char* MediaErrorStrings[];

 signals:
    void statusChanged(MediaStatus oldStatus, MythMediaDevice* pMedia);

 protected:
    /// Override this to perform any post mount/unmount logic.
    virtual void onDeviceMounted() {};
    virtual void onDeviceUnmounted() {};

    MediaStatus setStatus(MediaStatus newStat, bool CloseIt=false);

    QString m_MountPath;        ///< The path to this media's mount point (i.e. /mnt/cdrom). Read only.
    QString m_DevicePath;       ///< The path to this media's device (i.e. /dev/cdrom). Read/write.
    MediaStatus m_Status;       ///< The status of the media as of the last call to checkMedia. Read only.
    QString m_VolumeID;         ///< The volume ID of the media. Read Only.
    QString m_KeyID;            ///< KeyID of the media. Read Only
                                ///< for iso9660 volumeid + creation_date
    bool m_Locked;              ///< Is this media locked? Read only.
    bool m_AllowEject;          ///< Allow the user to eject the media? Read/write.
    int m_DeviceHandle;         ///< A file handle for opening and closing the device.
    MediaType m_MediaType;      ///< The type of media
    bool m_SuperMount;          ///< Is this a supermount device?
};

#endif
