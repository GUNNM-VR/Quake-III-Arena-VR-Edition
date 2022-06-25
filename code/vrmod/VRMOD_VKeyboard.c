#include "VRMOD_VKeyboard.h"
//#include "VRMOD_input.h"

#if defined(__ANDROID__) && defined(__ANDROID_LOG__)
#include "../android/android_local.h" // for debug, delete me
#endif

const float d8= 0.125f; // convenient const for texture uv ( 1/8 )

static keys_t event_keyFocus;
static keyboard_state_t VkeyboardState;

//=================================================
//        Virtual Keyboard color
//=================================================
static byte color_frame[4] = {150, 150, 150, 255};
static byte color_white[4] = {255, 255, 255, 255};
static byte color_red[4]   = {255, 0, 0, 255};
//static byte color_green[4]   = {0, 116, 116, 255};
static byte color_yellow[4]   = {254, 226, 9, 255};
static byte color_letters[4] = {255, 255, 255, 255};

//========================================
// get special keys width
//========================================
static float width_from_kbCode( keys_t kbCode ) {
	switch( kbCode )
	{
		case vk_CAPSLOCK:
		case vk_BACKSPACE:
		case vk_HIDE_KB:
		case vk_ENTER:
			return 1.5f;
		case vk_SPACE:
			return 5.0f;
		default:
			return 1.0f;
	}
}


keys_t get_key_code( const int line, const int col, const V_Type_t type)
{
	if (type != V_keyboard)
		return 0;

	if (VkeyboardState == State_LowerCase || VkeyboardState == State_UpperCase)
	{
		switch ( line ) {
		case 0:
			return line_1_abc[col];
		case 1:
			return line_2_abc[col];
		case 2:
			return line_3_abc[col];
		case 3:
			return line_4_abc[col];
		case 4:
			return line_5_abc[col];
		default:
			return 0;
			break;
		}
	}
	else if (VkeyboardState == State_Symbol)
	{
		switch ( line ) {
		case 0:
			return line_1_symb[col];
		case 1:
			return line_2_symb[col];
		case 2:
			return line_3_symb[col];
		case 3:
			return line_4_symb[col];
		case 4:
			return line_5_symb[col];
		default:
			return 0;
			break;
		}
	}
	// the color button is not implemented, I don't know if I will keep it
	else if (VkeyboardState == State_Color)
	{
		// TODO What to do?
	}
	return 0;
}

void create_and_rotate_quad_points( const board_t board, const vec3_t origin_frame, const vec2_t quad_size, vec3_t point0, vec3_t point1, vec3_t point2, vec3_t point3 )
{
	float halfWidth  = quad_size[0] / 2.0f;
	float halfHeight = quad_size[1] / 2.0f;
	float xStart = 0.0f;
	float xEnd = -quad_size[0];

	// use an axis rotation for yaw, but quaternion for pitch
	// FIXME: use quaternion for both YAW & PITCH rotation
	user_facing_quad( origin_frame, point0, point1, point2, point3, board.angle_deg_YAW, halfWidth, halfHeight, xStart, xEnd, 0, 0, 0, 0 );

	QuaternionRotatePoint(board.pitchRotation, point0, point0);
	QuaternionRotatePoint(board.pitchRotation, point1, point1);
	QuaternionRotatePoint(board.pitchRotation, point2, point2);
	QuaternionRotatePoint(board.pitchRotation, point3, point3);

	// move bellow menu H at distance Z
	VectorAdd( point0, board.origin, point0 );
	VectorAdd( point1, board.origin, point1 );
	VectorAdd( point2, board.origin, point2 );
	VectorAdd( point3, board.origin, point3 );
}

