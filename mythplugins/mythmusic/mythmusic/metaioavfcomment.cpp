#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <sys/stat.h>
using namespace std;

#include "metaioavfcomment.h"
#include "metadata.h"

#include <mythtv/ffmpeg/avformat.h>
#include <mythtv/ffmpeg/avcodec.h>

//==========================================================================
MetaIOAVFComment::MetaIOAVFComment(void)
    : MetaIO(".wma")
{
    av_register_all();
}

//==========================================================================
MetaIOAVFComment::~MetaIOAVFComment(void)
{
}


//==========================================================================
/*!
 * \brief Writes metadata back to a file
 *
 * \param mdata A pointer to a Metadata object
 * \param exclusive Flag to indicate if only the data in mdata should be
 *                  in the file. If false, any unrecognised tags already
 *                  in the file will be maintained.
 * \returns Boolean to indicate success/failure.
 */
bool MetaIOAVFComment::write(Metadata* mdata, bool exclusive)
{
    // Wont implement....
    mdata = mdata; // -Wall annoyance
    exclusive = exclusive; // -Wall annoyance
    return false;
}


//==========================================================================
/*!
 * \brief Reads Metadata from a file.
 *
 * \param filename The filename to read metadata from.
 * \returns Metadata pointer or NULL on error
 */
Metadata* MetaIOAVFComment::read(QString filename)
{
    QString artist = "", album = "", title = "", genre = "";
    int year = 0, tracknum = 0, length = 0;
    
    AVFormatContext* p_context = NULL;
    AVFormatParameters* p_params = NULL;
    AVInputFormat* p_inputformat = NULL;

    if (av_open_input_file(&p_context, filename.local8Bit(), 
                           p_inputformat, 0, p_params) < 0
        && av_open_input_file(&p_context, filename.ascii(), 
                           p_inputformat, 0, p_params) < 0)
        return NULL;
        
    if (av_find_stream_info(p_context) < 0)
        return NULL;

    
    title += (char *)p_context->title;
    if (title.isEmpty())
    {
        readFromFilename(filename, artist, album, title, genre, tracknum);
    }
    else
    {
        artist += (char *)p_context->author;
        album += (char *)p_context->album;
        genre += (char *)p_context->genre;
        year = p_context->year;
        tracknum = p_context->track;
    }
    
    length = getTrackLength(p_context);

    Metadata *retdata = new Metadata(filename, artist, album, title, genre,
                                     year, tracknum, length);

    av_close_input_file(p_context);
    
    return retdata;
}


//==========================================================================
/*!
 * \brief Find the length of the track (in seconds)
 *
 * \param filename The filename for which we want to find the length.
 * \returns An integer (signed!) to represent the length in seconds.
 */
int MetaIOAVFComment::getTrackLength(QString filename)
{
    AVFormatContext* p_context = NULL;
    AVFormatParameters* p_params = NULL;
    AVInputFormat* p_inputformat = NULL;
    
    // Open the specified file and populate the metadata info
    if (av_open_input_file(&p_context, filename.local8Bit(), 
                           p_inputformat, 0, p_params) < 0
        && av_open_input_file(&p_context, filename.ascii(), 
                           p_inputformat, 0, p_params) < 0)
        return 0;
        
    if (av_find_stream_info(p_context) < 0)
        return 0;

    int rv = getTrackLength(p_context);
    
    av_close_input_file(p_context);
    
    return rv;
}


//==========================================================================
/*!
 * \brief Find the length of the track (in seconds)
 *
 * \param p_context The AV Format Context.
 * \returns An integer (signed!) to represent the length in seconds.
 */
int MetaIOAVFComment::getTrackLength(AVFormatContext* pContext)
{
    if (!pContext)
        return 0;
        
    av_estimate_timings(pContext);
    
    return (pContext->duration / AV_TIME_BASE) * 1000;
}
