/*
** r_anim.cpp
** Routines for handling texture animation.
**
**---------------------------------------------------------------------------
** Copyright 1998-2006 Randy Heit
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**---------------------------------------------------------------------------
**
*/

#include "wl_def.h"
//#include "cmdlib.h"
//#include "i_system.h"
//#include "r_local.h"
//#include "r_sky.h"
#include "m_random.h"
//#include "d_player.h"
//#include "p_spec.h"
#include "scanner.h"
#include "templates.h"
#include "w_wad.h"
//#include "g_level.h"
#include "textures.h"
#include "farchive.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static FRandom pr_animatepictures ("AnimatePics");

// CODE --------------------------------------------------------------------

//==========================================================================
//
// FTextureManager :: AddAnim
//
// Adds a new animation to the array. If one with the same basepic as the
// new one already exists, it is replaced.
//
//==========================================================================

void FTextureManager::AddAnim (FAnimDef *anim)
{
	// Search for existing duplicate.
	for (unsigned int i = 0; i < mAnimations.Size(); ++i)
	{
		if (mAnimations[i]->BasePic == anim->BasePic)
		{
			// Found one!
			free (mAnimations[i]);
			mAnimations[i] = anim;
			return;
		}
	}
	// Didn't find one, so add it at the end.
	mAnimations.Push (anim);
}

//==========================================================================
//
// FTextureManager :: AddSimpleAnim
//
// Creates an animation with simple characteristics. This is used for
// original Doom (non-ANIMDEFS-style) animations and Build animations.
//
//==========================================================================

void FTextureManager::AddSimpleAnim (FTextureID picnum, int animcount, int animtype, DWORD speedmin, DWORD speedrange)
{
	if (AreTexturesCompatible(picnum, picnum + (animcount - 1)))
	{
		FAnimDef *anim = (FAnimDef *)M_Malloc (sizeof(FAnimDef));
		anim->CurFrame = 0;
		anim->BasePic = picnum;
		anim->NumFrames = animcount;
		anim->AnimType = animtype;
		anim->SwitchTime = 0;
		anim->Frames[0].SpeedMin = speedmin;
		anim->Frames[0].SpeedRange = speedrange;
		anim->Frames[0].FramePic = anim->BasePic;
		AddAnim (anim);
	}
}

//==========================================================================
//
// FTextureManager :: AddComplexAnim
//
// Creates an animation with individually defined frames.
//
//==========================================================================

void FTextureManager::AddComplexAnim (FTextureID picnum, const TArray<FAnimDef::FAnimFrame> &frames)
{
	FAnimDef *anim = (FAnimDef *)M_Malloc (sizeof(FAnimDef) + (frames.Size()-1) * sizeof(frames[0]));
	anim->BasePic = picnum;
	anim->NumFrames = frames.Size();
	anim->CurFrame = 0;
	anim->AnimType = FAnimDef::ANIM_DiscreteFrames;
	anim->SwitchTime = 0;
	memcpy (&anim->Frames[0], &frames[0], frames.Size() * sizeof(frames[0]));
	AddAnim (anim);
}

// ANIMATED comaptibility has no business in Wolf3D
#if 0
//==========================================================================
//
// FTextureManager :: Initanimated
//
// [description copied from BOOM]
// Load the table of animation definitions, checking for existence of
// the start and end of each frame. If the start doesn't exist the sequence
// is skipped, if the last doesn't exist, BOOM exits.
//
// Wall/Flat animation sequences, defined by name of first and last frame,
// The full animation sequence is given using all lumps between the start
// and end entry, in the order found in the WAD file.
//
// This routine modified to read its data from a predefined lump or
// PWAD lump called ANIMATED rather than a static table in this module to
// allow wad designers to insert or modify animation sequences.
//
// Lump format is an array of byte packed animdef_t structures, terminated
// by a structure with istexture == -1. The lump can be generated from a
// text source file using SWANTBLS.EXE, distributed with the BOOM utils.
// The standard list of switches and animations is contained in the example
// source text file DEFSWANI.DAT also in the BOOM util distribution.
//
// [RH] Rewritten to support BOOM ANIMATED lump but also make absolutely
//		no assumptions about how the compiler packs the animdefs array.
//
//==========================================================================

