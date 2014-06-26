/** @file id1map.h  id Tech 1 map format reader/interpreter.
 *
 * @authors Copyright © 2007-2014 Daniel Swanson <danij@dengine.net>
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

#ifndef WADMAPCONVERTER_ID1MAP_H
#define WADMAPCONVERTER_ID1MAP_H

#include "id1map_util.h"   // MapLumpType
#include "dd_types.h"      // lumpnum_t
#include <doomsday/uri.h>
#include <de/libcore.h>
#include <de/Error>
#include <de/String>
#include <de/StringPool>
#include <QMap>

namespace wadimp {

class MaterialDict;

/// Material group identifiers.
enum MaterialGroup {
    PlaneMaterials,
    WallMaterials
};

typedef de::StringPool::Id MaterialId;

/**
 * Map resource converter/interpreter for id Tech 1 map format(s).
 *
 * @ingroup wadmapconverter
 */
class Id1Map
{
public:
    /// Base class for load-related errors. @ingroup errors
    DENG2_ERROR(LoadError);

    /// Logical map format identifiers.
    enum Format {
        UnknownFormat = -1,

        DoomFormat,
        HexenFormat,
        Doom64Format,

        MapFormatCount
    };

    /**
     * Heuristic based map data (format) recognizer.
     *
     * Unfortunately id Tech 1 maps cannot be easily recognized, due to their
     * lack of identification signature, the mechanics of the WAD format lump
     * index and the existence of several subformat variations. Therefore it is
     * necessary to use heuristic analysis of the lump index and the lump data.
     */
    class Recognizer
    {
    public:
        typedef QMap<MapLumpType, lumpnum_t> Lumps;

    public:
        /**
         * Attempt to recognize an id Tech 1 format by traversing the WAD lump
         * index, beginning at the @a lumpIndexOffset specified.
         */
        Recognizer(lumpnum_t lumpIndexOffset);

        de::String const &mapId() const;
        Format mapFormat() const;

        Lumps const &lumps() const;

    private:
        DENG2_PRIVATE(d)
    };

public:
    /**
     * Attempt to construct a new Id1Map from the @a recognized data specified.
     */
    Id1Map(Recognizer const &recognized);

    /**
     * Transfer the map to Doomsday (i.e., rebuild in native map format via the
     * public MapEdit API).
     */
    void transfer(de::Uri const &uri);

    /**
     * Convert a textual material @a name to an internal material dictionary id.
     */
    MaterialId toMaterialId(de::String name, MaterialGroup group);

    /**
     * Convert a Doom64 style unique material @a number to an internal dictionary id.
     */
    MaterialId toMaterialId(de::dint number, MaterialGroup group);

public:
    /**
     * Returns the textual name for the identified map format @a id.
     */
    static de::String const &formatName(Format id);

private:
    DENG2_PRIVATE(d)
};

typedef Id1Map::Recognizer Id1MapRecognizer;

} // namespace wadimp

#endif // WADMAPCONVERTER_ID1MAP_H
