
// P_enemy.c

#include "Doomdef.h"
#include "P_local.h"
#include "Soundst.h"
#include "settings.h"

// Macros

#define MAX_BOSS_SPOTS 8

// Types

typedef struct
{
	fixed_t x;
	fixed_t y;
	angle_t angle;
} BossSpot_t;

// Public Data

// boolean fastMonsters = false;

// Private Data

static int BossSpotCount;
static BossSpot_t BossSpots[MAX_BOSS_SPOTS];

//----------------------------------------------------------------------------
//
// PROC P_InitMonsters
//
// Called at level load.
//
//----------------------------------------------------------------------------

void P_InitMonsters(void)
{
	BossSpotCount = 0;
}

//----------------------------------------------------------------------------
//
// PROC P_AddBossSpot
//
//----------------------------------------------------------------------------

void P_AddBossSpot(fixed_t x, fixed_t y, angle_t angle)
{
	if(BossSpotCount == MAX_BOSS_SPOTS)
	{
		Con_Error("Too many boss spots.");
	}
	BossSpots[BossSpotCount].x = x;
	BossSpots[BossSpotCount].y = y;
	BossSpots[BossSpotCount].angle = angle;
	BossSpotCount++;
}

//----------------------------------------------------------------------------
//
// PROC P_RecursiveSound
//
//----------------------------------------------------------------------------

mobj_t *soundtarget;

void P_RecursiveSound(sector_t *sec, int soundblocks)
{
	int i;
	line_t *check;
	sector_t *other;

	// Wake up all monsters in this sector
	if(sec->validcount == Validcount && sec->soundtraversed <= soundblocks+1)
	{ // Already flooded
		return;
	}
	sec->validcount = Validcount;
	sec->soundtraversed = soundblocks+1;
	sec->soundtarget = soundtarget;
	for(i = 0; i < sec->linecount; i++)
	{
		check = sec->Lines[i];
		if(!(check->flags&ML_TWOSIDED))
		{
			continue;
		}
		P_LineOpening(check);
		if(openrange <= 0)
		{ // Closed door
			continue;
		}
		if(sides[check->sidenum[0]].sector == sec)
		{
			other = sides[check->sidenum[1]].sector;
		}
		else
		{
			other = sides[check->sidenum[0]].sector;
		}
		if(check->flags&ML_SOUNDBLOCK)
		{
			if(!soundblocks)
			{
				P_RecursiveSound(other, 1);
			}
		}
		else
		{
			P_RecursiveSound(other, soundblocks);
		}
	}
}

//----------------------------------------------------------------------------
//
// PROC P_NoiseAlert
//
// If a monster yells at a player, it will alert other monsters to the
// player.
//
//----------------------------------------------------------------------------

void P_NoiseAlert(mobj_t *target, mobj_t *emmiter)
{
	soundtarget = target;
	Validcount++;
	P_RecursiveSound(emmiter->subsector->sector, 0);
}

//----------------------------------------------------------------------------
//
// FUNC P_CheckMeleeRange
//
//----------------------------------------------------------------------------

boolean P_CheckMeleeRange(mobj_t *actor)
{
	mobj_t *mo;
	fixed_t dist;

	if(!actor->target)
	{
		return(false);
	}
	mo = actor->target;
	dist = P_ApproxDistance(mo->x-actor->x, mo->y-actor->y);
	if(dist >= MELEERANGE)
	{
		return(false);
	}
	if(!P_CheckSight(actor, mo))
	{
		return(false);
	}
	if(mo->z > actor->z+actor->height)
	{ // Target is higher than the attacker
		return(false);
	}
	else if(actor->z > mo->z+mo->height)
	{ // Attacker is higher
		return(false);
	}
	return(true);
}

//----------------------------------------------------------------------------
//
// FUNC P_CheckMissileRange
//
//----------------------------------------------------------------------------

boolean P_CheckMissileRange(mobj_t *actor)
{
	fixed_t dist;

	if(!P_CheckSight(actor, actor->target))
	{
		return(false);
	}
	if(actor->flags&MF_JUSTHIT)
	{ // The target just hit the enemy, so fight back!
		actor->flags &= ~MF_JUSTHIT;
		return(true);
	}
	if(actor->reactiontime)
	{ // Don't attack yet
		return(false);
	}
	dist = (P_ApproxDistance(actor->x-actor->target->x,
		actor->y-actor->target->y)>>FRACBITS)-64;
	if(!actor->info->meleestate)
	{ // No melee attack, so fire more frequently
		dist -= 128;
	}
	if(actor->type == MT_IMP)
	{ // Imp's fly attack from far away
		dist >>= 1;
	}
	if(dist > 200)
	{
		dist = 200;
	}
	if(P_Random() < dist)
	{
		return(false);
	}
	return(true);
}

/*
================
=
= P_Move
=
= Move in the current direction
= returns false if the move is blocked
================
*/

fixed_t	xspeed[8] = {FRACUNIT,47000,0,-47000,-FRACUNIT,-47000,0,47000};
fixed_t yspeed[8] = {0,47000,FRACUNIT,47000,0,-47000,-FRACUNIT,-47000};

#define	MAXSPECIALCROSS		8
extern	line_t	*spechit[MAXSPECIALCROSS];
extern	int			 numspechit;

boolean P_Move(mobj_t *actor)
{
	fixed_t tryx, tryy, stepx, stepy;
	line_t *ld;
	boolean good;

	if(actor->movedir == DI_NODIR)
	{
		return(false);
	}
	stepx = actor->info->speed/FRACUNIT * xspeed[actor->movedir];
	stepy = actor->info->speed/FRACUNIT * yspeed[actor->movedir];
	tryx = actor->x + stepx;
	tryy = actor->y + stepy;
	if(!P_TryMove(actor, tryx, tryy))
	{ // open any specials
		if(actor->flags&MF_FLOAT && floatok)
		{ // must adjust height
			if(actor->z < tmfloorz)
			{
				actor->z += FLOATSPEED;
			}
			else
			{
				actor->z -= FLOATSPEED;
			}
			actor->flags |= MF_INFLOAT;
			return(true);
		}
		if(!numspechit)
		{
			return false;
		}
		actor->movedir = DI_NODIR;
		good = false;
		while(numspechit--)
		{
			ld = spechit[numspechit];
			// if the special isn't a door that can be opened, return false
			if(P_UseSpecialLine(actor, ld))
			{
				good = true;
			}
		}
		return(good);
	}
	else
	{
		P_SetThingSRVO(actor, stepx, stepy); // "servo": movement smoothing
		actor->flags &= ~MF_INFLOAT;
	}
	if(!(actor->flags&MF_FLOAT))
	{
		if(actor->z > actor->floorz)
		{
			P_HitFloor(actor);
		}
		actor->z = actor->floorz;
	}
	return(true);
}

//----------------------------------------------------------------------------
//
// FUNC P_TryWalk
//
// Attempts to move actor in its current (ob->moveangle) direction.
// If blocked by either a wall or an actor returns FALSE.
// If move is either clear of block only by a door, returns TRUE and sets.
// If a door is in the way, an OpenDoor call is made to start it opening.
//
//----------------------------------------------------------------------------

boolean P_TryWalk(mobj_t *actor)
{
	if(!P_Move(actor))
	{
		return(false);
	}
	actor->movecount = P_Random()&15;
	return(true);
}

/*
================
=
= P_NewChaseDir
=
================
*/

dirtype_t opposite[] =
{DI_WEST, DI_SOUTHWEST, DI_SOUTH, DI_SOUTHEAST, DI_EAST, DI_NORTHEAST,
DI_NORTH, DI_NORTHWEST, DI_NODIR};

dirtype_t diags[] = {DI_NORTHWEST,DI_NORTHEAST,DI_SOUTHWEST,DI_SOUTHEAST};

