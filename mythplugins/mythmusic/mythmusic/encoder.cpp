#include <iostream>

#include "metadata.h"
#include "encoder.h"

#include <mythtv/mythcontext.h>

using namespace std;

Encoder::Encoder(const QString &outfile, int qualitylevel, Metadata *metadata)
    : m_outfile(outfile), m_out(NULL), m_quality(qualitylevel),
      m_metadata(metadata)
{
    if (m_outfile)
    {
        m_out = fopen(m_outfile.local8Bit(), "w+");
        if (!m_out)
            VERBOSE(VB_GENERAL, QString("Error opening output file: %1")
                    .arg(m_outfile.local8Bit()));
    }
}

Encoder::~Encoder()
{
    if (m_out)
        fclose(m_out);
}
