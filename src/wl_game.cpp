// WL_GAME.C

#ifdef WINDOWS
#include <io.h>
#else
#include <unistd.h>
#endif

#include <algorithm>
#include <math.h>
#include "wl_def.h"
#include "wl_menu.h"
#include "id_ca.h"
#include "id_sd.h"
#include "id_vl.h"
#include "id_vh.h"
#include "id_us.h"
#include "w_wad.h"
#include "thinker.h"
#include "actor.h"
#include <SDL_mixer.h>
#include "wl_agent.h"
#include "g_intermission.h"
#include "g_mapinfo.h"
#include "r_sprites.h"
#include "wl_inter.h"
#include "wl_draw.h"
#include "wl_play.h"
#include "wl_game.h"
#include "wl_text.h"
#include "a_inventory.h"
#include "colormatcher.h"
#include "thingdef/thingdef.h"
#include "doomerrors.h"

#ifdef MYPROFILE
#include <TIME.H>
#endif


/*
=============================================================================

							LOCAL CONSTANTS

=============================================================================
*/


/*
=============================================================================

							GLOBAL VARIABLES

=============================================================================
*/

bool			ingame,fizzlein;
gametype        gamestate;

NewMap_t NewMap;

#ifdef USE_FEATUREFLAGS
int ffDataTopLeft, ffDataTopRight, ffDataBottomLeft, ffDataBottomRight;
#endif

//
// ELEVATOR BACK MAPS - REMEMBER (-1)!!
//
int ElevatorBackTo[]={1,1,7,3,5,3};

void SetupGameLevel (void);
bool GameLoop (void);

/*
=============================================================================

							LOCAL VARIABLES

=============================================================================
*/



//===========================================================================
//===========================================================================


/*
==========================
=
= SetSoundLoc - Given the location of an object (in terms of global
=       coordinates, held in globalsoundx and globalsoundy), munges the values
=       for an approximate distance from the left and right ear, and puts
=       those values into leftchannel and rightchannel.
=
= JAB
=
==========================
*/

int leftchannel, rightchannel, channeldist;
#define ATABLEMAX 15
byte righttable[ATABLEMAX][ATABLEMAX * 2] = {
{ 8, 8, 8, 8, 8, 8, 8, 7, 7, 7, 7, 7, 7, 6, 0, 0, 0, 0, 0, 1, 3, 5, 8, 8, 8, 8, 8, 8, 8, 8},
{ 8, 8, 8, 8, 8, 8, 8, 7, 7, 7, 7, 7, 6, 4, 0, 0, 0, 0, 0, 2, 4, 6, 8, 8, 8, 8, 8, 8, 8, 8},
{ 8, 8, 8, 8, 8, 8, 8, 7, 7, 7, 7, 6, 6, 4, 1, 0, 0, 0, 1, 2, 4, 6, 8, 8, 8, 8, 8, 8, 8, 8},
{ 8, 8, 8, 8, 8, 8, 8, 7, 7, 7, 7, 6, 5, 4, 2, 1, 0, 1, 2, 3, 5, 7, 8, 8, 8, 8, 8, 8, 8, 8},
{ 8, 8, 8, 8, 8, 8, 8, 8, 7, 7, 7, 6, 5, 4, 3, 2, 2, 3, 3, 5, 6, 8, 8, 8, 8, 8, 8, 8, 8, 8},
{ 8, 8, 8, 8, 8, 8, 8, 8, 7, 7, 7, 6, 6, 5, 4, 4, 4, 4, 5, 6, 7, 8, 8, 8, 8, 8, 8, 8, 8, 8},
{ 8, 8, 8, 8, 8, 8, 8, 8, 8, 7, 7, 7, 6, 6, 5, 5, 5, 6, 6, 7, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8},
{ 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 7, 7, 7, 6, 6, 7, 7, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8},
{ 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8},
{ 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8},
{ 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8},
{ 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8},
{ 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8},
{ 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8},
{ 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8}
};
byte lefttable[ATABLEMAX][ATABLEMAX * 2] = {
{ 8, 8, 8, 8, 8, 8, 8, 8, 5, 3, 1, 0, 0, 0, 0, 0, 6, 7, 7, 7, 7, 7, 7, 8, 8, 8, 8, 8, 8, 8},
{ 8, 8, 8, 8, 8, 8, 8, 8, 6, 4, 2, 0, 0, 0, 0, 0, 4, 6, 7, 7, 7, 7, 7, 8, 8, 8, 8, 8, 8, 8},
{ 8, 8, 8, 8, 8, 8, 8, 8, 6, 4, 2, 1, 0, 0, 0, 1, 4, 6, 6, 7, 7, 7, 7, 8, 8, 8, 8, 8, 8, 8},
{ 8, 8, 8, 8, 8, 8, 8, 8, 7, 5, 3, 2, 1, 0, 1, 2, 4, 5, 6, 7, 7, 7, 7, 8, 8, 8, 8, 8, 8, 8},
{ 8, 8, 8, 8, 8, 8, 8, 8, 8, 6, 5, 3, 3, 2, 2, 3, 4, 5, 6, 7, 7, 7, 8, 8, 8, 8, 8, 8, 8, 8},
{ 8, 8, 8, 8, 8, 8, 8, 8, 8, 7, 6, 5, 4, 4, 4, 4, 5, 6, 6, 7, 7, 7, 8, 8, 8, 8, 8, 8, 8, 8},
{ 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 7, 6, 6, 5, 5, 5, 6, 6, 7, 7, 7, 8, 8, 8, 8, 8, 8, 8, 8, 8},
{ 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 7, 7, 6, 6, 7, 7, 7, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8},
{ 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8},
{ 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8},
{ 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8},
{ 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8},
{ 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8},
{ 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8},
{ 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8}
};

