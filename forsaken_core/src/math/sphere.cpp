
/*===================================================================
		Include File...	
===================================================================*/

#include "game/main.h"
#include "math/sphere.h"

/************************************************************************
*									*
* This method computes the intersection between a ray and the sphere.	*
* It returns the distance between the origin of the ray and the closest *
* point of intersection (or 0.0 if no intersection occurs).		*
*									*
************************************************************************/


/************************************************************************
*									*
* This method computes the normal vector to the sphere at the point of	*
* intersection. (NB: normalization is "built-in" for the sphere).	*
*									*
************************************************************************/

//vec3 Sphere::normalAt(vec3& p)
//{ return (p - pos) / radius; }

/********************************************************
*							*
* This function determines if a float is small enough	*
* to be zero. The purpose of the subroutine is to try	*
* to overcome precision problems in math routines.	*
*							*
********************************************************/


/********************************************************
*							*
* This function determines the roots of a quadric	*
* equation.						*
* It takes as parameters a pointer to the three		*
* coefficient of the quadric equation (the c[2] is the	*
* coefficient of x2 and so on) and a pointer to the	*
* two element array in which the roots are to be	*
* placed.						*
* It outputs the number of roots found.			*
*							*
********************************************************/


/************************************************************************
*									*
* This method receives as parameter a list of intersections and the	*
* number of intersections in the list. It scans the list and returns	*
* the closest positive intersection, or 0.0 if no such intersection	*
* exists.								*
*									*
************************************************************************/


/*--------------------------------------------------------------------------------------------------------------------------
 * RAY -> SPHERE collision: RADIUS = 256
 *
 * Input:
 *		VECTOR *	sorigin	-	Origin of SPHERE
 *		float		radius	-	Radius of SPHERE
 *		VECTOR *	rorigin	-	Origin of RAY
 *		VECTOR *	rdir	-	Direction of RAY (Normalised)
 *		VECTOR *	inter	-	Intersection
 *		VECTOR *	inter	-	Intersection Back
 *
 * Output:
 *		bool		hit		-	Collision True/False
 *
 *-------------------------------------------------------------------------------------------------------------------------*/
bool RaytoSphere2( VECTOR * sorigin, float radius, VECTOR * rorigin, VECTOR * rdir, VECTOR * inter, VECTOR * inter2 )
{
	float		v;
	float		disc;
	float		d, d2;
	float		d0;
	float		d1;
	VECTOR		EO;

	EO.x = (rorigin->x - sorigin->x);
	EO.y = (rorigin->y - sorigin->y);
	EO.z = (rorigin->z - sorigin->z);

	if( VectorLength( &EO ) <= radius ) return false; // Vector Started inside Sphere!

	v = ( ( EO.x * rdir->x ) + ( EO.y * rdir->y ) + ( EO.z * rdir->z ) );

	disc = ( ( radius * radius ) - ( ( ( EO.x * EO.x ) + ( EO.y * EO.y ) + ( EO.z * EO.z ) ) - ( v * v ) ) );

	if ( disc < 0.0F ) return false;

	d  = (float) sqrtf( disc );
	d0 = d - v;
	d1 = ( -d ) - v;

	d  = (d0 < d1) ? d0 : d1;
	d2  = (d0 > d1) ? d0 : d1;

	if ( d < 0.0F )	return false;

	inter->x = rorigin->x + (rdir->x * d);
	inter->y = rorigin->y + (rdir->y * d);
	inter->z = rorigin->z + (rdir->z * d);

	inter2->x = rorigin->x + (rdir->x * d2);
	inter2->y = rorigin->y + (rdir->y * d2);
	inter2->z = rorigin->z + (rdir->z * d2);

	return true;
}

/*===================================================================
	Procedure	:	Ray To Sphere Short
	Input		:	VECTOR *	SphereCenter
				:	float		Radius
				:	VECTOR *	RayStart
				:	VECTOR *	RayDirection
				:	float		RayLength
	Output		:	bool		True/False
===================================================================*/
bool RaytoSphereShort( VECTOR * sorigin, float radius, VECTOR * rorigin, VECTOR * rdir, float rlength )
{
	float		v;
	float		disc;
	float		d, d0, d1;
	float		EO_LS;								// EO Length Squared
	VECTOR		EO;

	EO.x = ( rorigin->x - sorigin->x );
	EO.y = ( rorigin->y - sorigin->y );
	EO.z = ( rorigin->z - sorigin->z );

	EO_LS = ( EO.x * EO.x ) + ( EO.y * EO.y ) + ( EO.z * EO.z );

	if( EO_LS <= ( radius * radius ) ) return( true ); // Started inside?

	v = ( ( EO.x * rdir->x ) + ( EO.y * rdir->y ) + ( EO.z * rdir->z ) );

	disc = ( ( radius * radius ) - ( EO_LS - ( v * v ) ) );

	if( disc < 0.0F ) return( false );				// Ray does not hit sphere

	d  = (float) sqrtf( disc );
	d0 = d - v;
	d1 = ( -d ) - v;

	d  = (d0 < d1) ? d0 : d1;

	if ( d < 0.0F )	return( false );				// Ray Opposit Dir
	if( d > rlength ) return( false );				// Ray was Short!

	return true;
}
