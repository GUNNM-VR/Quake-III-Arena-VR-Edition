#ifndef __VRAPI_IMP__
#define __VRAPI_IMP__

#include "VrApi.h"
#include "VrApi_Helpers.h"
#include "VrApi_Vulkan.h"
#include "../../renderercommon/vulkan/vulkan.h"

#if defined(__ANDROID__)
#include "../../android/android_local.h" // for qboolean & AppState
#endif

/*
================================================================================
ovrColorSwapChain
================================================================================
*/
typedef struct
{
    int						SwapChainLength;
    ovrTextureSwapChain *	SwapChain;
    VkImage *               ColorTextures;

    // These two fragment density pointers are null if not supported
    VkImage *               FragmentDensityTextures;
    VkExtent2D *            FragmentDensityTextureSizes;
} ovrColorSwapChain;

ovrColorSwapChain colorSwapChains[2];

ovrTracking2 predictedTracking;

void VRAPI_Init(const ovrJava * java );

void VRAPI_SwapChainInit(void );

qboolean VRAPI_getInstanceExtensions(const char *ext );

void VRAPI_selectFormat(int bits );

void VRAPI_addDeviceExtensions(const char **extension_list, uint32_t *extension_count );

void VRAPI_createVulkan(void );

void VRAPI_SubmitFrame_layers(void );

void VRAPI_IN_Frame(void);

void VRAPI_Get_HMD_Info( void );
#endif

