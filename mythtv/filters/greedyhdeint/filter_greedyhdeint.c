/* Rewrite of neuron2's GreedyHDeint filter for Avisynth 
 *
 * converted for myth by Markus Schulz <msc@antzsystem.de>
 * */

#include <stdlib.h>
#include <stdio.h>

#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif

#include <string.h>
#include <math.h>

#include "filter.h"
#include "frame.h"

#include "config.h"
#include "../mm_arch.h"

#include "color.h"

#if defined (ARCH_X86)

#include "greedyhmacros.h"

#define MAXCOMB_DEFAULT          5
#define MOTIONTHRESHOLD_DEFAULT 25
#define MOTIONSENSE_DEFAULT     30

static unsigned int GreedyMaxComb = MAXCOMB_DEFAULT;
static unsigned int GreedyMotionThreshold = MOTIONTHRESHOLD_DEFAULT;
static unsigned int GreedyMotionSense = MOTIONSENSE_DEFAULT;

#define IS_MMX
#define SSE_TYPE MMXT
#define FUNCT_NAME greedyh_filter_mmx
#include "greedyh.asm"
#undef SSE_TYPE
#undef IS_MMX
#undef FUNCT_NAME

#define IS_SSE
#define SSE_TYPE SSE
#define FUNCT_NAME greedyh_filter_sse
#include "greedyh.asm"
#undef SSE_TYPE
#undef IS_SSE
#undef FUNCT_NAME

#define IS_3DNOW
#define FUNCT_NAME greedyh_filter_3dnow
#define SSE_TYPE 3DNOW
#include "greedyh.asm"
#undef SSE_TYPE
#undef IS_3DNOW
#undef FUNCT_NAME


#endif



#ifdef MMX
#include "i386/mmx.h"

static const mmx_t mm_cpool[] =
{
    { 0x0000000000000000LL },
};

#else
#define mmx_t int
#endif

typedef struct ThisFilter
{
    VideoFilter vf;

    long long frames_nr[2];
    int8_t got_frames[2];
    unsigned char* frames[2];
    unsigned char* deint_frame;
    long long last_framenr;

    int width;
    int height;

    int mm_flags;
    TF_STRUCT;
} ThisFilter;

static void AllocFilter(ThisFilter* filter, int width, int height)
{
    if ((width != filter->width) || height != filter->height)
    {
        printf("greedyhdeint: size changed from %d x %d -> %d x %d\n", filter->width, filter->height, width, height);
        if (filter->frames[0]) 
        {
            free(filter->frames[0]);
            free(filter->frames[1]);
            free(filter->deint_frame);
        }
        filter->frames[0] = malloc(width * height * 2);
        filter->frames[1] = malloc(width * height * 2);
        memset(filter->frames[0], 0, width * height * 2);
        memset(filter->frames[1], 0, width * height * 2);
        filter->deint_frame = malloc(width * height * 2);
        filter->width = width;
        filter->height = height;
        memset(filter->got_frames, 0, sizeof(filter->got_frames));
        memset(filter->frames_nr, 0, sizeof(filter->frames_nr));
    }
}


#include <sys/time.h>
#include <time.h>

/*
   XINE Algorithm
   int top_field_first = frame->top_field_first;

   if ( (frame->flags & VO_BOTH_FIELDS) != VO_BOTH_FIELDS ) {
   top_field_first = (frame->flags & VO_TOP_FIELD) ? 1 : 0;
   }

   if ( top_field_first ) {
   fields[0] = 0;
   fields[1] = 1;
   } else {
   fields[0] = 1;
   fields[1] = 0;
   }

   skip = deinterlace_build_output_field( 
   this, port, stream,
   frame, yuy2_frame,
   fields[0], 0,   //int bottom_field, int second_field,
   frame->pts,
   (framerate_mode == FRAMERATE_FULL) ? frame->duration/2 : frame->duration,
   0);

   skip = deinterlace_build_output_field( 
   this, port, stream,
   frame, yuy2_frame,
   fields[1], 1,   //int bottom_field, int second_field,
   0,
   frame->duration/2,
   skip);          

   move-recent-frames-ahead, last full frame only needed for greedyH
 * */


