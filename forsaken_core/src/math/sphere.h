#pragma once

/* sphere.h -- ray versus sphere intersection tests
 *
 * Stateless helpers in sphere.cpp, used by bullet, pickup and ship collision.
 * Ray directions must be unit length (the quadratic assumes |dir| = 1). The
 * variants differ on a ray that starts inside the sphere: RaytoSphere2 reports
 * no hit, RaytoSphereShort reports a hit.
 */

#include <math.h>
#include "math/new3d.h"


/*===================================================================
	Prototypes

	Ray/sphere intersection tests. intersect/RaytoSphere return a
	distance along the ray (0 = miss). All functions are pure:
	inputs are not modified except the explicit out-parameters.
===================================================================*/
float isNotZero(float x);
float solveQuadric(float * c, float * s);
float closest_intersection(float *x, float x_num);
float intersect(VECTOR * sphere_pos, float radius , VECTOR * ray_org, VECTOR * ray_dir);
void  SphereNormal( VECTOR * Pos , VECTOR * Point , float radius , NORMAL * Normal );
float RaytoSphere(VECTOR * sphere_pos, float radius , VECTOR * ray_org, VECTOR * ray_dir);
bool RaytoSphere2( VECTOR * sorigin, float radius, VECTOR * rorigin, VECTOR * rdir, VECTOR * inter, VECTOR * inter2 );
bool RaytoSphereShort( VECTOR * sorigin, float radius, VECTOR * rorigin, VECTOR * rdir, float rlength );
