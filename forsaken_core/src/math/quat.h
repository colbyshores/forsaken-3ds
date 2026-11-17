#ifndef	QUAT_DEFS
#define	QUAT_DEFS

#include "game/main.h"
#include "math/new3d.h"

/*===================================================================
	Quaternion Structures

	QUAT stores (w, x, y, z) - scalar first, Forsaken's layout since
	the original engine (RT64-style (x,y,z,w) code must convert, not
	cast). Quaternions are expected to be unit length on input to
	the multiply/matrix paths; Normalise restores that.
	QuatLerp performs normalised linear interpolation and
	relies on QUATLERP.dir for the shortest-arc sign choice - the
	caller sets it, the function does not compute the dot product.

	QUAT carries its operations as member functions. Its layout is
	the original C struct's -- four floats, no padding, no virtuals --
	so data that stores quaternions (saves, level and network
	structures) is unchanged.
===================================================================*/
typedef struct SHORTQUAT {
	int16_t	w;
	int16_t	x;
	int16_t	y;
	int16_t	z;
} SHORTQUAT;

class QUAT {
public:
	float	w;
	float	x;
	float	y;
	float	z;

	void	MakeEuler( float XRot, float YRot, float ZRot );
	void	Multiply( const QUAT * q2, QUAT * q1q2 ) const;
	void	MultiplyX_Y( const QUAT * qy, QUAT * qxy ) const;
	void	MultiplyXY_Z( const QUAT * qz, QUAT * qxyz ) const;
	void	Normalise();
	void	ToMatrix( MATRIX * m ) const;
	float	Dot( const QUAT * q2 ) const;
	void	Slerp( float alpha, const QUAT * b, QUAT * q, int spin ) const;
	void	FromVector( VECTOR * Tv );
	void	FromVector2( VECTOR * Tv );
	void	Inverse( QUAT * d ) const;
	void	MakeAxis( float x, float y, float z, float angle );
	void	From2Vectors( const VECTOR * v1, const VECTOR * v2 );
	void	FromMatrix( const MATRIX * srcMatrix );
	void	FromDirAndUp( const VECTOR * Dir, const VECTOR * Up );
};

class QUATLERP {
public:
	QUAT	start;		// Starting QUATERNION
	QUAT	end;		// Ending QUATERNION
	QUAT *	crnt;		// Current QUATERNION to change
	float	time;		// Time
	float	dir;		// Direction

	void	Step();
};


#endif
