#include "../client/client.h"
#include "VRMOD_VMainMenu.h"
#include "VRMOD_input.h"
#include "VRMOD_common.h"

qhandle_t 	laserBeamShader;
static int	dp_realtime;

#define UI_TIMER_GESTURE		2300
#define UI_TIMER_JUMP			1000
#define UI_TIMER_LAND			130
#define UI_TIMER_WEAPON_SWITCH	300
#define UI_TIMER_ATTACK			500
#define	UI_TIMER_MUZZLE_FLASH	20
#define	UI_TIMER_WEAPON_DELAY	250

#define SPIN_SPEED				0.9f
#define	DEFAULT_VIEWHEIGHT		26
#define MAIN_BANNER_MODEL		"models/mapobjects/banner/banner5.md3"
#define cg_swingSpeed 			0.3f //from cg cvar

static playersettings_t	s_playersettings;

static int	FloatAsInt( float f ) {
	floatint_t fi;
	fi.f = f;
	return fi.i;
}

static float spawnYaw = 270.0f;

/*
======================
UI_MovedirAdjustment
======================
*/
static float UI_MovedirAdjustment( playerInfo_t *pi ) {
	vec3_t		relativeAngles;
	vec3_t		moveVector;

	VectorSubtract( pi->viewAngles, pi->moveAngles, relativeAngles );
	AngleVectors( relativeAngles, moveVector, NULL, NULL );
	if ( Q_fabs( moveVector[0] ) < 0.01 ) {
		moveVector[0] = 0.0;
	}
	if ( Q_fabs( moveVector[1] ) < 0.01 ) {
		moveVector[1] = 0.0;
	}

	if ( moveVector[1] == 0 && moveVector[0] > 0 ) {
		return 0;
	}
	if ( moveVector[1] < 0 && moveVector[0] > 0 ) {
		return 22;
	}
	if ( moveVector[1] < 0 && moveVector[0] == 0 ) {
		return 45;
	}
	if ( moveVector[1] < 0 && moveVector[0] < 0 ) {
		return -22;
	}
	if ( moveVector[1] == 0 && moveVector[0] < 0 ) {
		return 0;
	}
	if ( moveVector[1] > 0 && moveVector[0] < 0 ) {
		return 22;
	}
	if ( moveVector[1] > 0 && moveVector[0] == 0 ) {
		return  -45;
	}

	return -22;
}

/*
==================
CG_SwingAngles
==================
*/
static void CG_SwingAngles( float destination, float swingTolerance, float clampTolerance, float speed, float *angle, qboolean *swinging ) { // todo virer si pas utilisé
	float	swing;
	float	move;
	float	scale;

	if ( !*swinging ) {
		// see if a swing should be started
		swing = AngleSubtract( *angle, destination );
		if ( swing > swingTolerance || swing < -swingTolerance ) {
			*swinging = qtrue;
		}
	}

	if ( !*swinging ) {
		return;
	}
	
	// modify the speed depending on the delta
	// so it doesn't seem so linear
	swing = AngleSubtract( destination, *angle );
	scale = fabs( swing );
	if ( scale < swingTolerance * 0.5 ) {
		scale = 0.5;
	} else if ( scale < swingTolerance ) {
		scale = 1.0;
	} else {
		scale = 2.0;
	}

	// swing towards the destination angle
	if ( swing >= 0 ) {
		move = cls.realtime * scale * speed;
		if ( move >= swing ) {
			move = swing;
			*swinging = qfalse;
		}
		*angle = AngleMod( *angle + move );
	} else if ( swing < 0 ) {
		move = cls.realtime * scale * -speed;
		if ( move <= swing ) {
			move = swing;
			*swinging = qfalse;
		}
		*angle = AngleMod( *angle + move );
	}

	// clamp to no more than tolerance
	swing = AngleSubtract( destination, *angle );
	if ( swing > clampTolerance ) {
		*angle = AngleMod( destination - (clampTolerance - 1) );
	} else if ( swing < -clampTolerance ) {
		*angle = AngleMod( destination + (clampTolerance - 1) );
	}
}

