// WL_DRAW.C

#include <algorithm>
#include "wl_def.h"
#include "id_sd.h"
#include "id_in.h"
#include "id_vl.h"
#include "id_vh.h"
#include "id_us.h"
#include "textures/textures.h"
#include "c_cvars.h"
#include "r_sprites.h"
#include "r_data/colormaps.h"

#include "wl_cloudsky.h"
#include "wl_atmos.h"
#include "wl_shade.h"
#include "actor.h"
#include "id_ca.h"
#include "gamemap.h"
#include "mapedit.h"
#include "g_mapinfo.h"
#include "lumpremap.h"
#include "wl_agent.h"
#include "wl_draw.h"
#include "wl_game.h"
#include "wl_play.h"
#include "wl_state.h"
#include "a_inventory.h"
#include "thingdef/thingdef.h"
#include "c_console.h"
#include "wl_menu.h"
#include "lw_vec.h"

/*
=============================================================================

							LOCAL CONSTANTS

=============================================================================
*/

#define MINDIST         (0x4000l)

#define mapheight (map->GetHeader().height)
#define mapwidth (map->GetHeader().width)
#define maparea (mapheight*mapwidth)

/*
=============================================================================

							GLOBAL VARIABLES

=============================================================================
*/

namespace Shading
{
	void PopulateHalos (void);

	int LightForIntercept (fixed xintercept, fixed yintercept, const ClassDef* &littype);

	const BYTE *GetCMapStart (const ClassDef *littype);

	bool GetFullBrightInhibit (const ClassDef *littype);
}

void DrawFloorAndCeiling(byte *vbuf, unsigned vbufPitch, TWallHeight min_wallheight);

const RatioInformation AspectCorrection[] =
{
	/* UNC */    {960,  600, 0x10000, 0,                    48,         false},
	/* 16:9 */   {1280, 450, 0x15555, 0,                    48*3/4,     true},
	/* 16:10 */  {1152, 500, 0x13333, 0,                    48*5/6,     true},
	/* 17:10 */  {1224, 471, 0x14666, 0,                    48*40/51,   true},
	/* 4:3 */    {960,  600, 0x10000, 0,                    48,         false},
	/* 5:4 */    {960,  640, 0x10000, (fixed) 6.5*FRACUNIT, 48*15/16,   false},
	/* 64:27 */  {1720, 346, 0x1C71C, 0,                    48*173/300, true},
};

/*static*/ byte *vbuf = NULL;
unsigned vbufPitch = 0;

int32_t	lasttimecount;
int32_t	frameon;
int32_t	moveobj_frameon;
int32_t	projectile_frameon;
bool	fpscounter;
int		r_extralight;

int fps_frames=0, fps_time=0, fps=0;

TUniquePtr<TWallHeight[]> wallheight;
TWallHeight min_wallheight;

TUniquePtr<TWallHeight[]> skywallheight;

//
// math tables
//
TUniquePtr<short[]> pixelangle;
fixed finetangent[FINEANGLES/2 + ANG180];
fixed finesine[FINEANGLES+FINEANGLES/4];
fixed *finecosine = finesine+ANG90;

//
// refresh variables
//
fixed   viewx,viewy;                    // the focal point
angle_t viewangle;
fixed   viewsin,viewcos;
int viewshift = 0;
fixed viewz = 32;
fixed viewcamz[2] = {0,0};

fixed gLevelVisibility = VISIBILITY_DEFAULT;
fixed gLevelMaxLightVis = MAXLIGHTVIS_DEFAULT;
int gLevelLight = LIGHTLEVEL_DEFAULT;

void    TransformActor (AActor *ob);
void    BuildTables (void);
void    ClearScreen (void);
unsigned int CalcRotate (AActor *ob);
void    DrawScaleds (void);
void    CalcTics (void);
void    ThreeDRefresh (void);



//
// wall optimization variables
//
int     lastside;               // true for vertical
int32_t    lastintercept;
MapSpot lasttilehit;
int     lasttexture;

//
// ray tracing variables
//
short    focaltx,focalty,viewtx,viewty;
longword xpartialup,xpartialdown,ypartialup,ypartialdown;

short   midangle;
short   angle;

MapTile::Side hitdir;
MapSpot tilehit;
int     pixx;

short   xtile,ytile;
short   xtilestep,ytilestep;
int32_t    xintercept,yintercept;
word    xstep,ystep;
int     texdelta;
int		texheight;

#define TEXTUREBASE 0x4000000
fixed	texxscale = FRACUNIT;
fixed	texyscale = FRACUNIT;


/*
============================================================================

						3 - D  DEFINITIONS

============================================================================
*/

/*
========================
=
= TransformActor
=
= Takes paramaters:
=   gx,gy               : globalx/globaly of point
=
= globals:
=   viewx,viewy         : point of view
=   viewcos,viewsin     : sin/cos of viewangle
=   scale               : conversion from global value to screen value
=
= sets:
=   screenx,transx,transy,screenheight: projected edge location and size
=
========================
*/


//
// transform actor
//
void TransformActor (AActor *ob)
{
	fixed gx,gy,gxt,gyt,nx,ny;
	const bool rel = (ob->flags & FL_DRAWRELATIVE) != 0;

//
// translate point to view centered coordinates
//
	gx = ob->x-(rel?0:viewx);
	gy = ob->y-(rel?0:viewy);

//
// calculate newx
//
	gxt = FixedMul(gx,(rel?TILEGLOBAL:viewcos));
	gyt = FixedMul(gy,(rel?0:viewsin));
	// Wolf4SDL used 0x2000 for statics and 0x4000 for moving actors, but since
	// we no longer tell the difference, use the smaller fudging value since
	// the larger one will just look ugly in general.
	nx = gxt-gyt-0x2000;

//
// calculate newy
//
	gxt = FixedMul(gx,(rel?0:viewsin));
	gyt = FixedMul(gy,(rel?TILEGLOBAL:viewcos));
	ny = gyt+gxt;

//
// calculate perspective ratio
//
	ob->transx = nx;
	ob->transy = ny;

	if (nx<MINDIST)                 // too close, don't overflow the divide
	{
		ob->viewheight = 0;
		return;
	}

	ob->viewx = (word)(centerx + ny*scale/nx);

//
// calculate height (heightnumerator/(nx>>8))
//
	ob->viewheight = (word)((heightnumerator<<8)/nx);

//
// reverse translate point to view centered coordinates
//
	if (rel)
	{
		nx += 0x2000;
		ob->absx = viewx + FixedMul(nx,viewcos) - FixedMul(nx,viewsin);
		ob->absy = viewy + FixedMul(ny,viewsin) + FixedMul(ny,viewcos);
	}
	else
	{
		ob->absx = ob->x;
		ob->absy = ob->y;
	}
}

