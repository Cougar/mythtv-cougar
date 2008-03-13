//////////////////////////////////////////////////////////////////////////////
// Program Name: mythxml.h
//                                                                            
// Purpose - Myth Frontend XML protocol HttpServerExtension 
//                                                                            
//////////////////////////////////////////////////////////////////////////////

#ifndef MYTHFEXML_H_
#define MYTHFEXML_H_

#include <qdom.h>
#include <qdatetime.h> 

#include "upnp.h"
#include "eventing.h"
#include "mythcontext.h"

typedef enum 
{
    MFEXML_Unknown                =  0,
    MFEXML_GetScreenShot          =  1

} MythFEXMLMethod;

class MythFEXML : public Eventing
{
    private:

        QString                      m_sControlUrl;
        QString                      m_sServiceDescFileName;

    protected:

        // Implement UPnpServiceImpl methods that we can

        virtual QString GetServiceType      () { return "urn:schemas-mythtv-org:service:MythTv:1"; }
        virtual QString GetServiceId        () { return "urn:mythtv-org:serviceId:MYTHTV_1-0"; }
        virtual QString GetServiceControlURL() { return m_sControlUrl.mid( 1 ); }
        virtual QString GetServiceDescURL   () { return m_sControlUrl.mid( 1 ) + "/GetServDesc"; }

    private:

        MythFEXMLMethod GetMethod( const QString &sURI );

	void    GetScreenShot    ( HTTPRequest *pRequest );

    public:
                 MythFEXML( UPnpDevice *pDevice );
        virtual ~MythFEXML();

        bool     ProcessRequest( HttpWorkerThread *pThread, HTTPRequest *pRequest );

        // Static methods shared with HttpStatus

};

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//
// 
//
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
#endif