void P_NewChaseDir (mobj_t *actor)
{
	fixed_t		deltax,deltay;
	dirtype_t	d[3];
	dirtype_t	tdir, olddir, turnaround;

	if (!actor->target)
		Con_Error ("P_NewChaseDir: called with no target");
		
	olddir = actor->movedir;
	turnaround=opposite[olddir];

	deltax = actor->target->x - actor->x;
	deltay = actor->target->y - actor->y;
	if (deltax>10*FRACUNIT)
		d[1]= DI_EAST;
	else if (deltax<-10*FRACUNIT)
		d[1]= DI_WEST;
	else
		d[1]=DI_NODIR;
	if (deltay<-10*FRACUNIT)
		d[2]= DI_SOUTH;
	else if (deltay>10*FRACUNIT)
		d[2]= DI_NORTH;
	else
		d[2]=DI_NODIR;

// try direct route
	if (d[1] != DI_NODIR && d[2] != DI_NODIR)
	{
		actor->movedir = diags[((deltay<0)<<1)+(deltax>0)];
		if (actor->movedir != turnaround && P_TryWalk(actor))
			return;
	}

// try other directions
	if (P_Random() > 200 || abs(deltay)>abs(deltax))
	{
		tdir=d[1];
		d[1]=d[2];
		d[2]=tdir;
	}

	if (d[1]==turnaround)
		d[1]=DI_NODIR;
	if (d[2]==turnaround)
		d[2]=DI_NODIR;
	
	if (d[1]!=DI_NODIR)
	{
		actor->movedir = d[1];
		if (P_TryWalk(actor))
			return;     /*either moved forward or attacked*/
	}

	if (d[2]!=DI_NODIR)
	{
		actor->movedir =d[2];
		if (P_TryWalk(actor))
			return;
	}

/* there is no direct path to the player, so pick another direction */

	if (olddir!=DI_NODIR)
	{
		actor->movedir =olddir;
		if (P_TryWalk(actor))
			return;
	}

	if (P_Random()&1) 	/*randomly determine direction of search*/
	{
		for (tdir=DI_EAST ; tdir<=DI_SOUTHEAST ; tdir++)
		{
			if (tdir!=turnaround)
			{
				actor->movedir =tdir;
				if ( P_TryWalk(actor) )
					return;
			}
		}
	}
	else
	{
		for (tdir=DI_SOUTHEAST ; tdir >= DI_EAST;tdir--)
		{
			if (tdir!=turnaround)
			{
				actor->movedir =tdir;
				if ( P_TryWalk(actor) )
				return;
			}
		}
	}

	if (turnaround !=  DI_NODIR)
	{
		actor->movedir =turnaround;
		if ( P_TryWalk(actor) )
			return;
	}

	actor->movedir = DI_NODIR;		// can't move
}

//---------------------------------------------------------------------------
//
// FUNC P_LookForMonsters
//
//---------------------------------------------------------------------------

#define MONS_LOOK_RANGE (20*64*FRACUNIT)
#define MONS_LOOK_LIMIT 64

boolean P_LookForMonsters(mobj_t *actor)
{
	int count;
	mobj_t *mo;
	thinker_t *think;

	if(!P_CheckSight(players[0].plr->mo, actor))
	{ // Player can't see monster
		return(false);
	}
	count = 0;
	for(think = thinkercap.next; think != &thinkercap; think = think->next)
	{
		if(think->function != P_MobjThinker)
		{ // Not a mobj thinker
			continue;
		}
		mo = (mobj_t *)think;
		if(!(mo->flags&MF_COUNTKILL) || (mo == actor) || (mo->health <= 0))
		{ // Not a valid monster
			continue;
		}
		if(P_ApproxDistance(actor->x-mo->x, actor->y-mo->y)
			> MONS_LOOK_RANGE)
		{ // Out of range
			continue;
		}
		if(P_Random() < 16)
		{ // Skip
			continue;
		}
		if(count++ > MONS_LOOK_LIMIT)
		{ // Stop searching
			return(false);
		}
		if(!P_CheckSight(actor, mo))
		{ // Out of sight
			continue;
		}
		// Found a target monster
		actor->target = mo;
		return(true);
	}
	return(false);
}

/*
================
=
= P_LookForPlayers
=
= If allaround is false, only look 180 degrees in front
= returns true if a player is targeted
================
*/

boolean P_LookForPlayers(mobj_t *actor, boolean allaround)
{
	int c;
	int stop;
	player_t *player;
	sector_t *sector;
	angle_t an;
	fixed_t dist;
	mobj_t *plrmo;
	int playerCount;

	if(!IS_NETGAME && players[0].health <= 0)
	{ // Single player game and player is dead, look for monsters
		return(P_LookForMonsters(actor));
	}

	for(c=playerCount=0; c<MAXPLAYERS; c++)
		if(players[c].plr->ingame) playerCount++;

	// Are there any players?
	if(!playerCount) return false;

	sector = actor->subsector->sector;
	c = 0;
	stop = (actor->lastlook-1)&3;
	for( ; ; actor->lastlook = (actor->lastlook+1)&3 )
	{
		if (!players[actor->lastlook].plr->ingame)
			continue;
			
		if (c++ == 2 || actor->lastlook == stop)
			return false;		// done looking

		player = &players[actor->lastlook];
		plrmo = player->plr->mo;
		if (player->health <= 0)
			continue;		// dead
		if (!P_CheckSight (actor, plrmo))
			continue;		// out of sight
			
		if (!allaround)
		{
			an = R_PointToAngle2 (actor->x, actor->y, 
			plrmo->x, plrmo->y) - actor->angle;
			if (an > ANG90 && an < ANG270)
			{
				dist = P_ApproxDistance (plrmo->x - actor->x,
					plrmo->y - actor->y);
				// if real close, react anyway
				if (dist > MELEERANGE)
					continue;		// behind back
			}
		}
		if(plrmo->flags&MF_SHADOW)
		{ // Player is invisible
			if((P_ApproxDistance(plrmo->x-actor->x,
				plrmo->y-actor->y) > 2*MELEERANGE)
				&& P_ApproxDistance(plrmo->momx, plrmo->momy)
				< 5*FRACUNIT)
			{ // Player is sneaking - can't detect
				return(false);
			}
			if(P_Random() < 225)
			{ // Player isn't sneaking, but still didn't detect
				return(false);
			}
		}
		actor->target = plrmo;
		return(true);
	}
	return(false);
}

/*
===============================================================================

						ACTION ROUTINES

===============================================================================
*/

/*
==============
=
= A_Look
=
= Stay in state until a player is sighted
=
==============
*/

void C_DECL A_Look (mobj_t *actor)
{
	mobj_t		*targ;
	
	actor->threshold = 0;		// any shot will wake up
	targ = actor->subsector->sector->soundtarget;
	if (targ && (targ->flags & MF_SHOOTABLE) )
	{
		actor->target = targ;
		if ( actor->flags & MF_AMBUSH )
		{
			if (P_CheckSight (actor, actor->target))
				goto seeyou;
		}
		else
			goto seeyou;
	}
	
	
	if (!P_LookForPlayers (actor, false) )
		return;
		
// go into chase state
seeyou:
	if (actor->info->seesound)
	{
		int		sound;
		
/*
		switch (actor->info->seesound)
		{
		case sfx_posit1:
		case sfx_posit2:
		case sfx_posit3:
			sound = sfx_posit1+P_Random()%3;
			break;
		case sfx_bgsit1:
		case sfx_bgsit2:
			sound = sfx_bgsit1+P_Random()%2;
			break;
		default:
			sound = actor->info->seesound;
			break;
		}
*/
		sound = actor->info->seesound;
		if(actor->flags2&MF2_BOSS)
		{ // Full volume
			S_StartSound(sound, NULL);
		}
		else
		{
			S_StartSound(sound, actor);
		}
	}
	P_SetMobjState(actor, actor->info->seestate);
}


/*
==============
=
= A_Chase
=
= Actor has a melee attack, so it tries to close as fast as possible
=
==============
*/

void C_DECL A_Chase(mobj_t *actor)
{
	int delta;

	if(actor->reactiontime)
	{
		actor->reactiontime--;
	}

	// Modify target threshold
	if(actor->threshold)
	{
		actor->threshold--;
	}

	if(gameskill == sk_nightmare 
		|| cfg.fastMonsters /*&& !demoplayback && !demorecording 
			&& !IS_NETGAME)*/)
	{ // Monsters move faster in nightmare mode
		actor->tics -= actor->tics/2;
		if(actor->tics < 3)
		{
			actor->tics = 3;
		}
	}

//
// turn towards movement direction if not there yet
//
	if(actor->movedir < 8)
	{
		actor->angle &= (7<<29);
		delta = actor->angle-(actor->movedir << 29);
		if(delta > 0)
		{
			actor->angle -= ANG90/2;
		}
		else if(delta < 0)
		{
			actor->angle += ANG90/2;
		}
	}

	if(!actor->target || !(actor->target->flags&MF_SHOOTABLE))
	{ // look for a new target
		if(P_LookForPlayers(actor, true))
		{ // got a new target
			return;
		}
		P_SetMobjState(actor, actor->info->spawnstate);
		return;
	}
	
//
// don't attack twice in a row
//
	if(actor->flags & MF_JUSTATTACKED)
	{
		actor->flags &= ~MF_JUSTATTACKED;
		if (gameskill != sk_nightmare)
			P_NewChaseDir (actor);
		return;
	}
	
//
// check for melee attack
//	
	if (actor->info->meleestate && P_CheckMeleeRange (actor))
	{
		if (actor->info->attacksound)
		{
			S_StartSound (actor->info->attacksound, actor);
		}
		P_SetMobjState (actor, actor->info->meleestate);
		return;
	}

//
// check for missile attack
//
	if (actor->info->missilestate)
	{
		if (gameskill < sk_nightmare && actor->movecount)
			goto nomissile;
		if (!P_CheckMissileRange (actor))
			goto nomissile;
		P_SetMobjState (actor, actor->info->missilestate);
		actor->flags |= MF_JUSTATTACKED;
		return;
	}
nomissile:

//
// possibly choose another target
//
	if (IS_NETGAME && !actor->threshold && !P_CheckSight (actor, actor->target) )
	{
		if (P_LookForPlayers(actor,true))
			return;		// got a new target
	}
	
//
// chase towards player
//
	if (--actor->movecount<0 || !P_Move (actor))
	{
		P_NewChaseDir (actor);
	}

//
// make active sound
//
	if(actor->info->activesound && P_Random() < 3)
	{
		if(actor->type == MT_WIZARD && P_Random() < 128)
		{
			S_StartSound(actor->info->seesound, actor);
		}
		else if(actor->type == MT_SORCERER2)
		{
			S_StartSound(actor->info->activesound, NULL);
		}
		else
		{
			S_StartSound(actor->info->activesound, actor);
		}
	}
}

