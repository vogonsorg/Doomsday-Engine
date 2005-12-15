/* $Id$
 *
 * Copyright (C) 1993-1996 by id Software, Inc.
 *
 * This source is available for distribution and/or modification
 * only under the terms of the DOOM Source Code License as
 * published by id Software. All rights reserved.
 *
 * The source is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
 * for more details.
 */

/*
 * Implements special effects:
 * Texture animation, height or lighting changes according to adjacent
 * sectors, respective utility functions, etc.
 *
 * Line Tag handling. Line and Sector triggers.
 *
 * Events are operations triggered by using, crossing,
 * or shooting special lines, or by timed thinkers.
 */

// HEADER FILES ------------------------------------------------------------

#include <stdlib.h>

#include "doomdef.h"
#include "doomstat.h"
#include "d_config.h"
#include "m_argv.h"
#include "m_random.h"
#include "r_local.h"
#include "p_local.h"
#include "g_game.h"
#include "s_sound.h"
#include "r_state.h"

// MACROS ------------------------------------------------------------------

// FIXME: Remove fixed limits

#define MAXANIMS        32
#define MAXLINEANIMS    64 // Animating line specials

// Limit number of sectors tested for adjoining height differences
#define MAX_ADJOINING_SECTORS   20

// TYPES -------------------------------------------------------------------

// Animating textures and planes
// There is another anim_t used in wi_stuff, unrelated.
typedef struct {
    boolean istexture;
    int     picnum;
    int     basepic;
    int     numpics;
    int     speed;
} anim_t;

// source animation definition
typedef struct {
    boolean istexture;  // if false, it is a flat
    char    endname[9];
    char    startname[9];
    int     speed;
} animdef_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern anim_t anims[MAXANIMS];
extern anim_t *lastanim;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

#if 0
// Floor/ceiling animation sequences,
//  defined by first and last frame,
//  i.e. the flat (64x64 tile) name to
//  be used.
// The full animation sequence is given
//  using all the flats between the start
//  and end entry, in the order found in
//  the WAD file.
//
animdef_t animdefs[] = {
    {false, "NUKAGE3", "NUKAGE1", 8},
    {false, "FWATER4", "FWATER1", 8},
    {false, "SWATER4", "SWATER1", 8},
    {false, "LAVA4", "LAVA1", 8},
    {false, "BLOOD3", "BLOOD1", 8},

    // DOOM II flat animations.
    {false, "RROCK08", "RROCK05", 8},
    {false, "SLIME04", "SLIME01", 8},
    {false, "SLIME08", "SLIME05", 8},
    {false, "SLIME12", "SLIME09", 8},

    {true, "BLODGR4", "BLODGR1", 8},
    {true, "SLADRIP3", "SLADRIP1", 8},

    {true, "BLODRIP4", "BLODRIP1", 8},
    {true, "FIREWALL", "FIREWALA", 8},
    {true, "GSTFONT3", "GSTFONT1", 8},
    {true, "FIRELAVA", "FIRELAV3", 8},
    {true, "FIREMAG3", "FIREMAG1", 8},
    {true, "FIREBLU2", "FIREBLU1", 8},
    {true, "ROCKRED3", "ROCKRED1", 8},

    {true, "BFALL4", "BFALL1", 8},
    {true, "SFALL4", "SFALL1", 8},
    {true, "WFALL4", "WFALL1", 8},
    {true, "DBRAIN4", "DBRAIN1", 8},

    {-1}
};

anim_t  anims[MAXANIMS];
anim_t *lastanim;
#endif

boolean levelTimer;
int     levelTimeCount;

int   numlinespecials;
line_t *linespeciallist[MAXLINEANIMS];

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

