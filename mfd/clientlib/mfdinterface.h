#ifndef MFDINTERFACE_H_
#define MFDINTERFACE_H_
/*
	mfdinterface.h

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Core entry point for the facilities available in libmfdclient

*/

#include <qobject.h>
#include <qptrlist.h>
#include <qintdict.h>

#include <mythtv/generictree.h>

class DiscoveryThread;
class MfdInstance;

class MfdInterface : public QObject
{

  Q_OBJECT

  public:

    MfdInterface();
    ~MfdInterface();

    void playAudio(int which_mfd, int container, int type, int which_id, int index=0);
    void stopAudio(int which_mfd);
    
  signals:

    //
    //  Signals we send out when something happens. Linking client wants to
    //  connect to all or some of these
    //

    void mfdDiscovery(int, QString, QString, bool);
    void audioPaused(int, bool);
    void audioStopped(int);
    void audioPlaying(int, int, int, int, int, int, int, int, int);
    void metadataChanged(int, GenericTree*);

  protected:
  
    void customEvent(QCustomEvent *ce);
 

  private:

    int             bumpMfdId(){ ++mfd_id_counter; return mfd_id_counter;}
    MfdInstance*    findMfd(
                            const QString &a_host,
                            const QString &an_ip_addesss,
                            int a_port
                           );
    void            swapMetadataTree(int which_mfd, GenericTree *new_metadata_tree);
  
    DiscoveryThread       *discovery_thread;
    QPtrList<MfdInstance> *mfd_instances;
    int                   mfd_id_counter;
    
    QIntDict<GenericTree>   metadata_trees;
};

#endif
