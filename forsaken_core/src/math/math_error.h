#pragma once
#ifdef DEBUG_ON

extern void InitMathErrors( void );
extern int DebugMathErrors( void );

// mingw still appears to support matherr so we'll leave it for now
// not sure if msvc still does but it appears matherr is going away
// https://man7.org/linux/man-pages/man3/matherr.3.html
// https://man7.org/linux/man-pages/man7/math_error.7.html
// https://man7.org/linux/man-pages/man3/fenv.3.html
#include <math.h>
#include <errno.h>
#include <fenv.h>



#endif // DEBUG_ON
