/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2009 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 2003-2005 Samuel Villarreal <svkaiser@gmail.com>
 *\author Copyright © 1999 by Chi Hoang, Lee Killough, Jim Flynn, Rand Phares, Ty Halderman (PrBoom 2.2.6)
 *\author Copyright © 1999-2000 by Jess Haas, Nicolas Kalkhof, Colin Phipps, Florian Schulze (PrBoom 2.2.6)
 *\author Copyright © 1993-1996 by id Software, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

/**
 * p_mobj.c: Moving object handling. Spawn functions.
 */

#ifdef MSVC
#  pragma optimize("g", off)
#endif

// HEADER FILES ------------------------------------------------------------

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "jdoom64.h"

#include "dmu_lib.h"
#include "hu_stuff.h"
#include "g_common.h"
#include "p_map.h"
#include "p_terraintype.h"
#include "p_player.h"
#include "p_tick.h"
#include "p_actor.h"
#include "p_start.h"

// MACROS ------------------------------------------------------------------

#define VANISHTICS              (2*TICSPERSEC)
#define SPAWNFADETICS           (1*TICSPERSEC)

#define MAX_BOB_OFFSET          (8)

#define NOMOMENTUM_THRESHOLD    (0.000001f)
#define STOPSPEED               (1.0f/1.6/10)
#define STANDSPEED              (1.0f/2)

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

const terraintype_t* P_MobjGetFloorTerrainType(mobj_t* mo)
{
    sector_t* sec = DMU_GetPtrp(mo->subsector, DMU_SECTOR);

    return P_GetPlaneMaterialType(sec, PLN_FLOOR);
}

/**
 * @return              @c true, if the mobj is still present.
 */
boolean P_MobjChangeState(mobj_t* mobj, statenum_t state)
{
    state_t* st;

    do
    {
        if(state == S_NULL)
        {
            mobj->state = (state_t *) S_NULL;
            P_MobjRemove(mobj, false);
            return false;
        }

        P_MobjSetState(mobj, state);
        st = &STATES[state];

        mobj->turnTime = false; // $visangle-facetarget

        // Modified handling.
        // Call action functions when the state is set
        if(st->action)
            st->action(mobj);

        state = st->nextState;
    } while(!mobj->tics);

    return true;
}

void P_ExplodeMissile(mobj_t *mo)
{
    if(IS_CLIENT)
    {
        // Clients won't explode missiles.
        P_MobjChangeState(mo, S_NULL);
        return;
    }

    mo->mom[MX] = mo->mom[MY] = mo->mom[MZ] = 0;

    P_MobjChangeState(mo, P_GetState(mo->type, SN_DEATH));

    mo->tics -= P_Random() & 3;

    if(mo->tics < 1)
        mo->tics = 1;

    if(mo->flags & MF_MISSILE)
    {
        mo->flags &= ~MF_MISSILE;
        mo->flags |= MF_VIEWALIGN;
        // Remove the brightshadow flag.
        if(mo->flags & MF_BRIGHTSHADOW)
            mo->flags &= ~MF_BRIGHTSHADOW;
        if(mo->flags & MF_BRIGHTEXPLODE)
            mo->flags |= MF_BRIGHTSHADOW;
    }

    if(mo->info->deathSound)
        S_StartSound(mo->info->deathSound, mo);
}

void P_FloorBounceMissile(mobj_t* mo)
{
    mo->mom[MZ] = -mo->mom[MZ];
    P_MobjChangeState(mo, P_GetState(mo->type, SN_DEATH));
}

/**
 * @return              The ground friction factor for the mobj.
 */
float P_MobjGetFriction(mobj_t* mo)
{
    if((mo->flags2 & MF2_FLY) && !(mo->pos[VZ] <= mo->floorZ) && !mo->onMobj)
    {
        return FRICTION_FLY;
    }

    return XS_Friction(DMU_GetPtrp(mo->subsector, DMU_SECTOR));
}

