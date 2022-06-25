#ifndef __OPENXR_IMP__
#define __OPENXR_IMP__

#include "../../qcommon/q_shared.h" // for qboolean

#define XR_USE_PLATFORM_WIN32
#define XR_USE_GRAPHICS_API_VULKAN

#ifdef XR_USE_PLATFORM_WIN32
#include <Windows.h>
/*#ifdef WIN32
#include  "../../../../openxr/win32/openxr.h"
#include "../../../../openxr/win32/openxr_platform.h"
#else
#include  "../../../../openxr/win64/openxr.h"
#include "../../../../openxr/win64/openxr_platform.h"
#endif*/
#endif // XR_USE_PLATFORM_WIN32

#include "../../vrmod/OpenXR/openxr/win32/openxr.h"
#include "../../vrmod/OpenXR/openxr/win32/openxr_platform.h"

#include "../../vrmod/VRMOD_common.h"
#include "../../vrmod/VRMOD_input.h"

#define XR_KHR_VULKAN_ENABLE_EXTENSION_NAME "XR_KHR_vulkan_enable"

#define MAX_NUM_EYES 2

#include "../OpenXR/xr_linear.h"

struct InputState_t {
	XrActionSet actionSet;

	XrAction thumbstickAction;
	XrAction thumbstickClickAction;
	XrAction triggerAction;
	XrAction grabAction;
	XrAction button_a_Action;
	XrAction button_b_Action;
	XrAction button_x_Action;
	XrAction button_y_Action;

	XrAction menuAction;
	//XrAction quitAction;

	XrAction pointerAction;
	XrAction vibrateAction;

	XrPath handSubactionPath[SideCOUNT];
	XrPath handSpace[SideCOUNT];
	XrBool32 handActive[SideCOUNT];
};

typedef struct InputState_t InputState;

typedef struct {
	XrSystemId	systemId;
	XrInstance	instance;
	XrSession	session;

	qboolean is_visible;
	qboolean is_running;

	PFN_vkGetInstanceProcAddr pfnGetInstanceProcAddr; // Vulkan GetInstanceProcAddr
	//XrViewConfigurationView *configViews;
	XrViewConfigurationView configViews[MAX_NUM_EYES];

	XrSpace visualizedSpace;
	XrSpace HeadSpace;
	XrSpace LocalSpace;
	XrSpace StageSpace;

	XrBool32 main_session_visible;
	XrFrameState frameState;
	//XrView* views;// array of view_count views, filled by the runtime with current HMD display pose
	XrView views[MAX_NUM_EYES];

	uint32_t view_count;
	XrViewConfigurationType view_config_type; // contain XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO
	const XrCompositionLayerBaseHeader** layers;
	uint32_t num_layers;

	XrSwapchain* swapchains;
	int64_t swapchain_format;
	uint32_t* swapchain_length; // One length per view
	uint32_t* last_acquired;
	XrSwapchainImageVulkanKHR** images;

	XrMatrix4x4f viewMatrix;

	InputState input;
} Xr_Instance;

extern Xr_Instance xr;

qboolean xr_check(XrResult result, const char* format, ...);

qboolean OpenXR_init_pre_vk( void );
qboolean OpenXR_init_post_vk(VkPhysicalDevice* vulkanPhysicalDevice, VkDeviceCreateInfo device_desc);
qboolean OpenXR_getExtensions(const char **extension_names, uint32_t *extension_count);
VkResult OpenXR_create_VK_instance(VkInstanceCreateInfo desc);

qboolean OpenXR_get_swapchain_format(VkSurfaceFormatKHR surface_format);
qboolean OpenXR_SwapChainInit();

qboolean OpenXR_begin_frame( void );
qboolean OpenXR_end_frame( void );

VkResult OpenXR_acquire_swapchain( uint32_t buffer_index );
qboolean OpenXR_release_swapchain( uint32_t eye );

qboolean OpenXR_IN_Frame( void );

void OpenXR_setupProjectionM(viewParms_t *dest, float zProj, float zFar, qboolean computeFrustum);

qboolean OpenXR_waitFrame( void );
qboolean OpenXR_runEvent( void );

VkResult OpenXR_acquireNextImage();
void OpenXR_SubmitFrame_layers(void);
#endif

