#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include "RingBuffer.h"

RingBuffer::RingBuffer(const char *lfilename, bool actasnormalfile, bool write)
{
    normalfile = true;
    filename = lfilename;
    fd = -1;

    if (write)
    {
        fd = open(lfilename, O_WRONLY|O_TRUNC|O_CREAT|O_LARGEFILE, 0644);
        writemode = true;
    }
    else
    {
        fd = open(lfilename, O_RDONLY|O_LARGEFILE);
        writemode = false;
    }
}

RingBuffer::RingBuffer(const char *lfilename, long long size)
{
    normalfile = false;
    filename = lfilename;
    filesize = size;
    
    fd = -1; fd2 = -1;

    fd = open(filename.c_str(), O_WRONLY|O_CREAT|O_LARGEFILE, 0644);
    fd2 = open(filename.c_str(), O_RDONLY|O_LARGEFILE);

    totalwritepos = writepos = 0;
    totalreadpos = readpos = 0;

    wrapcount = 0;
}

RingBuffer::~RingBuffer(void)
{
    if (fd > 0)
        close(fd);
    if (fd2 > 0)
        close(fd2);
}

int RingBuffer::Read(void *buf, int count)
{
    int ret = -1;
    if (normalfile)
    {
        if (!writemode)
        {
            ret = read(fd, buf, count);
        }
        else
        {
            fprintf(stderr, "Attempt to read from a write only file\n");
        }
    }
    else
    {
        while (totalreadpos + count >= totalwritepos)
	{
            usleep(5);
	}

	if (readpos + count > filesize)
        {
	    int toread = filesize - readpos;

	    ret = read(fd2, buf, toread);
	    
	    int left = count - toread;
	    lseek(fd2, 0, SEEK_SET);

	    ret = read(fd2, (char *)buf + toread, left);
	    ret += toread;

	    totalreadpos += ret;
	    readpos = left;
	}
	else
        {
            ret = read(fd2, buf, count);
            readpos += ret;
	    totalreadpos += ret;
        }
    }
    return ret;
}

int RingBuffer::Write(const void *buf, int count)
{
    int ret = -1;

    if (normalfile)
    {
        if (writemode)
        {
            ret = write(fd, buf, count);
        }
        else
        {
            fprintf(stderr, "Attempt to write to a read only file\n");
        }
    }
    else
    {
        if (writepos + count > filesize)
        {
	    int towrite = filesize - writepos;
	    ret = write(fd, buf, towrite);

	    int left = count - towrite;
	    lseek(fd, 0, SEEK_SET);

	    ret = write(fd, (char *)buf + towrite, left);
	    writepos = left;

	    ret += towrite;

	    totalwritepos += ret;
	    wrapcount++;
        }
	else
	{
            ret = write(fd, buf, count);
            writepos += ret;
	    totalwritepos += ret;
	}
    }

    return ret;
}

long long RingBuffer::Seek(long pos, int whence)
{
    long long ret = -1;
    if (normalfile)
    {
        ret = lseek(fd, pos, whence);
    }
    else
    {
        ret = lseek(fd2, pos, whence);

	if (whence == SEEK_SET)
	    readpos = ret;
	else if (whence == SEEK_CUR)
            readpos += pos;
    }
    return ret;
}
