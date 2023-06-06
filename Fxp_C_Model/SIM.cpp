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
char ValidOptions[] = "s:d:r:f:h";     // List of valid option switches.  The ':' after a character means it has must have an argument after it
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


static void SIM_FB_Init()
{
unsigned i;
FILE* fbf = NULL;
double fileval;

    for (i = 0; i < FB_SIM_TAPS; i++)
        SIM.OutBufDelay[i] = to_frac24(0);

    // Set up simulation counters
    SIM.CurOpIdx = 0;
    SIM.CurSample = 0;

    if (FBC_Params.Profile.Enable)
    {
        // Open feedback sim file for read
        if (SIM.FBSimFile != NULL)
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
        // Scale the FIR coefficients with any desired gain
            fscanf_s(fbf, "%le", &fileval);
            SIM.TransitionStart = (uint32_t)(fileval*(double)BASEBAND_SAMPLE_RATE);
            SIM.TransitionEnd = SIM.TransitionStart + (uint32_t)FB_SIM_TRNSTION_SMPLS_DBL;
            for (i = 0; i < FB_SIM_TAPS; i++)
            {
                fscanf_s(fbf, "%le", &(SIM.FB_FIR1[i]));
            }
            for (i = 0; i < FB_SIM_TAPS; i++)
            {
                fscanf_s(fbf, "%le", &(SIM.FB_FIR2[i]));
            }
            fclose(fbf);
        }
    }
}


// Call this setup after parameters have been initialized
// Enable file output for modules that are enabled.

static void SIM_OutputFileSetup()
{
unsigned fidx;
char fname[256];

// Open SYS files (always)
    sprintf_s(fname, "%s/%s", SIM.ResultPath, "SYS_Error.csv");       fopen_s(&SIM.SysFiles[SysError], fname, "w");     // if returns NULL, let error occur when trying to write to the file
    sprintf_s(fname, "%s/%s", SIM.ResultPath, "SYS_FwdGainLog2.csv"); fopen_s(&SIM.SysFiles[SysFwdGainL2], fname, "w");
    sprintf_s(fname, "%s/%s", SIM.ResultPath, "SYS_AgcoGainLog2");    fopen_s(&SIM.SysFiles[SysAgcoGainL2], fname, "w");

// Open WDRC files
    if (WDRC_Params.Profile.Enable)
    {
        sprintf_s(fname, "%s/%s", SIM.ResultPath, "WDRC_LevelLog2.csv");      fopen_s(&SIM.WdrcFiles[WdrcLevelL2], fname, "w");
        sprintf_s(fname, "%s/%s", SIM.ResultPath, "WDRC_BinGainLog2.csv");    fopen_s(&SIM.WdrcFiles[WdrcBinGainL2], fname, "w");
    }
    else
    {
        for (fidx = 0; fidx < NUM_WDRC_FILES; fidx++)
            SIM.WdrcFiles[fidx] = NULL;
    }

// Open FBC files
    if (FBC_Params.Profile.Enable)
    {
        sprintf_s(fname, "%s/%s", SIM.ResultPath, "FBC_Coeffs.csv");      fopen_s(&SIM.FbcFiles[FbcCoeffs], fname, "w");
        sprintf_s(fname, "%s/%s", SIM.ResultPath, "FBC_AdaptShift.csv");  fopen_s(&SIM.FbcFiles[FbcAdaptShift], fname, "w");
        sprintf_s(fname, "%s/%s", SIM.ResultPath, "FBC_Sinusoid.csv");    fopen_s(&SIM.FbcFiles[FbcSinusoid], fname, "w");
    }
    else
    {
        for (fidx = 0; fidx < NUM_FBC_FILES; fidx++)
            SIM.FbcFiles[fidx] = NULL;
    }

// Open NR files
    if (NR_Params.Profile.Enable)
    {
        sprintf_s(fname, "%s/%s", SIM.ResultPath, "NR_NoiseEst.csv");     fopen_s(&SIM.NrFiles[NrNoiseEst], fname, "w");
        sprintf_s(fname, "%s/%s", SIM.ResultPath, "NR_SpeechEst.csv");    fopen_s(&SIM.NrFiles[NrSpeechEst], fname, "w");
        sprintf_s(fname, "%s/%s", SIM.ResultPath, "NR_SnrEst.csv");       fopen_s(&SIM.NrFiles[NrSNREst], fname, "w");
        sprintf_s(fname, "%s/%s", SIM.ResultPath, "NR_BinGainLog2.csv");  fopen_s(&SIM.NrFiles[NrBinGainL2], fname, "w");
    }
    else
    {
        for (fidx = 0; fidx < NUM_NR_FILES; fidx++)
            SIM.NrFiles[fidx] = NULL;
    }
}


void SIM_Init()
{
    // Wola handle initialization. SIM ONLY! Normally done by compile-time config for HEAR code
    SIM_WolaInit();

    // Set up feedback simulation
    SIM_FB_Init();

    // Set up the simulation files
    SIM_OutputFileSetup();
}


