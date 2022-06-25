#include "VRMOD_input.h"

#include "../client/client.h"

#ifdef USE_VRAPI
#include "../vrmod/VRMOD_common.h"
#endif

#ifdef USE_VRAPI
#include "VRAPI/VRAPI_qinput.h"
#endif

#ifdef USE_OPENXR
#include "OpenXR/OpenXR_qinput.h"
#endif

#ifndef Com_QueueEvent
#define Com_QueueEvent Sys_QueEvent
#endif

float positional_movementSideways = 0.0f;
float positional_movementForward = 0.0f;

qboolean laserBeamSwitch = qtrue;

/*
====================
VRMOD_CL_VRInit
====================
*/
static cvar_t *vr_gesture_roll_delay          = NULL; // millisecond
static cvar_t *vr_gesture_roll_angle          = NULL; // degree
static cvar_t *vr_gesture_roll_angle_reset    = NULL; // degree
static cvar_t *vr_gesture_crouchThreshold     = NULL; // float
#ifdef USE_VR
extern cvar_t *vr_sendRollToServer;
#endif

void VRMOD_CL_VRInit( void )
{
    vr_controller_type          = Cvar_Get("vr_controller_type", "0", CVAR_USERINFO);
#ifdef USE_NATIVE_HACK
	vr_info.vr_controller_type = vr_controller_type->integer;
#endif
    laserBeamRGBA               = Cvar_Get("laserBeamRGBA", "FF0000", CVAR_USERINFO );
    vr_righthanded              = Cvar_Get("vr_righthanded", "1", CVAR_ARCHIVE);
    vr_gesture_roll_delay       = Cvar_Get("vr_gesture_roll_delay", "50", CVAR_USERINFO );
    vr_gesture_roll_angle       = Cvar_Get("vr_gesture_roll_angle", "70", CVAR_USERINFO );
    vr_gesture_roll_angle_reset = Cvar_Get("vr_gesture_roll_angle_reset", "45", CVAR_USERINFO );
    vr_gesture_crouchThreshold  = Cvar_Get("vr_gesture_crouchThreshold", "0.3", CVAR_USERINFO );
	vr_weaponPitch				= Cvar_Get("vr_weaponPitch", "-20", CVAR_ARCHIVE); // not used
}

/*
====================
Gesture
====================
*/

// use controller rotation (Roll axis) to change weapon.
static void VRMOD_checkGestureWeaponChange( void )
{
    static int timeElapsed;
    static qboolean nextWeaponState;
    static qboolean prevWeaponState;

    const float ctrlAnglesRoll = vr_info.controllers[SideRIGHT].angles.actual[ROLL];

    // Set event time for next frame to earliest possible time an event could happen
    in_eventTime = Sys_Milliseconds();

    // keep track of orientation in order to compare angle/times
    // Reset time if roll is finished
    if ( (int)ctrlAnglesRoll > -vr_gesture_roll_angle_reset->integer && (int)ctrlAnglesRoll < vr_gesture_roll_angle_reset->integer )
        timeElapsed = in_eventTime;

    // Next Weapon
    qboolean newNextWeapon = ( (int)ctrlAnglesRoll < vr_gesture_roll_angle->integer );
    if (nextWeaponState != newNextWeapon) {
        // check time of previous angle range, in order to detect fast ROLL ctrl movement
        int duration = in_eventTime - timeElapsed;
        if (duration < vr_gesture_roll_delay->integer && timeElapsed != 0 ) {
            Com_QueueEvent( in_eventTime, SE_KEY, K_MWHEELDOWN, nextWeaponState, 0, NULL );
            nextWeaponState = newNextWeapon;
            timeElapsed = 0;
        }
    }

    // Prev Weapon
    qboolean newPrevWeapon = ( (int)ctrlAnglesRoll > -vr_gesture_roll_angle->integer ) ;
    if (prevWeaponState != newPrevWeapon) {
        int duration = in_eventTime - timeElapsed;
        if (duration < vr_gesture_roll_delay->integer && timeElapsed != 0) {
            Com_QueueEvent( in_eventTime, SE_KEY, K_MWHEELUP, prevWeaponState, 0, NULL );
            prevWeaponState = newPrevWeapon;
            timeElapsed = 0;
        }
    }
}

