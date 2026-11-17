

/*===================================================================
	Includes
===================================================================*/
#include "game/main.h"
#include	<math.h>
#include	"math/new3d.h"
#include	"math/quat.h"
#include <stdio.h>
#include "game/oct2.h"


void QUAT::MakeEuler( float XRot, float YRot, float ZRot )
{
    float  cosZRot, sinZRot, cosYRot, sinYRot, cosXRot, sinXRot;
    float  half_XRot, half_YRot, half_ZRot;
	/* put angles into radians and divide by two, since all angles in formula
	 *  are (angle/2)
	 */
	
	half_ZRot = ZRot * 0.5F;
	half_YRot = YRot * 0.5F;
	half_XRot = XRot * 0.5F;
	
	cosZRot = (float) COSD(half_ZRot);
	sinZRot = (float) SIND(half_ZRot);
	
	cosYRot = (float) COSD(half_YRot);
	sinYRot = (float) SIND(half_YRot);
	
	cosXRot = (float) COSD(half_XRot);
	sinXRot = (float) SIND(half_XRot);
	
	
	x = sinXRot * cosYRot * cosZRot - cosXRot * sinYRot * sinZRot;
	y = cosXRot * sinYRot * cosZRot + sinXRot * cosYRot * sinZRot;
	z = cosXRot * cosYRot * sinZRot - sinXRot * sinYRot * cosZRot;
	
	w = cosXRot * cosYRot * cosZRot + sinXRot * sinYRot * sinZRot;

}	/* q_from_euler */
/*===================================================================
	Procedure	:	Multiply 2 Quaternions together creating 3rd
	Input		:	QUAT	*	Quaternion2
				:	QUAT	*	New Quaternion
	Output		:	Nothing
===================================================================*/
void QUAT::Multiply( const QUAT * q2, QUAT * q1q2 ) const
{
	float	q1_w, q1_x, q1_y, q1_z;
	float	q2_w, q2_x, q2_y, q2_z;

	q1_w = w;	q1_x = x;	q1_y = y;	q1_z = z;
	q2_w = q2->w;	q2_x = q2->x;	q2_y = q2->y;	q2_z = q2->z;

	q1q2->w = ( (q1_w * q2_w) - (q1_x * q2_x) - (q1_y * q2_y) - (q1_z * q2_z) );
	q1q2->x = ( (q1_w * q2_x) + (q1_x * q2_w) + (q1_y * q2_z) - (q1_z * q2_y) );
	q1q2->y = ( (q1_w * q2_y) + (q1_y * q2_w) + (q1_z * q2_x) - (q1_x * q2_z) );
	q1q2->z = ( (q1_w * q2_z) + (q1_z * q2_w) + (q1_x * q2_y) - (q1_y * q2_x) );
}

/*===================================================================
	Procedure	:	Multiply X Quaternion by Y Quaternion creating
				:	new XY quaternion
	Input		:	QUAT	*	Y Quaternion
				:	QUAT	*	New Quaternion
	Output		:	Nothing
===================================================================*/
void QUAT::MultiplyX_Y( const QUAT * qy, QUAT * qxy ) const
{
	float	x_w, x_x, x_y, x_z;
	float	y_w, y_x, y_y, y_z;

	x_w = w;	x_x = x;	x_y = y;	x_z = z;
	y_w = qy->w;	y_x = qy->x;	y_y = qy->y;	y_z = qy->z;

	qxy->w = ( ( x_w * y_w ) );
	qxy->x = ( ( x_x * y_w ) );
	qxy->y = ( ( x_w * y_y ) );
	qxy->z = ( ( x_x * y_y ) );
}

/*===================================================================
	Procedure	:	Multiply XY Quaternion by Z Quaternion creating
				:	new XYZ Quaternion
	Input		:	QUAT	*	Z Quaternion
				:	QUAT	*	New Quaternion
	Output		:	Nothing
===================================================================*/
void QUAT::MultiplyXY_Z( const QUAT * qz, QUAT * qxyz ) const
{
	float	xy_w, xy_x, xy_y, xy_z;
	float	z_w, z_x, z_y, z_z;

	xy_w = w;	xy_x = x;	xy_y = y;	xy_z = z;
	z_w = qz->w;	z_x = qz->x;	z_y = qz->y;	z_z = qz->z;

	qxyz->w = ( ( xy_w * z_w ) - ( xy_z * z_z ) );
	qxyz->x = ( ( xy_x * z_w ) + ( xy_y * z_z ) );
	qxyz->y = ( ( xy_y * z_w ) - ( xy_x * z_z ) );
	qxyz->z = ( ( xy_w * z_z ) + ( xy_z * z_w ) );
}