//==========================================================================

/*
====================
=
= CalcHeight
=
= Calculates the height of xintercept,yintercept from viewx,viewy
=
====================
*/

TWallHeight CalcHeight()
{
	fixed z = FixedMul(xintercept - viewx, viewcos)
		- FixedMul(yintercept - viewy, viewsin);
	if(z < MINDIST) z = MINDIST;
	TWallHeight height;
	height[0] = (heightnumerator << 8) / z;
	if(height[0] < min_wallheight[0]) min_wallheight[0] = height[0];
	for(int i = 1; i < 3; i++)
	{
		const int bot = ((i-1)*2 - 1);
		height[i] = WallMidY (height[0], bot);
		if(height[i] < min_wallheight[i]) min_wallheight[i] = height[i];
	}
	return height;
}

//==========================================================================

/*
===================
=
= ScalePost
=
===================
*/

const byte *postsource;
int postx;
int32_t postshadex, postshadey;
bool postbright, postdecal, poststarsky;
byte postdecalcolor = 187; // slade will not set the right color for this; its annoying but its true
bool postneedprec = false;

struct StandardScalePost
{
	static inline void WritePix(int yendoffs, byte col)
	{
		vbuf[yendoffs] = col;
	}

	static inline byte ReadColor(const byte *curshades, int yw)
	{
		return curshades[postsource[yw]];
	}
};

struct DecalScalePost
{
	static inline void WritePix(int yendoffs, byte col)
	{
		if(col != postdecalcolor)
			vbuf[yendoffs] = col;
	}

	static inline byte ReadColor(const byte *curshades, int yw)
	{
		auto col = postsource[yw];
		return (col == postdecalcolor ? postdecalcolor : curshades[col]);
	}
};

template<typename Algo>
void RunScalePost()
{
	if(postsource == NULL)
		return;

	int ywcount, yoffs, yw, yd, yendoffs;
	byte col;
	fixed halftexheight;
	int wraptexcoord, truncateprec;

	if(postneedprec) // stacked tiles
	{
		wraptexcoord = (texyscale>>2)-1;
		truncateprec = 0;
	}
	else // single tile (vanilla case)
	{
		wraptexcoord = 0;
		truncateprec = 3;
		// texyscale = 256 = 32*8 = (TEXTURESIZE/2)*8
		// halftexheight works out to (TEXTURESIZE/2) as in vanilla
	}
	halftexheight = texyscale>>truncateprec;

	const ClassDef *littype = NULL;
	const int shade = LIGHT2SHADE(gLevelLight + r_extralight + Shading::LightForIntercept (postshadex, postshadey, littype));
	const int tz = FixedMul(r_depthvisibility<<8, wallheight[postx][0]);
	const BYTE *curshades;
	const BYTE *cmapstart = Shading::GetCMapStart (littype);
	if(postbright && !Shading::GetFullBrightInhibit(littype))
		curshades = cmapstart;
	else
		curshades = &cmapstart[GETPALOOKUP(MAX(tz, MINZ), shade)<<8];

	ywcount = yd = wallheight[postx][0]>>truncateprec;
	if(yd <= 0)
		yd = 100;

	// Calculate starting and ending offsets
	{
		int ywcount = wallheight[postx][1]>>3;
		int midy = (viewheight / 2) - ywcount;

		yoffs = midy * vbufPitch;
		if(yoffs < 0) yoffs = 0;
		yoffs += postx;

		ywcount = wallheight[postx][2]>>3;
		yendoffs = (viewheight / 2) + ywcount;
		yw=(texyscale>>2)-1;
	}

	while(yendoffs >= viewheight)
	{
		ywcount -= halftexheight;
		while(ywcount <= 0)
		{
			ywcount += yd;
			yw--;
		}
		yendoffs--;
	}
	if(yw < 0)
		yw = (texyscale>>2) - ((-yw) % (texyscale>>2));

	col = Algo::ReadColor(curshades, yw);
	yendoffs = yendoffs * vbufPitch + postx;
	while(yoffs <= yendoffs)
	{
		Algo::WritePix(yendoffs, col);
		ywcount -= halftexheight;
		if(ywcount <= 0)
		{
			do
			{
				ywcount += yd;
				yw--;
			}
			while(ywcount <= 0);
			if(yw < 0) yw = wraptexcoord;
			col = Algo::ReadColor(curshades, yw);
		}
		yendoffs -= vbufPitch;
	}
}

void ScalePost()
{
	if(poststarsky)
	{
		// draw nothing when showsky is enabled and starsky is enabled
	}
	else if(postdecal)
	{
		RunScalePost<DecalScalePost>();
	}
	else
	{
		RunScalePost<StandardScalePost>();
	}
}

/*
===================
=
= Camz
=
===================
*/

inline fixed Camz (fixed height, int bot)
{
	unsigned int depth = map->GetPlane(0).depth;
	if(bot < 0 && depth > 64)
		height -= (fixed(depth-64)<<FRACBITS);
	fixed camz = (height / 64) - (TILEGLOBAL / 2);
	return camz;
}


/*
===================
=
= WallMidY
=
===================
*/

int WallMidY (int ywcount, int bot)
{
	const fixed camz = viewcamz[(bot+1)>>1];
	return ((TILEGLOBAL + (bot * camz * 2)) * ywcount)>>FRACBITS;
}


/*
===================
=
= InvWallMidY
=
===================
*/

int InvWallMidY(int y, int bot)
{
	const fixed camz = viewcamz[(bot+1)>>1];
	return ((FixedDiv((y<<FRACBITS), TILEGLOBAL + (bot * camz * 2)))>>FRACBITS) + 1;
}


void GlobalScalePost(byte *vidbuf, unsigned pitch)
{
	vbuf = vidbuf;
	vbufPitch = pitch;
	ScalePost();
}