CVAR(Bool, debuganimated, false, 0)

void FTextureManager::InitAnimated (void)
{
	const BITFIELD texflags = TEXMAN_Overridable;
		// I think better not! This is only for old ANIMATED definition that
		// don't know about ZDoom's more flexible texture system.
		// | FTextureManager::TEXMAN_TryAny;

	if (Wads.CheckNumForName ("ANIMATED") != -1)
	{
		FMemLump animatedlump = Wads.ReadLump ("ANIMATED");
		const char *animdefs = (const char *)animatedlump.GetMem();
		const char *anim_p;
		FTextureID pic1, pic2;
		int animtype;
		DWORD animspeed;

		// Init animation
		animtype = FAnimDef::ANIM_Forward;

		for (anim_p = animdefs; *anim_p != -1; anim_p += 23)
		{
			if (*anim_p /* .istexture */ & 1)
			{
				// different episode ?
				if (!(pic1 = CheckForTexture (anim_p + 10 /* .startname */, FTexture::TEX_Wall, texflags)).Exists() ||
					!(pic2 = CheckForTexture (anim_p + 1 /* .endname */, FTexture::TEX_Wall, texflags)).Exists())
					continue;		

				// [RH] Bit 1 set means allow decals on walls with this texture
				Texture(pic2)->bNoDecals = Texture(pic1)->bNoDecals = !(*anim_p & 2);
			}
			else
			{
				if (!(pic1 = CheckForTexture (anim_p + 10 /* .startname */, FTexture::TEX_Flat, texflags)).Exists() ||
					!(pic2 = CheckForTexture (anim_p + 1 /* .startname */, FTexture::TEX_Flat, texflags)).Exists())
					continue;
			}

			FTexture *tex1 = Texture(pic1);
			FTexture *tex2 = Texture(pic2);

			animspeed = (BYTE(anim_p[19]) << 0)  | (BYTE(anim_p[20]) << 8) |
						(BYTE(anim_p[21]) << 16) | (BYTE(anim_p[22]) << 24);

			// SMMU-style swirly hack? Don't apply on already-warping texture
			if (animspeed > 65535 && tex1 != NULL && !tex1->bWarped)
			{
				FTexture *warper = new FWarp2Texture (tex1);
				ReplaceTexture (pic1, warper, false);
			}

			// These tests were not really relevant for swirling textures, or even potentially
			// harmful, so they have been moved to the else block.
			else
			{
				if (tex1->UseType != tex2->UseType)
				{
					// not the same type - 
					continue;
				}

				if (debuganimated)
				{
					Printf("Defining animation '%s' (texture %d, lump %d, file %d) to '%s' (texture %d, lump %d, file %d)\n",
						tex1->Name, pic1.GetIndex(), tex1->GetSourceLump(), Wads.GetLumpFile(tex1->GetSourceLump()),
						tex2->Name, pic2.GetIndex(), tex2->GetSourceLump(), Wads.GetLumpFile(tex2->GetSourceLump()));
				}

				if (pic1 == pic2)
				{
					// This animation only has one frame. Skip it. (Doom aborted instead.)
					Printf ("Animation %s in ANIMATED has only one frame\n", anim_p + 10);
					continue;
				}
				// [RH] Allow for backward animations as well as forward.
				else if (pic1 > pic2)
				{
					swapvalues (pic1, pic2);
					animtype = FAnimDef::ANIM_Backward;
				}

				// Speed is stored as tics, but we want ms so scale accordingly.
				AddSimpleAnim (pic1, pic2 - pic1 + 1, animtype, Scale (animspeed, 1000, 35));
			}
		}
	}
}
#endif

//==========================================================================
//
// FTextureManager :: InitAnimDefs
//
// This uses a Hexen ANIMDEFS lump to define the animation sequences
//
//==========================================================================

