
// D_main.c

#include <stdio.h>
#include <stdlib.h>
//#include <direct.h>
#include <ctype.h>
#include "jHeretic/Doomdef.h"
#include "Common/d_net.h"
#include "jHeretic/P_local.h"
#include "jHeretic/Soundst.h"
#include "jHeretic/h_config.h"
#include "jHeretic/AcFnLink.h"
#include "jHeretic/Mn_def.h"
#include "Common/hu_msg.h"
#include "Common/hu_stuff.h"
#include "Common/am_map.h"
#include "f_infine.h"
#include "g_update.h"
#include "jHeretic/m_ctrl.h"

#define viewheight	Get(DD_VIEWWINDOW_HEIGHT)

game_import_t gi;
game_export_t gx;

boolean shareware = false;		// true if only episode 1 present
boolean ExtendedWAD = false;	// true if episodes 4 and 5 present

boolean nomonsters;				// checkparm of -nomonsters
boolean respawnparm;			// checkparm of -respawn
boolean debugmode;				// checkparm of -debug
boolean devparm;					// checkparm of -devparm
boolean cdrom;					// true if cd-rom mode active
boolean singletics;				// debug flag to cancel adaptiveness
boolean noartiskip;				// whether shift-enter skips an artifact

skill_t startskill;
int     startepisode;
int     startmap;
boolean autostart;
extern boolean automapactive;
extern boolean amap_fullyopen;
extern float lookOffset;

static boolean devMap;
static char gameModeString[17];

// default font colours
const float deffontRGB[] = { .425f, 0.986f, 0.378f};
const float deffontRGB2[] = { 1.0f, 1.0f, 1.0f};

FILE   *debugfile;

// In m_multi.c.
void    MN_DrCenterTextA_CS(char *text, int center_x, int y);
void    MN_DrCenterTextB_CS(char *text, int center_x, int y);

void    H_ConsoleRegistration();
void    R_DrawPlayerSprites(ddplayer_t *viewplr);
void    R_SetAllDoomsdayFlags();
void    R_DrawRingFilter();
void    X_Drawer();
int     D_PrivilegedResponder(event_t *event);

void R_DrawLevelTitle(void)
{
	float   alpha = 1;
	int     y = 13;
	char   *lname, *lauthor, *ptr;

	/*char buf[80];
	   sprintf(buf, "fcm: %i", players[consoleplayer].plr->fixedcolormap);
	   gl.Color3f(1, 1, 1);
	   MN_DrTextA_CS(buf, 50, 50); */

	if(!cfg.levelTitle || actual_leveltime > 6 * 35)
		return;

	if(actual_leveltime < 35)
		alpha = actual_leveltime / 35.0f;
	if(actual_leveltime > 5 * 35)
		alpha = 1 - (actual_leveltime - 5 * 35) / 35.0f;

	lname = (char *) DD_GetVariable(DD_MAP_NAME);
	lauthor = (char *) DD_GetVariable(DD_MAP_AUTHOR);
    Draw_BeginZoom((1 + cfg.hudScale)/2, 160, y);

	if(lname)
	{
		// Skip the ExMx.
		ptr = strchr(lname, ':');
		if(ptr)
		{
			lname = ptr + 1;
			while(*lname && isspace(*lname))
				lname++;
		}
		M_WriteText3(160 - M_StringWidth(lname, hu_font_b) / 2, y, lname,
					hu_font_b, deffontRGB[0], deffontRGB[1], deffontRGB[2], alpha, false, 0);
		y += 20;
	}

	if(lauthor && stricmp(lauthor, "raven software"))
	{
		M_WriteText3(160 - M_StringWidth(lauthor, hu_font_a) / 2, y, lauthor,
					hu_font_a, .5f, .5f, .5f, alpha, false, 0);
	}
    Draw_EndZoom();
}

//---------------------------------------------------------------------------
//
// PROC D_Display
//
// Draw current display, possibly wiping it from the previous.
//
//---------------------------------------------------------------------------

void    R_ExecuteSetViewSize(void);

// wipegamestate can be set to -1 to force a wipe on the next draw
gamestate_t wipegamestate = GS_DEMOSCREEN;
extern boolean inhelpscreens;