void P_MobjMoveXY(mobj_t* mo)
{
    float pos[3], mom[3];
    player_t* player;
    boolean largeNegative;

    // $democam: cameramen have their own movement code
    if(P_CameraXYMovement(mo))
        return;

    mom[MX] = MINMAX_OF(-MAXMOVE, mo->mom[MX], MAXMOVE);
    mom[MY] = MINMAX_OF(-MAXMOVE, mo->mom[MY], MAXMOVE);
    mo->mom[MX] = mom[MX];
    mo->mom[MY] = mom[MY];

    if(mom[MX] == 0 && mom[MY] == 0)
    {
        if(mo->flags & MF_SKULLFLY)
        {   // The skull slammed into something.
            mo->flags &= ~MF_SKULLFLY;
            mo->mom[MX] = mo->mom[MY] = mo->mom[MZ] = 0;

            P_MobjChangeState(mo, P_GetState(mo->type, SN_SPAWN));
        }

        return;
    }

    player = mo->player;

    do
    {
        /**
         * DOOM.exe bug fix:
         * Large negative displacements were never considered. This explains the
         * tendency for Mancubus fireballs to pass through walls.
         */

        largeNegative = false;
        if(!cfg.moveBlock &&
           (mom[MX] < -MAXMOVE / 2 || mom[MY] < -MAXMOVE / 2))
        {
            // Make an exception for "north-only wallrunning".
            if(!(cfg.wallRunNorthOnly && mo->wallRun))
                largeNegative = true;
        }

        if(largeNegative || mom[MX] > MAXMOVE / 2 || mom[MY] > MAXMOVE / 2)
        {
            pos[VX] = mo->pos[VX] + mom[VX] / 2;
            pos[VY] = mo->pos[VY] + mom[VY] / 2;
            mom[VX] /= 2;
            mom[VY] /= 2;
        }
        else
        {
            pos[VX] = mo->pos[VX] + mom[MX];
            pos[VY] = mo->pos[VY] + mom[MY];
            mom[MX] = mom[MY] = 0;
        }

        // If mobj was wallrunning - stop.
        if(mo->wallRun)
            mo->wallRun = false;

        // $dropoff_fix.
        if(!P_TryMove(mo, pos[VX], pos[VY], true, false))
        {   // Blocked move.
            if(mo->flags2 & MF2_SLIDE)
            {   // Try to slide along it.
                P_SlideMove(mo);
            }
            else if(mo->flags & MF_MISSILE)
            {
                sector_t*           backSec;

                //// kludge: Prevent missiles exploding against the sky.
                if(ceilingLine &&
                   (backSec = DMU_GetPtrp(ceilingLine, DMU_BACK_SECTOR)))
                {
                    material_t*         mat =
                        DMU_GetPtrp(backSec, DMU_CEILING_MATERIAL);

                    if((DMU_GetIntp(mat, DMU_FLAGS) & MATF_SKYMASK) &&
                       mo->pos[VZ] > DMU_GetFloatp(backSec, DMU_CEILING_HEIGHT))
                    {
                        P_MobjRemove(mo, false);
                        return;
                    }
                }

                if(floorLine &&
                   (backSec = DMU_GetPtrp(floorLine, DMU_BACK_SECTOR)))
                {
                    material_t*         mat =
                        DMU_GetPtrp(backSec, DMU_FLOOR_MATERIAL);

                    if((DMU_GetIntp(mat, DMU_FLAGS) & MATF_SKYMASK) &&
                       mo->pos[VZ] < DMU_GetFloatp(backSec, DMU_FLOOR_HEIGHT))
                    {
                        P_MobjRemove(mo, false);
                        return;
                    }
                }
                //// kludge end.

                P_ExplodeMissile(mo);
            }
            else
            {
                mo->mom[MX] = mo->mom[MY] = 0;
            }
        }
    } while(!INRANGE_OF(mom[MX], 0, NOMOMENTUM_THRESHOLD) ||
            !INRANGE_OF(mom[MY], 0, NOMOMENTUM_THRESHOLD));

    // Slow down.
    if(player && (P_GetPlayerCheats(player) & CF_NOMOMENTUM))
    {
        // Debug option for no sliding at all.
        mo->mom[MX] = mo->mom[MY] = 0;
        return;
    }

    if(mo->flags & (MF_MISSILE | MF_SKULLFLY))
        return; // No friction for missiles ever.

    if(mo->pos[VZ] > mo->floorZ && !mo->onMobj && !(mo->flags2 & MF2_FLY))
        return; // No friction when falling.

    if(cfg.slidingCorpses)
    {
        // $dropoff_fix: Add objects falling off ledges, does not apply to
        // players.
        if(((mo->flags & MF_CORPSE) || (mo->intFlags & MIF_FALLING)) &&
           !mo->player)
        {
            // Do not stop sliding if halfway off a step with some momentum.
            if(mo->mom[MX] > (1.0f / 4) || mo->mom[MX] < -(1.0f / 4) ||
               mo->mom[MY] > (1.0f / 4) || mo->mom[MY] < -(1.0f / 4))
            {
                if(mo->floorZ != DMU_GetFloatp(mo->subsector, DMU_FLOOR_HEIGHT))
                    return;
            }
        }
    }

    // Stop player walking animation.
    if(player && !player->plr->cmd.forwardMove && !player->plr->cmd.sideMove &&
       mo->mom[MX] > -STANDSPEED && mo->mom[MX] < STANDSPEED &&
       mo->mom[MY] > -STANDSPEED && mo->mom[MY] < STANDSPEED)
    {
        // If in a walking frame, stop moving.
        if((unsigned) ((player->plr->mo->state - STATES) - PCLASS_INFO(player->class)->runState) < 4)
            P_MobjChangeState(player->plr->mo, PCLASS_INFO(player->class)->normalState);
    }

    if((!player || (player->plr->cmd.forwardMove == 0 && player->plr->cmd.sideMove == 0)) &&
       mo->mom[MX] > -STOPSPEED && mo->mom[MX] < STOPSPEED &&
       mo->mom[MY] > -STOPSPEED && mo->mom[MY] < STOPSPEED)
    {
        mo->mom[MX] = mo->mom[MY] = 0;
    }
    else
    {
        if((mo->flags2 & MF2_FLY) && !(mo->pos[VZ] <= mo->floorZ) &&
           !mo->onMobj)
        {
            mo->mom[MX] *= FRICTION_FLY;
            mo->mom[MY] *= FRICTION_FLY;
        }
#if __JHERETIC__
        else if(P_ToXSector(DMU_GetPtrp(mo->face,
                                    DMU_SECTOR))->special == 15)
        {
            // Friction_Low
            mo->mom[MX] *= FRICTION_LOW;
            mo->mom[MY] *= FRICTION_LOW;
        }
#endif
        else
        {
            float               friction = P_MobjGetFriction(mo);

            mo->mom[MX] *= friction;
            mo->mom[MY] *= friction;
        }
    }
}