static void DetermineHitDir(bool vertical)
{
	if(vertical)
	{
		if(xtilestep==-1 && (xintercept>>16)<=xtile)
			hitdir = MapTile::East;
		else
			hitdir = MapTile::West;
	}
	else
	{
		if(ytilestep==-1 && (yintercept>>16)<=ytile)
			hitdir = MapTile::South;
		else
			hitdir = MapTile::North;
	}
}

static int SlideTextureOffset(unsigned int style, int intercept, int amount)
{
	if(!amount)
		return 0;

	switch(style)
	{
		default:
			return -amount;
		case SLIDE_Split:
			if(intercept < FRACUNIT/2)
				return amount/2;
			return -amount/2;
		case SLIDE_Invert:
			return amount;
	}
}

/*
====================
=
= HitVertWall
=
= tilehit bit 7 is 0, because it's not a door tile
= if bit 6 is 1 and the adjacent tile is a door tile, use door side pic
=
====================
*/

void HitVertWall (void)
{
	if(!tilehit)
		return;

	int texture;

	DetermineHitDir(true);

	tilehit->amFlags |= AM_Visible;
	texture = (yintercept+texdelta+SlideTextureOffset(tilehit->slideStyle, (word)yintercept, tilehit->slideAmount[hitdir]))&(FRACUNIT-1);
	if (xtilestep == -1 && !tilehit->tile->offsetVertical)
	{
		texture = (FRACUNIT - texture)&(FRACUNIT-1);
		xintercept += TILEGLOBAL;
	}
	if(tilehit->tile->textureFlip)
		texture = (FRACUNIT - 1 - texture);

	// nudge for zone lighting
	postshadex = xintercept-(int32_t)xtilestep;
	postshadey = yintercept;

	if(lastside==1 && lastintercept==xtile && lasttilehit==tilehit && !(lasttilehit->tile->offsetVertical))
	{
		texture -= texture%texxscale;

		ScalePost();
		wallheight[pixx] = CalcHeight();
		skywallheight[pixx] = (tilehit->tile->showSky ? TWallHeight{} : wallheight[pixx]);
		if(postsource)
			postsource+=(texture-lasttexture)*texheight/texxscale;
		postbright = tilehit->tile->bright;
		postdecal = tilehit->tile->decal;
		poststarsky = (tilehit->tile->showSky && levelInfo->StarSkyEnabled());
		postx=pixx;
		lasttexture=texture;
		return;
	}

	if(lastside!=-1) ScalePost();

	lastside=1;
	lastintercept=xtile;
	lasttilehit=tilehit;
	wallheight[pixx] = CalcHeight();
	skywallheight[pixx] = (tilehit->tile->showSky ? TWallHeight{} : wallheight[pixx]);
	postbright = tilehit->tile->bright;
	postdecal = tilehit->tile->decal;
	poststarsky = (tilehit->tile->showSky && levelInfo->StarSkyEnabled());
	postx = pixx;
	FTexture *source = NULL;

	MapSpot adj = tilehit->GetAdjacent(hitdir);
	if (adj && adj->tile && adj->tile->offsetHorizontal && !adj->tile->offsetVertical) // check for adjacent doors
		source = TexMan(adj->texture[hitdir]);
	else
		source = TexMan(tilehit->texture[hitdir]);

	if(source)
	{
		texheight = source->GetHeight();
		texxscale = TEXTUREBASE/source->xScale;
		texyscale = source->yScale>>(FRACBITS-8);
		texture -= texture%texxscale;

		postsource = source->GetColumn(texture/texxscale, NULL);
	}
	else
		postsource = NULL;

	lasttexture=texture;
}


/*
====================
=
= HitHorizWall
=
= tilehit bit 7 is 0, because it's not a door tile
= if bit 6 is 1 and the adjacent tile is a door tile, use door side pic
=
====================
*/

void HitHorizWall (void)
{
	if(!tilehit)
		return;

	int texture;

	DetermineHitDir(false);

	tilehit->amFlags |= AM_Visible;
	texture = (xintercept+texdelta+SlideTextureOffset(tilehit->slideStyle, (word)xintercept, tilehit->slideAmount[hitdir]))&(FRACUNIT-1);
	if(!tilehit->tile->offsetHorizontal)
	{
		if (ytilestep == -1)
			yintercept += TILEGLOBAL;
		else
			texture = (FRACUNIT - texture)&(FRACUNIT-1);
	}
	if(tilehit->tile->textureFlip)
		texture = (FRACUNIT - 1 - texture);

	// nudge for zone lighting
	postshadex = xintercept;
	postshadey = yintercept-(int32_t)ytilestep;

	if(lastside==0 && lastintercept==ytile && lasttilehit==tilehit && !(lasttilehit->tile->offsetHorizontal))
	{
		texture -= texture%texxscale;

		ScalePost();
		wallheight[pixx] = CalcHeight();
		skywallheight[pixx] = (tilehit->tile->showSky ? TWallHeight{} : wallheight[pixx]);
		if(postsource)
			postsource+=(texture-lasttexture)*texheight/texxscale;
		postbright = tilehit->tile->bright;
		postdecal = tilehit->tile->decal;
		poststarsky = (tilehit->tile->showSky && levelInfo->StarSkyEnabled());
		postx=pixx;
		lasttexture=texture;
		return;
	}

	if(lastside!=-1) ScalePost();

	lastside=0;
	lastintercept=ytile;
	lasttilehit=tilehit;
	wallheight[pixx] = CalcHeight();
	skywallheight[pixx] = (tilehit->tile->showSky ? TWallHeight{} : wallheight[pixx]);
	postbright = tilehit->tile->bright;
	postdecal = tilehit->tile->decal;
	poststarsky = (tilehit->tile->showSky && levelInfo->StarSkyEnabled());
	postx = pixx;
	FTexture *source = NULL;

	MapSpot adj = tilehit->GetAdjacent(hitdir);
	if (adj && adj->tile && adj->tile->offsetVertical && !adj->tile->offsetHorizontal) // check for adjacent doors
		source = TexMan(adj->texture[hitdir]);
	else
		source = TexMan(tilehit->texture[hitdir]);

	if(source)
	{
		texheight = source->GetHeight();
		texxscale = TEXTUREBASE/source->xScale;
		texyscale = source->yScale>>(FRACBITS-8);
		texture -= texture%texxscale;

		postsource = source->GetColumn(texture/texxscale, NULL);
	}
	else
		postsource = NULL;

	lasttexture=texture;
}