static char * char_From_kbCode( keys_t kbCode ) {
	qboolean u = (VkeyboardState == State_UpperCase);
	qboolean s = (VkeyboardState == State_Symbol);
	//ALOGE("u:%i, s:%i kbCode:%i", u, s, kbCode);

	switch( kbCode ) {
		case vk_a: 	return u? "A": "a";
		case vk_b: 	return u? "B": "b";
		case vk_c: 	return u? "C": "c";
		case vk_d: 	return u? "D": "d";
		case vk_e: 	return u? "E": "e";
		case vk_f: 	return u? "F": "f";
		case vk_g: 	return u? "G": "g";
		case vk_h: 	return u? "H": "h";
		case vk_i: 	return u? "I": "i";
		case vk_j: 	return u? "J": "j";
		case vk_k: 	return u? "K": "k";
		case vk_l: 	return u? "L": "l";
		case vk_m: 	return u? "M": "m";
		case vk_n: 	return u? "N": "n";
		case vk_o: 	return u? "O": "o";
		case vk_p: 	return u? "P": "p";
		case vk_q: 	return u? "Q": "q";
		case vk_r: 	return u? "R": "r";
		case vk_s: 	return u? "S": "s";
		case vk_t: 	return u? "T": "t";
		case vk_u: 	return u? "U": "u";
		case vk_v: 	return u? "V": "v";
		case vk_w: 	return u? "W": "w";
		case vk_x: 	return u? "X": "x";
		case vk_y: 	return u? "Y": "y";
		case vk_z: 	return u? "Z": "z";

		case vk_0: 	return "0";
		case vk_1: 	return "1";
		case vk_2: 	return "2";
		case vk_3: 	return "3";
		case vk_4: 	return "4";
		case vk_5: 	return "5";
		case vk_6: 	return "6";
		case vk_7: 	return "7";
		case vk_8:	return "8";
		case vk_9: 	return "9";

		case vk_SPACE: 			return " ";
		case vk_EXCLAMATION: 	return "!";
		case vk_DBLQUOTE: 		return "'";
		case vk_SHARP: 			return "#";
		case vk_DOLLAR: 		return "$";
		case vk_PERCENT: 		return "%";
		case vk_AMPERSAND: 		return "&";
		case vk_QUOTE: 			return "'";
		case vk_BRACKET_OPEN: 	return "(";
		case vk_BRACKET_CLOSE: 	return ")";
		case vk_STAR: 			return "*";
		case vk_PLUS: 			return "+";
		case vk_COMMA: 			return ",";
		case vk_MINUS: 			return "-";
		case vk_DOT: 			return ".";
		case vk_SLASH: 			return "/";
		case vk_COLON: 			return ":";
		case vk_SEMICOLON: 		return ";";
		case vk_INFERIOR: 		return "<";
		case vk_EQUAL: 			return "=";
		case vk_SUPERIOR: 		return ">";
		case vk_INTERROGATION: 	return "?";
		case vk_AROBASE: 		return "@";
		case vk_ARRAY_OPEN: 	return "[";
		case vk_BACKSLASH: 		return "\\";
		case vk_ARRAY_CLOSE: 	return "]";
		case vk_CIRCUMFLEX: 	return "^";
		case vk_UNDERSCORE: 	return "_";
		case vk_BRACE_OPEN: 	return "{";
		case vk_VERTICAL_BAR: 	return "{";
		case vk_BRACE_CLOSE: 	return "}";
		case vk_LEFTARROW: 		return "‹";
		case vk_SYMBOL: 		return s? "abc": "+=";
		default: return "NULL";
	}
}


qboolean create_outline_frame( const board_t board, const vec3_t origin_frame, const vec2_t outQuadSize, const keys_t keyCode, const qboolean needIntersection )
{
	qboolean hasFocus = qfalse; // bug: if not init to qfalse, it may stay qtrue like if static.
	vec3_t point0, point1, point2, point3;
	vec3_t intersection_uvz;

	// take a white pixel (actually 185x, 135y)
	float u = 0.72265625f;
	float v = 0.52734375f;

	//====================================
	// create outline frame
	// (a quad of the exact size)
	//====================================
	create_and_rotate_quad_points( board, origin_frame, outQuadSize, point0, point1, point2, point3 );

	if ( needIntersection ) {
		// check focus on outer frame only
		hasFocus = getIntersection( point0, point1, point2, point3,  0.0f, 0.0f, 1.0f, 1.0f, intersection_uvz);
	}

	if ( hasFocus ) {
		// Keep intersection for later use
		vr_info.mouse_z = (int) ( intersection_uvz[2] ) - 1; // we only need depth to get the 3D intersection
		vr_info.ray_focused = board.type;
	}

	// Draw outer quad with varying color depending of key state
	if ( hasFocus && keyCode != vk_NULL) {
	    // board outline don't change color
		drawQuad( point0, point1, point2, point3, u, v, u, v, color_red );
		event_keyFocus = keyCode; // static value for accessing keyboard state
	}
	// vk_CAPSLOCK change color when selected (and not focused)
	else if ( VkeyboardState == State_UpperCase && keyCode == vk_CAPSLOCK ) {
		drawQuad( point0, point1, point2, point3, u, v, u, v, color_yellow );
	}
	else {
		drawQuad( point0, point1, point2, point3, u, v, u, v, color_frame );
	}

	//====================================
	// create background texture
	// in a smaller quad
	//====================================
	vec2_t inQuadSize;
	inQuadSize[0] = outQuadSize[0] - board.style_frame_thickness;
	inQuadSize[1] = outQuadSize[1] - board.style_frame_thickness;

	create_and_rotate_quad_points( board, origin_frame, inQuadSize, point0, point1, point2, point3 );
	drawQuad( point0, point1, point2, point3, d8, 1, 0, 1-d8, color_white );

	return hasFocus;
}

