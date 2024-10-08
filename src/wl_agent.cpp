// WL_AGENT.C

#include <cmath>
#include <climits>
#include <iostream>

#include "wl_def.h"
#include "id_ca.h"
#include "id_sd.h"
#include "id_vl.h"
#include "id_vh.h"
#include "id_us.h"
#include "actor.h"
#include "thingdef/thingdef.h"
#include "lnspec.h"
#include "wl_agent.h"
#include "a_inventory.h"
#include "a_keys.h"
#include "m_random.h"
#include "g_mapinfo.h"
#include "thinker.h"
#include "r_sprites.h"
#include "wl_draw.h"
#include "wl_game.h"
#include "wl_iwad.h"
#include "wl_loadsave.h"
#include "wl_net.h"
#include "wl_state.h"
#include "wl_play.h"
#include "templates.h"
#include "mapedit.h"
#include "am_map.h"

#include "w_wad.h"
#include "scanner.h"

/*
=============================================================================

								LOCAL CONSTANTS

=============================================================================
*/

#define MAXMOUSETURN    10


#define MOVESCALE       150l
#define ANGLESCALE      20

/*
=============================================================================

								GLOBAL VARIABLES

=============================================================================
*/



//
// player state info
//
AActor			*LastAttacker;

player_t		players[MAXPLAYERS];

void ClipMove (AActor *ob, int32_t xmove, int32_t ymove);
static void Thrust (APlayerPawn *player, angle_t angle, int32_t speed);

int ApplyMobjDamageFactor(int damage, const FName &damagetype,
		const DmgFactors *factors);

EXTERN_CVAR (Int, godmode)

/*
=============================================================================

								GLOBAL VARIABLES

=============================================================================
*/

DBaseStatusBar *StatusBar;

DBaseStatusBar *CreateStatusBar_Blake();
#ifdef USE_GPL
DBaseStatusBar *CreateStatusBar_BlakeAOG();
#endif
DBaseStatusBar *CreateStatusBar_Wolf3D();

void DestroyStatusBar() { delete StatusBar; }
void CreateStatusBar()
{
	if(IWad::CheckGameFilter("Blake"))
		StatusBar = CreateStatusBar_Blake();
#ifdef USE_GPL
	else if(IWad::CheckGameFilter("BlakeAOG"))
		StatusBar = CreateStatusBar_BlakeAOG();
#endif
	else
		StatusBar = CreateStatusBar_Wolf3D();
	atterm(DestroyStatusBar);
}

/*
=============================================================================

								CONTROL STUFF

=============================================================================
*/

/*
======================
=
= CheckWeaponChange
=
= Keys 1-4 change weapons
=
======================
*/

void CheckWeaponChange (AActor *self)
{
	if(self->player->flags & player_t::PF_DISABLESWITCH)
		return;

	AWeapon *newWeapon = NULL;

	TicCmd_t &cmd = control[self->player - players];

	if(cmd.buttonstate[bt_nextweapon] && !cmd.buttonheld[bt_nextweapon])
	{
		newWeapon = self->player->weapons.PickNextWeapon(self->player);
		cmd.buttonheld[bt_nextweapon] = true;
	}
	else if(cmd.buttonstate[bt_prevweapon] && !cmd.buttonheld[bt_prevweapon])
	{
		newWeapon = self->player->weapons.PickPrevWeapon(self->player);
		cmd.buttonheld[bt_prevweapon] = true;
	}
	else
	{
		for(int i = 0;i <= 9;++i)
		{
			if(cmd.buttonstate[bt_slot0 + i] && !cmd.buttonheld[bt_slot0 + i])
			{
				AWeapon *&lastWeapon = self->player->GetWeaponSlotState(i).LastWeapon;
				newWeapon = self->player->weapons.Slots[i].PickWeapon(self->player, lastWeapon);
				cmd.buttonheld[bt_slot0 + i] = true;
				if (newWeapon && newWeapon != self->player->ReadyWeapon)
					lastWeapon = newWeapon;
				break;
			}
		}
	}

	if(newWeapon && newWeapon != self->player->ReadyWeapon)
		self->player->PendingWeapon = newWeapon;
}


/*
======================
=
= ThrustTracker
=
======================
*/
namespace ThrustTracker
{
	TVector2<double> p0;
	int32_t	a0;

	void Start (APlayerPawn *ob)
	{
		ob->forwardthrust = 0;
		ob->sidethrust = 0;
		ob->rotthrust = 0;
		p0 = TVector2<double>(FIXED2FLOAT(ob->x), FIXED2FLOAT(ob->y));
		a0 = ob->angle>>ANGLETOFINESHIFT;
	}

	void Finish (APlayerPawn *ob)
	{
		const TVector2<double> p1(FIXED2FLOAT(ob->x), FIXED2FLOAT(ob->y));

		const TVector2<double> fwd(FIXED2FLOAT(viewcos), -FIXED2FLOAT(viewsin));
		const TVector2<double> side = fwd.Rotated90CCW();

		ob->forwardthrust = FLOAT2FIXED((p1-p0)|fwd);
		ob->sidethrust = FLOAT2FIXED((p1-p0)|side);

		const int32_t a1 = (int32_t)(ob->angle>>ANGLETOFINESHIFT);

		int32_t a = a1 - a0;
		a += (a>(FINEANGLES/2)) ? -FINEANGLES : (a<-(FINEANGLES/2)) ? FINEANGLES : 0;

		ob->rotthrust = a;
	}
}


/*
=======================
=
= ControlMovement
=
= Changes the players's angle and position
=
=======================
*/

