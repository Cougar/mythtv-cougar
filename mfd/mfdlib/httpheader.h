#ifndef HTTPHEADER_H_
#define HTTPHEADER_H_
/*
    httpheader.h

    (c) 2003 Thor Sigvaldason and Isaac Richards
    Part of the mythTV project
    
    Headers for an object to hold ... well ... headers

*/

#include <qstring.h>

class HttpHeader
{

  public:
  
    HttpHeader(const QString &input_line);
    ~HttpHeader();

    const QString& getField();
    const QString& getValue();

  private:
  
    QString field;
    QString value;
};


#endif

