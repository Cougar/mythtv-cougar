#include <iostream>

#include "metadata.h"
#include "encoder.h"

#include <mythtv/mythcontext.h> 

using namespace std;

Encoder::Encoder(const QString &outfile, int qualitylevel, Metadata *metadata) 
{
    if (outfile) 
    {
        out = fopen(outfile.ascii(), "w");
        if (!out) 
            VERBOSE(VB_GENERAL, QString("Error opening output file: %1").arg(outfile));
    } 
    else 
        out = NULL;

    this->outfile = &outfile;
    this->quality = qualitylevel;
    this->metadata = metadata;
}

Encoder::~Encoder()
{
    if(out) 
       fclose(out);
}

