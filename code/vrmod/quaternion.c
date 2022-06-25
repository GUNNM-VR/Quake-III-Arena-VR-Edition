/**
 * Implementation of most relevant functions on quaternions.
 */
#include "quaternion.h"

#include "../vrmod/OpenXR/xr_linear.h"//test

#define EPS     0.0001

//TODO deplacer ds vr_common.c
#ifndef EPSILON
#define EPSILON 0.001f
#endif

// Return scalar product of vector a & b.
//TODO à mettre avec fct vector
float VectorScalarProduct (const vec3_t a, const vec3_t b)
{
	return a[0]*b[0] + a[1]*b[1] + a[2]*b[2];
}

Quaternion QuaternionCreate(vec3_t axis, float angle) {
	Quaternion q;
	q.s = cos (angle/2.0);
	VectorScale(axis, sin(angle/2.0), q.v);
	return q;
}

/**
 * Multiply two quaternions with each other.
 * Careful! Not commutative!!!
 * Calculates: q1 * q2
 */
Quaternion QuaternionMultiply (const Quaternion* q1, const Quaternion* q2)
{
	Quaternion res;
	res.s = q1->s*q2->s - VectorScalarProduct(q1->v, q2->v);
	vec3_t vres;
	CrossProduct(q1->v, q2->v, vres);

	vec3_t tmp;
	VectorScale (q2->v, q1->s, tmp);

	VectorAdd(vres, tmp, vres);
	VectorScale(q1->v, q2->s, tmp);
	VectorAdd(vres, tmp, res.v);
	return res;
}

/**
 * Multiplies a quaternion and a scalar.
 * Therefore the scalar will be converted to a quaternion.
 * After that the two quaternions will be muliplied.
 */
Quaternion QuaternionMultiplyScalar (const Quaternion* q1, float s)
{
	Quaternion q2 = { s, {0.0f, 0.0f, 0.0f} };
	return QuaternionMultiply (q1, &q2);
}

/**
 * Calculates: q1 + q2.
 */
Quaternion QuaternionAdd (const Quaternion* q1, const Quaternion* q2)
{
	Quaternion res;
	res.s = q1->s + q2->s;
	VectorAdd(q1->v, q2->v, res.v);
	return res;
}

/**
 * Calculates q1 - q2.
 */
Quaternion QuaternionSubtract (const Quaternion* q1, const Quaternion* q2)
{
	Quaternion res;
	res.s = q1->s - q2->s;
	VectorSubtract(q1->v, q2->v, res.v);
	return res;
}

/**
 * Complex conjugate the quaternion.
 */
Quaternion QuaternionConjugate (const Quaternion* q1)
{
	Quaternion res;
	res.s = q1->s;
	VectorScale(q1->v, -1.0, res.v);
	return res;
}


// Return quaternion lenght unit
float QuaternionLength (const Quaternion* q1)
{
	return sqrtf (q1->s*q1->s + q1->v[0]*q1->v[0] + q1->v[1]*q1->v[1] + q1->v[2]*q1->v[2]);
}

/**
 * Invert the quaternion.
 */
Quaternion QuaternionInverse (const Quaternion* q1)
{
	float qlen = pow (QuaternionLength (q1), 2);

	Quaternion tmp = QuaternionConjugate(q1);

	return QuaternionMultiplyScalar (&tmp, 1.0 / qlen);
}

/**
 * Normalize the quaternion to a length of 1.
 */
void QuaternionNormalize (Quaternion* q1)
{
	float qlen = QuaternionLength (q1);
	q1->s /= qlen;
	VectorScale(q1->v, 1.0 / qlen, q1->v);
}


/**
 * Check if the quaternion is normalized.
 */
/*int QuaternionIsNormalized (const Quaternion* q1)
{
	float res = q1->s*q1->s + q1->v.x*q1->v.x + q1->v.y*q1->v.y + q1->v.z*q1->v.z;
	return (res + EPS >= 1.0) && (res - EPS <= 1.0);
}*/

