#ifndef SERVER_H_
#define SERVER_H_

#include <qsocket.h>
#include <qserversocket.h>

class MythServer : public QServerSocket
{
    Q_OBJECT
  public:
    MythServer(int port, QObject *parent = 0);

    void newConnection(int socket);

  signals:
    void newConnect(QSocket *);
    void endConnect(QSocket *);

  private slots:
    void discardClient();
};


#endif
