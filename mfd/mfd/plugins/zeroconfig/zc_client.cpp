/*
    zc_client.cpp

    (c) 2003 Thor Sigvaldason and Isaac Richards
    Part of the mythTV project
    
    !! This code is covered the the Apple Public Source License !!
    
    See ./apple/APPLE_LICENSE

*/

#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>


#include <qapplication.h>

#include "zc_supervisor.h"

static  mDNS mDNSStorage;                       // mDNS core uses this to store its globals
static  mDNS_PlatformSupport PlatformStorage;   // Stores this platform's globals
#define RR_CACHE_SIZE 500                       // Same arbitrary number that Apple uses
static  CacheRecord gRRCache[RR_CACHE_SIZE]; // cache foe resource records (?)

ZeroConfigClient     *zero_config_client = NULL;

/*
---------------------------------------------------------------------
*/

MFDService::MFDService(ZeroConfigClient *owner, QString name, QString type, QString domain)
{
    parent = owner;
    service_name = name;
    service_type = type;
    service_domain = domain;
    resolved = false;
    service_info = NULL;
    service_info_query = NULL;
    
    //
    //  Zero out stuff we won't know until (if?) the service gets resolved
    //
    
    ip_address_one   = 0;
    ip_address_two   = 0;
    ip_address_three = 0;
    ip_address_four  = 0;
    
    port_number = 0;

    service_location = SLOCATION_UNKNOWN;
}

void MFDService::takeOwnershipOfServiceInfo(ServiceInfo *si, ServiceInfoQuery *siq)
{
    //
    //  This object takes responsibility for deleting them.
    //
    
    service_info = si;
    service_info_query = siq;
}

void MFDService::setResolved(uint ip1, uint ip2, uint ip3, uint ip4, uint port_high, uint port_low)
{
    //
    //  This gets called in the second stage of the lookup process, when the
    //  mDNS code has resolved a service to an actual ip address and port.
    //
    
    ip_address_one = ip1;        
    ip_address_two = ip2;        
    ip_address_three = ip3;        
    ip_address_four = ip4;
    port_number = (port_high * 256) + port_low;
    resolved = true;

    //
    //  Now, is this service on this host, on the local. network or (at some
    //  point in the future) elsewhere
    //
    
    char my_hostname[2049];
    if(gethostname(my_hostname, 2048) < 0)
    {
        warning("could not call gethostname()");
        return;
    }

    QString local_hostname = my_hostname;

    QString ip_address_string = QString("%1.%2.%3.%4").arg(ip1).arg(ip2).arg(ip3).arg(ip4);

    struct hostent* host_information;
    struct in_addr  ip_address_number;
    ip_address_number.s_addr = inet_addr(ip_address_string.ascii());
    host_information = gethostbyaddr((char*)&(ip_address_number.s_addr), sizeof(ip_address_number), AF_INET);
    QString service_hostname = "";
    if(!host_information)
    {
        warning(QString("could not call gethostbyaddr() using ip address %1 (will assume lan/net)")
                .arg(ip_address_string));
    }
    else
    {
        service_hostname = host_information->h_name;
        service_hostname = service_hostname.section('.',0,0);
    }

    //
    //  Determine service location: local if hostname's are the same (i.e. 
    //  even if different network interfaces) or no IP hostname lookup (i.e.
    //  probably no functioning network), lan otherwise, but at somepoint
    //  we'll want to add net (on the network, beyond the local lan).
    //

    if(service_hostname == local_hostname)
    {
        service_location = SLOCATION_HOST;
    }
    else
    {
        service_location = SLOCATION_LAN;
    }
}

const QString& MFDService::getLongDescription()
{
    QString location_string = "Unknown";
    
    switch(service_location)
    {
        case SLOCATION_HOST:
            location_string = "host";
            break;
        case SLOCATION_LAN:
            location_string = "lan";
            break;
    }

    //
    //  Format this objects state for printing to, say, a logfile.
    //

    long_description = QString("%1.%2.%3.%4:%5 (%6)")
               .arg(ip_address_one)
               .arg(ip_address_two)
               .arg(ip_address_three)
               .arg(ip_address_four)
               .arg(port_number)
               .arg(location_string);
    return long_description;
}

