#pragma once

/* new3d.h -- core vector and matrix types, vertex formats, fast trig and the game RNG
 *
 * new3d.cpp defines the routines, the sine table (trig_table_init, called once
 * at startup) and the RNG seeds. MATRIX is D3D row-vector layout: MxV computes
 * v * M with translation in _41.._43. ApplyMatrix uses the transposed 3x3 with
 * the same translation and is not a general transform. Angle units differ:
 * BuildRotMatrix and SIND/COSD take degrees; MatrixFromAxisAndAngle,
 * SINR/COSR and fast_sinf/fast_cosf take radians. COLOR is a u32 0xAARRGGBB.
 * Random_Range draws from Seed1/Seed2, which a multiplayer host seeds and
 * sends to joining players.
 */

#include <math.h>
#include "game/main.h"


// taken from d3d9.h

typedef DWORD COLOR; // bgra

/*===================================================================
	Defines
===================================================================*/

constexpr auto EPS = 1e-7;
constexpr float PI = 3.14159265358979323846F;
#ifndef M_PI // defined in mingw
#define M_PI				3.14159265358979323846F
#endif
#ifndef M_PI_2 // pi/2
#define M_PI_2				1.57079632679489661923
#endif
#define	D2R(x)				( ( x ) * ( 1.0F / 57.2957F) )		/* Nick Pelling changed */
#define	R2D(x)				( ( x ) * 57.2957F )

/*===================================================================
	Fast sin/cos via a 1024-entry LUT with linear interpolation.
	Table is built once by trig_table_init() (new3d.cpp, called from
	main.cpp); max error vs sinf() is < 4.7e-6.  4 KB fits in the
	ARM11's 16 KB L1D.

	Both accessors are static inline for the same reason
	fast_invsqrtf below is: they are a handful of VFP ops, and the
	~127 SINR/COSR/SIND/COSD call sites should not each pay a
	non-leaf `bl fast_sinf` (plus its register spills) to reach them.

	★ The range reduction deliberately does NOT use floorf().  VFPv2
	has no float-floor instruction, so floorf() — and __builtin_floorf,
	which looks like it must be free — compiles to a real `bl floorf`
	into newlib.  That turns this "lookup" into a non-leaf function
	wrapping a libm call, forces callers to spill VFP registers around
	it (vpush {d8}), and drags in newlib's VFP->ARM flag transfers
	(vmov + vcmpe / vmrs APSR_nzcv, fpscr) which stall the ARM1176
	pipeline — i.e. the "optimisation" can cost more than the sinf() it
	replaced.  -fno-math-errno does not help; there is simply no VFPv2
	instruction to emit.

	Instead: truncate with a (int) cast and correct toward -inf by
	hand.  `i &= TRIG_TABLE_MASK` then does the modulo wrap for free
	in two's complement (-1 & 1023 == 1023), so no explicit
	range-reduction step is needed at all.  This is exact, not an
	approximation — it selects the same table index and the same
	fraction as the floorf form.

	DOMAIN: (int)idx is undefined once |rad * TRIG_SCALE| >= 2^31,
	i.e. |rad| >= ~1.3e7 radians.  Every caller here feeds either a
	bounded angle or a per-object/per-frame accumulator, none of which
	approach that; if a future caller passes an unbounded time-derived
	phase, wrap it before the call rather than adding a branch here.
===================================================================*/

constexpr int TRIG_TABLE_SIZE = 1024;
constexpr auto TRIG_TABLE_MASK = (TRIG_TABLE_SIZE - 1);
constexpr auto TRIG_SCALE = (TRIG_TABLE_SIZE / (2.0f * 3.14159265358979323846f));

extern void  trig_table_init(void);
extern float fast_sin_table[TRIG_TABLE_SIZE + 1]; /* +1 for the interpolation boundary */

static inline float fast_sinf(float rad)
{
	const float idx = rad * TRIG_SCALE;
	int   i = (int)idx;               /* truncates toward zero */
	float f = idx - (float)i;
	if (f < 0.0f) { --i; f += 1.0f; } /* -> floor(idx), fraction back into [0,1) */
	i &= TRIG_TABLE_MASK;             /* two's complement modulo wrap */
	return fast_sin_table[i] + (fast_sin_table[i + 1] - fast_sin_table[i]) * f;
}

