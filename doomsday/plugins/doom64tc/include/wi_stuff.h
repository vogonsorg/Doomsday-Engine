/* $Id: wi_stuff.h 3305 2006-06-11 17:00:36Z skyjake $
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
 * Intermission screens
 */

#ifndef __WI_STUFF__
#define __WI_STUFF__

#ifndef __JDOOM__
#  error "Using jDoom headers without __JDOOM__"
#endif

#include "d_player.h"

// States for the intermission

typedef enum {
    NoState = -1,
    StatCount,
    ShowNextLoc
} stateenum_t;

// Called by main loop, animate the intermission.
void            WI_Ticker(void);

// Called by main loop,
// draws the intermission directly into the screen buffer.
void            WI_Drawer(void);

// Setup for an intermission screen.
void            WI_Start(wbstartstruct_t * wbstartstruct);

void            WI_SetState(stateenum_t st);
void            WI_End(void);

#endif
