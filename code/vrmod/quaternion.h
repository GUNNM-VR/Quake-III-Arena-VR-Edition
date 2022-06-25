#ifndef __Quaternion_H__
#define __Quaternion_H__
/**
 * Interface for some operations on Quaternions.
 */

#include "../qcommon/q_shared.h" // for vec3_t

typedef struct
{
    float s;
    vec3_t v;
} Quaternion;

/* Low level operations on Quaternions */

Quaternion QuaternionCreate(vec3_t axis, float angle);

Quaternion QuaternionMultiply (const Quaternion* q1, const Quaternion* q2);

Quaternion QuaternionMultiplyScalar (const Quaternion* q1, float s);

Quaternion QuaternionAdd (const Quaternion* q1, const Quaternion* q2);

Quaternion QuaternionSubtract (const Quaternion* q1, const Quaternion* q2);

Quaternion QuaternionConjugate (const Quaternion* q1);

Quaternion QuaternionInverse (const Quaternion* q1);

void QuaternionNormalize (Quaternion* q1);

float QuaternionLength (const Quaternion* q1);

int QuaternionIsNormalized (const Quaternion* q1);

/* Some higher level functions, using Quaternions */
void Quaternion_MultiplyMatrix_toVec3( const float m[4][4], const vec3_t v, vec3_t out );

void QuaternionRotatePoint(Quaternion q, vec3_t point, vec3_t pointOut);
void QuaternionRotatePointAroundAxis (vec3_t axis, float angle, vec3_t point, vec3_t pointOut);
void VectorsToAngles( const vec3_t forward, const vec3_t right, const vec3_t up, vec3_t angles );
//void Quaternion_fromEulerZYX(double eulerZYX[3], Quaternion* output);
//void Quaternion_toEulerZYX(Quaternion* q, double output[3]);
void Quaternion_MultiplyMatrix( /*const*/ float m[4][4], const Quaternion v, Quaternion *out );
void matrix_CreateFromQuaternion( const Quaternion * q, float out[4][4] );
void QuatToAngles( float* q, vec3_t out );
void NormalizeAngles( vec3_t angles );

//void QuatToYawPitchRoll(float * q, vec3_t rotation, vec3_t out);
//void QuatToYawPitchRoll(float * q, vec3_t out);
#endif

