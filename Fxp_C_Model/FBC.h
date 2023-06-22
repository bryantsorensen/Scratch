//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
// Feedback Canceller (FBC) header file for fixed-point C code
//
// Novidan, Inc. (c) 2023.  May not be used or copied without prior consent.
//
// Bryant Sorensen, author
// Started 11 May 2023
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

#ifndef _FBC_H
#define _FBC_H

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Defines

#define     FBC_START_BIN               5       // Bin at which adaptation starts (no adaptation below this)
#define     FBC_END_BIN                 (WOLA_NUM_BINS-1)
#define     FBC_BINS_PER_CALL           9

#define     MAX_GAIN_MU_ADJ             4       // Max amount that gain difference can adjust Mu shift
#define     FBC_LEVEL_ATK_SHIFT         0
#define     FBC_LEVEL_REL_SHIFT         8

#define     FBC_FILT_SHIFT              2       // Shift to give some headroom in FBC filter
#define     FBC_FILT_SHIFT_GAIN_LOG2    to_frac16((double)FBC_FILT_SHIFT)

#define     FBC_MU_NORM_BIAS            -22     // Power of two limit on normalization Mu


//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Module structure

struct strFBC
{
    int24_t     StartBin;
    int24_t     EndBin;
    frac24_t    TargetGainLog2[WOLA_NUM_BINS];
    int24_t     IntermLeak;
    int24_t     AdaptShift[WOLA_NUM_BINS];
    Complex24   Coeffs[WOLA_NUM_BINS][FBC_COEFFS_PER_BIN];
    frac16_t    CoefMag[WOLA_NUM_BINS];
    Complex24   FiltSig[WOLA_NUM_BINS];
    frac16_t    GainLimLog2[WOLA_NUM_BINS];

    // Note that Error energy already calculated in SYS module
    frac48_t    BEEnergy[WOLA_NUM_BINS];
    frac48_t    ASmoothed[WOLA_NUM_BINS];
    frac48_t    ESmoothed[WOLA_NUM_BINS];
    frac48_t    BESmoothed[WOLA_NUM_BINS];

    Complex24   Sinusoid[2];        // Cos(x) + jSin(x)
    int24_t     FreqShiftEnable;

};


//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Function prototypes

void FBC_Init();
void FBC_HEAR_Levels();
void FBC_HEAR_DoFiltering();
void FBC_FilterAdaptation();
void FBC_DoFreqShift();

#endif  // _FBC_H
