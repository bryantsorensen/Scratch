//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
// Feedback Canceller (FBC) for fixed-point C code
//
// Novidan, Inc. (c) 2023.  May not be used or copied without prior consent.
//
// Bryant Sorensen, author
// Started 11 May 2023
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

#include "Common.h"


void FBC_Init()
{
int24_t CurCh;
int24_t bin;
int24_t i;
frac16_t WdrcMaxGainLog2;
frac16_t BbTargetGainLog2;
frac16_t TargetGainBinLog2;

/*Gains in forward path, per bin:
    WDRC    EQ      NR      filterbank_gain
Broadband gains:
    MicCal  RcvrCal     VC      EQbb    AGCo

Of these, only the following are dynamic:
    WDRC(bin)    NR(bin)    AGCo(Bb)    Any kind of fade-in/fade-out(Bb)

Not sure we should consider VC; assume we put it in the target as high, and later it gets turned down; then 
we would slow down adaptation all the time.  Also, if we use it in target as unity, then it gets turned up,
then we could miss gains that are turned down as it swamps their negative gain (is this a bad thing?)
*/
    BbTargetGainLog2 = SYS_Params.Persist.InpMicGain + SYS_Params.Profile.AgcoGain;
    for (CurCh = 0; CurCh < WDRC_NUM_CHANNELS; CurCh++)
    {
    // Don't include NR; it is unity by default at reset or rest

        WdrcMaxGainLog2 = max16(WDRC.Gain[CurCh][1], WDRC.Gain[CurCh][2]);
        WdrcMaxGainLog2 = max16(WdrcMaxGainLog2, WDRC.Gain[CurCh][3]);
        TargetGainBinLog2 = BbTargetGainLog2 + WdrcMaxGainLog2;
    // Now distribute gain across all bins in that channel
        for (bin = WDRC.ChannelStartBin[CurCh]; bin <= WDRC.ChannelLastBin[CurCh]; bin++)
            FBC.TargetGainLog2[bin] = TargetGainBinLog2;
    }

    FBC.StartBin = FBC_START_BIN;
    FBC.EndBin = (FBC_START_BIN + FBC_BINS_PER_CALL - 1);
    FBC.EndBin = (FBC.EndBin > FBC_END_BIN) ? FBC_END_BIN : FBC.EndBin;

    for (bin = FBC_START_BIN; bin <= FBC_END_BIN; bin++)
    {
        FBC.AdaptShift[bin] = FBC_Params.Profile.ActiveShift;
        FBC.GainLimLog2[bin] = to_frac16(0);        // Set to unity (max)
    }

    // Zero out coeffs and fed-back output analysis buffer
    for (bin = 0; bin < WOLA_NUM_BINS; bin++)
    {
        for (i = 0; i < FBC_COEFFS_PER_BIN; i++)
            FBC.Coeffs[bin][i].SetVal(to_frac24(0), to_frac24(0));
        FBC.CoefMag[bin] = to_frac16(-23.0);
    }

    // Create intermediate leakage average between fast and slow leakage; 
    // if these are close, could wind up being equal to one or the other (which is OK)
    FBC.IntermLeak = (FBC_Params.Persist.LeakFast + FBC_Params.Persist.LeakSlow) >> 1;  

    SYS.RevDlyPtr = FBC_Params.Persist.BulkDelay;       // NON-STANDARD - init SYS delay with FBC param

    FBC.Sinusoid[0].SetVal(to_frac24(0x7FFFFF), to_frac24(0));                          // y[n-2] - used for multiplication
    FBC.Sinusoid[1].SetVal(FBC_Params.Profile.CosInit, FBC_Params.Profile.SineInit);    // y[n-1]
}


