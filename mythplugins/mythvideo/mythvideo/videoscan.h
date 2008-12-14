#ifndef VIDEO_SCANNER_H
#define VIDEO_SCANNER_H

#include <QObject> // for moc

class QStringList;

class VideoScanner : public QObject
{
    Q_OBJECT

  public:
    VideoScanner();
    ~VideoScanner();

    void doScan(const QStringList &dirs);

  signals:
    void finished();

  public slots:
    void finishedScan();

  private:
    class VideoScannerThread *m_scanThread;
    bool                m_cancel;
};

#endif
