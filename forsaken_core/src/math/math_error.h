#pragma once

/* math_error.h -- debug-build floating-point exception reporting
 *
 * Only active with DEBUG_ON. math_error.cpp's DebugMathErrors prints a summary
 * of the fenv exception flags raised;
 * main.cpp calls it on the failure exit path, and every frame when
 * DEBUG_ALL_MATH_ERRORS is set.
 */

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