/*
static boolean PIT_Splash(sector_t *sector, void *data)
{
    mobj_t             *mo = data;
    float               floorheight;

    floorheight = DMU_GetFloatp(sector, DMU_FLOOR_HEIGHT);

    // Is the mobj touching the floor of this sector?
    if(mo->pos[VZ] < floorheight &&
       mo->pos[VZ] + mo->height / 2 > floorheight)
    {
        //// \todo Play a sound, spawn a generator, etc.
    }

    // Continue checking.
    return true;
}
*/

void P_HitFloor(mobj_t* mo)
{
    //P_MobjSectorsIterator(mo, PIT_Splash, mo);
}

void P_MobjMoveZ(mobj_t* mo)
{
    float gravity, dist, delta;

    gravity = XS_Gravity(DMU_GetPtrp(mo->subsector, DMU_SECTOR));

    // $democam: cameramen get special z movement.
    if(P_CameraZMovement(mo))
        return;

    // $voodoodolls: Check for smooth step up unless a voodoo doll.
    if(mo->player && mo->player->plr->mo == mo &&
       mo->pos[VZ] < mo->floorZ)
    {
        mo->player->viewHeight -= mo->floorZ - mo->pos[VZ];
        mo->player->viewHeightDelta =
            (cfg.plrViewHeight - mo->player->viewHeight) / 8;
    }

    // Adjust height.
    mo->pos[VZ] += mo->mom[MZ];

    if((mo->flags2 & MF2_FLY) &&
       mo->onMobj && mo->pos[VZ] > mo->onMobj->pos[VZ] + mo->onMobj->height)
        mo->onMobj = NULL; // We were on a mobj, we are NOT now.

    if(!((mo->flags ^ MF_FLOAT) & (MF_FLOAT | MF_SKULLFLY | MF_INFLOAT)) &&
       mo->target && !P_MobjIsCamera(mo->target))
    {
        // Float down towards target if too close.
        dist = P_ApproxDistance(mo->pos[VX] - mo->target->pos[VX],
                                mo->pos[VY] - mo->target->pos[VY]);

        //delta = (mo->target->pos[VZ] + (mo->height / 2)) - mo->pos[VZ];
        delta = (mo->target->pos[VZ] + mo->target->height / 2) -
                (mo->pos[VZ] + mo->height / 2);

        // Don't go INTO the target.
        if(!(dist < mo->radius + mo->target->radius &&
             fabs(delta) < mo->height + mo->target->height))
        {
            if(delta < 0 && dist < -(delta * 3))
            {
                mo->pos[VZ] -= FLOATSPEED;
                P_MobjSetSRVOZ(mo, -FLOATSPEED);
            }
            else if(delta > 0 && dist < (delta * 3))
            {
                mo->pos[VZ] += FLOATSPEED;
                P_MobjSetSRVOZ(mo, FLOATSPEED);
            }
        }
    }

    // Do some fly-bobbing.
    if(mo->player && (mo->flags2 & MF2_FLY) && mo->pos[VZ] > mo->floorZ &&
       !mo->onMobj && (mapTime & 2))
    {
        mo->pos[VZ] += FIX2FLT(finesine[(FINEANGLES / 20 * mapTime >> 2) & FINEMASK]);
    }

    // Clip movement. Another thing?
    // jd64 >
    // DJS - FIXME!
    if(mo->pos[VZ] <= mo->floorZ && (mo->flags & MF_MISSILE))
    {
        // Hit the floor
        mo->pos[VZ] = mo->floorZ;
        P_ExplodeMissile(mo);
        return;
    }
    // < d64tc

    // Clip movement. Another thing?
    if(mo->onMobj && mo->pos[VZ] <= mo->onMobj->pos[VZ] + mo->onMobj->height)
    {
        if(mo->mom[MZ] < 0)
        {
            if(mo->player && mo->mom[MZ] < -gravity * 8 &&
               !(mo->flags2 & MF2_FLY))
            {
                // Squat down.
                // Decrease viewheight for a moment after hitting the ground
                // (hard), and utter appropriate sound.
                mo->player->viewHeightDelta = mo->mom[MZ] / 8;

                if(mo->player->health > 0)
                    S_StartSound(SFX_OOF, mo);
            }

            mo->mom[MZ] = 0;
        }

        if(mo->mom[MZ] == 0)
            mo->pos[VZ] = mo->onMobj->pos[VZ] + mo->onMobj->height;

        if((mo->flags & MF_MISSILE) && !(mo->flags & MF_NOCLIP))
        {
            P_ExplodeMissile(mo);
            return;
        }
    }

    // jd64 >
    // MotherDemon's Fire attacks can climb up/down stairs
    // DJS - FIXME!
#if 0
    if((mo->flags & MF_MISSILE) && (mo->type == MT_FIREEND))
    {
        mo->pos[VZ] = mo->floorZ;

        if(mo->type == MT_FIREEND)
        {
            return;
        }
        else
        {
            P_ExplodeMissile(mo);
            return;
        }
    }
#endif
    // < d64tc

    // The floor.
    if(mo->pos[VZ] <= mo->floorZ)
    {   // Hit the floor.
        boolean             movingDown;

        // Note (id):
        //  somebody left this after the setting momz to 0,
        //  kinda useless there.
        //
        // cph - This was the a bug in the linuxdoom-1.10 source which
        //  caused it not to sync Doom 2 v1.9 demos. Someone
        //  added the above comment and moved up the following code. So
        //  demos would desync in close lost soul fights.
        // Note that this only applies to original Doom 1 or Doom2 demos - not
        //  Final Doom and Ultimate Doom.  So we test demo_compatibility *and*
        //  gameMission. (Note we assume that Doom1 is always Ult Doom, which
        //  seems to hold for most published demos.)
        //
        //  fraggle - cph got the logic here slightly wrong.  There are three
        //  versions of Doom 1.9:
        //
        //  * The version used in registered doom 1.9 + doom2 - no bounce
        //  * The version used in ultimate doom - has bounce
        //  * The version used in final doom - has bounce
        //
        // So we need to check that this is either retail or commercial
        // (but not doom2)
        int correctLostSoulBounce = true;

        if(correctLostSoulBounce && (mo->flags & MF_SKULLFLY))
        {
            // The skull slammed into something.
            mo->mom[MZ] = -mo->mom[MZ];
        }

        if(movingDown = (mo->mom[MZ] < 0))
        {
            if(mo->player && mo->player->plr->mo == mo &&
               mo->mom[MZ] < -gravity * 8 && !(mo->flags2 & MF2_FLY))
            {
                // Squat down.
                // Decrease viewheight for a moment after hitting the ground
                // (hard), and utter appropriate sound.
                mo->player->viewHeightDelta = mo->mom[MZ] / 8;
                mo->player->jumpTics = 10;

                /**
                 * DOOM bug:
                 * Dead players would grunt when hitting the ground (e.g.,
                 * after an archvile attack).
                 */
                if(mo->player->health > 0)
                    S_StartSound(SFX_OOF, mo);
            }
        }

        mo->pos[VZ] = mo->floorZ;

        if(movingDown)
            P_HitFloor(mo);

        /**
         * See lost soul bouncing comment above. We need this here for bug
         * compatibility with original Doom2 v1.9 - if a soul is charging
         * and hit by a raising floor this would incorrectly reverse it's
         * Y momentum.
         */
        if(!correctLostSoulBounce && (mo->flags & MF_SKULLFLY))
            mo->mom[MZ] = -mo->mom[MZ];

        if(!((mo->flags ^ MF_MISSILE) & (MF_MISSILE | MF_NOCLIP)))
        {
            if(mo->flags2 & MF2_FLOORBOUNCE)
            {
                P_FloorBounceMissile(mo);
                return;
            }
            else
            {
                P_ExplodeMissile(mo);
                return;
            }
        }

        if(movingDown && mo->mom[MZ] < 0)
            mo->mom[MZ] = 0;
    }
    else if(mo->flags2 & MF2_LOGRAV)
    {
        if(mo->mom[MZ] == 0)
            mo->mom[MZ] = -(gravity / 8) * 2;
        else
            mo->mom[MZ] -= (gravity / 8);
    }
    else if(!(mo->flags & MF_NOGRAVITY))
    {
        if(mo->mom[MZ] == 0)
            mo->mom[MZ] = -gravity * 2;
        else
            mo->mom[MZ] -= gravity;
    }

    if(mo->pos[VZ] + mo->height > mo->ceilingZ)
    {   // Hit the ceiling.
        if(mo->mom[MZ] > 0)
            mo->mom[MZ] = 0;

        mo->pos[VZ] = mo->ceilingZ - mo->height;

        if(mo->flags & MF_SKULLFLY)
        {   // The skull slammed into something.
            mo->mom[MZ] = -mo->mom[MZ];
        }

        if(!((mo->flags ^ MF_MISSILE) & (MF_MISSILE | MF_NOCLIP)))
        {
            // Don't explode against sky.
            if(DMU_GetIntp(DMU_GetPtrp(mo->subsector, DMU_CEILING_MATERIAL),
                           DMU_FLAGS) & MATF_SKYMASK)
            {
                P_MobjRemove(mo, false);
            }
            else
            {
                P_ExplodeMissile(mo);
            }
        }
    }
}