void drawLetters( const board_t board, const vec3_t origin_char, keys_t keyCode, vec2_t key_Size ) {
	vec3_t point0, point1, point2, point3;
	float row, col;
	const char* s;
	char ch;
	// police texture, support multiple char
	float space_between_letter = 1.0f;

	const refEntity_t *e = &backEnd.currentEntity->e; // todo à deplacer

	const char * str = "";

	vec3_t letter_origin;
	VectorCopy(origin_char, letter_origin);

	if ( VkeyboardState == State_Color ) {
		//TODO implement
		return;
	}
	else {
		str = char_From_kbCode( keyCode );
	}

	// center justify at x
	if ( strlen(str) > 1 ) {
		
		float offset = strlen(str) * space_between_letter / 2.0f;
		VectorMA( letter_origin, offset, e->axis[1], letter_origin );
	}

	create_and_rotate_quad_points( board, letter_origin, key_Size, point0, point1, point2, point3 );
	
	s = str;
	while ( *s )
	{
		ch = *s & 255;
		if (ch != ' ')
		{
			row = (ch>>4)*0.0625;
			col = (ch&15)*0.0625;
			if( strlen(str) > 1 ) {
				VectorMA( point0, -space_between_letter, e->axis[1], point0 );
				VectorMA( point1, -space_between_letter, e->axis[1], point1 );
				VectorMA( point2, -space_between_letter, e->axis[1], point2 );
				VectorMA( point3, -space_between_letter, e->axis[1], point3 );
			}	
			drawQuad( point0, point1, point2, point3, col + 0.0625, row + 0.0625, col, row, color_letters );
		}
		s++;
	}
}

// Event are managed inside
void paintTextureQuad(const board_t board, const vec3_t origin_char, const keys_t keyCode )
{
	vec3_t point0, point1, point2, point3;
	vec2_t key_Size = { 1.5f, 1.5f }; // letters width & height

	switch( keyCode )
	{
		//=======================
		// Textured keys
		//=======================
		case vk_BACKSPACE:
			key_Size[0] = 2.5f;
			create_and_rotate_quad_points( board, origin_char, key_Size, point0, point1, point2, point3 );
			drawQuad( point0, point1, point2, point3, 2*d8, d8, 0, 0, color_white );
			break;
		case vk_ENTER:
			key_Size[0] = 2.5f;
			create_and_rotate_quad_points( board, origin_char, key_Size, point0, point1, point2, point3 );
			drawQuad( point0, point1, point2, point3, 3*d8, d8, 0.25, 0, color_white );
			break;
		case vk_CAPSLOCK:
			key_Size[0] = 2.5f;
			create_and_rotate_quad_points( board, origin_char, key_Size, point0, point1, point2, point3 );
			drawQuad( point0, point1, point2, point3, 4*d8, d8, 3*d8, 0, color_white );
			break;
		case vk_COLOR:
			key_Size[0] = 2.5f;
			key_Size[1] = 2.5f;
			create_and_rotate_quad_points( board, origin_char, key_Size, point0, point1, point2, point3 );
			drawQuad( point0, point1, point2, point3, 5*d8, d8, 4*d8, 0, color_white );
			break;
		case vk_HIDE_KB:
			key_Size[0] = 2.5f;
			key_Size[1] = 2.2f;
			create_and_rotate_quad_points( board, origin_char, key_Size, point0, point1, point2, point3 );
			drawQuad( point0, point1, point2, point3, 6*d8, d8, 5*d8, 0, color_white );
			break;
		//=======================
		// Tool bar textures
		//=======================
		case vk_HOME:
			key_Size[0] = 2.5f;
			key_Size[1] = 2.5f;
			create_and_rotate_quad_points( board, origin_char, key_Size, point0, point1, point2, point3 );
			drawQuad( point0, point1, point2, point3, 5*d8, d8, 4*d8, 0, color_white );
			break;
		case vk_CHAT:
			key_Size[0] = 2.5f;
			key_Size[1] = 2.5f;
			create_and_rotate_quad_points( board, origin_char, key_Size, point0, point1, point2, point3 );
			drawQuad( point0, point1, point2, point3, 5*d8, d8, 4*d8, 0, color_white );
			break;
		case vk_CONSOLE:
			key_Size[0] = 2.5f;
			key_Size[1] = 2.5f;
			create_and_rotate_quad_points( board, origin_char, key_Size, point0, point1, point2, point3 );
			drawQuad( point0, point1, point2, point3, 4*d8, d8, 3*d8, 0, color_white );
			break;
		case vk_CONFIG:
			key_Size[0] = 2.5f;
			key_Size[1] = 2.5f;
			create_and_rotate_quad_points( board, origin_char, key_Size, point0, point1, point2, point3 );
			drawQuad( point0, point1, point2, point3, 5*d8, d8, 4*d8, 0, color_white );
			break;
		case vk_MIRROR:
			key_Size[0] = 2.5f;
			key_Size[1] = 2.5f;
			create_and_rotate_quad_points( board, origin_char, key_Size, point0, point1, point2, point3 );
			drawQuad( point0, point1, point2, point3, 5*d8, d8, 4*d8, 0, color_white );
			break;
		//=======================
		// Draw the letters
		//=======================
		default:
			drawLetters( board, origin_char, keyCode, key_Size);
			break;
	}
}