void ControlMovement (APlayerPawn *ob)
{
	if(playstate == ex_died)
		return;

	const unsigned int playernum = ob->player - players;
	int controlx = control[playernum].controlx;
	int controly = control[playernum].controly;
	int controlstrafe = control[playernum].controlstrafe;

	int32_t oldx,oldy;
	angle_t angle;
	int strafe = controlstrafe;

	ob->player->thrustspeed = 0;
	ThrustTracker::Start (ob);

	oldx = ob->x;
	oldy = ob->y;

	//
	// side to side move
	//
	if (control[playernum].buttonstate[bt_strafe])
	{
		//
		// strafing
		//
		//
		strafe += controlx;
	}
	else
	{
		if(ob->player->ReadyWeapon && ob->player->ReadyWeapon->fovscale > 0)
			controlx = xs_ToInt(controlx*ob->player->ReadyWeapon->fovscale);

		//
		// not strafing
		//
		ob->angle -= controlx*(ANGLE_1/ANGLESCALE);
	}

	if(strafe)
	{
		// Cap the speed
		if (strafe > 100)
			strafe = 100;
		else if (strafe < -100)
			strafe = -100;

		strafe = FixedMul(ob->speed<<7, FixedMul(strafe, ob->sidemove[abs(strafe) >= RUNMOVE]));

		if (strafe > 0)
		{
			angle = ob->angle - ANGLE_90;
			Thrust (ob,angle,strafe*MOVESCALE);      // move to left
		}
		else if (strafe < 0)
		{
			angle = ob->angle + ANGLE_90;
			Thrust (ob,angle,-strafe*MOVESCALE);     // move to right
		}
	}

	//
	// forward/backwards move
	//
	if (controly < 0)
	{
		if(controly < -100)
			controly = -100;

		controly = FixedMul(ob->speed<<7, FixedMul(controly, ob->forwardmove[controly <= -RUNMOVE]));

		Thrust (ob,ob->angle,-controly*MOVESCALE); // move forwards
	}
	else if (controly > 0)
	{
		if(controly > 100)
			controly = 100;

		controly = FixedMul(ob->speed<<7, FixedMul(controly, ob->forwardmove[controly >= RUNMOVE]));

		angle = ob->angle + ANGLE_180;
		Thrust (ob,angle,controly*MOVESCALE*2/3);          // move backwards
	}

	// Running animation
	if (ob->player->thrustspeed)
	{
		if(ob->SeeState && ob->InStateSequence(ob->SpawnState))
			ob->SetState(ob->SeeState);

		const auto& lastbob = ob->player->lastbob;
		if (lastbob.step && map->IsValidTileCoordinate(ob->tilex, ob->tiley, 0))
		{
			auto get_footsplash_cls = [](auto name) {
				const ClassDef *cls = nullptr;
				if (name != NAME_None && name.IsValidName())
				{
					cls = ClassDef::FindClass(name);
				}
				return cls;
			};

			const MapSpot spot = map->GetSpot(ob->tilex, ob->tiley, 0);
			const ClassDef *cls = nullptr;
			if ((spot->sector != nullptr && (cls =
							get_footsplash_cls(spot->sector->footSplash))) ||
				(cls = get_footsplash_cls(levelInfo->FootSplash)))
			{
				auto splashobj = AActor::Spawn(cls, ob->x, ob->y, 0,
						SPAWN_AllowReplacement);
			}
		}
	}
	else
	{
		if(ob->SpawnState && ob->InStateSequence(ob->SeeState))
			ob->SetState(ob->SpawnState);
	}

	if (gamestate.victoryflag)              // watching the BJ actor
		return;
	
	ThrustTracker::Finish (ob);
}

/*
===============
=
= GiveExtraMan
=
===============
*/

void player_t::GiveExtraMan (int amount)
{
	if (gamestate.difficulty->LivesCount >= 0)
	{
		lives += amount;
		if (lives < 0)
			lives = 0;
		else if(lives > 9)
			lives = 9;
	}
	SD_PlaySound ("misc/1up");
}

/*
===============
=
= GivePoints
=
===============
*/

void player_t::GivePoints (int32_t points)
{
	score += FixedMul(points, gamestate.difficulty->ScoreMultiplier);
	while (score >= nextextra)
	{
		nextextra += EXTRAPOINTS;
		GiveExtraMan (1);
	}
}

/*
===============
=
= TakeDamage
=
===============
*/

static FRandom pr_damageplayer("PlayerTakeDamge");
void player_t::TakeDamage (int points, AActor *attacker, const ClassDef  *damagetype)
{
	LastAttacker = attacker;

	if (gamestate.victoryflag)
		return;
	const int damage = points;
	points = (points*gamestate.difficulty->DamageFactor)>>FRACBITS;
	// fix for baby mode
	if (points <= 0 && damage > 0)
		points = 1;

	if(ReadyWeapon)
	{
		if(points > 0)
		{
			points = FixedMul(points, ReadyWeapon->DamageFactor);
			if(points > 0 && damagetype != nullptr)
				points = ApplyMobjDamageFactor(points,
						damagetype->GetName(),
						ReadyWeapon->GetClass()->DamageFactors);
		}
	}

	NetDPrintf("%s %d points\n", __FUNCTION__, points);

	if (points > 0 && mo->inventory)
	{
		int newdam = points;
		mo->inventory->AbsorbDamage(points,
			damagetype ? damagetype->GetName() : FName(), newdam);
		points = newdam;
	}

	if (attacker)
	{
		auto infomessage = attacker->InfoMessage();
		if (infomessage != NULL)
		{
			auto infomsg_texids = R_GetAttackingFrames(attacker);
			StatusBar->InfoMessage(infomessage, infomsg_texids);
		}
	}

	if (!godmode)
	{
		mo->health = health -= points;
	}

	if (health<=0)
	{
		if(attacker)
		{
			mo->killerx = attacker->x;
			mo->killery = attacker->y;
			mo->killerdamagetype = damagetype;
		}
		mo->target = attacker;
		mo->Die();
		health = 0;
		playstate = ex_died;
		killerobj = attacker;
	}
	else
	{
		if(mo->PainState && pr_damageplayer() < mo->painchance)
			mo->SetState(mo->PainState);
	}

	if (godmode != 2 && (this - players) == ConsolePlayer)
		StartDamageFlash (points);

	if (points > 0)
		SD_PlaySound("player/pain");

	StatusBar->UpdateFace(points);
	StatusBar->DrawStatusBar();
}