void P_InitPicAnims(void)
{
#if 0
    int     i, k;

    //  Init animation
    lastanim = anims;
    for(i = 0; animdefs[i].istexture != -1; i++)
    {
        if(animdefs[i].istexture)
        {
            // different episode ?
            if(R_CheckTextureNumForName(animdefs[i].startname) == -1)
                continue;

            lastanim->picnum = R_TextureNumForName(animdefs[i].endname);
            lastanim->basepic = R_TextureNumForName(animdefs[i].startname);
        }
        else
        {
            if(W_CheckNumForName(animdefs[i].startname) == -1)
                continue;

            lastanim->picnum = R_FlatNumForName(animdefs[i].endname);
            lastanim->basepic = R_FlatNumForName(animdefs[i].startname);
        }

        lastanim->istexture = animdefs[i].istexture;
        lastanim->numpics = lastanim->picnum - lastanim->basepic + 1;

        if(lastanim->numpics < 2)
        {
            Con_Error("P_InitPicAnims: bad cycle from %s to %s",
                      animdefs[i].startname, animdefs[i].endname);
        }

        // Tell Doomsday about the animation sequence.
        for(k = lastanim->basepic; k <= lastanim->picnum; k++)
        {
            R_SetAnimGroup(lastanim->istexture ? DD_TEXTURE : DD_FLAT, k,
                           i + 1);
        }

        lastanim->speed = animdefs[i].speed;
        lastanim++;
    }
#endif
}

/*
 * Return sector_t * of sector next to current.
 * NULL if not two-sided line
 */
sector_t *getNextSector(line_t *line, sector_t *sec)
{
    if(!(P_GetIntp(DMU_LINE, line, DMU_FLAGS) & ML_TWOSIDED))
        return NULL;

    if(P_GetPtrp(DMU_LINE, line, DMU_FRONT_SECTOR) == sec)
        return P_GetPtrp(DMU_LINE, line, DMU_BACK_SECTOR);

    return P_GetPtrp(DMU_LINE, line, DMU_FRONT_SECTOR);
}

/*
 * FIND LOWEST FLOOR HEIGHT IN SURROUNDING SECTORS
 */
fixed_t P_FindLowestFloorSurrounding(sector_t *sec)
{
    int     i;
    line_t *check;
    sector_t *other;

    fixed_t floor = P_GetFixedp(DMU_SECTOR, sec, DMU_FLOOR_HEIGHT);

    for(i = 0; i < P_GetIntp(DMU_SECTOR, sec, DMU_LINECOUNT); i++)
    {
#ifdef TODO_MAP_UPDATE
        check = sec->Lines[i];
        other = getNextSector(check, sec);

        if(!other)
            continue;

        if(other->floorheight < floor)
            floor = other->floorheight;
#endif
    }
    return floor;
}

/*
 * FIND HIGHEST FLOOR HEIGHT IN SURROUNDING SECTORS
 */
fixed_t P_FindHighestFloorSurrounding(sector_t *sec)
{
    int     i;
    line_t *check;
    sector_t *other;
    fixed_t floor = -500 * FRACUNIT;

    for(i = 0; i < P_GetIntp(DMU_SECTOR, sec, DMU_LINECOUNT); i++)
    {
#ifdef TODO_MAP_UPDATE
        check = sec->Lines[i];
        other = getNextSector(check, sec);

        if(!other)
            continue;

        if(other->floorheight > floor)
            floor = other->floorheight;
    }
#endif
    return floor;
}

/*
 * FIND NEXT HIGHEST FLOOR IN SURROUNDING SECTORS
 * Note: this should be doable w/o a fixed array.
 */
fixed_t P_FindNextHighestFloor(sector_t *sec, int currentheight)
{
    int     i;
    int     h;
    int     min;
    line_t *check;
    sector_t *other;
    fixed_t height = currentheight;

    fixed_t heightlist[MAX_ADJOINING_SECTORS];

    for(i = 0, h = 0; i < P_GetIntp(DMU_SECTOR, sec, DMU_LINECOUNT); i++)
    {
#ifdef TODO_MAP_UPDATE
        check = sec->Lines[i];
        other = getNextSector(check, sec);

        if(!other)
            continue;

        if(other->floorheight > height)
            heightlist[h++] = other->floorheight;
#endif
        // Check for overflow. Exit.
        if(h >= MAX_ADJOINING_SECTORS)
        {
            fprintf(stderr, "Sector with more than 20 adjoining sectors\n");
            break;
        }
    }

    // Find lowest height in list
    if(!h)
        return currentheight;

    min = heightlist[0];

    // Range checking?
    for(i = 1; i < h; i++)
        if(heightlist[i] < min)
            min = heightlist[i];

    return min;
}