float getKey_x( const int line, const int col, const float key_w, const float key_width, const board_t board )
{
	float w;
	keys_t keyCode;
	float key_pos_x = board.width/2 - board.style_keySpace/2 -board.style_keySpace;
	
	// find pos X, by measuring previous keys in the line, taking spaces into account
	for (int i = 0; i < col; i++) {
		keyCode = get_key_code( line, i, board.type );
		w = width_from_kbCode( keyCode );

		key_pos_x -= (key_w * w) + (board.style_keySpace * (w - 1)); // Key number i real width
		key_pos_x -= board.style_keySpace;
	}

	key_pos_x -= key_width / 2;
	return key_pos_x;
}

qboolean create_key( const board_t board, const int line, const int col, const float key_pos_y, const vec2_t keySize, qboolean needIntersection )
{
	vec2_t key_Size;
	vec3_t keyOrigin = { 0.0f, 0.0f, 0.0f};
	const refEntity_t *e = &backEnd.currentEntity->e;

	keys_t keyCode = get_key_code( line, col, board.type );
	float w = width_from_kbCode( keyCode );

	float key_width = (keySize[0] * w) + (board.style_keySpace * (w - 1)); // some key have different size (ex: space key)

	float key_pos_x = getKey_x( line, col, keySize[0], key_width, board );

	// Calculate key middle point x & y
	VectorMA( keyOrigin, key_pos_x, e->axis[1], keyOrigin );
	VectorMA( keyOrigin, key_pos_y, e->axis[2], keyOrigin );

	//====================================
	// Draw the key frame
	//====================================
	key_Size[0] = key_width; // real key width from config
	key_Size[1] = keySize[1]; // all keys have same height
	qboolean hasFocus = create_outline_frame( board, keyOrigin, key_Size, keyCode, needIntersection );

	//====================================
	// Draw letter(s) or texture centered
	//====================================
	paintTextureQuad(board, keyOrigin, keyCode);

	return hasFocus;
}

static qboolean trigger_event( void )
{
	static qboolean clic;

	if (vr_info.controllers[SideRIGHT].axisButtons & VR_TOUCH_AXIS_TRIGGER_INDEX && !clic) {
		clic = qtrue;
	}

	if (!(vr_info.controllers[SideRIGHT].axisButtons & VR_TOUCH_AXIS_TRIGGER_INDEX) && clic) {
		clic = qfalse;
		return qtrue;
	}

	return qfalse;
}

#ifndef Com_QueueEvent
#define Com_QueueEvent Sys_QueEvent
#endif

