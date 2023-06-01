//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
// System processing header file for fixed-point C code
//
// Novidan, Inc. (c) 2023.  May not be used or copied without prior consent.
//
// Bryant Sorensen, author
// Started 09 May 2023
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

#ifndef _SYS_H
#define _SYS_H

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Defines

#define     AGCO_ATK_TC     (1.0/32.0)      // TODO: Convert to fixed point
#define     AGCO_REL_TC     (1.0/512.0)

#define     MAX_REV_DELAY   32      // USE POWER OF TWO for easy roll-over


//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Module structure

struct strSYS
{
    frac16_t    MicCalGainLog2;
    frac24_t    InBuf[BLOCK_SIZE];
    frac24_t    FwdAnaIn[BLOCK_SIZE];
    Complex24   FwdAnaBuf[WOLA_NUM_BINS];
    frac48_t    MicEnergy[WOLA_NUM_BINS];
    Complex24   Error[WOLA_NUM_BINS];
    frac48_t    BinEnergy[WOLA_NUM_BINS];
    frac16_t    BinEnergyLog2[WOLA_NUM_BINS];
    frac16_t    FwdGainLog2[WOLA_NUM_BINS];
    Complex24   FwdSynBuf[WOLA_NUM_BINS];
    frac24_t    FwdSynOut[BLOCK_SIZE];
    frac24_t    OutBuf[BLOCK_SIZE];
    frac16_t    AgcoLevelLog2;
    frac16_t    AgcoGainLog2;
    frac16_t    DynamicGainLog2[WOLA_NUM_BINS];
    frac16_t    LimitedFwdGain[WOLA_NUM_BINS];

    frac24_t    RevAnaIn[BLOCK_SIZE];
    frac24_t    RevDelayBuf[MAX_REV_DELAY];
    int24_t     RevDlyPtr;
    Complex24   RevAnaBuf[WOLA_NUM_BINS][NUM_FBC_ANA_TAPS];
    frac48_t    RevEnergy[WOLA_NUM_BINS];

};

// TODO: Replace this simulation WOLA structure with a NULL / empty structure
struct strWOLA
{
    uint32_t    FwdWolaHandle;
    uint32_t    RevWolaHandle;

};

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Function prototypes

void SYS_Init();
void SYS_FENG_ApplyInputGain();
void SYS_HEAR_WolaFwdAnalysis();
void SYS_HEAR_WolaRevAnalysis();
void SYS_HEAR_ErrorSubAndEnergy();
void SYS_HEAR_ApplySubbandGain();
void SYS_HEAR_WolaFwdSynthesis();
void SYS_FENG_AgcO();


#endif  // _SYS_H
