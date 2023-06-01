//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
// Simulation helpers functions for fixed-point C code
//
// Novidan, Inc. (c) 2023.  May not be used or copied without prior consent.
//
// Bryant Sorensen, author
// Started 01 Jun 2023
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

#include "Common.h"

//++++++++++++++++++++++++++++++++++++++++++++++++++
// Command line parsing; port of Linux getopt() function
// Code from https://stackoverflow.com/questions/10404448/getopt-h-compiling-linux-c-code-in-windows

// Change from original code: make these static, visible only to this file
static int  opterr = 1;     /* if error message should be printed */
static int  optind = 1;     /* index into parent argv vector */
static int  optreset;       /* reset getopt */
static char *optarg;        /* argument associated with option */

/*
* getopt --
*      Parse argc/argv argument vector.
*/
int getopt(int nargc, char * const nargv[], const char *ostr)
{
int  optopt;         /* character checked for validity */   // CHANGE: moved inside function
static char e[] = EMSG;
static char *place = e;              /* option letter processing */
const char *oli;                        /* option letter list index */

  if (optreset || !*place) {              /* update scanning pointer */
    optreset = 0;
    if (optind >= nargc || *(place = nargv[optind]) != '-') {
      place = e;
      return (-1);
    }
    if (place[1] && *++place == '-') {      /* found "--" */
      ++optind;
      place = e;
      return (-1);
    }
  }                                       /* option letter okay? */
  if ((optopt = (int)*place++) == (int)':' ||
    !(oli = strchr(ostr, optopt))) {
      /*
      * if the user didn't specify '-' as an option,
      * assume it means -1.
      */
      if (optopt == (int)'-')
        return (-1);
      if (!*place)
        ++optind;
      if (opterr && *ostr != ':')
        (void)printf("illegal option -- %c\n", optopt);
      return (BADCH);
  }
  if (*++oli != ':') {                    /* don't need argument */
    optarg = NULL;
    if (!*place)
      ++optind;
  }
  else {                                  /* need an argument */
    if (*place)                     /* no white space */
      optarg = place;
    else if (nargc <= ++optind) {   /* no arg */
      place = e;
      if (*ostr == ':')
        return (BADARG);
      if (opterr)
        (void)printf("option requires an argument -- %c\n", optopt);
      return (BADCH);
    }
    else                            /* white space */
      optarg = nargv[optind];
    place = e;
    ++optind;
  }
  return (optopt);                        /* dump back option letter */
} // getopt


// Change to oiginal code: Helper function to keep optarg a local static, return its value
char* get_optarg()
{
    return optarg;
}


int8_t parse_command_line(int argc, char * const argv[])
{
char ValidOptions[] = "s:d:r:h";     // List of valid option switches.  The ':' after a character means it has must have an argument after it
int option;
int8_t ExitVal = 0;

    SIM.InfileName = NULL;
    SIM.OutFileName = NULL;
    SIM.ResultPath = NULL;
    SIM.FBSimFile = NULL;

    option = 0;
    while ((option != -1) && (!ExitVal))
    {
        option = getopt(argc, argv, ValidOptions);
        switch (option)
        {
            case 'h':
                printf ("Command line options for %s:", argv[0]);
                printf ("-s <source file name and path>         REQUIRED\n");
                printf ("-d <destination file name and path>    REQUIRED\n");
                printf ("-r <results output directory>          REQUIRED\n");
                printf ("-h                                     THIS HELP MENU\n");
                printf ("\nNow exiting...\n\n");
                ExitVal = 1;
                break;
            case 's':
                SIM.InfileName = get_optarg();      // Point to input file name string
                break;
            case 'd':
                SIM.OutFileName = get_optarg();
                break;
            case 'r':
                SIM.ResultPath = get_optarg();
                break;
            case 'f':
                SIM.FBSimFile = get_optarg();
            case '?':
                printf ("\nErroneous Command Line Argument; use -h for help. Now exiting...\n\n");
                ExitVal = 2;
                break;
        }
    }
    return (ExitVal);

}

//++++++++++++++++++++++++++++++++++++++++++++++++++
// Audio processing simulation functions

static inline void SIM_WolaInit()
{
    WOLASim.FwdWolaHandle = WolaInit(WOLA_LA, WOLA_LS, WOLA_R, WOLA_N, WOLA_STACKING);
//    WolaSetAnaWin(WOLASim.FwdWolaHandle, WolaAnaWin, WOLA_N);     // Comment out to leave default window
//    WolaSetSynWin(WOLASim.FwdWolaHandle, WolaSynWin, WOLA_N);

    WOLASim.RevWolaHandle = WolaInit(WOLA_LA, WOLA_LS, WOLA_R, WOLA_N, WOLA_STACKING);
//    WolaSetAnaWin(WOLASim.RevWolaHandle, WolaAnaWin, WOLA_N);
}


