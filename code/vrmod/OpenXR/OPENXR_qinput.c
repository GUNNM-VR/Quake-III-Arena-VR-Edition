#include "../../client/client.h"
#include "../../vrmod/quaternion.h"
#include "../../vrmod/VRMOD_input.h"
#include "../OpenXR/OPENXR_imp.h"

extern cvar_t *vr_twoHandedWeapons;
extern cvar_t *vr_weaponPitch;

static float length(float x, float y)
{
	return sqrtf(powf(x, 2.0f) + powf(y, 2.0f));
}

// we need an identity pose for creating spaces without offsets
static XrPosef identity_pose = { 
	.orientation	= {.x = 0,.y = 0,.z = 0,.w = 1.0},
	.position		= {.x = 0,.y = 0,.z = 0}
};

static float OpenXR_Get_ipd(XrVector3f posA, XrVector3f posB) {
	// Compute the vector from the origin of the left eye to the right eye by
	// using the transform part of the view matrix
	float dx = posA.x - posB.x;
	float dy = posA.y - posB.y;
	float dz = posA.z - posB.z;
	// the IPD is then just the length of this vector
	float ipd = sqrtf(dx * dx + dy * dy + dz * dz);
	return ipd * 1000.0f;
}

void OpenXR_Get_HMD_Info( void )
{
	// vk.eye always 0 here
	vr_info.hmdinfo.fov_x = (xr.views[vk.eye].fov.angleRight - xr.views[vk.eye].fov.angleLeft);
	vr_info.hmdinfo.fov_y = (xr.views[vk.eye].fov.angleDown - xr.views[vk.eye].fov.angleUp);
	vr_info.hmdinfo.ipd = OpenXR_Get_ipd(xr.views[1].pose.position, xr.views[0].pose.position);
}

static void OpenXR_Get_Position_and_delta( position_t *position, const XrVector3f pos )
{
	VectorSet(position->actual, pos.x, pos.y, pos.z);
	VectorSubtract(position->last, position->actual, position->delta);
	VectorCopy(position->actual, position->last);
}

static void OpenXR_Get_Orientation_and_delta( XrQuaternionf orientation, angles_t *angles )
{
	QuatToAngles((float *)&orientation, angles->actual);
	VectorSubtract(angles->last, angles->actual, angles->delta);
	VectorCopy(angles->actual, angles->last);
}

void OpenXR_Get_HMD_Angles( void )
{
	OpenXR_Get_Orientation_and_delta( xr.views[vk.eye].pose.orientation, &vr_info.hmdinfo.angles );
}

void OpenXR_Get_HMD_Pos(void)
{
	OpenXR_Get_Position_and_delta(&vr_info.hmdinfo.position, xr.views[0].pose.position);
}

void ctrl_to_HMD_Offset(vrController_t *ctrl) {
	VectorCopy(ctrl->offset_last[1], ctrl->offset_last[0]);
	VectorCopy(ctrl->hmd_ctrl_offset, ctrl->offset_last[1]);
	VectorSubtract(ctrl->position.actual, vr_info.hmdinfo.position.actual, ctrl->hmd_ctrl_offset);
}

void twoHandedWeapons( void ) {
	vec3_t smooth_weaponoffset, vec;
	vrController_t weaponHand = vr_info.controllers[SideRIGHT];
	vrController_t offHand = vr_info.controllers[SideLEFT];

	//Apply smoothing to the weapon hand
	VectorAdd(weaponHand.hmd_ctrl_offset, weaponHand.offset_last[0], smooth_weaponoffset);
	VectorAdd(smooth_weaponoffset, weaponHand.offset_last[1], smooth_weaponoffset);
	VectorScale(smooth_weaponoffset, 1.0f / 3.0f, smooth_weaponoffset);
	VectorSubtract(offHand.hmd_ctrl_offset, smooth_weaponoffset, vec);

	//Dampen roll on stabilised weapon
	float zxDist = length(vec[0], vec[2]);
	if (zxDist != 0.0f && vec[2] != 0.0f) {
		VectorSet(weaponHand.angles.actual, -RAD2DEG(atanf(vec[1] / zxDist)),
			-RAD2DEG(atan2f(vec[0], -vec[2])), weaponHand.angles.actual[ROLL] / 2.0f);
		
	}
}

