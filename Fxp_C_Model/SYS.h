//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
// System processing header file for fixed-point C code
//
// Novidan, Inc. (c) 2023.  May not be used or copied with prior consent.
//
// Bryant Sorensen, author
// Started 09 May 2023
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

#ifndef _SYS_H
#define _SYS_H

struct strSYS
{
    frac24_t    FwdAnaIn[BLOCK_SIZE];
    Complex24   FwdAnaBuf[WOLA_NUM_BINS];
    Complex24   Error[WOLA_NUM_BINS];
    frac48_t    BinEnergy[WOLA_NUM_BINS];
    frac16_t    FwdGainLog2[WOLA_NUM_BINS];
    Complex24   FwdSynBuf[WOLA_NUM_BINS];
    frac24_t    FwdSynOut[BLOCK_SIZE];
    frac24_t    OutBuf[BLOCK_SIZE];
    frac24_t    AgcoLevel;

    frac24_t    RevAnaIn[BLOCK_SIZE];
    Complex24   RevAnaBuf[WOLA_NUM_BINS];

};

// TODO: Replace this simulation WOLA structure with a NULL / empty structure
struct strWOLA
{
    uint32_t    FwdWolaHandle;
    uint32_t    RevWolaHandle;

};

void SYS_Init();
void SYS_FENG_ApplyInputGain(frac24_t* InBuf);
void SYS_HEAR_WolaFwdAnalysis();
void SYS_HEAR_WolaRevAnalysis();
void SYS_HEAR_ErrorSubAndEnergy(Complex24* FBC_FilterOut);
void SYS_HEAR_ApplySubbandGain();
void SYS_HEAR_WolaFwdSynthesis();
void SYS_FENG_AgcO();
void SYS_SimCloseWola();    // SIM ONLY


#endif  // _SYS_H
