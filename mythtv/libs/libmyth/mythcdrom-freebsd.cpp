#include "mythcdrom.h"
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/cdio.h>
#include "mythcontext.h"


#define ASSUME_WANT_AUDIO 1

MediaError MythCDROMFreeBSD::eject() 
{
    if (ioctl(m_DeviceHandle, CDIOCEJECT) == 0)
        return MEDIAERR_OK;
    return MEDIAERR_FAILED;
}

bool MythCDROMFreeBSD::mediaChanged()
{  
    // Not implemented
    return false;
}

bool MythCDROMLinux::checkOK()
{
    // Not implemented
    return true;
}

// Helper function, perform a sanity check on the device
MediaError MythCDROMFreeBSD::testMedia()
{
    //cout << "MythCDROMLinux::testMedia - ";
    bool OpenedHere = false;
    if (!isDeviceOpen()) 
    {
        //cout << "Device is not open - ";
        if (!openDevice()) 
        {
            //cout << "failed to open device - ";
            if (errno == EBUSY)
            {
                //cout << "errno == EBUSY" << endl;
                return isMounted(true) ? MEDIAERR_OK : MEDIAERR_FAILED;
            } 
            else 
            { 
                return MEDIAERR_FAILED; 
            }
        }
        //cout << "Opened it - ";
        OpenedHere = true;
    }

    // Be nice and close the device if we opened it, otherwise it might be locked when the user doesn't want it to be.
    if (OpenedHere)
        closeDevice();

    return MEDIAERR_OK;
}

MediaStatus MythCDROMFreeBSD::checkMedia()
{
    return setStatus(MEDIASTAT_UNKNOWN, false);
}

MediaError MythCDROMLinux::lock() 
{
    MediaError ret = MythMediaDevice::lock();
    if (ret == MEDIAERR_OK)
#ifdef __FreeBSD__
        ioctl(m_DeviceHandle, CDIOCPREVENT);
#else
        ioctl(m_DeviceHandle, CDROM_LOCKDOOR, 1);
#endif

    return ret;
}

MediaError MythCDROMLinux::unlock() 
{
    if (openDevice()) 
    { 
        // The call to the base unlock will close it if needed.
        VERBOSE( VB_ALL, "Unlocking CDROM door");
	ioctl(m_DeviceHandle, CDIOCALLOW);
    }
    else
    {
        VERBOSE(VB_GENERAL, "Failed to open device, CDROM try will remain "
                            "locked.");
    }

    return MythMediaDevice::unlock();
}
