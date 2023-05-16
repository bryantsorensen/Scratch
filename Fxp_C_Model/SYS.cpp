//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
// System processing module for fixed-point C code
//
// Novidan, Inc. (c) 2023.  May not be used or copied with prior consent.
//
// Bryant Sorensen, author
// Started 09 May 2023
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

#include "Common.h"

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Simulation convention - WOLA values

static strWOLA WOLASim;
//frac24_t WolaAnaWin[WOLA_LA];
//frac24_t WolaSynWin[WOLA_LS];


static void SYS_SimWolaInit()
{
    WOLASim.FwdWolaHandle = WolaInit(WOLA_LA, WOLA_LS, WOLA_R, WOLA_N, WOLA_STACKING);
//    WolaSetAnaWin(WOLASim.FwdWolaHandle, WolaAnaWin, WOLA_N);     // Comment out to leave default window
//    WolaSetSynWin(WOLASim.FwdWolaHandle, WolaSynWin, WOLA_N);

    WOLASim.RevWolaHandle = WolaInit(WOLA_LA, WOLA_LS, WOLA_R, WOLA_N, WOLA_STACKING);
//    WolaSetAnaWin(WOLASim.RevWolaHandle, WolaAnaWin, WOLA_N);

}


void SYS_Init()
{
int24_t i;
int24_t tap;

    // SIM ONLY! Normally done by compile-time config for HEAR code
    SYS_SimWolaInit();

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
}



void SYS_FENG_ApplyInputGain(frac24_t* InBuf)
{
int24_t i;

    for (i = 0; i < BLOCK_SIZE; i++)
    {
        SYS.FwdAnaIn[i] = mult_log2(InBuf[i], SYS_Params.Persist.InpMicGain);
    }
}


void SYS_HEAR_WolaFwdAnalysis()
{
WolaComplex AnaOut[WOLA_N];
int24_t i;
frac24_t Ar, Ai;

    WolaAnalyze(WOLASim.FwdWolaHandle, SYS.FwdAnaIn, AnaOut);     // Ignoring return value

    if (WOLA_STACKING == WOLA_STACKING_EVEN)
        AnaOut[0].i = 0.0;                  // Clear out Nyquist frequency to simplify things for Even stacking

    for (i = 0; i < WOLA_NUM_BINS; i++)
    {
        Ar = to_frac24(AnaOut[i].r);    Ai = to_frac24(AnaOut[i].i);
        SYS.MicEnergy[i] = sat48(Ar*Ar + Ai*Ai);
        SYS.FwdAnaBuf[i].SetVal(Ar, Ai);  // In case we need conversion, WolaComplex --> Complex24, for sim
    }   
    
}


void SYS_HEAR_WolaRevAnalysis()
{
WolaComplex AnaOut[WOLA_N];
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

    WolaAnalyze(WOLASim.RevWolaHandle, SYS.RevAnaIn, AnaOut);     // Ignoring return value

    if (WOLA_STACKING == WOLA_STACKING_EVEN)
        AnaOut[0].i = 0.0;                  // Clear out Nyquist frequency to simplify things for Even stacking

// TODO: Correctly emulate analysis buffer
    for (i = 0; i < WOLA_NUM_BINS; i++)
    {
        Ar = to_frac24(AnaOut[i].r);    Ai = to_frac24(AnaOut[i].i);
        SYS.RevAnaBuf[0][i].SetVal(Ar, Ai);    // In case we need conversion, WolaComplex --> Complex24, for sim
        SYS.RevEnergy[i] = sat48(Ar*Ar + Ai*Ai);
    }   
    
}

void SYS_HEAR_ErrorSubAndEnergy(Complex24* FBC_FilterOut)
{
int24_t i;
frac24_t Er, Ei;

    for (i = 0; i < WOLA_NUM_BINS; i++)
    {
        if (FBC_Params.Profile.Enable)
            SYS.Error[i] = SYS.FwdAnaBuf[i] - FBC_FilterOut[i];
        else
            SYS.Error[i] = SYS.FwdAnaBuf[i];
        Er = SYS.Error[i].Real();   Ei = SYS.Error[i].Imag();
        SYS.BinEnergy[i] = sat48(Er*Er + Ei*Ei);
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
    // TODO: Add EQ, filterbank gains
//        BinGainLog2 += EQ.BinGainLog2[i] + FILTERBANK_GAIN_LOG2;
        BinGainLog2 = min16(BinGainLog2, FBC.GainLimLog2[i]);
        SYS.LimitedFwdGain[i] = BinGainLog2;        // For debugging
        SYS.FwdSynBuf[i].SetReal(rnd_sat24(mult_log2(SYS.Error[i].Real(), BinGainLog2)));
        SYS.FwdSynBuf[i].SetImag(rnd_sat24(mult_log2(SYS.Error[i].Imag(), BinGainLog2)));
    }

}


void SYS_HEAR_WolaFwdSynthesis()
{
WolaComplex SynIn[WOLA_N];
int24_t i;

    for (i = 0; i < WOLA_NUM_BINS; i++)     // In case we need conversion, Complex24 --> WolaComplex, for sim
    {
        SynIn[i].r = SYS.FwdSynBuf[i].Real();
        SynIn[i].i = SYS.FwdSynBuf[i].Imag();
    }   
    WolaSynthesize(WOLASim.FwdWolaHandle, SynIn, SYS.FwdSynOut);  // Ignore return value
    
}


void SYS_FENG_AgcO()
{
int24_t i;
frac24_t TC;
frac16_t Diff;
frac16_t LevelLog2;

// TODO: Add VC gain, EQ BB gain

    for (i = 0; i < BLOCK_SIZE; i++)
    {
        LevelLog2 = log2_approx(abs_f24(SYS.FwdSynOut[i]));
        Diff = LevelLog2 - SYS.AgcoLevelLog2;
        TC = (Diff > 0) ? AGCO_ATK_TC : AGCO_REL_TC;
        SYS.AgcoLevelLog2 = rnd_sat24(TC*Diff) + SYS.AgcoLevelLog2;
        if ((SYS.AgcoLevelLog2+SYS_Params.Profile.AgcoGain) > SYS_Params.Persist.AgcoThresh)
            SYS.AgcoGainLog2 = SYS_Params.Persist.AgcoThresh - SYS.AgcoLevelLog2;
        else
            SYS.AgcoGainLog2 = SYS_Params.Profile.AgcoGain;
        SYS.OutBuf[i] = mult_log2(SYS.FwdSynOut[i], SYS.AgcoGainLog2);
    }
}


void SYS_SimCloseWola()
{
    WolaClose(WOLASim.FwdWolaHandle);
    WolaClose(WOLASim.RevWolaHandle);
}