void
SetSoundLoc(fixed gx,fixed gy,double attenuation)
{
	fixed   xt,yt;
	int     x,y;

//
// translate point to view centered coordinates
//
	gx -= viewx;
	gy -= viewy;

//
// calculate newx
//
	xt = FixedMul(gx,viewcos);
	yt = FixedMul(gy,viewsin);
	x = (xt - yt) >> TILESHIFT;

//
// calculate newy
//
	xt = FixedMul(gx,viewsin);
	yt = FixedMul(gy,viewcos);
	y = (yt + xt) >> TILESHIFT;

	if (y >= ATABLEMAX)
		y = ATABLEMAX - 1;
	else if (y <= -ATABLEMAX)
		y = -ATABLEMAX;
	if (x < 0)
		x = -x;
	if (x >= ATABLEMAX)
		x = ATABLEMAX - 1;
	leftchannel  =  lefttable[x][y + ATABLEMAX];
	rightchannel = righttable[x][y + ATABLEMAX];

	const double ATTN_NONE = 0.0;
	const double ATTN_NORM = 1.0;

	if (fabs(attenuation-ATTN_NORM)<=EQUAL_EPSILON)
	{
		channeldist = 0;
	}
	else if (fabs(attenuation-ATTN_NONE)>EQUAL_EPSILON)
	{
		const double dist = sqrt(FIXED2FLOAT(FixedMul(gx,gx) + FixedMul(gy,gy)));
		const double maxdist = 64.0; // "standard" map size
		channeldist = (int)(dist * attenuation * 255 / maxdist);
		channeldist = std::min(channeldist, 255);
	}
	else // ATTN_NONE
	{
		leftchannel = rightchannel = 0;
		channeldist = 0;
	}

#if 0
	US_CenterWindow(8,1);
	US_PrintSigned(leftchannel);
	US_Print(",");
	US_PrintSigned(rightchannel);
	VW_UpdateScreen();
#endif
}

/*
==========================
=
= SetSoundLocGlobal - Sets up globalsoundx & globalsoundy and then calls
=       UpdateSoundLoc() to transform that into relative channel volumes. Those
=       values are then passed to the Sound Manager so that they'll be used for
=       the next sound played (if possible).
=
= JAB
=
==========================
*/
void PlaySoundLocGlobal(const char* s,fixed gx,fixed gy,int chan,unsigned int objId,bool looped,double attenuation, double volume)
{
	if (looped && objId != 0)
	{
		if (LoopedAudio::has (objId))
			return;
	}

	SetSoundLoc(gx, gy, attenuation);
	SD_PositionSound(leftchannel, rightchannel, channeldist);
	SD_SetLoopingPlay(looped);
	SD_SetPlayVolume(volume);

	int channel = SD_PlaySound(s, static_cast<SoundChannel> (chan));
	if(channel != -1)
	{
		channelSoundPos[channel].globalsoundx = gx;
		channelSoundPos[channel].globalsoundy = gy;
		channelSoundPos[channel].attenuation = attenuation;
		channelSoundPos[channel].volume = volume;
		channelSoundPos[channel].valid = 1;
	}

	if (looped && objId != 0)
	{
		const SoundIndex &sound = SoundInfo.FindSound(s);
		LoopedAudio::add (objId, channel, sound, attenuation, volume);
	}
}

void UpdateSoundLoc(void)
{
	for(int i = 0; i < MIX_CHANNELS; i++)
	{
		if(channelSoundPos[i].valid)
		{
			SetSoundLoc(channelSoundPos[i].globalsoundx,
				channelSoundPos[i].globalsoundy,
				channelSoundPos[i].attenuation);
			SD_SetPosition(i, leftchannel, rightchannel, channeldist);
			SD_SetChannelVolume (i, channelSoundPos[i].volume);
		}
	}
}

/*
**      JAB End
*/

//==========================================================================

/*
==================
=
= SetupGameLevel
=
==================
*/

