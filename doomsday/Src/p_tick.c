
//**************************************************************************
//**
//** P_TICK.C
//**
//**************************************************************************

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_render.h"
#include "de_play.h"

#include "r_sky.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

//===========================================================================
// P_MobjTicker
//===========================================================================
void P_MobjTicker(mobj_t *mo)
{
	lumobj_t *lum = DL_GetLuminous(mo->light);
	int i; 

	// Set the high bit of halofactor if the light is clipped. This will 
	// make P_Ticker diminish the factor to zero. Take the first step here
	// and now, though.
	if(!lum || lum->flags & LUMF_CLIPPED)
	{
		if(mo->halofactor & 0x80)
		{
			i = (mo->halofactor & 0x7f);// - haloOccludeSpeed;
			if(i < 0) i = 0;
			mo->halofactor = i;
		}
	}
	else
	{
		if(!(mo->halofactor & 0x80))
		{
			i = (mo->halofactor & 0x7f);// + haloOccludeSpeed;
			if(i > 127) i = 127;
			mo->halofactor = 0x80 | i;
		}
	}

	// Handle halofactor.
	i = mo->halofactor & 0x7f;
	if(mo->halofactor & 0x80)
	{
		// Going up.
		i += haloOccludeSpeed;
		if(i > 127) i = 127;
	}
	else
	{
		// Going down.
		i -= haloOccludeSpeed;
		if(i < 0) i = 0;
	}
	mo->halofactor &= ~0x7f;
	mo->halofactor |= i;
}

//===========================================================================
// P_Ticker
//	Doomsday's own play-ticker.
//===========================================================================
void P_Ticker(void)
{
	thinker_t *th;

	if(!thinkercap.next) return; // Not initialized yet.

	// New ptcgens for planes?
	P_CheckPtcPlanes();	
	R_AnimateAnimGroups();
	R_SkyTicker();

	// Check all mobjs.
	for(th = thinkercap.next; th != &thinkercap; th = th->next)
	{
		// FIXME: Client mobjs!
		if(!P_IsMobjThinker(th->function)) continue;
		P_MobjTicker( (mobj_t*) th);
	}
}