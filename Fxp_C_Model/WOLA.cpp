//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
// WOLA source code for fixed-point C code
//
// Novidan, Inc. (c) 2023, except as noted below.  
// Copyright portions may not be used or copied without prior consent.
//
// Bryant Sorensen, author
// Started 07 Jun 2023
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

#include "Common.h"

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Global variables used
// Declare WOLA window arrays, AnalysisWin and SynthesisWin
// Declare bit reverse table

#include "WolaWins.h"
#include "BitRevTable.h"        // Table of bit-reversed addresses


//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Functions

// Re-order the input buffer into the output buffer using bit-reversed addressing
static inline void BitReverseBuffer(Complex24* inBuf, Complex24* outBuf, int16_t Log2N)
{
int16_t N = (1 << Log2N);
int16_t i;
int16_t Shift = WOLA_MAX_SIZE_LOG2 - Log2N;

    for (i = 0; i < N; i++)
    {
        outBuf[i] = inBuf[BitRevTable[i] >> Shift];
    }
}


static Complex24 TwidTable(int16_t idx, int16_t N, bool Inv)
{
frac24_t R, I;
Complex24 Ret;

    R =  cos(2.0*M_PI*(double)idx/(double)N);
    I = -sin(2.0*M_PI*(double)idx/(double)N);       // Forward FFT is negative, inverse is positive
    if (Inv) 
        I = -I;
    Ret.SetVal(R, I);
    return Ret;
}

// The original FFT code was found at http://www.strauss-acoustics.ch/libdsp.html 
// by Bryant Sorensen (BES) 31Jul12.
// Modified for use in WOLA code for Novidan, Inc.  No copyright implied

//+++++++++++++++++++++++++
// R2FFTdif : Radix 2, in place, complex in & out, decimation in frequency FFT algorithm.
// 
// sFFT is both input and output complex buffer
// iLog2N = log2(size_of_FFT)
// Inv = false for forward FFT, true for inverse FFT

void R2FFTdif (Complex24* sFFT, int16_t iLog2N, bool Inv)
{
int16_t iN;
int16_t iCnt1, iCnt2, iCnt3;
int16_t iQ,    iL,    iM;
int16_t iA,    iB;
double fRealTemp, fImagTemp;
Complex24 Wq;

    iN = 1 << iLog2N;
    iL = 1;
    iM = iN >> 1;     // iN/2

    for (iCnt1 = 0; iCnt1 < iLog2N; ++iCnt1)
    {
        iQ = 0;
        for (iCnt2 = 0; iCnt2 < iM; ++iCnt2)
        {
            iA = iCnt2;
            Wq = TwidTable(iQ, iN, Inv);

            for (iCnt3 = 0; iCnt3 < iL; ++iCnt3)
            {
                iB = iA + iM;

                /* Butterfly: 10 FOP, 4 FMUL, 6 FADD */

                fRealTemp      = sFFT[iA].Real() - sFFT[iB].Real();
                sFFT[iA].SetReal(sFFT[iA].Real() + sFFT[iB].Real());
                fImagTemp      = sFFT[iA].Imag() - sFFT[iB].Imag();
                sFFT[iA].SetImag(sFFT[iA].Imag() + sFFT[iB].Imag());

                sFFT[iB].SetReal(fRealTemp * Wq.Real() - fImagTemp * Wq.Imag());
                sFFT[iB].SetImag(fImagTemp * Wq.Real() + fRealTemp * Wq.Imag());

                iA += (iM<<1);  // iA + 2*iM;
            }
            iQ += iL;
        }
        iL <<= 1;   // *= 2;
        iM >>= 1;   // /= 2;
    }
}


//+++++++++++++++++++++++++
// R2FFTdit : Radix 2, in place, complex in & out, decimation in time FFT algorithm.
// 
// sFFT is both input and output complex buffer
// iLog2N = log2(size_of_FFT)
// Inv = false for forward FFT, true for inverse FFT

