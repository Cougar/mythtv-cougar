//////////////////////////////////////////////////////////////////////////////
// Program Name: httprequest.h
// Created     : Oct. 21, 2005
//
// Purpose     : Http Request/Response
//                                                                            
// Copyright (c) 2005 David Blain <mythtv@theblains.net>
//                                          
// This library is free software; you can redistribute it and/or 
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 2.1 of the License, or at your option any later version of the LGPL.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public
// License along with this library.  If not, see <http://www.gnu.org/licenses/>.
//
//////////////////////////////////////////////////////////////////////////////

#ifndef HTTPREQUEST_H_
#define HTTPREQUEST_H_

#include <QRegExp>
#include <QTextStream>

using namespace std;

#include "upnputil.h"
#include "bufferedsocketdevice.h"

#define SOAP_ENVELOPE_BEGIN  "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" " \
                             "s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">"     \
                             "<s:Body>"
#define SOAP_ENVELOPE_END    "</s:Body>\r\n</s:Envelope>";


/////////////////////////////////////////////////////////////////////////////
// Typedefs / Defines
/////////////////////////////////////////////////////////////////////////////

typedef enum 
{
    RequestTypeUnknown      = 0x0000,
    RequestTypeGet          = 0x0001,
    RequestTypeHead         = 0x0002,
    RequestTypePost         = 0x0004,
    RequestTypeMSearch      = 0x0008,
    RequestTypeSubscribe    = 0x0010,
    RequestTypeUnsubscribe  = 0x0020,
    RequestTypeNotify       = 0x0040,
    RequestTypeResponse     = 0x0080

} RequestType;                

typedef enum 
{
    ContentType_Unknown    = 0,
    ContentType_Urlencoded = 1,
    ContentType_XML        = 2

} ContentType;

typedef enum 
{
    ResponseTypeNone     = -1,
    ResponseTypeUnknown  =  0,
    ResponseTypeXML      =  1,
    ResponseTypeHTML     =  2,
    ResponseTypeFile     =  3,
    ResponseTypeOther    =  4

} ResponseType;


typedef struct
{
    const char *pszExtension;
    const char *pszType;

} MIMETypes;

/////////////////////////////////////////////////////////////////////////////

class IPostProcess
{
    public:
        virtual void ExecutePostProcess( ) = 0;
        virtual ~IPostProcess() {};
};

/////////////////////////////////////////////////////////////////////////////
// 
/////////////////////////////////////////////////////////////////////////////

class HTTPRequest
{
    protected:

        static const char  *m_szServerHeaders;

        QRegExp             m_procReqLineExp;
        QRegExp             m_parseRangeExp;
        QByteArray          m_aBuffer;


    public:    
        
        RequestType         m_eType;
        ContentType         m_eContentType;

        QString             m_sRawRequest;

        QString             m_sBaseUrl;
        QString             m_sMethod;

        QStringMap          m_mapParams;
        QStringMap          m_mapHeaders;

        QString             m_sPayload;

        QString             m_sProtocol;
        int                 m_nMajor;
        int                 m_nMinor;


        bool                m_bSOAPRequest;
        QString             m_sNameSpace;

        // Response

        ResponseType        m_eResponseType;
        QString             m_sResponseTypeText;    // used for ResponseTypeOther

        long                m_nResponseStatus;
        QStringMap          m_mapRespHeaders;

        QString             m_sFileName;

        QTextStream         m_response;

        IPostProcess       *m_pPostProcess;

    protected:

        RequestType     SetRequestType      ( const QString &sType  );
        void            SetRequestProtocol  ( const QString &sLine  );
        ContentType     SetContentType      ( const QString &sType  );

        void            SetServerHeaders    ( void );

        void            ProcessRequestLine  ( const QString &sLine  );
        bool            ProcessSOAPPayload  ( const QString &sSOAPAction );
        void            ExtractMethodFromURL( );

        QString         GetResponseStatus   ( void );
        QString         GetResponseType     ( void );
        QString         GetAdditionalHeaders( void );

        bool            ParseRange          ( QString sRange, 
                                              long long   llSize, 
                                              long long *pllStart, 
                                              long long *pllEnd   );

        QString         BuildHeader         ( long long nSize );

    public:
        
                        HTTPRequest     ();
        virtual        ~HTTPRequest     () {};

        void            Reset           ();

        bool            ParseRequest    ();

        void            FormatErrorResponse ( bool  bServerError, 
                                              const QString &sFaultString, 
                                              const QString &sDetails );

        void            FormatActionResponse( const NameValues &pArgs );
        void            FormatFileResponse ( const QString &sFileName );

        long            SendResponse    ( void );
        long            SendResponseFile( QString sFileName );

        QString         GetHeaderValue  ( const QString &sKey, QString sDefault );

        bool            GetKeepAlive    ();

        static QString  GetMimeType     ( const QString &sFileExtension );
        static QString  TestMimeType    ( const QString &sFileName );
        static long     GetParameters   ( QString  sParams, QStringMap &mapParams );
        static QString  Encode          ( const QString &sIn );

        // ------------------------------------------------------------------

        virtual qlonglong  BytesAvailable  () = 0;
        virtual qulonglong WaitForMore     ( int msecs, bool *timeout = NULL ) = 0;
        virtual bool    CanReadLine     () = 0;
        virtual QString ReadLine        ( int msecs = 0 ) = 0;
        virtual qlonglong  ReadBlock       ( char *pData, qulonglong nMaxLen, int msecs = 0 ) = 0;
        virtual qlonglong  WriteBlock      ( const char *pData,
                                          qulonglong nLen    ) = 0;
        virtual qlonglong  WriteBlockDirect( const char *pData,
                                          qulonglong nLen    ) = 0;
        virtual QString GetHostAddress  () = 0;
        virtual QString GetPeerAddress  () = 0;
        virtual void    Flush           () = 0;
        virtual bool    IsValid         () = 0;
        virtual int     getSocketHandle () = 0;
        virtual void    SetBlocking     ( bool bBlock ) = 0;
        virtual bool    IsBlocking      () = 0;
};

/////////////////////////////////////////////////////////////////////////////
// 
/////////////////////////////////////////////////////////////////////////////

class BufferedSocketDeviceRequest : public HTTPRequest
{
    public:    

        BufferedSocketDevice    *m_pSocket;

    public:
        
                 BufferedSocketDeviceRequest( BufferedSocketDevice *pSocket );
        virtual ~BufferedSocketDeviceRequest() {};

        virtual qlonglong  BytesAvailable  ();
        virtual qulonglong WaitForMore     ( int msecs, bool *timeout = NULL );
        virtual bool    CanReadLine     ();
        virtual QString ReadLine        ( int msecs = 0 );
        virtual qlonglong  ReadBlock       ( char *pData, qulonglong nMaxLen, int msecs = 0  );
        virtual qlonglong  WriteBlock      ( const char *pData, qulonglong nLen    );
        virtual qlonglong  WriteBlockDirect( const char *pData, qulonglong nLen    );
        virtual QString GetHostAddress  ();
        virtual QString GetPeerAddress  ();
        virtual void    Flush           () { m_pSocket->Flush(); }
        virtual bool    IsValid         () {return( m_pSocket->IsValid() ); }
        virtual int     getSocketHandle () {return( m_pSocket->socket() ); }
        virtual void    SetBlocking     ( bool bBlock );
        virtual bool    IsBlocking      ();

};

#endif
