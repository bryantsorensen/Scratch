//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
// Top-level simulation code for fixed-point C code
//
// Novidan, Inc. (c) 2023.  May not be used or copied without prior consent.
//
// Bryant Sorensen, author
// Started 24 Apr 2023
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

#include "Common.h"

using namespace std;

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Simulation variables
// 
// TODO: For Simulation: Use module enables to decide whether to output files for that module
//      Some modules may always have output, like WOLA

strSIM  SIM;            // Declared as global because both top level and SIM modules use it


//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Instantiate module structures as globals
// TODO: Determine if we want to make module classes instead of structures

strWDRC WDRC;
strSYS  SYS;
strFBC  FBC;
strNR   NR;

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Instantiate module parameter structures

strParams_WDRC  WDRC_Params;
strParams_SYS   SYS_Params;
strParams_FBC   FBC_Params;
strParams_EQ    EQ_Params;
strParams_NR    NR_Params;

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Main function. Drives simulation, calls all modules

#include "FW_Param_Init.cpp"      // Include init routine code here. Don't want to add Common.h to auto-generated init code, as this may vary by project

int main (int argc, char* argv[])
{
cWAVops WavInp;
cWAVops WavOutp;
int N;
int BlocksInSim;
int CurBlock;
int32_t Buf[8];
int8_t RetVal = 0;

//++++++++++++++++++++
// Firmware

// Initialize all parameters (in actual FW, this is call to EEPROM read)
    FW_Param_Init();

// Initialize all modules
    SYS_Init();
    WDRC_Init();
    FBC_Init();     // NOTE: FBC_Init MUST be called after WDRC_Init to capture correct target gains
    NR_Init();

//++++++++++++++++++++
// Simulation

// Simulation variables 

const double Scale24 = 0.00000011920928955078125;   // 2^-23
int k;

// Parse command line options until it returns -1
    RetVal = parse_command_line(argc, argv);
    if (RetVal != 0)
        exit(RetVal);

// Get elements of simulation initialized, including output file names; 
// call AFTER init of parameters and AFTER parsing command line to get file names & paths

    SIM_Init();     

// Open input .wav file, check its parameters, get size
// Call AFTER parsing command line to get file name

    WavInp.OpenReadFile(SIM.InfileName);
    if ((WavInp.GetBitsPerSample() != 24) || (WavInp.GetSampleRate() != BASEBAND_SAMPLE_RATE) || (!WavInp.IsMono()))
        printf("Something wrong with input file!\n");

    N = WavInp.GetNumSamples();
    BlocksInSim = N >> 3;    // Round off to modulo 8 floor
    N = BlocksInSim << 3;    // Update number of samples to be multiple of 8

// Create output .wav file; call AFTER simulation init to get file name

    WavOutp.OpenMonoWriteFile(SIM.OutFileName, BASEBAND_SAMPLE_RATE, 24, N);

// Do simulation, going through all blocks

    for (CurBlock = 0; CurBlock < BlocksInSim; CurBlock++)
    {

        WavInp.ReadNVals(8, Buf);       // SIM ONLY
        for (k = 0; k < BLOCK_SIZE; k++)
            SYS.InBuf[k] = to_frac24((double)Buf[k]*Scale24);   // This needs to be replaced with moving data in from audio I/O block

        SIM_Feedback(SYS.InBuf, SYS.OutBuf);

//++++++++++++++++++++
// Firmware

        SYS_FENG_ApplyInputGain();
        SYS_HEAR_WolaFwdAnalysis();
        SYS_HEAR_WolaRevAnalysis();

        NR_Main();

        FBC_HEAR_Levels();
        FBC_HEAR_DoFiltering();

        SYS_HEAR_ErrorSubAndEnergy();

        WDRC_Main();
        
        FBC_FilterAdaptation();

        SYS_HEAR_ApplySubbandGain();

        FBC_DoFreqShift();

        SYS_HEAR_WolaFwdSynthesis();

        SYS_FENG_AgcO();

//++++++++++++++++++++
// Simulation
        if (CurBlock > 5000)
            CurBlock = CurBlock;

        for (k = 0; k < BLOCK_SIZE; k++)
            Buf[k] = (int32_t)(round(SYS.OutBuf[k]/Scale24));       // This needs to be replaced with sending data to audio I/O block
        WavOutp.WriteNVals(8, Buf);      // SIM ONLY

        SIM_LogFiles();
    }

//++++++++++++++++++++
// Simulation

// Close all files
    WavInp.CloseFile();
    WavOutp.CloseFile();
    SIM_CloseSim();

// Exit the program
    exit(0);
}