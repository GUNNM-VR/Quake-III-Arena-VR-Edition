#ifndef VR_KEYBOARD_H
#define VR_KEYBOARD_H

#include "../client/client.h" // for Con_ToggleConsole_f()
#include "VRMOD_common.h"
#include "../vrmod/quaternion.h"

#if defined USE_VR
#include "VRMOD_input.h"
#endif

#define MAX_CONSOLE_KEYS 16

#define CTRL(a) ((a)-'a'+1)

typedef enum
{
	State_LowerCase,
	State_UpperCase,
	State_Symbol,
	State_Color,
} keyboard_state_t;

typedef struct {
	//=========================
	//      Configuration
	//=========================
	const V_Type_t type;
	const float width; // 64 menu
	const float height;	// 24 menu
	const float distance;
	const float h_below_menu;
	const float pitch; // in degree
	const int lines;
	// number of keys in each line of the keyboard 
	const int buttonsInLine[5];
	//=========================
	//      Board style
	//=========================
	const float style_keySpace;
	const float style_frame_thickness;
	//=========================
	//      Dynamic
	//=========================
	vec3_t origin;
	float angle_deg_YAW;
	Quaternion pitchRotation; // calculate from pitch
} board_t;

//=================================================
//         Virtual Keyboard config & style
//=================================================
static board_t keysBoard = {
		.type 			= V_keyboard,
		.width 			= 60.0f,
		.height 		= 18.0f,
		.distance 		= 60.0f,
		.h_below_menu 	= 34.0f,
		.pitch 			= 25.0f,
		.lines 			= 5,
		.buttonsInLine 	= { 10, 10, 10, 9, 5 },
		.style_keySpace = 0.4f,
		.style_frame_thickness = 0.4f
};

//=================================================
// ASCII char in texture:
//line 3: space ! " # $ % & ' ( ) * + , - . /
//line 4: 0 1 2 3 4 5 6 7 8 9 : ; < = > ?
//line 6: @ ABC-Z [ \ ] ^ _
//line 7: ' abc-z { : } " leftArrow
//=================================================
typedef enum
{
	vk_NULL, // special case: no focused key
	// a-z A-Z
	vk_a,
	vk_b,
	vk_c,
	vk_d,
	vk_e,
	vk_f,
	vk_g,
	vk_h,
	vk_i,
	vk_j,
	vk_k,
	vk_l,
	vk_m,
	vk_n,
	vk_o,
	vk_p,
	vk_q,
	vk_r,
	vk_s,
	vk_t,
	vk_u,
	vk_v,
	vk_w,
	vk_x,
	vk_y,
	vk_z,
	// 0-9
	vk_0,
	vk_1,
	vk_2,
	vk_3,
	vk_4,
	vk_5,
	vk_6,
	vk_7,
	vk_8,
	vk_9,
	// texture line 3
	vk_SPACE, 			// empty
	vk_EXCLAMATION, 	// !
	vk_DBLQUOTE, 		// "
	vk_SHARP, 			// #
	vk_DOLLAR, 			// $
	vk_PERCENT, 		// %
	vk_AMPERSAND, 		// &
	vk_QUOTE, 			// '
	vk_BRACKET_OPEN, 	// (
	vk_BRACKET_CLOSE, 	// )
	vk_STAR, 			// *
	vk_PLUS, 			// +
	vk_COMMA, 			// ,
	vk_MINUS, 			// -
	vk_DOT, 			// .
	vk_SLASH, 			// /
	// texture line 4
	vk_COLON,	 		// :
	vk_SEMICOLON, 		// ;
	vk_INFERIOR, 		// <
	vk_EQUAL, 			// =
	vk_SUPERIOR, 		// >
	vk_INTERROGATION, 	// ?
	// texture line 5
	vk_AROBASE, 		// @
	vk_ARRAY_OPEN,		// [
	vk_BACKSLASH, 		/* \ */
	vk_ARRAY_CLOSE,		// ]
	vk_CIRCUMFLEX, 		// ^
	vk_UNDERSCORE, 		// _
	// texture line 6
	vk_BRACE_OPEN, 		// {
	vk_VERTICAL_BAR,	// |
	vk_BRACE_CLOSE,		// }
	// texture line 7
	vk_LEFTARROW, 		// ‹
	// not in texture:
	/*
	vk_UPARROW,
	vk_DOWNARROW,
	vk_RIGHTARROW, 		// ›
	vk_TILDE, 			// ~
	vk_DEGREE, 			// °
	*/
// ========================================
//	function Keys, special texture
	vk_SYMBOL,
	vk_BACKSPACE,
	vk_CAPSLOCK,

	vk_HIDE_KB,
	vk_COLOR,
	vk_ENTER,
// ========================================
// Tool bar button, special texture
	vk_HOME,
	vk_CHAT,
	vk_CONSOLE,
	vk_CONFIG,
	vk_MIRROR,

	vk_LAST

/* // others key name from ioq3:
	k_TAB
	K_ESCAPE
	K_COMMAND
	K_POWER,
	K_PAUSE,

	K_ALT,
	K_CTRL,
	K_SHIFT,
	K_INS,
	K_DEL,
	K_PGDN,
	K_PGUP,
	K_HOME,
	K_END,

	K_KP_HOME,
	K_KP_UPARROW,
	K_KP_PGUP,
	K_KP_LEFTARROW,
	K_KP_5,
	K_KP_RIGHTARROW,
	K_KP_END,
	K_KP_DOWNARROW,
	K_KP_PGDN,
	K_KP_ENTER,
	K_KP_INS,
	K_KP_DEL,
	K_KP_SLASH,
	K_KP_MINUS,
	K_KP_PLUS,
	K_KP_NUMLOCK,
	K_KP_STAR,
	K_KP_EQUALS	*/
} keys_t;