//==========================================================================

#define HitHorizBorder HitHorizWall
#define HitVertBorder HitVertWall

//==========================================================================

/*
=====================
=
= CalcRotate
=
=====================
*/

unsigned int CalcRotate (AActor *ob)
{
	angle_t angle, viewangle;

	// this isn't exactly correct, as it should vary by a trig value,
	// but it is close enough with only eight rotations

	viewangle = players[ConsolePlayer].camera->angle + (centerx - ob->viewx)/8;

	angle = viewangle - ob->angle;

	if(ob->TwoSidedRotate.first)
	{
		const auto x = players[ConsolePlayer].camera->x;
		const auto y = players[ConsolePlayer].camera->y;
		const auto v = lwlib::vec2f(FIXED2FLOAT(x - ob->x),
				FIXED2FLOAT(y - ob->y));
		const auto obj_n = lwlib::vec2f(FIXED2FLOAT(finecosine[ob->angle>>ANGLETOFINESHIFT]),
				-FIXED2FLOAT(finesine[ob->angle>>ANGLETOFINESHIFT]));
		const auto prod = vec_dot(v, obj_n);
		return (ob->TwoSidedRotate.second * 2) + (prod > 0 ? 0 : 1);
	}

	angle+= ANGLE_180 + ANGLE_45/2;

	return angle/ANGLE_45;
}

/*
=====================
=
= DrawScaleds
=
= Draws all objects that are visable
=
=====================
*/

#define MAXVISABLE 500

typedef struct
{
	AActor *actor;
	short viewheight;
	//short      viewx,
	//		viewheight,
	//		shapenum;
	//short      flags;          // this must be changed to uint32_t, when you
							// you need more than 16-flags for drawing
} visobj_t;

visobj_t vislist[MAXVISABLE];
visobj_t *visptr,*visstep,*farthest;

void DrawScaleds (void)
{
	int      i,least,numvisable,height;

	visptr = &vislist[0];

//
// place active objects
//
	for(AActor::Iterator iter = AActor::GetIterator();iter.Next();)
	{
		AActor *obj = iter;

		if (obj->sprite == SPR_NONE || (obj->flags & FL_STATUSBAR))
			continue;

		if (obj->flags & FL_DRAWRELATIVE)
		{
			TransformActor (obj);
			if (!obj->viewheight)
				continue;                                               // too close or far away

			visptr->actor = obj;
			visptr->viewheight = obj->viewheight;

			if (visptr < &vislist[MAXVISABLE-1])    // don't let it overflow
				visptr++;

			obj->flags |= FL_VISABLE;
			continue;
		}

		MapSpot spot = map->GetSpot(obj->tilex, obj->tiley, 0);
		MapSpot spots[8];
		spots[0] = spot->GetAdjacent(MapTile::East);
		spots[1] = spots[0] ? spots[0]->GetAdjacent(MapTile::North) : NULL;
		spots[2] = spot->GetAdjacent(MapTile::North);
		spots[3] = spots[2] ? spots[2]->GetAdjacent(MapTile::West) : NULL;
		spots[4] = spot->GetAdjacent(MapTile::West);
		spots[5] = spots[4] ? spots[4]->GetAdjacent(MapTile::South) : NULL;
		spots[6] = spot->GetAdjacent(MapTile::South);
		spots[7] = spots[6] ? spots[6]->GetAdjacent(MapTile::East) : NULL;

		//
		// could be in any of the nine surrounding tiles
		//
		if (spot->visible
			|| ( spots[0] && (spots[0]->visible && !spots[0]->tile) )
			|| ( spots[1] && (spots[1]->visible && !spots[1]->tile) )
			|| ( spots[2] && (spots[2]->visible && !spots[2]->tile) )
			|| ( spots[3] && (spots[3]->visible && !spots[3]->tile) )
			|| ( spots[4] && (spots[4]->visible && !spots[4]->tile) )
			|| ( spots[5] && (spots[5]->visible && !spots[5]->tile) )
			|| ( spots[6] && (spots[6]->visible && !spots[6]->tile) )
			|| ( spots[7] && (spots[7]->visible && !spots[7]->tile) ) )
		{
			TransformActor (obj);
			if (!obj->viewheight || (gamestate.victoryflag && obj == players[ConsolePlayer].mo))
				continue;                                               // too close or far away

			visptr->actor = obj;
			visptr->viewheight = obj->viewheight;

			if (visptr < &vislist[MAXVISABLE-1])    // don't let it overflow
				visptr++;

			obj->flags |= FL_VISABLE;
		}
		else
			obj->flags &= ~FL_VISABLE;
	}

//
// draw from back to front
//
	numvisable = (int) (visptr-&vislist[0]);

	if (!numvisable)
		return;                                                                 // no visable objects

	for (i = 0; i<numvisable; i++)
	{
		least = 32000;
		for (visstep=&vislist[0] ; visstep<visptr ; visstep++)
		{
			height = visstep->viewheight;
			if (height < least)
			{
				least = height;
				farthest = visstep;
			}
		}
		//
		// draw farthest
		//
		if(farthest->actor->flags & FL_BILLBOARD)
			Scale3DSprite(farthest->actor, farthest->actor->state, farthest->viewheight);
		else
			ScaleSprite(farthest->actor, farthest->actor->viewx, farthest->actor->state, farthest->viewheight);

		farthest->viewheight = 32000;
	}
}

//==========================================================================

/*
==============
=
= DrawPlayerWeapon
=
= Draw the player's hands
=
==============
*/

void DrawPlayerWeapon (void)
{
	for(unsigned int i = 0;i < player_t::NUM_PSPRITES;++i)
	{
		if(!players[ConsolePlayer].psprite[i].frame)
			return;

		fixed xoffset, yoffset;
		players[ConsolePlayer].BobWeapon(&xoffset, &yoffset);

		R_DrawPlayerSprite(players[ConsolePlayer].ReadyWeapon, players[ConsolePlayer].psprite[i].frame, players[ConsolePlayer].psprite[i].sx+xoffset, players[ConsolePlayer].psprite[i].sy+yoffset);
	}
}

//==========================================================================