const QString& MFDService::getFormalServiceDescription()
{
    //
    //  Build a string that can get passed to a client using the mfd
    //  protocol.
    //
    
    QString location_string = "unknown";
    
    switch(service_location)
    {
        case SLOCATION_HOST:
            location_string = "host";
            break;
        case SLOCATION_LAN:
            location_string = "lan";
            break;
    }

    QString formal_type = "unknown";
    if(service_type == "_mfdp._tcp.")
    {
        formal_type = "mfdp";
    }
    else if(service_type == "_macp._tcp.")
    {
        formal_type = "macp";
    }
    else if(service_type == "_daap._tcp.")
    {
        formal_type = "daap";
    }
    else if(service_type == "_mdcap._tcp.")
    {
        formal_type = "mmdp";
    }
    else if(service_type == "_mhttp._tcp.")
    {
        formal_type = "mhttp";
    }

    formal_description = QString("services found %1 %2 %3.%4.%5.%6 %7 %8")
               .arg(location_string)
               .arg(formal_type)
               .arg(ip_address_one)
               .arg(ip_address_two)
               .arg(ip_address_three)
               .arg(ip_address_four)
               .arg(port_number)
               .arg(service_name);
               

    return formal_description;
}

const QString& MFDService::getFormalServiceRemoval()
{
    //
    //  We get asked to build a string describing our demise just before we
    //  get delete.
    //

    QString location_string = "unknown";
    
    switch(service_location)
    {
        case SLOCATION_HOST:
            location_string = "host";
            break;
        case SLOCATION_LAN:
            location_string = "lan";
            break;
    }

    QString formal_type = "unknown";
    if(service_type == "_mfdp._tcp.")
    {
        formal_type = "mfdp";
    }
    else if(service_type == "_macp._tcp.")
    {
        formal_type = "macp";
    }
    else if(service_type == "_daap._tcp.")
    {
        formal_type = "daap";
    }
    else if(service_type == "_mdcap._tcp.")
    {
        formal_type = "mmdp";
    }
    else if(service_type == "_mhttp._tcp.")
    {
        formal_type = "mhttp";
    }

    formal_removal = QString("services lost %1 %2 %3.%4.%5.%6 %7 %8")
               .arg(location_string)
               .arg(formal_type)
               .arg(ip_address_one)
               .arg(ip_address_two)
               .arg(ip_address_three)
               .arg(ip_address_four)
               .arg(port_number)
               .arg(service_name);

    return formal_removal;
}

MFDService::~MFDService()
{
    if(service_info)
    {
        delete service_info;
        service_info = NULL;
    }
    if(service_info_query)
    {
        delete service_info_query;
        service_info_query = NULL;
    }
}

/*
---------------------------------------------------------------------
*/



//
//  These two functions are C callbacks that the Apple mDNS calls when it
//  (1) figures out where to resolve a given service, or (2) discovers a
//  service we're listening for (respectively). We just jump from the
//  callbacks back into out client object.
//
extern "C" void ServiceQueryCallback(mDNS *const m, ServiceInfoQuery *query)
{
    zero_config_client->handleServiceQueryCallback(m, query);
}

extern "C" void BrowseCallback(mDNS *const m, DNSQuestion *question, const ResourceRecord *const answer, mDNSBool AddRecord)
{
    zero_config_client->handleBrowseCallback(m, question, answer, AddRecord);
}

/*
---------------------------------------------------------------------
*/



ZeroConfigClient::ZeroConfigClient(MFD *owner, int identifier)
                 :MFDServicePlugin(owner, identifier, -1, "zeroconfig client", false)
{

    dns_questions.clear();
    dns_questions.setAutoDelete(true);
    
    available_services.clear();
    available_services.setAutoDelete(true);

    things_to_do.clear();
    things_to_do.setAutoDelete(true);
}