extern boolean finalestage;
#define SIZEFACT 4
#define SIZEFACT2 16
void D_Display(void)
{
	static boolean viewactivestate = false;
	static boolean menuactivestate = false;
	static boolean inhelpscreensstate = false;
	static int targx =0, targy = 0, targw =0, targh = 0;
	static int /*x = 0, y = 0, */w = 320, h = 200, offy = 0;
	static int fullscreenmode = 0;
	static gamestate_t oldgamestate = -1;
	int     ay;
	player_t *vplayer = &players[displayplayer];
	boolean iscam = (vplayer->plr->flags & DDPF_CAMERA) != 0;	// $democam

	// $democam: can be set on every frame
	if(cfg.setblocks > 10 || iscam)
	{
		// Full screen.
		targx = 0;
		targy = 0;
		targw = 320;
		targh = 200;
	}
	else
	{
		targw = cfg.setblocks * 32;
		targh = cfg.setblocks * (200 - SBARHEIGHT * cfg.sbarscale / 20) / 10;
		targx = 160 - (targw >> 1);
		targy = (200 - SBARHEIGHT * cfg.sbarscale / 20 - targh) >> 1;
	}

	if(targw > w)
		w+= (((targw-w)>> 1)+SIZEFACT2)>>SIZEFACT;
	if(targw < w)
		w-= (((w-targw)>> 1)+SIZEFACT2)>>SIZEFACT;
	if(targh > h)
		h+= (((targh-h)>> 1)+SIZEFACT2)>>SIZEFACT;
	if(targh < h)
		h-= (((h-targh)>> 1)+SIZEFACT2)>>SIZEFACT;

	if(cfg.setblocks < 10)
	{
		offy = (SBARHEIGHT * cfg.sbarscale/20);
		R_ViewWindow(160-(w>>1), 100-((h+offy) >>1), w, h);
	} else
		R_ViewWindow(targx, targy, targw, targh);

	// Do buffered drawing
	switch (gamestate)
	{
	case GS_LEVEL:
		if(IS_CLIENT && (!Get(DD_GAME_READY) || !Get(DD_GOTFRAME)))
			break;
		if(leveltime < 2)
		{
			// Don't render too early; the first couple of frames 
			// might be a bit unstable -- this should be considered
			// a bug, but since there's an easy fix...
			break;
		}
		if(!automapactive || !amap_fullyopen || cfg.automapBack[3] < 1 || cfg.automapWidth < 1 || cfg.automapHeight < 1)
		{
			// Draw the player view.
			if(IS_CLIENT)
			{
				// Server updates mobj flags in NetSv_Ticker.
				R_SetAllDoomsdayFlags();
			}
			// The view angle offset.
			Set(DD_VIEWANGLE_OFFSET, ANGLE_MAX * -lookOffset);
			GL_SetFilter(vplayer->plr->filter);

			// How about fullbright?
			Set(DD_FULLBRIGHT, vplayer->powers[pw_invulnerability]);

			// Render the view with possible custom filters.
			R_RenderPlayerView(vplayer->plr);

			if(vplayer->powers[pw_invulnerability])
				R_DrawRingFilter();

			// Crosshair.
			if(!iscam)
				X_Drawer();
		}

        // Draw the automap?
        if(automapactive)
            AM_Drawer();

        // These various HUD's will be drawn unless Doomsday advises not to
        if(DD_GetInteger(DD_GAME_DRAW_HUD_HINT))
		{

            // Level information is shown for a few seconds in the 
            // beginning of a level.
            R_DrawLevelTitle();

            // Do we need to render a full status bar at this point?
            if (!(automapactive && cfg.automapHudDisplay == 0 )){

                if(!iscam)
                    if(true == (viewheight == 200) )
                    {
                        // Fullscreen. Which mode?
                        ST_Drawer(cfg.setblocks - 10, true);	// $democam
                    } else {
                        ST_Drawer(0 , true);	// $democam
                    }

                    fullscreenmode = viewheight == 200;

                }

            HU_Drawer();
		}

		// Need to update the borders?
		if(oldgamestate != GS_LEVEL ||
			((Get(DD_VIEWWINDOW_WIDTH) != 320 || menuactive ||
			 	cfg.sbarscale < 20 || (cfg.sbarscale == 20 && h < targh) || (automapactive && cfg.automapHudDisplay == 0 ))))
		{
			// Update the borders.
			GL_Update(DDUF_BORDER);
		}
		break;

	case GS_INTERMISSION:
		IN_Drawer();
		break;

	case GS_WAITING:
		// Clear the screen while waiting, doesn't mess up the menu.
		gl.Clear(DGL_COLOR_BUFFER_BIT);
		break;

	default:
		break;
	}

	GL_Update(DDUF_FULLSCREEN);

	menuactivestate = menuactive;
	viewactivestate = viewactive;
	inhelpscreensstate = inhelpscreens;
	oldgamestate = wipegamestate = gamestate;

	// draw pause pic (but not if InFine active)
	if(paused && !fi_active)
	{
		if(automapactive)
			ay = 4;
		else
			ay = 4;	// in jDOOM this is viewwindowy + 4

		GL_DrawPatch(160, ay, W_GetNumForName("PAUSED"));
	}

	// InFine is drawn whenever active.
	FI_Drawer();
}