/*
===============
CG_PlayerAngles

Handles separate torso motion

  legs pivot based on direction of movement

  head always looks exactly at cl.viewAngles

  if motion < 20 degrees, show in head only
  if < 45 degrees, also show in torso
===============
*/
static void CL_PlayerAngles( playerInfo_t *pi, vec3_t legs[3], vec3_t torso[3], vec3_t head[3], vec3_t gun[3]  ) {
	vec3_t		legsAngles, torsoAngles, headAngles, weaponAngles;
	float		dest;

#ifdef USE_VR_QVM
	VectorCopy( cl.viewangles, headAngles );
#endif
#ifdef USE_NATIVE_HACK
	VectorCopy(vr_info.hmdinfo.angles.actual , headAngles);
#endif

	headAngles[YAW] = AngleMod( spawnYaw + headAngles[YAW] );

#ifdef USE_VR
	if ( vr_controller_type->integer == 1 ) {
		//===================
		//		3 Dof
		//===================
		// vr controller control torso yaw & pich
		torsoAngles[YAW]	= AngleMod( vr_info.controllers[SideRIGHT].angles.actual[YAW] + spawnYaw );
		torsoAngles[PITCH]	= vr_info.controllers[SideRIGHT].angles.actual[PITCH];
		torsoAngles[ROLL]	= 0.0f;

		// vr controller roll only on the weapon
		weaponAngles[YAW]	= 0.0f;
		weaponAngles[PITCH]	= 0.0f;
		weaponAngles[ROLL]	= vr_info.controllers[SideRIGHT].angles.actual[ROLL];
	}
	else if ( vr_controller_type->integer >= 2 ) {
		//===================
		//		6 Dof
		//===================
		// vr controller control torso yaw & pich
		torsoAngles[YAW] = AngleMod( vr_info.rightAngles[YAW] + spawnYaw );
		torsoAngles[PITCH] = vr_info.rightAngles[PITCH];
		torsoAngles[ROLL] = 0.0f;

		// vr controller roll only on the weapon
		weaponAngles[YAW] = 0.0f;
		weaponAngles[PITCH] = 0.0f;
		weaponAngles[ROLL] = vr_info.rightAngles[ROLL];
	}
	else 
#endif
	{
		//===================
		//		2 Dof
		//===================
		VectorClear(torsoAngles);
		VectorClear(weaponAngles);
	}

	// ==============================
	//              yaw
	// ==============================

	// allow yaw to drift a bit
	if 	( ( pi->legsAnim  & ~ANIM_TOGGLEBIT ) != LEGS_IDLE || 
		( ( pi->torsoAnim & ~ANIM_TOGGLEBIT ) != TORSO_STAND && 
		  ( pi->torsoAnim & ~ANIM_TOGGLEBIT ) != TORSO_STAND2 ) )
	{
		// if not standing still, always point all in the same direction
		pi->torso.yawing 	= qtrue;	// always center
		pi->torso.pitching 	= qtrue;	// always center
		pi->legs.yawing 	= qtrue;	// always center
	}

	// adjust legs for movement dir
	VectorClear(legsAngles);
	legsAngles[YAW] = headAngles[YAW] + UI_MovedirAdjustment( pi );
	CG_SwingAngles( legsAngles[YAW], 40, 90, cg_swingSpeed, &pi->legs.yawAngle, &pi->legs.yawing );
	legsAngles[YAW] = pi->legs.yawAngle;

	// torso
	if( vr_controller_type->integer == 0 )
	{
		torsoAngles[YAW] = headAngles[YAW] + 0.25 * UI_MovedirAdjustment( pi );
		CG_SwingAngles( torsoAngles[YAW], 25, 90, cg_swingSpeed, &pi->torso.yawAngle, &pi->torso.yawing );
		torsoAngles[YAW] = pi->torso.yawAngle;
	}

	// ==============================
	//             pitch
	// ==============================
	if( vr_controller_type->integer == 0 ) // if bot or VR player
	{
		// only show a fraction of the pitch angle in the torso
		if ( headAngles[PITCH] > 180 ) {
			dest = (-360 + headAngles[PITCH]) * 0.75f;
		} else {
			dest = headAngles[PITCH] * 0.75f;
		}
		CG_SwingAngles( dest, 15, 30, 0.1f, &pi->torso.pitchAngle, &pi->torso.pitching );
		torsoAngles[PITCH] = pi->torso.pitchAngle;
	}

	// for fixedtorso models
	if ( pi->fixedtorso ) {
		torsoAngles[PITCH] = 0.0f;
	}

	// ==============================
	//             roll
	// ==============================
	// lean towards the direction of travel
	/*
	VectorCopy( cent->currentState.pos.trDelta, velocity );
	speed = VectorNormalize( velocity );
	if ( speed ) {
		vec3_t	axis[3];
		float	side;

		speed *= 0.05f;

		AnglesToAxis( legsAngles, axis );
		side = speed * DotProduct( velocity, axis[1] );
		legsAngles[ROLL] -= side;

		side = speed * DotProduct( velocity, axis[0] );
		legsAngles[PITCH] += side;
	}*/

	//
	if ( pi->fixedlegs ) {
		legsAngles[YAW] = torsoAngles[YAW];
		legsAngles[PITCH] = 0.0f;
		legsAngles[ROLL] = 0.0f;
	}

	// ============================
	// pull the orientation back out of the hierarchial chain
	// ============================
	AnglesSubtract( headAngles, torsoAngles, headAngles );
	AnglesSubtract( torsoAngles, legsAngles, torsoAngles );

	AnglesToAxis( legsAngles, legs );
	AnglesToAxis( torsoAngles, torso );
	AnglesToAxis( headAngles, head );

	if ( vr_controller_type->integer == 0 ) {
		AnglesToAxis( torsoAngles, gun );
	}
	else {
		AnglesToAxis( weaponAngles, gun );
	}
}

/*
==========================
CL_RegisterClientSkin
==========================
*/
static qboolean CL_RegisterClientSkin( playerInfo_t *pi, const char *modelName, const char *skinName ) {
	char		filename[MAX_QPATH];

	Com_sprintf( filename, sizeof( filename ), "models/players/%s/lower_%s.skin", modelName, skinName );
	pi->legsSkin = re.RegisterSkin( filename );

	Com_sprintf( filename, sizeof( filename ), "models/players/%s/upper_%s.skin", modelName, skinName );
	pi->torsoSkin = re.RegisterSkin( filename );

	Com_sprintf( filename, sizeof( filename ), "models/players/%s/head_%s.skin", modelName, skinName );
	pi->headSkin = re.RegisterSkin( filename );

	if ( !pi->legsSkin || !pi->torsoSkin || !pi->headSkin ) {
		return qfalse;
	}

	return qtrue;
}

/*
======================
UI_ParseAnimationFile
======================
*/
static qboolean CL_ParseAnimationFile( const char *filename, playerInfo_t *pi ) {
	const char	*text_p, *prev;
	int			len;
	int			i;
	const char	*token;
	float		fps;
	int			skip;
	char		text[20000];
	fileHandle_t	f;
	animation_t *animations;

	animations = pi->animations;

	memset( animations, 0, sizeof( animation_t ) * MAX_ANIMATIONS );

	pi->fixedlegs = qfalse;
	pi->fixedtorso = qfalse;

	// load the file
	//len = trap_FS_FOpenFile( filename, &f, FS_READ );
	len = FS_FOpenFileByMode( filename, &f, FS_READ );
	if ( len <= 0 ) {
		return qfalse;
	}
	if ( len >= ( sizeof( text ) - 1 ) ) {
		Com_Printf( "File %s too long\n", filename );
		//trap_FS_FCloseFile( f );
		FS_FCloseFile( f );
		return qfalse;
	}
	//trap_FS_Read( text, len, f );
	FS_Read( text, len, f );
	text[len] = 0;
	//trap_FS_FCloseFile( f );
	FS_FCloseFile( f );

	// parse the text
	text_p = text;
	skip = 0;	// quite the compiler warning

	// read optional parameters
	while ( 1 ) {
		prev = text_p;	// so we can unget
		token = COM_Parse( &text_p );
		if ( !token[0] ) {
			break;
		}
		if ( !Q_stricmp( token, "footsteps" ) ) {
			token = COM_Parse( &text_p );
			if ( !token[0] ) {
				break;
			}
			continue;
		} else if ( !Q_stricmp( token, "headoffset" ) ) {
			for ( i = 0 ; i < 3 ; i++ ) {
				token = COM_Parse( &text_p );
				if ( !token[0] ) {
					break;
				}
			}
			continue;
		} else if ( !Q_stricmp( token, "sex" ) ) {
			token = COM_Parse( &text_p );
			if ( !token[0] ) {
				break;
			}
			continue;
		} else if ( !Q_stricmp( token, "fixedlegs" ) ) {
			pi->fixedlegs = qtrue;
			continue;
		} else if ( !Q_stricmp( token, "fixedtorso" ) ) {
			pi->fixedtorso = qtrue;
			continue;
		}

		// if it is a number, start parsing animations
		if ( token[0] >= '0' && token[0] <= '9' ) {
			text_p = prev;	// unget the token
			break;
		}

		Com_Printf( "unknown token '%s' in %s\n", token, filename );
	}

	// read information for each frame
	for ( i = 0 ; i < MAX_ANIMATIONS ; i++ ) {

		token = COM_Parse( &text_p );
		if ( !token[0] ) {
			if( i >= TORSO_GETFLAG && i <= TORSO_NEGATIVE ) {
				animations[i].firstFrame = animations[TORSO_GESTURE].firstFrame;
				animations[i].frameLerp = animations[TORSO_GESTURE].frameLerp;
				animations[i].initialLerp = animations[TORSO_GESTURE].initialLerp;
				animations[i].loopFrames = animations[TORSO_GESTURE].loopFrames;
				animations[i].numFrames = animations[TORSO_GESTURE].numFrames;
				animations[i].reversed = qfalse;
				animations[i].flipflop = qfalse;
				continue;
			}
			break;
		}
		animations[i].firstFrame = atoi( token );
		// leg only frames are adjusted to not count the upper body only frames
		if ( i == LEGS_WALKCR ) {
			skip = animations[LEGS_WALKCR].firstFrame - animations[TORSO_GESTURE].firstFrame;
		}
		if ( i >= LEGS_WALKCR && i<TORSO_GETFLAG) {
			animations[i].firstFrame -= skip;
		}

		token = COM_Parse( &text_p );
		if ( !token[0] ) {
			break;
		}
		animations[i].numFrames = atoi( token );

		animations[i].reversed = qfalse;
		animations[i].flipflop = qfalse;
		// if numFrames is negative the animation is reversed
		if (animations[i].numFrames < 0) {
			animations[i].numFrames = -animations[i].numFrames;
			animations[i].reversed = qtrue;
		}

		token = COM_Parse( &text_p );
		if ( !token[0] ) {
			break;
		}
		animations[i].loopFrames = atoi( token );

		token = COM_Parse( &text_p );
		if ( !token[0] ) {
			break;
		}
		fps = atof( token );
		if ( fps == 0 ) {
			fps = 1;
		}
		animations[i].frameLerp = 1000 / fps;
		animations[i].initialLerp = 1000 / fps;
	}

	if ( i != MAX_ANIMATIONS ) {
		Com_Printf( "Error parsing animation file: %s\n", filename );
		return qfalse;
	}
	return qtrue;
}

