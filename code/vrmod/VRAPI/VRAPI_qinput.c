#include "../../renderervk/vk.h"
#include <VrApi_Input.h>
#include "VRAPI_imp.h"
#include "../../client/client.h"
#include "../../android/android_local.h" // for androidApp_struct

#include "../../vrmod/quaternion.h"
#include "../../vrmod/VRMOD_common.h"
#include "../../vrmod/VRMOD_input.h"

static qboolean controllerInit = qfalse;

static void VRAPI_Get_Position_and_delta(position_t *position, const ovrVector3f positionHmd )
{
    VectorSet(position->actual, positionHmd.x, positionHmd.y , positionHmd.z);
    VectorSubtract(position->last, position->actual, position->delta);
    VectorCopy(position->actual, position->last);
}

static void VRAPI_get_Orientation_and_delta( angles_t *angles, ovrQuatf or )
{
    QuatToAngles((float *) &or, angles->actual );
    VectorSubtract(angles->last, angles->actual, angles->delta);
    VectorCopy(angles->actual, angles->last);
}

void VRAPI_Get_HMD_Angles( void )
{
    // Set event time for next frame to earliest possible time an event could happen
    in_eventTime = Sys_Milliseconds();

    // VRAPI quaternion to Euler orientation
    QuatToAngles( (float *) &predictedTracking.HeadPose.Pose.Orientation, vr_info.hmdinfo.angles.actual );

    VectorSubtract(vr_info.hmdinfo.angles.last, vr_info.hmdinfo.angles.actual, vr_info.hmdinfo.angles.delta);
    VectorCopy(vr_info.hmdinfo.angles.actual, vr_info.hmdinfo.angles.last);
}

#ifndef OCULUSGO
void VRAPI_Get_HMD_Pos( void )
{
    const ovrVector3f posHMD = predictedTracking.HeadPose.Pose.Position;
    VRAPI_Get_Position_and_delta( &vr_info.hmdinfo.position, posHMD);
}
#endif

static inline qboolean isPressed( vrController_t* controller, uint32_t buttons, ovrButton b ) {
   return (buttons & b) && !(controller->buttons & b);
}
static inline qboolean isReleased( vrController_t* controller, uint32_t buttons, ovrButton b ) {
    return (!(buttons & b) && (controller->buttons & b));
}