void SIM_Init()
{
unsigned i;
FILE* fbf;
double fileval;

    // Wola handle initialization. SIM ONLY! Normally done by compile-time config for HEAR code
    SIM_WolaInit();
    for (i = 0; i < FB_SIM_TAPS; i++)
        SIM.OutBufDelay[i] = to_frac24(0);

    // Set up counters
    SIM.CurOpIdx = 0;
    SIM.CurSample = 0;

    // Open feedback sim file for read
    fopen_s(&fbf, SIM.FBSimFile, "r");
    if (fbf == NULL)    // If can't read, default to no sim (FIRs set to 0.0s)
    {
        printf ("\nERROR! Could not open FB sim file %s for read...\n\n", SIM.FBSimFile);
        SIM.TransitionStart = (uint32_t)(-1);   // Set so never encountered
        SIM.TransitionEnd = (uint32_t)(-1);
        for (i = 0; i < FB_SIM_TAPS; i++)
        {
            SIM.FB_FIR1[i] = to_frac24(0.0);
            SIM.FB_FIR2[i] = to_frac24(0.0);
        }
    }
    else
    {
    // FORMAT (all double values): transition start in seconds, FIR1 values, FIR2 values
        fscanf_s(fbf, "%le", &fileval);
        SIM.TransitionStart = (uint32_t)(fileval*(double)BASEBAND_SAMPLE_RATE);
        SIM.TransitionEnd = SIM.TransitionStart + (uint32_t)FB_SIM_TRNSTION_SMPLS_DBL;
        for (i = 0; i < FB_SIM_TAPS; i++)
        {
            fscanf_s(fbf, "%le", &fileval);
            SIM.FB_FIR1[i] = to_frac24(fileval);
        }
        for (i = 0; i < FB_SIM_TAPS; i++)
        {
            fscanf_s(fbf, "%le", &fileval);
            SIM.FB_FIR2[i] = to_frac24(fileval);
        }
        fclose(fbf);
    }
}


// Simulate acoustic feedback, to test FBC
void SIM_Feedback(frac24_t* inBuf, frac24_t* outBuf)
{
unsigned sample;
unsigned tap;
unsigned k;     // output buffer delay line index
accum_t Acc1, Acc2;
frac24_t F1, F2;
frac24_t S1, S2;
frac24_t FBSig;

    for (sample = 0; sample < BLOCK_SIZE; sample++)
    {
        Acc1 = to_accum(0);
        Acc2 = to_accum(0);
        SIM.OutBufDelay[SIM.CurOpIdx] = outBuf[sample];    // overwrite oldest with new sample
        for (tap = 0, k = SIM.CurOpIdx; tap < FB_SIM_TAPS; tap++)
        {
            Acc1 += SIM.FB_FIR1[tap]*SIM.OutBufDelay[k];
            Acc2 += SIM.FB_FIR2[tap]*SIM.OutBufDelay[k];
            k = (k - 1) & FB_SIM_TAPS_MASK;     // go back in time in out buffer delay line
        }
        SIM.CurOpIdx = (SIM.CurOpIdx+1) & FB_SIM_TAPS_MASK;   // move to next sample in buffer, forward in time

        // Create the mixing factors
        if (SIM.CurSample < SIM.TransitionStart)
            S1 = to_frac24(1.0);
        if ((SIM.CurSample >= SIM.TransitionStart) && (SIM.CurSample < SIM.TransitionEnd))
        {
            S1 = to_frac24(1.0) - to_frac24((double)(SIM.CurSample - SIM.TransitionStart)/FB_SIM_TRNSTION_SMPLS_DBL);
            S1 = (S1 < to_frac24(0.0)) ? to_frac24(0.0) : S1;       // Catch edge case, keep scale positive
        }
        else    // After TransitionEnd
            S1 = to_frac24(0.0);
        S2 = to_frac24(1.0) - S1;

        // Combine Acc1 and Acc2 using scale factors, into feedback signal; add back into input buffer
        F1 = rnd_sat24(Acc1);
        F2 = rnd_sat24(Acc2);
        Acc1 = F1*S1 + F2*S2;   FBSig = rnd_sat24(Acc1);
        inBuf[sample] = inBuf[sample] + FBSig;

        SIM.CurSample++;        // Update simulation sample count
    }
}


void SIM_CloseWola()
{
    WolaClose(WOLASim.FwdWolaHandle);
    WolaClose(WOLASim.RevWolaHandle);
}


//++++++++++++++++++++++++++++++++++++++++++++++++++
// File output functions

