//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
// Common header file for fixed-point C code
//
// Novidan, Inc. (c) 2023.  May not be used or copied without prior consent.
//
// Bryant Sorensen, author
// Started 24 Apr 2023
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

#ifndef _COMMON_H
#define _COMMON_H

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Includes

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#define _USE_MATH_DEFINES
#include <math.h>

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Define data types & helper / conversion functions

typedef int32_t int24_t;
typedef double frac24_t;
typedef double frac16_t;
typedef double accum_t;
typedef double frac48_t;

#define     MAX_VAL24       (0.99999988079071044921875)    // 0x7FFFFF
#define     MIN_VAL24       (-1.0)                         // 0x800000

#define     MAX_VAL16       (127.9999847412109375)         // 0x7F.FFFF along binary point split
#define     MIN_VAL16       (-128.0)                       // 0x80.0000 along binary point split

#define     MAX_VAL48       (1.0 - 7.1054273576e-15)
#define     MIN_VAL48       (-1.0)

// TODO: Replace these double-precision functions with functions for fixed-point types
inline frac16_t rnd_sat16(accum_t a)
{
frac16_t ret;

    ret = (a > MAX_VAL16) ? MAX_VAL16 : a;
    ret = (ret < MIN_VAL16) ? MIN_VAL16 : ret;
    return ret;
}

inline frac24_t rnd_sat24(accum_t a)
{
frac24_t ret;

    ret = (a > MAX_VAL24) ? MAX_VAL24 : a;
    ret = (ret < MIN_VAL24) ? MIN_VAL24 : ret;
    return ret;
}


inline frac48_t sat48(accum_t a)
{
frac48_t ret;

    ret = (a > MAX_VAL48) ? MAX_VAL48 : a;
    ret = (ret < MIN_VAL48) ? MIN_VAL48 : ret;
    return ret;
}


inline accum_t to_accum(double a)
{
accum_t ret = a;
    return ret;
}


inline frac48_t to_frac48(double a)
{
frac48_t ret;

    ret = sat48(a);
    return ret;
}


inline frac24_t to_frac24(double a)
{
frac24_t ret;

    ret = rnd_sat24(a);
    return ret;
}


inline frac16_t to_frac16(double a)
{
frac16_t ret;

    ret = rnd_sat16(a);
    return ret;
}

inline int24_t upper_accum_to_i24(double a)
{
    double scale = pow(2.0, 23);
    double b = a * scale;
    int24_t i = (int24_t)b;
    return i;
}

inline frac24_t lower_accum_to_f24(double a)
{
    double scale = pow(2.0, 16);
    double b = a * scale;
    int i = (int)b;
    b = b - (double)i;      // remove integer part
    return b;
}

// Shift left
inline accum_t shl(accum_t a, unsigned sh)
{
    double shmul = pow(2.0, sh);
    accum_t ret = a * shmul;
    return ret;
}

// Shift right
inline accum_t shr(accum_t a, unsigned sh)
{
    int ss = -(int)sh;
    double shmul = pow(2.0, ss);
    accum_t ret = a * shmul;
    return ret;
}

// Shift signed; if shift count is negative, shift left, else shift right
inline accum_t shs(accum_t a, int sh)
{
accum_t ret;

    if (sh >= 0)
        ret = shr(a, sh);
    else
        ret = shl(a, (unsigned)(-sh));
    return ret;
}


inline frac16_t mul_rnd16(frac24_t a, frac16_t b)
{
frac16_t ret;

// s1i0f23 * s1i7f16 --> s1i8f39; then with fixed point mult shift, s1i7f40
// Need to remove 24b to get back to s1i7f16
    accum_t acc = a * b;    // TODO: replace with scaled integer multiplier, remove correct # of bits
    ret = rnd_sat16(acc);    // TODO: Determine if this is done just by moving the accumulator to the smaller container
    return ret;
}


inline frac16_t log2_approx(accum_t a)
{
frac16_t ret;

    if (a == 0.0)
        ret = -47;
    else
        ret = to_frac16(log2(a));
    return ret;
}


// Model of log2abs instruction; add 1 to floor calculation to match
inline int24_t log2_int24 (accum_t a)
{
int24_t ret;

    if (a <= 0.0)
        ret = -47;
    else
        ret = (int24_t)floor(log2(a)) + 1;
    return ret;
}


inline accum_t mult_log2(frac24_t a, frac16_t g)
{
accum_t ret;
double gain = pow(2.0, g);

    ret = a * gain;
    return ret;
}

inline frac24_t abs_f24 (frac24_t a)
{
frac24_t ret;

    ret = (a < 0) ? -a : a;
    return ret;
}

