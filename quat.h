#ifndef	QUAT_DEFS
#define	QUAT_DEFS

#include "main.h"
#include "new3d.h"

#ifdef __cplusplus
extern "C" {
#endif

/*===================================================================
	Quaternion Structures

	QUAT stores (w, x, y, z) - scalar first, Forsaken's layout since
	the original engine (RT64-style (x,y,z,w) code must convert, not
	cast). Quaternions are expected to be unit length on input to
	the multiply/matrix paths; QuatNormalise restores that.
	QuatInterpolate performs normalised linear interpolation and
	relies on QUATLERP.dir for the shortest-arc sign choice - the
	caller sets it, the function does not compute the dot product.
===================================================================*/
typedef struct QUAT {
	float	w;
	float	x;
	float	y;
	float	z;
} QUAT;
typedef struct SHORTQUAT {
	int16_t	w;
	int16_t	x;
	int16_t	y;
	int16_t	z;
} SHORTQUAT;


typedef struct {

	QUAT		start;		// Starting QUATERNION
	QUAT		end;		// Ending QUATERNION
	QUAT *		crnt;		// Current QUATERNION to change
	float		time;		// Time
	float		dir;		// Direction

} QUATLERP;

/*===================================================================
	Prototypes
===================================================================*/
void MakeQuat( float XRot, float YRot, float ZRot, QUAT * qxyz );
void QuatMultiply( QUAT * q1, QUAT * q2, QUAT * q1q2 );
void QuatMultiplyX_Y( QUAT * qx, QUAT * qy, QUAT * qxy );
void QuatMultiplyXY_Z( QUAT * qxy, QUAT * qz, QUAT * qxyz );
void QuatNormalise( QUAT * q );
void QuatToMatrix( QUAT * q, MATRIX * m );
float QuatDotProduct( QUAT * q1, QUAT * q2 );
void QuatInterpolate( register QUATLERP * ql );
void QuatFromVector( VECTOR * Tv, QUAT * q );
void QuatFromVector2( VECTOR * Tv, QUAT * q );
void Quaternion_Slerp( float alpha, QUAT * a, QUAT * b, QUAT * q, int spin );
void QuatInverse( QUAT * s, QUAT * d );
void QuatMake( QUAT * destQuat, float x, float y, float z, float angle);
void QuatFrom2Vectors( QUAT * destQuat, VECTOR * v1, VECTOR * v2 );
void MatrixToQuat(QUAT * destQuat, MATRIX * srcMatrix);
void QuatFromDirAndUp( VECTOR * Dir, VECTOR * Up, QUAT * Quat );

#ifdef __cplusplus
}
#endif

#endif
