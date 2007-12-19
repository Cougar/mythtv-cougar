#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <string>
#include <qobject.h>
#include <qiodevice.h>
#include <qdir.h>
#include <qfile.h>
using namespace std;

#include <mythtv/mythconfig.h>
#include "cddecoder.h"
#include "constants.h"
#include <mythtv/audiooutput.h>
#include "metadata.h"

#include <mythtv/mythcontext.h>
#include <mythtv/mythmediamonitor.h>
#include <mythtv/httpcomms.h>

CdDecoder::CdDecoder(const QString &file, DecoderFactory *d, QIODevice *i, 
                     AudioOutput *o) 
         : Decoder(d, i, o)
{
    filename = file;
    inited = FALSE;
}

CdDecoder::~CdDecoder(void)
{
    if (inited)
        deinit();
}

bool CdDecoder::initialize()
{
    inited = true;
    return true;
}

void CdDecoder::seek(double pos)
{   
    (void)pos;
}

void CdDecoder::stop()
{   
}   

void CdDecoder::run()
{
}

void CdDecoder::flush(bool final)
{
    (void)final;
}

void CdDecoder::deinit()
{
    inited = false;
}

int CdDecoder::getNumTracks(void)
{
    return 0;
}

int CdDecoder::getNumCDAudioTracks(void)
{
    return 0;
}

Metadata* CdDecoder::getMetadata(int track)
{
    return new Metadata();
}

Metadata *CdDecoder::getLastMetadata()
{
    return new Metadata();
}

Metadata *CdDecoder::getMetadata()
{
    return new Metadata();
}    

void CdDecoder::commitMetadata(Metadata *mdata)
{
    (void)mdata;
}

bool CdDecoderFactory::supports(const QString &source) const
{
    return (source.right(extension().length()).lower() == extension());
}

const QString &CdDecoderFactory::extension() const
{
    static QString ext(".cda");
    return ext;
}


const QString &CdDecoderFactory::description() const
{
    static QString desc(QObject::tr("Windows CD parser"));
    return desc;
}

Decoder *CdDecoderFactory::create(const QString &file, QIODevice *input, 
                                  AudioOutput *output, bool deletable)
{
    if (deletable)
        return new CdDecoder(file, this, input, output);

    static CdDecoder *decoder = 0;
    if (! decoder) {
        decoder = new CdDecoder(file, this, input, output);
    } else {
        decoder->setInput(input);
        decoder->setFilename(file);
        decoder->setOutput(output);
    }

    return decoder;
}
