/*
    daapinstance.cpp

    (c) 2003 Thor Sigvaldason and Isaac Richards
    Part of the mythTV project
    
    An object that knows how to talk to daap servers

*/

#include <vector>
#include <string>
using namespace std;

#include "daapclient.h"
#include "daaprequest.h"
#include "daapresponse.h"


DaapInstance::DaapInstance(
                            DaapClient *owner, 
                            const QString &l_server_address, 
                            uint l_server_port,
                            const QString &l_service_name
                           )
{
    //
    //  Store the basic info about where to find the daap server this
    //  instance is supposed to talk to
    //
    
    parent = owner;
    server_address = l_server_address;
    server_port = l_server_port;
    service_name = l_service_name;
    keep_going = true;
    client_socket_to_daap_server = NULL;
    if(pipe(u_shaped_pipe) < 0)
    {
        warning("daap instance could not create a u shaped pipe");
    }
    daap_server_type = DAAP_SERVER_UNKNOWN;

    //
    //  Server supplied information
    //


    //
    //  Data that comes from /server-info
    //

    have_server_info = false;
    supplied_service_name = "";
    server_requires_login = false;
    server_timeout_interval = -1;
    server_supports_autologout = false;
    server_authentication_method = -1;
    server_supports_update = false;
    server_supports_persistentid = false;
    server_supports_extensions = false;
    server_supports_browse = false;
    server_supports_query = false;
    server_supports_index = false;
    server_supports_resolve = false;
    server_numb_databases = -1;


    //
    //  Data that comes from /login
    //

    logged_in = false;    
    session_id = -1;
    
    //
    //  Data that comes from /update
    //
    
    metadata_generation = 1;
}                           

void DaapInstance::run()
{
    //
    //  So, first thing we need to do is try and open a client socket to the
    //  daap server
    //
    
    QHostAddress daap_server_address;
    if(!daap_server_address.setAddress(server_address))
    {
        warning(QString("daap instance could not "
                        "set daap server's address "
                        "of %1")
                        .arg(server_address));
        return;
    }

    client_socket_to_daap_server = new QSocketDevice(QSocketDevice::Stream);
    client_socket_to_daap_server->setBlocking(false);

    int connect_tries = 0;
    while(!(client_socket_to_daap_server->connect(daap_server_address, 
                                                  server_port)))
    {
        //
        //  Try a few times (it's a non-blocking socket)
        //
        ++connect_tries;
        if(connect_tries > 10)
        {
            warning(QString("daap instance could not "
                            "connect to server %1:%2")
                            .arg(server_address)
                            .arg(server_port));
            return;
        }
        usleep(100);
    }

    log(QString("daap instance made basic "
                "connection to \"%1\" (%2:%3)")
                .arg(service_name)
                .arg(server_address)
                .arg(server_port), 4);
    

    //
    //  OK, we have a connection, let's send our first request to "prime the
    //  pump"
    //
    
    DaapRequest first_request(this, "/server-info", server_address);
    first_request.send(client_socket_to_daap_server);
    
    while(keep_going)
    {
        //
        //  Just keep checking the server socket to look for data
        //
        
        int nfds = 0;
        fd_set readfds;
        struct timeval timeout;
        
        //
        //  Listen to the server if it's there
        //

        FD_ZERO(&readfds);
        if(client_socket_to_daap_server)
        {
            FD_SET(client_socket_to_daap_server->socket(), &readfds);
            if(nfds <= client_socket_to_daap_server->socket())
            {
                nfds = client_socket_to_daap_server->socket() + 1;
            }
        }
        
        //
        //  Listen on our u-shaped pipe
        //

        FD_SET(u_shaped_pipe[0], &readfds);
        if(nfds <= u_shaped_pipe[0])
        {
            nfds = u_shaped_pipe[0] + 1;
        }
    

        
        timeout.tv_sec = 200;
        timeout.tv_usec = 0;
        int result = select(nfds, &readfds, NULL, NULL, &timeout);
        if(result < 0)
        {
            warning("daap instance got an error from select() "
                    "... not sure what to do");
        }
        else
        {
            if(client_socket_to_daap_server)
            {
                if(FD_ISSET(client_socket_to_daap_server->socket(), &readfds))
                {
                    //
                    //  Excellent ... the daap server is sending us some data.
                    //
                
                    handleIncoming();
                }
            }
            if(FD_ISSET(u_shaped_pipe[0], &readfds))
            {
                u_shaped_pipe_mutex.lock();
                    char read_back[2049];
                    read(u_shaped_pipe[0], read_back, 2048);
                    u_shaped_pipe_mutex.unlock();
            }
        }
    }
    
    //
    //  If we're logged in, send a logoff (just being polite)
    //  But don't bother waiting for a response
    //
    
    if(logged_in)
    {
        DaapRequest logout_request(this, "/logout", server_address);
        logout_request.addGetVariable("session-id", session_id);
        logout_request.send(client_socket_to_daap_server, true);
    }
    
    //
    //  Kill off the socket
    //
    
    if(client_socket_to_daap_server)
    {
        delete client_socket_to_daap_server;
        client_socket_to_daap_server = NULL;
    }
}