void SetupGameLevel (void)
{
	if (!loadedgame)
	{
		gamestate.victoryflag = false;
		gamestate.fullmap = false;
		gamestate.TimeCount
			= gamestate.secrettotal
			= gamestate.killtotal
			= gamestate.treasuretotal
			= gamestate.secretcount
			= gamestate.killcount
			= gamestate.treasurecount = 0;
		LastAttacker = NULL;
		players[0].killerobj = NULL;

		// if revisiting this map for a second time then recover the kill stats
		HubWorld::MapData *hubmapdata = 
			(gamestate.phubworld->hasMap(gamestate.mapname) ?
				&(gamestate.phubworld->mapdata.find(gamestate.mapname)->second) :
				NULL);
		if(hubmapdata != NULL)
		{
			gamestate.secretcount = hubmapdata->secretcount;
			gamestate.killcount = hubmapdata->killcount;
			gamestate.treasurecount = hubmapdata->treasurecount;

			gamestate.killtotal = hubmapdata->killtotal;
			gamestate.treasuretotal = hubmapdata->treasuretotal;
		}
	}

	gamestate.faceframe.SetInvalid();

#ifdef USE_GPL
	bibendovsky::level_initialize();
#endif

//
// load the level
//
	CA_CacheMap (gamestate.mapname, loadedgame);
	if (!loadedgame)
		StartMusic ();

#ifdef USE_FEATUREFLAGS
	// Temporary definition to make things clearer
	#define MXX MAPSIZE - 1

	// Read feature flags data from map corners and overwrite corners with adjacent tiles
	ffDataTopLeft     = MAPSPOT(0,   0,   0); MAPSPOT(0,   0,   0) = MAPSPOT(1,       0,       0);
	ffDataTopRight    = MAPSPOT(MXX, 0,   0); MAPSPOT(MXX, 0,   0) = MAPSPOT(MXX,     1,       0);
	ffDataBottomRight = MAPSPOT(MXX, MXX, 0); MAPSPOT(MXX, MXX, 0) = MAPSPOT(MXX - 1, MXX,     0);
	ffDataBottomLeft  = MAPSPOT(0,   MXX, 0); MAPSPOT(0,   MXX, 0) = MAPSPOT(0,       MXX - 1, 0);

	#undef MXX
#endif

//
// spawn actors
//
	if(!loadedgame)
	{
		map->SpawnThings();

		// Check to see if a player spawned
		if(players[0].mo == NULL)
			throw CRecoverableError("No player 1 start!");
	}

	ClearGameMessage ();
}


//==========================================================================


/*
===================
=
= DrawPlayBorderSides
=
= To fix window overwrites
=
===================
*/
void DrawPlayBorderSides(void)
{
	if((unsigned)viewheight == screenHeight) return;

	if(!gameinfo.Border.issolid)
	{
		static FTexture * const BorderTextures[8] =
		{
			TexMan(gameinfo.Border.tl), TexMan(gameinfo.Border.t), TexMan(gameinfo.Border.tr),
			TexMan(gameinfo.Border.l), TexMan(gameinfo.Border.r),
			TexMan(gameinfo.Border.bl), TexMan(gameinfo.Border.b), TexMan(gameinfo.Border.br)
		};
		const int offset = gameinfo.Border.offset;

		// Draw frame
		if((unsigned)viewwidth != screenWidth)
		{
			VWB_DrawFill(BorderTextures[0], viewscreenx-offset, viewscreeny-offset, viewscreenx, viewscreeny, true);
			VWB_DrawFill(BorderTextures[1], viewscreenx, viewscreeny-BorderTextures[1]->GetHeight(), viewscreenx+viewwidth, viewscreeny, true);
			VWB_DrawFill(BorderTextures[2], viewscreenx, viewscreeny-offset, viewscreenx+viewwidth+offset, viewscreeny, true);
			VWB_DrawFill(BorderTextures[3], viewscreenx-BorderTextures[3]->GetWidth(), viewscreeny, viewscreenx, viewscreeny+viewheight, true);
			VWB_DrawFill(BorderTextures[4], viewscreenx+viewwidth, viewscreeny, viewscreenx+viewwidth+BorderTextures[4]->GetWidth(), viewscreeny+viewheight, true);
			VWB_DrawFill(BorderTextures[5], viewscreenx-offset, viewscreeny+viewheight, viewscreenx, viewscreeny+viewheight+offset, true);
			VWB_DrawFill(BorderTextures[6], viewscreenx, viewscreeny+viewheight, viewscreenx+viewwidth, viewscreeny+viewheight+BorderTextures[3]->GetHeight(), true);
			VWB_DrawFill(BorderTextures[7], viewscreenx+viewwidth, viewscreeny+viewheight, viewscreenx+viewwidth+offset, viewscreeny+viewheight+offset, true);
		}
		else
			VWB_DrawFill(BorderTextures[6], 0, viewscreeny+viewheight, screenWidth, viewscreeny+viewheight+BorderTextures[6]->GetHeight(), true);
	}
	else
	{
		byte colors[3] =
		{
			ColorMatcher.Pick(RPART(gameinfo.Border.topcolor), GPART(gameinfo.Border.topcolor), BPART(gameinfo.Border.topcolor)),
			ColorMatcher.Pick(RPART(gameinfo.Border.bottomcolor), GPART(gameinfo.Border.bottomcolor), BPART(gameinfo.Border.bottomcolor)),
			ColorMatcher.Pick(RPART(gameinfo.Border.highlightcolor), GPART(gameinfo.Border.highlightcolor), BPART(gameinfo.Border.highlightcolor))
		};

		if((unsigned)viewwidth != screenWidth)
		{
			VWB_Clear(colors[0], viewscreenx-scaleFactorX, viewscreeny-scaleFactorY, viewscreenx+viewwidth, viewscreeny);
			VWB_Clear(colors[0], viewscreenx-scaleFactorX, viewscreeny, viewscreenx, viewscreeny+viewheight);
			VWB_Clear(colors[1], viewscreenx, viewscreeny+viewheight, viewscreenx+viewwidth+scaleFactorX, viewscreeny+viewheight+scaleFactorY);
			VWB_Clear(colors[1], viewscreenx+viewwidth, viewscreeny-scaleFactorY, viewscreenx+viewwidth+scaleFactorX, viewscreeny+viewheight);
			VWB_Clear(colors[2], viewscreenx-scaleFactorX, viewscreeny+viewheight, viewscreenx, viewscreeny+viewheight+scaleFactorY);
		}
		else
		{
			if(statusbary1)
				VWB_Clear(colors[0], 0, viewscreeny-scaleFactorY, screenWidth, viewscreeny);
			VWB_Clear(colors[1], 0, viewscreeny+viewheight, screenWidth, viewscreeny+viewheight+scaleFactorY);
		}
	}
}

