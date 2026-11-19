
#include "game/main.h"
#include <stdio.h>
#include "math/new3d.h"

/*===================================================================
	Sin/Cos lookup table — 1024 entries covering [0, 2*PI)
	Linear interpolation gives < 4.7e-6 max error vs sinf().
	4KB footprint fits in L1 cache (ARM11 has 16KB L1D).

	Only the table storage and its one-shot builder live here; the
	fast_sinf / fast_cosf accessors are static inline in new3d.h so
	the call sites don't pay an out-of-line call.  See the comment
	block there for why the range reduction must not use floorf() on
	VFPv2.
===================================================================*/

float fast_sin_table[TRIG_TABLE_SIZE + 1]; /* +1 for interpolation at boundary */

void trig_table_init(void)
{
	int i;
	for (i = 0; i <= TRIG_TABLE_SIZE; i++)
		fast_sin_table[i] = sinf((float)i * (2.0f * 3.14159265358979323846f / TRIG_TABLE_SIZE));
}

/* fast_invsqrtf moved to new3d.h as a static inline wrapping the VFPv2
 * hardware vsqrt.f32 (1.0f/sqrtf).  The Quake III bit-hack that used to
 * live here forced int<->VFP register crossings that stall the ARM1176
 * VFP11 pipeline; the hardware path is both faster and exact.  See the
 * comment on the declaration in new3d.h. */

/*===================================================================
	Globals
===================================================================*/
u_int16_t	Seed1 = 0x1234;
u_int16_t	Seed2 = 0x4321;

MATRIX	MATRIX_Identity = {
			1.0F, 0.0F, 0.0F, 0.0F,
			0.0F, 1.0F, 0.0F, 0.0F,
			0.0F, 0.0F, 1.0F, 0.0F,
			0.0F, 0.0F, 0.0F, 1.0F };

/*===================================================================
	Procedure	:	Build Rotation Matrix
	Input		:	float		X Rotations ( Degrees )
				:	float		Y Rotations ( Degrees )
				:	float		Z Rotations ( Degrees )
				:	MATRIX	*	New Matrix
	Output		:	Nothing
===================================================================*/
void BuildRotMatrix( float xa, float ya, float za, MATRIX * m )
{
	float	c0, c1, c2, s0, s1, s2, c0s1, s0s1;

	/* One shared range reduction per angle (fast_sincosf). */
	fast_sincosf( D2R( xa ), &s0, &c0 );
	fast_sincosf( D2R( ya ), &s1, &c1 );
	fast_sincosf( D2R( za ), &s2, &c2 );

	c0s1 = c0 * s1;
	s0s1 = s0 * s1;

	m->_11 = c1 * c2;
	m->_12 = -c1 * s2;
	m->_13 = -s1;
	m->_14 = 0.0F;

	m->_21 = ( ( c0 * s2 ) - ( s0s1 * c2 ) );
	m->_22 = ( ( s0s1 * s2 ) + ( c0 * c2 ) );
	m->_23 = -s0 * c1;
	m->_24 = 0.0F;

	m->_31 = ( ( c0s1 * c2 ) + ( s0 * s2 ) );
	m->_32 = ( ( s0 * c2 ) - ( c0s1 * s2 ) );
	m->_33 = c0 * c1;
	m->_34 = 0.0F;

	m->_41 = 0.0F;
	m->_42 = 0.0F;
	m->_43 = 0.0F;
	m->_44 = 1.0F;

}