//=============================================================
// Recenter stuff
//=============================================================
#ifdef USE_VR
static float hmd_stand_height;

static void ResetCrouchHeight( void ) {
    Com_QueueEvent(in_eventTime, SE_KEY, K_C, qfalse, 0, NULL);

	// reset crouch detection
    //hmd_stand_height = vr_info.hmdinfo.position.actual[1];
    hmd_stand_height = vr_info.hmdinfo.position.actual[1] + hmd_stand_height;
}

void VRMOD_recenterView( void ) {
    VectorCopy(vr_info.controllers[SideRIGHT].angles.actual, vr_info.rightAngles);
    VectorSet(vr_info.controllers[SideRIGHT].angles.delta, 0.0f, 0.0f, 0.0f);
    VectorCopy(vr_info.controllers[SideRIGHT].angles.actual, vr_info.controllers[SideRIGHT].angles.last);
    VectorSet(vr_info.hmdinfo.angles.delta, 0.0f, 0.0f, 0.0f);
    VectorCopy(vr_info.hmdinfo.angles.actual, vr_info.hmdinfo.angles.last);
    ResetCrouchHeight();
    vr_info.recenterAsked = qfalse;
}

//=============================================================
// Gesture Crouch
//=============================================================
void VRMOD_CL_GestureCrouchCheck( void )
{
    const qboolean playerIsCrouch = ( cl.snap.ps.pm_flags & PMF_DUCKED );
    const float viewHeight = (hmd_stand_height - vr_info.hmdinfo.position.actual[1]);
    // player crouch irl
    if ( !playerIsCrouch && (viewHeight > vr_gesture_crouchThreshold->value) ) {
        Com_QueueEvent(in_eventTime, SE_KEY, K_C, qtrue, 0, NULL);
    }
    // player stand irl
    else if ( playerIsCrouch && (viewHeight < vr_gesture_crouchThreshold->value) ) {
        Com_QueueEvent(in_eventTime, SE_KEY, K_C, qfalse, 0, NULL);
    }
}
#endif
//=============================================================
// Joystick (joypad, trackpad, VR Controller)
//=============================================================
const float pressedThreshold = 0.75f;
const float releasedThreshold = 0.5f;
const float moveThreshold = 0.05f;
//const int snap = 45;
extern cvar_t *vr_snapturn;