/*
==========================
UI_RegisterClientModelname
==========================
*/
qboolean CL_RegisterClientModelname( playerInfo_t *pi, const char *modelSkinName ) {
	char		modelName[MAX_QPATH];
	char		skinName[MAX_QPATH];
	char		filename[MAX_QPATH];
	char		*slash;

	pi->torsoModel = 0;
	pi->headModel = 0;

	if ( !modelSkinName[0] ) {
		return qfalse;
	}

	Q_strncpyz( modelName, modelSkinName, sizeof( modelName ) );

	slash = strchr( modelName, '/' );
	if ( !slash ) {
		// modelName did not include a skin name
		Q_strncpyz( skinName, "default", sizeof( skinName ) );
	} else {
		Q_strncpyz( skinName, slash + 1, sizeof( skinName ) );
		// truncate modelName
		*slash = 0;
	}

	// load cmodels before models so filecache works

	Com_sprintf( filename, sizeof( filename ), "models/players/%s/lower.md3", modelName );
	pi->legsModel = re.RegisterModel( filename );

	if ( !pi->legsModel ) {
		Com_Printf( "Failed to load model file %s\n", filename );
		return qfalse;
	}

	Com_sprintf( filename, sizeof( filename ), "models/players/%s/upper.md3", modelName );
	pi->torsoModel = re.RegisterModel( filename );
	if ( !pi->torsoModel ) {
		Com_Printf( "Failed to load model file %s\n", filename );
		return qfalse;
	}

	Com_sprintf( filename, sizeof( filename ), "models/players/%s/head.md3", modelName );
	pi->headModel = re.RegisterModel( filename );
	if ( !pi->headModel ) {
		Com_Printf( "Failed to load model file %s\n", filename );
		return qfalse;
	}

	// if any skins failed to load, fall back to default
	if ( !CL_RegisterClientSkin( pi, modelName, skinName ) ) {
		if ( !CL_RegisterClientSkin( pi, modelName, "default" ) ) {
			Com_Printf( "Failed to load skin file: %s : %s\n", modelName, skinName );
			return qfalse;
		}
	}

	// load the animations
	Com_sprintf( filename, sizeof( filename ), "models/players/%s/animation.cfg", modelName );
	if ( !CL_ParseAnimationFile( filename, pi ) ) {
		Com_Printf( "Failed to load animation file %s\n", filename );
		return qfalse;
	}

	return qtrue;
}

/*
===============
UI_PlayerInfo_SetWeapon
anly load vr controller
===============
*/
static void UI_PlayerInfo_SetWeapon( playerInfo_t *pi, weapon_t weaponNum ) {
#ifdef USE_VR
	pi->currentWeapon = WP_VR_CONTROLLER;
	pi->realWeapon = WP_VR_CONTROLLER;
	pi->weaponModel = re.RegisterModel( "models/weapons2/oculusgo/oculusgo.md3" );
	pi->flashModel = re.RegisterModel( "models/weapons2/oculusgo/oculusgo_flash.md3" );
	pi->barrelModel = 0;
#endif
	return;
}

/*
===============
UI_PlayerInfo_SetModel
===============
*/
void UI_PlayerInfo_SetModel( playerInfo_t *pi, const char *model ) {
	memset( pi, 0, sizeof(*pi) );
	CL_RegisterClientModelname( pi, model );
	pi->weapon = WP_MACHINEGUN;
	pi->currentWeapon = pi->weapon;
	pi->lastWeapon = pi->weapon;
	pi->pendingWeapon = WP_NUM_WEAPONS;
	pi->weaponTimer = 0;
	pi->chat = qfalse;
	pi->newModel = qtrue;
	UI_PlayerInfo_SetWeapon( pi, pi->weapon );
}

/*
===============
UI_ForceLegsAnim
===============
*/
static void UI_ForceLegsAnim( playerInfo_t *pi, int anim ) {
	pi->legsAnim = ( ( pi->legsAnim & ANIM_TOGGLEBIT ) ^ ANIM_TOGGLEBIT ) | anim;

	if ( anim == LEGS_JUMP ) {
		pi->legsAnimationTimer = UI_TIMER_JUMP;
	}
}

/*
===============
UI_ForceTorsoAnim
===============
*/
static void UI_ForceTorsoAnim( playerInfo_t *pi, int anim ) {
	pi->torsoAnim = ( ( pi->torsoAnim & ANIM_TOGGLEBIT ) ^ ANIM_TOGGLEBIT ) | anim;

	if ( anim == TORSO_GESTURE ) {
		pi->torsoAnimationTimer = UI_TIMER_GESTURE;
	}

	if ( anim == TORSO_ATTACK || anim == TORSO_ATTACK2 ) {
		pi->torsoAnimationTimer = UI_TIMER_ATTACK;
	}
}

