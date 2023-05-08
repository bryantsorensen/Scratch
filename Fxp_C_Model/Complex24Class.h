//++++++++++++++++++++++++++++++++++++++++++++++++++
//
// Header file for Complex24 class definition. Uses 24b fractional data type.
//
// Novidan, Inc. (c) 2023.  May not be used or copied with prior consent.
//
// Bryant Sorensen, author
// Started 24 Apr 2023
// 
//++++++++++++++++++++++++++++++++++++++++++++++++++

#ifndef _CMPLX24_H
#define _CMPLX24_H

#include "Common.h"

//++++++++++++++++++++++++++++++++++++++++++++++++++
// Complex24 class
//using namespace std;

class Complex24
{
private:
	frac24_t r;
	frac24_t i;
public:
	// Constructors
	Complex24() {	r = (frac24_t)0; i = (frac24_t)0;	}
	Complex24(Complex24& a) {	r = a.r;  i = a.i;	}				// copy constructor
	Complex24(frac24_t& a) { r = a; i = (frac24_t)0; }			// Real to complex
	Complex24(frac24_t& ar, frac24_t& ai) {	r = ar; i = ai;	}	// Two reals to complex
	// Destructor
	~Complex24() {};
	// Assignment operators
	inline Complex24 const& operator = (frac24_t const x)    // Assign Complex24 to a real - zero out imag
	{
		r = x; i = (frac24_t)0;
		return *this;
	}
	inline class Complex24 const& operator = (const class Complex24& a)    // Assign Complex24 to a real - zero out imag
	{
		r = a.r; i = a.i;
		return *this;
	}
	
	inline frac24_t Real(void) { return r; }
	inline frac24_t Imag(void) { return i; }

};

//++++++++++++++++++++++++++++++++++++++++++++++++++
// Overloaded arithmetic functions on Complex 

inline Complex24 operator+(Complex24& a, Complex24& b)
{
	frac24_t cR = a.Real() + b.Real();
	frac24_t cI = a.Imag() + b.Imag();
	Complex24 Ret(cR, cI);
	return(Ret);
}

inline Complex24 operator-(Complex24& a, Complex24& b)
{
	frac24_t cR = a.Real() - b.Real();
	frac24_t cI = a.Imag() - b.Imag();
	Complex24 Ret(cR, cI);
	return(Ret);
}

inline Complex24 operator*(Complex24& a, Complex24& b)
{
	frac24_t ra, rb;
	frac24_t ia, ib;
	frac24_t Rprod, Iprod;

	ra = a.Real();
	ia = a.Imag();
	rb = b.Real();
	ib = b.Imag();

	Rprod = (ra * rb) - (ia * ib);
	Iprod = (ra * ib) + (rb * ia);

	Complex24 Ret(Rprod, Iprod);
	return(Ret);
}

#endif  // _CMPLX24_H