/*
 * FIND LOWEST CEILING IN THE SURROUNDING SECTORS
 */
fixed_t P_FindLowestCeilingSurrounding(sector_t *sec)
{
    int     i;
    line_t *check;
    sector_t *other;
    fixed_t height = MAXINT;

    for(i = 0; i < P_GetIntp(DMU_SECTOR, sec, DMU_LINECOUNT); i++)
    {
#ifdef TODO_MAP_UPDATE
        check = sec->Lines[i];
        other = getNextSector(check, sec);

        if(!other)
            continue;

        if(other->ceilingheight < height)
            height = other->ceilingheight;
#endif
    }
    return height;
}

/*
 * FIND HIGHEST CEILING IN THE SURROUNDING SECTORS
 */
fixed_t P_FindHighestCeilingSurrounding(sector_t *sec)
{
    int     i;
    line_t *check;
    sector_t *other;
    fixed_t height = 0;

    for(i = 0; i < P_GetIntp(DMU_SECTOR, sec, DMU_LINECOUNT); i++)
    {
#ifdef TODO_MAP_UPDATE
        check = sec->Lines[i];
        other = getNextSector(check, sec);

        if(!other)
            continue;

        if(other->ceilingheight > height)
            height = other->ceilingheight;
#endif
    }
    return height;
}

/*
 * RETURN NEXT SECTOR # THAT LINE TAG REFERS TO
 */
int P_FindSectorFromLineTag(line_t *line, int start)
{
    int     i;
    int count = DD_GetInteger(DD_SECTOR_COUNT);

#ifdef TODO_MAP_UPDATE
    for(i = start + 1; i < count; i++)
        if(sectors[i].tag == line->tag)
            return i;
#endif

    return -1;
}

/*
 * Find minimum light from an adjacent sector
 */
int P_FindMinSurroundingLight(sector_t *sector, int max)
{
    int     i;
    int     min;

    line_t *line;
    sector_t *check;

    min = max;
    for(i = 0; i < P_GetIntp(DMU_SECTOR, sector, DMU_LINECOUNT); i++)
    {
#ifdef TODO_MAP_UPDATE
        line = sector->Lines[i];
        check = getNextSector(line, sector);

        if(!check)
            continue;

        if(check->lightlevel < min)
            min = check->lightlevel;
#endif
    }
    return min;
}

/*
 * Called every time a thing origin is about to cross a line with
 * a non 0 special.
 */