/*
===============
UI_PlayerInfo_SetInfo
===============
*/
#ifdef USE_VR
void UI_PlayerInfo_SetInfo( playerInfo_t *pi, int legsAnim, int torsoAnim, vec3_t viewAngles, vec3_t moveAngles, weapon_t weaponNumber, qboolean chat ) {
	int			currentAnim;
	weapon_t	weaponNum;
	int			c;

	pi->chat = chat;

	pi->laserBeamPlayer = Cvar_VariableValue( "laserBeamPlayer" );

 	c = (int)FloatAsInt( Cvar_VariableValue( "color1" ) );
	VectorClear( pi->color1 );

	if( c < 1 || c > 7 ) {//from cg_players.c/ CG_ColorFromRGBAString()
		VectorSet( pi->color1, 1, 1, 1 );
	}
	else {
		if( c & 1 ) {
			pi->color1[2] = 1.0f;
		}

		if( c & 2 ) {
			pi->color1[1] = 1.0f;
		}

		if( c & 4 ) {
			pi->color1[0] = 1.0f;
		}
	}

	pi->c1RGBA[0] = 255 * pi->color1[0];
	pi->c1RGBA[1] = 255 * pi->color1[1];
	pi->c1RGBA[2] = 255 * pi->color1[2];
	pi->c1RGBA[3] = 255;

	// view orientation
	VectorCopy( viewAngles, pi->viewAngles );

	// move orientation
	VectorCopy( moveAngles, pi->moveAngles );

	if ( pi->newModel ) {
		pi->newModel = qfalse;
		pi->pendingLegsAnim = 0;
		UI_ForceLegsAnim( pi, legsAnim );
		pi->legs.yawAngle = viewAngles[YAW];
		pi->legs.yawing = qfalse;

		pi->pendingTorsoAnim = 0;
		UI_ForceTorsoAnim( pi, torsoAnim );
		pi->torso.yawAngle = viewAngles[YAW];
		pi->torso.yawing = qfalse;

		if ( weaponNumber != WP_NUM_WEAPONS ) {
			pi->weapon = weaponNumber;
			pi->currentWeapon = weaponNumber;
			pi->lastWeapon = weaponNumber;
			pi->pendingWeapon = WP_NUM_WEAPONS;
			pi->weaponTimer = 0;
			UI_PlayerInfo_SetWeapon( pi, pi->weapon );
		}
		return;
	}

	// weapon
	if ( weaponNumber == WP_NUM_WEAPONS ) {
		pi->pendingWeapon = WP_NUM_WEAPONS;
		pi->weaponTimer = 0;
	}
	else if ( weaponNumber != WP_NONE ) {
		pi->pendingWeapon = weaponNumber;
		pi->weaponTimer = dp_realtime + UI_TIMER_WEAPON_DELAY;
	}
	weaponNum = pi->lastWeapon;
	pi->weapon = weaponNum;

	if ( torsoAnim == BOTH_DEATH1 || legsAnim == BOTH_DEATH1 ) {
		torsoAnim = legsAnim = BOTH_DEATH1;
		pi->weapon = pi->currentWeapon = WP_NONE;
		UI_PlayerInfo_SetWeapon( pi, pi->weapon );

		pi->pendingLegsAnim = 0;
		UI_ForceLegsAnim( pi, legsAnim );

		pi->pendingTorsoAnim = 0;
		UI_ForceTorsoAnim( pi, torsoAnim );

		return;
	}

	// leg animation
	currentAnim = pi->legsAnim & ~ANIM_TOGGLEBIT;
	if ( legsAnim != LEGS_JUMP && ( currentAnim == LEGS_JUMP || currentAnim == LEGS_LAND ) ) {
		pi->pendingLegsAnim = legsAnim;
	}
	else if ( legsAnim != currentAnim ) {
		pi->pendingLegsAnim = 0;
		UI_ForceLegsAnim( pi, legsAnim );
	}

	// torso animation
	if ( torsoAnim == TORSO_STAND || torsoAnim == TORSO_STAND2 ) {
		if ( weaponNum == WP_NONE || weaponNum == WP_GAUNTLET || weaponNum == WP_VR_CONTROLLER) {
			torsoAnim = TORSO_STAND2;
		}
		else {
			torsoAnim = TORSO_STAND;
		}
	}

	if ( torsoAnim == TORSO_ATTACK || torsoAnim == TORSO_ATTACK2 ) {
		if ( weaponNum == WP_NONE || weaponNum == WP_GAUNTLET || weaponNum == WP_VR_CONTROLLER) {
			torsoAnim = TORSO_ATTACK2;
		}
		else {
			torsoAnim = TORSO_ATTACK;
		}
		pi->muzzleFlashTime = dp_realtime + UI_TIMER_MUZZLE_FLASH;
		//TODO play firing sound here
	}

	currentAnim = pi->torsoAnim & ~ANIM_TOGGLEBIT;

	if ( weaponNum != pi->currentWeapon || currentAnim == TORSO_RAISE || currentAnim == TORSO_DROP ) {
		pi->pendingTorsoAnim = torsoAnim;
	}
	else if ( ( currentAnim == TORSO_GESTURE || currentAnim == TORSO_ATTACK ) && ( torsoAnim != currentAnim ) ) {
		pi->pendingTorsoAnim = torsoAnim;
	}
	else if ( torsoAnim != currentAnim ) {
		pi->pendingTorsoAnim = 0;
		UI_ForceTorsoAnim( pi, torsoAnim );
	}
}
#endif
/*
===============
UI_SetLegsAnim
===============
*/
static void UI_SetLegsAnim( playerInfo_t *pi, int anim ) {
	if ( pi->pendingLegsAnim ) {
		anim = pi->pendingLegsAnim;
		pi->pendingLegsAnim = 0;
	}
	UI_ForceLegsAnim( pi, anim );
}

/*
===============
UI_LegsSequencing
===============
*/
static void UI_LegsSequencing( playerInfo_t *pi ) {
	int		currentAnim;

	currentAnim = pi->legsAnim & ~ANIM_TOGGLEBIT;

	if ( pi->legsAnimationTimer > 0 ) {
		return;
	}

	if ( currentAnim == LEGS_JUMP ) {
		UI_ForceLegsAnim( pi, LEGS_LAND );
		pi->legsAnimationTimer = UI_TIMER_LAND;
		return;
	}

	if ( currentAnim == LEGS_LAND ) {
		UI_SetLegsAnim( pi, LEGS_IDLE );
		return;
	}
}

/*
===============
UI_SetTorsoAnim
===============
*/
static void UI_SetTorsoAnim( playerInfo_t *pi, int anim ) {
	if ( pi->pendingTorsoAnim ) {
		anim = pi->pendingTorsoAnim;
		pi->pendingTorsoAnim = 0;
	}
	UI_ForceTorsoAnim( pi, anim );
}