/*
=============================================================================

								MOVEMENT

=============================================================================
*/

/*
===================
=
= TryMove
=
= returns true if move ok
= debug: use pointers to optimize
===================
*/

bool TryMove (AActor *ob)
{
	if (noclip || (me_marker && automap == AMA_Normal))
	{
		return (ob->x-ob->radius >= 0 && ob->y-ob->radius >= 0
			&& ob->x+ob->radius < (((int32_t)(map->GetHeader().width))<<TILESHIFT)
			&& ob->y+ob->radius < (((int32_t)(map->GetHeader().height))<<TILESHIFT) );
	}

	int xl,yl,xh,yh,x,y;

	xl = (ob->x-ob->radius) >>TILESHIFT;
	yl = (ob->y-ob->radius) >>TILESHIFT;

	xh = (ob->x+ob->radius) >>TILESHIFT;
	yh = (ob->y+ob->radius) >>TILESHIFT;

	//
	// check for solid walls
	//
	for (y=yl;y<=yh;y++)
	{
		for (x=xl;x<=xh;x++)
		{
			const bool checkLines[4] =
			{
				(ob->x+ob->radius) > ((x+1)<<TILESHIFT),
				(ob->y-ob->radius) < (y<<TILESHIFT),
				(ob->x-ob->radius) < (x<<TILESHIFT),
				(ob->y+ob->radius) > ((y+1)<<TILESHIFT)
			};
			MapSpot spot = map->GetSpot(x, y, 0);
			if(spot->tile)
			{
				// Check pushwall backs
				if(spot->pushAmount != 0)
				{
					switch(spot->pushDirection)
					{
						case MapTile::North:
							if(ob->y-ob->radius <= static_cast<fixed>((y<<TILESHIFT)+((63-spot->pushAmount)<<10)))
								return false;
							break;
						case MapTile::West:
							if(ob->x-ob->radius <= static_cast<fixed>((x<<TILESHIFT)+((63-spot->pushAmount)<<10)))
								return false;
							break;
						case MapTile::East:
							if(ob->x+ob->radius >= static_cast<fixed>((x<<TILESHIFT)+(spot->pushAmount<<10)))
								return false;
							break;
						case MapTile::South:
							if(ob->y+ob->radius >= static_cast<fixed>((y<<TILESHIFT)+(spot->pushAmount<<10)))
								return false;
							break;
					}
				}
				else
				{
					for(unsigned short i = 0;i < 4;++i)
					{
						if(spot->sideSolid[i] && spot->slideAmount[i] != 0xffff && checkLines[i])
							return false;
					}
				}
			}
		}
	}

	//
	// check for actors
	//
	for(AActor::Iterator iter = AActor::GetIterator().Next();iter;)
	{
		// We need to iterate a little awkwardly since the object may disappear
		// on us rendering the next pointer invalid.
		AActor *check = iter;
		iter.Next();

		if(check == ob)
			continue;

		// Allow players to clip through each other for now.
		if(check->player && ob->player)
			continue;

		fixed r = check->radius + ob->radius;
		if(check->flags & FL_SOLID)
		{
			if(abs(ob->x - check->x) > r ||
				abs(ob->y - check->y) > r)
				continue;
			return false;
		}
		else
		{
			if(abs(ob->x - check->x) <= r &&
				abs(ob->y - check->y) <= r)
				check->Touch(ob);
		}
	}

	return true;
}

static void ExecuteWalkTriggers(AActor *ob, MapSpot spot, MapTrigger::Side dir)
{
	if(!spot)
		return;

	for(unsigned int i = spot->triggers.Size();i-- > 0;)
	{
		MapTrigger &trigger = spot->triggers[i];
		if(trigger.playerCross && trigger.activate[dir])
			map->ActivateTrigger(trigger, dir, ob);
	}
}

static void CheckWalkTriggers(AActor *ob, int32_t xmove, int32_t ymove)
{
	MapSpot spot;

	if(ob->fracx <= abs(xmove) || ob->fracx >= 0xFFFF-abs(xmove))
	{
		spot = map->GetSpot((ob->x-xmove)>>FRACBITS, ob->y>>FRACBITS, 0);
		if(xmove > 0)
			ExecuteWalkTriggers(ob, spot->GetAdjacent(MapTile::East), MapTrigger::West);
		else if(xmove < 0)
			ExecuteWalkTriggers(ob, spot->GetAdjacent(MapTile::West), MapTrigger::East);
	}

	if(ob->fracy <= abs(ymove) || ob->fracy >= 0xFFFF-abs(ymove))
	{
		spot = map->GetSpot(ob->x>>FRACBITS, (ob->y-ymove)>>FRACBITS, 0);
		if(ymove > 0)
			ExecuteWalkTriggers(ob, spot->GetAdjacent(MapTile::South), MapTrigger::North);
		else if(ymove < 0)
			ExecuteWalkTriggers(ob, spot->GetAdjacent(MapTile::North), MapTrigger::South);
	}
}


