#include "VRAPI_imp.h"
#include "../../renderervk/vk.h"
#include "../../vrmod/VRMOD_common.h"
#include "../../vrmod/VRMOD_input.h"

static ovrLayerProjection2 layer;
static cvar_t   *vrapi_CPU_LEVEL = NULL;
static cvar_t   *vrapi_GPU_LEVEL = NULL;

void VRAPI_SetFPS(ovrMobile* myOvr, ovrJava myjava )
{
	int hmdType = vrapi_GetSystemPropertyInt(&myjava, VRAPI_SYS_PROP_DEVICE_TYPE); // oculus GO: 64 | Quest2: 320
	// Querie the available refresh rates, for debug
	/*
	ALOGV("----------------------------------------------------");
	int modes_count = vrapi_GetSystemPropertyInt(&AppState.Java, VRAPI_SYS_PROP_NUM_SUPPORTED_DISPLAY_REFRESH_RATES);
	float modes[100];
	int nb = vrapi_GetSystemPropertyFloatArray(&AppState.Java, VRAPI_SYS_PROP_SUPPORTED_DISPLAY_REFRESH_RATES, modes, 100);
	//ALOGV("modes_count %d result howmany %d", modes_count, nb);
	ALOGV(" %d refresh rates supported by a device:", modes_count);
	for (int i = 0; i < nb; i++)
		ALOGV("modes %d %f", i, modes[i]);
	ALOGV("----------------------------------------------------");
	*/

	float refreshRate = 72.0F;

	switch (hmdType) {
		// OculusGo API is no more in the same vrapi than Quest
#ifdef OCULUSGO
		case VRAPI_DEVICE_TYPE_OCULUSGO:
			ri.Printf( PRINT_ALL, "Oculus Go detected, set refresh rate to 72");
			refreshRate = 72.0F;
			break;
#else
		case VRAPI_DEVICE_TYPE_OCULUSQUEST2:
			ri.Printf( PRINT_ALL, "Quest2 detected, set refresh rate to 120 fps");
			refreshRate = 120.0F;
			break;
#endif
		case VRAPI_DEVICE_TYPE_OCULUSQUEST:
			ri.Printf( PRINT_ALL, "Quest detected, set refresh rate to 90 fps");
			refreshRate = 90.0F;
			break;
	}

	ovrResult result = vrapi_SetDisplayRefreshRate( myOvr, refreshRate );
	switch (result) {
		case ovrSuccess:
			ri.Printf( PRINT_ALL, "Refresh rate is set to %iHz.\n", (int)refreshRate);
			break;
		case ovrError_InvalidParameter:
			ri.Printf( PRINT_ALL, "Refresh rate (%iHz) not supported by the device.\n", (int)refreshRate);
			break;
		case ovrError_InvalidOperation:
			ri.Printf( PRINT_ALL, "Refresh rate (%iHz) not allowed. Is the device in low power mode?)\n", (int)refreshRate);
			break;
		default:
			ri.Printf( PRINT_ALL, "Failed to change refresh rate to %iHz. Result = %d \n", (int)refreshRate, result);
	}
}

