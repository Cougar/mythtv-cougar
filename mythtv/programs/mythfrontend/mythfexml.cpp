//////////////////////////////////////////////////////////////////////////////
// Program Name: MythFE.cpp
//                                                                            
// Purpose - Frontend Html & XML status HttpServerExtension
//                                                                            
//////////////////////////////////////////////////////////////////////////////

#include "mythfexml.h"

#include "libmyth/mythcontext.h"
#include "libmyth/util.h"
#include "libmyth/mythdbcon.h"

#include "mythmainwindow.h"

#include <qtextstream.h>
#include <qdir.h>
#include <qfile.h>
#include <qregexp.h>
#include <qbuffer.h>
#include <math.h>

#include "../../config.h"

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

MythFEXML::MythFEXML( UPnpDevice *pDevice ) : Eventing( "MythFEXML", "MYTHTV_Event" )
{

    QString sUPnpDescPath = UPnp::g_pConfig->GetValue( "UPnP/DescXmlPath", m_sSharePath );

    m_sServiceDescFileName = sUPnpDescPath + "MFEXML_scpd.xml";
    m_sControlUrl          = "/MythFE";

    // Add our Service Definition to the device.

    RegisterService( pDevice );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

MythFEXML::~MythFEXML()
{
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

MythFEXMLMethod MythFEXML::GetMethod( const QString &sURI )
{
    if (sURI == "GetScreenShot"        ) return MFEXML_GetScreenShot;

    return( MFEXML_Unknown );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool MythFEXML::ProcessRequest( HttpWorkerThread *pThread, HTTPRequest *pRequest )
{
    try
    {
        if (pRequest)
        {
            if (pRequest->m_sBaseUrl != m_sControlUrl)
                return( false );

            VERBOSE(VB_UPNP, QString("MythFEXML::ProcessRequest: %1 : %2")
			             .arg(pRequest->m_sMethod)
				     .arg(pRequest->m_sRawRequest));

            switch( GetMethod( pRequest->m_sMethod ))
            {
		case MFEXML_GetScreenShot      : GetScreenShot    ( pRequest ); return true;


                default: 
                {
                    UPnp::FormatErrorResponse( pRequest, UPnPResult_InvalidAction );

                    return true;
                }
            }
        }
    }
    catch( ... )
    {
        VERBOSE( VB_IMPORTANT, "MythFEXML::ProcessRequest() - Unexpected Exception" );
    }

    return( false );
}           

// ==========================================================================
// Request handler Methods
// ==========================================================================

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void MythFEXML::GetScreenShot( HTTPRequest *pRequest )
{
    pRequest->m_eResponseType   = ResponseTypeFile;

    // Optional Parameters

    int     nWidth    = pRequest->m_mapParams[ "Width"     ].toInt();
    int     nHeight   = pRequest->m_mapParams[ "Height"    ].toInt();

    // Read Icon file path from database

    QString sFileName = QString("/%1/myth-screenshot-XML.jpg")
                    .arg(gContext->GetSetting("ScreenShotPath","/tmp/"));

    if (!gContext->GetMainWindow()->screenShot(sFileName,nWidth, nHeight))
    {
        VERBOSE(VB_GENERAL, "MythFEXML: Failed to take screenshot. Aborting");
	return;
    }

    pRequest->m_sFileName = sFileName;
}

