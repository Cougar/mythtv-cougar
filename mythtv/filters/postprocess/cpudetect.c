#include "cpudetect.h"

CpuCaps gCpuCaps;

#include <malloc.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>

static void check_os_katmai_support( void );

// return TRUE if cpuid supported
static int has_cpuid()
{
	int a, c;

// code from libavcodec:
    __asm__ __volatile__ (
                          /* See if CPUID instruction is supported ... */
                          /* ... Get copies of EFLAGS into eax and ecx */
                          "pushf\n\t"
                          "popl %0\n\t"
                          "movl %0, %1\n\t"
                          
                          /* ... Toggle the ID bit in one copy and store */
                          /*     to the EFLAGS reg */
                          "xorl $0x200000, %0\n\t"
                          "push %0\n\t"
                          "popf\n\t"
                          
                          /* ... Get the (hopefully modified) EFLAGS */
                          "pushf\n\t"
                          "popl %0\n\t"
                          : "=a" (a), "=c" (c)
                          :
                          : "cc" 
                          );

	return (a!=c);
}

static void
do_cpuid(unsigned int ax, unsigned int *p)
{
#if 0
	__asm __volatile(
	"cpuid;"
	: "=a" (p[0]), "=b" (p[1]), "=c" (p[2]), "=d" (p[3])
	:  "0" (ax)
	);
#else
// code from libavcodec:
    __asm __volatile
	("movl %%ebx, %%esi\n\t"
         "cpuid\n\t"
         "xchgl %%ebx, %%esi"
         : "=a" (p[0]), "=S" (p[1]), 
           "=c" (p[2]), "=d" (p[3])
         : "0" (ax));
#endif

}

void GetCpuCaps( CpuCaps *caps)
{
	unsigned int regs[4];
	unsigned int regs2[4];

	caps->isX86=1;

	memset(caps, 0, sizeof(*caps));
	if (!has_cpuid()) {
	    //printf("CPUID not supported!??? (maybe an old 486?)\n");
	    return;
	}
	do_cpuid(0x00000000, regs); // get _max_ cpuid level and vendor name
	//printf("CPU vendor name: %.4s%.4s%.4s  max cpuid level: %d\n",
	//       (char*) (regs+1),(char*) (regs+3),(char*) (regs+2), regs[0]);
	if (regs[0]>=0x00000001)
	{
		char *tmpstr;

		do_cpuid(0x00000001, regs2);

		tmpstr=GetCpuFriendlyName(regs, regs2);
		//printf("CPU: %s ",tmpstr);
		free(tmpstr);

		caps->cpuType=(regs2[0] >> 8)&0xf;
		if(caps->cpuType==0xf){
		    // use extended family (P4, IA64)
		    caps->cpuType=8+((regs2[0]>>20)&255);
		}
		caps->cpuStepping=regs2[0] & 0xf;
		//printf("(Family: %d, Stepping: %d)\n",
		//       caps->cpuType, caps->cpuStepping);

		// general feature flags:
		caps->hasMMX  = (regs2[3] & (1 << 23 )) >> 23; // 0x0800000
		caps->hasSSE  = (regs2[3] & (1 << 25 )) >> 25; // 0x2000000
		caps->hasSSE2 = (regs2[3] & (1 << 26 )) >> 26; // 0x4000000
		caps->hasMMX2 = caps->hasSSE; // SSE cpus supports mmxext too
	}
	do_cpuid(0x80000000, regs);
	if (regs[0]>=0x80000001) {
		//printf("extended cpuid-level: %d\n",regs[0]&0x7FFFFFFF);
		do_cpuid(0x80000001, regs2);
		caps->hasMMX  |= (regs2[3] & (1 << 23 )) >> 23; // 0x0800000
		caps->hasMMX2 |= (regs2[3] & (1 << 22 )) >> 22; // 0x400000
		caps->has3DNow    = (regs2[3] & (1 << 31 )) >> 31; //0x80000000
		caps->has3DNowExt = (regs2[3] & (1 << 30 )) >> 30;
	}
		/* FIXME: Does SSE2 need more OS support, too? */
#if defined(__linux__) || defined(__FreeBSD__)
		if (caps->hasSSE)
			check_os_katmai_support();
		if (!caps->hasSSE)
			caps->hasSSE2 = 0;
#else
		caps->hasSSE=0;
		caps->hasSSE2 = 0;
#endif
}