/* Some higher level functions, using quaternion */

void QuaternionRotatePoint (Quaternion q, vec3_t point, vec3_t pointOut)
{
	QuaternionNormalize(&q);

	// Create Quaternion of the point to rotate
	Quaternion p = {0.0, {point[0], point[1], point[2]} };

	// The actual calculations.
	//  ---  q p q*  ---
	Quaternion inverseQ = QuaternionInverse(&q);
	Quaternion res = QuaternionMultiply (&q, &p);
	res = QuaternionMultiply (&res, &inverseQ);

	// Write new rotated coordinates back to the point
	VectorCopy(res.v, pointOut);
}

/**
 * Rotates a given point around a given axis by a given angle.
 * The rotations uses quaternion internally and writes the rotated (modified)
 * coordinates back to the point.
 */
void QuaternionRotatePointAroundAxis (vec3_t axis, float angle, vec3_t point, vec3_t pointOut)
{
	// create quaternion from axis and angle
	Quaternion q;
	q.s = cos (angle/2.0);
	VectorScale(axis, sin(angle/2.0), q.v);

	QuaternionRotatePoint(q, point, pointOut);
}


void MatToQuat(float m[4][4], Quaternion * quat)
{
    float tr, s, q[4];
    int i, j, k;
    int nxt[3] = {1, 2, 0};
    tr = m[0][0] + m[1][1] + m[2][2];
    // check the diagonal
    if (tr > 0.0) {
        s = sqrtf (tr + 1.0);
        quat->s = s / 2.0;
        s = 0.5 / s;
        quat->v[0] = (m[1][2] - m[2][1]) * s;
        quat->v[1] = (m[2][0] - m[0][2]) * s;
        quat->v[2] = (m[0][1] - m[1][0]) * s;
    }
    else {
        // diagonal is negative 
        i = 0;
        if (m[1][1] > m[0][0]) i = 1;
        if (m[2][2] > m[i][i]) i = 2;
        j = nxt[i];
        k = nxt[j];
        s = sqrtf ((m[i][i] - (m[j][j] + m[k][k])) + 1.0);

        q[i] = s * 0.5;

        if (s != 0.0) s = 0.5 / s;
        q[3] = (m[j][k] - m[k][j]) * s;
        q[j] = (m[i][j] + m[j][i]) * s;
        q[k] = (m[i][k] + m[k][i]) * s;
        quat->v[0] = q[0];
        quat->v[1] = q[1];
        quat->v[2] = q[2];
        quat->s = q[3];
    }
}


void Quaternion_MultiplyMatrix( /*const*/ float m[4][4], const Quaternion v, Quaternion *out ) {
	  out->v[0] = m[0][0] * v.v[0] + m[0][1] * v.v[1] + m[0][2] * v.v[2] + m[0][3] * v.s;
	  out->v[1] = m[1][0] * v.v[0] + m[1][1] * v.v[1] + m[1][2] * v.v[2] + m[1][3] * v.s;
	  out->v[2] = m[2][0] * v.v[0] + m[2][1] * v.v[1] + m[2][2] * v.v[2] + m[2][3] * v.s;
	  out->s    = m[3][0] * v.v[0] + m[3][1] * v.v[1] + m[3][2] * v.v[2] + m[3][3] * v.s;
}

void Quaternion_MultiplyMatrix_toVec3( const float m[4][4], const vec3_t v, vec3_t out ) {
	out[0] = m[2][0] * v[0] + m[2][1] * v[1] + m[2][2] * v[2];
	out[1] = m[0][0] * v[0] + m[0][1] * v[1] + m[0][2] * v[2];
	out[2] = m[1][0] * v[0] + m[1][1] * v[1] + m[1][2] * v[2];
}