/*
===================
=
= ClipMove
=
===================
*/

void ClipMove (AActor *ob, int32_t xmove, int32_t ymove)
{
	fixed basex = ob->x;
	fixed basey = ob->y;

	ob->x = basex+xmove;
	ob->y = basey+ymove;

	if (TryMove (ob))
	{
		CheckWalkTriggers(ob, xmove, ymove);
		return;
	}

	if (!SD_SoundPlaying())
		SD_PlaySound ("world/hitwall", SD_BOSSWEAPONS);

	ob->x = basex+xmove;
	ob->y = basey;
	if (TryMove (ob))
	{
		CheckWalkTriggers(ob, xmove, 0);
		return;
	}

	ob->x = basex;
	ob->y = basey+ymove;
	if (TryMove (ob))
	{
		CheckWalkTriggers(ob, 0, ymove);
		return;
	}

	ob->x = basex;
	ob->y = basey;
}

//==========================================================================

/*
===================
=
= Thrust
=
===================
*/

static void Thrust (APlayerPawn *player, angle_t angle, int32_t speed)
{
	static const int MAXTHRUST = 0x5800l * 2;
	int32_t xmove,ymove;

	//
	// ZERO FUNNY COUNTER IF MOVED!
	//
	if (speed)
		funnyticount = 0;

	player->player->thrustspeed += speed;
	//
	// moving bounds speed
	//
	if (speed >= MAXTHRUST)
		speed = MAXTHRUST-1;

	xmove = FixedMul(speed,finecosine[angle>>ANGLETOFINESHIFT]);
	ymove = -FixedMul(speed,finesine[angle>>ANGLETOFINESHIFT]);

	ClipMove(player,xmove,ymove);

	player->EnterZone(map->GetSpot(player->tilex, player->tiley, 0)->zone);
}


/*
=============================================================================

								ACTIONS

=============================================================================
*/

//===========================================================================

/*
===============
=
= Cmd_Use
=
===============
*/

void APlayerPawn::Cmd_Use()
{
	int     checkx,checky;
	MapTrigger::Side direction;

	//
	// find which cardinal direction the player is facing
	//
	if (angle < ANGLE_45 || angle > 7*ANGLE_45)
	{
		checkx = tilex + 1;
		checky = tiley;
		direction = MapTrigger::West;
	}
	else if (angle < 3*ANGLE_45)
	{
		checkx = tilex;
		checky = tiley-1;
		direction = MapTrigger::South;
	}
	else if (angle < 5*ANGLE_45)
	{
		checkx = tilex - 1;
		checky = tiley;
		direction = MapTrigger::East;
	}
	else
	{
		checkx = tilex;
		checky = tiley + 1;
		direction = MapTrigger::North;
	}

	bool doNothing = true;
	bool isRepeatable = false;
	BYTE lastTrigger = 0;
	FString infoMessage;
	MapSpot spot = map->GetSpot(checkx, checky, 0);
	for(unsigned int i = 0;i < spot->triggers.Size();++i)
	{
		MapTrigger &trig = spot->triggers[i];
		if(trig.activate[direction] && trig.playerUse)
		{
			if(map->ActivateTrigger(trig, direction, this))
			{
				isRepeatable |= trig.repeatable;
				lastTrigger = trig.action;
				infoMessage = trig.infoMessage;
				doNothing = false;
			}
		}
	}

	TicCmd_t &cmd = control[player - players];
	if(doNothing && (!cmd.buttonheld[bt_use] && Interrogate()))
	{
		SD_PlaySound ("scientist/interrogate");
		cmd.buttonheld[bt_use] = true;
	}
	else if(doNothing)
	{
		SD_PlaySound ("misc/do_nothing", SD_ADLIB);
	}
	else
	{
		if(!infoMessage.IsEmpty())
			StatusBar->InfoMessage(infoMessage);
		P_ChangeSwitchTexture(spot, static_cast<MapTile::Side>(direction), isRepeatable, lastTrigger);
	}
}

/*
===============
=
= Interrogate
=
===============
*/