static inline float fast_cosf(float rad)
{
	return fast_sinf(rad + 1.5707963267948966f); /* rad + PI/2 */
}

/* Inverse square root.  On the 3DS ARM1176 (VFPv2) the classic Quake III
 * bit-hack is a PESSIMISATION: it forces two int<->VFP register transfers
 * (vmov r,s then vmov s,r) per call, each a serialising crossing of the
 * decoupled VFP11 pipeline, and only reaches ~0.17% accuracy.  VFPv2 has a
 * hardware vsqrt.f32, so the exact form below is both faster (stays
 * VFP-side, no crossing) and accurate.  Declared static inline so it folds
 * into the caller: at the call sites the argument is a guarded sum of
 * squares, which lets GCC prove x > 0 and drop the sqrt errno branch,
 * leaving a bare vsqrt.f32 + vdiv.f32.
 *
 * NOTE: VFPv2 has NO NEON, so the reciprocal-estimate instructions
 * VRSQRTE / VRECPE are unavailable on this CPU — do not reach for them. */
static inline float fast_invsqrtf(float x) { return 1.0f / sqrtf( x ); }

/* Floor / ceil without libm.  VFPv2 has no float-result floor instruction,
 * so floorf/ceilf are a real `bl` into newlib with vmrs/vmsr flag transfers
 * and a spill frame (see the fast_sinf rationale above); the (int) vcvt
 * below is one conversion with no boundary round trip.  Bit-identical to
 * floorf/ceilf on the domain |x| < 2^31 EXCEPT the sign of zero results:
 * fast_floorf(-0.0f) returns +0.0f and fast_ceilf of a negative fraction
 * returns +0.0f where libm returns -0.0f.  IEEE comparison treats ±0 as
 * equal, and every call site in this tree casts the result to an integer
 * type or feeds it into FMOD/FDIV below, where the sign of zero cannot
 * change the output — do not introduce a consumer that divides by or
 * sign-tests these results without revisiting this.  Outside |x| < 2^31
 * the (int) cast is UB, same caveat as fast_sinf.  Game coordinates,
 * texture phases, and cell indices are all far inside that domain.
 * R6/R2 standing exemption: this IS the sanctioned floor for hot paths. */
static inline float fast_floorf(float x)
{
	int   i = (int)x;
	float t = (float)i;
	return (x < t) ? t - 1.0f : t;
}

static inline float fast_ceilf(float x)
{
	int   i = (int)x;
	float t = (float)i;
	return (x > t) ? t + 1.0f : t;
}

#define	SINR(x)				fast_sinf( x )
#define	COSR(x)				fast_cosf( x )
#define	SIND(x)				fast_sinf( D2R( x ) )
#define	COSD(x)				fast_cosf( D2R( x ) )
/* @vfp-exempt R6 — inverse trig has no LUT form (fast trig is sin/cos only); LUT-izing is new numeric work needing a fidelity capture. measured n/a. Verified 2026-10-25. */
#define ATAND(y, x)			R2D( atan2f(y, x) )
#define FMOD( NUM, DIV )	( (NUM) - (DIV) * fast_floorf( (NUM) / (DIV) ) )
#define FDIV( NUM, DIV )	( (DIV) * fast_floorf( (NUM) / (DIV) ) )

/*===================================================================
	Structures
===================================================================*/

// colour packing, as in Direct3D's d3dtypes.h
#define RGBA_MAKE(r, g, b, a)   ((COLOR) (((a) << 24) | ((r) << 16) | ((g) << 8) | (b)))
#define	RGB_MAKE(r, g, b)    ((COLOR) (((r) << 16) | ((g) << 8) | (b)))

// COLOR is packed bgra
#define RGBA_GETALPHA(rgb)    ((rgb) >> 24)
#define RGBA_GETRED(rgb)    (((rgb) >> 16) & 0xff)
#define RGBA_GETGREEN(rgb)    (((rgb) >> 8) & 0xff)
#define RGBA_GETBLUE(rgb)    ((rgb) & 0xff)

#define RENDERVAL(val)    ((float)val)

/*
	Pre-DX8 vertex formats
	taken from http://www.mvps.org/directx/articles/definitions_for_dx7_vertex_types.htm
*/

