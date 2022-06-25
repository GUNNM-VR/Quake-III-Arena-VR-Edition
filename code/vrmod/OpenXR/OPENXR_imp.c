#include "../../renderervk/vk.h"
#ifdef XR_USE_GRAPHICS_API_VULKAN
#include "../../renderercommon/vulkan/vulkan.h"
#endif

#include "../../vrmod/VRMOD_input.h"

#include "OPENXR_imp.h"
#include "OPENXR_qinput.h"

// Function pointers for some Vulkan extension methods we'll use.
extern PFN_vkGetPhysicalDeviceFeatures qvkGetPhysicalDeviceFeatures;
extern PFN_vkGetPhysicalDeviceQueueFamilyProperties qvkGetPhysicalDeviceQueueFamilyProperties;
extern PFN_vkGetDeviceQueue qvkGetDeviceQueue;

extern Vk_Instance vk;

// Function pointers for some OpenXR extension methods we'll use.
// TODO GetVulkanGraphicsRequirementsKHR & GetVulkanGraphicsRequirements2KHR : one is for Android : use #ifdef __ANDROID__ / #else
PFN_xrGetVulkanGraphicsRequirementsKHR  qxrGetVulkanGraphicsRequirementsKHR = NULL;
PFN_xrGetVulkanGraphicsRequirements2KHR	qxrGetVulkanGraphicsRequirements2KHR = NULL;

PFN_xrGetVulkanInstanceExtensionsKHR    qxrGetVulkanInstanceExtensionsKHR = NULL;
PFN_xrGetVulkanGraphicsDeviceKHR        qxrGetVulkanGraphicsDeviceKHR = NULL;
PFN_xrGetVulkanDeviceExtensionsKHR      qxrGetVulkanDeviceExtensionsKHR = NULL;

Xr_Instance xr;