static FRandom pr_interrogateitem("InterrogateItem");
bool APlayerPawn::Interrogate()
{
	const double range = 64;

	int dist = 0x7fffffff;
	AActor *closest = NULL;
	for(AActor::Iterator check = AActor::GetIterator();check.Next();)
	{
		if(check == this)
			continue;

		if ( (check->flags & FL_SHOOTABLE) && (check->flags & FL_VISABLE)
			&& abs(check->viewx-centerx) < shootdelta &&
			(check->extraflags & FL_FRIENDLY) != 0)
		{
			if (check->transx < dist)
			{
				dist = check->transx;
				closest = check;
			}
		}
	}

	if (!closest || dist-(FRACUNIT/2) > (range/64)*FRACUNIT)
	{
		// missed
		return false;
	}
	// hit something

	// prioritise one of the interrogation items above the others at random
	std::vector<InterrogateItem> prbItems;
	bool chosenRandom = false;
	auto rndval = pr_interrogateitem();

	typedef AActor::InterrogateItemList Li;
	Li *li = closest->GetInterrogateItemList();
	if (li)
	{
		Li::Iterator item = li->Head();
		do
		{
			Li::Iterator interrogateItem = item;

			if(rndval >= 0 && rndval <= interrogateItem->probability)
			{
				rndval = -1;
				prbItems.insert(std::begin(prbItems), interrogateItem);
			}
			else
			{
				prbItems.push_back(interrogateItem);
			}
		}
		while(item.Next());
	}

	// first try to get an item which gives inventory
	for(auto interrogateItem: prbItems)
	{
		const auto usedMask = (1 << interrogateItem.id);

		const auto amtmin = interrogateItem.minAmount;
		const auto amtmax = interrogateItem.maxAmount;
		const auto amount = amtmin + (amtmax > amtmin ?
				pr_interrogateitem(amtmax - amtmin) : 0);

		const ClassDef *cls = ClassDef::FindClass(interrogateItem.dropItem);
		if((closest->interrogateItemsUsed & usedMask) == 0 &&
				cls && cls->IsDescendantOf(NATIVE_CLASS(Inventory)) &&
				GiveInventory(cls, amount))
		{
			closest->interrogateItemsUsed |= usedMask;
			StatusBar->InfoMessage(interrogateItem.infoMessage);
			return true;
		}
	}

	// now try to get an item which does not give inventory
	for(auto interrogateItem: prbItems)
	{
		const auto usedMask = (1 << interrogateItem.id);
		const auto amount = interrogateItem.minAmount;

		const ClassDef *cls = ClassDef::FindClass(interrogateItem.dropItem);
		if((closest->interrogateItemsUsed & usedMask) == 0 && !cls &&
				interrogateItem.minAmount == 0 && interrogateItem.maxAmount == 0)
		{
			const char *msgptr = nullptr;

			std::string msg = interrogateItem.infoMessage.GetChars();
			auto informantKey = "${INFMSG}";
			auto scientistKey = "${SCIMSG}";
			auto key = std::string{};

			if(msg.find(informantKey) != std::string::npos)
			{
				key = informantKey;
#ifdef USE_GPL
				msgptr = map->GetInformantMessage(closest, pr_interrogateitem);
#endif
			}
			else if(msg.find(scientistKey) != std::string::npos)
			{
				key = scientistKey;
#ifdef USE_GPL
				msgptr = map->GetScientistMessage(closest, pr_interrogateitem);
#endif
			}

			if(msgptr != nullptr)
			{
				closest->interrogateItemsUsed |= usedMask;

				auto n = msg.find(key);
				assert(n != std::string::npos);
				msg.erase(n, key.length());
				msg.insert(n, msgptr);

				StatusBar->InfoMessage(msg.c_str());
				return true;
			}
		}
	}

	return false;
}

/*
=============================================================================

								PLAYER CONTROL

=============================================================================
*/

player_t::player_t() : FOV(90), DesiredFOV(90), bob(0), attackheld(false)
{
}

// P_BobWeapon From ZDoom
//============================================================================
//
// P_BobWeapon
//
// [RH] Moved this out of A_WeaponReady so that the weapon can bob every
// tic and not just when A_WeaponReady is called. Not all weapons execute
// A_WeaponReady every tic, and it looks bad if they don't bob smoothly.
//
// [XA] Added new bob styles and exposed bob properties. Thanks, Ryan Cordell!
//
//============================================================================

void player_t::BobWeapon (fixed *x, fixed *y)
{
	AWeapon *weapon;

	weapon = ReadyWeapon;

	if (weapon == NULL || weapon->weaponFlags & WF_DONTBOB)
	{
		*x = *y = 0;
		return;
	}

	// [XA] Get the current weapon's bob properties.
	int bobstyle = weapon->BobStyle;
	int bobspeed = (weapon->BobSpeed * 128) >> 16;
	fixed rangex = weapon->BobRangeX;
	fixed rangey = weapon->BobRangeY;

	// Bob the weapon based on movement speed.
	int angle = (bobspeed*35/TICRATE*gamestate.TimeCount)&FINEMASK;
	fixed curbob = (flags & PF_WEAPONBOBBING) ? bob : 0;

	if (curbob != 0)
	{
		fixed_t bobx = FixedMul(curbob, rangex);
		fixed_t boby = FixedMul(curbob, rangey);
		switch (bobstyle)
		{
		case AWeapon::BobNormal:
			*x = FixedMul(bobx, finecosine[angle]);
			*y = FixedMul(boby, finesine[angle & (FINEANGLES/2-1)]);
			break;

		case AWeapon::BobInverse:
			*x = FixedMul(bobx, finecosine[angle]);
			*y = boby - FixedMul(boby, finesine[angle & (FINEANGLES/2-1)]);
			break;

		case AWeapon::BobAlpha:
			*x = FixedMul(bobx, finesine[angle]);
			*y = FixedMul(boby, finesine[angle & (FINEANGLES/2-1)]);
			break;

		case AWeapon::BobInverseAlpha:
			*x = FixedMul(bobx, finesine[angle]);
			*y = boby - FixedMul(boby, finesine[angle & (FINEANGLES/2-1)]);
			break;

		case AWeapon::BobSmooth:
			*x = FixedMul(bobx, finecosine[angle]);
			*y = (boby - FixedMul(boby, finecosine[angle*2 & (FINEANGLES-1)])) / 2;
			break;

		case AWeapon::BobInverseSmooth:
			*x = FixedMul(bobx, finecosine[angle]);
			*y = (FixedMul(boby, finecosine[angle*2 & (FINEANGLES-1)]) + boby) / 2;
			break;

		case AWeapon::BobThrust:
			{
				*x = 0;

				// Down thrust is faster than up
				// Blake Stone uses a linearly increasing velocity,
				// we use a sin table since it's available and requires no extra storage
				const int thrustPosition = (((angle<<3)*3)&(FRACUNIT-1)) * 3;
				if(thrustPosition < FRACUNIT*2)
					*y = -FixedMul(boby, thrustPosition - finesine[(thrustPosition/2)>>5] - FRACUNIT/2);
				else
					*y = FixedMul(boby, finesine[(thrustPosition - FRACUNIT*2)>>5] - FRACUNIT/2);
			}
			break;
		}
	}
	else
	{
		*x = 0;
		*y = 0;
	}
}