/*===================================================================
	Procedure	:	Normalise a Quaternion
	Input		:	Nothing (operates on this)
	Output		:	Nothing
===================================================================*/
void QUAT::Normalise()
{
	float 	len;
	VECTOR	sq;

	sq.x = ( x * x );
	sq.y = ( y * y );
	sq.z = ( z * z );

	{
		float dot = ( sq.x + sq.y + sq.z ) + ( w * w );
		if ( dot > 1e-14f )
		{
			len = fast_invsqrtf( dot );
			x *= len;
			y *= len;
			z *= len;
			w *= len;
		}
	}			   
}

/*===================================================================
	Procedure	:	NEW Create Matrix from Quaternion
	Input		:	MATRIX	*	Matrix
	Output		:	Nothing
===================================================================*/
void QUAT::ToMatrix( MATRIX * m ) const
{
	float	xs, ys, zs;
	float	wx, wy, wz;
	float	xx, xy, xz;
	float	yy, yz, zz;

	xs = x + x;
	ys = y + y;
	zs = z + z;

	wx = w * xs;
	wy = w * ys;
	wz = w * zs;

	xx = x * xs;
	xy = x * ys;
	xz = x * zs;

	yy = y * ys;
	yz = y * zs;
	zz = z * zs;

	m->_11 = ( 1.0F - ( yy + zz ) );
	m->_12 = ( xy - wz );
	m->_13 = ( xz + wy );
	m->_14 = 0.0F;

	m->_21 = ( xy + wz );
	m->_22 = ( 1.0F - ( xx + zz ) );
	m->_23 = ( yz - wx );
	m->_24 = 0.0F;

	m->_31 = ( xz - wy );
	m->_32 = ( yz + wx );
	m->_33 = ( 1.0F - ( xx + yy ) );
	m->_34 = 0.0F;

	m->_41 = 0.0F;
	m->_42 = 0.0F;
	m->_43 = 0.0F;
	m->_44 = 1.0F;
}

/*===================================================================
	Procedure	:	Quaternion Dot Product
	Input		:	QUAT	*	Quaternion2
	Output		:	float		Result
===================================================================*/
float QUAT::Dot( const QUAT * q2 ) const
{
	return ( ( x * q2->x ) + ( y * q2->y ) +
			 ( z * q2->z ) + ( w * q2->w ) );
}

/*===================================================================
	Procedure	:	Quaternion Interpolation
	Input		:	Nothing (operates on this QUATLERP)
	Output		:	Nothing
===================================================================*/
void QUATLERP::Step()
{
	float	 	alpha;
	float	 	beta;
	QUAT	*	start;
	QUAT	*	end;
	QUAT	*	result;
	
	alpha  =	time;
	start  =	&this->start;
	end	   =	&this->end;
	result =	crnt;

  	beta   =	( 1.0F - alpha );

/*===================================================================
	Check which direction we are going around hypersphere ...
===================================================================*/
	if ( dir < 0.0F ) alpha = -alpha;

/*===================================================================
	Interpolate ...
===================================================================*/

	result->x = ( ( beta * start->x ) ) + ( ( alpha * end->x ) );
	result->y = ( ( beta * start->y ) ) + ( ( alpha * end->y ) );
	result->z = ( ( beta * start->z ) ) + ( ( alpha * end->z ) );
	result->w = ( ( beta * start->w ) ) + ( ( alpha * end->w ) );
					 
	result->Normalise();
}