/*===================================================================
	Function	:	Matrix Multiplication
	Input		:	MATRIX	*	Matrix 1
				:	MATRIX	*	Matrix 2
				:	MATRIX	*	Result Matrix
	Output		:	Nothing
===================================================================*/
void MatrixMultiply( MATRIX * m0, MATRIX * m1, MATRIX * m0m1 )
{
	float	a, b, c, d, e, f, g, h, i;
	float	A, B, C, D, E, F, G, H, I;
	float   j, k, l, m;
	float   J, K, L, M;
	float	n, o, p;
	float	N, O, P;

	a = m0->_11; b = m0->_12; c = m0->_13;
	d = m0->_21; e = m0->_22; f = m0->_23;
	g = m0->_31; h = m0->_32; i = m0->_33;

	A = m1->_11; B = m1->_12; C = m1->_13;
	D = m1->_21; E = m1->_22; F = m1->_23;
	G = m1->_31; H = m1->_32; I = m1->_33;

	j = m0->_41; k = m0->_42; l = m0->_43; m = m0->_44;
	J = m1->_41; K = m1->_42; L = m1->_43; M = m1->_44;

	n = m0->_14; o = m0->_24; p = m0->_34;
	N = m1->_14; O = m1->_24; P = m1->_34;


	m0m1->_11 = ( A * a + D * b + G * c + J * n );
	m0m1->_12 = ( B * a + E * b + H * c + K * n );
	m0m1->_13 = ( C * a + F * b + I * c + L * n );
	m0m1->_14 = ( N * a + O * b + P * c + M * n );

	m0m1->_21 = ( A * d + D * e + G * f + J * o );
	m0m1->_22 = ( B * d + E * e + H * f + K * o );
	m0m1->_23 = ( C * d + F * e + I * f + L * o );
	m0m1->_24 = ( N * d + O * e + P * f + M * o );


	m0m1->_31 = ( A * g + D * h + G * i + J * p );
	m0m1->_32 = ( B * g + E * h + H * i + K * p );
	m0m1->_33 = ( C * g + F * h + I * i + L * p );
	m0m1->_34 = ( N * g + O * h + P * i + M * p );


	m0m1->_41 = ( j * A + k * D + l * G + m * J );
	m0m1->_42 = ( j * B + k * E + l * H + m * K );
	m0m1->_43 = ( j * C + k * F + l * I + m * L );
	m0m1->_44 = ( j * N + k * O + l * P + m * M );
}

/*===================================================================
	Function	:	Apply Matrix to a Vector
	Input		:	MATRIX	*	Matrix
				:	VECTOR	*	Vert
				:	VECTOR	*	New Vert
	Output		:	Nothing
===================================================================*/

// this is how you really apply a matrix to a vector

// this is not a general purpose m*v function
// it's meant to only be used in certain places

void ApplyMatrix( MATRIX * m, VECTOR * v0, VECTOR * v1 )
{
	float x = m->_41;
	float y = m->_42;
	float z = m->_43;

	x += ( ( m->_11 * v0->x ) + ( m->_12 * v0->y ) + ( m->_13 * v0->z ) );
	y += ( ( m->_21 * v0->x ) + ( m->_22 * v0->y ) + ( m->_23 * v0->z ) );
	z += ( ( m->_31 * v0->x ) + ( m->_32 * v0->y ) + ( m->_33 * v0->z ) );

	v1->x = x;
	v1->y = y;
	v1->z = z;
}

/*===================================================================
	Function	:	Apply Matrix to a Vector
	Input		:	MATRIX	*	Matrix
				:	VECTOR	*	Vert
				:	VECTOR	*	New Vert
	Output		:	Nothing
===================================================================*/
void VisPolyApplyMatrix( MATRIX * m, VECTOR * v0, VECTOR * v1 )
{
	float w;
	v1->x = ( ( m->_11 * v0->x ) + ( m->_21 * v0->y ) + ( m->_31 * v0->z ) +m->_41 );
	v1->y = ( ( m->_12 * v0->x ) + ( m->_22 * v0->y ) + ( m->_32 * v0->z ) +m->_42);
	v1->z = ( ( m->_13 * v0->x ) + ( m->_23 * v0->y ) + ( m->_33 * v0->z ) +m->_43);
	w	  = ( ( m->_14 * v0->x ) + ( m->_24 * v0->y ) + ( m->_34 * v0->z ) +m->_44);
	v1->x /= w;
	v1->y /= w;
	v1->z /= w;
}


/*===================================================================
	Procedure	:	Calculate the Cross product of 2 Vectors
	Input		:	VECTOR	*	Vector1
				:	VECTOR	*	Vector2
				:	VECTOR	*	New Vector
	Output		:	Nothing
===================================================================*/
void CrossProduct( VECTOR * a, VECTOR * b, VECTOR * ab )
{
	ab->x = ( a->y * b->z ) - ( a->z * b->y );
	ab->y = ( a->z * b->x ) - ( a->x * b->z );
	ab->z = ( a->x * b->y ) - ( a->y * b->x );
}
/*===================================================================
	Procedure	:	Calculate the Dod product of 2 VECTORS
	Input		:	VECTOR * a
				:	VECTOR * b
	Output		:	float		Dot Product
===================================================================*/
float DotProduct( VECTOR * a , VECTOR * b )
{
	return( ( a->x * b->x ) +
			( a->y * b->y ) +
			( a->z * b->z ) );
}

