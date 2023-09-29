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

        for (tap = 0; tap < FBC_REV_ANA_BUF_SIZE; tap++)
            SYS.RevAnaBuf[tap][i] = to_frac24(0);

        SYS.RevEnergy[i] = to_frac48(0);
    }

    SYS.RevBufPtr = 0;
    SYS.RevAnaPtr = -1;     // Back up one sample so pre-adjust goes to 0

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
        SYS.FwdAnaIn[i] = mult_log2(SYS.InBuf[i], SYS.MicCalGainLog2);
    }
    // TODO: If need to ramp input on start-up, modify SYS.MicCalGainLog2 here until it matches SYS_Params.Persist.InpMicGain
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
        Ar = SYS.FwdAnaBuf[i].Real();       Ai = SYS.FwdAnaBuf[i].Imag();
        Ar *= WOLA_ANA_POSTSCALE;           Ai *= WOLA_ANA_POSTSCALE;      // Emulate block floating point
        SYS.FwdAnaBuf[i].SetVal(Ar, Ai);
        SYS.MicEnergy[i] = sat48(Ar*Ar + Ai*Ai);
    }
    
}


void SYS_HEAR_WolaRevAnalysis()
{
int24_t i;
int24_t bufp, dlyp;     // Buffer and Delay pointers
frac24_t Ar, Ai;

    bufp = SYS.RevBufPtr;       // buffer pointer; where to put samples into RevDelayBuf
    dlyp = (bufp - FBC_Params.Persist.BulkDelay) & MAX_REV_DLY_MASK;    // Delay pointer into buffer; where to get samples from RevDelayBuf to put into RevAnaIn
    for (i = 0; i < BLOCK_SIZE; i++)
    {
        SYS.RevDelayBuf[bufp] = SYS.OutBuf[i];      // Get latest samples from OutBuf, put into delay buffer
        SYS.RevAnaIn[i] = SYS.RevDelayBuf[dlyp];    // Get delayed sample from RevDelayBuf, copy to analysis input
        bufp = (bufp + 1) & MAX_REV_DLY_MASK;       // increment buffer pointer, modulo buffer size
        dlyp = (dlyp + 1) & MAX_REV_DLY_MASK;       // increment delay pointer, modulo buffer size
    }
    SYS.RevBufPtr = bufp;     // update pointer

    // Adjust circular pointer into reverse analysis buffer
    dlyp = (SYS.RevAnaPtr+1) & FBC_REV_ANA_SIZE_MASK;   // Point to newest value (overwrite of oldest value)
    SYS.RevAnaPtr = dlyp;     

    WOLA_Analyze(&SYS.RevWOLA, SYS.RevAnaIn, SYS.RevAnaBuf[dlyp]);      // Ignoring return value

    if (WOLA_STACKING == WOLA_STACKING_EVEN)
        SYS.RevAnaBuf[dlyp][0].SetImag(0.0);        // Clear out Nyquist frequency to simplify things for Even stacking

    // Scale data into buffer, calculate energy
    for (i = 0; i < WOLA_NUM_BINS; i++)
    {
        Ar = SYS.RevAnaBuf[dlyp][i].Real();     Ai = SYS.RevAnaBuf[dlyp][i].Imag();
        Ar *= WOLA_ANA_POSTSCALE;               Ai *= WOLA_ANA_POSTSCALE;       // Emulate block floating point
        SYS.RevAnaBuf[dlyp][i].SetVal(Ar, Ai);
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
        SYS.BinEnergyLog2[i] = shr(log2_approx(SYS.BinEnergy[i]),1);    // divide by 2 to account for being squared
    }

}


void SYS_HEAR_ApplySubbandGain()
{
frac16_t BinGainLog2;
int24_t i;

    for (i = 0; i < WOLA_NUM_BINS; i++)
    {
        BinGainLog2 = WDRC.BinGainLog2[i] + NR.BinGainLog2[i];
        SYS.DynamicGainLog2[i] = BinGainLog2;    // for use in FBC mu mod by gain
        BinGainLog2 += EQ_Params.Profile.BinGain[i] + WOLA_FILTBANK_GAIN_LOG2;
        BinGainLog2 = min16(BinGainLog2, FBC.GainLimLog2[i]);
        SYS.LimitedFwdGain[i] = BinGainLog2;        // For debugging
        SYS.FwdSynBuf[i].SetReal(rnd_sat24(mult_log2(SYS.Error[i].Real(), BinGainLog2)));
        SYS.FwdSynBuf[i].SetImag(rnd_sat24(mult_log2(SYS.Error[i].Imag(), BinGainLog2)));
    }
}


void SYS_HEAR_WolaFwdSynthesis()
{
uint16_t i;
frac24_t Ar, Ai;

    for (i = 0; i < WOLA_NUM_BINS; i++)
    {
        Ar = SYS.FwdSynBuf[i].Real();   Ai = SYS.FwdSynBuf[i].Imag();
        Ar *= WOLA_SYN_PRESCALE;        Ai *= WOLA_SYN_PRESCALE;        // Restore block floating point
        SYS.FwdSynBuf[i].SetVal(Ar, Ai);
    }
    WOLA_Synthesize(&SYS.FwdWOLA, SYS.FwdSynBuf, SYS.FwdSynOut);
}


void SYS_FENG_AgcO()
{
int24_t i;
frac16_t BbGainLog2;
frac24_t TC;
frac16_t Diff;
frac16_t LevelLog2;
frac16_t ThreshDiff;

    for (i = 0; i < BLOCK_SIZE; i++)
    {
        BbGainLog2 = SYS_Params.Profile.VCGain + EQ_Params.Profile.BroadbandGain + SYS_Params.Profile.AgcoGain;     // Combine all broadband gains
        LevelLog2 = log2_approx(abs_f24(SYS.FwdSynOut[i]));
        Diff = LevelLog2 - SYS.AgcoLevelLog2;
        TC = (Diff > 0) ? AGCO_ATK_TC : AGCO_REL_TC;
        SYS.AgcoLevelLog2 = rnd_sat24(TC*Diff) + SYS.AgcoLevelLog2;
        ThreshDiff = SYS_Params.Persist.AgcoThresh - SYS.AgcoLevelLog2;
        if (BbGainLog2 > ThreshDiff)  // if ((SYS.AgcoLevelLog2+BbGainLog2) > SYS_Params.Persist.AgcoThresh); if proposed level exceeds threshold
        // If the result of applying all the gains is going to exceed threshold
        // then apply as much as possible = the amount of gain that will take level to thresh
            SYS.AgcoGainLog2 = ThreshDiff;
        else
            SYS.AgcoGainLog2 = BbGainLog2;      // Otherwise apply all the gain possible
        SYS.OutBuf[i] = mult_log2(SYS.FwdSynOut[i], SYS.AgcoGainLog2);
        SYS.OutBuf[i] = rnd_sat24(mult_log2(SYS.OutBuf[i], SYS_Params.Persist.OutpRcvrGain));      // Apply receiver calibration separately
    }
}
