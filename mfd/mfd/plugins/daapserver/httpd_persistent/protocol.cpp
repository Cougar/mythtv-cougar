/*
** Copyright (c) 2002  Hughes Technologies Pty Ltd.  All rights
** reserved.
**
** Terms under which this software may be used or copied are
** provided in the  specific license associated with this product.
**
** Hughes Technologies disclaims all warranties with regard to this
** software, including all implied warranties of merchantability and
** fitness, in no event shall Hughes Technologies be liable for any
** special, indirect or consequential damages or any damages whatsoever
** resulting from loss of use, data or profits, whether in an action of
** contract, negligence or other tortious action, arising out of or in
** connection with the use or performance of this software.
**
**
** $Id$
**
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>

#if defined(_WIN32) 
#else
#include <unistd.h>
#include <sys/file.h>
#endif

#ifdef __sgi__
#include <snprintf.h>
#endif

#include "config.h"
#include "select.h"
#include "httpd.h"
#include "httpd_priv.h"

int _httpd_net_read(
	int	sock,
	char	*buf,
	int	len)
{
#if defined(_WIN32) 
	return( recv(sock, buf, len, 0));
#else
	return( read(sock, buf, len));
#endif
}

int _httpd_net_write(
	int	sock,
	char	*buf,
	int	len)
{
#if defined(_WIN32) 
	return( send(sock, buf, len, 0));
#else
	return( write(sock, buf, len));
#endif
}

int _httpd_setnonblocking(
	const int desc)
{
	int opts = fcntl(desc, F_GETFL);
	if (opts < 0) {
		perror( "(fcntl(F_GETFL))" );
		return -1;
	}

	opts = (opts | O_NONBLOCK);
	if (fcntl(desc, F_SETFL,opts) < 0) {
		perror( "(fcntl(F_SETFL))" );
		return -1;
	}
	return 0;
}

int _httpd_readChar(
	httpd	*server,
	char	*cp)
{
	if (server->readBufRemain == 0)
	{
		bzero(server->readBuf, HTTP_READ_BUF_LEN + 1);
		server->readBufRemain = _httpd_net_read(server->clientSock, 
			server->readBuf, HTTP_READ_BUF_LEN);
		if (server->readBufRemain < 1)
			return(0);
		server->readBuf[server->readBufRemain] = 0;
		server->readBufPtr = server->readBuf;
	}
	*cp = *server->readBufPtr++;
	server->readBufRemain--;
	return(1);
}


int _httpd_readLine(
	httpd	*server,
	char	*destBuf,
	int	len)
{
	char	curChar,
		*dst;
	int	count;
	

	count = 0;
	dst = destBuf;
	while(count < len)
	{
		if (_httpd_readChar(server, &curChar) < 1)
			return(0);
		if (curChar == '\n')
		{
			*dst = 0;
			return(1);
		}
		if (curChar == '\r')
		{
			continue;
		}
		else
		{
			*dst++ = curChar;
			count++;
		}
	}
	*dst = 0;
	return(1);
}


int _httpd_readBuf(
	httpd	*server,
	char	*destBuf,
	int	len)
{
	char	curChar,
		*dst;
	int	count;
	

	count = 0;
	dst = destBuf;
	while(count < len)
	{
		if (_httpd_readChar(server, &curChar) < 1)
			return(0);
		*dst++ = curChar;
		count++;
	}
	return(1);
}

void _httpd_writeAccessLog(
	httpd	*server)
{
	char	dateBuf[30];
	struct 	tm *timePtr;
	time_t	clock;
	int	responseCode;


	if (server->accessLog == NULL)
		return;
	clock = time(NULL);
	timePtr = localtime(&clock);
	strftime(dateBuf, 30, "%d/%b/%Y:%T %Z",  timePtr);
	responseCode = atoi(server->response.response);
	fprintf(server->accessLog, "%s - - [%s] %s \"%s\" HTTP/%1.1f %d %d\n", 
		server->clientAddr, dateBuf, httpdRequestMethodName(server), 
		httpdRequestPath(server), httpdRequestVersion(server), responseCode, 
		server->response.responseLength);
}

void _httpd_writeErrorLog(
	httpd	*server,
	char	*level,
	char	*message)
{
	char	dateBuf[30];
	struct 	tm *timePtr;
	time_t	clock;


	if (server->errorLog == NULL)
		return;
	clock = time(NULL);
	timePtr = localtime(&clock);
	strftime(dateBuf, 30, "%a %b %d %T %Y",  timePtr);
	if (*server->clientAddr != 0)
	{
		fprintf(server->errorLog, "[%s] [%s] [client %s] %s\n",
			dateBuf, level, server->clientAddr, message);
	}
	else
	{
		fprintf(server->errorLog, "[%s] [%s] %s\n",
			dateBuf, level, message);
	}
}



int _httpd_decode (
	char *		bufcoded,
	char *		bufplain,
	int		outbufsize)
{
	static char six2pr[64] = {
    		'A','B','C','D','E','F','G','H','I','J','K','L','M',
    		'N','O','P','Q','R','S','T','U','V','W','X','Y','Z',
    		'a','b','c','d','e','f','g','h','i','j','k','l','m',
    		'n','o','p','q','r','s','t','u','v','w','x','y','z',
    		'0','1','2','3','4','5','6','7','8','9','+','/'   
	};
  
	static unsigned char pr2six[256];

	/* single character decode */
