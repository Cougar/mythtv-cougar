/* range / gamma adjustment filter
*/

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

#include "filter.h"
#include "frame.h"

#ifdef i386

#include "mmx.h"

static const mmx_t mm_cpool[] = {
    { w: {1, 1, 1, 1} },
    { ub: {36, 36, 36, 36, 36, 36, 36, 36} },
    { ub: {20, 20, 20, 20, 20, 20, 20, 20} },
    { ub: {31, 31, 31, 31, 31, 31, 31, 31} },
    { ub: {15, 15, 15, 15, 15, 15, 15, 15} }
};
    
#define MM_MMX    0x0001        /* standard MMX */
#define MM_3DNOW  0x0004        /* AMD 3DNOW */
#define MM_MMXEXT 0x0002        /* SSE integer functions or AMD MMX ext */
#define MM_SSE    0x0008        /* SSE functions */
#define MM_SSE2   0x0010        /* PIV SSE2 functions */

#define cpuid(index,eax,ebx,ecx,edx)\
    __asm __volatile\
        ("movl %%ebx, %%esi\n\t"\
         "cpuid\n\t"\
         "xchgl %%ebx, %%esi"\
         : "=a" (eax), "=S" (ebx),\
           "=c" (ecx), "=d" (edx)\
         : "0" (index));

/* Function to test if multimedia instructions are supported...  */
int
mm_support (void)
{
    int rval;
    int eax, ebx, ecx, edx;

    __asm__ __volatile__ (
                             /* See if CPUID instruction is supported ... */
                             /* ... Get copies of EFLAGS into eax and ecx */
                             "pushf\n\t" "popl %0\n\t" "movl %0, %1\n\t"
                             /* ... Toggle the ID bit in one copy and store */
                             /*     to the EFLAGS reg */
                             "xorl $0x200000, %0\n\t" "push %0\n\t" "popf\n\t"
                             /* ... Get the (hopefully modified) EFLAGS */
                             "pushf\n\t"
                             "popl %0\n\t":"=a" (eax), "=c" (ecx)::"cc");

    if (eax == ecx)
        return 0;               /* CPUID not supported */

    cpuid (0, eax, ebx, ecx, edx);

    if (ebx == 0x756e6547 && edx == 0x49656e69 && ecx == 0x6c65746e)
    {

        /* intel */
      inteltest:
        cpuid (1, eax, ebx, ecx, edx);
        if ((edx & 0x00800000) == 0)
            return 0;
        rval = MM_MMX;
        if (edx & 0x02000000)
            rval |= MM_MMXEXT | MM_SSE;
        if (edx & 0x04000000)
            rval |= MM_SSE2;
        return rval;
    }
    else if (ebx == 0x68747541 && edx == 0x69746e65 && ecx == 0x444d4163)
    {
        /* AMD */
        cpuid (0x80000000, eax, ebx, ecx, edx);
        if ((unsigned) eax < 0x80000001)
            goto inteltest;
        cpuid (0x80000001, eax, ebx, ecx, edx);
        if ((edx & 0x00800000) == 0)
            return 0;
        rval = MM_MMX;
        if (edx & 0x80000000)
            rval |= MM_3DNOW;
        if (edx & 0x00400000)
            rval |= MM_MMXEXT;
        return rval;
    }
    else if (ebx == 0x746e6543 && edx == 0x48727561 && ecx == 0x736c7561)
    {                           /*  "CentaurHauls" */
        /* VIA C3 */
        cpuid (0x80000000, eax, ebx, ecx, edx);
        if ((unsigned) eax < 0x80000001)
            goto inteltest;
        cpuid (0x80000001, eax, ebx, ecx, edx);
        rval = 0;
        if (edx & (1 << 31))
            rval |= MM_3DNOW;
        if (edx & (1 << 23))
            rval |= MM_MMX;
        if (edx & (1 << 24))
            rval |= MM_MMXEXT;
        return rval;
    }
    else if (ebx == 0x69727943 && edx == 0x736e4978 && ecx == 0x64616574)
    {
        /* Cyrix Section */
        /* See if extended CPUID level 80000001 is supported */
        /* The value of CPUID/80000001 for the 6x86MX is undefined
           according to the Cyrix CPU Detection Guide (Preliminary
           Rev. 1.01 table 1), so we'll check the value of eax for
           CPUID/0 to see if standard CPUID level 2 is supported.
           According to the table, the only CPU which supports level
           2 is also the only one which supports extended CPUID levels.
         */
        if (eax != 2)
            goto inteltest;
        cpuid (0x80000001, eax, ebx, ecx, edx);
        if ((eax & 0x00800000) == 0)
            return 0;
        rval = MM_MMX;
        if (eax & 0x01000000)
            rval |= MM_MMXEXT;
        return rval;
    }
    else
    {
        return 0;
    }
}
#else
int mm_support(void) { return 0; }
#endif // i386