// Returns the 4x4 rotation matrix for the given quaternion input as array of float
void matrix_CreateFromQuaternionArray( const float * quat_farray, float out[4][4] )
{
	Quaternion q;

	q.v[0] = quat_farray[0];
	q.v[1] = quat_farray[1];
	q.v[2] = quat_farray[2];
	q.s    = quat_farray[3];

	matrix_CreateFromQuaternion( &q, out );
}

// Returns the 4x4 rotation matrix for the given quaternion.
void matrix_CreateFromQuaternion( const Quaternion * q, float out[4][4] )
{
	const float ww = q->s * q->s;
	const float xx = q->v[0] * q->v[0];
	const float yy = q->v[1] * q->v[1];
	const float zz = q->v[2] * q->v[2];

	out[0][0] = ww + xx - yy - zz;
	out[0][1] = 2 * ( q->v[0] * q->v[1] - q->s * q->v[2] );
	out[0][2] = 2 * ( q->v[0] * q->v[2] + q->s * q->v[1] );
	out[0][3] = 0;

	out[1][0] = 2 * ( q->v[0] * q->v[1] + q->s * q->v[2] );
	out[1][1] = ww - xx + yy - zz;
	out[1][2] = 2 * ( q->v[1] * q->v[2] - q->s * q->v[0] );
	out[1][3] = 0;

	out[2][0] = 2 * ( q->v[0] * q->v[2] - q->s * q->v[1] );
	out[2][1] = 2 * ( q->v[1] * q->v[2] + q->s * q->v[0] );
	out[2][2] = ww - xx - yy + zz;
	out[2][3] = 0;

	out[3][0] = 0;
	out[3][1] = 0;
	out[3][2] = 0;
	out[3][3] = 1;
}

void NormalizeAngles( vec3_t angles )
{
	while (angles[0] >= 90)  angles[0] -= 180;
	while (angles[1] >= 180) angles[1] -= 360;
	while (angles[2] >= 180) angles[2] -= 360;
	while (angles[0] < -90)  angles[0] += 180;
	while (angles[1] < -180) angles[1] += 360;
	while (angles[2] < -180) angles[2] += 360;
}

void VectorsToAngles( const vec3_t forward, const vec3_t right, const vec3_t up, vec3_t angles )
{
	float sr, sp, sy, cr, cp, cy;

	sp = -forward[2];

	float cp_x_cy = forward[0];
	float cp_x_sy = forward[1];
	float cp_x_sr = -right[2];
	float cp_x_cr = up[2];

	float yaw = atan2(cp_x_sy, cp_x_cy);
	float roll = atan2(cp_x_sr, cp_x_cr);

	cy = cos(yaw);
	sy = sin(yaw);
	cr = cos(roll);
	sr = sin(roll);

	if (fabs(cy) > EPSILON) {
		cp = cp_x_cy / cy;
	}
	else if (fabs(sy) > EPSILON) {
		cp = cp_x_sy / sy;
	}
	else if (fabs(sr) > EPSILON) {
		cp = cp_x_sr / sr;
	}
	else if (fabs(cr) > EPSILON) {
		cp = cp_x_cr / cr;
	}
	else {
		cp = cos(asin(sp));
	}

	float pitch = atan2(sp, cp);

	angles[0] = -pitch / (M_PI*2.f / 360.f);
	angles[1] =  yaw   / (M_PI*2.f / 360.f);
	angles[2] =  roll  / (M_PI*2.f / 360.f);

	NormalizeAngles(angles);
}

void QuatToAngles(float * q, vec3_t out)
{
	float mat[4][4];
	matrix_CreateFromQuaternionArray(q, mat);

	vec3_t forward, right, up;
	const vec3_t qforward = { 0, 0, 1 };
	const vec3_t qright = { 1, 0, 0 };
	const vec3_t qup = { 0, 1, 0 };

	Quaternion_MultiplyMatrix_toVec3(mat, qforward, forward);
	Quaternion_MultiplyMatrix_toVec3(mat, qright, right);
	Quaternion_MultiplyMatrix_toVec3(mat, qup, up);

	VectorsToAngles(forward, right, up, out);
}

