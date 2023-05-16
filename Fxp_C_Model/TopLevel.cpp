//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
// Top-level simulation code for fixed-point C code
//
// Novidan, Inc. (c) 2023.  May not be used or copied with prior consent.
//
// Bryant Sorensen, author
// Started 24 Apr 2023
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

#include "Common.h"

using namespace std;

// TODO: Use module enables to decide whether to output files for that module
//      Some modules may always have output, like WOLA

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Instantiate module structures as globals
// TODO: Determine if we want to make module classes instead of structures

strWDRC WDRC;
strSYS  SYS;
strFBC  FBC;


//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Instantiate module parameter structures

strParams_WDRC  WDRC_Params;
strParams_SYS   SYS_Params;
strParams_FBC   FBC_Params;


//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Main function. Drives simulation, calls all modules

#include "FW_Param_Init.cpp"      // Include init routine code here. Don't want to add Common.h to auto-generated init code, as this may vary by project

int main (int argc, char* argv[])
{
cWAVops WavInp;
cWAVops WavOutp;
int N;
int Blocks;
int i;
int32_t Buf[8];
char ValidOptions[] = "s:d:r:h";     // List of valid option switches.  The ':' after a character means it has must have an argument after it
int option;
int8_t ExitVal = 0;
char* InfileName = NULL;
char* OutFileName = NULL;
char* ResultPath = NULL;


// Parse command line options until it returns -1
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
                InfileName = get_optarg();      // Point to input file name string
                break;
            case 'd':
                OutFileName = get_optarg();
                break;
            case 'r':
                ResultPath = get_optarg();
                break;
            case '?':
                printf ("\nErroneous Command Line Argument; use -h for help. Now exiting...\n\n");
                ExitVal = 2;
                break;
        }
    }
    if (ExitVal)
        exit(ExitVal);

// Open input .wav file, check its parameters, get size, create output .wav file

    WavInp.OpenReadFile(InfileName);
    if ((WavInp.GetBitsPerSample() != 24) || (WavInp.GetSampleRate() != 24000) || (!WavInp.IsMono()))
        printf("Something wrong with input file!\n");

    N = WavInp.GetNumSamples();
    Blocks = N >> 3;    // Round off to modulo 8 floor
    N = Blocks << 3;    // Update number of samples to be multiple of 8

    WavOutp.OpenMonoWriteFile(OutFileName, 24000, 24, N);

Complex24 FBC_FilterOut[WOLA_NUM_BINS];     // Temp for sim
frac24_t fBuf[BLOCK_SIZE];
const double Scale24 = 0.00000011920928955078125;   // 2^-23
int k;
for (k = 0; k < WOLA_NUM_BINS; k++)
    FBC_FilterOut[k] = 0;

// Initialize all modules
    SYS_Init();
    WDRC_Init();
    FBC_Init();     // NOTE: FBC_Init MUST be called after WDRC_Init in order to capture correct target gains

// Do simulation, going through all blocks
    for (i = 0; i < Blocks; i++)
    {
        WavInp.ReadNVals(8, Buf);       // SIM ONLY
        for (k = 0; k < BLOCK_SIZE; k++)
            fBuf[k] = to_frac24((double)Buf[k]*Scale24);
        
        SYS_FENG_ApplyInputGain(fBuf);
        SYS_HEAR_WolaFwdAnalysis();

        SYS_HEAR_ErrorSubAndEnergy(FBC_FilterOut);

        WDRC_Main(SYS.BinEnergy);
        
        SYS_HEAR_ApplySubbandGain();

        SYS_HEAR_WolaFwdSynthesis();

        SYS_FENG_AgcO();

        for (k = 0; k < BLOCK_SIZE; k++)
            Buf[k] = (int32_t)(round(SYS.OutBuf[k]/Scale24));
        WavOutp.WriteNVals(8, Buf);      // SIM ONLY
    }

// Close all files and exit
    WavInp.CloseFile();
    WavOutp.CloseFile();
    SYS_SimCloseWola();

    exit(0);
}