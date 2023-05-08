//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
// Parameter initialization code
//   Auto-Created Starting at 08-05-2023 14:49:49 -700 UTC
//   Created from input file: ParamValsTest.json
//   Uses Profile 1
//
// Novidan, Inc. (c) 2023.  May not be used or copied with prior consent.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

#include "Common.h"

void FW_Param_Init()
{
// SYS Parameters

    SYS_Params.Persist.GlobalA = (15);
    SYS_Params.Persist.MyGlobal = to_frac16(3.7818199999999997);

    SYS_Params.Profile.LocalParam = (0);
    SYS_Params.Profile.ArrayParam[0] = to_frac24(-0.26865671641791045);
    SYS_Params.Profile.ArrayParam[1] = to_frac24(0.20149253731343283);
    SYS_Params.Profile.ArrayParam[2] = to_frac24(-0.00044776119402985075);
    SYS_Params.Profile.ArrayParam[3] = to_frac24(0.3924820343283582);

// WDRC Parameters

    WDRC_Params.Persist.ExpansAtkTC = to_frac24(1.0);
    WDRC_Params.Persist.ExpansRelTC = to_frac24(0.9999999999861121);
    WDRC_Params.Persist.CompressAtkTC = to_frac24(1.0);
    WDRC_Params.Persist.CompressRelTC = to_frac24(0.9999546000702375);
    WDRC_Params.Persist.LimitAtkTC = to_frac24(1.0);
    WDRC_Params.Persist.LimitRelTC = to_frac24(1.0);

    WDRC_Params.Profile.Enable = (1);
    WDRC_Params.Profile.Gain[0] = to_frac16(0.0);
    WDRC_Params.Profile.Gain[1] = to_frac16(0.99996);
    WDRC_Params.Profile.Gain[2] = to_frac16(1.99992);
    WDRC_Params.Profile.Gain[3] = to_frac16(2.99988);
    WDRC_Params.Profile.Thresh[0] = to_frac16(8.333);
    WDRC_Params.Profile.Thresh[1] = to_frac16(9.33296);
    WDRC_Params.Profile.Thresh[2] = to_frac16(11.33288);
    WDRC_Params.Profile.Thresh[3] = to_frac16(14.1661);
    WDRC_Params.Profile.ExpSlope = to_frac16(0.41665);

}