/*
===================
=
= DrawPlayBorder
=
===================
*/

void DBaseStatusBar::RefreshBackground (bool noborder)
{
	FTexture *borderTex = TexMan(levelInfo->GetBorderTexture());

	if(viewscreeny > statusbary1)
		VWB_DrawFill(borderTex, 0, statusbary1, screenWidth, viewscreeny);
	VWB_DrawFill(borderTex, 0, viewscreeny, viewscreenx, viewheight + viewscreeny);
	VWB_DrawFill(borderTex, viewwidth + viewscreenx, viewscreeny, screenWidth, viewheight + viewscreeny);
	VWB_DrawFill(borderTex, 0, viewscreeny + viewheight, screenWidth, statusbary2);
	if(statusbarx)
	{
		VWB_DrawFill(borderTex, 0, 0, statusbarx, statusbary1);
		VWB_DrawFill(borderTex, screenWidth-statusbarx, 0, screenWidth, statusbary1);
		VWB_DrawFill(borderTex, 0, statusbary2, statusbarx, screenHeight);
		VWB_DrawFill(borderTex, screenWidth-statusbarx, statusbary2, screenWidth, screenHeight);
	}
	// Complete border
	if(statusbary1)
		VWB_DrawFill(borderTex, statusbarx, 0, screenWidth-statusbarx, statusbary1);
	VWB_DrawFill(borderTex, statusbarx, statusbary2, screenWidth-statusbarx, screenHeight);

	if(noborder)
		return;
	DrawPlayBorderSides();
}


/*
===================
=
= DrawActors
=
===================
*/

void DBaseStatusBar::DrawActors ()
{
	if(viewsize == 21 && ingame)
		return;

	for(AActor::Iterator iter = AActor::GetIterator();iter.Next();)
	{
		AActor *obj = iter;
		R_DrawSpriteAsGraphic (obj);
	}
}


/*
===================
=
= DrawPlayScreen
=
===================
*/

void DrawPlayScreen (bool noborder)
{
	StatusBar->RefreshBackground(noborder);
	StatusBar->DrawStatusBar();
}


//==========================================================================

/*
==================
=
= StartDemoRecord
=
==================
*/

char    demoname[13] = "DEMO?.";

#define MAXDEMOSIZE     8192

void StartDemoRecord (int levelnumber)
{
	demobuffer=malloc(MAXDEMOSIZE);
	CHECKMALLOCRESULT(demobuffer);
	demoptr = (int8_t *) demobuffer;
	lastdemoptr = demoptr+MAXDEMOSIZE;

	*demoptr = levelnumber;
	demoptr += 4;                           // leave space for length
	demorecord = true;
}


/*
==================
=
= FinishDemoRecord
=
==================
*/

static bool CA_WriteFile (const char *filename, void *ptr, int32_t length)
{
	const int handle = open(filename, O_CREAT | O_WRONLY | O_BINARY, 777);
	if (handle == -1)
		return false;

	if (!write (handle,ptr,length))
	{
		close (handle);
		return false;
	}
	close (handle);
	return true;
}

void FinishDemoRecord (void)
{
	int32_t    length,level;
	char str[80];

	demorecord = false;

	length = (int32_t) (demoptr - (int8_t *)demobuffer);

	demoptr = ((int8_t *)demobuffer)+1;
	demoptr[0] = (int8_t) length;
	demoptr[1] = (int8_t) (length >> 8);
	demoptr[2] = 0;

	VW_FadeIn();
	US_CenterWindow(24,3);
	PrintY+=6;
	US_Print(SmallFont, " Demo number (0-9): ");
	VW_UpdateScreen();

	if (US_LineInput (SmallFont,px,py,str,NULL,true,1,0,GPalette.WhiteIndex))
	{
		level = atoi (str);
		if (level>=0 && level<=9)
		{
			demoname[4] = (char)('0'+level);
			CA_WriteFile (demoname,demobuffer,length);
		}
	}

	free(demobuffer);
}

