#include "mythmedia.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "qfile.h"
#include "qtextstream.h"

#include "mythcontext.h"

#include <cstdio>
#include <iostream>
#include <cstdlib>
#include <sys/param.h>

using namespace std;

// end for testing

#define PATHTO_MOUNT "/bin/mount"
#define PATHTO_UNMOUNT "/bin/umount"
#define PATHTO_MOUNTS "/proc/mounts"

const char* MythMediaDevice::MediaStatusStrings[] = { "MEDIASTAT_ERROR", "MEDIASTAT_UNKNOWN", "MEDIASTAT_OPEN", "MEDIASTAT_USEABLE", "MEDIASTAT_NOTMOUNTED", "MEDIASTAT_MOUNTED" };
const char* MythMediaDevice::MediaTypeStrings[] = { "MEDIATYPE_UNKNOWN", "MEDIATYPE_DATA", "MEDIATYPE_MIXED", "MEDIATYPE_AUDIO", "MEDIATYPE_DVD", "MEDIATYPE_VCD" };
const char* MythMediaDevice::MediaErrorStrings[] = { "MEDIAERR_OK", "MEDIAERR_FAILED", "MEDIAERR_UNSUPPORTED" };

MythMediaDevice::MythMediaDevice(QObject* par, const char* DevicePath, 
                                 bool SuperMount,  bool AllowEject) 
               : QObject(par)
{
    m_DevicePath = DevicePath;
    m_AllowEject = AllowEject;
    m_Locked = false;
    m_DeviceHandle = -1;
    m_SuperMount = SuperMount;
    m_Status = (isMounted(true)) ? MEDIASTAT_MOUNTED : MEDIASTAT_NOTMOUNTED;
}

bool MythMediaDevice::openDevice()
{
    // Sanity check
    if (isDeviceOpen())
        return true;
 
    m_DeviceHandle = open(m_DevicePath, O_RDONLY | O_NONBLOCK);
    
    return isDeviceOpen();
}

bool MythMediaDevice::closeDevice()
{
    // Sanity check
    if (!isDeviceOpen())
        return true;

    int ret = close(m_DeviceHandle);
    m_DeviceHandle = -1;
    
    return (ret != -1) ? true : false;
}

bool MythMediaDevice::isDeviceOpen() const 
{ 
    return (m_DeviceHandle >= 0) ? true : false; 
}

bool MythMediaDevice::performMountCmd(bool DoMount)
{
    QString MountCommand;
    
    if (isDeviceOpen())
        closeDevice();

    if (!m_SuperMount) 
    {
        // Build a command line for mount/unmount and execute it...  Is there a better way to do this?
        if (DoMount)
            MountCommand.sprintf("%s %s", PATHTO_MOUNT, 
                                 (const char*)m_DevicePath);
        else
            MountCommand.sprintf("%s %s", PATHTO_UNMOUNT, 
                                 (const char*)m_DevicePath);
    
        VERBOSE(VB_ALL,  QString("Executing '%1'").arg(MountCommand));
        if (0 == system(MountCommand)) 
        {
            if (DoMount)
            {
                m_Status = MEDIASTAT_MOUNTED;
                onDeviceMounted();
            }
            else
                onDeviceUnmounted();
            return true;
        }
        else
        {
            VERBOSE(VB_GENERAL, QString("Failed to mount %1.")
                                       .arg(m_DevicePath));
        }
    } 
    else 
    {
        VERBOSE( VB_ALL,  "Disk iserted on a supermount device" );
        // If it's a super mount then the OS will handle mounting /  unmounting.
        // We just need to give derived classes a chance to perform their 
        // mount / unmount logic.
        if (DoMount)
            onDeviceMounted();
        else
            onDeviceUnmounted();
        return true;
    }
    return false;
}

MediaError MythMediaDevice::lock() 
{ 
    // We just open the device here, which may or may not do the trick, derived classes can do more...
    if (openDevice()) 
    {
        m_Locked = true;
        return MEDIAERR_OK;
    }
    m_Locked = false;
    return MEDIAERR_FAILED;
}

MediaError MythMediaDevice::unlock()
{ 
    m_Locked = false;
    
    return MEDIAERR_OK;
}

bool MythMediaDevice::isMounted(bool Verify)
{
    if (!Verify)
        return (m_Status == MEDIASTAT_MOUNTED);

    QFile Mounts(PATHTO_MOUNTS);
    char lpath[PATH_MAX];

    // Try to open the mounts file so we can search it for our device.
    if (Mounts.open(IO_ReadOnly)) 
    {
        QTextStream stream(&Mounts);
        QString line;

        while (!stream.eof()) 
        {
            QString MountPoint;
            QString DeviceName;
            
            // Extract the mount point and device name.
            stream >> DeviceName >> MountPoint;
            //cout << "Found Device: " << DeviceName << "  Mountpoint: " << MountPoint << endl; 

            // Skip the rest of the line
            line = stream.readLine();
            
            // Now lets see if we're mounted...
            int len = readlink(DeviceName, lpath, PATH_MAX);
            if (len > 0 && len < PATH_MAX)
                lpath[len] = 0;

            if (m_DevicePath == DeviceName || m_DevicePath == lpath) 
            {
                m_MountPath = MountPoint;
                Mounts.close();
                return true;
            }
        }
        
        Mounts.close();
    }

    return false;
}

MediaStatus MythMediaDevice::setStatus( MediaStatus NewStatus, bool CloseIt )
{
    MediaStatus OldStatus = m_Status;

    m_Status = NewStatus;

    // If the status is changed we need to take some actions depending on the old and new status.
    if (NewStatus != OldStatus) 
    {
        switch (NewStatus) 
        {
            // the disk is not / should not be mounted.
            case MEDIASTAT_ERROR:
            case MEDIASTAT_OPEN:
                if (MEDIASTAT_MOUNTED == OldStatus)
                    unmount();
                break;
            case MEDIASTAT_NOTMOUNTED:
                if (MEDIASTAT_MOUNTED == OldStatus)
                    unmount();
                // If we're here it's possible there's unmounted media in the device so try to mount it.
                // The case of no media in the deivce whould be handeled by derived classes and sent
                // as MEDIASTAT_OPEN, MEDISTAT_ERROR or MEDIASTAT_UNKNOWN.
                mount();
                break;
            case MEDIASTAT_UNKNOWN:
            case MEDIASTAT_USEABLE:
            case MEDIASTAT_MOUNTED:
                // get rid of the compiler warning...
                break;
        }
        
        // Don't fire off transitions to / from unknown states
        if (m_Status != MEDIASTAT_UNKNOWN && OldStatus != MEDIASTAT_UNKNOWN)
            emit statusChanged(OldStatus, this);
    }


    if (CloseIt)
        closeDevice();

    return m_Status;
}