//----------------------------------------------------------------------------
//
// PROC A_FaceTarget
//
//----------------------------------------------------------------------------

void C_DECL A_FaceTarget(mobj_t *actor)
{
	if(!actor->target)
	{
		return;
	}
	actor->turntime = true; // $visangle-facetarget
	actor->flags &= ~MF_AMBUSH;
	actor->angle = R_PointToAngle2(actor->x, actor->y, actor->target->x,
		actor->target->y);
	if(actor->target->flags&MF_SHADOW)
	{ // Target is a ghost
		actor->angle += (P_Random()-P_Random())<<21;
	}
}

//----------------------------------------------------------------------------
//
// PROC A_Pain
//
//----------------------------------------------------------------------------

void C_DECL A_Pain(mobj_t *actor)
{
	if(actor->info->painsound)
	{
		S_StartSound(actor->info->painsound, actor);
	}
}

//----------------------------------------------------------------------------
//
// PROC A_DripBlood
//
//----------------------------------------------------------------------------

void C_DECL A_DripBlood(mobj_t *actor)
{
	mobj_t *mo;

	mo = P_SpawnMobj(actor->x+((P_Random()-P_Random())<<11),
		actor->y+((P_Random()-P_Random())<<11), actor->z, MT_BLOOD);
	mo->momx = (P_Random()-P_Random())<<10;
	mo->momy = (P_Random()-P_Random())<<10;
	mo->flags2 |= MF2_LOGRAV;
}

//----------------------------------------------------------------------------
//
// PROC A_KnightAttack
//
//----------------------------------------------------------------------------

void C_DECL A_KnightAttack(mobj_t *actor)
{
	if(!actor->target)
	{
		return;
	}
	if(P_CheckMeleeRange(actor))
	{
		P_DamageMobj(actor->target, actor, actor, HITDICE(3));
		S_StartSound(sfx_kgtat2, actor);
		return;
	}
	// Throw axe
	S_StartSound(actor->info->attacksound, actor);
	if(actor->type == MT_KNIGHTGHOST || P_Random() < 40)
	{ // Red axe
		P_SpawnMissile(actor, actor->target, MT_REDAXE);
		return;
	}
	// Green axe
	P_SpawnMissile(actor, actor->target, MT_KNIGHTAXE);
}

//----------------------------------------------------------------------------
//
// PROC A_ImpExplode
//
//----------------------------------------------------------------------------

void C_DECL A_ImpExplode(mobj_t *actor)
{
	mobj_t *mo;

	mo = P_SpawnMobj(actor->x, actor->y, actor->z, MT_IMPCHUNK1);
	mo->momx = (P_Random() - P_Random ())<<10;
	mo->momy = (P_Random() - P_Random ())<<10;
	mo->momz = 9*FRACUNIT;
	mo = P_SpawnMobj(actor->x, actor->y, actor->z, MT_IMPCHUNK2);
	mo->momx = (P_Random() - P_Random ())<<10;
	mo->momy = (P_Random() - P_Random ())<<10;
	mo->momz = 9*FRACUNIT;
	if(actor->special1 == 666)
	{ // Extreme death crash
		P_SetMobjState(actor, S_IMP_XCRASH1);
	}
}

//----------------------------------------------------------------------------
//
// PROC A_BeastPuff
//
//----------------------------------------------------------------------------

void C_DECL A_BeastPuff(mobj_t *actor)
{
	if(P_Random() > 64)
	{
		P_SpawnMobj(actor->x+((P_Random()-P_Random())<<10),
			actor->y+((P_Random()-P_Random())<<10),
			actor->z+((P_Random()-P_Random())<<10), MT_PUFFY);
	}
}

//----------------------------------------------------------------------------
//
// PROC A_ImpMeAttack
//
//----------------------------------------------------------------------------

void C_DECL A_ImpMeAttack(mobj_t *actor)
{
	if(!actor->target)
	{
		return;
	}
	S_StartSound(actor->info->attacksound, actor);
	if(P_CheckMeleeRange(actor))
	{
		P_DamageMobj(actor->target, actor, actor, 5+(P_Random()&7));
	}
}

//----------------------------------------------------------------------------
//
// PROC A_ImpMsAttack
//
//----------------------------------------------------------------------------

void C_DECL A_ImpMsAttack(mobj_t *actor)
{
	mobj_t *dest;
	angle_t an;
	int dist;

	if(!actor->target || P_Random() > 64)
	{
		P_SetMobjState(actor, actor->info->seestate);
		return;
	}
	dest = actor->target;
	actor->flags |= MF_SKULLFLY;
	S_StartSound(actor->info->attacksound, actor);
	A_FaceTarget(actor);
	an = actor->angle >> ANGLETOFINESHIFT;
	actor->momx = FixedMul(12*FRACUNIT, finecosine[an]);
	actor->momy = FixedMul(12*FRACUNIT, finesine[an]);
	dist = P_ApproxDistance(dest->x-actor->x, dest->y-actor->y);
	dist = dist/(12*FRACUNIT);
	if(dist < 1)
	{
		dist = 1;
	}
	actor->momz = (dest->z+(dest->height>>1)-actor->z)/dist;
}

//----------------------------------------------------------------------------
//
// PROC A_ImpMsAttack2
//
// Fireball attack of the imp leader.
//
//----------------------------------------------------------------------------

void C_DECL A_ImpMsAttack2(mobj_t *actor)
{
	if(!actor->target)
	{
		return;
	}
	S_StartSound(actor->info->attacksound, actor);
	if(P_CheckMeleeRange(actor))
	{
		P_DamageMobj(actor->target, actor, actor, 5+(P_Random()&7));
		return;
	}
	P_SpawnMissile(actor, actor->target, MT_IMPBALL);
}

//----------------------------------------------------------------------------
//
// PROC A_ImpDeath
//
//----------------------------------------------------------------------------

void C_DECL A_ImpDeath(mobj_t *actor)
{
	actor->flags &= ~MF_SOLID;
	actor->flags2 |= MF2_FOOTCLIP;
	if(actor->z <= actor->floorz)
	{
		P_SetMobjState(actor, S_IMP_CRASH1);
	}
}

//----------------------------------------------------------------------------
//
// PROC A_ImpXDeath1
//
//----------------------------------------------------------------------------

void C_DECL A_ImpXDeath1(mobj_t *actor)
{
	actor->flags &= ~MF_SOLID;
	actor->flags |= MF_NOGRAVITY;
	actor->flags2 |= MF2_FOOTCLIP;
	actor->special1 = 666; // Flag the crash routine
}

//----------------------------------------------------------------------------
//
// PROC A_ImpXDeath2
//
//----------------------------------------------------------------------------

void C_DECL A_ImpXDeath2(mobj_t *actor)
{
	actor->flags &= ~MF_NOGRAVITY;
	if(actor->z <= actor->floorz)
	{
		P_SetMobjState(actor, S_IMP_CRASH1);
	}
}

//----------------------------------------------------------------------------
//
// FUNC P_UpdateChicken
//
// Returns true if the chicken morphs.
//
//----------------------------------------------------------------------------

boolean P_UpdateChicken(mobj_t *actor, int tics)
{
	mobj_t *fog;
	fixed_t x;
	fixed_t y;
	fixed_t z;
	mobjtype_t moType;
	mobj_t *mo;
	mobj_t oldChicken;

	actor->special1 -= tics;
	if(actor->special1 > 0)
	{
		return(false);
	}
	moType = actor->special2;
	x = actor->x;
	y = actor->y;
	z = actor->z;
	oldChicken = *actor;
	P_SetMobjState(actor, S_FREETARGMOBJ);
	mo = P_SpawnMobj(x, y, z, moType);
	if(P_TestMobjLocation(mo) == false)
	{ // Didn't fit
		P_RemoveMobj(mo);
		mo = P_SpawnMobj(x, y, z, MT_CHICKEN);
		mo->angle = oldChicken.angle;
		mo->flags = oldChicken.flags;
		mo->health = oldChicken.health;
		mo->target = oldChicken.target;
		mo->special1 = 5*35; // Next try in 5 seconds
		mo->special2 = moType;
		return(false);
	}
	mo->angle = oldChicken.angle;
	mo->target = oldChicken.target;
	fog = P_SpawnMobj(x, y, z+TELEFOGHEIGHT, MT_TFOG);
	S_StartSound(sfx_telept, fog);
	return(true);
}