/*===================================================================
	Procedure	:	Reflect Vector off a surface normal
	Input		:	VECTOR	*	Vector
				:	NORMAL	*	Normal
				:	VECTOR	*	New Vector
	Output		:	Nothing
===================================================================*/
void ReflectVector( VECTOR * old, NORMAL * normal, VECTOR * new1 )
{
	float        lambda;

	lambda= ( -2 * ( (normal->nx * old->x ) + (normal->ny * old->y ) + (normal->nz * old->z ) ) );

	if( fabsf( lambda ) < EPS )
	{
		new1->x = old->x;
		new1->y = old->y;
		new1->z = old->z;
	}
	else
	{
		new1->x = ( old->x + ( lambda * normal->nx ) );
		new1->y = ( old->y + ( lambda * normal->ny ) );
		new1->z = ( old->z + ( lambda * normal->nz ) );
	}
}


/*===================================================================
	Procedure	:	Normalises Vector V
	Input		:	VECTOR	*	Vector
	Output		:	Nothing
===================================================================*/
void NormaliseVector( VECTOR *  v )
{
	float dot = v->x * v->x + v->y * v->y + v->z * v->z;
	float inv;

	if ( dot < 1e-14f )
		return;

	/* dot is a guarded sum of squares, so GCC proves it > 0 and emits a
	 * bare vsqrt.f32 + vdiv.f32 here (no errno branch, no register
	 * crossing).  This is now identical to fast_invsqrtf (which is also
	 * just 1.0f/sqrtf on VFPv2) — the old Quake III bit-hack that
	 * fast_invsqrtf used to be was deliberately avoided at this site:
	 * its ~0.175% error left vectors at length ~0.99825, and
	 * QuatFrom2Vectors feeds the cross product of two such vectors into
	 * asinf(), which is highly sensitive near 1 (a 0.35% magnitude error
	 * became ~5 degrees of angle error -> crooked doors, drifting train
	 * paths).  With the bit-hack retired that hazard is gone everywhere. */
	inv = 1.0f / sqrtf( dot );
	v->x *= inv;
	v->y *= inv;
	v->z *= inv;
}


/*===================================================================
	Procedure	:	Calculate the Distance between a VERT and a VECTOR
	Input		:	VERT	*	VERTEX1
				:	VECTOR	*  VECTOR
	Output		:	float		Dot Product
===================================================================*/
float VectorLength( VECTOR * v )
{
	return( (float) sqrtf( ( v->x * v->x ) + ( v->y * v->y ) + ( v->z * v->z ) ) );
}

/*===================================================================
	Procedure	:	Calculate the Distance between a VERT and a VECTOR
	Input		:	VERT	*	VERTEX1
				:	VECTOR	*  VECTOR
	Output		:	float		Dot Product
===================================================================*/
float DistanceVert2Vector( VERT *  a , VECTOR * b )
{
	float x;
	float y;
	float z;

	x = a->x - b->x;
	y = a->y - b->y;
	z = a->z - b->z;

	return sqrtf( x*x + y*y + z*z );
}
/*===================================================================
	Procedure	:	Calculate the Distance between a VERT and a VECTOR
	Input		:	VECTOR	*  VECTOR
				:	VECTOR	*  VECTOR
	Output		:	float		Dot Product
===================================================================*/
float DistanceVector2Vector( VECTOR *  a , VECTOR * b )
{
	float x;
	float y;
	float z;

	x = a->x - b->x;
	y = a->y - b->y;
	z = a->z - b->z;

	return sqrtf( x*x + y*y + z*z );
}

