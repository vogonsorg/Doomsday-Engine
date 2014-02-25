/** @file gamerules.h  Game rule set.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBCOMMON_GAMERULES_H
#define LIBCOMMON_GAMERULES_H

#include "common.h"

#ifdef __cplusplus

/**
 * @ingroup libcommon
 *
 * @todo Separate behaviors so that each rule is singular.
*/
class GameRuleset
{
public:
    skillmode_t skill;
#if !__JHEXEN__
    byte fast;
#endif
    byte deathmatch;
    byte noMonsters;
#if __JHEXEN__
    byte randomClasses;
#else
    byte respawnMonsters;
#endif

public:
    GameRuleset();
    GameRuleset(GameRuleset const &other);

    GameRuleset &operator = (GameRuleset const &other);

    void write(Writer *writer) const;
    void read(Reader *reader);
};

#endif // __cplusplus

// C wrapper API ---------------------------------------------------------------

#ifdef __cplusplus
extern "C" {
#else
typedef void *GameRuleset;
#endif

skillmode_t GameRuleset_Skill(GameRuleset const *rules);
#if !__JHEXEN__
byte GameRuleset_Fast(GameRuleset const *rules);
#endif
byte GameRuleset_Deathmatch(GameRuleset const *rules);
byte GameRuleset_NoMonsters(GameRuleset const *rules);
#if __JHEXEN__
byte GameRuleset_RandomClasses(GameRuleset const *rules);
#else
byte GameRuleset_RespawnMonsters(GameRuleset const *rules);
#endif

#ifdef __cplusplus
} // extern "C"
#endif

#endif // LIBCOMMON_GAMERULES_H
