#ifndef __VR_COMMON_H__
#define __VR_COMMON_H__

#include "../renderervk/tr_local.h" // for vec3_t (or use qshared?)
#include "VRMOD_input.h"

extern qboolean laserBeamSwitch;

cvar_t 		*vr_controller_type;
cvar_t		*laserBeamRGBA;
cvar_t		*showVirtualKeyboard;
cvar_t 		*VR_hide_torso;
cvar_t 		*VR_angle_hide_torso;
cvar_t      *menu_distance;
cvar_t      *vr_righthanded;
cvar_t      *vr_weaponPitch;
cvar_t      *renderingThirdPerson;

static int in_eventTime = 0;

#define VectorSet(v, x, y, z)	((v)[0]=(x), (v)[1]=(y), (v)[2]=(z))

void rotate_pt_YAW( vec3_t in_out, const float angles );

void rotate_pt_PITCH( vec3_t in_out, const float angles );

void rotate_around_pt_YAW( vec3_t in_out, const vec3_t center, const float angles );

void rotate_around_pt_PITCH( vec3_t in_out, const vec3_t center, const float angles );

qboolean getIntersection( const vec3_t point0, const vec3_t point1, const vec3_t point2, const vec3_t point3, const float s1, const float t1, const float s2, const float t2, vec3_t intersection_uvz );

void drawQuad( vec3_t point0, vec3_t point1, vec3_t point2, vec3_t point3, const float s1, const float t1, const float s2, const float t2, const byte color[4] );

void user_facing_quad ( const vec3_t origin_frame, vec3_t point0, vec3_t point1, vec3_t point2, vec3_t point3, const float angle_YAW,
 const float halfWidth, const float halfHeight, const float xStart, const float xEnd, const float z1, const float z2, const float z3, const float z4 );

void rotateAboutOrigin(float x, float y, float rotation, vec2_t out);

void ParseExtensionString(char * extensionNames, uint32_t * numExtensions, const char * extensionArrayPtr[], const uint32_t arrayCount);
uint32_t remove_dups(uint32_t *n_items, const char *arr[]);

#endif // VR_COMMON