#	define DEC(c) pr2six[(int)c]
#	define _DECODE_MAXVAL 63

	static int first = 1;

	int nbytesdecoded, j;
	register char *bufin = bufcoded;
	register unsigned char *bufout = (unsigned char*) bufplain;
	register int nprbytes;

	/*
	** If this is the first call, initialize the mapping table.
	** This code should work even on non-ASCII machines.
	*/
	if(first) 
	{
		first = 0;
		for(j=0; j<256; j++) pr2six[j] = _DECODE_MAXVAL+1;
		for(j=0; j<64; j++) pr2six[(int)six2pr[j]] = (unsigned char)j;
	}

   	/* Strip leading whitespace. */

   	while(*bufcoded==' ' || *bufcoded == '\t') bufcoded++;

	/*
	** Figure out how many characters are in the input buffer.
	** If this would decode into more bytes than would fit into
	** the output buffer, adjust the number of input bytes downwards.
	*/
	bufin = bufcoded;
	while(pr2six[(int)*(bufin++)] <= _DECODE_MAXVAL);
	nprbytes = bufin - bufcoded - 1;
	nbytesdecoded = ((nprbytes+3)/4) * 3;
	if(nbytesdecoded > outbufsize) 
	{
		nprbytes = (outbufsize*4)/3;
	}
	bufin = bufcoded;
   
	while (nprbytes > 0) 
	{
		*(bufout++)=(unsigned char)(DEC(*bufin)<<2|DEC(bufin[1])>>4);
		*(bufout++)=(unsigned char)(DEC(bufin[1])<<4|DEC(bufin[2])>>2);
		*(bufout++)=(unsigned char)(DEC(bufin[2])<<6|DEC(bufin[3]));
		bufin += 4;
		nprbytes -= 4;
	}
	if(nprbytes & 03) 
	{
		if(pr2six[(int)bufin[-2]] > _DECODE_MAXVAL) 
		{
			nbytesdecoded -= 2;
		}
		else 
		{
			nbytesdecoded -= 1;
		}
	}
	bufplain[nbytesdecoded] = 0;
	return(nbytesdecoded);
}



char _httpd_from_hex (
        char    c)
{
    return  c >= '0' && c <= '9' ?  c - '0'
            : c >= 'A' && c <= 'F'? c - 'A' + 10
            : c - 'a' + 10;     /* accept small letters just in case */
}

char * _httpd_unescape(
        char    *str)
{
    char * p = str;
    char * q = str;
    static char blank[] = "";

    if (!str)
        return(blank);
    while(*p) {
        if (*p == '%') {
            p++;
            if (*p) *q = _httpd_from_hex(*p++) * 16;
            if (*p) *q = (*q + _httpd_from_hex(*p++));
            q++;
        } else {
            if (*p == '+') {
                *q++ = ' ';
                p++;
            } else {
                *q++ = *p++;
              }
        }
    }

    *q++ = 0;
    return str;
} 


void _httpd_freeVariables(
	httpVar	*var)
{
	httpVar	*curVar, *lastVar;

	if (var == NULL)
		return;
	_httpd_freeVariables(var->nextVariable);
	var->nextVariable = NULL;
	curVar = var;
	while(curVar)
	{
		lastVar = curVar;
		curVar = curVar->nextValue;
		free(lastVar->name);
		free(lastVar->value);
		free(lastVar);
	}
	return;
}