/*
   ===============================================================================

   DEMO LOOP

   ===============================================================================
 */

/*
   int             demosequence;
   int             pagetic;
   char            *pagename = "TITLE";
 */

/*
   ================
   =
   = D_PageTicker
   =
   = Handles timing for warped projection
   =
   ================
 */
/*
   void D_PageTicker (void)
   {
   // Dedicated servers do not go through the demos.
   if(Get(DD_DEDICATED)) return;

   if (--pagetic < 0)
   D_AdvanceDemo ();
   }
 */

/*
   ================
   =
   = D_PageDrawer
   =
   ================
 */

/*extern boolean MenuActive;

   void D_PageDrawer(void)
   {
   if(!pagename) return;
   GL_DrawRawScreen(W_GetNumForName(pagename), 0, 0);
   if(demosequence == 1)
   {
   GL_DrawPatch(4, 160, W_GetNumForName("ADVISOR"));
   }
   GL_Update(DDUF_FULLSCREEN);
   } */

/*
   =================
   =
   = D_AdvanceDemo
   =
   = Called after each demo or intro demosequence finishes
   =================
 */
/*
   void D_AdvanceDemo (void)
   {
   advancedemo = true;
   }

   void D_DoAdvanceDemo (void)
   {
   players[consoleplayer].playerstate = PST_LIVE;  // don't reborn
   advancedemo = false;
   usergame = false;               // can't save / end game here
   paused = false;
   gameaction = ga_nothing;
   demosequence = (demosequence+1)%7;
   switch (demosequence)
   {
   case 0:
   pagetic = 210;
   gamestate = GS_DEMOSCREEN;
   pagename = "TITLE";
   S_StartMusic("titl", false);
   break;
   case 1:
   pagetic = 140;
   gamestate = GS_DEMOSCREEN;
   pagename = "TITLE";
   break;
   case 2:
   GL_Update(DDUF_BORDER | DDUF_FULLSCREEN);
   G_DeferedPlayDemo ("demo1");
   break;
   case 3:
   pagetic = 200;
   gamestate = GS_DEMOSCREEN;
   pagename = "CREDIT";
   break;
   case 4:
   GL_Update(DDUF_BORDER | DDUF_FULLSCREEN);
   G_DeferedPlayDemo ("demo2");
   break;
   case 5:
   pagetic = 200;
   gamestate = GS_DEMOSCREEN;
   if(shareware)
   {
   pagename = "ORDER";
   }
   else
   {
   pagename = "CREDIT";
   }
   break;
   case 6:
   GL_Update(DDUF_BORDER | DDUF_FULLSCREEN);
   G_DeferedPlayDemo ("demo3");
   break;
   }
   }
 */

/*
   ==============
   =
   = D_CheckRecordFrom
   =
   = -recordfrom <savegame num> <demoname>
   ==============
 */

void D_CheckRecordFrom(void)
{
	/*  int     p;
	   char    file[256];

	   p = ArgCheck ("-recordfrom");
	   if (!p || p > myargc-2)
	   return;

	   if(cdrom)
	   {
	   sprintf(file, SAVEGAMENAMECD"%c.hsg",Argv(p+1)[0]);
	   }
	   else
	   {
	   sprintf(file, SAVEGAMENAME"%c.hsg",Argv(p+1)[0]);
	   }
	   G_LoadGame (file);
	   G_DoLoadGame ();    // load the gameskill etc info from savegame

	   G_RecordDemo (gameskill, 1, gameepisode, gamemap, Argv(p+2)); */
}

/*
   ===============
   =
   = D_AddFile
   =
   ===============
 */

#define MAXWADFILES 20

// MAPDIR should be defined as the directory that holds development maps
// for the -wart # # command

#ifdef __NeXT__
#  define MAPDIR "/Novell/Heretic/data/"
#  define SHAREWAREWADNAME "/Novell/Heretic/source/heretic1.wad"
char   *wadfiles[MAXWADFILES] = {
	"/Novell/Heretic/source/heretic.wad",
	"/Novell/Heretic/data/texture1.lmp",
	"/Novell/Heretic/data/texture2.lmp",
	"/Novell/Heretic/data/pnames.lmp"
};
#else

#  define MAPDIR "\\data\\"

#  define SHAREWAREWADNAME "heretic1.wad"

char   *wadfiles[MAXWADFILES] = {
	"heretic.wad",
	"texture1.lmp",
	"texture2.lmp",
	"pnames.lmp"
};

#endif

char   *basedefault = "heretic.cfg";

char    exrnwads[80];
char    exrnwads2[80];

