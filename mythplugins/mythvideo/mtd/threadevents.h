#ifndef THREADEVENTS_H_
#define THREADEVENTS_H_
/*
	threadevents.h

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Headers for custom events that the main mtd and
	the job threads can pass to each other.

*/

#include <qevent.h>

class LoggingEvent : public QCustomEvent
{
  public:

    LoggingEvent(const QString & init_logging_string);
    const QString & getString();

  private:

    QString logging_string;
};

class ErrorEvent : public QCustomEvent
{
  public:
    
    ErrorEvent(const QString &init_error_string);
    const QString & getString();
    
  private:
  
    QString error_string;  
};

#endif