//==========================================================================

/*
==================
=
= RecordDemo
=
= Fades the screen out, then starts a demo.  Exits with the screen faded
=
==================
*/

void RecordDemo (void)
{
	FString level;
	int levelnum, esc;
	char str[80];

	US_CenterWindow(26,3);
	PrintY+=6;
	US_Print(SmallFont, "  Demo which level (#): ");
	VW_UpdateScreen();
	VW_FadeIn ();
	esc = !US_LineInput (SmallFont,px,py,str,NULL,true,2,0,GPalette.WhiteIndex);
	if (esc)
		return;

	levelnum = atoi(str);
	level.Format("MAP%02d", levelnum);

	if (Wads.CheckNumForName(level) == -1)
		return;

	VW_FadeOut ();

	NewGame (gd_hard, level, false);

	StartDemoRecord (levelnum);

	DrawPlayScreen ();
	VW_FadeIn ();

	startgame = false;
	demorecord = true;

	SetupGameLevel ();

	if(usedoublebuffering) VH_UpdateScreen();
	fizzlein = true;

	PlayLoop ();

	demoplayback = false;

	StopMusic ();
	VW_FadeOut ();

	FinishDemoRecord ();
}



//==========================================================================

/*
==================
=
= PlayDemo
=
= Fades the screen out, then starts a demo.  Exits with the screen unfaded
=
==================
*/

void PlayDemo (int demonumber)
{
#if 0
	int length;
	char demoName[9];
	sprintf(demoName, "DEMO%d", demonumber);
	int lumpNum = Wads.GetNumForName(demoName);
	if(lumpNum == -1)
		return;
	FWadLump lump = Wads.OpenLumpNum(lumpNum);
	demoptr = new int8_t[Wads.LumpLength(lumpNum)];
	int8_t* demoptr_freeme = demoptr; // Since I can't delete[] demoptr when the time comes.
	lump.Read(demoptr, Wads.LumpLength(lumpNum));

	int mapon = *demoptr++;
	FString level;
	level.Format("MAP%02d", mapon);
	NewGame (1,level,false);
	length = READWORD(*(uint8_t **)&demoptr);
	// TODO: Seems like the original demo format supports 16 MB demos
	//       But T_DEM00 and T_DEM01 of Wolf have a 0xd8 as third length size...
	demoptr++;
	lastdemoptr = demoptr-4+length;

	VW_FadeOut ();

	DrawPlayScreen ();

	startgame = false;
	demoplayback = true;

	SetupGameLevel ();

	PlayLoop ();

	delete[] demoptr_freeme;

	demoplayback = false;

	StopMusic ();
#endif
}

//==========================================================================

/*
==================
=
= Died
=
==================
*/

void Died (void)
{
	float   fangle;
	int32_t dx,dy;
	angle_t iangle;

	if (screenfaded)
	{
		ThreeDRefresh ();
		VW_FadeIn ();
	}

	SD_PlaySound ("player/death");
	StatusBar->ClearInfoMessages();

	//
	// swing around to face attacker
	//
	if(players[0].killerobj)
	{
		dx = players[0].killerobj->x - players[0].mo->x;
		dy = players[0].mo->y - players[0].killerobj->y;

		fangle = (float) atan2((float) dy, (float) dx);     // returns -pi to pi
		if (fangle<0)
			fangle = (float) (M_PI*2+fangle);

		iangle = (angle_t) (fangle*ANGLE_180/M_PI);
	}
	else
	{
		iangle = players[0].mo->angle;
	}

	static const angle_t DEATHROTATE = ANGLE_1*2;
	angle_t &curangle = players[0].mo->angle;
	const int rotate = curangle - iangle > ANGLE_180 ? 1 : -1;

	do
	{
		for(unsigned int t = tics;t-- > 0;)
		{
			players[0].mo->Tick();

			if (curangle - iangle < DEATHROTATE)
				curangle = iangle;
			else
				curangle += rotate*DEATHROTATE;
		}

		ThreeDRefresh ();
		VH_UpdateScreen();
		CalcTics ();
	} while (curangle != iangle);

	// Wait for weapon to drop
	while(players[0].psprite[player_t::ps_weapon].frame)
	{
		for(unsigned int t = tics;t-- > 0;)
			players[0].mo->Tick();

		ThreeDRefresh();
		VH_UpdateScreen();
		CalcTics();
	}

	//
	// fade to red
	//
	FinishPaletteShifts ();

	if(usedoublebuffering) VH_UpdateScreen();

    if (gamestate.difficulty->LivesCount >= 0) {
        --players[0].lives;

        if (gameinfo.GameOverPic.IsNotEmpty() && players[0].lives == -1)
        {
            FTextureID texID = TexMan.CheckForTexture(gameinfo.GameOverPic, FTexture::TEX_Any);
            if(texID.isValid())
                R_DrawZoomer(texID);
        }
    }

	if(gameinfo.DeathTransition == GameInfo::TRANSITION_Fizzle)
	{
		FizzleFadeStart();

		// Fizzle fade used a slightly darker shade of red.
		byte fr = RPART(players[0].mo->damagecolor)*2/3;
		byte fg = GPART(players[0].mo->damagecolor)*2/3;
		byte fb = BPART(players[0].mo->damagecolor)*2/3;
		VWB_Clear(ColorMatcher.Pick(fr,fg,fb), viewscreenx, viewscreeny, viewwidth+viewscreenx, viewheight+viewscreeny);

		IN_ClearKeysDown ();

		FizzleFade(viewscreenx,viewscreeny,viewwidth,viewheight,70,false);

		IN_UserInput(100);
	}
	else
	{
		// If we get a game over we will fade out any way
		if((players[0].lives > -1) || (gamestate.difficulty->LivesCount < 0))
			VL_FadeOut(0, 255, 0, 0, 0, 64);
	}

	LoopedAudio::stopActiveSounds();
	SD_WaitSoundDone ();

	if ((players[0].lives > -1) || (gamestate.difficulty->LivesCount < 0))
		players[0].state = player_t::PST_REBORN;

	ClearGameMessage ();
}