void ZeroConfigClient::handleServiceQueryCallback(mDNS *const m, ServiceInfoQuery *query)
{
    //
    //  This get called when the Apple mDNS code has figured out
    //  where (ie. ip address and port number a service resolves to).
    //

    if(m){} //  -Wall -W

    domainname name = query->info->name;
    char nameC[256];
    ConvertDomainNameToCString_unescaped(&name, nameC);
    
    //
    //  Figure out which of our MFDService objects
    //  this callback data refers to
    //
    
    MFDService *a_service = findServiceByName(nameC);
    
    if(a_service)
    {
        switch(query->info->ip.type)
        {
            case mDNSAddrType_IPv4:
                a_service->setResolved(
                                        query->info->ip.ip.v4.b[0],
                                        query->info->ip.ip.v4.b[1],
                                        query->info->ip.ip.v4.b[2],
                                        query->info->ip.ip.v4.b[3],
                                        query->info->port.b[0],                               
                                        query->info->port.b[1]
                                      );
                log(QString("resolved service \"%1\" to %2")
                    .arg(a_service->getName())
                    .arg(a_service->getLongDescription()),1);
                sendCoreMFDMessage(a_service->getFormalServiceDescription());
                break;

            default:
                warning(QString("got a non-IP4 protocol resolution response for: %1")
                        .arg(nameC));
                break;

        }
    }
    else
    {
        warning(QString("was told it could resove %1, but it didn't know it existed")
                .arg(nameC));
    }
    
    //
    //  Tell the mDNS core code that we are done
    //  trying to resolve this service (which it should
    //  already know ... duh ... since it launched all
    //  this via a callback).
    //

    mDNS_StopResolveService(m, query);
    
}

void ZeroConfigClient::handleBrowseCallback(mDNS *const m, DNSQuestion *question, const ResourceRecord *const answer, mDNSBool AddRecord)
{
    if(m)
    {
        //  Apple's callback structure requires mDNS variable, but I can't
        //  imagine why we need it
    }
    if(question)
    {
        // dito
    }

    domainlabel name;
    domainname  type;
    domainname  domain;
    char nameC[256];
    char typeC[256];
    char domainC[256];

    if(answer->rrtype != kDNSType_PTR)
    {
        fatal("zeroconfig client object is getting callbacks from Apple code in a format it doesn't understand");
    }

    //
    //  Convert mDNS/Apple data structures into normal variables
    //

    DeconstructServiceName(&answer->rdata->u.name, &name, &type, &domain);
    ConvertDomainLabelToCString_unescaped(&name, nameC);
    ConvertDomainNameToCString(&type, typeC);
    ConvertDomainNameToCString(&domain, domainC);

    if (!AddRecord)
    {
        removeService(nameC, typeC, domainC);
    }
    else
    {
        if(checkServiceName(nameC))
        {
            MFDService *new_service = new MFDService(this, nameC, typeC, domainC);
            available_services.append(new_service);

            //
            //  YAH, new service found
            //

            log(QString("found service called \"%1\" (%2%3)")
                       .arg(new_service->getName())
                       .arg(new_service->getType())
                    .arg(new_service->getDomain()),1);
                    
            //
            //  In direct violation of how Apple thinks about Rendezvous, we
            //  try and resolve every service immediately (the mfd and its
            //  interface to clients only cares about live/resolved
            //  services, not things that might need to be resolved at some
            //  point (like a network printer)).
            //

            ServiceInfo         *service_info = new ServiceInfo;
            ServiceInfoQuery    *service_info_query = new ServiceInfoQuery;

            ConstructServiceName(&(service_info->name), &name, &type, &domain);
            service_info->InterfaceID = answer->InterfaceID;
            service_info->InterfaceID = mDNSInterface_Any;
    
            new_service->takeOwnershipOfServiceInfo(service_info, service_info_query);
            mStatus status = mDNS_StartResolveService(m, service_info_query, service_info, ServiceQueryCallback, NULL);    
            if(status != mStatus_NoError )
            {
                warning(QString("could not call a StartResolveService() for %1 (%2%3)")
                        .arg(nameC)
                        .arg(typeC)
                        .arg(domainC));
            }
        }
    }
}