void P_NightmareRespawn(mobj_t* mobj)
{
    mobj_t* mo;

    // Something is occupying it's position?
    if(!P_CheckPosition2f(mobj, mobj->spawnSpot.pos[VX],
                          mobj->spawnSpot.pos[VY]))
        return; // No respwan.

    if((mo = P_SpawnMobj3fv(mobj->type, mobj->spawnSpot.pos,
                            mobj->spawnSpot.angle, mobj->spawnSpot.flags)))
    {
        mo->reactionTime = 18;

        // Spawn a teleport fog at old spot.
        if((mo = P_SpawnMobj3f(MT_TFOG, mobj->pos[VX], mobj->pos[VY], 0,
                               mobj->angle, MSF_Z_FLOOR)))
            S_StartSound(SFX_TELEPT, mo);

        // Spawn a teleport fog at the new spot.
        if((mo = P_SpawnMobj3fv(MT_TFOG, mobj->spawnSpot.pos,
                                mobj->spawnSpot.angle,
                                mobj->spawnSpot.flags)))
            S_StartSound(SFX_TELEPT, mo);
    }

    // Remove the old monster.
    P_MobjRemove(mobj, true);
}

void P_MobjThinker(mobj_t *mobj)
{
    if(mobj->ddFlags & DDMF_REMOTE)
        return; // Remote mobjs are handled separately.

    // Spectres get selector = 1.
    if(mobj->type == MT_SHADOWS)
        mobj->selector = (mobj->selector & ~DDMOBJ_SELECTOR_MASK) | 1;

    // The first three bits of the selector special byte contain a
    // relative health level.
    P_UpdateHealthBits(mobj);

#if __JHERETIC__
    // Lightsources must stay where they're hooked.
    if(mobj->type == MT_LIGHTSOURCE)
    {
        if(mobj->moveDir > 0)
            mobj->pos[VZ] =
                DMU_GetFloatp(mobj->face, DMU_FLOOR_HEIGHT);
        else
            mobj->pos[VZ] =
                DMU_GetFixedp(mobj->face, DMU_CEILING_HEIGHT);

        mobj->pos[VZ] += FIX2FLT(mobj->moveDir);
        return;
    }
#endif

    // Handle X and Y momentums.
    if(mobj->mom[MX] != 0 || mobj->mom[MY] != 0 || (mobj->flags & MF_SKULLFLY))
    {
        P_MobjMoveXY(mobj);

        //// \fixme decent NOP/NULL/Nil function pointer please.
        if(mobj->thinker.function == NOPFUNC)
            return; // Mobj was removed.
    }

    if(mobj->flags2 & MF2_FLOATBOB)
    {   // Floating item bobbing motion.
        // Keep it on the floor.
        mobj->pos[VZ] = mobj->floorZ;
        mobj->floorClip = 0;

        if(mobj->floorClip < -MAX_BOB_OFFSET)
        {
            // We don't want it going through the floor.
            mobj->floorClip = -MAX_BOB_OFFSET;
        }
    }
    else if(mobj->pos[VZ] != mobj->floorZ || mobj->mom[MZ] != 0)
    {
        P_MobjMoveZ(mobj);
        if(mobj->thinker.function != P_MobjThinker) // Must've been removed.
            return; // Mobj was removed.
    }
    // Non-sentient objects at rest.
    else if(!(mobj->mom[MX] == 0 && mobj->mom[MY] == 0) && !sentient(mobj) &&
            !(mobj->player) && !((mobj->flags & MF_CORPSE) &&
            cfg.slidingCorpses))
    {
        // Objects fall off ledges if they are hanging off slightly push
        // off of ledge if hanging more than halfway off

        if(mobj->pos[VZ] > mobj->dropOffZ && // Only objects contacting dropoff.
           !(mobj->flags & MF_NOGRAVITY) && cfg.fallOff)
        {
            P_ApplyTorque(mobj);
        }
        else
        {
            mobj->intFlags &= ~MIF_FALLING;
            mobj->gear = 0; // Reset torque.
        }
    }

    if(cfg.slidingCorpses)
    {
        if(((mobj->flags & MF_CORPSE)? mobj->pos[VZ] > mobj->dropOffZ :
                                       mobj->pos[VZ] - mobj->dropOffZ > 24) && // Only objects contacting drop off
           !(mobj->flags & MF_NOGRAVITY)) // Only objects which fall.
        {
            P_ApplyTorque(mobj); // Apply torque.
        }
        else
        {
            mobj->intFlags &= ~MIF_FALLING;
            mobj->gear = 0; // Reset torque.
        }
    }

    // $vanish: dead monsters disappear after some time.
    if(cfg.corpseTime && (mobj->flags & MF_CORPSE) && mobj->corpseTics != -1)
    {
        if(++mobj->corpseTics < cfg.corpseTime * TICSPERSEC)
        {
            mobj->translucency = 0; // Opaque.
        }
        else if(mobj->corpseTics < cfg.corpseTime * TICSPERSEC + VANISHTICS)
        {
            // Translucent during vanishing.
            mobj->translucency =
                ((mobj->corpseTics -
                  cfg.corpseTime * TICSPERSEC) * 255) / VANISHTICS;
        }
        else
        {
            // Too long; get rid of the corpse.
            mobj->corpseTics = -1;
            return;
        }
    }

    // jd64 >
    // Fade monsters upon spawning.
    if(mobj->intFlags & MIF_FADE)
    {
        if(++mobj->spawnFadeTics < SPAWNFADETICS)
        {
            mobj->translucency = MINMAX_OF(0, 255 -
                255 * mobj->spawnFadeTics / SPAWNFADETICS, 255);
        }
        else
        {
            mobj->intFlags &= ~MIF_FADE;
            mobj->translucency = 0;
        }
    }
    // < d64tc

    // Cycle through states, calling action functions at transitions.
    if(mobj->tics != -1)
    {
        mobj->tics--;

        P_MobjAngleSRVOTicker(mobj); // "angle-servo"; smooth actor turning.

        // You can cycle through multiple STATES in a tic.
        if(!mobj->tics)
        {
            P_MobjClearSRVO(mobj);
            if(!P_MobjChangeState(mobj, mobj->state->nextState))
                return; // Freed itself.
        }
    }
    else if(!IS_CLIENT)
    {
        // Check for nightmare respawn.
        if(!(mobj->flags & MF_COUNTKILL))
            return;

        if(!respawnMonsters)
            return;

        mobj->moveCount++;

        if(mobj->moveCount < 12 * 35)
            return;

        if(mapTime & 31)
            return;

        if(P_Random() > 4)
            return;

        P_NightmareRespawn(mobj);
    }
}

