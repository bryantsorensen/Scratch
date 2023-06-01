//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
// Wide Dynamic Range Compressor (WDRC) header file for fixed-point C code
//
// Novidan, Inc. (c) 2023.  May not be used or copied without prior consent.
//
// Bryant Sorensen, author
// Started 05 May 2023
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

#ifndef _WDRC_H
#define _WDRC_H

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Defines

// Set up these values at build time
#define     NUM_WDRC_REGIONS        5       // 5 regions of WDRC action: expansion, lower compress, middle compress, upper compress, limiting

#define     WDRC_SIZE_CHAN_0        1
#define     WDRC_SIZE_CHAN_1        2
#define     WDRC_SIZE_CHAN_2        2
#define     WDRC_SIZE_CHAN_3        3
#define     WDRC_SIZE_CHAN_4        4
#define     WDRC_SIZE_CHAN_5        6
#define     WDRC_SIZE_CHAN_6        7
#define     WDRC_SIZE_CHAN_7        7

// Derived values
#define     WDRC_START_BIN_CHAN0    0
#define     WDRC_START_BIN_CHAN1    (WDRC_START_BIN_CHAN0 + WDRC_SIZE_CHAN_0)
#define     WDRC_START_BIN_CHAN2    (WDRC_START_BIN_CHAN1 + WDRC_SIZE_CHAN_1)
#define     WDRC_START_BIN_CHAN3    (WDRC_START_BIN_CHAN2 + WDRC_SIZE_CHAN_2)
#define     WDRC_START_BIN_CHAN4    (WDRC_START_BIN_CHAN3 + WDRC_SIZE_CHAN_3)
#define     WDRC_START_BIN_CHAN5    (WDRC_START_BIN_CHAN4 + WDRC_SIZE_CHAN_4)
#define     WDRC_START_BIN_CHAN6    (WDRC_START_BIN_CHAN5 + WDRC_SIZE_CHAN_5)
#define     WDRC_START_BIN_CHAN7    (WDRC_START_BIN_CHAN6 + WDRC_SIZE_CHAN_6)


//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Module structure

struct strWDRC
{
    int24_t     CurrentChannel;
    frac16_t    ChanEnergyLog2[NUM_WDRC_CHANNELS];      // Keep track for debug only
    frac16_t    LevelLog2[NUM_WDRC_CHANNELS];
    frac16_t    BinGainLog2[WOLA_NUM_BINS];
    frac16_t    ChanGainLog2[NUM_WDRC_CHANNELS];        // Keep track for debug only
    // Bin boundaries are const, known at build time
    const int24_t     ChannelStartBin[NUM_WDRC_CHANNELS];
    const int24_t     ChannelLastBin[NUM_WDRC_CHANNELS];

// WDRC working values
    frac16_t    Gain[NUM_WDRC_CHANNELS][NUM_WDRC_REGIONS];
    frac16_t    Thresh[NUM_WDRC_CHANNELS][NUM_WDRC_REGIONS];
    frac16_t    Slope[NUM_WDRC_CHANNELS][NUM_WDRC_REGIONS];

    strWDRC();              // Treating struct as public class; make constructor and destructor
    ~strWDRC() {};

};


//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Function prototypes

void WDRC_Init();
void WDRC_Main();

#endif _WDRC_H