#define CPUID_EXTFAMILY	((regs2[0] >> 20)&0xFF) /* 27..20 */
#define CPUID_EXTMODEL	((regs2[0] >> 16)&0x0F) /* 19..16 */
#define CPUID_TYPE		((regs2[0] >> 12)&0x04) /* 13..12 */
#define CPUID_FAMILY	((regs2[0] >>  8)&0x0F) /* 11..08 */
#define CPUID_MODEL		((regs2[0] >>  4)&0x0F) /* 07..04 */
#define CPUID_STEPPING	((regs2[0] >>  0)&0x0F) /* 03..00 */

char *GetCpuFriendlyName(unsigned int regs[], unsigned int regs2[]){
#include "cputable.h" /* get cpuname and cpuvendors */
	char vendor[17];
	char *retname;
	int i;

	if (NULL==(retname=(char*)malloc(256))) {
		//printf("Error: GetCpuFriendlyName() not enough memory\n");
		exit(1);
	}

	sprintf(vendor,"%.4s%.4s%.4s",(char*)(regs+1),(char*)(regs+3),(char*)(regs+2));

	for(i=0; i<MAX_VENDORS; i++){
		if(!strcmp(cpuvendors[i].string,vendor)){
			if(cpuname[i][CPUID_FAMILY][CPUID_MODEL]){
				snprintf(retname,255,"%s %s",cpuvendors[i].name,cpuname[i][CPUID_FAMILY][CPUID_MODEL]);
			} else {
				snprintf(retname,255,"unknown %s %d. Generation CPU",cpuvendors[i].name,CPUID_FAMILY); 
				//printf("unknown %s CPU:\n",cpuvendors[i].name);
				//printf("Vendor:   %s\n",cpuvendors[i].string);
				//printf("Type:     %d\n",CPUID_TYPE);
				//printf("Family:   %d (ext: %d)\n",CPUID_FAMILY,CPUID_EXTFAMILY);
				//printf("Model:    %d (ext: %d)\n",CPUID_MODEL,CPUID_EXTMODEL);
				//printf("Stepping: %d\n",CPUID_STEPPING);
				//printf("Please send the above info along with the exact CPU name"
				//       "to the MPlayer-Developers, so we can add it to the list!\n");
			}
		}
	}

	//printf("Detected CPU: %s\n", retname);
	return retname;
}

#undef CPUID_EXTFAMILY
#undef CPUID_EXTMODEL
#undef CPUID_TYPE
#undef CPUID_FAMILY
#undef CPUID_MODEL
#undef CPUID_STEPPING


#if defined(__linux__) && defined(_POSIX_SOURCE) && defined(X86_FXSR_MAGIC)
static void sigill_handler_sse( int signal, struct sigcontext sc )
{
   //printf("SIGILL, " );

   /* Both the "xorps %%xmm0,%%xmm0" and "divps %xmm0,%%xmm1"
    * instructions are 3 bytes long.  We must increment the instruction
    * pointer manually to avoid repeated execution of the offending
    * instruction.
    *
    * If the SIGILL is caused by a divide-by-zero when unmasked
    * exceptions aren't supported, the SIMD FPU status and control
    * word will be restored at the end of the test, so we don't need
    * to worry about doing it here.  Besides, we may not be able to...
    */
   sc.eip += 3;

   gCpuCaps.hasSSE=0;
}

static void sigfpe_handler_sse( int signal, struct sigcontext sc )
{
   //printf("SIGFPE, " );

   if ( sc.fpstate->magic != 0xffff ) {
      /* Our signal context has the extended FPU state, so reset the
       * divide-by-zero exception mask and clear the divide-by-zero
       * exception bit.
       */
      sc.fpstate->mxcsr |= 0x00000200;
      sc.fpstate->mxcsr &= 0xfffffffb;
   } else {
      /* If we ever get here, we're completely hosed.
       */
      //printf("\n\n" );
      //printf("SSE enabling test failed badly!" );
   }
}
#endif /* __linux__ && _POSIX_SOURCE && X86_FXSR_MAGIC */

