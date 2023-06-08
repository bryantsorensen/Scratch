//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
// System processing module for fixed-point C code
//
// Novidan, Inc. (c) 2023.  May not be used or copied without prior consent.
//
// Bryant Sorensen, author
// Started 09 May 2023
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

#include "Common.h"

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Functions

void SYS_Init()
{
int24_t i;
int24_t tap;

    SYS.MicCalGainLog2 = SYS_Params.Persist.InpMicGain;     // TODO: If we need to ramp gain, do it using this SYS variable

    for (i = 0; i < BLOCK_SIZE; i++)
    {
        SYS.FwdAnaIn[i] = to_frac24(0);
        SYS.FwdSynOut[i] = to_frac24(0);
        SYS.OutBuf[i] = to_frac24(0);
        SYS.RevAnaIn[i] = to_frac24(0);
    }

    for (i = 0; i < WOLA_NUM_BINS; i++)
    {
        SYS.FwdAnaBuf[i] = to_frac24(0);    // Complex, both parts set to 0
        SYS.MicEnergy[i] = to_frac48(0);
        SYS.Error[i] = to_frac24(0);        // Complex, both parts set to 0
        SYS.BinEnergy[i] = to_frac48(0);
        SYS.FwdGainLog2[i] = to_frac16(0);  // Unity gain
        SYS.FwdSynBuf[i] = to_frac24(0);    // Complex, both parts set to 0

        for (tap = 0; tap < NUM_FBC_ANA_TAPS; tap++)
            SYS.RevAnaBuf[i][tap] = to_frac24(0);

        SYS.RevEnergy[i] = to_frac48(0);
    }
    
    for (i = 0; i < MAX_REV_DELAY; i++)
        SYS.RevDelayBuf[i] = to_frac24(0);

    SYS.AgcoLevelLog2 = to_frac16(-40.0);
    SYS.AgcoGainLog2 = SYS_Params.Profile.AgcoGain;

    // WOLA initialization with windows & stacking mode
    WOLA_Init(&SYS.FwdWOLA, WOLA_STACKING, AnalysisWin, SynthesisWin);
    WOLA_Init(&SYS.RevWOLA, WOLA_STACKING, AnalysisWin, NULL);

    // Also initialize params-only EQ module
    if (!EQ_Params.Profile.Enable)
    {   
    // If module is disabled, set all the gains to unity (cheap help to configuration; only 
    // have to enable / disable instead of changing lots of EQ params). Re-use existing memory locations.
        EQ_Params.Profile.BroadbandGain = to_frac16(0);
        for (i = 0; i < WOLA_NUM_BINS; i++)
            EQ_Params.Profile.BinGain[i] = to_frac16(0);
    }
}



void SYS_FENG_ApplyInputGain()
{
int24_t i;

    for (i = 0; i < BLOCK_SIZE; i++)
    {
        SYS.FwdAnaIn[i] = mult_log2(SYS.InBuf[i], SYS_Params.Persist.InpMicGain);
    }
}


void SYS_HEAR_WolaFwdAnalysis()
{
int24_t i;
frac24_t Ar, Ai;

    WOLA_Analyze(&SYS.FwdWOLA, SYS.FwdAnaIn, SYS.FwdAnaBuf);     // Ignoring return value

    if (WOLA_STACKING == WOLA_STACKING_EVEN)
        SYS.FwdAnaBuf[0].SetImag(0.0);                  // Clear out Nyquist frequency to simplify things for Even stacking

    for (i = 0; i < WOLA_NUM_BINS; i++)
    {
        Ar = to_frac24(SYS.FwdAnaBuf[i].Real());    Ai = to_frac24(SYS.FwdAnaBuf[i].Imag());
        SYS.MicEnergy[i] = sat48(Ar*Ar + Ai*Ai);
    }   
    
}


