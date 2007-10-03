/////////////////////////////////////////////////////////////////////////////
// Program Name: mediarenderer.cpp
//                                                                           
// Purpose - uPnp Media Renderer main Class
//                                                                           
// Created By  : David Blain                    Created On : Jan. 15, 2007
// Modified By :                                Modified On:                 
//                                                                           
/////////////////////////////////////////////////////////////////////////////

#include "mediarenderer.h"

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//
// UPnp MediaRenderer Class implementaion
//
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

MediaRenderer::MediaRenderer() 
{
    VERBOSE(VB_UPNP, "MediaRenderer::Begin" );

    // ----------------------------------------------------------------------
    // Initialize Configuration class (XML file for frontend)
    // ----------------------------------------------------------------------

    SetConfiguration( new XmlConfiguration( "config.xml" ));

    // ----------------------------------------------------------------------
    // Create mini HTTP Server
    // ----------------------------------------------------------------------

    int nPort = g_pConfig->GetValue( "UPnP/MythFrontend/ServicePort", 6547 );

    m_pHttpServer = new HttpServer( nPort ); 

    if (!m_pHttpServer->ok())
    { 
        VERBOSE(VB_IMPORTANT, "MediaRenderer::HttpServer Create Error");
        // exit(BACKEND_BUGGY_EXIT_NO_BIND_STATUS);
        return;
    }

    // ----------------------------------------------------------------------
    // Initialize UPnp Stack
    // ----------------------------------------------------------------------

    if (Initialize( nPort, m_pHttpServer ))
    {
        // ------------------------------------------------------------------
        // Create device Description
        // ------------------------------------------------------------------

        VERBOSE(VB_UPNP, QString( "MediaRenderer::Creating UPnp Description" ));

        UPnpDevice &device = g_UPnpDeviceDesc.m_rootDevice;

        device.m_sDeviceType        = "urn:schemas-upnp-org:device:MediaRenderer:1";
        device.m_sFriendlyName      = "MythTv AV Renderer";
        device.m_sManufacturer      = "MythTV";
        device.m_sManufacturerURL   = "http://www.mythtv.org";
        device.m_sModelDescription  = "MythTV AV Media Renderer";
        device.m_sModelName         = "MythTV AV Media Renderer";
        device.m_sModelURL          = "http://www.mythtv.org";
        device.m_sUPC               = "";
        device.m_sPresentationURL   = "";

        // ------------------------------------------------------------------
        // Register any HttpServerExtensions...
        // ------------------------------------------------------------------

        QString sSinkProtocols = "http-get:*:image/gif:*,"
                                 "http-get:*:image/jpeg:*,"
                                 "http-get:*:image/png:*,"
                                 "http-get:*:video/avi:*,"
                                 "http-get:*:audio/mpeg:*,"
                                 "http-get:*:audio/wav:*,"
                                 "http-get:*:video/mpeg:*,"
                                 "http-get:*:video/nupplevideo:*,"
                                 "http-get:*:video/x-ms-wmv:*";

        // VERBOSE(VB_UPNP, QString( "MediaRenderer::Registering AVTransport Service." ));
        // m_pHttpServer->RegisterExtension( m_pUPnpAVT = new UPnpAVTransport( RootDevice() ));

        VERBOSE(VB_UPNP, QString( "MediaRenderer::Registering CMGR Service." ));
        m_pHttpServer->RegisterExtension( m_pUPnpCMGR= new UPnpCMGR( RootDevice(), "", sSinkProtocols ));

        // VERBOSE(VB_UPNP, QString( "MediaRenderer::Registering RenderingControl Service." ));
        // m_pHttpServer->RegisterExtension( m_pUPnpRCTL= new UPnpRCTL( RootDevice() ));

        Start();

    }
    else
    {
        VERBOSE(VB_IMPORTANT, "MediaRenderer::Unable to Initialize UPnp Stack");
        // exit(BACKEND_BUGGY_EXIT_NO_BIND_STATUS);
    }



    VERBOSE(VB_UPNP, QString( "MediaRenderer::End" ));
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

MediaRenderer::~MediaRenderer()
{
    if (m_pHttpServer)
        delete m_pHttpServer;
}

/////////////////////////////////////////////////////////////////////////////
// Caller MUST call Release on returned pointer
/////////////////////////////////////////////////////////////////////////////

DeviceLocation *MediaRenderer::GetDefaultMaster()
{
    UPnp::PerformSearch( "urn:schemas-mythtv-org:device:MasterMediaServer:1" );

    QString sUSN = g_pConfig->GetValue( "UPnP/MythFrontend/DefaultBackend/USN"        , "" );
    QString sPin = g_pConfig->GetValue( "UPnP/MythFrontend/DefaultBackend/SecurityPin", "" );

    if (sUSN.isEmpty())
        return NULL;

    DeviceLocation *pDeviceLoc = NULL;

    // Lets wait up to 2 seconds for the backend to answer our Search request;

    QTime timer;
    timer.start();

    while (timer.elapsed() < 2000 )
    {
       pDeviceLoc = UPnp::g_SSDPCache.Find( "urn:schemas-mythtv-org:device:MasterMediaServer:1",
                                            sUSN );

        if ( pDeviceLoc != NULL)
        {
            pDeviceLoc->AddRef();

            pDeviceLoc->m_sSecurityPin = sPin;

            return pDeviceLoc;
        }

       usleep(10000);
    }

    return NULL;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void MediaRenderer::SetDefaultMaster( DeviceLocation *pDeviceLoc, const QString &sPin )
{
    if ( pDeviceLoc != NULL)
    {
        pDeviceLoc->m_sSecurityPin = sPin;

        g_pConfig->SetValue( "UPnP/MythFrontend/DefaultBackend/USN"        , pDeviceLoc->m_sUSN );
        g_pConfig->SetValue( "UPnP/MythFrontend/DefaultBackend/SecurityPin", sPin );
        g_pConfig->Save();
    }
}