// used with vr controller thumb stick, gamepad thumb stick, Oculus GO trackPad
void VRMOD_IN_Joystick(qboolean isRightController, float joystickX, float joystickY )
{
	//if (joystickX == 0.0f && joystickY == 0.0f)
	//	return;
	int snap = vr_snapturn->integer;

	if ( vr_controller_type->integer >= 2 ) {
		vr_info.thumbstick_location[isRightController][0] = joystickX;
		vr_info.thumbstick_location[isRightController][1] = joystickY;
	}

    //TODO Use thumbstick UP/DOWN as PAGEUP/PAGEDOWN in menus

	if ( isRightController != (vr_righthanded->integer != 0) )
	{
		// off hand controller (Left Controller)
        vec3_t positional;
        VectorClear(positional);

		if ( vr_controller_type->integer == 1 ) {
			//Positional movement, for 6Dof to play 3Dof mod
			float factor = 60.0f;
			rotateAboutOrigin(-vr_info.hmdinfo.position.delta[0] * factor, vr_info.hmdinfo.position.delta[2] * factor, -vr_info.hmdinfo.position.delta[YAW], positional);
		}
		else if ( vr_controller_type->integer >= 2 ) {
			//HMD Based
			vec2_t joystick;// TODO use this with Com_QueueEvent()
			rotateAboutOrigin(joystickX, joystickY, vr_info.hmdinfo.angles.actual[YAW], joystick);
		}

        // Avoid always running
        if (positional[0] < moveThreshold && positional[0] > -moveThreshold) {
            positional[0]  = 0.0f;
        }
        if (positional[1] < moveThreshold && positional[1] > -moveThreshold) {
            positional[1] = 0.0f;
        }

        //sideways
        Com_QueueEvent(in_eventTime, SE_JOYSTICK_AXIS, 0, (joystickX + positional[0]) * 127.0f, 0, NULL);

        //forward/back
        Com_QueueEvent(in_eventTime, SE_JOYSTICK_AXIS, 1, (joystickY + positional[1]) * 127.0f, 0, NULL);
	}
    else
	{
		// Weapon hand controller
#ifdef USE_WEAPON_WHEEL
		if ( vr_info.weapon_select )
			return;
#endif

        //vrController_t* controller = isRightController == qtrue ? &vr_info.controllers[SideRIGHT] : &vr_info.controllers[SideLEFT]; // TODO
        vrController_t* controller = &vr_info.controllers[SideRIGHT];


        if (!(controller->axisButtons & VR_TOUCH_AXIS_RIGHT) && joystickX > pressedThreshold) {
            cl.viewangles[YAW]  -= snap;
            vr_info.rightAngles[YAW] -= snap;
            controller->axisButtons |= VR_TOUCH_AXIS_RIGHT;
        } else if ((controller->axisButtons & VR_TOUCH_AXIS_RIGHT) && joystickX < releasedThreshold) {
            controller->axisButtons &= ~VR_TOUCH_AXIS_RIGHT;
        }

        if (!(controller->axisButtons & VR_TOUCH_AXIS_LEFT) && joystickX < -pressedThreshold) {
            cl.viewangles[YAW]  += snap;
            vr_info.rightAngles[YAW] += snap;
            controller->axisButtons |= VR_TOUCH_AXIS_LEFT;
        } else if ((controller->axisButtons & VR_TOUCH_AXIS_LEFT) && joystickX > -releasedThreshold) {
            controller->axisButtons &= ~VR_TOUCH_AXIS_LEFT;
        }


        //Default up/down on right thumbstick is weapon switch
        if (!(controller->axisButtons & VR_TOUCH_AXIS_UP) && joystickY > pressedThreshold)
        {
            Com_QueueEvent( in_eventTime, SE_KEY, K_MWHEELDOWN, qtrue, 0, NULL );
            controller->axisButtons |= VR_TOUCH_AXIS_UP;
        }
        else if ((controller->axisButtons & VR_TOUCH_AXIS_UP) && joystickY < releasedThreshold)
        {
            Com_QueueEvent( in_eventTime, SE_KEY, K_MWHEELDOWN, qfalse, 0, NULL );
            controller->axisButtons &= ~VR_TOUCH_AXIS_UP;
        }

        if (!(controller->axisButtons & VR_TOUCH_AXIS_DOWN) && joystickY < -pressedThreshold) {
            Com_QueueEvent( in_eventTime, SE_KEY, K_MWHEELUP, qtrue, 0, NULL );
            controller->axisButtons |= VR_TOUCH_AXIS_DOWN;
        } else if ((controller->axisButtons & VR_TOUCH_AXIS_DOWN) && joystickY > -releasedThreshold) {
            Com_QueueEvent( in_eventTime, SE_KEY, K_MWHEELUP, qfalse, 0, NULL );
            controller->axisButtons &= ~VR_TOUCH_AXIS_DOWN;
        }
    }
}

void VRMOD_IN_Grab(qboolean isRightController, float index)
{
#ifdef USE_WEAPON_WHEEL
	if ( isRightController )
		vr_info.weapon_select = (index > 0.3f);
	//else
		//weapon zoom when the two controllers's grab_buttons are pressed
#endif
}

void VRMOD_togglePlayerLaserBeam(qboolean pressed)
{
	if (pressed)
		return;
	laserBeamSwitch = !laserBeamSwitch;
}

