/** @file r_main.h
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#pragma once

#include "dd_types.h"
#include <de/matrix.h>

DE_EXTERN_C int      levelFullBright;

DE_EXTERN_C float    pspOffset[2], pspLightLevelMultiplier;
DE_EXTERN_C int      psp3d;
DE_EXTERN_C float    weaponOffsetScale, weaponFOVShift;
DE_EXTERN_C int      weaponOffsetScaleY;
DE_EXTERN_C byte     weaponScaleMode; // cvar

/**
 * Draws 2D HUD sprites. If they were already drawn 3D, this won't do anything.
 */
void Rend_Draw2DPlayerSprites();

/**
 * Draws 3D HUD models.
 */
void Rend_Draw3DPlayerSprites();

#ifdef min
#  undef min
#endif
#ifdef max
#  undef max
#endif

/**
 * Description of an inclusive..inclusive light intensity range.
 *
 * @ingroup data
 */
struct LightRange
{
    float min;
    float max;

    LightRange(float _min = 0, float _max = 0) : min(_min), max(_max)    {}
    LightRange(float const minMax[2]) : min(minMax[0]), max(minMax[1])   {}
    LightRange(const de::Vec2f &minMax) : min(minMax.x), max(minMax.y) {}
    LightRange(const LightRange &other) : min(other.min), max(other.max) {}

    /// Returns a textual representation of the lightlevels.
    de::String asText() const {
        return de::Stringf("(min: %.2f max: %.2f)", min, max);
    }
};

/**
 * Get a global angle from Cartesian coordinates in the map coordinate space
 * relative to the viewer.
 *
 * @param point  Map point to test.
 *
 * @return  Angle between the test point and view x,y.
 */
angle_t R_ViewPointToAngle(de::Vec2d point);

/// @copydoc R_ViewPointToAngle()
inline angle_t R_ViewPointToAngle(coord_t x, coord_t y) {
    return R_ViewPointToAngle(de::Vec2d(x, y));
}

/**
 * Determine distance to the specified point relative to the viewer.
 *
 * @param x   X coordinate to test.
 * @param y   Y coordinate to test.
 *
 * @return  Distance from the viewer to the test point.
 */
coord_t R_ViewPointDistance(coord_t x, coord_t y);

void R_ProjectViewRelativeLine2D(coord_t const center[2], dd_bool alignToViewPlane,
                                 coord_t width, coord_t offset, coord_t start[2], coord_t end[2]);

void R_ProjectViewRelativeLine2D(de::Vec2d const center, bool alignToViewPlane,
                                 coord_t width, coord_t offset, de::Vec2d &start, de::Vec2d &end);


/**
 * Generate texcoords on the surface centered on point.
 *
 * @param s              Texture s coords written back here.
 * @param t              Texture t coords written back here.
 * @param point          Point on surface around which texture is centered.
 * @param xScale         Scale multiplier on the horizontal axis.
 * @param yScale         Scale multiplier on the vertical axis.
 * @param v1             Top left vertex of the surface being projected on.
 * @param v2             Bottom right vertex of the surface being projected on.
 * @param tangentMatrix  Normalized tangent space matrix for the surface being projected to.
 *
 * @return  @c true if the generated coords are within bounds.
 */
bool R_GenerateTexCoords(de::Vec2f &s, de::Vec2f &t, const de::Vec3d &point,
                         float xScale, float yScale, const de::Vec3d &v1, const de::Vec3d &v2,
                         const de::Mat3f &tangentMatrix);