void VRAPI_ExtraLatencyMode( androidApp_struct * app ) {
    // Too much stuttering, test later when less stalls frames
    ovrResult res = vrapi_SetExtraLatencyMode(app->Ovr, VRAPI_EXTRA_LATENCY_MODE_ON);
    if (res == ovrSuccess)
        ri.Printf( PRINT_ALL, "SetExtraLatencyMode success \n");
    else
        ri.Printf( PRINT_ERROR, "SetExtraLatencyMode failed \n");
}
/*
================================================================================
VRAPI_processAppLifeEvent
================================================================================
*/
void VRAPI_processAppLifeEvent( androidApp_struct * app )
{
	if ( app->Resumed != false && app->NativeWindow != NULL )
	{
		if ( app->Ovr == NULL )
		{
			ovrModeParmsVulkan parms = vrapi_DefaultModeParmsVulkan( &app->Java, (unsigned long long)vk.queue );
			// No need to reset the FLAG_FULLSCREEN window flag when using a View
			parms.ModeParms.Flags &= ~VRAPI_MODE_FLAG_RESET_WINDOW_FULLSCREEN;
			parms.ModeParms.Flags |= VRAPI_MODE_FLAG_NATIVE_WINDOW;
			//TODO Enable Phase Sync https://developer.oculus.com/documentation/unity/enable-phase-sync/
			//parms.ModeParms.Flags |= VRAPI_MODE_FLAG_PHASE_SYNC;
			parms.ModeParms.WindowSurface = (size_t)app->NativeWindow;
			// Leave explicit egl objects defaulted.
			parms.ModeParms.Display = 0;
			parms.ModeParms.ShareContext = 0;

			app->Ovr = vrapi_EnterVrMode( (ovrModeParms *)&parms );

			// If entering VR mode failed then the ANativeWindow was not valid.
			if ( app->Ovr == NULL )
			{
				ri.Printf( PRINT_ERROR, "Invalid ANativeWindow!\n" );
				app->NativeWindow = NULL;
			}

			// Set performance parameters once we have entered VR mode and have a valid ovrMobile.
			if ( app->Ovr != NULL )
			{
				ri.Printf( PRINT_ALL, "---------- Set VRAPI performance parameters ----------");
				vrapi_SetClockLevels( app->Ovr, vrapi_CPU_LEVEL->integer, vrapi_GPU_LEVEL->integer );
				ri.Printf( PRINT_ALL, "\t\tSet VRAPI clock levels, CPU : %d, GPU %d \n", vrapi_CPU_LEVEL->integer, vrapi_GPU_LEVEL->integer );

				vrapi_SetPerfThread( app->Ovr, VRAPI_PERF_THREAD_TYPE_MAIN, app->MainThreadTid );
				ri.Printf( PRINT_ALL, "\t\tvrapi_SetPerfThread( MAIN, %d ) \n", app->MainThreadTid );

				vrapi_SetPerfThread( app->Ovr, VRAPI_PERF_THREAD_TYPE_RENDERER, app->RenderThreadTid );
				ri.Printf( PRINT_ALL, "\t\tvrapi_SetPerfThread( RENDERER, %d ) \n", app->RenderThreadTid );

#ifdef OCULUSGO
				vrapi_SetPropertyInt(&AppState.Java, VRAPI_REORIENT_HMD_ON_CONTROLLER_RECENTER, 1 ); // VRAPI < 1.5
#endif
				VRAPI_SetFPS(app->Ovr, AppState.Java);

				VRAPI_ExtraLatencyMode(app);//see no change.

				vr_info.hmdinfo.fov_x = vrapi_GetSystemPropertyInt( &AppState.Java, VRAPI_SYS_PROP_SUGGESTED_EYE_FOV_DEGREES_X);
				vr_info.hmdinfo.fov_y = vrapi_GetSystemPropertyInt( &AppState.Java, VRAPI_SYS_PROP_SUGGESTED_EYE_FOV_DEGREES_Y);
			}
		}
	}
	else
	{
		if ( app->Ovr != NULL )
		{
			vrapi_LeaveVrMode( app->Ovr );
			app->Ovr = NULL;
		}
	}
}

void VRAPI_CheckEvent( struct android_app* app )
{
	int events; // Used to poll the events in the loops
	struct android_poll_source* source;
	// Read all pending events.
	for ( ; ; )
	{
		const int timeoutMilliseconds = (AppState.Ovr == NULL && app->destroyRequested == 0 ) ? -1 : 0;
		if ( ALooper_pollAll( timeoutMilliseconds, NULL, &events, (void **)&source ) < 0 )
			break;

		// Process this event.
		if ( source != NULL )
			source->process( app, source );

		VRAPI_processAppLifeEvent(&AppState);
	}
}

static float get_ipd_From_EyesVector( void ) {
	ovrTracking2 head_tracker = predictedTracking;//session->get_head_tracker();

	// here we compute the vector from the origin of the left eye to the right eye by
	// using the transform part of the view matrix
	float dx = head_tracker.Eye[VRAPI_EYE_RIGHT].ViewMatrix.M[0][3] -
			   head_tracker.Eye[VRAPI_EYE_LEFT ].ViewMatrix.M[0][3];
	float dy = head_tracker.Eye[VRAPI_EYE_RIGHT].ViewMatrix.M[1][3] -
			   head_tracker.Eye[VRAPI_EYE_LEFT ].ViewMatrix.M[1][3];
	float dz = head_tracker.Eye[VRAPI_EYE_RIGHT].ViewMatrix.M[2][3] -
			   head_tracker.Eye[VRAPI_EYE_LEFT ].ViewMatrix.M[2][3];

	// the IPD is then just the length of this vector
	float ipd = sqrtf(dx * dx + dy * dy + dz * dz);
	//ALOGE("dx: %f dy: %f dz: %f" , dx, dy, dz);
	//ALOGE("ipd: %i" , (int)( ipd*1000.0f ));
	return ipd * 1000.0f;
}