const fixed RAISERANGE = 96*FRACUNIT;
const fixed RAISESPEED = FRACUNIT*6;

void player_t::BringUpWeapon()
{
	if(PendingWeapon == WP_NOCHANGE)
	{
		SetPSprite(ReadyWeapon ? ReadyWeapon->GetReadyState() : NULL, player_t::ps_weapon);
		return;
	}

	psprite[player_t::ps_weapon].sy = RAISERANGE;
	psprite[player_t::ps_weapon].sx = 0;

	ReadyWeapon = PendingWeapon;
	PendingWeapon = WP_NOCHANGE;
	SetPSprite(ReadyWeapon ? ReadyWeapon->GetUpState() : NULL, player_t::ps_weapon);
}
ACTION_FUNCTION(A_Lower)
{
	player_t *player = self->player;

	player->psprite[player_t::ps_weapon].sy += RAISESPEED;
	if(player->psprite[player_t::ps_weapon].sy < RAISERANGE)
		return false;
	player->psprite[player_t::ps_weapon].sy = RAISERANGE;

	if(player->PendingWeapon == WP_NOCHANGE)
		player->PendingWeapon = NULL;

	player->SetPSprite(NULL, player_t::ps_flash);
	// If we're dead, don't bother trying to raise a weapon.
	// In fact, we want to keep the current weapon "up" so that the status bar
	// displays the correct information.
	if(player->state != player_t::PST_DEAD)
		player->BringUpWeapon();
	else
		player->SetPSprite(NULL, player_t::ps_weapon);
	return true;
}
ACTION_FUNCTION(A_Raise)
{
	player_t *player = self->player;

	if(player->PendingWeapon != WP_NOCHANGE)
	{
		player->SetPSprite(player->ReadyWeapon->GetDownState(), player_t::ps_weapon);
		return false;
	}

	player->psprite[player_t::ps_weapon].sy -= RAISESPEED;
	if(player->psprite[player_t::ps_weapon].sy > 0)
		return false;
	player->psprite[player_t::ps_weapon].sy = 0;

	if(player->ReadyWeapon)
		player->SetPSprite(player->ReadyWeapon->GetReadyState(), player_t::ps_weapon);
	else
		player->psprite[player_t::ps_weapon].frame = NULL;
	return true;
}

// Finds the target closest to the player within shooting range.
AActor *player_t::FindTarget()
{
	//
	// find potential targets
	//

	int32_t viewdist = 0x7fffffffl;
	AActor *closest = NULL, *oldclosest = NULL;

	while (1)
	{
		oldclosest = closest;

		for(AActor::Iterator check = AActor::GetIterator();check.Next();)
		{
			if(check == mo)
				continue;

			if ((check->flags & FL_SHOOTABLE))// && (check->flags & FL_VISABLE)
			//	&& abs(check->viewx-centerx) < shootdelta)
			{
				const int dist = MAX(abs(check->x - mo->x), abs(check->y - mo->y));

				float angle = (float) atan2 ((float) (check->y - mo->y), (float) (check->x - mo->x));
				if (angle<0)
					angle = (float) (M_PI*2+angle);
				angle_t iangle = 0-(angle_t)(angle*ANGLE_180/M_PI);
				angle_t lowerAngle = MIN(iangle, mo->angle);
				angle_t upperAngle = MAX(iangle, mo->angle);
				if(MIN(upperAngle - lowerAngle, lowerAngle - upperAngle) <= (ANGLE_90/9) &&
					CheckLine(check, mo) && dist < viewdist)
				{
					viewdist = dist;
					closest = check;
				}
			}
		}

		if (closest == oldclosest)
			return NULL; // no more targets, all missed

		//
		// trace a line from player to enemey
		//
		if (CheckLine(closest, mo))
			break;
	}

	return closest;
}

size_t player_t::PropagateMark()
{
	GC::Mark(mo);
	GC::Mark(camera);
	GC::Mark(ReadyWeapon);
	if(PendingWeapon != WP_NOCHANGE)
		GC::Mark(PendingWeapon);

	std::size_t i;
	for(i = 0; i < weaponSlotStates.size(); i++)
		GC::Mark(weaponSlotStates[i].LastWeapon);

	return sizeof(*this);
}

void player_t::Reborn()
{
	ReadyWeapon = NULL;
	PendingWeapon = WP_NOCHANGE;
	flags = 0;
	FOV = DesiredFOV;
	std::fill(std::begin(weaponSlotStates), std::end(weaponSlotStates),
			WeaponSlotState{});

	if(state == PST_ENTER)
	{
		lives = gamestate.difficulty->LivesCount;
		score = oldscore = 0;
		nextextra = EXTRAPOINTS;
	}

	mo->GiveStartingInventory();
	health = mo->health;

	// Recalculate the projection here so that player classes with differing radii are supported.
	CalcProjection(mo->radius);
}

FArchive &operator<< (FArchive &arc, player_t::WeaponSlotState &player)
{
	arc << player.LastWeapon;
	return arc;
}

FArchive &operator<< (FArchive &arc,
		player_t::TWeaponSlotStates &weaponSlotStates)
{
	std::size_t i;
	for(i = 0; i < weaponSlotStates.size(); i++)
		arc << weaponSlotStates[i];
	return arc;
}

