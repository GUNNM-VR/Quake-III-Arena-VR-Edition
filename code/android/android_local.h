#ifndef __ANDROID_LOCAL_H__
#define __ANDROID_LOCAL_H__

#include "../qcommon/q_shared.h" // needed for qboolean

#if !( defined __ANDROID__ )
#error You should include this file only on Android platforms
#endif

#include <android_native_app_glue.h>
#include <android/native_window.h>

#ifdef USE_VRAPI
#include <VrApi.h>
//#include <VrApi_Ext.h>
#endif

#ifndef EXPORT_API
#define EXPORT_API __attribute__ ((visibility("default")))
#endif

//#ifdef __ANDROID_LOG__
#include <android/log.h>

#if !defined( ALOGD )
#define ALOGD(...) __android_log_print( ANDROID_LOG_DEBUG, "Info", __VA_ARGS__)
#endif

#if !defined( ALOGE )
#define ALOGE(...) __android_log_print( ANDROID_LOG_ERROR, "Error", __VA_ARGS__ )
#endif

#if !defined( ALOGV )
#define ALOGV(...) __android_log_print( ANDROID_LOG_VERBOSE, "Verbose", __VA_ARGS__ )
#endif

#if !defined( ALOGW )
#define ALOGW(...) __android_log_print( ANDROID_LOG_WARN, "Warn", __VA_ARGS__ )
#endif
//#endif

/*
================================================================================
androidApp_struct
================================================================================
*/
typedef struct
{
#ifdef USE_VRAPI
	ovrJava				Java;
	ovrMobile *			Ovr;
	int					MainThreadTid;
	int					RenderThreadTid;
	long long			FrameIndex;
#endif
	struct android_app* NativeApp;
	ANativeWindow *		NativeWindow;
	int 				nativeWindow_Width;
	int 				nativeWindow_Height;
	void * 				VulkanLib;
	qboolean 		    Resumed;
	qboolean 		    Focused;
	double 				DisplayTime;
	int					SwapInterval;
	qboolean            BackButtonDownLastFrame;
} androidApp_struct;

androidApp_struct 		AppState;

#endif // __ANDROID_LOCAL_H__