void P_CrossSpecialLine(int linenum, int side, mobj_t *thing)
{
    line_t *line;
    int     ok;

    line = P_ToPtr(DMU_LINE, linenum);

    // Extended functionality overrides old.
    if(XL_CrossLine(line, side, thing))
        return;

    //  Triggers that other things can activate
    if(!thing->player)
    {
        // Things that should NOT trigger specials...
        switch (thing->type)
        {
        case MT_ROCKET:
        case MT_PLASMA:
        case MT_BFG:
        case MT_TROOPSHOT:
        case MT_HEADSHOT:
        case MT_BRUISERSHOT:
            return;
            break;

        default:
            break;
        }

        ok = 0;
        switch (line->special)
        {
        case 39:                // TELEPORT TRIGGER
        case 97:                // TELEPORT RETRIGGER
        case 125:               // TELEPORT MONSTERONLY TRIGGER
        case 126:               // TELEPORT MONSTERONLY RETRIGGER
        case 4:             // RAISE DOOR
        case 10:                // PLAT DOWN-WAIT-UP-STAY TRIGGER
        case 88:                // PLAT DOWN-WAIT-UP-STAY RETRIGGER
            ok = 1;
            break;
        }

        // Anything can trigger this line!
            if(line->flags & ML_ALLTRIGGER)
                ok = 1;

        if(!ok)
            return;
    }

    // Note: could use some const's here.
    switch (line->special)
    {
        // TRIGGERS.
        // All from here to RETRIGGERS.
    case 2:
        // Open Door
        EV_DoDoor(line, open);
        line->special = 0;
        break;

    case 3:
        // Close Door
        EV_DoDoor(line, close);
        line->special = 0;
        break;

    case 4:
        // Raise Door
        EV_DoDoor(line, normal);
        line->special = 0;
        break;

    case 5:
        // Raise Floor
        EV_DoFloor(line, raiseFloor);
        line->special = 0;
        break;

    case 6:
        // Fast Ceiling Crush & Raise
        EV_DoCeiling(line, fastCrushAndRaise);
        line->special = 0;
        break;

    case 8:
        // Build Stairs
        EV_BuildStairs(line, build8);
        line->special = 0;
        break;

    case 10:
        // PlatDownWaitUp
        EV_DoPlat(line, downWaitUpStay, 0);
        line->special = 0;
        break;

    case 12:
        // Light Turn On - brightest near
        EV_LightTurnOn(line, 0);
        line->special = 0;
        break;

    case 13:
        // Light Turn On 255
        EV_LightTurnOn(line, 255);
        line->special = 0;
        break;

    case 16:
        // Close Door 30
        EV_DoDoor(line, close30ThenOpen);
        line->special = 0;
        break;

    case 17:
        // Start Light Strobing
        EV_StartLightStrobing(line);
        line->special = 0;
        break;

    case 19:
        // Lower Floor
        EV_DoFloor(line, lowerFloor);
        line->special = 0;
        break;

    case 22:
        // Raise floor to nearest height and change texture
        EV_DoPlat(line, raiseToNearestAndChange, 0);
        line->special = 0;
        break;

    case 25:
        // Ceiling Crush and Raise
        EV_DoCeiling(line, crushAndRaise);
        line->special = 0;
        break;

    case 30:
        // Raise floor to shortest texture height
        //  on either side of lines.
        EV_DoFloor(line, raiseToTexture);
        line->special = 0;
        break;

    case 35:
        // Lights Very Dark
        EV_LightTurnOn(line, 35);
        line->special = 0;
        break;

    case 36:
        // Lower Floor (TURBO)
        EV_DoFloor(line, turboLower);
        line->special = 0;
        break;

    case 37:
        // LowerAndChange
        EV_DoFloor(line, lowerAndChange);
        line->special = 0;
        break;

    case 38:
        // Lower Floor To Lowest
        EV_DoFloor(line, lowerFloorToLowest);
        line->special = 0;
        break;

    case 39:
        // TELEPORT!
        EV_Teleport(line, side, thing);
        line->special = 0;
        break;

    case 40:
        // RaiseCeilingLowerFloor
        EV_DoCeiling(line, raiseToHighest);
        EV_DoFloor(line, lowerFloorToLowest);
        line->special = 0;
        break;

    case 44:
        // Ceiling Crush
        EV_DoCeiling(line, lowerAndCrush);
        line->special = 0;
        break;

    case 52:
        // EXIT!
        G_ExitLevel();
        break;

    case 53:
        // Perpetual Platform Raise
        EV_DoPlat(line, perpetualRaise, 0);
        line->special = 0;
        break;

    case 54:
        // Platform Stop
        EV_StopPlat(line);
        line->special = 0;
        break;

    case 56:
        // Raise Floor Crush
        EV_DoFloor(line, raiseFloorCrush);
        line->special = 0;
        break;

    case 57:
        // Ceiling Crush Stop
        EV_CeilingCrushStop(line);
        line->special = 0;
        break;

    case 58:
        // Raise Floor 24
        EV_DoFloor(line, raiseFloor24);
        line->special = 0;
        break;

    case 59:
        // Raise Floor 24 And Change
        EV_DoFloor(line, raiseFloor24AndChange);
        line->special = 0;
        break;

    case 104:
        // Turn lights off in sector(tag)
        EV_TurnTagLightsOff(line);
        line->special = 0;
        break;

    case 108:
        // Blazing Door Raise (faster than TURBO!)
        EV_DoDoor(line, blazeRaise);
        line->special = 0;
        break;

    case 109:
        // Blazing Door Open (faster than TURBO!)
        EV_DoDoor(line, blazeOpen);
        line->special = 0;
        break;

    case 100:
        // Build Stairs Turbo 16
        EV_BuildStairs(line, turbo16);
        line->special = 0;
        break;

    case 110:
        // Blazing Door Close (faster than TURBO!)
        EV_DoDoor(line, blazeClose);
        line->special = 0;
        break;

    case 119:
        // Raise floor to nearest surr. floor
        EV_DoFloor(line, raiseFloorToNearest);
        line->special = 0;
        break;

    case 121:
        // Blazing PlatDownWaitUpStay
        EV_DoPlat(line, blazeDWUS, 0);
        line->special = 0;
        break;

    case 124:
        // Secret EXIT
        G_SecretExitLevel();
        break;

    case 125:
        // TELEPORT MonsterONLY
        if(!thing->player)
        {
            EV_Teleport(line, side, thing);
            line->special = 0;
        }
        break;

    case 130:
        // Raise Floor Turbo
        EV_DoFloor(line, raiseFloorTurbo);
        line->special = 0;
        break;

    case 141:
        // Silent Ceiling Crush & Raise
        EV_DoCeiling(line, silentCrushAndRaise);
        line->special = 0;
        break;

        // RETRIGGERS.  All from here till end.
    case 72:
        // Ceiling Crush
        EV_DoCeiling(line, lowerAndCrush);
        break;

    case 73:
        // Ceiling Crush and Raise
        EV_DoCeiling(line, crushAndRaise);
        break;

    case 74:
        // Ceiling Crush Stop
        EV_CeilingCrushStop(line);
        break;

    case 75:
        // Close Door
        EV_DoDoor(line, close);
        break;

    case 76:
        // Close Door 30
        EV_DoDoor(line, close30ThenOpen);
        break;

    case 77:
        // Fast Ceiling Crush & Raise
        EV_DoCeiling(line, fastCrushAndRaise);
        break;

    case 79:
        // Lights Very Dark
        EV_LightTurnOn(line, 35);
        break;

    case 80:
        // Light Turn On - brightest near
        EV_LightTurnOn(line, 0);
        break;

    case 81:
        // Light Turn On 255
        EV_LightTurnOn(line, 255);
        break;

    case 82:
        // Lower Floor To Lowest
        EV_DoFloor(line, lowerFloorToLowest);
        break;

    case 83:
        // Lower Floor
        EV_DoFloor(line, lowerFloor);
        break;

    case 84:
        // LowerAndChange
        EV_DoFloor(line, lowerAndChange);
        break;

    case 86:
        // Open Door
        EV_DoDoor(line, open);
        break;

    case 87:
        // Perpetual Platform Raise
        EV_DoPlat(line, perpetualRaise, 0);
        break;

    case 88:
        // PlatDownWaitUp
        EV_DoPlat(line, downWaitUpStay, 0);
        break;

    case 89:
        // Platform Stop
        EV_StopPlat(line);
        break;

    case 90:
        // Raise Door
        EV_DoDoor(line, normal);
        break;

    case 91:
        // Raise Floor
        EV_DoFloor(line, raiseFloor);
        break;

    case 92:
        // Raise Floor 24
        EV_DoFloor(line, raiseFloor24);
        break;

    case 93:
        // Raise Floor 24 And Change
        EV_DoFloor(line, raiseFloor24AndChange);
        break;

    case 94:
        // Raise Floor Crush
        EV_DoFloor(line, raiseFloorCrush);
        break;

    case 95:
        // Raise floor to nearest height
        // and change texture.
        EV_DoPlat(line, raiseToNearestAndChange, 0);
        break;

    case 96:
        // Raise floor to shortest texture height
        // on either side of lines.
        EV_DoFloor(line, raiseToTexture);
        break;

    case 97:
        // TELEPORT!
        EV_Teleport(line, side, thing);
        break;

    case 98:
        // Lower Floor (TURBO)
        EV_DoFloor(line, turboLower);
        break;

    case 105:
        // Blazing Door Raise (faster than TURBO!)
        EV_DoDoor(line, blazeRaise);
        break;

    case 106:
        // Blazing Door Open (faster than TURBO!)
        EV_DoDoor(line, blazeOpen);
        break;

    case 107:
        // Blazing Door Close (faster than TURBO!)
        EV_DoDoor(line, blazeClose);
        break;

    case 120:
        // Blazing PlatDownWaitUpStay.
        EV_DoPlat(line, blazeDWUS, 0);
        break;

    case 126:
        // TELEPORT MonsterONLY.
        if(!thing->player)
            EV_Teleport(line, side, thing);
        break;

    case 128:
        // Raise To Nearest Floor
        EV_DoFloor(line, raiseFloorToNearest);
        break;

    case 129:
        // Raise Floor Turbo
        EV_DoFloor(line, raiseFloorTurbo);
        break;
    }
}

