//===========================================================================
// GL_MAIN.H
//===========================================================================
#ifndef __DOOMSDAY_GRAPHICS_H__
#define __DOOMSDAY_GRAPHICS_H__

#include "con_decl.h"

typedef enum blendmode_e {
	BM_NORMAL,
	BM_ADD,
	BM_DARK,
	BM_SUBTRACT,
	BM_REVERSE_SUBTRACT,
	BM_MUL,
	BM_INVERSE_MUL
} blendmode_t;

boolean			GL_IsInited(void);

void			GL_Init(void);
void			GL_Shutdown(void);
void			GL_TotalReset(boolean doShutdown, boolean loadLightMaps);
void			GL_Init2DState(void);
void			GL_SwitchTo3DState(boolean push_state);
void			GL_Restore2DState(int step);
void			GL_ProjectionMatrix(void);
void			GL_RuntimeMode(void);
int				GL_ChangeResolution(int w, int h, int bits);
void			GL_Update(int flags);
void			GL_DoUpdate(void);
void			GL_BlendMode(blendmode_t mode);

void			GL_InitRefresh(boolean loadLightMaps);
void			GL_ShutdownRefresh(void);
void			GL_UseFog(int yes);
void			GL_InitVarFont(void);
void			GL_ShutdownVarFont(void);

// Returns a pointer to a copy of the screen. The pointer must be 
// deallocated by the caller.
unsigned char	*GL_GrabScreen(void);

extern int		screenWidth, screenHeight, screenBits;
extern int		numTexUnits;
extern boolean	envModAdd;
extern int		defResX, defResY;
extern float	nearClip, farClip;
extern int		viewph, viewpw, viewpx, viewpy;
extern int		r_framecounter;
extern char		hiTexPath[], hiTexPath2[];
extern int		UpdateState;
extern float	vid_gamma, vid_bright, vid_contrast;
extern int		glFontFixed, glFontVariable;
extern int		r_detail;

// Console commands.
D_CMD( UpdateGammaRamp );

#endif 