inline frac16_t max16 (frac16_t a, frac16_t b)
{
frac16_t ret;
    ret = (a > b) ? a : b;
    return ret;
}

inline frac16_t min16 (frac16_t a, frac16_t b)
{
frac16_t ret;
    ret = (a < b) ? a : b;
    return ret;
}

inline frac24_t max24 (frac24_t a, frac24_t b)
{
frac24_t ret;
    ret = (a > b) ? a : b;
    return ret;
}

inline frac24_t min24 (frac24_t a, frac24_t b)
{
frac24_t ret;
    ret = (a < b) ? a : b;
    return ret;
}

inline int24_t maxint(int24_t a, int24_t b)
{
int24_t ret;
    ret = (a > b) ? a : b;
    return ret;
}

inline int24_t minint(int24_t a, int24_t b)
{
int24_t ret;
    ret = (a < b) ? a : b;
    return ret;
}

inline frac48_t dualTC_Smooth_48 (frac48_t Inp, frac48_t Prev, int24_t ATC, int24_t RTC)
{
frac48_t Ret;
frac48_t Diff;
int24_t TC;

    Diff = Inp - Prev;
    if (Diff > 0)
        TC = ATC;
    else
        TC = RTC;
    Ret = shr(Diff, TC) + Prev;
    return Ret;

}


#include "Complex24Class.h"

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Definitions common across modules

#define     BLOCK_SIZE              8
#define     BASEBAND_SAMPLE_RATE    24000
#define     SUBBAND_SAMPLE_RATE     (BASEBAND_SAMPLE_RATE/BLOCK_SIZE)

#define     WOLA_MAX_SIZE_LOG2      11
#define     WOLA_MAX_SIZE           (1 << WOLA_MAX_SIZE_LOG2)

#define     WOLA_LOG2_LA            6
#define     WOLA_LA                 (1<<WOLA_LOG2_LA)
#define     WOLA_LOG2_LS            6
#define     WOLA_LS                 (1<<WOLA_LOG2_LS)
#define     WOLA_LOG2_N             6
#define     WOLA_N                  (1<<WOLA_LOG2_N)
#define     WOLA_NUM_BINS           (WOLA_N/2)
#define     WOLA_R                  BLOCK_SIZE
#define     WOLA_OS                 (WOLA_N/WOLA_R)

#define     WOLA_STACKING_EVEN      0
#define     WOLA_STACKING_ODD       1
#define     WOLA_STACKING           WOLA_STACKING_EVEN

#define     WOLA_FILTBANK_GAIN_LOG2 to_frac16(-2.0)     // For LA=LS=N=64, default window; 0.252825850907156 linear

#define     WOLA_BFP_SHIFT          4                                   // TODO: Determine if this is sufficient or if we need to go to 5
#define     WOLA_SYN_PRESCALE       (double)(1 << WOLA_BFP_SHIFT)
#define     WOLA_ANA_POSTSCALE      (1.0/WOLA_SYN_PRESCALE)             // TODO: Convert to right shift for fixed point

#define     WOLA_WINDOW_DEFAULT     0
#define     WOLA_WINDOW_HANNING     1
#define     WOLA_WINDOW_TYPE        WOLA_WINDOW_DEFAULT

#define     BITREV_SHIFT            (WOLA_MAX_SIZE_LOG2 - WOLA_LOG2_N)

#define     WDRC_NUM_CHANNELS       8

#define     FBC_COEFFS_PER_BIN      4
#define     FBC_COEFF_SPACING       2       // Should be either 1 (continguous coeffs) or 2 (zero between each coeff)
#define     FBC_REV_ANA_BUF_SIZE    8       // MUST BE POWER OF 2 >= (FBC_COEFFS_PER_BIN*FBC_COEFF_SPACING)
#define     FBC_REV_ANA_SIZE_MASK   (FBC_REV_ANA_BUF_SIZE-1)


//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Include module parameter structure headers

#include "SYS_ParamStruct.h"
#include "WDRC_ParamStruct.h"
#include "FBC_ParamStruct.h"
#include "EQ_ParamStruct.h"
#include "NR_ParamStruct.h"

extern strParams_SYS    SYS_Params;
extern strParams_WDRC   WDRC_Params;
extern strParams_FBC    FBC_Params;
extern strParams_EQ     EQ_Params;
extern strParams_NR     NR_Params;


//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Include module headers, declare globals

#include "WOLA.h"
#include "SYS.h"
#include "WDRC.h"
#include "FBC.h"
#include "NR.h"

extern strSYS   SYS;
extern strWDRC  WDRC; 
extern strFBC   FBC;
extern strNR    NR;


//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Simulation helper includes, references

#include "SIM.h"

extern  strSIM  SIM;

#endif  // _COMMON_H
