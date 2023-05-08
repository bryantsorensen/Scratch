//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
// Wide Dynamic Range Compressor (WDRC) fixed-point C code
//
// Novidan, Inc. (c) 2023.  May not be used or copied with prior consent.
//
// Bryant Sorensen, author
// Started 05 May 2023
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

#include "Common.h"


// Constructor; used to initialize const values
strWDRC::strWDRC() : ChannelStartBin{   WDRC_START_BIN_CHAN0, WDRC_START_BIN_CHAN1, WDRC_START_BIN_CHAN2, WDRC_START_BIN_CHAN3,
                                        WDRC_START_BIN_CHAN4, WDRC_START_BIN_CHAN5, WDRC_START_BIN_CHAN6, WDRC_START_BIN_CHAN7},
                    ChannelLastBin {(WDRC_START_BIN_CHAN1-1), (WDRC_START_BIN_CHAN2-1), (WDRC_START_BIN_CHAN3-1), (WDRC_START_BIN_CHAN4-1),
                                    (WDRC_START_BIN_CHAN5-1), (WDRC_START_BIN_CHAN6-1), (WDRC_START_BIN_CHAN7-1), (WOLA_NUM_BINS-1)}
{
}


//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Initialization at boot or profile switch

void WDRC_Init()
{
uint8_t i;

    WDRC.CurrentChannel = 0;     // Reset
// Set energies, levels to low values.  Reset gains to unity in log2 (0 --> 1.0 linear)
    for (i = 0; i < NUM_WDRC_CHANNELS; i++)
    {
        WDRC.ChanEnergyLog2[i] = to_frac16(-40.0);
        WDRC.LevelLog2[i] = to_frac16(-40.0);
        WDRC.ChanGainLog2[i] = 0;
    }
    for (i = 0; i < WOLA_NUM_BINS; i++)
        WDRC.BinGainLog2[i] = 0;

// Convert parameters to working values
    WDRC.Gain[0] = WDRC_Params.Profile.Gain[0];
    WDRC.Gain[1] = WDRC_Params.Profile.Gain[1];
    WDRC.Gain[2] = WDRC_Params.Profile.Gain[2];
    WDRC.Gain[3] = WDRC_Params.Profile.Gain[3];

    WDRC.Thresh[0] = WDRC_Params.Profile.Thresh[0];
    WDRC.Thresh[1] = WDRC_Params.Profile.Thresh[1];
    WDRC.Thresh[2] = WDRC_Params.Profile.Thresh[2];
    WDRC.Thresh[3] = WDRC_Params.Profile.Thresh[3];
    WDRC.Thresh[4] = MAX_VAL16;     // Upper numeric limit

//    Slope[N] = (Gain[N] - Gain[N-1])/(Thresh[N] - Thresh[N-1])
// TODO: Replace these divides by fixed-point iterative approximations

    WDRC.Slope[0] = WDRC_Params.Profile.ExpSlope;       // Start with expansion slope
    WDRC.Slope[1] = (WDRC.Gain[1] - WDRC.Gain[0])/(WDRC.Thresh[1] - WDRC.Thresh[0]);
    WDRC.Slope[1] = (WDRC.Gain[2] - WDRC.Gain[1])/(WDRC.Thresh[2] - WDRC.Thresh[1]);
    WDRC.Slope[1] = (WDRC.Gain[3] - WDRC.Gain[2])/(WDRC.Thresh[3] - WDRC.Thresh[2]);
    WDRC.Slope[4] = to_frac16(-1.0);        // Always fixed at -1.0 for limiting

    // Now finish Gain4 calc, knowing that Slope4 = -1
    WDRC.Gain[4] = (WDRC.Thresh[3] - WDRC.Thresh[4]) + WDRC.Gain[3];  // WDRC.Slope[4]*(WDRC.Thresh[4] - WDRC.Thresh[3]) + WDRC.Gain[3];

}


//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Main processing function

void WDRC_Main(frac48_t* BinEnergy)
{
accum_t Acc;
int24_t CurCh;
frac16_t ChanEnergyLog2;
frac16_t LevelDiff;
frac24_t TC;
frac24_t Diff0, Diff3;
frac16_t DiffThr;
frac16_t Slope;
frac16_t Gain;
frac16_t ChanGainLog2;
int24_t i;

    if (WDRC_Params.Profile.Enable)
    {
        CurCh = WDRC.CurrentChannel;

        Acc = 0;
        for (i = WDRC.ChannelStartBin[CurCh]; i <= WDRC.ChannelLastBin[CurCh]; i++)
            Acc += (accum_t)BinEnergy[i];

        ChanEnergyLog2 = log2_approx(Acc);

        LevelDiff = ChanEnergyLog2 - WDRC.LevelLog2[CurCh];
        Diff0 = ChanEnergyLog2 - WDRC.Thresh[0];
        Diff3 = ChanEnergyLog2 - WDRC.Thresh[3];
        WDRC.ChanEnergyLog2[CurCh] = ChanEnergyLog2;        // Save for debug
        if (LevelDiff > 0)      // Input level is greater than current level --> attacking
        {
            if (Diff0 < 0)          // input below Thresh0, in expansion region  BE - T0 < 0 ==> BE < T0
                TC = WDRC_Params.Persist.ExpansAtkTC;
            else if (Diff3 >= 0)    // input above Thresh3, in limiting region  BE - T3 >= 0 ==> BE >= T3
                TC = WDRC_Params.Persist.LimitAtkTC;
            else                    // input between T0 and T3
                TC = WDRC_Params.Persist.CompressAtkTC;
        }
        else    // Input level below current level --> release
        {
            if (Diff0 < 0)
                TC = WDRC_Params.Persist.ExpansRelTC;
            else if (Diff3 >= 0)
                TC = WDRC_Params.Persist.LimitRelTC;
            else
                TC = WDRC_Params.Persist.CompressRelTC;
        }

    // s1i7f16*s1i7f16 --> s1i14f33; shift left 7b to get f40, then HW rnd takes off 24b, back to f16
        WDRC.LevelLog2[CurCh] = mul_rnd16(TC, LevelDiff) + WDRC.LevelLog2[CurCh];     // Single-pole smoothing filter to update level

    // Loop through regions until we find where level is below the region's threshold
        for (i = 0; i <= 4; i++)
        {
            DiffThr = WDRC.LevelLog2[CurCh] - WDRC.Thresh[i];
            if (DiffThr <= 0)
            {
                Slope = WDRC.Slope[i];
                Gain = WDRC.Gain[i];
                break;      // get out of 'for' loop
            }
        }

    // Calculate the gain for this channel; distribute across bins
        ChanGainLog2 = mul_rnd16(Slope, DiffThr) + Gain;
        for (i = WDRC.ChannelStartBin[CurCh]; i <= WDRC.ChannelLastBin[CurCh]; i++)
            WDRC.BinGainLog2[i] = ChanGainLog2;         // Keep gain in log2; to combine gains across all algos, we'll add in log2, then do a single exp2 calc
        WDRC.ChanGainLog2[CurCh] = ChanGainLog2;        // Keep track for debugging

    // Go to next bin or wrap around
        WDRC.CurrentChannel = CurCh+1;
        if (WDRC.CurrentChannel >= NUM_WDRC_CHANNELS)
        {
            WDRC.CurrentChannel = 0;
        }
    }
}