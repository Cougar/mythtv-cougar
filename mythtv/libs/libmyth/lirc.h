#ifndef LIRC_H_
#define LIRC_H_

#include <lirc/lirc_client.h>
#include <qobject.h>
#include <qsocket.h>
#include <qstring.h>

#include "mythdialogs.h"

class LircClient : public QObject
{
    Q_OBJECT
  public:
    LircClient(QObject *main_window);
    ~LircClient();
    int Init(QString &config_file, QString &program);
    void Process(void);

  private:
    struct lirc_config *lircConfig;
    QObject *mainWindow;
    pthread_t pth;
};

#endif