//----------------------------------------------------------------------------
//
// PROC A_ChicAttack
//
//----------------------------------------------------------------------------

void C_DECL A_ChicAttack(mobj_t *actor)
{
	if(P_UpdateChicken(actor, 18))
	{
		return;
	}
	if(!actor->target)
	{
		return;
	}
	if(P_CheckMeleeRange(actor))
	{
		P_DamageMobj(actor->target, actor, actor, 1+(P_Random()&1));
	}
}

//----------------------------------------------------------------------------
//
// PROC A_ChicLook
//
//----------------------------------------------------------------------------

void C_DECL A_ChicLook(mobj_t *actor)
{
	if(P_UpdateChicken(actor, 10))
	{
		return;
	}
	A_Look(actor);
}

//----------------------------------------------------------------------------
//
// PROC A_ChicChase
//
//----------------------------------------------------------------------------

void C_DECL A_ChicChase(mobj_t *actor)
{
	if(P_UpdateChicken(actor, 3))
	{
		return;
	}
	A_Chase(actor);
}

//----------------------------------------------------------------------------
//
// PROC A_ChicPain
//
//----------------------------------------------------------------------------

void C_DECL A_ChicPain(mobj_t *actor)
{
	if(P_UpdateChicken(actor, 10))
	{
		return;
	}
	S_StartSound(actor->info->painsound, actor);
}

//----------------------------------------------------------------------------
//
// PROC A_Feathers
//
//----------------------------------------------------------------------------

void C_DECL A_Feathers(mobj_t *actor)
{
	int i;
	int count;
	mobj_t *mo;

	if(actor->health > 0)
	{ // Pain
		count = P_Random() < 32 ? 2 : 1;
	}
	else
	{ // Death
		count = 5+(P_Random()&3);
	}
	for(i = 0; i < count; i++)
	{
		mo = P_SpawnMobj(actor->x, actor->y, actor->z+20*FRACUNIT,
			MT_FEATHER);
		mo->target = actor;
		mo->momx = (P_Random()-P_Random())<<8;
		mo->momy = (P_Random()-P_Random())<<8;
		mo->momz = FRACUNIT+(P_Random()<<9);
		P_SetMobjState(mo, S_FEATHER1+(P_Random()&7));
	}
}

//----------------------------------------------------------------------------
//
// PROC A_MummyAttack
//
//----------------------------------------------------------------------------

void C_DECL A_MummyAttack(mobj_t *actor)
{
	if(!actor->target)
	{
		return;
	}
	S_StartSound(actor->info->attacksound, actor);
	if(P_CheckMeleeRange(actor))
	{
		P_DamageMobj(actor->target, actor, actor, HITDICE(2));
		S_StartSound(sfx_mumat2, actor);
		return;
	}
	S_StartSound(sfx_mumat1, actor);
}

//----------------------------------------------------------------------------
//
// PROC A_MummyAttack2
//
// Mummy leader missile attack.
//
//----------------------------------------------------------------------------

void C_DECL A_MummyAttack2(mobj_t *actor)
{
	mobj_t *mo;

	if(!actor->target)
	{
		return;
	}
	//S_StartSound(actor->info->attacksound, actor);
	if(P_CheckMeleeRange(actor))
	{
		P_DamageMobj(actor->target, actor, actor, HITDICE(2));
		return;
	}
	mo = P_SpawnMissile(actor, actor->target, MT_MUMMYFX1);
	//mo = P_SpawnMissile(actor, actor->target, MT_EGGFX);
	if(mo != NULL)
	{
		mo->special1 = (int)actor->target;
	}
}

//----------------------------------------------------------------------------
//
// PROC A_MummyFX1Seek
//
//----------------------------------------------------------------------------

void C_DECL A_MummyFX1Seek(mobj_t *actor)
{
	P_SeekerMissile(actor, ANGLE_1*10, ANGLE_1*20);
}

//----------------------------------------------------------------------------
//
// PROC A_MummySoul
//
//----------------------------------------------------------------------------

void C_DECL A_MummySoul(mobj_t *mummy)
{
	mobj_t *mo;

	mo = P_SpawnMobj(mummy->x, mummy->y, mummy->z+10*FRACUNIT, MT_MUMMYSOUL);
	mo->momz = FRACUNIT;
}

//----------------------------------------------------------------------------
//
// PROC A_Sor1Pain
//
//----------------------------------------------------------------------------

void C_DECL A_Sor1Pain(mobj_t *actor)
{
	actor->special1 = 20; // Number of steps to walk fast
	A_Pain(actor);
}

//----------------------------------------------------------------------------
//
// PROC A_Sor1Chase
//
//----------------------------------------------------------------------------

void C_DECL A_Sor1Chase(mobj_t *actor)
{
	if(actor->special1)
	{
		actor->special1--;
		actor->tics -= 3;
	}
	A_Chase(actor);
}

//----------------------------------------------------------------------------
//
// PROC A_Srcr1Attack
//
// Sorcerer demon attack.
//
//----------------------------------------------------------------------------

void C_DECL A_Srcr1Attack(mobj_t *actor)
{
	mobj_t *mo;
	fixed_t momz;
	angle_t angle;

	if(!actor->target)
	{
		return;
	}
	S_StartSound(actor->info->attacksound, actor);
	if(P_CheckMeleeRange(actor))
	{
		P_DamageMobj(actor->target, actor, actor, HITDICE(8));
		return;
	}
	if(actor->health > (actor->info->spawnhealth/3)*2)
	{ // Spit one fireball
		P_SpawnMissile(actor, actor->target, MT_SRCRFX1);
	}
	else
	{ // Spit three fireballs
		mo = P_SpawnMissile(actor, actor->target, MT_SRCRFX1);
		if(mo)
		{
			momz = mo->momz;
			angle = mo->angle;
			P_SpawnMissileAngle(actor, MT_SRCRFX1, angle-ANGLE_1*3, momz);
			P_SpawnMissileAngle(actor, MT_SRCRFX1, angle+ANGLE_1*3, momz);
		}
		if(actor->health < actor->info->spawnhealth/3)
		{ // Maybe attack again
			if(actor->special1)
			{ // Just attacked, so don't attack again
				actor->special1 = 0;
			}
			else
			{ // Set state to attack again
				actor->special1 = 1;
				P_SetMobjState(actor, S_SRCR1_ATK4);
			}
		}
	}
}

//----------------------------------------------------------------------------
//
// PROC A_SorcererRise
//
//----------------------------------------------------------------------------

void C_DECL A_SorcererRise(mobj_t *actor)
{
	mobj_t *mo;

	actor->flags &= ~MF_SOLID;
	mo = P_SpawnMobj(actor->x, actor->y, actor->z, MT_SORCERER2);
	P_SetMobjState(mo, S_SOR2_RISE1);
	mo->angle = actor->angle;
	mo->target = actor->target;
}

//----------------------------------------------------------------------------
//
// PROC P_DSparilTeleport
//
//----------------------------------------------------------------------------

void P_DSparilTeleport(mobj_t *actor)
{
	int i;
	fixed_t x;
	fixed_t y;
	fixed_t prevX;
	fixed_t prevY;
	fixed_t prevZ;
	mobj_t *mo;

	if(!BossSpotCount)
	{ // No spots
		return;
	}
	i = P_Random();
	do
	{
		i++;
		x = BossSpots[i%BossSpotCount].x;
		y = BossSpots[i%BossSpotCount].y;
	} while(P_ApproxDistance(actor->x-x, actor->y-y) < 128*FRACUNIT);
	prevX = actor->x;
	prevY = actor->y;
	prevZ = actor->z;
	if(P_TeleportMove(actor, x, y))
	{
		mo = P_SpawnMobj(prevX, prevY, prevZ, MT_SOR2TELEFADE);
		S_StartSound(sfx_telept, mo);
		P_SetMobjState(actor, S_SOR2_TELE1);
		S_StartSound(sfx_telept, actor);
		actor->z = actor->floorz;
		actor->angle = BossSpots[i%BossSpotCount].angle;
		actor->momx = actor->momy = actor->momz = 0;
	}
}

//----------------------------------------------------------------------------
//
// PROC A_Srcr2Decide
//
//----------------------------------------------------------------------------

void C_DECL A_Srcr2Decide(mobj_t *actor)
{
	static int chance[] =
	{
		192, 120, 120, 120, 64, 64, 32, 16, 0
	};

	if(!BossSpotCount)
	{ // No spots
		return;
	}
	if(P_Random() < chance[actor->health/(actor->info->spawnhealth/8)])
	{
		P_DSparilTeleport(actor);
	}
}

