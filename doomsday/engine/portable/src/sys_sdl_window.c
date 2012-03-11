/**\file sys_sdl_window.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2012 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 2008 Jamie Jones <jamie_jones_au@yahoo.com.au>
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
 * Cross-platform, SDL-based window management.
 *
 * This code wraps SDL window management routines in order to provide
 * common behavior. The availabilty of features and behavioral traits can
 * be queried for.
 */

// HEADER FILES ------------------------------------------------------------

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <SDL.h>
#include <SDL_syswm.h>

#include "de_base.h"
#include "de_console.h"
#include "de_system.h"
#include "de_refresh.h"
#include "de_misc.h"
#include "de_ui.h"

#include "gl_texmanager.h"
#include "rend_particle.h" // \todo Should not be necessary at this level.

// MACROS ------------------------------------------------------------------

#define LINELEN                 (1024)

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static boolean setDDWindow(ddwindow_t *win, int newWidth, int newHeight,
                           int newBPP, uint wFlags, uint uFlags);
static void setConWindowCmdLine(uint idx, const char *text,
                                unsigned int cursorPos, int flags);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// Currently active window where all drawing operations are directed at.
const ddwindow_t* theWindow;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static boolean winManagerInited = false;

static ddwindow_t mainWindow;
static boolean mainWindowInited = false;

static int screenWidth, screenHeight, screenBPP;
static boolean screenIsWindow;

// CODE --------------------------------------------------------------------

ddwindow_t* Sys_MainWindow(void)
{
    return &mainWindow;
}

static __inline ddwindow_t *getWindow(uint idx)
{
    if(!winManagerInited)
        return NULL; // Window manager is not initialized.

    if(idx != 0)
        return NULL;

    return &mainWindow;
}

ddwindow_t* Sys_Window(uint idx)
{
    return getWindow(idx);
}

boolean Sys_ChangeVideoMode(int width, int height, int bpp)
{
    int flags = SDL_OPENGL;
    const SDL_VideoInfo *info = NULL;

    LIBDENG_ASSERT_IN_MAIN_THREAD();

    // Do we need to change it?
    if(width == screenWidth && height == screenHeight && bpp == screenBPP &&
       screenIsWindow == !(theWindow->flags & DDWF_FULLSCREEN))
    {
        // Got it already.
        DEBUG_Message(("Sys_ChangeVideoMode: Ignoring because already using %ix%i bpp:%i window:%i\n",
                       width, height, bpp, screenIsWindow));
        return true;
    }

    if(theWindow->flags & DDWF_FULLSCREEN)
        flags |= SDL_FULLSCREEN;

    DEBUG_Message(("Sys_ChangeVideoMode: Setting %ix%i bpp:%i window:%i\n",
                   width, height, bpp, screenIsWindow));

    if(!SDL_SetVideoMode(width, height, bpp, flags))
    {
        // This could happen for a variety of reasons, including
        // DISPLAY not being set, the specified resolution not being
        // available, etc.
        Con_Message("SDL Error: %s\n", SDL_GetError());
        return false;
    }

    info = SDL_GetVideoInfo();
    screenWidth = info->current_w;
    screenHeight = info->current_h;
    screenBPP = info->vfmt->BitsPerPixel;
    screenIsWindow = (theWindow->flags & DDWF_FULLSCREEN? false : true);

    return true;
}

/**
 * Initialize the window manager.
 * Tasks include; checking the system environment for feature enumeration.
 *
 * @return              @c true, if initialization was successful.
 */
boolean Sys_InitWindowManager(void)
{
    if(winManagerInited)
        return true; // Already been here.

    Con_Message("Sys_InitWindowManager: Using SDL window management.\n");

    // Initialize the SDL video subsystem, unless we're going to run in
    // dedicated mode.
    if(!ArgExists("-dedicated"))
    {
/**
 * @attention Solaris has no Joystick support according to
 * https://sourceforge.net/tracker/?func=detail&atid=542099&aid=1732554&group_id=74815
 */
#ifdef SOLARIS
        if(SDL_InitSubSystem(SDL_INIT_VIDEO))
#else
        if(SDL_InitSubSystem(SDL_INIT_VIDEO | (!ArgExists("-nojoy")?SDL_INIT_JOYSTICK : 0)))
#endif
        {
            Con_Message("SDL Init Failed: %s\n", SDL_GetError());
            return false;
        }
    }

    memset(&mainWindow, 0, sizeof(mainWindow));
    winManagerInited = true;
    theWindow = &mainWindow;
    return true;
}

/**
 * Shutdown the window manager.
 *
 * @return              @c true, if shutdown was successful.
 */