//change name of VRAPI_buttonsChanged or VRMOD_IN_Button
void VRMOD_IN_Button(qboolean isRightController, xrButton button, qboolean isPressed )
{
	// if not connected to a server or pause then we are in Menu
	qboolean onPause = (cls.state != CA_ACTIVE || cl_paused->integer);
	vrController_t *ctrl = (isRightController) ? &vr_info.controllers[SideRIGHT] : &vr_info.controllers[SideLEFT];

	//----------------
	// Button [A]
	// in game -> "Jump"
	// in menu -> "esc"
	//----------------
	static qboolean xrButton_A_isPressed = qfalse;

	if (button == xrButton_A) {
		int actionButtonA = (onPause) ? K_ESCAPE : K_SPACE;

		// button is pressed
		if (isPressed) {
			Com_QueueEvent(in_eventTime, SE_KEY, actionButtonA, qtrue, 0, NULL);
			xrButton_A_isPressed = isPressed;
		}
		// button is released
		if (!isPressed && xrButton_A_isPressed) {
			Com_QueueEvent(in_eventTime, SE_KEY, actionButtonA, qfalse, 0, NULL);
			xrButton_A_isPressed = isPressed;
		}
	}
	//----------------
	// Button [B]
	// in game -> "Crouch"
	// in menu -> TODO
	//----------------
	static qboolean xrButton_B_isPressed = qfalse;
	//const qboolean playerIsCrouch = (cl.snap.ps.pm_flags & PMF_DUCKED);

	// Toggle Crouch
	static qboolean crouchAsked = qfalse;
	if (button == xrButton_B) {
		// button is pressed
		if (isPressed && !xrButton_B_isPressed ) {
			if (!crouchAsked)
				Com_QueueEvent(in_eventTime, SE_KEY, K_C, qtrue, 0, NULL);
			else
				Com_QueueEvent(in_eventTime, SE_KEY, K_C, qfalse, 0, NULL);

			crouchAsked = !crouchAsked;
			xrButton_B_isPressed = isPressed;
		}
		// button is released
		if (!isPressed && xrButton_B_isPressed ) {
			xrButton_B_isPressed = isPressed;
		}
	}

	//----------------
	// Button [X]
	// in game -> "use item"
	// in menu -> "show console"
	//----------------
	static qboolean xrButton_X_isPressed = qfalse;
	if (button == xrButton_X) {
		// button is pressed
		if (isPressed) {
			if ( onPause ) {
				// show console on button release only
			}
			else if (cl.snap.ps.pm_flags & PMF_FOLLOW)
			{
				// Switch follow mode
				vr_info.follow_mode = (vr_info.follow_mode + 1) % VRFM_NUM_FOLLOWMODES;
			}
			// In game: "use item"
			else {
				Com_QueueEvent(in_eventTime, SE_KEY, K_ENTER, qtrue, 0, NULL);
			}

			xrButton_X_isPressed = isPressed;
		}
		// button is released
		if (!isPressed && xrButton_X_isPressed) {
			// In menu: show console
			if (onPause) {
				Con_ToggleConsole_f();
			}
			// In game: "use item"
			else {
				Com_QueueEvent(in_eventTime, SE_KEY, K_ENTER, qfalse, 0, NULL);
			}

			xrButton_X_isPressed = isPressed;
		}
	}
	//----------------
	// Button [Y]
	// in game -> "laserbeam toggle"
	// in menu -> ?
	//----------------
	static qboolean xrButton_Y_isPressed = qfalse;
	if (button == xrButton_Y) {
		// button is pressed
		if (isPressed) {
            VRMOD_togglePlayerLaserBeam(qtrue);
			xrButton_Y_isPressed = isPressed;
		}
		// button is released
		if (!isPressed && xrButton_Y_isPressed) {
            VRMOD_togglePlayerLaserBeam(qfalse);
			xrButton_Y_isPressed = isPressed;
		}
	}

	//----------------
	// Button [menu] (left hand)
	// in game -> "menu"
	// in menu -> "esc"
	//----------------
	static qboolean xrButton_menu_isPressed = qfalse;
	if (button == xrButton_Enter) {
		// button is pressed
		if (isPressed && !xrButton_menu_isPressed) {
			Com_QueueEvent(in_eventTime, SE_KEY, K_ESCAPE, qtrue, 0, NULL);
			xrButton_menu_isPressed = isPressed;
		}
		// button is released
		if (!isPressed && xrButton_menu_isPressed ) {
			Com_QueueEvent(in_eventTime, SE_KEY, K_ESCAPE, qfalse, 0, NULL);
			xrButton_menu_isPressed = isPressed;
		}
	}


	//----------------
	// Button Back (OCULUS GO headset or Gear VR Controller)
	// in game -> "laserbeam toggle"
	// in menu -> ?
	//----------------
	static qboolean xrButton_Back_isPressed = qfalse;
	if (button == xrButton_Back) {
		// button is pressed
		if (isPressed) {
			Com_QueueEvent(in_eventTime, SE_KEY, K_ESCAPE, qtrue, 0, NULL);

			xrButton_Back_isPressed = isPressed;
		}
		// button is released
		if (!isPressed && xrButton_Back_isPressed) {
			Com_QueueEvent(in_eventTime, SE_KEY, K_ESCAPE, qfalse, 0, NULL);

			xrButton_Back_isPressed = isPressed;
		}
	}
}