/* If we're running on a processor that can do SSE, let's see if we
 * are allowed to or not.  This will catch 2.4.0 or later kernels that
 * haven't been configured for a Pentium III but are running on one,
 * and RedHat patched 2.2 kernels that have broken exception handling
 * support for user space apps that do SSE.
 */
static void check_os_katmai_support( void )
{
#if defined(__FreeBSD__)
   int has_sse=0, ret;
   size_t len=sizeof(has_sse);

   ret = sysctlbyname("hw.instruction_sse", &has_sse, &len, NULL, 0);
   if (ret || !has_sse)
      gCpuCaps.hasSSE=0;

#elif defined(__linux__)
#if defined(_POSIX_SOURCE) && defined(X86_FXSR_MAGIC)
   struct sigaction saved_sigill;
   struct sigaction saved_sigfpe;

   /* Save the original signal handlers.
    */
   sigaction( SIGILL, NULL, &saved_sigill );
   sigaction( SIGFPE, NULL, &saved_sigfpe );

   signal( SIGILL, (void (*)(int))sigill_handler_sse );
   signal( SIGFPE, (void (*)(int))sigfpe_handler_sse );

   /* Emulate test for OSFXSR in CR4.  The OS will set this bit if it
    * supports the extended FPU save and restore required for SSE.  If
    * we execute an SSE instruction on a PIII and get a SIGILL, the OS
    * doesn't support Streaming SIMD Exceptions, even if the processor
    * does.
    */
   if ( gCpuCaps.hasSSE ) {
      //printf("Testing OS support for SSE... " );

//      __asm __volatile ("xorps %%xmm0, %%xmm0");
      __asm __volatile ("xorps %xmm0, %xmm0");

      if ( gCpuCaps.hasSSE ) {
	 //printf("yes.\n" );
      } else {
	 //printf("no!\n" );
      }
   }

   /* Emulate test for OSXMMEXCPT in CR4.  The OS will set this bit if
    * it supports unmasked SIMD FPU exceptions.  If we unmask the
    * exceptions, do a SIMD divide-by-zero and get a SIGILL, the OS
    * doesn't support unmasked SIMD FPU exceptions.  If we get a SIGFPE
    * as expected, we're okay but we need to clean up after it.
    *
    * Are we being too stringent in our requirement that the OS support
    * unmasked exceptions?  Certain RedHat 2.2 kernels enable SSE by
    * setting CR4.OSFXSR but don't support unmasked exceptions.  Win98
    * doesn't even support them.  We at least know the user-space SSE
    * support is good in kernels that do support unmasked exceptions,
    * and therefore to be safe I'm going to leave this test in here.
    */
   if ( gCpuCaps.hasSSE ) {
      //printf("Testing OS support for SSE unmasked exceptions... " );

//      test_os_katmai_exception_support();

      if ( gCpuCaps.hasSSE ) {
	 //printf("yes.\n" );
      } else {
	 //printf("no!\n" );
      }
   }

   /* Restore the original signal handlers.
    */
   sigaction( SIGILL, &saved_sigill, NULL );
   sigaction( SIGFPE, &saved_sigfpe, NULL );

   /* If we've gotten to here and the XMM CPUID bit is still set, we're
    * safe to go ahead and hook out the SSE code throughout Mesa.
    */
   if ( gCpuCaps.hasSSE ) {
      //printf("Tests of OS support for SSE passed.\n" );
   } else {
      //printf("Tests of OS support for SSE failed!\n" );
   }
#else
   /* We can't use POSIX signal handling to test the availability of
    * SSE, so we disable it by default.
    */
   //printf("Cannot test OS support for SSE, disabling to be safe.\n" );
   gCpuCaps.hasSSE=0;
#endif /* _POSIX_SOURCE && X86_FXSR_MAGIC */
#else
   /* Do nothing on other platforms for now.
    */
   //printf("Cannot test OS support for SSE, leaving disabled.\n" );
   gCpuCaps.hasSSE=0;
#endif /* __linux__ */
}
