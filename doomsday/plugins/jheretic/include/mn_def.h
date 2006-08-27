/*
 * Menu defines and types.
 */

#ifndef __MENU_DEFS_H_
#define __MENU_DEFS_H_

#ifndef __JHERETIC__
#  error "Using jHeretic headers without __JHeretic__"
#endif

#include "hu_stuff.h"

// Macros

#define LEFT_DIR        0
#define RIGHT_DIR       1
#define DIR_MASK        0x1
#define ITEM_HEIGHT     20
#define SLOTTEXTLEN     16
#define ASCII_CURSOR    '_'

#define LINEHEIGHT      20
#define LINEHEIGHT_A    10
#define LINEHEIGHT_B    20

#define SKULLXOFF       -22
#define SKULLYOFF       -1
#define SKULLW          22
#define SKULLH          15
#define CURSORPREF      "M_SLCTR%d"
#define SKULLBASELMP    "M_SKL00"
#define NUMCURSORS      2

#define NUMSAVESLOTS    8

#define MAX_EDIT_LEN    256

// Types

typedef struct {
    char    text[MAX_EDIT_LEN];
    char    oldtext[MAX_EDIT_LEN];  // If the current edit is canceled...
    int     firstVisible;
} EditField_t;

typedef enum {
    ITT_EMPTY,
    ITT_EFUNC,
    ITT_LRFUNC,
    ITT_SETMENU,
    ITT_INERT,
    ITT_NAVLEFT,
    ITT_NAVRIGHT
} ItemType_t;

typedef enum {
    MENU_MAIN,
    MENU_EPISODE,
    MENU_SKILL,
    MENU_OPTIONS,
    MENU_OPTIONS2,
    MENU_GAMEPLAY,
    MENU_HUD,
    MENU_MAP,
    MENU_CONTROLS,
    MENU_MOUSE,
    MENU_JOYSTICK,
    MENU_FILES,
    MENU_LOAD,
    MENU_SAVE,
    MENU_MULTIPLAYER,
    MENU_GAMESETUP,
    MENU_PLAYERSETUP,
    MENU_WEAPONSETUP,
    MENU_NONE
} MenuType_t;

// menu item flags
#define MIF_NOTALTTXT   0x01  // don't use alt text instead of lump (M_NMARE)

typedef struct {
    ItemType_t      type;
    int             flags;
    char            *text;
    void            (*func) (int option, void *data);
    int             option;
    char           *lumpname;
    void           *data;
} MenuItem_t;

typedef struct {
    int             x;
    int             y;
    void            (*drawFunc) (void);
    int             itemCount;
    const MenuItem_t *items;
    int             lastOn;
    MenuType_t      prevMenu;
    int     noHotKeys;  // 1= hotkeys are disabled on this menu
    dpatch_t       *font;          // Font for menu items.
    float          *color;      // their color.
    int             itemHeight;
    // For multipage menus.
    int             firstItem, numVisItems;
} Menu_t;

extern int      MenuTime;
extern boolean  shiftdown;
extern Menu_t  *currentMenu;
extern short    itemOn;

extern Menu_t   MapDef;

// Multiplayer menus.
extern Menu_t   MultiplayerMenu;
extern Menu_t   GameSetupMenu;
extern Menu_t   PlayerSetupMenu;

void    SetMenu(MenuType_t menu);
void    M_DrawTitle(char *text, int y);
void    M_WriteText(int x, int y, char *string);
void    M_WriteText2(int x, int y, char * string, dpatch_t * font,
                     float red, float green, float blue, float alpha);
void    M_WriteText3(int x, int y, const char *string, dpatch_t *font,
                     float red, float green, float blue, float alpha,
                     boolean doTypeIn, int initialCount);
void    M_WriteMenuText(const Menu_t * menu, int index, char *text);

// Color widget.
void    DrawColorWidget();
void    SCColorWidget(int index, void *data);
void    M_WGCurrentColor(int option, void *data);

void    M_DrawSaveLoadBorder(int x, int y);
void    M_SetupNextMenu(Menu_t* menudef);
void    M_DrawThermo(int x, int y, int thermWidth, int thermDot);
void    M_DrawSlider(const Menu_t* menu, int index, int width, int dot);
void    M_DrawColorBox(const Menu_t* menu, int index, float r, float g, float b, float a);
int     M_StringWidth(char *string, dpatch_t * font);
int     M_StringHeight(char *string, dpatch_t * font);
void    M_StartControlPanel(void);
void    M_StartMessage(char *string, void *routine, boolean input);
void    M_StopMessage(void);
void    M_ClearMenus(void);
void    M_FloatMod10(float *variable, int option);


void    SCEnterMultiplayerMenu(int option, void *data);

void    MN_TickerEx(void); // The extended ticker.

#endif
