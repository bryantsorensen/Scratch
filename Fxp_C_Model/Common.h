//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
// Common header file for fixed-point C code
//
// Novidan, Inc. (c) 2023.  May not be used or copied with prior consent.
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


inline accum_t slr(accum_t a, int sh)
{
    double shmul = pow(2.0, sh);
    accum_t ret = a * shmul;
    return ret;

}


inline frac16_t mul_rnd16(frac16_t a, frac16_t b)
{
frac16_t ret;

    accum_t acc = a * b;
    acc = slr(acc, 7);  // acc <<= 7;   TODO: Replace function with shift operator
    ret = rnd_sat16(acc);    // TODO: Determine if this is done just by moving the accumulator to the smaller container
    return ret;
}


inline frac16_t log2_approx(accum_t a)
{
frac16_t ret;

    ret = log2(a);
    return ret;
}


//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Definitions common across modules
#define     WOLA_N          64
#define     WOLA_NUM_BINS   (WOLA_N/2)
#define     WOLA_R          8

#define     MAX_NUM         4       // TODO: This is a junk test value, remove

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Include module parameter structure headers

#include "SYS_ParamStruct.h"
#include "WDRC_ParamStruct.h"

extern strParams_WDRC   WDRC_Params;
extern strParams_SYS    SYS_Params;

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Include module headers, declare globals

#include "Complex24Class.h"
#include "wola.h"
#include "WDRC.h"

extern strWDRC WDRC; 

void FW_Param_Init();

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Simulation helper function prototypes and includes

#include "WAV_Utils.h"

int getopt(int nargc, char * const nargv[], const char *ostr);
char* get_optarg();

#endif  // _COMMON_H
