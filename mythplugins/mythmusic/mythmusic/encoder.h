#ifndef ENCODER_H_
#define ENCODER_H_

#include <sys/types.h>

#define EENCODEERROR -1
#define EPARTIALSAMPLE -2
#define ENOTIMPL -3

class Metadata;

class Encoder 
{
  public:
    Encoder(const QString &outfile, int qualitylevel, Metadata *metadata);
    virtual ~Encoder();
    virtual int addSamples(int16_t * bytes, unsigned int len) = 0;

  protected:
    const QString *outfile;
    FILE *out;
    int quality;
    Metadata *metadata;
};

#endif