void DaapInstance::stop()
{
    keep_going_mutex.lock();
        keep_going = false;
    keep_going_mutex.unlock();
    wakeUp();
}

bool DaapInstance::keepGoing()
{
    bool return_value = true;
    keep_going_mutex.lock();
        return_value = keep_going;
    keep_going_mutex.unlock();
    return return_value;
}

void DaapInstance::wakeUp()
{
    u_shaped_pipe_mutex.lock();
        write(u_shaped_pipe[1], "wakeup\0", 7);
    u_shaped_pipe_mutex.unlock();
}

void DaapInstance::log(const QString &log_message, int verbosity)
{
    parent->log(log_message, verbosity);
}

void DaapInstance::warning(const QString &warn_message)
{
    parent->warning(warn_message);
}

void DaapInstance::handleIncoming()
{
    //
    //  This just makes a daapresponse object out of the raw incoming socket
    //  data. NB: assume all the response is here ... probably need to FIX
    //  that at some point.
    //

    
    
    //
    //  Collect the incoming data
    //  
    
    char incoming[10000];   // FIX this crap soon
    int length = 0;
    
    length = client_socket_to_daap_server->readBlock(incoming, 10000 - 1);
    if(length > 0)
    {
    
        DaapResponse *new_response = new DaapResponse(this, incoming, length);
        processResponse(new_response);
        delete new_response;
    }
    
}

bool DaapInstance::checkServerType(const QString &server_description)
{
    if(have_server_info)
    {
        //
        //  We already know (from previous daap responses) what kind iof
        //  server we're talking to ... so we just make sure it hasn't
        //  suddenly changed.
        //
        
        bool copasetic = true;
        
        if(server_description.left(6) == "iTunes")
        {
            if(daap_server_type != DAAP_SERVER_ITUNESLESSTHAN401)
            {
                copasetic = false;
            }
        }
        else if(server_description.left(6) == "MythTV")
        {
            if(daap_server_type != DAAP_SERVER_MYTH)
            {
                copasetic = false;
            }
        }
        else
        {
            if(daap_server_type != DAAP_SERVER_UNKNOWN)
            {
                copasetic = false;
            }
        }
        if(!copasetic)
        {
            warning(QString("daap instance is completely confused about "
                            "what kind of server it is talking to: \"%1\"")
                            .arg(server_description));
            
        }
        
    }
    else
    {
        if(server_description.left(6) == "iTunes")
        {
            //
            //  Figure out if this is 4.0.1 or greater, in which case, we
            //  can't talk to it.
            //
            
            QString itunes_version_string = server_description.section('/', -1, -1);
            itunes_version_string = itunes_version_string.section(' ',0,0);
            bool ok;
            float itunes_version = itunes_version_string.toFloat(&ok);
            if(!ok)
            {
                warning(QString("could not determine iTunes version from: %1")
                                .arg(server_description));
                daap_server_type = DAAP_SERVER_ITUNES401ORGREATER;
                return false;
            }
            else
            {
                if(itunes_version >= 4.1)
                {
                    daap_server_type = DAAP_SERVER_ITUNES401ORGREATER;
                    return false;
                }
                else
                {
                    daap_server_type = DAAP_SERVER_ITUNESLESSTHAN401;
                    log(QString("daap instance discovered service "
                                "named \"%1\" is being served out by "
                                "iTunes (v < 4.1 !!)")
                                .arg(service_name), 2);
                    return true;
                }
            }
            
        }
        else if(server_description.left(6) == "MythTV")
        {
            daap_server_type = DAAP_SERVER_MYTH;
            log(QString("daap instance discovered service "
                        "named \"%1\" is being served out by "
                        "another myth box :-)")
                        .arg(service_name), 2);
        }
        else
        {
            daap_server_type = DAAP_SERVER_UNKNOWN;
            warning(QString("daap instance did not recognize the "
                            "type of server it's connected to: \"%1\"")
                            .arg(server_description));
        }
    }
    return true;
}