/**
 * Spawns a mobj of "type" at the specified position.
 */
mobj_t* P_SpawnMobj3f(mobjtype_t type, float x, float y, float z,
                      angle_t angle, int spawnFlags)
{
    mobj_t*             mo;
    mobjinfo_t*         info;
    float               space;
    int                 ddflags = 0;

    if(type < MT_FIRST || type >= Get(DD_NUMMOBJTYPES))
    {
#ifdef _DEBUG
        Con_Error("P_SpawnMobj: Illegal mo type %i.\n", type);
#endif
        return NULL;
    }

    info = &MOBJINFO[type];

    // Clients only spawn local objects.
    if(!(info->flags & MF_LOCAL) && IS_CLIENT)
        return NULL;

    // Not for deathmatch?
    if(deathmatch && (info->flags & MF_NOTDMATCH))
        return NULL;

    // Check for specific disabled objects.
    if(IS_NETGAME)
    {
        // Cooperative weapons?
        if(cfg.noCoopWeapons && !deathmatch && type >= MT_CLIP &&
           type <= MT_SUPERSHOTGUN)
            return NULL;

        // Don't spawn any special objects in coop?
        if(cfg.noCoopAnything && !deathmatch)
            return NULL;

        // BFG disabled in netgames?
        if(cfg.noNetBFG && type == MT_MISC25)
            return NULL;
    }

    // Don't spawn any monsters if -noMonstersParm.
    if(noMonstersParm && ((info->flags & MF_COUNTKILL) || type == MT_SKULL))
        return NULL;

    if(info->flags & MF_SOLID)
        ddflags |= DDMF_SOLID;
    if(info->flags2 & MF2_DONTDRAW)
        ddflags |= DDMF_DONTDRAW;

    mo = P_MobjCreate(P_MobjThinker, x, y, z, angle, info->radius,
                      info->height, ddflags);
    mo->type = type;
    mo->info = info;
    mo->flags = info->flags;
    mo->flags2 = info->flags2;
    mo->flags3 = info->flags3;
    mo->damage = info->damage;
    mo->health =
        info->spawnHealth * (IS_NETGAME ? cfg.netMobHealthModifier : 1);
    mo->moveDir = DI_NODIR;

    mo->reactionTime = info->reactionTime;

    mo->lastLook = P_Random() % MAXPLAYERS;

    // Must link before setting state (ID assigned for the mo).
    P_MobjSetState(mo, P_GetState(mo->type, SN_SPAWN));

    // Set subsector and/or block links.
    P_MobjSetPosition(mo);

    mo->floorZ   = DMU_GetFloatp(mo->subsector, DMU_FLOOR_HEIGHT);
    mo->dropOffZ = mo->floorZ;
    mo->ceilingZ = DMU_GetFloatp(mo->subsector, DMU_CEILING_HEIGHT);

    if((spawnFlags & MSF_Z_CEIL) || (info->flags & MF_SPAWNCEILING))
    {
        mo->pos[VZ] = mo->ceilingZ - mo->info->height - z;
    }
    else if((spawnFlags & MSF_Z_RANDOM) || (info->flags2 & MF2_SPAWNFLOAT))
    {
        space = mo->ceilingZ - mo->info->height - mo->floorZ;
        if(space > 48)
        {
            space -= 40;
            mo->pos[VZ] = ((space * P_Random()) / 256) + mo->floorZ + 40;
        }
        else
        {
            mo->pos[VZ] = mo->floorZ;
        }
    }
    else if(spawnFlags & MSF_Z_FLOOR)
    {
        mo->pos[VZ] = mo->floorZ + z;
    }

    /*if(spawnFlags & MTF_FLOAT)
    {
        mo->pos[VZ] += 96;
        mo->flags |= (MF_FLOAT | MF_NOGRAVITY);
    }*/

    if(spawnFlags & MSF_DEAF)
        mo->flags |= MF_AMBUSH;

    mo->floorClip = 0;

    if((mo->flags2 & MF2_FLOORCLIP) &&
       mo->pos[VZ] == DMU_GetFloatp(mo->subsector, DMU_FLOOR_HEIGHT))
    {
        const terraintype_t* tt = P_MobjGetFloorTerrainType(mo);

        if(tt->flags & TTF_FLOORCLIP)
        {
            mo->floorClip = 10;
        }
    }

    /*if(spawnFlags & MTF_WALKOFF)
        mo->flags |= (MF_FLOAT | MF_DROPOFF);

    if(spawnFlags & MTF_TRANSLUCENT)
        mo->flags |= MF_SHADOW;*/

    // Copy spawn attributes to the new mobj.
    mo->spawnSpot.pos[VX] = x;
    mo->spawnSpot.pos[VY] = y;
    mo->spawnSpot.pos[VZ] = z;
    mo->spawnSpot.angle = angle;
    mo->spawnSpot.flags = spawnFlags;

    return mo;
}

