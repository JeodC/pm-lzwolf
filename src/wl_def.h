#ifndef WL_DEF_H
#define WL_DEF_H

#include <assert.h>
#include <fcntl.h>
#include <math.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#if defined(_arch_dreamcast)
#	include <kos.h>
#elif !defined(_WIN32)
#	include <stdint.h>
#	include <string.h>
#	include <stdarg.h>
#elif defined(__GNUC__)
#	include <stdint.h>
#endif
#include <SDL.h>

#if !defined O_BINARY
#	define O_BINARY 0
#endif

#ifdef _arch_dreamcast
typedef uint8 uint8_t;
typedef uint16 uint16_t;
typedef uint32 uint32_t;
typedef int8 int8_t;
typedef int16 int16_t;
typedef int32 int32_t;
typedef int64 int64_t;
typedef ptr_t uintptr_t;
#endif

#define FRACBITS 16
#define FRACUNIT (1<<FRACBITS)

typedef uint8_t byte;
typedef uint8_t BYTE;
typedef int8_t SBYTE;
typedef uint16_t word;
typedef uint16_t WORD;
typedef int16_t SWORD;
typedef int32_t fixed;
typedef fixed fixed_t;
typedef uint32_t longword;
#ifndef USE_WINDOWS_DWORD
typedef uint32_t DWORD;
#endif
typedef int32_t SDWORD;
typedef uint64_t QWORD;
typedef int64_t SQWORD;
typedef void * memptr;
typedef uint32_t uint32;
typedef uint32_t BITFIELD;
typedef int INTBOOL;

// Screenshot buffer image data types
enum ESSType
{
	SS_PAL,
	SS_RGB,
	SS_BGRA
};

[[noreturn]] void Quit(const char *errorStr, ...);
void NetDPrintf(const char *format, ...);

#define FIXED2FLOAT(fixed) ((double)(fixed)/65536.0)
#define FLOAT2FIXED(x) (fixed_t((x)*FRACUNIT))

#ifdef _WIN32
#define stricmp _stricmp
#endif

typedef double real64;
typedef SDWORD int32;
#include "xs_Float.h"

/*
=============================================================================

							GLOBAL CONSTANTS

=============================================================================
*/

#define MAXPLAYERS		8 // You wish! :P  (This is just here to satisfy ZDoom stuff)
#define BODYQUESIZE		32
#define NUMCOLORMAPS	64

#define TICRATE 70
#define MAXTICS 10
#define DEMOTICS        4

//
// tile constants
//

#define ICONARROWS      90
#define PUSHABLETILE    98
#define EXITTILE        99          // at end of castle
#define AREATILE        107         // first of NUMAREAS floor tiles
#define NUMAREAS        37
#define ELEVATORTILE    21
#define AMBUSHTILE      106
#define ALTELEVATORTILE 107

#define NUMBERCHARS     9


//----------------

#define EXTRAPOINTS     40000

#define PLAYERSPEED     3000
#define RUNSPEED        6000

#define SCREENSEG       0xa000

#define SCREENBWIDE     80

#define HEIGHTRATIO     0.50            // also defined in id_mm.c

#define FLASHCOLOR      5
#define FLASHTICS       4

#undef M_PI
#define PI              3.141592657
#define M_PI PI

#define GLOBAL1         (1l<<16)
#define TILEGLOBAL      GLOBAL1
#define TILESHIFT       16l
#define UNSIGNEDSHIFT   8

#define ANGLETOFINESHIFT 19
#define FINEANGLES      8192
#define FINEMASK        (FINEANGLES-1)
#define ANG90           (FINEANGLES/4)
#define ANG180          (ANG90*2)
#define ANG270          (ANG90*3)
#define ANG360          (ANG90*4)
#define ANGLE_45		(0x20000000u)
#define ANGLE_90		(ANGLE_45*2)
#define ANGLE_180		(ANGLE_45*4)
#define ANGLE_270		(ANGLE_45*6)
#define ANGLE_360		(ANGLE_45*8)
#define ANGLE_1			(ANGLE_45/45)
#define ANGLE_60		(ANGLE_180/3)
#define ANGLE_NEG(x)	(static_cast<angle_t>(0xFFFFFFFFu-x+1u))
typedef uint32_t angle_t;