static int GreedyHDeint (VideoFilter * f, VideoFrame * frame)
{
    ThisFilter *filter = (ThisFilter *) f;
    TF_VARS;

    int last_frame = 0;
    int cur_frame = 0;
    int second_field = 0;
    int bottom_field = 0;

    AllocFilter((ThisFilter*)f, frame->width, frame->height);

    if (filter->last_framenr != frame->frameNumber)
    {
        //this is no double call, really a new frame
        cur_frame = (filter->last_framenr + 1) & 1;
        last_frame = (filter->last_framenr) & 1;
        //check if really the previous frame (cause mythtv AutoDeInt behauviour)
        if (filter->last_framenr != (frame->frameNumber - 1))
        {
            cur_frame = frame->frameNumber & 1;
            last_frame = cur_frame;
        }
        second_field = 0;
        bottom_field = frame->top_field_first? 0 : 1;
    }
    else
    {
        //double call
        cur_frame = (filter->last_framenr) & 1;
        last_frame = (filter->last_framenr + 1) & 1;
        second_field = 1;
        bottom_field = frame->top_field_first? 1 : 0;
    }
    filter->got_frames[cur_frame] = 1;
    filter->frames_nr[cur_frame] = frame->frameNumber;

#if 0
    struct timeval l_stTV;
    char l_achBuf[256];
    char lbuf[13];
    gettimeofday(&l_stTV, NULL);
    strftime(&l_achBuf[0], 255, "%Y-%m-%d %H:%M:%S", localtime(&l_stTV.tv_sec));
    snprintf(lbuf, 13, ".%ld", l_stTV.tv_usec);
    strcat(l_achBuf, lbuf);

    printf("GreedyHDeint call: %s\n", l_achBuf);
#endif 

    switch(frame->codec)
    {
        case FMT_YV12: //must convert from yv12 planar to yuv422 packed
            /*printf(" CF=%d, LF=%d, FNr=%lld T=%lld IL=%d TF=%d RP=%d -> ", cur_frame, last_frame, frame->frameNumber, frame->timecode, frame->interlaced_frame, frame->top_field_first, frame->repeat_pict);*/
            /*printf("SAVE Nr %lld to %d\n", frame->frameNumber, cur_frame);*/

            //only needed for first call, on second we already have this frame
            if (second_field == 0)
            {
                yv12_to_yuy2(
                        frame->buf + frame->offsets[0], frame->pitches[0],
                        frame->buf + frame->offsets[1], frame->pitches[1],
                        frame->buf + frame->offsets[2], frame->pitches[2],
                        filter->frames[cur_frame], 2 * frame->width,
                        frame->width, frame->height,
                        1 - frame->interlaced_frame);
            }
            break;
        default:
            fprintf(stderr, "Unsupported pixel format.\n");
            return 0;
    }
    //must be done for first frame or deinterlacing would use an "empty" memory block/frame
    if (!filter->got_frames[last_frame])
        last_frame = cur_frame;

#ifdef MMX
    /* SSE Version has best quality. 3DNOW and MMX a litte bit impure */
    if (filter->mm_flags & MM_SSE) 
    {
        greedyh_filter_sse(
            filter->deint_frame, 2 * frame->width,
            filter->frames[cur_frame], filter->frames[last_frame],
            bottom_field, second_field, frame->width, frame->height);
    }
    else if (filter->mm_flags & MM_3DNOW)
    {
        greedyh_filter_3dnow(
            filter->deint_frame, 2 * frame->width,
            filter->frames[cur_frame], filter->frames[last_frame],
            bottom_field, second_field, frame->width, frame->height);
    }
    else if (filter->mm_flags & MM_MMX) 
    {
        greedyh_filter_mmx(
            filter->deint_frame, 2 * frame->width,
            filter->frames[cur_frame], filter->frames[last_frame],
            bottom_field, second_field, frame->width, frame->height);
    }
    else
#endif
    {
        /* TODO plain old C implementation */
    }

#if 0
      apply_chroma_filter(filter->deint_frame, frame->width * 2,
                          frame->width, frame->height );
#endif

    /* convert back to yv12, cause myth only works with this format */
    yuy2_to_yv12(
        filter->deint_frame, 2 * frame->width,
        frame->buf + frame->offsets[0], frame->pitches[0],
        frame->buf + frame->offsets[1], frame->pitches[1],
        frame->buf + frame->offsets[2], frame->pitches[2],
        frame->width, frame->height);

    filter->last_framenr = frame->frameNumber;

    return 0;
}


void CleanupGreedyHDeintFilter (VideoFilter * filter)
{
    ThisFilter* f = (ThisFilter*)filter;
    free(f->deint_frame);
    free(f->frames[0]);
    free(f->frames[1]);
}

VideoFilter* GreedyHDeintFilter (VideoFrameType inpixfmt, VideoFrameType outpixfmt,
        int *width, int *height, char *options)
{
    ThisFilter *filter;
    (void) height;
    (void) options;

    filter = (ThisFilter *) malloc (sizeof(ThisFilter));
    if (filter == NULL)
    {
        fprintf (stderr, "GreedyHDeint: failed to allocate memory for filter.\n");
        return NULL;
    }

    filter->width = 0;
    filter->height = 0;
    memset(filter->frames, 0, sizeof(filter->frames));
    filter->deint_frame = 0;

    AllocFilter(filter, *width, *height);

    init_yuv_conversion();
#ifdef MMX
    filter->mm_flags = mm_support();
    TF_INIT(filter);
#else
    filter->mm_flags = 0;
#endif

    filter->vf.filter = &GreedyHDeint;
    filter->vf.cleanup = &CleanupGreedyHDeintFilter;
    return (VideoFilter *) filter;
}


static FmtConv FmtList[] =
{
    { FMT_YV12, FMT_YV12 } ,
    FMT_NULL
};

ConstFilterInfo filter_table[] =
{
    {
symbol:     "GreedyHDeintFilter",
            name:       "greedyhdeint",
            descript:   "combines data from several fields to deinterlace with less motion blur",
            formats:    FmtList,
            libname:    NULL
    },
    {
symbol:     "GreedyHDeintFilter",
            name:       "greedyhdoubleprocessdeint",
            descript:   "combines data from several fields to deinterlace with less motion blur",
            formats:    FmtList,
            libname:    NULL
    },FILT_NULL
};

/* vim: set expandtab tabstop=4 shiftwidth=4: */