//----------------------------------------------------------------------------
//
// PROC A_Srcr2Attack
//
//----------------------------------------------------------------------------

void C_DECL A_Srcr2Attack(mobj_t *actor)
{
	int chance;

	if(!actor->target)
	{
		return;
	}
	S_StartSound(actor->info->attacksound, NULL);
	if(P_CheckMeleeRange(actor))
	{
		P_DamageMobj(actor->target, actor, actor, HITDICE(20));
		return;
	}
	chance = actor->health < actor->info->spawnhealth/2 ? 96 : 48;
	if(P_Random() < chance)
	{ // Wizard spawners
		P_SpawnMissileAngle(actor, MT_SOR2FX2,
			actor->angle-ANG45, FRACUNIT/2);
		P_SpawnMissileAngle(actor, MT_SOR2FX2,
			actor->angle+ANG45, FRACUNIT/2);
	}
	else
	{ // Blue bolt
		P_SpawnMissile(actor, actor->target, MT_SOR2FX1);
	}
}

//----------------------------------------------------------------------------
//
// PROC A_BlueSpark
//
//----------------------------------------------------------------------------

void C_DECL A_BlueSpark(mobj_t *actor)
{
	int i;
	mobj_t *mo;

	for(i = 0; i < 2; i++)
	{
		mo = P_SpawnMobj(actor->x, actor->y, actor->z, MT_SOR2FXSPARK);
		mo->momx = (P_Random()-P_Random())<<9;
		mo->momy = (P_Random()-P_Random())<<9;
		mo->momz = FRACUNIT+(P_Random()<<8);
	}
}

//----------------------------------------------------------------------------
//
// PROC A_GenWizard
//
//----------------------------------------------------------------------------

void C_DECL A_GenWizard(mobj_t *actor)
{
	mobj_t *mo;
	mobj_t *fog;

	mo = P_SpawnMobj(actor->x, actor->y,
		actor->z-mobjinfo[MT_WIZARD].height/2, MT_WIZARD);
	if(P_TestMobjLocation(mo) == false)
	{ // Didn't fit
		P_RemoveMobj(mo);
		return;
	}
	actor->momx = actor->momy = actor->momz = 0;
	P_SetMobjState(actor, mobjinfo[actor->type].deathstate);
	actor->flags &= ~MF_MISSILE;
	fog = P_SpawnMobj(actor->x, actor->y, actor->z, MT_TFOG);
	S_StartSound(sfx_telept, fog);
}

//----------------------------------------------------------------------------
//
// PROC A_Sor2DthInit
//
//----------------------------------------------------------------------------

void C_DECL A_Sor2DthInit(mobj_t *actor)
{
	actor->special1 = 7; // Animation loop counter
	P_Massacre(); // Kill monsters early
}

//----------------------------------------------------------------------------
//
// PROC A_Sor2DthLoop
//
//----------------------------------------------------------------------------

void C_DECL A_Sor2DthLoop(mobj_t *actor)
{
	if(--actor->special1)
	{ // Need to loop
		P_SetMobjState(actor, S_SOR2_DIE4);
	}
}

//----------------------------------------------------------------------------
//
// D'Sparil Sound Routines
//
//----------------------------------------------------------------------------

void C_DECL A_SorZap(mobj_t *actor) {S_StartSound(sfx_sorzap, NULL);}
void C_DECL A_SorRise(mobj_t *actor) {S_StartSound(sfx_sorrise, NULL);}
void C_DECL A_SorDSph(mobj_t *actor) {S_StartSound(sfx_sordsph, NULL);}
void C_DECL A_SorDExp(mobj_t *actor) {S_StartSound(sfx_sordexp, NULL);}
void C_DECL A_SorDBon(mobj_t *actor) {S_StartSound(sfx_sordbon, NULL);}
void C_DECL A_SorSightSnd(mobj_t *actor) {S_StartSound(sfx_sorsit, NULL);}

//----------------------------------------------------------------------------
//
// PROC A_MinotaurAtk1
//
// Melee attack.
//
//----------------------------------------------------------------------------

void C_DECL A_MinotaurAtk1(mobj_t *actor)
{
	player_t *player;

	if(!actor->target)
	{
		return;
	}
	S_StartSound(sfx_stfpow, actor);
	if(P_CheckMeleeRange(actor))
	{
		P_DamageMobj(actor->target, actor, actor, HITDICE(4));
		if((player = actor->target->player) != NULL)
		{ // Squish the player
			player->plr->deltaviewheight = -16*FRACUNIT;
		}
	}
}

//----------------------------------------------------------------------------
//
// PROC A_MinotaurDecide
//
// Choose a missile attack.
//
//----------------------------------------------------------------------------

#define MNTR_CHARGE_SPEED (13*FRACUNIT)

void C_DECL A_MinotaurDecide(mobj_t *actor)
{
	angle_t angle;
	mobj_t *target;
	int dist;

	target = actor->target;
	if(!target)
	{
		return;
	}
	S_StartSound(sfx_minsit, actor);
	dist = P_ApproxDistance(actor->x-target->x, actor->y-target->y);
	if(target->z+target->height > actor->z
		&& target->z+target->height < actor->z+actor->height
		&& dist < 8*64*FRACUNIT
		&& dist > 1*64*FRACUNIT
		&& P_Random() < 150)
	{ // Charge attack
		// Don't call the state function right away
		P_SetMobjStateNF(actor, S_MNTR_ATK4_1);
		actor->flags |= MF_SKULLFLY;
		A_FaceTarget(actor);
		angle = actor->angle>>ANGLETOFINESHIFT;
		actor->momx = FixedMul(MNTR_CHARGE_SPEED, finecosine[angle]);
		actor->momy = FixedMul(MNTR_CHARGE_SPEED, finesine[angle]);
		actor->special1 = 35/2; // Charge duration
	}
	else if(target->z == target->floorz
		&& dist < 9*64*FRACUNIT
		&& P_Random() < 220)
	{ // Floor fire attack
		P_SetMobjState(actor, S_MNTR_ATK3_1);
		actor->special2 = 0;
	}
	else
	{ // Swing attack
		A_FaceTarget(actor);
		// Don't need to call P_SetMobjState because the current state
		// falls through to the swing attack
	}
}

//----------------------------------------------------------------------------
//
// PROC A_MinotaurCharge
//
//----------------------------------------------------------------------------

void C_DECL A_MinotaurCharge(mobj_t *actor)
{
	mobj_t *puff;

	if(actor->special1)
	{
		puff = P_SpawnMobj(actor->x, actor->y, actor->z, MT_PHOENIXPUFF);
		puff->momz = 2*FRACUNIT;
		actor->special1--;
	}
	else
	{
		actor->flags &= ~MF_SKULLFLY;
		P_SetMobjState(actor, actor->info->seestate);
	}
}

//----------------------------------------------------------------------------
//
// PROC A_MinotaurAtk2
//
// Swing attack.
//
//----------------------------------------------------------------------------

void C_DECL A_MinotaurAtk2(mobj_t *actor)
{
	mobj_t *mo;
	angle_t angle;
	fixed_t momz;

	if(!actor->target)
	{
		return;
	}
	S_StartSound(sfx_minat2, actor);
	if(P_CheckMeleeRange(actor))
	{
		P_DamageMobj(actor->target, actor, actor, HITDICE(5));
		return;
	}
	mo = P_SpawnMissile(actor, actor->target, MT_MNTRFX1);
	if(mo)
	{
		S_StartSound(sfx_minat2, mo);
		momz = mo->momz;
		angle = mo->angle;
		P_SpawnMissileAngle(actor, MT_MNTRFX1, angle-(ANG45/8), momz);
		P_SpawnMissileAngle(actor, MT_MNTRFX1, angle+(ANG45/8), momz);
		P_SpawnMissileAngle(actor, MT_MNTRFX1, angle-(ANG45/16), momz);
		P_SpawnMissileAngle(actor, MT_MNTRFX1, angle+(ANG45/16), momz);
	}
}

//----------------------------------------------------------------------------
//
// PROC A_MinotaurAtk3
//
// Floor fire attack.
//
//----------------------------------------------------------------------------

void C_DECL A_MinotaurAtk3(mobj_t *actor)
{
	mobj_t *mo;
	player_t *player;

	if(!actor->target)
	{
		return;
	}
	if(P_CheckMeleeRange(actor))
	{
		P_DamageMobj(actor->target, actor, actor, HITDICE(5));
		if((player = actor->target->player) != NULL)
		{ // Squish the player
			player->plr->deltaviewheight = -16*FRACUNIT;
		}
	}
	else
	{
		mo = P_SpawnMissile(actor, actor->target, MT_MNTRFX2);
		if(mo != NULL)
		{
			S_StartSound(sfx_minat1, mo);
		}
	}
	if(P_Random() < 192 && actor->special2 == 0)
	{
		P_SetMobjState(actor, S_MNTR_ATK3_4);
		actor->special2 = 1;
	}
}