void _httpd_storeData(
	httpd	*server,
        char    *query)
{
        char    *cp,
		*cp2,
                var[50],
                *val,
		*tmpVal;

        if (!query)
                return;

	cp = query;
	cp2 = var;
        bzero(var,sizeof(var));
	val = NULL;
        while(*cp)
        {
                if (*cp == '=')
                {
                        cp++;
			*cp2 = 0;
                        val = cp;
                        continue;
                }
                if (*cp == '&')
                {
			*cp = 0;
			tmpVal = _httpd_unescape(val);
			httpdAddVariable(server, var, tmpVal);
                        cp++;
                        cp2 = var;
			val = NULL;
                        continue;
                }
		if (val)
		{
			cp++;
		}
		else
		{
                	*cp2 = *cp++;
			if (*cp2 == '.')
			{
				strcpy(cp2,"_dot_");
				cp2 += 5;
			}
			else
			{
				cp2++;
			}
		}
        }
	*cp = 0;
	tmpVal = _httpd_unescape(val);
	httpdAddVariable(server, var, tmpVal);
}


void _httpd_formatTimeString(
	httpd	*server,
	char	*ptr,
	int	clock)
{
    if(server)
    {
        //  -Wall -W
    }
	struct 	tm *timePtr;

	if (clock == 0)
		clock = time(NULL);
	timePtr = gmtime((time_t*)&clock);
	strftime(ptr, HTTP_TIME_STRING_LEN,"%a, %d %b %Y %T GMT",timePtr);
}


void _httpd_sendHeaders(
	httpd	*server,
	int	contentLength = 0,
	int	modTime = 0,
	int	skip = 0)
{
	char	tmpBuf[255],
		timeBuf[HTTP_TIME_STRING_LEN];

	if(server->response.headersSent)
		return;

	server->response.headersSent = 1;
	
	if( server->request.version <= 1.0 ) {
		server->clients->doWrite(server->clientSock, "HTTP/1.0 ", 9);
	} else {
		server->clients->doWrite(server->clientSock, "HTTP/1.1 ", 9);
	}
	server->clients->doWrite(server->clientSock, server->response.response, 
		strlen(server->response.response));
	server->clients->doWrite(server->clientSock, server->response.headers, 
		strlen(server->response.headers));

	_httpd_formatTimeString(server, timeBuf, 0);
	server->clients->doWrite(server->clientSock,"Date: ", 6);
	server->clients->doWrite(server->clientSock, timeBuf, strlen(timeBuf));
	server->clients->doWrite(server->clientSock, "\r\n", 2);

	server->clients->doWrite(server->clientSock, "Content-Type: ", 14);
	server->clients->doWrite(server->clientSock, server->response.contentType, 
		strlen(server->response.contentType));
	server->clients->doWrite(server->clientSock, "\r\n", 2);

	server->clients->doWrite(server->clientSock, "Content-Length: ", 16);
	snprintf(tmpBuf, sizeof(tmpBuf), "%d", contentLength);
	server->clients->doWrite(server->clientSock, tmpBuf, strlen(tmpBuf));
	server->clients->doWrite(server->clientSock, "\r\n", 2);

	if (server->request.version < 1.1 || server->request.close) 
	{
		server->clients->doWrite(server->clientSock, "Connection: Close\r\n", 19);
	}
	if (modTime > 0)
	{
		_httpd_formatTimeString(server, timeBuf, modTime);
		server->clients->doWrite(server->clientSock, "Last-Modified: ", 15);
		server->clients->doWrite(server->clientSock, timeBuf, strlen(timeBuf));
		server->clients->doWrite(server->clientSock, "\r\n", 2);
	}
	server->clients->doWrite(server->clientSock, "Accept-Ranges: bytes\r\n", 22);

	if( skip != 0 ) {
		snprintf(tmpBuf, sizeof(tmpBuf), "Content-Range: bytes %d-%d/%d\r\n", 
			server->request.contentRange, contentLength, contentLength);
		server->clients->doWrite(server->clientSock, tmpBuf, strlen(tmpBuf));
	}

	server->clients->doWrite(server->clientSock, "\r\n", 2);
}

httpDir *_httpd_findContentDir(
	httpd	*server,
	char	*dir,
	int	createFlag)
{
	char	buffer[HTTP_MAX_URL],
		*curDir;
	httpDir	*curItem,
		*curChild;

	strncpy(buffer, dir, HTTP_MAX_URL);
	curItem = server->content;
	curDir = strtok(buffer,"/");
	while(curDir)
	{
		curChild = curItem->children;
		while(curChild)
		{
			if (strcmp(curChild->name, curDir) == 0)
				break;
			curChild = curChild->next;
		}
		if (curChild == NULL)
		{
			if (createFlag == HTTP_TRUE)
			{
				curChild = (httpDir*) malloc(sizeof(httpDir));
				bzero(curChild, sizeof(httpDir));
				curChild->name = strdup(curDir);
				curChild->next = curItem->children;
				curItem->children = curChild;
			}
			else
			{
				return(NULL);
			}
		}
		curItem = curChild;
		curDir = strtok(NULL,"/");
	}
	return(curItem);
}