#define TEXTURESHIFT    6
#define TEXTURESIZE     (1<<TEXTURESHIFT)
#define TEXTUREFROMFIXEDSHIFT 4
#define TEXTUREMASK     (TEXTURESIZE*(TEXTURESIZE-1))

#define NORTH   0
#define EAST    1
#define SOUTH   2
#define WEST    3

#define SCREENSIZE      (SCREENBWIDE*208)
#define PAGE1START      0
#define PAGE2START      (SCREENSIZE)
#define PAGE3START      (SCREENSIZE*2u)
#define FREESTART       (SCREENSIZE*3u)


#define PIXRADIUS       512

#define STARTAMMO       8


// object flag values

enum ActorFlag
{
	FL_NONE             = 0x00000000,
	FL_SHOOTABLE        = 0x00000001,
	FL_DRAWRELATIVE     = 0x00000002,
	FL_VISABLE          = 0x00000008,
	FL_ATTACKMODE       = 0x00000010,
	FL_FIRSTATTACK      = 0x00000020,
	FL_AMBUSH           = 0x00000040,
	FL_STATUSBAR        = 0x00000080,
	FL_BRIGHT           = 0x00000100,

	FL_ISMONSTER        = 0x00001000,
	FL_CANUSEWALLS		= 0x00002000,
	FL_COUNTKILL		= 0x00004000,
	FL_SOLID			= 0x00008000,
	FL_PATHING			= 0x00010000,
	FL_PICKUP			= 0x00020000,
	FL_MISSILE			= 0x00040000,
	FL_COUNTITEM		= 0x00080000,
	FL_COUNTSECRET		= 0x00100000,
	FL_DROPBASEDONTARGET= 0x00200000,
	FL_REQUIREKEYS		= 0x00400000,
	FL_ALWAYSFAST		= 0x00800000,
	FL_RANDOMIZE		= 0x01000000,
	FL_RIPPER			= 0x02000000,
	FL_DONTRIP			= 0x04000000,
	FL_OLDRANDOMCHASE	= 0x08000000,
	FL_PLOTONAUTOMAP	= 0x10000000,
	FL_BILLBOARD        = 0x20000000,
};

enum ExtraActorFlag
{
	FL_ENEMYSOLID       = 0x00000001,
	FL_FRIENDLY         = 0x00000002,
	FL_RANDOMTURN       = 0x00000004,
	FL_BLAKEPATROL      = 0x00000008,
	FL_MUSTATTACK       = 0x00000010,
	FL_PROJHITENEMY     = 0x00000020,
	FL_IGNOREENEMYSOLID = 0x00000040,
	FL_TRYTURN180       = 0x00000080,
};

enum ItemFlag
{
	IF_AUTOACTIVATE		= 0x00000001,
	IF_INVBAR			= 0x00000002,
	IF_ALWAYSPICKUP		= 0x00000004,
	IF_INACTIVE			= 0x00000008, // For picked up items that remain on the map
	IF_IGNORESKILL		= 1<<14,	// Ignores any skill related multiplicators when giving this item.
};

enum WeaponFlag
{
	WF_NOGRIN			= 0x00000001,
	WF_NOAUTOFIRE		= 0x00000002,
	WF_DONTBOB			= 0x00000004,
	WF_ALWAYSGRIN		= 0x00000008,
	WF_NOALERT			= 0x00000010,
};

/*
=============================================================================

							GLOBAL TYPES

=============================================================================
*/

typedef enum {
	di_north,
	di_east,
	di_south,
	di_west
} controldir_t;

typedef enum {
	east,
	northeast,
	north,
	northwest,
	west,
	southwest,
	south,
	southeast,
	nodir
} dirtype;