//==========================================================================

static void StripInventory(AActor *actor)
{
	// Remove inventory items that don't transfer (keys for example)
	for(AInventory *inv = actor->inventory;inv;)
	{
		if(inv->interhubamount < 1)
		{
			// Remove the inventory item and clean it up
			AInventory *removeMe = inv;
			inv = inv->inventory;
			players[0].mo->RemoveInventory(removeMe);
			removeMe->Destroy();
			continue;
		}
		else if((inv->itemFlags & IF_INVBAR) && inv->amount > inv->interhubamount)
			inv->amount = inv->interhubamount;

		inv = inv->inventory;
	}
}

/*
===================
=
= GameLoop
=
===================
*/

void StartTravel ();
void FinishTravel ();
bool GameLoop (void)
{
	bool died;
	bool dointermission;
	bool forceshowpsyched;

restartgame:
	VW_FadeOut();
	DrawPlayScreen ();
	died = false;
	dointermission = true;
	forceshowpsyched = false;
	do
	{
		if (!loadedgame)
			players[0].score = players[0].oldscore;

		startgame = false;
		if (!loadedgame)
		{
			SetupGameLevel ();
			if(playstate != ex_warped)
			{
				FinishTravel ();
				if(playstate == ex_newmap)
				{
					if(NewMap.flags & NEWMAP_KEEPPOSITION)
					{
						players[0].mo->x = NewMap.x;
						players[0].mo->y = NewMap.y;
					}
					if(NewMap.flags & NEWMAP_KEEPFACING)
						players[0].mo->angle = NewMap.angle;
				}
			}

			if(levelInfo->ResetHealth)
				players[0].health = players[0].mo->health = players[0].mo->SpawnHealth();

			if(levelInfo->ResetInventory)
			{
				players[0].mo->ClearInventory();
				players[0].mo->GiveStartingInventory();
			}

			if(levelInfo->EnsureInventory.Size() > 0)
			{
				for(unsigned int i = 0;i < levelInfo->EnsureInventory.Size();++i)
				{
					const ClassDef *ensure = levelInfo->EnsureInventory[i];
					AInventory *holding = players[0].mo->FindInventory(ensure);

					if(ensure->IsDescendantOf(NATIVE_CLASS(Ammo)))
					{
						// For ammo ensure we have the proper amount
						AAmmo *ammo = static_cast<AAmmo*>(AActor::Spawn(ensure, 0, 0, 0, 0));
						ammo->RemoveFromWorld();

						if(!holding)
							holding = players[0].mo->FindInventory(ammo->GetAmmoType());

						if(holding && holding->amount < ammo->amount)
							ammo->amount -= holding->amount;
						else if(holding && holding->amount >= ammo->amount)
						{
							ammo->Destroy();
							ammo = NULL;
						}

						if(ammo && !ammo->CallTryPickup(players[0].mo))
								ammo->Destroy();
						continue;
					}

					if(holding)
						continue;

					AInventory *item = static_cast<AInventory*>(AActor::Spawn(ensure, 0, 0, 0, 0));
					item->RemoveFromWorld();
					if(!item->CallTryPickup(players[0].mo))
						item->Destroy();
				}
			}
		}
		else
		{
			loadedgame = false;
			StartMusic ();
		}

		ingame = true;

		if (!died)
			PreloadGraphics (dointermission || forceshowpsyched);
		else
		{
			died = false;
			fizzlein = true;
		}

		StatusBar->DrawStatusBar();

		dointermission = true;
		forceshowpsyched = false;

		PlayLoop ();

		if(playstate == ex_victorious)
		{
			if(gameinfo.VictoryPic.IsNotEmpty())
			{
				FTextureID ywin = TexMan.CheckForTexture(gameinfo.VictoryPic, FTexture::TEX_Any);
				if(ywin.isValid())
					R_DrawZoomer(ywin);
			}
		}

		StopMusic ();
		ingame = false;

		if (demorecord && playstate != ex_warped)
			FinishDemoRecord ();

		if (startgame || loadedgame)
			goto restartgame;

		switch (playstate)
		{
			case ex_completed:
			case ex_secretlevel:
			case ex_newmap:
			case ex_victorious:
			{
				dointermission = !levelInfo->NoIntermission;
				forceshowpsyched = levelInfo->ForceShowPsyched;

				LoopedAudio::stopActiveSounds();

				FString next;
				if(playstate != ex_newmap)
				{
					switch(playstate)
					{
						case ex_secretlevel:
							next = levelInfo->NextSecret;
							break;
						case ex_victorious:
							if(!levelInfo->NextVictory.IsEmpty())
							{
								next = levelInfo->NextVictory;
								break;
							}
						default:
							next = levelInfo->NextMap;
					}

					if(next.IndexOf("EndSequence:") == 0 || next.CompareNoCase("EndTitle") == 0)
					{
						bool endSequence = next.IndexOf("EndSequence:") == 0;

						VL_FadeOut(0, 255, RPART(levelInfo->ExitFadeColor), GPART(levelInfo->ExitFadeColor), BPART(levelInfo->ExitFadeColor), levelInfo->ExitFadeDuration);

						if(dointermission)
						{
							if(levelInfo->ForceTally)
							{
								LevelCompleted();
								VW_FadeOut();
							}

							Victory (false);
						}

						bool gotoMenu = false;
						if(endSequence)
						{
							IntermissionInfo *intermission = IntermissionInfo::Find(next.Mid(12));
							gotoMenu = ShowIntermission(intermission);
						}

						CheckHighScore (players[0].score,levelInfo);
						return gotoMenu;
					}
				}
				else
				{
					NewMap.x = players[0].mo->x;
					NewMap.y = players[0].mo->y;
					NewMap.angle = players[0].mo->angle;

					LevelInfo &teleportMap = LevelInfo::FindByNumber(NewMap.newmap);
					if(teleportMap.MapName[0] == 0)
						Quit("Tried to teleport to unkown map.");
					next = teleportMap.MapName;
				}

				StripInventory(players[0].mo);

				if(dointermission)
					VL_FadeOut(0, 255, RPART(levelInfo->ExitFadeColor), GPART(levelInfo->ExitFadeColor), BPART(levelInfo->ExitFadeColor), levelInfo->ExitFadeDuration);

				StartTravel ();
				if(dointermission)
					LevelCompleted ();              // do the intermission

				if (dointermission && !levelInfo->Intermission.IsEmpty())
				{
					VW_FadeOut ();
					IntermissionInfo *intermission = IntermissionInfo::Find(levelInfo->Intermission);
					ShowIntermission(intermission);
				}

				LevelInfo &nextLevel = LevelInfo::Find(next);
				if(nextLevel.Cluster != levelInfo->Cluster)
					EndText (levelInfo->Cluster, nextLevel.Cluster);

				if(dointermission)
					VW_FadeOut ();
				if(viewsize == 21) DrawPlayScreen();

				ClearGameMessage ();

				if(next.CompareNoCase("EndDemo") == 0)
				{
					CheckHighScore (players[0].score,levelInfo);
					return false;
				}

				players[0].oldscore = players[0].score;

				strncpy(gamestate.mapname, next, 8);
				gamestate.mapname[8] = 0;
				break;
			}

			case ex_died:
				Died ();
				died = true;                    // don't "get psyched!"

				if ((players[0].lives > -1) || (gamestate.difficulty->LivesCount < 0))
					break;                          // more lives left

				VW_FadeOut ();
				if(screenHeight % 200 != 0)
					VL_ClearScreen(0);

				CheckHighScore (players[0].score,levelInfo);
				return false;

			case ex_warped:
				players[0].state = player_t::PST_ENTER;
				LoopedAudio::stopActiveSounds();
				break;

			default:
				if(viewsize == 21) DrawPlayScreen();
				break;
		}
	} while (1);
	return false;
}