// Simulate acoustic feedback, to test FBC
void SIM_Feedback(frac24_t* inBuf, frac24_t* outBuf)
{
unsigned sample;
unsigned tap;
unsigned k;     // output buffer delay line index
double F1, F2;      // Use doubles for simulation
double S1, S2;
double FBSig;
double CombinedSig;

    for (sample = 0; sample < BLOCK_SIZE; sample++)
    {
        F1 = 0.0;
        F2 = 0.0;
        SIM.OutBufDelay[SIM.CurOpIdx] = outBuf[sample];    // overwrite oldest with new sample
        for (tap = 0, k = SIM.CurOpIdx; tap < FB_SIM_TAPS; tap++)
        {
            F1 += (double)(SIM.FB_FIR1[tap])*SIM.OutBufDelay[k];
            F2 += (double)(SIM.FB_FIR2[tap])*SIM.OutBufDelay[k];
            k = (k - 1) & FB_SIM_TAPS_MASK;     // go back in time in out buffer delay line
        }
        SIM.CurOpIdx = (SIM.CurOpIdx+1) & FB_SIM_TAPS_MASK;   // move to next sample in buffer, forward in time

        // Create the mixing factors
        if (SIM.CurSample < SIM.TransitionStart)
            S1 = 1.0;
        if ((SIM.CurSample >= SIM.TransitionStart) && (SIM.CurSample < SIM.TransitionEnd))
        {
            S1 = 1.0 - ((double)(SIM.CurSample - SIM.TransitionStart)/FB_SIM_TRNSTION_SMPLS_DBL);
            S1 = (S1 < 0.0) ? 0.0 : S1;       // Catch edge case, keep scale positive
        }
        else    // After TransitionEnd
            S1 = 0.0;
        S2 = 1.0 - S1;

        // Combine F1 and F2 into feedback signal using scale factors; saturate; add back into input buffer
        FBSig = F1*S1 + F2*S2;
        CombinedSig = (double)(inBuf[sample]) + FBSig;
        CombinedSig = (CombinedSig >= 1.0) ? (1.0-1.0e-23) : CombinedSig;
        CombinedSig = (CombinedSig < -1.0) ? -1.0 : CombinedSig;
        inBuf[sample] = to_frac24(CombinedSig);

        SIM.CurSample++;        // Update simulation sample count
    }
}


// TODO: Update write functions to handle fixed-point types
void SIM_WriteComplex24 (FILE* fp, Complex24* Cval, unsigned NumVals)
{
unsigned i = 0;     // init to 0 for NumVals = 1 case

    if (fp != NULL)
    {
        for (; i < (NumVals-1); i++)
            fprintf(fp, "%2.12e+%2.12ej, ", Cval[i].Real(), Cval[i].Imag());
        fprintf(fp, "%2.12e+%2.12ej\n", Cval[i].Real(), Cval[i].Imag());
    }
}

void SIM_Write16 (FILE* fp, frac16_t* Cval, unsigned NumVals)
{
unsigned i = 0;     // init to 0 for NumVals = 1 case

    if (fp != NULL)
    {
        for (; i < (NumVals-1); i++)
            fprintf(fp, "%2.12e, ", Cval[i]);
        fprintf(fp, "%2.12e\n", Cval[i]);
    }
}

void SIM_WriteInt (FILE* fp, int24_t* Cval, unsigned NumVals)
{
unsigned i = 0;     // init to 0 for NumVals = 1 case

    if (fp != NULL)
    {
        for (; i < (NumVals-1); i++)
            fprintf(fp, "%d, ", Cval[i]);
        fprintf(fp, "%d\n", Cval[i]);
    }

}


void SIM_LogFiles()
{
    SIM_WriteComplex24 (SIM.SysFiles[SysError], SYS.Error, WOLA_NUM_BINS);
    SIM_Write16 (SIM.SysFiles[SysFwdGainL2], SYS.FwdGainLog2, WOLA_NUM_BINS);
    SIM_Write16 (SIM.SysFiles[SysAgcoGainL2], &SYS.AgcoGainLog2, 1);

    SIM_Write16 (SIM.WdrcFiles[WdrcLevelL2], WDRC.LevelLog2, WDRC_NUM_CHANNELS);
    SIM_Write16 (SIM.WdrcFiles[WdrcBinGainL2], WDRC.BinGainLog2, WOLA_NUM_BINS);

    SIM_WriteComplex24 (SIM.FbcFiles[FbcCoeffs], &FBC.Coeffs[0][0], (WOLA_NUM_BINS*FBC_COEFFS_PER_BIN));
    SIM_WriteInt (SIM.FbcFiles[FbcAdaptShift], FBC.AdaptShift, WOLA_NUM_BINS);
    SIM_WriteComplex24 (SIM.FbcFiles[FbcSinusoid], FBC.Sinusoid, 1);

    SIM_Write16 (SIM.NrFiles[NrNoiseEst], NR.NoiseEst, WOLA_NUM_BINS);
    SIM_Write16 (SIM.NrFiles[NrSpeechEst], NR.SpeechEst, WOLA_NUM_BINS);
    SIM_Write16 (SIM.NrFiles[NrSNREst], NR.SNREst, WOLA_NUM_BINS);
    SIM_Write16 (SIM.NrFiles[NrBinGainL2], NR.BinGainLog2, WOLA_NUM_BINS);
}


static void SIM_CloseOutFiles(FILE** FileList, unsigned NumFiles)
{
unsigned fidx;

    for (fidx = 0; fidx < NumFiles; fidx++)
    {
        if (FileList[fidx] != NULL)
        {
            fclose(FileList[fidx]);
            FileList[fidx] = NULL;
        }
    }
}


void SIM_CloseSim()
{
    WolaClose(WOLASim.FwdWolaHandle);
    WolaClose(WOLASim.RevWolaHandle);

    SIM_CloseOutFiles(SIM.SysFiles,  NUM_SYS_FILES);
    SIM_CloseOutFiles(SIM.WdrcFiles, NUM_WDRC_FILES);
    SIM_CloseOutFiles(SIM.FbcFiles,  NUM_FBC_FILES);
    SIM_CloseOutFiles(SIM.NrFiles,   NUM_NR_FILES);
}


//++++++++++++++++++++++++++++++++++++++++++++++++++
// File output functions