void DaapInstance::processResponse(DaapResponse *daap_response)
{
    //
    //  Check the type of server
    //
    
    if(!checkServerType(daap_response->getHeader("DAAP-Server")))
    {
        //
        //  We got an iTunes 4.1 or greater server.
        //
        
        warning("daap instance cannot talk to iTunes because\n"
                "                   it uses a totally undocumented Client-DAAP-Validation header\n\n"
                "                   Please complain to Apple at:\n\n"
                "                   Apple Customer Relations, (800) 767-2775\n" 
                "                   http://www.apple.com/feedback/itunes.html\n"
               ); 
        delete client_socket_to_daap_server;
        client_socket_to_daap_server = NULL;
        return;
    }

    //
    //  Sanity check ... is the payload xdmap-tagged ?
    //
    
    if(
        daap_response->getHeader("Content-Type") != 
        "application/x-dmap-tagged"
      )
    {
        warning("daap instance got a daap response which "
                "is not x-dmap-tagged ... this is bad");
        return;
    }
    
    //
    //  Get a reference to the payload, and make a u8 version of it. Why? 
    //  Because libdaap needs a u8 to create a TagInput object, and I'm too
    //  stupid to figure out a smarter way to do this. 
    //
    
    std::vector<char> *daap_payload = daap_response->getPayload();
    u8 *daap_payload_u8 = new u8[daap_payload->size()];
    for(uint i = 0; i < daap_payload->size(); i++)
    {
        daap_payload_u8[i] = daap_payload->at(i);
    }

    Chunk raw_dmap_data(daap_payload_u8, daap_payload->size());
 
    
    //
    //  OK we have the raw payload data in a format that we can construct a
    //  TagInput object from (TagInput is part of daaplib, up in the
    //  daapserver directory). Make the TagInput object.
    //

    TagInput dmap_payload(raw_dmap_data);
    
    //
    //  Pull off the "outside container" (top level) of this data, and
    //  decide what to based on what kind of response it is. We also
    //  "rebuild" the data inside the top level tag into a well formed
    //  TagInput
    //


    Tag top_level_tag;
    Chunk internal_data;
    dmap_payload >> top_level_tag >> internal_data >> end;
    TagInput rebuilt_internal(internal_data);
            

    if(top_level_tag.type == 'msrv')
    {
        doServerInfoResponse(rebuilt_internal);
    }
    else if(top_level_tag.type == 'mlog')
    {
        doLoginResponse(rebuilt_internal);
    }
    else
    {
        warning("daap instance got a top level unknown tag in a dmap payload");
    }
}