#define INIT_OPENXR_INSTANCE_FUNCTION(func) \
    if (xrGetInstanceProcAddr(xr.instance, #func, (PFN_xrVoidFunction*)(&q##func)) != XR_SUCCESS || q##func == NULL) { \
        printf("OpenXr error: failed to find entrypoint %s \n", #func); \
	}

// true if XrResult is a success code, else print error message and return false
qboolean xr_check( XrResult result, const char* format, ...) {
	if (XR_SUCCEEDED(result)) return qtrue;
	char resultString[XR_MAX_RESULT_STRING_SIZE];
	xrResultToString(xr.instance, result, resultString);
	ri.Error(ERR_FATAL, "%s [%s] (%d)\n", format, resultString, result);
	return qfalse;
}


static qboolean oxr_CreateXrInstance( void ) {
	XrResult result = XR_SUCCESS;
	uint32_t enabled_ext_count = 1;
#ifdef __ANDROID__
	const char* enabled_exts[1] = { XR_KHR_ANDROID_CREATE_INSTANCE_EXTENSION_NAME };
#else
	const char* enabled_exts[1] = { XR_KHR_VULKAN_ENABLE_EXTENSION_NAME };
#endif

	XrInstanceCreateInfo createInfo = { XR_TYPE_INSTANCE_CREATE_INFO };
	createInfo.next = XR_NULL_HANDLE;
	createInfo.enabledExtensionCount = enabled_ext_count;
	createInfo.enabledExtensionNames = enabled_exts;
	createInfo.applicationInfo.apiVersion = XR_CURRENT_API_VERSION;
	strncpy_s(createInfo.applicationInfo.applicationName, XR_MAX_APPLICATION_NAME_SIZE, "Quake 3 Arena", XR_MAX_APPLICATION_NAME_SIZE);
	strncpy_s(createInfo.applicationInfo.engineName, XR_MAX_APPLICATION_NAME_SIZE, "Q3e Vulkan OpenXR", XR_MAX_ENGINE_NAME_SIZE);

	result = xrCreateInstance(&createInfo, &xr.instance);
	if (!xr_check(result, "xrCreateInstance Failed"))
		return qfalse;

	// Get OpenXR function pointer now that we have the OpenXr instance
	INIT_OPENXR_INSTANCE_FUNCTION(xrGetVulkanGraphicsRequirementsKHR);
	INIT_OPENXR_INSTANCE_FUNCTION(xrGetVulkanGraphicsRequirements2KHR);
	INIT_OPENXR_INSTANCE_FUNCTION(xrGetVulkanInstanceExtensionsKHR);
	INIT_OPENXR_INSTANCE_FUNCTION(xrGetVulkanGraphicsDeviceKHR);
	INIT_OPENXR_INSTANCE_FUNCTION(xrGetVulkanDeviceExtensionsKHR);

	return qtrue;
}

static qboolean oxr_get_systemId( void )
{
	XrResult result = XR_SUCCESS;
	XrSystemGetInfo system_get_info;
	system_get_info.type = XR_TYPE_SYSTEM_GET_INFO;
	system_get_info.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
	result = xrGetSystem(xr.instance, &system_get_info, &xr.systemId);
	if (result == XR_ERROR_FORM_FACTOR_UNAVAILABLE) {
		//ri.Printf(PRINT_ALL, "ERROR: No OpenXR device was found.");
		ri.Error(ERR_FATAL, "ERROR: No OpenXR device was found.");
		return qfalse;
	}
	else if (!xr_check(result, "xrGetSystem Failed. No OpenXR device was found."))
		return qfalse;
	else
		return qtrue;
}

extern const char* GetValidationLayerName();

VkResult OpenXR_create_VK_instance(VkInstanceCreateInfo desc)
{
	XrResult result = XR_SUCCESS;
	uint32_t extensionNamesSize = 0;

	XrVulkanInstanceCreateInfoKHR xrcreateInfo = { XR_TYPE_VULKAN_INSTANCE_CREATE_INFO_KHR };
	xrcreateInfo.systemId = xr.systemId;
	xrcreateInfo.pfnGetInstanceProcAddr = (PFN_vkGetInstanceProcAddr)xr.pfnGetInstanceProcAddr;
	xrcreateInfo.vulkanCreateInfo = &desc;
	xrcreateInfo.vulkanAllocator = VK_NULL_HANDLE;

	result = qxrGetVulkanInstanceExtensionsKHR(xr.instance, xrcreateInfo.systemId, 0, &extensionNamesSize, NULL);
	if (!xr_check(result, "pfnGetVulkanInstanceExtensionsKHR count Failed"))
		return VK_ERROR_INITIALIZATION_FAILED;

	char* extensionNames = (char*)malloc(sizeof(char) * extensionNamesSize);

	result = qxrGetVulkanInstanceExtensionsKHR(xr.instance, xrcreateInfo.systemId, extensionNamesSize, &extensionNamesSize, &extensionNames[0]);
	if (!xr_check(result, "pfnGetVulkanInstanceExtensionsKHR names Failed"))
		return VK_ERROR_INITIALIZATION_FAILED;

	const char* extensions[32];
	ParseExtensionString(&extensionNames[0], &extensionNamesSize, extensions, 32);

	// Merge the runtime's request with the applications requests
	for (uint32_t i = 0; i < xrcreateInfo.vulkanCreateInfo->enabledExtensionCount; ++i) {
		extensions[extensionNamesSize++] = xrcreateInfo.vulkanCreateInfo->ppEnabledExtensionNames[i];
	}

	// remove duplicate
	remove_dups(&extensionNamesSize, extensions);

	// Validation layers
	const char* layers[1];
	uint16_t layersCount = 0;
#ifndef NDEBUG
	const char* const validationLayerName = GetValidationLayerName();
	if (validationLayerName) {
		layers[layersCount++] = validationLayerName;
	}
	else {
		ri.Printf(PRINT_WARNING, "No validation layers found in the system, skipping");
	}
#endif

	VkInstanceCreateInfo instInfo;
	instInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	memcpy(&instInfo, xrcreateInfo.vulkanCreateInfo, sizeof(instInfo));
	instInfo.enabledExtensionCount = extensionNamesSize;
	instInfo.ppEnabledExtensionNames = (extensionNamesSize > 0) ? extensions : VK_NULL_HANDLE;
	instInfo.enabledLayerCount = layersCount;
	instInfo.ppEnabledLayerNames = layersCount > 0 ? layers : VK_NULL_HANDLE;

	PFN_vkCreateInstance pfnCreateInstance = (PFN_vkCreateInstance)xrcreateInfo.pfnGetInstanceProcAddr(NULL, "vkCreateInstance");
	return pfnCreateInstance(&instInfo, xrcreateInfo.vulkanAllocator, &vk.instance);
}

extern PFN_vkGetDeviceQueue qvkGetDeviceQueue;
static qboolean oxr_createVulkanDevice(VkPhysicalDevice* vulkanPhysicalDevice, const VkDeviceCreateInfo deviceInfo)
{
	XrResult result = XR_SUCCESS;
	VkResult res;
	uint32_t deviceExtensionNamesSize = 0;

	XrVulkanDeviceCreateInfoKHR deviceCreateInfo = { XR_TYPE_VULKAN_DEVICE_CREATE_INFO_KHR };
	deviceCreateInfo.systemId = xr.systemId;
	deviceCreateInfo.pfnGetInstanceProcAddr = xr.pfnGetInstanceProcAddr;// Vulkan qvkGetInstanceProcAddr;
	deviceCreateInfo.vulkanCreateInfo = &deviceInfo;
	deviceCreateInfo.vulkanPhysicalDevice = *vulkanPhysicalDevice;
	deviceCreateInfo.vulkanAllocator = VK_NULL_HANDLE;

	result = qxrGetVulkanDeviceExtensionsKHR(xr.instance, deviceCreateInfo.systemId, 0, &deviceExtensionNamesSize, NULL);
	if (!xr_check(result, "qxrGetVulkanDeviceExtensionsKHR Failed"))
		return qfalse;

	char* deviceExtensionNames;
	deviceExtensionNames = (char*)malloc(sizeof(char) * deviceExtensionNamesSize);

	result = qxrGetVulkanDeviceExtensionsKHR(xr.instance, deviceCreateInfo.systemId, deviceExtensionNamesSize, &deviceExtensionNamesSize, &deviceExtensionNames[0]);
	if (!xr_check(result, "qxrGetVulkanDeviceExtensionsKHR Failed"))
		return qfalse;

	{
		const char* extensions[32];
		ParseExtensionString(&deviceExtensionNames[0], &deviceExtensionNamesSize, extensions, 32);

		// Merge the runtime's request with the applications requests
		for (uint32_t i = 0; i < deviceCreateInfo.vulkanCreateInfo->enabledExtensionCount; ++i) {
			extensions[deviceExtensionNamesSize++] = deviceCreateInfo.vulkanCreateInfo->ppEnabledExtensionNames[i];
		}

		VkPhysicalDeviceFeatures features;
		memcpy(&features, deviceCreateInfo.vulkanCreateInfo->pEnabledFeatures, sizeof(features));

#if !defined(XR_USE_PLATFORM_ANDROID) && defined(USE_OPENXR)
		VkPhysicalDeviceFeatures availableFeatures;
		qvkGetPhysicalDeviceFeatures(*vulkanPhysicalDevice, &availableFeatures);

		if (availableFeatures.shaderStorageImageMultisample == VK_TRUE) {
			// Setting this quiets down a validation error triggered by the Oculus runtime
			features.shaderStorageImageMultisample = VK_TRUE;
		}
#endif
		VkDeviceCreateInfo deviceInfo;
		deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		memcpy(&deviceInfo, deviceCreateInfo.vulkanCreateInfo, sizeof(deviceInfo));
		deviceInfo.pEnabledFeatures = &features;
		deviceInfo.enabledExtensionCount = deviceExtensionNamesSize;
		deviceInfo.ppEnabledExtensionNames = (deviceExtensionNamesSize > 0) ? extensions : VK_NULL_HANDLE;

		PFN_vkCreateDevice pfnCreateDevice = (PFN_vkCreateDevice)deviceCreateInfo.pfnGetInstanceProcAddr(vk.instance, "vkCreateDevice");
		res = pfnCreateDevice(*vulkanPhysicalDevice, &deviceInfo, deviceCreateInfo.vulkanAllocator, &vk.device);
		if (res != VK_SUCCESS)
			return qfalse;
	}

	return qtrue;
}

static qboolean oxr_get_VkPhysicalDevice(VkPhysicalDevice* vulkanPhysicalDevice)
{
	XrResult result = XR_SUCCESS;

	XrVulkanGraphicsDeviceGetInfoKHR deviceGetInfo = { XR_TYPE_VULKAN_GRAPHICS_DEVICE_GET_INFO_KHR };
	deviceGetInfo.systemId = xr.systemId;
	deviceGetInfo.vulkanInstance = vk.instance;

	result = qxrGetVulkanGraphicsDeviceKHR(xr.instance, deviceGetInfo.systemId, deviceGetInfo.vulkanInstance, vulkanPhysicalDevice);
	if (!xr_check(result, "pfnGetVulkanGraphicsDeviceKHR Failed"))
		return qfalse;

	return qtrue;
}

qboolean OpenXR_getExtensions(const char **extension_names, uint32_t *extension_count) {
	uint32_t count = *extension_count;
	extension_names[count++] = "VK_EXT_debug_report";
/*#if defined(USE_MIRROR_WINDOW)
	extension_names[count++] = "VK_KHR_surface";
#if defined(VK_USE_PLATFORM_WIN32_KHR)
	extension_names[count++] = "VK_KHR_win32_surface";
#else
#error CreateSurface not supported on this OS
#endif  // defined(VK_USE_PLATFORM_WIN32_KHR)
#endif  // defined(USE_MIRROR_WINDOW)

	remove_dups(&count, extension_names);
	*extension_count = count;

	return qtrue;*/
	return qtrue;
}

static qboolean oxr_GetVulkanGraphicsRequirements() {
	XrResult result = XR_SUCCESS;
	XrGraphicsRequirementsVulkan2KHR graphicsRequirements = { XR_TYPE_GRAPHICS_REQUIREMENTS_VULKAN2_KHR };

	//result = qxrGetVulkanGraphicsRequirements2KHR(xr.instance, xr.systemId, &graphicsRequirements); // android ?
	result = qxrGetVulkanGraphicsRequirementsKHR(xr.instance, xr.systemId, &graphicsRequirements);
	if (!xr_check(result, "qxrGetVulkanGraphicsRequirements2KHR Failed"))
		return qfalse;

	return qtrue;
}

static qboolean oxr_createSession(VkPhysicalDevice* vulkanPhysicalDevice) {
	XrResult result = XR_SUCCESS;

	XrGraphicsBindingVulkan2KHR m_graphicsBinding;
	m_graphicsBinding.type = XR_TYPE_GRAPHICS_BINDING_VULKAN2_KHR;
	m_graphicsBinding.instance = vk.instance;
	m_graphicsBinding.physicalDevice = *vulkanPhysicalDevice;
	m_graphicsBinding.device = vk.device;
	m_graphicsBinding.queueFamilyIndex = vk.queue_family_index;
	m_graphicsBinding.queueIndex = 0;// vk.queue_index;

	XrSessionCreateInfo createInfo;
	createInfo.type = XR_TYPE_SESSION_CREATE_INFO;
	createInfo.next = &m_graphicsBinding;
	createInfo.systemId = xr.systemId;

	result = xrCreateSession(xr.instance, &createInfo, &xr.session);
	if (!xr_check(result, "xrCreateSession Failed"))
		return qfalse;

	return qtrue;
}

static qboolean oxr_begin_session(void)
{
	XrSessionBeginInfo sessionBeginInfo = {
	  .type = XR_TYPE_SESSION_BEGIN_INFO,
	  .primaryViewConfigurationType = xr.view_config_type,
	};
	XrResult result = xrBeginSession(xr.session, &sessionBeginInfo);
	if (!xr_check(result, "Failed to begin session!"))
		return qfalse;

	return qtrue;
}

static qboolean _check_supported_spaces()
{
	uint32_t referenceSpacesCount;
	XrResult result = xrEnumerateReferenceSpaces(xr.session, 0, &referenceSpacesCount, NULL);
	if (!xr_check(result, "Getting number of reference spaces failed!"))
		return qfalse;

	XrReferenceSpaceType *referenceSpaces = (XrReferenceSpaceType*)malloc(sizeof(XrReferenceSpaceType) * referenceSpacesCount);

	result = xrEnumerateReferenceSpaces(xr.session, referenceSpacesCount, &referenceSpacesCount, referenceSpaces);
	if (!xr_check(result, "Enumerating reference spaces failed!"))
		return qfalse;

	qboolean localSpaceSupported = qfalse;
	qboolean viewSpaceSupported = qfalse;
	qboolean stageSpaceSupported = qfalse;
	ri.Printf(PRINT_ALL, "Enumerated %d reference spaces.", referenceSpacesCount);
	for (uint32_t i = 0; i < referenceSpacesCount; i++) {
		if (referenceSpaces[i] == XR_REFERENCE_SPACE_TYPE_LOCAL) { // LOCAL is relative to the device's starting location
			localSpaceSupported = qtrue;
		}
		if (referenceSpaces[i] == XR_REFERENCE_SPACE_TYPE_VIEW) {
			viewSpaceSupported = qtrue;
		}
		if (referenceSpaces[i] == XR_REFERENCE_SPACE_TYPE_STAGE) { // STAGE is relative to the center of the guardian system's bounds
			stageSpaceSupported = qtrue;
		}
	}

	if (!localSpaceSupported) {
		ri.Printf(PRINT_ERROR, "XR_REFERENCE_SPACE_TYPE_LOCAL unsupported.");
		return qfalse;
	}

	// Create a space to the first path
	XrReferenceSpaceCreateInfo spaceCreateInfo = {
		.type = XR_TYPE_REFERENCE_SPACE_CREATE_INFO,
		.poseInReferenceSpace.orientation.w = 1.0f,
	};

	if (viewSpaceSupported) {
		spaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW,
		result = xrCreateReferenceSpace(xr.session, &spaceCreateInfo, &xr.HeadSpace);
		if (!xr_check(result, "xrCreateReferenceSpace HeadSpace failed"))
			return qfalse;
	}

	if (localSpaceSupported) {
		spaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
		result = xrCreateReferenceSpace(xr.session, &spaceCreateInfo, &xr.LocalSpace);
		if (!xr_check(result, "xrCreateReferenceSpace local failed"))
			return qfalse;
	}

	if (stageSpaceSupported) {
		spaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_STAGE;
		spaceCreateInfo.poseInReferenceSpace.position.y = 0.0f;
		result = xrCreateReferenceSpace(xr.session, &spaceCreateInfo, &xr.StageSpace);
		if (!xr_check(result, "xrCreateReferenceSpace stage failed"))
			return qfalse;
	}

	return qtrue;
}

qboolean OpenXR_init_pre_vk( void )
{
	if (!oxr_CreateXrInstance()) {
		ri.Printf(PRINT_WARNING, "Failed to create OpenXR instance");
		return qfalse;
	}

	if (!oxr_get_systemId()) {
		//ri.Printf(PRINT_WARNING, "Failed to get OpenXR system ID");
		return qfalse;
	}

	if (!oxr_GetVulkanGraphicsRequirements()) {
		ri.Printf(PRINT_WARNING, "Failed to Get Vulkan Graphics Requirements");
		return qfalse;
	}

	return qtrue;
}

static qboolean oxr_init_views( void )
{
	XrResult result;
	uint32_t viewportConfigTypeCount;

	result = xrEnumerateViewConfigurations(xr.instance, xr.systemId, 0, &viewportConfigTypeCount, NULL);
	if (!xr_check(result, "Failed to get Viewport configuration count"))
		return qfalse;

	XrViewConfigurationType *viewportConfigurationTypes = malloc(sizeof(XrViewConfigurationType) * viewportConfigTypeCount);

	result = xrEnumerateViewConfigurations(xr.instance, xr.systemId, viewportConfigTypeCount, &viewportConfigTypeCount, viewportConfigurationTypes);
	if (!xr_check(result, "Failed to enumerate Viewport configurations!"))
		return qfalse;

	ri.Printf(PRINT_DEVELOPER, "Available Viewport Configuration Types: %d", viewportConfigTypeCount);

	// if struct (more specifically .type) is still 0 after searching,
	// then we have not found the config.
	XrViewConfigurationProperties requiredViewConfigProperties = { 0 };

	for (uint32_t i = 0; i < viewportConfigTypeCount; ++i) {
		XrViewConfigurationProperties properties = {
		  .type = XR_TYPE_VIEW_CONFIGURATION_PROPERTIES,
		};

		result = xrGetViewConfigurationProperties(xr.instance, xr.systemId, viewportConfigurationTypes[i], &properties);
		if (!xr_check(result, "Failed to get view configuration info %d!", i))
			return qfalse;

		if (viewportConfigurationTypes[i] == xr.view_config_type && properties.viewConfigurationType == xr.view_config_type) {
			requiredViewConfigProperties = properties;
		}

	}
	if (requiredViewConfigProperties.type != XR_TYPE_VIEW_CONFIGURATION_PROPERTIES) {
		ri.Printf(PRINT_ERROR, "Couldn't get required VR View Configuration from Runtime!");
		return qfalse;
	}

	// reset views
	xr.view_count = 0;

	result = xrEnumerateViewConfigurationViews(xr.instance, xr.systemId, xr.view_config_type, 0, &xr.view_count, NULL);
	if (!xr_check(result, "Failed to get view configuration view count!"))
		return qfalse;

	for (uint32_t i = 0; i < xr.view_count; ++i) {
		xr.configViews[i].type = XR_TYPE_VIEW_CONFIGURATION_VIEW;
		xr.configViews[i].next = NULL;
	}

	result = xrEnumerateViewConfigurationViews(xr.instance, xr.systemId, xr.view_config_type, xr.view_count, &xr.view_count, xr.configViews);
	if (!xr_check(result, "Failed to enumerate view configuration views!"))
		return qfalse;

	// print some log
	for (uint32_t i = 0; i < xr.view_count; i++) {
		const XrViewConfigurationView v = xr.configViews[i];
		ri.Printf(PRINT_ALL, "View [%d]: Recommended Width=%d Height=%d SampleCount=%d", i, v.recommendedImageRectWidth, v.recommendedImageRectHeight, v.recommendedSwapchainSampleCount);
		ri.Printf(PRINT_ALL, "View [%d]: Maximum Width=%d Height=%d SampleCount=%d", i, v.maxImageRectWidth, v.maxImageRectHeight, v.maxSwapchainSampleCount);
	}

	return qtrue;
}

qboolean OpenXR_InitializeSession(VkPhysicalDevice* vulkanPhysicalDevice)
{
	if (!oxr_createSession(vulkanPhysicalDevice)) {
		ri.Printf(PRINT_ERROR, "Failed to create the OpenXR session");
		return qfalse;
	}

	if (!_check_supported_spaces(vulkanPhysicalDevice)) {
		ri.Printf(PRINT_ERROR, "Failed to check the OpenXR space");
		return qfalse;
	}

	if (!OpenXR_InitializeActions()) {
		ri.Printf(PRINT_ERROR, "Failed to Initialize OpenXR Actions");
		return qfalse;
	}

	return qtrue;
}

qboolean OpenXR_init_post_vk(VkPhysicalDevice* vulkanPhysicalDevice, VkDeviceCreateInfo device_desc)
{
	xr.view_config_type = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;

	if (!oxr_init_views()) {
		ri.Printf(PRINT_WARNING, "Failed to set up the OpenXR views");
		return qfalse;
	}

	if (!oxr_get_VkPhysicalDevice(vulkanPhysicalDevice)) {
		ri.Printf(PRINT_WARNING, "Failed to get the OpenXR physical device");
		return qfalse;
	}

	if (!oxr_createVulkanDevice(vulkanPhysicalDevice, device_desc)) {
		ri.Printf(PRINT_WARNING, "Failed to get the OpenXR device");
		return qfalse;
	}

	if (!OpenXR_InitializeSession(vulkanPhysicalDevice)) {
		ri.Printf(PRINT_WARNING, "OpenXR_InitializeSession Failed");
		return qfalse;
	}

	if (!oxr_begin_session()) {
		ri.Printf(PRINT_ERROR, "Failed to begin OpenXR session");
		return qfalse;
	}

	xr.layers = malloc(sizeof(const XrCompositionLayerBaseHeader*) * xr.num_layers);

	return qtrue;
}

qboolean OpenXR_get_swapchain_format(VkSurfaceFormatKHR surface_format)
{
	XrResult result = XR_SUCCESS;
	uint32_t swapchainFormatCount;
	int64_t *swapchainFormats;

	result = xrEnumerateSwapchainFormats(xr.session, 0, &swapchainFormatCount, NULL);
	if (!xr_check(result, "Failed to get number of supported swapchain formats"))
		return qfalse;

	swapchainFormats = (int64_t*)malloc(sizeof(int64_t) * swapchainFormatCount);

	result = xrEnumerateSwapchainFormats(xr.session, swapchainFormatCount, &swapchainFormatCount, swapchainFormats);
	if (!xr_check(result, "Failed to enumerate swapchain formats"))
		return qfalse;

	ri.Printf(PRINT_ALL, "%i swapchain formats availlable :", swapchainFormatCount);

	for (uint32_t i = 0; i < swapchainFormatCount; i++) {
		ri.Printf(PRINT_ALL, "[%i] swapchain formats : %i", i, swapchainFormats[i]);
	}

	xr.swapchain_format = surface_format.format;//use q3e format

	return qtrue;
}

qboolean OpenXR_SwapChainInit( void )
{
	XrResult result = XR_SUCCESS;

	// First create swapchains and query the length for each swapchain
	xr.swapchains = (XrSwapchain*) malloc(sizeof(XrSwapchain) * xr.view_count);

	xr.swapchain_length = (uint32_t*) malloc(sizeof(uint32_t) * xr.view_count);

	xr.last_acquired = (uint32_t*) malloc(sizeof(uint32_t) * xr.view_count);

	for (uint32_t eye = 0; eye < xr.view_count; eye++) {
		const XrViewConfigurationView vp = xr.configViews[eye];
		XrSwapchainCreateInfo swapchainCreateInfo = {
			.type = XR_TYPE_SWAPCHAIN_CREATE_INFO,
			.createFlags	= 0,
			.usageFlags		= XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT,
			.format			= xr.swapchain_format,
			.sampleCount	= vp.recommendedSwapchainSampleCount,
			.width			= vp.recommendedImageRectWidth,
			.height			= vp.recommendedImageRectHeight,
			.faceCount		= 1,
			.arraySize		= 1,
			.mipCount		= 1,
		};

		ri.Printf(PRINT_ALL, "Swapchain %d dimensions: %dx%d", eye, vp.recommendedImageRectWidth, vp.recommendedImageRectHeight);

		result = xrCreateSwapchain(xr.session, &swapchainCreateInfo, &xr.swapchains[eye]);
		if (!xr_check(result, "Failed to create swapchain %d!", eye))
			return qfalse;

		result = xrEnumerateSwapchainImages(xr.swapchains[eye], 0, &xr.swapchain_length[eye], NULL);
		if (!xr_check(result, "Failed to enumerate swapchain lengths"))
			return qfalse;
	}

	xr.images = (XrSwapchainImageVulkanKHR**) malloc(sizeof(XrSwapchainImageVulkanKHR*) * xr.view_count);
	for (uint32_t eye = 0; eye < xr.view_count; eye++) {
		xr.images[eye] = (XrSwapchainImageVulkanKHR*) malloc(sizeof(XrSwapchainImageVulkanKHR) * xr.swapchain_length[eye]);

		for (uint32_t j = 0; j < xr.swapchain_length[eye]; j++) {
			xr.images[eye][j].type = XR_TYPE_SWAPCHAIN_IMAGE_VULKAN_KHR;
			xr.images[eye][j].next = NULL;
		}
	}

	for (uint32_t eye = 0; eye < xr.view_count; eye++) {
		result = xrEnumerateSwapchainImages(xr.swapchains[eye], xr.swapchain_length[eye], &xr.swapchain_length[eye], (XrSwapchainImageBaseHeader*)xr.images[eye]);
		if (!xr_check(result, "Failed to enumerate swapchains"))
			return qfalse;
		ri.Printf(PRINT_ALL, "xrEnumerateSwapchainImages: swapchain_length[%d] %d", eye, xr.swapchain_length[eye]);
	}

	return qtrue;
}

qboolean OpenXR_IN_Frame( void )
{
	XrResult result;

	// Create projection matrices and view matrices for each eye
	XrViewLocateInfo viewLocateInfo = {
	  .type = XR_TYPE_VIEW_LOCATE_INFO,
	  .viewConfigurationType = xr.view_config_type,
	  .displayTime = xr.frameState.predictedDisplayTime,
	  .space = xr.LocalSpace,
	};

	for (uint32_t i = 0; i < xr.view_count; i++) {
		xr.views[i] = (XrView) { .type = XR_TYPE_VIEW };
	};

	XrViewState viewState = {
	  .type = XR_TYPE_VIEW_STATE,
	};
	uint32_t viewCountOutput;
	result = xrLocateViews(xr.session, &viewLocateInfo, &viewState, xr.view_count, &viewCountOutput, xr.views);
	if (!xr_check(result, "Could not locate views"))
		return qfalse;


	// TODO switch to 3Dof instead of throwing error
	if ((viewState.viewStateFlags & XR_VIEW_STATE_POSITION_VALID_BIT) == 0 ||
		(viewState.viewStateFlags & XR_VIEW_STATE_ORIENTATION_VALID_BIT) == 0) {
		ri.Error(ERR_DROP, "There is no valid tracking poses for the views. (or could not find compatible OpenXR HMD)");
		return qfalse;
	}


	OpenXR_Get_HMD_Angles();

	return qtrue;
}

qboolean OpenXR_waitFrame( void )
{
	XrResult result;

	xr.frameState = (XrFrameState) {
		.type = XR_TYPE_FRAME_STATE,
	};
	XrFrameWaitInfo frameWaitInfo = {
	  .type = XR_TYPE_FRAME_WAIT_INFO,
	};
	result = xrWaitFrame(xr.session, &frameWaitInfo, &xr.frameState);
	if (!xr_check(result, "xrWaitFrame() was not successful, exiting..."))
		return qfalse;

	return qtrue;
}

qboolean OpenXR_runEvent( void )
{
	XrEventDataBuffer runtimeEvent = {
	  .type = XR_TYPE_EVENT_DATA_BUFFER,
	  .next = NULL,
	};

	XrResult pollResult = xrPollEvent(xr.instance, &runtimeEvent);
	if (pollResult == XR_SUCCESS) {
		switch (runtimeEvent.type) {
		case XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED: {
			XrEventDataSessionStateChanged* event = (XrEventDataSessionStateChanged*)&runtimeEvent;
			XrSessionState state = event->state;
			xr.is_visible = event->state <= XR_SESSION_STATE_FOCUSED;
			ri.Printf(PRINT_DEVELOPER, "EVENT: session state changed to %d. Visible: %d", state, xr.is_visible);
			
			if (event->state >= XR_SESSION_STATE_STOPPING) { // TODO
				//ri.CL_IsMinimized = qtrue;
				xr.is_running = qfalse;
			}
			break;
		}
		case XR_TYPE_EVENT_DATA_MAIN_SESSION_VISIBILITY_CHANGED_EXTX: {
			XrEventDataMainSessionVisibilityChangedEXTX* event = (XrEventDataMainSessionVisibilityChangedEXTX*)&runtimeEvent;
			xr.main_session_visible = event->visible;
		}
		default: break;
		}
	}
	else if (pollResult == XR_EVENT_UNAVAILABLE) {
		// this is the usual case
	}
	else {
		ri.Printf(PRINT_ERROR, "Failed to poll events!\n");
		return qfalse;
	}
	return qtrue;
}

qboolean OpenXR_begin_frame(void)
{
	XrResult result;

	if (!xr.is_visible) {
		//ri.CL_IsMinimized = qtrue;
		return qfalse;//TODO
	}

	// Begin frame
	XrFrameBeginInfo frameBeginInfo = {
	  .type = XR_TYPE_FRAME_BEGIN_INFO,
	};

	result = xrBeginFrame(xr.session, &frameBeginInfo);
	if (!xr_check(result, "failed to begin frame!"))
		return qfalse;

	return qtrue;
}

static qboolean quit = qfalse;

qboolean OpenXR_release_swapchain( uint32_t eye )
{
	XrResult result = XR_SUCCESS;
	XrSwapchainImageReleaseInfo info = {
	  .type = XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO,
	};
	result = xrReleaseSwapchainImage(xr.swapchains[eye], &info);
	if (!xr_check(result, "failed to release swapchain image!"))
	{
		quit = qtrue;
		return qfalse;
	}

	return qtrue;
}

void OpenXR_setupViewM( uint32_t eye )
{
	const float worldUnit = -32.0f;

	XrVector3f position_vec;
	XrVector3f_Scale(&position_vec, &xr.views[eye].pose.position, worldUnit);
	XrMatrix4x4f_CreateTranslation(&xr.viewMatrix, position_vec.x, position_vec.y, position_vec.z);
}

void OpenXR_setupFrustum(viewParms_t *dest,
	const float tanAngleLeft, const float tanAngleRight, const float tanAngleUp, float const tanAngleDown,
	GraphicsAPI graphicsApi, const XrFovf fov, const XrVector3f pos, float zProj, float zFar)
{
	float stereoSep = r_stereoSeparation->value;

	if (stereoSep != 0)
	{
		if (dest->stereoFrame == STEREO_LEFT)
			stereoSep = zProj / stereoSep;
		else if (dest->stereoFrame == STEREO_RIGHT)
			stereoSep = zProj / -stereoSep;
		else
			stereoSep = 0;
	}

	//ymax = zProj * tan(dest->fovY * M_PI / 360.0f);

	/*float tanUp = zProj * tan(DEG2RAD(fov.angleUp)* M_PI / 360.0f);
	float tanDown = zProj * tan(DEG2RAD(-fov.angleDown)* M_PI / 360.0f);
	float tanRight = zProj * tan(DEG2RAD(fov.angleRight)* M_PI / 360.0f);
	float tanLeft = zProj * tan(DEG2RAD(-fov.angleLeft)* M_PI / 360.0f);*/
		
	/*float tanUp = zProj * tan(fov.angleUp);
	float tanDown = zProj * tan(-fov.angleDown);
	float tanRight = zProj * tan((fov.angleRight) );
	float tanLeft = zProj * tan((-fov.angleLeft) );*/

	vec3_t ofsorigin;
	float oppleg, adjleg, length;
	int i;

	if (stereoSep == 0 && tanAngleLeft == -tanAngleRight)
	{
		// symmetric case can be simplified
		VectorCopy(dest->or.origin, ofsorigin);

		length = sqrt(tanAngleRight * tanAngleRight + zProj * zProj);
		oppleg = tanAngleRight / length;
		adjleg = zProj / length;

		// right frustum side plane
		VectorScale(dest->or.axis[0], oppleg, dest->frustum[0].normal);
		VectorMA(dest->frustum[0].normal, adjleg, dest-> or .axis[1], dest->frustum[0].normal);

		// left frustum side plane
		VectorScale(dest->or.axis[0], oppleg, dest->frustum[1].normal);
		VectorMA(dest->frustum[1].normal, -adjleg, dest-> or .axis[1], dest->frustum[1].normal);
	}
	else
	{
		const float worldUnit = -64.0f;
		XrVector3f position_vec;
		XrVector3f_Scale(&position_vec, &pos, worldUnit);

		// In stereo rendering, due to the modification of the projection matrix, dest->or.origin is not the
		// actual origin that we're rendering so offset the tip of the view pyramid.
		dest->or.origin[2] += position_vec.x;
		dest->or.origin[0] += position_vec.y;
		dest->or.origin[1] += position_vec.z;

		// Right frustum side plane
		oppleg = tanAngleRight + stereoSep;
		length = sqrt(oppleg * oppleg + zProj * zProj);
		VectorScale(dest->or.axis[0], oppleg / length, dest->frustum[0].normal);
		VectorMA(dest->frustum[0].normal, zProj / length, dest->or.axis[1], dest->frustum[0].normal);

		// Left frustum side plane
		oppleg = tanAngleLeft + stereoSep;
		length = sqrt(oppleg * oppleg + zProj * zProj);
		VectorScale(dest->or.axis[0], oppleg / length, dest->frustum[1].normal);
		VectorMA(dest->frustum[1].normal, zProj / length, dest->or.axis[1], dest->frustum[1].normal);
	}

	// Bottom frustum side plane
	length = sqrt(tanAngleDown * tanAngleDown + zProj * zProj);
	oppleg = tanAngleDown / length;
	adjleg = zProj / length;
	VectorScale(dest->or.axis[0], oppleg, dest->frustum[2].normal);
	VectorMA(dest->frustum[2].normal, adjleg, dest->or.axis[2], dest->frustum[2].normal);

	// Top frustum side plane
	length = sqrt(tanAngleUp * tanAngleUp + zProj * zProj);
	oppleg = tanAngleUp / length;
	adjleg = zProj / length;
	VectorScale(dest->or.axis[0], oppleg, dest->frustum[3].normal);
	VectorMA(dest->frustum[3].normal, adjleg, dest->or.axis[2], dest->frustum[3].normal);

	for (i = 0; i < 4; i++) {
		dest->frustum[i].type = PLANE_NON_AXIAL;
		dest->frustum[i].dist = DotProduct(ofsorigin, dest->frustum[i].normal);
		SetPlaneSignbits(&dest->frustum[i]);
	}

	// Set the frustum far plane
	if (zFar != 0.0f) {
		// near clipping plane
		//VectorCopy( dest->or.axis[0], dest->frustum[4].normal );
		//dest->frustum[4].type = PLANE_NON_AXIAL;
		//dest->frustum[4].dist = DotProduct( ofsorigin, dest->frustum[4].normal ) + r_znear->value;
		//SetPlaneSignbits( &dest->frustum[4] );

		//test from ioq3 quest
		vec3_t farpoint;
		VectorMA(ofsorigin, zFar, dest->or.axis[0], farpoint);
		VectorScale(dest->or.axis[0], -1.0f, dest->frustum[4].normal);
		dest->frustum[4].type = PLANE_NON_AXIAL;
		dest->frustum[4].dist = DotProduct(farpoint, dest->frustum[4].normal) + r_znear->value;
		SetPlaneSignbits(&dest->frustum[4]);
		//dest->flags |= VPF_FARPLANEFRUSTUM;
	}
}
// uidentique � tr_main
static void R_SetupFrustum(viewParms_t *dest, float xmin, float xmax, float ymax, float zProj, float zFar, float stereoSep)
{
	vec3_t ofsorigin;
	float oppleg, adjleg, length;
	int i;

	if (stereoSep == 0 && xmin == -xmax)
	{
		// symmetric case can be simplified
		VectorCopy(dest-> or .origin, ofsorigin);

		length = sqrt(xmax * xmax + zProj * zProj);
		oppleg = xmax / length;
		adjleg = zProj / length;

		VectorScale(dest-> or .axis[0], oppleg, dest->frustum[0].normal);
		VectorMA(dest->frustum[0].normal, adjleg, dest-> or .axis[1], dest->frustum[0].normal);

		VectorScale(dest-> or .axis[0], oppleg, dest->frustum[1].normal);
		VectorMA(dest->frustum[1].normal, -adjleg, dest-> or .axis[1], dest->frustum[1].normal);
	}
	else
	{
		// In stereo rendering, due to the modification of the projection matrix, dest->or.origin is not the
		// actual origin that we're rendering so offset the tip of the view pyramid.
		VectorMA(dest-> or .origin, stereoSep, dest-> or .axis[1], ofsorigin);

		oppleg = xmax + stereoSep;
		length = sqrt(oppleg * oppleg + zProj * zProj);
		VectorScale(dest-> or .axis[0], oppleg / length, dest->frustum[0].normal);
		VectorMA(dest->frustum[0].normal, zProj / length, dest-> or .axis[1], dest->frustum[0].normal);

		oppleg = xmin + stereoSep;
		length = sqrt(oppleg * oppleg + zProj * zProj);
		VectorScale(dest-> or .axis[0], -oppleg / length, dest->frustum[1].normal);
		VectorMA(dest->frustum[1].normal, -zProj / length, dest-> or .axis[1], dest->frustum[1].normal);
	}

	length = sqrt(ymax * ymax + zProj * zProj);
	oppleg = ymax / length;
	adjleg = zProj / length;

	VectorScale(dest-> or .axis[0], oppleg, dest->frustum[2].normal);
	VectorMA(dest->frustum[2].normal, adjleg, dest-> or .axis[2], dest->frustum[2].normal);

	VectorScale(dest-> or .axis[0], oppleg, dest->frustum[3].normal);
	VectorMA(dest->frustum[3].normal, -adjleg, dest-> or .axis[2], dest->frustum[3].normal);

	for (i = 0; i < 4; i++) {
		dest->frustum[i].type = PLANE_NON_AXIAL;
		dest->frustum[i].dist = DotProduct(ofsorigin, dest->frustum[i].normal);
		SetPlaneSignbits(&dest->frustum[i]);
	}

	if (zFar != 0.0f)
	{
		// near clipping plane
		/*VectorCopy( dest->or.axis[0], dest->frustum[4].normal );
		dest->frustum[4].type = PLANE_NON_AXIAL;
		dest->frustum[4].dist = DotProduct( ofsorigin, dest->frustum[4].normal ) + r_znear->value;
		SetPlaneSignbits( &dest->frustum[4] );*/

		//test from ioq3 quest
		vec3_t farpoint;
		VectorMA(ofsorigin, zFar, dest-> or .axis[0], farpoint);
		VectorScale(dest-> or .axis[0], -1.0f, dest->frustum[4].normal);
		dest->frustum[4].type = PLANE_NON_AXIAL;
		dest->frustum[4].dist = DotProduct(farpoint, dest->frustum[4].normal) + r_znear->value;
		SetPlaneSignbits(&dest->frustum[4]);
		//dest->flags |= VPF_FARPLANEFRUSTUM;
	}
}
void call_R_SetupFrustum(viewParms_t *dest, float zProj, float zFar, qboolean computeFrustum) {
	float	xmin, xmax, ymin, ymax;
	float	width, height, stereoSep = r_stereoSeparation->value;

	/*
	 * offset the view origin of the viewer for stereo rendering
	 * by setting the projection matrix appropriately.
	 */

	if (stereoSep != 0)
	{
		if (dest->stereoFrame == STEREO_LEFT)
			stereoSep = zProj / stereoSep;
		else if (dest->stereoFrame == STEREO_RIGHT)
			stereoSep = zProj / -stereoSep;
		else
			stereoSep = 0;
	}

	ymax = zProj * tan(dest->fovY * M_PI / 360.0f);
	ymin = -ymax;

	xmax = zProj * tan(dest->fovX * M_PI / 360.0f);
	xmin = -xmax;

	width = xmax - xmin;
	height = ymax - ymin;

	// Now that we have all the data for the projection matrix we can also setup the view frustum.
	if (computeFrustum)
		R_SetupFrustum(dest, xmin, xmax, ymax, zProj, zFar, stereoSep);
}

// angles to the right of the center and upwards from the center are positive
// angles to the left of the center and down from the center are negative.
// total horizontal fov = angleRight - angleLeft
// total vertical fov = angleUp - angleDown.
// for a symmetric fov, angleRight and angleUp will have positive values, angleLeft will be -angleRight, and angleDown will be -angleUp.
// angles must be specified in radians, and must be between - Pi / 2 and Pi / 2 exclusively.
// When angleLeft > angleRight, the content of the view must be flipped horizontally.
// When angleDown > angleUp, the content of the view must be flipped vertically.
void OpenXR_setupProjectionM(viewParms_t *dest, float zProj, float zFar, qboolean computeFrustum) {
	int eye = max((dest->stereoFrame - 1), 0);

	XrFovf fov = xr.views[eye].fov;
	const float tanLeft = tanf(fov.angleLeft);
	const float tanRight = tanf(fov.angleRight);
	const float tanDown = tanf(fov.angleDown);
	const float tanUp = tanf(fov.angleUp);

	XrMatrix4x4f_CreateProjection((XrMatrix4x4f*)&tr.viewParms.projectionMatrix, GRAPHICS_VULKAN, tanLeft, tanRight, tanUp, tanDown, r_znear->value, zFar);// , fov);

	// stereo eye displacement is set in the view matrix
	OpenXR_setupViewM(eye);

	// Now that we have all the data for the projection matrix we can also setup the view frustum.
	// This one uses 4 fov of OpenXR but bug with negative horizontal angle
	//OpenXR_setupFrustum(&tr.viewParms, GRAPHICS_VULKAN, tanLeft, tanRight, tanUp, tanDown, fov, pos, zProj, zFar);

	// this one is a copy from tr_main, TODO: delete call_R_SetupFrustum() & R_SetupFrustum() of this file
	call_R_SetupFrustum(&tr.viewParms, zProj, zFar, qtrue);
}


qboolean OpenXR_end_frame(void)
{
	XrResult result;

	static const XrCompositionLayerBaseHeader* layers[1];

	// array of view_count containers for submitting swapchains with rendered VR frames
	static XrCompositionLayerProjectionView projection_views[2];

	static XrCompositionLayerProjection layer;

	for (uint32_t i = 0; i < xr.view_count; i++)
	{
		projection_views[i] = (XrCompositionLayerProjectionView) {
			.type = XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW,
				.subImage = {
					.swapchain = xr.swapchains[i],
					.imageRect = {
					.extent = {
						.width	= (int32_t)xr.configViews[i].recommendedImageRectWidth,
						.height	= (int32_t)xr.configViews[i].recommendedImageRectHeight,
					},
				},
			},
		};
	}

	layer = (XrCompositionLayerProjection)
	{
		.type = XR_TYPE_COMPOSITION_LAYER_PROJECTION,
		.layerFlags = XR_COMPOSITION_LAYER_CORRECT_CHROMATIC_ABERRATION_BIT,//XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT | XR_COMPOSITION_LAYER_CORRECT_CHROMATIC_ABERRATION_BIT,
		.space = xr.LocalSpace,
		.viewCount = xr.view_count,
		.views = projection_views,
	};


	//if (xr.is_visible) {
		layers[0] = (const XrCompositionLayerBaseHeader* const)&layer;

		for (uint32_t i = 0; i < xr.view_count; i++) {
			projection_views[i].pose = xr.views[i].pose;
			projection_views[i].fov = xr.views[i].fov;
		}

		XrFrameEndInfo frameEndInfo =
		{
			.type = XR_TYPE_FRAME_END_INFO,
			.displayTime = xr.frameState.predictedDisplayTime,
			.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE,
			.layerCount = 1,
			.layers = layers,
		};

		result = xrEndFrame(xr.session, &frameEndInfo);
		if (!xr_check(result, "failed to end frame!"))
			return qfalse;
	//}

	return qtrue;
}


qboolean OpenXR_acquire_swapchain(uint32_t eye)
{
	XrResult result;

	XrSwapchainImageAcquireInfo acquire_info = {
	  .type = XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO,
	};

	result = xrAcquireSwapchainImage(xr.swapchains[eye], &acquire_info, &xr.last_acquired[eye]);
	if (!xr_check(result, "failed to acquire swapchain image!"))
		return qfalse;

	XrSwapchainImageWaitInfo wait_info = {
	  .type = XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO,
	  .timeout = INT64_MAX,
	};
	result = xrWaitSwapchainImage(xr.swapchains[eye], &wait_info);
	if (!xr_check(result, "failed to wait for swapchain image!"))
		return qfalse;

	return qtrue;
}

static qboolean eyeAcquired[2];

qboolean OpenXR_PollActions( void ) {
	XrResult result;

	// Sync actions
	const XrActiveActionSet activeActionSet = { xr.input.actionSet, XR_NULL_PATH };

	XrActionsSyncInfo syncInfo;
	syncInfo.type = XR_TYPE_ACTIONS_SYNC_INFO;
	syncInfo.countActiveActionSets = 1;
	syncInfo.activeActionSets = &activeActionSet;
	result = xrSyncActions(xr.session, &syncInfo);
	if (!xr_check(result, "xrSyncActions failed"))
		return qfalse;

	// Get pose and action
	//for (auto hand : { Side::LEFT, Side::RIGHT }) {
	for (uint16_t hand = 0; hand < 2; hand++ ) {
		xr.input.handActive[hand] = qfalse;
		qboolean isRight = (hand == SideRIGHT) ? qtrue : qfalse;

		// reuse the same struct for several actions
		XrActionStateGetInfo getInfo;
		getInfo.type = XR_TYPE_ACTION_STATE_GET_INFO;

		//---------------------------------
		//	thumbstick Action
		//---------------------------------
		//XrActionStateGetInfo getInfostick;
		getInfo.action = xr.input.thumbstickAction;
		getInfo.subactionPath = xr.input.handSubactionPath[hand];

		XrActionStateVector2f thumbstickValue;
		thumbstickValue.type = XR_TYPE_ACTION_STATE_VECTOR2F;

		result = xrGetActionStateVector2f(xr.session, &getInfo, &thumbstickValue);
		if (!xr_check(result, "Get thumbstick Action failed"))
			return qfalse;

		if (thumbstickValue.isActive == XR_TRUE) {
			VRMOD_IN_Joystick(isRight, thumbstickValue.currentState.x, thumbstickValue.currentState.y);
		}

		//---------------------------------
		//	thumbstick click Action
		//---------------------------------	
		getInfo.action = xr.input.thumbstickClickAction;
		getInfo.subactionPath = xr.input.handSubactionPath[hand];
		XrActionStateBoolean thumbstickClickValue;
		thumbstickClickValue.type = XR_TYPE_ACTION_STATE_BOOLEAN;
		result = xrGetActionStateBoolean(xr.session, &getInfo, &thumbstickClickValue);
		if (!xr_check(result, "Get thumbstick click Action failed"))
			return qfalse;

		if (thumbstickClickValue.isActive == XR_TRUE) {
			xrButton thumbClick = (hand == SideRIGHT) ? xrButton_LThumb : xrButton_RThumb;
			VRMOD_IN_Button(isRight, thumbClick, thumbstickClickValue.currentState);
		}

		//---------------------------------
		//	Trigger Action
		//---------------------------------
		getInfo.action = xr.input.triggerAction;
		getInfo.subactionPath = xr.input.handSubactionPath[hand];

		XrActionStateFloat triggerValue;
		triggerValue.type = XR_TYPE_ACTION_STATE_FLOAT;
		result = xrGetActionStateFloat(xr.session, &getInfo, &triggerValue);
		if (!xr_check(result, "Get Trigger Action failed"))
			return qfalse;

		if (triggerValue.isActive == XR_TRUE) {
			VRMOD_IN_Triggers(isRight, triggerValue.currentState);
		}

		//---------------------------------
		//	button [Grab] Action
		//---------------------------------
		getInfo.action = xr.input.grabAction;
		getInfo.subactionPath = xr.input.handSubactionPath[hand];

		XrActionStateFloat grabValue;
		grabValue.type = XR_TYPE_ACTION_STATE_FLOAT;
		result = xrGetActionStateFloat(xr.session, &getInfo, &grabValue);
		if (!xr_check(result, "Get grab Action failed"))
			return qfalse;

		if (grabValue.isActive == XR_TRUE) {
			static qboolean state;
			VRMOD_IN_Grab(isRight, grabValue.currentState);

			//TODO create a fonction: SendHaptic(XRController.rightHand, amplitude, duration);

			if (grabValue.currentState > 0.9f) {
				state = qtrue;
				// fixme why haptic is not working?
				XrHapticVibration vibration;
				vibration.type = XR_TYPE_HAPTIC_VIBRATION;
				vibration.amplitude = 0.5;
				//vibration.duration = XR_MIN_HAPTIC_DURATION;
				//vibration.frequency = XR_FREQUENCY_UNSPECIFIED;
				vibration.duration = 1.0;
				vibration.frequency = 0.5;

				XrHapticActionInfo hapticActionInfo;
				hapticActionInfo.type = XR_TYPE_HAPTIC_ACTION_INFO;
				hapticActionInfo.action = xr.input.vibrateAction;
				hapticActionInfo.subactionPath = xr.input.handSubactionPath[hand];
				result = xrApplyHapticFeedback(xr.session, &hapticActionInfo, (XrHapticBaseHeader*)&vibration);
				if (!xr_check(result, "xrApplyHapticFeedback grab failed"))
					return qfalse;
			}
			else if (grabValue.currentState > 0.3f && state )
			{
				// release
				//send_action("+weapon_select");
				// on release, select the focused weapon
				Cbuf_AddText("weapon_select");
				state = qfalse;
			}
		}

		//---------------------------------
		//	button [A] Action
		//---------------------------------
		getInfo.action = xr.input.button_a_Action;
		getInfo.subactionPath = xr.input.handSubactionPath[hand];
		XrActionStateBoolean button_a_Value;
		button_a_Value.type = XR_TYPE_ACTION_STATE_BOOLEAN;
		result = xrGetActionStateBoolean(xr.session, &getInfo, &button_a_Value);
		if (!xr_check(result, "Get [A] click Action failed"))
			return qfalse;

		if (button_a_Value.isActive == XR_TRUE) {
			VRMOD_IN_Button(isRight, xrButton_A, button_a_Value.currentState);
		}

		//---------------------------------
		//	button [B] Action
		//---------------------------------
		getInfo.action = xr.input.button_b_Action;
		getInfo.subactionPath = xr.input.handSubactionPath[hand];
		XrActionStateBoolean button_b_Value;
		button_b_Value.type = XR_TYPE_ACTION_STATE_BOOLEAN;
		result = xrGetActionStateBoolean(xr.session, &getInfo, &button_b_Value);
		if (!xr_check(result, "Get [B] click Action failed"))
			return qfalse;

		if (button_b_Value.isActive == XR_TRUE) {
			VRMOD_IN_Button(isRight, xrButton_B, button_b_Value.currentState);
		}

		//---------------------------------
		//	button [X] Action
		//---------------------------------
		getInfo.action = xr.input.button_x_Action;
		getInfo.subactionPath = xr.input.handSubactionPath[hand];
		XrActionStateBoolean button_x_Value;
		button_x_Value.type = XR_TYPE_ACTION_STATE_BOOLEAN;
		result = xrGetActionStateBoolean(xr.session, &getInfo, &button_x_Value);
		if (!xr_check(result, "Get [X] click Action failed"))
			return qfalse;

		if (button_x_Value.isActive == XR_TRUE) {
			VRMOD_IN_Button(isRight, xrButton_X, button_x_Value.currentState);
		}

		//---------------------------------
		//	button [Y] Action
		//---------------------------------
		getInfo.action = xr.input.button_y_Action;
		getInfo.subactionPath = xr.input.handSubactionPath[hand];
		XrActionStateBoolean button_y_Value;
		button_y_Value.type = XR_TYPE_ACTION_STATE_BOOLEAN;
		result = xrGetActionStateBoolean(xr.session, &getInfo, &button_y_Value);
		if (!xr_check(result, "Get [Y] click Action failed"))
			return qfalse;

		if (button_y_Value.isActive == XR_TRUE) {
			VRMOD_IN_Button(isRight, xrButton_Y, button_y_Value.currentState);
		}

		//---------------------------------
		//	button [left menu] Action
		// no menu right availlable on oculus touch
		//---------------------------------
		getInfo.action = xr.input.menuAction;
		getInfo.subactionPath = xr.input.handSubactionPath[hand];
		XrActionStateBoolean button_menu_Value;
		button_menu_Value.type = XR_TYPE_ACTION_STATE_BOOLEAN;
		result = xrGetActionStateBoolean(xr.session, &getInfo, &button_menu_Value);
		if (!xr_check(result, "Get [menu] click Action failed"))
			return qfalse;

		if (button_menu_Value.isActive == XR_TRUE) {
			VRMOD_IN_Button(isRight, xrButton_Enter, button_menu_Value.currentState);
		}
		//---------------------------------
		//	pose Action
		//---------------------------------
		getInfo.action = xr.input.pointerAction;
		XrActionStatePose poseState;
		poseState.type = XR_TYPE_ACTION_STATE_POSE;
		result = xrGetActionStatePose(xr.session, &getInfo, &poseState);
		if (!xr_check(result, "xrGetActionStatePose failed"))
			return qfalse;
		xr.input.handActive[hand] = poseState.isActive;

		//---------------------------------
		//	[exit] Action
		//---------------------------------
		/*
		// There were no subaction paths specified for the quit action, because we don't care which hand did it.
		getInfo.action = xr.input.quitAction;
		//getInfo.subactionPath = XR_NULL_PATH;

		XrActionStateBoolean quitValue;
		quitValue.type = XR_TYPE_ACTION_STATE_BOOLEAN;
		result = xrGetActionStateBoolean(xr.session, &getInfo, &quitValue);
		if (!xr_check(result, "xrGetActionStateBoolean failed"))
			return qfalse;
		if ((quitValue.isActive == XR_TRUE) && (quitValue.changedSinceLastSync == XR_TRUE) && (quitValue.currentState == XR_TRUE)) {
			result = xrRequestExitSession(xr.session);
			if (!xr_check(result, "xrRequestExitSession failed"))
				return qfalse;
		}*/
	}

	return qtrue;
}

VkResult OpenXR_acquireNextImage()
{
	VkResult res = VK_SUCCESS;

	if (vk.eye == 1) return VK_SUCCESS;

	if (!eyeAcquired[0] && !eyeAcquired[1])
	{
		if (!OpenXR_waitFrame())
			return VK_NOT_READY;

		if (!OpenXR_runEvent())
			return VK_NOT_READY;

		if (!OpenXR_begin_frame())
			res = VK_NOT_READY;

		// acquire left & right eye images
		for (uint16_t i = 0; i < 2; i++)
		{
			if (!OpenXR_acquire_swapchain(i)) {
				res = VK_NOT_READY;
				break;
			}
			//vk.swapchain_image_index[i] = xr.last_acquired[i];
			vk.swapchain_image_index = xr.last_acquired[i];
			eyeAcquired[i] = qtrue;
		}

		res = VK_SUCCESS;
	}

	return res;
}

void OpenXR_SubmitFrame_layers(void)
{
	if (vk.eye == 0) return;

	if (eyeAcquired[0] && eyeAcquired[1])
	{
		for (uint16_t i = 0; i < 2; i++) {
			if (!OpenXR_release_swapchain(i))
				return;
			eyeAcquired[i] = qfalse;
		}

		if (!OpenXR_end_frame())
			return;
	}
}