boolean Sys_ShutdownWindowManager(void)
{
    if(!winManagerInited)
        return false; // Window manager is not initialized.

    if(mainWindow.type == WT_CONSOLE)
        Sys_DestroyWindow(1);

    // Now off-line, no more window management will be possible.
    winManagerInited = false;

    return true;
}

static boolean initOpenGL(void)
{
    // Attempt to set the video mode.
    if(!Sys_ChangeVideoMode(theWindow->geometry.size.width,
            theWindow->geometry.size.height, theWindow->normal.bpp))
        return false;

    Sys_GLConfigureDefaultState();
    return true;
}

/**
 * Attempt to acquire a device context for OGL rendering and then init.
 *
 * @param width         Width of the OGL window.
 * @param height        Height of the OGL window.
 * @param bpp           0= the current display color depth is used.
 * @param windowed      @c true = windowed mode ELSE fullscreen.
 * @param data          Ptr to system-specific data, e.g a window handle
 *                      or similar.
 *
 * @return              @c true if successful.
 */
static boolean createContext(int width, int height, int bpp, boolean windowed, void *data)
{
    Con_Message("createContext: OpenGL.\n");

    // Set GL attributes.  We want at least 5 bits per color and a 16
    // bit depth buffer.  Plus double buffering, of course.
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 5);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 5);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 5);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 16);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    if(!initOpenGL())
    {
        Con_Error("createContext: OpenGL init failed.\n");
    }

#ifdef MACOSX
    // Vertical sync is a GL context property.
    GL_SetVSync(true);
#endif

    return true;
}

/**
 * Complete the given wminfo_t, detailing what features are supported by
 * this window manager implementation.
 *
 * @param info          Ptr to the wminfo_t structure to complete.
 *
 * @return              @c true, if successful.
 */
boolean Sys_GetWindowManagerInfo(wminfo_t *info)
{
    if(!winManagerInited)
        return false; // Window manager is not initialized.

    if(!info)
        return false; // Wha?

    // Complete the structure detailing what features are available.
    info->canMoveWindow = false;
    info->maxWindows = 1;
    info->maxConsoles = 1;

    return true;
}

static ddwindow_t* createDDWindow(application_t* app, const Size2Raw* size,
    int bpp, int flags, ddwindowtype_t type, const char* title)
{
    // SDL only supports one window.
    if(mainWindowInited) return NULL;

    if(type == WT_CONSOLE)
    {
        Sys_ConInit(title);
    }
    else
    {
        if(!(bpp == 32 || bpp == 16))
        {
            Con_Message("createWindow: Unsupported BPP %i.", bpp);
            return 0;
        }

#if defined(WIN32)
        // We need to grab a handle from SDL so we can link other subsystems
        // (e.g. DX-based input).
        {
        struct SDL_SysWMinfo wmInfo;
        if(!SDL_GetWMInfo(&wmInfo))
            return NULL;

        mainWindow.hWnd = wmInfo.window;
        }
#endif
    }

    setDDWindow(&mainWindow, size->width, size->height, bpp, flags,
                DDSW_NOVISIBLE | DDSW_NOCENTER | DDSW_NOFULLSCREEN);

    mainWindowInited = true;
    return &mainWindow;
}

uint Sys_CreateWindow(application_t* app, uint parentIdx, const Point2Raw* origin,
    const Size2Raw* size, int bpp, int flags, ddwindowtype_t type, const char* title,
    void* userData)
{
    ddwindow_t* win;
    if(!winManagerInited) return 0;

    win = createDDWindow(app, size, bpp, flags, type, title);
    if(win) return 1; // Success.
    return 0;
}

/**
 * Destroy the specified window.
 *
 * Side-effects: If the window is fullscreen and the current video mode is
 * not that set as the desktop default: an attempt will be made to change
 * back to the desktop default video mode.
 *
 * @param idx           Index of the window to destroy (1-based).
 *
 * @return              @c true, if successful.
 */
boolean Sys_DestroyWindow(uint idx)
{
    ddwindow_t* window = getWindow(idx - 1);

    if(!window)
        return false;

    if(window->type == WT_CONSOLE)
    {
        Sys_ConShutdown(idx);
    }

    return true;
}

/**
 * Change the currently active window.
 *
 * @param idx           Index of the window to make active (1-based).
 *
 * @return              @c true, if successful.
 */
boolean Sys_SetActiveWindow(uint idx)
{
    // We only support one window, so yes its active.
    return true;
}