//----------------------------------------------------------------------------
//
// PROC A_MntrFloorFire
//
//----------------------------------------------------------------------------

void C_DECL A_MntrFloorFire(mobj_t *actor)
{
	mobj_t *mo;

	actor->z = actor->floorz;
	mo = P_SpawnMobj(actor->x+((P_Random()-P_Random())<<10),
		actor->y+((P_Random()-P_Random())<<10), ONFLOORZ, MT_MNTRFX3);
	mo->target = actor->target;
	mo->momx = 1; // Force block checking
	P_CheckMissileSpawn(mo);
}

//----------------------------------------------------------------------------
//
// PROC A_BeastAttack
//
//----------------------------------------------------------------------------

void C_DECL A_BeastAttack(mobj_t *actor)
{
	if(!actor->target)
	{
		return;
	}
	S_StartSound(actor->info->attacksound, actor);
	if(P_CheckMeleeRange(actor))
	{
		P_DamageMobj(actor->target, actor, actor, HITDICE(3));
		return;
	}
	P_SpawnMissile(actor, actor->target, MT_BEASTBALL);
}

//----------------------------------------------------------------------------
//
// PROC A_HeadAttack
//
//----------------------------------------------------------------------------

void C_DECL A_HeadAttack(mobj_t *actor)
{
	int i;
	mobj_t *fire;
	mobj_t *baseFire;
	mobj_t *mo;
	mobj_t *target;
	int randAttack;
	static int atkResolve1[] = { 50, 150 };
	static int atkResolve2[] = { 150, 200 };
	int dist;

	// Ice ball		(close 20% : far 60%)
	// Fire column	(close 40% : far 20%)
	// Whirlwind	(close 40% : far 20%)
	// Distance threshold = 8 cells

	target = actor->target;
	if(target == NULL)
	{
		return;
	}
	A_FaceTarget(actor);
	if(P_CheckMeleeRange(actor))
	{
		P_DamageMobj(target, actor, actor, HITDICE(6));
		return;
	}
	dist = P_ApproxDistance(actor->x-target->x, actor->y-target->y)
		> 8*64*FRACUNIT;
	randAttack = P_Random();
	if(randAttack < atkResolve1[dist])
	{ // Ice ball
		P_SpawnMissile(actor, target, MT_HEADFX1);
		S_StartSound(sfx_hedat2, actor);	
	}
	else if(randAttack < atkResolve2[dist])
	{ // Fire column
		baseFire = P_SpawnMissile(actor, target, MT_HEADFX3);
		if(baseFire != NULL)
		{
			P_SetMobjState(baseFire, S_HEADFX3_4); // Don't grow
			for(i = 0; i < 5; i++)
			{
				fire = P_SpawnMobj(baseFire->x, baseFire->y,
					baseFire->z, MT_HEADFX3);
				if(i == 0)
				{
					S_StartSound(sfx_hedat1, actor);
				}
				fire->target = baseFire->target;
				fire->angle = baseFire->angle;
				fire->momx = baseFire->momx;
				fire->momy = baseFire->momy;
				fire->momz = baseFire->momz;
				fire->damage = 0;
				fire->health = (i+1)*2;
				P_CheckMissileSpawn(fire);
			}
		}
	}
	else
	{ // Whirlwind
		mo = P_SpawnMissile(actor, target, MT_WHIRLWIND);
		if(mo != NULL)
		{
			mo->z -= 32*FRACUNIT;
			mo->special1 = (int)target;
			mo->special2 = 50; // Timer for active sound
			mo->health = 20*TICSPERSEC; // Duration
			S_StartSound(sfx_hedat3, actor);			
		}
	}
}

//----------------------------------------------------------------------------
//
// PROC A_WhirlwindSeek
//
//----------------------------------------------------------------------------

void C_DECL A_WhirlwindSeek(mobj_t *actor)
{
	actor->health -= 3;
	if(actor->health < 0)
	{
		actor->momx = actor->momy = actor->momz = 0;
		P_SetMobjState(actor, mobjinfo[actor->type].deathstate);
		actor->flags &= ~MF_MISSILE;
		return;
	}
	if((actor->special2 -= 3) < 0)
	{
		actor->special2 = 58+(P_Random()&31);
		S_StartSound(sfx_hedat3, actor);
	}
	if(actor->special1
		&& (((mobj_t *)(actor->special1))->flags&MF_SHADOW))
	{
		return;
	}
	P_SeekerMissile(actor, ANGLE_1*10, ANGLE_1*30);
}

//----------------------------------------------------------------------------
//
// PROC A_HeadIceImpact
//
//----------------------------------------------------------------------------

void C_DECL A_HeadIceImpact(mobj_t *ice)
{
	int i;
	angle_t angle;
	mobj_t *shard;

	for(i = 0; i < 8; i++)
	{
		shard = P_SpawnMobj(ice->x, ice->y, ice->z, MT_HEADFX2);
		angle = i*ANG45;
		shard->target = ice->target;
		shard->angle = angle;
		angle >>= ANGLETOFINESHIFT;
		shard->momx = FixedMul(shard->info->speed, finecosine[angle]);
		shard->momy = FixedMul(shard->info->speed, finesine[angle]);
		shard->momz = (fixed_t) (-.6*FRACUNIT);
		P_CheckMissileSpawn(shard);
	}
}

//----------------------------------------------------------------------------
//
// PROC A_HeadFireGrow
//
//----------------------------------------------------------------------------

void C_DECL A_HeadFireGrow(mobj_t *fire)
{
	fire->health--;
	fire->z += 9*FRACUNIT;
	if(fire->health == 0)
	{
		fire->damage = fire->info->damage;
		P_SetMobjState(fire, S_HEADFX3_4);
	}
}

//----------------------------------------------------------------------------
//
// PROC A_SnakeAttack
//
//----------------------------------------------------------------------------

void C_DECL A_SnakeAttack(mobj_t *actor)
{
	if(!actor->target)
	{
		P_SetMobjState(actor, S_SNAKE_WALK1);
		return;
	}
	S_StartSound(actor->info->attacksound, actor);
	A_FaceTarget(actor);
	P_SpawnMissile(actor, actor->target, MT_SNAKEPRO_A);
}

//----------------------------------------------------------------------------
//
// PROC A_SnakeAttack2
//
//----------------------------------------------------------------------------

void C_DECL A_SnakeAttack2(mobj_t *actor)
{
	if(!actor->target)
	{
		P_SetMobjState(actor, S_SNAKE_WALK1);
		return;
	}
	S_StartSound(actor->info->attacksound, actor);
	A_FaceTarget(actor);
	P_SpawnMissile(actor, actor->target, MT_SNAKEPRO_B);
}

//----------------------------------------------------------------------------
//
// PROC A_ClinkAttack
//
//----------------------------------------------------------------------------

void C_DECL A_ClinkAttack(mobj_t *actor)
{
	int damage;

	if(!actor->target)
	{
		return;
	}
	S_StartSound(actor->info->attacksound, actor);
	if(P_CheckMeleeRange(actor))
	{
		damage = ((P_Random()%7)+3);
		P_DamageMobj(actor->target, actor, actor, damage);
	}
}

//----------------------------------------------------------------------------
//
// PROC A_GhostOff
//
//----------------------------------------------------------------------------

void C_DECL A_GhostOff(mobj_t *actor)
{
	actor->flags &= ~MF_SHADOW;
}

//----------------------------------------------------------------------------
//
// PROC A_WizAtk1
//
//----------------------------------------------------------------------------

void C_DECL A_WizAtk1(mobj_t *actor)
{
	A_FaceTarget(actor);
	actor->flags &= ~MF_SHADOW;
}

//----------------------------------------------------------------------------
//
// PROC A_WizAtk2
//
//----------------------------------------------------------------------------

void C_DECL A_WizAtk2(mobj_t *actor)
{
	A_FaceTarget(actor);
	actor->flags |= MF_SHADOW;
}

//----------------------------------------------------------------------------
//
// PROC A_WizAtk3
//
//----------------------------------------------------------------------------

void C_DECL A_WizAtk3(mobj_t *actor)
{
	mobj_t *mo;
	angle_t angle;
	fixed_t momz;

	actor->flags &= ~MF_SHADOW;
	if(!actor->target)
	{
		return;
	}
	S_StartSound(actor->info->attacksound, actor);
	if(P_CheckMeleeRange(actor))
	{
		P_DamageMobj(actor->target, actor, actor, HITDICE(4));
		return;
	}
	mo = P_SpawnMissile(actor, actor->target, MT_WIZFX1);
	if(mo)
	{
		momz = mo->momz;
		angle = mo->angle;
		P_SpawnMissileAngle(actor, MT_WIZFX1, angle-(ANG45/8), momz);
		P_SpawnMissileAngle(actor, MT_WIZFX1, angle+(ANG45/8), momz);
	}
}