/*===================================================================
	Procedure	:	Create Quaternion from Vector
	Input		:	VECTOR	*	Vector
	Output		:	Nothing
===================================================================*/
void QUAT::FromVector( VECTOR * Tv )
{
	float	angle;
	VECTOR	Av;

/*===================================================================
	Normalise TARGET Vector
===================================================================*/
	NormaliseVector( Tv );

/*===================================================================
	Create AXIS vector
===================================================================*/
	Av.x = Tv->y;
	Av.y = -Tv->x;
	Av.z = 0.0F;
	NormaliseVector( &Av );

/*===================================================================
	Calculate ANGLE between TARGET and LOOK vectors
===================================================================*/
	/* @vfp-exempt R6 — inverse trig has no LUT form (fast trig is sin/cos only); LUT-izing is new numeric work needing a fidelity capture. measured n/a. Verified 2026-10-25. */
	angle = (float) ( acosf( Tv->z ) / 2 );

/*===================================================================
	Finally build TARGET QUATERNION
===================================================================*/
	x = (float) ( fast_sinf( angle ) * Av.x );
	y = (float) ( fast_sinf( angle ) * Av.y );
	z = 0.0F;
	w = (float) fast_cosf( angle );

	Normalise();
}

/*===================================================================
	Procedure	:	Create Quaternion from Vector ( Uuuummmmm )
	Input		:	VECTOR	*	Vector
	Output		:	Nothing
===================================================================*/
void QUAT::FromVector2( VECTOR * Tv )
{
	float	angle;
	VECTOR	Av;

/*===================================================================
	Normalise TARGET Vector
===================================================================*/
	NormaliseVector( Tv );

/*===================================================================
	Create AXIS vector
===================================================================*/
	Av.x = Tv->y;
	Av.y = -Tv->x;
	Av.z = 0.0F;
	NormaliseVector( &Av );

/*===================================================================
	Calculate ANGLE between TARGET and LOOK vectors
===================================================================*/
	/* @vfp-exempt R6 — inverse trig has no LUT form (fast trig is sin/cos only); LUT-izing is new numeric work needing a fidelity capture. measured n/a. Verified 2026-10-25. */
	angle = (float) ( acosf( -Tv->z ) / 2 );

/*===================================================================
	Finally build TARGET QUATERNION
===================================================================*/
	x = (float) ( fast_sinf( angle ) * Av.x );
	y = (float) ( fast_sinf( angle ) * Av.y );
	z = 0.0F;
	w = (float) fast_cosf( angle );

	Normalise();
}

/*===================================================================
	Procedure	:		Quaternion Spherical Interpolation (Slerp)
	Input		:		float		alpha ( interpolation parameter (0 to 1) )
				:		QUAT	*	b ( end unit quaternions )
				:		QUAT	*	q ( output interpolated quaternion )
				:		int		spin ( number of extra spin rotations )
	Output		:	Nothing
===================================================================*/
void QUAT::Slerp( float alpha, const QUAT * b, QUAT * q, int spin ) const
{
	float	beta;			/* complementary interp parameter */
	float	theta;			/* angle between A and B */
	float	sin_t, cos_t;	/* sine, cosine of theta */
	float	phi;			/* theta plus spins */
	int		bflip;			/* use negation of B? */

	cos_t = Dot( b );

	/* if B is on opposite hemisphere from A, use -B instead */
 	if( cos_t < 0.0F )
	{
		cos_t = -cos_t;
		bflip = true;
	}
	else
	{
		bflip = false;
	}

	/* if B is (within precision limits) the same as A,
	 * just linear interpolate between A and B.
	 * Can't do spins, since we don't know what direction to spin.
	 */
	if( ( 1.0F - cos_t ) < EPS )
	{
		beta = ( 1.0F - alpha );
 	}
	else
	{				/* normal case */
		/* @vfp-exempt R6 — acosf has no LUT form (fast trig is sin/cos only);
		 * quaternion slerp angle per-ship-per-frame; LUT-izing inverse trig is
		 * new numeric work needing a fidelity capture. measured n/a. Verified 2026-10-25. */
 		theta	= ( (float) acosf( cos_t ) );
 		phi		= ( theta + ( spin * M_PI ) );
 		sin_t	= ( (float) fast_sinf( theta ) );
 		beta	= ( (float) fast_sinf( theta - ( alpha * phi ) ) / sin_t );
 		alpha	= ( (float) fast_sinf( alpha * phi ) / sin_t );
 	}

	if( bflip ) alpha = -alpha;

	/* interpolate */
 	q->x = ( ( beta * x ) + ( alpha * b->x ) );
 	q->y = ( ( beta * y ) + ( alpha * b->y ) );
 	q->z = ( ( beta * z ) + ( alpha * b->z ) );
 	q->w = ( ( beta * w ) + ( alpha * b->w ) );
}