void VRMOD_IN_Triggers(qboolean isRightController, float index )
{
    vrController_t* controller = isRightController == qtrue ? &vr_info.controllers[SideRIGHT] : &vr_info.controllers[SideLEFT];

    //Primary trigger (Right hand)
    if (isRightController == (vr_righthanded->integer != 0)) {
        if (!(controller->axisButtons & VR_TOUCH_AXIS_TRIGGER_INDEX) && index > pressedThreshold) {
            Com_QueueEvent(in_eventTime, SE_KEY, K_MOUSE1, qtrue, 0, NULL);
            controller->axisButtons |= VR_TOUCH_AXIS_TRIGGER_INDEX;
        }
        else if ((controller->axisButtons & VR_TOUCH_AXIS_TRIGGER_INDEX) && index < releasedThreshold) {
            Com_QueueEvent(in_eventTime, SE_KEY, K_MOUSE1, qfalse, 0, NULL);
            controller->axisButtons &= ~VR_TOUCH_AXIS_TRIGGER_INDEX;
        }
    }

    //off hand trigger
    if (isRightController != (vr_righthanded->integer != 0)) {
        if (!(controller->axisButtons & VR_TOUCH_AXIS_TRIGGER_INDEX) && index > pressedThreshold) {
            // JUMP on
            Com_QueueEvent(in_eventTime, SE_KEY, K_SPACE, qtrue, 0, NULL);
            controller->axisButtons |= VR_TOUCH_AXIS_TRIGGER_INDEX;
        }
        else if ((controller->axisButtons & VR_TOUCH_AXIS_TRIGGER_INDEX) && index < releasedThreshold) {
            // JUMP off
            Com_QueueEvent(in_eventTime, SE_KEY, K_SPACE, qfalse, 0, NULL);
            controller->axisButtons &= ~VR_TOUCH_AXIS_TRIGGER_INDEX;
        }
    }
}


