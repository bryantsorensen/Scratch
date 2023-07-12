//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
// Noise reduction module for fixed-point C code
//
// Novidan, Inc. (c) 2023.  May not be used or copied without prior consent.
//
// Bryant Sorensen, author
// Started 19 May 2023
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

#include "Common.h"

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Local constant memory (look-up table)

// Table is normalized gain curve in log2.
// Input is SNR in log2, times 8 (so 0.125 log2 steps)
// -1.0 --> max reduction (use -1 so we can get true full scale)
// 0.0 --> min reduction (unity gain)
static const frac24_t NR_NormalizedGainTable[NR_GAIN_TABLE_SIZE] = {
-1.0,       // 0 log2 = 0 dB
-1.0,       // 0.125 log2 = 0.75dB
-1.0,
-1.0,
-1.0,           // 0.5 log2 = 3.01dB
-0.905928125,
-0.81185625,
-0.717784375,
-0.625,         // 1.0 log2 = 6.02dB
-0.577964063,
-0.530928125,
-0.483892188,
-0.4375,        // 1.5 log2 = 9.03dB
-0.390464063,
-0.343428125,
-0.296392188,
-0.25,          // 2.0 log2 = 12.04dB
-0.19375,
-0.1125,
-0.053125,
0.0,            // 2.5 log2 = 15.05dB
0.0,
0.0,
0.0,
0.0,            // 3.0 log2 = 18.06dB
0.0,
0.0,
0.0,
0.0,
0.0,
0.0,
0               // 3.875 log2 = 23.33dB
};

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Functions

void NR_Init()
{
int24_t bin;

    NR.StartBin = 0;
    NR.EndBin = NR_BINS_PER_CALL-1;

    for (bin = 0; bin < WOLA_NUM_BINS; bin++)
    {
        NR.NoiseSlowEst[bin] = NR_INITIAL_NOISE_ESTIMATE;
        NR.NoiseFastEst[bin] = NR_INITIAL_NOISE_ESTIMATE;
        NR.SpeechEst[bin] = NR_INITIAL_SPEECH_ESTIMATE;
        NR.SNREst[bin] = NR.SpeechEst[bin] - NR.NoiseSlowEst[bin];
        NR.BinGainLog2[bin] = to_frac16(0);
    }
}


void NR_Main()
{
/*
1. I don't think forward masking does anything; remove
2. Consider converting BinPower to log2 and doing all filtering in that domain rather than 48b
3. Consider using shifts instead of multiplies for TCs if we must use 48b linear levels
4. Consider using log2 gains in look-up table instead of linear. OR, is there a multiplication to convert to log2 gains from log2 SNR diff?

TODO: Work out interpolation between table gains if entries are log2 domain, or other mapping between them
*/
int24_t bin;
frac16_t Diff;
accum_t Acc;
int24_t GainTableIndex;
frac24_t GainScaleTableOut;
frac16_t GainTarget;
frac16_t GainDiff;

    if (NR_Params.Profile.Enable)
    {
        for (bin = NR.StartBin; bin <= NR.EndBin; bin++)
        {
        // NoiseFastEst = NoiseFastTC*BinPower + (1-NoiseFastTC)*NoiseFastEst = NoiseFastTC*(BinPower - NoiseFastEst) + NoiseFastEst
        // Doing smoothing in log2 domain
            Diff = SYS.BinEnergyLog2[bin] - NR.NoiseFastEst[bin];
            NR.NoiseFastEst[bin] = mul_rnd16(NR_Params.Persist.NoiseFastTC, Diff) + NR.NoiseFastEst[bin];

        // SlowEst: in linear = (1+NoiseSlowCoeff)*SlowEst = SlowEst + NoiseSlowCoeff*SlowEst
        // in log2:  log2(1+NoiseSlowCoeff) + log2(SlowEst); so just convert NoiseSlowCoeff to log2 domain and add
            NR.NoiseSlowEst[bin] = NR_Params.Persist.NoiseSlowCoeff + NR.NoiseSlowEst[bin];

        // SlowEst = NoiseEst = min(FastEst, SlowEst)
            NR.NoiseSlowEst[bin] = min16(NR.NoiseFastEst[bin], NR.NoiseSlowEst[bin]);
            NR.NoiseSlowEst[bin] = max16(NR.NoiseSlowEst[bin], NR_MIN_NOISE_ESTIMATE);  // Don't let noise est go too low

        // SpeechEst = SpeechTC*BinPower + (1-SpeechTC)*SpeechEst = SpeechTC*(BinPower - SpeechEst) + SpeechEst
            Diff = SYS.BinEnergyLog2[bin] - NR.SpeechEst[bin];
            NR.SpeechEst[bin] = mul_rnd16(NR_Params.Persist.SpeechTC, Diff) + NR.SpeechEst[bin];

        //  SNR = log2(SpeechEst) - log2(NoiseEst);

            NR.SNREst[bin] = NR.SpeechEst[bin] - NR.NoiseSlowEst[bin];

        // Use SNR*8 (keep 3 fractional bits) as gain LUT input index.  Gain table output is 
        // normalized on [-1.0, 0]; multiply this by the max reduction to get gain
            Acc = NR.SNREst[bin];
            GainTableIndex = upper_accum_to_i24(shr(Acc, 13));    // Remove 13 fractional bits, keep 3
            GainTableIndex = maxint(GainTableIndex, 0);
            GainTableIndex = minint(GainTableIndex,(NR_GAIN_TABLE_SIZE-1));
            GainScaleTableOut = NR_NormalizedGainTable[GainTableIndex];
            GainTarget = mul_rnd16(GainScaleTableOut, NR_Params.Profile.MaxReduction[bin]);      // Scale normalized table by max gain reduction

        // Smooth the gain (in log2 domain)
            GainDiff = GainTarget - NR.BinGainLog2[bin];
            NR.BinGainLog2[bin] = mul_rnd16(NR_Params.Persist.GainSmoothTC, GainDiff) + NR.BinGainLog2[bin];     // Single-pole smoothing filter to update gain


        } // for bin

    // Set up next set of bins
        NR.StartBin = NR.EndBin+1;
        if (NR.StartBin >= WOLA_NUM_BINS)
            NR.StartBin = 0;
        NR.EndBin = NR.StartBin + (NR_BINS_PER_CALL - 1);
        NR.EndBin = (NR.EndBin > WOLA_NUM_BINS) ? WOLA_NUM_BINS : NR.EndBin;
    }
    else
    {
        for (bin = 0; bin < WOLA_NUM_BINS; bin++)
            NR.BinGainLog2[bin] = to_frac16(0);
    }
}