static void VRAPI_buttonsChanged(vrController_t* ctrl, uint32_t buttons )
{
    static qboolean consoleShow;

    // if not connected to a server or pause then we are in Menu
    qboolean onPause = (cls.state != CA_ACTIVE || cl_paused->integer);

#ifdef OCULUSGO
    //  touchpad click on the OCULUS Go Controller
    int actionButtonEnter = K_SPACE;
#else
    // Menu button on Left Quest Controller
    int actionButtonEnter = K_ESCAPE;
#endif

    if (isPressed(ctrl, buttons, ovrButton_Enter)) {
        Com_QueueEvent(in_eventTime, SE_KEY, actionButtonEnter, qtrue, 0, NULL);
    }
    else if (isReleased(ctrl, buttons, ovrButton_Enter)) {
        Com_QueueEvent(in_eventTime, SE_KEY, actionButtonEnter, qfalse, 0, NULL);
    }

    // Left Grip
    if (ctrl->isRight == !vr_righthanded->integer) {
        if (isPressed(ctrl, buttons, ovrButton_GripTrigger)) {

        }
        else if (isReleased(ctrl, buttons, ovrButton_GripTrigger)) {

        }
    }
    // Right Grip
    else {
        if (isPressed(ctrl, buttons, ovrButton_GripTrigger)) {
            //Cbuf_AddText("weapon_select");
            vr_info.weapon_select = qtrue;
        }
        else if (isReleased(ctrl, buttons, ovrButton_GripTrigger)) {
            vr_info.weapon_select = qfalse;
            // on release, select the focused weapon
            Cbuf_AddText("weapon_select");
        }
    }

    // Left stick button
    if (ctrl->isRight == !vr_righthanded->integer) {
        if (isPressed(ctrl, buttons, ovrButton_LThumb)) {

        }
        else if (isReleased(ctrl, buttons, ovrButton_LThumb)) {

        }
    }
    // Right stick button
    else {
        if (isPressed(ctrl, buttons, ovrButton_RThumb)) {

        }
        else if (isReleased(ctrl, buttons, ovrButton_RThumb)) {

        }
    }

#ifdef OCULUSGO
    int actionButtonA = K_MOUSE1;
#else
    int actionButtonA = (!onPause) ? K_SPACE : K_ESCAPE;
#endif
    //Button A -> Jump in game, esc in menu
    if (isPressed(ctrl, buttons, ovrButton_A)) {
        Com_QueueEvent(in_eventTime, SE_KEY, actionButtonA, qtrue, 0, NULL);
    }
    else if (isReleased(ctrl, buttons, ovrButton_A)) {
        Com_QueueEvent(in_eventTime, SE_KEY, actionButtonA, qfalse, 0, NULL);
    }

    //Button B -> Crouch
    if (isPressed(ctrl, buttons, ovrButton_B)) {
        Com_QueueEvent(in_eventTime, SE_KEY, K_C, qtrue, 0, NULL);
    }
    else if (isReleased(ctrl, buttons, ovrButton_B)) {
        Com_QueueEvent(in_eventTime, SE_KEY, K_C, qfalse, 0, NULL);
    }

    //Button X -> "use item"
    if (isPressed(ctrl, buttons, ovrButton_X)) {
        // In menu: show console
        if ( onPause ) {
            // show console on button release
        }
        // In game: "use item"
        else {
            Com_QueueEvent(in_eventTime, SE_KEY, K_ENTER, qtrue, 0, NULL);
        }
    }
    else if (isReleased(ctrl, buttons, ovrButton_X)) {
        // In menu: show console
        if ( onPause ) {
            Con_ToggleConsole_f();
        }
        // In game: "use item"
        else {
            Com_QueueEvent(in_eventTime, SE_KEY, K_ENTER, qfalse, 0, NULL);
        }
    }

    //Button Y -> laserbeam toggle
    if (isPressed(ctrl, buttons, ovrButton_Y)) {
        VRMOD_togglePlayerLaserBeam(qtrue);
    }
    else if (isReleased(ctrl, buttons, ovrButton_Y)) {
        VRMOD_togglePlayerLaserBeam(qfalse);
    }

    // Back button on the OCULUS GO headset or Gear VR Controller
    if (isPressed(ctrl, buttons, ovrButton_Back)) {
        Com_QueueEvent(in_eventTime, SE_KEY, K_ESCAPE, qtrue, 0, NULL);
    }
    else if (isReleased(ctrl, buttons, ovrButton_Back)) {
        Com_QueueEvent(in_eventTime, SE_KEY, K_ESCAPE, qfalse, 0, NULL);
    }

    ctrl->buttons = buttons;
}

#ifdef OCULUSGO
void VRAPI_IN_Gamepad(ovrInputCapabilityHeader capsHeader ) {
    static qboolean backButtonDownThisFrame = qfalse;

    ovrInputStateGamepad gamepadState;
    gamepadState.Header.ControllerType = ovrControllerType_Gamepad;
    ovrResult result = vrapi_GetCurrentInputState(AppState.Ovr, capsHeader.DeviceID, &gamepadState.Header );
    if ( result == ovrSuccess )
    {
        // Fire
        //if ((gamepadState.Buttons & ovrButton_A) != 0)
        //    fireEvent 	            = (gamepadState.Buttons & ovrButton_A) != 0;
        // B ?
        qboolean button_B_State 	= (gamepadState.Buttons & ovrButton_B) != 0;
        // Back button
        qboolean button_Back_State 	= (gamepadState.Buttons & ovrButton_Back) != 0;

        backButtonDownThisFrame |= ( button_Back_State ) || ( button_B_State );

        // Left Joystick
        VRMOD_IN_Joystick(qfalse, gamepadState.LeftJoystick.x, gamepadState.LeftJoystick.y);
        // Right Joystick
        VRMOD_IN_Joystick(qtrue, gamepadState.RightJoystick.x, gamepadState.RightJoystick.y);
    }
}
#endif