void wadprintf(void)
{
	if(debugmode)
	{
		return;
	}
#ifdef __WATCOMC__
	_settextposition(23, 2);
	_setbkcolor(1);
	_settextcolor(0);
	_outtext(exrnwads);
	_settextposition(24, 2);
	_outtext(exrnwads2);
#endif
}

void D_AddFile(char *file)
{
	int     numwadfiles;
	char   *new;

	//  char text[256];

	for(numwadfiles = 0; wadfiles[numwadfiles]; numwadfiles++);
	new = malloc(strlen(file) + 1);
	strcpy(new, file);
	if(strlen(exrnwads) + strlen(file) < 78)
	{
		if(strlen(exrnwads))
		{
			strcat(exrnwads, ", ");
		}
		else
		{
			strcpy(exrnwads, "External Wadfiles: ");
		}
		strcat(exrnwads, file);
	}
	else if(strlen(exrnwads2) + strlen(file) < 79)
	{
		if(strlen(exrnwads2))
		{
			strcat(exrnwads2, ", ");
		}
		else
		{
			strcpy(exrnwads2, "     ");
			strcat(exrnwads, ",");
		}
		strcat(exrnwads2, file);
	}
	wadfiles[numwadfiles] = new;
}

//==========================================================
//
//  Startup Thermo code
//
//==========================================================
/*#define MSG_Y       9
   //#define THERM_X 15
   //#define THERM_Y 16
   //#define THERMCOLOR  3
   #define THERM_X     14
   #define THERM_Y     14

   int thermMax;
   int thermCurrent;
   char    *startup;           // * to text screen
   char smsg[80];      // status bar line

   //
   //  Heretic startup screen shit
   //

   byte *hscreen;

   void hgotoxy(int x,int y)
   {
   hscreen = (byte *)(0xb8000 + y*160 + x*2);
   }

   void hput(unsigned char c, unsigned char a)
   {
   *hscreen++ = c;
   *hscreen++ = a;
   }

   void hprintf(char *string, unsigned char a)
   {
   #ifdef __WATCOMC__
   int i;

   if(debugmode)
   {
   puts(string);
   return;
   }
   for(i = 0; i < strlen(string); i++)
   {
   hput(string[i], a);
   }
   #endif
   }

   void drawstatus(void)
   {
   if(debugmode)
   {
   return;
   }
   #ifdef __WATCOMC__
   _settextposition(25, 2);
   _setbkcolor(1);
   _settextcolor(15);
   _outtext(smsg);
   _settextposition(25, 1);
   #endif
   }

   void status(char *string)
   {
   strcat(smsg,string);
   drawstatus();
   }

   void DrawThermo(void)
   {
   #ifdef __WATCOMC__
   unsigned char       *screen;
   int     progress;
   int     i;

   if(debugmode)
   {
   return;
   }
   #if 0
   progress = (98*thermCurrent)/thermMax;
   screen = (char *)0xb8000 + (THERM_Y*160 + THERM_X*2);
   for (i = 0;i < progress/2; i++)
   {
   switch(i)
   {
   case 4:
   case 9:
   case 14:
   case 19:
   case 29:
   case 34:
   case 39:
   case 44:
   *screen++ = 0xb3;
   *screen++ = (THERMCOLOR<<4)+15;
   break;
   case 24:
   *screen++ = 0xba;
   *screen++ = (THERMCOLOR<<4)+15;
   break;
   default:
   *screen++ = 0xdb;
   *screen++ = 0x40 + THERMCOLOR;
   break;
   }
   }
   if (progress&1)
   {
   *screen++ = 0xdd;
   *screen++ = 0x40 + THERMCOLOR;
   }
   #else
   progress = (50*thermCurrent)/thermMax+2;
   //  screen = (char *)0xb8000 + (THERM_Y*160 + THERM_X*2);
   hgotoxy(THERM_X,THERM_Y);
   for (i = 0; i < progress; i++)
   {
   //      *screen++ = 0xdb;
   //      *screen++ = 0x2a;
   hput(0xdb,0x2a);
   }
   #endif
   #endif
   }

   #ifdef __WATCOMC__
   void blitStartup(void)
   {
   byte *textScreen;

   if(debugmode)
   {
   return;
   }

   // Blit main screen
   textScreen = (byte *)0xb8000;
   memcpy(textScreen, startup, 4000);

   // Print version string
   _setbkcolor(4); // Red
   _settextcolor(14); // Yellow
   _settextposition(3, 47);
   _outtext(VERSION_TEXT);

   // Hide cursor
   _settextcursor(0x2000);
   }
   #endif

   char tmsg[300];
   void tprintf(char *msg,int initflag)
   {
   #if 0
   #ifdef __WATCOMC__
   char    temp[80];
   int start;
   int add;
   int i;
   #endif

   if(debugmode)
   {
   printf(msg);
   return;
   }
   #ifdef __WATCOMC__
   if (initflag)
   tmsg[0] = 0;
   strcat(tmsg,msg);
   blitStartup();
   DrawThermo();
   _setbkcolor(4);
   _settextcolor(15);
   for (add = start = i = 0; i <= strlen(tmsg); i++)
   if ((tmsg[i] == '\n') || (!tmsg[i]))
   {
   memset(temp,0,80);
   strncpy(temp,tmsg+start,i-start);
   _settextposition(MSG_Y+add,40-strlen(temp)/2);
   _outtext(temp);
   start = i+1;
   add++;
   }
   _settextposition(25,1);
   drawstatus();
   #else
   printf(msg);
   #endif
   #endif
   }

   void CheckAbortStartup(void)
   {
   #ifdef __WATCOMC__
   extern int lastpress;

   if(lastpress == 1)
   { // Abort if escape pressed
   CleanExit();
   }
   #endif
   }

   void IncThermo(void)
   {
   thermCurrent++;
   DrawThermo();
   CheckAbortStartup();
   }

   void InitThermo(int max)
   {
   thermMax = max;
   thermCurrent = 0;
   }

   #ifdef __WATCOMC__
   void CleanExit(void)
   {
   union REGS regs;

   I_ShutdownKeyboard();
   regs.x.eax = 0x3;
   int386(0x10, &regs, &regs);
   printf("Exited from HERETIC.\n");
   exit(1);
   }
   #endif
 */