void R2FFTdit (Complex24* sFFT, int16_t iLog2N, bool Inv)
{
int16_t iN;
int16_t iCnt1, iCnt2,iCnt3;
int16_t iQ,    iL,   iM;
int16_t iA,    iB;
double fRealTemp, fImagTemp;
Complex24 Wq;

    iN = 1 << iLog2N;
    iL = iN >> 1;   // iN/2
    iM = 1;

    for (iCnt1 = 0; iCnt1 < iLog2N; ++iCnt1)
    {
        iQ = 0;
        for (iCnt2 = 0; iCnt2 < iM; ++iCnt2)
        {
            iA = iCnt2;
            Wq = TwidTable(iQ, iN, Inv);

            for (iCnt3 = 0; iCnt3 < iL; ++iCnt3)
            {
                iB = iA + iM;

                /* Butterfly: 10 FOP, 4 FMUL, 6 FADD */

                fRealTemp = sFFT[iB].Real() * Wq.Real() - sFFT[iB].Imag() * Wq.Imag();
                fImagTemp = sFFT[iB].Real() * Wq.Imag() + sFFT[iB].Imag() * Wq.Real();
                // Do these in order to allow in-place calcs
                sFFT[iB].SetReal(sFFT[iA].Real() - fRealTemp);
                sFFT[iA].SetReal(sFFT[iA].Real() + fRealTemp);
                sFFT[iB].SetImag(sFFT[iA].Imag() - fImagTemp);
                sFFT[iA].SetImag(sFFT[iA].Imag() + fImagTemp);

                iA += (iM<<1);  // iA + 2*iM;
            }
            iQ += iL;
        }
        iL >>= 1;   // /= 2;
        iM <<= 1;   // *= 2;
    }
}


//+++++++++++++++++++++++++
// WOLA routines

void WOLA_Init(strWOLA* sWOLA, int8_t StackingSel, const frac24_t* AnalysisWin, const frac24_t* SynthesisWin)
{
    sWOLA->Stacking = StackingSel;
    sWOLA->AnaWindow = (frac24_t*)AnalysisWin;
    sWOLA->SynWindow = (frac24_t*)SynthesisWin;
}


// Analysis: bring in WOLA_R samples (AnaIn), buffer inside the WOLA structure (hidden memory), perform WOLA
// processing, produce WOLA_NUM_BINS complex samples out (AnaOut)

void WOLA_Analyze(strWOLA* sWOLA, frac24_t* AnaIn, Complex24* AnaOut)
{
int i;
int j;
int TimeBlocks = (WOLA_N/WOLA_LA);      // Required this be integer ratio
uint16_t bra;    // Bit reversed address

    // Simulate movement of the buffer; don't worry about being efficient, this is simulation of what happens in HEAR
    // Sample 0 --> 8
    // Sample 8 --> 16 etc.
    // new samples: 0-7

// TODO: Add sign sequencing for odd stacking

    for (i = (WOLA_LA-1); i >= WOLA_R; i--)
        sWOLA->AnaBuf[i] = sWOLA->AnaBuf[i-WOLA_R];     // Shift samples up in buffer
    for (i = (WOLA_R-1); i >= 0; i--)
        sWOLA->AnaBuf[i] = AnaIn[i];

    // Do windowing
    for (i = 0; i < WOLA_LA; i++)
        sWOLA->AnaWinBuf[i] = sWOLA->AnaBuf[i] * sWOLA->AnaWindow[i];

    // Time folding
    for (i = 0; i < WOLA_N; i++)
    {
        for (j = 1; j < TimeBlocks; j++)
            sWOLA->AnaWinBuf[i] += sWOLA->AnaWinBuf[j*WOLA_N+i];
    }

    // Circular shift by WOLA_R samples (again, emulated w/o efficiency)
    // Also do bit reversal addressing at the same time
    for (i = 0; i < WOLA_N; i++)
    {
        bra = BitRevTable[i] >> BITREV_SHIFT;
        if (WOLA_STACKING == WOLA_STACKING_EVEN)
            sWOLA->FFTBuf[bra].SetVal(sWOLA->AnaWinBuf[(i+WOLA_R)&(WOLA_N-1)], to_frac24(0.0));
        else
        {
// TODO: Add frequency shift for odd stacking
        }
    }

    // Take forward FFT
    R2FFTdit(sWOLA->FFTBuf, WOLA_LOG2_N, false);

    // Copy only the 1st half of (what should be) symmetric output
    for (i = 0; i < WOLA_NUM_BINS; i++)
        AnaOut[i] = sWOLA->FFTBuf[i];
    AnaOut[0].SetImag(sWOLA->FFTBuf[WOLA_NUM_BINS].Real());     // Copy Nyquist to imag part of [0]
}