mobj_t* P_SpawnMobj3fv(mobjtype_t type, const float pos[3], angle_t angle,
                       int spawnFlags)
{
    return P_SpawnMobj3f(type, pos[VX], pos[VY], pos[VZ], angle, spawnFlags);
}

mobj_t* P_SpawnCustomPuff(mobjtype_t type, float x, float y, float z,
                          angle_t angle)
{
    mobj_t*             mo;

    // Clients do not spawn puffs.
    if(IS_CLIENT)
        return NULL;

    z += FIX2FLT((P_Random() - P_Random()) << 10);

    if((mo = P_SpawnMobj3f(type, x, y, z, angle, 0)))
    {
        mo->mom[MZ] = FIX2FLT(FRACUNIT);
        mo->tics -= P_Random() & 3;

        // Make it last at least one tic.
        if(mo->tics < 1)
            mo->tics = 1;
    }

    return mo;
}

void P_SpawnPuff(float x, float y, float z, angle_t angle)
{
    mobj_t*             th =
        P_SpawnCustomPuff(MT_PUFF, x, y, z, angle);

    // Don't make punches spark on the wall.
    if(th && attackRange == MELEERANGE)
        P_MobjChangeState(th, S_PUFF3);
}

void P_SpawnBlood(float x, float y, float z, int damage, angle_t angle)
{
    mobj_t*             mo;

    z += FIX2FLT((P_Random() - P_Random()) << 10);
    if((mo = P_SpawnMobj3f(MT_BLOOD, x, y, z, angle, 0)))
    {
        mo->mom[MZ] = 2;
        mo->tics -= P_Random() & 3;

        if(mo->tics < 1)
            mo->tics = 1;

        if(damage <= 12 && damage >= 9)
            P_MobjChangeState(mo, S_BLOOD2);
        else if(damage < 9)
            P_MobjChangeState(mo, S_BLOOD3);
    }
}