char   *borderLumps[] = {
	"FLAT513",					// background
	"bordt",					// top
	"bordr",					// right
	"bordb",					// bottom
	"bordl",					// left
	"bordtl",					// top left
	"bordtr",					// top right
	"bordbr",					// bottom right
	"bordbl"					// bottom left
};

void H_PreInit(void)
{
	int     i, p, e, m;
	char    file[256];

	if(gi.version < DOOMSDAY_VERSION)
		Con_Error("jHeretic requires at least Doomsday " DOOMSDAY_VERSION_TEXT
				  "!\n");

	// Setup the DGL interface.
	G_InitDGL();

	// Setup the players.
	for(i = 0; i < MAXPLAYERS; i++)
	{
		players[i].plr = DD_GetPlayer(i);
		players[i].plr->extradata = (void *) &players[i];
	}
	//  gi.SetSpriteNameList(sprnames);
	DD_SetVariable(DD_SKYFLAT_NAME, "F_SKY1");
	DD_SetDefsFile("jHeretic\\jHeretic.ded");
	DD_SetConfigFile("jHeretic.cfg");
	R_SetDataPath("}Data\\jHeretic\\");
	R_SetBorderGfx(borderLumps);
	Con_DefineActions(actions);
	// Add the jHeretic cvars and ccmds to the console databases.
	H_ConsoleRegistration();

	// Add a couple of probable locations for Heretic.wad.  
	DD_AddIWAD("}Data\\jHeretic\\Heretic.wad");
	DD_AddIWAD("}Data\\Heretic.wad");
	DD_AddIWAD("}Heretic.wad");
	DD_AddIWAD("Heretic.wad");
	DD_AddStartupWAD("}Data\\jHeretic\\jHeretic.wad");

	// Default settings (used if no config file found).
	memset(&cfg, 0, sizeof(cfg));
	cfg.playerMoveSpeed = 1;
	cfg.dclickuse = false;
	cfg.mouseSensiX = 8;
	cfg.mouseSensiY = 8;
	//  cfg.joySensi = 5;
	//  cfg.joydead = 10;
	cfg.joyaxis[0] = JOYAXIS_TURN;
	cfg.joyaxis[1] = JOYAXIS_MOVE;
	cfg.screenblocks = cfg.setblocks = 10;
	cfg.ringFilter = 1;
	cfg.eyeHeight = 41;
	cfg.menuScale = .9f;
	cfg.menuColor[0] = deffontRGB[0];	// use the default colour by default.
	cfg.menuColor[1] = deffontRGB[1];
	cfg.menuColor[2] = deffontRGB[2];
	cfg.menuColor2[0] = deffontRGB2[0];	// use the default colour by default.
	cfg.menuColor2[1] = deffontRGB2[1];
	cfg.menuColor2[2] = deffontRGB2[2];
	cfg.menuEffects = 1;
	cfg.menuFog = 4;
	cfg.menuSlam = true;
	cfg.flashcolor[0] = .7f;
	cfg.flashcolor[1] = .9f;
	cfg.flashcolor[2] = 1;
	cfg.flashspeed = 4;
	cfg.turningSkull = false;
	cfg.sbarscale = 20;
	cfg.hudShown[HUD_AMMO] = true;
	cfg.hudShown[HUD_ARMOR] = true;
	cfg.hudShown[HUD_KEYS] = true;
	cfg.hudShown[HUD_HEALTH] = true;
	cfg.hudShown[HUD_ARTI] = true;
    	cfg.hudScale = .7f;
	cfg.hudColor[0] = .325f;
	cfg.hudColor[1] = .686f;
	cfg.hudColor[2] = .278f;
	cfg.hudColor[3] = 1;
	cfg.hudIconAlpha = 1;
	cfg.usePatchReplacement = true;

	cfg.tomeCounter = 10;
	cfg.tomeSound = 3;
	cfg.lookSpeed = 3;
	cfg.xhairSize = 1;
	for(i = 0; i < 4; i++)
		cfg.xhairColor[i] = 255;
	cfg.netJumping = true;
	cfg.netEpisode = 1;
	cfg.netMap = 1;
	cfg.netSkill = sk_medium;
	cfg.netColor = 4;			// Use the default color by default.
	cfg.levelTitle = true;
	cfg.customMusic = true;

	cfg.cameraNoClip = true;
	cfg.bobView = cfg.bobWeapon = 1;
	cfg.jumpPower = 9;

	cfg.statusbarAlpha = 1;
	cfg.statusbarCounterAlpha = 1;

	cfg.automapPos = 5;
	cfg.automapWidth = 1.0f;
	cfg.automapHeight = 1.0f;

	cfg.automapL0[0] = 0.42f;	// Unseen areas
	cfg.automapL0[1] = 0.42f;
	cfg.automapL0[2] = 0.42f;

	cfg.automapL1[0] = 0.41f;	// onesided lines 
	cfg.automapL1[1] = 0.30f;
	cfg.automapL1[2] = 0.15f;

	cfg.automapL2[0] = 0.82f;	// floor height change lines
	cfg.automapL2[1] = 0.70f;
	cfg.automapL2[2] = 0.52f;

	cfg.automapL3[0] = 0.47f;	// ceiling change lines
	cfg.automapL3[1] = 0.30f;
	cfg.automapL3[2] = 0.16f;

	cfg.automapBack[0] = 1.0f;
	cfg.automapBack[1] = 1.0f;
	cfg.automapBack[2] = 1.0f;
	cfg.automapBack[3] = 1.0f;
	cfg.automapLineAlpha = 1.0f;
	cfg.automapShowDoors = true;
	cfg.automapDoorGlow = 8;
	cfg.automapHudDisplay = 2;
	cfg.automapRotate = true;
	cfg.automapBabyKeys = true;
	cfg.counterCheatScale = .7f; //From jHeretic

	cfg.msgShow = true;
	cfg.msgCount = 4;
	cfg.msgScale = .8f;
	cfg.msgUptime = 5 * TICSPERSEC;
	cfg.msgAlign = ALIGN_CENTER;
	cfg.msgBlink = true;

	cfg.msgColor[0] = deffontRGB2[0];
	cfg.msgColor[1] = deffontRGB2[1];
	cfg.msgColor[2] = deffontRGB2[2];

	//  M_FindResponseFile();
	//  setbuf(stdout, NULL);
	nomonsters = ArgCheck("-nomonsters");
	respawnparm = ArgCheck("-respawn");
	devparm = ArgCheck("-devparm");
	noartiskip = ArgCheck("-noartiskip");
	debugmode = ArgCheck("-debug");
	startskill = sk_medium;
	startepisode = 1;
	startmap = 1;
	autostart = false;

	G_Register();			// read-only game status cvars (for playsim)

	// Add the automap related cvars and ccmds to the console databases
	AM_Register();

	// Add the menu related cvars and ccmds to the console databases
	MN_Register();

	// Check for -CDROM
	cdrom = false;
#if 0
	if(ArgCheck("-cdrom"))
	{
		cdrom = true;
		_mkdir("c:\\heretic.cd");
	}
#endif

	// -DEVMAP <episode> <map>
	// Adds a map wad from the development directory to the wad list,
	// and sets the start episode and the start map.
	devMap = false;
	p = ArgCheck("-devmap");
	if(p && p < myargc - 2)
	{
		e = Argv(p + 1)[0];
		m = Argv(p + 2)[0];
		sprintf(file, MAPDIR "E%cM%c.wad", e, m);
		D_AddFile(file);
		printf("DEVMAP: Episode %c, Map %c.\n", e, m);
		startepisode = e - '0';
		startmap = m - '0';
		autostart = true;
		devMap = true;
	}

	/*  p = ArgCheck("-playdemo");
	   if(!p)
	   {
	   p = ArgCheck("-timedemo");
	   }
	   if (p && p < myargc-1)
	   {
	   sprintf(file, "%s.lmp", Argv(p+1));
	   D_AddFile(file);
	   printf("Playing demo %s.lmp.\n", Argv(p+1));
	   } */

	//
	// get skill / episode / map from parms
	//
	if(ArgCheck("-deathmatch"))
	{
		cfg.netDeathmatch = true;
	}

	p = ArgCheck("-skill");
	if(p && p < myargc - 1)
	{
		startskill = Argv(p + 1)[0] - '1';
		autostart = true;
	}

	p = ArgCheck("-episode");
	if(p && p < myargc - 1)
	{
		startepisode = Argv(p + 1)[0] - '0';
		startmap = 1;
		autostart = true;
	}

	p = ArgCheck("-warp");
	if(p && p < myargc - 2)
	{
		startepisode = Argv(p + 1)[0] - '0';
		startmap = Argv(p + 2)[0] - '0';
		autostart = true;
	}
}