void OpenXR_handle_controllers( void ) {
	XrResult result;
	const char* handName[] = { "left", "right" };

	if (!xr.is_visible) return;

	for (uint16_t hand = 0; hand < SideCOUNT; hand++)
	{
		XrSpaceLocation spaceLocation;
		spaceLocation.type = XR_TYPE_SPACE_LOCATION;
		spaceLocation.next = NULL;
		spaceLocation.pose = identity_pose;

		result = xrLocateSpace(xr.input.handSpace[hand], xr.LocalSpace, xr.frameState.predictedDisplayTime, &spaceLocation);
		if ( xr_check(result, "xrLocateSpace failed for %s hand", handName[hand]))
		{
			if ((spaceLocation.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT) != 0 &&
				(spaceLocation.locationFlags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT) != 0) {

				vrController_t *ctrl = &vr_info.controllers[hand];


				OpenXR_Get_Orientation_and_delta(spaceLocation.pose.orientation, &ctrl->angles);

				if ( vr_controller_type->integer >= 2 ) {
					OpenXR_Get_Position_and_delta(&ctrl->position, spaceLocation.pose.position);

					// get controller location relative to view
					ctrl_to_HMD_Offset(ctrl);
				}

#ifdef USE_NATIVE_HACK
				if ( vr_controller_type->integer == 1 )
					ctrl->angles.actual[1] = AngleMod(360 + vr_info.spawnAngles[YAW] + ctrl->angles.actual[YAW]);
#endif
			}
		}
		else
		{
			// Tracking loss is expected when the hand is not active
			// so only log a message if the hand is active.
			if (xr.input.handActive[hand] == XR_TRUE)
			{			
				ri.Printf(PRINT_ALL, "Unable to locate %s hand action space in app space: %d", handName[hand], result);
			}
		}
	}

	if (vr_twoHandedWeapons->integer && vr_info.weapon_stabilised)
	{
		twoHandedWeapons();
	}

#ifdef USE_VR_ZOOM
	cvar_t *vr_weaponScope = NULL;
	vr_weaponScope = Cvar_Get("vr_weaponScope", "1", CVAR_ARCHIVE);

	vr_info.weapon_zoomed = vr_weaponScope->integer &&
		vr_info.weapon_stabilised &&
		(cl.snap.ps.weapon == WP_RAILGUN) &&
		(VectorLength(vr_info.weaponoffset) < 0.24f) &&
		cl.snap.ps.stats[STAT_HEALTH] > 0;

#endif
}