/**
 * Moves the missile forward a bit and possibly explodes it right there.
 *
 * @param th            The missile to be checked.
 *
 * @return              @c true, if the missile is at a valid location else
 *                      @c false
 */
boolean P_CheckMissileSpawn(mobj_t *th)
{
    th->tics -= P_Random() & 3;
    if(th->tics < 1)
        th->tics = 1;

    // Move forward slightly so an angle can be computed if it explodes
    // immediately.
    th->pos[VX] += th->mom[MX] / 2;
    th->pos[VY] += th->mom[MY] / 2;
    th->pos[VZ] += th->mom[MZ] / 2;

    if(!P_TryMove(th, th->pos[VX], th->pos[VY], false, false))
    {
        P_ExplodeMissile(th);
        return false;
    }

    return true;
}

/**
 * Tries to aim at a nearby monster if source is a player. Else aim is
 * taken at dest.
 *
 * @param source        The mobj doing the shooting.
 * @param dest          The mobj being shot at. Can be @c NULL if source
 *                      is a player.
 * @param type          The type of mobj to be shot.
 *
 * @return              Pointer to the newly spawned missile.
 */
mobj_t* P_SpawnMissile(mobjtype_t type, mobj_t* source, mobj_t* dest)
{
    float               pos[3];
    mobj_t*             th = 0;
    unsigned int        an;
    angle_t             angle = 0;
    float               dist = 0;
    float               slope = 0;
    float               spawnZOff = 0;

    memcpy(pos, source->pos, sizeof(pos));

    if(source->player)
    {
        // See which target is to be aimed at.
        angle = source->angle;
        slope = P_AimLineAttack(source, angle, 16 * 64);
        if(!cfg.noAutoAim)
            if(!lineTarget)
            {
                angle += 1 << 26;
                slope = P_AimLineAttack(source, angle, 16 * 64);

                if(!lineTarget)
                {
                    angle -= 2 << 26;
                    slope = P_AimLineAttack(source, angle, 16 * 64);
                }

                if(!lineTarget)
                {
                    angle = source->angle;
                    slope =
                        tan(LOOKDIR2RAD(source->dPlayer->lookDir)) / 1.2f;
                }
            }

        if(!P_MobjIsCamera(source->player->plr->mo))
            spawnZOff = cfg.plrViewHeight - 9 +
                source->player->plr->lookDir / 173;
    }
    else
    {
        spawnZOff = 32;
    }

    pos[VZ] += spawnZOff;
    pos[VZ] -= source->floorClip;

    angle = R_PointToAngle2(pos[VX], pos[VY], dest->pos[VX], dest->pos[VY]);

    if(!source->player)
    {
        // Fuzzy player.
        if(dest->flags & MF_SHADOW)
            angle += (P_Random() - P_Random()) << 20;
    }

    if(!(th = P_SpawnMobj3fv(type, pos, angle, 0)))
        return NULL;

    if(th->info->seeSound)
        S_StartSound(th->info->seeSound, th);

    th->target = source; // Where it came from.
    an = angle >> ANGLETOFINESHIFT;
    th->mom[MX] = th->info->speed * FIX2FLT(finecosine[an]);
    th->mom[MY] = th->info->speed * FIX2FLT(finesine[an]);

    /*if(source->player)
    {   // Allow free-aim with the BFG in deathmatch?
        if(deathmatch && cfg.netBFGFreeLook == 0 && type == MT_BFG)
            th->mom[MZ] = 0;
        else
            th->mom[MZ] = th->info->speed * slope;
    }
    else*/
    {
        dist = P_ApproxDistance(dest->pos[VX] - pos[VX],
                                dest->pos[VY] - pos[VY]);
        dist /= th->info->speed;
        if(dist < 1)
            dist = 1;
        th->mom[MZ] = (dest->pos[VZ] - source->pos[VZ]) / dist;
    }

    // Make sure the speed is right (in 3D).
    dist = P_ApproxDistance(P_ApproxDistance(th->mom[MX], th->mom[MY]),
                            th->mom[MZ]);
    if(dist < 1)
        dist = 1;
    dist = th->info->speed / dist;

    th->mom[MX] *= dist;
    th->mom[MY] *= dist;
    th->mom[MZ] *= dist;

    if(P_CheckMissileSpawn(th))
        return th;

    return NULL;
}