/*
===============
UI_TorsoSequencing
===============
*/
static void UI_TorsoSequencing( playerInfo_t *pi ) {
	int		currentAnim;

	currentAnim = pi->torsoAnim & ~ANIM_TOGGLEBIT;

	if ( pi->weapon != pi->currentWeapon ) {
		if ( currentAnim != TORSO_DROP ) {
			pi->torsoAnimationTimer = UI_TIMER_WEAPON_SWITCH;
			UI_ForceTorsoAnim( pi, TORSO_DROP );
		}
	}

	if ( pi->torsoAnimationTimer > 0 ) {
		return;
	}

	if( currentAnim == TORSO_GESTURE ) {
		UI_SetTorsoAnim( pi, TORSO_STAND );
		return;
	}

	if( currentAnim == TORSO_ATTACK || currentAnim == TORSO_ATTACK2 ) {
		UI_SetTorsoAnim( pi, TORSO_STAND );
		return;
	}

	if ( currentAnim == TORSO_DROP ) {
		//UI_PlayerInfo_SetWeapon( pi, pi->weapon );
		pi->torsoAnimationTimer = UI_TIMER_WEAPON_SWITCH;
		UI_ForceTorsoAnim( pi, TORSO_RAISE );
		return;
	}

	if ( currentAnim == TORSO_RAISE ) {
		UI_SetTorsoAnim( pi, TORSO_STAND );
		return;
	}
}

/*
===============
UI_SetLerpFrameAnimation
===============
*/
static void UI_SetLerpFrameAnimation( playerInfo_t *ci, lerpFrame_t *lf, int newAnimation ) {
	animation_t	*anim;

	lf->animationNumber = newAnimation;
	newAnimation &= ~ANIM_TOGGLEBIT;

	if ( newAnimation < 0 || newAnimation >= MAX_ANIMATIONS ) {
		Com_Error( ERR_DROP, "Bad animation number: %i\n", newAnimation );
	}

	anim = &ci->animations[ newAnimation ];

	lf->animation = anim;
	lf->animationTime = lf->frameTime + anim->initialLerp;
}

/*
===============
UI_RunLerpFrame
===============
*/
static void UI_RunLerpFrame( playerInfo_t *ci, lerpFrame_t *lf, int newAnimation ) {
	int			f, numFrames;
	animation_t	*anim;

	// see if the animation sequence is switching
	if ( newAnimation != lf->animationNumber || !lf->animation ) {
		UI_SetLerpFrameAnimation( ci, lf, newAnimation );
	}

	// if we have passed the current frame, move it to
	// oldFrame and calculate a new frame
	if ( dp_realtime >= lf->frameTime ) {
		lf->oldFrame = lf->frame;
		lf->oldFrameTime = lf->frameTime;

		// get the next frame based on the animation
		anim = lf->animation;
		if ( !anim->frameLerp ) {
			return;		// shouldn't happen
		}
		if ( dp_realtime < lf->animationTime ) {
			lf->frameTime = lf->animationTime;		// initial lerp
		} else {
			lf->frameTime = lf->oldFrameTime + anim->frameLerp;
		}
		f = ( lf->frameTime - lf->animationTime ) / anim->frameLerp;

		numFrames = anim->numFrames;
		if (anim->flipflop) {
			numFrames *= 2;
		}
		if ( f >= numFrames ) {
			f -= numFrames;
			if ( anim->loopFrames ) {
				f %= anim->loopFrames;
				f += anim->numFrames - anim->loopFrames;
			} else {
				f = numFrames - 1;
				// the animation is stuck at the end, so it
				// can immediately transition to another sequence
				lf->frameTime = dp_realtime;
			}
		}
		if ( anim->reversed ) {
			lf->frame = anim->firstFrame + anim->numFrames - 1 - f;
		}
		else if (anim->flipflop && f>=anim->numFrames) {
			lf->frame = anim->firstFrame + anim->numFrames - 1 - (f%anim->numFrames);
		}
		else {
			lf->frame = anim->firstFrame + f;
		}
		if ( dp_realtime > lf->frameTime ) {
			lf->frameTime = dp_realtime;
		}
	}

	if ( lf->frameTime > dp_realtime + 200 ) {
		lf->frameTime = dp_realtime;
	}

	if ( lf->oldFrameTime > dp_realtime ) {
		lf->oldFrameTime = dp_realtime;
	}
	// calculate current lerp value
	if ( lf->frameTime == lf->oldFrameTime ) {
		lf->backlerp = 0;
	} else {
		lf->backlerp = 1.0 - (float)( dp_realtime - lf->oldFrameTime ) / ( lf->frameTime - lf->oldFrameTime );
	}
}

/*
===============
UI_PlayerAnimation
===============
*/
static void UI_PlayerAnimation( playerInfo_t *pi, int *legsOld, int *legs, float *legsBackLerp, int *torsoOld, int *torso, float *torsoBackLerp ) {

	// ===================
	// legs animation
	// ===================
	pi->legsAnimationTimer -= cls.frametime;
	if ( pi->legsAnimationTimer < 0 ) {
		pi->legsAnimationTimer = 0;
	}

	UI_LegsSequencing( pi );

	/*if ( pi->legs.yawing && ( pi->legsAnim & ~ANIM_TOGGLEBIT ) == LEGS_IDLE ) {
		UI_RunLerpFrame( pi, &pi->legs, LEGS_TURN );
	} else {*/
		UI_RunLerpFrame( pi, &pi->legs, pi->legsAnim );
	//}
	*legsOld = pi->legs.oldFrame;
	*legs = pi->legs.frame;
	*legsBackLerp = pi->legs.backlerp;

	// ===================
	// torso animation
	// ===================
	pi->torsoAnimationTimer -= cls.frametime;
	if ( pi->torsoAnimationTimer < 0 ) {
		pi->torsoAnimationTimer = 0;
	}

	UI_TorsoSequencing( pi );

	UI_RunLerpFrame( pi, &pi->torso, pi->torsoAnim );
	*torsoOld = pi->torso.oldFrame;
	*torso = pi->torso.frame;
	*torsoBackLerp = pi->torso.backlerp;
}

/*
======================
UI_PositionEntityOnTag
======================
*/
static void UI_PositionEntityOnTag( refEntity_t *entity, const refEntity_t *parent, clipHandle_t parentModel, char *tagName ) {
	int				i;
	orientation_t	lerped;
	
	// lerp the tag
	re.LerpTag( &lerped, parentModel, parent->oldframe, parent->frame, 1.0 - parent->backlerp, tagName );

	// FIXME: allow origin offsets along tag?
	VectorCopy( parent->origin, entity->origin );
	for ( i = 0 ; i < 3 ; i++ ) {
		VectorMA( entity->origin, lerped.origin[i], parent->axis[i], entity->origin );
	}

	// cast away const because of compiler problems
	MatrixMultiply( lerped.axis, ((refEntity_t*)parent)->axis, entity->axis );
	entity->backlerp = parent->backlerp;
}