void status(char *msg)
{
	Con_Message("%s\n", msg);
}

/*
 * Set the game mode string.
 */
void H_IdentifyVersion(void)
{
	// The game mode string is used in netgames.
	strcpy(gameModeString, "heretic");

	if(W_CheckNumForName("E2M1") == -1)
	{
		// Can't find episode 2 maps, must be the shareware WAD
		strcpy(gameModeString, "heretic-share");
	}
	else if(W_CheckNumForName("EXTENDED") != -1)
	{
		// Found extended lump, must be the extended WAD
		strcpy(gameModeString, "heretic-ext");
	}
}

//===========================================================================
// H_PostInit
//===========================================================================
void H_PostInit(void)
{
	int     p;
	char    file[256];

	Con_FPrintf(CBLF_RULER | CBLF_WHITE | CBLF_CENTER,
				"jHeretic " VERSIONTEXT "\n");
	Con_FPrintf(CBLF_RULER, "");

	// Init savegames.
	SV_Init();

	XG_ReadTypes();
	XG_Register();			// register XG classnames

	// Set the default bindings, if needed.
	G_DefaultBindings();

	// Init the view.
	R_SetViewSize(cfg.screenblocks, 0);

	G_SetGlowing();

	if(W_CheckNumForName("E2M1") == -1)
	{							// Can't find episode 2 maps, must be the shareware WAD
		shareware = true;
		borderLumps[0] = "FLOOR04";
		R_SetBorderGfx(borderLumps);
	}
	else if(W_CheckNumForName("EXTENDED") != -1)
	{							// Found extended lump, must be the extended WAD
		ExtendedWAD = true;
	}

	//
	//  Build status bar line!
	//
	if(deathmatch)
		status("DeathMatch...");
	if(nomonsters)
		status("No Monsters...");
	if(respawnparm)
		status("Respawning...");
	if(autostart)
	{
		Con_Message("Warp to Episode %d, Map %d, Skill %d\n", startepisode,
					startmap, startskill + 1);
	}

	Con_Message("P_Init: Init Playloop state.\n");
	P_Init();

	Con_Message("HU_Init: Setting up heads up display.\n");
	HU_Init();

	Con_Message("ST_Init: Loading patches.\n");
	ST_Init();

	Con_Message("MN_Init: Init menu system.\n");
	MN_Init();

	//
	// start the apropriate game based on parms
	//

	D_CheckRecordFrom();

	p = ArgCheck("-loadgame");
	if(p && p < myargc - 1)
	{
		/*      if(cdrom)
		   {
		   sprintf(file, SAVEGAMENAMECD"%c.hsg", Argv(p+1)[0]);
		   }
		   else
		   {
		   sprintf(file, SAVEGAMENAME"%c.hsg", Argv(p+1)[0]);
		   } */
		SV_SaveGameFile(Argv(p + 1)[0] - '0', file);
		G_LoadGame(file);
	}

	// Check valid episode and map
	if((autostart || IS_NETGAME) && (devMap == false))
	{
		if(M_ValidEpisodeMap(startepisode, startmap) == false)
		{
			startepisode = 1;
			startmap = 1;
		}
	}

	if(gameaction != ga_loadgame)
	{
		GL_Update(DDUF_BORDER | DDUF_FULLSCREEN);
		if(autostart || IS_NETGAME)
		{
			G_InitNew(startskill, startepisode, startmap);
		}
		else
		{
			G_StartTitle();
		}
	}
}

