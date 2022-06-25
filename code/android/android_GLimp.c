/*
** android_GLimp.c
**
** GLimp_EndFrame
** GLimp_Init
** GLimp_Shutdown
** GLimp_SetGamma
**
*/

#include "../client/client.h"
#include "../unix/linux_local.h"

#if defined(__ANDROID__)
#include "../android/android_local.h" // needed for AppState
#endif

typedef enum
{
    RSERR_OK,

    RSERR_INVALID_FULLSCREEN,
    RSERR_INVALID_MODE,
    RSERR_FATAL_ERROR,

    RSERR_UNKNOWN
} rserr_t;


#ifdef USE_JOYSTICK
cvar_t   *in_joystick      = NULL;
cvar_t   *in_joystickDebug = NULL;
cvar_t   *joy_threshold    = NULL;
#endif

/*
**
** GL mandatory stuffs
**
*/
void *GL_GetProcAddress( const char *symbol )
{
    return NULL;
}


void GLimp_SetGamma( unsigned char red[256], unsigned char green[256], unsigned char blue[256] )
{
}


void GLimp_Shutdown( qboolean unloadDLL )
{
}


void GLimp_Init( glconfig_t *config )
{
}


void GLimp_InitGamma( glconfig_t *config )
{
}

/*
** GLimp_EndFrame
**
** Responsible for doing a swapbuffers and possibly for other stuff
** as yet to be determined.  Probably better not to make this a GLimp
** function and instead do a call to GLimp_SwapBuffers.
*/
void GLimp_EndFrame( void )
{
    // TODO: implement
    //
    // swapinterval stuff
    //
    if ( r_swapInterval->modified ) {
        r_swapInterval->modified = qfalse;

        /*if ( qglXSwapIntervalEXT ) {
          qglXSwapIntervalEXT( dpy, win, r_swapInterval->integer );
        } else if ( qglXSwapIntervalMESA ) {
          qglXSwapIntervalMESA( r_swapInterval->integer );
        } else if ( qglXSwapIntervalSGI ) {
          qglXSwapIntervalSGI( r_swapInterval->integer );
        }*/
    }

    // don't flip if drawing to front buffer
    /*if ( Q_stricmp( cl_drawBuffer->string, "GL_FRONT" ) != 0 )
    {
      qglXSwapBuffers( dpy, win );
    }*/
}

void IN_Restart_f( void );

void IN_Init( void )
{
    Com_DPrintf( "\n------- Input Initialization -------\n" );

    // mouse variables
    //in_mouse = Cvar_Get( "in_mouse", "1", CVAR_ARCHIVE );

#ifdef USE_JOYSTICK
    // bk001130 - from cvs.17 (mkv), joystick variables
  in_joystick = Cvar_Get( "in_joystick", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
  // bk001130 - changed this to match win32
  in_joystickDebug = Cvar_Get( "in_debugjoystick", "0", CVAR_TEMP );
  joy_threshold = Cvar_Get( "joy_threshold", "0.15", CVAR_ARCHIVE_ND ); // FIXME: in_joythreshold
#endif

    /*if ( in_mouse->integer )
    {
      mouse_avail = qtrue;
    }
    else
    {
      mouse_avail = qfalse;
    }*/

#ifdef USE_JOYSTICK
    IN_StartupJoystick(); // bk001130 - from cvs1.17 (mkv)
#endif

    //Cmd_AddCommand( "minimize", IN_Minimize );
    Cmd_AddCommand( "in_restart", IN_Restart_f );

    Com_DPrintf( "------------------------------------\n" );
}


void IN_Shutdown( void )
{
    //mouse_avail = qfalse;
//  Cmd_RemoveCommand( "minimize" );
    Cmd_RemoveCommand( "in_restart" );
}


/*
=================
IM_Restart

Restart the input subsystem
=================
*/
void IN_Restart_f( void )
{
    IN_Shutdown();
    IN_Init();
}


void IN_Frame( void )
{
#ifdef USE_JOYSTICK
    IN_JoyMove();
#endif
}

void HandleEvents( void )
{
    // TODO: implement
}

/*
=================
Sys_GetClipboardData
=================
*/
char *Sys_GetClipboardData( void )
{
    // TODO: implement
    return NULL;
}


/*
=================
Sys_SetClipboardBitmap
=================
*/
void Sys_SetClipboardBitmap( const byte *bitmap, int length )
{
    // TODO: implement
}


#ifdef USE_VULKAN_API
/*
** VKimp_Shutdown
*/
void VKimp_Shutdown( qboolean unloadDLL )
{
    // TODO if need:
    // deactivate control

    IN_Shutdown();

    // restore gamma
    // restore vidmode
    // close display

    /*if ( unloadDLL )
    {
      RandR_Done();
      VidMode_Done();
    }*/

    unsetenv( "vblank_mode" );

    QVK_Shutdown( unloadDLL );
}


/*
** LoadVulkan
*/
static qboolean LoadVulkan( void )
{
    if ( r_swapInterval->integer )
        setenv( "vblank_mode", "2", 1 ); // TODO
    else
        setenv( "vblank_mode", "1", 1 );

    return QVK_Init();
}


static qboolean StartVulkan( void )
{
    //
    // load and initialize the specific Vulkan driver
    //
    if ( !LoadVulkan() )
    {
        Com_Error( ERR_FATAL, "StartVulkan() - could not load Vulkan subsystem\n" );
        return qfalse;
    }

    return qtrue;
}

/*
** VKimp_Init
**
** This routine is responsible for initializing the OS specific portions
** of Vulkan.
*/
cvar_t *r_stereoEnabled;
cvar_t *r_multiviewEnabled;
//cvar_t *r_anaglyphMode;
void VKimp_Init( glconfig_t *config )
{
    int colorbits;
    int depthbits;
    int stencilbits;

    if ( r_colorbits->integer <= 0 )
        colorbits = 24;
    else
        colorbits = MIN( r_colorbits->integer, 24);

    if ( cl_depthbits->integer == 0 )
        depthbits = 24;
    else
        depthbits = MIN( cl_depthbits->integer, 32);

    stencilbits = cl_stencilbits->integer;

    config->colorBits = colorbits;
    config->depthBits = depthbits;
    config->stencilBits = stencilbits;

    // /!\ important otherwise error "native_window_api_connect() failed: Invalid argument (-22)"
    config->vidWidth = AppState.nativeWindow_Width;
    config->vidHeight = AppState.nativeWindow_Height;

    r_stereoEnabled = Cvar_Get( "r_stereoEnabled", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
    if ( r_stereoEnabled->integer )
    {
        config->stereoEnabled = qtrue;
    }
#ifdef USE_MULTIVIEW
	//int in_anaglyphMode = Cvar_VariableIntegerValue("r_anaglyphMode");
    r_multiviewEnabled = Cvar_Get( "r_multiviewEnabled", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
    if ( r_multiviewEnabled->integer ) {
        isMultiview = true;
    }
#endif
//---------------------------

    InitSig();

    // load and initialize the specific Vulkan driver
    if ( !StartVulkan() )
    {
        return;
    }

    IN_Init();

    // This values force the UI to disable driver selection
    config->driverType = GLDRV_ICD;
    config->hardwareType = GLHW_GENERIC;
}
#endif // USE_VULKAN_API
