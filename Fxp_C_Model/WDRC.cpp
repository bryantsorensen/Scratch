//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
// Wide Dynamic Range Compressor (WDRC) fixed-point C code
//
// Novidan, Inc. (c) 2023.  May not be used or copied without prior consent.
//
// Bryant Sorensen, author
// Started 05 May 2023
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

#include "Common.h"

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Initialization at boot or profile switch

void WDRC_Init()
{
uint8_t i;

    if (WDRC_Params.Profile.Enable)
    {
    // Convert parameters to working values
        for (i = 0; i < WDRC_NUM_CHANNELS; i++)
        {
            WDRC.Gain[i][0] = WDRC_Params.Profile.Gain[i][0];
            WDRC.Gain[i][1] = WDRC_Params.Profile.Gain[i][1];
            WDRC.Gain[i][2] = WDRC_Params.Profile.Gain[i][2];
            WDRC.Gain[i][3] = WDRC_Params.Profile.Gain[i][3];

            WDRC.Thresh[i][0] = WDRC_Params.Profile.Thresh[i][0];
            WDRC.Thresh[i][1] = WDRC_Params.Profile.Thresh[i][1];
            WDRC.Thresh[i][2] = WDRC_Params.Profile.Thresh[i][2];
            WDRC.Thresh[i][3] = WDRC_Params.Profile.Thresh[i][3];
            WDRC.Thresh[i][4] = to_frac16(0);     // Upper numeric limit - full scale log2 (unity)

        //    Slope[N] = (Gain[N] - Gain[N-1])/(Thresh[N] - Thresh[N-1])
        // TODO: Replace these divides by fixed-point iterative approximations

            WDRC.Slope[i][0] = WDRC_Params.Profile.ExpSlope[i];       // Start with expansion slope
            WDRC.Slope[i][1] = (WDRC.Gain[i][1] - WDRC.Gain[i][0])/(WDRC.Thresh[i][1] - WDRC.Thresh[i][0]);
            WDRC.Slope[i][2] = (WDRC.Gain[i][2] - WDRC.Gain[i][1])/(WDRC.Thresh[i][2] - WDRC.Thresh[i][1]);
            WDRC.Slope[i][3] = (WDRC.Gain[i][3] - WDRC.Gain[i][2])/(WDRC.Thresh[i][3] - WDRC.Thresh[i][2]);
            WDRC.Slope[i][4] = to_frac16(-1.0);        // Always fixed at -1.0 for limiting

        // Now finish Gain4 calc, knowing that Slope4 = -1
            WDRC.Gain[i][4] = (WDRC.Thresh[i][3] - WDRC.Thresh[i][4]) + WDRC.Gain[i][3];  // WDRC.Slope[4]*(WDRC.Thresh[4] - WDRC.Thresh[3]) + WDRC.Gain[3];
        }

    }
}


//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Main processing function

void WDRC_Main()
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
            Acc += (accum_t)SYS.BinEnergy[i];       // Add in linear domain; values are squared

        ChanEnergyLog2 = shr(log2_approx(Acc),1);       // Divide log2 by 2 to account for squared values

        LevelDiff = ChanEnergyLog2 - WDRC.LevelLog2[CurCh];
        Diff0 = ChanEnergyLog2 - WDRC.Thresh[CurCh][0];
        Diff3 = ChanEnergyLog2 - WDRC.Thresh[CurCh][3];
        WDRC.ChanEnergyLog2[CurCh] = ChanEnergyLog2;        // Save for debug
        if (LevelDiff > 0)      // Input level is greater than current level --> attacking
        {
            if (Diff0 < 0)          // input below Thresh0, in expansion region  BE - T0 < 0 ==> BE < T0
            {
                if (LevelDiff > WDRC_Params.Profile.ExpansDiffThresh)
                    TC = WDRC_Params.Persist.ExpansSupraAtkTC;
                else
                    TC = WDRC_Params.Persist.ExpansAtkTC;
            }
            else if (Diff3 >= 0)    // input above Thresh3, in limiting region  BE - T3 >= 0 ==> BE >= T3
            {
                if (LevelDiff > WDRC_Params.Profile.LimitDiffThresh)
                    TC = WDRC_Params.Persist.LimitSupraAtkTC;
                else
                    TC = WDRC_Params.Persist.LimitAtkTC;
            }
            else                    // input between T0 and T3 - compress region
            {
                if (LevelDiff > WDRC_Params.Profile.CompressDiffThresh)
                    TC = WDRC_Params.Persist.CompressSupraAtkTC;
                else
                    TC = WDRC_Params.Persist.CompressAtkTC;
            }
        }
        else    // Input level below current level --> release
        {
            if (Diff0 < 0)      // Expansion region
            {
                if (-LevelDiff > WDRC_Params.Profile.ExpansDiffThresh)    // Notice negation; if diff is large, use 'supra' TCs
                    TC = WDRC_Params.Persist.ExpansSupraRelTC;
                else
                    TC = WDRC_Params.Persist.ExpansRelTC;
            }
            else if (Diff3 >= 0)    // Limiting region
            {
                if (-LevelDiff > WDRC_Params.Profile.LimitDiffThresh)
                    TC = WDRC_Params.Persist.LimitSupraRelTC;
                else
                    TC = WDRC_Params.Persist.LimitRelTC;
            }
            else            // Compress region
            {
                if (-LevelDiff > WDRC_Params.Profile.CompressDiffThresh)
                    TC = WDRC_Params.Persist.CompressSupraRelTC;
                else
                    TC = WDRC_Params.Persist.CompressRelTC;
            }
        }

    // s1i7f16*s1i7f16 --> s1i14f33; shift left 7b to get f40, then HW rnd takes off 24b, back to f16
        WDRC.LevelLog2[CurCh] = mul_rnd16(TC, LevelDiff) + WDRC.LevelLog2[CurCh];     // Single-pole smoothing filter to update level

    // Loop through regions until we find where level is below the region's threshold
        Slope = to_frac16(0);
        Gain = to_frac16(0);
        for (i = 0; i <= 4; i++)
        {
            DiffThr = WDRC.LevelLog2[CurCh] - WDRC.Thresh[CurCh][i];
            if (DiffThr <= 0)
            {
                Slope = WDRC.Slope[CurCh][i];
                Gain = WDRC.Gain[CurCh][i];
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
        if (WDRC.CurrentChannel >= WDRC_NUM_CHANNELS)
        {
            WDRC.CurrentChannel = 0;
        }
    }
    else    // If WDRC not enabled, use unity gain
    {
        for (i = 0; i < WOLA_NUM_BINS; i++)
            WDRC.BinGainLog2[i] = to_frac16(0.0);
    }
}