// compute the distance from left eye to right eye by
// using the transform part of the view matrix
static float VRAPI_get_ipd_simple( void ) {
	float dx = 	predictedTracking.Eye[VRAPI_EYE_RIGHT].ViewMatrix.M[0][3] -
				predictedTracking.Eye[VRAPI_EYE_LEFT ].ViewMatrix.M[0][3];
	return fabs( dx * 1000.0f);
}

void VRAPI_IN_Frame( void )
{
	// Read all pending events.
	VRAPI_CheckEvent(AppState.NativeApp);

	// This is the only place the frame index is incremented, right before
	// calling vrapi_GetPredictedDisplayTime().
	AppState.FrameIndex++;

	// Get the HMD pose, predicted for the middle of the time period during which
	// the new eye images will be displayed. The number of frames predicted ahead
	// depends on the pipeline depth of the engine and the synthesis rate.
	// The better the prediction, the less black will be pulled in at the edges.
	AppState.DisplayTime = vrapi_GetPredictedDisplayTime(AppState.Ovr, AppState.FrameIndex );
	predictedTracking = vrapi_GetPredictedTracking2(AppState.Ovr, AppState.DisplayTime );
	layer.HeadPose = predictedTracking.HeadPose;

	// if vr_use_hmd_ipd 0 r_stereoSeparation will be used, if 1 use the hmd ipd
	/*if ( vr_use_hmd_ipd->integer ) {
		vr_info.hmdinfo.ipd = VRAPI_get_ipd_simple();
		r_stereoSeparation->value = vr_info.hmdinfo.ipd;
	}*/
}

void VRAPI_Get_HMD_Info( void )
{
	// if vr_use_hmd_ipd 0 r_stereoSeparation will be used, if 1 use the hmd ipd
	if ( vr_use_hmd_ipd->integer ) {
		vr_info.hmdinfo.ipd = VRAPI_get_ipd_simple();
		r_stereoSeparation->value = vr_info.hmdinfo.ipd;
	}
}

/*
================================================================================
VRAPI_SubmitFrame_layers
================================================================================
*/
void VRAPI_SubmitFrame_layers( void )
{
	const ovrMatrix4f defaultProjection = ovrMatrix4f_CreateProjectionFov( vr_info.hmdinfo.fov_x, vr_info.hmdinfo.fov_y, 0.0f, 0.0f, 1.0f, 0.0f );
	const ovrLayerHeader2 * layers[] = { &layer.Header };
	ovrSubmitFrameDescription2 frameDesc = { 0 };

	layer.Textures[vk.eye].ColorSwapChain = colorSwapChains[vk.eye].SwapChain;
	layer.Textures[vk.eye].SwapChainIndex = vk.swapchain_image_index;
	layer.Textures[vk.eye].TexCoordsFromTanAngles = ovrMatrix4f_TanAngleMatrixFromProjection(&defaultProjection);

#ifdef USE_MULTIVIEW
	if( vk.eye > 0 || isMultiview)
#else
	if( vk.eye > 0)
#endif
	{
		frameDesc.Flags		 	= 0;
		frameDesc.SwapInterval  = AppState.SwapInterval;
		frameDesc.FrameIndex	= AppState.FrameIndex;
		frameDesc.DisplayTime   = AppState.DisplayTime;
		frameDesc.LayerCount	= 1;
		frameDesc.Layers		= layers;

		// Hand over the eye images to the time warp.
		vrapi_SubmitFrame2(AppState.Ovr, &frameDesc );
	}
}

