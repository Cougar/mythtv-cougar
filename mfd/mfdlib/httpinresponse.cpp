/*
    httpinresponse.cpp

    (c) 2003 Thor Sigvaldason and Isaac Richards
    Part of the mythTV project
    
    Methods for parsing an incoming http response

*/

#include <qstringlist.h>


#include "httpinresponse.h"

HttpInResponse::HttpInResponse(
                            char *raw_incoming, 
                            int length
                          )
{
    preserved_top_line = "";
    expected_payload_size = -1;
    raw_length = length;
    all_is_well = true;
    headers.setAutoDelete(true);


    
    //
    //  Read the response
    //
    
    int parse_point = 0;
    bool first_line = true;
    char parsing_buffer[10000]; //  FIX
    QString top_line = "";

    while(readLine(&parse_point, parsing_buffer, raw_incoming))
    {
        if(first_line)
        {
            //  
            //  First line of an http response is pretty important ... do
            //  some checking that things are sane
            //
            
            top_line = QString(parsing_buffer);
            preserved_top_line = top_line;
            QStringList line_tokens = QStringList::split( " ", top_line );

            if(line_tokens.count() < 3)
            {
                warning(QString("http in response got a malformed first line: \"%1\"")
                        .arg(top_line));
                all_is_well = false;
                return;
            }
            if(line_tokens[0] != "HTTP/1.1")
            {
                warning(QString("http in response wanted \"HTTP/1.1\" but got \"%1\"")
                                .arg(line_tokens[0])); 
                all_is_well = false;
                return;
            }
                
            //
            //  Set the http status code
            //
                
            bool ok = true;
            status_code = line_tokens[1].toInt(&ok);
            if(!ok || status_code != 200)
            {
                warning(QString("http in response got HTTP bad (not 200) http status: %1")
                                .arg(status_code));
                all_is_well = false;
                return;
            }
                
            first_line = false;
        }
        else
        {
            //
            //  Store the headers
            //

            QString header_line = QString(parsing_buffer);
            HttpHeader *new_header = new HttpHeader(header_line);
            headers.insert(new_header->getField(), new_header);
            
        }
    }

    // printHeaders();
    
    //
    //  Now we need to store the payload.
    //
    

    payload.clear();
    payload.insert( 
                    payload.begin(), 
                    (raw_incoming + parse_point), 
                    (raw_incoming + parse_point + (length - parse_point))
                  );
    
    //
    //  Make note of how many bytes we expect in total
    //
    
    HttpHeader *content_length_header = headers.find("Content-Length");
    if(content_length_header)
    {
        bool ok = true;
        int content_length = content_length_header->getValue().toInt(&ok);
        if(ok)
        {
            expected_payload_size = content_length;
            return;
        }
    }
    
    warning("http in response could not set the "
            "expected payload size ... BAD  ");
    all_is_well = false;
}

int HttpInResponse::readLine(int *parse_point, char *parsing_buffer, char *raw_response)
{
    int  amount_read = 0;
    bool keep_reading = true;
    int  index = *parse_point;
    while(keep_reading)
    {
        
        if(index >= raw_length )
        {
            //
            //  No data at all.
            //

            parsing_buffer[amount_read] = '\0';
            keep_reading = false;
        }
         
        else if(raw_response[index] == '\r')
        {
            //
            // ignore
            //

            index++;
        }
        else if(raw_response[index] == '\n')
        {
            //
            //  done with this line
            //

            index++;
            parsing_buffer[amount_read] = '\0';
            keep_reading = false;
        }
        else
        {
            parsing_buffer[amount_read] = raw_response[index];
            index++;
            amount_read++;
        }
    }
    *parse_point = index;
    return amount_read;
}

bool HttpInResponse::complete()
{
    if( ((int) (payload.size())) == expected_payload_size)
    {
        return true;
    }
    return false;
}

void HttpInResponse::appendToPayload(char *raw_incoming, int length)
{
    payload.insert( 
                    payload.end(), 
                    raw_incoming, 
                    raw_incoming + length
                  );
}


QString HttpInResponse::getHeader(const QString& field_label)
{
    HttpHeader *which_one = headers.find(field_label);
    if(which_one)
    {
        return which_one->getValue();
    }
    return NULL;
}

void HttpInResponse::printHeaders()
{
    cout << "============== Debugging Output - HTTP Response Headers ===============" << endl;
    cout << preserved_top_line << endl;
    QDictIterator<HttpHeader> it( headers );
    for( ; it.current(); ++it )
    {
        cout << it.currentKey() << ": " << it.current()->getValue() << endl;
    }
    
    cout << "=======================================================================" << endl;
}

void HttpInResponse::warning(const QString &warn_text)
{
    cout << "WARNING httpinresponse.o: " << warn_text << endl;
}

HttpInResponse::~HttpInResponse()
{
    headers.clear();
}
