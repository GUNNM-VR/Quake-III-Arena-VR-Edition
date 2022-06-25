#include "VRMOD_common.h"
#include "VRMOD_input.h"

//===========================================================================
// some usefull fonctions
//===========================================================================
// q3 convention:
// 0 => z
// 1 => x
// 2 => y

/**
 * Removes duplicate strings from the array and shifts items left.
 * Returns the number of items in the modified array.
 *
 * Parameters:
 * n_items   - number of items in the array.
 * arr	   - an array of strings with possible duplicates.
 */
 //cf. https://stackoverflow.com/questions/35425255/remove-duplicates-from-array-of-strings-in-c
uint32_t remove_dups(uint32_t *n_items, const char *arr[])
{
	uint32_t i, j = 1, k = 1;

	for (i = 0; i < *n_items; i++)
	{
		for (j = i + 1, k = j; j < *n_items; j++)
		{
			/* If strings don't match... */
			if (strcmp(arr[i], arr[j]))
			{
				arr[k] = arr[j];
				k++;
			}
		}
		*n_items -= j - k;
	}
	return *n_items;
}

void ParseExtensionString(char * extensionNames, uint32_t * numExtensions, const char * extensionArrayPtr[], const uint32_t arrayCount)
{
	uint32_t extensionCount = 0;
	char * nextExtensionName = extensionNames;

	while (*nextExtensionName && (extensionCount < arrayCount))
	{
		extensionArrayPtr[extensionCount++] = nextExtensionName;

		// Skip to a space or null
		while (*(++nextExtensionName))
		{
			if (*nextExtensionName == ' ')
			{
				// Null-terminate and break out of the loop
				*nextExtensionName++ = '\0';
				break;
			}
		}
	}
	*numExtensions = extensionCount;
}

void copyColor(byte src[4], byte dst[4])
{
	dst[1] = src[1];
	dst[2] = src[2];
	dst[3] = src[3];
	dst[4] = src[4];
}

void rotate_pt_YAW(vec3_t in_out, const float angles )
{
	vec3_t rotatedPoint;
	vec3_t dir = {0.0, 0.0, 1.0};

	RotatePointAroundVector(rotatedPoint, dir, in_out, angles);

	in_out[0] = rotatedPoint[0];
	in_out[1] = rotatedPoint[1];
	in_out[2] = rotatedPoint[2];
}

void rotate_pt_PITCH(vec3_t in_out, const float angles )
{
	vec3_t rotatedPoint;
	vec3_t dir = {-1.0, 0.0, 0.0};

	RotatePointAroundVector(rotatedPoint, dir, in_out, angles);

	in_out[0] = rotatedPoint[0];
	in_out[1] = rotatedPoint[1];
	in_out[2] = rotatedPoint[2];
}

void rotate_around_pt_YAW(vec3_t in_out, const vec3_t center, const float angles )
{
	vec3_t rotatedPoint;
	vec3_t dir = {0.0, 0.0, 1.0};
	VectorSubtract(in_out, center, in_out);
	RotatePointAroundVector(rotatedPoint, dir, in_out, angles);
	VectorAdd(rotatedPoint, center, in_out);
}

void rotate_around_pt_PITCH(vec3_t in_out, const vec3_t center, const float angles )
{
	vec3_t rotatedPoint;
	vec3_t dir = {-1.0, 0.0, 0.0};
	VectorSubtract(in_out, center, in_out);
	RotatePointAroundVector(rotatedPoint, dir, in_out, angles);
	VectorAdd(rotatedPoint, center, in_out);
}

void rotateAboutOrigin(float x, float y, float rotation, vec2_t out)
{
	out[0] = cos(DEG2RAD(-rotation)) * x + sin(DEG2RAD(-rotation)) * y;
	out[1] = cos(DEG2RAD(-rotation)) * y - sin(DEG2RAD(-rotation)) * x;
}