//=============================================================
// OCULUS GO Trackpad
//=============================================================
#ifdef OCULUSGO
static void VRMOD_IN_Trackpad_Menu(qboolean padClic, qboolean padTouch, float touchX, float touchY )
{
    static float m_TouchX = MAXFLOAT;
    static float m_TouchY = MAXFLOAT;

    if (padClic || (padTouch && m_TouchX == MAXFLOAT))
    {
        m_TouchX = touchX;
        m_TouchY = touchY;
    }
    else if (!padTouch && m_TouchX != MAXFLOAT)
    {
        m_TouchX = MAXFLOAT;
        m_TouchY = MAXFLOAT;
    }
    else if (m_TouchX != MAXFLOAT)
    {
        if ((touchY - m_TouchY) < -0.4f)
        {
            m_TouchY = touchY;
            Com_QueueEvent(in_eventTime, SE_KEY, K_UPARROW, qtrue, 0, NULL);
            Com_QueueEvent(in_eventTime, SE_KEY, K_UPARROW, qfalse, 0, NULL);
        }
        else if ((touchY - m_TouchY) > 0.4f)
        {
            m_TouchY = touchY;
            Com_QueueEvent(in_eventTime, SE_KEY, K_DOWNARROW, qtrue, 0, NULL);
            Com_QueueEvent(in_eventTime, SE_KEY, K_DOWNARROW, qfalse, 0, NULL);
        }

        if ((touchX - m_TouchX) < -0.4f)
        {
            m_TouchX = touchX;
            Com_QueueEvent(in_eventTime, SE_KEY, K_LEFTARROW, qtrue, 0, NULL);
            Com_QueueEvent(in_eventTime, SE_KEY, K_LEFTARROW, qfalse, 0, NULL);
        }
        else if ((touchX - m_TouchX) > 0.4f)
        {
            m_TouchX = touchX;
            Com_QueueEvent(in_eventTime, SE_KEY, K_RIGHTARROW, qtrue, 0, NULL);
            Com_QueueEvent(in_eventTime, SE_KEY, K_RIGHTARROW, qfalse, 0, NULL);
        }
    }
}

void VRMOD_IN_Trackpad(qboolean padClic, qboolean padTouch, float touchX, float touchY) {
    // if not connected to a server or pause then we are in Menu
    if ( cls.state != CA_ACTIVE || cl_paused->integer ) {
        VRMOD_IN_Trackpad_Menu(padClic, padTouch, touchX, touchY);
    }
    else {
        static qboolean bIsTouchingState = qfalse;
        if (padTouch || bIsTouchingState)
        {
            VRMOD_IN_Joystick(qfalse, touchX, -touchY);
            bIsTouchingState = padTouch;
        }
    }
}
#endif

void VRMOD_Get_HMD_Info(void) {
#ifdef USE_VRAPI
	VRAPI_Get_HMD_Info();
#endif

#ifdef USE_OPENXR
	OpenXR_Get_HMD_Info();
#endif
}


/*
====================
Check VR HMD & VRcontrollers
====================
*/
// update cl.viewangles
void VRMOD_CL_Get_HMD_Angles(void )
{
#ifdef USE_VRAPI
    VRAPI_Get_HMD_Angles();
#endif

#ifdef USE_OPENXR
	// GUNNM as OpenXR is picky with API call order, use the same call in the same order with VRApi
	// this should allow to save CPU time and refactorize the code.

	//OpenXR_Get_HMD_Angles(); // this is not working at the moment, VRMOD_CL_Get_HMD_Angles() is called in CL_CreateCmd(), and should be called somewhere else.
#endif
}


// update cl.vrOrigin
void VRMOD_CL_Get_HMD_Position(void )
{
#if defined( USE_VRAPI ) && !defined(OCULUSGO)
     VRAPI_Get_HMD_Pos();
#endif

#ifdef USE_OPENXR
    OpenXR_Get_HMD_Pos();
#endif
}


#ifdef USE_VR
void VRMOD_CL_handle_controllers( void )
{
    if ( vr_controller_type ) {
#ifdef USE_VRAPI
        VRAPI_handle_controllers();
#endif

#ifdef USE_OPENXR
		OpenXR_handle_controllers();
#endif

		if( vr_info.recenterAsked )
            VRMOD_recenterView();

#ifdef USE_VR_QVM
        VectorSubtract(vr_info.rightAngles, vr_info.controllers[SideRIGHT].angles.delta, vr_info.rightAngles);
#endif

        VRMOD_checkGestureWeaponChange();

        // we need the VR controller to act like a mouse in menu
        qboolean onPause = (cls.state == CA_ACTIVE && ( cl_paused && cl_paused->integer ) );
        if ( cls.state != CA_ACTIVE || onPause ) {
            VRMOD_CL_MouseEvent(vr_info.mouse_x, vr_info.mouse_y, vr_info.mouse_z);
        }
    }
}
#endif // USE_VR
/*
====================
Communication with QVM
====================
*/
#ifdef USE_VR_QVM
void VRMOD_CL_KeepRightPos( int x, int y, int z )
{
	VectorSet(vr_info.ray_origin, x, y, z);
}