void player_t::Serialize(FArchive &arc)
{
	BYTE state = this->state;
	arc << state;
	this->state = static_cast<State>(state);

	arc << mo
		<< camera
		<< killerobj
		<< oldscore
		<< score
		<< nextextra
		<< lives
		<< health
		<< ReadyWeapon
		<< PendingWeapon
		<< flags
		<< extralight
		<< weaponSlotStates;

	for(unsigned int i = 0;i < NUM_PSPRITES;++i)
	{
		arc << psprite[i].frame
			<< psprite[i].ticcount
			<< psprite[i].sx
			<< psprite[i].sy;
	}

	arc << heightanim.period
		<< heightanim.ticcount
		<< heightanim.startpos
		<< heightanim.endpos;

	arc << lastbob.dir
		<< lastbob.step
		<< lastbob.value
		<< lastbob.inhibitstep;

	if(GameSave::SaveProdVersion >= 0x001002FF && GameSave::SaveVersion > 1374729160)
		arc << FOV << DesiredFOV;

	if(arc.IsLoading())
	{
		mo->SetupWeaponSlots();
		CalcProjection(mo->radius);
	}
}

void player_t::SetPSprite(const Frame *frame, player_t::PSprite layer)
{
	flags &= ~(player_t::PF_READYFLAGS);
	psprite[layer].frame = frame;

	while(psprite[layer].frame)
	{
		if(psprite[layer].frame->offsetX != 0)
			psprite[layer].sx = psprite[layer].frame->offsetX;

		if(psprite[layer].frame->offsetY != 0)
			psprite[layer].sy = psprite[layer].frame->offsetY;

		psprite[layer].ticcount = psprite[layer].frame->GetTics();
		psprite[layer].frame->action(mo, ReadyWeapon, psprite[layer].frame);

		if(mo->player->flags & player_t::PF_WEAPONBOBBING)
			psprite[layer].sx = psprite[layer].sy = 0;

		if(psprite[layer].frame && psprite[layer].ticcount == 0)
			psprite[layer].frame = psprite[layer].frame->next;
		else
			break;
	}
}

void player_t::SetFOV(float newlyDesiredFOV)
{
	DesiredFOV = newlyDesiredFOV;

		// If they're not dead, holding a weapon, and the weapon has a non-zero scale, then we adjust the FOV
	if(state != player_t::PST_DEAD && ReadyWeapon != NULL && ReadyWeapon->fovscale != 0) 
	{
		FOV = -DesiredFOV * ReadyWeapon->fovscale;
		if(mo != NULL) CalcProjection(mo->radius);
	}
	else
	{
		FOV = DesiredFOV;
	}
}

void player_t::AdjustFOV()
{
	// [RH] Zoom the player's FOV
	float desired = DesiredFOV;

	// Adjust FOV using on the currently held weapon.
	if (state != player_t::PST_DEAD &&		// No adjustment while dead.
		ReadyWeapon != NULL &&				// No adjustment if no weapon.
		ReadyWeapon->fovscale != 0)			// No adjustment if the adjustment is zero.
	{

		// A negative scale is used to prevent G_AddViewAngle/G_AddViewPitch
		// from scaling with the FOV scale.
		desired *= fabsf(ReadyWeapon->fovscale);
	}

	if (FOV != desired)
	{
		// Negative FOV means recalculate projection
		if (FOV < 0)
		{
			FOV *= -1;
		}
		else if (fabsf(FOV - desired) < 7.f)
		{
			FOV = desired;
		}
		else
		{
			float zoom = MAX(7.f, fabsf(FOV - desired) * 0.025f);
			if (FOV > desired)
			{
				FOV = FOV - zoom;
			}
			else
			{
				FOV = FOV + zoom;
			}
		}

		CalcProjection(mo->radius);
	}
}

FArchive &operator<< (FArchive &arc, player_t *&player)
{
	return arc.SerializePointer(players, (BYTE**)&player, sizeof(players[0]));
}

/*
===============
=
= SpawnPlayer
=
===============
*/

void SpawnPlayer (int tilex, int tiley, int dir)
{
	for(unsigned int i = 0;i < Net::InitVars.numPlayers;++i)
	{
		player_t &player = players[i];

		player.mo = (APlayerPawn *) AActor::Spawn(gamestate.playerClass, ((int32_t)tilex<<TILESHIFT)+TILEGLOBAL/2, ((int32_t)tiley<<TILESHIFT)+TILEGLOBAL/2, 0, 0);
		player.mo->angle = dir*ANGLE_1;
		player.mo->player = &player;
		Thrust (player.mo,0,0); // set some variables
		player.mo->SetPriority(ThinkerList::PLAYER);

		if(player.state == player_t::PST_ENTER || player.state == player_t::PST_REBORN)
			player.Reborn();

		player.camera = player.mo;
		player.state = player_t::PST_LIVE;
		player.extralight = 0;

		// Re-raise the weapon like Doom if we don't have the flag set in mapinfo.
		if(!levelInfo->SpawnWithWeaponRaised && player.PendingWeapon == WP_NOCHANGE)
			player.PendingWeapon = player.ReadyWeapon;
		player.BringUpWeapon();
	}
}


//===========================================================================

ACTION_FUNCTION(A_BeginHeightAnim)
{
	ACTION_PARAM_FIXED(newheight, 0);
	ACTION_PARAM_INT(period, 1);

	player_t *player = self->player;

	player_t::HeightAnim &ha = player->heightanim;
	ha.period = period;
	ha.ticcount = period;
	ha.startpos = player->mo->viewheight;
	ha.endpos = newheight;
	return true;
}

/*
===============
=
= T_KnifeAttack
=
= Update player hands, and try to do damage when the proper frame is reached
=
===============
*/