/*===================================================================
	Function	:	Matrix Transpose
	Input		:	MATRIX	*	Matrix 1
				:	MATRIX	*	Matrix 2
	Output		:	Nothing
===================================================================*/
void MatrixTranspose( MATRIX * m1, MATRIX * m2 )
{
	MATRIX	T;

	T = *m1;


	m2->_11 = T._11;	m2->_12 = T._21;	m2->_13 = T._31;	m2->_14 = T._41;
	m2->_21 = T._12;	m2->_22 = T._22;	m2->_23 = T._32;	m2->_24 = T._42;
	m2->_31 = T._13;	m2->_32 = T._23;	m2->_33 = T._33;	m2->_34 = T._43;
	m2->_41 = T._14;	m2->_42 = T._24;	m2->_43 = T._34;	m2->_44 = T._44;
}

/*===================================================================
	Function	:	Calculate Random Number within range
	Input		:	u_int16_t		Max
	Output		:	Nothing
===================================================================*/
u_int16_t	Random_Range( u_int16_t Max )
{
	u_int16_t	bit1, bit2;

	if ( !Max )
		return 0;
	Seed1	^=	Seed2;
	Seed1	+=	7;
	Seed2	-=	0x5347;
	Seed2	^=	0x7c9f;
	bit1	=	( Seed1 >> 15 );
	bit2	=	( Seed2 << 15 );
	Seed1	<<=	1;
	Seed2	>>=	1;
	Seed1	|=	bit1;
	Seed2	|=	bit2;

	/* R11: exact (Seed1 & 32767) % Max without the __aeabi_idivmod call and
	 * its spill barrier (ARMv6 has no integer divide). x = Seed1 & 32767 is
	 * in [0,32767] and Max in [1,65535], both exact in float32. The truncated
	 * float quotient can only undershoot the true floor if x = k*Max - 1 with
	 * 1/Max below half-ULP(k), which needs k*Max >= 2^24 while x <= 32767
	 * forces k*Max <= 32768 — impossible. Exhaustively verified on host for
	 * all 2,147,450,880 (x,Max) pairs: zero mismatches. measured n/a pending
	 * hardware capture. Verified 2026-10-25. */
	{
		unsigned	x	= (unsigned)( Seed1 & 32767 );
		int		q	= (int)( (float)x / (float)Max );
		return (u_int16_t)( x - (unsigned)Max * (unsigned)q );
	}
}


float Random_Range_Float( float Max )
{
	float Rand;
	Rand = (float) Random_Range( 32767 );
	Rand /= 32767.0F;
	return (Max * Rand);
}


void MakeViewMatrix(VECTOR *viewpos, VECTOR *lookpos, VECTOR *up, MATRIX *view)
{
	VECTOR vx, vy, vz;

	CalcViewAxes(viewpos, lookpos, up, &vx, &vy, &vz);
	view->_11 = vx.x;
	view->_12 = vy.x;
	view->_13 = vz.x;
	view->_14 = 0.0F;
	view->_21 = vx.y;
	view->_22 = vy.y;
	view->_23 = vz.y;
	view->_24 = 0.0F;
	view->_31 = vx.z;
	view->_32 = vy.z;
	view->_33 = vz.z;
	view->_34 = 0.0F;
	view->_41 = 0.0F;
	view->_42 = 0.0F;
	view->_43 = 0.0F;
	view->_44 = 1.0F;
}


/*===================================================================
	Procedure	:	Calculate View Axes
	Input		:	VECTOR	*	Position
				:	VECTOR	*	Direction Vector
				:	VECTOR	*	Up Vector
				:	VECTOR	*	New X Axis Vector
				:	VECTOR	*	New Y Axis Vector
				:	VECTOR	*	New Z Axis Vector
	Output		:	Nothing
===================================================================*/
void CalcViewAxes( VECTOR * viewpos, VECTOR * lookpos, VECTOR * up,
				   VECTOR * vx, VECTOR * vy, VECTOR * vz )
{

	/*	Calculate camera z axis as normalised vector from viewpos to lookpos */

	vz->x = (lookpos->x - viewpos->x);
	vz->y = (lookpos->y - viewpos->y);
	vz->z = (lookpos->z - viewpos->z);
	NormaliseVector( vz );

	/*	Calculate camera x axis as normalised cross product of camera z axis and up vector */

	CrossProduct( up, vz, vx);
	NormaliseVector( vx );

	/*	Calculate camera y axis as normalised cross product of camera z and x axes */

	CrossProduct( vz, vx, vy);
	NormaliseVector( vy );
}



/* rotate_matrix

   compute rotation matrix required

   to rotate a vector about given axis by given angle

*/