void FTextureManager::InitAnimDefs ()
{
	const BITFIELD texflags = TEXMAN_Overridable | TEXMAN_TryAny;
	int lump, lastlump = 0;
	
	while ((lump = Wads.FindLump ("ANIMDEFS", &lastlump)) != -1)
	{
		Scanner sc(lump);

		while (sc.GetNextString ())
		{
			if (sc->str.Compare ("flat") == 0)
			{
				ParseAnim (sc, FTexture::TEX_Flat);
			}
			else if (sc->str.Compare ("texture") == 0)
			{
				ParseAnim (sc, FTexture::TEX_Wall);
			}
			else if (sc->str.Compare ("switch") == 0)
			{
				ProcessSwitchDef (sc);
			}
			// [GRB] Added warping type 2
			else if (sc->str.Compare ("warp") == 0 || sc->str.Compare ("warp2") == 0)
			{
				ParseWarp(sc);
			}
			else if (sc->str.Compare ("cameratexture") == 0)
			{
				ParseCameraTexture(sc);
			}
			else if (sc->str.Compare ("animatedDoor") == 0)
			{
				ParseAnimatedDoor (sc);
			}
			else if (sc->str.Compare("skyoffset") == 0)
			{
				if(!sc.GetNextString()) sc.ScriptMessage(Scanner::ERROR, "Expected string.");
				FTextureID picnum = CheckForTexture (sc->str, FTexture::TEX_Wall, texflags);
				sc.MustGetToken(TK_IntConst);
				if (picnum.Exists())
				{
					Texture(picnum)->SkyOffset = sc->number;
				}
			}
			else
			{
				sc.ScriptMessage (Scanner::ERROR, "");
			}
		}
	}
}

//==========================================================================
//
// FTextureManager :: ParseAnim
//
// Parse a single animation definition out of an ANIMDEFS lump and
// create the corresponding animation structure.
//
//==========================================================================

void FTextureManager::ParseAnim (Scanner &sc, int usetype)
{
	const BITFIELD texflags = TEXMAN_Overridable | TEXMAN_TryAny;
	TArray<FAnimDef::FAnimFrame> frames (32);
	FTextureID picnum;
	int defined = 0;
	bool optional = false, missing = false;

	if(!sc.GetNextString()) sc.ScriptMessage(Scanner::ERROR, "Expected string.");
	if (sc->str.Compare ("optional") == 0)
	{
		optional = true;
		if(!sc.GetNextString()) sc.ScriptMessage(Scanner::ERROR, "Expected string.");
	}
	picnum = CheckForTexture (sc->str, usetype, texflags);

	if (!picnum.Exists())
	{
		if (optional)
		{
			missing = true;
		}
		else
		{
			//Printf (PRINT_BOLD, "ANIMDEFS: Can't find %s\n", sc->str);
			Printf ("ANIMDEFS: Can't find %s\n", sc->str.GetChars());
		}
	}

	// no decals on animating textures, by default
	if (picnum.isValid())
	{
		Texture(picnum)->bNoDecals = true;
	}

	while (sc.GetNextString ())
	{
		if (sc->str.Compare ("allowdecals") == 0)
		{
			if (picnum.isValid())
			{
				Texture(picnum)->bNoDecals = false;
			}
			continue;
		}
		else if (sc->str.Compare ("range") == 0)
		{
			if (defined == 2)
			{
				sc.ScriptMessage (Scanner::ERROR, "You cannot use \"pic\" and \"range\" together in a single animation.");
			}
			if (defined == 1)
			{
				sc.ScriptMessage (Scanner::ERROR, "You can only use one \"range\" per animation.");
			}
			defined = 1;
			ParseRangeAnim (sc, picnum, usetype, missing);
		}
		else if (sc->str.Compare ("pic") == 0)
		{
			if (defined == 1)
			{
				sc.ScriptMessage (Scanner::ERROR, "You cannot use \"pic\" and \"range\" together in a single animation.");
			}
			defined = 2;
			ParsePicAnim (sc, picnum, usetype, missing, frames);
		}
		else
		{
			sc.Rewind ();
			break;
		}
	}

	// If base pic is not present, don't add this anim
	// ParseRangeAnim adds the anim itself, but ParsePicAnim does not.
	if (picnum.isValid() && defined == 2)
	{
		if (frames.Size() < 2)
		{
			sc.ScriptMessage (Scanner::ERROR, "Animation needs at least 2 frames");
		}
		AddComplexAnim (picnum, frames);
	}
}

