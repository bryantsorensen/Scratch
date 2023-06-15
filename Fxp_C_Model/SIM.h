//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
// Simulation helpers header file for fixed-point C code
//
// Novidan, Inc. (c) 2023.  May not be used or copied without prior consent.
//
// Bryant Sorensen, author
// Started 01 Jun 2023
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

#ifndef _SIM_H
#define _SIM_H

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Includes

#include "WAV_Utils.h"      // This is useful as a separate (re-usable) entity

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Defines

// Command line processing defines
#define BADCH   (int)'?'
#define BADARG  (int)':'
#define EMSG    ""

// Feedback setup
#define     FB_SIM_TAPS_LOG2            7
#define     FB_SIM_TAPS                 (1 << FB_SIM_TAPS_LOG2)     // Guarantee power of two size. NOTE: This needs to account for bulk delay 0s at start of filters
#define     FB_SIM_TAPS_MASK            (FB_SIM_TAPS - 1)

#define     FB_SIM_TRANSITION_TIME      0.1             // Transition between feedback FIRs, in seconds
#define     FB_SIM_TRNSTION_SMPLS_DBL   (FB_SIM_TRANSITION_TIME*(double)BASEBAND_SAMPLE_RATE)

// File output enumerations
enum enSysFiles
{
    SysError = 0,
    SysFwdGainL2,
    SysAgcoGainL2,
    SysFwdAnaBuf,
    SysFwdSynOut,
    NUM_SYS_FILES
};

enum enWdrcFiles
{
    WdrcLevelL2 = 0,
    WdrcBinGainL2,
    NUM_WDRC_FILES
};

enum enFbcFiles
{
    FbcCoeffs = 0,
    FbcAdaptShift,
    FbcSinusoid,
    NUM_FBC_FILES
};

enum enNrFiles
{
    NrNoiseEst = 0,
    NrSpeechEst,
    NrSNREst,
    NrBinGainL2,
    NUM_NR_FILES
};


//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Module structure

struct strSIM
{
    char*       InfileName;         // Input .wav file name including path
    char        OutFileName[256];   // Output .wav file name including path; reserve the space here
    char*       ResultPath;         // Result path where to write simulation results
    char*       FBSimFile;          // Input file including path with feedback sim values (start time in seconds, FIR1 coeffs, FIR2 coeffs)

// Feedback simulation members
    double      FB_FIR1[FB_SIM_TAPS];       // Keep these as doubles; put any gain into the filter coefficients
    double      FB_FIR2[FB_SIM_TAPS];
    frac24_t    OutBufDelay[FB_SIM_TAPS];
    uint16_t    CurOpIdx;       // Current output buffer index
    uint32_t    CurSample;      // current simulation sample; limits sim size to 4292967295 samples, which is 178956.97 sec @ 24kHz
    uint32_t    TransitionStart;
    uint32_t    TransitionEnd;

// File output
    FILE*       SysFiles[NUM_SYS_FILES];
    FILE*       WdrcFiles[NUM_WDRC_FILES];
    FILE*       FbcFiles[NUM_FBC_FILES];
    FILE*       NrFiles[NUM_NR_FILES];

};


//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Function prototypes

int getopt(int nargc, char * const nargv[], const char *ostr);
char* get_optarg();
int8_t parse_command_line(int argc, char * const argv[]);

void SIM_Init();
void SIM_Feedback(frac24_t* inBuf, frac24_t* outBuf);
void SIM_LogFiles();
void SIM_CloseSim();

#endif      // _SIM_H

