#ifndef HTTPRESPONSE_H_
#define HTTPRESPONSE_H_
/*
	httpresponse.h

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Headers for an object to build http responses and send them
*/

#include <vector>
#include <iostream>
using namespace std;

#include <qstring.h>
#include <qdict.h>
#include <qfile.h>
#include <FLAC/all.h>

#include "httprequest.h"
#include "clientsocket.h"

enum FileSendTransformation {
    FILE_TRANSFORM_NONE = 0,
    FILE_TRANSFORM_TOWAV
};



class HttpResponse
{

  public:

    HttpResponse(MFDHttpPlugin *owner, HttpRequest *requestor);
    ~HttpResponse();
    
    void setError(int error_number);
    void send(MFDServiceClientSocket *which_client);
    void addHeader(const QString &new_header);
    void setPayload(char *new_payload, int new_payload_size);
    void sendFile(
                    QString file_path, 
                    int skip, 
                    FileSendTransformation transform = FILE_TRANSFORM_NONE
                 );


    //
    //  functions that are only of use for streaming out flac's as wav files
    //
    
    FLAC__StreamDecoderWriteStatus flacWrite(const FLAC__Frame *frame, const FLAC__int32 *const buffer[]);
    void                           flacMetadata(const FLAC__StreamMetadata *metadata);
    void                           flacError(FLAC__StreamDecoderErrorStatus status);
    
    
  private:
    
    void addText(std::vector<char> *buffer, QString text_to_add);
    void createHeaderBlock(std::vector<char> *header_block, int payload_size);
    bool sendBlock(MFDServiceClientSocket *which_client, std::vector<char> block_to_send);
    void streamFile(MFDServiceClientSocket *which_client);
    void convertToWavAndStreamFile(MFDServiceClientSocket *which_client);
    void streamEmptyWav(MFDServiceClientSocket *which_client);

    int     status_code;
    QString status_string;
    bool    all_is_well;


    std::vector<char> payload;

    QDict<HttpHeader> headers;
    HttpRequest       *my_request;
    MFDHttpPlugin     *parent; 
    
    
    int     range_begin;
    int     range_end;
    int     total_possible_range;
    int     stored_skip;
    
    QFile                  *file_to_send;
    FileSendTransformation file_transformation;

    //
    //  Some flac metadatadata values (yes, this is a cruddy place for this
    //  to be)
    //
    
    int                     flac_bitspersample;
    int                     flac_channels;
    long                    flac_frequency;
    unsigned long           flac_totalsamples;
    MFDServiceClientSocket *flac_client;
};

#endif