void AsmRefresh()
{
	static word xspot[2],yspot[2];
	int32_t xstep=0,ystep=0;
	longword xpartial=0,ypartial=0;
	MapSpot focalspot = map->GetSpot(focaltx, focalty, 0);
	bool playerInPushwallBackTile = focalspot->pushAmount != 0;

	if(gameinfo.walldecalcolor >= 256)
		postdecalcolor = (byte)(gameinfo.walldecalcolor&0xff);

	// need extra precision when drawing stacked tiles
	postneedprec = (map->GetPlane(0).depth > 64);

	for(pixx=0;pixx<viewwidth;pixx++)
	{
		short angl=midangle+pixelangle[pixx];
		if(angl<0) angl+=FINEANGLES;
		if(angl>=ANG360) angl-=FINEANGLES;
		if(angl<ANG90)
		{
			xtilestep=1;
			ytilestep=-1;
			xstep=finetangent[ANG90-1-angl];
			ystep=-finetangent[angl];
			xpartial=xpartialup;
			ypartial=ypartialdown;
		}
		else if(angl<ANG180)
		{
			xtilestep=-1;
			ytilestep=-1;
			xstep=-finetangent[angl-ANG90];
			ystep=-finetangent[ANG180-1-angl];
			xpartial=xpartialdown;
			ypartial=ypartialdown;
		}
		else if(angl<ANG270)
		{
			xtilestep=-1;
			ytilestep=1;
			xstep=-finetangent[ANG270-1-angl];
			ystep=finetangent[angl-ANG180];
			xpartial=xpartialdown;
			ypartial=ypartialup;
		}
		else if(angl<ANG360)
		{
			xtilestep=1;
			ytilestep=1;
			xstep=finetangent[angl-ANG270];
			ystep=finetangent[ANG360-1-angl];
			xpartial=xpartialup;
			ypartial=ypartialup;
		}
		yintercept=FixedMul(ystep,xpartial)+viewy;
		xtile=focaltx+xtilestep;
		xspot[0]=xtile;
		xspot[1]=yintercept>>16;
		xintercept=FixedMul(xstep,ypartial)+viewx;
		ytile=focalty+ytilestep;
		yspot[0]=xintercept>>16;
		yspot[1]=ytile;
		texdelta=0;

		// Special treatment when player is in back tile of pushwall
		if(playerInPushwallBackTile)
		{
			if(focalspot->pushReceptor)
				focalspot = focalspot->pushReceptor;

			if((focalspot->pushDirection == MapTile::East && xtilestep == 1) ||
				(focalspot->pushDirection == MapTile::West && xtilestep == -1))
			{
				int32_t yintbuf = yintercept - ytilestep*(abs(ystep * signed(64 - focalspot->pushAmount)) >> 6);
				if((yintbuf >> 16) == focalty)   // ray hits pushwall back?
				{
					if(focalspot->pushDirection == MapTile::East)
						xintercept = (focaltx << TILESHIFT) + (focalspot->pushAmount << 10);
					else
						xintercept = (focaltx << TILESHIFT) - TILEGLOBAL + ((64 - focalspot->pushAmount) << 10);
					yintercept = yintbuf;
					ytile = (short) (yintercept >> TILESHIFT);
					tilehit = focalspot;
					HitVertWall();
					continue;
				}
			}
			else if((focalspot->pushDirection == MapTile::South && ytilestep == 1) ||
				(focalspot->pushDirection == MapTile::North && ytilestep == -1))
			{
				int32_t xintbuf = xintercept - xtilestep*(abs(xstep * signed(64 - focalspot->pushAmount)) >> 6);
				if((xintbuf >> 16) == focaltx)   // ray hits pushwall back?
				{
					xintercept = xintbuf;
					if(focalspot->pushDirection == MapTile::South)
						yintercept = (focalty << TILESHIFT) + (focalspot->pushAmount << 10);
					else
						yintercept = (focalty << TILESHIFT) - TILEGLOBAL + ((64 - focalspot->pushAmount) << 10);
					xtile = (short) (xintercept >> TILESHIFT);
					tilehit = focalspot;
					HitHorizWall();
					continue;
				}
			}
		}

		do
		{
			if(ytilestep==-1 && (yintercept>>16)<=ytile) goto horizentry;
			if(ytilestep==1 && (yintercept>>16)>=ytile) goto horizentry;
vertentry:

			if((uint32_t)yintercept>mapheight*65536-1 || (word)xtile>=mapwidth)
			{
				if(xtile<0) xintercept=0, xtile=0;
				else if((unsigned)xtile>=mapwidth) xintercept=mapwidth<<TILESHIFT, xtile=mapwidth-1;
				else xtile=(short) (xintercept >> TILESHIFT);
				if(yintercept<0) yintercept=0, ytile=0;
				else if((unsigned)yintercept>=(mapheight<<TILESHIFT)) yintercept=mapheight<<TILESHIFT, ytile=mapheight-1;
				yspot[0]=0xffff;
				tilehit=0;
				HitHorizBorder();
				break;
			}
			if(xspot[0]>=mapwidth || xspot[1]>=mapheight) break;
			tilehit=map->GetSpot(xspot[0], xspot[1], 0);
			if(tilehit && tilehit->tile)
			{
				if(tilehit->tile->offsetVertical)
				{
					DetermineHitDir(true);
					int32_t yintbuf=yintercept+(ystep>>1);
					if((yintbuf>>16)!=(yintercept>>16))
						goto passvert;
					if(CheckSlidePass(tilehit->slideStyle, (word)yintbuf, tilehit->slideAmount[hitdir]))
						goto passvert;
					yintercept=yintbuf;
					xintercept=(xtile<<TILESHIFT)|0x8000;
					ytile = (short) (yintercept >> TILESHIFT);
					HitVertWall();
				}
				else
				{
					bool isPushwall = tilehit->pushAmount != 0 || tilehit->pushReceptor;
					if(tilehit->pushReceptor)
						tilehit = tilehit->pushReceptor;

					if(isPushwall)
					{
						if(tilehit->pushDirection==MapTile::West || tilehit->pushDirection==MapTile::East)
						{
							int32_t yintbuf;
							int pwallposnorm;
							int pwallposinv;
							if(tilehit->pushDirection==MapTile::West)
							{
								pwallposnorm = 64-tilehit->pushAmount;
								pwallposinv = tilehit->pushAmount;
							}
							else
							{
								pwallposnorm = tilehit->pushAmount;
								pwallposinv = 64-tilehit->pushAmount;
							}
							if((tilehit->pushDirection==MapTile::East && xtile==(signed)tilehit->GetX() && ((uint32_t)yintercept>>16)==tilehit->GetY())
								|| (tilehit->pushDirection==MapTile::West && !(xtile==(signed)tilehit->GetX() && ((uint32_t)yintercept>>16)==tilehit->GetY())))
							{
								yintbuf=yintercept+((ystep*pwallposnorm)>>6);
								if((yintbuf>>16)!=(yintercept>>16))
									goto passvert;

								xintercept=(xtile<<TILESHIFT)+TILEGLOBAL-(pwallposinv<<10);
								yintercept=yintbuf;
								ytile = (short) (yintercept >> TILESHIFT);
								HitVertWall();
							}
							else
							{
								yintbuf=yintercept+((ystep*pwallposinv)>>6);
								if((yintbuf>>16)!=(yintercept>>16))
									goto passvert;

								xintercept=(xtile<<TILESHIFT)-(pwallposinv<<10);
								yintercept=yintbuf;
								ytile = (short) (yintercept >> TILESHIFT);
								HitVertWall();
							}
						}
						else
						{
							int pwallposi = tilehit->pushAmount;
							if(tilehit->pushDirection==MapTile::North) pwallposi = 64-tilehit->pushAmount;
							if((tilehit->pushDirection==MapTile::South && (word)yintercept<(pwallposi<<10))
								|| (tilehit->pushDirection==MapTile::North && (word)yintercept>(pwallposi<<10)))
							{
								if(((uint32_t)yintercept>>16)==tilehit->GetY() && xtile==(signed)tilehit->GetX())
								{
									if((tilehit->pushDirection==MapTile::South && (int32_t)((word)yintercept)+ystep<(pwallposi<<10))
										|| (tilehit->pushDirection==MapTile::North && (int32_t)((word)yintercept)+ystep>(pwallposi<<10)))
										goto passvert;

									if(tilehit->pushDirection==MapTile::South)
									{
										yintercept=(yintercept&0xffff0000)+(pwallposi<<10);
										xintercept=xintercept-((xstep*(64-pwallposi))>>6);
									}
									else
									{
										yintercept=(yintercept&0xffff0000)-TILEGLOBAL+(pwallposi<<10);
										xintercept=xintercept-((xstep*pwallposi)>>6);
									}
									xtile = (short) (xintercept >> TILESHIFT);
									HitHorizWall();
								}
								else
								{
									texdelta = -(pwallposi<<10);
									xintercept=xtile<<TILESHIFT;
									ytile = (short) (yintercept >> TILESHIFT);
									HitVertWall();
								}
							}
							else
							{
								if(((uint32_t)yintercept>>16)==tilehit->GetY() && xtile==(signed)tilehit->GetX())
								{
									texdelta = -(pwallposi<<10);
									xintercept=xtile<<TILESHIFT;
									ytile = (short) (yintercept >> TILESHIFT);
									HitVertWall();
								}
								else
								{
									if((tilehit->pushDirection==MapTile::South && (int32_t)((word)yintercept)+ystep>(pwallposi<<10))
										|| (tilehit->pushDirection==MapTile::North && (int32_t)((word)yintercept)+ystep<(pwallposi<<10)))
										goto passvert;

									if(tilehit->pushDirection==MapTile::South)
									{
										yintercept=(yintercept&0xffff0000)-TILEGLOBAL+(pwallposi<<10);
										xintercept=xintercept-((xstep*pwallposi)>>6);
									}
									else
									{
										yintercept=(yintercept&0xffff0000)+(pwallposi<<10);
										xintercept=xintercept-((xstep*(64-pwallposi))>>6);
									}
									xtile = (short) (xintercept >> TILESHIFT);
									HitHorizWall();
								}
							}
						}
					}
					else
					{
						xintercept=xtile<<TILESHIFT;
						ytile = (short) (yintercept >> TILESHIFT);
						HitVertWall();
					}
				}
				break;
			}
passvert:
			tilehit->visible=true;
			tilehit->amFlags |= AM_Visible;
			xtile+=xtilestep;
			yintercept+=ystep;
			xspot[0]=xtile;
			xspot[1]=yintercept>>16;
		}
		while(1);
		continue;

		do
		{
			if(xtilestep==-1 && (xintercept>>16)<=xtile) goto vertentry;
			if(xtilestep==1 && (xintercept>>16)>=xtile) goto vertentry;
horizentry:

			if((uint32_t)xintercept>mapwidth*65536-1 || (word)ytile>=mapheight)
			{
				if(ytile<0) yintercept=0, ytile=0;
				else if((unsigned)ytile>=mapheight) yintercept=mapheight<<TILESHIFT, ytile=mapheight-1;
				else ytile=(short) (yintercept >> TILESHIFT);
				if(xintercept<0) xintercept=0, xtile=0;
				else if((unsigned)xintercept>=(mapwidth<<TILESHIFT)) xintercept=mapwidth<<TILESHIFT, xtile=mapwidth-1;
				xspot[0]=0xffff;
				tilehit=0;
				HitVertBorder();
				break;
			}
			if(yspot[0]>=mapwidth || yspot[1]>=mapheight) break;
			tilehit=map->GetSpot(yspot[0], yspot[1], 0);
			if(tilehit && tilehit->tile)
			{
				if(tilehit->tile->offsetHorizontal)
				{
					DetermineHitDir(false);
					int32_t xintbuf=xintercept+(xstep>>1);
					if((xintbuf>>16)!=(xintercept>>16))
						goto passhoriz;
					if(CheckSlidePass(tilehit->slideStyle, (word)xintbuf, tilehit->slideAmount[hitdir]))
						goto passhoriz;
					xintercept=xintbuf;
					yintercept=(ytile<<TILESHIFT)+0x8000;
					xtile = (short) (xintercept >> TILESHIFT);
					HitHorizWall();
				}
				else
				{
					bool isPushwall = tilehit->pushAmount != 0 || tilehit->pushReceptor;
					if(tilehit->pushReceptor)
						tilehit = tilehit->pushReceptor;

					if(isPushwall)
					{
						if(tilehit->pushDirection==MapTile::North || tilehit->pushDirection==MapTile::South)
						{
							int32_t xintbuf;
							int pwallposnorm;
							int pwallposinv;
							if(tilehit->pushDirection==MapTile::North)
							{
								pwallposnorm = 64-tilehit->pushAmount;
								pwallposinv = tilehit->pushAmount;
							}
							else
							{
								pwallposnorm = tilehit->pushAmount;
								pwallposinv = 64-tilehit->pushAmount;
							}
							if((tilehit->pushDirection == MapTile::South && ytile==(signed)tilehit->GetY() && ((uint32_t)xintercept>>16)==tilehit->GetX())
								|| (tilehit->pushDirection == MapTile::North && !(ytile==(signed)tilehit->GetY() && ((uint32_t)xintercept>>16)==tilehit->GetX())))
							{
								xintbuf=xintercept+((xstep*pwallposnorm)>>6);
								if((xintbuf>>16)!=(xintercept>>16))
									goto passhoriz;

								yintercept=(ytile<<TILESHIFT)+TILEGLOBAL-(pwallposinv<<10);
								xintercept=xintbuf;
								xtile = (short) (xintercept >> TILESHIFT);
								HitHorizWall();
							}
							else
							{
								xintbuf=xintercept+((xstep*pwallposinv)>>6);
								if((xintbuf>>16)!=(xintercept>>16))
									goto passhoriz;

								yintercept=(ytile<<TILESHIFT)-(pwallposinv<<10);
								xintercept=xintbuf;
								xtile = (short) (xintercept >> TILESHIFT);
								HitHorizWall();
							}
						}
						else
						{
							int pwallposi = tilehit->pushAmount;
							if(tilehit->pushDirection==MapTile::West) pwallposi = 64-tilehit->pushAmount;
							if((tilehit->pushDirection==MapTile::East && (word)xintercept<(pwallposi<<10))
								|| (tilehit->pushDirection==MapTile::West && (word)xintercept>(pwallposi<<10)))
							{
								if(((uint32_t)xintercept>>16)==tilehit->GetX() && ytile==(signed)tilehit->GetY())
								{
									if((tilehit->pushDirection==MapTile::East && (int32_t)((word)xintercept)+xstep<(pwallposi<<10))
										|| (tilehit->pushDirection==MapTile::West && (int32_t)((word)xintercept)+xstep>(pwallposi<<10)))
										goto passhoriz;

									if(tilehit->pushDirection==MapTile::East)
									{
										xintercept=(xintercept&0xffff0000)+(pwallposi<<10);
										yintercept=yintercept-((ystep*(64-pwallposi))>>6);
									}
									else
									{
										xintercept=(xintercept&0xffff0000)-TILEGLOBAL+(pwallposi<<10);
										yintercept=yintercept-((ystep*pwallposi)>>6);
									}
									ytile = (short) (yintercept >> TILESHIFT);
									HitVertWall();
								}
								else
								{
									texdelta = -(pwallposi<<10);
									yintercept=ytile<<TILESHIFT;
									xtile = (short) (xintercept >> TILESHIFT);
									HitHorizWall();
								}
							}
							else
							{
								if(((uint32_t)xintercept>>16)==tilehit->GetX() && ytile==(signed)tilehit->GetY())
								{
									texdelta = -(pwallposi<<10);
									yintercept=ytile<<TILESHIFT;
									xtile = (short) (xintercept >> TILESHIFT);
									HitHorizWall();
								}
								else
								{
									if((tilehit->pushDirection==MapTile::East && (int32_t)((word)xintercept)+xstep>(pwallposi<<10))
										|| (tilehit->pushDirection==MapTile::West && (int32_t)((word)xintercept)+xstep<(pwallposi<<10)))
										goto passhoriz;

									if(tilehit->pushDirection==MapTile::East)
									{
										xintercept=(xintercept&0xffff0000)-TILEGLOBAL+(pwallposi<<10);
										yintercept=yintercept-((ystep*pwallposi)>>6);
									}
									else
									{
										xintercept=(xintercept&0xffff0000)+(pwallposi<<10);
										yintercept=yintercept-((ystep*(64-pwallposi))>>6);
									}
									ytile = (short) (yintercept >> TILESHIFT);
									HitVertWall();
								}
							}
						}
					}
					else
					{
						yintercept=ytile<<TILESHIFT;
						xtile = (short) (xintercept >> TILESHIFT);
						HitHorizWall();
					}
				}
				break;
			}
passhoriz:
			tilehit->visible=true;
			tilehit->amFlags |= AM_Visible;
			ytile+=ytilestep;
			xintercept+=xstep;
			yspot[0]=xintercept>>16;
			yspot[1]=ytile;
		}
		while(1);
	}
}