/*===================================================================
	Procedure	:		Invert Quaternion
				:		Create it's multiplicative inverse.
	Input		:		QUAT	*	Destin Quaternion
	Output		:	Nothing
===================================================================*/
void QUAT::Inverse( QUAT * d ) const
{
    float	sNorm;

	sNorm = ( 1.0F / ( ( x * x ) +
					   ( y * y ) +
					   ( z * z ) +
					   ( w * w ) ) );

	d->x = -x * sNorm;
	d->y = -y * sNorm;
	d->z = -z * sNorm;
	d->w =  w * sNorm;

}

/*===================================================================
	Procedure	:		create a quaternion from two vectors that rotates
    	    	     	v1 to v2 about an axis perpendicular to both
	Input		:		VECTOR * v1
				:		VECTOR * v2
	Output		:	Nothing (operates on this)
===================================================================*/
void QUAT::From2Vectors( const VECTOR * v1, const VECTOR * v2 )
{
	VECTOR	u1, u2;
	VECTOR	axis;					 /* axis of rotation */
	float	theta;				     /* angle of rotation about axis */
	float	theta_complement;
	float	crossProductMagnitude;
/*
** Normalize both vectors and take cross product to get rotation axis. 
*/
	u1 = *v1;
	u2 = *v2;

	NormaliseVector( &u1 );
	NormaliseVector( &u2 );

	CrossProduct( &u1 , &u2, &axis );
/*
** | u1 X u2 | = |u1||u2|fast_sinf(theta)
**
** Since u1 and u2 are normalized, 
**
**  theta = arcsin(|axis|)
*/
	crossProductMagnitude = (float) sqrtf( DotProduct( &axis , &axis ) );

/*
** Occasionally, even though the vectors are normalized, the magnitude will
** be calculated to be slightly greater than one.  If this happens, just
** set it to 1 or asinf() will barf.
*/
	if( crossProductMagnitude > 1.0F )
	   crossProductMagnitude = 1.0F;
/*
** Take arcsin of magnitude of rotation axis to compute rotation angle.
** Since crossProductMagnitude=[0,1], we will have theta=[0,pi/2].
*/
	/* @vfp-exempt R6 — asinf has no LUT form; quaternion build from vector
	 * pair, fidelity-sensitive orientation math. measured n/a. Verified 2026-10-25. */
	theta = (float) asinf( crossProductMagnitude );
	theta_complement = PI - theta;
/*
** If fast_cosf(theta) < 0, use complement of theta as rotation angle.
*/
	if( DotProduct( &u1 , &u2 ) < 0.0F )
	{
		theta = theta_complement;
		theta_complement = PI - theta;
	}

/* if angle is 0, just return identity quaternion   */
	if( theta < EPS )
	{
		x = 0.0F;
		y = 0.0F;
		z = 0.0F;
		w = 1.0F;
	}else{
		if( theta_complement < EPS )
		{
			/*
			** The two vectors are opposed.  Find some arbitrary axis vector.
			** First try cross product with x-axis if u1 not parallel to x-axis.
			*/
			if( (u1.y*u1.y + u1.z*u1.z) >= EPS )
			{
				axis.x = 0.0F ;
				axis.y = u1.z ;
				axis.z = u1.y ;
			}else{
				/*
				** u1 is parallel to to x-axis.  Use z-axis as axis of rotation.
				*/
				axis.x = axis.y = 0.0F ;
				axis.z = 1.0F ;
			}
		}
		NormaliseVector( &axis );
		MakeAxis( axis.x, axis.y, axis.z, theta ) ;
		Normalise();
   }
}