/*
================================================================================
ovrColorSwapChain_Init
================================================================================
*/
static qboolean VRAPI_ColorSwapChain_Init( ovrColorSwapChain * swapChain, const VkFormat colorFormat, const int width, const int height )
{
	// Create a triple buffered swapChain
#ifdef USE_MULTIVIEW
	swapChain->SwapChain = vrapi_CreateTextureSwapChain3( isMultiview ? VRAPI_TEXTURE_TYPE_2D_ARRAY : VRAPI_TEXTURE_TYPE_2D, colorFormat, width, height, 1, 3 );
#else
	swapChain->SwapChain = vrapi_CreateTextureSwapChain3( VRAPI_TEXTURE_TYPE_2D, colorFormat, width, height, 1, 3 );
#endif
	swapChain->SwapChainLength = vrapi_GetTextureSwapChainLength( swapChain->SwapChain );
	swapChain->ColorTextures = (VkImage *) malloc( swapChain->SwapChainLength * sizeof( VkImage ) );
	swapChain->FragmentDensityTextures = (VkImage *) malloc( swapChain->SwapChainLength * sizeof( VkImage ) );
	swapChain->FragmentDensityTextureSizes = (VkExtent2D *) malloc( swapChain->SwapChainLength * sizeof( VkExtent2D ) );

	for ( int i = 0; i < swapChain->SwapChainLength; i++ )
	{
		swapChain->ColorTextures[i] = vrapi_GetTextureSwapChainBufferVulkan( swapChain->SwapChain, i );
		if ( swapChain->FragmentDensityTextures != NULL && swapChain->FragmentDensityTextureSizes != NULL )
		{
			ovrResult result = vrapi_GetTextureSwapChainBufferFoveationVulkan(
					swapChain->SwapChain, i,
					&swapChain->FragmentDensityTextures[i],
					&swapChain->FragmentDensityTextureSizes[i].width,
					&swapChain->FragmentDensityTextureSizes[i].height );
			if ( result != ovrSuccess )
			{
				free( swapChain->FragmentDensityTextures );
				free( swapChain->FragmentDensityTextureSizes );
				swapChain->FragmentDensityTextures = NULL;
				swapChain->FragmentDensityTextureSizes = NULL;
			}
		}
	}
	return qtrue;
}


/*
================================================================================
VRAPI_SwapChainInit
================================================================================
*/
void VRAPI_SwapChainInit(void )
{
	//bool useFFR = supportsFragmentDensity;
#ifdef USE_MULTIVIEW
	vk.NumEyes = isMultiview ? 1 : VR_FRAME_LAYER_EYE_MAX;
#else
	vk.NumEyes = VR_FRAME_LAYER_EYE_MAX;
#endif
	// Get swapchain images from vrapi first so that we know what attachments to use for the renderpass
	for (int eye = 0; eye < vk.NumEyes; eye++ )
	{
		VRAPI_ColorSwapChain_Init( &colorSwapChains[eye], VK_FORMAT_R8G8B8A8_UNORM, glConfig.vidWidth, glConfig.vidHeight );
		//useFFR = useFFR && colorSwapChains[eye].FragmentDensityTextures != NULL;

		vk.swapchain_image_count = MIN( colorSwapChains[eye].SwapChainLength, MAX_SWAPCHAIN_IMAGES );
	}
	vk.fastSky = qtrue;

	// Done here to avoid doing it every frame
	layer = vrapi_DefaultLayerProjection2();
	layer.Header.Flags |= VRAPI_FRAME_LAYER_FLAG_CHROMATIC_ABERRATION_CORRECTION;

	// initialisation done here because needed Cvar_Get()
	vrapi_CPU_LEVEL = Cvar_Get( "vrapi_CPU_LEVEL", "2", CVAR_USERINFO );
	vrapi_GPU_LEVEL = Cvar_Get( "vrapi_GPU_LEVEL", "2", CVAR_USERINFO );
}


/*
================================================================================
Vulkan initialisation stuff
================================================================================
*/
void VRAPI_Init(const ovrJava * java )
{
	ovrInitParms initParms = vrapi_DefaultInitParms( java );
	initParms.GraphicsAPI = VRAPI_GRAPHICS_API_VULKAN_1;
	int32_t initResult = vrapi_Initialize( &initParms );
	if ( initResult != VRAPI_INITIALIZE_SUCCESS )
	{
		ri.Printf( PRINT_ERROR, "Failed to initialize VrApi\n" );
		// If intialization failed, vrapi_* function calls will not be available.
		exit( 0 );
	}
}

const char * VRAPI_ext_array[32];
uint32_t VRAPI_ext_count = 0;

static void initVRAPI_InstanceExtension_array( )
{
	char VrApi_InstanceExtensionNames[4096];
	uint32_t VrApi_InstanceExtensionNamesSize = sizeof( VrApi_InstanceExtensionNames );
	if ( vrapi_GetInstanceExtensionsVulkan( VrApi_InstanceExtensionNames, &VrApi_InstanceExtensionNamesSize ) )
	{
		ri.Printf( PRINT_ERROR, "vrapi_GetInstanceExtensionsVulkan FAILED" );
		vrapi_Shutdown();
		exit( 0 );
	}
	ParseExtensionString(VrApi_InstanceExtensionNames, &VRAPI_ext_count, VRAPI_ext_array, 32 );
}

qboolean VRAPI_getInstanceExtensions(const char *ext )
{
	if ( VRAPI_ext_array[0] == NULL )
		initVRAPI_InstanceExtension_array();

	for (uint32_t i = 0; i < VRAPI_ext_count; i++ )
	{
		if (Q_stricmp( ext, VRAPI_ext_array[i] ) == 0)
		{
			ri.Printf( PRINT_ALL, "\t Add extension %s needed by VRAPI", ext );
			return qtrue;
		}

	}
	return qfalse;
}