//==========================================================================
//
// FTextureManager :: ParseRangeAnim
//
// Parse an animation defined using "range". Not that one range entry is
// enough to define a complete animation, unlike "pic".
//
//==========================================================================

void FTextureManager::ParseRangeAnim (Scanner &sc, FTextureID picnum, int usetype, bool missing)
{
	int type;
	FTextureID framenum;
	DWORD min, max;

	type = FAnimDef::ANIM_Forward;
	framenum = ParseFramenum (sc, picnum, usetype, missing);
	ParseTime (sc, min, max);

	if (framenum == picnum || !picnum.Exists())
	{
		return;		// Animation is only one frame or does not exist
	}
	if (framenum < picnum)
	{
		type = FAnimDef::ANIM_Backward;
		Texture(framenum)->bNoDecals = Texture(picnum)->bNoDecals;
		swapvalues (framenum, picnum);
	}
	if (sc.GetNextString())
	{
		if (sc->str.Compare ("Oscillate") == 0)
		{
			type = type == FAnimDef::ANIM_Forward ? FAnimDef::ANIM_OscillateUp : FAnimDef::ANIM_OscillateDown;
		}
		else
		{
			sc.Rewind ();
		}
	}
	AddSimpleAnim (picnum, framenum - picnum + 1, type, min, max - min);
}

//==========================================================================
//
// FTextureManager :: ParsePicAnim
//
// Parse a single frame from ANIMDEFS defined using "pic".
//
//==========================================================================

void FTextureManager::ParsePicAnim (Scanner &sc, FTextureID picnum, int usetype, bool missing, TArray<FAnimDef::FAnimFrame> &frames)
{
	FTextureID framenum;
	DWORD min, max;

	framenum = ParseFramenum (sc, picnum, usetype, missing);
	ParseTime (sc, min, max);

	if (picnum.isValid())
	{
		FAnimDef::FAnimFrame frame;

		frame.SpeedMin = min;
		frame.SpeedRange = max - min;
		frame.FramePic = framenum;
		frames.Push (frame);
	}
}

//==========================================================================
//
// FTextureManager :: ParseFramenum
//
// Reads a frame's texture from ANIMDEFS. It can either be an integral
// offset from basepicnum or a specific texture name.
//
//==========================================================================

FTextureID FTextureManager::ParseFramenum (Scanner &sc, FTextureID basepicnum, int usetype, bool allowMissing)
{
	const BITFIELD texflags = TEXMAN_Overridable | TEXMAN_TryAny;
	FTextureID framenum;

	if (sc.CheckToken(TK_IntConst))
	{
		framenum = basepicnum + (atoi(sc->str) - 1);
	}
	else
	{
		if(!sc.GetNextString()) sc.ScriptMessage(Scanner::ERROR, "Expected string.");
		framenum = CheckForTexture (sc->str, usetype, texflags);
		if (!framenum.Exists() && !allowMissing)
		{
			sc.ScriptMessage (Scanner::ERROR, "Unknown texture %s", sc->str.GetChars());
		}
	}
	return framenum;
}

//==========================================================================
//
// FTextureManager :: ParseTime
//
// Reads a tics or rand time definition from ANIMDEFS.
//
//==========================================================================