/*
 * Called when a thing shoots a special line.
 */
void P_ShootSpecialLine(mobj_t *thing, line_t *line)
{
    int     ok;

    //  Impacts that other things can activate.
    if(!thing->player)
    {
        ok = 0;
        switch (line->special)
        {
        case 46:
            // OPEN DOOR IMPACT
            ok = 1;
            break;
        }
        if(!ok)
            return;
    }

    switch (line->special)
    {
    case 24:
        // RAISE FLOOR
        EV_DoFloor(line, raiseFloor);
        P_ChangeSwitchTexture(line, 0);
        break;

    case 46:
        // OPEN DOOR
        EV_DoDoor(line, open);
        P_ChangeSwitchTexture(line, 1);
        break;

    case 47:
        // RAISE FLOOR NEAR AND CHANGE
        EV_DoPlat(line, raiseToNearestAndChange, 0);
        P_ChangeSwitchTexture(line, 0);
        break;
    }
}

/*
 * Called every tic frame that the player origin is in a special sector
 */
void P_PlayerInSpecialSector(player_t *player)
{
    sector_t *sector;

    sector = player->plr->mo->subsector->sector;

    // Falling, not all the way down yet?
    if(player->plr->mo->z != sector->floorheight)
        return;

    // Has hitten ground.
    switch (sector->special)
    {
    case 5:
        // HELLSLIME DAMAGE
        if(!player->powers[pw_ironfeet])
            if(!(leveltime & 0x1f))
                P_DamageMobj(player->plr->mo, NULL, NULL, 10);
        break;

    case 7:
        // NUKAGE DAMAGE
        if(!player->powers[pw_ironfeet])
            if(!(leveltime & 0x1f))
                P_DamageMobj(player->plr->mo, NULL, NULL, 5);
        break;

    case 16:
        // SUPER HELLSLIME DAMAGE
    case 4:
        // STROBE HURT
        if(!player->powers[pw_ironfeet] || (P_Random() < 5))
        {
            if(!(leveltime & 0x1f))
                P_DamageMobj(player->plr->mo, NULL, NULL, 20);
        }
        break;

    case 9:
        // SECRET SECTOR
        player->secretcount++;
        sector->special = 0;
        if(cfg.secretMsg)
        {
            P_SetMessage(player, "You've found a secret area!");
            S_ConsoleSound(sfx_getpow, 0, player - players);
            //S_NetStartPlrSound(player, sfx_itmbk);
            //gamemode==commercial? sfx_radio : sfx_tink);
        }
        break;

    case 11:
        // EXIT SUPER DAMAGE! (for E1M8 finale)
        player->cheats &= ~CF_GODMODE;

        if(!(leveltime & 0x1f))
            P_DamageMobj(player->plr->mo, NULL, NULL, 20);

        if(player->health <= 10)
            G_ExitLevel();
        break;

    default:
        /*Con_Error ("P_PlayerInSpecialSector: "
           "unknown special %i",
           sector->special); */
        break;
    };
}