httpContent *_httpd_findContentEntry(
	httpd	*server,
	httpDir	*dir,
	char	*entryName)
{
	httpContent *curEntry;

	curEntry = dir->entries;
	while(curEntry)
	{
		if (curEntry->type == HTTP_WILDCARD || 
		    curEntry->type ==HTTP_C_WILDCARD)
			break;
		if (*entryName == 0 && curEntry->indexFlag)
			break;
		if (strcmp(curEntry->name, entryName) == 0)
			break;
		curEntry = curEntry->next;
	}
	if (curEntry)
		server->response.content = curEntry;
	return(curEntry);
}


void _httpd_send204(
	httpd	*server)
{
	httpdSetResponse(server, "204 No Content\n");
	_httpd_sendHeaders(server,0,0);
}


void _httpd_send304(
	httpd	*server)
{
	httpdSetResponse(server, "304 Not Modified\n");
	_httpd_sendHeaders(server,0,0);
}


void _httpd_send400(
	httpd	*server)
{
	httpdSetResponse(server, "400 Bad Request\n");
	_httpd_sendHeaders(server,0,0);
}


void _httpd_send403(
	httpd	*server)
{
	httpdSetResponse(server, "403 Permission Denied\n");
	_httpd_sendHeaders(server,strlen(HTTP_403_RESPONSE),0);
	_httpd_sendText(server,HTTP_403_RESPONSE);
}


void _httpd_send404(
	httpd	*server)
{
	char	msg[HTTP_MAX_URL];

	snprintf(msg, HTTP_MAX_URL,
		"File does not exist: %s", server->request.path);
	_httpd_writeErrorLog(server,LEVEL_ERROR, msg);
	httpdSetResponse(server, "404 Not Found\n");
	_httpd_sendHeaders(server,strlen(HTTP_404_RESPONSE),0);
	_httpd_sendText(server,HTTP_404_RESPONSE);
}


void _httpd_send501(
	httpd	*server)
{
	httpdSetResponse(server, "501 Not Implemented\n");
	_httpd_sendHeaders(server,strlen(HTTP_501_RESPONSE),0);
	_httpd_sendText(server,HTTP_501_RESPONSE);
}


void _httpd_send505(
	httpd	*server)
{
	httpdSetResponse(server, "505 HTTP Version Not Supported\n");
	_httpd_sendHeaders(server,strlen(HTTP_505_RESPONSE),0);
	_httpd_sendText(server,HTTP_505_RESPONSE);
}



void _httpd_catFile(
	httpd	*server,
	char	*path,
	int	skip)
{
	int	fd,
		len;
	char	buf[HTTP_MAX_LEN];

	fd = open(path,O_RDONLY);
	if (fd < 0)
		return;
	lseek(fd, skip, SEEK_SET);
	len = read(fd, buf, HTTP_MAX_LEN);
	while(len > 0)
	{
		server->response.responseLength += len;
		server->clients->doWrite(server->clientSock, buf, len);
		len = read(fd, buf, HTTP_MAX_LEN);
	}
	close(fd);
}


void _httpd_sendStatic(
	httpd	*server,
	char	*data)
{
	if (_httpd_checkLastModified(server,server->startTime) == 0)
	{
		_httpd_send304(server);
	}
	_httpd_sendHeaders(server, strlen(data), server->startTime);
	httpdOutput(server, data);
}



void _httpd_sendFile(
	httpd	*server,
	char	*path,
	int 	skip)
{
	char	*suffix;
	struct 	stat sbuf;

	suffix = rindex(path, '.');
	if (suffix != NULL)
	{
		if (strcasecmp(suffix,".gif") == 0) 
			strcpy(server->response.contentType,"image/gif");
		if (strcasecmp(suffix,".jpg") == 0) 
			strcpy(server->response.contentType,"image/jpeg");
		if (strcasecmp(suffix,".xbm") == 0) 
			strcpy(server->response.contentType,"image/xbm");
		if (strcasecmp(suffix,".png") == 0) 
			strcpy(server->response.contentType,"image/png");
	}
	if (stat(path, &sbuf) < 0)
	{
		_httpd_send404(server);
		return;
	}
	if (_httpd_checkLastModified(server,sbuf.st_mtime) == 0)
	{
		_httpd_send304(server);
	}
	else
	{
		_httpd_sendHeaders(server, sbuf.st_size, sbuf.st_mtime, skip);
		_httpd_catFile(server, path, skip);
	}
}