qboolean OpenXR_InitializeActions( void )
{
	XrResult result = XR_SUCCESS;
	// Create an action set.
	{
		XrActionSetCreateInfo actionSetInfo;
		actionSetInfo .type = XR_TYPE_ACTION_SET_CREATE_INFO;
		strcpy(actionSetInfo.actionSetName, "gameplay");
		strcpy(actionSetInfo.localizedActionSetName, "Gameplay");
		actionSetInfo.priority = 0;
		result = xrCreateActionSet(xr.instance, &actionSetInfo, &xr.input.actionSet);
		if (!xr_check(result, "xrCreateActionSet Failed"))
			return qfalse;
	}

	// Get the XrPath for the left and right hands - we will use them as subaction paths.
	result = xrStringToPath(xr.instance, "/user/hand/left", &xr.input.handSubactionPath[SideLEFT]);
	if (!xr_check(result, "xrStringToPath left Failed"))
		return qfalse;

	result = xrStringToPath(xr.instance, "/user/hand/right", &xr.input.handSubactionPath[SideRIGHT]);
	if (!xr_check(result, "xrStringToPath right Failed"))
		return qfalse;

	// Create actions.
	{
		XrActionCreateInfo actionInfo;
		actionInfo.type = XR_TYPE_ACTION_CREATE_INFO;

		//-------------------
		// thumbstick x & y
		//-------------------
		strcpy(actionInfo.actionName, "thumbstick");
		strcpy(actionInfo.localizedActionName, "thumbstick_");
		actionInfo.actionType = XR_ACTION_TYPE_VECTOR2F_INPUT;
		actionInfo.countSubactionPaths = SideCOUNT;
		actionInfo.subactionPaths = xr.input.handSubactionPath;
		result = xrCreateAction(xr.input.actionSet, &actionInfo, &xr.input.thumbstickAction);
		if (!xr_check(result, "xrCreateAction \"thumbstick\" Failed"))
			return qfalse;
	
		//-------------------
		// thumbstick click
		//-------------------
		strcpy(actionInfo.actionName, "thumbstickclick");
		strcpy(actionInfo.localizedActionName, "thumbstick_click");
		actionInfo.actionType = XR_ACTION_TYPE_BOOLEAN_INPUT;
		actionInfo.countSubactionPaths = SideCOUNT;
		actionInfo.subactionPaths = xr.input.handSubactionPath;
		result = xrCreateAction(xr.input.actionSet, &actionInfo, &xr.input.thumbstickClickAction);
		if (!xr_check(result, "xrCreateAction \"thumbstick click\" Failed"))
			return qfalse;

		//-------------------
		// trigger
		//-------------------
		strcpy(actionInfo.actionName, "trigger");
		strcpy(actionInfo.localizedActionName, "Trigger_");
		actionInfo.actionType = XR_ACTION_TYPE_FLOAT_INPUT;
		actionInfo.countSubactionPaths = SideCOUNT;
		actionInfo.subactionPaths = xr.input.handSubactionPath;
		result = xrCreateAction(xr.input.actionSet, &actionInfo, &xr.input.triggerAction);
		if (!xr_check(result, "xrCreateAction \"trigger\" Failed"))
			return qfalse;

		//-------------------
		// [Grab]
		//-------------------
		strcpy(actionInfo.actionName, "grab_object");
		strcpy(actionInfo.localizedActionName, "Grab Object");
		actionInfo.actionType = XR_ACTION_TYPE_FLOAT_INPUT;
		actionInfo.countSubactionPaths = SideCOUNT;
		actionInfo.subactionPaths = xr.input.handSubactionPath;
		result = xrCreateAction(xr.input.actionSet, &actionInfo, &xr.input.grabAction);
		if (!xr_check(result, "xrCreateAction \"Grab Object\" Failed"))
			return qfalse;

		//-------------------
		// button [A] click
		//-------------------
		strcpy(actionInfo.actionName, "button_a");
		strcpy(actionInfo.localizedActionName, "button_a_");
		actionInfo.actionType = XR_ACTION_TYPE_BOOLEAN_INPUT;
		actionInfo.countSubactionPaths = 0;
		result = xrCreateAction(xr.input.actionSet, &actionInfo, &xr.input.button_a_Action);
		if (!xr_check(result, "xrCreateAction \"button [A] click\" Failed"))
			return qfalse;
		//-------------------
		// button [B] click
		//-------------------
		strcpy(actionInfo.actionName, "button_b");
		strcpy(actionInfo.localizedActionName, "button_b_");
		actionInfo.actionType = XR_ACTION_TYPE_BOOLEAN_INPUT;
		actionInfo.countSubactionPaths = 0;
		actionInfo.subactionPaths = XR_NULL_PATH;
		result = xrCreateAction(xr.input.actionSet, &actionInfo, &xr.input.button_b_Action);
		if (!xr_check(result, "xrCreateAction \"button [B] click\" Failed"))
			return qfalse;
		//-------------------
		// button [X] click
		//-------------------
		strcpy(actionInfo.actionName, "button_x");
		strcpy(actionInfo.localizedActionName, "button_x_");
		actionInfo.actionType = XR_ACTION_TYPE_BOOLEAN_INPUT;
		actionInfo.countSubactionPaths = 0;
		actionInfo.subactionPaths = XR_NULL_PATH;
		result = xrCreateAction(xr.input.actionSet, &actionInfo, &xr.input.button_x_Action);
		if (!xr_check(result, "xrCreateAction \"button [X] click\" Failed"))
			return qfalse;
		//-------------------
		// button [Y] click
		//-------------------
		strcpy(actionInfo.actionName, "button_y");
		strcpy(actionInfo.localizedActionName, "button_y_");
		actionInfo.actionType = XR_ACTION_TYPE_BOOLEAN_INPUT;
		actionInfo.countSubactionPaths = 0;
		actionInfo.subactionPaths = XR_NULL_PATH;
		result = xrCreateAction(xr.input.actionSet, &actionInfo, &xr.input.button_y_Action);
		if (!xr_check(result, "xrCreateAction \"button [Y] click\" Failed"))
			return qfalse;
		//-------------------
		// button [menu] click
		//-------------------
		strcpy(actionInfo.actionName, "button_menu");
		strcpy(actionInfo.localizedActionName, "button_menu_");
		actionInfo.actionType = XR_ACTION_TYPE_BOOLEAN_INPUT;
		actionInfo.countSubactionPaths = SideCOUNT;
		actionInfo.subactionPaths = xr.input.handSubactionPath;
		result = xrCreateAction(xr.input.actionSet, &actionInfo, &xr.input.menuAction);
		if (!xr_check(result, "xrCreateAction \"button [menu] click\" Failed"))
			return qfalse;


		//-------------------
		// Hand Pose
		//-------------------
		// Create an input action getting the left and right hand poses.
		actionInfo.actionType = XR_ACTION_TYPE_POSE_INPUT;
		strcpy(actionInfo.actionName, "hand_pose");
		strcpy(actionInfo.localizedActionName, "Hand Pose");
		actionInfo.countSubactionPaths = SideCOUNT;
		actionInfo.subactionPaths = xr.input.handSubactionPath;
		result = xrCreateAction(xr.input.actionSet, &actionInfo, &xr.input.pointerAction);
		if(!xr_check(result, "xrCreateAction \"Hand Pose\" Failed"))
			return qfalse;

		//-------------------
		// vibrating controller
		//-------------------
		// Create output actions for vibrating the left and right controller.
		actionInfo.actionType = XR_ACTION_TYPE_VIBRATION_OUTPUT;
		strcpy(actionInfo.actionName, "vibrate_hand");
		strcpy(actionInfo.localizedActionName, "Vibrate Hand");
		actionInfo.countSubactionPaths = SideCOUNT;
		actionInfo.subactionPaths = xr.input.handSubactionPath;
		result = xrCreateAction(xr.input.actionSet, &actionInfo, &xr.input.vibrateAction);
		if(!xr_check(result, "xrCreateAction \"Vibrate Hand\" Failed"))
			return qfalse;

		/*
		//-------------------
		// quit
		//-------------------
		// Create input actions for quitting the session using the left and right controller.
		// Since it doesn't matter which hand did this, we do not specify subaction paths for it.
		// We will just suggest bindings for both hands, where possible.
		actionInfo.actionType = XR_ACTION_TYPE_BOOLEAN_INPUT;
		strcpy(actionInfo.actionName, "quit_session");
		strcpy(actionInfo.localizedActionName, "Quit Session");
		actionInfo.countSubactionPaths = 0;
		actionInfo.subactionPaths = NULL;
		result = xrCreateAction(xr.input.actionSet, &actionInfo, &xr.input.quitAction);
		if(!xr_check(result, "xrCreateAction \"Quit Session\" Failed"))
			return qfalse;*/
	}

	XrPath thumbstickPath[SideCOUNT];
	XrPath thumbstickClickPath[SideCOUNT];
	XrPath triggerValuePath[SideCOUNT];
	XrPath grabValuePath[SideCOUNT];
	XrPath aClickPath;
	XrPath bClickPath;
	XrPath xClickPath;
	XrPath yClickPath;

	XrPath selectPath[SideCOUNT];
	XrPath squeezeForcePath[SideCOUNT];
	XrPath squeezeClickPath[SideCOUNT];
	XrPath pointerPath[SideCOUNT];
	XrPath hapticPath[SideCOUNT];
	XrPath menuClickPath[SideCOUNT];

	// Thumbstick
	// if error with CV1 or others HMD try path "/user/hand/left/input/thumbstick/x" and "/user/hand/left/input/thumbstick/y"
	if ((xrStringToPath(xr.instance, "/user/hand/left/input/thumbstick", &thumbstickPath[SideLEFT]) < XR_SUCCESS))
		return qfalse;
	if ((xrStringToPath(xr.instance, "/user/hand/right/input/thumbstick", &thumbstickPath[SideRIGHT]) < XR_SUCCESS))
		return qfalse;


	// Thumbstick click
	if ((xrStringToPath(xr.instance, "/user/hand/left/input/thumbstick/click", &thumbstickClickPath[SideLEFT]) < XR_SUCCESS))
		return qfalse;
	if ((xrStringToPath(xr.instance, "/user/hand/right/input/thumbstick/click", &thumbstickClickPath[SideRIGHT]) < XR_SUCCESS))
		return qfalse;


	// Trigger
	if ((xrStringToPath(xr.instance, "/user/hand/left/input/trigger/value", &triggerValuePath[SideLEFT]) < XR_SUCCESS))
		return qfalse;
	if ((xrStringToPath(xr.instance, "/user/hand/right/input/trigger/value", &triggerValuePath[SideRIGHT]) < XR_SUCCESS))
		return qfalse;


	// Grab
	if ((xrStringToPath(xr.instance, "/user/hand/left/input/squeeze/value", &grabValuePath[SideLEFT]) < XR_SUCCESS))
		return qfalse;
	if ((xrStringToPath(xr.instance, "/user/hand/right/input/squeeze/value", &grabValuePath[SideRIGHT]) < XR_SUCCESS))
		return qfalse;


	// button A click
	if ((xrStringToPath(xr.instance, "/user/hand/right/input/a/click", &aClickPath/*[SideRIGHT]*/) < XR_SUCCESS))
		return qfalse;


	// button B click
	if ((xrStringToPath(xr.instance, "/user/hand/right/input/b/click", &bClickPath) < XR_SUCCESS))
		return qfalse;


	// button X click
	if ((xrStringToPath(xr.instance, "/user/hand/left/input/x/click", &xClickPath) < XR_SUCCESS))
		return qfalse;


	// button Y click
	if ((xrStringToPath(xr.instance, "/user/hand/left/input/y/click", &yClickPath) < XR_SUCCESS))
		return qfalse;


	// Menu / reposition
	if ((xrStringToPath(xr.instance, "/user/hand/left/input/menu/click", &menuClickPath[SideLEFT]) < XR_SUCCESS))
		return qfalse;
	if ((xrStringToPath(xr.instance, "/user/hand/right/input/menu/click", &menuClickPath[SideRIGHT]) < XR_SUCCESS))
		return qfalse;


	// Grab force (?)
	if ((xrStringToPath(xr.instance, "/user/hand/left/input/squeeze/force", &squeezeForcePath[SideLEFT]) < XR_SUCCESS))
		return qfalse;
	if ((xrStringToPath(xr.instance, "/user/hand/right/input/squeeze/force", &squeezeForcePath[SideRIGHT]) < XR_SUCCESS))
		return qfalse;


	// Grab click (?)
	if ((xrStringToPath(xr.instance, "/user/hand/left/input/squeeze/click", &squeezeClickPath[SideLEFT]) < XR_SUCCESS))
		return qfalse;
	if ((xrStringToPath(xr.instance, "/user/hand/right/input/squeeze/click", &squeezeClickPath[SideRIGHT]) < XR_SUCCESS))
		return qfalse;


	// select click (?)
	if ((xrStringToPath(xr.instance, "/user/hand/left/input/select/click", &selectPath[SideLEFT]) < XR_SUCCESS))
		return qfalse;
	if ((xrStringToPath(xr.instance, "/user/hand/right/input/select/click", &selectPath[SideRIGHT]) < XR_SUCCESS))
		return qfalse;


	// Position
	// cf. https://docs.unity3d.com/Packages/com.unity.xr.openxr@1.2/manual/features/oculustouchcontrollerprofile.html
	if ((xrStringToPath(xr.instance, "/user/hand/left/input/aim/pose", &pointerPath[SideLEFT]) < XR_SUCCESS))
		return qfalse;
	if ((xrStringToPath(xr.instance, "/user/hand/right/input/aim/pose", &pointerPath[SideRIGHT]) < XR_SUCCESS))
		return qfalse;


	// Haptic
	if ((xrStringToPath(xr.instance, "/user/hand/left/output/haptic", &hapticPath[SideLEFT]) < XR_SUCCESS))
		return qfalse;
	if ((xrStringToPath(xr.instance, "/user/hand/right/output/haptic", &hapticPath[SideRIGHT]) < XR_SUCCESS))
		return qfalse;

	// Suggest bindings for KHR Simple.
	{
		XrPath khrSimpleInteractionProfilePath;
		result = xrStringToPath(xr.instance, "/interaction_profiles/khr/simple_controller", &khrSimpleInteractionProfilePath);
		if(!xr_check(result, "xrStringToPath khr Failed"))
			return qfalse;

		XrActionSuggestedBinding bindings[2][9] = { {// Fall back to a click input for the grab action.
												{xr.input.grabAction, selectPath[SideLEFT]},
												{xr.input.grabAction, selectPath[SideRIGHT]},
												{xr.input.pointerAction, pointerPath[SideLEFT]},
												{xr.input.pointerAction, pointerPath[SideRIGHT]},
												{xr.input.menuAction, menuClickPath[SideLEFT]},
												{xr.input.menuAction, menuClickPath[SideRIGHT]},
												//{xr.input.quitAction, menuClickPath[SideRIGHT]},
												{xr.input.vibrateAction, hapticPath[SideLEFT]},
												{xr.input.vibrateAction, hapticPath[SideRIGHT]}} };

		XrInteractionProfileSuggestedBinding suggestedBindings;
		suggestedBindings.type = XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING;
		suggestedBindings.interactionProfile = khrSimpleInteractionProfilePath;
		suggestedBindings.suggestedBindings = (XrActionSuggestedBinding*)bindings;
		suggestedBindings.countSuggestedBindings = 8;// (uint32_t)bindings.size();
		result = xrSuggestInteractionProfileBindings(xr.instance, &suggestedBindings);
		if(!xr_check(result, "xrSuggestInteractionProfileBindings Failed"))
			return qfalse;
	}
	// Suggest bindings for the Oculus Touch.
	{
		XrPath oculusTouchInteractionProfilePath;
		result = xrStringToPath(xr.instance, "/interaction_profiles/oculus/touch_controller", &oculusTouchInteractionProfilePath);
		if (!xr_check(result, "xrStringToPath oculus Failed"))
			return qfalse;
		XrActionSuggestedBinding bindings[2][17] = { {
												// thumbstick x & y
												{xr.input.thumbstickAction, thumbstickPath[SideLEFT]},
												{xr.input.thumbstickAction, thumbstickPath[SideRIGHT]},
												// thumbstick clic
												{xr.input.thumbstickClickAction, thumbstickClickPath[SideLEFT]},
												{xr.input.thumbstickClickAction, thumbstickClickPath[SideRIGHT]},
												// Trigger
												{xr.input.triggerAction, triggerValuePath[SideLEFT]},
												{xr.input.triggerAction, triggerValuePath[SideRIGHT]},
												// Grab
												{xr.input.grabAction, grabValuePath[SideLEFT]},
												{xr.input.grabAction, grabValuePath[SideRIGHT]},
												// button a
												{xr.input.button_a_Action, aClickPath},
												// button b
												{xr.input.button_b_Action, bClickPath},
												// button x
												{xr.input.button_x_Action, xClickPath},
												// button y
												{xr.input.button_y_Action, yClickPath},

												// ?
												{xr.input.pointerAction, pointerPath[SideLEFT]},
												{xr.input.pointerAction, pointerPath[SideRIGHT]},

												// quit bouton ?
												{xr.input.menuAction, menuClickPath[SideLEFT]},

												// vibrate
												{xr.input.vibrateAction, hapticPath[SideLEFT]},
												{xr.input.vibrateAction, hapticPath[SideRIGHT]}} };

		XrInteractionProfileSuggestedBinding suggestedBindings;
		suggestedBindings.type = XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING;
		suggestedBindings.interactionProfile = oculusTouchInteractionProfilePath;
		suggestedBindings.suggestedBindings = (XrActionSuggestedBinding*)bindings;
		suggestedBindings.countSuggestedBindings = 16;
		result = xrSuggestInteractionProfileBindings(xr.instance, &suggestedBindings);
		if (!xr_check(result, "xrSuggestInteractionProfileBindings oculus Failed"))
			return qfalse;
	}
	// Suggest bindings for the Vive Controller.
	{
		XrPath viveControllerInteractionProfilePath;
		result = xrStringToPath(xr.instance, "/interaction_profiles/htc/vive_controller", &viveControllerInteractionProfilePath);
		if (!xr_check(result, "xrStringToPath htc Failed"))
			return qfalse;
		XrActionSuggestedBinding bindings[2][9] = { {
												{xr.input.grabAction, triggerValuePath[SideLEFT]},
												{xr.input.grabAction, triggerValuePath[SideRIGHT]},
												{xr.input.pointerAction, pointerPath[SideLEFT]},
												{xr.input.pointerAction, pointerPath[SideRIGHT]},
												{xr.input.menuAction, menuClickPath[SideLEFT]},
												{xr.input.menuAction, menuClickPath[SideRIGHT]},
												{xr.input.vibrateAction, hapticPath[SideLEFT]},
												{xr.input.vibrateAction, hapticPath[SideRIGHT]}} };
		XrInteractionProfileSuggestedBinding suggestedBindings;
		suggestedBindings.type = XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING;
		suggestedBindings.interactionProfile = viveControllerInteractionProfilePath;
		suggestedBindings.suggestedBindings = (XrActionSuggestedBinding*)bindings;
		suggestedBindings.countSuggestedBindings = 8;
		result = xrSuggestInteractionProfileBindings(xr.instance, &suggestedBindings);
		if (!xr_check(result, "xrSuggestInteractionProfileBindings htc Failed"))
			return qfalse;
	}

	// Suggest bindings for the Valve Index Controller.
	{
		XrPath indexControllerInteractionProfilePath;
		result = xrStringToPath(xr.instance, "/interaction_profiles/valve/index_controller", &indexControllerInteractionProfilePath);
		if (!xr_check(result, "xrStringToPath valve Failed"))
			return qfalse;
		XrActionSuggestedBinding bindings[2][9] = {{
												{xr.input.grabAction, squeezeForcePath[SideLEFT]},
												{xr.input.grabAction, squeezeForcePath[SideRIGHT]},
												{xr.input.pointerAction, pointerPath[SideLEFT]},
												{xr.input.pointerAction, pointerPath[SideRIGHT]},
												{xr.input.menuAction, bClickPath},
												{xr.input.menuAction, bClickPath},
												{xr.input.vibrateAction, hapticPath[SideLEFT]},
												{xr.input.vibrateAction, hapticPath[SideRIGHT]}} };
		XrInteractionProfileSuggestedBinding suggestedBindings;
		suggestedBindings.type = XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING;
		suggestedBindings.interactionProfile = indexControllerInteractionProfilePath;
		suggestedBindings.suggestedBindings = (XrActionSuggestedBinding*)bindings;
		suggestedBindings.countSuggestedBindings = 8;
		result = xrSuggestInteractionProfileBindings(xr.instance, &suggestedBindings);
		if (!xr_check(result, "xrSuggestInteractionProfileBindings valve Failed"))
			return qfalse;
	}

	// Suggest bindings for the Microsoft Mixed Reality Motion Controller.
	{
		XrPath microsoftMixedRealityInteractionProfilePath;
		result = xrStringToPath(xr.instance, "/interaction_profiles/microsoft/motion_controller", &microsoftMixedRealityInteractionProfilePath);
		if (!xr_check(result, "xrStringToPath microsoft Failed"))
			return qfalse;
		XrActionSuggestedBinding bindings[2][8] = {{
												{xr.input.grabAction, squeezeClickPath[SideLEFT]},
												{xr.input.grabAction, squeezeClickPath[SideRIGHT]},
												{xr.input.pointerAction, pointerPath[SideLEFT]},
												{xr.input.pointerAction, pointerPath[SideRIGHT]},
												{xr.input.menuAction, menuClickPath[SideLEFT]},
												{xr.input.menuAction, menuClickPath[SideRIGHT]},
												{xr.input.vibrateAction, hapticPath[SideLEFT]},
												{xr.input.vibrateAction, hapticPath[SideRIGHT]}} };
		XrInteractionProfileSuggestedBinding suggestedBindings;
		suggestedBindings.type = XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING;
		suggestedBindings.interactionProfile = microsoftMixedRealityInteractionProfilePath;
		suggestedBindings.suggestedBindings = (XrActionSuggestedBinding*)bindings;
		suggestedBindings.countSuggestedBindings = 8;
		result = xrSuggestInteractionProfileBindings(xr.instance, &suggestedBindings);
		if (!xr_check(result, "xrSuggestInteractionProfileBindings microsoft Failed"))
			return qfalse;
	}

	XrActionSpaceCreateInfo actionSpaceInfo;
	actionSpaceInfo.type = XR_TYPE_ACTION_SPACE_CREATE_INFO;
	actionSpaceInfo.action = xr.input.pointerAction;
	actionSpaceInfo.poseInActionSpace = identity_pose;

	actionSpaceInfo.subactionPath = xr.input.handSubactionPath[SideLEFT];
	result = xrCreateActionSpace(xr.session, &actionSpaceInfo, &xr.input.handSpace[SideLEFT]);
	if (!xr_check(result, "xrCreateActionSpace left side Failed"))
		return qfalse;

	actionSpaceInfo.subactionPath = xr.input.handSubactionPath[SideRIGHT];
	result = xrCreateActionSpace(xr.session, &actionSpaceInfo, &xr.input.handSpace[SideRIGHT]);
	if (!xr_check(result, "xrCreateActionSpace right side Failed"))
		return qfalse;

	XrSessionActionSetsAttachInfo attachInfo;
	attachInfo.type = XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO;
	attachInfo.countActionSets = 1;
	attachInfo.actionSets = &xr.input.actionSet;
	result = xrAttachSessionActionSets(xr.session, &attachInfo);
	if (!xr_check(result, "xrAttachSessionActionSets Failed"))
		return qfalse;
	
	return qtrue;
}