//==========================================================================

namespace LoopedAudio
{
	class Chan
	{
	public:
		SndChannel channel;
		SoundIndex sound;
		double attenuation;
		double volume;

		Chan() : channel(-1), attenuation(0.0), volume(1.0)
		{
		}

		explicit Chan(
			SndChannel channel_,
			SoundIndex sound_,
			double attenuation_,
			double volume_
			) :
			channel(channel_),
			sound(sound_),
			attenuation(attenuation_),
			volume(volume_)
		{
		}
	};

	inline FArchive &operator<< (FArchive &arc, Chan &x)
	{
		if (arc.IsLoading())
			x.channel = -1;
		arc << x.sound
			<< x.attenuation
			<< x.volume;
		return arc;
	}

	typedef std::map<ObjId, Chan> ChanMap;
	static ChanMap chans;

	bool has (ObjId objId)
	{
		return chans.find(objId) != chans.end();
	}

	void add (ObjId objId, SndChannel channel, const SoundIndex &sound, double attenuation, double volume)
	{
		chans[objId] = Chan(channel, sound, attenuation, volume);
	}

	bool claimed (SndChannel channel)
	{
		for (ChanMap::const_iterator it = chans.begin();
			it != chans.end(); ++it)
		{
			const Chan &chan = it->second;
			if (chan.channel == channel)
				return true;
		}

		return false;
	}