static boolean setDDWindow(ddwindow_t *window, int newWidth, int newHeight,
                           int newBPP, uint wFlags, uint uFlags)
{
    int             width, height, bpp, flags;
    boolean         newGLContext = false;
    boolean         changeWindowDimensions = false;
    boolean         inControlPanel = false;

    if(novideo)
        return true;

    if(uFlags & DDSW_NOCHANGES)
        return true; // Nothing to do.

    // Grab the current values.
    width = window->geometry.size.width;
    height = window->geometry.size.height;
    bpp = window->normal.bpp;
    flags = window->flags;
    // Force update on init?
    if(!window->inited && window->type == WT_NORMAL)
    {
        newGLContext = true;
    }

    if(window->type == WT_NORMAL)
        inControlPanel = UI_IsActive();

    // Change to fullscreen?
    if(!(uFlags & DDSW_NOFULLSCREEN) &&
       (flags & DDWF_FULLSCREEN) != (wFlags & DDWF_FULLSCREEN))
    {
        flags ^= DDWF_FULLSCREEN;

        if(window->type == WT_NORMAL)
        {
            newGLContext = true;
            //changeVideoMode = true;
        }
    }

    // Change window size?
    if(!(uFlags & DDSW_NOSIZE) && (width != newWidth || height != newHeight))
    {
        width = newWidth;
        height = newHeight;
        changeWindowDimensions = true;

        if(window->type == WT_NORMAL)
            newGLContext = true;
    }

    // Change BPP (bits per pixel)?
    if(window->type == WT_NORMAL)
    {
        if(!(uFlags & DDSW_NOBPP) && bpp != newBPP)
        {
            if(!(newBPP == 32 || newBPP == 16))
                Con_Error("Sys_SetWindow: Unsupported BPP %i.", newBPP);

            bpp = newBPP;

            newGLContext = true;
            //changeVideoMode = true;
        }
    }

    if(changeWindowDimensions && window->type == WT_NORMAL)
    {
        // Can't change the resolution while the UI is active.
        // (controls need to be repositioned).
        if(inControlPanel)
            UI_End();
    }
/*
    if(changeVideoMode)
    {
        if(flags & DDWF_FULLSCREEN)
        {
            if(!Sys_ChangeVideoMode(width, height, bpp))
            {
                Sys_CriticalMessage("Sys_SetWindow: Resolution change failed.");
                return false;
            }
        }
        else
        {
            // Go back to normal display settings.
            ChangeDisplaySettings(0, 0);
        }
    }
*/
    // Update the current values.
    window->geometry.size.width = width;
    window->geometry.size.height = height;
    window->normal.bpp = bpp;
    window->flags = flags;
    if(!window->inited)
        window->inited = true;

    // Do NOT modify ddwindow_t properties after this point.

    // Do we need a new GL context due to changes to the window?
    if(newGLContext)
    {   // Maybe requires a renderer restart.
extern boolean usingFog;

        boolean         glIsInited = GL_IsInited();
#if defined(WIN32)
        void           *data = window->hWnd;
#else
        void           *data = NULL;
#endif
        boolean         hadFog = false;

        if(glIsInited)
        {
            // Shut everything down, but remember our settings.
            hadFog = usingFog;
            GL_TotalReset();

            if(DD_GameLoaded() && gx.UpdateState)
                gx.UpdateState(DD_RENDER_RESTART_PRE);

            R_UnloadSvgs();
            GL_ReleaseTextures();
        }

        if(createContext(window->geometry.size.width, window->geometry.size.height,
               window->normal.bpp, (window->flags & DDWF_FULLSCREEN)? false : true, data))
        {
            // We can get on with initializing the OGL state.
            Sys_GLConfigureDefaultState();
        }

        if(glIsInited)
        {
            // Re-initialize.
            GL_TotalRestore();
            GL_InitRefresh();

            if(hadFog)
                GL_UseFog(true);

            if(DD_GameLoaded() && gx.UpdateState)
                gx.UpdateState(DD_RENDER_RESTART_POST);
        }
    }
    /*else
    {
        Sys_ChangeVideoMode(window->geometry.size.width,
            window->geometry.size.height, window->normal.bpp);
    }*/

    // If the window dimensions have changed, update any sub-systems
    // which need to respond.
    if(changeWindowDimensions && window->type == WT_NORMAL)
    {
        // Update viewport coordinates.
        R_SetViewGrid(0, 0);

        if(inControlPanel) // Reactivate the panel?
            Con_Execute(CMDS_DDAY, "panel", true, false);
    }

    return true;
}

