/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2007 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2007 Daniel Swanson <danij@dengine.net>
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
 * dd_input.h: Input Subsystem
 */

#ifndef __DOOMSDAY_BASEINPUT_H__
#define __DOOMSDAY_BASEINPUT_H__

#include "con_decl.h"

// Input devices.
enum
{
    IDEV_KEYBOARD = 0,
    IDEV_MOUSE,
    IDEV_JOY1,
    IDEV_JOY2,
    IDEV_JOY3,
    IDEV_JOY4,
    NUM_INPUT_DEVICES       // Theoretical maximum.
};	

typedef enum ddeventtype_e {
    E_TOGGLE,               // two-state device
    E_AXIS,                 // axis position
    E_ANGLE                 // hat angle
} ddeventtype_t;

typedef enum ddeevent_togglestate_e {
    ETOG_DOWN,
    ETOG_UP,
    ETOG_REPEAT
} ddevent_togglestate_t;

typedef enum ddevent_axistype_e {
    EAXIS_ABSOLUTE,         // absolute position on the axis
    EAXIS_RELATIVE          // offset relative to the previous position
} ddevent_axistype_t;

// These are used internally, a cutdown version containing
// only need-to-know stuff is sent down the games' responder chain. 
typedef struct ddevent_s {
    uint            device; // e.g. IDEV_KEYBOARD
    ddeventtype_t   type;   // E_TOGGLE, E_AXIS, or E_ANGLE
    union {
        struct {
            int             id;         // button/key index number
            ddevent_togglestate_t state;// state of the toggle
        } toggle;
        struct {
            int             id;         // axis index number
            float           pos;        // position of the axis
            ddevent_axistype_t type;    // type of the axis (absolute or relative)
        } axis;
        struct {
            int             id;         // angle index number
            float           pos;        // angle, or negative if centered
        } angle;
        /*
        struct {
            uint            controlID;  // axis/control/key id.
            boolean         isAxis;     // <code>true</code> = controlID is an axis id.
            uint            useclass;   // use a specific bindclass command
            boolean         noclass;
            
            int             data1;      // control is key; state (e.g. EVS_DOWN)
                                        // control is axis; position delta.
        } obsolete;
         */
    };
} ddevent_t;

// Convenience macros.
#define IS_TOGGLE_DOWN(evp)            (evp->type == E_TOGGLE && evp->toggle.state == ETOG_DOWN)
#define IS_TOGGLE_DOWN_ID(evp, togid)  (evp->type == E_TOGGLE && evp->toggle.state == ETOG_DOWN && evp->toggle.id == togid)
#define IS_TOGGLE_UP(evp)              (evp->type == E_TOGGLE && evp->toggle.state == ETOG_UP)
#define IS_TOGGLE_REPEAT(evp)          (evp->type == E_TOGGLE && evp->toggle.state == ETOG_REPEAT)
#define IS_KEY_TOGGLE(evp)             (evp->device == IDEV_KEYBOARD && evp->type == E_TOGGLE)
#define IS_KEY_DOWN(evp)               (evp->device == IDEV_KEYBOARD && evp->type == E_TOGGLE && evp->toggle.state == ETOG_DOWN) 
#define IS_KEY_PRESS(evp)              (evp->device == IDEV_KEYBOARD && evp->type == E_TOGGLE && evp->toggle.state != ETOG_UP) 
#define IS_MOUSE_DOWN(evp)             (evp->device == IDEV_MOUSE && IS_TOGGLE_DOWN(evp))
#define IS_MOUSE_UP(evp)               (evp->device == IDEV_MOUSE && IS_TOGGLE_UP(evp))
#define IS_MOUSE_MOTION(evp)           (evp->device == IDEV_MOUSE && evp->type == E_AXIS)

// Input device axis types.
enum
{
    IDAT_STICK = 0,         // joysticks, gamepads
    IDAT_POINTER = 1        // mouse
};

// Input device axis flags.
#define IDA_DISABLED 0x1    // Axis is always zero.
#define IDA_INVERT 0x2      // Real input data should be inverted.

typedef struct inputdevaxis_s {
    char    name[20];       // Symbolic name of the axis.
    int     type;           // Type of the axis (pointer or stick).
    int     flags;
    float   position;       // Current translated position of the axis (-1..1) including any filtering.
    float   realPosition;   // The actual position of the axis (-1..1).
    float   scale;          // Scaling factor for real input values.
    float   deadZone;       // Dead zone, in (0..1) range.
    int     filter;
    uint    time;           // Timestamp for the latest update that changed the position.
    struct bclass_s* bClass;
} inputdevaxis_t;

typedef struct inputdevkey_s {
    char    isDown;         // True/False for each key.
    uint    time;
    struct bclass_s* bClass;
} inputdevkey_t;

typedef struct inputdevhat_s {
    int     pos;            // Position of each hat, -1 if centered.
    uint    time;           // Timestamp for each hat for the latest change.
    struct bclass_s* bClass;
} inputdevhat_t;

// Input device flags.
#define ID_ACTIVE 0x1		// The input device is active.

typedef struct inputdev_s {
    char    name[20];       // Symbolic name of the device.
    int     flags;
    uint    numAxes;        // Number of axes in this input device.
    inputdevaxis_t *axes;
    uint    numKeys;        // Number of keys for this input device.
    inputdevkey_t *keys;
    uint    numHats;        // NUmber of hats.
    inputdevhat_t *hats;    
} inputdev_t;

extern boolean  ignoreInput;
extern int      repWait1, repWait2;
extern int      keyRepeatDelay1, keyRepeatDelay2;   // milliseconds
/*
extern int      mouseFilter;
extern int      mouseDisableX, mouseDisableY;
extern int      mouseInverseY;
extern int      mouseWheelSensi;
extern int      joySensitivity;
extern int      joyDeadZone;
*/
extern byte     showScanCodes;
extern boolean  shiftDown, altDown;

void        DD_RegisterInput(void);
void        DD_InitInput(void);
void        DD_ShutdownInput(void);
void        DD_StartInput(void);
void        DD_StopInput(void);

void        DD_ReadKeyboard(void);
void        DD_ReadMouse(void);
void        DD_ReadJoystick(void);

void        DD_PostEvent(ddevent_t *ev);
void        DD_ProcessEvents(timespan_t ticLength);
void        DD_ClearEvents(void);
void        DD_ClearKeyRepeaters(void);
byte        DD_ScanToKey(byte scan);
byte        DD_KeyToScan(byte key);
byte        DD_ModKey(byte key);

void        I_InitInputDevices(void);
void        I_ShutdownInputDevices(void);
void        I_ClearDeviceClassAssociations(void);
inputdev_t *I_GetDevice(uint ident, boolean ifactive);
inputdev_t *I_GetDeviceByName(const char *name, boolean ifactive);
boolean     I_ParseDeviceAxis(const char *str, uint *deviceID, uint *axis);
inputdevaxis_t *I_GetAxisByID(inputdev_t *device, uint id);
int         I_GetAxisByName(inputdev_t *device, const char *name);
float       I_TransformAxis(inputdev_t* dev, uint axis, float rawPos);
boolean     I_IsDeviceKeyDown(uint ident, uint code);

#endif