void VKeyboard_checkEvent( void )
{
	if ( vr_controller_type->integer == 1 ) {
		if ( !trigger_event() )
			return;
	}

	// dont detect keyboard frame
	if ( event_keyFocus == vk_NULL)
		return;

	in_eventTime = Sys_Milliseconds( );

	//==========================================
	// ToolBar event
	//==========================================
	switch( event_keyFocus )
	{
	//==========================================
	// Keyboard event
	//==========================================
		case vk_ENTER:
			Com_QueueEvent( in_eventTime, SE_CHAR, K_ENTER, qfalse, 0, NULL );
			return;
		case vk_BACKSPACE:
			Com_QueueEvent( in_eventTime, SE_CHAR, CTRL('h'), qfalse, 0, NULL ); // K_BACKSPACE doesn't works
			return;
		case vk_CAPSLOCK:
			VkeyboardState = (VkeyboardState == State_LowerCase) ? State_UpperCase : State_LowerCase;
			return;
		case vk_COLOR:
			VkeyboardState = State_Color;
			return;
		case vk_HIDE_KB:
			Cvar_Set("showVirtualKeyboard", "0" );
			return;
		case vk_SYMBOL:
			VkeyboardState = (VkeyboardState == State_LowerCase) ? State_Symbol : State_LowerCase;
			return;
		default:
		{
			char * str = char_From_kbCode( event_keyFocus );
			int ascii_key = Key_StringToKeynum( str );
			// TODO use ascii code with enum correspondence
			// Key_StringToKeynum() doesn't return capital letter ASCII code
			/*if (VkeyboardState == State_UpperCase)
				ascii_key -= 32;*/
			Com_QueueEvent( in_eventTime, SE_CHAR, ascii_key, qfalse, 0, NULL );
			return;
		}
	}

}

void createBoard( board_t board )
{
	vec3_t origin = { 0.0f, 0.0f, 0.0f };
	vec2_t keySize;
	const refEntity_t *e = &backEnd.currentEntity->e;

	float key_pos_y;
	int col, line;
	qboolean needEvent;
	qboolean keyFocused = qfalse;
	event_keyFocus = vk_NULL; // dont need intersection when vk_NULL.

	if (vr_info.ray_focused == board.type) {
		vr_info.ray_focused = V_NULL;
		vr_info.mouse_z = 0;
	}

	// Move in front of player
	VectorCopy(e->origin, board.origin);

	// put keyboard below menu
	VectorMA( board.origin, -board.h_below_menu, e->axis[2], board.origin );

	// keyboard distance
	VectorMA( board.origin, board.distance, e->axis[0], board.origin );

	// face the player
	//board.angle_deg_YAW = AngleNormalize360( 270.0f + e->rotation);
	board.angle_deg_YAW = AngleNormalize360( -180.0f + 90.0f + e->rotation);

	// create quaternion from axis and angle for a z rotate
	board.pitchRotation.s = cos(DEG2RAD(board.pitch)/2.0);
	VectorScale(e->axis[1], sin(DEG2RAD(board.pitch)/2.0), board.pitchRotation.v);

	//=============================================
	// Create the keyboard frame
	//=============================================
	keySize[0] = board.width;
	keySize[1] = board.height;

	// look for intersection on the larger frame (th efull keyboard)
	// to avoid useless calculation with each buttons inside.
	qboolean needIntersection = create_outline_frame( board, origin, keySize, vk_NULL, qtrue );

	//=============================================
	//  Create all the keys
	//=============================================
	// we need the larger number of button in the array, to calculate the keySize and key space
	int maxbuttons = 0;
	for (line = 0; line < board.lines; line++)
	{
		if (board.buttonsInLine[line] > maxbuttons)
			maxbuttons = board.buttonsInLine[line];
	}

	float totalSpaces_w = board.style_keySpace * maxbuttons + 1;
	keySize[0] = ( board.width - totalSpaces_w ) / maxbuttons;

	float totalSpaces_H = board.style_keySpace * board.lines + 1;
	keySize[1] = ( board.height - totalSpaces_H ) / board.lines;
	float first_key_pos_y = board.height/2 - keySize[1]/2 - board.style_keySpace/2 - board.style_keySpace;

	for (line = 0; line < board.lines; line++)
	{
		key_pos_y = first_key_pos_y - (keySize[1] + board.style_keySpace) * line;
		int nbButtons = board.buttonsInLine[line];
		for (col = 0; col < nbButtons; col++)
		{
			if (keyFocused) {
				// if a previous key was already focused, stop detection
				//needIntersection = qfalse;  // FIXME: does not work
				needEvent = qtrue;
			}
			keyFocused = create_key( board, line, col, key_pos_y, keySize, needIntersection );
		}
	}

	if (needEvent) {
		VKeyboard_checkEvent();
	}
}

//==========================================
//	1   2   3   4   5   6   7   8   9   0
//  q   w   e   r   t   y   u   i   o   p
//  Smb a   s   d   f   g   h   j   k   l
//  Cap   z   x   c   v   b   n   m   Bk
//  Hid  ^         Sp         .   Enter
//==========================================
void RB_Surface_VR_keyboard( void )
{
	 createBoard( keysBoard );
}
