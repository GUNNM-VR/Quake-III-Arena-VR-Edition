#ifndef VRMOD_INPUT_H
#define VRMOD_INPUT_H

#ifndef Q3_VM
#include "../qcommon/q_shared.h"
#endif

#ifndef Com_QueueEvent
#define Com_QueueEvent Sys_QueEvent
#endif

#define MAX_NUM_EYES 2

extern float positional_movementSideways;
extern float positional_movementForward;

typedef enum
{
	V_NULL,
	V_menu,
	V_keyboard,
} V_Type_t;

typedef enum {
	WS_CONTROLLER,
	WS_HMD,
	WS_ALTKEY,
	WS_PREVNEXT,
	WS_ROLL
} weaponSelectorType_t;

typedef enum {
	VRFM_THIRDPERSON_1,		//Camera will auto move to keep up with player
	VRFM_THIRDPERSON_2,		//Camera is completely free movement with the thumbstick
	VRFM_FIRSTPERSON,		//Obvious isn't it?..
	VRFM_NUM_FOLLOWMODES,

	//VRFM_QUERY = 99	//Used to query which mode is active
} vrFollowMode_t;

typedef struct {
    vec3_t actual;
    vec3_t last; // Don't use this, it's for calculating delta
    vec3_t delta;
} angles_t;

typedef struct {
    vec3_t actual;
    vec3_t last; // Don't use this, it's for calculating delta
    vec3_t delta;
} position_t;

enum {
	SideLEFT = 0,
	SideRIGHT = 1,
	SideCOUNT = 2,
};

typedef struct {
	int			buttons;
	int			axisButtons;
    angles_t    angles;
    position_t  position;
    qboolean    isRight;
	vec3_t		hmd_ctrl_offset;	// 6Dof only
	vec3_t		offset_last[2];		// 6Dof only
} vrController_t;

typedef struct {
    float       ipd;
    angles_t    angles;
    position_t  position;

	//test openXR
	float       fov_x;
	float       fov_y;
} hmdinfo_t;

#define THUMB_LEFT  0
#define THUMB_RIGHT 1

typedef struct {
	hmdinfo_t		hmdinfo;
	vrController_t	controllers[2]; // left[0] / right[1]

	vec3_t		rightAngles;
	vec3_t		leftAngles;
	vec3_t		spawnAngles;

	qboolean	recenterAsked;

	qboolean	laserBeam; // qtrue when laser sight is on

	vec3_t		ray_origin; // right weapon muzzle point
	vec3_t 		ray_orientation; // right hand angles

	vec3_t		left_origin; // left weapon muzzle point
	vec3_t 		left_angles; // left hand angles

	int 		mouse_x;
	int 		mouse_y;
	int 		mouse_z; // the intersection point with the menu
	V_Type_t	ray_focused;  // 0: none, 1: menu, 2: keyboard, 3: toolbar

	//-----------------------
	int			vr_controller_type; // 0 : std 2D controller (mouse/keyboard/joypad/touchscreen) / 1 : 3Dof (go gear) / 2 : 6Dof (quest)
	float		fov_x;
	float		fov_y;
	qboolean	weapon_stabilised;
	qboolean	right_handed;
	vrFollowMode_t follow_mode;
#ifdef USE_VR_ZOOM
	qboolean	weapon_zoomed;
	float		weapon_zoomLevel;
#endif
#ifdef USE_WEAPON_WHEEL
	qboolean	weapon_select;
	qboolean	weapon_select_autoclose;

	qboolean	weapon_select_using_thumbstick;
#endif
	vec2_t		thumbstick_location[2]; //left / right thumbstick locations - used in cgame

	int			realign; // used to realign the fake 6DoF playspace in a multiplayer game
	int			clientNum;
	vec3_t		clientviewangles; //orientation in the client - we use this in the cgame
	vec3_t		hmdorigin; //used to recenter the mp fake 6DoF playspace (without vr_heightAdjust anymore)
	vec3_t		calculated_weaponangles; //Calculated as the angle required to hit the point that the controller is pointing at, but coming from the view origin

	int			vrFlags; // can contain flags : EF_LASER_SIGHT | EF_WEAPON_STABILISED | EF_WEAPON_WHEEL | EF_FM_THIRDPERSON_1 / EF_FM_THIRDPERSON_2 / EF_FM_FIRSTPERSON
} vr_clientinfo_t;

#ifdef USE_NATIVE_HACK
vr_clientinfo_t *vrinfo; // VM side
#endif

#ifndef Q3_VM
vr_clientinfo_t vr_info; // client side
#endif

//from drBeef ioq3
enum {
    VR_TOUCH_AXIS_UP            = 1 << 0,
    VR_TOUCH_AXIS_DOWN          = 1 << 1,
    VR_TOUCH_AXIS_LEFT          = 1 << 2,
    VR_TOUCH_AXIS_RIGHT         = 1 << 3,
    VR_TOUCH_AXIS_TRIGGER_INDEX = 1 << 4,
};

// same as oculus VRAPI ovrButton
typedef enum xrButton_ {
	xrButton_A				= 0x00000001, // Set for trigger pulled on the Gear VR and Go Controllers
	xrButton_B				= 0x00000002,
	xrButton_RThumb			= 0x00000004,
	xrButton_RShoulder		= 0x00000008,

	xrButton_X				= 0x00000100,
	xrButton_Y				= 0x00000200,
	xrButton_LThumb			= 0x00000400,
	xrButton_LShoulder		= 0x00000800,

	xrButton_Up				= 0x00010000,
	xrButton_Down			= 0x00020000,
	xrButton_Left			= 0x00040000,
	xrButton_Right			= 0x00080000,
	xrButton_Enter			= 0x00100000, //< Set for touchpad click on the Go Controller, menu button on Left Quest Controller
	xrButton_Back			= 0x00200000, //< Back button on the Go Controller (only set when
	// a short press comes up)
	xrButton_GripTrigger	= 0x04000000, //< grip trigger engaged
	xrButton_Trigger		= 0x20000000, //< Index Trigger engaged
	//xrButton_Joystick		= 0x80000000, //< Click of the Joystick
	xrButton_Joystick		= 0x40000000, //< Click of the Joystick
	//xrButton_EnumSize		= 0x7fffffff
} xrButton;

void VRMOD_CL_GestureCrouchCheck(void );
void VRMOD_CL_Get_HMD_Angles( void );
void VRMOD_CL_Get_HMD_Position( void );
void VRMOD_CL_handle_controllers( void );
void VRMOD_CL_Finish_VR_Move( usercmd_t *cmd );

#ifdef USE_VR_QVM
void VRMOD_CL_KeepRightPos( int x, int y, int z );
void VRMOD_CL_KeepRightAngles( vec3_t angles );
void VRMOD_CL_KeepLeftAngles( vec3_t angles );
#endif

void VRMOD_CL_VRInit( void );
void VRMOD_CL_MouseEvent( int dx, int dy, int dz );
void VRMOD_IN_Joystick(qboolean isRightController, float joystickX, float joystickY );
void VRMOD_IN_Triggers(qboolean isRightController, float index );
void VRMOD_IN_Grab(qboolean isRightController, float index);
void VRMOD_togglePlayerLaserBeam(qboolean pressed);
void VRMOD_Get_HMD_Info(void);

void VRMOD_IN_Button(qboolean isRightController, xrButton button, qboolean isPressed);
#ifdef OCULUSGO
void VRMOD_IN_Trackpad(qboolean padClic, qboolean padTouch, float touchX, float touchY);
#endif

#endif//VRMOD_INPUT_H
