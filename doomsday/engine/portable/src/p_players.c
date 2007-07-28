/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2007 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006 Daniel Swanson <danij@dengine.net>
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

/*
 * p_players.c: Players
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_network.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

/**
 * Determine which console is used by the given local player.  Local
 * players are numbered starting from zero.
 */
int P_LocalToConsole(int localPlayer)
{
    int         i, count;

    for(i = 0, count = 0; i < DDMAXPLAYERS; ++i)
    {
        if(players[i].flags & DDPF_LOCAL)
        {
            if(count++ == localPlayer)
                return i;
        }
    }

    // No match!
    return -1;
}

/**
 * Determine the local player number used by a particular console.
 * Local players are numbered starting from zero.
 */
int P_ConsoleToLocal(int playerNum)
{
    int         i, count;
    
    if(!(players[playerNum].flags & DDPF_LOCAL))
        return -1; // Not local at all.
    
    for(i = 0, count = 0; i < playerNum; ++i)
    {
        if(players[i].flags & DDPF_LOCAL)
            count++;
    }
    return count;
}