void H_Ticker(void)
{
	//  if(advancedemo) D_DoAdvanceDemo();
	MN_Ticker();
	G_Ticker();
}

char *G_Get(int id)
{
	switch (id)
	{
	case DD_GAME_ID:
		return "jHeretic " VERSION_TEXT;

	case DD_GAME_MODE:
		return gameModeString;

	case DD_GAME_CONFIG:
		return gameConfigString;

	case DD_VERSION_SHORT:
		return VERSION_TEXT;

	case DD_VERSION_LONG:
		return VERSIONTEXT
			"\njHeretic is based on Heretic v1.3 by Raven Software.";

	case DD_ACTION_LINK:
		return (char *) actionlinks;

	case DD_ALT_MOBJ_THINKER:
		return (char *) P_BlasterMobjThinker;

	case DD_PSPRITE_BOB_X:
		return (char *) (FRACUNIT +
						 FixedMul(FixedMul
								  (FRACUNIT * cfg.bobWeapon,
								   players[consoleplayer].bob),
								  finecosine[(128 * leveltime) & FINEMASK]));

	case DD_PSPRITE_BOB_Y:
		return (char *) (32 * FRACUNIT +
						 FixedMul(FixedMul
								  (FRACUNIT * cfg.bobWeapon,
								   players[consoleplayer].bob),
								  finesine[(128 *
											leveltime) & FINEMASK & (FINEANGLES
																	 / 2 -
																	 1)]));

	default:
		break;
	}
	return 0;
}