//----------------------------------------------------------------------------
//
// PROC A_Scream
//
//----------------------------------------------------------------------------

void C_DECL A_Scream(mobj_t *actor)
{
	switch(actor->type)
	{
		case MT_CHICPLAYER:
		case MT_SORCERER1:
		case MT_MINOTAUR:
			// Make boss death sounds full volume
			S_StartSound(actor->info->deathsound, NULL);
			break;
		case MT_PLAYER:
			// Handle the different player death screams
			if(actor->special1 < 10)
			{ // Wimpy death sound
				S_StartSound(sfx_plrwdth, actor);
			}
			else if(actor->health > -50)
			{ // Normal death sound
				S_StartSound(actor->info->deathsound, actor);
			}
			else if(actor->health > -100)
			{ // Crazy death sound
				S_StartSound(sfx_plrcdth, actor);
			}
			else
			{ // Extreme death sound
				S_StartSound(sfx_gibdth, actor);
			}
			break;
		default:
			S_StartSound(actor->info->deathsound, actor);
			break;
	}
}

//---------------------------------------------------------------------------
//
// PROC P_DropItem
//
//---------------------------------------------------------------------------

void P_DropItem(mobj_t *source, mobjtype_t type, int special, int chance)
{
	mobj_t *mo;

	if(P_Random() > chance)
	{
		return;
	}
	mo = P_SpawnMobj(source->x, source->y,
		source->z+(source->height>>1), type);
	mo->momx = (P_Random()-P_Random())<<8;
	mo->momy = (P_Random()-P_Random())<<8;
	mo->momz = FRACUNIT*5+(P_Random()<<10);
	mo->flags |= MF_DROPPED;
	mo->health = special;
}

//----------------------------------------------------------------------------
//
// PROC A_NoBlocking
//
//----------------------------------------------------------------------------

void C_DECL A_NoBlocking(mobj_t *actor)
{
	actor->flags &= ~MF_SOLID;
	// Check for monsters dropping things
	switch(actor->type)
	{
		case MT_MUMMY:
		case MT_MUMMYLEADER:
		case MT_MUMMYGHOST:
		case MT_MUMMYLEADERGHOST:
			P_DropItem(actor, MT_AMGWNDWIMPY, 3, 84);
			break;
		case MT_KNIGHT:
		case MT_KNIGHTGHOST:
			P_DropItem(actor, MT_AMCBOWWIMPY, 5, 84);
			break;
		case MT_WIZARD:
			P_DropItem(actor, MT_AMBLSRWIMPY, 10, 84);
			P_DropItem(actor, MT_ARTITOMEOFPOWER, 0, 4);
			break;
		case MT_HEAD:
			P_DropItem(actor, MT_AMBLSRWIMPY, 10, 84);
			P_DropItem(actor, MT_ARTIEGG, 0, 51);
			break;
		case MT_BEAST:
			P_DropItem(actor, MT_AMCBOWWIMPY, 10, 84);
			break;
		case MT_CLINK:
			P_DropItem(actor, MT_AMSKRDWIMPY, 20, 84);
			break;
		case MT_SNAKE:
			P_DropItem(actor, MT_AMPHRDWIMPY, 5, 84);
			break;
		case MT_MINOTAUR:
			P_DropItem(actor, MT_ARTISUPERHEAL, 0, 51);
			P_DropItem(actor, MT_AMPHRDWIMPY, 10, 84);
			break;
		default:
			break;
	}
}

//----------------------------------------------------------------------------
//
// PROC A_Explode
//
// Handles a bunch of exploding things.
//
//----------------------------------------------------------------------------

void C_DECL A_Explode(mobj_t *actor)
{
	int damage;

	damage = 128;
	switch(actor->type)
	{
		case MT_FIREBOMB: // Time Bombs
			actor->z += 32*FRACUNIT;
			actor->flags &= ~MF_SHADOW;
			actor->flags |= MF_BRIGHTSHADOW | MF_VIEWALIGN;
			break;
		case MT_MNTRFX2: // Minotaur floor fire
			damage = 24;
			break;
		case MT_SOR2FX1: // D'Sparil missile
			damage = 80+(P_Random()&31);
			break;
		default:
			break;
	}
	P_RadiusAttack(actor, actor->target, damage);
	P_HitFloor(actor);
}

//----------------------------------------------------------------------------
//
// PROC A_PodPain
//
//----------------------------------------------------------------------------

void C_DECL A_PodPain(mobj_t *actor)
{
	int i;
	int count;
	int chance;
	mobj_t *goo;

	chance = P_Random();
	if(chance < 128)
	{
		return;
	}
	count = chance > 240 ? 2 : 1;
	for(i = 0; i < count; i++)
	{
		goo = P_SpawnMobj(actor->x, actor->y,
			actor->z+48*FRACUNIT, MT_PODGOO);
		goo->target = actor;
		goo->momx = (P_Random()-P_Random())<<9;
		goo->momy = (P_Random()-P_Random())<<9;
		goo->momz = FRACUNIT/2+(P_Random()<<9);
	}
}

//----------------------------------------------------------------------------
//
// PROC A_RemovePod
//
//----------------------------------------------------------------------------

void C_DECL A_RemovePod(mobj_t *actor)
{
	mobj_t *mo;

	if(actor->special2)
	{
		mo = (mobj_t *)actor->special2;
		if(mo->special1 > 0)
		{
			mo->special1--;
		}
	}
}

//----------------------------------------------------------------------------
//
// PROC A_MakePod
//
//----------------------------------------------------------------------------

#define MAX_GEN_PODS 16

void C_DECL A_MakePod(mobj_t *actor)
{
	mobj_t *mo;
	fixed_t x;
	fixed_t y;
	fixed_t z;

	if(actor->special1 == MAX_GEN_PODS)
	{ // Too many generated pods
		return;
	}
	x = actor->x;
	y = actor->y;
	z = actor->z;
	mo = P_SpawnMobj(x, y, ONFLOORZ, MT_POD);
	if(P_CheckPosition(mo, x, y) == false)
	{ // Didn't fit
		P_RemoveMobj(mo);
		return;
	}
	P_SetMobjState(mo, S_POD_GROW1);
	P_ThrustMobj(mo, P_Random()<<24, (fixed_t)(4.5*FRACUNIT));
	S_StartSound(sfx_newpod, mo);
	actor->special1++; // Increment generated pod count
	mo->special2 = (int)actor; // Link the generator to the pod
	return;
}

//----------------------------------------------------------------------------
//
// PROC P_Massacre
//
// Kills all monsters.
//
//----------------------------------------------------------------------------

void P_Massacre(void)
{
	mobj_t *mo;
	thinker_t *think;

	// Only massacre when in a level.
	if(gamestate != GS_LEVEL) return;

	for(think = thinkercap.next; think != &thinkercap;
		think = think->next)
	{
		if(think->function != P_MobjThinker)
		{ // Not a mobj thinker
			continue;
		}
		mo = (mobj_t *)think;
		if((mo->flags&MF_COUNTKILL) && (mo->health > 0))
		{
			P_DamageMobj(mo, NULL, NULL, 10000);
		}
	}
}

//----------------------------------------------------------------------------
//
// PROC A_BossDeath
//
// Trigger special effects if all bosses are dead.
//
//----------------------------------------------------------------------------

void C_DECL A_BossDeath(mobj_t *actor)
{
	mobj_t *mo;
	thinker_t *think;
	line_t dummyLine;
	static mobjtype_t bossType[6] =
	{
		MT_HEAD,
		MT_MINOTAUR,
		MT_SORCERER2,
		MT_HEAD,
		MT_MINOTAUR,
		-1
	};

	if(gamemap != 8)
	{ // Not a boss level
		return;
	}
	if(actor->type != bossType[gameepisode-1])
	{ // Not considered a boss in this episode
		return;
	}
	// Make sure all bosses are dead
	for(think = thinkercap.next; think != &thinkercap; think = think->next)
	{
		if(think->function != P_MobjThinker)
		{ // Not a mobj thinker
			continue;
		}
		mo = (mobj_t *)think;
		if((mo != actor) && (mo->type == actor->type) && (mo->health > 0))
		{ // Found a living boss
			return;
		}
	}
	if(gameepisode > 1)
	{ // Kill any remaining monsters
		P_Massacre();
	}
	dummyLine.tag = 666;
	EV_DoFloor(&dummyLine, lowerFloor);
}

//----------------------------------------------------------------------------
//
// PROC A_ESound
//
//----------------------------------------------------------------------------

void C_DECL A_ESound(mobj_t *mo)
{
	int sound;

	switch(mo->type)
	{
		case MT_SOUNDWATERFALL:
			sound = sfx_waterfl;
			break;
		case MT_SOUNDWIND:
			sound = sfx_wind;
			break;
		default:
			break;
	}
	S_StartSound(sound, mo);
}