void VRAPI_handle_controllers( void )
{
    static int recenterCount = 0;

    if (controllerInit == qfalse) {
        memset(&vr_info.controllers[SideLEFT], 0, sizeof(vrController_t));
        memset(&vr_info.controllers[SideRIGHT], 0, sizeof(vrController_t));
        vr_info.controllers[SideRIGHT].isRight = qtrue;
        vr_info.controllers[SideLEFT].isRight = qfalse;
        controllerInit = qtrue;
    }

    ovrMobile* ovr = AppState.Ovr;
    if (!ovr) {
        return;
    }

    ovrInputCapabilityHeader capsHeader;

    for ( int32_t index = 0; ; index++ ) {
        ovrResult enumResult = vrapi_EnumerateInputDevices(ovr, index, &capsHeader);
        if (enumResult < 0) {
            break;
        }

#ifdef OCULUSGO
        if (capsHeader.Type == ovrControllerType_Gamepad )
        {
            VRAPI_IN_Gamepad(capsHeader);
            continue;
        }
#endif

        if (capsHeader.Type != ovrControllerType_TrackedRemote) {
            continue;
        }

        ovrInputTrackedRemoteCapabilities caps;
        caps.Header = capsHeader;
        ovrResult capsResult = vrapi_GetInputDeviceCapabilities(ovr, &caps.Header);
        if (capsResult < 0) {
            continue;
        }

        ovrInputStateTrackedRemote state;
        state.Header.ControllerType = ovrControllerType_TrackedRemote;
        ovrResult stateResult = vrapi_GetCurrentInputState(ovr, capsHeader.DeviceID, &state.Header);
        if (stateResult < 0) {
            continue;
        }

        ovrTracking remoteTracking;
        stateResult = vrapi_GetInputTrackingState(ovr, capsHeader.DeviceID, AppState.DisplayTime, &remoteTracking);
        if (stateResult < 0) {
            continue;
        }

        vrController_t* controller;
        if (caps.ControllerCapabilities & ovrControllerCaps_LeftHand) {
            controller = &vr_info.controllers[SideLEFT];
        } else if (caps.ControllerCapabilities & ovrControllerCaps_RightHand) {
            controller = &vr_info.controllers[SideRIGHT];
        }
        else {
            continue;
        }

        // This is done after VRAPI_handle_controllersa recenter
        if (controller->isRight && recenterCount != state.RecenterCount) {
            vr_info.recenterAsked = qtrue;
            recenterCount = state.RecenterCount;
        }

        if (controller->buttons ^ state.Buttons) {
            VRAPI_buttonsChanged(controller, state.Buttons);
        }

        // Get the controller orientation
        VRAPI_get_Orientation_and_delta(&controller->angles, remoteTracking.HeadPose.Pose.Orientation);
        // Get the controller position
        VRAPI_Get_Position_and_delta(&controller->position, remoteTracking.HeadPose.Pose.Position);

        // Get the controller joystick actions
        VRMOD_IN_Joystick(controller->isRight, state.Joystick.x, state.Joystick.y);
        // Get the controller triggers actions
        VRMOD_IN_Triggers(controller->isRight, state.IndexTrigger);

#ifdef OCULUSGO
        qboolean pad_clic  = (state.Buttons & ovrButton_Enter) != 0;
        qboolean pad_touch = (state.TrackpadStatus != 0);
        float touchX = pad_touch ? ((state.TrackpadPosition.x / caps.TrackpadMaxX) - 0.5f) * 2.0f : 0.0f;
        float touchY = pad_touch ? ((state.TrackpadPosition.y / caps.TrackpadMaxY) - 0.5f) * 2.0f : 0.0f;

        VRMOD_IN_Trackpad(pad_clic, pad_touch, touchX, touchY);
#endif
    }
}
