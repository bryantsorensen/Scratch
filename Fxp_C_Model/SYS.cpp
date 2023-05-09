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

    // SIM ONLY! Normally done by compile-time config for HEAR code
    SYS_SimWolaInit();

    for (i = 0; i < BLOCK_SIZE; i++)
    {
        SYS.FwdAnaIn[i] = to_frac24(0);
        SYS.FwdSynOut[i] = to_frac24(0);
        SYS.OutBuf[i] = to_frac24(0);
        SYS.RevAnaIn[i] =to_frac24(0);
    }

    for (i = 0; i < WOLA_NUM_BINS; i++)
    {
        SYS.FwdAnaBuf[i] = to_frac24(0);    // Complex, both parts set to 0
        SYS.Error[i] = to_frac24(0);        // Complex, both parts set to 0
        SYS.BinEnergy[i] = to_frac48(0);
        SYS.FwdGainLog2[i] = to_frac16(0);  // Unity gain
        SYS.FwdSynBuf[i] = to_frac24(0);    // Complex, both parts set to 0

        SYS.RevAnaBuf[i] = to_frac24(0);
    }

    SYS.AgcoLevelLog2 = to_frac16(-40.0);
    SYS.AgcoGainLog2 = to_frac16(0.0);

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

    WolaAnalyze(WOLASim.FwdWolaHandle, SYS.FwdAnaIn, AnaOut);     // Ignoring return value

    if (WOLA_STACKING == WOLA_STACKING_EVEN)
        AnaOut[0].i = 0.0;                  // Clear out Nyquist frequency to simplify things for Even stacking

    for (i = 0; i < WOLA_NUM_BINS; i++)
    {
        SYS.FwdAnaBuf[i].SetVal(to_frac24(AnaOut[i].r), to_frac24(AnaOut[i].i));  // In case we need conversion, WolaComplex --> Complex24, for sim
    }   
    
}


void SYS_HEAR_WolaRevAnalysis()
{
WolaComplex AnaOut[WOLA_N];
int24_t i;

    WolaAnalyze(WOLASim.RevWolaHandle, SYS.RevAnaIn, AnaOut);     // Ignoring return value

    if (WOLA_STACKING == WOLA_STACKING_EVEN)
        AnaOut[0].i = 0.0;                  // Clear out Nyquist frequency to simplify things for Even stacking

    for (i = 0; i < WOLA_NUM_BINS; i++)
    {
        SYS.RevAnaBuf[i].SetVal(to_frac24(AnaOut[i].r), to_frac24(AnaOut[i].i));  // In case we need conversion, WolaComplex --> Complex24, for sim
    }   
    
}

void SYS_HEAR_ErrorSubAndEnergy(Complex24* FBC_FilterOut)
{
int24_t i;

    for (i = 0; i < WOLA_NUM_BINS; i++)
    {
        SYS.Error[i] = SYS.FwdAnaBuf[i] - FBC_FilterOut[i];
        SYS.BinEnergy[i] = sat48((SYS.Error[i].Real()*SYS.Error[i].Real()) + (SYS.Error[i].Imag()*SYS.Error[i].Imag()));
    }

}


void SYS_HEAR_ApplySubbandGain()
{
frac16_t BinGainLog2;
int24_t i;

    for (i = 0; i < WOLA_NUM_BINS; i++)
    {
        BinGainLog2 = 0.0;  // EQ.BinGainLog2[i] + WDRC.BinGainLog2[i] + NR.BinGainLog2[i] + FilterbankGainLog2
        //BinGainLog2 = min(BinGainLog2, FBC.GainLimitLog2[i]);
        SYS.FwdSynBuf[i].SetReal(rnd_sat24(mult_log2(SYS.Error[i].Real(), BinGainLog2)));
        SYS.FwdSynBuf[i].SetImag(rnd_sat24(mult_log2(SYS.Error[i].Imag(), BinGainLog2)));
        SYS.FwdGainLog2[i] = BinGainLog2;   // For debugging
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