/**
 * Attempt to set the appearance/behavioral properties of the given window.
 *
 * @param idx           Index identifier (1-based) to the window.
 * @param newX          New x position of the left side of the window.
 * @param newY          New y position of the top of the window.
 * @param newWidth      New width to set the window to.
 * @param newHeight     New height to set the window to.
 * @param newBPP        New BPP (bits-per-pixel) to change to.
 * @param wFlags        DDWF_* flags to change other appearance/behavior.
 * @param uFlags        DDSW_* flags govern how the other paramaters should be
 *                      interpreted.
 *
 *                      DDSW_NOSIZE:
 *                      If set, params 'newWidth' and 'newHeight' are ignored
 *                      and no change will be made to the window dimensions.
 *
 *                      DDSW_NOMOVE:
 *                      If set, params 'newX' and 'newY' are ignored and no
 *                      change will be made to the window position.
 *
 *                      DDSW_NOBPP:
 *                      If set, param 'newBPP' is ignored and the no change
 *                      will be made to the window color depth.
 *
 *                      DDSW_NOFULLSCREEN:
 *                      If set, the value of the DDWF_FULLSCREEN bit in param
 *                      'wFlags' is ignored and no change will be made to the
 *                      fullscreen state of the window.
 *
 *                      DDSW_NOVISIBLE:
 *                      If set, the value of the DDWF_VISIBLE bit in param
 *                      'wFlags' is ignored and no change will be made to the
 *                      window's visibility.
 *
 *                      DDSW_NOCENTER:
 *                      If set, the value of the DDWF_CENTER bit in param
 *                      'wFlags' is ignored and no change will be made to the
 *                      auto-center state of the window.
 *
 * @return              @c true, if successful.
 */
boolean Sys_SetWindow(uint idx, int newX, int newY, int newWidth, int newHeight,
                      int newBPP, uint wFlags, uint uFlags)
{
    ddwindow_t *window = getWindow(idx - 1);

    if(window)
        return setDDWindow(window, newWidth, newHeight, newBPP,
                           wFlags, uFlags);

    return false;
}

/**
 * Make the content of the framebuffer visible.
 */
void Sys_UpdateWindow(uint idx)
{
    LIBDENG_ASSERT_IN_MAIN_THREAD();

    SDL_GL_SwapBuffers();
}

/**
 * Attempt to set the title of the given window.
 *
 * @param idx           Index identifier (1-based) to the window.
 * @param title         New title for the window.
 *
 * @return              @c true, if successful.
 */
boolean Sys_SetWindowTitle(uint idx, const char *title)
{
    ddwindow_t *window = getWindow(idx - 1);

    LIBDENG_ASSERT_IN_MAIN_THREAD();

    if(window)
    {
        if(window->type == WT_NORMAL)
        {
            SDL_WM_SetCaption(title, NULL);
        }
        else // It's a terminal window.
        {
            Sys_ConSetTitle(idx, title);
        }
        return true;
    }

    return false;
}

const RectRaw* Sys_GetWindowGeometry(uint idx)
{
    ddwindow_t* window = getWindow(idx - 1);
    if(!window) return NULL;
    // Moving does not work in dedicated mode.
    if(isDedicated) return NULL;
    return &window->geometry;
}

const Point2Raw* Sys_GetWindowOrigin(uint idx)
{
    ddwindow_t* window = getWindow(idx - 1);
    if(!window) return NULL;
    // Moving does not work in dedicated mode.
    if(isDedicated) return NULL;
    return &window->geometry.origin;
}

const Size2Raw* Sys_GetWindowSize(uint idx)
{
    ddwindow_t* window = getWindow(idx - 1);
    if(!window) return NULL;
    // Moving does not work in dedicated mode.
    if(isDedicated) return NULL;
    return &window->geometry.size;
}

/**
 * Attempt to get the BPP (bits-per-pixel) of the given window.
 *
 * @param idx           Index identifier (1-based) to the window.
 * @param bpp           Address to write the BPP back to (if any).
 *
 * @return              @c true, if successful.
 */
boolean Sys_GetWindowBPP(uint idx, int *bpp)
{
    ddwindow_t* window = getWindow(idx - 1);

    if(!window || !bpp)
        return false;

    // Not in dedicated mode.
    if(isDedicated)
        return false;

    *bpp = window->normal.bpp;

    return true;
}

/**
 * Attempt to get the fullscreen-state of the given window.
 *
 * @param idx           Index identifier (1-based) to the window.
 * @param fullscreen    Address to write the fullscreen state back to (if any).
 *
 * @return              @c true, if successful.
 */
boolean Sys_GetWindowFullscreen(uint idx, boolean *fullscreen)
{
    ddwindow_t *window = getWindow(idx - 1);

    if(!window || !fullscreen)
        return false;

    *fullscreen = ((window->flags & DDWF_FULLSCREEN)? true : false);

    return true;
}

/**
 * Attempt to get a HWND handle to the given window.
 *
 * \todo: Factor platform specific design patterns out of Doomsday. We should
 * not be passing around HWND handles...
 *
 * @param idx           Index identifier (1-based) to the window.
 *
 * @return              HWND handle if successful, ELSE @c NULL,.
 */
#if defined(WIN32)
HWND Sys_GetWindowHandle(uint idx)
{
    ddwindow_t *window = getWindow(idx - 1);

    if(!window)
        return NULL;

    return window->hWnd;
}
#endif