/*
======================
UI_PositionRotatedEntityOnTag
======================
*/
static void UI_PositionRotatedEntityOnTag( refEntity_t *entity, const refEntity_t *parent, clipHandle_t parentModel, char *tagName ) {
	int				i;
	orientation_t	lerped;
	vec3_t			tempAxis[3];

	// lerp the tag
	re.LerpTag( &lerped, parentModel, parent->oldframe, parent->frame, 1.0 - parent->backlerp, tagName );

	// FIXME: allow origin offsets along tag?
	VectorCopy( parent->origin, entity->origin );
	for ( i = 0 ; i < 3 ; i++ ) {
		VectorMA( entity->origin, lerped.origin[i], parent->axis[i], entity->origin );
	}

	// cast away const because of compiler problems
	MatrixMultiply( entity->axis, lerped.axis, tempAxis );
	MatrixMultiply( tempAxis, ((refEntity_t *)parent)->axis, entity->axis );
}

/*
===============
CG_PlayerShadow

===============
*/
/*static qboolean CG_PlayerShadow( centity_t *cent, float *shadowPlane ) {
	
}*/

// ===================================================================================
// = 								laser beam 										 =
// ===================================================================================
// laser beam origin: weapon tag_flash point
// laser beam axis: gun & vrcontroller orientation difference (static vec3_t gun_delta_axis)
// (in order to filter the beam movements, done in cg_weapon.c/CG_AddPlayerWeapon()
// ===================================================================================
#ifdef USE_LASER_SIGHT
void CL_DrawLaserBeam( vec3_t origin, vec3_t axis[3] )
{
	refEntity_t ent;
	float maxdist = 512.0f;

	memset(&ent, 0, sizeof(ent));

	ent.reType = RT_LASERSIGHT;
	ent.customShader = laserBeamShader;

	maxdist = (vr_info.mouse_z == 0) ? maxdist : (float)vr_info.mouse_z;

	// origin of the laser beam
	VectorCopy( origin, ent.origin );

	// find the target point
	VectorMA( origin, maxdist, axis[0], ent.oldorigin );

	char tmp[2];

	memcpy(tmp, &laserBeamRGBA->string[0], sizeof(tmp));
	ent.shader.rgba[0] = strtol( tmp, NULL, 16 );

	memcpy(tmp, &laserBeamRGBA->string[2], sizeof(tmp));
	ent.shader.rgba[1] = strtol( tmp, NULL, 16 );

	memcpy(tmp, &laserBeamRGBA->string[4], sizeof(tmp));
	ent.shader.rgba[2] = strtol( tmp, NULL, 16 );

	memcpy(tmp, &laserBeamRGBA->string[6], sizeof(tmp));
	ent.shader.rgba[3] = strtol( tmp, NULL, 16 );

	re.AddRefEntityToScene(&ent, qfalse);
}
#endif

/*
===============
UI_DrawPlayer
===============
*/
static vec3_t head_origin;

void CL_DrawWeapon( playerInfo_t *pi ) {
	refEntity_t		gun		= { 0 };
	refEntity_t		flash	= { 0 }; // will not be shown
	//vec3_t 			vr_controller_axis[3];
	int				renderfx;
	vec3_t			origin;
	vec3_t 			cl_rightAngles;// with angle offset
	float			worldScale = 32.0f;

	// RF_LIGHTING_ORIGIN : use the same origin for all
	renderfx = RF_LIGHTING_ORIGIN | RF_NOSHADOW;

	// get controller position
	origin[0] = -vr_info.controllers[SideRIGHT].position.actual[0] * worldScale; // X
	origin[1] =  vr_info.controllers[SideRIGHT].position.actual[2] * worldScale; // Z
	origin[2] =  vr_info.controllers[SideRIGHT].position.actual[1] * worldScale; // Y
	//origin[2] -= DEFAULT_VIEWHEIGHT + 8.0f;// y

	// get controller angles
	VectorCopy(vr_info.controllers[SideRIGHT].angles.actual, cl_rightAngles);

	// =============================
	// init the gun
	// =============================
	gun.hModel = pi->weaponModel;
	gun.renderfx = renderfx;

	VectorCopy(origin, gun.lightingOrigin);

	VectorCopy(origin, gun.origin);

	// use the same direction than view direction
	cl_rightAngles[YAW] = AngleMod( cl_rightAngles[YAW] + spawnYaw );

	AnglesToAxis(cl_rightAngles, gun.axis);

#ifdef USE_LASER_SIGHT
	// =============================
	// init the laser beam
	// =============================
	// find VR controller muzzle point
	UI_PositionEntityOnTag(&flash, &gun, pi->weaponModel, "tag_flash");

	// keep origin and angles in order to draw the cursor in menu geometry 

	// laser beam origin
	VectorScale(flash.origin, 1000.0f, vr_info.ray_origin);

	// laser beam orientation
	VectorCopy(gun.axis[0], vr_info.ray_orientation);

	if (pi->laserBeamPlayer)
		CL_DrawLaserBeam(flash.origin, gun.axis);
#endif

	// add models to main menu scene
	re.AddRefEntityToScene(&gun, qfalse);
}