void user_facing_quad ( const vec3_t origin_frame, vec3_t point0, vec3_t point1, vec3_t point2, vec3_t point3, const float angle_YAW,
 const float halfWidth, const float halfHeight, const float xStart, const float xEnd, const float z1, const float z2, const float z3, const float z4 )
{
	vec3_t vec0, vec1, vec_z1, vec_z2 ;

	vec0[0] = halfWidth;
	vec0[2] = -halfHeight;
	vec0[1] = z3; // bottom z

	vec1[0] = halfWidth;
	vec1[2] = halfHeight;
	vec1[1] = z4; // top z

	vec_z1[0] = xStart;
	vec_z1[2] = 0.0f;
	vec_z1[1] = z1; // right z

	vec_z2[0] = xEnd;
	vec_z2[2] = 0.0f;
	vec_z2[1] = z2; // left z

	VectorAdd( vec0, vec_z1, point0 );
	VectorAdd( vec0, vec_z2, point1 );
	VectorAdd( vec1, vec_z2, point2 );
	VectorAdd( vec1, vec_z1, point3 );

	// Rotations
	// x axis rotation need origin_frame to be add later
	rotate_pt_YAW( point0, angle_YAW );
	rotate_pt_YAW( point1, angle_YAW );
	rotate_pt_YAW( point2, angle_YAW );
	rotate_pt_YAW( point3, angle_YAW );

	VectorAdd( point0, origin_frame, point0 );
	VectorAdd( point1, origin_frame, point1 );
	VectorAdd( point2, origin_frame, point2 );
	VectorAdd( point3, origin_frame, point3 );
}

//===========================================================================
// rayTriangleIntersect_fast
// return qtrue if vector intersect a triangle
// give barycentre in order to calcul UV of the intersection point
// cf: https://www.scratchapixel.com/lessons/3d-basic-rendering/ray-tracing-rendering-a-triangle/moller-trumbore-ray-triangle-intersection
//===========================================================================
// use of the Möller-Trumbore algorithm
qboolean rayTriangleIntersect( const vec3_t origin, const vec3_t dir, const vec3_t pt0, const vec3_t pt1, const vec3_t pt2, vec2_t uv0, vec2_t uv1, vec2_t uv2, vec3_t intersection_uvz)
{
	const float kEpsilon = 0.000001f;
	vec3_t p0p1, p0p2, pvec;
	vec3_t orig, tvec, qvec;
	float det;
	float t, u, v;

	// origin was multiplied by 1000 to save net bandwidth
	VectorSet(orig, origin[0] * 0.001f, origin[1] * 0.001f, origin[2] * 0.001f);
	
	VectorSubtract( pt1, pt0, p0p1);
	VectorSubtract( pt2, pt0, p0p2);
	CrossProduct(dir, p0p2, pvec);
	det = DotProduct(p0p1, pvec);

	/*#ifdef CULLING 
		// if the determinant is negative the triangle is backfacing
		// if the determinant is close to 0, the ray misses the triangle
		if (det < kEpsilon) return qfalse;
	#else
		// ray and triangle are parallel if det is close to 0
		if (fabs(det) < kEpsilon) return qfalse;
	#endif */

	if (det > -kEpsilon && det < kEpsilon)
		return qfalse;

	float invDet = 1.0f / det;
	VectorSubtract(orig, pt0, tvec);
	u = DotProduct(tvec, pvec) * invDet;

	if (u < 0.0f || u > 1.0f) return qfalse;
 
	CrossProduct(tvec, p0p1, qvec);
	v = DotProduct(dir, qvec) * invDet;

	if (v < 0.0f || u + v > 1.0f) return qfalse;
 
	t = DotProduct(p0p2, qvec) * invDet;

	intersection_uvz[0] = (1.0f - u - v) * uv0[0] + u * uv1[0] + v * uv2[0];
	intersection_uvz[1] = (1.0f - u - v) * uv0[1] + u * uv1[1] + v * uv2[1];
	intersection_uvz[2] = t;

	return qtrue;
}