static const int dirdeltax[9] = { 1, 1, 0, -1, -1, -1, 0, 1, 0 };
static const int dirdeltay[9] = { 0, -1, -1, -1, 0, 1, 1, 1, 0 };

//--------------------
//
// thinking actor structure
//
//--------------------

class AActor;

enum Button
{
	bt_nobutton=-1,
	bt_attack=0,
	bt_strafe,
	bt_run,
	bt_use,
	bt_slot0,
	bt_slot1,
	bt_slot2,
	bt_slot3,
	bt_slot4,
	bt_slot5,
	bt_slot6,
	bt_slot7,
	bt_slot8,
	bt_slot9,
	bt_nextweapon,
	bt_prevweapon,
	bt_esc,
	bt_pause,
	bt_strafeleft,
	bt_straferight,
	bt_moveforward,
	bt_movebackward,
	bt_turnleft,
	bt_turnright,
	bt_altattack,
	bt_reload,
	bt_zoom,
	bt_automap,
	bt_showstatusbar,
	bt_toggleconsole,
	NUMBUTTONS,

	// AM buttons
	bt_zoomin = 0,
	bt_zoomout,
	bt_panup,
	bt_pandown,
	bt_panleft,
	bt_panright,
	bt_amtoggleconsole,
	NUMAMBUTTONS
};

struct ControlScheme
{
public:
	enum
	{
		MWheel_Left = 33,
		MWheel_Right = 34,
		MWheel_Down = 35,
		MWheel_Up = 36
	};

	static void	setKeyboard(ControlScheme* scheme, Button button, int value);
	static void setJoystick(ControlScheme* scheme, Button button, int value);
	static void setMouse(ControlScheme* scheme, Button button, int value);

	Button		button;
	const char*	name;
	int			joystick;
	int			keyboard;
	int			mouse;
	int			axis;
	bool		negative;
};

extern ControlScheme controlScheme[];
extern ControlScheme amControlScheme[];
extern ControlScheme &schemeAutomapKey;

enum
{
	gd_baby,
	gd_easy,
	gd_medium,
	gd_hard
};

typedef enum
{
	ex_stillplaying,
	ex_completed,
	ex_died,
	ex_warped,
	ex_resetgame,
	ex_loadedgame,
	ex_abort,
	ex_demodone,
	ex_secretlevel,
	ex_newmap,
	ex_victorious,
	ex_fullconsole,
	ex_demoscreen,
	ex_startup,
} exit_t;

typedef enum
{
	ga_nothing,
	ga_loadlevel,
	ga_newgame,
	ga_newgame2,
	ga_recordgame,
	ga_loadgame,
	ga_loadgamehidecon,
	ga_loadgameplaydemo,
	ga_autoloadgame,
	ga_savegame,
	ga_autosave,
	ga_playdemo,
	ga_completed,
	ga_slideshow,
	ga_worlddone,
	ga_screenshot,
	ga_togglemap,
	ga_fullconsole,
} gameaction_t;

enum
{
	PRINT_LOW,		// pickup messages
	PRINT_MEDIUM,	// death messages
	PRINT_HIGH,		// critical messages
	PRINT_CHAT,		// chat messages
	PRINT_TEAMCHAT	// chat messages from a teammate
};
#define PRINT_LOW				0				// pickup messages
#define PRINT_MEDIUM			1				// death messages
#define PRINT_HIGH				2				// critical messages
#define PRINT_CHAT				3				// chat messages
#define PRINT_TEAMCHAT			4				// chat messages from a teammate
#define PRINT_LOG				5				// only to logfile
#define PRINT_BOLD				200				// What Printf_Bold used

/*
=============================================================================

							MISC DEFINITIONS

=============================================================================
*/

void atterm(void (*func)(void));

extern const struct RatioInformation
{
	int baseWidth;
	int baseHeight;
	int viewGlobal;
	fixed tallscreen;
	int multiplier;
	bool isWide;
} AspectCorrection[];
#define CorrectWidthFactor(x)	((x)*AspectCorrection[r_ratio].multiplier/48)
#define CorrectHeightFactor(x)	((x)*48/AspectCorrection[r_ratio].multiplier)