void CL_DrawPlayer( playerInfo_t *pi, int time ) {
	refEntity_t		legs = {0};
	refEntity_t		torso = {0};
	refEntity_t		head = {0};
	refEntity_t		gun = {0};
	refEntity_t		flash = {0}; // will not be shown

	vec3_t			origin;
	vec3_t 			vr_controller_axis[3];
	vec3_t 			res_axis;
	vec3_t 			cl_rightAngles;// with angle offset
	int				renderfx;

	if ( !pi->legsModel || !pi->torsoModel || !pi->headModel || !pi->animations[0].numFrames ) {
		return;
	}

	int cg_thirdperson = (int)FloatAsInt(Cvar_VariableValue("cg_thirdperson"));

	dp_realtime = time;

	origin[0] = 0.0f; // x
	origin[1] = 0.0f; // z
	origin[2] = -DEFAULT_VIEWHEIGHT -8.0f;// y

	memset( &legs, 	0, sizeof(legs) );
	memset( &torso, 0, sizeof(torso) );
	memset( &head, 	0, sizeof(head) );
	memset( &gun, 	0, sizeof(gun) );
	memset( &flash, 0, sizeof(flash) );

	re.ClearScene();

	//renderfx = RF_LIGHTING_ORIGIN | RF_NOSHADOW;   //RF_LIGHTING_ORIGIN ==> use the same origin for all
	renderfx = RF_LIGHTING_ORIGIN;   //RF_LIGHTING_ORIGIN ==> use the same origin for all

		// show only in mirrors in VR
	if (/*mySelf && !isDead &&*/ !cg_thirdperson)
		renderfx = RF_LIGHTING_ORIGIN | RF_THIRD_PERSON;

	// get the rotation information
	CL_PlayerAngles( pi, legs.axis, torso.axis, head.axis, gun.axis );

	// get the animation state (after rotation, to allow feet shuffle)
	UI_PlayerAnimation( pi, &legs.oldframe, &legs.frame, &legs.backlerp, &torso.oldframe, &torso.frame, &torso.backlerp );

	// allow non vr users to have virtual menu
	VectorCopy(pi->viewAngles, cl_rightAngles);

#ifdef USE_VR_QVM
	VectorCopy(vr_info.rightAngles, cl_rightAngles);
#endif
#if defined USE_NATIVE_HACK
	VectorCopy(vr_info.controllers[SideRIGHT].angles.actual, cl_rightAngles);
#endif

	// =========================
	// init the legs
	// =========================
	legs.hModel 	 = pi->legsModel;
	legs.customSkin  = pi->legsSkin;
	legs.renderfx 	 = renderfx;
	VectorCopy( origin, legs.origin );
	VectorCopy( origin, legs.lightingOrigin );
	VectorCopy( legs.origin, legs.oldorigin );

	// =========================
	// init the torso
	// =========================
	torso.hModel 	  = pi->torsoModel;
	torso.customSkin  = pi->torsoSkin;

	// don't show torso when leaning forward
	if ( cl_rightAngles[PITCH] >= VR_angle_hide_torso->integer || VR_hide_torso->integer == 1 )
		torso.renderfx = RF_THIRD_PERSON; // show only in mirror
	else
		torso.renderfx 	  = renderfx;

	VectorCopy( origin, torso.lightingOrigin );
	UI_PositionRotatedEntityOnTag( &torso, &legs, pi->legsModel, "tag_torso");

	// =============================
	// init the gun
	// =============================
	gun.hModel   = pi->weaponModel;

	gun.renderfx = RF_LIGHTING_ORIGIN | RF_NOSHADOW;// renderfx;

	VectorCopy( origin, gun.lightingOrigin );
	UI_PositionRotatedEntityOnTag( &gun, &torso, pi->torsoModel, "tag_weapon");

	// for the gun to have the vr controller orientation, the torso have to do the appropriate rotations
	// we place torso and gun, get vr controller and gun orientation difference, add difference to torso

	cl_rightAngles[YAW] = AngleMod( cl_rightAngles[YAW] + spawnYaw );
	AnglesToAxis( cl_rightAngles, vr_controller_axis );

	VectorSubtract( gun.axis[0], vr_controller_axis[0], res_axis ); // Remove VR controller axis part from gun axis
	VectorAdd( torso.axis[0], res_axis, torso.axis[0] ); // we only change the matrix[0]

	memcpy( gun.axis, vr_controller_axis, 36 ); //memcpy(to, from, size)

	UI_PositionEntityOnTag(&flash, &gun, pi->weaponModel, "tag_flash");

	// laser beam origin
	VectorScale(flash.origin, 1000.0f, vr_info.ray_origin);

	// laser beam orientation
	VectorCopy(gun.axis[0], vr_info.ray_orientation);

#ifdef USE_LASER_SIGHT
	if ( pi->laserBeamPlayer )
		CL_DrawLaserBeam( flash.origin, vr_controller_axis );
#endif

	// =========================
	// init the head
	// =========================
	head.hModel 	  = pi->headModel;
	head.customSkin   = pi->headSkin;
	head.renderfx 	  = renderfx | RF_THIRD_PERSON; // show only in mirror
	VectorCopy( origin, head.lightingOrigin );

	UI_PositionRotatedEntityOnTag( &head, &torso, pi->torsoModel, "tag_head");

	// keep head origin to be able to draw the menu in front of player eyes
	if ( head_origin[1] != 0.0f && head_origin[2] != 0.0f) {
		VectorCopy(head.origin, head_origin);
	}

	// =========================
	// add models to main menu scene
	// =========================
	re.AddRefEntityToScene( &legs , qfalse);
	re.AddRefEntityToScene( &torso , qfalse);
	re.AddRefEntityToScene( &gun , qfalse);
	re.AddRefEntityToScene( &head , qfalse);
}

/*
=================
PlayerModel_UpdateModel
=================
*/
#ifdef USE_VR
static void PlayerModel_UpdateModel( vec3_t viewangles )
{
	char		buf[MAX_QPATH];
	qhandle_t 	index;
	index = s_playersettings.playerinfo.legsModel; // test if model is loaded

	Cvar_VariableStringBuffer( "model", buf, sizeof( buf ) );
	if ( (strcmp( buf, s_playersettings.playerModel ) != 0 ) || ( (index < 0) || (index >= tr.numModels) ) )
	{
		memset( &s_playersettings.playerinfo, 0, sizeof(playerInfo_t) );

		laserBeamShader = re.RegisterShader( "laserbeam" );

		UI_PlayerInfo_SetModel( &s_playersettings.playerinfo, buf );
		strcpy( s_playersettings.playerModel, buf );

		UI_PlayerInfo_SetInfo( &s_playersettings.playerinfo, LEGS_IDLE, TORSO_STAND2, viewangles, vec3_origin, WP_VR_CONTROLLER, qfalse );
	}

	//if ( vr_controller_type->integer >= 2 ) {
	if (renderingThirdPerson->integer == 1 )
		CL_DrawPlayer(&s_playersettings.playerinfo, cls.realtime / 2);
	else
		CL_DrawWeapon(&s_playersettings.playerinfo);
}
#endif


#ifdef USE_VIRTUAL_KEYBOARD
static void createVirtualKeyboard( vec3_t angles_menu )
{
	refEntity_t		ent_vkb;
	qhandle_t keyboard_shader = re.RegisterShader( "menu/keyboard_tex.tga" );

	memset( &ent_vkb, 0, sizeof(ent_vkb) );

	ent_vkb.reType = RT_VR_KEYBOARD;
	ent_vkb.customShader = keyboard_shader;

	VectorCopy( head_origin, ent_vkb.origin );
	ent_vkb.origin[1] -= 10.0f;

	AnglesToAxis( angles_menu, ent_vkb.axis );

	ent_vkb.rotation = angles_menu[YAW];

	re.AddRefEntityToScene( &ent_vkb, qfalse );
}
#endif
//==========================================
// create_menu_scene
//
// Create the menu display canvas
// in order to render the menu texture
//==========================================

//FIXME: caching may works if setting isInit each time we call the main menu
static qboolean isInit;

static qhandle_t floor_shader;
static qhandle_t banner_model;
//static qhandle_t keyboard_shader;

void initMainMenuScene() {
	if ( isInit )
		return;

	menu_distance = Cvar_Get("menu_distance", "80.0", CVAR_ARCHIVE | CVAR_LATCH);
	showVirtualKeyboard = Cvar_Get("showVirtualKeyboard", "0", 0);
	VR_hide_torso = Cvar_Get("VR_hide_torso", "1", 1);
	VR_angle_hide_torso = Cvar_Get("VR_angle_hide_torso", "10.0", 10.0f);
	renderingThirdPerson = Cvar_Get("cg_thirdperson", "0", CVAR_LATCH);
	banner_model = re.RegisterModel(MAIN_BANNER_MODEL);
	floor_shader =  re.RegisterShader( "menu/floor_grid_black.jpg" );
	//keyboard_shader = re.RegisterShader( "menu/keyboard_tex.tga" );//TODO
	isInit = qtrue;
}

