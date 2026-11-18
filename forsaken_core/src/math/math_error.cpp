// see math_error.h for an explain of some details
#ifdef DEBUG_ON

// matherr was deprecated
// https://man7.org/linux/man-pages/man7/math_error.7.html
// https://man7.org/linux/man-pages/man3/fenv.3.html

#include "math/math_error.h"
#include "util/util.h"

void InitMathErrors(void)
{
}

// https://en.cppreference.com/w/c/numeric/fenv/fetestexcept
int DebugMathErrors( void )
{

	if(!fetestexcept(FE_ALL_EXCEPT))
		return 0;

	DebugPrintf( "======================\n" );
	DebugPrintf( "Math Exception Summary\n" );
	DebugPrintf( "======================\n" );

	if(fetestexcept(FE_DIVBYZERO))
		DebugPrintf("FE_DIVBYZERO\n");

	if(fetestexcept(FE_INEXACT))
		DebugPrintf("FE_INEXACT\n");

	if(fetestexcept(FE_INVALID))
		DebugPrintf("FE_INVALID\n");

	if(fetestexcept(FE_OVERFLOW))
		DebugPrintf("FE_OVERFLOW\n");

	if(fetestexcept(FE_UNDERFLOW))
		DebugPrintf("FE_UNDERFLOW\n");

	DebugPrintf( "======================\n" );

	feclearexcept(FE_ALL_EXCEPT);

	return 1;
}

#endif // DEBUG_ON
