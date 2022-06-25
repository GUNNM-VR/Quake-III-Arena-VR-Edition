
//FIXME ripped from cg_local.h, and also present in ui_local
typedef struct {
    int			oldFrame;
    int			oldFrameTime;		// time when ->oldFrame was exactly on

    int			frame;
    int			frameTime;			// time when ->frame will be exactly on

    float		backlerp;

    float		yawAngle;
    qboolean	yawing;
    float		pitchAngle;
    qboolean	pitching;

    int			animationNumber;	// may include ANIM_TOGGLEBIT
    animation_t	*animation;
    int			animationTime;		// time when the first frame of the animation will be exact
} lerpFrame_t;


typedef struct {
    // model info
    qhandle_t		legsModel;
    qhandle_t		legsSkin;

    lerpFrame_t		legs;

    qhandle_t		torsoModel;
    qhandle_t		torsoSkin;
    lerpFrame_t		torso;

    qhandle_t		headModel;
    qhandle_t		headSkin;

    animation_t		animations[MAX_ANIMATIONS];

    qboolean		fixedlegs;		// true if legs yaw is always the same as torso yaw
    qboolean		fixedtorso;		// true if torso never changes yaw

    qhandle_t		weaponModel;
    qhandle_t		barrelModel;
    qhandle_t		flashModel;
    vec3_t			flashDlightColor;
    int				muzzleFlashTime;

    vec3_t			color1;
    byte			c1RGBA[4];

    // currently in use drawing parms
    vec3_t			viewAngles;
    vec3_t			moveAngles;
    weapon_t		currentWeapon;
    int				legsAnim;
    int				torsoAnim;

    // animation vars
    weapon_t		weapon;
    weapon_t		lastWeapon;
    weapon_t		pendingWeapon;
    int				weaponTimer;
    int				pendingLegsAnim;
    int				torsoAnimationTimer;

    int				pendingTorsoAnim;
    int				legsAnimationTimer;

    qboolean		chat;
    qboolean		newModel;

    qboolean		barrelSpinning;
    float			barrelAngle;
    int				barrelTime;

    int				realWeapon;
    qboolean 		laserBeamPlayer;
} playerInfo_t;


typedef struct {
    playerInfo_t		playerinfo;
    char				playerModel[MAX_QPATH];
} playersettings_t;

void CL_create_menu_scene( void );
