#ifndef RTSPSERVER_H_
#define RTSPSERVER_H_
/*
	rtspserver.h

	(c) 2005 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	An rtsp server (as an mfd plugin).

*/

#include "mfd_plugin.h"

class MFDRtspPlugin : public MFDServicePlugin
{

  public:
  
    MFDRtspPlugin(
                    MFD *owner, 
                    int identifier, 
                    uint port, 
                    const QString &a_name = "unkown",
                    int l_minimum_thread_pool_size = 0
                 );
    ~MFDRtspPlugin();


    virtual void    processRequest(MFDServiceClientSocket *a_client);

/*
    virtual void    handleIncoming(HttpInRequest *request, int client_id);
    virtual void    sendResponse(int client_id, HttpOutResponse *http_response);
*/
};

#endif