/*
 * Animate planes, scroll walls, etc.
 */
void P_UpdateSpecials(void)
{
    int     i;
    line_t *line;

    // Extended lines and sectors.
    XG_Ticker();

    //  LEVEL TIMER
    if(levelTimer == true)
    {
        levelTimeCount--;
        if(!levelTimeCount)
            G_ExitLevel();
    }

#if 0
    //  ANIMATE FLATS AND TEXTURES GLOBALLY
    for(anim = anims; anim < lastanim; anim++)
    {
        for(i = anim->basepic; i < anim->basepic + anim->numpics; i++)
        {
            pic =
                anim->basepic +
                ((leveltime / anim->speed + i) % anim->numpics);
            if(anim->istexture)
                R_SetTextureTranslation(i, pic);
            else
                R_SetFlatTranslation(i, pic);
        }
    }
#endif

    //  ANIMATE LINE SPECIALS
    for(i = 0; i < numlinespecials; i++)
    {
        line = linespeciallist[i];
        switch (line->special)
        {
        case 48:
            // EFFECT FIRSTCOL SCROLL +
#ifdef TODO_MAP_UPDATE
            sides[line->sidenum[0]].textureoffset += FRACUNIT;
#endif
            break;
        }
    }

    //  DO BUTTONS
    // FIXME! remove fixed limt.
    for(i = 0; i < MAXBUTTONS; i++)
        if(buttonlist[i].btimer)
        {
            buttonlist[i].btimer--;
            if(!buttonlist[i].btimer)
            {
#ifdef TODO_MAP_UPDATE
                switch (buttonlist[i].where)
                {
                case top:
                    sides[buttonlist[i].line->sidenum[0]].toptexture =
                        buttonlist[i].btexture;
                    break;

                case middle:
                    sides[buttonlist[i].line->sidenum[0]].midtexture =
                        buttonlist[i].btexture;
                    break;

                case bottom:
                    sides[buttonlist[i].line->sidenum[0]].bottomtexture =
                        buttonlist[i].btexture;
                    break;
                }
                S_StartSound(sfx_swtchn, buttonlist[i].soundorg);
                memset(&buttonlist[i], 0, sizeof(button_t));
#endif
            }
        }

}