//----------------------------------------------------------------------------
//
// PROC A_SpawnTeleGlitter
//
//----------------------------------------------------------------------------

void C_DECL A_SpawnTeleGlitter(mobj_t *actor)
{
	mobj_t *mo;

	mo = P_SpawnMobj(actor->x+((P_Random()&31)-16)*FRACUNIT,
		actor->y+((P_Random()&31)-16)*FRACUNIT,
		actor->subsector->sector->floorheight, MT_TELEGLITTER);
	mo->momz = FRACUNIT/4;
}

//----------------------------------------------------------------------------
//
// PROC A_SpawnTeleGlitter2
//
//----------------------------------------------------------------------------

void C_DECL A_SpawnTeleGlitter2(mobj_t *actor)
{
	mobj_t *mo;

	mo = P_SpawnMobj(actor->x+((P_Random()&31)-16)*FRACUNIT,
		actor->y+((P_Random()&31)-16)*FRACUNIT,
		actor->subsector->sector->floorheight, MT_TELEGLITTER2);
	mo->momz = FRACUNIT/4;
}

//----------------------------------------------------------------------------
//
// PROC A_AccTeleGlitter
//
//----------------------------------------------------------------------------

void C_DECL A_AccTeleGlitter(mobj_t *actor)
{
	if(++actor->health > 35)
	{
		actor->momz += actor->momz/2;
	}
}

//----------------------------------------------------------------------------
//
// PROC A_InitKeyGizmo
//
//----------------------------------------------------------------------------

void C_DECL A_InitKeyGizmo(mobj_t *gizmo)
{
	mobj_t *mo;
	statenum_t state;

	switch(gizmo->type)
	{
		case MT_KEYGIZMOBLUE:
			state = S_KGZ_BLUEFLOAT1;
			break;
		case MT_KEYGIZMOGREEN:
			state = S_KGZ_GREENFLOAT1;
			break;
		case MT_KEYGIZMOYELLOW:
			state = S_KGZ_YELLOWFLOAT1;
			break;
		default:
			break;
	}
	mo = P_SpawnMobj(gizmo->x, gizmo->y, gizmo->z+60*FRACUNIT,
		MT_KEYGIZMOFLOAT);
	P_SetMobjState(mo, state);
}

//----------------------------------------------------------------------------
//
// PROC A_VolcanoSet
//
//----------------------------------------------------------------------------

void C_DECL A_VolcanoSet(mobj_t *volcano)
{
	volcano->tics = 105+(P_Random()&127);
}

//----------------------------------------------------------------------------
//
// PROC A_VolcanoBlast
//
//----------------------------------------------------------------------------

void C_DECL A_VolcanoBlast(mobj_t *volcano)
{
	int i;
	int count;
	mobj_t *blast;
	angle_t angle;

	count = 1+(P_Random()%3);
	for(i = 0; i < count; i++)
	{
		blast = P_SpawnMobj(volcano->x, volcano->y,
			volcano->z+44*FRACUNIT, MT_VOLCANOBLAST); // MT_VOLCANOBLAST
		blast->target = volcano;
		angle = P_Random()<<24;
		blast->angle = angle;
		angle >>= ANGLETOFINESHIFT;
		blast->momx = FixedMul(1*FRACUNIT, finecosine[angle]);
		blast->momy = FixedMul(1*FRACUNIT, finesine[angle]);
		blast->momz = (fixed_t) ((2.5*FRACUNIT)+(P_Random()<<10));
		S_StartSound(sfx_volsht, blast);
		P_CheckMissileSpawn(blast);
	}
}

//----------------------------------------------------------------------------
//
// PROC A_VolcBallImpact
//
//----------------------------------------------------------------------------

void C_DECL A_VolcBallImpact(mobj_t *ball)
{
	int i;
	mobj_t *tiny;
	angle_t angle;

	if(ball->z <= ball->floorz)
	{
		ball->flags |= MF_NOGRAVITY;
		ball->flags2 &= ~MF2_LOGRAV;
		ball->z += 28*FRACUNIT;
		//ball->momz = 3*FRACUNIT;
	}
	P_RadiusAttack(ball, ball->target, 25);
	for(i = 0; i < 4; i++)
	{
		tiny = P_SpawnMobj(ball->x, ball->y, ball->z, MT_VOLCANOTBLAST);
		tiny->target = ball;
		angle = i*ANG90;
		tiny->angle = angle;
		angle >>= ANGLETOFINESHIFT;
		tiny->momx = FixedMul(FRACUNIT*.7, finecosine[angle]);
		tiny->momy = FixedMul(FRACUNIT*.7, finesine[angle]);
		tiny->momz = FRACUNIT+(P_Random()<<9);
		P_CheckMissileSpawn(tiny);
	}
}

//----------------------------------------------------------------------------
//
// PROC A_SkullPop
//
//----------------------------------------------------------------------------

void C_DECL A_SkullPop(mobj_t *actor)
{
	mobj_t *mo;
	player_t *player;

	actor->flags &= ~MF_SOLID;
	mo = P_SpawnMobj(actor->x, actor->y, actor->z+48*FRACUNIT,
		MT_BLOODYSKULL);
	//mo->target = actor;
	mo->momx = (P_Random()-P_Random())<<9;
	mo->momy = (P_Random()-P_Random())<<9;
	mo->momz = FRACUNIT*2+(P_Random()<<6);
	// Attach player mobj to bloody skull
	player = actor->player;
	actor->player = NULL;
	actor->dplayer = NULL;
	mo->player = player;
	mo->dplayer = player->plr;
	mo->health = actor->health;
	mo->angle = actor->angle;
	player->plr->mo = mo;
	player->plr->lookdir = 0;
	player->damagecount = 32;
}

//----------------------------------------------------------------------------
//
// PROC A_CheckSkullFloor
//
//----------------------------------------------------------------------------

void C_DECL A_CheckSkullFloor(mobj_t *actor)
{
	if(actor->z <= actor->floorz)
	{
		P_SetMobjState(actor, S_BLOODYSKULLX1);
	}
}

//----------------------------------------------------------------------------
//
// PROC A_CheckSkullDone
//
//----------------------------------------------------------------------------

void C_DECL A_CheckSkullDone(mobj_t *actor)
{
	if(actor->special2 == 666)
	{
		P_SetMobjState(actor, S_BLOODYSKULLX2);
	}
}

//----------------------------------------------------------------------------
//
// PROC A_CheckBurnGone
//
//----------------------------------------------------------------------------

void C_DECL A_CheckBurnGone(mobj_t *actor)
{
	if(actor->special2 == 666)
	{
		P_SetMobjState(actor, S_PLAY_FDTH20);
	}
}

//----------------------------------------------------------------------------
//
// PROC A_FreeTargMobj
//
//----------------------------------------------------------------------------

void C_DECL A_FreeTargMobj(mobj_t *mo)
{
	mo->momx = mo->momy = mo->momz = 0;
	mo->z = mo->ceilingz+4*FRACUNIT;
	mo->flags &= ~(MF_SHOOTABLE|MF_FLOAT|MF_SKULLFLY|MF_SOLID);
	mo->flags |= MF_CORPSE|MF_DROPOFF|MF_NOGRAVITY;
	mo->flags2 &= ~(MF2_PASSMOBJ|MF2_LOGRAV);
	mo->player = NULL;
	mo->dplayer = NULL;
}

//----------------------------------------------------------------------------
//
// PROC A_AddPlayerCorpse
//
//----------------------------------------------------------------------------

#define BODYQUESIZE 32
mobj_t *bodyque[BODYQUESIZE];
int bodyqueslot;

void C_DECL A_AddPlayerCorpse(mobj_t *actor)
{
	if(bodyqueslot >= BODYQUESIZE)
	{ // Too many player corpses - remove an old one
		P_RemoveMobj(bodyque[bodyqueslot%BODYQUESIZE]);
	}
	bodyque[bodyqueslot%BODYQUESIZE] = actor;
	bodyqueslot++;
}

//----------------------------------------------------------------------------
//
// PROC A_FlameSnd
//
//----------------------------------------------------------------------------

void C_DECL A_FlameSnd(mobj_t *actor)
{
	S_StartSound(sfx_hedat1, actor); // Burn sound
}

//----------------------------------------------------------------------------
//
// PROC A_HideThing
//
//----------------------------------------------------------------------------

void C_DECL A_HideThing(mobj_t *actor)
{
	//P_UnsetThingPosition(actor);
	actor->flags2 |= MF2_DONTDRAW;
}

//----------------------------------------------------------------------------
//
// PROC A_UnHideThing
//
//----------------------------------------------------------------------------

void C_DECL A_UnHideThing(mobj_t *actor)
{
	//P_SetThingPosition(actor);
	actor->flags2 &= ~MF2_DONTDRAW;
}