void FBC_HEAR_Levels()
{
int24_t i;

// A energy: SYS.MicEnergy
// E energy already calculated:  SYS.BinEnergy
// B energy: SYS.RevEnergy (output signal, fed back & analysis taken)

    for (i = 0; i < WOLA_NUM_BINS; i++)
    {
    // Create B + E
        FBC.BEEnergy[i] = SYS.BinEnergy[i] + SYS.RevEnergy[i];      // Add in the linear domain
        FBC.ASmoothed[i] = dualTC_Smooth_48(SYS.MicEnergy[i], FBC.ASmoothed[i], FBC_LEVEL_ATK_SHIFT, FBC_LEVEL_REL_SHIFT);
        FBC.ESmoothed[i] = dualTC_Smooth_48(SYS.BinEnergy[i], FBC.ESmoothed[i], FBC_LEVEL_ATK_SHIFT, FBC_LEVEL_REL_SHIFT);
        FBC.BESmoothed[i] = dualTC_Smooth_48(FBC.BEEnergy[i], FBC.BESmoothed[i], FBC_LEVEL_ATK_SHIFT, FBC_LEVEL_REL_SHIFT);
    }
}


void FBC_HEAR_DoFiltering()
{
int24_t bin;
int24_t tap;
int24_t BufDly;
accum_t Ar, Ai;
frac24_t Sr, Si;
frac24_t Cr, Ci;

// TODO: Should I apply scaling factor to FIR products, to give FBC coeffs more room to grow?
// Also have to compensate when calculating FBC MaxGain

    for (bin = 0; bin < WOLA_NUM_BINS; bin++)
    {
        Ar = to_accum(0);   Ai = to_accum(0);
        for (tap = 0, BufDly = 0; tap < FBC_COEFFS_PER_BIN; tap++)
        {
            Sr = SYS.RevAnaBuf[bin][BufDly].Real(); Si = SYS.RevAnaBuf[bin][BufDly].Imag();
            BufDly += FBC_COEFF_SPACING;
            Cr = FBC.Coeffs[bin][tap].Real();       Ci = FBC.Coeffs[bin][tap].Imag();
            Ar += Sr * Cr;      Ai += Sr * Ci;
            Ar -= Si * Ci;      Ai += Si * Cr;
        }
        FBC.FiltSig[bin].SetVal(rnd_sat24(Ar), rnd_sat24(Ai));
    }
}