int EV_DoDonut(line_t *line)
{
    sector_t *s1;
    sector_t *s2;
    sector_t *s3;
    int     secnum;
    int     rtn;
    int     i;
    floormove_t *floor;

    secnum = -1;
    rtn = 0;
    while((secnum = P_FindSectorFromLineTag(line, secnum)) >= 0)
    {
#ifdef TODO_MAP_UPDATE
        s1 = &sectors[secnum];
#endif

        // ALREADY MOVING?  IF SO, KEEP GOING...
        if(s1->specialdata)
            continue;

        rtn = 1;
        s2 = getNextSector(s1->Lines[0], s1);
        for(i = 0; i < s2->linecount; i++)
        {
            if((!s2->Lines[i]->flags & ML_TWOSIDED) ||
               (s2->Lines[i]->backsector == s1))
                continue;
            s3 = s2->Lines[i]->backsector;

            //  Spawn rising slime
            floor = Z_Malloc(sizeof(*floor), PU_LEVSPEC, 0);
            P_AddThinker(&floor->thinker);
            s2->specialdata = floor;
            floor->thinker.function = T_MoveFloor;
            floor->type = donutRaise;
            floor->crush = false;
            floor->direction = 1;
            floor->sector = s2;
            floor->speed = FLOORSPEED / 2;
            floor->texture = s3->floorpic;
            floor->newspecial = 0;
            floor->floordestheight = s3->floorheight;

            //  Spawn lowering donut-hole
            floor = Z_Malloc(sizeof(*floor), PU_LEVSPEC, 0);
            P_AddThinker(&floor->thinker);
            s1->specialdata = floor;
            floor->thinker.function = T_MoveFloor;
            floor->type = lowerFloor;
            floor->crush = false;
            floor->direction = -1;
            floor->sector = s1;
            floor->speed = FLOORSPEED / 2;
            floor->floordestheight = s3->floorheight;
            break;
        }
    }
    return rtn;
}