void WOLA_Synthesize(strWOLA* sWOLA, Complex24* SynIn, frac24_t* SynOut)
{
int16_t i;
uint16_t bra;
uint16_t j;
int TimeBlocks = (WOLA_N/WOLA_LS);      // Required this be integer ratio

    // Copy input data to work buffer
    // Separate DC and Nyquist from bin[0] of input to their respective bins
    sWOLA->FFTBuf[0].SetVal(SynIn[0].Real(), 0.0);
    sWOLA->FFTBuf[WOLA_NUM_BINS].SetVal(SynIn[0].Imag(), 0.0);
    for (i = 1; i < WOLA_NUM_BINS; i++)
        sWOLA->FFTBuf[i] = SynIn[i];
    for (i = (WOLA_NUM_BINS+1); i < WOLA_N; i++)     // Complete the complex conjugate symmetry
        sWOLA->FFTBuf[i].SetVal(sWOLA->FFTBuf[WOLA_N-i].Real(), -sWOLA->FFTBuf[WOLA_N-i].Imag());

    R2FFTdif(sWOLA->FFTBuf, WOLA_LOG2_N, true);     // Take inverse FFT

// TODO: Add frequency shift for odd stacking

    // Do circular shift, combine with bit reversal; also clear out imag part (which should already be 0)
    for (i = 0; i < WOLA_N; i++)
    {
        bra = BitRevTable[i] >> BITREV_SHIFT;
        if (WOLA_STACKING == WOLA_STACKING_EVEN)
            sWOLA->SynWinBuf[bra] = sWOLA->FFTBuf[(i+WOLA_R)&(WOLA_N-1)].Real();
    }

    // Replicate the block
    for (i = 0; i < WOLA_N; i++)
    {
        for (j = 1; j < TimeBlocks; j++)
            sWOLA->SynWinBuf[i+j*WOLA_N] = sWOLA->SynWinBuf[i];
    }

    // Window
    for (i = 0; i < WOLA_LS; i++)
        sWOLA->SynWinBuf[i] = sWOLA->SynWinBuf[i] * sWOLA->SynWindow[i];

    // Do overlap-add for blocks of size WOLA_R. 
    // Do the shift and add in one step; bringing in zeros to is equivalent to copying windowed data
    for (i = (WOLA_LS-1); i >= WOLA_R; i--)
        sWOLA->SynOlaBuf[i] = sWOLA->SynOlaBuf[i-WOLA_R] + sWOLA->SynWinBuf[i];
    for (i = 0; i < WOLA_R; i++)
        sWOLA->SynOlaBuf[i] = sWOLA->SynWinBuf[i];

    // Capture the oldest samples out of the OLA buffer, for output
    if (WOLA_STACKING == WOLA_STACKING_EVEN)
    {
        for (i = 0; i < WOLA_R; i++)
            SynOut[i] = sWOLA->SynOlaBuf[WOLA_LS-WOLA_R+i];
    }
// TODO: Add sign sequencer for odd stacking
    else
    {
    }
}
