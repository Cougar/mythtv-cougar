#include <qapplication.h>
#include <qdatetime.h>

#include <unistd.h>
#include <iostream>
#include <sys/types.h>
#include <sys/stat.h>
using namespace std;

#include "filetransfer.h"

#include "RingBuffer.h"
#include "libmyth/util.h"

FileTransfer::FileTransfer(QString &filename, QSocket *remote)
{
    rbuffer = new RingBuffer(filename, false);
    sock = remote;
    readthreadlive = true;
    ateof = false;
}

FileTransfer::~FileTransfer()
{
    Stop();

    if (rbuffer)
        delete rbuffer;

    readthreadLock.unlock();
}

bool FileTransfer::isOpen(void)
{
    if (rbuffer && rbuffer->IsOpen())
        return true;
    return false;
}

void FileTransfer::Stop(void)
{
    if (readthreadlive)
    {
        readthreadlive = false;
        rbuffer->StopReads();
        readthreadLock.lock();
    }
}

void FileTransfer::Pause(void)
{
    rbuffer->StopReads();
    readthreadLock.lock();
}

void FileTransfer::Unpause(void)
{
    rbuffer->StartReads();
    readthreadLock.unlock();
}

int FileTransfer::RequestBlock(int size)
{
    if (!readthreadlive || !rbuffer)
        return -1;

    char *buffer = new char[256001];
    int tot = 0;
    int ret = 0;

    readthreadLock.lock();
    while (tot < size && !rbuffer->GetStopReads() && readthreadlive)
    {
        int request = size - tot;

        if (request > 256000)
            request = 256000;

        ret = rbuffer->Read(buffer, request);
        
        if (rbuffer->GetStopReads() || ret <= 0)
            break;
            
        if (!WriteBlock(sock->socketDevice(), buffer, ret))
        {
            tot = -1;
            break;
        }

        tot += ret;
        if (ret < request)
            break; // we hit eof
    }
    readthreadLock.unlock();

    delete[] buffer;

    if (ret < 0)
        tot = -1;

    return tot;
}

long long FileTransfer::Seek(long long curpos, long long pos, int whence)
{
    if (!rbuffer)
        return -1;
    if (!readthreadlive)
        return -1;

    ateof = false;

    Pause();

    if (whence == SEEK_CUR)
    {
        long long desired = curpos + pos;
        long long realpos = rbuffer->GetReadPosition();

        pos = desired - realpos;
    }

    long long ret = rbuffer->Seek(pos, whence);

    Unpause();
    return ret;
}

long long FileTransfer::GetFileSize(void)
{
    QString filename = rbuffer->GetFilename();

    struct stat st;
    long long size = 0;

    if (stat(filename.ascii(), &st) == 0)
        size = st.st_size;

    return size;
}
