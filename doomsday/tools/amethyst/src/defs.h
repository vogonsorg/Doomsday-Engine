/*
 * Copyright (c) 2002-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __AMETHYST_COMMON_DEFS_H__
#define __AMETHYST_COMMON_DEFS_H__

#define VERSION_STR     "1.1.2"
#define BUILD_STR       "Version " VERSION_STR " (" __DATE__ ")"
#define MAX_COLUMNS     40

#define IS_BREAK(c)     ((c)=='@' || (c)=='{' || (c)=='}' || (c)=='$')

#endif