/*===================================================================
	Procedure	:		make a quaternion given an axis and an angle (in radians)
				    	notes:
					    - rotation is counter-clockwise when rotation axis vector is 
					    	pointing at you
				    	    - if angle or vector are 0, the identity quaternion is returned.
	Input		:		float x , y , z , angle
	Output		:	Nothing (operates on this)
===================================================================*/
void QUAT::MakeAxis( float x, float y, float z, float angle)
{
    float length, cosA, sinA;
	/* normalize vector */
	{
		float dot = x*x + y*y + z*z;
		float inv;

		/* if zero vector passed in, just return identity quaternion	*/
		if ( dot < EPS * EPS )
		{
			this->x = 0.0F;
			this->y = 0.0F;
			this->z = 0.0F;
			this->w = 1.0F;
			return;
		}
		inv = fast_invsqrtf( dot );
		x *= inv;
		y *= inv;
		z *= inv;
	}

	cosA = (float) fast_cosf(angle * 0.5F);
	sinA = (float) fast_sinf(angle * 0.5F);

	this->w = cosA;
	this->x = sinA * x;
	this->y = sinA * y;
	this->z = sinA * z;

}

/*===================================================================
	Procedure	:		Convert GL 4x4 row-major 
    	    	    	rotation matrix to unit quaternion.
	Input		:		MATRIX	*	SrcMatrix
	Output		:	Nothing (operates on this)
===================================================================*/
void QUAT::FromMatrix( const MATRIX * srcMatrix)
{
    float	trace, s;
    int     i, j, k;
	int 	next[3] = {1, 2, 0};
	float	Temp[4][4];
	trace = srcMatrix->_11 + srcMatrix->_22 + srcMatrix->_33;

if (trace > 0.0F)
    {
    s = sqrtf(trace + 1.0F);
    w = s * 0.5F;
    s = 0.5F / s;
    
    x = (srcMatrix->_23 - srcMatrix->_32) * s;
    y = (srcMatrix->_31 - srcMatrix->_13) * s;
    z = (srcMatrix->_12 - srcMatrix->_21) * s;
    } 
else 
    {

	Temp[0][0]=srcMatrix->_11;Temp[0][1]=srcMatrix->_12;Temp[0][2]=srcMatrix->_13;Temp[0][3]=srcMatrix->_14;
	Temp[1][0]=srcMatrix->_21;Temp[1][1]=srcMatrix->_22;Temp[1][2]=srcMatrix->_23;Temp[1][3]=srcMatrix->_24;
	Temp[2][0]=srcMatrix->_31;Temp[2][1]=srcMatrix->_32;Temp[2][2]=srcMatrix->_33;Temp[2][3]=srcMatrix->_34;
	Temp[3][0]=srcMatrix->_41;Temp[3][1]=srcMatrix->_42;Temp[3][2]=srcMatrix->_43;Temp[3][3]=srcMatrix->_44;

    
	i = 0;
    if (Temp[1][1] > Temp[0][0])
	i = 1;
    if (Temp[2][2] > Temp[i][i])
	i = 2;
    j = next[i];  
    k = next[j];
    
    s = (float) sqrtf( (Temp[i][i] - (Temp[j][j]+Temp[k][k])) + 1.0F );
	if( i == 0)
		x = s*0.5F;
	if( i == 1)
		y = s*0.5F;
	if( i == 2)
		z = s*0.5F;
    
    s = 0.5F / s;
    
    w = (Temp[j][k] - Temp[k][j]) * s;
	if( j == 0)
		x = (Temp[i][j] + Temp[j][i]) * s;
	if( j == 1)
		y = (Temp[i][j] + Temp[j][i]) * s;
	if( j == 2)
		z = (Temp[i][j] + Temp[j][i]) * s;

	if( k == 0)
		x = (Temp[i][k] + Temp[k][i]) * s;
	if( k == 1)
		y = (Temp[i][k] + Temp[k][i]) * s;
	if( k == 2)
		z = (Temp[i][k] + Temp[k][i]) * s;
    }
}

/*===================================================================
	Procedure	:		Create Quat from dir vector and up vector
	Input		:		VECTOR	*	Direction Vector
				:		VECTOR	*	Up Vector
	Output		:	Nothing (operates on this)
===================================================================*/
void QUAT::FromDirAndUp( const VECTOR * Dir, const VECTOR * Up )
{
	MATRIX		TempMat;
	VECTOR		TempUp;
	QUAT		RotQuat;

	From2Vectors( &Forward, Dir );
	ToMatrix( &TempMat );
	ApplyMatrix( &TempMat, &SlideUp, &TempUp );
	RotQuat.From2Vectors( &TempUp, Up );
	RotQuat.Multiply( this, this );
}