void FTextureManager::ParseTime (Scanner &sc, DWORD &min, DWORD &max)
{
	if(!sc.GetNextString()) sc.ScriptMessage(Scanner::ERROR, "Expected string.");
	if (sc->str.Compare ("tics") == 0)
	{
		sc.MustGetToken (TK_FloatConst);
		min = max = DWORD(sc->decimal * 1000 / 35);
	}
	else if (sc->str.Compare ("rand") == 0)
	{
		sc.MustGetToken (TK_FloatConst);
		min = DWORD(sc->decimal * 1000 / 35);
		sc.MustGetToken (TK_FloatConst);
		max = DWORD(sc->decimal * 1000 / 35);
	}
	else
	{
		min = max = 1;
		sc.ScriptMessage (Scanner::ERROR, "Must specify a duration for animation frame");
	}
}

//==========================================================================
//
// FTextureManager :: ParseWarp
//
// Parses a warping texture definition
//
//==========================================================================

void FTextureManager::ParseWarp(Scanner &sc)
{
	const BITFIELD texflags = TEXMAN_Overridable | TEXMAN_TryAny | TEXMAN_ShortNameOnly;
	bool isflat = false;
	bool type2 = sc->str.Compare ("warp2") == 0;	// [GRB]
	if(!sc.GetNextString()) sc.ScriptMessage(Scanner::ERROR, "Expected string.");
	if (sc->str.Compare ("flat") == 0)
	{
		isflat = true;
		if(!sc.GetNextString()) sc.ScriptMessage(Scanner::ERROR, "Expected string.");
	}
	else if (sc->str.Compare ("texture") == 0)
	{
		isflat = false;
		if(!sc.GetNextString()) sc.ScriptMessage(Scanner::ERROR, "Expected string.");
	}
	else
	{
		sc.ScriptMessage (Scanner::ERROR, NULL);
	}
	FTextureID picnum = CheckForTexture (sc->str, isflat ? FTexture::TEX_Flat : FTexture::TEX_Wall, texflags);
	if (picnum.isValid())
	{
		FTexture *warper = Texture(picnum);

		// don't warp a texture more than once
		if (!warper->bWarped)
		{
			if (type2) warper = new FWarp2Texture (warper);
			else warper = new FWarpTexture (warper);

			ReplaceTexture (picnum, warper, false);
		}

		if (sc.CheckToken(TK_FloatConst))
		{
			static_cast<FWarpTexture*>(warper)->SetSpeed(float(sc->decimal));
		}

		// No decals on warping textures, by default.
		// Warping information is taken from the last warp 
		// definition for this texture.
		warper->bNoDecals = true;
		if (sc.GetNextString ())
		{
			if (sc->str.Compare ("allowdecals") == 0)
			{
				warper->bNoDecals = false;
			}
			else
			{
				sc.Rewind ();
			}
		}
	}
}

//==========================================================================
//
// ParseCameraTexture
//
// Parses a camera texture definition
//
//==========================================================================

void FTextureManager::ParseCameraTexture(Scanner &sc)
{
	sc.ScriptMessage(Scanner::ERROR, "Not ready yet to do cameras!");
#if 0
	const BITFIELD texflags = TEXMAN_Overridable | TEXMAN_TryAny | TEXMAN_ShortNameOnly;
	int width, height;
	int fitwidth, fitheight;
	FString picname;

	if(!sc.GetNextString()) sc.ScriptMessage(Scanner::ERROR, "Expected string.");
	picname = sc->str;
	sc.MustGetToken(TK_IntConst);
	width = sc->number;
	sc.MustGetToken(TK_IntConst);
	height = sc->number;
	FTextureID picnum = CheckForTexture (picname, FTexture::TEX_Flat, texflags);
	FTexture *viewer = new FCanvasTexture (picname, width, height);
	if (picnum.Exists())
	{
		FTexture *oldtex = Texture(picnum);
		fitwidth = oldtex->GetScaledWidth ();
		fitheight = oldtex->GetScaledHeight ();
		viewer->UseType = oldtex->UseType;
		ReplaceTexture (picnum, viewer, true);
	}
	else
	{
		fitwidth = width;
		fitheight = height;
		// [GRB] No need for oldtex
		viewer->UseType = FTexture::TEX_Wall;
		AddTexture (viewer);
	}
	if (sc.GetNextString())
	{
		if (sc->str.Compare ("fit") == 0)
		{
			sc.MustGetToken(TK_IntConst);
			fitwidth = sc->number;
			sc.MustGetToken(TK_IntConst);
			fitheight = sc->number;
		}
		else
		{
			sc.Rewind ();
		}
	}
	viewer->SetScaledSize(fitwidth, fitheight);
#endif
}