/**
 * d64tc
 * DJS - It would appear this routine has been stolen from HEXEN and then
 *       butchered...
 * FIXME: Make sure this still works correctly.
 */
mobj_t* P_SPMAngle(mobjtype_t type, mobj_t *source, angle_t sourceAngle)
{
    mobj_t             *th;
    uint                an;
    angle_t             angle;
    float               pos[3], slope, spawnZOff;
    float               fangle =
        LOOKDIR2RAD(source->player->plr->lookDir), movfactor = 1;

    pos[VX] = source->pos[VX];
    pos[VY] = source->pos[VY];
    pos[VZ] = source->pos[VZ];

    // See which target is to be aimed at.
    angle = sourceAngle;
    slope = P_AimLineAttack(source, angle, 16 * 64);
    if(!lineTarget)
    {
        angle += 1 << 26;
        slope = P_AimLineAttack(source, angle, 16 * 64);
        if(!lineTarget)
        {
            angle -= 2 << 26;
            slope = P_AimLineAttack(source, angle, 16 * 64);
        }

        if(!lineTarget)
        {
            angle = sourceAngle;

            slope = sin(fangle) / 1.2;
            movfactor = cos(fangle);
        }
    }

    if(!P_MobjIsCamera(source->player->plr->mo))
        spawnZOff = cfg.plrViewHeight - 9 +
                        source->player->plr->lookDir / 173;
    else
        spawnZOff = 0;

    pos[VZ] += spawnZOff;
    pos[VZ] -= source->floorClip;

    if((th = P_SpawnMobj3fv(type, pos, angle, 0)))
    {
        th->target = source;
        an = angle >> ANGLETOFINESHIFT;
        th->mom[MX] = movfactor * th->info->speed * FIX2FLT(finecosine[an]);
        th->mom[MY] = movfactor * th->info->speed * FIX2FLT(finesine[an]);
        th->mom[MZ] = th->info->speed * slope;

        if(th->info->seeSound)
            S_StartSound(th->info->seeSound, th);

        P_CheckMissileSpawn(th);
    }

    return th;
}

/**
 * d64tc
 */
mobj_t* P_SpawnMotherMissile(mobjtype_t type, float x, float y, float z,
                             mobj_t *source, mobj_t *dest)
{
    mobj_t             *th;
    uint                an;
    angle_t             angle;
    float               dist;

    z -= source->floorClip;

    angle = R_PointToAngle2(x, y, dest->pos[VX], dest->pos[VY]);
    if(dest->flags & MF_SHADOW) // Invisible target
    {
        angle += (P_Random() - P_Random()) << 21;
    }

    if(!(th = P_SpawnMobj3f(type, x, y, z, angle, 0)))
        return NULL;

    if(th->info->seeSound)
        S_StartSound(th->info->seeSound, th);

    th->target = source; // Originator
    an = angle >> ANGLETOFINESHIFT;
    th->mom[MX] = th->info->speed * FIX2FLT(finecosine[an]);
    th->mom[MY] = th->info->speed * FIX2FLT(finesine[an]);

    dist = P_ApproxDistance(dest->pos[VX] - x, dest->pos[VY] - y);
    dist /= th->info->speed;

    if(dist < 1)
        dist = 1;
    th->mom[MZ] = (dest->pos[VZ] - z + 30) / dist;

    P_CheckMissileSpawn(th);
    return th;
}