/*===================================================================
	Function	:	Create matrix to rotate around arbitary axis
	Input		:	float		Angle
				:	VECTOR		Axis
				:	MATRIX	*	Matrix
	Output		:	Nothing
===================================================================*/
void MatrixFromAxisAndAngle( float angle, VECTOR * axis, MATRIX * rot )
{

	MATRIX	rotx, rotangle;
	float	xz, xyz, s, c;

	xz	= (float) sqrtf( axis->x * axis->x + axis->z * axis->z );
	xyz	= (float) sqrtf( axis->x * axis->x + axis->y * axis->y + axis->z * axis->z ) ;

	if( xz != 0.0F )
	{
		rotx._11 = ( axis->x / xyz );
		rotx._12 = -( axis->x * axis->y ) / ( xz * xyz );
		rotx._13 = -( axis->z / xz );
		rotx._14 = 0.0F;

		rotx._21 = ( axis->y / xyz );
		rotx._22 = ( xz / xyz );
		rotx._23 = 0.0F;
		rotx._24 = 0.0F;

		rotx._31 = ( axis->z / xyz );
		rotx._32 = -( axis->y * axis->z ) / ( xz * xyz );
		rotx._33 = ( axis->x / xz );
		rotx._34 = 0.0F;

		rotx._41 = 0.0F;
		rotx._42 = 0.0F;
		rotx._43 = 0.0F;
		rotx._44 = 1.0F;
	}
	else
	{
		rotx._11 = 0.0F;
		rotx._12 = -( axis->y / xyz );
		rotx._13 = 0.0F;
		rotx._14 = 0.0F;

		rotx._21 = ( axis->y / xyz );
		rotx._22 = 0.0F;
		rotx._23 = 0.0F;
		rotx._24 = 0.0F;

		rotx._31 = 0.0F;
		rotx._32 = 0.0F;
		rotx._33 = 1.0F;
		rotx._34 = 0.0F;

		rotx._41 = 0.0F;
		rotx._42 = 0.0F;
		rotx._43 = 0.0F;
		rotx._44 = 1.0F;
	}

	c = (float) fast_cosf( angle );
	s = (float) fast_sinf( angle );

	rotangle._11 = 1.0F;
	rotangle._12 = 0.0F;
	rotangle._13 = 0.0F;
	rotangle._14 = 0.0F;

	rotangle._21 = 0.0F;
	rotangle._22 = c;
	rotangle._23 = s;
	rotangle._24 = 0.0F;

	rotangle._31 = 0.0F;
	rotangle._32 = -s;
	rotangle._33 = c;
	rotangle._34 = 0.0F;

	rotangle._41 = 0.0F;
	rotangle._42 = 0.0F;
	rotangle._43 = 0.0F;
	rotangle._44 = 1.0F;

	MatrixMultiply( &rotx, &rotangle, rot );
	MatrixTranspose( &rotx, &rotx );
	MatrixMultiply( rot, &rotx, rot );
}

/*
 * Calculates a point along a B-Spline curve defined by four points. p
 * n output, contain the point. t                                Position
 * along the curve between p2 and p3.  This position is a float between 0
 * and 1. p1, p2, p3, p4    Points defining spline curve. p, at parameter
 * t along the spline curve
 */
void
spline(VECTOR * p, float t, VECTOR * p1, VECTOR * p2, VECTOR * p3, VECTOR * p4)
{
	float t2, t3, m1, m2, m3, m4;

	t2 = t * t;
	t3 = t2 * t;

	m1 = ( -1.0F * t3) + ( 2.0F  * t2) + ( -1.0F * t);
	m2 = ( 3.0F  * t3) + ( -5.0F * t2) + 2.0F;
	m3 = ( -3.0F * t3) + ( 4.0F  * t2) + t;
	m4 = t3 + ( -1.0F * t2);

	m1 *= 0.5;
	m2 *= 0.5;
	m3 *= 0.5;
	m4 *= 0.5;

	p->x = p1->x * m1 + p2->x * m2 + p3->x * m3 + p4->x * m4;
	p->y = p1->y * m1 + p2->y * m2 + p3->y * m3 + p4->y * m4;
	p->z = p1->z * m1 + p2->z * m2 + p3->z * m3 + p4->z * m4;

}