	void finished (SndChannel channel)
	{
		for (ChanMap::iterator it = chans.begin();
			it != chans.end(); ++it)
		{
			Chan &chan = it->second;

			// keep objId but wipe out the channel
			// we will restart the sound on a different channel later
			if (chan.channel == channel)
				chan.channel = -1;
		}
	}

	int tileDist (AActor *ob, AActor *check)
	{
		return MAX(abs(check->tilex - ob->tilex), abs(check->tiley - ob->tiley));
	}

	void updateSoundPos (void)
	{
		std::map<unsigned int, AActor *> actors;
		for(AActor::Iterator iter = AActor::GetIterator();iter.Next();)
		{
			AActor * const ob = iter;
			actors[ob->spawnid] = ob;
		}

		typedef int FadeLevel;
		typedef std::map<FadeLevel, ChanMap::iterator> FLMap;
		FLMap flm;
		for (ChanMap::iterator it = chans.begin(); it != chans.end(); ++it)
		{
			AActor *ob = actors[it->first];
			Chan &chan = it->second;
			flm[tileDist(ob, players[0].mo)] = it;
		}
		
		int closestCounter;
		FLMap::const_reverse_iterator it2;
		for (closestCounter = (int)flm.size() - 1, it2 = flm.rbegin(); it2 != flm.rend(); ++it2, --closestCounter)
		{
			ChanMap::iterator it = it2->second;
			AActor *ob = actors[it->first];
			Chan &chan = it->second;

			// group 2 has 2 channels only
			// so stop looped audio from objects too distant
			if (closestCounter >= 2 || (MusicMode == smm_Off && chan.volume < 0))
			{
				if (chan.channel != -1)
				{
					globalsoundpos *soundpos = &channelSoundPos[chan.channel];
					if (soundpos->valid)
					{
						soundpos->valid = 0;
						Mix_HaltChannel(chan.channel);
					}

					chan.channel = -1;
				}
			}
			// start looped audio for proximal objects
			else
			{
				if (chan.channel == -1)
				{
					const SoundIndex sound = chan.sound;
					const double attenuation = chan.attenuation;
					const double volume = chan.volume;
					chans.erase(it);

					const char *s = SoundInfo[sound].GetLogicalChars();
					PlaySoundLocGlobal(s, ob->x, ob->y, SD_GENERIC, ob->spawnid, true, attenuation, volume);
					break;
				}
			}
		}

		for (ChanMap::const_iterator it = chans.begin();
			it != chans.end(); ++it)
		{
			AActor *ob = actors[it->first];
			const Chan &chan = it->second;

			if (chan.channel != -1)
			{
				globalsoundpos *soundpos = &channelSoundPos[chan.channel];
				soundpos->globalsoundx = ob->x;
				soundpos->globalsoundy = ob->y;
			}
		}
	}

	void stopSoundFrom (ObjId objId, bool halt_only)
	{
		ChanMap::iterator it = chans.find(objId);
		if (it != chans.end())
		{
			Chan &chan = it->second;

			if (chan.channel != -1)
			{
				globalsoundpos *soundpos = &channelSoundPos[chan.channel];
				if (soundpos->valid)
				{
					soundpos->valid = 0;
					Mix_HaltChannel(chan.channel);
				}

				if(halt_only)
					chan.channel = -1;
			}

			if(!halt_only)
				chans.erase(objId);
		}
	}

	void setVolume (ObjId objId, double volume)
	{
		ChanMap::iterator it = chans.find(objId);
		if (it != chans.end())
		{
			Chan &chan = it->second;

			if (chan.channel != -1)
			{
				chan.volume = volume;

				globalsoundpos *soundpos = &channelSoundPos[chan.channel];
				if (soundpos->valid)
				{
					soundpos->volume = volume;
					SD_SetChannelVolume (chan.channel, volume);
				}
			}
		}
	}

	void Serialize(FArchive &arc)
	{
		arc << chans;
	}

	void stopActiveSounds (bool halt_only)
	{
		std::set<ObjId> obj_ids;
		for(auto it = std::begin(chans); it != std::end(chans); ++it)
		{
			obj_ids.insert(std::begin(obj_ids), it->first);
		}
		for(auto id: obj_ids)
			stopSoundFrom(id, halt_only);
		if(!halt_only)
			chans.clear();
	}
}
