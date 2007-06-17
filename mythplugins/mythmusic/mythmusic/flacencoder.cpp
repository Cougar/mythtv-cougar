#include <qstring.h>

#include <iostream>
#include <unistd.h>
#include <stdlib.h>
using namespace std;

#include "metadata.h"
#include "flacencoder.h"
#include "metaioflacvorbiscomment.h"

#include <FLAC/export.h>
#if !defined(NEWFLAC)
  /* FLAC 1.0.4 to 1.1.2 */
  #include <FLAC/file_encoder.h>
#else
  /* FLAC 1.1.3 and up */
  #include <FLAC/stream_encoder.h>
#endif

#include <FLAC/assert.h>
#include <mythtv/mythcontext.h>

FlacEncoder::FlacEncoder(const QString &outfile, int qualitylevel,
                         Metadata *metadata)
           : Encoder(outfile, qualitylevel, metadata)
{
    sampleindex = 0;

    bool streamable_subset = true;
    bool do_mid_side = true;
    bool loose_mid_side = false;
    int bits_per_sample = 16;
    int sample_rate = 44100;
    int blocksize = 4608;
    int max_lpc_order = 8;
    int qlp_coeff_precision = 0;
    bool qlp_coeff_prec_search = false;
    bool do_escape_coding = false;
    bool do_exhaustive_model_search = false;
    int min_residual_partition_order = 3;
    int max_residual_partition_order = 3;
    int rice_parameter_search_dist = 0;

    encoder = encoder_new();
    encoder_setup(encoder, streamable_subset,
                  do_mid_side, loose_mid_side,
                  NUM_CHANNELS, bits_per_sample,
                  sample_rate, blocksize,
                  max_lpc_order, qlp_coeff_precision,
                  qlp_coeff_prec_search, do_escape_coding,
                  do_exhaustive_model_search,
                  min_residual_partition_order,
                  max_residual_partition_order,
                  rice_parameter_search_dist);

#if !defined(NEWFLAC)
    /* FLAC 1.0.4 to 1.1.2 */
    FLAC__file_encoder_set_filename(encoder, outfile.local8Bit());

    int ret = FLAC__file_encoder_init(encoder);
    if (ret != FLAC__FILE_ENCODER_OK)
#else
    /* FLAC 1.1.3 and up */
    int ret = FLAC__stream_encoder_init_file(encoder,
                           outfile.local8Bit(),
                           NULL,
                           NULL);
    if(ret != FLAC__STREAM_ENCODER_INIT_STATUS_OK)
#endif
    {
        VERBOSE(VB_GENERAL, QString("Error initializing FLAC encoder."
                                    " Got return code: %1").arg(ret));
    }

    for (int i = 0; i < NUM_CHANNELS; i++)
        input[i] = &(inputin[i][0]); 
}

FlacEncoder::~FlacEncoder()
{
    addSamples(0, 0); // flush buffer

    if (encoder)
    {
        encoder_finish(encoder);
        encoder_delete(encoder);
    }

    if (m_metadata)
    {
        QString filename = m_metadata->Filename();
        m_metadata->setFilename(m_outfile);
        MetaIOFLACVorbisComment().write(m_metadata);
        m_metadata->setFilename(filename);
    }
}

int FlacEncoder::addSamples(int16_t *bytes, unsigned int length)
{
    unsigned int index = 0;

    length /= sizeof(int16_t);

    do {
        while (index < length && sampleindex < MAX_SAMPLES) 
        {
            input[0][sampleindex] = (FLAC__int32)(bytes[index++]);
            input[1][sampleindex] = (FLAC__int32)(bytes[index++]);
            sampleindex += 1;
        }

        if(sampleindex == MAX_SAMPLES || (length == 0 && sampleindex > 0) ) 
        {
            if (!encoder_process(encoder, (const FLAC__int32 * const *) input,
                                 sampleindex))
            {
                VERBOSE(VB_GENERAL, QString("Failed to write flac data."
                                            " Aborting."));
                return EENCODEERROR;
            }
            sampleindex = 0;
        }
    } while (index < length);

    return 0;
}

