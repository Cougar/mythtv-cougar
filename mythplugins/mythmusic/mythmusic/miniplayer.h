#ifndef MINIPLAYER_H_
#define MINIPLAYER_H_

#include <qstring.h>

#include "mythtv/mythdialogs.h"

class MusicPlayer;
class QTimer;
class Metadata;

/// Preferred position to place popup miniplayer.
enum PlayerPosition 
{
    MP_POSTOPDIALOG,
    MP_POSBOTTOMDIALOG,
    MP_POSCENTERDIALOG
};

class MPUBLIC MiniPlayer : public MythThemedDialog
{
  Q_OBJECT

  public:
    MiniPlayer(MythMainWindow *parent, 
                    MusicPlayer *parentPlayer,
                    const char *name = 0,
                    bool setsize = true);
    ~MiniPlayer();

  public slots:
    virtual void show();
    virtual void hide();
    void timerTimeout(void);
    void showPlayer(int showTime);
    void showInfoTimeout(void);

  protected:
    virtual void keyPressEvent(QKeyEvent *e);
    virtual void customEvent(QCustomEvent *event);

  private:
    void    wireupTheme(void);
    QString getTimeString(int exTime, int maxTime);
    void    updateTrackInfo(Metadata *mdata);
    void    seekforward(void);
    void    seekback(void);
    void    seek(int pos);
    void    increaseRating(void);
    void    decreaseRating(void);
    void    showShuffleMode(void);
    void    showRepeatMode(void);
    void    showVolume(void);

    int           m_currTime, m_maxTime;

    MusicPlayer  *m_parentPlayer;
    int           m_popupWidth;
    int           m_popupHeight;

    QTimer       *m_displayTimer;
    QTimer       *m_infoTimer;

    bool          m_showingInfo;

    UITextType   *m_titleText;
    UITextType   *m_artistText;
    UITextType   *m_albumText;
    UITextType   *m_timeText;
    UITextType   *m_infoText;
    UIImageType  *m_coverImage;

    UITextType   *m_shufflestateText;
    UITextType   *m_repeatstateText;

    UIRepeatedImageType *m_ratingsImage;
};

#endif