void VRMOD_CL_KeepRightAngles( vec3_t angles )
{
	VectorCopy(angles, vr_info.ray_orientation);
}

void VRMOD_CL_KeepLeftAngles( vec3_t angles )
{
	VectorCopy(angles, vr_info.left_angles);
}
#endif // USE_VR_QVM

void VRMOD_CL_MouseEvent( int dx, int dy, int dz ) {
    qboolean onPause = (cls.state == CA_ACTIVE && ( cl_paused && cl_paused->integer ) ); // game launched but pause

    if ( Key_GetCatcher( ) & KEYCATCH_UI ) {
        VM_Call( uivm, 3, UI_MOUSE_EVENT, dx, dy, dz );

        if ( onPause ) {
            VM_Call ( cgvm, 3, CG_MOUSE_EVENT, dx, dy, dz );
        }
    }
    else if ( Key_GetCatcher( ) & KEYCATCH_CGAME ) {
        VM_Call ( cgvm, 3, CG_MOUSE_EVENT, dx, dy, dz );
    }
    else {
#ifdef USE_LASER_SIGHT
        cl.mouseDx[cl.mouseIndex] += dx;
        cl.mouseDy[cl.mouseIndex] += dy;
        cl.mouseDz[cl.mouseIndex] += dz;
#endif
    }
}


static int get_vrFlags( void ) {
	int vrFlags = 0;
#ifdef USE_LASER_SIGHT
	if ( laserBeamSwitch )
		vrFlags |= EF_LASER_SIGHT;
#endif

	if ( vr_info.weapon_stabilised )
		vrFlags |= EF_WEAPON_STABILISED;

#ifdef USE_WEAPON_WHEEL
	if (vr_info.weapon_select)
		vrFlags |= EF_WEAPON_WHEEL;
#endif

#ifdef USE_DEATHCAM
	if (vr_info.follow_mode == VRFM_THIRDPERSON_1)
		vrFlags |= EF_FM_THIRDPERSON_1;
	else if (vr_info.follow_mode == VRFM_THIRDPERSON_2)
		vrFlags |= EF_FM_THIRDPERSON_2;
	else if (vr_info.follow_mode == VRFM_FIRSTPERSON)
		vrFlags |= EF_FM_FIRSTPERSON;
#endif
	return vrFlags;
}

