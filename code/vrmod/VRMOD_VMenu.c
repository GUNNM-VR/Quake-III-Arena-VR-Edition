#include "VRMOD_VMenu.h"
#include "../client/client.h" // test pour clc.state / cvar_t
#include "../renderervk/tr_local.h"
#include "VRMOD_common.h"

#if defined(__ANDROID__)
#include "../android/android_local.h"
#endif

cvar_t *menu_distance;
cvar_t *menu_width;
cvar_t *menu_height;
cvar_t *menu_planes_nb; // menu number of quad
// Values for menu curvature
cvar_t *menu_h_radius; // menu ellipse horizontal(x) radius
cvar_t *menu_v_radius; // menu ellipse vertical(y) radius

				   
/*
==================
MENU_Init
==================
*/
void MENU_Init( void ) {
	menu_distance 	= Cvar_Get ("menu_distance", "80.0", CVAR_ARCHIVE | CVAR_LATCH);
	menu_width 		= Cvar_Get ("menu_width", "64", CVAR_ARCHIVE | CVAR_LATCH);
	menu_height 	= Cvar_Get ("menu_height", "48", CVAR_ARCHIVE | CVAR_LATCH);
	menu_planes_nb 	= Cvar_Get ("menu_planes_nb", "10", CVAR_ARCHIVE | CVAR_LATCH);
	menu_h_radius 	= Cvar_Get ("menu_h_radius", "31.5", CVAR_ARCHIVE | CVAR_LATCH);
	menu_v_radius 	= Cvar_Get ("menu_v_radius", "16.5", CVAR_ARCHIVE | CVAR_LATCH);
}


/*
==================
 RB_Surface_VR_main_menu_floor
 create out game menu floor geometry
==================
*/
void RB_Surface_VR_main_menu_floor( void )
{
	const float width = 32.0f;
	const float height = 24.0f;
	//const byte color[4] = {255, 255, 255, 255};
	color4ub_t color;
	color.u32 = 0xFFFFFFFF;

	const vec3_t left = { -10000.0f, 0.0f,  0.0f};
	const vec3_t up   = { 0.0f, 10000.0f, 0.0f};

	const refEntity_t *e = &backEnd.currentEntity->e;

	RB_AddQuadStampExt( e->origin, left, up, color, 0, 0, width * 10, height * 10 );
}


/*
==================
RB_Surface_VR_menu_plane
==================
*/
void RB_Surface_VR_menu_plane( void )
{
	vec3_t origin;
	qboolean needIntersection;
	qboolean hasFocus;
	float s1, s2, t1, t2;

	vec3_t point0, point1, point2, point3;

	const byte color[4] = {255, 255, 255, 255};
	vec3_t intersection_uvz;

	float halfWidth = menu_width->value / 2;
	float halfHeight = menu_height->value / 2;

	const refEntity_t *e = &backEnd.currentEntity->e;

	if (vr_info.ray_focused == V_menu) {
		vr_info.ray_focused = V_NULL;
		vr_info.mouse_z = 0; // mouse_z zero => cursor isn't in the plane
	}

	// some precalc
	// e->rotation from CG_VR_Menu() or CL_create_menu_scene()
	//float angle_deg = AngleNormalize180( 270.0f + e->rotation);

	float angle_deg = e->rotation - 90 ;
	//float angle_deg = M_PI * e->rotation / 180;

	t1 = 0.0f;
	t2 = 1.0f;

	VectorCopy(e->origin, origin);

	// TODO follow y axis if not VR

	// fixme: when menu is not player's one, set needIntersection = qfalse
	// myself = (cent->currentState.number == cg.snap->ps.clientNum);

	needIntersection = qtrue;

	/*qboolean onPause = (cls.state == CA_ACTIVE && (cl_paused && cl_paused->integer));
	if (cls.state != CA_ACTIVE || onPause)
		needIntersection = qtrue;*/

	/*if (cls.state == CA_LOADING || cls.state == CA_CHALLENGING || cls.state == CA_PRIMED)
		needIntersection = qfalse; // in main menu, but no action required
	else*/
	if (cls.state == CA_ACTIVE)
		needIntersection = qtrue; // in game menu
	else if (cls.state == CA_DISCONNECTED) // main menu
		needIntersection = qtrue;
	else
		needIntersection = qfalse;


	// at least avoid mirrors & portals
	if (backEnd.viewParms.portalView == PV_MIRROR || backEnd.viewParms.portalView == PV_PORTAL) {
		needIntersection = qfalse;
	}

	// width of one plane
	float Xdiv = (menu_width->value / menu_planes_nb->integer);

	float Vradius = menu_v_radius->value;
	float Hradius = menu_h_radius->value;

	for (int i = 0; i < menu_planes_nb->integer; i++)
	{
		float xStart = i * Xdiv;
		float xEnd = (i + 1) * Xdiv;

		float z1 = Vradius / Hradius * sqrtf(fabs(pow(Hradius, 2) - pow( (xStart - halfWidth), 2))) - Vradius;
		float z2 = Vradius / Hradius * sqrtf(fabs(pow(Hradius, 2) - pow( (xEnd   - halfWidth), 2))) - Vradius;

		user_facing_quad( origin, point0, point1, point2, point3, angle_deg, -halfWidth, halfHeight, xStart, xEnd, z1, z2, 0, 0 );

		s1 = xStart / menu_width->value;
		s2 = xEnd / menu_width->value;

        drawQuad( point0, point1, point2, point3, s1, t2, s2, t1, color );

		VectorClear(intersection_uvz);

		if ( needIntersection ) {
			hasFocus = getIntersection( point0, point1, point2, point3, s1, t1, s2, t2, intersection_uvz);
			if ( hasFocus ) {
				vr_info.mouse_x = (int) ( intersection_uvz[0] * 640.0f) - 1;
				vr_info.mouse_y = (int) ( 480.0f- intersection_uvz[1] * 480.0f ) - 1;
				vr_info.mouse_z = (int) ( intersection_uvz[2] ) - 1;
				// don't check intersection in others planes
				needIntersection = qfalse;
				vr_info.ray_focused = V_menu;
			}
		}
	}
}