void SYS_HEAR_WolaRevAnalysis()
{
int24_t i;
int24_t dp;     // Delay pointer
frac24_t Ar, Ai;

// TODO: Correctly emulate delay buffer
    for (i = 0, dp = SYS.RevDlyPtr; i < BLOCK_SIZE; i++)
    {
        SYS.RevDelayBuf[i] = SYS.OutBuf[i];
        SYS.RevAnaIn[i] = SYS.RevDelayBuf[dp];
        dp = (dp + 1) & (MAX_REV_DELAY-1);      // increment delay pointer, modulo max delay
    }
    SYS.RevDlyPtr = dp;

    WOLA_Analyze(&SYS.RevWOLA, SYS.RevAnaIn, SYS.RevAnaBuf[0]);     // Ignoring return value

    if (WOLA_STACKING == WOLA_STACKING_EVEN)
        SYS.RevAnaBuf[0][0].SetImag(0.0);                  // Clear out Nyquist frequency to simplify things for Even stacking

// TODO: Correctly emulate analysis buffer
    for (i = 0; i < WOLA_NUM_BINS; i++)
    {
        Ar = SYS.RevAnaBuf[0][i].Real();    Ai = SYS.RevAnaBuf[0][i].Imag();
        SYS.RevEnergy[i] = sat48(Ar*Ar + Ai*Ai);
    }   
    
}

void SYS_HEAR_ErrorSubAndEnergy()
{
int24_t i;
frac24_t Er, Ei;

    for (i = 0; i < WOLA_NUM_BINS; i++)
    {
        if (FBC_Params.Profile.Enable)
            SYS.Error[i] = SYS.FwdAnaBuf[i] - FBC.FiltSig[i];
        else
            SYS.Error[i] = SYS.FwdAnaBuf[i];
        Er = SYS.Error[i].Real();   Ei = SYS.Error[i].Imag();
        SYS.BinEnergy[i] = sat48(Er*Er + Ei*Ei);
        SYS.BinEnergyLog2[i] = log2(SYS.BinEnergy[i]);
    }

}


void SYS_HEAR_ApplySubbandGain()
{
frac16_t BinGainLog2;
int24_t i;

    for (i = 0; i < WOLA_NUM_BINS; i++)
    {
    // TODO: Add in NR gain
        BinGainLog2 = WDRC.BinGainLog2[i]; // + NR.BinGainLog2[i];
        SYS.DynamicGainLog2[i] = BinGainLog2;    // for use in FBC mu mod by gain
        BinGainLog2 += EQ_Params.Profile.BinGain[i] + FILTERBANK_GAIN_LOG2;
        BinGainLog2 = min16(BinGainLog2, FBC.GainLimLog2[i]);
        SYS.LimitedFwdGain[i] = BinGainLog2;        // For debugging
        SYS.FwdSynBuf[i].SetReal(rnd_sat24(mult_log2(SYS.Error[i].Real(), BinGainLog2)));
        SYS.FwdSynBuf[i].SetImag(rnd_sat24(mult_log2(SYS.Error[i].Imag(), BinGainLog2)));
    }

}


void SYS_HEAR_WolaFwdSynthesis()
{
    WOLA_Synthesize(&SYS.FwdWOLA, SYS.FwdSynBuf, SYS.FwdSynOut);
}


void SYS_FENG_AgcO()
{
int24_t i;
frac16_t BbGainLog2;
frac24_t TC;
frac16_t Diff;
frac16_t LevelLog2;

    for (i = 0; i < BLOCK_SIZE; i++)
    {
        BbGainLog2 = SYS_Params.Profile.VCGain + EQ_Params.Profile.BroadbandGain + SYS_Params.Profile.AgcoGain;     // Combine all broadband gains
        LevelLog2 = log2_approx(abs_f24(SYS.FwdSynOut[i]));
        Diff = LevelLog2 - SYS.AgcoLevelLog2;
        TC = (Diff > 0) ? AGCO_ATK_TC : AGCO_REL_TC;
        SYS.AgcoLevelLog2 = rnd_sat24(TC*Diff) + SYS.AgcoLevelLog2;
        if ((SYS.AgcoLevelLog2+BbGainLog2) > SYS_Params.Persist.AgcoThresh)
        // If the result of applying all the gains is going to exceed threshold
        // then apply as much as possible = the amount of gain that will take level to thresh
            SYS.AgcoGainLog2 = SYS_Params.Persist.AgcoThresh - SYS.AgcoLevelLog2;
        else
            SYS.AgcoGainLog2 = BbGainLog2;      // Otherwise apply all the gain possible
        SYS.OutBuf[i] = mult_log2(SYS.FwdSynOut[i], SYS.AgcoGainLog2);
    }
}