// configuration for normal and capital keys
//========================================
//	1   2   3   4   5   6   7   8   9   0
//  q   w   e   r   t   y   u   i   o   p
//  Smb a   s   d   f   g   h   j   k   l
//  Cap   z   x   c   v   b   n   m   Bk
//  Hid  ^         Sp         .   Enter
//========================================
static const keys_t line_1_abc[10] = { vk_1, vk_2, vk_3, vk_4, vk_5, vk_6, vk_7, vk_8, vk_9, vk_0 };
static const keys_t line_2_abc[10] = { vk_q, vk_w, vk_e, vk_r, vk_t, vk_y, vk_u, vk_i, vk_o, vk_p };
static const keys_t line_3_abc[10] = { vk_SYMBOL, vk_a, vk_s, vk_d, vk_f, vk_g, vk_h, vk_j, vk_k, vk_l };
static const keys_t line_4_abc[9] = { vk_CAPSLOCK, vk_z, vk_x, vk_c, vk_v, vk_b, vk_n, vk_m, vk_BACKSPACE };
static const keys_t line_5_abc[5] = { vk_HIDE_KB, vk_COLOR, vk_SPACE, vk_DOT, vk_ENTER};

// configuration for symbol keys
//========================================
//  1   2   3   4   5   6   7   8   9   0
//  !   ~   #   $   %   ^   &   *   (   )    => ~ devient vk_AROBASE
//  abc -   _   +   =   \   ;   :   '   "
//  N°  {   }   <   >   ,   /   ?   back
//========================================
static const keys_t line_1_symb[10] = { vk_1, vk_2, vk_3, vk_4, vk_5, vk_6, vk_7, vk_8, vk_9, vk_0 }; // same as line_1_abc
static const keys_t line_2_symb[10] = { vk_EXCLAMATION, vk_AROBASE, vk_SHARP, vk_DOLLAR, vk_PERCENT, vk_CIRCUMFLEX, vk_AMPERSAND, vk_STAR, vk_BRACKET_OPEN, vk_BRACKET_CLOSE };
static const keys_t line_3_symb[10] = { vk_SYMBOL, vk_MINUS, vk_UNDERSCORE, vk_PLUS, vk_EQUAL, vk_BACKSLASH, vk_SEMICOLON, vk_COLON, vk_QUOTE, vk_DBLQUOTE };
static const keys_t line_4_symb[9] = { vk_CAPSLOCK, vk_BRACE_OPEN, vk_BRACE_CLOSE, vk_INFERIOR, vk_SUPERIOR, vk_COMMA, vk_SLASH, vk_INTERROGATION, vk_BACKSPACE };
static const keys_t line_5_symb[5] = { vk_HIDE_KB, vk_COLOR, vk_SPACE, vk_DOT, vk_ENTER};

//========================================
// public functions
//========================================
void RB_Surface_VR_keyboard( void );
#endif //VR_KEYBOARD_H
