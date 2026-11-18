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

	Ray/sphere intersection tests. Both are pure: inputs are not
	modified except the explicit out-parameters.
===================================================================*/
bool RaytoSphere2( VECTOR * sorigin, float radius, VECTOR * rorigin, VECTOR * rdir, VECTOR * inter, VECTOR * inter2 );
bool RaytoSphereShort( VECTOR * sorigin, float radius, VECTOR * rorigin, VECTOR * rdir, float rlength );
