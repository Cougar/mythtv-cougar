#ifndef ZC_CLIENT_H_
#define ZC_CLIENT_H_
/*
	zc_client.h

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project

    !! This code is covered the the Apple Public Source License !!
    
    See ./apple/APPLE_LICENSE
	
*/

#include <qthread.h>
#include <qptrlist.h>

#include "./apple/mDNSClientAPI.h"// Defines the interface to the mDNS core code
#include "./apple/mDNSPosix.h"    // Defines the specific types needed to run mDNS on this platform

#include "../../mfd.h"




class MFDService
{
    //
    //  This is an object that holds existing services
    //  and manages resolution of services to instances
    //  (ie. the actual IP address and port number)
    //
  
  public:
   
    MFDService(QString name, QString type, QString domain);
    ~MFDService();
    
    const QString & getName(){return service_name;}
    const QString & getType(){return service_type;}
    const QString & getDomain(){return service_domain;}
    const QString & getLongDescription();
    const QString & getFormalServiceDescription();
    const QString & getFormalServiceRemoval();
    
    void  takeOwnershipOfServiceInfo(ServiceInfo *si, ServiceInfoQuery *siq);
    void  setResolved(uint ip1, uint ip2, uint ip3, uint ip4, uint port_high, uint port_low);
    bool  isResolved(){return resolved;}
    
    
  private:
  
    QString service_name;
    QString service_type;
    QString service_domain;
    bool    resolved;

    ServiceInfo         *service_info;
    ServiceInfoQuery    *service_info_query;
    
    uint     ip_address_one;
    uint     ip_address_two;
    uint     ip_address_three;
    uint     ip_address_four;
    
    uint     port_number;

    QString  long_description;
    QString  formal_description;
    QString  formal_removal;

    enum ServiceLocation
    {
        SLOCATION_UNKNOWN = 0,
        SLOCATION_HOST, 
        SLOCATION_LAN, 
        SLOCATION_NET,  //  don't actually use that one yet.
        MAX_SLOCATION
    };

    int     service_location;    
};


class ZeroConfigClient: public MFDBasePlugin
{
    //
    //  The client continously looks for
    //  advertised services anywhere on the
    //  local network.
    //
    
  public:
  
    ZeroConfigClient(MFD *owner, int identifier);
    ~ZeroConfigClient();
    void run();
    void handleBrowseCallback(mDNS *const m, DNSQuestion *question, const ResourceRecord *const answer, mDNSBool AddRecord);
    void handleServiceQueryCallback(mDNS *const m, ServiceInfoQuery *query);
    void browseForService(mDNS *const mdns_storage, const QString& service_name);
    void addRequest(const QStringList &tokens, int socket_identifier);
    void doSomething(const QStringList &tokens, int socket_identifier);
    
  private:
  
    bool        checkServiceName(const QString &new_name);
    MFDService* findServiceByName(const QString &a_name);
    void       removeService(const QString &name, const QString &type, const QString &domain);
  
    
    MFD         *parent;
    int         unique_identifier;

    QPtrList<DNSQuestion>   dns_questions;
    QPtrList<MFDService>    available_services;

    QPtrList<SocketBuffer>  things_to_do;
    QMutex                  things_to_do_mutex;
    
    MFDFileDescriptorWatchingPlugin *fd_watcher;
    QMutex file_descriptors_mutex;
};

#endif  // zc_client_h_