//=============================================================
// send the VR command, even if not a vr user
//=============================================================
void VRMOD_CL_Finish_VR_Move( usercmd_t *cmd ) {
	int		i;
	qboolean isVisible = qfalse;
	int vrFlags;

	// copy the state that the cgame is currently sending
	cmd->weapon = cl.cgameUserCmdValue;

	// send the current server time so the amount of movement
	// can be determined without allowing cheating
	cmd->serverTime = cl.serverTime;

#ifdef USE_VRAPI
	isVisible = AppState.Focused;
#endif

#ifdef USE_OPENXR
	isVisible = xr.is_visible;
#endif

#if defined USE_LASER_SIGHT && defined USE_NATIVE_HACK
	vr_info.laserBeam = laserBeamSwitch;
#endif

	// construct flags from vr_clientinfo_t's booleans
	vrFlags = get_vrFlags();

	//=========================
	// Adjust view angles from HMD
	//=========================
	// GUNNM TODO fix CG_CalcViewValues() in VM to use the same code here
#ifdef USE_NATIVE_HACK
	cl.viewangles[YAW] -= vr_info.hmdinfo.angles.delta[YAW];

	//Make angles good
	while (cl.viewangles[YAW] > 180.0f)
		cl.viewangles[YAW] -= 360.0f;
	while (cl.viewangles[YAW] < -180.0f)
		cl.viewangles[YAW] += 360.0f;

	cl.viewangles[PITCH] = vr_info.hmdinfo.angles.actual[PITCH];
	cl.viewangles[ROLL] = vr_info.hmdinfo.angles.actual[ROLL];

	VectorCopy(cl.viewangles, vr_info.clientviewangles);
#endif
#ifdef USE_VR_QVM
	VectorSubtract(cl.viewangles, vr_info.hmdinfo.angles.delta, cl.viewangles);
#endif

#ifdef USE_VR_QVM
	//=========================
	//	   Interpreted QVM
	//=========================
	cmd->vrFlags = vrFlags;

	for (i = 0; i < 3; i++) {
		//------------------------------
		// VR player
		//------------------------------
		if ( vr_controller_type->integer && isVisible )
		{
			// Right VR controller orientation tracking
			cmd->right_hand_angles[i] = ANGLE2SHORT(vr_info.rightAngles[i]);

			// Right weapon muzzle position
			cmd->right_muzzle_pos[i] = vr_info.ray_origin[i];

			if ( vr_controller_type->integer >= 2 )
			{
				//=====================
				//		6 Dof
				//=====================
				// HMD position tracking
				VectorScale(vr_info.hmdinfo.position.actual, 1000, cmd->hmd_origin);

				// Right VR controller position tracking
				cmd->right_hand_pos[i] = (int)(vr_info.controllers[SideRIGHT].position.actual[i] * 1000.0f);

				// Left VR controller position tracking
				cmd->left_hand_pos[i] = (int)(vr_info.controllers[SideLEFT].position.actual[i] * 1000.0f);

				// Left VR controller orientation tracking
				cmd->left_hand_angles[i] = ANGLE2SHORT(vr_info.leftAngles[i]);
			}
		}
		//------------------------------
		// Non-VR player (or VR minimized)
		//------------------------------
		else {
			// same as viewangles (mouse input), it's a test, maybe we could just send zero
			cmd->right_hand_angles[i] = ANGLE2SHORT(cl.viewangles[i]);
			cmd->right_muzzle_pos[i] = 0;
			cmd->right_hand_pos[i] = 0;
			cmd->hmd_origin[i] = 0;
		}
	}
#endif // USE_VR_QVM

#ifdef USE_NATIVE_HACK
	vr_info.clientNum = cl.snap.ps.clientNum;
	vr_info.vrFlags = vrFlags;

	//=========================
	//		Native QVM
	//=========================

	//If we are running multiplayer, pass the angles from the weapon and adjust the move values accordingly,
	// to "fake" a 3DoF weapon but keeping the movement correct (necessary with a remote non-vr server)
	if ( vr_controller_type->integer >= 2 )// vr_info.use_fake_6dof )
	{
		vec3_t angles, out;

		//Realign in playspace
		if ( --vr_info.realign == 0 )
			VectorCopy(vr_info.hmdinfo.position.actual, vr_info.hmdorigin);

		VectorCopy(vr_info.calculated_weaponangles, angles);

		//Adjust for difference in server angles
		float deltaPitch = SHORT2ANGLE(cl.snap.ps.delta_angles[PITCH]);
		angles[PITCH] -= deltaPitch;
		angles[YAW] += (cl.viewangles[YAW] - vr_info.hmdinfo.angles.actual[YAW]);

		//suppress roll // GUNNM TODO check in ioq3quest what this var do
		if (!vr_sendRollToServer->integer)
			angles[ROLL] = 0;

		for (i = 0; i < 3; i++) {
			cmd->angles[i] = ANGLE2SHORT(angles[i]);
		}

		// TODO the same way for native and non-native VM (in non native cmd->rightmove & cmd->forwardmove are sent by CL_JoystickMove())
		rotateAboutOrigin(cmd->rightmove, cmd->forwardmove, -vr_info.calculated_weaponangles[YAW], out);
		cmd->rightmove = out[0];
		cmd->forwardmove = out[1];
	}
	else 
#endif // USE_NATIVE_HACK
	for (i = 0; i < 3; i++) {
		cmd->angles[i] = ANGLE2SHORT(cl.viewangles[i]);
	}
}