typedef struct ThisFilter
{
    VideoFilter vf;

    int yend;
    int cend;

#ifdef i386
    int yfilt;
    int cfilt;

    mmx_t yscale;
    mmx_t yshift;
    mmx_t ymin;

    mmx_t cscale;
    mmx_t cshift;
    mmx_t cmin;
#endif /* i386 */

    uint8_t ytable[256];
    uint8_t ctable[256];

    TF_STRUCT;
} ThisFilter;

void adjustRegion(uint8_t *buf, uint8_t *end, const uint8_t *table)
{
    while (buf < end)
    {
        *buf = table[*buf];
        buf++;
    }
}

void adjustRegionMMX(uint8_t *buf, uint8_t *end, const uint8_t *table,
                     const mmx_t *shift, const mmx_t *scale, const mmx_t *min,
                     const mmx_t *clamp1, const mmx_t *clamp2)
{
    movq_m2r (*scale, mm6);
    movq_m2r (*min, mm7);
    while (buf < end - 15)
    {
        movq_m2r (buf[0], mm0);
        movq_m2r (buf[8], mm2);
        movq_m2r (*shift, mm4);
        pxor_r2r (mm5, mm5);

        psubusb_r2r (mm7, mm0);
        psubusb_r2r (mm7, mm2);

        movq_r2r (mm0, mm1);
        movq_r2r (mm2, mm3);

        punpcklbw_r2r (mm5, mm0);
        punpckhbw_r2r (mm5, mm1);
        punpcklbw_r2r (mm5, mm2);
        punpckhbw_r2r (mm5, mm3);

        movq_m2r (mm_cpool[0], mm5);

        psllw_r2r (mm4, mm0);
        psllw_r2r (mm4, mm1);
        psllw_r2r (mm4, mm2);
        psllw_r2r (mm4, mm3);

        movq_m2r (*clamp1, mm4);

        pmulhw_r2r (mm6, mm0);
        pmulhw_r2r (mm6, mm1);
        pmulhw_r2r (mm6, mm2);
        pmulhw_r2r (mm6, mm3);

        paddw_r2r (mm5, mm0);
        paddw_r2r (mm5, mm1);
        paddw_r2r (mm5, mm2);
        paddw_r2r (mm5, mm3);

        movq_m2r (*clamp2, mm5);

        psrlw_i2r (1, mm0);
        psrlw_i2r (1, mm1);
        psrlw_i2r (1, mm2);
        psrlw_i2r (1, mm3);

        packuswb_r2r (mm1, mm0);
        packuswb_r2r (mm3, mm2);

        paddusb_r2r (mm4, mm0);
        paddusb_r2r (mm4, mm2);

        psubusb_r2r (mm5, mm0);
        psubusb_r2r (mm5, mm2);

        movq_r2m (mm0, buf[0]);
        movq_r2m (mm2, buf[8]);

        buf += 16;
    }
    while (buf < end)
    {
        *buf = table[*buf];
        buf++;
    }
}

int adjustFilter (VideoFilter *vf, VideoFrame *frame)
{
    ThisFilter *filter = (ThisFilter *) vf;
    TF_VARS;

    TF_START;

#ifdef i386
    if (filter->yfilt)
        adjustRegionMMX(frame->buf, frame->buf + filter->yend, filter->ytable,
                        &(filter->yshift), &(filter->yscale), &(filter->ymin),
                        mm_cpool + 1, mm_cpool + 2);
    else
        adjustRegion(frame->buf, frame->buf + filter->yend, filter->ytable);
    if (filter->cfilt)
        adjustRegionMMX(frame->buf + filter->yend, frame->buf + filter->cend,
                        filter->ctable, &(filter->cshift), &(filter->cscale),
                        &(filter->cmin), mm_cpool + 3, mm_cpool + 4);
    else
        adjustRegion(frame->buf + filter->yend, frame->buf + filter->cend,
                     filter->ytable);
    if (filter->yfilt || filter->cfilt)
        emms();
#else /* i386 */
    adjustRegion(frame->buf, frame->buf + filter->yend, filter->ytable);
    adjustRegion(frame->buf + filter->yend, frame->buf + filter->cend,
                 filter->ytable);
#endif /* i386 */
    TF_END(filter, "Adjust: ");
    return 0;
}