void ZeroConfigClient::browseForService(mDNS *const mdns_storage, const QString& service_name)
{
    domainname  type;
    domainname  domain;

    DNSQuestion *a_question = new DNSQuestion;


    //
    //  Munge the variables around and tell the mDNS code to start looking
    //  for this Service.
    //

    MakeDomainNameFromDNSNameString(&type, service_name);
    MakeDomainNameFromDNSNameString(&domain, "local.");
    mStatus status = mDNS_StartBrowse(mdns_storage, a_question, &type, &domain, mDNSInterface_Any, BrowseCallback, NULL);
    
    if(status != mStatus_NoError)
    {
        fatal("zeroconfig client object could not register an mDNS_StartBrowse request");
        return;
    }

    //
    //  Store the question so we can delete it later.
    //

    dns_questions.append(a_question);
}

void ZeroConfigClient::run()
{

    mStatus     status;

    //
    //  Initialize Apple's mDNS code
    //

    status = mDNS_Init(
                        &mDNSStorage, 
                        &PlatformStorage,
                        gRRCache, 
                        RR_CACHE_SIZE,
                        mDNS_Init_DontAdvertiseLocalAddresses,
                        mDNS_Init_NoInitCallback, 
                        mDNS_Init_NoInitCallbackContext
                      );

    if(status != mStatus_NoError)
    {
        fatal("zeroconfig client object could not initialize mDNS code");
        return;
    }

    //
    //  Start a browse for every kind of service we (as an mfd) care about
    //  At some point, these should be defined by the pluged in modules (?).
    //

    browseForService(&mDNSStorage, "_mfdp._tcp.");
    browseForService(&mDNSStorage, "_macp._tcp.");
    browseForService(&mDNSStorage, "_daap._tcp.");
    browseForService(&mDNSStorage, "_mdcap._tcp.");
    browseForService(&mDNSStorage, "_mhttp._tcp.");

    //
    //  Set up some file descriptors and a pipe. This lets a separate thread
    //  just sit and wait for any new mDNS data and wake us up when it
    //  arrives. The pipe just lets us wake up that thread from this thread.
    //    


    while(keep_going)
    {
        //
        //  Pull off any pending command request
        //

        QStringList pending_tokens;
        int         pending_socket = 0;
        things_to_do_mutex.lock();
            if(things_to_do.count() > 0)
            {
                pending_tokens = things_to_do.getFirst()->getTokens();
                pending_socket = things_to_do.getFirst()->getSocketIdentifier();
                things_to_do.removeFirst();
            }
        things_to_do_mutex.unlock();
      
        //
        //  If we have something to do (respond to requests from a client
        //  connected to the mfd), then go off and do it.
        //
              
        if(pending_tokens.count() > 0)
        {
            doSomething(pending_tokens, pending_socket);
        }
        else
        {

            //
            //  Let mDNS code do whatever it needs to do, and then tell us when
            //  it next wants to be called.
            //

            int next_time = mDNS_Execute(&mDNSStorage);

            int time_difference = next_time - mDNSPlatformTimeNow();
            int seconds_to_wait = ((int) (time_difference / 1000));
            int usecs_to_wait = time_difference % 1000;
            if(seconds_to_wait < 0)
            {
                seconds_to_wait = 0;
            }
            if(usecs_to_wait < 0)
            {
                usecs_to_wait = 0;
            }
        
            timeout.tv_sec = seconds_to_wait;
            timeout.tv_usec = usecs_to_wait;
        
            int nfds = 0;
            fd_set readfds;
            FD_ZERO(&readfds);

            mDNSPosixGetFDSet(&mDNSStorage, &nfds, &readfds, &timeout);

            //
            //  Add our pipe to the watcher thread as one of file descriptors it
            //  should monitor
            //

            FD_SET(u_shaped_pipe[0], &readfds);
            if(nfds <= u_shaped_pipe[0])
            {
                nfds = u_shaped_pipe[0] + 1;
            }
 
            int result = select(nfds, &readfds, NULL, NULL, &timeout);
            if(result < 0)
            {
                warning("got an error on select()");
            }

            if(FD_ISSET(u_shaped_pipe[0], &readfds))
            {
                u_shaped_pipe_mutex.lock();
                    char read_back[10];
                    read(u_shaped_pipe[0], read_back, 9);
                u_shaped_pipe_mutex.unlock();
            }

            mDNSPosixProcessFDSet(&mDNSStorage, &readfds);

        }    
    }

    log("leaving its event loop", 10);

    //
    //  We're done and exiting. Clean up a bit.
    //

    for(uint i = 0; i < dns_questions.count(); i++)
    {
        DNSQuestion *a_question = dns_questions.at(i);
        mDNS_StopQuery(&mDNSStorage, a_question);
    }
      
    mDNS_Close(&mDNSStorage);
    dns_questions.clear();
      
      
}