static inline fixed FixedMul(fixed a, fixed b)
{
	return (fixed)(((int64_t)a * b + 0x8000) >> 16);
}

static inline fixed FixedDiv(fixed a, fixed b)
{
	return (fixed)(((((int64_t)a)<<32) / b) >> 16);
}

#define GetTicks() ((SDL_GetTicks()*7)/100)

#define CHECKMALLOCRESULT(x) if(!(x)) Quit("Out of memory at %s:%i", __FILE__, __LINE__)

#ifndef _WIN32
	static inline char* itoa(int value, char* string, int radix)
	{
		sprintf(string, "%d", value);
		return string;
	}

	static inline char* ltoa(long value, char* string, int radix)
	{
		sprintf(string, "%ld", value);
		return string;
	}
#endif

#define typeoffsetof(type,variable) ((int)(size_t)&((type*)1)->variable - 1)

#define lengthof(x) (sizeof(x) / sizeof(*(x)))
#define endof(x)    ((x) + lengthof(x))

static inline word READWORD(byte *&ptr)
{
	word val = ptr[0] | ptr[1] << 8;
	ptr += 2;
	return val;
}

static inline longword READLONGWORD(byte *&ptr)
{
	longword val = ptr[0] | ptr[1] << 8 | ptr[2] << 16 | ptr[3] << 24;
	ptr += 4;
	return val;
}


/*
=============================================================================

						FEATURE DEFINITIONS

=============================================================================
*/

#ifdef USE_FEATUREFLAGS
	// The currently available feature flags
	#define FF_STARSKY      0x0001
	#define FF_PARALLAXSKY  0x0002
	#define FF_CLOUDSKY     0x0004
	#define FF_RAIN         0x0010
	#define FF_SNOW         0x0020

	// The ffData... variables contain the 16-bit values of the according corners of the current level.
	// The corners are overwritten with adjacent tiles after initialization in SetupGameLevel
	// to avoid interpretation as e.g. doors.
	extern int ffDataTopLeft, ffDataTopRight, ffDataBottomLeft, ffDataBottomRight;

	/*************************************************************
	* Current usage of ffData... variables:
	* ffDataTopLeft:     lower 8-bit: ShadeDefID
	* ffDataTopRight:    FeatureFlags
	* ffDataBottomLeft:  CloudSkyDefID or ParallaxStartTexture
	* ffDataBottomRight: unused
	*************************************************************/

	// The feature flags are stored as a wall in the upper right corner of each level
	static inline word GetFeatureFlags()
	{
		return ffDataTopRight;
	}

#endif

	void DrawParallax(byte *vbuf, unsigned vbufPitch);

#ifdef USE_DIR3DSPR
	void Scale3DShape(byte *vbuf, unsigned vbufPitch, statobj_t *ob);
#endif

#ifdef USE_GPL

/*
=============================================================================

						FEATURE DEFINITIONS

=============================================================================
*/

namespace bibendovsky
{

class AssetsInfo
{
public:
	bool is_ps() const
	{
		return false;
	}

	bool is_aog() const
	{
		return true;
	}

	int level() const noexcept;

	int get_levels_per_episode() const
	{
		return 15;
	}

	int get_barrier_switches_per_level() const noexcept
	{
		return 5;
	}

	int get_max_barrier_switches_per_level_bits() const noexcept
	{
		return 3;
	}

	bool is_secret_level(
		const int level_number) const
	{
		return level_number <= 0 || level_number >= 10;
	}

	int secret_floor_get_index(
		const int level_number) const
	{
		if (level_number <= 0)
		{
			return 0;
		}

		if (level_number >= 10)
		{
			return level_number - 10 + 1;
		}

		return -1;
	}
}; // AssetsInfo

} // namespace bibendovsky

#endif

#endif
