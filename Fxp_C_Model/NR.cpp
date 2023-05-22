//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
// Noise reduction module for fixed-point C code
//
// Novidan, Inc. (c) 2023.  May not be used or copied with prior consent.
//
// Bryant Sorensen, author
// Started 19 May 2023
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

#include "Common.h"

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Local constant memory (look-up table)

static const frac24_t NR_GainTable[32] = {

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
        NR.NoiseEst[bin] = NR_INITIAL_NOISE_ESTIMATE;
        NR.SpeechEst[bin] = NR_INITIAL_SPEECH_ESTIMATE;
        NR.SNREst[bin] = NR.SpeechEst[bin] - NR.NoiseEst[bin];
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
frac24_t GainTableFrac;
frac24_t LowerGain;
frac24_t UpperGain;
frac24_t TableDiff;
frac16_t GainTableOut;

    for (bin = NR.StartBin; bin <= NR.EndBin; bin++)
    {
    // NoiseFastEst = NoiseFastTC*BinPower + (1-NoiseFastTC)*NoiseFastEst = NoiseFastTC*(BinPower - NoiseFastEst) + NoiseFastEst
        Diff = SYS.BinEnergyLog2[bin] - NR.NoiseFastEst[bin];
        Acc = Diff * NR_Params.Persist.NoiseFastTC + NR.NoiseFastEst[bin];
        NR.NoiseFastEst[bin] = rnd_sat16(Acc);

    // If FastEst < SlowEst then SlowEst = FastEst
    // Else SlowEst = (1+NoiseSlowCoeff)*SlowEst = SlowEst + NoiseSlowCoeff*SlowEst

        if (NR.NoiseFastEst[bin] < NR.NoiseSlowEst[bin])
            NR.NoiseSlowEst[bin] = NR.NoiseFastEst[bin];
        else
        {
            Acc = NR.NoiseSlowEst[bin] * NR_Params.Persist.NoiseSlowCoeff + NR.NoiseSlowEst[bin];
            NR.NoiseSlowEst[bin] = rnd_sat16(Acc);
        }

    // NoiseEst = min(FastEst, SlowEst)

        NR.NoiseEst[bin] = (NR.NoiseFastEst[bin] < NR.NoiseSlowEst[bin]) ? NR.NoiseFastEst[bin] : NR.NoiseSlowEst[bin];

    // SpeechEst = (1-SpeechTC)*BinPower + SpeechTC*SpeechEst = SpeechTC*(SpeechEst - BinPower) + BinPower

        Diff = NR.SpeechEst[bin] - SYS.BinEnergyLog2[bin];
        Acc = Diff * NR_SPEECH_COEFF + SYS.BinEnergyLog2[bin];
        NR.SpeechEst[bin] = rnd_sat16(Acc);

    //  SNR = log2(SpeechEst) - log2(NoiseEst);

        NR.SNREst[bin] = NR.SpeechEst[bin] - NR.NoiseEst[bin];

        //  GainTableIndex = floor((SNR - LowerLimit) * UpperLimit);
        //  GainTableIndex = max(GainTableIndex,0);
        //  GainTableIndex = min(GainTableIndex,31);
        // LowerLimit and UpperLimit are fixed values
        // Linearly interpolate between gain table entries
        // take (table output + linear interpolation)*DeltaComp ==> gain

// TODO: The conversion needs to go directly from log2 SNR to log2 gain, AND needs to be much, much simpler;
// probably need to get rid of DeltaComp correction factor

        Diff = NR.SNREst[bin] - NR_LOWER_LIMIT;
        Acc = Diff * NR_UPPER_LIMIT;
        Acc = shr(Acc, 9);      // effectively take floor(), integer part of product
        GainTableIndex = (int24_t)upper_accum_to_f24(Acc);
        if (GainTableIndex < 0)
        {
            GainTableOut = NR_GainTable[0];
        } 
        else if (GainTableIndex >= 31)
        {
            GainTableOut = NR_GainTable[31];
        }
        else
        {
            GainTableFrac = (frac24_t)lower_accum_to_f24(Acc);    // Cast as s0f24 unsigned
            LowerGain = NR_GainTable[GainTableIndex];
            UpperGain = NR_GainTable[GainTableIndex+1];
        // Gain = LowerGain + (GainTableFrac * (UpperGain - LowerGain)) = GainTableFrac*UpperGain + (1-GainTableFrac)*LowerGain
            TableDiff = UpperGain - LowerGain;
            Acc = (TableDiff * GainTableFrac) + LowerGain;       // Auto adjust f24 to accum for add
            GainTableOut = rnd_sat24(Acc);
        }
        NR.BinGainLog2[bin] = rnd_sat24(GainTableOut * NR_Params.Profile.MaxReduction[bin]);    // TODO: Get this working with log2 table output

    } // for bin

// Set up next set of bins
    NR.StartBin = NR.EndBin+1;
    if (NR.StartBin >= WOLA_NUM_BINS)
        NR.StartBin = 0;
    NR.EndBin = NR.StartBin + (NR_BINS_PER_CALL - 1);
}