void DaapInstance::doServerInfoResponse(TagInput& dmap_data)
{


    Tag a_tag;
    Chunk emergency_throwaway_chunk;

    while(!dmap_data.isFinished())
    {
        //
        //  Go through all the tags and update our info about the server
        //  appropriately. This is not pretty to look at, but the comments
        //  help describe what is going on
        //

        dmap_data >> a_tag;
        std::string a_string;
        u32 a_u32_variable;
        u8  a_u8_variable;
        

        switch(a_tag.type)
        {
            case 'mstt':

                //
                //  A daap status value
                //
                
                dmap_data >> a_u32_variable;
                if(a_u32_variable != 200)    // DAAP ok magic, like HTTP 200
                {
                    warning("daap instance got non 200 for DAAP status");
                }
                break;

            case 'mpro':
            case 'apro':

                //
                //  DMAP and DAAP version numbers ... which we ignore (!)
                //  for the time being.
                //
                
                dmap_data >> a_u32_variable;
                break;

            case 'minm':
            
                //
                //  Supplied service name, usually same as service name
                //  published by zeronfig
                //
                
                dmap_data >> a_string;
                {
                    QString q_string = QString(a_string);
                    if(q_string != service_name)
                    {
                        warning(QString("daap instance got conflicting names for "
                                        "service: \"%1\" versus \"%2\" (sticking "
                                        "with \"%3\")")
                                        .arg(service_name)
                                        .arg(q_string)
                                        .arg(service_name));
                    }
                }
                break;
                
            case 'mslr':
            
                //
                //  Login required? We throw away the value passed, the
                //  point is that the flag is present.
                //
                
                dmap_data >> a_u8_variable;
                server_requires_login = true;
                break;
                
            case 'mstm':
            
                //
                //  Server timeout interval (how many seconds? microseconds?
                //  it will wait before deciding something has timed out).
                //  Dunno. Save it though.
                //
                
                dmap_data >> a_u32_variable;
                server_timeout_interval = (bool) a_u32_variable;
                break;
                
            case 'msal':
            
                //
                //  Server supports auto logout?
                //
                
                dmap_data >> a_u8_variable;
                server_supports_autologout = true;
                break;
                
            case 'msau':
            
                //
                //  Authentication method. Only ever seen 2 here. Not a clue
                // 
                //
                
                dmap_data >> a_u32_variable;
                server_authentication_method = a_u32_variable;
                break;
                
            case 'msup':
            
                //
                //  Server supports update
                //
                
                dmap_data >> a_u8_variable;
                server_supports_update = true;
                break;
            
            case 'mspi':
            
                dmap_data >> a_u8_variable;
                server_supports_persistentid = true;
                break;
                
            case 'msex':
            
                //
                //  server supports extensions ... don't really have any
                //  idea what that means?
                //

                dmap_data >> a_u8_variable;
                server_supports_extensions = true;
                break;

            case 'msbr':
            
                //
                //  server supports browse (?)
                //                                

                dmap_data >> a_u8_variable;
                server_supports_browse = true;
                break;

            case 'msqy':
            
                //
                //  server supports query
                //

                dmap_data >> a_u8_variable;
                server_supports_query = true;
                break;
                
            case 'msix':
            
                //
                //  server supports index
                //

                dmap_data >> a_u8_variable;
                server_supports_index = true;
                break;
                
            case 'msrs':

                //
                //  server supports resolve
                //

                dmap_data >> a_u8_variable;
                server_supports_resolve = true;
                break;
                
            case 'msdc':
            
                //
                //  Rather important, the number of databases the server has
                //
                
                dmap_data >> a_u32_variable;
                server_numb_databases = (int) a_u32_variable;
                break;

            default:
                warning("daap instance got an unknown tag type "
                        "while doing doServerInfoResponse()");
                dmap_data >> emergency_throwaway_chunk;
        }

        dmap_data >> end;

    }

    //
    //  Well, I think we're done with that .... make note of the fact that
    //  we have recieved server-info and then try a /login ?
    //  
    
    have_server_info = true;

    DaapRequest login_request(this, "/login", server_address);
    login_request.send(client_socket_to_daap_server);
    
}


void DaapInstance::doLoginResponse(TagInput& dmap_data)
{
    bool login_status = false;
    Tag a_tag;
    Chunk emergency_throwaway_chunk;

    while(!dmap_data.isFinished())
    {
        //
        //  parse responses to a /login request
        //

        dmap_data >> a_tag;

        u32 a_u32_variable;

        switch(a_tag.type)
        {
            case 'mstt':

                //
                //  status of login request
                //
                
                dmap_data >> a_u32_variable;
                if(a_u32_variable == 200)    // like HTTP 200 (OK!)
                {
                    login_status = true;
                }
                break;

            case 'mlid':

                //
                //  session id ... important, we definitely need this
                //
                
                dmap_data >> a_u32_variable;
                session_id = (int) a_u32_variable;
                break;

            default:
                warning("daap instance got an unknown tag type "
                        "while doing doLoginResponse()");
                dmap_data >> emergency_throwaway_chunk;
        }

        dmap_data >> end;

    }

    if(session_id != -1 && login_status)
    {
        logged_in = true;
        log(QString("daap instance has managed to log "
                    "on to \"%1\" and will start loading "
                    "metadata ... ")
                    .arg(service_name), 5);

        //
        //  Time to use our new session id to get ourselves an /update
        //
        
        DaapRequest update_request(this, "/update", server_address);
        update_request.addGetVariable("session-id", session_id);
        //update_request.addGetVariable("revision-number", metadata_generation);
        update_request.send(client_socket_to_daap_server);
                
        
    }
    else
    {
        warning(QString("daap instance could not log on "
                        "to \"%1\" (password protected?) "
                        "and is giving up")
                        .arg(service_name));
    }  
}

DaapInstance::~DaapInstance()
{
}