static FRandom pr_cwpunch("CustomWpPunch");
ACTION_FUNCTION(A_CustomPunch)
{
	enum
	{
		CPF_USEAMMO = 1,
		CPF_ALWAYSPLAYSOUND = 2
	};

	ACTION_PARAM_INT(damage, 0);
	ACTION_PARAM_BOOL(norandom, 1);
	ACTION_PARAM_INT(flags, 2);
	ACTION_PARAM_STRING(pufftype, 3);
	ACTION_PARAM_DOUBLE(range, 4);
	ACTION_PARAM_FIXED(lifesteal, 5);

	player_t *player = self->player;

	if(flags & CPF_ALWAYSPLAYSOUND)
		SD_PlaySound(player->ReadyWeapon->attacksound, SD_WEAPONS);
	if(range == 0)
		range = 64;

	if(!(player->ReadyWeapon->weaponFlags & WF_NOALERT))
		madenoise = 1;

	// actually fire
	int dist = 0x7fffffff;
	AActor *closest = NULL;
	for(AActor::Iterator check = AActor::GetIterator();check.Next();)
	{
		if(check == self)
			continue;

		if ( (check->flags & FL_SHOOTABLE) && (check->flags & FL_VISABLE)
			&& abs(check->viewx-centerx) < shootdelta)
		{
			if (check->transx < dist)
			{
				dist = check->transx;
				closest = check;
			}
		}
	}

	if (!closest || dist-(FRACUNIT/2) > (range/64)*FRACUNIT)
	{
		// missed
		return false;
	}

	if(!norandom)
		damage *= pr_cwpunch()%8 + 1;

	// hit something
	if(!(flags & CPF_ALWAYSPLAYSOUND))
		SD_PlaySound(player->ReadyWeapon->attacksound, SD_WEAPONS);
	DamageActor(closest, self, damage, player->ReadyWeapon->damagetype);

	// Ammo is only used when hit
	if(flags & CPF_USEAMMO)
	{
		if(!player->ReadyWeapon->DepleteAmmo())
			return true;
	}

	if(lifesteal > 0 && player->health < self->health)
	{
		damage *= lifesteal;
		player->health += damage;
		if(player->health > self->health)
			player->health = self->health;
	}
	return true;
}

static FRandom pr_cwbullet("CustomWpBullet");
ACTION_FUNCTION(A_GunAttack)
{
	enum
	{
		GAF_NORANDOM = 1,
		GAF_NOAMMO = 2,
		GAF_MACDAMAGE = 4
	};

	player_t *player = self->player;
	int      dx,dy,dist;

	ACTION_PARAM_INT(flags, 0);
	ACTION_PARAM_STRING(sound, 1);
	ACTION_PARAM_FIXED(snipe, 2);
	ACTION_PARAM_INT(maxdamage, 3);
	ACTION_PARAM_INT(blocksize, 4);
	ACTION_PARAM_INT(pointblank, 5);
	ACTION_PARAM_INT(longrange, 6);
	ACTION_PARAM_INT(maxrange, 7);

	if(!(flags & GAF_NOAMMO))
	{
		if(!player->ReadyWeapon->DepleteAmmo())
			return false;
	}

	if(sound.Len() == 1 && sound[0] == '*')
		SD_PlaySound(player->ReadyWeapon->attacksound, SD_WEAPONS);
	else
		SD_PlaySound(sound, SD_WEAPONS);

	if(self->MeleeState)
		self->SetState(self->MeleeState);

	if(!(player->ReadyWeapon->weaponFlags & WF_NOALERT))
		madenoise = 1;

	AActor *closest = player->FindTarget();
	if(!closest)
		return false;

	//
	// hit something
	//
	dx = abs(closest->x - self->x);
	dy = abs(closest->y - self->y);
	dist = dx>dy ? dx:dy;

	dist = FixedMul(dist, snipe);
	dist /= blocksize<<9;

	int damage = flags & GAF_NORANDOM ? maxdamage : (1 + (pr_cwbullet()%maxdamage));
	if (dist >= pointblank)
		damage = (flags & GAF_MACDAMAGE) ? damage >> 1 : damage * 2 / 3;
	if (dist >= longrange)
	{
		if ( (pr_cwbullet() % maxrange) < dist)           // missed
			return false;
	}
	DamageActor (closest, self, damage, player->ReadyWeapon->damagetype);
	return true;
}

ACTION_FUNCTION(A_FireCustomMissile)
{
	ACTION_PARAM_STRING(missiletype, 0);
	ACTION_PARAM_DOUBLE(angleOffset, 1);
	ACTION_PARAM_BOOL(useammo, 2);
	ACTION_PARAM_INT(spawnoffset, 3);
	ACTION_PARAM_INT(spawnheight, 4);
	ACTION_PARAM_BOOL(aim, 5);

	if(useammo && !self->player->ReadyWeapon->DepleteAmmo())
		return false;

	if(!(self->player->ReadyWeapon->weaponFlags & WF_NOALERT))
		madenoise = 1;

	if(self->MeleeState)
		self->SetState(self->MeleeState);

	fixed newx = self->x + spawnoffset*finesine[self->angle>>ANGLETOFINESHIFT]/64;
	fixed newy = self->y + spawnoffset*finecosine[self->angle>>ANGLETOFINESHIFT]/64;

	angle_t iangle = self->angle + (angle_t) ((angleOffset*ANGLE_45)/45);

	const ClassDef *cls = ClassDef::FindClass(missiletype);
	if(!cls)
		return false;
	AActor *newobj = AActor::Spawn(cls, newx, newy, 0, SPAWN_AllowReplacement);
	if (self->missileParent)
		newobj->missileParent = self->missileParent;
	else
		newobj->missileParent = self;
	newobj->target = newobj->missileParent;
	newobj->angle = iangle;

	newobj->velx = FixedMul(newobj->speed,finecosine[iangle>>ANGLETOFINESHIFT]);
	newobj->vely = -FixedMul(newobj->speed,finesine[iangle>>ANGLETOFINESHIFT]);
	return true;
}
