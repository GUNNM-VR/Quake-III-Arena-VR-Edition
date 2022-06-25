/*
** android_qvk.c
**
** This file implements the operating system binding of VK to QVK function
** pointers.  When doing a port of Quake2 you must implement the following
** two functions:
**
** QVK_Init() - loads libraries, assigns function pointers, etc.
** QVK_Shutdown() - unloads libraries, NULLs function pointers
*/

#include <unistd.h>
#include <sys/types.h>
#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"
#include "../renderercommon/tr_types.h"

#if defined( __ANDROID__ )
#include "../android/android_local.h" // for AppState
#endif

#include <dlfcn.h> // for dlclose

#define VK_USE_PLATFORM_ANDROID_KHR
#include "../renderercommon/vulkan/vulkan.h"


static PFN_vkGetInstanceProcAddr qvkGetInstanceProcAddr;
static PFN_vkCreateAndroidSurfaceKHR qvkCreateAndroidSurfaceKHR;


void *VK_GetInstanceProcAddr( VkInstance instance, const char *name )
{
	return qvkGetInstanceProcAddr( instance, name );
}


static void *load_vulkan_library( const char *dllname )
{
	void *lib;

	lib = Sys_LoadLibrary( dllname );
	if ( lib )
	{
		qvkGetInstanceProcAddr = (PFN_vkGetInstanceProcAddr) Sys_LoadFunction( lib, "vkGetInstanceProcAddr" );
		if ( qvkGetInstanceProcAddr )
		{
			return lib;
		}
		Sys_UnloadLibrary( lib );
	}

	return NULL;
}


/*
** QVK_Init
**
*/
qboolean QVK_Init( void )
{
	Com_Printf( "...initializing Android Vulkan API\n" );
	if (AppState.VulkanLib == NULL )
	{
		const char *dllnames[] = { "libvulkan.so.1", "libvulkan.so" };
		int i;
		for ( i = 0; i < ARRAY_LEN( dllnames ); i++ ) {
			AppState.VulkanLib = load_vulkan_library(dllnames[i] );
			Com_Printf("...loading '%s' : %s\n", dllnames[i], AppState.VulkanLib ? "succesed" : "failed" );
			if ( AppState.VulkanLib ) {
				break;
			}
		}

		if ( !AppState.VulkanLib ) {
			return qfalse;
		}
	}
	Sys_LoadFunctionErrors(); // reset error counter
	return qtrue;
}


/*
** QVK_Shutdown
**
** Unloads the specified DLL then nulls out all the proc pointers.
*/
void QVK_Shutdown( qboolean unloadDLL )
{
	Com_Printf( "...shutting down Android Vulkan API\n" );

	if (AppState.VulkanLib && unloadDLL )
	{
		Com_Printf( "...unloading Vulkan DLL\n" );
		dlclose(AppState.VulkanLib );
		AppState.VulkanLib = NULL;

		qvkGetInstanceProcAddr = NULL;
	}

	qvkCreateAndroidSurfaceKHR = NULL;
}


qboolean VK_CreateSurface( VkInstance instance, VkSurfaceKHR *surface )
{
	qvkCreateAndroidSurfaceKHR = (PFN_vkCreateAndroidSurfaceKHR) VK_GetInstanceProcAddr( instance, "vkCreateAndroidSurfaceKHR" );

	if ( !qvkCreateAndroidSurfaceKHR )
		return qfalse;

	VkAndroidSurfaceCreateInfoKHR desc;
	desc.sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
	desc.pNext = NULL;
	desc.flags = 0;
	desc.window = AppState.NativeWindow;

	return ( qvkCreateAndroidSurfaceKHR( instance, &desc, NULL, surface ) == VK_SUCCESS );
}