bool ZeroConfigClient::checkServiceName(const QString &new_name)
{
    //
    //  These have to be unique! The Apple mDNS code my not strictly demand
    //  that they are unique, but it is sufficiently opaque (it has
    //  variables called "Opaque", really ... I'm not kidding) that it's
    //  just a whole lot simpler to demand uniqueness.
    //
    
    QPtrListIterator<MFDService> iterator(available_services);
    MFDService *a_service;
    while ( (a_service = iterator.current()) != 0 )
    {
        ++iterator;
        if(a_service->getName() == new_name)
        {
            return false;
        }
    }
    return true;
}

MFDService * ZeroConfigClient::findServiceByName(const QString &a_name)
{
    QPtrListIterator<MFDService> iterator(available_services);
    MFDService *a_service;
    while ( (a_service = iterator.current()) != 0 )
    {
        ++iterator;
        QString long_name = a_service->getName() + "."
                          + a_service->getType()
                          + a_service->getDomain();
        if(long_name == a_name)
        {
            return a_service;
        }
    }
    return NULL;
}

void ZeroConfigClient::removeService(const QString &name, const QString &type, const QString &domain)
{
    QPtrListIterator<MFDService> iterator(available_services);
    MFDService *a_service;
    while ( (a_service = iterator.current()) != 0 )
    {
        ++iterator;
        if(a_service->getName()   == name &&
           a_service->getType()   == type &&
           a_service->getDomain() == domain)
        {
            sendCoreMFDMessage(a_service->getFormalServiceRemoval());
            log(QString("lost service called %1 (%2%3)")
                       .arg(name)
                       .arg(type)
                       .arg(domain),1);

            available_services.remove(a_service);
            return;
        }
    }
    warning(QString("asked to remove a service it didn't know existed: %1 (%2%3)")
            .arg(name)
            .arg(type)
            .arg(domain));
}

void ZeroConfigClient::addRequest(const QStringList &tokens, int socket_identifier)
{
    SocketBuffer *sb = new SocketBuffer(tokens, socket_identifier);
    things_to_do_mutex.lock();
        things_to_do.append(sb);
        if(things_to_do.count() > 99)
        {
            warning(QString("more than %1 pending commands")
                    .arg(things_to_do.count()));
        }
    things_to_do_mutex.unlock();
}

void ZeroConfigClient::doSomething(const QStringList &tokens, int socket_identifier)
{
    if(tokens.count() < 2)
    {
        warning(QString("got passed insufficient tokens: %1")
                .arg(tokens.join(" ")));
        huh(tokens, socket_identifier);
        return;
    }
    if(tokens[1] == "list")
    {
        QPtrListIterator<MFDService> iterator(available_services);
        MFDService *a_service;
        while ( (a_service = iterator.current()) != 0 )
        {
            ++iterator;
            if(a_service->isResolved())
            {
                sendCoreMFDMessage(a_service->getFormalServiceDescription(), socket_identifier);            
            }
        }
        return;
    }
    warning(QString("got passed tokens is doesn't understand: %1")
                .arg(tokens.join(" ")));
    huh(tokens, socket_identifier);
}

ZeroConfigClient::~ZeroConfigClient()
{
    dns_questions.clear();
    available_services.clear();
}
