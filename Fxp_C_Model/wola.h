/*-----------------------------------------------------------------------------
** Copyright (c) 2010 Semiconductor Components Industries, LLC (d/b/a ON Semiconductor). All rights reserved.
**
** This code is the property of ON Semiconductor and may not be redistributed
** in any form without prior written permission from ON Semiconductor. The
** terms of use and warranty for this code are covered by contractual
** agreements between ON Semiconductor and the licensee.
**
**-----------------------------------------------------------------------------
** wola.h
** - C API interface to the functions in a WOLA Filterbank
**-----------------------------------------------------------------------------
** $RCSfile: wola.h,v $
** $Revision: 1.1 $
** $Date: 2010/11/01 15:27:43 $
** $Author: mcgrathk $
**---------------------------------------------------------------------------*/

#ifndef __WOLA_INCLUDED__
#define __WOLA_INCLUDED__


/*
** Windows specific code for DLL implementation
*/
//#ifdef _WIN32

	/* define API calls to be import or export */
	#ifdef WOLA_EXPORTS
		#define WOLA_API(type)	__declspec(dllexport) type __stdcall
	#else
		#define WOLA_API(type)	__declspec(dllimport) type __stdcall
	#endif /*WOLA_EXPORTS*/

//#else

	/* define it to be nothing for other platforms */
//	#define WOLA_API(type) type

//#endif /*_WIN32*/


