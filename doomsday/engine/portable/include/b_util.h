/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2007 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * b_main.h: Bindings
 */

#ifndef __DOOMSDAY_BIND_UTIL_H__
#define __DOOMSDAY_BIND_UTIL_H__

#include "de_base.h"

// Event Binding Toggle State
typedef enum ebstate_e {
    EBTOG_UNDEFINED = 0,
    EBTOG_DOWN,
    EBTOG_REPEAT,
    EBTOG_PRESS,
    EBTOG_UP,
    EBAXIS_WITHIN,
    EBAXIS_BEYOND,
    EBAXIS_BEYOND_POSITIVE,
    EBAXIS_BEYOND_NEGATIVE
} ebstate_t;

typedef enum stateconditiontype_e {
    SCT_TOGGLE_STATE,               ///< Toggle is in a specific state.
    SCT_AXIS_BEYOND,                ///< Axis is past a specific position.
    SCT_ANGLE_AT                    ///< Angle is pointing to a specific direction.
} stateconditiontype_t;

// Device state condition.
typedef struct statecondition_s {
    uint        device;             // Which device? 
    stateconditiontype_t type;
    boolean     negate;             // Test the inverse (e.g., not in a specific state).
    int         id;                 // Toggle/axis/angle identifier in the device.
    ebstate_t   state;
    float       pos;                // Axis position/angle condition.
} statecondition_t;

boolean     B_ParseToggleState(const char* toggleName, ebstate_t* state);
boolean     B_ParseAxisPosition(const char* desc, ebstate_t* state, float* pos);
boolean     B_ParseKeyId(const char* desc, int* id);
boolean     B_ParseMouseTypeAndId(const char* desc, ddeventtype_t* type, int* id);
boolean     B_ParseJoystickTypeAndId(uint device, const char* desc, ddeventtype_t* type, int* id);
boolean     B_ParseAnglePosition(const char* desc, float* pos);
boolean     B_ParseStateCondition(statecondition_t* cond, const char* desc);
boolean     B_CheckAxisPos(ebstate_t test, float testPos, float pos);
boolean     B_CheckCondition(statecondition_t* cond);

#endif // __DOOMSDAY_BIND_UTIL_H__