typedef struct {
    union {
       float x;
       float dvX;
    };
    union {
       float y;
        float dvY;
    };
    union {
        float z;
        float dvZ;
    };
    union {
        COLOR color;
        COLOR dcColor;
    };
    union {
        float tu;
        float dvTU;
    };
    union {
        float tv;
        float dvTV;
	};
} LVERTEX, *LPLVERTEX;

typedef struct {
    union {
		float x;
        float sx;
        float dvSX;
    };
    union {
		float y;
        float sy;
        float dvSY;
    };
    union {
	    float z;
        float sz;
        float dvSZ;
    };
    union {
		float w;
        float rhw;
        float dvRHW;
    };
    union {
        COLOR color;
        COLOR dcColor;
    };
    union {
        float tu;
        float dvTU;
    };
    union {
        float tv;
        float dvTV;
    };

} TLVERTEX, *LPTLVERTEX;

typedef struct {
    union {
        WORD v1;
        WORD wV1;
    };
    union {
        WORD v2;
        WORD wV2;
    };
    union {
        WORD v3;
        WORD wV3;
    };
} TRIANGLE, *LPTRIANGLE;


/*===================================================================
	2D Vertices
===================================================================*/
typedef struct VERT2D {
	float	x;
	float	y;
} VERT2D;
/*===================================================================
	3D Vertices
===================================================================*/
typedef struct VERT {
	float	x;
	float	y;
	float	z;
} VERT;

/*===================================================================
	3D Normal
===================================================================*/
typedef struct NORMAL {
	union { float nx; float x; };
	union { float ny; float y; };
	union { float nz; float z; };
} NORMAL;

/*===================================================================
	4 X 4 Matrix
===================================================================*/
typedef struct MATRIX {
	float	_11, _12, _13, _14;
	float	_21, _22, _23, _24;
	float	_31, _32, _33, _34;
	float	_41, _42, _43, _44;
} MATRIX;

/*===================================================================
	3 X 3 Matrix
===================================================================*/
typedef struct MATRIX3X3 {
	float	_11, _12, _13;
	float	_21, _22, _23;
	float	_31, _32, _33;
} MATRIX3X3;


/*===================================================================
	Vector
===================================================================*/
typedef struct VECTOR {
	float	x;
	float	y;
	float	z;
} VECTOR;

/*===================================================================
	Short Vector
===================================================================*/
typedef struct SHORTVECTOR {
	int16_t	x;
	int16_t	y;
	int16_t	z;
} SHORTVECTOR;


/*===================================================================
	Plane
===================================================================*/
typedef struct PLANE {
	VECTOR Normal;
	float Offset;
} PLANE;

/*===================================================================
	Prototypes
===================================================================*/
float DotProduct( VECTOR * a , VECTOR * b );

void CrossProduct( VECTOR * a, VECTOR * b, VECTOR * ab );

void NormaliseVector( VECTOR *  v );


void BuildRotMatrix( float xa, float ya, float za, MATRIX * m );
void MatrixMultiply( MATRIX * m0, MATRIX * m1, MATRIX * m0m1 );
void ApplyMatrix( MATRIX * m, VECTOR * v0, VECTOR * v1 );

void ReflectVector( VECTOR * old, NORMAL * normal, VECTOR * new1 );

float VectorLength( VECTOR * v ) ;

float DistanceVert2Vector( VERT *  a , VECTOR * b ) ;
float DistanceVector2Vector( VECTOR *  a , VECTOR * b );


void VisPolyApplyMatrix( MATRIX * m, VECTOR * v0, VECTOR * v1 );

void MatrixTranspose( MATRIX * m1, MATRIX * m2 );

u_int16_t	Random_Range( u_int16_t Max );
float Random_Range_Float( float Max );


void	MakeViewMatrix(VECTOR *viewpos, VECTOR *lookpos, VECTOR *up, MATRIX *view);
void CalcViewAxes(VECTOR *viewpos, VECTOR *lookpos, VECTOR *up, VECTOR *vx, VECTOR *vy, VECTOR *vz);
void MatrixFromAxisAndAngle( float angle, VECTOR * axis, MATRIX * rot );
void spline(VECTOR * p, float t, VECTOR * p1, VECTOR * p2, VECTOR * p3, VECTOR * p4);

/* Defined in new3d.cpp; shared with the modules that use it. */
extern MATRIX MATRIX_Identity;
extern u_int16_t Seed1;
extern u_int16_t Seed2;