void VRAPI_addDeviceExtensions(const char **extension_list, uint32_t *extension_count )
{
	// Add q3e extensions into a string separate by a space
	char q3e_ext_str[4096] = "";

	for (uint32_t i = 0; i < *extension_count; i++ )
	{
		strcat(q3e_ext_str, extension_list[i]);
		strcat(q3e_ext_str, " ");
	}

	// Get the VRAPI required device extensions.
	// (VK_KHR_swapchain / VK_KHR_external / VK_KHR_external_memory_fd)
	char vrapi_deviceExtensionNames[4096];
	uint32_t VRAPI_deviceExtensionNamesSize = sizeof( vrapi_deviceExtensionNames );

	if ( vrapi_GetDeviceExtensionsVulkan(vrapi_deviceExtensionNames, &VRAPI_deviceExtensionNamesSize ) )
	{
		ri.Printf( PRINT_ERROR, "vrapi_GetDeviceExtensionsVulkan FAILED\n" );
		vrapi_Shutdown();
		exit( 0 );
	}

	// Add the VRAPI extensions To q3e extensions string
	strcat(q3e_ext_str, vrapi_deviceExtensionNames);

	// I don't understand why we need this extra step. A problem with '\0' ?
	char all_ext_str[4096];
	strncpy(all_ext_str, q3e_ext_str, 4096);

	ParseExtensionString(all_ext_str, extension_count, extension_list, 32 );

	//*extension_count = remove_dups( *extension_count, extension_list );
	*extension_count = remove_dups( extension_count, extension_list );
}

void VRAPI_createVulkan(void )
{
	ovrSystemCreateInfoVulkan systemInfo;
	systemInfo.Instance = vk.instance;
	systemInfo.Device = vk.device;
	systemInfo.PhysicalDevice = vk.physical_device; //vk.physicalDevice;
	int32_t initResult = vrapi_CreateSystemVulkan( &systemInfo );
	if ( initResult != ovrSuccess )
	{
		ri.Printf( PRINT_ERROR, "Failed to create VrApi Vulkan System\n" );
		vrapi_Shutdown();
		exit( 0 );
	}
}

void VRAPI_selectFormat( int bits ) {
	switch ( bits )
	{
		case 12:
			vk.base_format.format = VK_FORMAT_B4G4R4A4_UNORM_PACK16;
			vk.present_format.format = VK_FORMAT_R4G4B4A4_UNORM_PACK16;
			break;
		case 15:
			vk.base_format.format = VK_FORMAT_B5G5R5A1_UNORM_PACK16;
			vk.present_format.format = VK_FORMAT_R5G5B5A1_UNORM_PACK16;
			break;
		case 16:
			vk.base_format.format = VK_FORMAT_B5G6R5_UNORM_PACK16;
			vk.present_format.format = VK_FORMAT_R5G6B5_UNORM_PACK16;
			break;
		case 24:
			vk.base_format.format = VK_FORMAT_B8G8R8A8_UNORM;
			vk.present_format.format = VK_FORMAT_R8G8B8A8_UNORM;
			break;
		case 30:
			vk.base_format.format = VK_FORMAT_A2B10G10R10_UNORM_PACK32;
			vk.present_format.format = VK_FORMAT_A2R10G10B10_UNORM_PACK32;
			break;
		case 32:
			vk.base_format.format = VK_FORMAT_B10G11R11_UFLOAT_PACK32;
			vk.present_format.format = VK_FORMAT_B10G11R11_UFLOAT_PACK32;
			break;
		default:
			vk.base_format.format = VK_FORMAT_B8G8R8A8_UNORM;
			vk.present_format.format = VK_FORMAT_R8G8B8A8_UNORM;
			break;
	};

	vk.base_format.colorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
	vk.present_format.colorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;

	/*ri.Printf( PRINT_ALL, "===========================================");
	ri.Printf( PRINT_ALL, "vk.base_format.format : %s", vk_format_string(vk.base_format.format));
	ri.Printf( PRINT_ALL, "vk.base_format.colorSpace : %i", vk.base_format.colorSpace);
	ri.Printf( PRINT_ALL, "vk.present_format.format : %s", vk_format_string(vk.present_format.format));
	ri.Printf( PRINT_ALL, "vk.present_format.colorSpace : %i", vk.present_format.colorSpace);
	ri.Printf( PRINT_ALL, "===========================================");*/
}