const float worldScale = 32.0f;

void CL_create_menu_scene( void ) {
#ifdef USE_VIRTUAL_MENU 
	refdef_t		refdef;
	refEntity_t		ent_banner, ent_menu, ent_floor;
	vec3_t			origin_view, origin_banner, origin_light;
	vec3_t			angles_view, angles_menu, angles_floor, angles_banner;
	float x, fov_y;

	// Cache shaders & models
	initMainMenuScene();

	//==========================================
	//           setup the refdef
	//==========================================
	memset( &refdef, 0, sizeof( refdef ) );
	AxisClear( refdef.viewaxis );

	refdef.rdflags = RDF_NOWORLDMODEL;
	refdef.time = cls.realtime;
	refdef.x = 0;
	refdef.y = 0;
	refdef.width = glConfig.vidWidth;
	refdef.height = glConfig.vidHeight;
	
	VectorClear(origin_view);
	VectorCopy( origin_view, refdef.vieworg );

#ifdef USE_NATIVE_HACK
	if ( vr_controller_type->integer  ) {
		VectorCopy(vr_info.hmdinfo.angles.actual, angles_view);
	}
	else 
#endif
		VectorCopy(cl.viewangles, angles_view);

	//angles_view[YAW] = AngleMod(angles_view[YAW] - 90.0f);
	angles_view[YAW] = AngleMod(angles_view[YAW] + spawnYaw);

	AnglesToAxis(angles_view, refdef.viewaxis);

#ifdef USE_VRAPI
	if ( vr_controller_type->integer == 1 ) {
		refdef.fov_x = vr_info.hmdinfo.fov_x;
		refdef.fov_y = vr_info.hmdinfo.fov_y;
	}
	else
#endif
	{
		const float fov_x = 90.0f;
		x = refdef.width / tan(fov_x / 360 * M_PI);
		fov_y = atan2(refdef.height, x);
		fov_y = fov_y * 360 / M_PI;
		refdef.fov_x = fov_x;
		refdef.fov_y = fov_y;
	}

#ifdef USE_VR
	// Allow strafe move
	refdef.vieworg[0] = -vr_info.hmdinfo.position.actual[0] * worldScale;
	refdef.vieworg[1] =  vr_info.hmdinfo.position.actual[2] * worldScale;
	refdef.vieworg[2] =  vr_info.hmdinfo.position.actual[1] * worldScale;
#endif

	//==========================================
	//            Clear the scene
	//             ( black sky )
	//==========================================
	re.ClearScene();

	//==========================================
	//       add banner model in the sky
	//==========================================
	memset( &ent_banner, 0, sizeof(ent_banner) );

	origin_banner[0] =     0.0f; // x
	origin_banner[1] = -1200.0f; // z
	origin_banner[2] =   500.0f; // y

	ent_banner.hModel = banner_model;
	VectorCopy( origin_banner, ent_banner.origin );
	VectorCopy( origin_banner, ent_banner.lightingOrigin );
	ent_banner.renderfx = RF_LIGHTING_ORIGIN | RF_NOSHADOW;
	VectorCopy (ent_banner.origin, ent_banner.oldorigin);

	float adjust = 5.0 * sin((float)cls.realtime / 5000); // add yaw move

	float yawAngle = AngleMod( 360.0f - spawnYaw + adjust );

	VectorSet(angles_banner, 25.0f, yawAngle, 0.0f);
	AnglesToAxis( angles_banner, ent_banner.axis );

	VectorScale( ent_banner.axis[0], 5.0f, ent_banner.axis[0] );
	VectorScale( ent_banner.axis[1], 5.0f, ent_banner.axis[1] );
	VectorScale( ent_banner.axis[2], 5.0f, ent_banner.axis[2] );

	re.AddRefEntityToScene( &ent_banner , qtrue );

	//==========================================
	//               add floor
	//==========================================
	memset(&ent_floor, 0, sizeof(ent_floor));
	VectorSet(ent_floor.origin, 0.0f, 0.0f, -60.0f);

	ent_floor.reType = RT_VR_MAIN_MENU_FLOOR;
	ent_floor.customShader = floor_shader;
	ent_floor.renderfx = 0;

	AnglesToAxis( angles_floor, ent_floor.axis );

	re.AddRefEntityToScene( &ent_floor , qfalse);
	re.RenderScene( &refdef );
//	re.ClearScene();

	//==========================================
	//             add menu geometry
	//==========================================
	memset( &ent_menu, 0, sizeof(ent_menu) );
	ent_menu.reType = RT_VR_MENU;
	VectorCopy( head_origin, ent_menu.origin );

	VectorSet(vr_info.spawnAngles, 0, 0, 0);
	//vr_info.spawnAngles[YAW] = 270;// -90;
    //vr_info.spawnAngles[YAW] = AngleMod(360 + vr_info.spawnAngles[YAW] - 90);

	angles_menu[PITCH]	= 0.0f;
	angles_menu[YAW]	= spawnYaw;
	angles_menu[ROLL]	= 0.0f;
	AnglesToAxis( angles_menu, ent_menu.axis );

	// Add some distance to the player
	VectorMA( ent_menu.origin, menu_distance->value, ent_menu.axis[0], ent_menu.origin );

	ent_menu.rotation = angles_menu[YAW];

	re.AddRefEntityToScene( &ent_menu, qfalse );
	re.RenderScene( &refdef );
//	re.ClearScene();

#ifdef USE_VIRTUAL_KEYBOARD
	//==========================================
	//            add virtual keyboard
	//==========================================
	if ( showVirtualKeyboard->integer )
	{
		createVirtualKeyboard( angles_menu );
		re.RenderScene( &refdef );
		re.ClearScene();
	}
#endif

#ifdef USE_VR
	//==========================================
	//            add player model
	//==========================================
	// no player or laser beam during loading & initialisation
	if ( cls.state == CA_DISCONNECTED ) {
		PlayerModel_UpdateModel( angles_view );
	}
#endif

	// =========================
	// add an accent light
	// doesn't work
	// =========================
	VectorCopy( origin_light, ent_menu.origin );

	origin_light[0] -= 100;
	origin_light[1] += 100;
	origin_light[2] += 100;
	re.AddLightToScene( origin_light, 500, 1.0, 1.0, 1.0 );

	VectorCopy(origin_light, ent_menu.origin);
	origin_light[0] -= 100;
	origin_light[1] -= 100;
	origin_light[2] -= 100;
	re.AddLightToScene( origin_light, 500, 1.0, 0.0, 0.0 );

	re.RenderScene( &refdef );
#endif // USE_VIRTUAL_MENU
}