/*
 * After the map has been loaded, scan for specials that spawn thinkers
 * Parses command line parameters (FIXME: use global state variables).
 */
void P_SpawnSpecials(void)
{
    sector_t *sector;
    int     i;
    int     nsecs = DD_GetInteger(DD_SECTOR_COUNT);
    int     nlines = DD_GetInteger(DD_LINE_COUNT);

    // See if -TIMER needs to be used.
    levelTimer = false;

    i = ArgCheck("-avg");
    if(i && deathmatch)
    {
        levelTimer = true;
        levelTimeCount = 20 * 60 * 35;
    }

    i = ArgCheck("-timer");
    if(i && deathmatch)
    {
        int     time;

        time = atoi(Argv(i + 1)) * 60 * 35;
        levelTimer = true;
        levelTimeCount = time;
    }

    //  Init special SECTORs.
#ifdef TODO_MAP_UPDATE
    sector = sectors;
    for(i = 0; i < nsecs; i++)
    {
        if(!sector->special)
            continue;

        if(IS_CLIENT)
        {
            switch (sector->special)
            {
            case 9:
                // SECRET SECTOR
                totalsecret++;
                break;
            }
            continue;
        }

        switch (sector->special)
        {
        case 1:
            // FLICKERING LIGHTS
            P_SpawnLightFlash(sector);
            break;

        case 2:
            // STROBE FAST
            P_SpawnStrobeFlash(sector, FASTDARK, 0);
            break;

        case 3:
            // STROBE SLOW
            P_SpawnStrobeFlash(sector, SLOWDARK, 0);
            break;

        case 4:
            // STROBE FAST/DEATH SLIME
            P_SpawnStrobeFlash(sector, FASTDARK, 0);
            sector->special = 4;
            break;

        case 8:
            // GLOWING LIGHT
            P_SpawnGlowingLight(sector);
            break;

        case 9:
            // SECRET SECTOR
            totalsecret++;
            break;

        case 10:
            // DOOR CLOSE IN 30 SECONDS
            P_SpawnDoorCloseIn30(sector);
            break;

        case 12:
            // SYNC STROBE SLOW
            P_SpawnStrobeFlash(sector, SLOWDARK, 1);
            break;

        case 13:
            // SYNC STROBE FAST
            P_SpawnStrobeFlash(sector, FASTDARK, 1);
            break;

        case 14:
            // DOOR RAISE IN 5 MINUTES
            P_SpawnDoorRaiseIn5Mins(sector, i);
            break;

        case 17:
            P_SpawnFireFlicker(sector);
            break;
        }
    }
#endif

    //  Init line EFFECTs
#ifdef TODO_MAP_UPDATE
    numlinespecials = 0;
    for(i = 0; i < nlines; i++)
    {
        switch (lines[i].special)
        {
        case 48:
            // EFFECT FIRSTCOL SCROLL+
            linespeciallist[numlinespecials] = &lines[i];
            numlinespecials++;
            break;
        }
    }
#endif

    P_RemoveAllActiveCeilings();  // jff 2/22/98 use killough's scheme

    P_RemoveAllActivePlats();     // killough

    // FIXME: Remove fixed limit
    for(i = 0; i < MAXBUTTONS; i++)
        memset(&buttonlist[i], 0, sizeof(button_t));

    // Init extended generalized lines and sectors.
    XG_Init();
}
