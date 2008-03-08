/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2007 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2008 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 2003-2005 Samuel Villarreal <svkaiser@gmail.com>
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

// HEADER FILES ------------------------------------------------------------

#include "jdoom64.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

actionlink_t actionlinks[] = {
    {"A_BabyMetal",         A_BabyMetal},
    {"A_BFGsound",          A_BFGsound},
    {"A_BFGSpray",          A_BFGSpray},
    {"A_BossDeath",         A_BossDeath},
    {"A_BrainAwake",        A_BrainAwake},
    {"A_BrainDie",          A_BrainDie},
    {"A_BrainExplode",      A_BrainExplode},
    {"A_BrainPain",         A_BrainPain},
    {"A_BrainScream",       A_BrainScream},
    {"A_BrainSpit",         A_BrainSpit},
    {"A_BruisAttack",       A_BruisAttack},
    {"A_BruisredAttack",    A_BruisredAttack}, //jd64
    {"A_BspiAttack",        A_BspiAttack},
    {"A_Chase",             A_Chase},
    {"A_CheckReload",       A_CheckReload},
    {"A_CloseShotgun2",     A_CloseShotgun2},
    {"A_CPosAttack",        A_CPosAttack},
    {"A_CposPanLeft",       A_CposPanLeft}, //jd64
    {"A_CposPanRight",      A_CposPanRight}, //jd64
    {"A_CPosRefire",        A_CPosRefire},
    {"A_CyberAttack",       A_CyberAttack},
    {"A_Explode",           A_Explode},
    {"A_FaceTarget",        A_FaceTarget},
    {"A_Fall",              A_Fall},
    {"A_FatAttack1",        A_FatAttack1},
    {"A_FatAttack2",        A_FatAttack2},
    {"A_FatAttack3",        A_FatAttack3},
    {"A_FatRaise",          A_FatRaise},
    {"A_FireBFG",           A_FireBFG},
    {"A_FireCGun",          A_FireCGun},
    {"A_FireMissile",       A_FireMissile},
    {"A_FirePistol",        A_FirePistol},
    {"A_FirePlasma",        A_FirePlasma},
    {"A_DrawPlasmaTube",    A_DrawPlasmaTube}, //jd64
    {"A_PlasmaBuzz",        A_PlasmaBuzz}, //jd64
    {"A_Lasersmoke",        A_Lasersmoke}, //jd64
    {"A_FireSingleLaser",   A_FireSingleLaser}, //jd64
    {"A_FireDoubleLaser",   A_FireDoubleLaser}, //jd64
    {"A_FireDoubleLaser1",  A_FireDoubleLaser1}, //jd64
    {"A_FireDoubleLaser2",  A_FireDoubleLaser2}, //jd64
    {"A_FireShotgun",       A_FireShotgun},
    {"A_FireShotgun2",      A_FireShotgun2},
    {"A_GunFlash",          A_GunFlash},
    {"A_HeadAttack",        A_HeadAttack},
    {"A_Hoof",              A_Hoof},
    //{"A_KeenDie", A_KeenDie},
    {"A_PossSpecial",       A_PossSpecial}, //jd64
    {"A_SposSpecial",       A_SposSpecial}, //jd64
    {"A_TrooSpecial",       A_TrooSpecial}, //jd64
    {"A_NtroSpecial",       A_NtroSpecial}, //jd64
    {"A_SargSpecial",       A_SargSpecial}, //jd64
    {"A_Sar2Special",       A_Sar2Special}, //jd64
    {"A_HeadSpecial",       A_HeadSpecial}, //jd64
    {"A_SkulSpecial",       A_SkulSpecial}, //jd64
    {"A_Bos2Special",       A_Bos2Special}, //jd64
    {"A_BossSpecial",       A_BossSpecial}, //jd64
    {"A_PainSpecial",       A_PainSpecial}, //jd64
    {"A_FattSpecial",       A_FattSpecial}, //jd64
    {"A_BabySpecial",       A_BabySpecial}, //jd64
    {"A_CybrSpecial",       A_CybrSpecial}, //jd64
    {"A_BitchSpecial",      A_BitchSpecial}, //jd64
    {"A_Light0",            A_Light0},
    {"A_Light1",            A_Light1},
    {"A_Light2",            A_Light2},
    {"A_LoadShotgun2",      A_LoadShotgun2},
    {"A_Look",              A_Look},
    {"A_Lower",             A_Lower},
    {"A_Metal",             A_Metal},
    {"A_OpenShotgun2",      A_OpenShotgun2},
    {"A_Pain",              A_Pain},
    {"A_PainAttack",        A_PainAttack},
    {"A_PainDie",           A_PainDie},
    {"A_Rocketpuff",        A_Rocketpuff}, //jd64
    {"A_PlayerScream",      A_PlayerScream},
    {"A_PosAttack",         A_PosAttack},
    {"A_Punch",             A_Punch},
    {"A_Raise",             A_Raise},
    {"A_ReFire",            A_ReFire},
    {"A_SargAttack",        A_SargAttack},
    {"A_Saw",               A_Saw},
    {"A_Scream",            A_Scream},
    {"A_BossExplode",       A_BossExplode}, //jd64
    {"A_SkelFist",          A_SkelFist},
    {"A_SkelMissile",       A_SkelMissile},
    {"A_SkelWhoosh",        A_SkelWhoosh},
    {"A_SkullAttack",       A_SkullAttack},
    {"A_SpawnFly",          A_SpawnFly},
    {"A_SpawnSound",        A_SpawnSound},
    {"A_SpidRefire",        A_SpidRefire},
    {"A_SPosAttack",        A_SPosAttack},
    {"A_Tracer",            A_Tracer},
    {"A_TroopAttack",       A_TroopAttack},
    {"A_TroopClaw",         A_TroopClaw}, //jd64
    {"A_NtroopAttack",      A_NtroopAttack}, //jd64
    {"A_MotherFloorFire",   A_MotherFloorFire}, //jd64
    {"A_MotherMissle",      A_MotherMissle}, //jd64
    {"A_SetFloorFire",      A_SetFloorFire}, //jd64
    {"A_MotherBallExplode", A_MotherBallExplode}, //jd64
    {"A_BitchTracerPuff",   A_BitchTracerPuff}, //jd64
    {"A_WeaponReady",       A_WeaponReady},
    {"A_XScream",           A_XScream},
    {0, 0}
};

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------