void FBC_FilterAdaptation()
{
int24_t bin;
int24_t cf;
int24_t GainMuAdj;
frac48_t ASmoothShifted;
int24_t LeakSh;
frac16_t DynBinGainLog2;
int24_t MuShift;
frac24_t Er, Ei;
accum_t Ar, Ai;
frac24_t Br, Bi;
accum_t Cr, Ci;
frac24_t Sr, Si;
frac24_t Tr, Ti;
int24_t BufDly;     // Delay into output analysis buffer (treats buffer as FIFO, with newest value at offset 0)

// TODO: Put in simulation code to emulate action of FIFOs
/*
Emulate the FIFO
SYS.RevAnaBuf[bin][0] = newest analysis result
SYS.RevAnaBuf[bin][1] = second-to-newest analysis result
...
SYS.RevAnaBuf[bin][K-1] = oldest analysis result
*/
    if (FBC_Params.Profile.Enable)
    {
        for (bin = FBC.StartBin; bin <= FBC.EndBin; bin++)
        {
        // Determine MuShift, based on log2(||B+E||^2), per-bin offset, gain below target, and mu parameter

        // TODO: Add in NR gain
            DynBinGainLog2 = SYS.AgcoGainLog2 + SYS.MicCalGainLog2 + WDRC.BinGainLog2[bin]; // + NR.BinGainLog2[bin];
            GainMuAdj = (int24_t)shr((FBC.TargetGainLog2[bin] - DynBinGainLog2), 15);    // T - D < 0 --> D > T --> no adjust; T - D > 0 --> D < T --> adjust accordingly
                                                            // Shift by 15 so that every 0.5 of diff in log2 (3dB) adjusts MuShift by 1 count
            GainMuAdj = (GainMuAdj < 0) ? 0 : GainMuAdj;    // Only take positive results
            GainMuAdj = (GainMuAdj > MAX_GAIN_MU_ADJ) ? MAX_GAIN_MU_ADJ : GainMuAdj;    // limit at top

        // TODO: Do we need to add bias to BESmoothed, before taking log2abs? If so, can this be a fixed value?
            MuShift = FBC_Params.Profile.ActiveShift + GainMuAdj + FBC_Params.Persist.MuOffset[bin] + log2abs(FBC.BESmoothed[bin]);
            FBC.AdaptShift[bin] = MuShift;      // Save for debugging

        // Determine LeakSh based on energies at different points
        // If Esmooth > 4*Asmooth --> use fast leak
        // else if Esmooth > 2*Asmooth --> use an intermediate leakage
        // else use slow leakage

            LeakSh = FBC_Params.Persist.LeakSlow;           // Default leakage
            ASmoothShifted = shl(FBC.ASmoothed[bin], 2);
            if (FBC.ESmoothed[bin] >= ASmoothShifted)
                LeakSh = FBC_Params.Persist.LeakFast;
            else 
            {
                ASmoothShifted = shr(ASmoothShifted, 1);   // 2*Asmooth
                if (FBC.ESmoothed[bin] >= ASmoothShifted)
                    LeakSh = FBC.IntermLeak;
            }

        // Update coefficients using nMLS equation, with leakage on the previous coefficients; for each bin:
        // C[k][n] = C[k][n-1]*Leakage + mu*conj(E[n])*B[n-k], where mu is normalization shift created as shown above
        // k is the coefficient index, n is time index (the bin index is not shown)
        // Also sum up the coefficients in the complex domain to get the response in the center of the bin

            Er = SYS.Error[bin].Real();     Ei = SYS.Error[bin].Imag();     // get current error signal
            Sr = to_frac24(0);              Si = to_frac24(0);              // Clear out coeff sum
            for (cf = 0, BufDly = 0; cf < FBC_COEFFS_PER_BIN; cf++)
            {
            // FBC.Coeffs[c][n] = FBC.Coeffs[c][n-1]*Leak + (conj(Err[n])*B[-c])>>norm_and_mu_shift)
            // (Br + j*Bi)*(Er - j*Ei) = (Br*Er + Bi*Ei) + j*(Bi*Er - Br*Ei)
                Br = SYS.RevAnaBuf[bin][BufDly].Real();  Bi = SYS.RevAnaBuf[bin][BufDly].Imag();  BufDly += FBC_COEFF_SPACING;
                Ar  = Br * Er;              Ai  = Bi * Er;
                Ar += Bi * Ei;              Ai -= Br * Ei;
                Ar = shs(Ar, MuShift);      Ai = shs(Ai, MuShift);
                Cr = FBC.Coeffs[bin][cf].Real();        Ci = FBC.Coeffs[bin][cf].Imag();
                Ar += Cr;                   Ai += Ci;
                Ar -= shr(Cr, LeakSh);      Ai -= shr(Ci, LeakSh);
                Tr = rnd_sat24(Ar);         Ti = rnd_sat24(Ai);     // Back to single precision
                Sr += Tr;                   Si += Ti;               // Sum the new coefficients in this bin
                FBC.Coeffs[cf][bin].SetVal(Tr, Ti);                  // Update the coefficients
            }

        // Estimate FB magnitude in each bin by taking log2(sum(coeffs)) in the bin
            Ar = Sr*Sr + Si*Si;
            FBC.CoefMag[bin] = shr(log2_approx(Ar), 1);        // Divide by 2 to account for it being squared magnitude in linear

        // Update FBC maximum gain limit, based on CoefMag and current gain
            if (FBC_Params.Profile.GainLimitEnable)
                FBC.GainLimLog2[bin] = FBC_Params.Profile.GainLimitMax - FBC.CoefMag[bin] 
                    -(SYS.AgcoGainLog2 + SYS.MicCalGainLog2 + SYS_Params.Persist.OutpRcvrGain + EQ_Params.Profile.BroadbandGain + SYS_Params.Profile.VCGain) 
                    - WDRC_RESERVE_GAIN;
            else
                FBC.GainLimLog2[bin] = to_frac16(0);    // Max value
        }

    // Determine next set of bins to adapt
        if (FBC.EndBin == FBC_END_BIN)
            FBC.StartBin = FBC_START_BIN;
        else
            FBC.StartBin = FBC.EndBin + 1;
        FBC.EndBin = FBC.StartBin + FBC_BINS_PER_CALL;
        FBC.EndBin = (FBC.EndBin > FBC_END_BIN) ? FBC_END_BIN : FBC.EndBin;

    }
    else
    {   // if FBC is disabled, set GainLim to max value (unity gain)
        for (bin = 0; bin < WOLA_NUM_BINS; bin++)
        {
            FBC.GainLimLog2[bin] = to_frac16(0);
            for (cf = 0; cf < FBC_COEFFS_PER_BIN; cf++)
                FBC.Coeffs[bin][cf].SetVal(to_frac24(0), to_frac24(0));
        }
    }
}