/*
====================
=
= WallRefresh
=
====================
*/

void WallRefresh (void)
{
	xpartialdown = viewx&(TILEGLOBAL-1);
	xpartialup = TILEGLOBAL-xpartialdown;
	ypartialdown = viewy&(TILEGLOBAL-1);
	ypartialup = TILEGLOBAL-ypartialdown;

	min_wallheight = TWallHeight{viewheight,viewheight,viewheight};
	lastside = -1;                  // the first pixel is on a new wall
	viewshift = FixedMul(focallengthy, finetangent[(ANGLE_180+players[ConsolePlayer].camera->pitch)>>ANGLETOFINESHIFT]);

	
	fixed bobspeed = players[ConsolePlayer].mo->GetClass()->Meta.GetMetaFixed(APMETA_MoveBobSpeed);
	if (bobspeed == 0)
		bobspeed = FRACUNIT;
	angle_t bobangle = ((gamestate.TimeCount<<13)/((20*FRACUNIT/bobspeed)*TICRATE/35)) & FINEMASK;
	const fixed playerMovebob = players[ConsolePlayer].mo->GetClass()->Meta.GetMetaFixed(APMETA_MoveBob);
	fixed curbob = gamestate.victoryflag ? 0 : FixedMul(FixedMul(players[ConsolePlayer].bob, playerMovebob)>>1, finesine[bobangle]);
	//fprintf(stderr, "sinebobangle=%.2f,bob=%.2f,playermovebob=%.2f,curbob=%.2f\n", FIXED2FLOAT(finesine[bobangle]),FIXED2FLOAT(players[ConsolePlayer].bob), FIXED2FLOAT(playerMovebob), FIXED2FLOAT(curbob));

	// simulating foot step or splash effect
	fixed nextbob = gamestate.victoryflag ? 0 :
		FixedMul(players[ConsolePlayer].bob>>1, finesine[bobangle]);
	auto& lastbob = players[ConsolePlayer].lastbob;
	//fprintf(stderr, "nextbob=%.2f, bobangle=%d\n", FIXED2FLOAT(nextbob), (int)bobangle);
	lastbob.step = false;
	if (lastbob.dir > 0 && nextbob < lastbob.value && nextbob > TILEGLOBAL &&
			!lastbob.inhibitstep)
	{
		//fprintf(stderr, "step\n");
		lastbob.step = true;
		lastbob.inhibitstep = true; // inhibit until bob goes positive again
	}
	if (nextbob - lastbob.value > 0)
	{
		lastbob.dir = 1;
	}
	else if (nextbob - lastbob.value < 0)
	{
		lastbob.dir = -1;
	}
	else
		lastbob.dir = 0;
	lastbob.value = nextbob;
	if (nextbob < 0)
	{
		lastbob.inhibitstep = false;
	}

	fixed height = players[ConsolePlayer].mo->viewheight;
	viewz = curbob - height;

	viewcamz[0] = Camz (height - curbob, -1);
	viewcamz[1] = Camz (height - curbob, 1);

	AsmRefresh();
	ScalePost ();                   // no more optimization on last post
}

