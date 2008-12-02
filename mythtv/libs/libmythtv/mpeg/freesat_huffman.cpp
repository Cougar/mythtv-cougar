#include "freesat_huffman.h"

#include <qstring.h>

struct fsattab {
    unsigned int value;
    short bits;
    char next;
};

#define START   '\0'
#define STOP    '\0'
#define ESCAPE  '\1'

#include "freesat_tables.h"

QString freesat_huffman_to_string(const unsigned char *src, uint size)
{
    if (src[1] == 1 || src[1] == 2)
    {
        QByteArray uncompressed(size * 3, '\0');
        int p = 0;
        unsigned value = 0, byte = 2, bit = 0;
        while (byte < 6 && byte < size)
        {
            value |= src[byte] << ((5-byte) * 8);
            byte++;
        }
        char lastch = START;

        do
        {
            bool found = false;
            unsigned bitShift = 0;
            char nextCh = STOP;
            if (lastch == ESCAPE)
            {
                found = true;
                // Encoded in the next 8 bits.
                // Terminated by the first ASCII character.
                nextCh = (value >> 24) & 0xff;
                bitShift = 8;
                if ((nextCh & 0x80) == 0)
                {
                    if (nextCh < ' ')
                        nextCh = STOP;
                    lastch = nextCh;
                }
            }
            else
            {
                unsigned indx = (unsigned)lastch;
                if (src[1] == 2)
                    indx |= 0x80;
                for (unsigned j = fsat_index[indx]; j < fsat_index[indx+1]; j++)
                {
                    unsigned mask = 0, maskbit = 0x80000000;
                    for (short kk = 0; kk < fsat_table[j].bits; kk++)
                    {
                        mask |= maskbit;
                        maskbit >>= 1;
                    }
                    if ((value & mask) == fsat_table[j].value)
                    {
                        nextCh = fsat_table[j].next;
                        bitShift = fsat_table[j].bits;
                        found = true;
                        lastch = nextCh;
                        break;
                    }
                }
            }
            if (found)
            {
                if (nextCh != STOP && nextCh != ESCAPE)
                {
                    if (p >= uncompressed.count())
                        uncompressed.resize(p+10);
                    uncompressed[p++] = nextCh;
                }
                // Shift up by the number of bits.
                for (unsigned b = 0; b < bitShift; b++)
                {
                    value = (value << 1) & 0xfffffffe;
                    if (byte < size)
                        value |= (src[byte] >> (7-bit)) & 1;
                    if (bit == 7)
                    {
                        bit = 0;
                        byte++;
                    }
                    else bit++;
                }
            }
            else
            {
                // Entry missing in table.
                QString result = QString::fromUtf8(uncompressed, p);
                result.append("...");
                return result;
            }
        } while (lastch != STOP && byte < size+4);

        return QString::fromUtf8(uncompressed, p);
    }
    else return QString("");
}
