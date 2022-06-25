/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
/*
** LINUX_QVK.C
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
#include "unix_glw.h"
#include <dlfcn.h>


#define VK_USE_PLATFORM_XLIB_KHR
//#define VK_USE_PLATFORM_XLIB_XRANDR_EXT
#include "../renderercommon/vulkan/vulkan.h"

static PFN_vkGetInstanceProcAddr qvkGetInstanceProcAddr;
static PFN_vkCreateXlibSurfaceKHR qvkCreateXlibSurfaceKHR;
// Xlib + XRandR
//static PFN_vkAcquireXlibDisplayEXT vkAcquireXlibDisplayEXT;
//static PFN_vkGetRandROutputDisplayEXT PFN_vkGetRandROutputDisplayEXT;

/*
** QVK_Shutdown
**
** Unloads the specified DLL then nulls out all the proc pointers.
*/
void QVK_Shutdown( qboolean unloadDLL )
{
	Com_Printf( "...shutting down QVK\n" );

	if ( glw_state.VulkanLib && unloadDLL )
	{
		Com_Printf( "...unloading Vulkan DLL\n" );
		dlclose( glw_state.VulkanLib );
		glw_state.VulkanLib = NULL;

		qvkGetInstanceProcAddr = NULL;
	}

	qvkCreateXlibSurfaceKHR = NULL;
}


void *VK_GetInstanceProcAddr( VkInstance instance, const char *name )
{
	return qvkGetInstanceProcAddr( instance, name );
}


qboolean VK_CreateSurface( VkInstance instance, VkSurfaceKHR *surface )
{
	VkXlibSurfaceCreateInfoKHR desc;

	qvkCreateXlibSurfaceKHR = (PFN_vkCreateXlibSurfaceKHR) VK_GetInstanceProcAddr( instance, "vkCreateXlibSurfaceKHR" );
	if ( !qvkCreateXlibSurfaceKHR )
		return qfalse;

	desc.sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR;
	desc.pNext = NULL;
	desc.flags = 0;
	desc.dpy = dpy;
	desc.window = win;

	if ( qvkCreateXlibSurfaceKHR( instance, &desc, NULL, surface ) == VK_SUCCESS )
		return qtrue;
	else
		return qfalse;
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

	Com_Printf( "...initializing QVK\n" );

	if ( glw_state.VulkanLib == NULL )
	{
		const char *dllnames[] = { "libvulkan.so.1", "libvulkan.so" };
		int i;

		for ( i = 0; i < ARRAY_LEN( dllnames ); i++ )
		{
			glw_state.VulkanLib = load_vulkan_library( dllnames[i] );

			Com_Printf( "...loading '%s' : %s\n", dllnames[i], glw_state.VulkanLib ? "succesed" : "failed" );
			if ( glw_state.VulkanLib )
			{
				break;
			}
		}

		if ( !glw_state.VulkanLib )
		{
			return qfalse;
		}
	}

	Sys_LoadFunctionErrors(); // reset error counter

	return qtrue;
}