//==========================================================================
//
// FTextureManager :: FixAnimations
//
// Copy the "front sky" flag from an animated texture to the rest
// of the textures in the animation, and make every texture in an
// animation range use the same setting for bNoDecals.
//
//==========================================================================

void FTextureManager::FixAnimations ()
{
	unsigned int i;
	int j;

	for (i = 0; i < mAnimations.Size(); ++i)
	{
		FAnimDef *anim = mAnimations[i];
		if (anim->AnimType == FAnimDef::ANIM_DiscreteFrames)
		{
			if (Texture(anim->BasePic)->bNoRemap0)
			{
				for (j = 0; j < anim->NumFrames; ++j)
				{
					Texture(anim->Frames[j].FramePic)->SetFrontSkyLayer ();
				}
			}
		}
		else
		{
			bool nodecals;
			bool noremap = false;
			const char *name;

			name = Texture(anim->BasePic)->Name;
			nodecals = Texture(anim->BasePic)->bNoDecals;
			for (j = 0; j < anim->NumFrames; ++j)
			{
				FTexture *tex = Texture(anim->BasePic + j);
				noremap |= tex->bNoRemap0;
				tex->bNoDecals = nodecals;
			}
			if (noremap)
			{
				for (j = 0; j < anim->NumFrames; ++j)
				{
					Texture(anim->BasePic + j)->SetFrontSkyLayer ();
				}
			}
		}
	}
}

//==========================================================================
//
// ParseAnimatedDoor
//
// Parses an animated door definition
//
//==========================================================================

void FTextureManager::ParseAnimatedDoor(Scanner &sc)
{
	const BITFIELD texflags = TEXMAN_Overridable | TEXMAN_TryAny;
	FDoorAnimation anim;
	TArray<FTextureID> frames;
	bool error = false;
	FTextureID v;

	if(!sc.GetNextString()) sc.ScriptMessage(Scanner::ERROR, "Expected string.");
	anim.BaseTexture = CheckForTexture (sc->str, FTexture::TEX_Wall, texflags);

	if (!anim.BaseTexture.Exists())
	{
		error = true;
	}

	while (sc.GetNextString ())
	{
		if (sc->str.Compare ("opensound") == 0)
		{
			if(!sc.GetNextString()) sc.ScriptMessage(Scanner::ERROR, "Expected string.");
			anim.OpenSound = sc->str;
		}
		else if (sc->str.Compare ("closesound") == 0)
		{
			if(!sc.GetNextString()) sc.ScriptMessage(Scanner::ERROR, "Expected string.");
			anim.CloseSound = sc->str;
		}
		else if (sc->str.Compare ("pic") == 0)
		{
			if (sc.CheckToken(TK_IntConst))
			{
				v = anim.BaseTexture + (atoi(sc->str) - 1);
			}
			else
			{
				if(!sc.GetNextString()) sc.ScriptMessage(Scanner::ERROR, "Expected string.");
				v = CheckForTexture (sc->str, FTexture::TEX_Wall, texflags);
				if (!v.Exists() && anim.BaseTexture.Exists() && !error)
				{
					sc.ScriptMessage (Scanner::ERROR, "Unknown texture %s", sc->str.GetChars());
				}
				frames.Push (v);
			}
		}
		else
		{
			sc.Rewind ();
			break;
		}
	}
	if (!error)
	{
		anim.TextureFrames = new FTextureID[frames.Size()];
		memcpy (anim.TextureFrames, &frames[0], sizeof(FTextureID) * frames.Size());
		anim.NumTextureFrames = frames.Size();
		mAnimatedDoors.Push (anim);
	}
}

//==========================================================================
//
// Return index into "DoorAnimations" array for which door type to use
//
//==========================================================================

