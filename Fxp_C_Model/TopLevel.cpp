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

// NEED: Input file name & path; output .wav file name; where to write output files

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Instantiate module structures as globals
// TODO: Determine if we want to make module classes instead of structures

strWDRC WDRC;

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Instantiate module parameter structures

strParams_WDRC  WDRC_Params;
strParams_SYS   SYS_Params;


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
char InfileName[256];
char OutFileName[256];
char ResultPath[256];


// Test WAV read and write
    WavInp.OpenReadFile((char*)"whitenoise_3s.wav");
    if ((WavInp.GetBitsPerSample() != 24) || (WavInp.GetSampleRate() != 24000) || (!WavInp.IsMono()))
        printf("Something wrong with input file!\n");
    WavOutp.OpenMonoWriteFile((char*)"testnoise.wav", 24000, 24, 24000*3);
    N = WavInp.GetNumSamples();
    Blocks = N >> 3;
    for (i = 0; i < Blocks; i++)
    {
        WavInp.ReadNVals(8, Buf);
        WavOutp.WriteNVals(8, Buf);
    }
    WavInp.CloseFile();
    WavOutp.CloseFile();

// Test getopt
// Parse options until it returns -1
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
                strcpy_s(InfileName, get_optarg());
                break;
            case 'd':
                strcpy_s(OutFileName, get_optarg());
                break;
            case 'r':
                strcpy_s(ResultPath, get_optarg());
                break;
            case '?':
                printf ("\nErroneous Command Line Argument; use -h for help. Now exiting...\n\n");
                ExitVal = 2;
                break;
        }
    }
    if (ExitVal)
        exit(ExitVal);

    exit(0);
}