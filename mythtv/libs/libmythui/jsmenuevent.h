/*----------------------------------------------------------------------------
** jsmenuevent.h
**  GPL license; Original copyright 2004 Jeremy White <jwhite@whitesen.org>
**     although this is largely a derivative of lircevent.h
**--------------------------------------------------------------------------*/
#ifndef JSMENUEVENT_H_
#define JSMENUEVENT_H_

#include <QEvent>
#include <QString>

class JoystickKeycodeEvent : public QEvent
{
  public:
    enum EventID { JoystickKeycodeEventType = 24425 };

  public:
    JoystickKeycodeEvent(const QString &jsmenuevent_text, int key_code,
                         bool key_down) :
                         QEvent((QEvent::Type)JoystickKeycodeEventType),
                            m_jsmenueventtext(jsmenuevent_text),
                            m_keycode(key_code), m_keydown(key_down) {}

    QString getJoystickMenuText() const { return m_jsmenueventtext; }
    int getKeycode() const { return m_keycode; }
    bool isKeyDown() const { return m_keydown; }

  private:
    QString m_jsmenueventtext;
    int m_keycode;
    bool m_keydown;
};

class JoystickMenuMuteEvent : public QEvent
{
  public:
    enum EventID { JoystickMuteEventType = 24426 };

  public:
    JoystickMenuMuteEvent(bool mute_events) :
            QEvent((QEvent::Type)JoystickMuteEventType),
            m_muteJsmenuEvents(mute_events) {}

    bool eventsMuted() const { return m_muteJsmenuEvents; }

  private:
    bool m_muteJsmenuEvents;
};

class JoystickMenuEventLock
{
  public:
    JoystickMenuEventLock(bool lock_events = true);
    ~JoystickMenuEventLock();
    void lock();
    void unlock();

  private:
    bool m_eventsLocked;
};

#endif