FDoorAnimation *FTextureManager::FindAnimatedDoor (FTextureID picnum)
{
	unsigned int i;

	for (i = 0; i < mAnimatedDoors.Size(); ++i)
	{
		if (picnum == mAnimatedDoors[i].BaseTexture)
			return &mAnimatedDoors[i];
	}

	return NULL;
}

//==========================================================================
//
// FAnimDef :: SetSwitchTime
//
// Determines when to switch to the next frame.
//
//==========================================================================

void FAnimDef::SetSwitchTime (DWORD mstime)
{
	int speedframe = (AnimType == FAnimDef::ANIM_DiscreteFrames) ? CurFrame : 0;

	SwitchTime = mstime + Frames[speedframe].SpeedMin;
	if (Frames[speedframe].SpeedRange != 0)
	{
		SwitchTime += pr_animatepictures(Frames[speedframe].SpeedRange);
	}
}


//==========================================================================
//
// FTextureManager :: SetTranslation
//
// Sets animation translation for a texture
//
//==========================================================================

void FTextureManager::SetTranslation (FTextureID fromtexnum, FTextureID totexnum)
{
	if ((size_t)fromtexnum.texnum < Translation.Size())
	{
		if ((size_t)totexnum.texnum >= Textures.Size())
		{
			totexnum.texnum = fromtexnum.texnum;
		}
		Translation[fromtexnum.texnum] = totexnum.texnum;
	}
}


//==========================================================================
//
// FTextureManager :: UpdateAnimations
//
// Updates texture translations for each animation and scrolls the skies.
//
//==========================================================================

void FTextureManager::UpdateAnimations (DWORD mstime)
{
	for (unsigned int j = 0; j < mAnimations.Size(); ++j)
	{
		FAnimDef *anim = mAnimations[j];

		// If this is the first time through R_UpdateAnimations, just
		// initialize the anim's switch time without actually animating.
		if (anim->SwitchTime == 0)
		{
			anim->SetSwitchTime (mstime);
		}
		else while (anim->SwitchTime <= mstime)
		{ // Multiple frames may have passed since the last time calling
		  // R_UpdateAnimations, so be sure to loop through them all.

			switch (anim->AnimType)
			{
			default:
			case FAnimDef::ANIM_Forward:
			case FAnimDef::ANIM_DiscreteFrames:
				anim->CurFrame = (anim->CurFrame + 1) % anim->NumFrames;
				break;

			case FAnimDef::ANIM_Backward:
				if (anim->CurFrame == 0)
				{
					anim->CurFrame = anim->NumFrames - 1;
				}
				else
				{
					anim->CurFrame -= 1;
				}
				break;

			case FAnimDef::ANIM_OscillateUp:
				anim->CurFrame = anim->CurFrame + 1;
				if (anim->CurFrame >= anim->NumFrames - 1)
				{
					anim->AnimType = FAnimDef::ANIM_OscillateDown;
				}
				break;

			case FAnimDef::ANIM_OscillateDown:
				anim->CurFrame = anim->CurFrame - 1;
				if (anim->CurFrame == 0)
				{
					anim->AnimType = FAnimDef::ANIM_OscillateUp;
				}
				break;
			}
			anim->SetSwitchTime (mstime);
		}

		if (anim->AnimType == FAnimDef::ANIM_DiscreteFrames)
		{
			SetTranslation (anim->BasePic, anim->Frames[anim->CurFrame].FramePic);
		}
		else
		{
			for (unsigned int i = 0; i < anim->NumFrames; i++)
			{
				SetTranslation (anim->BasePic + i, anim->BasePic + (i + anim->CurFrame) % anim->NumFrames);
			}
		}
	}
}

//==========================================================================
//
// operator<<
//
//==========================================================================

template<> FArchive &operator<< (FArchive &arc, FDoorAnimation* &Doorani)
{
	if (arc.IsStoring())
	{
		arc << Doorani->BaseTexture;
	}
	else
	{
		FTextureID tex;
		arc << tex;
		Doorani = TexMan.FindAnimatedDoor(tex);
	}
	return arc;
}

