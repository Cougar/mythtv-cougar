/*
** Copyright (c) 2002  Hughes Technologies Pty Ltd.  All rights
** reserved.
**
** Copyright (c) 2003  deleet, Alexander Oberdoerster
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

/*
**  libhttpd Private Header File
*/


/***********************************************************************
** Standard header preamble.  Ensure singular inclusion, setup for
** function prototypes and c++ inclusion
*/

#ifndef LIB_HTTPD_PRIV_H

#define LIB_HTTPD_H_PRIV 1

#if !defined(__ANSI_PROTO)
#if defined(_WIN32) || defined(__STDC__) || defined(__cplusplus)
#  define __ANSI_PROTO(x)       x
#else
#  define __ANSI_PROTO(x)       ()
#endif
#endif

#ifdef __cplusplus
//extern "C" {
#endif


#define	LEVEL_NOTICE	"notice"
#define LEVEL_ERROR	"error"

char * _httpd_unescape __ANSI_PROTO((char*));
char *_httpd_escape __ANSI_PROTO((char*));
char _httpd_from_hex  __ANSI_PROTO((char));


void _httpd_catFile __ANSI_PROTO((httpd*, char*, int));
void _httpd_send204 __ANSI_PROTO((httpd*));
void _httpd_send304 __ANSI_PROTO((httpd*));
void _httpd_send400 __ANSI_PROTO((httpd*));
void _httpd_send403 __ANSI_PROTO((httpd*));
void _httpd_send404 __ANSI_PROTO((httpd*));
void _httpd_send501 __ANSI_PROTO((httpd*));
void _httpd_send505 __ANSI_PROTO((httpd*));
void _httpd_sendText __ANSI_PROTO((httpd*, char*));
void _httpd_sendFile __ANSI_PROTO((httpd*, char*, int));
void _httpd_sendStatic __ANSI_PROTO((httpd*, char*));
void _httpd_sendHeaders __ANSI_PROTO((httpd*, int, int, int);)
void _httpd_sanitiseUrl __ANSI_PROTO((char*));
void _httpd_freeVariables __ANSI_PROTO((httpVar*));
void _httpd_formatTimeString __ANSI_PROTO((httpd*, char*, int));
void _httpd_storeData __ANSI_PROTO((httpd*, char*));
void _httpd_writeAccessLog __ANSI_PROTO((httpd*));
void _httpd_writeErrorLog __ANSI_PROTO((httpd*, char*, char*));
int _httpd_decode  __ANSI_PROTO((char *bufcoded,char *bufplain,int outbufsize));


int _httpd_setnonblocking __ANSI_PROTO((int));
int _httpd_net_read __ANSI_PROTO((int, char*, int));
int _httpd_net_write __ANSI_PROTO((int, char*, int));
int _httpd_readBuf __ANSI_PROTO((httpd*, char*, int));
int _httpd_readChar __ANSI_PROTO((httpd*, char*));
int _httpd_readLine __ANSI_PROTO((httpd*, char*, int));
int _httpd_checkLastModified __ANSI_PROTO((httpd*, int));
int _httpd_sendDirectoryEntry __ANSI_PROTO((httpd*, httpContent*, char*));

httpContent *_httpd_findContentEntry __ANSI_PROTO((httpd*, httpDir*, char*));
httpDir *_httpd_findContentDir __ANSI_PROTO((httpd*, char*, int));

#ifdef __cplusplus
//}
#endif

#endif  /* LIB_HTTPD_PRIV_H */