void fillTable(uint8_t *table, int in_min, int in_max, int out_min,
                int out_max, float gamma)
{
    int i;
    float f;

    for (i = 0; i < 256; i++)
    {
        f = ((float)i - in_min) / (in_max - in_min);
        f = f < 0.0 ? 0.0 : f;
        f = f > 1.0 ? 1.0 : f;
        table[i] = (pow (f, gamma) * (out_max - out_min) + out_min + 0.5);
    }
}

#ifdef i386
int fillTableMMX(uint8_t *table, mmx_t *shift, mmx_t *scale, mmx_t *min,
                   int in_min, int in_max, int out_min, int out_max,
                   float gamma)
{
    int shiftc, scalec, i;

    fillTable(table, in_min, in_max, out_min, out_max, gamma);
    scalec = ((out_max - out_min) << 15)/(in_max - in_min);
    if ((mm_support() & MM_MMX) == 0 || gamma < 0.9999 || gamma > 1.00001
        || scalec > 32767 << 7)
        return 0;
    shiftc = 2;
    while (scalec > 32767)
    {
        shiftc++;
        scalec >>= 1;
    }
    if (shiftc > 7)
        return 0;
    for (i = 0; i < 4; i++)
    {
        scale->w[i] = scalec;
    }
    for (i = 0; i < 8; i++)
        min->b[i] = in_min;
    shift->q = shiftc;
    return 1;
}
#endif /* i386 */

VideoFilter *
newAdjustFilter (VideoFrameType inpixfmt, VideoFrameType outpixfmt, 
                    int *width, int *height, char *options)
{
    ThisFilter *filter;
    int numopts, ymin, ymax, cmin, cmax;
    float ygamma, cgamma;

    if (inpixfmt != outpixfmt ||
        (inpixfmt != FMT_YV12 && inpixfmt != FMT_YUV422P))
    {
        fprintf(stderr, "adjust: only YV12->YV12 and YUV422P->YUV422P"
                " conversions are supported\n");
        return NULL;
    }

    numopts = 0;
    if (options)
        numopts = sscanf(options, "%d:%d:%f:%d:%d:%f", &ymin, &ymax, &ygamma,
                         &cmin, &cmax, &cgamma);

    if (numopts != 6 && (numopts !=1 && ymin != -1))
    {
        ymin = 16;
        ymax = 253;
        ygamma = 1.0;
        cmin = 16;
        cmax = 240;
        cgamma = 1.0;
    }

    filter = malloc (sizeof (ThisFilter));

    if (filter == NULL)
    {
        fprintf (stderr, "adjust: failed to allocate memory for filter\n");
        return NULL;
    }

    if (ymin == -1)
    {
        filter->vf.filter = NULL;
        filter->vf.cleanup = NULL;
        return (VideoFilter *) filter;
    }

#ifdef i386
    filter->yfilt = fillTableMMX (filter->ytable, &(filter->yshift),
                                    &(filter->yscale), &(filter->ymin),
                                    ymin, ymax, 16, 235, ygamma);
    filter->yfilt = fillTableMMX (filter->ctable, &(filter->cshift),
                                    &(filter->cscale), &(filter->cmin),
                                    cmin, cmax, 16, 240, cgamma);
#else /* i386 */
    fillTable (filter->ytable, ymin, ymax, 16, 235, ygamma);
    fillTable (filter->ctable, cmin, cmax, 16, 240, cgamma);
#endif /* i386 */

    filter->yend = *width * *height;
    
    switch (inpixfmt)
    {
        case FMT_YV12:
            filter->cend = filter->yend + *width * *height / 2;
            break;
        case FMT_YUV422P:
            filter->cend = filter->yend + *width * *height;
            break;
        default:
            break;
    }

    filter->vf.filter = &adjustFilter;
    filter->vf.cleanup = NULL;
    
    TF_INIT(filter);
    return (VideoFilter *) filter;
}

static FmtConv FmtList[] = 
{
    { FMT_YV12, FMT_YV12 },
    { FMT_YUV422P, FMT_YUV422P },
    FMT_NULL
};

FilterInfo filter_table[] = 
{
    {
        symbol:     "newAdjustFilter",
        name:       "adjust",
        descript:   "adjust range and gamma of video",
        formats:    FmtList,
        libname:    NULL
    },
    FILT_NULL
};
