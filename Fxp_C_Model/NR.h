//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
// Noise Reduction header file for fixed-point C code
//
// Novidan, Inc. (c) 2023.  May not be used or copied without prior consent.
//
// Bryant Sorensen, author
// Started 19 May 2023
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

#ifndef _NR_H
#define _NR_H

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Defines

#define     NR_SPEECH_COEFF             to_frac24(0.013244838192804309)     // About 100msec TC
#define     NR_INITIAL_SPEECH_ESTIMATE  to_frac16(-5.0)
#define     NR_INITIAL_NOISE_ESTIMATE   to_frac16(-5.0)
#define     NR_MIN_NOISE_ESTIMATE       to_frac16(-30.0)
#define     NR_INITIAL_SNR              (NR_INITIAL_SPEECH_ESTIMATE - NR_INITIAL_NOISE_ESTIMATE)
#define     NR_LOWER_LIMIT_GAIN_TABLE   to_frac16(0.332193)     // log2(10^(1/10))
#define     NR_UPPER_LIMIT_GAIN_TABLE   to_frac16(7.178408)     // 31/(log2(10^(14/10)) - log2(10^(1/10)))

#define     NR_BINS_PER_CALL            (WOLA_NUM_BINS>>2)      // 8 Make WOLA_NUM_BINS a multiple of this value

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Module structure

struct strNR
{
    int24_t     StartBin;
    int24_t     EndBin;
    frac16_t    NoiseSlowEst[WOLA_NUM_BINS];    // This is also the overall noise estimate
    frac16_t    NoiseFastEst[WOLA_NUM_BINS];
    frac16_t    SpeechEst[WOLA_NUM_BINS];
    frac16_t    SNREst[WOLA_NUM_BINS];
    frac16_t    BinGainLog2[WOLA_NUM_BINS];

};


//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Function prototypes

void NR_Init();
void NR_Main();

#endif      // _NR_H