/*
Sine oscillator:
    a = 2.0*cos(2.0*pi*freq/fsamp);
    s[n-1] = ampl*sin(2.0*pi*freq/fsamp);
    s[n-2] = 0
    s[n] = a*s[n-1] - s[n-2]

Cosine oscillator:
    b = 2.0*cos(2.0*pi*freq/fsamp) (= a, same as above)
    c[n-1] = ampl*cos(2.0*pi*freq/fsamp);    % Change this to 2nd value of cosine (after initial value of ampl*1.0 = ampl*cos(0))
    c[n-2] = ampl
    c[n] = a*c[n-1] - c[n-2]

Complex sinusoid:  c[n] + j*s[n]

Disable sinusoid by setting frequency = 0; then c = 1.0 and s = 0.0 always
*/
void FBC_DoFreqShift()
{
int24_t i;
accum_t Ar, Ai;
frac24_t Sr, Si;
frac24_t Dr, Di;
frac24_t ResCoef;       // Resonance coefficient for mults

    if (FBC_Params.Profile.Enable)
    {
    // Multiply synthesis input by complex sinusoid to perform frequency shift

        for (i = FBC_Params.Profile.FreqShStartBin; i <= FBC_Params.Profile.FreqShEndBin; i++)
        {
            Sr = FBC.Sinusoid[0].Real();    Si = FBC.Sinusoid[0].Imag();    // Get y[n-2], complex
            Dr = SYS.FwdSynBuf[i].Real();   Di = SYS.FwdSynBuf[i].Imag();

            Ar  = Sr * Dr;      Ai  = Sr * Di;
            Ar -= Si * Di;      Ai += Si * Dr;

            SYS.FwdSynBuf[i].SetVal(rnd_sat24(Ar), rnd_sat24(Ai));
        }

    // Create next complex sinusoid sample

        ResCoef = FBC_Params.Profile.CosInit;       // This value is 1/2 the resonator coefficient
        Sr = FBC.Sinusoid[1].Real();    Si = FBC.Sinusoid[1].Imag();    // Get y[n-1], complex
        Ar = Sr * ResCoef;              Ai = Si * ResCoef;              // Mult a*y[n-1]
        Ar = shl(Ar, 1);                Ai = shl(Ai, 1);                // Account for coeff being on [-2.0, 2.0)
        Ar -= FBC.Sinusoid[0].Real();   Ai -= FBC.Sinusoid[0].Imag();   // Subtract y[n-2]
        FBC.Sinusoid[0].SetVal(Sr, Si);                                 // Move y[n-1] --> y[n-2]
        FBC.Sinusoid[1].SetVal(rnd_sat24(Ar), rnd_sat24(Ai));           // Update y[n-1] with latest result
    }
}