#ifdef __cplusplus
extern "C" {
#endif /*__cplusplus*/

/******************************************************************************
** Types and defines
******************************************************************************/
/* base type definitions */
typedef double WolaReal;    

typedef struct _WolaComplex
{
	double r;
	double i;

} WolaComplex;

typedef uint32_t WolaHandle;



/******************************************************************************
** Error functions and codes
******************************************************************************/
void SetError(const int16_t nErrCode);
int16_t GetError(void);

/* external errors */
#define Wola_errNoError				0
#define Wola_errNotPowerofTwo		1
#define Wola_errMemoryAllocFailed	2
#define Wola_errBadFFTOrder			3
#define Wola_errInvalidPointer		4
#define Wola_errNNotPositive		5
#define Wola_errBadWinMode			6
#define Wola_errBadAnaWinSize		7
#define Wola_errNLowerThanR			8
#define Wola_errBadStacking			9
#define Wola_errBadSynWinSize		10
#define Wola_errRNotPositive		11

/* internal errors */
#define Wola_intBadWindowSize		100
#define Wola_intBadZeroSpacing		101
#define Wola_intBadUpdateMode       102



/******************************************************************************
** Initialize a WOLA object and return a handle to it. The analysis and synthesis
** windows are initialized with Brennan-windowed sinc functions. The extended
** version enables to set the windows mode and shift, set by default to shift = 0, 
** AnaWinMode = WOLA_NONSYM and SynWinMode = WOLA_SYM.
**
** Inputs
**  La		  :  length of analysis window and input FIFO
**  Ls		  :  length of syntheasis window and output FIFO
**  R		  :  input block step size (oversampling = N/R)
**  N		  :  FFT size, number_of_bands = N/2 (odd) or N/2+1 (even)
**  stacking  :  filterbank stacking (WOLA_STACKING_ODD = odd; WOLA_STACKING_EVEN = even)
**  wshift    :  shift for the processing windows
**  AnaWinMode:  symetry mode for analysis window (must be WOLA_SYM or WOLA_NONSYM)
**  SynWinMode:  symetry mode for synthesis window (must be WOLA_SYM or WOLA_NONSYM)
**
**
** Outputs
**  ret		:  a WOLA handle (DO NOT MODIFY THIS VALUE)
******************************************************************************/
WOLA_API(WolaHandle)	WolaInit(int16_t La,
								 int16_t Ls,
								 int16_t R,
								 int16_t N,
								 int16_t stacking
/*								 int16_t wshift = 0,
								 int16_t AnaWinMode = WOLA_NONSYM,
								 int16_t SynWinMode = WOLA_SYM
*/								);

WOLA_API(WolaHandle)	WolaInitExtd(int16_t La,
									 int16_t Ls,
									 int16_t R,
									 int16_t N,
									 int16_t stacking,
									 int16_t wshift,
									 int16_t AnaWinMode,
									 int16_t SynWinMode
									 );


/******************************************************************************
** Destroy a WOLA object
**
** Inputs
**  obj	:  valid WOLA handle that was returned by WolaInit()
**
** Outputs
**  ret	:  WOLA_TRUE (success) or WOLA_FALSE (fail)
******************************************************************************/
WOLA_API(int16_t)			WolaClose(WolaHandle obj);


/******************************************************************************
** Set the analysis window for a WOLA object
**
** Inputs
**   Wa	 :  La real analysis window points
**   Len :  size of analysis window - must be equal to La
**   obj :  valid WOLA handle that was returned by WolaInit()
**
** Outputs
**   ret : WOLA_TRUE (success) or WOLA_FALSE (fail)
******************************************************************************/
WOLA_API(int16_t)			WolaSetAnaWin(WolaHandle obj,
									  const WolaReal Wa[/* La */],
									  int16_t Len
									 );


/******************************************************************************
** Set the synthesis window for a WOLA object
**
** Inputs
**  Ws	:  Ls real synthesis window points
**  Len	:  size of synthesis window - must be equal to Ls
**  obj :  valid WOLA handle that was returned by WolaInit()
**
** Outputs
**  ret	: WOLA_TRUE (success) or WOLA_FALSE (fail)
******************************************************************************/
WOLA_API(int16_t)			WolaSetSynWin(WolaHandle obj,
									  const WolaReal Ws[/* Ls */],
									  int16_t Len
									 );


/******************************************************************************
** Perform an analysis step on a WOLA object
**
** Inputs
**  inData	:  R real samples to analysis filterbank
**  obj     :  valid WOLA handle that was returned by WolaInit()
**
** Outputs
**  outData	:  N/2 complex outputs from analysis filterbank
**  ret		:  WOLA_TRUE (success) or WOLA_FALSE (fail)
**
** For EVEN stacking, the DC and Nyquist points are packed into the first
** location: outData[0].r = DC and outData[0].i = Nyquist. These values are
** both real.  For ODD stacking, all points returned in outData[] are complex,
** one for each band.
**
** There are two versions of the function, one performs an analysis step
** as the hardware WOLA would do (WolaAnalyze(.)), the other gives more 
** flexibility to the programmer by enabling to perform the analysis and 
** synthesis steps in whatever order thus giving the same results as if 
** the steps had been performed sequentially (analysis, synthesis, analysis,
** synthesis, ...) with WolaAnalyze(.). Both functions are used the same
** way (their interface is the same). See the WOLA toolbox user's guide
** for more details on the use of those functions
**
** Note that the outData[] array of WolaComplex values must have space for N
** WolaComplex values, even though only N/2 values are returned, in the first
** N/2 locations.  The extra space is required for internal use.
******************************************************************************/
WOLA_API(int16_t)			WolaAnalyze(WolaHandle obj,
									const WolaReal inData[/* R */],
									WolaComplex outData[/* N */]
								   );

WOLA_API(int16_t)			WolaAnalyzeSeq(WolaHandle obj,
									   const WolaReal inData[/* R */],
									   WolaComplex outData[/* N */]
									  );


/******************************************************************************
** Apply gains to a WOLA object
**
** Inputs
**  freqData:  N/2 complex inputs and outputs
**  gain	:  N/2 values between 0 and 1 inclusive
**  obj		:  valid WOLA handle that was returned by WolaInit()
**
** Outputs
**  ret		:  WOLA_TRUE (success) or WOLA_FALSE (fail)
**
** For EVEN stacking, gain[0] is applied to the DC point stored in freqData[0].r,
** while the Nyquist point stored in freqData[0].i is set to zero.  For ODD
** stacking, all points in freqData[] are complex, one for each band, so the i'th
** gain is applied to the i'th point.
**
** Note that WolaApplyGain() only requires the freqData[] array of WolaComplex
** values to have space for the N/2 values that are modified by the gain vector,
** unlike WolaAnalyze() and WolaSynthesize() which require space for N values
** despite returning/using only N/2.  However, since WolaApplyGain() is
** typically called after WolaAnalyze() and before WolaSynthesize() - with the
** freqData[] array being the outData[] array that was passed to WolaAnalyze() and
** the inData[] array that will be passed to WolaSynthesize() - this freqData[]
** array will typically have space for N WolaComplex values.
******************************************************************************/
WOLA_API(int16_t)			WolaApplyGain(const WolaHandle obj,
									  WolaComplex freqData[/* N/2 */],
									  const WolaReal gain[/* N/2 */]
									 );


/******************************************************************************
** Apply complex gains to a WOLA object
**
** Inputs
**  freqData: N/2 complex inputs and outputs
**  gain    : N/2 complex values inside the unit circle
**  obj     : valid WolaHandle that was returned by WolaInit()
**
** Outputs
**  ret     : WOLA_TRUE (success) or WOLA_FALSE (fail)
**
** For EVEN stacking, cplxGain[0].r is applied to the DC point stored in
** freqData[0].r, and cplxGain[0].i is applied to the Nyquist point stored
** in freqData[0].i. For ODD stacking, all points in freqData[] are 
** complex, so the i'th gain is applied to the i'th point (true complex
** multiplication).
**
** Note that WolaApplyGainComplex() only requires the freqData[] array of WolaComplex
** values to have space for the N/2 values that are modified by the gain vector,
** unlike WolaAnalyze() and WolaSynthesize() which require space for N values
** despite returning/using only N/2.  However, since WolaApplyGainComplex() is
** typically called after WolaAnalyze() and before WolaSynthesize() - with the
** freqData[] array being the outData[] array that was passed to WolaAnalyze() and
** the inData[] array that will be passed to WolaSynthesize() - this freqData[]
** array will typically have space for N WolaComplex values.
******************************************************************************/
WOLA_API(int16_t)			WolaApplyGainComplex(const WolaHandle obj,
											 WolaComplex freqData[/* N/2 */],
											 const WolaComplex cplxGain[/* N/2 */]
											);


/******************************************************************************
** Perform a synthesis step on a WOLA object
**
** Inputs
**  inData	:  N/2 complex inputs to synthesis filterbank
**  obj		:  valid WOLA handle that was returned by WolaInit()
**
** Outputs
**  outData	:  R real reconstructed outputs from synthesis filterbank
**  ret		:  WOLA_TRUE (success) or WOLA_FALSE (fail)
**
** For EVEN stacking, the DC and Nyquist points are packed into the first
** location: inData[0].r = DC and inData[0].i = Nyquist. These values
** are both real.  For ODD stacking, all input points are complex, one for
** each band.
**
** There are two versions of the function, one performs a synthesis step
** as the hardware WOLA would do (WolaSynthesize(.)), the other gives more 
** flexibility to the programmer by enabling to perform the analysis and 
** synthesis steps in whatever order thus giving the same results as if 
** the steps had been performed sequentially (analysis, synthesis, analysis,
** synthesis, ...) with WolaSynthesize(.). Both functions are used the same
** way (their interface is the same). See the WOLA toolbox user's guide
** for more details on the use of those functions
**
** Note that the inData[] array of WolaComplex values must have space for N
** WolaComplex values, even though only the values in the first N/2 locations
** are used.  The extra space is required for internal use.
******************************************************************************/
WOLA_API(int16_t)			WolaSynthesize(WolaHandle obj,
									   WolaComplex inData[/* N */],
									   WolaReal outData[/* R */]
									  );

WOLA_API(int16_t)			WolaSynthesizeSeq(WolaHandle obj,
										  WolaComplex inData[/* N */],
										  WolaReal outData[/* R */]
										 );


/******************************************************************************
** Get the analysis window size of a WOLA object
**
** Inputs
**  obj		:  valid WOLA handle that was returned by WolaInit()
**
** Outputs
**  ret		:  analysis window size (La)
******************************************************************************/
WOLA_API(int16_t)			WolaGetLa(const WolaHandle obj);


/******************************************************************************
** Get the synthesis window size of a WOLA object
**
** Inputs
**  obj		:  valid WOLA handle that was returned by WolaInit()
**
** Outputs
**  ret		:  analysis window size (Ls)
******************************************************************************/
WOLA_API(int16_t)			WolaGetLs(const WolaHandle obj);


/******************************************************************************
** Get the input block size of a WOLA object
**
** Inputs
**  obj		:  valid WOLA handle that was returned by WolaInit()
**
** Outputs
**  ret		:  input block size (R)
*****************************************************************************/
WOLA_API(int16_t)			WolaGetR(const WolaHandle obj);


/******************************************************************************
** Get the FFT size of a WOLA object
**
** Inputs
**  obj		:  valid WOLA handle that was returned by WolaInit()
**
** Outputs
**  ret		:  FFT size (N)
******************************************************************************/
WOLA_API(int16_t)			WolaGetN(const WolaHandle obj);


/******************************************************************************
** Get the window shift of a WOLA object
**
** Inputs
**  obj		:  valid WOLA handle that was returned by WolaInit()
**
** Outputs
**  ret		:  window shift
******************************************************************************/
WOLA_API(int16_t)			WolaGetWshift(const WolaHandle obj);


/******************************************************************************
** Get the stacking mode (odd or even) of a WOLA object
**
** Inputs
**  obj		:  valid WOLA handle that was returned by WolaInit()
**
** Outputs
**  ret		:  stacking mode (WOLA_STACKING_ODD or WOLA_STACKING_EVEN)
******************************************************************************/
WOLA_API(int16_t)			WolaGetStacking(const WolaHandle obj);


/******************************************************************************
** Get the latest error code set by a function in the library
**
** Inputs
**   npError	:  a pointer to a location where the error code will be stored
**   
** Outputs
**   ret	: WOLA_TRUE (success) or WOLA_FALSE (fail)
******************************************************************************/
WOLA_API(int16_t) WolaGetLastError(int16_t *const npError);


/******************************************************************************
** Reset the current error code to Wola_errNoError
**
** Inputs
**   none
**   
** Outputs
**   ret	: WOLA_TRUE (success) or WOLA_FALSE (fail)
******************************************************************************/
WOLA_API(int16_t) WolaResetError(void);



#ifdef __cplusplus
}
#endif /*__cplusplus*/

#endif /*__WOLA_INCLUDED__*/
