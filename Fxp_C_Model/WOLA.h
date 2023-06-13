//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
// WOLA header file for fixed-point C code
//
// Novidan, Inc. (c) 2023.  May not be used or copied without prior consent.
//
// Bryant Sorensen, author
// Started 07 Jun 2023
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

#ifndef _WOLA_H
#define _WOLA_H

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Defines

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Module structure
struct strWOLA
{
    frac24_t    AnaBuf[WOLA_LA];
    frac24_t    AnaWinBuf[WOLA_LA];
    Complex24   BitRevBuf[WOLA_N];
    Complex24   FFTBuf[WOLA_N];
    frac24_t    SynWinBuf[WOLA_LS];
    frac24_t    SynOlaBuf[WOLA_LS];     // Overlap-add buffer
    frac24_t*   AnaWindow;
    frac24_t*   SynWindow;
    int8_t      Stacking;
    uint8_t     AnaBlockCnt;
    uint8_t     SynBlockCnt;
    frac24_t    AnaSign;
    frac24_t    SynSign;

    strWOLA()
    {
    int16_t i;
        for (i = 0; i < WOLA_LA; i++)
        {
            AnaBuf[i] = to_frac24(0.0);
            AnaWinBuf[i] = to_frac24(0.0);
            AnaWindow = NULL;
        }

        for (i = 0; i < WOLA_N; i++)
        {
            FFTBuf[i].SetVal(to_frac24(0.0), to_frac24(0.0));
            BitRevBuf[i].SetVal(to_frac24(0.0), to_frac24(0.0));
        }

        for (i = 0; i < WOLA_LS; i++)
        {
            SynWinBuf[i] = to_frac24(0.0);
            SynOlaBuf[i] = to_frac24(0.0);
            SynWindow = NULL;
        }
        Stacking = -1;      // Init with illegal value
        AnaBlockCnt = 0;
        SynBlockCnt = 0;
        AnaSign = 1.0;
        SynSign = -1.0;
    };
    ~strWOLA() {};
};

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Function and variable prototypes

void WOLA_Init(strWOLA* sWOLA, int8_t StackingSel, const frac24_t* AnalysisWin, const frac24_t* SynthesisWin);
void WOLA_Analyze(strWOLA* sWOLA, frac24_t* AnaIn, Complex24* AnaOut);
void WOLA_Synthesize(strWOLA* sWOLA, Complex24* SynIn, frac24_t* SynOut);

extern const frac24_t AnalysisWin[];
extern const frac24_t SynthesisWin[];

#endif  // _WOLA_H