void CalcViewVariables()
{
	viewangle = players[ConsolePlayer].camera->angle;
	midangle = viewangle>>ANGLETOFINESHIFT;
	viewsin = finesine[viewangle>>ANGLETOFINESHIFT];
	viewcos = finecosine[viewangle>>ANGLETOFINESHIFT];
	viewx = players[ConsolePlayer].camera->x - FixedMul(focallength,viewcos);
	viewy = players[ConsolePlayer].camera->y + FixedMul(focallength,viewsin);

	focaltx = (short)(viewx>>TILESHIFT);
	focalty = (short)(viewy>>TILESHIFT);

	viewtx = (short)(players[ConsolePlayer].camera->x >> TILESHIFT);
	viewty = (short)(players[ConsolePlayer].camera->y >> TILESHIFT);

	if(players[ConsolePlayer].camera->player)
		r_extralight = players[ConsolePlayer].camera->player->extralight << 3;
	else
		r_extralight = 0;
}

//==========================================================================

void R_RenderView()
{
	CalcViewVariables();

//
// follow the walls from there to the right, drawing as we go
//
	if (levelInfo->Atmos[0])
		DrawStarSky(vbuf, vbufPitch);
	if (levelInfo->Atmos[3])
		DrawHighQualityStarSky(vbuf, vbufPitch);

	if (levelInfo->ParallaxDecals && levelInfo->ParallaxSky.Size() > 0)
	{
		std::fill(skywallheight.Get(), skywallheight.Get() + SCREENWIDTH, TWallHeight{});
		DrawParallax(vbuf, vbufPitch);
	}

	Shading::PopulateHalos ();

	WallRefresh ();

	if (!levelInfo->ParallaxDecals && levelInfo->ParallaxSky.Size() > 0)
		DrawParallax(vbuf, vbufPitch);
#if defined(USE_FEATUREFLAGS) && defined(USE_CLOUDSKY)
	if(GetFeatureFlags() & FF_CLOUDSKY)
		DrawClouds(vbuf, vbufPitch, min_wallheight);
#endif
	DrawFloorAndCeiling(vbuf, vbufPitch, min_wallheight);

//
// draw all the scaled images
//
	DrawScaleds();                  // draw scaled stuff

	if (levelInfo->Atmos[1])
		DrawRain(vbuf, vbufPitch, 0, 0);
	if (levelInfo->SnowEnabled())
		DrawSnow(vbuf, vbufPitch, 0, 0);

	DrawPlayerWeapon ();    // draw player's hands

	if((StatusBar->ShowWithoutKey() || control[ConsolePlayer].buttonstate[bt_showstatusbar] || control[ConsolePlayer].buttonheld[bt_showstatusbar]) && viewsize == 21)
	{
		ingame = false;
		StatusBar->DrawStatusBar();
		ingame = true;
	}

	// Always mark the current spot as visible in the automap
	map->GetSpot(players[ConsolePlayer].mo->tilex, players[ConsolePlayer].mo->tiley, 0)->amFlags |= AM_Visible;
}