int _httpd_sendDirectoryEntry(
	httpd		*server,
	httpContent	*entry,
	char		*entryName)
{
	char		path[HTTP_MAX_URL];

	snprintf(path, HTTP_MAX_URL, "%s/%s", entry->path, entryName);
	_httpd_sendFile(server,path,0);
	return(0);
}


void _httpd_sendText(
	httpd	*server,
	char	*msg)
{
	server->response.responseLength += strlen(msg);
	server->clients->doWrite(server->clientSock,msg,strlen(msg));
}


int _httpd_checkLastModified(
	httpd	*server,
	int	modTime)
{
	char 	timeBuf[HTTP_TIME_STRING_LEN];

	_httpd_formatTimeString(server, timeBuf, modTime);
	if (strcmp(timeBuf, server->request.ifModified) == 0)
		return(0);
	return(1);
}


static unsigned char isAcceptable[96] =

/* Overencodes */
#define URL_XALPHAS     (unsigned char) 1
#define URL_XPALPHAS    (unsigned char) 2

/*      Bit 0           xalpha          -- see HTFile.h
**      Bit 1           xpalpha         -- as xalpha but with plus.
**      Bit 2 ...       path            -- as xpalpha but with /
*/
    /*   0 1 2 3 4 5 6 7 8 9 A B C D E F */
    {    7,0,0,0,0,0,0,0,0,0,7,0,0,7,7,7,       /* 2x   !"#$%&'()*+,-./ */
         7,7,7,7,7,7,7,7,7,7,0,0,0,0,0,0,       /* 3x  0123456789:;<=>?  */
         7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,       /* 4x  @ABCDEFGHIJKLMNO */
         7,7,7,7,7,7,7,7,7,7,7,0,0,0,0,7,       /* 5X  PQRSTUVWXYZ[\]^_ */
         0,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,       /* 6x  `abcdefghijklmno */
         7,7,7,7,7,7,7,7,7,7,7,0,0,0,0,0 };     /* 7X  pqrstuvwxyz{\}~ DEL */
 
#define ACCEPTABLE(a)   ( a>=32 && a<128 && ((isAcceptable[a-32]) & mask))

static char *hex_ = "0123456789ABCDEF";


char *_httpd_escape(
        char *str)
{
    unsigned char mask = URL_XPALPHAS;
    char * p;
    char * q;
    char * result;
    int unacceptable = 0;
    for(p=str; *p; p++)
        if (!ACCEPTABLE((unsigned char)*p))
                unacceptable +=2;
    result = (char *) malloc(p-str + unacceptable + 1);
    bzero(result,(p-str + unacceptable + 1));

    if (result == NULL)
    {
	return(NULL);
    }
    for(q=result, p=str; *p; p++) {
        unsigned char a = *p;
        if (!ACCEPTABLE(a)) {
            *q++ = '%';  /* Means hex commming */
            *q++ = hex_[a >> 4];
            *q++ = hex_[a & 15];
        }
        else *q++ = *p;
    }
    *q++ = 0;                   /* Terminate */
    return result;
}



void _httpd_sanitiseUrl(
	char	*url)
{
	char	*from,
		*to,
		*last;

	/*
	** Remove multiple slashes
	*/
	from = to = url;
	while(*from)
	{
		if (*from == '/' && *(from+1) == '/')
		{
			from++;
			continue;
		}
		*to = *from;
		to++;
		from++;
	}
	*to = 0;


	/*
	** Get rid of ./ sequences
	*/
	from = to = url;
	while(*from)
	{
		if (*from == '/' && *(from+1) == '.' && *(from+2)=='/')
		{
			from += 2;
			continue;
		}
		*to = *from;
		to++;
		from++;
	}
	*to = 0;


	/*
	** Catch use of /../ sequences and remove them.  Must track the
	** path structure and remove the previous path element.
	*/
	from = to = last = url;
	while(*from)
	{
		if (*from == '/' && *(from+1) == '.' && 
			*(from+2)=='.' && *(from+3)=='/')
		{
			to = last;
			from += 3;
			continue;
		}
		if (*from == '/')
		{
			last = to;
		}
		*to = *from;
		to++;
		from++;
	}
	*to = 0;
}