//===========================================================================
// find intersection point(text coord), and depth
//===========================================================================
qboolean getIntersection( const vec3_t point0, const vec3_t point1, const vec3_t point2, const vec3_t point3, const float s1, const float t1, const float s2, const float t2, vec3_t intersection_uvz )
{
	qboolean hasFocus = qfalse;
	vec2_t uv0, uv1, uv2, uv3;

	// standard square texture coordinates
	uv0[0] = s1;
	uv0[1] = t1;//0.0f;

	uv1[0] = s2;
	uv1[1] = t1;//0.0f;

	uv2[0] = s2;
	uv2[1] = t2;//1.0f;

	uv3[0] = s1;
	uv3[1] = t2;//1.0f;

	//==============================================================
	// detect intersection with a vector
	// vector is defined by its origin and direction
	//==============================================================

	// check if ray inside triangle 1
	hasFocus = rayTriangleIntersect( vr_info.ray_origin, vr_info.ray_orientation, point1, point0, point2, uv1, uv0, uv2, intersection_uvz );
	if (!hasFocus) {
		// check if ray inside triangle 2
		hasFocus = rayTriangleIntersect(vr_info.ray_origin, vr_info.ray_orientation, point3, point2, point0, uv3, uv2, uv0, intersection_uvz );
	}
	return hasFocus;
}

//===========================================================================
// create menu geometry
// call from tr_surface.c/RB_SurfaceEntity()
//===========================================================================
void drawQuad( vec3_t point0, vec3_t point1, vec3_t point2, vec3_t point3, const float s1, const float t1, const float s2, const float t2, const byte color[4] )
{
#ifdef USE_VBO
	VBO_Flush();
#endif

	RB_CHECKOVERFLOW( 4, 6 );

#ifdef USE_VBO
	tess.surfType = SF_TRIANGLES;
#endif

	const uint32_t ndx0 = tess.numVertexes;
	const uint32_t ndx1 = ndx0 + 1;
	const uint32_t ndx2 = ndx0 + 2;
	const uint32_t ndx3 = ndx0 + 3;

	// triangle indexes for a simple quad

	// triangle 1
	tess.indexes[ tess.numIndexes ] = ndx0;
	tess.indexes[ tess.numIndexes + 1 ] = ndx1;
	tess.indexes[ tess.numIndexes + 2 ] = ndx3;

	// triangle 2
	tess.indexes[ tess.numIndexes + 3 ] = ndx3;
	tess.indexes[ tess.numIndexes + 4 ] = ndx1;
	tess.indexes[ tess.numIndexes + 5 ] = ndx2;

	VectorCopy( point0, tess.xyz[ndx0] );
	VectorCopy( point1, tess.xyz[ndx1] );
	VectorCopy( point2, tess.xyz[ndx2] );
	VectorCopy( point3, tess.xyz[ndx3] );

	// standard square texture coordinates
	tess.texCoords[0][ndx0][0] = tess.texCoords[1][ndx0][0] = s1;
	tess.texCoords[0][ndx0][1] = tess.texCoords[1][ndx0][1] = t1;

	tess.texCoords[0][ndx1][0] = tess.texCoords[1][ndx1][0] = s2;
	tess.texCoords[0][ndx1][1] = tess.texCoords[1][ndx1][1] = t1;

	tess.texCoords[0][ndx2][0] = tess.texCoords[1][ndx2][0] = s2;
	tess.texCoords[0][ndx2][1] = tess.texCoords[1][ndx2][1] = t2;

	tess.texCoords[0][ndx3][0] = tess.texCoords[1][ndx3][0] = s1;
	tess.texCoords[0][ndx3][1] = tess.texCoords[1][ndx3][1] = t2;

	// constant color all the way around
	// should this be identity and let the shader specify from entity?
	* ( unsigned int * ) &tess.vertexColors[ndx0] =
	* ( unsigned int * ) &tess.vertexColors[ndx1] =
	* ( unsigned int * ) &tess.vertexColors[ndx2] =
	* ( unsigned int * ) &tess.vertexColors[ndx3] =
			* ( unsigned int * )color;

	tess.numVertexes += 4;
	tess.numIndexes += 6;
}