/*
========================
=
= ThreeDRefresh
=
========================
*/

void    ThreeDRefresh (void)
{
	MapEdit::AdjustGameMap adjustGameMap;

	// Ensure we have a valid camera
	if(players[ConsolePlayer].camera == NULL)
		players[ConsolePlayer].camera = players[ConsolePlayer].mo;

	if (fizzlein && gameinfo.DeathTransition == GameInfo::TRANSITION_Fizzle)
		FizzleFadeStart();

//
// clear out the traced array
//
	map->ClearVisibility();

	vbuf = VL_LockSurface();
	if(vbuf == NULL) return;

	vbuf += screenofs;
	vbufPitch = SCREENPITCH;

	R_RenderView();

	VL_UnlockSurface();
	vbuf = NULL;

//
// show screen and time last cycle
//
	if (fizzlein)
	{
		if(gameinfo.DeathTransition == GameInfo::TRANSITION_Fizzle)
			FizzleFade(0, 0, screenWidth, screenHeight, 20, false);
		else
			VL_FadeIn(0, 255, 24);
		fizzlein = false;

		// don't make a big tic count
		ResetTimeCount();
	}
	else if (fpscounter)
	{
		FString fpsDisplay;
		fpsDisplay.Format("%2u fps", fps);

		word x = 0;
		word y = 0;
		word width, height;
		VW_MeasurePropString(ConFont, fpsDisplay, width, height);
		MenuToRealCoords(x, y, width, height, MENU_TOP);
		VWB_Clear(GPalette.BlackIndex, x, y, x+width+1, y+height+1);
		px = 0;
		py = 0;
		pa = MENU_TOP;
		VWB_DrawPropString(ConFont, fpsDisplay, CR_WHITE);
		pa = MENU_CENTER;
	}

	if (fpscounter)
	{
		fps_frames++;
		fps_time+=tics;

		if(fps_time>35)
		{
			fps_time-=35;
			fps=fps_frames<<1;
			fps_frames=0;
		}
	}

	if (gameinfo.DrawGameMessage)
		DrawGameMessage ();
}