void H_EndFrame(void)
{
	//  S_UpdateSounds(players[displayplayer].plr->mo);
}

void H_ConsoleBg(int *width, int *height)
{
	extern int consoleFlat;
	extern float consoleZoom;

	GL_SetFlat(consoleFlat + W_CheckNumForName("F_START") + 1);
	*width = (int) (64 * consoleZoom);
	*height = (int) (64 * consoleZoom);
}

void H_Shutdown(void)
{
}

/*
   void G_DiscardTiccmd(ticcmd_t *discarded, ticcmd_t *current)
   {
   // We're only interested in buttons.
   // Old Attack and Use buttons apply, if they're set.
   current->buttons |= discarded->buttons & (BT_ATTACK | BT_USE);
   if(discarded->buttons & BT_SPECIAL || current->buttons & BT_SPECIAL) 
   return;
   if(discarded->buttons & BT_CHANGE && !(current->buttons & BT_CHANGE))
   {
   // Use the old weapon change.
   current->buttons |= discarded->buttons & (BT_CHANGE | BT_WEAPONMASK);
   }
   }
 */

//===========================================================================
// GetGameAPI
//  Takes a copy of the engine's entry points and exported data. Returns
//  a pointer to the structure that contains our entry points and exports.
//===========================================================================
game_export_t *GetGameAPI(game_import_t * imports)
{
	// Take a copy of the imports, but only copy as much data as is 
	// allowed and legal.
	memset(&gi, 0, sizeof(gi));
	memcpy(&gi, imports, MIN_OF(sizeof(game_import_t), imports->apiSize));

	// Clear all of our exports.
	memset(&gx, 0, sizeof(gx));

	// Fill in the data for the exports.
	gx.apiSize = sizeof(gx);
	gx.PreInit = H_PreInit;
	gx.PostInit = H_PostInit;
	gx.Shutdown = H_Shutdown;
	gx.BuildTicCmd = (void (*)(void*, float)) G_BuildTiccmd;
	gx.MergeTicCmd = (void (*)(void*, void*)) G_MergeTiccmd;
	gx.G_Drawer = D_Display;
	gx.Ticker = H_Ticker;
	gx.MN_Drawer = M_Drawer;
	gx.PrivilegedResponder = (boolean (*)(event_t *)) D_PrivilegedResponder;
	gx.MN_Responder = M_Responder;
	gx.G_Responder = G_Responder;
	gx.MobjThinker = P_MobjThinker;
	gx.MobjFriction = (fixed_t (*)(void *)) P_GetMobjFriction;
	gx.EndFrame = H_EndFrame;
	gx.ConsoleBackground = H_ConsoleBg;
	gx.UpdateState = G_UpdateState;
#undef Get
	gx.Get = G_Get;

	gx.R_Init = R_InitTranslationTables;

	gx.NetServerStart = D_NetServerStarted;
	gx.NetServerStop = D_NetServerClose;
	gx.NetConnect = D_NetConnect;
	gx.NetDisconnect = D_NetDisconnect;
	gx.NetPlayerEvent = D_NetPlayerEvent;
	gx.NetWorldEvent = D_NetWorldEvent;
	gx.HandlePacket = D_HandlePacket;

	// The structure sizes.
	gx.ticcmd_size = sizeof(ticcmd_t);
	gx.vertex_size = sizeof(vertex_t);
	gx.seg_size = sizeof(seg_t);
	gx.sector_size = sizeof(sector_t);
	gx.subsector_size = sizeof(subsector_t);
	gx.node_size = sizeof(node_t);
	gx.line_size = sizeof(line_t);
	gx.side_size = sizeof(side_t);

	return &gx;
}
